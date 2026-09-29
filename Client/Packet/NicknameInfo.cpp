#include "Client_PCH.h"
#include <fstream>
#include "NicknameInfo.h"
#include "Packet/PacketAssert.h"
NicknameInfo::NicknameInfo()
{
	m_NicknameID = 0;
	m_NicknameType = 0;
	m_NicknameIndex = 0;
}
PacketSize_t NicknameInfo::getSize() const
{
	switch ( m_NicknameType )
	{
		case NICK_NONE:
			return szWORD + szBYTE;
		case NICK_BUILT_IN:
		case NICK_QUEST:
		case NICK_FORCED:
			return szWORD + szBYTE + szWORD;
		case NICK_CUSTOM_FORCED:
		case NICK_CUSTOM:
			return static_cast<PacketSize_t>(szWORD + szBYTE + szBYTE + m_Nickname.size());
		default:
			assert(false);
	}

	return 0;
}

void NicknameInfo::read(SocketInputStream& iStream)
{
	__BEGIN_TRY

	iStream.read( m_NicknameID );
	iStream.read( m_NicknameType );

	switch ( m_NicknameType )
	{
		case NICK_NONE:
			{
				break;
			}
		case NICK_BUILT_IN:
		case NICK_QUEST:
		case NICK_FORCED:
			{
				iStream.read( m_NicknameIndex );
				break;
			}
		case NICK_CUSTOM_FORCED:
		case NICK_CUSTOM:
			{
				BYTE szSTR;
				iStream.read( szSTR );
				if ( szSTR != 0 ) iStream.read( m_Nickname, szSTR );
				break;
			}
		default:
			// The type is a wire byte and can hold any value: refuse an
			// unknown one as a protocol violation, as the server's copy does.
			throw InvalidProtocolException("nickname type out of range");
	}

	__END_CATCH
}

void NicknameInfo::write(SocketOutputStream& oStream) const
{
	__BEGIN_TRY

	oStream.write( m_NicknameID );
	oStream.write( m_NicknameType );

	switch ( m_NicknameType )
	{
		case NICK_NONE:
			{
				break;
			}
		case NICK_BUILT_IN:
		case NICK_QUEST:
		case NICK_FORCED:
			{
				oStream.write( m_NicknameIndex );
				break;
			}
		case NICK_CUSTOM_FORCED:
		case NICK_CUSTOM:
			{
				BYTE szSTR = static_cast<BYTE>(m_Nickname.size());
				oStream.write( szSTR );
				if ( szSTR != 0 ) oStream.write( m_Nickname );
				break;
			}
		default:
			assert(false);
	}

	__END_CATCH
}
