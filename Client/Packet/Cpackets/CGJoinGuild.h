//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGJoinGuild.h 
// Written By  :
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_JOIN_GUILD_H__
#define __CG_JOIN_GUILD_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class CGJoinGuild;
//
//////////////////////////////////////////////////////////////////////

class CGJoinGuild : public Packet
{
public:
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read(SocketInputStream & iStream);
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_CG_JOIN_GUILD; }
	
	// get packet's body size
	PacketSize_t getPacketSize() const { return static_cast<PacketSize_t>(szGuildID + szGuildMemberRank + szBYTE + m_GuildMemberIntro.size()); }

#ifdef __DEBUG_OUTPUT__
	// get packet name
	std::string getPacketName() const { return "CGJoinGuild"; }

	// get packet's debug std::string
	std::string toString() const;
#endif

	// get/set Guild ID
	GuildID_t getGuildID() const noexcept { return m_GuildID; }
	void setGuildID( GuildID_t GuildID ) noexcept { m_GuildID = GuildID; }

	// get/set Guild Member Rank
	GuildMemberRank_t getGuildMemberRank() const noexcept { return m_GuildMemberRank; }
	void setGuildMemberRank( GuildMemberRank_t GuildMemberRank ) noexcept { m_GuildMemberRank = GuildMemberRank; }

	// get/set Guild Member Intro
	std::string getGuildMemberIntro() const { return m_GuildMemberIntro; }
	void setGuildMemberIntro( const std::string& GuildMemberIntro ) { m_GuildMemberIntro = GuildMemberIntro; }


private :

	// Guild ID
	GuildID_t m_GuildID;

	// Guild Member Rank
	GuildMemberRank_t m_GuildMemberRank;

	// Guild Member Intro
	std::string m_GuildMemberIntro;
	
};

//////////////////////////////////////////////////////////////////////
//
// class CGJoinGuildFactory;
//
// Factory for CGJoinGuild
//
//////////////////////////////////////////////////////////////////////

class CGJoinGuildFactory : public PacketFactory {

public:
	
	// constructor
	CGJoinGuildFactory() {}
	
	// destructor
	virtual ~CGJoinGuildFactory() {}

	
public:
	
	// create packet
	Packet* createPacket() { return new CGJoinGuild(); }

	// get packet name
	std::string getPacketName() const { return "CGJoinGuild"; }
	
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_JOIN_GUILD; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szGuildID + szGuildMemberRank + szBYTE + 256; }
};

#endif
