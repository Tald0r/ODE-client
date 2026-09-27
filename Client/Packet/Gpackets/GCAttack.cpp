//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCAttack.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "GCAttack.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

#include <cstdint>

// Pin the wire width so a change to ObjectID_t is a compile error here.
static_assert(sizeof(ObjectID_t) == sizeof(std::uint32_t),
	"GCAttack stages its ObjectID as a 32-bit wire scalar");


//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GCAttack::GCAttack ()
{
	__BEGIN_TRY
	__END_CATCH
}

	
//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
GCAttack::~GCAttack ()
{
}


//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void GCAttack::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY
		
	// ObjectID_t is DWORD: uint32_t off Windows, but unsigned long on
	// MSVC, which readWire does not accept. Stage it through the
	// 32-bit scalar the wire carries so one spelling builds everywhere.
	std::uint32_t objectID = 0;
	iStream.readWire(objectID);
	m_ObjectID = static_cast<ObjectID_t>(objectID);
	iStream.readWire(m_X);
	iStream.readWire(m_Y);
	iStream.readWire(m_Dir);

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void GCAttack::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	oStream.writeWire(static_cast<std::uint32_t>(m_ObjectID));
	oStream.writeWire(m_X);
	oStream.writeWire(m_Y);
	oStream.writeWire(m_Dir);

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
	std::string GCAttack::toString () const
	{
		__BEGIN_TRY
			
		StringStream msg;
		msg << "GCAttack(X:" << (int)m_X << ",Y:" << (int)m_Y << ",ObjectID:" << (int)m_ObjectID;
		return msg.toString();

		__END_CATCH
	}
#endif
