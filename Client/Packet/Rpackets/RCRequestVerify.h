//////////////////////////////////////////////////////////////////////
// 
// Filename    : RCRequestVerify.h 
// Written By  : elca@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __RC_REQUEST_VERIFY_H__
#define __RC_REQUEST_VERIFY_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

// One byte on the wire: the fixed type holds any byte the server sends.
enum REQUEST_VERIFY : BYTE
{
	REQUEST_VERIFY_NULL,
	REQUEST_VERIFY_WHISPER_FAILED,
	REQUEST_VERIFY_PROFILE_NOT_EXIST,		// there is no profile at all
	REQUEST_VERIFY_PROFILE_WRONG_USER,		// asked of the wrong user
	REQUEST_VERIFY_PROFILE_DONE,			// the profile is all sent.
};

//////////////////////////////////////////////////////////////////////
//
// class RCRequestVerify;
//
//////////////////////////////////////////////////////////////////////

class RCRequestVerify : public Packet {

public :
	
	// constructor
	RCRequestVerify ();
	
	// destructor
	~RCRequestVerify ();

	
public :
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_RC_REQUEST_VERIFY; }
	
	// get packet's body size
	// 최적화시, 미리 계산된 정수를 사용한다.
	PacketSize_t getPacketSize () const noexcept { return szBYTE; }

	#ifdef __DEBUG_OUTPUT__
		// get packet's name
		std::string getPacketName () const { return "RCRequestVerify"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif

	REQUEST_VERIFY getCode() const noexcept { return m_Code ; }
	void setCode ( REQUEST_VERIFY code ) noexcept { m_Code = code ; }


private :
	REQUEST_VERIFY		m_Code;

};

//////////////////////////////////////////////////////////////////////
//
// class RCRequestVerifyFactory;
//
// Factory for RCRequestVerify
//
//////////////////////////////////////////////////////////////////////

class RCRequestVerifyFactory : public PacketFactory {

public :
	
	// constructor
	RCRequestVerifyFactory () {}
	
	// destructor
	virtual ~RCRequestVerifyFactory () {}

	
public :
	
	// create packet
	Packet * createPacket () { return new RCRequestVerify(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "RCRequestVerify"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_RC_REQUEST_VERIFY; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szBYTE; }

};


//////////////////////////////////////////////////////////////////////
//
// class RCRequestVerifyHandler;
//
//////////////////////////////////////////////////////////////////////

class RCRequestVerifyHandler {

public :

	// execute packet's handler
	static void execute ( RCRequestVerify * pRCRequestVerify , Player * pPlayer );

};

#endif
