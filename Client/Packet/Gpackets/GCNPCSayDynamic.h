//////////////////////////////////////////////////////////////////////////////
// Filename    : GCNPCSayDynamic.h 
// Written By  : excel96
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_NPC_SAY_DYNAMIC_H__
#define __GC_NPC_SAY_DYNAMIC_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class GCNPCSayDynamic;
// NPC 의 대사를 주변의 PC 들에게 전송한다.
//////////////////////////////////////////////////////////////////////////////

class GCNPCSayDynamic : public Packet 
{
public:
    void read ( SocketInputStream & iStream );
    void write ( SocketOutputStream & oStream ) const;
	PacketID_t getPacketID () const noexcept { return PACKET_GC_NPC_SAY_DYNAMIC; }
	PacketSize_t getPacketSize () const { return static_cast<PacketSize_t>(szObjectID + szBYTE + m_Message.size()); }

	#ifdef __DEBUG_OUTPUT__
		std::string getPacketName () const { return "GCNPCSayDynamic"; }
		std::string toString () const;
	#endif

	ObjectID_t getObjectID () const noexcept { return m_ObjectID; }
	void setObjectID ( const ObjectID_t & creatureID ) { m_ObjectID = creatureID; }

	std::string getMessage () const { return m_Message; }
	void setMessage ( const std::string & msg ) { m_Message = msg; }

private:
	ObjectID_t m_ObjectID; // NPC's object id
	std::string     m_Message;  // chatting message
	
};


//////////////////////////////////////////////////////////////////////////////
// class GCNPCSayDynamicFactory;
//////////////////////////////////////////////////////////////////////////////


class GCNPCSayDynamicFactory : public PacketFactory 
{
public:
	Packet * createPacket () { return new GCNPCSayDynamic(); }

	#ifdef __DEBUG_OUTPUT__
		std::string getPacketName () const { return "GCNPCSayDynamic"; }
	#endif

	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_NPC_SAY_DYNAMIC; }
	PacketSize_t getPacketMaxSize () const noexcept { return szObjectID + szBYTE + 2048 ; }
};


//////////////////////////////////////////////////////////////////////////////
// class GCNPCSayDynamicHandler;
//////////////////////////////////////////////////////////////////////////////

class GCNPCSayDynamicHandler 
{
public:
	static void execute ( GCNPCSayDynamic * pPacket , Player * pPlayer );

};

#endif
