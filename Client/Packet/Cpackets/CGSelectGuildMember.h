//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGSelectGuildMember.h 
// Written By  :
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_SELECT_GUILD_MEMBER_H__
#define __CG_SELECT_GUILD_MEMBER_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class CGSelectGuildMember;
//
//////////////////////////////////////////////////////////////////////

class CGSelectGuildMember : public Packet
{
public:
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read(SocketInputStream & iStream);
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_CG_SELECT_GUILD_MEMBER; }
	
	// get packet's body size
	PacketSize_t getPacketSize() const { return static_cast<PacketSize_t>(szGuildID + szBYTE + m_Name.size()); }

#ifdef __DEBUG_OUTPUT__
	// get packet name
	std::string getPacketName() const { return "CGSelectGuildMember"; }

	// get packet's debug std::string
	std::string toString() const;
#endif
	// get/set GuildID
	GuildID_t getGuildID() const noexcept { return m_GuildID; }
	void setGuildID( GuildID_t GuildID ) noexcept { m_GuildID = GuildID; }

	// get/set name
	const std::string& getName() const noexcept { return m_Name; }
	void setName( const std::string& name ) { m_Name = name; }


private :

	// Guild ID
	GuildID_t m_GuildID;

	// name
	std::string m_Name;
	
};


//////////////////////////////////////////////////////////////////////
//
// class CGSelectGuildMemberFactory;
//
// Factory for CGSelectGuildMember
//
//////////////////////////////////////////////////////////////////////

class CGSelectGuildMemberFactory : public PacketFactory {

public:
	
	// constructor
	CGSelectGuildMemberFactory() {}
	
	// destructor
	virtual ~CGSelectGuildMemberFactory() {}

	
public:
	
	// create packet
	Packet* createPacket() { return new CGSelectGuildMember(); }

	// get packet name
	std::string getPacketName() const { return "CGSelectGuildMember"; }
	
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_SELECT_GUILD_MEMBER; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szGuildID + szBYTE + 20; }
};

#endif
