//////////////////////////////////////////////////////////////////////////////
// Filename    : GCPartyLeave.cpp 
// Written By  : 김성민
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "GCPartyLeave.h"

//////////////////////////////////////////////////////////////////////////////
// class GCPartyLeave member methods
//////////////////////////////////////////////////////////////////////////////

void GCPartyLeave::read (SocketInputStream & iStream)
{
	__BEGIN_TRY

	// GCPartyLeaveFactory::getPacketMaxSize budgets szBYTE*2 + 20 for the whole
	// body, so the two names together never exceed 20 bytes on a packet that
	// ClientPlayer::processCommand already accepted -- it rejects any declared
	// size above that maximum. read() itself is not bounded by the declared
	// size (SocketInputStream bounds each read against the whole buffer, not
	// against this packet), so without this check a server can declare 22 bytes
	// and then hand us two 255-byte names taken from the packets queued behind
	// it, which GCPartyLeaveHandler sprintf's into a char[256].
	const int MAX_NAME_TOTAL = 20;

	BYTE name_length = 0;

	iStream.read(name_length);
	if (name_length > MAX_NAME_TOTAL)
		throw InvalidProtocolException("GCPartyLeave: expeller name too long");
	if (name_length > 0)
		iStream.read(m_Expeller, name_length);

	const int nameBudgetLeft = MAX_NAME_TOTAL - name_length;

	iStream.read(name_length);
	if (name_length > nameBudgetLeft)
		throw InvalidProtocolException("GCPartyLeave: expellee name too long");
	if (name_length > 0)
		iStream.read(m_Expellee, name_length);

	__END_CATCH
}
		    
void GCPartyLeave::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY
	
	BYTE name_length = 0;

	name_length = static_cast<BYTE>(m_Expeller.size());
	oStream.write(name_length);
	if (name_length > 0)
		oStream.write(m_Expeller);

	name_length = static_cast<BYTE>(m_Expellee.size());
	oStream.write(name_length);
	if (name_length > 0)
		oStream.write(m_Expellee);

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string GCPartyLeave::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "GCPartyLeave("
		<< "Expeller:" << m_Expeller
		<< "Expellee:" << m_Expellee
		<< ")";
	return msg.toString();
		
	__END_CATCH
}


#endif