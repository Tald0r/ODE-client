//----------------------------------------------------------------------
// MItemUse.cpp - the executable half of the item classes
//----------------------------------------------------------------------
// Split from MItem.cpp (docs/RESTRUCTURING.md task 4.4): the factory
// table MItem::NewItem dispatches through and every item class with a
// UseInventory/UseQuickItem/UseGear body, because those reach packets,
// the player, the zone and the dialogs (MCorpse too: it owns an
// MCreature). Such a class moves here whole: if the library constructed
// it, its vtable there would reference symbols a test binary cannot
// link. (MBomb and MHolyWater keep GetMaxNumber in the core with
// NewItem here, and MItem::NewItem is here - safe because the core
// constructs none of them.) MItem.cpp keeps the model half in
// gamemodel, the container-based gear (belt, arms band, motorcycle)
// included since the item managers joined it (task 4.3). Bodies are
// moved verbatim (the always-true __GAME_CLIENT__ guards they carried
// went with task 5.2's tenth slice, which retired the macro).
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MItem.h"
#include "MItemTable.h"
#include "MItemOptionTable.h"
#include "MGameStringTable.h"
#include "ClientConfig.h"
#include "MCreatureTable.h"
#include "MFakeCreature.h"
#include "MItemLimits.h"

	#include "Client.h"
	#include "SkillDef.h"
	#include "MSkillManager.h"
	#include "ClientFunction.h"
	#include "MGameStringTable.h"
	#include "MInventory.h"
	#include "MSlayerGear.h"
	#include "ServerInfo.h"
	#include "UIFunction.h"
	#include "MZone.h"
	#include "MZoneTable.h"
	#include "MPlayer.h"
#include "Packet/Cpackets/CGUseItemFromGear.h"
	#include "Packet/Cpackets/CGUsePotionFromInventory.h"
	#include "Packet/Cpackets/CGUsePotionFromQuickSlot.h"

	#include "Fl2.h"
	#include "VS_UI_GameCommon.h"
	#include "VS_UI.h"
	#include "UIDialog.h"

#include <fstream>
#include <algorithm>

	#include "DebugInfo.h"
	#include "MCreature.h"
	#include "MTopView.h"	

extern bool g_bHolyLand;
extern bool g_bZoneSafe;


//----------------------------------------------------------------------
// The factory table: one NewItem per item class, indexed by ITEM_CLASS.
//----------------------------------------------------------------------
MItem::FUNCTION_NEWITEMCLASS
MItem::s_NewItemClassTable[MAX_ITEM_CLASS] =
{
	MMotorcycle::NewItem,
	MPotion::NewItem,
	MWater::NewItem,
	MHolyWater::NewItem,
	MMagazine::NewItem,
	MBombMaterial::NewItem,
	MItemETC::NewItem,
	MKey::NewItem,
	MRing::NewItem,
	MBracelet::NewItem,
	MNecklace::NewItem,
	MCoat::NewItem,
	MTrouser::NewItem,
	MShoes::NewItem,
	MSword::NewItem,
	MBlade::NewItem,
	MShield::NewItem,
	MCross::NewItem,
	MGlove::NewItem,
	MHelm::NewItem,
	MGunSG::NewItem,
	MGunSMG::NewItem,
	MGunAR::NewItem,
	MGunTR::NewItem,
	MBomb::NewItem,
	MMine::NewItem,
	MBelt::NewItem,
	MLearningItem::NewItem,
	MMoney::NewItem,
	MCorpse::NewItem,
	//MSkull::NewItem,

	// vampire용 item
	MVampireRing::NewItem,
	MVampireBracelet::NewItem,
	MVampireNecklace::NewItem,
	MVampireCoat::NewItem,

	// skull
	MSkull::NewItem,

	// mace추가 - 2001/3/26
	MMace::NewItem,

	MSerum::NewItem,

	// 2001.8.21
	MVampireETC::NewItem,

	// 2001.10.22
	MSlayerPortalItem::NewItem,
	MVampirePortalItem::NewItem,

	// 2001.12.10
	MEventGiftBoxItem::NewItem,
	MEventStarItem::NewItem,

	MVampireEarRing::NewItem,

	// 2002.6.7
	MRelic::NewItem,

	// 2002.8.18
	MVampireWeapon::NewItem,
	MVampireAmulet::NewItem,

	// 2002.9.7
	MQuestItem::NewItem,

	// 2002.12.9
	MEventTreeItem::NewItem,
	MEventEtcItem::NewItem,

	// 2003.1.29
	MBloodBible::NewItem,
	
	// 2003.2.12
	MCastleSymbol::NewItem,

	MCoupleRing::NewItem,
	MVampireCoupleRing::NewItem,
	
	MEventItem::NewItem,
	MDyePotionItem::NewItem,
	MResurrectItem::NewItem,
	MMixingItem::NewItem,

	MOustersArmsBand::NewItem,
	MOustersBoots::NewItem,
	MOustersChakram::NewItem,
	MOustersCirclet::NewItem,
	MOustersCoat::NewItem,
	MOustersPendent::NewItem,
	MOustersRing::NewItem,
	MOustersStone::NewItem,
	MOustersWristlet::NewItem,

	MOustersLarva::NewItem,					// 66
	MOustersPupa::NewItem,					// 67
	MOustersComposMei::NewItem,				// 68
	MOustersSummonGem::NewItem,				// 69
	MEffectItem::NewItem,					// 70
	MCodeSheetItem::NewItem,				// 71
	MMoonCardItem::NewItem,
	
	MSweeperItem::NewItem,
	MPetItem::NewItem,
	MPetFood::NewItem,
	MPetEnchantItem::NewItem,
	MLuckyBag::NewItem,
	MSms_item::NewItem,
	MCoreZap::NewItem,
	MGQuestItem::NewItem,
	MTrapItem::NewItem,
	MBloodBibleSign::NewItem,
	MWarItem::NewItem,
		//by csm 2차 전직 아이템 클래스 추가 
	MCarryingReceiver::NewItem,
	MShoulderArmor::NewItem,
	MDermis::NewItem,
	MPersona::NewItem,
	MFascia::NewItem,
	MMitten::NewItem,
	
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
		MSubInventory::NewItem,
	#endif

};

//----------------------------------------------------------------------
// new functions
//----------------------------------------------------------------------
/*	
MItem*	MMotorcycle::NewItem()		{ return new MMotorcycle; }
MItem*	MPotion::NewItem()			{ return new MPotion; }	
MItem*	MWater::NewItem()			{ return new MWater; }
MItem*	MHolyWater::NewItem()		{ return new MHolyWater; }
MItem*	MMagazine::NewItem()		{ return new MMagazine; }
MItem*	MBombMaterial::NewItem()	{ return new MBombMaterial; }
MItem*	MItemETC::NewItem()			{ return new MItemETC; }
MItem*	MKey::NewItem()				{ return new MKey; }
MItem*	MRing::NewItem()			{ return new MRing; }
MItem*	MBracelet::NewItem()		{ return new MBracelet; }
MItem*	MNecklace::NewItem()		{ return new MNecklace; }
MItem*	MCoat::NewItem()			{ return new MCoat; }
MItem*	MTrouser::NewItem()			{ return new MTrouser; }
MItem*	MShoes::NewItem()			{ return new MShoes; }
MItem*	MSword::NewItem()			{ return new MSword; }
MItem*	MBlade::NewItem()			{ return new MBlade; }
MItem*	MShield::NewItem()			{ return new MShield; }
MItem*	MCross::NewItem()			{ return new MCross; }
MItem*	MGlove::NewItem()			{ return new MGlove; }
MItem*	MHelm::NewItem()			{ return new MHelm; }
MItem*	MGunSG::NewItem()			{ return new MGunSG; }
MItem*	MGunSMG::NewItem()			{ return new MGunSMG; }
MItem*	MGunAR::NewItem()			{ return new MGunAR; }
MItem*	MGunTR::NewItem()			{ return new MGunTR; }
MItem*	MBomb::NewItem()			{ return new MBomb; }
MItem*	MMine::NewItem()			{ return new MMine; }
MItem*	MBelt::NewItem()			{ return new MBelt; }
MItem*	MLearningItem::NewItem()	{ return new MLearningItem; }
MItem*	MMoney::NewItem()			{ return new MMoney; }
MItem*	MCorpse::NewItem()			{ return new MCorpse; }

// vampire용 item
MItem*	MVampireRing::NewItem()		{ return new MVampireRing; }
MItem*	MVampireBracelet::NewItem()	{ return new MVampireBracelet; }
MItem*	MVampireNecklace::NewItem()	{ return new MVampireNecklace; }
MItem*	MVampireCoat::NewItem()		{ return new MVampireCoat; }
MItem*	MVampireShoes::NewItem()	{ return new MVampireShoes; }
*/

