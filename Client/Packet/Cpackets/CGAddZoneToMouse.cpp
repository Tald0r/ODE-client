//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAddZoneToMouse.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGAddZoneToMouse.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "PacketAssert.h"

#include <cstdint>

// Pin the wire width so a change to ObjectID_t is a compile error here.
static_assert(sizeof(ObjectID_t) == sizeof(std::uint32_t),
	"CGAddZoneToMouse stages its ObjectID as a 32-bit wire scalar");


CGAddZoneToMouse::CGAddZoneToMouse ()
{
	__BEGIN_TRY
	__END_CATCH
}

CGAddZoneToMouse::~CGAddZoneToMouse ()
{
}

void CGAddZoneToMouse::read (SocketInputStream & iStream)
{
	__BEGIN_TRY
		
#ifdef __USE_ENCRYPTER__
	SocketEncryptInputStream* pEIStream = dynamic_cast<SocketEncryptInputStream*>(&iStream);
    Assert(pEIStream!=NULL);

	if (pEIStream->getEncryptCode()!=0)
	{
		SHUFFLE_STATEMENT_3(pEIStream->getEncryptCode(),
							pEIStream->readEncrypt(m_ObjectID),
							pEIStream->readEncrypt(m_ZoneX),
							pEIStream->readEncrypt(m_ZoneY));
	}
	else
#endif
	{
		// ObjectID_t is DWORD: uint32_t off Windows, but unsigned long on
		// MSVC, which readWire does not accept. Stage it through the
		// 32-bit scalar the wire carries so one spelling builds everywhere.
		std::uint32_t objectID = 0;
		iStream.readWire(objectID);
		m_ObjectID = static_cast<ObjectID_t>(objectID);
		iStream.readWire(m_ZoneX);
		iStream.readWire(m_ZoneY);
	}

	__END_CATCH
}

void CGAddZoneToMouse::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

#ifdef __USE_ENCRYPTER__
	SocketEncryptOutputStream* pEOStream = dynamic_cast<SocketEncryptOutputStream*>(&oStream);
    Assert(pEOStream!=NULL);

	if (pEOStream->getEncryptCode()!=0)
	{
		SHUFFLE_STATEMENT_3(pEOStream->getEncryptCode(),
							pEOStream->writeEncrypt(m_ObjectID),
							pEOStream->writeEncrypt(m_ZoneX),
							pEOStream->writeEncrypt(m_ZoneY));
	}
	else
#endif
	{
		oStream.writeWire(static_cast<std::uint32_t>(m_ObjectID));
		oStream.writeWire(m_ZoneX);
		oStream.writeWire(m_ZoneY);
	}

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGAddZoneToMouse::toString () 
	const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGAddZoneToMouse(" 
	    << "ObjectID : " << (int)m_ObjectID 
		<< ", ZoneX : " << (int)m_ZoneX 
		<< ", ZoneY : " << (int)m_ZoneY 
		<< ")";
	return msg.toString();

	__END_CATCH
}
#endif