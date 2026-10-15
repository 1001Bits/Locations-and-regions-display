#pragma once

#include "DistrictData.h"

#include <cstddef>
#include <string_view>

// Commonwealth district subtitles from official game strings, keyed by the map marker that
// raised the popup. Kept engine-independent so the lookup can be exercised in tests.
namespace Districts
{
	inline constexpr int kUnknownLanguage = -1;

	// A popup is raised from inside the marker's radius; a same-named marker farther away
	// than this is a different (modded) place.
	inline constexpr float kMaxMarkerDistance = 20000.0f;

	// The official language whose names match every reference location, or kUnknownLanguage
	// (unofficial translation or renamed locations). a_nameOf(formID) returns the loaded name.
	template <class NameOf>
	[[nodiscard]] int DetectLanguage(NameOf a_nameOf)
	{
		for (std::size_t lang = 0; lang < data::kLanguageCount; ++lang) {
			bool matches = true;
			for (const auto& fingerprint : data::kFingerprints) {
				const char* name = a_nameOf(fingerprint.formID);
				if (!name || std::string_view{ name } != fingerprint.names[lang]) {
					matches = false;
					break;
				}
			}
			if (matches) return static_cast<int>(lang);
		}
		return kUnknownLanguage;
	}

	// District for a popup title shown at (a_x, a_y) in the Commonwealth worldspace. Names
	// shared by several markers resolve to the nearest; a district equal to the title is skipped.
	[[nodiscard]] inline const char* Find(int a_language, std::string_view a_title, float a_x, float a_y)
	{
		if (a_language < 0 || a_language >= static_cast<int>(data::kLanguageCount) || a_title.empty()) return nullptr;
		const auto lang = static_cast<std::size_t>(a_language);
		const data::Marker* nearest = nullptr;
		float nearestDistance = kMaxMarkerDistance * kMaxMarkerDistance;
		for (const auto& marker : data::kMarkers) {
			const char* name = marker.names[lang];
			if (!name || a_title != name) continue;
			const float dx = marker.x - a_x;
			const float dy = marker.y - a_y;
			if (const float distance = dx * dx + dy * dy; distance <= nearestDistance) {
				nearest = &marker;
				nearestDistance = distance;
			}
		}
		if (!nearest || nearest->district < 0) return nullptr;
		const char* district = data::kDistricts[static_cast<std::size_t>(nearest->district)][lang];
		return a_title != district ? district : nullptr;
	}
}
