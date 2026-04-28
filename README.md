# Locations and regions display

F4SE plugin that brings back the older Bethesda-game feel for location titles
in **Fallout 4**: every time the player crosses into a named location the big
title and parent region appear on screen, not just on first discovery.

Fallout 4's stock behaviour is silent on re-entry — you only get a
"Location Discovered" splash the first time you arrive. This mod hooks the
engine's own location-text leaf renderer (`FlashLocationText::ShowLocationText`)
and broadcasts a `HUDNotificationDisplayEvent` to the big-title widget every
time the engine fires the small subtitle, so:

- the location name appears as a large top-left title
- the wiki-style parent region (e.g. "The Glowing Sea", "Diamond City",
  "Far Harbor") appears as a smaller subtitle below

Inspired by powerof3's
[Dynamic Location Popups](https://github.com/powerof3/DynamicLocationPopups)
for Skyrim — the Fallout 4 implementation is event-driven from the engine's
own HUD pipeline, so no engine hooks beyond the leaf render of the small
subtitle are needed.

## Supported runtimes

Single DLL covers all three:

| Runtime | Version | Hook target (`FlashLocationText::ShowLocationText`) |
| --- | --- | --- |
| F4 OG | 1.10.163 | `0x140b34a40` (manual offset) |
| F4 NG / AE | 1.11.191 | `0x140a741f0` (Address Library ID 2223245) |
| F4 VR | 1.2.72 | `0x140b563e0` (manual offset) |

## Installation

1. Drop `DynamicLocationPopupsF4.dll` and `DynamicLocationPopupsF4.toml` into
   `Data/F4SE/Plugins/`.
2. (Optional) Edit the TOML — `iMode` controls re-entry deduplication:
   - `0` — pop every time `TESActorLocationChangeEvent` fires (legacy mode,
     reserved for the unused event-sink fallback path)
   - `1` — same plus a one-step ping-pong filter

The default behaviour matches the engine's own proximity-correct timing
because the popup fires from the same leaf the engine uses for its native
"you've arrived" subtitle.

## Building

Requirements:

- Visual Studio 2022 (MSVC, C++23)
- CMake ≥ 3.23
- vcpkg (with `VCPKG_ROOT` set)
- a checkout of [CommonLibF4](https://github.com/alandtse/CommonLibF4) (alandtse
  fork — needed for NG/AE address-library support) at
  `external/CommonLibF4/CommonLibF4/`, or set the `CommonLibF4Path` environment
  variable to its location

```sh
git clone --recurse-submodules https://github.com/1001Bits/Locations-and-regions-display.git
cd Locations-and-regions-display

# Vendor CommonLibF4 (NG fork)
git clone https://github.com/alandtse/CommonLibF4 external/CommonLibF4/CommonLibF4

cmake --preset vs2022-windows-vcpkg
cmake --build build --config Release
```

The output DLL lands in `build/src/Release/DynamicLocationPopupsF4.dll`. If
the `FalloutPluginTargets` environment variable is set to one or more MO2 mod
folders (semicolon-separated), the post-build step auto-deploys to them.

## Project layout

```
src/
  main.cpp        — F4SE plugin entry; installs the leaf hook on every runtime
  RE.h            — HUDNotificationDisplayEvent layout + SetValue dispatcher
  Areas.h         — wiki-sourced location → parent region table
  Settings.h      — TOML loader (iMode)
scripts/          — Python tooling used during the AE port
                    (binary disassembly, address-library cross-ref, etc.)
data/             — packaged TOML for installation
cmake/            — common build helpers and Plugin.h template
```

## Credits

- powerof3 — original Skyrim mod (Dynamic Location Popups)
- alandtse — CommonLibF4 NG fork providing the cross-runtime address library

## License

MIT — see `LICENSE`.
