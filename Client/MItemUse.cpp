//----------------------------------------------------------------------
// MItemUse.cpp - using items
//----------------------------------------------------------------------
// Split from MItem.cpp (docs/reviews/RESTRUCTURING.md task 4.4): the factory
// table MItem::NewItem dispatches through and every item class with a
// UseInventory/UseQuickItem/UseGear body (MCorpse too: it owns a
// creature). A class lives here whole, so its vtable is emitted with its
// bodies. Executable-side until task 4.237, which put it in gamemodel:
// the bodies send through g_pSocket (GameSocket.h) and reach the player,
// the zone and the windows through PlayerState, CreatureState, ZoneState,
// DialogPresentation, NoticePresentation, VsUiPresentation and
// WorldEvents. MItem.cpp keeps the model half, the container-based gear
// (belt, arms band, motorcycle) included since the item managers joined
// it (task 4.3).
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MItem.h"
#include "MItemTable.h"
#include "MItemOptionTable.h"
#include "MGameStringTable.h"
#include "ClientConfig.h"
#include "MCreatureTable.h"
#include "MItemLimits.h"
#include "SkillDef.h"
#include "MSkillManager.h"
#include "MActionInfoTable.h"
#include "ClientFunction.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MZoneTable.h"
#include "GameSocket.h"
#include "GameMessages.h"
#include "../basic/CMessageArray.h"
#include "MQuickSlot.h"
#include "ZoneFlags.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "DialogPresentationHost.h"
#include "NoticePresentationHost.h"
#include "VsUiPresentationHost.h"
#include "WorldEventsHost.h"
#include "Packet/ClientPlayer.h"
#include "Packet/Cpackets/CGAddInventoryToMouse.h"
#include "Packet/Cpackets/CGReloadFromInventory.h"
#include "Packet/Cpackets/CGReloadFromQuickSlot.h"
#include "Packet/Cpackets/CGSkillToInventory.h"
#include "Packet/Cpackets/CGUseItemFromGear.h"
#include "Packet/Cpackets/CGUseItemFromInventory.h"
#include "Packet/Cpackets/CGUsePotionFromInventory.h"
#include "Packet/Cpackets/CGUsePotionFromQuickSlot.h"
#include "DebugLog.h"


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

	// vampire items
	MVampireRing::NewItem,
	MVampireBracelet::NewItem,
	MVampireNecklace::NewItem,
	MVampireCoat::NewItem,

	// skull
	MSkull::NewItem,

	// mace added - 2001/3/26
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
		// by csm: the second advancement's item classes
	MCarryingReceiver::NewItem,
	MShoulderArmor::NewItem,
	MDermis::NewItem,
	MPersona::NewItem,
	MFascia::NewItem,
	MMitten::NewItem,

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
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

// vampire items
MItem*	MVampireRing::NewItem()		{ return new MVampireRing; }
MItem*	MVampireBracelet::NewItem()	{ return new MVampireBracelet; }
MItem*	MVampireNecklace::NewItem()	{ return new MVampireNecklace; }
MItem*	MVampireCoat::NewItem()		{ return new MVampireCoat; }
MItem*	MVampireShoes::NewItem()	{ return new MVampireShoes; }
*/

