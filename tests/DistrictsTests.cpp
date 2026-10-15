#include "Districts.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	using Districts::data::kDistricts;
	using Districts::data::kFingerprints;
	using Districts::data::kLanguageCount;
	using Districts::data::kMarkers;

	int checks = 0;
	int failures = 0;

	void Check(bool a_result, const std::string& a_case)
	{
		++checks;
		if (!a_result) {
			++failures;
			std::cerr << "FAIL: " << a_case << '\n';
		}
	}

	int Language(std::string_view a_code)
	{
		for (std::size_t i = 0; i < kLanguageCount; ++i) {
			if (a_code == Districts::data::kLanguages[i]) return static_cast<int>(i);
		}
		return Districts::kUnknownLanguage;
	}

	std::vector<const Districts::data::Marker*> MarkersNamed(std::string_view a_english)
	{
		std::vector<const Districts::data::Marker*> result;
		for (const auto& marker : kMarkers) {
			if (a_english == marker.names[0]) result.push_back(&marker);
		}
		return result;
	}

	// The district shown for a marker's title, standing on that marker.
	std::string At(std::string_view a_english, std::string_view a_language, std::size_t a_which = 0)
	{
		const auto markers = MarkersNamed(a_english);
		if (a_which >= markers.size()) return "<no marker>";
		const auto lang = Language(a_language);
		const auto* marker = markers[a_which];
		const char* district = Districts::Find(lang, marker->names[static_cast<std::size_t>(lang)], marker->x, marker->y);
		return district ? district : "";
	}

	void Expect(std::string_view a_english, std::string_view a_language, std::string_view a_expected)
	{
		const auto result = At(a_english, a_language);
		Check(result == a_expected, std::string(a_english) + " [" + std::string(a_language) + "] -> '" + result +
			"', expected '" + std::string(a_expected) + "'");
	}

	void LanguageDetection()
	{
		for (std::size_t lang = 0; lang < kLanguageCount; ++lang) {
			const int detected = Districts::DetectLanguage([lang](std::uint32_t a_formID) -> const char* {
				for (const auto& fingerprint : kFingerprints) {
					if (fingerprint.formID == a_formID) return fingerprint.names[lang];
				}
				return nullptr;
			});
			Check(detected == static_cast<int>(lang), std::string("detects ") + Districts::data::kLanguages[lang]);
		}
		Check(Districts::DetectLanguage([](std::uint32_t) -> const char* { return "Unofficial translation"; }) ==
			Districts::kUnknownLanguage, "unofficial strings are not guessed");
		Check(Districts::DetectLanguage([](std::uint32_t) -> const char* { return nullptr; }) ==
			Districts::kUnknownLanguage, "missing reference locations are not guessed");

		std::set<std::vector<std::string>> tuples;
		for (std::size_t lang = 0; lang < kLanguageCount; ++lang) {
			std::vector<std::string> tuple;
			for (const auto& fingerprint : kFingerprints) tuple.emplace_back(fingerprint.names[lang]);
			tuples.insert(tuple);
		}
		Check(tuples.size() == kLanguageCount, "every language has a distinct fingerprint");
	}

	void OfficialNames()
	{
		Expect("Boston Public Library", "en", "Back Bay");
		Expect("Boston Public Library", "ru", "Бэк-бэй");
		Expect("Boston Public Library", "pl", "Dzielnica Back Bay");
		Expect("Mass Fusion Building", "en", "Financial District");
		Expect("Mass Fusion Building", "de", "Finanzbezirk");
		Expect("Mass Fusion Building", "fr", "Quartier des affaires");
		Expect("Custom House Tower", "en", "Waterfront");
		Expect("Custom House Tower", "de", "Hafenviertel");
		Expect("Crater of Atom", "ja", "輝きの海");
		Expect("Crater of Atom", "es", "Mar Resplandeciente");
		Expect("Navy Yard", "en", "Charlestown");
		Expect("Massachusetts State House", "en", "Beacon Hill");
	}

	void WikiTowns()
	{
		Expect("Malden Center", "en", "Malden");
		Expect("Malden Center", "ru", "Мальден");
		Expect("Nahant Wharf", "cn", "奈罕");
		Expect("East Boston Preparatory School", "de", "Ost-Boston");
		Expect("East Boston Preparatory School", "pl", "Wschodni Boston");
		Expect("Revere Beach Station", "ja", "リビア");
		Expect("Quincy Quarries", "it", "Rovine di Quincy");
		Expect("Wreck of the USS Riptide", "en", "The Fens");
		Expect("Drumlin Diner", "en", "");  // the wiki placed it in East Boston; it is near Concord
	}

	void Lookups()
	{
		const auto en = Language("en");
		Expect("Boston Common", "en", "");  // district equals the title
		Expect("General Atomics Galleria", "en", "");
		Expect("Sanctuary", "en", "");      // no district: falls back to the location hierarchy
		Check(!Districts::Find(en, "Not a marker", 0.0f, 0.0f), "unknown title");
		const auto library = MarkersNamed("Boston Public Library");
		Check(library.size() == 1, "one Boston Public Library marker");
		if (!library.empty()) {
			const auto* m = library.front();
			Check(!Districts::Find(Districts::kUnknownLanguage, m->names[0], m->x, m->y), "unknown language shows no district");
			Check(!Districts::Find(en, m->names[0], m->x + 100000.0f, m->y), "a far-away same-named marker is another place");
			Check(Districts::Find(en, m->names[0], m->x + 1500.0f, m->y - 1500.0f) != nullptr, "inside the marker radius");
		}

		// The USS Constitution has a marker at its Charlestown berth and where it lands.
		std::set<std::string> constitution;
		for (std::size_t i = 0; i < MarkersNamed("USS Constitution").size(); ++i) {
			constitution.insert(At("USS Constitution", "en", i));
		}
		Check(constitution == std::set<std::string>{ "Charlestown", "Financial District" },
			"USS Constitution resolves to the nearest of its markers");

		// A cp1252 apostrophe the English strings really contain.
		Expect("O\222Neill Family Manufacturing", "en", "Glowing Sea");
	}

	void DataIntegrity()
	{
		for (const auto& district : kDistricts) {
			for (const char* name : district) Check(name && *name, "district names are present in every language");
		}
		for (const auto& marker : kMarkers) {
			Check(marker.names[0] && *marker.names[0], "markers have an English name");
			Check(marker.district >= -1 && marker.district < static_cast<int>(kDistricts.size()), "district index in range");
			if (marker.district < 0) continue;
			for (std::size_t lang = 0; lang < kLanguageCount; ++lang) {
				const char* title = marker.names[lang];
				if (!title) continue;
				const char* district = Districts::Find(static_cast<int>(lang), title, marker.x, marker.y);
				const char* expected = kDistricts[static_cast<std::size_t>(marker.district)][lang];
				const bool same = std::string_view{ title } == expected;
				Check(same ? district == nullptr : (district && std::string_view{ district } == expected),
					std::string("round trip ") + marker.names[0] + " [" + Districts::data::kLanguages[lang] + "]");
			}
		}
	}
}

int main()
{
	LanguageDetection();
	OfficialNames();
	WikiTowns();
	Lookups();
	DataIntegrity();
	std::cout << checks << " checks, " << failures << " failures\n";
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
