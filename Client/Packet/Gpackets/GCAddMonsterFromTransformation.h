//////////////////////////////////////////////////////////////////////////////
// Filename    : GCAddMonsterFromTransformation.h 
// Written By  : 
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_ADD_MONSTER_FROM_TRANSFORMATION_H__
#define __GC_ADD_MONSTER_FROM_TRANSFORMATION_H__

#include "Packet.h"
#include "PacketFactory.h"
#include "EffectInfo.h"

//////////////////////////////////////////////////////////////////////////////
// class GCAddMonsterFromTransformation;
//////////////////////////////////////////////////////////////////////////////

class GCAddMonsterFromTransformation : public Packet 
{
public:
	GCAddMonsterFromTransformation();
	virtual ~GCAddMonsterFromTransformation();
	
public:
    void read ( SocketInputStream & iStream );
    void write ( SocketOutputStream & oStream ) const;
	PacketID_t getPacketID () const noexcept { return PACKET_GC_ADD_MONSTER_FROM_TRANSFORMATION; }
	PacketSize_t getPacketSize () const 
	{ 
		return static_cast<PacketSize_t>(szObjectID + // object id
			szMonsterType + // monster type
			szBYTE + // monster name length
			m_MonsterName.size() + // monster name
			szColor + // monster main color
			szColor +  // monster sub color
			szCoord + // x coord
			szCoord + // y coord
			szDir +  // direction
			m_pEffectInfo->getSize() + // effects info on monster
			szHP * 2); // current & max hp
	}

	#ifdef __DEBUG_OUTPUT__
		std::string getPacketName () const { return "GCAddMonsterFromTransformation"; }
		std::string toString () const;
	#endif

public:
	ObjectID_t getObjectID () const noexcept { return m_ObjectID; }
	void setObjectID ( ObjectID_t creatureID ) noexcept { m_ObjectID = creatureID; }

	MonsterType_t getMonsterType () const noexcept { return m_MonsterType; }
	void setMonsterType ( MonsterType_t monsterType ) noexcept { m_MonsterType = monsterType; }

	std::string getMonsterName() const { return m_MonsterName; }
	void setMonsterName(std::string name) { m_MonsterName = name; }

	Color_t getMainColor () const noexcept { return m_MainColor; }
	void setMainColor ( Color_t color ) noexcept { m_MainColor = color; }

	Color_t getSubColor () const noexcept { return m_SubColor; }
	void setSubColor ( Color_t color ) noexcept { m_SubColor = color; }

	Coord_t getX () const noexcept { return m_X; }
	void setX ( Coord_t x ) noexcept { m_X = x; }
	
	Coord_t getY () const noexcept { return m_Y; }
	void setY ( Coord_t y ) noexcept { m_Y = y; }

	Dir_t getDir () const noexcept { return m_Dir; }
	void setDir ( Dir_t dir ) noexcept { m_Dir = dir; }

	EffectInfo * getEffectInfo() const { return m_pEffectInfo; }
	void setEffectInfo( EffectInfo * pEffectInfo ) { m_pEffectInfo = pEffectInfo; }

	HP_t getMaxHP() const noexcept { return m_MaxHP; }
	void setMaxHP( HP_t MaxHP ) noexcept { m_MaxHP = MaxHP; }

	HP_t getCurrentHP() const noexcept { return m_CurrentHP; }
	void setCurrentHP( HP_t CurrentHP ) noexcept { m_CurrentHP = CurrentHP; }

private :
    ObjectID_t    m_ObjectID;    // object id
	MonsterType_t m_MonsterType; // monster type
	std::string        m_MonsterName; // monster name
	Color_t       m_MainColor;   // monster main color
	Color_t       m_SubColor;    // monster sub color
   	Coord_t       m_X;           // x coord.
	Coord_t       m_Y;           // y coord.
	Dir_t         m_Dir;         // monster direction
	EffectInfo*   m_pEffectInfo; // effects info on monster
	HP_t          m_CurrentHP;   // current hp
	HP_t          m_MaxHP;       // maximum hp

};


//////////////////////////////////////////////////////////////////////////////
// class GCAddMonsterFromTransformationFactory;
//////////////////////////////////////////////////////////////////////////////

class GCAddMonsterFromTransformationFactory : public PacketFactory 
{
public:
	Packet * createPacket () { return new GCAddMonsterFromTransformation(); }

	#ifdef __DEBUG_OUTPUT__
		std::string getPacketName () const { return "GCAddMonsterFromTransformation"; }
	#endif

	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_ADD_MONSTER_FROM_TRANSFORMATION; }
	PacketSize_t getPacketMaxSize () const 
	{ 
		return szObjectID               // object id
			+ szMonsterType             // monster type
			+ szBYTE                    // monster name length
			+ 32                        // monster namx max
			+ szColor + szColor         // monster main & sub color 
			+ szCoord + szCoord + szDir // monster x, y coord & direction
			+ EffectInfo::getMaxSize()  // effects info on monster
			+ szHP * 2;                 // current & max hp
	}
};


//////////////////////////////////////////////////////////////////////////////
// class GCAddMonsterFromTransformationHandler;
//////////////////////////////////////////////////////////////////////////////

class GCAddMonsterFromTransformationHandler 
{
public:
	static void execute ( GCAddMonsterFromTransformation * pPacket , Player * pPlayer );

};

#endif