//----------------------------------------------------------------------
// The item factory
//----------------------------------------------------------------------
// MItem::NewItem (MItem.cpp) range-checks the class and calls this through
// MItemHost::NewItem, which GameInit.cpp installs: a new object of the
// class itemClass names. The range check repeats here because the table
// is indexed with it; the NULL-entry check is cheap defence in depth
// should a future edit leave a table slot unfilled.
//----------------------------------------------------------------------
MItem*
NewItemOfClass(ITEM_CLASS itemClass)
{
	if (itemClass < 0 || itemClass >= MAX_ITEM_CLASS
		|| MItem::s_NewItemClassTable[itemClass] == NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] NewItemOfClass: no constructor for item class %d", itemClass);
		return NULL;
	}

	return (MItem*)(*MItem::s_NewItemClassTable[itemClass])();
}


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
	// The creature is the executable's (MCreature): it frees it.
	if (m_pCreature!=NULL)
	{
		ZoneState::DeleteCreature( m_pCreature );
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
// Can it be added to another item?
//----------------------------------------------------------------------
bool
MMagazine::IsInsertToItem(const MItem* pItem) const
{
	if (pItem==NULL)
	{
		return false;
	}

	//------------------------------------------------
	// A gun:
	//------------------------------------------------
	if (pItem->IsGunItem())
	{
		// one this magazine fits.
		if (GetGunClass()==pItem->GetItemClass())
		{
			return true;
		}

		return false;
	}

	// The same magazine?
	return MItem::IsInsertToItem( pItem );
}

//----------------------------------------------------------------------
//
//						MSlayer PortalItem
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Get MaxNumber - the charges
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER
MSlayerPortalItem::GetMaxNumber() const
{
	return (*g_pItemTable)[ITEM_CLASS_SLAYER_PORTAL_ITEM][m_ItemType].MaxNumber;
}

//----------------------------------------------------------------------
// Set EnchantLevel
// It holds the number of charges.
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
// Get ZoneName - the mark is the server's (the item's silver)
//----------------------------------------------------------------------
const char*
MVampirePortalItem::GetZoneName() const
{
	if (!IsMarked() || g_pZoneTable==NULL)
	{
		return NULL;
	}

	const ZONETABLE_INFO* pZoneInfo = g_pZoneTable->Get( m_ZoneID );

	return pZoneInfo!=NULL? pZoneInfo->Name.GetString() : NULL;
}

//----------------------------------------------------------------------
// Get MaxNumber - the charges
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
// It holds the number of charges.
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
	m_ZoneX = (d >> 8) & 0xFF;		// the 0xFF looks unneeded
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
// It holds the number of charges.
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
	PlayerState::SetItemCheckBuffer( pPotion, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
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
	// The skill can be used:
	//----------------------------------------------------
	if (bUseOK)//playerSkill < (*g_pActionInfoTable).GetMinResultActionInfo())
	{
		//if ((*g_pActionInfoTable)[playerSkill].IsTargetItem())
		{
			//----------------------------------------------------
			// nothing awaits verification.
			//----------------------------------------------------
			if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_NULL
				&& PlayerState::GetItemCheckBuffer()==NULL)
			{
				POINT fitPoint;			// where the holy water goes

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

						// (Removing it at once was left out.)
						//(*g_pInventory).RemoveItem( pItem->GetID() );

						//----------------------------------------------------
						// Wait for the server to verify the item's use.
						//----------------------------------------------------
						PlayerState::SetWaitVerify( PlayerState::WAIT_VERIFY_SKILL_SUCCESS );
						PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );

						if (auto* entry = g_pSkillInfoTable->GetMutable(useSkill)) {
							entry->SetNextAvailableTime();
						}

						//----------------------------------------------------
						// The skill's casting motion
						//----------------------------------------------------
						AddNewInventoryEffect( GetID(),
											useSkill, //+ (*g_pActionInfoTable).GetMinResultActionInfo(),
											g_pClientConfig->FPS*3	// 3 seconds
										);
					}
//								else
//								{
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

						PlayerState::SetWaitVerify( PlayerState::WAIT_VERIFY_SKILL_SUCCESS );
						PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );

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
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
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

		// Does the magazine fit the gun in hand?
		if (IsInsertToItem( pGunItem ))
		{
			CGReloadFromInventory _CGReloadFromInventory;
			_CGReloadFromInventory.setObjectID( GetID() );
			_CGReloadFromInventory.setX( GetGridX() );
			_CGReloadFromInventory.setY( GetGridY() );

			g_pSocket->sendPacket( &_CGReloadFromInventory );


			// (Removing it at once was left out.)
			//(*g_pInventory).RemoveItem( GetID() );

			//----------------------------------------------------
			// Wait for the server to verify the item's use.
			//----------------------------------------------------
			PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
		}
		else
		{
			// [help] the bullets do not fit
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
		// Does the magazine fit the gun in hand?
		//----------------------------------------------------
		if (IsInsertToItem( pGunItem ))
		{
			CGReloadFromQuickSlot _CGReloadFromQuickSlot;
			_CGReloadFromQuickSlot.setObjectID( GetID() );
			_CGReloadFromQuickSlot.setSlotID( GetItemSlot() );

			g_pSocket->sendPacket( &_CGReloadFromQuickSlot );


			//----------------------------------------------------
			// Wait for the server to verify the item's use.
			//----------------------------------------------------
			PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);

			//----------------------------------------------------
			// Keep the belt from being taken off.
			//----------------------------------------------------
			VsUiPresentation::LockGear();

//			__BEGIN_HELP_EVENT
//				// [help] using a magazine
//				ExecuteHelpEvent( HE_ITEM_USE_MAGAZINE );
//				// [help] using a belt item
//				ExecuteHelpEvent( HE_ITEM_USE_BELT_ITEM );
//			__END_HELP_EVENT
		}
		else
		{
			// [help] the bullets do not fit
//			__BEGIN_HELP_EVENT
//				ExecuteHelpEvent( HE_ITEM_DIFFERENT_BULLET_TYPE );
//			__END_HELP_EVENT

			WorldEvents::PlaySound( SOUND_ITEM_NO_MAGAZINE );

			// The magazine does not fit
			DEBUG_ADD("[Can't use Magazine] Wrong Magazine");
		}
	}
	else
	{
		//PlaySound( SOUND_ITEM_NO_MAGAZINE );

		// No gun
		DEBUG_ADD("[Can't use Magazine] No Gun");
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
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
	// A bomb or a mine...
	//-------------------------------------------
	// hard-coded -_-;
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
	// The skill can be used:
	//----------------------------------------------------
	if (bUseOK)//playerSkill < (*g_pActionInfoTable).GetMinResultActionInfo())
	{
		//----------------------------------------------------
		// nothing awaits verification.
		//----------------------------------------------------
		if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_NULL
			&& PlayerState::GetItemCheckBuffer()==NULL)
		{
			POINT fitPoint;			// where the bomb goes

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

				// (Removing it at once was left out.)
				//(*g_pInventory).RemoveItem( GetID() );

				//----------------------------------------------------
				// Wait for the server to verify the item's use.
				//----------------------------------------------------
				PlayerState::SetWaitVerify( PlayerState::WAIT_VERIFY_SKILL_SUCCESS );
				PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );

				// The attempt itself already takes the delay.
				//(*g_pSkillInfoTable)[useSkill].SetNextAvailableTime();

				//----------------------------------------------------
				// The skill's casting motion
				//----------------------------------------------------
				AddNewInventoryEffect( GetID(),
									useSkill, //+ (*g_pActionInfoTable).GetMinResultActionInfo(),
									g_pClientConfig->FPS*3	// 3 seconds
								);
			}
			else
			{
				// (no room: nothing is said)
			}
		}
	}
}

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MMoney::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MMoney::UseInventory()
	#endif
{
	// Only a stack of two or more splits.
	if(GetNumber() > 1)
	{
		// Split the stack
		CGAddInventoryToMouse _CGAddInventoryToMouse;
		_CGAddInventoryToMouse.setObjectID( 0 );	// 0: a split
		_CGAddInventoryToMouse.setX( GetGridX() );
		_CGAddInventoryToMouse.setY( GetGridY() );

		g_pSocket->sendPacket( &_CGAddInventoryToMouse );

		//----------------------------------------------------
		// Wait for the server to verify lifting some of the stack;
		// for now just one.
		//----------------------------------------------------
		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY);
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MMine::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MMine::UseInventory()
	#endif

{
	bool bUseOK = true;
	ACTIONINFO useSkill;

	//----------------------------------------------------
	// Can it be used?
	//----------------------------------------------------
	useSkill = SKILL_INSTALL_MINE;

	if (
		g_pSkillAvailable->IsEnableSkill( useSkill )
		&& (*g_pSkillInfoTable)[useSkill].IsAvailableTime()
		&& CreatureState::GetMoveDevice(PlayerState::Creature()) != CreatureState::MOVE_DEVICE_RIDE)
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
		// the direction the mine is laid in
		_CGSkillToInventory.setTargetX( CreatureState::GetDirection(PlayerState::Creature()) );
		_CGSkillToInventory.setSkillType( useSkill );

		g_pSocket->sendPacket( &_CGSkillToInventory );

		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);

		PlayerState::SetWaitVerify( PlayerState::WAIT_VERIFY_SKILL_SUCCESS );
		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );

		if (auto* entry = g_pSkillInfoTable->GetMutable(useSkill)) {
			entry->SetNextAvailableTime();
		}

		//----------------------------------------------------
		// The skill's casting motion
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
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MSlayerPortalItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MSlayerPortalItem::UseInventory()
	#endif

