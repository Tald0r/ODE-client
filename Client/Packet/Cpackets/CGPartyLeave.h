//////////////////////////////////////////////////////////////////////////////
// Filename    : CGPartyLeave.h 
// Written By  : excel96
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __CG_PARTY_LEAVE_H__
#define __CG_PARTY_LEAVE_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class CGPartyLeave
//////////////////////////////////////////////////////////////////////////////

class CGPartyLeave : public Packet 
{
public:
	void read(SocketInputStream & iStream);
	void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_CG_PARTY_LEAVE; }
	PacketSize_t getPacketSize() const { return static_cast<PacketSize_t>(szBYTE + m_TargetName.size()); }

	#ifdef __DEBUG_OUTPUT__
		std::string getPacketName() const { return "CGPartyLeave"; }
		std::string toString() const;
	#endif
	
public:
	std::string getTargetName(void) const { return m_TargetName; }
	void setTargetName(const std::string& name) { m_TargetName = name; }

private:
	std::string m_TargetName;
};


//////////////////////////////////////////////////////////////////////////////
// class CGPartyLeaveFactory;
//////////////////////////////////////////////////////////////////////////////
class CGPartyLeaveFactory : public PacketFactory 
{
public:
	Packet* createPacket() { return new CGPartyLeave(); }

	#ifdef __DEBUG_OUTPUT__
		std::string getPacketName() const { return "CGPartyLeave"; }
	#endif

	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_PARTY_LEAVE; }
	PacketSize_t getPacketMaxSize() const noexcept { return szBYTE + 10; }
};

#endif
