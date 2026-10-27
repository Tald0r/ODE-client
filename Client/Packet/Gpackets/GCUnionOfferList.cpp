//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCUnionOfferList.cpp 
// Written By  : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "GCUnionOfferList.h"
#include <memory>

#define SAFE_DELETE(x)		{ if(x!=NULL) delete x; x=NULL; }
//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
GCUnionOfferList::~GCUnionOfferList()
{
    while (!m_UnionOfferList.empty())
    {
        SingleGuildUnionOffer* pUnionOffer = m_UnionOfferList.front();

        SAFE_DELETE(pUnionOffer);

        m_UnionOfferList.pop_front();
    }

    m_UnionOfferList.clear();

}

PacketSize_t GCUnionOfferList::getPacketSize() const
{
	PacketSize_t ret = szBYTE;

	std::list<SingleGuildUnionOffer*>::const_iterator itr = m_UnionOfferList.begin();

	for ( ; itr != m_UnionOfferList.end() ; ++itr )
	{
		ret += (*itr)->getSize();
	}

	return ret;
}

//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void GCUnionOfferList::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	BYTE szNum;
	iStream.read(szNum);
	for ( int i=0; i<szNum; i++ )
	{
		// Owned until the list takes it, so a read that throws frees it.
		std::unique_ptr<SingleGuildUnionOffer> pInfo( new SingleGuildUnionOffer );
		pInfo->read(iStream);
		m_UnionOfferList.push_back(pInfo.get());
		pInfo.release();
	}

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void GCUnionOfferList::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	BYTE szNum = static_cast<BYTE>(m_UnionOfferList.size());
	oStream.write(szNum);

	std::list<SingleGuildUnionOffer*>::const_iterator itr = m_UnionOfferList.begin();

	for ( ; itr != m_UnionOfferList.end() ; ++itr )
	{
		(*itr)->write(oStream);
	}

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
string GCUnionOfferList::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	
	msg << "GCUnionOfferList("
		<< ")";
	
	return msg.toString();
		
	__END_CATCH
}
#endif
