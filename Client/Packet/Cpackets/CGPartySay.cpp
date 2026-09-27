//////////////////////////////////////////////////////////////////////////////
// Filename    : CGPartySay.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGPartySay.h"

CGPartySay::CGPartySay ()
{
	__BEGIN_TRY
	__END_CATCH
}

CGPartySay::~CGPartySay ()
{
}

void CGPartySay::read (SocketInputStream & iStream)
{
	__BEGIN_TRY

	BYTE szMessage;
	iStream.read(m_Color);
	iStream.read(szMessage);
	iStream.read(m_Message, szMessage);
	
	__END_CATCH
}
		    
void CGPartySay::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

	// Cap before narrowing; the length byte must describe every byte written.
	if (m_Message.size() > 128)
		throw InvalidProtocolException("too large message length");

	const BYTE szMessage = (BYTE)m_Message.size();

	oStream.write(m_Color);
	oStream.write(szMessage);
	oStream.write(std::span<const char>(m_Message.data(), szMessage));

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// get debug std::string
//////////////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
string CGPartySay::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGPartySay("
		<< "Color :" << m_Color
		<< "Message :" << m_Message
		<< ")";
	return msg.toString();

	__END_CATCH
}
#endif