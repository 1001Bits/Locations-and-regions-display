#pragma once

#include "Areas.h"
#include "Districts.h"
#include "Notifications.h"
#include "Settings.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>

namespace RE::SendHUDMessage
{
	struct NotificationText
	{
		RE::BSFixedStringCS subtitle;
		RE::BSFixedStringCS title;
	};
	static_assert(sizeof(NotificationText) == 0x10);

	// Layout used by BSTValueEventSource<HUDNotificationDisplayEvent>. The
	// broadcast helper only reads this optional and locks dataLock; it makes
	// no virtual calls. We construct a PRIVATE source for each display/clear.
	// The native source is inspected under its lock, never assigned or reset.
	struct NotificationSource
	{
		using value_type = NotificationText;
		std::uintptr_t vtable{};
		RE::BSTOptional<NotificationText> optionalValue;
		RE::BSSpinLock dataLock;
	};
	static_assert(offsetof(NotificationSource, optionalValue) == 0x08);
	static_assert(sizeof(RE::BSTOptional<NotificationText>) == 0x18);
	static_assert(offsetof(NotificationSource, dataLock) == 0x20);
	static_assert(sizeof(NotificationSource) == 0x28);

	inline std::atomic<std::uint64_t> hudEpoch{ 0 };
	inline Notifications::PopupState popupState;  // UI thread only

	inline void InvalidateHUD()
	{
		// Also protects against an allocator reusing the old HUDDataModel address.
		++hudEpoch;
	}

	[[nodiscard]] inline RE::PlayerCharacter* GetPlayerForLocation()
	{
		static const REL::Relocation<RE::PlayerCharacter**> player{ REL::RelocationID(303410, 2698073) };
		return *player;
	}

	// Official language of the loaded strings; districts are only shown when it is known.
	inline std::atomic<int> districtLanguage{ Districts::kUnknownLanguage };

	// TESForm::GetFormByNumericID(formID), which takes the form map's read lock itself.
	// Exact builds only, verified in each executable. CommonLibF4's GetFormByID uses NG IDs
	// 2689178/2689189, which the 1.11.x address libraries lack; the lookup then silently
	// resolves to an unrelated entry and the game crashes.
	[[nodiscard]] inline std::uintptr_t GetFormByNumericIDAddr()
	{
		if (REL::Module::IsF4()) return 0x152c90;
		if (REL::Module::IsVR()) return 0x1598c0;
		const auto ver = REL::Module::get().version();
		if (ver == REL::Version{ 1, 10, 984, 0 }) return 0x2bd440;
		if (ver == REL::Version{ 1, 11, 191, 0 }) return 0x311850;
		if (ver == REL::Version{ 1, 11, 221, 0 }) return 0x311860;
		if (ver == REL::Version{ 1, 11, 240, 0 }) return 0x311b80;
		return 0;
	}

	inline void DetectDistrictLanguage()
	{
		const auto address = GetFormByNumericIDAddr();
		if (!address) {
			logger::info("District names: form lookup not verified for this runtime; using location names only");
			return;
		}
		const REL::Relocation<RE::TESForm*(std::uint32_t)> getForm{ REL::Offset(address) };
		districtLanguage = Districts::DetectLanguage([&](std::uint32_t a_formID) -> const char* {
			const auto* form = getForm(a_formID);
			if (!form || !form->Is(RE::ENUM_FORM_ID::kLCTN)) return nullptr;
			return static_cast<const RE::BGSLocation*>(form)->fullName.c_str();
		});
		const int language = districtLanguage;
		if (language != Districts::kUnknownLanguage) {
			logger::info("District names: {}", Districts::data::kLanguages[static_cast<std::size_t>(language)]);
		} else {
			logger::info("District names: game strings are not an official translation; using location names only");
		}
	}

	[[nodiscard]] inline const char* FindAreaNameForLocation(const char* a_locationName)
	{
		const auto* player = GetPlayerForLocation();
		if (!player) return nullptr;
		const auto* cell = player->GetParentCell();
		// This field is a union; an interior's value is not a worldspace pointer.
		const auto* world = cell && cell->IsExterior() ? cell->worldSpace : nullptr;
		if (world && world->formID == Districts::data::kCommonwealth) {
			const auto position = player->GetPosition();
			if (const auto* district = Districts::Find(districtLanguage, a_locationName, position.x, position.y)) {
				return district;
			}
		}
		return Areas::GetAreaForLocation(a_locationName, player->GetCurrentLocation(), world);
	}

	[[nodiscard]] inline std::uintptr_t GetHUDDataAddr()
	{
		if (REL::Module::IsF4()) return 0x5a60a30;
		if (REL::Module::IsVR()) return 0x5ac1850;
		constexpr REL::Version ver191{ 1, 11, 191, 0 };
		constexpr REL::Version ver221{ 1, 11, 221, 0 };
		constexpr REL::Version ver240{ 1, 11, 240, 0 };
		const auto ver = REL::Module::get().version();
		if (ver < ver191) return 0x2ff2078;
		if (ver < ver221) return 0x32694e8;
		if (ver < ver240) return 0x3269568;
		return 0x32745f8;
	}