{
	if(g_bHolyLand == false					// not in the holy land,
		&& g_pZoneTable->Get( ZoneState::GetID() )->CannotUseSpecialItem == false		// special items allowed in the zone,
		&& IsAffectStatus()						// usable by the player,
		&& IsChargeItem() && GetNumber() > 0	// and charges left
		&& !ZoneState::HasHelicopter( PlayerState::GetID() )	// no helicopter called
		&& !PlayerState::HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
	{
		// a slayer uses the item
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
		if(0 != SubInventoryItemID)
			_CGUseItemFromInventory.setInventoryItemObjectID( SubInventoryItemID );
	#else

	#endif
		g_pSocket->sendPacket( &_CGUseItemFromInventory );


		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MVampirePortalItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MVampirePortalItem::UseInventory()
	#endif

{
	if(g_bHolyLand == false					// not in the holy land,
		&& g_pZoneTable->Get( ZoneState::GetID() )->CannotUseSpecialItem == false		// special items allowed in the zone,
		&& IsAffectStatus()						// usable by the player,
		&& IsChargeItem() && GetNumber() > 0	// and charges left
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
			// a vampire uses a skill
			CGSkillToInventory _CGSkillToInventory;
			_CGSkillToInventory.setObjectID( GetID() );
			_CGSkillToInventory.setX( GetGridX() );
			_CGSkillToInventory.setY( GetGridY() );
			_CGSkillToInventory.setSkillType( useSkill );
			//_CGSkillToInventory.setCEffectID( 0 );	// -_-;;

			g_pSocket->sendPacket( &_CGSkillToInventory );

			PlayerState::SetWaitVerify( PlayerState::WAIT_VERIFY_SKILL_SUCCESS );
			PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );

			if (auto* entry = g_pSkillInfoTable->GetMutable(useSkill)) {
				entry->SetNextAvailableTime();
			}

			//----------------------------------------------------
			// The skill's casting motion
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
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MEventStarItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MEventStarItem::UseInventory()
	#endif

{
	// Only a stack of two or more splits.
	if(GetNumber() > 1)
	{
		// Split the stack
		CGAddInventoryToMouse _CGAddInventoryToMouse;
		_CGAddInventoryToMouse.setObjectID( 0 );	// 0: a split
		_CGAddInventoryToMouse.setX( GetGridX() );
		_CGAddInventoryToMouse.setY( GetGridY() );

		g_pSocket->sendPacket( &_CGAddInventoryToMouse );

		//----------------------------------------------------
		// Wait for the server to verify lifting some of the stack;
		// for now just one.
		//----------------------------------------------------
		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY);
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MMixingItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MMixingItem::UseInventory()
	#endif

{
	if( GetItemType() <= 8 )
	{
		if( GetNumber() > 1 )	// split the stack
		{
			CGAddInventoryToMouse _CGAddInventoryToMouse;
			_CGAddInventoryToMouse.setObjectID( 0 );
			_CGAddInventoryToMouse.setX( GetGridX() );
			_CGAddInventoryToMouse.setY( GetGridY() );

			g_pSocket->sendPacket( &_CGAddInventoryToMouse );
			PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY );
		}
		else
		{
			// The forge's type counts from its first (A).
			int fc = VsUiPresentation::FORGE_CLASS_WEAPON;
			int ft = 0;

			if(GetItemType() <= 2)
			{
				fc = VsUiPresentation::FORGE_CLASS_WEAPON;
				ft = GetItemType();
			} else
			if(GetItemType() >= 3 && GetItemType() <= 5)
			{
				fc = VsUiPresentation::FORGE_CLASS_ARMOR;
				ft = GetItemType()-3;
			} else
			if(GetItemType() >= 6 && GetItemType() <= 8 )
			{
				fc = VsUiPresentation::FORGE_CLASS_ACCESSORY;
				ft = GetItemType()-6;
			}
			VsUiPresentation::RunMixingForge(fc,ft);
			PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_MIXING_ITEM );
		}
	} else
	if( GetItemType() >= 9 && GetItemType() <= 18 )
	{
		// Puritas splits on a right click.
		CGAddInventoryToMouse _CGAddInventoryToMouse;
		_CGAddInventoryToMouse.setObjectID( 0 );
		_CGAddInventoryToMouse.setX( GetGridX() );
		_CGAddInventoryToMouse.setY( GetGridY() );

		g_pSocket->sendPacket( &_CGAddInventoryToMouse );
		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY );
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void MEffectItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void MEffectItem::UseInventory()
	#endif

{
	// add by Coffee 2007-8-5
	if (GetItemType() >= 10 && GetItemType() <= 12)
	{
		VsUiPresentation::RunBulletinBoardWindow(this);
		return;
	}

	// a megaphone and the like
	CGUseItemFromInventory _CGUseItemFromInventory;

	_CGUseItemFromInventory.setObjectID( GetID() );
	_CGUseItemFromInventory.setX( GetGridX() );
	_CGUseItemFromInventory.setY( GetGridY() );
	g_pSocket->sendPacket( &_CGUseItemFromInventory );
	PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY );
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
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
		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void MVampireETC::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void MVampireETC::UseInventory()
	#endif
{
	//----------------------------------------------------
	//
	//					Transforming items - a vampire's
	//
	//----------------------------------------------------
	if (g_pZoneTable->Get( ZoneState::GetID() )->CannotUseSpecialItem == false )
	{
		ACTIONINFO useSkill;

		if ( GetItemClass() == ITEM_CLASS_SKULL )						// a wolf's head
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

				PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
			}
			return;
		}

		//----------------------------------------------------
		// The skill can be used:
		//----------------------------------------------------
		// useSkill is always set above (every other path returns)
		{
			if (//(*g_pActionInfoTable)[playerSkill].IsTargetItem()
				g_pSkillAvailable->IsEnableSkill( useSkill )
				&& (*g_pSkillInfoTable)[useSkill].IsAvailableTime())
			{
				//----------------------------------------------------
				// nothing awaits verification.
				//----------------------------------------------------
				if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_NULL
					&& PlayerState::GetItemCheckBuffer()==NULL)
				{
					CGSkillToInventory _CGSkillToInventory;
					_CGSkillToInventory.setObjectID( GetID() );
					_CGSkillToInventory.setX( GetGridX() );
					_CGSkillToInventory.setY( GetGridY() );
					_CGSkillToInventory.setSkillType( useSkill );
					//_CGSkillToInventory.setCEffectID( 0 );	// -_-;;
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
					if(0 != SubInventoryItemID)
						_CGSkillToInventory.setInventoryItemObjectID( SubInventoryItemID );
	#endif
					g_pSocket->sendPacket( &_CGSkillToInventory );


					// (Removing it at once was left out.)
					//(*g_pInventory).RemoveItem( GetID() );

					//----------------------------------------------------
					// Wait for the server to verify the item's use.
					//----------------------------------------------------
					PlayerState::SetWaitVerify( PlayerState::WAIT_VERIFY_SKILL_SUCCESS );
					PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY);


					if (auto* entry = g_pSkillInfoTable->GetMutable(useSkill)) {
						entry->SetNextAvailableTime();
					}

					//----------------------------------------------------
					// The skill's casting motion
					//----------------------------------------------------
					AddNewInventoryEffect( GetID(),
										useSkill, //+ (*g_pActionInfoTable).GetMinResultActionInfo(),
										g_pClientConfig->FPS*3	// 3 seconds
									);
				}
			}
		}
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MSkull::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MSkull::UseInventory()
	#endif

{
	//----------------------------------------------------
	//
	//					Transforming items - a vampire's
	//
	//----------------------------------------------------
	if (g_pZoneTable->Get( ZoneState::GetID() )->CannotUseSpecialItem == false )
	{
		ACTIONINFO useSkill;

		if ( GetItemClass() == ITEM_CLASS_SKULL && GetItemType() == 39)						// a wolf's head
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

				PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
			}
			return;
		}

		//----------------------------------------------------
		// The skill can be used:
		//----------------------------------------------------
		// useSkill is always set above (every other path returns)
		{
			if (//(*g_pActionInfoTable)[playerSkill].IsTargetItem()
				g_pSkillAvailable->IsEnableSkill( useSkill )
				&& (*g_pSkillInfoTable)[useSkill].IsAvailableTime())
			{
				//----------------------------------------------------
				// nothing awaits verification.
				//----------------------------------------------------
				if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_NULL
					&& PlayerState::GetItemCheckBuffer()==NULL)
				{
					CGSkillToInventory _CGSkillToInventory;
					_CGSkillToInventory.setObjectID( GetID() );
					_CGSkillToInventory.setX( GetGridX() );
					_CGSkillToInventory.setY( GetGridY() );
					_CGSkillToInventory.setSkillType( useSkill );
					//_CGSkillToInventory.setCEffectID( 0 );	// -_-;;

					g_pSocket->sendPacket( &_CGSkillToInventory );


					// (Removing it at once was left out.)
					//(*g_pInventory).RemoveItem( GetID() );

					//----------------------------------------------------
					// Wait for the server to verify the item's use.
					//----------------------------------------------------
					PlayerState::SetWaitVerify( PlayerState::WAIT_VERIFY_SKILL_SUCCESS );
					PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );

					if (auto* entry = g_pSkillInfoTable->GetMutable(useSkill)) {
						entry->SetNextAvailableTime();
					}

					//----------------------------------------------------
					// The skill's casting motion
					//----------------------------------------------------
					AddNewInventoryEffect( GetID(),
										useSkill, //+ (*g_pActionInfoTable).GetMinResultActionInfo(),
										g_pClientConfig->FPS*3	// 3 seconds
									);
				}
			}
		}
	}
}

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MKey::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MKey::UseInventory()
	#endif
{
	// No flag may lie around.
	if( !ZoneState::IsExistCorpseFromPlayer( PlayerState::Creature(), 670 ) )
	{
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
		if(0 != SubInventoryItemID)
			_CGUseItemFromInventory.setInventoryItemObjectID( SubInventoryItemID );
	#endif
		g_pSocket->sendPacket( &_CGUseItemFromInventory );

	//modify by viva for notice : i don't understand it :(
//	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
//		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY, SubInventoryItemID );
//	#else
		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
//	#endif
	//end

	} else
	{
		g_pSystemMessage->Add((*g_pGameStringTable)[UI_STRING_MESSAGE_CANNOT_ACTION_MOTORCYCLE_FLAG].GetString() );
	}

}
void	MKey::UseQuickItem()
{
	// 2004, 12, 16, sobeit add start - riding a motorcycle with a turret installed breaks, so it is refused
	if(PlayerState::HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
	{
		return ;
	}
	// 2004, 12, 16, sobeit add end
	CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
	_CGUsePotionFromQuickSlot.setObjectID( GetID() );
	_CGUsePotionFromQuickSlot.setSlotID( GetItemSlot() );

	g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );

	VsUiPresentation::LockGear();
	PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	// (Removing it at once was left out.)
	//(*g_pInventory).RemoveItem( GetID() );

	//----------------------------------------------------
	// Wait for the server to verify the item's use.
	//----------------------------------------------------
	PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	//----------------------------------------------------
	// Keep the belt from being taken off.
	//----------------------------------------------------
	VsUiPresentation::LockGear();

}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MEventTreeItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MEventTreeItem::UseInventory()
	#endif
{
	if(GetItemType() == 12 ||
		(GetItemType() >= 26 && GetItemType() <= 28))
	{
		bool bUseOK = true;

		bool bMasterWords = CreatureState::GetCreatureType(PlayerState::Creature())==CREATURETYPE_SLAYER_OPERATOR
					|| CreatureState::GetCreatureType(PlayerState::Creature())==CREATURETYPE_VAMPIRE_OPERATOR
					|| CreatureState::GetCreatureType(PlayerState::Creature())==CREATURETYPE_OUSTERS_OPERATOR;

		if(IsPlayerInSafePosition() || (ZoneState::Exists() && (ZoneState::GetID() == 1104 || ZoneState::GetID() == 1106 || ZoneState::GetID() == 1114 || ZoneState::GetID() == 1115)))
			// Not in a safe zone or a master lair
		{
			// unless in a castle or an operator
			if( !( ZoneState::GetID() >= 1201 && ZoneState::GetID() <= 1206 ) && !bMasterWords )
			{
				bUseOK = false;
				DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_NOT_USE_SAFETY_POSITION].GetString() );
				return;
			}
		}

		if( CreatureState::GetMoveDevice(PlayerState::Creature())==CreatureState::MOVE_DEVICE_RIDE )
		{
			if( PlayerState::GetRace()==RACE_SLAYER )
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_CANNOT_USE_RIDE_MOTORCYCLE].GetString() );
			else if (CreatureState::IsOusters(PlayerState::Creature()) )
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_CANNOT_USE_SUMMON_SYLPH].GetString() );

			return;
		}

		// Upstream then looked around the player (3 cells up and down, 2
		// across) for a tree or a board already standing (a corpse of
		// creature 482 or 650), to refuse with
		// STRING_MESSAGE_XMAS_TREE_CANNOT_USE or
		// STRING_MESSAGE_BULLETIN_BOARD_CANNOT_USE. It asked whether this
		// item, not the one on the ground, was a corpse, which it never is,
		// so it never refused: retired in task 4.237 (recorded, not fixed).
		if (bUseOK)
		{
			if(GetItemType() == 12)		// a Christmas tree
			{
				VsUiPresentation::RunXmasCardWindow(this);
			}
			else						// a bulletin board
			{
				VsUiPresentation::RunBulletinBoardWindow(this);
			}
		}
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
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
		&& GetItemType()!=17) // 14 is candy, 15 to 17 rice cakes
	{
		bUseOK = false;
		DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_NOT_USE_SAFETY_ZONE].GetString() );
		return;
	}

	if (bUseOK)
	{
		if(GetItemType() == 14||
		GetItemType() == 15||
		GetItemType() == 16||
		GetItemType() == 17) // candy and rice cakes only
		{
			CGUsePotionFromInventory _CGUsePotionFromInventory;
			_CGUsePotionFromInventory.setObjectID( GetID() );
			_CGUsePotionFromInventory.setX( GetGridX() );
			_CGUsePotionFromInventory.setY( GetGridY() );
			g_pSocket->sendPacket( &_CGUsePotionFromInventory );
			PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);

		}
		else
		{
			CGUseItemFromInventory _CGUseItemFromInventory;
			_CGUseItemFromInventory.setObjectID( GetID() );
			_CGUseItemFromInventory.setX( GetGridX() );
			_CGUseItemFromInventory.setY( GetGridY() );
			g_pSocket->sendPacket( &_CGUseItemFromInventory );
			PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);

		}

	}
}
void	MEventEtcItem::UseQuickItem()
{
	if(GetItemType() == 14||
		GetItemType() == 15||
		GetItemType() == 16||
		GetItemType() == 17) // candy and rice cakes only
	{
		if( CreatureState::IsOusters(PlayerState::Creature()) )
		{
			CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
			_CGUsePotionFromQuickSlot.setObjectID( GetID() );
			int slotID = GetItemSlot();

			if( g_pArmsBand1 != NULL )	// with the first arms band, as it is
			{
				if(g_pArmsBand1->GetItem( slotID ) == this)
					_CGUsePotionFromQuickSlot.setSlotID( slotID );
				else
					_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
			}
			else						// without it, the second's slots count from 3
			{
				_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
			}

		//					_CGUsePotionFromQuickSlot.setSlotID( slotID );
			g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );
			PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
		//	DEBUG_ADD_FORMAT("[ Ousters Item Use From QuickSlot ] %d %d",slotID, GetID() );

			//----------------------------------------------------
			// Keep the belt from being taken off.
			//----------------------------------------------------
			VsUiPresentation::LockGear();
		}
		else
		{
			CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
			_CGUsePotionFromQuickSlot.setObjectID( GetID() );
			_CGUsePotionFromQuickSlot.setSlotID( GetItemSlot() );

			g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );


			// (Removing it at once was left out.)
			//(*g_pInventory).RemoveItem( GetID() );

			//----------------------------------------------------
			// Wait for the server to verify the item's use.
			//----------------------------------------------------
			PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);

			//----------------------------------------------------
			// Keep the belt from being taken off.
			//----------------------------------------------------
			VsUiPresentation::LockGear();
		}
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MDyePotionItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MDyePotionItem::UseInventory()
	#endif

