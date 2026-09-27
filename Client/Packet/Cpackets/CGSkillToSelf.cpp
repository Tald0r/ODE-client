//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSkillToSelf.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGSkillToSelf.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "PacketAssert.h"


CGSkillToSelf::CGSkillToSelf ()
{
	__BEGIN_TRY
	__END_CATCH
}

CGSkillToSelf::~CGSkillToSelf ()
{
}

void CGSkillToSelf::read (SocketInputStream & iStream)
{
	__BEGIN_TRY
		
#ifdef __USE_ENCRYPTER__
	SocketEncryptInputStream* pEIStream = dynamic_cast<SocketEncryptInputStream*>(&iStream);
    Assert(pEIStream!=NULL);

	if (pEIStream->getEncryptCode()!=0)
	{
		SHUFFLE_STATEMENT_2(pEIStream->getEncryptCode(),
							pEIStream->readEncrypt(m_SkillType),
							pEIStream->readEncrypt(m_CEffectID));
	}
	else
#endif
	{
		iStream.readWire(m_SkillType);
		iStream.readWire(m_CEffectID);
	}

	__END_CATCH
}

void CGSkillToSelf::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY
		
#ifdef __USE_ENCRYPTER__
	SocketEncryptOutputStream* pEOStream = dynamic_cast<SocketEncryptOutputStream*>(&oStream);
    Assert(pEOStream!=NULL);

	if (pEOStream->getEncryptCode()!=0)
	{
		SHUFFLE_STATEMENT_2(pEOStream->getEncryptCode(),
							pEOStream->writeEncrypt(m_SkillType),
							pEOStream->writeEncrypt(m_CEffectID));
	}
	else
#endif
	{
		oStream.writeWire(m_SkillType);
		oStream.writeWire(m_CEffectID);
	}

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGSkillToSelf::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGSkillToSelf("
		<< "SkillType:" << (int)m_SkillType 
		<< ",CEffectID:" << (int)m_CEffectID 
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif