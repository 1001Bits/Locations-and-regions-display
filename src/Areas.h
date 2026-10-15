#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// Use the game's localized FULL names. Display text is never an English lookup
// key, and an unknown location must never imply that we are in the Commonwealth.
namespace Areas
{
	namespace detail
	{
		// Fallout4.esm incorrectly parents USSRiptideLocation to Concord.
		// Correct the display traversal only, using stable form IDs so every
		// language keeps its localized names. Respect mods that change this link.
		template <class T>
		[[nodiscard]] const T* Parent(const T* a_location)
		{
			const auto* parent = a_location->parentLoc;
			if (a_location->formID == 0x0002FF7B && parent && parent->formID == 0x00017E00 &&
				parent->parentLoc && parent->parentLoc->formID == 0x00002CF0) {
				return parent->parentLoc;
			}
			return parent;
		}

		template <class T>
		[[nodiscard]] const char* Name(const T* a_form)
		{
			const auto* name = a_form ? a_form->fullName.c_str() : nullptr;
			return name && *name ? name : nullptr;
		}

		template <class T>
		struct Chain
		{
			std::array<const T*, 64> nodes{};
			std::size_t size{ 0 };

			[[nodiscard]] bool Contains(const T* a_node) const
			{
				for (std::size_t i = 0; i < size; ++i) {
					if (nodes[i] == a_node) return true;
				}
				return false;
			}
		};

		template <class T, class Parent>
		[[nodiscard]] Chain<T> Parents(const T* a_node, Parent a_parent)
		{
			Chain<T> chain;
			for (; a_node; a_node = a_parent(a_node)) {
				// Broken/modded hierarchies must not hang the HUD thread.
				if (chain.size == chain.nodes.size() || chain.Contains(a_node)) return {};
				chain.nodes[chain.size++] = a_node;
			}
			return chain;
		}
	}

	// Templates keep the selection policy independent of the engine so the same
	// code can be exercised with localized location/worldspace fixtures.
	template <class Location, class World>
	[[nodiscard]] const char* GetAreaForLocation(
		const char* a_title, const Location* a_location, const World* a_world)
	{
		if (!a_title || !*a_title) return nullptr;
		const std::string_view title{ a_title };
		const auto locations = detail::Parents(a_location, [](const Location* a_loc) { return detail::Parent(a_loc); });
		const auto worlds = detail::Parents(a_world, [](const World* a_ws) { return a_ws->parentWorld; });

		const char* worldName = nullptr;
		const Location* worldLocation = nullptr;
		for (std::size_t i = 0; i < worlds.size; ++i) {
			if (!worldName) worldName = detail::Name(worlds.nodes[i]);
			if (!worldLocation) worldLocation = worlds.nodes[i]->location;
		}
		if (!worldName) worldName = detail::Name(worldLocation);
		if (worldName && title == worldName) return nullptr;

		// The renderer can announce a nearby marker before the player's current
		// location changes. Only use a specific parent when its child really is
		// the displayed place, and don't carry an old location across worldspaces.
		if (!a_world || (worldLocation && locations.Contains(worldLocation))) {
			bool foundTitle = false;
			for (std::size_t i = 0; i < locations.size; ++i) {
				if (const auto* name = detail::Name(locations.nodes[i])) {
					if (title == name) {
						foundTitle = true;
					} else if (foundTitle) {
						return name;
					}
				}
			}
		}

		if (worldName) return worldName;
		if (a_world) return nullptr;

		// Interiors have no exterior worldspace. Their highest named location
		// supplies the region when the popup name differs from the location name.
		for (std::size_t i = locations.size; i > 0; --i) {
			if (const auto* name = detail::Name(locations.nodes[i - 1])) {
				return title != name ? name : nullptr;
			}
		}
		return nullptr;
	}
}