	[[nodiscard]] inline Notifications::Context GetContext()
	{
		const REL::Relocation<std::uintptr_t*> slot{ REL::Offset(GetHUDDataAddr()) };
		return { *slot.get(), hudEpoch.load() };
	}

	[[nodiscard]] inline NotificationSource& NativeSource(std::uintptr_t a_hudData)
	{
		const auto modelOffset = REL::Module::IsVR() ? 0x13a8u : 0xc40u;
		return *reinterpret_cast<NotificationSource*>(a_hudData + modelOffset + 0x120);
	}

	// The notification queue precedes the display sources in the model. Lock in
	// the engine's order (queue, then source). Pending vignettes also get priority,
	// including banners using the other native display source at model + 0xd8.
	[[nodiscard]] inline RE::BSSpinLock& NativeQueueLock(NotificationSource& a_source)
	{
		return *reinterpret_cast<RE::BSSpinLock*>(reinterpret_cast<std::uintptr_t>(&a_source) - 0x70);
	}

	[[nodiscard]] inline bool NativeQueued(const NotificationSource& a_source)
	{
		return *reinterpret_cast<const std::uint32_t*>(reinterpret_cast<std::uintptr_t>(&a_source) - 0x78) != 0;
	}

	inline void Broadcast(NotificationSource& a_privateSource)
	{
		// The Notify helper used by the engine's SetValue and real optional clear.
		// Verified from OG/VR and 1.11.240 disassembly. NG ID 2221259 is Notify;
		// ID 2221317 (used in 1.2.2) is SetValue and occupies the native queue.
		using notify_t = void(NotificationSource*);
		const REL::Relocation<notify_t> notify = REL::Module::IsNG() ?
			REL::Relocation<notify_t>{ REL::ID(2221259) } :
			REL::Relocation<notify_t>{ REL::Offset(REL::Module::IsVR() ? 0xa733a0 : 0xa6f8c0) };
		notify(&a_privateSource);
	}

	inline void ClearPopup(Notifications::Ticket a_ticket)
	{
		const auto context = GetContext();
		if (!popupState.Expire(a_ticket, context) || !context.hudData) return;
		auto& native = NativeSource(context.hudData);
		const RE::BSAutoLock queueLock{ NativeQueueLock(native) };
		const RE::BSAutoLock lock{ native.dataLock };
		// A native quest/level/discovery banner always has priority. Its source
		// remains untouched even if our fade task was already queued beforehand.
		Notifications::PublishIfIdle(native, nullptr, Broadcast, NativeQueued(native));
	}

	class FadeTimer
	{
	public:
		[[nodiscard]] static FadeTimer& GetSingleton()
		{
			// Process-lifetime worker: do not destroy its mutex while it is asleep
			// during DLL/process teardown. It never accesses HUD data itself.
			static auto* singleton = new FadeTimer;
			return *singleton;
		}

		void Schedule(Notifications::Ticket a_ticket, std::uint32_t a_delayMS)
		{
			{
				std::lock_guard lock{ _mutex };
				++_generation;
				_ticket = a_ticket;
				_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(a_delayMS);
				_armed = true;
				if (!_workerStarted) {
					_workerStarted = true;
					std::thread(&FadeTimer::Run, this).detach();
				}
			}
			_cv.notify_one();
		}

	private:
		void Run()
		{
			std::unique_lock lock{ _mutex };
			for (;;) {
				_cv.wait(lock, [this] { return _armed; });
				const auto generation = _generation;
				if (_cv.wait_until(lock, _deadline, [this, generation] { return _generation != generation; })) continue;
				if (!_armed || _generation != generation) continue;
				_armed = false;
				const auto ticket = _ticket;
				lock.unlock();
				if (const auto task = F4SE::GetTaskInterface()) {
					task->AddUITask([ticket] { ClearPopup(ticket); });
				}
				lock.lock();
			}
		}

		std::mutex _mutex;
		std::condition_variable _cv;
		std::chrono::steady_clock::time_point _deadline{};
		Notifications::Ticket _ticket{};
		std::uint64_t _generation{};
		bool _armed{};
		bool _workerStarted{};
	};

	inline void ShowDiscoveryNotification(const char* a_name)
	{
		if (!a_name || !*a_name) return;
		const auto task = F4SE::GetTaskInterface();
		if (!task) return;
		const auto epoch = hudEpoch.load();
		NotificationText text{};
		text.title = a_name;
		if (const auto* area = FindAreaNameForLocation(a_name)) text.subtitle = area;

		// Own both interned strings across the UI handoff; no raw title pointer or
		// engine form pointer survives the hook. Display and fade use one thread.
		task->AddUITask([text, epoch] {
			const auto context = GetContext();
			if (!context.hudData || context.epoch != epoch) return;
			auto& native = NativeSource(context.hudData);
			const RE::BSAutoLock queueLock{ NativeQueueLock(native) };
			const RE::BSAutoLock lock{ native.dataLock };
			if (!Notifications::PublishIfIdle(native, &text, Broadcast, NativeQueued(native))) return;
			const auto ticket = popupState.Start(context);
			if (!Settings::bStayOnScreen) {
				FadeTimer::GetSingleton().Schedule(ticket, Settings::iDisplayTimeMS);
			}
		});
	}
}