//----------------------------------------------------------------------
// New Item
//----------------------------------------------------------------------
// itemClass에 맞는 class의 객체를 생성해서(new) 넘겨준다.
//----------------------------------------------------------------------
MItem*
MItem::NewItem(ITEM_CLASS itemClass)
{
	// itemClass reaches here straight from network packets, so it has to be
	// validated before indexing the function-pointer table — an out-of-range
	// value would read a code pointer past the table and call through it.
	// The NULL-entry check is cheap defence in depth should a future edit
	// leave a table slot unfilled. Every caller must handle a NULL return.
	if (itemClass < 0 || itemClass >= MAX_ITEM_CLASS
		|| s_NewItemClassTable[itemClass] == NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] MItem::NewItem: invalid item class %d", itemClass);
		return NULL;
	}

	return (MItem*)(*s_NewItemClassTable[itemClass])();
};


//----------------------------------------------------------------------
// Potion :: Get MaxNumber
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MPotion::GetMaxNumber() const	
{ 
	return MAX_POTION_PILE_NUMBER; 
}

//----------------------------------------------------------------------
// Magazine :: Get MaxNumber
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MMagazine::GetMaxNumber() const	
{ 
	return MAX_MAGAZINE_PILE_NUMBER; 
}

//----------------------------------------------------------------------
// Money :: Get MaxNumber
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MMoney::GetMaxNumber() const	
{ 
	return MAX_MONEY_NUMBER; 
}

//----------------------------------------------------------------------
// MWater - Get MaxNumber
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MWater::GetMaxNumber() const
{
	return MAX_WATER_NUMBER;
}

//----------------------------------------------------------------------
// MBombMaterial - Get MaxNumber
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MBombMaterial::GetMaxNumber() const
{
	return MAX_BOMB_MATERIAL_NUMBER;	
}

//----------------------------------------------------------------------
// MMine - Get MaxNumber
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MMine::GetMaxNumber() const
{
	return MAX_MINE_NUMBER;	
}

//----------------------------------------------------------------------
// MVampireETC - Get MaxNumber
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MVampireETC::GetMaxNumber() const
{
	return MAX_VAMPIRE_ETC_NUMBER;
}

//----------------------------------------------------------------------
// MSkull - Get MaxNumber
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MSkull::GetMaxNumber() const
{
	return MAX_SKULL_NUMBER;
}

TYPE_ITEM_NUMBER
MEventEtcItem::GetMaxNumber() const
{
	return MAX_FIRE_CRACKER_PILE_NUMBER;
}

//----------------------------------------------------------------------
//
//						MCorpse
// 
//----------------------------------------------------------------------
MCorpse::MCorpse()
{ 
	m_pCreature=NULL; 
}

MCorpse::~MCorpse() 
{ 
	if (m_pCreature!=NULL) 
	{		
		delete m_pCreature; 		
	}
}

//----------------------------------------------------------------------
//
//						MMagazine
// 
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Is Insert To Item
//----------------------------------------------------------------------
// 다른 Item에 추가될 수 있는가?
//----------------------------------------------------------------------
bool		
MMagazine::IsInsertToItem(const MItem* pItem) const
{
	if (pItem==NULL)
	{
		return false;
	}

	//------------------------------------------------
	// 총인 경우
	//------------------------------------------------
	if (pItem->IsGunItem())
	{
		// 이 탄창을 장착할 수 있는 총인 경우..
		if (GetGunClass()==pItem->GetItemClass())
		{
			return true;
		}

		return false;
	}
	
	// 같은 탄창인 경우 체크..
	return MItem::IsInsertToItem( pItem );	
}

//----------------------------------------------------------------------
// 
//						MSlayer PortalItem
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Get MaxNumber - Charge개수
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MSlayerPortalItem::GetMaxNumber() const
{
	return (*g_pItemTable)[ITEM_CLASS_SLAYER_PORTAL_ITEM][m_ItemType].MaxNumber;
}

//----------------------------------------------------------------------
// Set EnchantLevel
// 실제로는 Charge 개수를 저장한다.
//----------------------------------------------------------------------
void				
MSlayerPortalItem::SetEnchantLevel(WORD s)
{
	m_Number = s;
}

//----------------------------------------------------------------------
// 
//						MVampire PortalItem
//
//----------------------------------------------------------------------
MVampirePortalItem::MVampirePortalItem()
{ 
	m_Number = 0; 

	m_ZoneID = 0;//OBJECTID_NULL;
	m_ZoneX = SECTORPOSITION_NULL;
	m_ZoneY = SECTORPOSITION_NULL;
}

//----------------------------------------------------------------------
// Is Marked
//----------------------------------------------------------------------
bool				
MVampirePortalItem::IsMarked() const
{
	return m_ZoneID!=0 && static_cast<unsigned int>(m_ZoneID)!=OBJECTID_NULL;
}

//----------------------------------------------------------------------
// Get MaxNumber - Charge개수
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MVampirePortalItem::GetMaxNumber() const
{
	return (*g_pItemTable)[ITEM_CLASS_VAMPIRE_PORTAL_ITEM][m_ItemType].MaxNumber;
}

//----------------------------------------------------------------------
// Set Zone
//----------------------------------------------------------------------
void				
MVampirePortalItem::SetZone(int zoneID, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	m_ZoneID	= zoneID;
	m_ZoneX		= x;
	m_ZoneY		= y;
}

//----------------------------------------------------------------------
// Set EnchantLevel
// 실제로는 Charge 개수를 저장한다.
//----------------------------------------------------------------------
void				
MVampirePortalItem::SetEnchantLevel(WORD s)
{
	m_Number = s;
}

//----------------------------------------------------------------------
// Set Silver
//----------------------------------------------------------------------
void				
MVampirePortalItem::SetSilver(int s)
{
	m_ZoneID = s;
}

//----------------------------------------------------------------------
// Set Current Durability
//----------------------------------------------------------------------
void				
MVampirePortalItem::SetCurrentDurability(TYPE_ITEM_DURATION d)
{
	m_ZoneX = (d >> 8) & 0xFF;		// 0xFF는 없어도 될 듯..
	m_ZoneY = (d & 0xFF);
}

//----------------------------------------------------------------------
// Get Max Number
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MEventStarItem::GetMaxNumber() const
{
	return MAX_EVENT_STAR_PILE_NUMBER;
}

//----------------------------------------------------------------------
// Get Max Number
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MPetEnchantItem::GetMaxNumber() const
{
	return MAX_PET_ENCHANT_PILE_NUMBER;
}

//----------------------------------------------------------------------
// Get Max Number
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MPetFood::GetMaxNumber() const
{
	return MAX_PET_FOOD_PILE_NUMBER;
}
TYPE_ITEM_NUMBER	
MItemETC::GetMaxNumber() const
{
	return MAX_RESURRECT_SCROLL_NUMBER;
}

TYPE_ITEM_NUMBER
MMixingItem::GetMaxNumber() const 
{
	return MAX_MIXING_ITEM_NUMBER;
}

//----------------------------------------------------------------------
// Is BombMaterial
//----------------------------------------------------------------------
// 이거는 나중에 필살~수정이 필요하다.
// BombMaterial에 member로 넣든지.. 
// Bomb / Mine Material을 분리하든지....
//----------------------------------------------------------------------
bool
IsBombMaterial(const MItem* pItem)
{
	return pItem->GetItemClass()==ITEM_CLASS_BOMB_MATERIAL
			&& pItem->GetItemType() <= 4;
}

//----------------------------------------------------------------------
// Is MineMaterial
//----------------------------------------------------------------------
// 이거는 나중에 필살~수정이 필요하다.
// BombMaterial에 member로 넣든지.. 
// Bomb / Mine Material을 분리하든지....
//----------------------------------------------------------------------
bool
IsMineMaterial(const MItem* pItem)
{
	return pItem->GetItemClass()==ITEM_CLASS_BOMB_MATERIAL
			&& pItem->GetItemType() >= 5;
}

TYPE_ITEM_NUMBER
MOustersSummonGem::GetMaxNumber() const 
{
	return (*g_pItemTable)[ITEM_CLASS_OUSTERS_SUMMON_ITEM][m_ItemType].MaxNumber;
}

TYPE_ITEM_NUMBER
MOustersPupa::GetMaxNumber() const
{
	return MAX_OUSTERS_LARVA_NUMBER;
}

TYPE_ITEM_NUMBER
MOustersComposMei::GetMaxNumber() const
{
	return MAX_OUSTERS_LARVA_NUMBER;
}

//----------------------------------------------------------------------
// Set EnchantLevel
// 실제로는 Charge 개수를 저장한다.
//----------------------------------------------------------------------
void				
MOustersSummonGem::SetEnchantLevel(WORD s)
{
	m_Number = s;
}

TYPE_ITEM_NUMBER	
MEffectItem::GetMaxNumber() const	
{ 
	return MAX_EFFECT_ITEM_NUMBER; 
}

TYPE_ITEM_NUMBER		
MMoonCardItem::GetMaxNumber() const
{
	return MAX_MOON_CARD_NUMBER;	
}





