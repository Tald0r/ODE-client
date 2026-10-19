//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGSkillToSelf.h 
// Written By  : elca@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_SKILL_TO_SELF_H__
#define __CG_SKILL_TO_SELF_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class CGSkillToSelf;
//
//////////////////////////////////////////////////////////////////////

class CGSkillToSelf : public Packet {

public :
	
	// constructor
	CGSkillToSelf ();
	
	// destructor
	~CGSkillToSelf ();

	
public :
	
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_CG_SKILL_TO_SELF; }
	
	// get packet's body size
	PacketSize_t getPacketSize () const noexcept { return szSkillType + szCEffectID ; }

	// get SkillType
	SkillType_t getSkillType() const noexcept  { return m_SkillType; }
	void setSkillType( SkillType_t SkillType ) noexcept { m_SkillType = SkillType; }

	// get CEffectID
	CEffectID_t getCEffectID() const noexcept { return m_CEffectID; }
	void setCEffectID( CEffectID_t CEffectID ) noexcept { m_CEffectID = CEffectID; }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGSkillToSelf"; }

		// get packet's debug std::string
		std::string toString () const;
	#endif

private :

	// SkillType
	SkillType_t m_SkillType;

	// CEffectID (Eternity's request never sets it)
	CEffectID_t m_CEffectID = 0;

};


//////////////////////////////////////////////////////////////////////
//
// class CGSkillToSelfFactory;
//
// Factory for CGSkillToSelf
//
//////////////////////////////////////////////////////////////////////
class CGSkillToSelfFactory : public PacketFactory {

public :
	
	// constructor
	CGSkillToSelfFactory () {}
	
	// destructor
	virtual ~CGSkillToSelfFactory () {}

	
public :
	
	// create packet
	Packet * createPacket () { return new CGSkillToSelf(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGSkillToSelf"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CG_SKILL_TO_SELF; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize () const noexcept { return szSkillType + szCEffectID; }

};


#endif
