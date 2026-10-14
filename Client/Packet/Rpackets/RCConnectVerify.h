//////////////////////////////////////////////////////////////////////
// 
// Filename    : RCConnectVerify.h 
// Written By  : crazydog
// Description : Effect 제거.
// 
//////////////////////////////////////////////////////////////////////

#ifndef __RC_CONNECT_VERIFY_H__
#define __RC_CONNECT_VERIFY_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class RCConnectVerify;
//
// 게임서버에서 클라이언트로 자신의 변형된 데이터를 알려주기 위한 객채
// RemoveEffectrmation, SkillToObjectOK 등에 실려서 날아간다.
//
//////////////////////////////////////////////////////////////////////
// One byte on the wire: the fixed type holds any byte the server sends.
enum RC_CONNECT_VERIFY_CODE : BYTE
{
	RC_CONNECT_NULL,			// not a value

	RC_CONNECT_OK,				// allowed
	RC_CONNECT_REJECT,			// refused - -; 
	RC_CONNECT_WRONG,			// a wrong connection (the RequestServerName differs, say..)
};

class RCConnectVerify : public Packet
{

public :
	
	// constructor
	RCConnectVerify ();
	
	// destructor
	~RCConnectVerify ();
	
public :
    PacketID_t getPacketID () const noexcept { return PACKET_RC_CONNECT_VERIFY; }

	#ifdef __DEBUG_OUTPUT__
		std::string getPacketName () const { return "RCConnectVerify"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif


    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;

	// get packet's body size
	// 최적화시, 미리 계산된 정수를 사용한다.
	PacketSize_t getPacketSize () const noexcept { return szBYTE; }
	static PacketSize_t getPacketMaxSize() noexcept { return szBYTE;}

	// get / set ListNumber
	RC_CONNECT_VERIFY_CODE	getCode() const noexcept { return m_Code; }
	void					setCode( RC_CONNECT_VERIFY_CODE code ) noexcept { m_Code = code; }

protected :
	
	RC_CONNECT_VERIFY_CODE		m_Code;

};

//////////////////////////////////////////////////////////////////////
//
// class RCConnectVerifyFactory;
//
// Factory for RCConnectVerify
//
//////////////////////////////////////////////////////////////////////

class RCConnectVerifyFactory : public PacketFactory {

public :
	
	// constructor
	RCConnectVerifyFactory () {}
	
	// destructor
	virtual ~RCConnectVerifyFactory () {}

	
public :
	
	// create packet
	Packet * createPacket () { return new RCConnectVerify(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "RCConnectVerify"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_RC_CONNECT_VERIFY; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize () const noexcept { return 255;}

};


//////////////////////////////////////////////////////////////////////
//
// class RCConnectVerifyHandler;
//
//////////////////////////////////////////////////////////////////////

class RCConnectVerifyHandler {

public :

	// execute packet's handler
	static void execute ( RCConnectVerify * pRCConnectVerify , Player * pPlayer );

};


#endif
