//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAttack.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGAttack.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "PacketAssert.h"

#include <cstdint>

// Pin the wire width so a change to ObjectID_t is a compile error here.
static_assert(sizeof(ObjectID_t) == sizeof(std::uint32_t),
	"CGAttack stages its ObjectID as a 32-bit wire scalar");


CGAttack::CGAttack ()
{
	__BEGIN_TRY
	__END_CATCH
}

CGAttack::~CGAttack ()
{
}

void CGAttack::read (SocketInputStream & iStream)
{
	__BEGIN_TRY
		
#ifdef __USE_ENCRYPTER__
	SocketEncryptInputStream* pEIStream = dynamic_cast<SocketEncryptInputStream*>(&iStream);
    Assert(pEIStream!=NULL);

	if (pEIStream->getEncryptCode()!=0)
	{
		SHUFFLE_STATEMENT_4(pEIStream->getEncryptCode(),
							pEIStream->readEncrypt(m_ObjectID),
							pEIStream->readEncrypt(m_X),
							pEIStream->readEncrypt(m_Y),
							pEIStream->readEncrypt(m_Dir));
	}
	else
#endif
	{
		std::uint32_t objectID = 0;
		iStream.readWire(objectID);
		m_ObjectID = static_cast<ObjectID_t>(objectID);
		iStream.readWire(m_X);
		iStream.readWire(m_Y);
		iStream.readWire(m_Dir);
	}

	__END_CATCH
}

void CGAttack::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

#ifdef __USE_ENCRYPTER__
	SocketEncryptOutputStream* pEOStream = dynamic_cast<SocketEncryptOutputStream*>(&oStream);
    Assert(pEOStream!=NULL);

	if (pEOStream->getEncryptCode()!=0)
	{
		SHUFFLE_STATEMENT_4(pEOStream->getEncryptCode(),
							pEOStream->writeEncrypt(m_ObjectID),
							pEOStream->writeEncrypt(m_X),
							pEOStream->writeEncrypt(m_Y),
							pEOStream->writeEncrypt(m_Dir));
	}
	else
#endif
	{
		oStream.writeWire(static_cast<std::uint32_t>(m_ObjectID));
		oStream.writeWire(m_X);
		oStream.writeWire(m_Y);
		oStream.writeWire(m_Dir);
	}

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGAttack::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGAttack(" 
		<< "X:" << (int)m_X	
		<< ",Y:" << (int)m_Y 
		<< ",ObjectID :" << (int)m_ObjectID 
		<< ", Dir:" << (int)m_Dir 
		<< ")";
	return msg.toString();

	__END_CATCH
}
#endif
