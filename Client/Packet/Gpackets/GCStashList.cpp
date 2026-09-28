//////////////////////////////////////////////////////////////////////////////
// Filename    : GCStashList.cpp 
// Written By  : 김성민
// Description : 
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "GCStashList.h"
#include "PacketAssert.h"

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////

GCStashList::GCStashList()
{
	__BEGIN_TRY
 
	for (int r=0; r<STASH_RACK_MAX; r++)
	{
		for (int i=0; i<STASH_INDEX_MAX; i++)
		{
			m_bExist[r][i]       = false;
			m_SubItemsCount[r][i] = 0;
		}
	}

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
GCStashList::~GCStashList()
{
	
	for (int r=0; r<STASH_RACK_MAX; r++)
	{
		for (int i=0; i<STASH_INDEX_MAX; i++)
		{
			std::list<SubItemInfo*>::iterator itr = m_pSubItems[r][i].begin();
			for (; itr != m_pSubItems[r][i].end(); itr++)
			{
				SubItemInfo* pItemInfo = *itr;
				delete pItemInfo;
			}
		}
	}

}

//////////////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////////////
void GCStashList::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	BYTE i      = 0;
	BYTE nTotal = 0;
	BYTE rack   = 0;
	BYTE index  = 0;
	
	// 보관함의 갯수를 읽어들인다.
	iStream.read(m_StashNum);

	// 총 아이템의 숫자를 읽어들인다.
	iStream.read(nTotal);

	// 각 아이템의 정보를 읽어들인다.
	for (i=0; i<nTotal; i++)
	{
		
		iStream.read(rack);
		iStream.read(index);

		// rack and index arrive as raw BYTEs and address four parallel arrays
		// dimensioned [STASH_RACK_MAX][STASH_INDEX_MAX], i.e. [3][20]. Unchecked,
		// rack=index=255 reaches 255*20+255 elements past the base, and the writes
		// below include m_pSubItems[rack][index].push_back(), which would run
		// std::list's members over that memory.
		if (rack >= STASH_RACK_MAX || index >= STASH_INDEX_MAX)
			throw InvalidProtocolException("GCStashList: stash rack or index out of range");

		_STASHITEM& item = m_pItems[rack][index];

		iStream.read(item.objectID);
		iStream.read(item.itemClass);
		iStream.read(item.itemType);

		BYTE optionSize;
		iStream.read(optionSize);
		for (int j=0; j<optionSize; j++)
		{
			OptionType_t optionType;
			iStream.read(optionType);
			item.optionType.push_back( optionType );
		}

		iStream.read(item.durability);
		iStream.read(item.num);
		iStream.read(item.silver);
		iStream.read(item.grade);
		iStream.read(item.enchantLevel);
		
		// sub 아이템 정보를 읽어들인다.
		iStream.read( m_SubItemsCount[rack][index] );
		for (int s=0; s < m_SubItemsCount[rack][index]; s++)
		{
			SubItemInfo* pSubItemInfo = new SubItemInfo();
			pSubItemInfo->read(iStream);
			m_pSubItems[rack][index].push_back(pSubItemInfo);
		}
		
		m_bExist[rack][index] = true;
	}

	// 돈의 양을 읽어들인다.
	iStream.read(m_StashGold);

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////////////
void GCStashList::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY
	__BEGIN_DEBUG
		
	BYTE r = 0;
	BYTE i = 0;
	BYTE nTotal = 0;
	
	// 총 아이템의 숫자를 계산한다.
	for (r=0; r<STASH_RACK_MAX; r++)
		for (i=0; i<STASH_INDEX_MAX; i++)
			if (m_bExist[r][i]) nTotal++;
		
	// 보관함의 갯수를 날려준다.
	oStream.write(m_StashNum);

	// 총 아이템의 숫자를 날려준다
	oStream.write(nTotal);

	// write each item info
	for (r=0; r<STASH_RACK_MAX; r++)
	{
		for (i=0; i<STASH_INDEX_MAX; i++)
		{
			if (m_bExist[r][i])
			{
				const _STASHITEM& item = m_pItems[r][i];
				oStream.write(r);
				oStream.write(i);
				oStream.write(item.objectID);
				oStream.write(item.itemClass);
				oStream.write(item.itemType);

				BYTE optionSize = static_cast<BYTE>(item.optionType.size());
				oStream.write(optionSize);
				std::list<OptionType_t>::const_iterator iOption;
				for (iOption=item.optionType.begin(); iOption!=item.optionType.end(); iOption++)
				{
					OptionType_t optionType = *iOption;
					oStream.write(optionType);
				}

				oStream.write(item.durability);
				oStream.write(item.num);
				oStream.write(item.silver);
				oStream.write(item.grade);
				oStream.write(item.enchantLevel);

				// sub 아이템 정보를 쓴다.
				oStream.write(m_SubItemsCount[r][i]);

				std::list<SubItemInfo*>::const_iterator itr = m_pSubItems[r][i].begin();
				for (; itr != m_pSubItems[r][i].end(); itr++)
				{
					if (*itr) (*itr)->write(oStream);
				}
			}
		}
	}

	// 돈의 양을 써준다.
	oStream.write(m_StashGold);
	
	__END_DEBUG
	__END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
PacketSize_t GCStashList::getPacketSize () const
{ 
	__BEGIN_TRY
	__BEGIN_DEBUG

	PacketSize_t size = szBYTE; // 보관함의 갯수

	size += szBYTE; // 총 아이템 숫자

	for (int r=0; r<STASH_RACK_MAX; r++)
	{
		for (int i=0; i<STASH_INDEX_MAX; i++)
		{
			if (m_bExist[r][i]) 
			{
				// rack과 인덱스
				size += szBYTE*2; 

				// 실제 정보
				/*
				size += szObjectID;
				size += szBYTE;
				size += szItemType;
				size += szBYTE + optionType.size();
				size += szDurability;
				size += szItemNum;
				size += szSilver;
				size += szGrade;
				size += szEnchantLevel;
				*/
				size += m_pItems[r][i].getPacketSize();

				// 벨트에 들어있는 아이템의 숫자
				size += szBYTE;

				// 벨트에 들어 있는 아이템의 크기
				size += SubItemInfo::getSize() * m_SubItemsCount[r][i];
			}
		}
	}

	size += szGold; // 보관함에 들어있는 돈

	return size;

	__END_DEBUG
	__END_CATCH
}


#ifdef __DEBUG_OUTPUT__	
//////////////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////////////
std::string GCStashList::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "GCStashList(";
	for (int r=0; r<STASH_RACK_MAX; r++)
	{
		for (int i=0; i<STASH_INDEX_MAX; i++)
		{
			msg << "(Item:" << r << ", " << i << ":";
			if (m_bExist[r][i])
			{
				msg << "ObjectID:"     << (int)(m_pItems[r][i].objectID)
					<< "ItemClass:"    << (int)(m_pItems[r][i].itemClass) 
					<< "ItemType:"     << (int)(m_pItems[r][i].itemType)
					<< "OptionTypeSize:"   << (int)(m_pItems[r][i].optionType.size())
					<< "Durability:"   << (int)(m_pItems[r][i].durability)
					<< "Num:"          << (int)(m_pItems[r][i].num)
					<< "Silver:"       << (int)(m_pItems[r][i].silver)
					<< "Grade:"        << (int)(m_pItems[r][i].grade)
					<< "EnchantLevel:" << (int)(m_pItems[r][i].enchantLevel);
			}
			msg << ")";
		}
	}
	msg << ")";
	return msg.toString();
		
	__END_CATCH
}
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
bool GCStashList::isExist(BYTE rack, BYTE index) const
{
	__BEGIN_TRY

	// Assert reports the caller's mistake in a debug build, but compiles to
	// nothing under NDEBUG, so the read stays in range on its own terms too.
	Assert(rack < STASH_RACK_MAX && index < STASH_INDEX_MAX);

	if (rack >= STASH_RACK_MAX || index >= STASH_INDEX_MAX)
		return false;

	return m_bExist[rack][index];

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
STASHITEM GCStashList::getStashItem(BYTE rack, BYTE index) const
{
	__BEGIN_TRY

	Assert(rack < STASH_RACK_MAX && index < STASH_INDEX_MAX);

	if (rack >= STASH_RACK_MAX || index >= STASH_INDEX_MAX)
		return STASHITEM();

	return m_pItems[rack][index];

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
std::list<SubItemInfo*>& GCStashList::getSubItems(BYTE rack, BYTE index)
{
	__BEGIN_TRY

	Assert(rack < STASH_RACK_MAX && index < STASH_INDEX_MAX);
	return m_pSubItems[rack][index];

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
BYTE GCStashList::getSubItemCount(BYTE rack, BYTE index)
{
	__BEGIN_TRY

	Assert(rack < STASH_RACK_MAX && index < STASH_INDEX_MAX);
	return m_SubItemsCount[rack][index];

	__END_CATCH
}


