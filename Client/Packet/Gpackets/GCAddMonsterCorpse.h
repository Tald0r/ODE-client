//--------------------------------------------------------------------------------
// 
// Filename    : GCAddMonsterCorpse.h 
// Written By  : Reiot
// 
//--------------------------------------------------------------------------------

#ifndef __GC_ADD_MONSTER_CORPSE_H__
#define __GC_ADD_MONSTER_CORPSE_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"


//--------------------------------------------------------------------------------
//
// class GCAddMonsterCorpse;
//
//--------------------------------------------------------------------------------

class GCAddMonsterCorpse : public Packet {

public :
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_GC_ADD_MONSTER_CORPSE; }
	
	// get packet's body size
	// *OPTIMIZATION HINT*
	// const static GCAddMonsterCorpsePacketSize 를 정의, 리턴하라.
	PacketSize_t getPacketSize () const 
	{ 
		return static_cast<PacketSize_t>(szObjectID + 
			szMonsterType + 
			szBYTE +
			m_MonsterName.size() +
			szCoord + 
			szCoord + 
			szDir + 
			szBYTE +
			szObjectID); 
	}

	#ifdef __DEBUG_OUTPUT__
		// get packet's name
		std::string getPacketName () const { return "GCAddMonsterCorpse"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif

public :

	// get/set creature ID 
	ObjectID_t getObjectID () const noexcept { return m_ObjectID; }
	void setObjectID ( ObjectID_t creatureID ) noexcept { m_ObjectID = creatureID; }

	// get/set monster type
	MonsterType_t getMonsterType () const noexcept { return m_MonsterType; }
	void setMonsterType ( MonsterType_t monsterType ) noexcept { m_MonsterType = monsterType; }

	// get/set monster name
	std::string getMonsterName() const { return m_MonsterName; }
	void setMonsterName(std::string name) { m_MonsterName = name; }

	// get/set X
	Coord_t getX () const noexcept { return m_X; }
	void setX ( Coord_t x ) noexcept { m_X = x; }
	
	// get/set Y
	Coord_t getY () const noexcept { return m_Y; }
	void setY ( Coord_t y ) noexcept { m_Y = y; }

	// get/set Dir
	Dir_t getDir () const noexcept { return m_Dir; }
	void setDir ( Dir_t dir ) noexcept { m_Dir = dir; }

	bool gethasHead () const noexcept { return m_bhasHead; }
	void sethasHead ( bool bhasHead ) noexcept { m_bhasHead = bhasHead; }

	// get/set Treasure Count
	BYTE getTreasureCount() const noexcept { return m_TreasureCount; }
	void setTreasureCount( BYTE Count ) noexcept { m_TreasureCount = Count; }

	ObjectID_t getLastKiller() const { return m_LastKiller; }
	void setLastKiller( ObjectID_t id ) { m_LastKiller = id; }

private :
	
    ObjectID_t m_ObjectID;			// OID

	MonsterType_t m_MonsterType; 	// 몬스터 타입

	std::string m_MonsterName;

   	Coord_t m_X;					// X 좌표
	Coord_t m_Y;					// Y 좌표
	Dir_t m_Dir;					// 방향
	bool m_bhasHead;				// 머리가 있는가?
	
	BYTE m_TreasureCount;

	ObjectID_t m_LastKiller;

};


//--------------------------------------------------------------------------------
//
// class GCAddMonsterCorpseFactory;
//
// Factory for GCAddMonsterCorpse
//
//--------------------------------------------------------------------------------

class GCAddMonsterCorpseFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new GCAddMonsterCorpse(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "GCAddMonsterCorpse"; }
	#endif	
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_ADD_MONSTER_CORPSE; }

	// get packet's body size
	// *OPTIMIZATION HINT*
	// const static GCAddMonsterCorpsePacketSize 를 정의, 리턴하라.
	PacketSize_t getPacketMaxSize () const 
	{ 
		return szObjectID +
			szMonsterType +
			szBYTE +
			128 +
			szCoord +
			szCoord +
			szDir +
			szBYTE +	// m_bhasHead
			szBYTE +	// m_TreasureCount
			szObjectID;
	}

};


//--------------------------------------------------------------------------------
//
// class GCAddMonsterCorpseHandler;
//
//--------------------------------------------------------------------------------

class GCAddMonsterCorpseHandler {

public :

	// execute packet's handler
	static void execute ( GCAddMonsterCorpse * pPacket , Player * pPlayer );

};

#endif
