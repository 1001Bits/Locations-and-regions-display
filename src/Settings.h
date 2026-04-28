#pragma once

#include <toml++/toml.h>

struct Settings
{
	static void Load()
	{
		constexpr auto path = "Data/F4SE/Plugins/DynamicLocationPopupsF4.toml"sv;

		try {
			const auto tbl = toml::parse_file(path);
			iMode = tbl["Settings"]["iMode"].value_or<std::uint32_t>(0);
			logger::info("Settings loaded from {}: iMode={}", path, iMode);
		} catch (const toml::parse_error& e) {
			logger::warn("Failed to parse {} ({}); using defaults: iMode={}", path, e.description(), iMode);
		}
	}

	// 0 = pop on every entry that differs from the current location.
	// 1 = pop only when the new location differs from BOTH current and last
	//     (suppresses ping-pong spam when crossing a single boundary back and forth).
	inline static std::uint32_t iMode{ 0 };
};
