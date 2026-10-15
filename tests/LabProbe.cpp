// Private-desktop verification only. No product hooks, native HUD mutations,
// synthesized Windows input, save imports, or deployment/package rules.
#include "PCH.h"
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <fstream>
#include <thread>
#include <iomanip>

namespace
{
    constexpr REL::Version runtime{ 1, 11, 240, 0 };
    std::filesystem::path directory;
    std::mutex logLock;
    std::atomic_bool ready{}, observing{};
    std::atomic_bool snapshotPending{};
    unsigned captureIndex{};
    std::atomic_bool captureRequested{};
    using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
    PresentFn originalPresent{};
    std::vector<std::string> watchPaths;
    std::string lastSnapshot;
    using Value = RE::Scaleform::GFx::Value;

    void Log(const std::string& message)
    {
        std::scoped_lock lock(logLock);
        std::ofstream out(directory / "probe.log", std::ios::app);
        out << GetTickCount64() << ' ' << message << '\n';
    }

    RE::PlayerCharacter* Player()
    {
        return *REL::Relocation<RE::PlayerCharacter**>{ REL::ID(2698073) };
    }

    RE::Scaleform::Ptr<RE::IMenu> HUD()
    {
        // Direct AE ID; the older multi-runtime header's UI singleton ID was removed.
        const auto ui = *REL::Relocation<RE::UI**>{ REL::ID(4796314) };
        return ui ? ui->GetMenu("HUDMenu") : nullptr;
    }

    std::string Describe(const Value& value)
    {
        if (value.IsString()) {
            std::ostringstream out;
            out << std::quoted(value.GetString());
            return out.str();
        }
        if (value.IsBoolean()) return value.GetBoolean() ? "true" : "false";
        if (value.IsNumber()) return fmt::format("{}", value.GetNumber());
        if (value.IsInt()) return fmt::format("{}", value.GetInt());
        if (value.IsUInt()) return fmt::format("{}", value.GetUInt());
        return fmt::format("type:{}", static_cast<unsigned>(value.GetType()));
    }

    struct Text { RE::BSFixedStringCS subtitle, title; };
    struct Source { std::uintptr_t vtable; RE::BSTOptional<Text> value; RE::BSSpinLock lock; };
    static_assert(sizeof(Source) == 0x28);

    void Snapshot(bool force = false)
    {
        const auto model = *REL::Relocation<std::uintptr_t*>{ REL::Offset(0x32745f8) };
        std::string state = fmt::format("hud={:X}", model);
        if (model) {
            auto* hnm = reinterpret_cast<std::byte*>(model + 0xc40);
            const RE::BSAutoLock queueLock{ *reinterpret_cast<RE::BSSpinLock*>(hnm + 0xb0) };
            state += fmt::format(" queue={}", *reinterpret_cast<std::uint32_t*>(hnm + 0xa8));
            for (const auto offset : { 0x120 }) {
                auto& source = *reinterpret_cast<Source*>(hnm + offset);
                const RE::BSAutoLock sourceLock{ source.lock };
                state += fmt::format(" source{:X}={}", offset, source.value.has_value());
                if (offset == 0x120 && source.value.has_value()) {
                    state += fmt::format(" subtitle=[{}] title=[{}]", source.value->subtitle.c_str(), source.value->title.c_str());
                }
            }
        }
        if (const auto menu = HUD(); menu && menu->uiMovie) {
            for (const auto& path : watchPaths) {
                Value value;
                state += " " + path + "=";
                state += menu->uiMovie->GetVariable(&value, path.c_str()) ? Describe(value) : "missing";
            }
        }
        if (force || state != lastSnapshot) {
            Log("snapshot " + state);
            lastSnapshot = std::move(state);
        }
    }

    void Status()
    {
        const auto ui = *REL::Relocation<RE::UI**>{ REL::ID(4796314) };
        Log(fmt::format("menus loading={} main={}", ui && ui->GetMenuOpen("LoadingMenu"), ui && ui->GetMenuOpen("MainMenu")));
        const auto player = Player();
        const auto cell = player ? player->GetParentCell() : nullptr;
        const auto location = player ? player->GetCurrentLocation() : nullptr;
        Log(fmt::format("status player={} cell={:08X} location={:08X} name=[{}] parent={:08X}",
            player != nullptr, cell ? cell->formID : 0, location ? location->formID : 0,
            location ? location->fullName.c_str() : "", location && location->parentLoc ? location->parentLoc->formID : 0));
        const auto world = cell && cell->IsExterior() ? cell->worldSpace : nullptr;
        Log(fmt::format("world id={:08X} name=[{}] product={}", world ? world->formID : 0,
            world ? world->fullName.c_str() : "", GetModuleHandleW(L"DynamicLocationPopupsF4.dll") != nullptr));
        if (player) Log(fmt::format("position x={} y={} z={}", player->data.location.x, player->data.location.y, player->data.location.z));
        Snapshot(true);
    }

