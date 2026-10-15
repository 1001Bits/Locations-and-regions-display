#include "Notifications.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace
{
	struct Source
	{
		using value_type = std::string;
		std::optional<std::string> optionalValue;
	};
	int checks{}, failures{};
	void Expect(bool a_result, const char* a_case)
	{
		++checks;
		if (!a_result) {
			++failures;
			std::cerr << a_case << '\n';
		}
	}

	struct HUD
	{
		Source native;
		std::vector<std::optional<std::string>> events;
		Notifications::PopupState state;
		Notifications::Context context{ 0x1234, 1 };
		bool queued{};

		bool Publish(const std::string* a_text)
		{
			return Notifications::PublishIfIdle(native, a_text, [this](Source& a_source) {
				Expect(&a_source != &native, "custom display must never use the native source");
				events.push_back(a_source.optionalValue);
			}, queued);
		}
		Notifications::Ticket Show(const std::string& a_text)
		{
			return Publish(&a_text) ? state.Start(context) : Notifications::Ticket{};
		}
		void Clear(Notifications::Ticket a_ticket)
		{
			if (state.Expire(a_ticket, context)) Publish(nullptr);
		}
		// Model the engine's occupancy gate: a native vignette can start only
		// with a free slot. Reproduces the reason 1.2.2 delayed or blocked it.
		bool StartNative(const std::string& a_text)
		{
			if (native.optionalValue) return false;
			native.optionalValue = a_text;
			events.emplace_back(a_text);
			return true;
		}
	};
}

int main()
{
	for (const auto* banner : { "Quest started", "Level up", "Location discovered / XP" }) {
		for (int duration : { 1000, 5000, 10000, 20000, 60000, -1 }) {
			HUD hud;
			const auto ticket = hud.Show("Sanctuary Hills");
			Expect(!hud.native.optionalValue, "display must leave the native queue free");
			Expect(hud.StartNative(banner), "native banner must start before the custom deadline, including stay-on-screen");
			if (duration > 0) hud.Clear(ticket);
			Expect(hud.native.optionalValue == banner, "custom timer must preserve the native banner");
			Expect(hud.events.back() == banner, "custom clear must not hide the native banner");
			const auto before = hud.events.size();
			hud.Show("Concord");
			Expect(hud.events.size() == before, "new location must not overwrite an active native banner");
		}
	}
	{
		HUD hud;
		const auto first = hud.Show("Same location");
		const auto second = hud.Show("Same location");
		hud.Clear(first);  // old task was already in the UI queue when re-armed
		Expect(hud.events.size() == 2, "an old task must not clear a newer identical title");
		hud.Clear(second);
		Expect(!hud.events.back().has_value(), "fade must publish an absent value, not an active empty string");
		Expect(!hud.native.optionalValue, "fade must leave native occupancy unchanged");
		const auto count = hud.events.size();
		hud.Clear(second);
		Expect(hud.events.size() == count, "duplicate fade execution must do nothing");
	}
	for (bool reuseAddress : { false, true }) {
		HUD hud;
		const auto beforeLoad = hud.Show("Old save");
		if (!reuseAddress) hud.context.hudData = 0x5678;
		++hud.context.epoch;
		hud.Clear(beforeLoad);
		Expect(hud.events.size() == 1, "load must invalidate the old fade even when the HUD address is reused");
		const auto afterLoad = hud.Show("New save");
		hud.Clear(beforeLoad);
		Expect(hud.events.back() == "New save", "old save task must not hide the new save's title");
		hud.Clear(afterLoad);
		Expect(!hud.events.back(), "new save's own fade should still work");
	}
	{
		HUD hud;
		hud.queued = true;
		hud.Show("Arrival during queued quest");
		Expect(hud.events.empty(), "a queued native vignette must take priority before it starts");
		hud.queued = false;
		const auto ticket = hud.Show("Location");
		hud.queued = true;
		hud.Clear(ticket);
		Expect(hud.events.size() == 1, "fade must not interfere with a pending native vignette");
	}
	{
		HUD hud;
		const auto ticket = hud.Show("Location");
		hud.StartNative("Quest completed");
		hud.native.optionalValue.reset();
		hud.events.emplace_back(std::nullopt);
		hud.Clear(ticket);
		Expect(!hud.events.back() && !hud.native.optionalValue, "late fade after native completion must leave the HUD empty");
	}
	std::cout << checks << " checks, " << failures << " failures\n";
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
