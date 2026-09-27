//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSkillToObject.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGSkillToObject.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "PacketAssert.h"

#include <cstdint>

// Pin the wire width so a change to ObjectID_t is a compile error here.
static_assert(sizeof(ObjectID_t) == sizeof(std::uint32_t),
	"CGSkillToObject stages its target ObjectID as a 32-bit wire scalar");


CGSkillToObject::CGSkillToObject ()
{
	__BEGIN_TRY
	m_CEffectID = 0;
	__END_CATCH
}

CGSkillToObject::~CGSkillToObject ()
{
}

void CGSkillToObject::read (SocketInputStream & iStream)
{
	__BEGIN_TRY
		
#ifdef __USE_ENCRYPTER__
	SocketEncryptInputStream* pEIStream = dynamic_cast<SocketEncryptInputStream*>(&iStream);
    Assert(pEIStream!=NULL);

	if (pEIStream->getEncryptCode()!=0)
	{
		SHUFFLE_STATEMENT_3(pEIStream->getEncryptCode(),
							pEIStream->readEncrypt(m_SkillType),
							pEIStream->readEncrypt(m_CEffectID),
							pEIStream->readEncrypt(m_TargetObjectID));
	}
	else 
#endif
	{
		iStream.readWire(m_SkillType);
		iStream.readWire(m_CEffectID);

		// ObjectID_t is DWORD: uint32_t off Windows, but unsigned long on
		// MSVC, which readWire does not accept. Stage it through the
		// 32-bit scalar the wire carries so one spelling builds everywhere.
		std::uint32_t targetObjectID = 0;
		iStream.readWire(targetObjectID);
		m_TargetObjectID = static_cast<ObjectID_t>(targetObjectID);
	}

	__END_CATCH
}

void CGSkillToObject::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY
		
#ifdef __USE_ENCRYPTER__
	SocketEncryptOutputStream* pEOStream = dynamic_cast<SocketEncryptOutputStream*>(&oStream);
    Assert(pEOStream!=NULL);

	if (pEOStream->getEncryptCode()!=0)
	{
		SHUFFLE_STATEMENT_3(pEOStream->getEncryptCode(),
							pEOStream->writeEncrypt(m_SkillType),
							pEOStream->writeEncrypt(m_CEffectID),
							pEOStream->writeEncrypt(m_TargetObjectID));
	}
	else
#endif
	{
		oStream.writeWire(m_SkillType);
		oStream.writeWire(m_CEffectID);
		oStream.writeWire(static_cast<std::uint32_t>(m_TargetObjectID));
	}

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGSkillToObject::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGSkillToObject("
		<< "SkillType:"      << (int)m_SkillType 
		<< ",CEffectID:"     << (int)m_CEffectID 
		<< ",TargetSelfID :" << (int)m_TargetObjectID 
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif