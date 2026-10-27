//////////////////////////////////////////////////////////////////////////////
// Filename	: GCNicknameList.h 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_NICKNAME_LIST_H__
#define __GC_NICKNAME_LIST_H__

#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

#include "NicknameInfo.h"

#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////////
// class GCNicknameList;
//////////////////////////////////////////////////////////////////////////////

#define MAX_NICKNAME_NUM	500

class GCNicknameList : public Packet 
{
public:
	GCNicknameList();
	~GCNicknameList();
	// It owns its entries, so a copy would free them twice.
	GCNicknameList( const GCNicknameList& ) = delete;
	GCNicknameList& operator = ( const GCNicknameList& ) = delete;
	
public:
	void read(SocketInputStream & iStream);
	void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_GC_NICKNAME_LIST; }
	PacketSize_t getPacketSize() const;
#ifdef __DEBUG_OUTPUT__
	string getPacketName() const { return "GCNicknameList"; }
	string toString() const;
#endif

public:
	std::vector<NicknameInfo*>& getAddresses() { return m_Nicknames; }

private :
	std::vector<NicknameInfo*>	m_Nicknames;
};

//////////////////////////////////////////////////////////////////////////////
// class GCNicknameListFactory;
//////////////////////////////////////////////////////////////////////////////

class GCNicknameListFactory : public PacketFactory 
{
public :
	GCNicknameListFactory() {}
	virtual ~GCNicknameListFactory() {}
	
public:
	Packet* createPacket() { return new GCNicknameList(); }
#ifdef __DEBUG_OUTPUT__
	string getPacketName() const { return "GCNicknameList"; }
#endif
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_NICKNAME_LIST; }
	PacketSize_t getPacketMaxSize() const { return szBYTE + NicknameInfo::getMaxSize() * MAX_NICKNAME_NUM; }
};

//////////////////////////////////////////////////////////////////////////////
// class GCNicknameListHandler;
//////////////////////////////////////////////////////////////////////////////

class GCNicknameListHandler 
{
public:
	static void execute(GCNicknameList* pGCNicknameList, Player* pPlayer);

};

#endif
