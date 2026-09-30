//----------------------------------------------------------------------
//
// Filename    : PCOustersInfo.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "PCOustersInfo.h"

//----------------------------------------------------------------------
// read data from socket input stream
//----------------------------------------------------------------------
void PCOustersInfo::read ( SocketInputStream & iStream ) 
{
	__BEGIN_TRY

	//--------------------------------------------------
	// read ousters name
	//--------------------------------------------------
	BYTE szName;

	iStream.read( szName );

	if ( szName == 0 )
		throw InvalidProtocolException("szName == 0");

	if ( szName > 20 )
		throw InvalidProtocolException("too long name length");

	iStream.read( m_Name , szName );

	//--------------------------------------------------
	// read slot
	//
	// Range-checked before it becomes a Slot. LCPCList::read
	// stores this info at m_pPCInfos[getSlot()], an array of
	// SLOT_MAX entries: a byte of SLOT_MAX (3) is a valid
	// enumerator that indexes one past it, and a byte past
	// SLOT_MAX is no Slot value at all.
	//--------------------------------------------------
	BYTE slot;	
	iStream.read( slot );
	if ( slot >= SLOT_MAX )
		throw InvalidProtocolException("pc slot out of range");
	m_Slot = Slot(slot);

	//--------------------------------------------------
	// read Alignment
	//--------------------------------------------------
	iStream.read( m_Alignment );

	//--------------------------------------------------
	// read sex
	//--------------------------------------------------
	BYTE sex;
	iStream.read( sex );
	m_Sex = Sex(sex);

	//--------------------------------------------------
	// read colors
	//--------------------------------------------------
	iStream.read( m_CoatColor );
	iStream.read( m_HairColor );
	iStream.read( m_ArmColor );
	iStream.read( m_BootsColor );

	//--------------------------------------------------
	// read Shape
	//--------------------------------------------------
	BYTE shapeType;
	iStream.read( shapeType );

	// Bits 1 to 3 are the CoatType
	m_CoatType = (OustersCoatType)(shapeType & 7);
	// bit 4 is the ArmType
	m_ArmType = (OustersArmType)(( shapeType >> 3 ) & 1);

	//--------------------------------------------------
	// read attributes
	//--------------------------------------------------
	iStream.read( m_STR );
	iStream.read( m_DEX );
	iStream.read( m_INT );

	//--------------------------------------------------
	// read hp
	//--------------------------------------------------
	iStream.read( m_HP[ATTR_CURRENT] );
	iStream.read( m_HP[ATTR_MAX] );
//	m_HP[ATTR_CURRENT] = min(m_HP[ATTR_CURRENT],m_HP[ATTR_MAX]);
	
	//--------------------------------------------------
	// read mp
	//--------------------------------------------------
	iStream.read( m_MP[ATTR_CURRENT] );
	iStream.read( m_MP[ATTR_MAX] );
//	m_MP[ATTR_CURRENT] = min(m_MP[ATTR_CURRENT],m_MP[ATTR_MAX]);
	
	//--------------------------------------------------
	// read misc
	//--------------------------------------------------
	iStream.read( m_Level );
	iStream.read( m_Rank );
	iStream.read( m_Exp );

	//--------------------------------------------------
	// read Fame 
	//--------------------------------------------------
	iStream.read( m_Fame );
	
	//--------------------------------------------------
	// read Bonus Point
	//--------------------------------------------------
	iStream.read( m_Bonus );
	iStream.read( m_SkillBonus );

	iStream.read( m_AdvancementLevel );

	__END_CATCH
}

//----------------------------------------------------------------------
// write data to socket output stream
//----------------------------------------------------------------------
void PCOustersInfo::write ( SocketOutputStream & oStream ) const 
{
	__BEGIN_TRY

	//--------------------------------------------------
	// write vampire name
	//--------------------------------------------------
	BYTE szName = static_cast<BYTE>(m_Name.size());

	if ( szName == 0 )
		throw InvalidProtocolException("szName == 0");

	if ( szName > 20 )
		throw InvalidProtocolException("too long name length");

	oStream.write( szName );

	oStream.write( m_Name );

	//--------------------------------------------------
	// write slot
	//--------------------------------------------------
	oStream.write( (BYTE)m_Slot );

	//--------------------------------------------------
	// write Alignment
	//--------------------------------------------------
	oStream.write( m_Alignment);

	//--------------------------------------------------
	// write sex
	//--------------------------------------------------
	oStream.write( (BYTE)m_Sex );

	//--------------------------------------------------
	// write colors
	//--------------------------------------------------
	oStream.write( m_CoatColor );
	oStream.write( m_HairColor );
	oStream.write( m_ArmColor );
	oStream.write( m_BootsColor );

	//--------------------------------------------------
	// write Shape
	//--------------------------------------------------
	// 1,2,3 bit 은 CoatType 이다
	// 4 bit 은 ArmType 이다
	BYTE shapeType = 0;
	shapeType = ( m_ArmType << 3 ) | m_CoatType;
	oStream.write( shapeType );

	//--------------------------------------------------
	// write attributes
	//--------------------------------------------------
	oStream.write( m_STR );
	oStream.write( m_DEX );
	oStream.write( m_INT );

	//--------------------------------------------------
	// write hp
	//--------------------------------------------------
	oStream.write( m_HP[ATTR_CURRENT] );
	oStream.write( m_HP[ATTR_MAX] );

	//--------------------------------------------------
	// write mp
	//--------------------------------------------------
	oStream.write( m_MP[ATTR_CURRENT] );
	oStream.write( m_MP[ATTR_MAX] );

	//--------------------------------------------------
	// write misc
	//--------------------------------------------------
	oStream.write( m_Level );
	oStream.write( m_Rank );
	oStream.write( m_Exp );

	//--------------------------------------------------
	// read Fame
	//--------------------------------------------------
	oStream.write( m_Fame );
	
	//--------------------------------------------------
	// write Bonus Point
	//--------------------------------------------------
	oStream.write( m_Bonus );
	oStream.write( m_SkillBonus );
	
	oStream.write( m_AdvancementLevel );
	__END_CATCH
}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
std::string PCOustersInfo::toString () const 
{

	StringStream msg;

	msg << "PCOustersInfo("
		<< "Name:" << m_Name
		<< ",Level:" << (int)m_Level
		<< ",Slot:" << Slot2String[m_Slot]
		<< ",Alignment:" << m_Alignment
		<< ",Sex:" << Sex2String[m_Sex]
		<< ",CoatColor:" << (int)m_CoatColor
		<< ",HairColor:" << (int)m_HairColor
		<< ",ArmColor:" << (int)m_ArmColor
		<< ",BootsColor:" << (int)m_BootsColor
		<< ",CoatType:" << (int)m_CoatType
		<< ",ArmType:" << (int)m_ArmType
		<< ",STR[BASIC]:" << (int)m_STR
		<< ",DEX[BASIC]:" << (int)m_DEX
		<< ",INT[BASIC]:" << (int)m_INT
		<< ",HP:" << m_HP[ATTR_CURRENT] << "/" << m_HP[ATTR_MAX]
		<< ",MP:" << m_MP[ATTR_CURRENT] << "/" << m_MP[ATTR_MAX]
		<< ",Rank:" << m_Rank
		<< ",Exp:" << m_Exp
		<< ",Fame:" << m_Fame
		<< ",Bonus:" << m_Bonus
		<< ",SkillBonus:" << m_SkillBonus
		<< ")";

	return msg.toString();
}

//----------------------------------------------------------------------