///////////////////////////////////////////////////////////////////////////
//
//	Item Use
//
///////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------
// MUsePotionItem::UseInventory's body, reached through the item host
// (MItemHost::UsePotionFromInventory, installed in GameInit.cpp). The
// member itself is defined in MItem.cpp: MSerum inherits it and GCC
// emits MSerum's vtable in the library, so the slot must resolve there.
//----------------------------------------------------------------------
void	UsePotionFromInventory(MItem* pPotion)
{
	CGUsePotionFromInventory _CGUsePotionFromInventory;
	_CGUsePotionFromInventory.setObjectID( pPotion->GetID() );
	_CGUsePotionFromInventory.setX( pPotion->GetGridX() );
	_CGUsePotionFromInventory.setY( pPotion->GetGridY() );

	g_pSocket->sendPacket( &_CGUsePotionFromInventory );

	// The item stays until the server confirms the use.
	//(*g_pInventory).RemoveItem( pItem->GetID() );

	//----------------------------------------------------
	// Wait for the server to verify the use from the inventory.
	//----------------------------------------------------
	g_pPlayer->SetItemCheckBuffer( pPotion, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MWater::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MWater::UseInventory()
	#endif

{
	bool bUseOK = false;
	bool bCreateHolyWater = GetItemType() <= 2;
	bool bCreateHolyPotion = GetItemType() >= 3 && GetItemType() <= 6;

	TYPE_ACTIONINFO useSkill = ACTIONINFO_NULL;

	if( bCreateHolyWater )
		useSkill = MAGIC_CREATE_HOLY_WATER;
	else if (bCreateHolyPotion )
		useSkill = SKILL_CREATE_HOLY_POTION;

	if ( useSkill != ACTIONINFO_NULL && 
		g_pSkillAvailable->IsEnableSkill( (ACTIONINFO) useSkill ) &&
		(*g_pSkillInfoTable)[useSkill].IsAvailableTime() )
	{
		bUseOK = true;
	}

	//----------------------------------------------------
	// skill이 제대로 설정된 경우
	//----------------------------------------------------
	if (bUseOK)//playerSkill < (*g_pActionInfoTable).GetMinResultActionInfo())
	{
		//if ((*g_pActionInfoTable)[playerSkill].IsTargetItem())
		{
			//----------------------------------------------------
			// 검증 받을게 없는 경우..
			//----------------------------------------------------
			if (!g_pPlayer->IsWaitVerify()
				&& g_pPlayer->IsItemCheckBufferNULL())							
			{	
				POINT fitPoint;			// 성수가 들어갈 자리

				if( bCreateHolyWater )
				{
//								if (GetMakeItemFitPosition(pItem, 
//															ITEM_CLASS_HOLYWATER, 
//															pItem->GetItemType(), 
//															fitPoint))
					{
						CGSkillToInventory _CGSkillToInventory;
						_CGSkillToInventory.setObjectID( GetID() );
						_CGSkillToInventory.setX( GetGridX() );
						_CGSkillToInventory.setY( GetGridY() );
						_CGSkillToInventory.setTargetX( GetGridX() );
						_CGSkillToInventory.setTargetY( GetGridY() );
						_CGSkillToInventory.setSkillType( useSkill );

						g_pSocket->sendPacket( &_CGSkillToInventory );								
					
						// 일단(!) 그냥 없애고 본다.
						//(*g_pInventory).RemoveItem( pItem->GetID() );

						//----------------------------------------------------
						// Inventory에서 item을 사용하는 걸 검증받기를 기다린다.
						//----------------------------------------------------
						g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );
						g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );								

						if (auto* entry = g_pSkillInfoTable->GetMutable(useSkill)) {
							entry->SetNextAvailableTime();
						}

						//----------------------------------------------------
						// 기술 사용 시도 동작
						//----------------------------------------------------
						AddNewInventoryEffect( GetID(),
											useSkill, //+ (*g_pActionInfoTable).GetMinResultActionInfo(),
											g_pClientConfig->FPS*3	// 3초
										);
					}
//								else
//								{
//									// g_pGameMessage( "자리가 없따야~" );
//								}
				}
				else if (bCreateHolyPotion )
				{
					if (GetMakeItemFitPosition(this, 
												ITEM_CLASS_POTION, 
												(*g_pItemTable)[GetItemClass()][GetItemType()].Value3 , 
												fitPoint))
					{
						CGSkillToInventory _CGSkillToInventory;

						_CGSkillToInventory.setObjectID( GetID() );
						_CGSkillToInventory.setObjectID( GetID() );
						_CGSkillToInventory.setX( GetGridX() );
						_CGSkillToInventory.setY( GetGridY() );
						_CGSkillToInventory.setTargetX( static_cast<Coord_t>(fitPoint.x) );
						_CGSkillToInventory.setTargetY( static_cast<Coord_t>(fitPoint.y) );
						_CGSkillToInventory.setSkillType( useSkill );

						g_pSocket->sendPacket( &_CGSkillToInventory );

						g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );
						g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );
						
						if (auto* entry = g_pSkillInfoTable->GetMutable(useSkill)) {
							entry->SetNextAvailableTime();
						}
						
						AddNewInventoryEffect( GetID(),
							useSkill,
							g_pClientConfig->FPS*3
							);
					}
					else
					{
					}
				}						
			}
		}
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MMagazine::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MMagazine::UseInventory()
	#endif

{
	bool		bPossible = false;

	int value = (*g_pItemTable)[ GetItemClass() ][ GetItemType() ].Value3;

	if( value <= 0 )
		bPossible = true;
	else
	{
		if( value > 0 && value < g_pActionInfoTable->GetSize() )
			bPossible = g_pSkillAvailable->IsEnableSkill( (ACTIONINFO) value );
	}

	if( bPossible == false )
	{
		g_pSystemMessage->Add((*g_pGameStringTable)[UI_STRING_MESSAGE_CANNOT_RELOAD_BY_VIVID_MAGAZINE].GetString() );
		return;
	}

	const MItem* pHandItem = (*g_pSlayerGear).GetItem( MSlayerGear::GEAR_SLAYER_RIGHTHAND );

	if (pHandItem!=NULL && pHandItem->IsGunItem())
	{
		const MGunItem* pGunItem = (const MGunItem*)pHandItem;

		// 현재 총에 장착할 수 있는 탄창인가?
		if (IsInsertToItem( pGunItem ))	
		{
			CGReloadFromInventory _CGReloadFromInventory;
			_CGReloadFromInventory.setObjectID( GetID() );
			_CGReloadFromInventory.setX( GetGridX() );
			_CGReloadFromInventory.setY( GetGridY() );

			g_pSocket->sendPacket( &_CGReloadFromInventory );

			
			// 일단(!) 그냥 없애고 본다.
			//(*g_pInventory).RemoveItem( GetID() );

			//----------------------------------------------------
			// Inventory에서 item을 사용하는 걸 검증받기를 기다린다.
			//----------------------------------------------------
			g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
		}
		else
		{
			// [도움말] 총알이 안 맞는 경우
//			__BEGIN_HELP_EVENT
//				ExecuteHelpEvent( HE_ITEM_DIFFERENT_BULLET_TYPE );
//			__END_HELP_EVENT

			DEBUG_ADD("Magazine is not fit to Gun");
		}
	}
	else
	{
		DEBUG_ADD("Weapon is not Gun");
	}
}

