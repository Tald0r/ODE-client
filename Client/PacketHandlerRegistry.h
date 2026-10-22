//////////////////////////////////////////////////////////////////////////////
// Filename    : PacketHandlerRegistry.h
// Description : the game's packet-handler composition root (gamemodel's
//               since task 4.218)
//               (docs/reviews/RESTRUCTURING.md task 2.2). Binds packet ids to
//               their handlers on the PacketDispatcher table; called
//               from InitSocket() (Client/GameInit.cpp) beside the
//               PacketFactoryManager creation, before the connection
//               that login attempt opens. InitSocket runs on every
//               login attempt, so the registration itself is guarded
//               to run once per process.
//////////////////////////////////////////////////////////////////////////////

#ifndef __PACKET_HANDLER_REGISTRY_H__
#define __PACKET_HANDLER_REGISTRY_H__

// Idempotent: registration runs once; later calls are no-ops.
void registerClientPacketHandlers();

#endif
