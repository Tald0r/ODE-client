//////////////////////////////////////////////////////////////////////////////
// Filename    : GCPetStashList.h 
// Written By  : 김성민
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_PET_STASH_LIST_H__
#define __GC_PET_STASH_LIST_H__

#include "Packet.h"
#include "PacketFactory.h"
#include "PetInfo.h"
#include <numeric>
#include <vector>

#define MAX_PET_STASH	20

struct PetStashItemInfo
{
	PetInfo* pPetInfo;
	DWORD KeepDays;

	PacketSize_t getPacketSize() const { return szBYTE + pPetInfo->getSize() + szDWORD; }
	static PacketSize_t	getPacketMaxSize() { return szBYTE + PetInfo::getMaxSize() + szDWORD; }
};

//////////////////////////////////////////////////////////////////////////////
// class GCPetStashList;
//////////////////////////////////////////////////////////////////////////////

class GCPetStashList : public Packet 
{
public:
	GCPetStashList();
	virtual ~GCPetStashList();
	// It owns its entries, so a copy would free them twice.
	GCPetStashList( const GCPetStashList& ) = delete;
	GCPetStashList& operator = ( const GCPetStashList& ) = delete;

	void read(SocketInputStream & iStream);
	void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_GC_PET_STASH_LIST; }
	PacketSize_t getPacketSize() const;
#ifdef __DEBUG_OUTPUT__
	string getPacketName() const { return "GCPetStashList"; }
	string toString() const;
#endif
public:
	BYTE	getCode() const { return m_Code; }
	void	setCode( BYTE code ) { m_Code = code; }

	std::vector<PetStashItemInfo*>& getPetStashItemInfos() { return m_PetStashItemInfos; }

private:
	BYTE						m_Code;
	std::vector<PetStashItemInfo*>	m_PetStashItemInfos;
};


//////////////////////////////////////////////////////////////////////////////
// class GCPetStashListFactory;
//////////////////////////////////////////////////////////////////////////////

class GCPetStashListFactory : public PacketFactory 
{
public :
	Packet* createPacket() { return new GCPetStashList(); }
	string getPacketName() const { return "GCPetStashList"; }
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_PET_STASH_LIST; }
	PacketSize_t getPacketMaxSize() const { return szBYTE + PetStashItemInfo::getPacketMaxSize() * MAX_PET_STASH; }
};


//////////////////////////////////////////////////////////////////////////////
// class GCPetStashListHandler;
//////////////////////////////////////////////////////////////////////////////

class GCPetStashListHandler 
{
public :
	static void execute(GCPetStashList* pPacket, Player* pPlayer);

};

#endif