void	MMagazine::UseQuickItem()
{
	const MItem* pHandItem = (*g_pSlayerGear).GetItem( MSlayerGear::GEAR_SLAYER_RIGHTHAND );

	if (pHandItem!=NULL && pHandItem->IsGunItem())
	{
		const MGunItem* pGunItem = (const MGunItem*)pHandItem;

		//----------------------------------------------------
		// 현재 총에 장착할 수 있는 탄창인가?
		//----------------------------------------------------
		if (IsInsertToItem( pGunItem ))	
		{
			CGReloadFromQuickSlot _CGReloadFromQuickSlot;
			_CGReloadFromQuickSlot.setObjectID( GetID() );
			_CGReloadFromQuickSlot.setSlotID( GetItemSlot() );							

			g_pSocket->sendPacket( &_CGReloadFromQuickSlot );

			
			//----------------------------------------------------
			// Inventory에서 item을 사용하는 걸 검증받기를 기다린다.
			//----------------------------------------------------
			g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);

			//----------------------------------------------------
			// 벨트 못 없애도록..
			//----------------------------------------------------
			UI_LockGear();

//			__BEGIN_HELP_EVENT
//				// [도움말] 탄창 사용
//				ExecuteHelpEvent( HE_ITEM_USE_MAGAZINE );	
//				// [도움말] 벨트의 아이템 사용
//				ExecuteHelpEvent( HE_ITEM_USE_BELT_ITEM );	
//			__END_HELP_EVENT
		}
		else
		{
			// [도움말] 총알이 안 맞는 경우
//			__BEGIN_HELP_EVENT
//				ExecuteHelpEvent( HE_ITEM_DIFFERENT_BULLET_TYPE );
//			__END_HELP_EVENT

			PlaySound( SOUND_ITEM_NO_MAGAZINE );

			// 탄창이 안 맞는 경우
			DEBUG_ADD("[Can't use Magazine] Wrong Magazine");
		}
	}
	else
	{
		//PlaySound( SOUND_ITEM_NO_MAGAZINE );

		// 총이 없는 경우
		DEBUG_ADD("[Can't use Magazine] No Gun");
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MBombMaterial::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MBombMaterial::UseInventory()
	#endif

{
	//int playerSkill = g_pPlayer->GetSpecialActionInfo();
	bool bUseOK = false;

	int currentItemType = GetItemType();

	ACTIONINFO			useSkill;
	ITEM_CLASS	itemClass;
	int					itemType;

	//-------------------------------------------
	// 폭탄이냐 지뢰냐...
	//-------------------------------------------
	// 하드코딩 -_-;
	//-------------------------------------------
	if (currentItemType < 5)
	{
		useSkill	= SKILL_MAKE_BOMB;
		itemClass	= ITEM_CLASS_BOMB;
		itemType	= currentItemType;
	}
	else
	{
		useSkill	= SKILL_MAKE_MINE;
		itemClass	= ITEM_CLASS_MINE;
		itemType	= currentItemType - 5;
	}

	if (g_pSkillAvailable->IsEnableSkill( useSkill )
		&& (*g_pSkillInfoTable)[useSkill].IsAvailableTime())
	{
		bUseOK = true;
	}

	//----------------------------------------------------
	// skill이 제대로 설정된 경우
	//----------------------------------------------------
	if (bUseOK)//playerSkill < (*g_pActionInfoTable).GetMinResultActionInfo())
	{				
		//----------------------------------------------------
		// 검증 받을게 없는 경우..
		//----------------------------------------------------
		if (!g_pPlayer->IsWaitVerify()
			&& g_pPlayer->IsItemCheckBufferNULL())							
		{	
			POINT fitPoint;			// 폭탄이 들어갈 자리						
			
			if (GetMakeItemFitPosition(this, itemClass, itemType, fitPoint))
			{
				CGSkillToInventory _CGSkillToInventory;
				_CGSkillToInventory.setObjectID( GetID() );
				_CGSkillToInventory.setX( GetGridX() );
				_CGSkillToInventory.setY( GetGridY() );
				_CGSkillToInventory.setTargetX( static_cast<Coord_t>(fitPoint.x) );
				_CGSkillToInventory.setTargetY( static_cast<Coord_t>(fitPoint.y) );
				_CGSkillToInventory.setSkillType( useSkill );

				g_pSocket->sendPacket( &_CGSkillToInventory );								
			
				// 일단(!) 그냥 없애고 본다.
				//(*g_pInventory).RemoveItem( GetID() );

				//----------------------------------------------------
				// Inventory에서 item을 사용하는 걸 검증받기를 기다린다.
				//----------------------------------------------------
				g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );
				g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );

				// 만드는 시도 중에 이미 delay만큼 시간을 보낸다.
				//(*g_pSkillInfoTable)[useSkill].SetNextAvailableTime();

				//----------------------------------------------------
				// 기술 사용 시도 동작
				//----------------------------------------------------
				AddNewInventoryEffect( GetID(),
									useSkill, //+ (*g_pActionInfoTable).GetMinResultActionInfo(),
									g_pClientConfig->FPS*3	// 3초
								);
			}
			else
			{
				// g_pGameMessage( "자리가 없따야~" );
			}
		}
	}
}

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MMoney::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MMoney::UseInventory()
	#endif
{
	// 2개 이상 있어야 분리 된다.
	if(GetNumber() > 1)
	{
		// 아이템 분리
		CGAddInventoryToMouse _CGAddInventoryToMouse;
		_CGAddInventoryToMouse.setObjectID( 0 );	// 분리한다는 의미
		_CGAddInventoryToMouse.setX( GetGridX() );
		_CGAddInventoryToMouse.setY( GetGridY() );
		
		g_pSocket->sendPacket( &_CGAddInventoryToMouse );				
		
		//----------------------------------------------------
		// Inventory에서 item을 몇개 드는 걸 검증받는다.
		// 일단은 1개만 들리도록 한다.
		//----------------------------------------------------
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY);
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MMine::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MMine::UseInventory()
	#endif

{
	bool bUseOK = true;
	ACTIONINFO useSkill;					
		
	//----------------------------------------------------
	// 사용가능한가?
	//----------------------------------------------------			
	useSkill = SKILL_INSTALL_MINE;					

	if (//g_pPlayer->GetSpecialActionInfo()==useSkill ||
		g_pSkillAvailable->IsEnableSkill( useSkill )
		&& (*g_pSkillInfoTable)[useSkill].IsAvailableTime()
		&& g_pPlayer->GetMoveDevice() != MCreature::MOVE_DEVICE_RIDE)
	{
		bUseOK = true;
	}
	else
	{
		bUseOK = false;
	}			

	if (bUseOK)
	{
		CGSkillToInventory _CGSkillToInventory;
		_CGSkillToInventory.setObjectID( GetID() );
		_CGSkillToInventory.setX( GetGridX() );
		_CGSkillToInventory.setY( GetGridY() );
		// 지뢰 심는 방향
		_CGSkillToInventory.setTargetX( g_pPlayer->GetDirection() );
		_CGSkillToInventory.setSkillType( useSkill );

		g_pSocket->sendPacket( &_CGSkillToInventory );
		
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);

		g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );

		if (auto* entry = g_pSkillInfoTable->GetMutable(useSkill)) {
			entry->SetNextAvailableTime();
		}

		//----------------------------------------------------
		// 기술 사용 시도 동작
		//----------------------------------------------------
		/*
		if (useSkill == MAGIC_BLOODY_MARK)
		{
			AddNewInventoryEffect( GetID(),
								useSkill,
								14
							);
		}
		*/
	
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MSlayerPortalItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MSlayerPortalItem::UseInventory()
	#endif

{
	if(g_bHolyLand == false					// 성지가 아니어야 하고
		&& g_pZoneTable->Get( g_pZone->GetID() )->CannotUseSpecialItem == false		// 현재 존이 스페셜 아이템을 사용할 수 있어야 하고
		&& IsAffectStatus()						// 적용 가능해야 하고
		&& IsChargeItem() && GetNumber() > 0	// 충전아이템이며 숫자가 0보다 커야 한다.
		&& g_pZone->GetHelicopter( g_pPlayer->GetID() ) == NULL// 헬기를 불렀으면 안된다.
		&& !g_pPlayer->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))	
	{
		// slayer는 item사용하는거당
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
		if(0 != SubInventoryItemID)
			_CGUseItemFromInventory.setInventoryItemObjectID( SubInventoryItemID );
	#else

	#endif
		g_pSocket->sendPacket( &_CGUseItemFromInventory );

		
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MVampirePortalItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MVampirePortalItem::UseInventory()
	#endif

{
	if(g_bHolyLand == false					// 성지가 아니어야 하고
		&& g_pZoneTable->Get( g_pZone->GetID() )->CannotUseSpecialItem == false		// 현재 존이 스페셜 아이템을 사용할 수 있어야 하고
		&& IsAffectStatus()						// 적용 가능해야 하고
		&& IsChargeItem() && GetNumber() > 0	// 충전아이템이며 숫자가 0보다 커야 한다.
		)
	{
		ACTIONINFO useSkill;					
		
		if (IsMarked())
		{
			useSkill = MAGIC_BLOODY_TUNNEL;
		}
		else
		{
			useSkill = MAGIC_BLOODY_MARK;
		}

		if(g_pSkillAvailable->IsEnableSkill( useSkill )
			&& (*g_pSkillInfoTable)[useSkill].IsAvailableTime())
		{
			// vampire는 기술 사용하는거다
			CGSkillToInventory _CGSkillToInventory;
			_CGSkillToInventory.setObjectID( GetID() );
			_CGSkillToInventory.setX( GetGridX() );
			_CGSkillToInventory.setY( GetGridY() );
			_CGSkillToInventory.setSkillType( useSkill );
			//_CGSkillToInventory.setCEffectID( 0 );	// -_-;;							

			g_pSocket->sendPacket( &_CGSkillToInventory );
				
			g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );
			g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );

			if (auto* entry = g_pSkillInfoTable->GetMutable(useSkill)) {
				entry->SetNextAvailableTime();
			}

			//----------------------------------------------------
			// 기술 사용 시도 동작
			//----------------------------------------------------
			if (useSkill == MAGIC_BLOODY_MARK)
			{
				AddNewInventoryEffect( GetID(),
									useSkill,
									14
								);
			}
		}

	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MEventStarItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MEventStarItem::UseInventory()
	#endif

{
	// 2개 이상 있어야 분리 된다.
	if(GetNumber() > 1)
	{
		// 아이템 분리
		CGAddInventoryToMouse _CGAddInventoryToMouse;
		_CGAddInventoryToMouse.setObjectID( 0 );	// 분리한다는 의미
		_CGAddInventoryToMouse.setX( GetGridX() );
		_CGAddInventoryToMouse.setY( GetGridY() );
		
		g_pSocket->sendPacket( &_CGAddInventoryToMouse );				
		
		//----------------------------------------------------
		// Inventory에서 item을 몇개 드는 걸 검증받는다.
		// 일단은 1개만 들리도록 한다.
		//----------------------------------------------------
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY);
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MMixingItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MMixingItem::UseInventory()
	#endif

{
	if( GetItemType() <= 8 )
	{
		if( GetNumber() > 1 )	// 아이템 분리
		{
			CGAddInventoryToMouse _CGAddInventoryToMouse;
			_CGAddInventoryToMouse.setObjectID( 0 );
			_CGAddInventoryToMouse.setX( GetGridX() );
			_CGAddInventoryToMouse.setY( GetGridY() );
			
			g_pSocket->sendPacket( &_CGAddInventoryToMouse );
			g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY );
		}
		else
		{
			C_VS_UI_MIXING_FORGE::FORGE_CLASS fc = C_VS_UI_MIXING_FORGE::CLASS_WEAPON;
			C_VS_UI_MIXING_FORGE::FORGE_TYPE  ft = C_VS_UI_MIXING_FORGE::TYPE_A;
			
			if(GetItemType() <= 2)
			{
				fc = C_VS_UI_MIXING_FORGE::CLASS_WEAPON;
				ft = C_VS_UI_MIXING_FORGE::FORGE_TYPE(C_VS_UI_MIXING_FORGE::TYPE_A + GetItemType());
			} else
			if(GetItemType() >= 3 && GetItemType() <= 5)
			{
				fc = C_VS_UI_MIXING_FORGE::CLASS_ARMOR;
				ft = C_VS_UI_MIXING_FORGE::FORGE_TYPE(C_VS_UI_MIXING_FORGE::TYPE_A + (GetItemType()-3));
			} else
			if(GetItemType() >= 6 && GetItemType() <= 8 )
			{
				fc = C_VS_UI_MIXING_FORGE::CLASS_ACCESSORY;
				ft = C_VS_UI_MIXING_FORGE::FORGE_TYPE(C_VS_UI_MIXING_FORGE::TYPE_A + (GetItemType()-6));
			}
			gC_vs_ui.RunMixingForge(fc,ft);
			g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_MIXING_ITEM );			
		}
	} else
	if( GetItemType() >= 9 && GetItemType() <= 18 )
	{
		// Puritas 는 우클릭하면 분리한다.
		CGAddInventoryToMouse _CGAddInventoryToMouse;
		_CGAddInventoryToMouse.setObjectID( 0 );
		_CGAddInventoryToMouse.setX( GetGridX() );
		_CGAddInventoryToMouse.setY( GetGridY() );

		g_pSocket->sendPacket( &_CGAddInventoryToMouse );
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY );
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void MEffectItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void MEffectItem::UseInventory()
	#endif

{
	// add by Coffee 2007-8-5 
	if (GetItemType() >= 10 && GetItemType() <= 12)
	{
		gC_vs_ui.RunBulletinBoardWindow(this);
		return;
	}
	
	// 확성기같은것..
	CGUseItemFromInventory _CGUseItemFromInventory;
	
	_CGUseItemFromInventory.setObjectID( GetID() );
	_CGUseItemFromInventory.setX( GetGridX() );
	_CGUseItemFromInventory.setY( GetGridY() );
	g_pSocket->sendPacket( &_CGUseItemFromInventory );
	g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY );
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void MItemETC::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void MItemETC::UseInventory()
	#endif
{
	if(GetItemType() == 1 )
	{
		CGUseItemFromInventory _CGUseItemFromInventory;						
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );
		g_pSocket->sendPacket( &_CGUseItemFromInventory );				
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void MVampireETC::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void MVampireETC::UseInventory()
	#endif
{
	//----------------------------------------------------
	//
	//					변신용 아이템 - vampire인 경우
	//
	//----------------------------------------------------
	if (g_pZoneTable->Get( g_pZone->GetID() )->CannotUseSpecialItem == false )
	{				
		ACTIONINFO useSkill;// = g_pPlayer->GetSpecialActionInfo();

		if ( GetItemClass() == ITEM_CLASS_SKULL )						// 늑대 머리
		{
			useSkill = SKILL_TRANSFORM_TO_WERWOLF;
		}
		else if (GetItemType()==0)
		{
			useSkill = MAGIC_TRANSFORM_TO_WOLF;
		}
		else if (GetItemType()==1)
		{
			useSkill = MAGIC_TRANSFORM_TO_BAT;
		}
		else
		{
			// - -;
			if( GetItemType() == 2)
			{
				
				CGUseItemFromInventory _CGUseItemFromInventory;						
				_CGUseItemFromInventory.setObjectID( GetID() );
				_CGUseItemFromInventory.setX( GetGridX() );
				_CGUseItemFromInventory.setY( GetGridY() );

				g_pSocket->sendPacket( &_CGUseItemFromInventory );

				g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
			}
			return;					
		}

		//----------------------------------------------------
		// skill이 제대로 설정된 경우
		//----------------------------------------------------
		// useSkill is always set above (every other path returns)
		{
			if (//(*g_pActionInfoTable)[playerSkill].IsTargetItem()
				g_pSkillAvailable->IsEnableSkill( useSkill )
				&& (*g_pSkillInfoTable)[useSkill].IsAvailableTime())					
			{
				//----------------------------------------------------
				// 검증 받을게 없는 경우..
				//----------------------------------------------------
				if (!g_pPlayer->IsWaitVerify()
					&& g_pPlayer->IsItemCheckBufferNULL())
				{					
					CGSkillToInventory _CGSkillToInventory;
					_CGSkillToInventory.setObjectID( GetID() );
					_CGSkillToInventory.setX( GetGridX() );
					_CGSkillToInventory.setY( GetGridY() );
					_CGSkillToInventory.setSkillType( useSkill );
					//_CGSkillToInventory.setCEffectID( 0 );	// -_-;;							
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
					if(0 != SubInventoryItemID)
						_CGSkillToInventory.setInventoryItemObjectID( SubInventoryItemID );
	#endif
					g_pSocket->sendPacket( &_CGSkillToInventory );

					
					// 일단(!) 그냥 없애고 본다.
					//(*g_pInventory).RemoveItem( GetID() );

					//----------------------------------------------------
					// Inventory에서 item을 사용하는 걸 검증받기를 기다린다.
					//----------------------------------------------------
					g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );
				#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
					g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY, SubInventoryItemID );
				#else
					g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );
				#endif
					

					if (auto* entry = g_pSkillInfoTable->GetMutable(useSkill)) {
						entry->SetNextAvailableTime();
					}

					//----------------------------------------------------
					// 기술 사용 시도 동작
					//----------------------------------------------------
					AddNewInventoryEffect( GetID(),
										useSkill, //+ (*g_pActionInfoTable).GetMinResultActionInfo(),
										g_pClientConfig->FPS*3	// 3초
									);								
				}
			}
		}
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MSkull::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MSkull::UseInventory()
	#endif

{
	//----------------------------------------------------
	//
	//					변신용 아이템 - vampire인 경우
	//
	//----------------------------------------------------
	if (g_pZoneTable->Get( g_pZone->GetID() )->CannotUseSpecialItem == false )
	{				
		ACTIONINFO useSkill;// = g_pPlayer->GetSpecialActionInfo();

		if ( GetItemClass() == ITEM_CLASS_SKULL && GetItemType() == 39)						// 늑대 머리
		{
			useSkill = SKILL_TRANSFORM_TO_WERWOLF;
		}
		else if (GetItemType()==0)
		{
			useSkill = MAGIC_TRANSFORM_TO_WOLF;
		}
		else if (GetItemType()==1)
		{
			useSkill = MAGIC_TRANSFORM_TO_BAT;
		}
		else
		{
			// - -;
			if( GetItemType() == 2)
			{
				
				CGUseItemFromInventory _CGUseItemFromInventory;						
				_CGUseItemFromInventory.setObjectID( GetID() );
				_CGUseItemFromInventory.setX( GetGridX() );
				_CGUseItemFromInventory.setY( GetGridY() );

				g_pSocket->sendPacket( &_CGUseItemFromInventory );

				g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
			}
			return;					
		}

		//----------------------------------------------------
		// skill이 제대로 설정된 경우
		//----------------------------------------------------
		// useSkill is always set above (every other path returns)
		{
			if (//(*g_pActionInfoTable)[playerSkill].IsTargetItem()
				g_pSkillAvailable->IsEnableSkill( useSkill )
				&& (*g_pSkillInfoTable)[useSkill].IsAvailableTime())					
			{
				//----------------------------------------------------
				// 검증 받을게 없는 경우..
				//----------------------------------------------------
				if (!g_pPlayer->IsWaitVerify()
					&& g_pPlayer->IsItemCheckBufferNULL())
				{					
					CGSkillToInventory _CGSkillToInventory;
					_CGSkillToInventory.setObjectID( GetID() );
					_CGSkillToInventory.setX( GetGridX() );
					_CGSkillToInventory.setY( GetGridY() );
					_CGSkillToInventory.setSkillType( useSkill );
					//_CGSkillToInventory.setCEffectID( 0 );	// -_-;;							

					g_pSocket->sendPacket( &_CGSkillToInventory );

					
					// 일단(!) 그냥 없애고 본다.
					//(*g_pInventory).RemoveItem( GetID() );

					//----------------------------------------------------
					// Inventory에서 item을 사용하는 걸 검증받기를 기다린다.
					//----------------------------------------------------
					g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );
					g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );

					if (auto* entry = g_pSkillInfoTable->GetMutable(useSkill)) {
						entry->SetNextAvailableTime();
					}

					//----------------------------------------------------
					// 기술 사용 시도 동작
					//----------------------------------------------------
					AddNewInventoryEffect( GetID(),
										useSkill, //+ (*g_pActionInfoTable).GetMinResultActionInfo(),
										g_pClientConfig->FPS*3	// 3초
									);								
				}
			}
		}
	}
}

