#pragma once
#include "PacketDispatcher.h"

// Forgets every registration in the process-wide dispatch table, so a test
// can register the whole table (PacketHandlerRegistry) without meeting
// another test's ids. Tests that bind one handler re-bind it when the table
// lacks it (PacketDispatcher::handlerFor), so a clear between tests is safe.
struct PacketDispatcherTestAccess
{
	static void Clear()
	{
		for (PacketID_t id = 0; id < Packet::PACKET_MAX; ++id)
			PacketDispatcher::s_Handlers[id] = 0;
	}

	// registerClientPacketHandlers fills the table only the first time it
	// runs in the process (its own flag, which Clear does not reset), and
	// InitSocket runs it as well as the registry's own tests. Whoever ran
	// it calls this next, on a table it cleared first: the first fill is
	// kept, and a later run, which fills nothing, gets it back - so the
	// table is the registry's whichever test reached it first.
	static void KeepRegistry()
	{
		bool empty = true;
		for (PacketID_t id = 0; id < Packet::PACKET_MAX && empty; ++id)
			empty = PacketDispatcher::s_Handlers[id] == 0;
		for (PacketID_t id = 0; id < Packet::PACKET_MAX; ++id)
		{
			if (empty && s_Kept)
				PacketDispatcher::s_Handlers[id] = s_Registry[id];
			else if (!empty && !s_Kept)
				s_Registry[id] = PacketDispatcher::s_Handlers[id];
		}
		s_Kept = s_Kept || !empty;
	}

	// Binds an id whatever it holds: the handler fuzz targets drop the
	// two handlers that reconnect the socket.
	static void Set(PacketID_t id, PacketDispatcher::HandlerFn fn)
	{
		PacketDispatcher::s_Handlers[id] = fn;
	}

	// Whether a fill has been kept: the registry has run in the process.
	static bool Kept() { return s_Kept; }

	static bool Empty()
	{
		for (PacketID_t id = 0; id < Packet::PACKET_MAX; ++id)
			if (PacketDispatcher::s_Handlers[id] != 0) return false;
		return true;
	}

private:
	static inline bool s_Kept = false;
	static inline PacketDispatcher::HandlerFn s_Registry[Packet::PACKET_MAX] = {};
};
