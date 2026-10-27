//////////////////////////////////////////////////////////////////////////////
// Filename    : GCMyStoreInfo.h 
// Written By  : excel96
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_MY_STORE_INFO_H__
#define __GC_MY_STORE_INFO_H__

#include "Packet.h"
#include "PacketFactory.h"
#include "StoreInfo.h"

//////////////////////////////////////////////////////////////////////////////
// class GCMyStoreInfo;
//////////////////////////////////////////////////////////////////////////////

class GCMyStoreInfo : public Packet
{
public:
	GCMyStoreInfo() : m_pInfo(NULL) { }
	virtual ~GCMyStoreInfo();
	// It owns what its read allocates, so a copy would free it twice.
	GCMyStoreInfo( const GCMyStoreInfo& ) = delete;
	GCMyStoreInfo& operator = ( const GCMyStoreInfo& ) = delete;

public:
	void read(SocketInputStream & iStream);
	void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_GC_MY_STORE_INFO; }
	PacketSize_t getPacketSize() const { return szBYTE + m_pInfo->getSize(false); }
#ifdef __DEBUG_OUTPUT__	
	string getPacketName() const { return "GCMyStoreInfo"; }
	string toString() const;
#endif
public:
	BYTE		getOpenUI() const { return m_OpenUI; }
	void		setOpenUI(BYTE ui) { m_OpenUI = ui; }

	StoreInfo*	getStoreInfo() const { return m_pInfo; }
	void		setStoreInfo(StoreInfo* pInfo ) { m_pInfo = pInfo; }

private:
	BYTE		m_OpenUI;
	StoreInfo*	m_pInfo;
};

//////////////////////////////////////////////////////////////////////////////
// class GCMyStoreInfoFactory;
//////////////////////////////////////////////////////////////////////////////

class GCMyStoreInfoFactory : public PacketFactory 
{
public:
	Packet* createPacket() { return new GCMyStoreInfo(); }
#ifdef __DEBUG_OUTPUT__
	string getPacketName() const { return "GCMyStoreInfo"; }
#endif
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_MY_STORE_INFO; }
	PacketSize_t getPacketMaxSize() const
	{
		return szBYTE + StoreInfo::getMaxSize();
	}
};

//////////////////////////////////////////////////////////////////////////////
// class GCMyStoreInfoHandler;
//////////////////////////////////////////////////////////////////////////////

class GCMyStoreInfoHandler 
{
public:
	static void execute(GCMyStoreInfo* pPacket, Player* pPlayer);
};

#endif
