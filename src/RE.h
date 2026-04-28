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
		// Address-library layout (offsets from image base, all confirmed via
		// Ghidra; NG values target the 1.11.191 binary specifically — the
		// 1.10.984 patch has different offsets, see scripts/addrlib.py for
		// the cross-reference):
		//
		//                                              F4 OG       F4 NG (1.11.191)  F4 VR
		//   HUDDataModel singleton pointer (data)      0x5a60a30   0x32694e8         0x5ac1850
		//   HUDNotificationsModel offset within HDM    0x0c40      0x0c40            0x13a8
		//   BSTValueEventSource offset within HNM      0x0120      0x0120            0x0120
		//   ::SetValue                                 0xa74e90    0x9f41b0          0xa78970
		//   ::ClearValueIfHasValue                     0xa70630    (n/a — see        0xa74110
		//                                                          fallback below)
		//
		// NG inlined the spinlock+notify into every caller of the event source,
		// so there's no standalone ClearValueIfHasValue for us to call. We work
		// around it by reusing SetValue with empty BSFixedStringCS values —
		// Flash renders the cleared text and the widget effectively hides.
		inline void ShowDiscoveryNotification(const char* a_name)
		{
			if (!a_name || *a_name == '\0') {
				return;
			}

			// Write to the HUDNotificationsModel's BSTValueEventSource at HNM+0x120.
			// Layout offsets:
			//                                        F4 OG       F4 NG (AE)   F4 VR
			//   HUDDataModel singleton ptr           0x5a60a30   0x32694e8    0x5ac1850
			//   HUDNotificationsModel within HDM     0x0c40      0x0c40       0x13a8
			//   BSTValueEventSource within HNM       0x0120      0x0120       0x0120
			//   ::SetValue                           0xa74e90    0x9f41b0     0xa78970
			//   ::ClearValueIfHasValue               0xa70630    (n/a)        0xa74110
			// NG inlined the spinlock+notify into every caller of the event
			// source's clear path, so there's no standalone ClearValueIfHasValue.
			// We work around it by reusing SetValue with empty BSFixedStringCS
			// values — Flash renders the cleared text and the widget effectively
			// hides.
			static const auto hudDataAddr    = REL::Relocate<std::uintptr_t>(0x5a60a30, 0x32694e8, 0x5ac1850);
			static const auto hnmOffset      = REL::Relocate<std::uintptr_t>(0xc40,     0xc40,     0x13a8);
			static const auto setValueAddr   = REL::Relocate<std::uintptr_t>(0xa74e90,  0x9f41b0,  0xa78970);
			static const auto clearValueAddr = REL::Relocate<std::uintptr_t>(0xa70630,  0,         0xa74110);
			constexpr std::uintptr_t evSourceOffset = 0x120;

			const REL::Relocation<std::uintptr_t*> hudDataSlot{ REL::Offset(hudDataAddr) };
			const auto hudData = *hudDataSlot.get();
			if (hudData == 0) {
				return;
			}

			const auto eventSource = hudData + hnmOffset + evSourceOffset;

			HUDNotificationDisplayEvent ev{};
			ev.title = a_name;
			if (const char* areaName = FindAreaNameForLocation(a_name)) {
				ev.subtitle = areaName;
			}

			using setvalue_t = std::uint64_t(std::uintptr_t, HUDNotificationDisplayEvent*);
			const REL::Relocation<setvalue_t> setValue{ REL::Offset(setValueAddr) };
			setValue(eventSource, &ev);

			static std::atomic<std::uint64_t> generation{ 0 };
			const auto myGeneration = ++generation;
			std::thread([eventSource, myGeneration]() {
				std::this_thread::sleep_for(std::chrono::milliseconds(3000));
				if (generation.load() != myGeneration) {
					return;
				}
				if (REL::Module::IsNG()) {
					// NG: no standalone clear — overwrite with an empty event.
					HUDNotificationDisplayEvent emptyEv{};
					using setvalue_t = std::uint64_t(std::uintptr_t, HUDNotificationDisplayEvent*);
					const REL::Relocation<setvalue_t> setValue{ REL::Offset(setValueAddr) };
					setValue(eventSource, &emptyEv);
				} else {
					using clearvalue_t = std::uint64_t(std::uintptr_t);
					const REL::Relocation<clearvalue_t> clearValueIfHasValue{ REL::Offset(clearValueAddr) };
					clearValueIfHasValue(eventSource);
				}
			}).detach();
		}

	}
}