{
	// Ousters have no gender, so they cannot use the gender change item
	if( CreatureState::IsOusters(PlayerState::Creature()) && GetItemType() == 48 )
	{
		NoticePresentation::PopupMessage(STRING_MESSAGE_CANNOT_USE_OUSTERS);
	}
	else
	{
		// sonic (2006.10.29) dropped upstream's refusal of the master effect
		// colours (58 to 61) to advanced players.
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );

		g_pSocket->sendPacket( &_CGUseItemFromInventory );
		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	}
}
//
//void	MDyePotionItem::UseQuickItem()
//{
//#ifdef __GAME_CLIENT__
//
//	if( g_pPlayer->IsOusters() )
//	{
//		// Ousters have no gender, so they cannot use the gender change item
//		if(GetItemType() == 48)
//			UI_PopupMessage(STRING_MESSAGE_CANNOT_USE_OUSTERS);
//		else
//		{
//			CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
//			_CGUsePotionFromQuickSlot.setObjectID( GetID() );
//			int slotID = GetItemSlot();
//
//			if( g_pArmsBand1 != NULL )	// with the first arms band, as it is
//			{
//				if(g_pArmsBand1->GetItem( slotID ) == this)
//					_CGUsePotionFromQuickSlot.setSlotID( slotID );
//				else
//					_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
//			}
//			else						// without it, the second's slots count from 3
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
//			// Keep the belt from being taken off.
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
//		// (Removing it at once was left out.)
//		//(*g_pInventory).RemoveItem( GetID() );
//
//		//----------------------------------------------------
//		// Wait for the server to verify the item's use.
//		//----------------------------------------------------
//		g_pPlayer->SetItemCheckBuffer( this, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
//
//		//----------------------------------------------------
//		// Keep the belt from being taken off.
//		//----------------------------------------------------
//		UI_LockGear();
//	}
//#endif
//}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MOustersSummonGem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MOustersSummonGem::UseInventory()
	#endif
{
	if( IsChargeItem() && GetNumber() > 0 && IsAffectStatus() && !g_bZoneSafe &&
		!PlayerState::IsInSafeSector() && CreatureState::IsStop(PlayerState::Creature()) && !PlayerState::HasEffectStatus( EFFECTSTATUS_HAS_FLAG) )
	{
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
		if(0 != SubInventoryItemID)
			_CGUseItemFromInventory.setInventoryItemObjectID( SubInventoryItemID );
	#endif

		g_pSocket->sendPacket( &_CGUseItemFromInventory );

		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);

		if (auto* entry = g_pSkillInfoTable->GetMutable(SKILL_SUMMON_SYLPH)) {
			entry->SetAvailableTime( 4000 );
		}
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MCodeSheetItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MCodeSheetItem::UseInventory()
	#endif

{
	if( GetItemType() == 0 && GetItemOptionListCount() == 30)
	{
		VsUiPresentation::RunQuestInventory(this);
	}
}

void	MPotion::UseQuickItem()
{
	CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
	_CGUsePotionFromQuickSlot.setObjectID( GetID() );
	_CGUsePotionFromQuickSlot.setSlotID( GetItemSlot() );

	g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );


	// (Removing it at once was left out.)
	//(*g_pInventory).RemoveItem( GetID() );

	//----------------------------------------------------
	// Wait for the server to verify the item's use.
	//----------------------------------------------------
	PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);

	//----------------------------------------------------
	// Keep the belt from being taken off.
	//----------------------------------------------------
	VsUiPresentation::LockGear();

	// [help] using a belt item
