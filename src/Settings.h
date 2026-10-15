#pragma once

#include <toml++/toml.h>

#include <algorithm>

struct Settings
{
	static void Load()
	{
		constexpr auto path = "Data/F4SE/Plugins/DynamicLocationPopupsF4.toml"sv;

		try {
			const auto tbl = toml::parse_file(path);
			bStayOnScreen = tbl["Settings"]["bStayOnScreen"].value_or<bool>(false);
			iDisplayTimeMS = tbl["Settings"]["iDisplayTimeMS"].value_or<std::uint32_t>(std::uint32_t{ kDefaultDisplayTimeMS });
			ClampDisplayTime();
			logger::info("Settings loaded from {}: bStayOnScreen={}, iDisplayTimeMS={}",
				path, bStayOnScreen, iDisplayTimeMS);
		} catch (const toml::parse_error& e) {
			logger::warn("Failed to parse {} ({}); using defaults: bStayOnScreen={}, iDisplayTimeMS={}",
				path, e.description(), bStayOnScreen, iDisplayTimeMS);
		}
	}

	static constexpr std::uint32_t kDefaultDisplayTimeMS{ 3000 };
	static constexpr std::uint32_t kMinDisplayTimeMS{ 250 };
	static constexpr std::uint32_t kMaxDisplayTimeMS{ 60000 };

	// false = (default) we clear the title ~3s after showing it.
	// true  = keep the title visible until a native banner replaces it.
	// Neither setting occupies or delays the game's notification queue.
	inline static bool bStayOnScreen{ false };

	// How long the popup stays up, in milliseconds, before we clear it.
	// Ignored entirely when bStayOnScreen is true.
	inline static std::uint32_t iDisplayTimeMS{ kDefaultDisplayTimeMS };

private:
	static void ClampDisplayTime()
	{
		if (iDisplayTimeMS < kMinDisplayTimeMS || iDisplayTimeMS > kMaxDisplayTimeMS) {
			const auto requested = iDisplayTimeMS;
			iDisplayTimeMS = std::clamp(iDisplayTimeMS, kMinDisplayTimeMS, kMaxDisplayTimeMS);
			logger::warn("iDisplayTimeMS={} is outside [{}, {}]; clamped to {}",
				requested, kMinDisplayTimeMS, kMaxDisplayTimeMS, iDisplayTimeMS);
		}
	}
};
