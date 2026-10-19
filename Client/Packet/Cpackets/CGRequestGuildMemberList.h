//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGRequestGuildMemberList.h 
// Written By  :
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_REQUER_GUILD_MEMBER_LIST_H__
#define __CG_REQUER_GUILD_MEMBER_LIST_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class CGRequestGuildMemberList;
//
//////////////////////////////////////////////////////////////////////

class CGRequestGuildMemberList : public Packet
{
public:
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read(SocketInputStream & iStream);
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_CG_REQUEST_GUILD_MEMBER_LIST; }
	
	// get packet's body size
	PacketSize_t getPacketSize() const noexcept { return szGuildID; }

#ifdef __DEBUG_OUTPUT__
	// get packet name
	std::string getPacketName() const { return "CGRequestGuildMemberList"; }

	// get packet's debug std::string
	std::string toString() const;
#endif

	// get/set GuildID
	GuildID_t getGuildID() const noexcept { return m_GuildID; }
	void setGuildID( GuildID_t GuildID ) noexcept { m_GuildID = GuildID; }


private :

	// Guild ID (the UI's request never sets it)
	GuildID_t m_GuildID = 0;
	
};

//////////////////////////////////////////////////////////////////////
//
// class CGRequestGuildMemberListFactory;
//
// Factory for CGRequestGuildMemberList
//
//////////////////////////////////////////////////////////////////////

class CGRequestGuildMemberListFactory : public PacketFactory {

public:
	
	// constructor
	CGRequestGuildMemberListFactory() {}
	
	// destructor
	virtual ~CGRequestGuildMemberListFactory() {}

	
public:
	
	// create packet
	Packet* createPacket() { return new CGRequestGuildMemberList(); }

	// get packet name
	std::string getPacketName() const { return "CGRequestGuildMemberList"; }
	
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_REQUEST_GUILD_MEMBER_LIST; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szGuildID; }
};

#endif