//	__BEGIN_HELP_EVENT
///		ExecuteHelpEvent( HE_ITEM_USE_BELT_ITEM );
//	__END_HELP_EVENT
}

void	MOustersPupa::UseQuickItem()
{
	CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
	_CGUsePotionFromQuickSlot.setObjectID( GetID() );
	int slotID = GetItemSlot();

	if( g_pArmsBand1 != NULL )	// with the first arms band, as it is
	{
		if(g_pArmsBand1->GetItem( slotID ) == this)
			_CGUsePotionFromQuickSlot.setSlotID( slotID );
		else
			_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
	}
	else						// without it, the second's slots count from 3
	{
		_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
	}

//					_CGUsePotionFromQuickSlot.setSlotID( slotID );
	g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );
	PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
//	DEBUG_ADD_FORMAT("[ Ousters Item Use From QuickSlot ] %d %d",slotID, GetID() );

	//----------------------------------------------------
	// Keep the belt from being taken off.
	//----------------------------------------------------
	VsUiPresentation::LockGear();
}

void	MOustersComposMei::UseQuickItem()
{
	CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
	_CGUsePotionFromQuickSlot.setObjectID( GetID() );
	int slotID = GetItemSlot();

	if( g_pArmsBand1 != NULL )	// with the first arms band, as it is
	{
		if(g_pArmsBand1->GetItem( slotID ) == this)
			_CGUsePotionFromQuickSlot.setSlotID( slotID );
		else
			_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
	}
	else						// without it, the second's slots count from 3
	{
		_CGUsePotionFromQuickSlot.setSlotID( slotID + 3);
	}

//					_CGUsePotionFromQuickSlot.setSlotID( slotID );
	g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );
	PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
