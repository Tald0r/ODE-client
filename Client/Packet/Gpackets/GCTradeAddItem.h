////////////////////////////////////////////////////////////////////////////////
// Filename    : GCTradeAddItem.h 
// Written By  : 김성민
// Description : 
////////////////////////////////////////////////////////////////////////////////

#ifndef __GC_TRADE_ADD_ITEM_H__
#define __GC_TRADE_ADD_ITEM_H__

#include "Packet.h"
#include "PacketFactory.h"
#include "SubItemInfo.h"
#include <list>

////////////////////////////////////////////////////////////////////////////////
//
// class GCTradeAddItem;
//
////////////////////////////////////////////////////////////////////////////////

class GCTradeAddItem : public Packet 
{
public:
	GCTradeAddItem();
	~GCTradeAddItem();
	
public:
	void read(SocketInputStream & iStream);
	void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_GC_TRADE_ADD_ITEM; }
	PacketSize_t getPacketSize() const 
	{ 
		PacketSize_t size = 0;
		size += szObjectID;                           // m_TargetObjectID
		size += szCoordInven;                         // m_X
		size += szCoordInven;                         // m_Y
		size += szObjectID;                           // m_ItemObjectID
		size += szBYTE;                               // m_ItemClass
		size += szItemType;                           // m_ItemType
		size = static_cast<PacketSize_t>(size + (szBYTE + m_OptionType.size()));         // m_OptionType
		size += szDurability;                         // m_Durability
		size += szItemNum;                            // m_ItemNum
		size += szSilver;                             // silver coating amount
		size += szGrade;                             // grade
		size += szEnchantLevel;                       // enchant level
		size += szBYTE;                               // m_ListNum
		size +=(SubItemInfo::getSize()* m_ListNum); // list<SubItemInfo*> m_InfoList;
		return size;
	}

#ifdef __DEBUG_OUTPUT__
	std::string getPacketName() const { return "GCTradeAddItem"; }
	std::string toString() const;
#endif

public:
	ObjectID_t getTargetObjectID() const noexcept { return m_TargetObjectID; }
	void setTargetObjectID(ObjectID_t id) noexcept { m_TargetObjectID = id; }

	CoordInven_t getX() const noexcept { return m_X; }
	void setX(CoordInven_t x) noexcept { m_X = x; }

	CoordInven_t getY() const noexcept { return m_Y; }
	void setY(CoordInven_t y) noexcept { m_Y = y; }

	ObjectID_t getItemObjectID() const noexcept { return m_ItemObjectID; }
	void setItemObjectID(ObjectID_t id) noexcept { m_ItemObjectID = id; }

	BYTE getItemClass() const noexcept { return m_ItemClass; }
	void setItemClass(BYTE IClass) noexcept { m_ItemClass = IClass; }

	ItemType_t getItemType() const noexcept { return m_ItemType; }
	void setItemType(ItemType_t itemType) { m_ItemType = itemType; }

	int getOptionTypeSize() const { return static_cast<int>(m_OptionType.size()); }
	const std::list<OptionType_t>& getOptionType() const { return m_OptionType; }
	OptionType_t popOptionType()
	{
		if (m_OptionType.empty()) return 0;
		OptionType_t optionType = m_OptionType.front();
		m_OptionType.pop_front();
		return optionType;
	}
	void addOptionType(OptionType_t otype) { m_OptionType.push_back( otype ); }
	void setOptionType(const std::list<OptionType_t>& OptionTypes) { m_OptionType = OptionTypes; }

	Durability_t getDurability() const noexcept { return m_Durability; }
	void setDurability(Durability_t dur) noexcept { m_Durability = dur; }

	ItemNum_t getItemNum() const noexcept { return m_ItemNum; }
	void setItemNum(ItemNum_t itemNum) { m_ItemNum = itemNum; }

	Silver_t getSilver() const noexcept { return m_Silver; }
	void setSilver(Silver_t amount) noexcept { m_Silver = amount; }

	Grade_t getGrade() const noexcept { return m_Grade; }
	void setGrade(Grade_t grade) noexcept { m_Grade = grade; }

	EnchantLevel_t getEnchantLevel() const noexcept { return m_EnchantLevel; }
	void setEnchantLevel(EnchantLevel_t level) noexcept { m_EnchantLevel = level; }

	BYTE getListNum() const noexcept { return m_ListNum; }
	void setListNum(BYTE num) noexcept { m_ListNum = num; }

	void addListElement(SubItemInfo* pInfo) { m_InfoList.push_back(pInfo); }
	void clearList() { m_InfoList.clear(); m_ListNum = 0; }

	SubItemInfo* popListElement() 
	{
		SubItemInfo* pInfo = m_InfoList.front();
		m_InfoList.pop_front();
		return pInfo;
	}

private:
	ObjectID_t         m_TargetObjectID;  // 교환을 하고 있는 상대방의 OID
	ObjectID_t         m_ItemObjectID;    // 아이템 OID
	CoordInven_t       m_X;               // 인벤토리에서의 X 좌표
	CoordInven_t       m_Y;               // 인벤토리에서의 Y 좌표
	BYTE               m_ItemClass;       // 아이템 클래스
	ItemType_t         m_ItemType;        // 아이템 타입
	std::list<OptionType_t> m_OptionType;      // 옵션 타입
	Durability_t       m_Durability;      // 내구도
	ItemNum_t          m_ItemNum;         // 아이템 숫자
	Silver_t           m_Silver;          // silver coating amount
	Grade_t            m_Grade;           // grade
	EnchantLevel_t     m_EnchantLevel;    // enchant level
	BYTE               m_ListNum;         // 벨트일 경우, 안에 있는 아이템의 숫자
	std::list<SubItemInfo*> m_InfoList;        // 벨트일 경우, 안에 있는 아이템의 정보

};


////////////////////////////////////////////////////////////////////////////////
//
// class GCTradeAddItemFactory;
//
////////////////////////////////////////////////////////////////////////////////

class GCTradeAddItemFactory : public PacketFactory 
{
public:
	Packet* createPacket() { return new GCTradeAddItem(); }
	std::string getPacketName() const { return "GCTradeAddItem"; }
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_TRADE_ADD_ITEM; }
	PacketSize_t getPacketMaxSize() const 
	{ 
		PacketSize_t size = 0;
		size += szObjectID;                   // m_TargetObjectID
		size += szCoordInven;                 // m_X
		size += szCoordInven;                 // m_Y
		size += szObjectID;                   // m_ItemObjectID
		size += szBYTE;                       // m_ItemClass
		size += szItemType;                   // m_ItemType
		size += szBYTE + 255;                 // m_OptionType
		size += szDurability;                 // m_Durability
		size += szItemNum;                    // m_ItemNum
		size += szSilver;                     // silver coating amount
		size += szGrade;                      // Grade coating amount
		size += szEnchantLevel;               // enchant level
		size += szBYTE;                       // m_ListNum
		size +=(SubItemInfo::getSize()* 8); // list<SubItemInfo*> m_InfoList;
		return size;
	}
};


////////////////////////////////////////////////////////////////////////////////
//
// class GCTradeAddItemHandler;
//
////////////////////////////////////////////////////////////////////////////////

class GCTradeAddItemHandler 
{
public:
	static void execute(GCTradeAddItem* pPacket, Player* pPlayer);

};

#endif