    void CaptureNow()
    {
        // Read back the actual rendered backbuffer, even on a non-input desktop.
        const REL::Relocation<RE::BSGraphics::RendererData*> renderer{ REL::ID(2704527) };
        // Called on the rendering thread immediately before Present. Main/UI
        // tasks run while the renderer lock belongs to another thread in AE.
        auto* swap = reinterpret_cast<IDXGISwapChain*>(renderer->renderWindow[0].swapChain);
        auto* device = reinterpret_cast<ID3D11Device*>(renderer->device);
        auto* context = reinterpret_cast<ID3D11DeviceContext*>(renderer->context);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer, staging;
        if (!swap || !device || !context || FAILED(swap->GetBuffer(0, IID_PPV_ARGS(&buffer)))) {
            Log("capture-error backbuffer unavailable"); return;
        }
        D3D11_TEXTURE2D_DESC desc{};
        buffer->GetDesc(&desc);
        const bool bgra = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        const bool rgba = desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        if ((!bgra && !rgba) || desc.SampleDesc.Count != 1 || desc.Width > 4096 || desc.Height > 4096) {
            Log("capture-error unsupported format"); return;
        }
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) { Log("capture-error allocation"); return; }
        context->CopyResource(staging.Get(), buffer.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) { Log("capture-error readback"); return; }
        const auto name = fmt::format("capture-{:04}.ppm", ++captureIndex);
        std::ofstream out(directory / name, std::ios::binary);
        out << "P6\n" << desc.Width << ' ' << desc.Height << "\n255\n";
        std::vector<char> rgb(desc.Width * 3);
        for (UINT y = 0; y < desc.Height; ++y) {
            const auto* row = static_cast<const unsigned char*>(mapped.pData) + y * mapped.RowPitch;
            for (UINT x = 0; x < desc.Width; ++x) {
                rgb[x*3] = row[x*4 + (bgra ? 2 : 0)];
                rgb[x*3+1] = row[x*4+1];
                rgb[x*3+2] = row[x*4 + (bgra ? 0 : 2)];
            }
            out.write(rgb.data(), rgb.size());
        }
        context->Unmap(staging.Get(), 0);
        out.close();
        Log("capture=" + name);
    }

    HRESULT STDMETHODCALLTYPE Present(IDXGISwapChain* swap, UINT interval, UINT flags)
    {
        if (captureRequested.exchange(false)) CaptureNow();
        return originalPresent(swap, interval, flags);
    }

    void Capture()
    {
        if (!originalPresent) {
            const REL::Relocation<RE::BSGraphics::RendererData*> renderer{ REL::ID(2704527) };
            auto* swap = reinterpret_cast<IDXGISwapChain*>(renderer->renderWindow[0].swapChain);
            if (!swap) { Log("capture-error swapchain unavailable"); return; }
            auto** table = *reinterpret_cast<void***>(swap);
            DWORD oldProtection{};
            if (!VirtualProtect(table + 8, sizeof(void*), PAGE_READWRITE, &oldProtection)) {
                Log("capture-error cannot install present observer"); return;
            }
            originalPresent = reinterpret_cast<PresentFn>(table[8]);
            InterlockedExchangePointer(table + 8, reinterpret_cast<void*>(&Present));
            DWORD ignored{};
            VirtualProtect(table + 8, sizeof(void*), oldProtection, &ignored);
            Log("capture observer installed on private process swapchain");
        }
        captureRequested = true;
    }

    bool AllowedConsole(std::string_view command)
    {
        constexpr std::array exact{ "qqq"sv, "tgm"sv, "tcai"sv, "tai"sv, "tcl"sv, "set timescale to 0"sv,
            "set gamehour to 12"sv, "fov 80"sv, "save DLPVerification"sv, "load DLPVerification"sv,
            "save DLPApproach"sv, "load DLPApproach"sv };
        constexpr std::array prefixes{ "coc "sv, "cow "sv, "player.setpos "sv, "player.setangle "sv,
            "player.rewardxp "sv, "player.setlevel "sv, "setstage "sv, "startquest "sv, "stopquest "sv,
            "resetquest "sv, "player.moveto "sv, "player.getav "sv, "getstage "sv, "rewardxp "sv };
        return std::find(exact.begin(), exact.end(), command) != exact.end() ||
               std::any_of(prefixes.begin(), prefixes.end(), [&](auto p) { return command.starts_with(p); });
    }

    void Run(const std::string& command)
    {
        Log("command=" + command);
        if (command == "status") Status();
        else if (command == "snapshot") Snapshot(true);
        else if (command == "capture") Capture();
        else if (command == "observe on") observing = true;
        else if (command == "observe off") observing = false;
        else if (command.starts_with("xp ")) {
            const auto amount = std::stof(command.substr(3));
            if (amount > 0 && amount <= 10000 && Player()) Player()->RewardExperience(amount, false, nullptr, nullptr);
        }
        else if (command.starts_with("watch ")) watchPaths.push_back(command.substr(6));
        else if (command.starts_with("dump ")) {
            if (const auto menu = HUD(); menu && menu->uiMovie) {
                Value value;
                if (menu->uiMovie->GetVariable(&value, command.c_str() + 5) && value.IsObject()) {
                    value.VisitMembers([](const char* name, const Value& v) { Log(std::string("member ") + name + "=" + Describe(v)); });
                } else Log("dump missing object");
            }
        } else if (command.starts_with("popup ") || command == "popup-current") {
            if (!GetModuleHandleW(L"DynamicLocationPopupsF4.dll")) { Log("popup rejected: product absent"); }
            else {
                // Exercise the installed product's real hooked engine entry.
                const REL::Relocation<void(void*, const char*)> entry{ REL::ID(2223245) };
                const auto location = Player() ? Player()->GetCurrentLocation() : nullptr;
                const char* name = command == "popup-current" ? (location ? location->fullName.c_str() : nullptr) : command.c_str() + 6;
                if (name) entry(nullptr, name);
            }
        } else if (AllowedConsole(command)) {
            const REL::Relocation<void(const char*)> execute{ REL::ID(2248537) };
            execute(command.c_str());
        } else Log("rejected command");
        Log("completed=" + command);
    }

    void Worker()
    {
        std::size_t consumed{};
        for (;;) {
            if (ready) {
                std::ifstream in(directory / "commands.txt");
                std::string line;
                std::size_t index{};
                while (std::getline(in, line)) {
                    if (index++ < consumed) continue;
                    ++consumed;
                    if (!line.empty()) {
                        if (AllowedConsole(line) || line == "capture" || line.starts_with("xp ")) F4SE::GetTaskInterface()->AddTask([line] { Run(line); });
                        else F4SE::GetTaskInterface()->AddUITask([line] { Run(line); });
                    }
                }
                if (observing && !snapshotPending.exchange(true)) {
                    F4SE::GetTaskInterface()->AddUITask([] { Snapshot(); snapshotPending = false; });
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void Message(F4SE::MessagingInterface::Message* message)
    {
        Log(fmt::format("message={} data={}", message->type, reinterpret_cast<std::uintptr_t>(message->data)));
        if (message->type == F4SE::MessagingInterface::kGameDataReady && message->data) {
            ready = true; Log("GameDataReady");
        } else if (message->type == F4SE::MessagingInterface::kGameLoaded) Log("GameLoaded");
    }
}

extern "C" __declspec(dllexport) bool F4SEPlugin_Load(const F4SE::LoadInterface* api)
{
    wchar_t path[32768]{}, desktopName[256]{};
    if (!GetEnvironmentVariableW(L"DLP_VERIFICATION_DIR", path, 32768) || api->RuntimeVersion() != runtime ||
        !GetEnvironmentVariableW(L"PERFCMP_PRIVATE_DESKTOP", desktopName, 256)) return false;
    wchar_t actual[256]{};
    DWORD needed{};
    if (!GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME, actual, sizeof(actual), &needed) ||
        wcscmp(actual, desktopName) || !wcscmp(actual, L"Default")) return false;
    directory = path;
    F4SE::Init(api, false);
    Log(fmt::format("ProbeLoaded pid={} product={}", GetCurrentProcessId(), GetModuleHandleW(L"DynamicLocationPopupsF4.dll") != nullptr));
    if (!F4SE::GetMessagingInterface()->RegisterListener(Message)) return false;
    std::thread(Worker).detach();
    return true;
}

F4SE_EXPORT auto F4SEPlugin_Version = []() noexcept {
    F4SE::PluginVersionData data{};
    data.PluginName("DLPVerificationProbe");
    data.PluginVersion(REL::Version{ 1, 0, 0, 0 });
    data.CompatibleVersions({ runtime });
    return data;
}();