//	DEBUG_ADD_FORMAT("[ Ousters Item Use From QuickSlot ] %d %d",slotID, GetID() );

	//----------------------------------------------------
	// Keep the belt from being taken off.
	//----------------------------------------------------
	VsUiPresentation::LockGear();
}

void	MCoupleRing::UseGear()
{
	CGUseItemFromGear _CGUseItemFromGear;

	_CGUseItemFromGear.setObjectID ( GetID() );
	_CGUseItemFromGear.setPart ( GetItemSlot() );

	g_pSocket->sendPacket( &_CGUseItemFromGear );

	PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_GEAR );
}

void	MVampireCoupleRing::UseGear()
{
	CGUseItemFromGear _CGUseItemFromGear;

	_CGUseItemFromGear.setObjectID ( GetID() );
	_CGUseItemFromGear.setPart ( GetItemSlot() );

	g_pSocket->sendPacket( &_CGUseItemFromGear );

	PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_GEAR );
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

		PlayerState::SetItemCheckBuffer( pPet, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	}
//	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MPetFood::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MPetFood::UseInventory()
	#endif
{
	const TYPE_OBJECTID petID = CreatureState::GetPetID(PlayerState::Creature());

	if(PlayerState::Creature() != NULL && petID != OBJECTID_NULL
		)
	{
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
		if(0 != SubInventoryItemID)
			_CGUseItemFromInventory.setInventoryItemObjectID( SubInventoryItemID );
	#endif
		g_pSocket->sendPacket( &_CGUseItemFromInventory );

		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);

	}
	else
	{
		// Only a stack of two or more splits.
		if(GetNumber() > 1)
		{
			// Split the stack
			CGAddInventoryToMouse _CGAddInventoryToMouse;
			_CGAddInventoryToMouse.setObjectID( 0 );	// 0: a split
			_CGAddInventoryToMouse.setX( GetGridX() );
			_CGAddInventoryToMouse.setY( GetGridY() );

			g_pSocket->sendPacket( &_CGAddInventoryToMouse );

			//----------------------------------------------------
			// Wait for the server to verify lifting some of the stack;
			// for now just one.
			//----------------------------------------------------
			PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY);
		}
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MSms_item::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MSms_item::UseInventory()
	#endif
{
	if(PlayerState::Creature() != NULL )
	{
		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( GetID() );
		_CGUseItemFromInventory.setX( GetGridX() );
		_CGUseItemFromInventory.setY( GetGridY() );

		g_pSocket->sendPacket( &_CGUseItemFromInventory );

		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void	MPetEnchantItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void	MPetEnchantItem::UseInventory()
	#endif
{
	// Only a stack of two or more splits.
	if(GetNumber() > 1)
	{
		// Split the stack
		CGAddInventoryToMouse _CGAddInventoryToMouse;
		_CGAddInventoryToMouse.setObjectID( 0 );	// 0: a split
		_CGAddInventoryToMouse.setX( GetGridX() );
		_CGAddInventoryToMouse.setY( GetGridY() );

		g_pSocket->sendPacket( &_CGAddInventoryToMouse );

		//----------------------------------------------------
		// Wait for the server to verify lifting some of the stack;
		// for now just one.
		//----------------------------------------------------
		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY);
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
		if(optionLevel != 0 && bFound == true)	// past the part's options
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
		if(optionLevel != 0 && bFound == true)	// past the part's options
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
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
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

		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	}
}
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
void MMoonCardItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
	#else
void MMoonCardItem::UseInventory()
	#endif
{
	// Only a stack of two or more splits.
	// edit by sonic 2006.11.1: four-leaf clovers split too
	//if(GetItemType() == 2 && GetNumber() > 1 && GetNumber() < GetMaxNumber())
	if((GetItemType() == 2 || GetItemType() == 3) && GetNumber() > 1 && GetNumber() < GetMaxNumber())
	{
		// Split the stack
		CGAddInventoryToMouse _CGAddInventoryToMouse;
		_CGAddInventoryToMouse.setObjectID( 0 );	// 0: a split
		_CGAddInventoryToMouse.setX( GetGridX() );
		_CGAddInventoryToMouse.setY( GetGridY() );

		g_pSocket->sendPacket( &_CGAddInventoryToMouse );

		//----------------------------------------------------
		// Wait for the server to verify lifting some of the stack;
		// for now just one.
		//----------------------------------------------------
		PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY);
	}
}

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags
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

	PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
}


