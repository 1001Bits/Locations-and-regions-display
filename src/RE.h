#pragma once

#include "Areas.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace RE
{
	namespace SendHUDMessage
	{
		// HUDNotificationDisplayEvent — payload broadcast to the HUDMenu Flash UI.
		// Layout reverse-engineered from HUDNotificationsModel::UpdateLocationVignette
		// (F4 OG @ 0x140a66380) plus empirical testing: the Flash widget renders
		// field +8 as the big top title and field +0 as the smaller subtitle below.
		// (The engine's first-discovery path puts the location name in +0 and the
		// "HDT_LOCATION_DISCOVERED" interned BSFixedString in +8 — Flash special-
		// cases that marker to swap to the centred first-discovery layout. We
		// don't try to hit that special case anymore: we just write directly to
		// the slots Flash renders by default.)
		struct HUDNotificationDisplayEvent
		{
			RE::BSFixedStringCS subtitle;  // 00 — small subtitle slot (under)
			RE::BSFixedStringCS title;     // 08 — big top title slot
		};
		static_assert(sizeof(HUDNotificationDisplayEvent) == 0x10);

		// Map a location name to its wiki-style "area" subtitle. The wiki's
		// areas (Back Bay, Diamond City, The Glowing Sea, Far Harbor, etc.)
		// don't line up with the in-game BGSLocation parent chain — e.g. Rocky
		// Cave's parentLoc is Commonwealth directly, with no Glowing Sea node
		// in between. So we use a hardcoded location → area lookup sourced
		// from the wiki (see Areas.h). Unknown locations (mods, "Other"
		// category, or top-level area names) get no subtitle.
		[[nodiscard]] inline const char* FindAreaNameForLocation(const char* a_locationName)
		{
			return Areas::GetAreaForLocation(a_locationName);
		}

		// Big left-side first-discovery title widget. Bypasses HUDNotificationsModel's
		// queue + UpdateLocationVignette entirely: we go straight to the Flash UI by
		// setting the value on the model's HUDNotificationDisplayEvent source and
		// broadcasting. The chime ("UIDiscoverLocation") and XP-rollup vignette live
		// inside UpdateLocationVignette, so neither fires.
		//
		// Because we bypass the model's queue, the engine never ticks its fade timer
		// (UpdateDisplayTimers → ClearValueIfHasValue) for our popups, so the title
		// would otherwise stay on screen forever. We schedule a delayed clear
		// ourselves; an atomic generation counter ensures rapid re-entries don't let
		// an older timer wipe out a newer popup.
		//
		// Address-library layout — offsets from image base, all confirmed via
		// Ghidra. The HUDDataModel singleton pointer differs between NG patches
		// because 1.11.191 dropped its address-library ID; see GetHUDDataAddr.
		//
		//                                            F4 OG     F4 NG 984    F4 NG 1.11.191    F4 VR
		//   HUDDataModel singleton pointer (data)   0x5a60a30  0x2ff2078    0x32694e8         0x5ac1850
		//   HUDNotificationsModel offset within HDM 0x0c40     0x0c40       0x0c40            0x13a8
		//   BSTValueEventSource offset within HNM   0x0120     0x0120       0x0120            0x0120
		//   ::SetValue                              0xa74e90   0x9a09e0     0x9f41b0          0xa78970
		//                                                      (NG ID 2221317 covers both NG patches)
		//   ::ClearValueIfHasValue                  0xa70630   inlined      inlined           0xa74110
		//
		// NG inlined the spinlock+notify into every caller of the event source,
		// so there's no standalone ClearValueIfHasValue for us to call on either
		// NG patch. We work around it by reusing SetValue with empty
		// BSFixedStringCS values — Flash renders the cleared text and the widget
		// effectively hides.
		using setvalue_t = std::uint64_t(std::uintptr_t, HUDNotificationDisplayEvent*);
		using clearvalue_t = std::uint64_t(std::uintptr_t);

		// HUDDataModel singleton pointer per runtime. NG splits between the
		// 1.10.984 and 1.11.191 patches — the singleton's address-library ID
		// (2694613) was dropped on 1.11.191 so we version-detect the offset
		// rather than relying on REL::ID for it.
		[[nodiscard]] inline std::uintptr_t GetHUDDataAddr()
		{
			if (REL::Module::IsF4()) return 0x5a60a30;  // 1.10.163
			if (REL::Module::IsVR()) return 0x5ac1850;  // 1.2.72
			constexpr REL::Version kVer_1_11_191{ 1, 11, 191, 0 };
			return (REL::Module::get().version() < kVer_1_11_191)
				? 0x2FF2078u   // NG 1.10.984
				: 0x32694E8u;  // NG 1.11.191 (recovered via direct binary analysis)
		}

		// HUDNotificationsModel offset within HUDDataModel.
		[[nodiscard]] inline std::uintptr_t GetHNMOffset()
		{
			return REL::Module::IsVR() ? 0x13a8u : 0xc40u;
		}

		// SetValue and ClearValueIfHasValue addresses. NG patches share the
		// SetValue address-library ID (2221317), but ClearValueIfHasValue was
		// inlined into every caller on NG so we fall back to SetValue with an
		// empty event when clearing on either NG patch.
		[[nodiscard]] inline REL::Relocation<setvalue_t> GetSetValue()
		{
			if (REL::Module::IsNG()) return REL::Relocation<setvalue_t>{ REL::ID(2221317) };
			const auto off = REL::Module::IsVR() ? 0xa78970u : 0xa74e90u;
			return REL::Relocation<setvalue_t>{ REL::Offset(off) };
		}

		inline void ShowDiscoveryNotification(const char* a_name)
		{
			if (!a_name || *a_name == '\0') {
				return;
			}

			constexpr std::uintptr_t evSourceOffset = 0x120;
			const REL::Relocation<std::uintptr_t*> hudDataSlot{ REL::Offset(GetHUDDataAddr()) };
			const auto hudData = *hudDataSlot.get();
			if (hudData == 0) {
				return;
			}
			const auto eventSource = hudData + GetHNMOffset() + evSourceOffset;

			HUDNotificationDisplayEvent ev{};
			ev.title = a_name;
			if (const char* areaName = FindAreaNameForLocation(a_name)) {
				ev.subtitle = areaName;
			} else if (!Areas::IsTopLevelArea(a_name)) {
				// Default region for any unmapped Commonwealth-side location.
				// Top-level worldspace headings (Far Harbor, Nuka-World, …)
				// stay subtitle-less so we don't render "Far Harbor\nCommonwealth".
				ev.subtitle = "Commonwealth";
			}

			const auto setValue = GetSetValue();
			setValue(eventSource, &ev);

			static std::atomic<std::uint64_t> generation{ 0 };
			const auto myGeneration = ++generation;
			std::thread([eventSource, myGeneration]() {
				std::this_thread::sleep_for(std::chrono::milliseconds(3000));
				if (generation.load() != myGeneration) {
					return;
				}
				if (REL::Module::IsNG()) {
					// NG inlined the standalone clear; overwrite with an empty event.
					HUDNotificationDisplayEvent emptyEv{};
					const auto setValue = GetSetValue();
					setValue(eventSource, &emptyEv);
				} else {
					const auto clearAddr = REL::Module::IsVR() ? 0xa74110u : 0xa70630u;
					const REL::Relocation<clearvalue_t> clearValueIfHasValue{ REL::Offset(clearAddr) };
					clearValueIfHasValue(eventSource);
				}
			}).detach();
		}

	}
}
