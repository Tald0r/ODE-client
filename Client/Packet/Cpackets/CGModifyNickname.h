//////////////////////////////////////////////////////////////////////////////
// Filename    : CGModifyNickname.h 
// Written By  : reiot@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __CG_MODIFY_NICKNAME_H__
#define __CG_MODIFY_NICKNAME_H__

#include "Packet.h"
#include "PacketFactory.h"

#include "NicknameInfo.h"

//////////////////////////////////////////////////////////////////////////////
// class CGModifyNickname;
//////////////////////////////////////////////////////////////////////////////

class CGModifyNickname : public Packet 
{
public:
	CGModifyNickname();
	~CGModifyNickname();

public:
    void read(SocketInputStream & iStream);
    void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_CG_MODIFY_NICKNAME; }
	PacketSize_t getPacketSize() const { return static_cast<PacketSize_t>(szObjectID + szBYTE + m_Nickname.size()); }
#ifdef __DEBUG_OUTPUT__
	string getPacketName() const { return "CGModifyNickname"; }
	string toString() const;
#endif
	
public:
	ObjectID_t	getNicknameID() const { return m_NicknameID; }
	void		setNicknameID(ObjectID_t id) { m_NicknameID = id; }

	string	getNickname() const { return m_Nickname; }
	void	setNickname(const string& name) { m_Nickname = name; }
private:
	ObjectID_t	m_NicknameID;
	string		m_Nickname;
};

//////////////////////////////////////////////////////////////////////////////
// class CGModifyNicknameFactory;
//////////////////////////////////////////////////////////////////////////////
class CGModifyNicknameFactory : public PacketFactory 
{
public:
	Packet* createPacket() { return new CGModifyNickname(); }
	string getPacketName() const { return "CGModifyNickname"; }
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_MODIFY_NICKNAME; }
	PacketSize_t getPacketMaxSize() const noexcept { return szObjectID + szBYTE + MAX_NICKNAME_SIZE; }
};

#endif
