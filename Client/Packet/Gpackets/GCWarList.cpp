//////////////////////////////////////////////////////////////////////////////
// Filename    : GCWarList.cpp 
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "GCWarList.h"
#include "RaceWarInfo.h"
#include "LevelWarInfo.h"
#include "GuildWarInfo.h"
#include "PacketAssert.h"

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
GCWarList::GCWarList()
{
	__BEGIN_TRY 

	__END_CATCH;
}

//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
GCWarList::~GCWarList()
{

	clear();

}

//////////////////////////////////////////////////////////////////////////////
// clear
//////////////////////////////////////////////////////////////////////////////
void
GCWarList::clear()
{
	__BEGIN_TRY 

	WarInfoList::iterator itr = m_WarInfos.begin();

	for( ; itr != m_WarInfos.end() ; itr++ )
	{
		WarInfo* pWarInfo = *itr;

		if(pWarInfo != NULL)
		{
			delete pWarInfo;
			pWarInfo = NULL;
		}
	}

	m_WarInfos.clear();
	
	__END_CATCH;
}

//////////////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////////////
void GCWarList::read (SocketInputStream & iStream)
{
	__BEGIN_TRY

	BYTE count = 0;
	iStream.read( count );

	WarType_t warType;
	for( int i=0; i<count; i++ )
	{
		WarInfo* pWarInfo = NULL;
		iStream.read( warType );

		switch ((WarType)warType)
		{
			case WAR_GUILD :
				pWarInfo = new GuildWarInfo;
			break;

			case WAR_RACE :
				pWarInfo = new RaceWarInfo;
			break;

			case WAR_LEVEL :
				pWarInfo = new LevelWarInfo;
			break;

			default :
				throw Error("wrong WarType");
		}

		pWarInfo->read( iStream );

		addWarInfo( pWarInfo );
	}

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////////////
void GCWarList::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

	// At most 24 wars, the server's bound. The count is checked on the
	// list's own size, before the narrowing to the BYTE that goes on
	// the wire: 256 wars would narrow to 0 ahead of every entry.
	if ( m_WarInfos.size() > 24 )
		throw InvalidProtocolException("too many wars");

	BYTE count = static_cast<BYTE>(m_WarInfos.size());
		
	oStream.write( count );

	WarInfoListItor itr = m_WarInfos.begin();

	for( ; itr != m_WarInfos.end(); itr++ )
	{
		WarInfo* pWarInfo = *itr;

		WarType_t warType = pWarInfo->getWarType();
		oStream.write( warType );

		pWarInfo->write( oStream );
	}

	__END_CATCH
}


PacketSize_t GCWarList::getPacketSize () const
{ 
	__BEGIN_TRY

	PacketSize_t size = szBYTE;

	WarInfoListItor itr = m_WarInfos.begin();
	
	for( ; itr != m_WarInfos.end(); itr++ )
	{
		WarInfo* pWarInfo = *itr;
		size += szWarType;
		size += pWarInfo->getSize();
	}
		
	return size;

	__END_CATCH
}


#ifdef __DEBUG_OUTPUT__
//////////////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////////////
std::string GCWarList::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	
	msg << "GCWarList(" 
		<< "WarNum : " << m_WarInfos.size();

	WarInfoListItor itr = m_WarInfos.begin();

	for( ; itr != m_WarInfos.end(); itr++ )
	{
		WarInfo* pWarInfo = *itr;
		msg << pWarInfo->toString();
	}

	msg << ")";

	return msg.toString();
		
	__END_CATCH
}
#endif

WarInfo* GCWarList::popWarInfo()
{
	__BEGIN_TRY

	if( m_WarInfos.empty() ) return NULL;

	WarInfo* pWarInfo = m_WarInfos.front();
	m_WarInfos.pop_front();

	return pWarInfo;

	__END_CATCH
}

void GCWarList::operator = (const GCWarList& WL)
{
	clear();

	WarInfoListItor itr = WL.m_WarInfos.begin();

	for( ; itr != WL.m_WarInfos.end(); itr++ )
	{
		WarInfo* pWarInfo = *itr;
		WarInfo* pNewWarInfo = NULL;

		switch (pWarInfo->getWarType())
		{
			case WAR_GUILD 	: 
			{
				GuildWarInfo* pGWI = dynamic_cast<GuildWarInfo*>(pWarInfo);
				Assert(pGWI!=NULL);

				GuildWarInfo* pNewGWI = new GuildWarInfo; 	
				*pNewGWI = *pGWI;
				pNewWarInfo = pNewGWI;
			}
			break;

			case WAR_RACE 	: 
			{
				RaceWarInfo* pRWI = dynamic_cast<RaceWarInfo*>(pWarInfo);
				Assert(pRWI!=NULL);

				RaceWarInfo* pNewRWI = new RaceWarInfo; 	
				*pNewRWI = *pRWI;
				pNewWarInfo = pNewRWI;
			}
			break;

			case WAR_LEVEL	:
			{
				LevelWarInfo* pLWI = dynamic_cast<LevelWarInfo*>(pWarInfo);
				Assert(pLWI!=NULL);

				LevelWarInfo* pNewLWI = new LevelWarInfo;
				*pNewLWI = *pLWI;
				pNewWarInfo = pNewLWI;
			}
			break;

			default :
				throw Error("wrong WarType");
		}

		addWarInfo( pNewWarInfo );
//		cout << "GCWarList::operator = New ()" << pNewWarInfo->getStartTime() << endl;
//		cout << "GCWarList::operator = Ori ()" << pWarInfo->getStartTime() << endl;
	}
}

