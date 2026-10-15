//////////////////////////////////////////////////////////////////////////////
// Filename    : PacketDispatcher.cpp
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "PacketDispatcher.h"

PacketDispatcher::HandlerFn PacketDispatcher::s_Handlers[Packet::PACKET_MAX] = {0};

void PacketDispatcher::registerHandler(PacketID_t packetID, HandlerFn fn)
{
	// Real checks, not Assert: Assert compiles away under NDEBUG, and an
	// out-of-range table write or a silently overwritten handler must
	// not survive into a Release build. Registration runs only at
	// startup, so throwing here fails fast at the composition root.
	if (packetID >= Packet::PACKET_MAX)
		throw Error("PacketDispatcher::registerHandler: packet id out of range");
	if (fn == 0)
		throw Error("PacketDispatcher::registerHandler: null handler");
	if (s_Handlers[packetID] != 0)
		throw Error("PacketDispatcher::registerHandler: id already registered");
	s_Handlers[packetID] = fn;
}

void PacketDispatcher::dispatch(Packet* pPacket, Player* pPlayer)
{
	PacketID_t packetID = pPacket->getPacketID();
	if (packetID >= Packet::PACKET_MAX)
		throw InvalidProtocolException("packet id out of range");

	HandlerFn fn = s_Handlers[packetID];
	if (fn == 0)
		throw InvalidProtocolException("packet has no registered handler");

	fn(pPacket, pPlayer);
}

PacketDispatcher::HandlerFn PacketDispatcher::handlerFor(PacketID_t packetID)
{
	return packetID < Packet::PACKET_MAX ? s_Handlers[packetID] : 0;
}

void PacketDispatcher::clearForTests()
{
	for (PacketID_t id = 0; id < Packet::PACKET_MAX; ++id)
		s_Handlers[id] = 0;
}
