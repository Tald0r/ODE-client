//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSkillToNamed.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGSkillToNamed.h"

#include <span>

CGSkillToNamed::CGSkillToNamed ()
{
	__BEGIN_TRY
	__END_CATCH
}

CGSkillToNamed::~CGSkillToNamed ()
{
}

void CGSkillToNamed::read (SocketInputStream & iStream)
{
	__BEGIN_TRY

	BYTE szTargetName;

	iStream.readWire(m_SkillType);
	iStream.readWire(m_CEffectID);
	iStream.read( szTargetName );

	if ( szTargetName == 0 )
		throw InvalidProtocolException( "szTargetName == 0" );
	if ( szTargetName > 20 )
		throw InvalidProtocolException( "too long target name length" );

	iStream.read( m_TargetName, szTargetName );

	__END_CATCH
}

void CGSkillToNamed::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY
		
	// Cap the std::string's own size, before narrowing to the length byte.
	if ( m_TargetName.size() > 20 )
		throw InvalidProtocolException( "too long target name" );

	const BYTE szTargetName = (BYTE)m_TargetName.size();

	if ( szTargetName == 0 )
		throw InvalidProtocolException( "szTargetName == 0" );

	oStream.writeWire(m_SkillType);
	oStream.writeWire(m_CEffectID);
	oStream.write( szTargetName );
	oStream.write( std::span<const char>(m_TargetName.data(), szTargetName) );

	__END_CATCH
}


#ifdef __DEBUG_OUTPUT__
std::string CGSkillToNamed::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGSkillToNamed("
		<< "SkillType:" << (int)m_SkillType 
		<< ",CEffectID:" << (int)m_CEffectID 
		<< ",TargetName:" << m_TargetName
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif