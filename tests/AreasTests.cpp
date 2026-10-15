#include "Areas.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	struct Location
	{
		std::string fullName;
		const Location* parentLoc{ nullptr };
		std::uint32_t formID{};
	};

	struct World
	{
		std::string fullName;
		const World* parentWorld{ nullptr };
		const Location* location{ nullptr };
	};

	int failures = 0;
	int checks = 0;

	void Expect(const char* a_case, const char* a_title, const Location* a_location,
		const World* a_world, std::string_view a_expected)
	{
		++checks;
		const auto* result = Areas::GetAreaForLocation(a_title, a_location, a_world);
		const std::string_view actual = result ? result : "";
		if (actual != a_expected) {
			std::cerr << a_case << ": expected '" << a_expected << "', got '" << actual << "'\n";
			++failures;
		}
	}
}

int main()
{
	const Location cw{ "Содружество" };
	const Location island{ "Остров" };
	const Location nuka{ "Ядер-Мир" };
	const Location sanctuary{ "Сэнкчуари", &cw };
	const Location acadia{ "Акадия", &island };
	const Location safari{ "Сафари", &nuka };
	const World cwWorld{ "Содружество", nullptr, &cw };
	const World islandWorld{ "Остров", nullptr, &island };
	const World nukaWorld{ "Ядер-Мир", nullptr, &nuka };

	Expect("localized Commonwealth", "Сэнкчуари", &sanctuary, &cwWorld, "Содружество");
	Expect("localized Far Harbor", "Акадия", &acadia, &islandWorld, "Остров");
	Expect("localized Nuka-World", "Сафари", &safari, &nukaWorld, "Ядер-Мир");
	Expect("travel with old location", "Акадия", &sanctuary, &islandWorld, "Остров");
	Expect("stale matching location", "Сэнкчуари", &sanctuary, &nukaWorld, "Ядер-Мир");
	Expect("missing DLC location", "Неизвестное место", nullptr, &nukaWorld, "Ядер-Мир");
	Expect("Commonwealth interior", "Сэнкчуари", &sanctuary, nullptr, "Содружество");
	Expect("Far Harbor interior", "Акадия", &acadia, nullptr, "Остров");
	Expect("Nuka-World interior", "Сафари", &safari, nullptr, "Ядер-Мир");

	const Location building{ "Дом приматов", &safari };
	Expect("nested interior", "Дом приматов", &building, nullptr, "Сафари");
	Expect("nearby marker keeps world context", "Новое место", &building, &nukaWorld, "Ядер-Мир");
	Expect("different interior marker name", "Вход", &building, nullptr, "Ядер-Мир");
	Expect("regional heading", "Ядер-Мир", &safari, &nukaWorld, "");
	Expect("root heading in interior", "Остров", &island, nullptr, "");

	const Location unnamed{ "", &island };
	const Location cellar{ "Подвал", &unnamed };
	Expect("unnamed parent", "Подвал", &cellar, nullptr, "Остров");
	const Location duplicate{ "Акадия", &acadia };
	Expect("duplicate parent name", "Акадия", &duplicate, nullptr, "Остров");

	const Location customRegion{ "Nouvelle région" };
	const Location customTown{ "Ville ajoutée", &customRegion };
	const World customWorld{ "Nouvelle région", nullptr, &customRegion };
	Expect("translated custom world", "Ville ajoutée", &customTown, &customWorld, "Nouvelle région");
	const World unnamedWorld{ "", &customWorld };
	Expect("unnamed child world", "Lieu", nullptr, &unnamedWorld, "Nouvelle région");
	const World locationNamedWorld{ "", nullptr, &island };
	Expect("world named by its location", "Акадия", &sanctuary, &locationNamedWorld, "Остров");
	const World unanchoredWorld{ "Nouvelle région" };
	Expect("world without location link", "Сэнкчуари", &sanctuary, &unanchoredWorld, "Nouvelle région");
	const Location enCW{ "Commonwealth" };
	const Location transit{ "Nuka-World Transit Center", &enCW };
	const World enWorld{ "Commonwealth", nullptr, &enCW };
	Expect("DLC-owned place in Commonwealth", "Nuka-World Transit Center", &transit, &enWorld, "Commonwealth");
	const Location commonwealth{ "Содружество", nullptr, 0x00002CF0 };
	const Location concord{ "Конкорд", &commonwealth, 0x00017E00 };
	const Location riptide{ "Остов корабля «Риптайд»", &concord, 0x0002FF7B };
	const World riptideWorld{ "Содружество", nullptr, &commonwealth };
	Expect("Riptide wrong vanilla parent", "Остов корабля «Риптайд»", &riptide, &riptideWorld, "Содружество");
	Expect("Riptide without world context", "Остов корабля «Риптайд»", &riptide, nullptr, "Содружество");
	const Location museum{ "Музей Свободы", &concord, 0x0001D807 };
	Expect("valid Concord child", "Музей Свободы", &museum, &riptideWorld, "Конкорд");
	const Location correctedParent{ "Новый район", &commonwealth, 0x01000800 };
	const Location patchedRiptide{ "Остов корабля «Риптайд»", &correctedParent, 0x0002FF7B };
	Expect("respect patched Riptide parent", "Остов корабля «Риптайд»", &patchedRiptide, &riptideWorld, "Новый район");
	const Location modRiptide{ "Остов корабля «Риптайд»", &concord, 0x0102FF7B };
	Expect("do not match a mod's local form ID", "Остов корабля «Риптайд»", &modRiptide, &riptideWorld, "Конкорд");

	Expect("no context", "Unknown", nullptr, nullptr, "");
	Expect("no title", nullptr, &acadia, &islandWorld, "");
	Expect("empty title", "", &acadia, &islandWorld, "");
	const World emptyWorld{};
	Expect("unnamed context", "Unknown", nullptr, &emptyWorld, "");
	Expect("unknown exterior does not reuse old region", "Сэнкчуари", &sanctuary, &emptyWorld, "");

	Location cycle{ "Cycle" };
	cycle.parentLoc = &cycle;
	Expect("location cycle with world", "Cycle", &cycle, &islandWorld, "Остров");
	Expect("location cycle without world", "Cycle", &cycle, nullptr, "");
	World worldCycle{};
	worldCycle.parentWorld = &worldCycle;
	Expect("world cycle", "Unknown", nullptr, &worldCycle, "");
	std::array<Location, 65> deepChain{};
	for (std::size_t i = 1; i < deepChain.size(); ++i) {
		deepChain[i - 1].parentLoc = &deepChain[i];
	}
	Expect("excessive parent depth", "Unknown", &deepChain.front(), &islandWorld, "Остров");

	std::cout << checks << " checks, " << failures << " failures\n";
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
