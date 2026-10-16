//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGRelicToObject.h 
// Written By  : elca@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_RELIC_TO_OBJECT_H__
#define __CG_RELIC_TO_OBJECT_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class CGRelicToObject;
//
//////////////////////////////////////////////////////////////////////

class CGRelicToObject : public Packet {

public:
	
	// constructor
	CGRelicToObject();
	
	// destructor
	~CGRelicToObject();

	
public:
	
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read(SocketInputStream & iStream);
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_CG_RELIC_TO_OBJECT; }
	
	// get packet's body size
	PacketSize_t getPacketSize() const noexcept { return szObjectID + szObjectID + szCoord + szCoord; }

	// get/set Corpse's X
	Coord_t getX() const noexcept { return m_X; }
	void setX(Coord_t X) noexcept { m_X = X; }

	// get/set Corpse's Y
	Coord_t getY() const noexcept { return m_Y; }
	void setY(Coord_t Y) noexcept { m_Y = Y; }

	// get/set ObjectID
	ObjectID_t getObjectID() const noexcept { return m_ObjectID; }
	ObjectID_t getItemObjectID() const noexcept { return m_ItemObjectID; }

	void setObjectID(ObjectID_t ObjectID) noexcept { m_ObjectID = ObjectID; }
	void setItemObjectID(ObjectID_t ItemObjectID) noexcept { m_ItemObjectID = ItemObjectID; }

#ifdef __DEBUG_OUTPUT__
	// get packet's debug std::string
	std::string toString() const;

	// get packet name
	std::string getPacketName() const { return "CGRelicToObject"; }
#endif

private :

	ObjectID_t m_ItemObjectID;   // item object id
	ObjectID_t m_ObjectID;  // the relic shelf's object id

	// Not set by UI_ITEM_DROP_TO_CLIENT, and not read by the server.
	Coord_t m_X = 0;
	Coord_t m_Y = 0;
	
};

//////////////////////////////////////////////////////////////////////
//
// class CGRelicToObjectFactory;
//
// Factory for CGRelicToObject
//
//////////////////////////////////////////////////////////////////////

class CGRelicToObjectFactory : public PacketFactory {

public:
	
	// constructor
	CGRelicToObjectFactory() {}
	
	// destructor
	virtual ~CGRelicToObjectFactory() {}

	
public:
	
	// create packet
	Packet* createPacket() { return new CGRelicToObject(); }

	// get packet name
	std::string getPacketName() const { return "CGRelicToObject"; }
	
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_RELIC_TO_OBJECT; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szObjectID + szObjectID + szCoord + szCoord; }
};

#endif