// 해당 크리쳐의 주변에 원하는 시체가 있는지 검사한다.
extern bool IsExistCorpseFromPlayer(MCreature* OriginCreature, int creature_type);

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MKey::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MKey::UseInventory()
	#endif
{
	// 주변에 깃발이 없어야 한다.
	if( !IsExistCorpseFromPlayer( dynamic_cast<MCreature*>(g_pPlayer), 670 ) )
	{
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );
		
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
		if(0 != SubInventoryItemID)
			_CGUseItemFromInventory.setInventoryItemObjectID( SubInventoryItemID );
	#endif
		g_pSocket->sendPacket( &_CGUseItemFromInventory );

	//modify by viva for notice : i don't understand it :(
//	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
//		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY, SubInventoryItemID );
//	#else
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
//	#endif
	//end
		
	} else
	{
		g_pSystemMessage->Add((*g_pGameStringTable)[UI_STRING_MESSAGE_CANNOT_ACTION_MOTORCYCLE_FLAG].GetString() );
	}
	
}
void	MKey::UseQuickItem()
{
	// 2004, 12, 16, sobeit add start - 인스톨 터렛 상태에서 오토바이를 타면 문제 생김..ㅎㅎ 그래서 막음
	if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
	{
		return ;
	}
	// 2004, 12, 16, sobeit add end
	CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
	_CGUsePotionFromQuickSlot.setObjectID( GetID() );
	_CGUsePotionFromQuickSlot.setSlotID( GetItemSlot() );							

	g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );

	UI_LockGear();
	g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	// 일단(!) 그냥 없애고 본다.
	//(*g_pInventory).RemoveItem( GetID() );

	//----------------------------------------------------
	// Inventory에서 item을 사용하는 걸 검증받기를 기다린다.
	//----------------------------------------------------
	g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	//----------------------------------------------------
	// 벨트 못 없애도록..
	//----------------------------------------------------
	UI_LockGear();

}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MEventTreeItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MEventTreeItem::UseInventory()
	#endif
{
	if(GetItemType() == 12 ||
		(GetItemType() >= 26 && GetItemType() <= 28))
	{
		bool bUseOK = true;

		bool bMasterWords = g_pPlayer->GetCreatureType()==CREATURETYPE_SLAYER_OPERATOR
					|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_OPERATOR
					|| g_pPlayer->GetCreatureType()==CREATURETYPE_OUSTERS_OPERATOR;
		
		if(IsPlayerInSafePosition() || (g_pZone != NULL && (g_pZone->GetID() == 1104 || g_pZone->GetID() == 1106 || g_pZone->GetID() == 1114 || g_pZone->GetID() == 1115)))
			// 안전지대와 마스터 레어에서는 못쓴다
		{				
			// 각 성이 아니면서 운영자가 아니면
			if( !( g_pZone->GetID() >= 1201 && g_pZone->GetID() <= 1206 ) && !bMasterWords )
			{				
				bUseOK = false;
				g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_NOT_USE_SAFETY_POSITION].GetString() );
				return;
			}
		}

		if( g_pPlayer->GetMoveDevice()==MCreature::MOVE_DEVICE_RIDE )
		{				
			if( g_pPlayer->IsSlayer() )
				g_pUIDialog->PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_CANNOT_USE_RIDE_MOTORCYCLE].GetString() );
			else if (g_pPlayer->IsOusters() )
				g_pUIDialog->PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_CANNOT_USE_SUMMON_SYLPH].GetString() );

			return;
		}
		
		int playerX = g_pPlayer->GetX(), playerY = g_pPlayer->GetY();
		
		for(int y = playerY-3; y <= playerY+3; y++)
		{
			for(int x = playerX-2; x <= playerX+2; x++)
			{
				if(x < 0 || y < 0 || x >= g_pZone->GetWidth() || y >= g_pZone->GetHeight())
					continue;

				MItem *pItem = g_pZone->GetSector(x, y).GetItem();
				MCreature *pCreature = NULL;
				
				if(pItem != NULL)
				{
					if(GetItemClass() == ITEM_CLASS_CORPSE)
					{
						MCorpse *pCorpse = (MCorpse *)pItem;
						pCreature = pCorpse->GetCreature();
						
						if(pCreature != NULL && (pCreature->GetCreatureType() == 482 || pCreature->GetCreatureType() == 650))
						{
							bUseOK = false;
							if(GetItemType() == 12)	// 크리스 마스 트리
							{
								g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_XMAS_TREE_CANNOT_USE].GetString() );
							}
							else					// 게시판
							{
								g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_BULLETIN_BOARD_CANNOT_USE].GetString() );
							}
							return;
						}
					}
				}
			}
		}
		if (bUseOK)
		{
			if(GetItemType() == 12)		// 크리스마스 트리
			{
				gC_vs_ui.RunXmasCardWindow(this);
			}
			else						// 게시판
			{
				gC_vs_ui.RunBulletinBoardWindow(this);
			}
		}
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MEventEtcItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MEventEtcItem::UseInventory()
	#endif
{
	bool bUseOK = true;

	if(IsPlayerInSafePosition() == 2 
		&& GetItemType() != 14 
		&& GetItemType()!=15
		&& GetItemType()!=16
		&& GetItemType()!=17) // 14는 머시냐..사탕..과 송편들..
	{
		bUseOK = false;
		g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_NOT_USE_SAFETY_ZONE].GetString() );
		return;
	}
		
	if (bUseOK)
	{
		if(GetItemType() == 14||
		GetItemType() == 15||
		GetItemType() == 16||
		GetItemType() == 17) // 사탕만, + 송편
		{
			CGUsePotionFromInventory _CGUsePotionFromInventory;
			_CGUsePotionFromInventory.setObjectID( GetID() );
			_CGUsePotionFromInventory.setX( GetGridX() );
			_CGUsePotionFromInventory.setY( GetGridY() );
			g_pSocket->sendPacket( &_CGUsePotionFromInventory );
			g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);

		}
		else
		{
			CGUseItemFromInventory _CGUseItemFromInventory;
			_CGUseItemFromInventory.setObjectID( GetID() );
			_CGUseItemFromInventory.setX( GetGridX() );
			_CGUseItemFromInventory.setY( GetGridY() );
			g_pSocket->sendPacket( &_CGUseItemFromInventory );
			g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);

		}
		
	}
}
void	MEventEtcItem::UseQuickItem()
{
	if(GetItemType() == 14||
		GetItemType() == 15||
		GetItemType() == 16||
		GetItemType() == 17) // 사탕만, + 송편
	{
		if( g_pPlayer->IsOusters() )
		{
			CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
			_CGUsePotionFromQuickSlot.setObjectID( GetID() );
			int slotID = GetItemSlot();

			if( g_pArmsBand1 != NULL )	// 1번 암스밴드가 있으면 그냥
			{
				if(g_pArmsBand1->GetItem( slotID ) == this)
					_CGUsePotionFromQuickSlot.setSlotID( slotID );
				else
					_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
			}
			else						// 1번 암스밴드가 없으면 2번 +3 을 해준다
			{
				_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
			}
				
		//					_CGUsePotionFromQuickSlot.setSlotID( slotID );
			g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );
			g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
		//	DEBUG_ADD_FORMAT("[ Ousters Item Use From QuickSlot ] %d %d",slotID, GetID() );
			
			//----------------------------------------------------
			// 벨트 못 없애도록..
			//----------------------------------------------------
			UI_LockGear();
		}
		else
		{
			CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
			_CGUsePotionFromQuickSlot.setObjectID( GetID() );
			_CGUsePotionFromQuickSlot.setSlotID( GetItemSlot() );							

			g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );

			
			// 일단(!) 그냥 없애고 본다.
			//(*g_pInventory).RemoveItem( GetID() );

			//----------------------------------------------------
			// Inventory에서 item을 사용하는 걸 검증받기를 기다린다.
			//----------------------------------------------------
			g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);

			//----------------------------------------------------
			// 벨트 못 없애도록..
			//----------------------------------------------------
			UI_LockGear();
		}
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MDyePotionItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MDyePotionItem::UseInventory()
	#endif

