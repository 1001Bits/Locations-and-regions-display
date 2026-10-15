#pragma once

#include <cstdint>

namespace Notifications
{
	// The native source is only inspected. A temporary source broadcasts our
	// display event without claiming the slot that drives the game's queues.
	// A null value broadcasts a disengaged optional, not an active blank title.
	// The caller holds the native source's data lock throughout this operation.
	template <class Source, class Broadcast>
	bool PublishIfIdle(const Source& a_native, const typename Source::value_type* a_value, Broadcast a_broadcast,
		bool a_nativeQueued = false)
	{
		if (a_nativeQueued || a_native.optionalValue.has_value()) return false;
		Source transient{};
		if (a_value) transient.optionalValue.emplace(*a_value);
		a_broadcast(transient);
		return true;
	}

	struct Context
	{
		std::uintptr_t hudData{};
		std::uint64_t epoch{};
		bool operator==(const Context&) const = default;
	};

	struct Ticket
	{
		Context context;
		std::uint64_t generation{};
	};

	// Only used on the UI thread. Generations are checked when a queued clear
	// actually executes, including repeated popups with the very same title.
	class PopupState
	{
	public:
		[[nodiscard]] Ticket Start(Context a_context)
		{
			_context = a_context;
			_active = true;
			return { a_context, ++_generation };
		}

		[[nodiscard]] bool Expire(Ticket a_ticket, Context a_current)
		{
			if (!_active || a_ticket.generation != _generation || a_ticket.context != _context) return false;
			_active = false;
			return a_current == _context;
		}

	private:
		Context _context{};
		std::uint64_t _generation{};
		bool _active{};
	};
}
