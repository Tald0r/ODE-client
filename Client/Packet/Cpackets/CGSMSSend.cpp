//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSMSSend.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGSMSSend.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "PacketAssert.h"


void CGSMSSend::read (SocketInputStream & iStream)
{
	__BEGIN_TRY

	BYTE size;

	iStream.read(size);
	Assert( size < MAX_RECEVIER_NUM );

	m_Numbers.clear();
	for ( int i=0; i<size; ++i )
	{
		BYTE strSize;
		string number;
		iStream.read(strSize);
		Assert( strSize < MAX_NUMBER_LENGTH );
		iStream.read(number, strSize);
	}

	iStream.read(size);
	Assert( size < MAX_NUMBER_LENGTH );
	iStream.read(m_CallerNumber, size);

	iStream.read(size);
	Assert( size < MAX_MESSAGE_LENGTH );
	iStream.read(m_Message, size);
		
	__END_CATCH
}

void CGSMSSend::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

	// Cap before narrowing; the numbers follow the UI, the message read().
	if ( m_Numbers.size() > MAX_RECEVIER_NUM )
		throw InvalidProtocolException("too large number list size");

	BYTE size;

	size = (BYTE)m_Numbers.size();
	oStream.write(size);

	std::list<string>::const_iterator itr = m_Numbers.begin();
	std::list<string>::const_iterator endItr = m_Numbers.end();

	for ( ; itr != endItr ; ++itr )
	{
		if ( itr->size() > MAX_NUMBER_LENGTH )
			throw InvalidProtocolException("too large number length");

		size = (BYTE)itr->size();
		oStream.write(size);
		oStream.write(std::span<const char>(itr->data(), size));
	}

	if ( m_CallerNumber.size() > MAX_NUMBER_LENGTH )
		throw InvalidProtocolException("too large caller number length");

	size = (BYTE)m_CallerNumber.size();
	oStream.write(size);
	oStream.write(std::span<const char>(m_CallerNumber.data(), size));

	if ( m_Message.size() >= MAX_MESSAGE_LENGTH )
		throw InvalidProtocolException("too large message length");

	size = (BYTE)m_Message.size();
	oStream.write(size);
	oStream.write(std::span<const char>(m_Message.data(), size));

	__END_CATCH
}

PacketSize_t CGSMSSend::getPacketSize() const
{
	__BEGIN_TRY

	PacketSize_t ret = szBYTE;

	std::list<string>::const_iterator itr = m_Numbers.begin();
	std::list<string>::const_iterator endItr = m_Numbers.end();

	for ( ; itr != endItr ; ++itr )
	{
		ret = static_cast<PacketSize_t>(ret + (szBYTE + itr->size()));
	}

	ret = static_cast<PacketSize_t>(ret + (szBYTE + m_CallerNumber.size()));
	ret = static_cast<PacketSize_t>(ret + (szBYTE + m_Message.size()));

	return ret;

	__END_CATCH
}

string CGSMSSend::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGSMSSend("
		<< ")";
	return msg.toString(); 

	__END_CATCH
}
