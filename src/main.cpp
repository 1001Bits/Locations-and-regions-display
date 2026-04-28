#include "PCH.h"
#include "Plugin.h"
#include "Settings.h"
#include "RE.h"

using namespace std;
using namespace F4SE::stl;
using namespace REL;

constexpr size_t TRAMPOLINE_SIZE = 1u << 11;

// 1.11.191 isn't predefined in CommonLibF4 (RUNTIME_LATEST stops at 1.10.984)
// so we hand-roll the constant. Anything from 1.10.984 through 1.11.191 is
// considered an "NG" runtime by Module::IsNG().
constexpr REL::Version RUNTIME_1_11_191{ 1, 11, 191, 0 };

const auto RUNTIME_VERSION_MIN = Relocate(
	F4SE::RUNTIME_1_10_163,
	F4SE::RUNTIME_1_10_984,
	F4SE::RUNTIME_VR_1_2_72);
const auto RUNTIME_VERSION_MAX = Relocate(
	F4SE::RUNTIME_1_10_163,
	RUNTIME_1_11_191,
	F4SE::RUNTIME_VR_1_2_72);
constexpr auto RUNTIME_VERSION_NG_COMPAT = RUNTIME_1_11_191;

namespace
{
	void OpenLog()
	{
	#ifndef NDEBUG
		auto sink = make_shared<spdlog::sinks::msvc_sink_mt>();
	#else
		auto path = logger::log_directory();
		if (!path) {
			report_and_fail("Failed to find standard logging directory.");
		}
		const auto gamepath = Module::IsVR() ? "Fallout4VR/F4SE" : "Fallout4/F4SE";
		if (!path.value().generic_string().ends_with(gamepath)) {
			path = path.value().parent_path().append(gamepath);
		}
		*path /= fmt::format("{}.log", Plugin::NAME);
		auto sink = make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
	#endif

		const auto level = spdlog::level::trace;
		auto       log = make_shared<spdlog::logger>("global log", move(sink));
		log->set_level(level);
		log->flush_on(level);
		spdlog::set_default_logger(move(log));
		spdlog::set_pattern("[%Y-%m-%d %T.%e][%-16s:%-4#][%L]: %v");
	}

	void F4SEAPI MessageHandler(F4SE::MessagingInterface::Message* /*a_message*/)
	{
		// All runtimes drive popups off the FlashLocationText::ShowLocationText
		// hook installed in F4SEPlugin_Load — nothing to do here.
	}

	// Hook on FlashLocationText::ShowLocationText. Member fn: (this, name).
	// Same role across all three runtimes — leaf renderer that the engine
	// only invokes after the proximity / per-location throttle has approved
	// the display. We don't call the original, so the small native subtitle
	// never renders; our SetValue call broadcasts to the big-title widget.
	//   F4 OG  (1.10.163)  = 0x140b34a40              (manual offset)
	//   F4 NG  (1.11.191)  = 0x140a741f0  ID 2223245
	//   F4 VR  (1.2.72)    = 0x140b563e0              (manual offset)
	void ShowLocationTextHook(void* /*a_this*/, const char* a_name)
	{
		if (a_name && *a_name != '\0') {
			RE::SendHUDMessage::ShowDiscoveryNotification(a_name);
		}
	}

}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	if (!a_f4se || !a_info) return false;

	OpenLog();

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION[0];

	if (a_f4se->IsEditor()) {
		logger::critical("Loading into editor, aborting.");
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	if (!a_f4se) return false;

	F4SE::Init(a_f4se);

	logger::info("{} v{}.{}.{} [{} {}] is loading",
		Plugin::NAME, Plugin::VERSION[0], Plugin::VERSION[1], Plugin::VERSION[2],
		__DATE__, __TIME__);

	const auto ver = Module::get().version();
	logger::info("Detected runtime: Fallout 4{} (v{}).", Module::IsVR() ? " VR" : "", ver.string());

	if (ver < RUNTIME_VERSION_MIN || RUNTIME_VERSION_MAX < ver) {
		logger::critical("Runtime version is not supported, aborting.");
		return false;
	}

	const auto messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler)) {
		logger::critical("Failed to get message interface, aborting.");
		return false;
	}

	Settings::Load();
	F4SE::AllocTrampoline(TRAMPOLINE_SIZE);

	// Hook FlashLocationText::ShowLocationText on every runtime. AE uses the
	// address-library ID; OG/VR use direct offsets (no NG-fork addrlib for those
	// in the project's CommonLibF4 checkout).
	REL::Relocation<std::uintptr_t> showLocationText{
		REL::Module::IsNG() ? REL::ID(2223245).address() :
		REL::Module::IsVR() ? (REL::Module::get().base() + 0xb563e0) :
		                      (REL::Module::get().base() + 0xb34a40)
	};
	showLocationText.write_branch<5>(reinterpret_cast<std::uintptr_t>(&ShowLocationTextHook));
	logger::info("Hooked FlashLocationText::ShowLocationText @ {:#x}", showLocationText.address());

	return true;
}

F4SE_EXPORT auto F4SEPlugin_Version = []() noexcept {
	F4SE::PluginVersionData data{};
	data.PluginName(Plugin::NAME);
	data.PluginVersion(Plugin::VERSION);
	data.AuthorName("powerofthree (port: Noud)");
	data.UsesAddressLibrary(true);
	data.UsesSigScanning(false);
	data.IsLayoutDependent(true);
	data.HasNoStructUse(false);
	data.CompatibleVersions({ RUNTIME_VERSION_NG_COMPAT });
	return data;
}();