{
	// 아우스터즈는 성이 없기 때문에 성전환 아이템을 사용할 수 없다
	if( g_pPlayer->IsOusters() && GetItemType() == 48 )
	{
		UI_PopupMessage(STRING_MESSAGE_CANNOT_USE_OUSTERS);
	}
	else
	{
		// edit by sonic 2006.10.29  去除二转后不能使用改变火颜色道具

			/*	
				// 2005, 2, 22, sobeit add start
				if(g_pPlayer->IsAdvancementClass() && GetItemType() >= 58 && GetItemType() <= 61)
				{ // 승직 유저는 마스터 이펙트 색상 바꾸는 아이템을 못쓰게 한다네..왤까...?
					UI_PopupMessage(UI_STRING_MESSAGE_CANNOT_USE_ADVANCEMENTCLASS);
					return;
				}
				// 2005, 2, 22, sobeit add end
			*/

		// 2006.10.29,Sonic edit end
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );
		
		g_pSocket->sendPacket( &_CGUseItemFromInventory );			
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);				
	}
}
//
//void	MDyePotionItem::UseQuickItem()
//{
//#ifdef __GAME_CLIENT__
//	
//	if( g_pPlayer->IsOusters() )
//	{
//		// 아우스터즈는 성이 없기 때문에 성전환 아이템을 사용할 수 없다
//		if(GetItemType() == 48)
//			UI_PopupMessage(STRING_MESSAGE_CANNOT_USE_OUSTERS);
//		else
//		{
//			CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
//			_CGUsePotionFromQuickSlot.setObjectID( GetID() );
//			int slotID = GetItemSlot();
//
//			if( g_pArmsBand1 != NULL )	// 1번 암스밴드가 있으면 그냥
//			{
//				if(g_pArmsBand1->GetItem( slotID ) == this)
//					_CGUsePotionFromQuickSlot.setSlotID( slotID );
//				else
//					_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
//			}
//			else						// 1번 암스밴드가 없으면 2번 +3 을 해준다
//			{
//				_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
//			}
//				
//		//					_CGUsePotionFromQuickSlot.setSlotID( slotID );
//			g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );
//			g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
//		//	DEBUG_ADD_FORMAT("[ Ousters Item Use From QuickSlot ] %d %d",slotID, GetID() );
//			
//			//----------------------------------------------------
//			// 벨트 못 없애도록..
//			//----------------------------------------------------
//			UI_LockGear();
//		}
//	}
//	else
//	{
//		CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
//		_CGUsePotionFromQuickSlot.setObjectID( GetID() );
//		_CGUsePotionFromQuickSlot.setSlotID( GetItemSlot() );							
//
//		g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );
//
//		
//		// 일단(!) 그냥 없애고 본다.
//		//(*g_pInventory).RemoveItem( GetID() );
//
//		//----------------------------------------------------
//		// Inventory에서 item을 사용하는 걸 검증받기를 기다린다.
//		//----------------------------------------------------
//		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
//
//		//----------------------------------------------------
//		// 벨트 못 없애도록..
//		//----------------------------------------------------
//		UI_LockGear();
//	}
//#endif
//}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MOustersSummonGem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MOustersSummonGem::UseInventory()
	#endif
{
	if( IsChargeItem() && GetNumber() > 0 && IsAffectStatus() && !g_bZoneSafe &&
		!g_pPlayer->IsInSafeSector() && g_pPlayer->IsStop() && !g_pPlayer->HasEffectStatus( EFFECTSTATUS_HAS_FLAG) )
	{
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
		if(0 != SubInventoryItemID)
			_CGUseItemFromInventory.setInventoryItemObjectID( SubInventoryItemID );
	#endif

		g_pSocket->sendPacket( &_CGUseItemFromInventory );

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY, SubInventoryItemID);
	#else
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	#endif
		
		if (auto* entry = g_pSkillInfoTable->GetMutable(SKILL_SUMMON_SYLPH)) {
			entry->SetAvailableTime( 4000 );
		}
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MCodeSheetItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MCodeSheetItem::UseInventory()
	#endif

{
	if( GetItemType() == 0 && GetItemOptionListCount() == 30)
	{
		gC_vs_ui.RunQuestInventory(this);
	}
}

void	MPotion::UseQuickItem()
{
	CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
	_CGUsePotionFromQuickSlot.setObjectID( GetID() );
	_CGUsePotionFromQuickSlot.setSlotID( GetItemSlot() );							

	g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );

	
	// 일단(!) 그냥 없애고 본다.
	//(*g_pInventory).RemoveItem( GetID() );

	//----------------------------------------------------
	// Inventory에서 item을 사용하는 걸 검증받기를 기다린다.
	//----------------------------------------------------
	g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);

	//----------------------------------------------------
	// 벨트 못 없애도록..
	//----------------------------------------------------
	UI_LockGear();

	// [도움말] 벨트의 아이템 사용
//	__BEGIN_HELP_EVENT
///		ExecuteHelpEvent( HE_ITEM_USE_BELT_ITEM );	
//	__END_HELP_EVENT
}

void	MOustersPupa::UseQuickItem()
{
	CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
	_CGUsePotionFromQuickSlot.setObjectID( GetID() );
	int slotID = GetItemSlot();

	if( g_pArmsBand1 != NULL )	// 1번 암스밴드가 있으면 그냥
	{
		if(g_pArmsBand1->GetItem( slotID ) == this)
			_CGUsePotionFromQuickSlot.setSlotID( slotID );
		else
			_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
	}
	else						// 1번 암스밴드가 없으면 2번 +3 을 해준다
	{
		_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
	}
		
//					_CGUsePotionFromQuickSlot.setSlotID( slotID );
	g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );
	g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
//	DEBUG_ADD_FORMAT("[ Ousters Item Use From QuickSlot ] %d %d",slotID, GetID() );
	
	//----------------------------------------------------
	// 벨트 못 없애도록..
	//----------------------------------------------------
	UI_LockGear();
}

void	MOustersComposMei::UseQuickItem()
{
	CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
	_CGUsePotionFromQuickSlot.setObjectID( GetID() );
	int slotID = GetItemSlot();

	if( g_pArmsBand1 != NULL )	// 1번 암스밴드가 있으면 그냥
	{
		if(g_pArmsBand1->GetItem( slotID ) == this)
			_CGUsePotionFromQuickSlot.setSlotID( slotID );
		else
			_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
	}
	else						// 1번 암스밴드가 없으면 2번 +3 을 해준다
	{
		_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
	}
		
//					_CGUsePotionFromQuickSlot.setSlotID( slotID );
	g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );
	g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
//	DEBUG_ADD_FORMAT("[ Ousters Item Use From QuickSlot ] %d %d",slotID, GetID() );
	
	//----------------------------------------------------
	// 벨트 못 없애도록..
	//----------------------------------------------------
	UI_LockGear();
}

void	MCoupleRing::UseGear()
{
	CGUseItemFromGear _CGUseItemFromGear;
	
	_CGUseItemFromGear.setObjectID ( GetID() );
	_CGUseItemFromGear.setPart ( GetItemSlot() );
	
	g_pSocket->sendPacket( &_CGUseItemFromGear );
	
	g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_GEAR );
}

