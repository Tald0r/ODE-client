//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGAbsorbSoul 
// Written By  :
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_ABSORB_SOUL_H__
#define __CG_ABSORB_SOUL_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"


//////////////////////////////////////////////////////////////////////
//
// class CGAbsorbSoul;
//
//////////////////////////////////////////////////////////////////////

class CGAbsorbSoul : public Packet {

public:
	
	// constructor
	CGAbsorbSoul();
	
	// destructor
	~CGAbsorbSoul();

	
public:
	
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read(SocketInputStream & iStream);
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_CG_ABSORB_SOUL; }
	
	// get packet's body size
	PacketSize_t getPacketSize() const noexcept { return szObjectID + szZoneCoord*2 + szObjectID + szCoordInven*4;}

#ifdef __DEBUG_OUTPUT__
	// get packet name
	std::string getPacketName() const { return "CGAbsorbSoul"; }
	
	// get packet's debug string
	std::string toString() const;
#endif

	// get/set ObjectID
	ObjectID_t getObjectID() const noexcept { return m_ObjectID; }
	void setObjectID(ObjectID_t ObjectID) noexcept { m_ObjectID = ObjectID; }
	
	ObjectID_t getInvenObjectID() const noexcept { return m_InvenObjectID; }
	void setInvenObjectID(ObjectID_t invenObjectID) noexcept { m_InvenObjectID = invenObjectID; }

	Coord_t getInvenX() const noexcept { return m_InvenX; }
	void setInvenX( CoordInven_t x ) noexcept { m_InvenX = x; }

	Coord_t getInvenY() const noexcept { return m_InvenY; }
	void setInvenY( CoordInven_t y ) noexcept { m_InvenY = y; }

	Coord_t getTargetInvenX() const noexcept { return m_TargetInvenX; }
	void setTargetInvenX( CoordInven_t x ) noexcept { m_TargetInvenX = x; }

	Coord_t getTargetInvenY() const noexcept { return m_TargetInvenY; }
	void setTargetInvenY( CoordInven_t y ) noexcept { m_TargetInvenY = y; }

	ZoneCoord_t getTargetZoneX() const noexcept { return m_TargetZoneX; }
	void setTargetZoneX( ZoneCoord_t x ) noexcept { m_TargetZoneX = x; }

	ZoneCoord_t getTargetZoneY() const noexcept { return m_TargetZoneY; }
	void setTargetZoneY( ZoneCoord_t y ) noexcept { m_TargetZoneY = y; }
	
private :
	
	ObjectID_t m_ObjectID;  // ObjectID
	
	ObjectID_t m_InvenObjectID; // 라바의 ObjectID
	CoordInven_t m_InvenX;			// X 좌표
	CoordInven_t m_InvenY;			// Y 좌표
	CoordInven_t m_TargetInvenX;	// Target X 좌표
	CoordInven_t m_TargetInvenY;	// Target Y 좌표
	ZoneCoord_t m_TargetZoneX;	// Target X 좌표
	ZoneCoord_t m_TargetZoneY;	// Target Y 좌표
};


//////////////////////////////////////////////////////////////////////
//
// class CGAbsorbSoulFactory;
//
// Factory for CGAbsorbSoul
//
//////////////////////////////////////////////////////////////////////
class CGAbsorbSoulFactory : public PacketFactory {

public:
	
	// constructor
	CGAbsorbSoulFactory() {}
	
	// destructor
	virtual ~CGAbsorbSoulFactory() {}

	
public:
	
	// create packet
	Packet* createPacket() { return new CGAbsorbSoul(); }

	// get packet name
	std::string getPacketName() const { return "CGAbsorbSoul"; }
	
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_ABSORB_SOUL; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szObjectID + szZoneCoord*2+ szObjectID + szCoordInven*4; }

};

//////////////////////////////////////////////////////////////////////
//
// class CGAbsorbSoulHandler;
//
//////////////////////////////////////////////////////////////////////

class CGAbsorbSoulHandler {
	
public:

	// execute packet's handler
	static void execute(CGAbsorbSoul* pCGAbsorbSoul, Player* player);
};

#endif
