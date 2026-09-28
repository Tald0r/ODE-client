//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCMiniGameScores.cpp 
// Written By  : elca@ewestsoft.com
// Description : 자신에게 쓰는 기술의 성공을 알리기 위한 패킷 클래스의
//               멤버 정의.
// 
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "GCMiniGameScores.h"

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GCMiniGameScores::GCMiniGameScores ()
{
	__BEGIN_TRY
	__END_CATCH
}

	
//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
GCMiniGameScores::~GCMiniGameScores ()
{
}


//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void GCMiniGameScores::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	iStream.read(m_GameType);
	iStream.read(m_Level);

	BYTE count;
	iStream.read(count);

	for ( BYTE i=0; i<count; ++i )
	{
		BYTE len;
		iStream.read(len);
		std::string name;
		iStream.read(name, len);
		WORD score;
		iStream.read(score);

		addScore(name,score);
	}
		
	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void GCMiniGameScores::write ( SocketOutputStream & oStream ) 
     const
{
	__BEGIN_TRY

	oStream.write(m_GameType);
	oStream.write(m_Level);

	// At most 10 scores go out; clamp before narrowing to the count byte.
	const uint count = m_Scores.size() > 10 ? 10 : static_cast<uint>(m_Scores.size());
	
	oStream.write( static_cast<BYTE>(count) );

	std::list<std::pair<std::string,WORD> >::const_iterator itr = m_Scores.begin();

	for ( uint i=0; i<count; ++i )
	{
		// A name is at most 20 bytes, the bound the factory's max size budgets.
		if ( (*itr).first.size() > 20 )
			throw InvalidProtocolException( "too long MiniGameName length" );

		oStream.write( static_cast<BYTE>((*itr).first.size()) );
		oStream.write( (*itr).first );
		oStream.write( (*itr).second );
		itr++;
	}
		
	__END_CATCH
}

PacketSize_t GCMiniGameScores::getPacketSize() const
{
	PacketSize_t ret = szBYTE + szBYTE + szBYTE;

	const uint count = m_Scores.size() > 10 ? 10 : static_cast<uint>(m_Scores.size());

	std::list<std::pair<std::string,WORD> >::const_iterator itr = m_Scores.begin();

	for ( uint i=0; i<count; ++i )
	{
		ret = static_cast<PacketSize_t>(ret + (szBYTE + (*itr).first.size() + szWORD));
		itr++;
	}
	return ret;
}


#ifdef __DEBUG_OUTPUT__

//////////////////////////////////////////////////////////////////////
//
// get packet's debug string
//
//////////////////////////////////////////////////////////////////////
std::string GCMiniGameScores::toString () 
	const
{
	__BEGIN_TRY

	StringStream msg;
	msg << "GCMiniGameScores("
		<< ")";
	return msg.toString();

	__END_CATCH
}

#endif