#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags within bags

void MSubInventory::UseInventory(TYPE_OBJECTID SubInventoryItemID)
{
	CGUseItemFromInventory _CGUseItemFromInventory;
	_CGUseItemFromInventory.setObjectID( GetID() );
	_CGUseItemFromInventory.setX( GetGridX() );
	_CGUseItemFromInventory.setY( GetGridY() );

	g_pSocket->sendPacket( &_CGUseItemFromInventory );

	PlayerState::SetItemCheckBuffer( this, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
}

void
MSubInventory::SetItemType(TYPE_ITEMTYPE type)
{
	switch(type)
	{
	case 0:	// 2*4 pack
		MGridItemManager::Init(2, 4);
		break;

	//  2005.08.15 Sjheon 4x6 pack Add
	case 1:	// 4*6 pack
		MGridItemManager::Init(4, 6);
		break;
	//  2005.08.15 Sjheon 4x6 pack End

	default:
		MGridItemManager::Init(2, 4);
		break;
	}
	m_ItemType = type;
}
//----------------------------------------------------------------------
// AddItem ( pItem )
//----------------------------------------------------------------------
// Adds pItem in a fitting slot.
//----------------------------------------------------------------------
bool
MSubInventory::AddItem(MItem* pItem)
{
	bool re = MGridItemManager::AddItem( pItem );
	SetEnchantLevel(MGridItemManager::GetItemNum()); // the enchant level holds the item count
	return re;
}

//----------------------------------------------------------------------
// AddItem ( pItem, n )
//----------------------------------------------------------------------
// Adds pItem in slot (n).
//----------------------------------------------------------------------
bool
MSubInventory::AddItem(MItem* pItem, BYTE X, BYTE Y)
{
	bool re = MGridItemManager::AddItem( pItem, X, Y );
	SetEnchantLevel(MGridItemManager::GetItemNum()); // the enchant level holds the item count
	return re;
}

//----------------------------------------------------------------------
// ReplaceItem
//----------------------------------------------------------------------
// Adds pItem, swapping it with what is there
//----------------------------------------------------------------------
bool
MSubInventory::ReplaceItem(MItem* pItem, BYTE X, BYTE Y, MItem*& pOldItem)
{
	bool re =  MGridItemManager::ReplaceItem( pItem, X, Y, pOldItem );
	SetEnchantLevel(MGridItemManager::GetItemNum()); // the enchant level holds the item count
	return re;
}


//----------------------------------------------------------------------
// Can ReplaceItem : can pItem go into slot (n)
//						or replace the item there?
//----------------------------------------------------------------------
bool
MSubInventory::CanReplaceItem(MItem* pItem, BYTE X, BYTE Y, MItem*& pOldItem)
{
	return MGridItemManager::CanReplaceItem( pItem, X, Y, pOldItem );
}

#endif
