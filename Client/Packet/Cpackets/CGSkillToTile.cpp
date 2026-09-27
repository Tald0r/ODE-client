//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSkillToTile.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGSkillToTile.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "PacketAssert.h"


CGSkillToTile::CGSkillToTile ()
{
	__BEGIN_TRY
	__END_CATCH
}

CGSkillToTile::~CGSkillToTile ()
{
}

void CGSkillToTile::read (SocketInputStream & iStream)
{
	__BEGIN_TRY
		
#ifdef __USE_ENCRYPTER__
	SocketEncryptInputStream* pEIStream = dynamic_cast<SocketEncryptInputStream*>(&iStream);
    Assert(pEIStream!=NULL);

	if (pEIStream->getEncryptCode()!=0)
	{
		SHUFFLE_STATEMENT_4(pEIStream->getEncryptCode(),
							pEIStream->readEncrypt(m_SkillType),
							pEIStream->readEncrypt(m_CEffectID),
							pEIStream->readEncrypt(m_X),
							pEIStream->readEncrypt(m_Y));
	}
	else
#endif
	{
		iStream.readWire(m_SkillType);
		iStream.readWire(m_CEffectID);
		iStream.readWire(m_X);
		iStream.readWire(m_Y);
	}


	__END_CATCH
}

void CGSkillToTile::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY
		
#ifdef __USE_ENCRYPTER__
	SocketEncryptOutputStream* pEOStream = dynamic_cast<SocketEncryptOutputStream*>(&oStream);
    Assert(pEOStream!=NULL);

	if (pEOStream->getEncryptCode()!=0)
	{
		SHUFFLE_STATEMENT_4(pEOStream->getEncryptCode(),
							pEOStream->writeEncrypt(m_SkillType),
							pEOStream->writeEncrypt(m_CEffectID),
							pEOStream->writeEncrypt(m_X),
							pEOStream->writeEncrypt(m_Y));
	}
	else
#endif
	{
		oStream.writeWire(m_SkillType);
		oStream.writeWire(m_CEffectID);
		oStream.writeWire(m_X);
		oStream.writeWire(m_Y);
	}

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGSkillToTile::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGSkillToTile("
		<< "SkillType:" << (int)m_SkillType 
		<< ",CEffectID:" << (int)m_CEffectID 
		<< ",X:" << (int)m_X 
		<< ",Y: " << (int)m_Y 
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif