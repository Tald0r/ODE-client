//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGBloodDrain.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "CGBloodDrain.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

#include <cstdint>

// Pin the wire width so a change to ObjectID_t is a compile error here.
static_assert(sizeof(ObjectID_t) == sizeof(std::uint32_t),
	"CGBloodDrain stages its ObjectID as a 32-bit wire scalar");

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
CGBloodDrain::CGBloodDrain ()
{
	__BEGIN_TRY
	__END_CATCH
}

	
//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
CGBloodDrain::~CGBloodDrain ()
{
}


//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void CGBloodDrain::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY
		
	// ObjectID_t is DWORD: uint32_t off Windows, but unsigned long on
	// MSVC, which readWire does not accept. Stage it through the
	// 32-bit scalar the wire carries so one spelling builds everywhere.
	std::uint32_t objectID = 0;
	iStream.readWire(objectID);
	m_ObjectID = static_cast<ObjectID_t>(objectID);
/*	
	iStream.read( (char*)&m_X , szCoord );
	iStream.read( (char*)&m_Y , szCoord );
	iStream.read( (char*)&m_Dir , szDir );
*/
	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void CGBloodDrain::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	oStream.writeWire(static_cast<std::uint32_t>(m_ObjectID));
/*	
	oStream.write( (char*)&m_X , szCoord );
	oStream.write( (char*)&m_Y , szCoord );
	oStream.write( (char*)&m_Dir , szDir );
*/
	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
	std::string CGBloodDrain::toString () const
	{
		__BEGIN_TRY
			
		StringStream msg;
		msg << "CGBloodDrain(ObjectID :" << (int)m_ObjectID << ")";
		return msg.toString();

		__END_CATCH
	}
#endif
