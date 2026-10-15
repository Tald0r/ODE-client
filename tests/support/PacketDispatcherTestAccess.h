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
};
