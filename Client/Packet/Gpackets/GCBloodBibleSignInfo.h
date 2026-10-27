//////////////////////////////////////////////////////////////////////////////
// Filename    : GCBloodBibleSignInfo.h 
// Written By  : excel96
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_BLOOD_BIBLE_SIGN_INFO_H__
#define __GC_BLOOD_BIBLE_SIGN_INFO_H__

#include "Packet.h"
#include "PacketFactory.h"
#include "BloodBibleSignInfo.h"

//////////////////////////////////////////////////////////////////////////////
// class GCBloodBibleSignInfo;
//////////////////////////////////////////////////////////////////////////////

class GCBloodBibleSignInfo : public Packet
{
public:
	GCBloodBibleSignInfo() : m_pInfo(NULL) { }
	virtual ~GCBloodBibleSignInfo();
	// It owns what its read allocates, so a copy would free it twice.
	GCBloodBibleSignInfo( const GCBloodBibleSignInfo& ) = delete;
	GCBloodBibleSignInfo& operator = ( const GCBloodBibleSignInfo& ) = delete;

public:
	void read(SocketInputStream & iStream);
	void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_GC_BLOOD_BIBLE_SIGN_INFO; }
	PacketSize_t getPacketSize() const { return m_pInfo->getSize(); }
#ifdef __DEBUG_OUTPUT__
	string getPacketName() const { return "GCBloodBibleSignInfo"; }
	string toString() const;
#endif
public:
	BloodBibleSignInfo* getSignInfo() const { return m_pInfo; }
	void setSignInfo(BloodBibleSignInfo* pInfo) { m_pInfo = pInfo; }

private:
	BloodBibleSignInfo*	m_pInfo;
};

//////////////////////////////////////////////////////////////////////////////
// class GCBloodBibleSignInfoFactory;
//////////////////////////////////////////////////////////////////////////////

class GCBloodBibleSignInfoFactory : public PacketFactory 
{
public:
	Packet* createPacket() { return new GCBloodBibleSignInfo(); }
#ifdef __DEBUG_OUTPUT__
	string getPacketName() const { return "GCBloodBibleSignInfo"; }
#endif
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_BLOOD_BIBLE_SIGN_INFO; }
	PacketSize_t getPacketMaxSize() const
	{
		return BloodBibleSignInfo::getMaxSize();
	}
};

//////////////////////////////////////////////////////////////////////////////
// class GCBloodBibleSignInfoHandler;
//////////////////////////////////////////////////////////////////////////////

class GCBloodBibleSignInfoHandler 
{
public:
	static void execute(GCBloodBibleSignInfo* pPacket, Player* pPlayer);
};

#endif
