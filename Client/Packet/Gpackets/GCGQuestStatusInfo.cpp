//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCGQuestStatusInfo.cpp 
// Written By  : elca@ewestsoft.com
// Description : 자신에게 쓰는 기술의 성공을 알리기 위한 패킷 클래스의
//               멤버 정의.
// 
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "GCGQuestStatusInfo.h"

#include <numeric>

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GCGQuestStatusInfo::GCGQuestStatusInfo ()
{
	__BEGIN_TRY
	__END_CATCH
}

	
//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
GCGQuestStatusInfo::~GCGQuestStatusInfo ()
{

	std::list<QuestStatusInfo*>::iterator itr = m_Infos.begin();

	for ( ; itr != m_Infos.end() ; ++itr )
	{
		delete *itr;
		*itr = NULL;
	}

}


//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void GCGQuestStatusInfo::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	m_Infos.clear();
	BYTE size;
	iStream.read(size);

	for ( int i=0; i<size; i++ )
	{
		QuestStatusInfo* pInfo = new QuestStatusInfo;
		pInfo->read(iStream);
		m_Infos.push_back(pInfo);
	}

	
	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void GCGQuestStatusInfo::write ( SocketOutputStream & oStream ) 
     const
{
	__BEGIN_TRY

	BYTE size = m_Infos.size();
	oStream.write(size);

	std::list<QuestStatusInfo*>::const_iterator itr = m_Infos.begin();

	for ( ; itr != m_Infos.end(); ++itr )
	{
		(*itr)->write(oStream);
	}
		
	__END_CATCH
}

PacketSize_t	addSize( PacketSize_t tot, const QuestStatusInfo* pInfo )
{
	return tot + pInfo->getSize();
}

PacketSize_t GCGQuestStatusInfo::getPacketSize() 
const 
{ 
	return std::accumulate( m_Infos.begin(), m_Infos.end(), szBYTE, addSize ); 
}

//////////////////////////////////////////////////////////////////////
//
// get packet's debug string
//
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
string GCGQuestStatusInfo::toString () 
	const
{
	__BEGIN_TRY

	StringStream msg;
	msg << "GCGQuestStatusInfo("
		<< ")";
	return msg.toString();

	__END_CATCH
}
#endif