void	MVampireCoupleRing::UseGear()
{
	CGUseItemFromGear _CGUseItemFromGear;
	
	_CGUseItemFromGear.setObjectID ( GetID() );
	_CGUseItemFromGear.setPart ( GetItemSlot() );
	
	g_pSocket->sendPacket( &_CGUseItemFromGear );
	
	g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_GEAR );
}
// MPetItem::UseInventory's body (GameInit.cpp installs it through the
// host); the member is the library's, beside the pet's life countdown.
void	UsePetFromInventory(MItem* pPet)
{
	// Upstream once refused the call while a pet was already out, then
	// commented that out because the same item dismisses the pet, and
	// noted that marking the item that summoned the pet was not done
	// either (the motorcycle does not do it); the guard is kept as it
	// was left.
//	if(g_pPlayer->GetPetID() == OBJECTID_NULL)
//	{
	if(pPet->GetCurrentDurability() > 0)
	{
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( pPet->GetID() );
		_CGUseItemFromInventory.setX( pPet->GetGridX() );
		_CGUseItemFromInventory.setY( pPet->GetGridY() );

		g_pSocket->sendPacket( &_CGUseItemFromInventory );

		g_pPlayer->SetItemCheckBuffer( pPet, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	}
//	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MPetFood::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MPetFood::UseInventory()
	#endif
{
	const TYPE_OBJECTID petID = g_pPlayer->GetPetID();

	if(g_pPlayer != NULL && petID != OBJECTID_NULL
		)
	{
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );
		
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
		if(0 != SubInventoryItemID)
			_CGUseItemFromInventory.setInventoryItemObjectID( SubInventoryItemID );
	#endif
		g_pSocket->sendPacket( &_CGUseItemFromInventory );

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY, SubInventoryItemID);
	#else
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	#endif
		
	}
	else
	{
		// 2개 이상 있어야 분리 된다.
		if(GetNumber() > 1)
		{
			// 아이템 분리
			CGAddInventoryToMouse _CGAddInventoryToMouse;
			_CGAddInventoryToMouse.setObjectID( 0 );	// 분리한다는 의미
			_CGAddInventoryToMouse.setX( GetGridX() );
			_CGAddInventoryToMouse.setY( GetGridY() );
			
			g_pSocket->sendPacket( &_CGAddInventoryToMouse );				
			
			//----------------------------------------------------
			// Inventory에서 item을 몇개 드는 걸 검증받는다.
			// 일단은 1개만 들리도록 한다.
			//----------------------------------------------------
			g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY);
		}
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MSms_item::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MSms_item::UseInventory()
	#endif
{
	if(g_pPlayer != NULL )
	{
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );
		
		g_pSocket->sendPacket( &_CGUseItemFromInventory );
		
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MPetEnchantItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MPetEnchantItem::UseInventory()
	#endif
{
	// 2개 이상 있어야 분리 된다.
	if(GetNumber() > 1)
	{
		// 아이템 분리
		CGAddInventoryToMouse _CGAddInventoryToMouse;
		_CGAddInventoryToMouse.setObjectID( 0 );	// 분리한다는 의미
		_CGAddInventoryToMouse.setX( GetGridX() );
		_CGAddInventoryToMouse.setY( GetGridY() );
		
		g_pSocket->sendPacket( &_CGAddInventoryToMouse );				
		
		//----------------------------------------------------
		// Inventory에서 item을 몇개 드는 걸 검증받는다.
		// 일단은 1개만 들리도록 한다.
		//----------------------------------------------------
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY);
	}
}

std::string MPetItem::GetPetOptionName()
{
	std::string petOptionName;
	if(GetSilver() > 0/* && GetEnchantLevel()!=0xFFFF by viva */)
	{
		ITEMOPTION_TABLE::ITEMOPTION_PART optionPart = static_cast<ITEMOPTION_TABLE::ITEMOPTION_PART>(GetEnchantLevel());
		int optionLevel = (GetNumber()-10)/10;

		int size = g_pItemOptionTable->GetSize();
		bool bFound = false;

		for(int i = 1; i < size; i++)
		{
			const ITEMOPTION_INFO &optionInfo = g_pItemOptionTable->Get(i);
			if(optionInfo.Part == optionPart)
			{
				bFound = true;
				if(optionLevel == 0)
				{
					petOptionName = optionInfo.Name;
					petOptionName += " ";
					break;
				}
				else
				{
					optionLevel--;
				}
			}
		}
		if(optionLevel != 0 && bFound == true)	// 다른 옵션 파트가 나왔을때
		{
			switch(optionPart)
			{
			case ITEMOPTION_TABLE::PART_LUCK:
				petOptionName = (*g_pGameStringTable)[STRING_MESSAGE_OPTION_NAME_LUCK_3+optionLevel].GetString();
				petOptionName += " ";
				break;

			case ITEMOPTION_TABLE::PART_ALL_ATTR:
				petOptionName = (*g_pGameStringTable)[STRING_MESSAGE_OPTION_NAME_ATTR_3+optionLevel].GetString();
				petOptionName += " ";
				break;

			default:
				break;
			}
		}
	}

	return petOptionName;
}

std::string MPetItem::GetPetOptionEName()
{
	std::string petOptionName;
	if(GetSilver() > 0)
	{
		ITEMOPTION_TABLE::ITEMOPTION_PART optionPart = static_cast<ITEMOPTION_TABLE::ITEMOPTION_PART>(GetEnchantLevel());
		int optionLevel = (GetNumber()-10)/10;

		int size = g_pItemOptionTable->GetSize();
		bool bFound = false;

		for(int i = 1; i < size; i++)
		{
			const ITEMOPTION_INFO &optionInfo = g_pItemOptionTable->Get(i);
			if(optionInfo.Part == optionPart)
			{
				bFound = true;
				if(optionLevel == 0)
				{
					petOptionName = optionInfo.EName;
					petOptionName += " ";
					break;
				}
				else
				{
					optionLevel--;
				}
			}
		}
		if(optionLevel != 0 && bFound == true)	// 다른 옵션 파트가 나왔을때
		{
			switch(optionPart)
			{
			case ITEMOPTION_TABLE::PART_LUCK:
				petOptionName = (*g_pGameStringTable)[STRING_MESSAGE_OPTION_ENAME_LUCK_3+optionLevel].GetString();
				petOptionName += " ";
				break;

			case ITEMOPTION_TABLE::PART_ALL_ATTR:
				petOptionName = (*g_pGameStringTable)[STRING_MESSAGE_OPTION_ENAME_ATTR_3+optionLevel].GetString();
				petOptionName += " ";
				break;

			default:
				break;
			}
		}
	}

	return petOptionName;
}

std::string MPetItem::GetPetName()
{
	std::string petName = GetPetOptionName();
	switch(GetItemType())
	{
	case 1:
		petName += (*g_pCreatureTable)[688].Name.GetString();
		break;

	case 2:
		petName += (*g_pCreatureTable)[693].Name.GetString();
		break;

	case 3:
		petName += (*g_pCreatureTable)[700].Name.GetString();
		break;
	
	case 4:
		petName += (*g_pCreatureTable)[706].Name.GetString();
		break;

	case 5:
		petName += (*g_pCreatureTable)[711].Name.GetString();
		break;
		// add by coffee 2006-12-22
	case 6:
		petName += (*g_pCreatureTable)[400].Name.GetString();
		break;
		// end 2006-12-22
	
	}

	return petName;
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void	MEventGiftBoxItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MEventGiftBoxItem::UseInventory()
	#endif
{
	if(GetItemType() >= 6)
	{
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );
		
		g_pSocket->sendPacket( &_CGUseItemFromInventory );
		
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void MMoonCardItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void MMoonCardItem::UseInventory()
	#endif
{
	// 2개 이상 있어야 분리 된다.
	// edit by sonic 2006.11.1  将四叶草设为可分开
	//if(GetItemType() == 2 && GetNumber() > 1 && GetNumber() < GetMaxNumber())
	if((GetItemType() == 2 || GetItemType() == 3) && GetNumber() > 1 && GetNumber() < GetMaxNumber())
	{
		// 아이템 분리
		CGAddInventoryToMouse _CGAddInventoryToMouse;
		_CGAddInventoryToMouse.setObjectID( 0 );	// 분리한다는 의미
		_CGAddInventoryToMouse.setX( GetGridX() );
		_CGAddInventoryToMouse.setY( GetGridY() );
		
		g_pSocket->sendPacket( &_CGAddInventoryToMouse );				
		
		//----------------------------------------------------
		// Inventory에서 item을 몇개 드는 걸 검증받는다.
		// 일단은 1개만 들리도록 한다.
		//----------------------------------------------------
		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY);
	}
}

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
void MTrapItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void MTrapItem::UseInventory()
	#endif
{
	CGUseItemFromInventory _CGUseItemFromInventory;
	_CGUseItemFromInventory.setObjectID( GetID() );
	_CGUseItemFromInventory.setX( GetGridX() );
	_CGUseItemFromInventory.setY( GetGridY() );
	
	g_pSocket->sendPacket( &_CGUseItemFromInventory );
	
	g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
}
	

#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包

void MSubInventory::UseInventory(TYPE_OBJECTID SubInventoryItemID)
{
	CGUseItemFromInventory _CGUseItemFromInventory;
	_CGUseItemFromInventory.setObjectID( GetID() );
	_CGUseItemFromInventory.setX( GetGridX() );
	_CGUseItemFromInventory.setY( GetGridY() );
	
	g_pSocket->sendPacket( &_CGUseItemFromInventory );
	
	g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
}

void
MSubInventory::SetItemType(TYPE_ITEMTYPE type)
{
	switch(type)
	{
	case 0:	// 2*4 pack
		MGridItemManager::Init(2, 4);   
		break;
	
	//  2005.08.15 Sjheon 4x6 팩 Add	
	case 1:	// 4*6 pack
		MGridItemManager::Init(4, 6); 
		break;
	//  2005.08.15 Sjheon 4x6 팩 End

	default:
		MGridItemManager::Init(2, 4);
		break;
	}
	m_ItemType = type; 
}
//----------------------------------------------------------------------
// AddItem ( pItem )
//----------------------------------------------------------------------
// 적절한 slot에 pItem을 추가한다.
//----------------------------------------------------------------------
bool			
MSubInventory::AddItem(MItem* pItem)
{
	bool re = MGridItemManager::AddItem( pItem );
	SetEnchantLevel(MGridItemManager::GetItemNum()); // Enchant level을 아이템 갯수로 쓰기로 했음.
	return re;
}

//----------------------------------------------------------------------
// AddItem ( pItem, n )
//----------------------------------------------------------------------
// slot(n)에 pItem을 추가한다.
//----------------------------------------------------------------------
bool			
MSubInventory::AddItem(MItem* pItem, BYTE X, BYTE Y)
{
	bool re = MGridItemManager::AddItem( pItem, X, Y );
	SetEnchantLevel(MGridItemManager::GetItemNum()); // Enchant level을 아이템 갯수로 쓰기로 했음.
	return re;
}

//----------------------------------------------------------------------
// ReplaceItem
//----------------------------------------------------------------------
// pItem을 추가하고 딴게 있다면 Item교환
//----------------------------------------------------------------------
bool			
MSubInventory::ReplaceItem(MItem* pItem, BYTE X, BYTE Y, MItem*& pOldItem)
{
	bool re =  MGridItemManager::ReplaceItem( pItem, X, Y, pOldItem );
	SetEnchantLevel(MGridItemManager::GetItemNum()); // Enchant level을 아이템 갯수로 쓰기로 했음.
	return re;
}


//----------------------------------------------------------------------
// Can ReplaceItem : (n) slot에 pItem을 추가하거나 
//						원래 있던 Item과 교체가 가능한가?
//----------------------------------------------------------------------
bool			
MSubInventory::CanReplaceItem(MItem* pItem, BYTE X, BYTE Y, MItem*& pOldItem)
{
	return MGridItemManager::CanReplaceItem( pItem, X, Y, pOldItem );
}

#endif
