//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCGetDamage.cc 
// Written By  : elca@ewestsoft.com
// Description : CGMove가 날아 왓을때 자기 자신에게 OK 사인을 날리기
//               위한 패킷 클래스 함수 정의
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "GCGetDamage.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

#include <cstdint>

// Pin the wire width so a change to ObjectID_t is a compile error here.
static_assert(sizeof(ObjectID_t) == sizeof(std::uint32_t),
	"GCGetDamage stages its ObjectID as a 32-bit wire scalar");

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GCGetDamage::GCGetDamage ()
{
	__BEGIN_TRY
	__END_CATCH
}

	
//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
GCGetDamage::~GCGetDamage ()
{
}


//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void GCGetDamage::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY
	// ObjectID_t is DWORD: uint32_t off Windows, but unsigned long on
	// MSVC, which readWire does not accept. Stage it through the
	// 32-bit scalar the wire carries so one spelling builds everywhere.
	std::uint32_t objectID = 0;
	iStream.readWire(objectID);
	m_ObjectID = static_cast<ObjectID_t>(objectID);
	iStream.readWire(m_GetDamage);
	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void GCGetDamage::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY
	oStream.writeWire(static_cast<std::uint32_t>(m_ObjectID));
	oStream.writeWire(m_GetDamage);
	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// get packet's debug std::string
//
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
	std::string GCGetDamage::toString () const
	{
		__BEGIN_TRY

		StringStream msg;
		msg << "GCGetDamage ( ObjectID : " << (int)m_ObjectID << "  Damage : " << (int)m_GetDamage << " )";
		return msg.toString();

		__END_CATCH
	}

#endif
