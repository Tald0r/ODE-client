//----------------------------------------------------------------------
// MItem.cpp - the item model (gamemodel; docs/RESTRUCTURING.md task 4.4)
//----------------------------------------------------------------------
// The classes whose behaviour is pure game model: MItem itself and the
// gear/armour/weapon families, whose members read the item and option
// tables and their own state, and the container-based gear (belt, arms
// band, motorcycle) over the item managers (task 4.3). Item classes
// that act on use (potions, portals, pets, keys) live in MItemUse.cpp, compiled
// into the executable, because their UseInventory/UseQuickItem/UseGear
// bodies drive packets, the player, the zone and the dialogs; the
// per-class factory table is there too. The two reaches MItem itself
// had into the executable (the animation clock, the item-drop frame
// pack, the pet affect refresh) go through the MItemHost the
// executable installs at start-up (GameInit.cpp).
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "CrtCompat.h"
#include "MItem.h"
#include "MItemTable.h"
#include "MItemOptionTable.h"
#include "UserInformation.h"
#include "ClientConfig.h"
#include "MTimeItemManager.h"
#include "MGameStringTable.h"
#include "MItemLimits.h"

#include "domain/ItemClass.h"
#include "domain/ItemDurability.h"
#include "domain/ItemGrade.h"

#include <fstream>
#include <vector>
#include <algorithm>

//----------------------------------------------------------------------
// de-core's rules (third_party/decore) name the wire item classes by
// decore::itemclass, and its grade and durability table is keyed by
// them: each id must be this client's.
//----------------------------------------------------------------------
static_assert(decore::itemclass::Motorcycle == ITEM_CLASS_MOTORCYCLE);
static_assert(decore::itemclass::Potion == ITEM_CLASS_POTION);
static_assert(decore::itemclass::Water == ITEM_CLASS_WATER);
static_assert(decore::itemclass::HolyWater == ITEM_CLASS_HOLYWATER);
static_assert(decore::itemclass::Magazine == ITEM_CLASS_MAGAZINE);
static_assert(decore::itemclass::BombMaterial == ITEM_CLASS_BOMB_MATERIAL);
static_assert(decore::itemclass::Etc == ITEM_CLASS_ETC);
static_assert(decore::itemclass::Key == ITEM_CLASS_KEY);
static_assert(decore::itemclass::Ring == ITEM_CLASS_RING);
static_assert(decore::itemclass::Bracelet == ITEM_CLASS_BRACELET);
static_assert(decore::itemclass::Necklace == ITEM_CLASS_NECKLACE);
static_assert(decore::itemclass::Coat == ITEM_CLASS_COAT);
static_assert(decore::itemclass::Trouser == ITEM_CLASS_TROUSER);
static_assert(decore::itemclass::Shoes == ITEM_CLASS_SHOES);
static_assert(decore::itemclass::Sword == ITEM_CLASS_SWORD);
static_assert(decore::itemclass::Blade == ITEM_CLASS_BLADE);
static_assert(decore::itemclass::Shield == ITEM_CLASS_SHIELD);
static_assert(decore::itemclass::Cross == ITEM_CLASS_CROSS);
static_assert(decore::itemclass::Glove == ITEM_CLASS_GLOVE);
static_assert(decore::itemclass::Helm == ITEM_CLASS_HELM);
static_assert(decore::itemclass::SG == ITEM_CLASS_SG);
static_assert(decore::itemclass::SMG == ITEM_CLASS_SMG);
static_assert(decore::itemclass::AR == ITEM_CLASS_AR);
static_assert(decore::itemclass::SR == ITEM_CLASS_SR);
static_assert(decore::itemclass::Bomb == ITEM_CLASS_BOMB);
static_assert(decore::itemclass::Mine == ITEM_CLASS_MINE);
static_assert(decore::itemclass::Belt == ITEM_CLASS_BELT);
static_assert(decore::itemclass::LearningItem == ITEM_CLASS_LEARNINGITEM);
static_assert(decore::itemclass::Money == ITEM_CLASS_MONEY);
static_assert(decore::itemclass::Corpse == ITEM_CLASS_CORPSE);
static_assert(decore::itemclass::VampireRing == ITEM_CLASS_VAMPIRE_RING);
static_assert(decore::itemclass::VampireBracelet == ITEM_CLASS_VAMPIRE_BRACELET);
static_assert(decore::itemclass::VampireNecklace == ITEM_CLASS_VAMPIRE_NECKLACE);
static_assert(decore::itemclass::VampireCoat == ITEM_CLASS_VAMPIRE_COAT);
static_assert(decore::itemclass::Skull == ITEM_CLASS_SKULL);
static_assert(decore::itemclass::Mace == ITEM_CLASS_MACE);
static_assert(decore::itemclass::Serum == ITEM_CLASS_SERUM);
static_assert(decore::itemclass::VampireEtc == ITEM_CLASS_VAMPIRE_ETC);
static_assert(decore::itemclass::SlayerPortalItem == ITEM_CLASS_SLAYER_PORTAL_ITEM);
static_assert(decore::itemclass::VampirePortalItem == ITEM_CLASS_VAMPIRE_PORTAL_ITEM);
static_assert(decore::itemclass::EventGiftBox == ITEM_CLASS_EVENT_GIFT_BOX);
static_assert(decore::itemclass::EventStar == ITEM_CLASS_EVENT_STAR);
static_assert(decore::itemclass::VampireEarring == ITEM_CLASS_VAMPIRE_EARRING);
static_assert(decore::itemclass::Relic == ITEM_CLASS_RELIC);
static_assert(decore::itemclass::VampireWeapon == ITEM_CLASS_VAMPIRE_WEAPON);
static_assert(decore::itemclass::VampireAmulet == ITEM_CLASS_VAMPIRE_AMULET);
static_assert(decore::itemclass::QuestItem == ITEM_CLASS_QUEST_ITEM);
static_assert(decore::itemclass::EventTree == ITEM_CLASS_EVENT_TREE);
static_assert(decore::itemclass::EventEtc == ITEM_CLASS_EVENT_ETC);
static_assert(decore::itemclass::BloodBible == ITEM_CLASS_BLOOD_BIBLE);
static_assert(decore::itemclass::CastleSymbol == ITEM_CLASS_CASTLE_SYMBOL);
static_assert(decore::itemclass::CoupleRing == ITEM_CLASS_COUPLE_RING);
static_assert(decore::itemclass::VampireCoupleRing == ITEM_CLASS_VAMPIRE_COUPLE_RING);
static_assert(decore::itemclass::EventItem == ITEM_CLASS_EVENT_ITEM);
static_assert(decore::itemclass::DyePotion == ITEM_CLASS_DYE_POTION);
static_assert(decore::itemclass::ResurrectItem == ITEM_CLASS_RESURRECT_ITEM);
static_assert(decore::itemclass::MixingItem == ITEM_CLASS_MIXING_ITEM);
static_assert(decore::itemclass::OustersArmsband == ITEM_CLASS_OUSTERS_ARMSBAND);
static_assert(decore::itemclass::OustersBoots == ITEM_CLASS_OUSTERS_BOOTS);
static_assert(decore::itemclass::OustersChakram == ITEM_CLASS_OUSTERS_CHAKRAM);
static_assert(decore::itemclass::OustersCirclet == ITEM_CLASS_OUSTERS_CIRCLET);
static_assert(decore::itemclass::OustersCoat == ITEM_CLASS_OUSTERS_COAT);
static_assert(decore::itemclass::OustersPendent == ITEM_CLASS_OUSTERS_PENDENT);
static_assert(decore::itemclass::OustersRing == ITEM_CLASS_OUSTERS_RING);
static_assert(decore::itemclass::OustersStone == ITEM_CLASS_OUSTERS_STONE);
static_assert(decore::itemclass::OustersWristlet == ITEM_CLASS_OUSTERS_WRISTLET);
static_assert(decore::itemclass::Larva == ITEM_CLASS_LARVA);
static_assert(decore::itemclass::Pupa == ITEM_CLASS_PUPA);
static_assert(decore::itemclass::ComposMei == ITEM_CLASS_COMPOS_MEI);
static_assert(decore::itemclass::OustersSummonItem == ITEM_CLASS_OUSTERS_SUMMON_ITEM);
static_assert(decore::itemclass::EffectItem == ITEM_CLASS_EFFECT_ITEM);
static_assert(decore::itemclass::CodeSheet == ITEM_CLASS_CODE_SHEET);
static_assert(decore::itemclass::MoonCard == ITEM_CLASS_MOON_CARD);
static_assert(decore::itemclass::Sweeper == ITEM_CLASS_SWEEPER);
static_assert(decore::itemclass::PetItem == ITEM_CLASS_PET_ITEM);
static_assert(decore::itemclass::PetFood == ITEM_CLASS_PET_FOOD);
static_assert(decore::itemclass::PetEnchantItem == ITEM_CLASS_PET_ENCHANT_ITEM);
static_assert(decore::itemclass::LuckyBag == ITEM_CLASS_LUCKY_BAG);
static_assert(decore::itemclass::SMSItem == ITEM_CLASS_SMS_ITEM);
static_assert(decore::itemclass::CoreZap == ITEM_CLASS_CORE_ZAP);
static_assert(decore::itemclass::GQuestItem == ITEM_CLASS_GQUEST_ITEM);
static_assert(decore::itemclass::TrapItem == ITEM_CLASS_TRAP_ITEM);
static_assert(decore::itemclass::BloodBibleSign == ITEM_CLASS_BLOOD_BIBLE_SIGN);
static_assert(decore::itemclass::WarItem == ITEM_CLASS_WAR_ITEM);
static_assert(decore::itemclass::CarryingReceiver == ITEM_CLASS_CARRYING_RECEIVER);
static_assert(decore::itemclass::ShoulderArmor == ITEM_CLASS_SHOULDER_ARMOR);
static_assert(decore::itemclass::Dermis == ITEM_CLASS_DERMIS);
static_assert(decore::itemclass::Persona == ITEM_CLASS_PERSONA);
static_assert(decore::itemclass::Fascia == ITEM_CLASS_FASCIA);
static_assert(decore::itemclass::Mitten == ITEM_CLASS_MITTEN);
static_assert(decore::itemclass::Count == MAX_ITEM_CLASS);

//----------------------------------------------------------------------
//
// static members
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Korean names in use?
//----------------------------------------------------------------------
bool	MItem::s_bUseKorean	= true;

//----------------------------------------------------------------------
// Heights of a dropping item, per drop frame
//----------------------------------------------------------------------
int		MItem::s_DropHeight[MAX_DROP_COUNT] = 
{
	36, 49, 64, 36, 16, 4
};

//----------------------------------------------------------------------
// The executable's services MItem needs; NULL until GameInit installs
// them, and a test binary never does.
//----------------------------------------------------------------------
const MItemHost*	MItem::s_pHost = NULL;

//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------

MItem::MItem()
{
	m_ObjectType	= TYPE_ITEM;

	// 
	m_Number		= 1;

	//m_ItemClass		= ITEMCLASS_NULL;
	m_ItemType		= 0;//ITEMTYPE_NULL;
//	m_ItemOption	= 0;

	m_bDropping		= FALSE;	// 떨어지고 있는 중
	m_DropCount		= 0;		// 현재 count

	// identify
	m_bIdentified	= TRUE;

	m_pName = NULL;

	m_bAffectStatus = true;

	// Not in any grid, no durability yet, not offered in a trade: the
	// container that takes the item sets the first, the packet that
	// creates it the second, and the trade UI the third.
	m_GridX = 0;
	m_GridY = 0;
	m_CurrentDurability = 0;
	m_bTrade = FALSE;

	m_Silver = 0;

	m_Grade = -1;
	m_EnchantLevel = 0;
	m_Speed = 0;
	m_Quest = 0;
	m_ItemColorSet = 0xFFFF;
	m_persnal_price = -1;
	m_persnal = false;
}

MItem::~MItem()
{
	// Invalidate borrowed UI pointers for every ownership path. The callback
	// may compare this address, but must not read the already destroyed subtype.
	if (s_pHost != nullptr && s_pHost->ItemDestroyed != nullptr)
		s_pHost->ItemDestroyed(this);

	if (m_pName!=NULL)
	{
		delete [] m_pName;
	}
}


//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Is Insert To Item ( pItem )
//----------------------------------------------------------------------
// pItem에 추가될 수 있는가?
//----------------------------------------------------------------------
bool		
MItem::IsInsertToItem(const MItem* pItem) const
{
	if (pItem==NULL) 
	{
		return false;
	}

	//-------------------------------------------------
	// 쌓이는 Item이고 
	// class와 type이 같아야 쌓인다.	
	// 현재 개수가 한계를 넘지 않은 경우 
	// --> 추가 가능
	//-------------------------------------------------

	// (!)아직은 쌓이게 하면 안된다.

	///*
	if (pItem->IsPileItem() 
		&& GetItemClass()==pItem->GetItemClass()
		&& GetItemType()==pItem->GetItemType())
		//&& pItem->GetNumber() < pItem->GetMaxNumber())
	{
		return true;
	}
	//*/

	return false;
}

//----------------------------------------------------------------------
// Get Name
//----------------------------------------------------------------------
const char *
MItem::GetName()
{
	//---------------------------------------------------------
	// 정해진 이름이 따로 없다면.. 고정된 이름 return
	//---------------------------------------------------------
	if (m_pName==NULL)
	{
		//if (s_bUseKorean)
		{
			// In the teen build "<X> Head" is shown as "<X> Soul Stone".
			if(g_pUserInformation->GoreLevel == false && GetItemClass() == ITEM_CLASS_SKULL)
			{
				const char*	pszHName		= (*g_pItemTable)[GetItemClass()][m_ItemType].HName.GetString();
				const char*	pszSoulStone	= (*g_pGameStringTable)[STRING_MESSAGE_SOUL_STONE].GetString();

				//-----------------------------------------------------------
				// The head noun being replaced is the last 4 bytes of the
				// item name, so a name shorter than that has nothing to
				// substitute. Both strings come out of data files: MString
				// holds NULL for an entry the file never supplied, and the
				// replacement is not bounded by the noun it overwrites, so
				// the allocation has to be sized from both of them.
				//-----------------------------------------------------------
				if (pszHName!=NULL && pszSoulStone!=NULL && strlen(pszHName)>=4)
				{
					size_t	nPrefix = strlen(pszHName) - 4;

					const size_t soulBytes = strlen(pszSoulStone) + 1;
					m_pName = new char[nPrefix + soulBytes];

					memcpy(m_pName, pszHName, nPrefix);
					memcpy(m_pName + nPrefix, pszSoulStone, soulBytes);

					return m_pName;
				}
			}

			return (*g_pItemTable)[GetItemClass()][m_ItemType].HName;
		}
		//else 
//		{
		//	return (*g_pItemTable)[GetItemClass()][m_ItemType].EName; 
//		}
	}
	
	return m_pName;
}

//----------------------------------------------------------------------
// Get EnglishName
//----------------------------------------------------------------------
const char *
MItem::GetEName() const
{
	//---------------------------------------------------------
	// 정해진 이름이 따로 없다면.. 고정된 이름 return
	//---------------------------------------------------------
//	if (m_pName==NULL)
	{
		// In the teen build "<X>Head" / "<X>Skull" is shown as "<X>Soul Stone".
		if(g_pUserInformation->GoreLevel == false && GetItemClass() == ITEM_CLASS_SKULL)
		{
			static char sz_temp[256];

			const char*	pszEName		= (*g_pItemTable)[GetItemClass()][m_ItemType].EName.GetString();
			const char*	pszSoulStone	= (*g_pGameStringTable)[STRING_MESSAGE_SOUL_STONE].GetString();

			//---------------------------------------------------------------
			// The item name and the replacement are both data-file strings,
			// so neither length is bounded by anything in the code and every
			// write into sz_temp has to be clamped to what is left of it.
			//---------------------------------------------------------------
			if (pszEName!=NULL && pszSoulStone!=NULL)
			{
				Basic::CopyBounded(sz_temp, pszEName, sizeof(sz_temp)-1);
				sz_temp[sizeof(sz_temp)-1] = '\0';

				char *psz_temp = strstr(sz_temp, "Head");

				if(psz_temp == NULL)psz_temp = strstr(sz_temp, "Skull");

				if(psz_temp != NULL)
				{
					// Bytes between the match and the end of sz_temp.
					size_t	nRoom = sizeof(sz_temp) - (size_t)(psz_temp - sz_temp) - 1;

					Basic::CopyBounded(psz_temp, pszSoulStone, nRoom);
					psz_temp[nRoom] = '\0';

					return sz_temp;
				}
			}
		}
		return (*g_pItemTable)[GetItemClass()][m_ItemType].EName; 		
	}
	

	
	return m_pName;
}

//----------------------------------------------------------------------
// Set Name
//----------------------------------------------------------------------
// m_pName에 이름을 정해넣는다.
//----------------------------------------------------------------------
void
MItem::SetName(const char* pName)
{
	char* replacement = nullptr;
	if (pName) {
		const size_t length = strlen(pName);
		replacement = new char[length + 1];
		memcpy(replacement, pName, length + 1);
	}
	delete [] m_pName;
	m_pName = replacement;
}

//----------------------------------------------------------------------
// Get Description
//----------------------------------------------------------------------
const char*				
MItem::GetDescription() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].Description;
}

//----------------------------------------------------------------------
// Get Weight
//----------------------------------------------------------------------
TYPE_ITEM_WEIGHT		
MItem::GetWeight() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].Weight;
}

//----------------------------------------------------------------------
// Get Price
//----------------------------------------------------------------------
// A gear item whose grade is not -1 is priced by its grade, as the
// server's PriceManager prices every item whose getGrade() is not -1.
// Only gear is: the server sends a pet item's days since its last
// feeding in the grade field, while pricing the pet with no grade.
//----------------------------------------------------------------------
static bool
IsPricedByGrade(const MItem* pItem)
{
	return pItem->GetGrade() != -1 && pItem->IsGearItem();
}

TYPE_ITEM_PRICE
MItem::GetPrice() const
{
	TYPE_ITEM_PRICE price;
	if( IsPricedByGrade(this) )
		price = (*g_pItemTable)[GetItemClass()][m_ItemType].Price * ( 100 + (GetGrade()-4)*5 ) / 100;
	else
		price = (*g_pItemTable)[GetItemClass()][m_ItemType].Price;
	return price;
}

//----------------------------------------------------------------------
// Get Price Grade
//----------------------------------------------------------------------
// The grade the shop's price rule (decore::itemPrice and repairPrice,
// MPriceManager) scales the table price by: the item's grade when it
// is priced by grade, otherwise -1, which the rule reads as none.
//----------------------------------------------------------------------
int
MItem::GetPriceGrade() const
{
	return IsPricedByGrade(this) ? GetGrade() : -1;
}

//----------------------------------------------------------------------
// Get GridWidth
//----------------------------------------------------------------------
BYTE					
MItem::GetGridWidth() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].GridWidth;
}

//----------------------------------------------------------------------
// Get GridHeight
//----------------------------------------------------------------------
BYTE					
MItem::GetGridHeight() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].GridHeight;
}

//----------------------------------------------------------------------
// Get TileFrameID
//----------------------------------------------------------------------
TYPE_FRAMEID			
MItem::GetTileFrameID() const
{
	// 틴버전인 경우 머리는 보석으로 표시한다.
	if(g_pUserInformation->GoreLevel == false && GetItemClass() == ITEM_CLASS_SKULL)
		return 271;

	return (*g_pItemTable)[GetItemClass()][m_ItemType].TileFrameID;
}

//----------------------------------------------------------------------
// Get InventoryFrameID
//----------------------------------------------------------------------
TYPE_FRAMEID			
MItem::GetInventoryFrameID() const
{
	// 틴버전인 경우 머리는 보석으로 표시한다.
	if(g_pUserInformation->GoreLevel == false && GetItemClass() == ITEM_CLASS_SKULL)
		return 285;

	return (*g_pItemTable)[GetItemClass()][m_ItemType].InventoryFrameID;
}

//----------------------------------------------------------------------
// Get GearFrameID
//----------------------------------------------------------------------
TYPE_FRAMEID			
MItem::GetGearFrameID() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].GearFrameID;
}


//----------------------------------------------------------------------
// Get DropFrameID
//----------------------------------------------------------------------
TYPE_FRAMEID			
MItem::GetDropFrameID() const
{
	// 틴버전인 경우 머리는 보석으로 표시한다.
	if(g_pUserInformation->GoreLevel == false && GetItemClass() == ITEM_CLASS_SKULL)
		return 271;

	return (*g_pItemTable)[GetItemClass()][m_ItemType].DropFrameID;
}

//----------------------------------------------------------------------
// Get AddonMaleFrameID
//----------------------------------------------------------------------
TYPE_FRAMEID			
MItem::GetAddonMaleFrameID() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].AddonMaleFrameID;
}

//----------------------------------------------------------------------
// Get AddonFemaleFrameID
//----------------------------------------------------------------------
TYPE_FRAMEID			
MItem::GetAddonFemaleFrameID() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].AddonFemaleFrameID;
}

//----------------------------------------------------------------------
// Get UseSoundID
//----------------------------------------------------------------------
TYPE_SOUNDID			
MItem::GetUseSoundID() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].UseSoundID;
}

//----------------------------------------------------------------------
// Get TileSoundID
//----------------------------------------------------------------------
TYPE_SOUNDID			
MItem::GetTileSoundID() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].TileSoundID;
}

//----------------------------------------------------------------------
// Get InventorySoundID
//----------------------------------------------------------------------
TYPE_SOUNDID			
MItem::GetInventorySoundID() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].InventorySoundID;
}

//----------------------------------------------------------------------
// Get GearSoundID
//----------------------------------------------------------------------
TYPE_SOUNDID			
MItem::GetGearSoundID() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].GearSoundID;
}

//----------------------------------------------------------------------
// Is Gender For Male
//----------------------------------------------------------------------
bool		
MItem::IsGenderForMale() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].IsGenderForMale();
}

//----------------------------------------------------------------------
// Is Gender For Female
//----------------------------------------------------------------------
bool		
MItem::IsGenderForFemale() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].IsGenderForFemale();
}

//----------------------------------------------------------------------
// Is Gender For All
//----------------------------------------------------------------------
bool		
MItem::IsGenderForAll() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].IsGenderForAll();
}

int	
MItem::IsQuestItem() const
{
	// The flag is the item's own. The timed-item register (a quest reward
	// with a lifetime) also makes one, when the register exists; the flag
	// used to sit inside that condition and vanished without it.
	if (m_Quest)
		return 1;

	if (g_pTimeItemManager != NULL && g_pTimeItemManager->IsExist( GetID() ))
		return 1;

	return 0;
}

bool				
MItem::IsSpecialColorItem() const
{	
	if( IsUniqueItem() || IsQuestItem() )
		return true;
	
	return false;
}

//----------------------------------------------------------------------
// MHolyWater - Get MaxNumber
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MHolyWater::GetMaxNumber() const
{
	return MAX_HOLY_WATER_NUMBER;	
}

//----------------------------------------------------------------------
// MBomb - Get MaxNumber
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER	
MBomb::GetMaxNumber() const
{
	return MAX_BOMB_NUMBER;	
}

//----------------------------------------------------------------------
// MSerum - Get MaxNumber
//----------------------------------------------------------------------
TYPE_ITEM_NUMBER
MSerum::GetMaxNumber() const
{
	return MAX_SERUM_NUMBER;
}

//----------------------------------------------------------------------
// MUsePotionItem - UseInventory
//
// The body lives in the executable (GameInit.cpp installs it through
// the host), like the other use bodies in MItemUse.cpp; this one is
// defined here because MSerum's key function is above, so GCC emits
// MSerum's vtable in this library, and that vtable names the inherited
// UseInventory - a definition in the executable would leave a test
// binary with an undefined reference. MSVC emits vtables where an
// object is constructed and never saw the gap.
//----------------------------------------------------------------------
#ifdef __TEST_SUB_INVENTORY__
void	MUsePotionItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
#else
void	MUsePotionItem::UseInventory()
#endif
{
	if (s_pHost != NULL && s_pHost->UsePotionFromInventory != NULL)
		s_pHost->UsePotionFromInventory( this );
}

//----------------------------------------------------------------------
// Grade Policy Of / Has Durability
//----------------------------------------------------------------------
// The grade policy and the durability of a wire item class: the
// server's table (decore::gradePolicyOf and hasDurability, the rule its
// ConcreteItem reads), except for the two couple rings.
//
// The server builds the couple rings outside ConcreteItem, so the table
// gives them no grade and no durability, and the server reports a
// maximum durability of 1 for them, a placeholder only its price reads.
// The client keeps the ring's rule for them; the item description shows
// neither their luck nor their durability.
//----------------------------------------------------------------------
static decore::GradePolicy
GradePolicyOf(ITEM_CLASS itemClass)
{
	switch (itemClass)
	{
		case ITEM_CLASS_COUPLE_RING :
		case ITEM_CLASS_VAMPIRE_COUPLE_RING :
			return decore::GradePolicy::Accessory;

		default :
			return decore::gradePolicyOf((int)itemClass);
	}
}

static bool
HasDurability(ITEM_CLASS itemClass)
{
	switch (itemClass)
	{
		case ITEM_CLASS_COUPLE_RING :
		case ITEM_CLASS_VAMPIRE_COUPLE_RING :
			return true;

		default :
			return decore::hasDurability((int)itemClass);
	}
}

//----------------------------------------------------------------------
// Item Grade Offsets
//----------------------------------------------------------------------
// How far the item's grade moves each attribute under its class's
// grade policy (decore::gradeOffsets); all 0 for a class without one.
//----------------------------------------------------------------------
static decore::GradeOffsets
ItemGradeOffsets(const MItem* pItem)
{
	return decore::gradeOffsets(GradePolicyOf(pItem->GetItemClass()), pItem->GetGrade());
}

//----------------------------------------------------------------------
// Gear Max Durability
//----------------------------------------------------------------------
// The server's maximum durability for a gear item (decore::maxDurability:
// ConcreteItem::getMaxDurability, then computeMaxDurability): for a
// class that keeps a durability, the table durability moved by the
// grade's durability offset and floored at 1000, otherwise the table
// durability as it is; then scaled by the durability options'
// plus-points. Nothing caps it.
//----------------------------------------------------------------------
static int
GearMaxDurability(const MItem* pItem, bool hasDurability, int gradeDurabilityOffset)
{
	std::vector<int> plusPoints;

	const std::list<TYPE_ITEM_OPTION>& optionList = pItem->GetItemOptionList();
	std::list<TYPE_ITEM_OPTION>::const_iterator itr = optionList.begin();

	while(itr != optionList.end())
	{
		const ITEMOPTION_INFO& optionInfo = (*g_pItemOptionTable)[*itr];
		
		if (optionInfo.Part == ITEMOPTION_TABLE::PART_DURABILITY)
		{
			plusPoints.push_back(optionInfo.PlusPoint);
		}

		itr++;
	}

	const int tableDurability = (*g_pItemTable)[pItem->GetItemClass()][pItem->GetItemType()].Value1;

	return (int)decore::maxDurability((unsigned)tableDurability, hasDurability, gradeDurabilityOffset,
									  plusPoints.data(), (int)plusPoints.size());
}

//----------------------------------------------------------------------
// MGearItem - what the grade moves
//----------------------------------------------------------------------
// The item's value is the server's (ConcreteItem): the table value plus
// the grade's offset, floored where the server floors it. Only what the
// policy moves is shown: damage and critical for a weapon (-1, which
// the item description hides, for any other gear), luck for an
// accessory (-9999 otherwise), and for the armor policies the defense
// and protection with the offset and floored at 0; any other gear shows
// the table's defense and protection as they are.
//----------------------------------------------------------------------
static bool
IsWeaponPolicy(decore::GradePolicy policy)
{
	return policy == decore::GradePolicy::Weapon;
}

static bool
IsArmorPolicy(decore::GradePolicy policy)
{
	return policy == decore::GradePolicy::Cloth || policy == decore::GradePolicy::Grocery;
}

int
MGearItem::GetMaxDurability() const
{
	return GearMaxDurability(this, HasDurability(GetItemClass()), ItemGradeOffsets(this).durability);
}

int
MGearItem::GetProtectionValue() const
{
	const int tableValue = (*g_pItemTable)[GetItemClass()][m_ItemType].Value2;

	if (!IsArmorPolicy(GradePolicyOf(GetItemClass())))
	{
		return tableValue;
	}

	return max( 0, tableValue + ItemGradeOffsets(this).protection );
}

int
MGearItem::GetDefenseValue() const
{
	const int tableValue = (*g_pItemTable)[GetItemClass()][m_ItemType].Value6;

	if (!IsArmorPolicy(GradePolicyOf(GetItemClass())))
	{
		return tableValue;
	}

	return max( 0, tableValue + ItemGradeOffsets(this).defense );
}

int
MGearItem::GetMinDamage() const
{
	if (!IsWeaponPolicy(GradePolicyOf(GetItemClass())))
	{
		return MItem::GetMinDamage();
	}

	return max( 1, (*g_pItemTable)[GetItemClass()][m_ItemType].Value3 + ItemGradeOffsets(this).damage );
}

int
MGearItem::GetMaxDamage() const
{
	if (!IsWeaponPolicy(GradePolicyOf(GetItemClass())))
	{
		return MItem::GetMaxDamage();
	}

	return max( 1, (*g_pItemTable)[GetItemClass()][m_ItemType].Value4 + ItemGradeOffsets(this).damage );
}

int
MGearItem::GetCriticalHit() const
{
	if (!IsWeaponPolicy(GradePolicyOf(GetItemClass())))
	{
		return MItem::GetCriticalHit();
	}

	return max( 0, (*g_pItemTable)[GetItemClass()][m_ItemType].CriticalHit + ItemGradeOffsets(this).critical );
}

int
MGearItem::GetLucky() const
{
	if (GradePolicyOf(GetItemClass()) != decore::GradePolicy::Accessory)
	{
		return MItem::GetLucky();
	}

	return ItemGradeOffsets(this).luck;
}

int
MWeaponItem::GetToHit() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].ToHit;
}

//----------------------------------------------------------------------
// MBloodBibleSign::Get MaxDurability
//----------------------------------------------------------------------
// The client makes a blood bible sign itself, from GCBloodBibleSignInfo;
// the server builds no such item, so its table has no rule for the
// class. It keeps the gear rule it was written with: a durability the
// grade moves 1000 a step. The item description does not show it, and
// the shop never repairs one.
//----------------------------------------------------------------------
int
MBloodBibleSign::GetMaxDurability() const
{
	return GearMaxDurability(this, true, (GetGrade()-4)*1000);
}

//----------------------------------------------------------------------
// Get ItemOption Part
//----------------------------------------------------------------------
BYTE					
MItem::GetItemOptionPart(int OptionNum) const
{
	if(m_ItemOptionList.empty()) return (*g_pItemOptionTable)[0].Part;

	int count = 0;

	std::list<TYPE_ITEM_OPTION>::const_iterator itr = m_ItemOptionList.begin();

	while(itr != m_ItemOptionList.end() && count < OptionNum)
	{
		count++;
		itr++;
	}

	if(itr != m_ItemOptionList.end())
		return (*g_pItemOptionTable)[*itr].Part;
	else
		return (*g_pItemOptionTable)[0].Part;
}

//----------------------------------------------------------------------
// Get ItemOption Name
//----------------------------------------------------------------------
const char*				
MItem::GetItemOptionName(int OptionNum) const
{
	if(m_ItemOptionList.empty()) return(*g_pItemOptionTable)[0].Name;

	int count = 0;

	std::list<TYPE_ITEM_OPTION>::const_iterator itr = m_ItemOptionList.begin();

	while(itr != m_ItemOptionList.end() && count < OptionNum)
	{
		count++;
		itr++;
	}

	if(itr != m_ItemOptionList.end() && *itr < g_pItemOptionTable->GetSize())
		return (*g_pItemOptionTable)[*itr].Name;
	else
		return (*g_pItemOptionTable)[0].Name;
}

//----------------------------------------------------------------------
// Get ItemOption Name
//----------------------------------------------------------------------
const char*				
MItem::GetItemOptionEName(int OptionNum) const
{
	if(m_ItemOptionList.empty()) return (*g_pItemOptionTable)[0].EName;

	int count = 0;

	std::list<TYPE_ITEM_OPTION>::const_iterator itr = m_ItemOptionList.begin();

	while(itr != m_ItemOptionList.end() && count < OptionNum)
	{
		count++;
		itr++;
	}

	if(itr != m_ItemOptionList.end() && *itr < g_pItemOptionTable->GetSize())
		return (*g_pItemOptionTable)[*itr].EName;
	else
		return (*g_pItemOptionTable)[0].EName;
}

//----------------------------------------------------------------------
// Get ItemOption PlusPoint
//----------------------------------------------------------------------
BYTE					
MItem::GetItemOptionPlusPoint(int OptionNum) const
{
	if(m_ItemOptionList.empty()) return (*g_pItemOptionTable)[0].PlusPoint;

	int count = 0;

	std::list<TYPE_ITEM_OPTION>::const_iterator itr = m_ItemOptionList.begin();

	while(itr != m_ItemOptionList.end() && count < OptionNum)
	{
		count++;
		itr++;
	}

	if(itr != m_ItemOptionList.end())
		return (*g_pItemOptionTable)[*itr].PlusPoint;
	else
		return (*g_pItemOptionTable)[0].PlusPoint;
}

//----------------------------------------------------------------------
// Get ItemOption PlusRequireAbility
//----------------------------------------------------------------------
/*
BYTE					
MItem::GetItemOptionPlusRequireAbility() const
{
	return (*g_pItemOptionTable)[m_ItemOption].PlusRequireAbility;
}
*/

//----------------------------------------------------------------------
// Get ItemOption PriceMultiplier
//----------------------------------------------------------------------
DWORD					
MItem::GetItemOptionPriceMultiplier() const
{
	int re = 0;

	std::list<TYPE_ITEM_OPTION>::const_iterator itr = m_ItemOptionList.begin();

	while(itr != m_ItemOptionList.end())
	{
		re += (*g_pItemOptionTable)[*itr].PriceMultiplier;
		itr++;
	}

	return re;
}

//----------------------------------------------------------------------
// Get ItemOption ColorSet
//----------------------------------------------------------------------
WORD					
MItem::GetItemOptionColorSet(int OptionNum)
{	
	if(GetItemClass() == ITEM_CLASS_PET_ITEM)
	{
		// The player re-evaluates the pet's affect before its colour is read.
		RefreshAffect(this);
		if(GetItemColorSet() == 0xFFFF)
		{
			if(GetSilver() > 0)	// an attribute shows as a colour
			{
				ITEMOPTION_TABLE::ITEMOPTION_PART optionPart = static_cast<ITEMOPTION_TABLE::ITEMOPTION_PART>(GetEnchantLevel());

				int size = g_pItemOptionTable->GetSize();

				for(int i = 1; i < size; i++)
				{
					const ITEMOPTION_INFO &optionInfo = g_pItemOptionTable->Get(i);
					if(optionInfo.Part == optionPart && optionInfo.UpgradeOptionType == 0)
					{
						SetItemColorSet(optionInfo.ColorSet);
						
						break;
					}
				}
			}
			else
			{
				SetItemColorSet((*g_pItemOptionTable)[0].ColorSet);
			}
		}

		return GetItemColorSet();
	}
	
	if(IsQuestItem() )
		return QUEST_ITEM_COLOR;

	if(IsUniqueItem())
		return UNIQUE_ITEM_COLOR;
	
	if(m_ItemOptionList.empty()) return (*g_pItemOptionTable)[0].ColorSet;

	int count = 0;

	std::list<TYPE_ITEM_OPTION>::const_iterator itr = m_ItemOptionList.begin();

	while(itr != m_ItemOptionList.end() && count < OptionNum)
	{
		count++;
		itr++;
	}

	if(itr != m_ItemOptionList.end())
		return (*g_pItemOptionTable)[*itr].ColorSet;
	else
		return (*g_pItemOptionTable)[0].ColorSet;
}

//----------------------------------------------------------------------
// Get ItemOption ColorSet
//----------------------------------------------------------------------
int					
MItem::GetItemOptionRequireSTR() const
{	
	int re = 0;

	std::list<TYPE_ITEM_OPTION>::const_iterator itr = m_ItemOptionList.begin();

	while(itr != m_ItemOptionList.end())
	{
		re += (*g_pItemOptionTable)[*itr].RequireSTR;
		itr++;
	}

	return re;
}

//----------------------------------------------------------------------
// Get ItemOption ColorSet
//----------------------------------------------------------------------
int					
MItem::GetItemOptionRequireDEX() const
{	
	int re = 0;

	std::list<TYPE_ITEM_OPTION>::const_iterator itr = m_ItemOptionList.begin();

	while(itr != m_ItemOptionList.end())
	{
		re += (*g_pItemOptionTable)[*itr].RequireDEX;
		itr++;
	}

	return re;
}

//----------------------------------------------------------------------
// Get ItemOption ColorSet
//----------------------------------------------------------------------
int					
MItem::GetItemOptionRequireINT() const
{	
	int re = 0;

	std::list<TYPE_ITEM_OPTION>::const_iterator itr = m_ItemOptionList.begin();

	while(itr != m_ItemOptionList.end())
	{
		re += (*g_pItemOptionTable)[*itr].RequireINT;
		itr++;
	}

	return re;
}

//----------------------------------------------------------------------
// Get ItemOption ColorSet
//----------------------------------------------------------------------
int					
MItem::GetItemOptionRequireLevel() const
{	
	int re = 0;

	std::list<TYPE_ITEM_OPTION>::const_iterator itr = m_ItemOptionList.begin();

	while(itr != m_ItemOptionList.end())
	{
		re += (*g_pItemOptionTable)[*itr].RequireLevel;
		itr++;
	}

	return re;
}

//----------------------------------------------------------------------
// Get ItemOption ColorSet
//----------------------------------------------------------------------
int					
MItem::GetItemOptionRequireSUM() const
{	
	int re = 0;

	std::list<TYPE_ITEM_OPTION>::const_iterator itr = m_ItemOptionList.begin();

	while(itr != m_ItemOptionList.end())
	{
		re += (*g_pItemOptionTable)[*itr].RequireSUM;
		itr++;
	}

	return re;
}

//----------------------------------------------------------------------
// Get Require STR
//----------------------------------------------------------------------
int
MItem::GetRequireSTR() const
{
	int original = (*g_pItemTable)[GetItemClass()][m_ItemType].GetRequireSTR();
	int maxValue = 0;

	if (original==0 || IsQuestItem() )
	{
		return 0;
	}

	if( original <= MAX_SLAYER_ATTR_OLD)
		maxValue = MAX_SLAYER_ATTR_OLD;
	else
		maxValue = MAX_SLAYER_ATTR;
	
	original += (GetItemOptionRequireSUM()<<1);

	if( IsOustersItem() )
		return original;
	
	return min(original, maxValue);
	
	// option에 따른 증가치
	//return max(original, GetItemOptionRequireSTR());
		//(*g_pItemOptionTable)[m_ItemOption].PlusRequireAbility;
}

//----------------------------------------------------------------------
// Get Require DEX
//----------------------------------------------------------------------
int
MItem::GetRequireDEX() const
{
	int original = (*g_pItemTable)[GetItemClass()][m_ItemType].GetRequireDEX();
	int maxValue = 0;

	if (original==0 || IsQuestItem() )
	{
		return 0;
	}

	if( original <= MAX_SLAYER_ATTR_OLD )
		maxValue = MAX_SLAYER_ATTR_OLD;
	else
		maxValue = MAX_SLAYER_ATTR;

	original += (GetItemOptionRequireSUM()<<1);
	
	if( IsOustersItem() )
		return original;

	return min(original, maxValue);

	// option에 따른 증가치
	//return max(original, GetItemOptionRequireDEX());
	//+ (*g_pItemOptionTable)[m_ItemOption].PlusRequireAbility;
}

//----------------------------------------------------------------------
// Get Require INT
//----------------------------------------------------------------------
int
MItem::GetRequireINT() const
{
	int original = (*g_pItemTable)[GetItemClass()][m_ItemType].GetRequireINT();
	int maxValue = 0;

	if (original==0 || IsQuestItem() )
	{
		return 0;
	}

	if( original <= MAX_SLAYER_ATTR_OLD)
		maxValue = MAX_SLAYER_ATTR_OLD;
	else
		maxValue = MAX_SLAYER_ATTR;

	original += (GetItemOptionRequireSUM()<<1);

	if( IsOustersItem() )
		return original;

	return min(original, maxValue);

	// option에 따른 증가치
	//return max(original, GetItemOptionRequireINT());
	//+ (*g_pItemOptionTable)[m_ItemOption].PlusRequireAbility;
}

//----------------------------------------------------------------------
// Get Require SUM
//----------------------------------------------------------------------
int
MItem::GetRequireSUM() const
{
	int original = (*g_pItemTable)[GetItemClass()][m_ItemType].GetRequireSUM();
	int maxValue = 0;

	if (original==0 || IsQuestItem() )
	{
		return 0;
	}
	if( original <= MAX_SLAYER_ATTR_SUM_OLD )
		maxValue = MAX_SLAYER_ATTR_SUM_OLD;
	else
		maxValue = MAX_SLAYER_ATTR_SUM;
	original += GetItemOptionRequireSUM();

	if( IsOustersItem() )
		return original;

	return min(original, maxValue);

	// option에 따른 증가치
	//return max(original, GetItemOptionRequireSUM());
	//+ (*g_pItemOptionTable)[m_ItemOption].PlusRequireAbility;
}

//----------------------------------------------------------------------
// Get Require Level
//----------------------------------------------------------------------
int
MItem::GetRequireLevel() const
{
	int original = (*g_pItemTable)[GetItemClass()][m_ItemType].GetRequireLevel();
	int maxValue = 0;
	
	if(original == 0 || IsQuestItem() )
		return 0;

	if( original <= 100 )
		maxValue = MAX_VAMPIRE_LEVEL_OLD;
	else
		maxValue = MAX_VAMPIRE_LEVEL;

	// option에 따른 증가치
	original += GetItemOptionRequireLevel();

	if( IsOustersItem() )
		return original;

	return min(original, maxValue);
	//return max(original, GetItemOptionRequireLevel());
	//+ (*g_pItemOptionTable)[m_ItemOption].PlusRequireAbility;
}

//----------------------------------------------------------------------
// Get UseActionInfo
//----------------------------------------------------------------------
TYPE_ACTIONINFO 
MItem::GetUseActionInfo() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].UseActionInfo;
}

//----------------------------------------------------------------------
// Get Original Speed
//----------------------------------------------------------------------
int			
MItem::GetOriginalSpeed() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].Value7;
}

//----------------------------------------------------------------------
// Get Speed
//----------------------------------------------------------------------
int
MItem::GetSpeed() const		
{ 
	return m_Speed; 
}

//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
/*
void	
MItem::SaveToFile(ofstream& file)
{
	MObject::SaveToFile(file);		
	file.write((const char*)&m_SpriteID, SIZE_SPRITEID);	
}
		
//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
void	
MItem::LoadFromFile(ifstream& file)
{
	MObject::LoadFromFile(file);
	file.read((char*)&m_SpriteID, SIZE_SPRITEID);	
}
*/

//----------------------------------------------------------------------
// Set Dropping
//----------------------------------------------------------------------
void		
MItem::SetDropping()
{
	TYPE_FRAMEID dropID = GetDropFrameID();
	// No drop animation for this item.
	if (dropID==FRAMEID_NULL)
	{
		return;
	}

	// The drop animation's frame count is in the executable's item-drop
	// frame pack; without a host there is no animation to run.
	if (s_pHost==NULL)
	{
		return;
	}

	CAnimationFrame::SetFrameID( dropID, s_pHost->DropFrameCount(dropID) );

	m_DropCount = 0;
	m_bDropping = TRUE;
}

//----------------------------------------------------------------------
// Next Drop Frame
//----------------------------------------------------------------------
void		
MItem::NextDropFrame()
{
	CAnimationFrame::NextFrame();

	if (++m_DropCount >= MAX_DROP_COUNT)
	{
		m_CurrentFrame = m_MaxFrame - 1;	// 마지막 frame으로..
		m_DropCount--;						// 하나 빼줘야 하는 듯 - -;
		m_bDropping = FALSE;
	}
}


void
MItem::RemoveItemOption(TYPE_ITEM_OPTION option)
{
	std::list<TYPE_ITEM_OPTION>::iterator itr = std::find(m_ItemOptionList.begin(), m_ItemOptionList.end(), option);

	if(itr != m_ItemOptionList.end())
		m_ItemOptionList.erase(itr);
}

void							
MItem::AddItemOption(TYPE_ITEM_OPTION option)
{
	if(option == 0)
		return;

	m_ItemOptionList.push_back(option);
}

void							
MItem::ChangeItemOption(TYPE_ITEM_OPTION ori_option, TYPE_ITEM_OPTION new_option)
{
	if(new_option == 0)
		RemoveItemOption(ori_option);

	std::list<TYPE_ITEM_OPTION>::iterator itr = std::find(m_ItemOptionList.begin(), m_ItemOptionList.end(), ori_option);

	if(itr != m_ItemOptionList.end())
	{
		*itr = new_option;
	}
}

// Unique items cycle their colour set with the animation clock
int
MItem::GetUniqueItemColorset()
{
	int grade = CurrentFrame()%28;	// 15*2-2
	if(grade > 14)
		grade = 28-grade;
	return g_pClientConfig->UniqueItemColorSet+grade;
}

int
MItem::GetQuestItemColorset()
{
	int grade = CurrentFrame() % 28;

	if ( grade > 14 )
		grade = 28 - grade;
	
	return g_pClientConfig->QuestItemColorSet+grade;
}

int					
MItem::GetSpecialColorItemColorset()
{
	if(IsQuestItem() )
	{
		return GetQuestItemColorset();
	} else
	if(IsUniqueItem() )
	{
		return GetUniqueItemColorset();
	}
	
	return 0xffff;
}

int					
MItem::GetSpecialColorItemColorset(unsigned short srcColor)
{
	switch( srcColor )
	{
	case UNIQUE_ITEM_COLOR :
		return GetUniqueItemColorset();
		break;
	case QUEST_ITEM_COLOR :
		return GetQuestItemColorset();
		break; 
	}
	
	return 0xffff;	
}

// Rare items: the option colour set (the cycling variant is kept below, disabled)
int	MItem::GetRareItemColorset()
{
/*
	int ColorSet = CurrentFrame() % (28*GetItemOptionListCount());
	int percent = CurrentFrame()%28;

	ColorSet = GetItemOptionColorSet(ColorSet/28);
	int grade = ColorSet%15;
	ColorSet = ColorSet - grade;

	if(percent > 14)
		percent = 28 - percent;
*/

	return GetItemOptionColorSet();//ColorSet+grade;//*percent/14;
}

TYPE_ITEM_NUMBER
MResurrectItem::GetMaxNumber() const
{
	return MAX_RESURRECT_SCROLL_NUMBER;
}

TYPE_ITEM_NUMBER
MOustersLarva::GetMaxNumber() const
{
	return MAX_OUSTERS_LARVA_NUMBER;
}

TYPE_ITEM_NUMBER		
MLuckyBag::GetMaxNumber() const
{
	return MAX_LUCKY_BAG_NUMBER;	
}


//----------------------------------------------------------------------
// MMotorcycle::Get MaxDurability
//----------------------------------------------------------------------
int
MMotorcycle::GetMaxDurability() const	
{ 
	int maxDur = (*g_pItemTable)[ITEM_CLASS_MOTORCYCLE][m_ItemType].Value1; 

	int plus_point = 100;

	std::list<TYPE_ITEM_OPTION>::const_iterator itr = m_ItemOptionList.begin();

	while(itr != m_ItemOptionList.end())
	{
		const ITEMOPTION_INFO& optionInfo = (*g_pItemOptionTable)[*itr];
		
		if (optionInfo.Part == ITEMOPTION_TABLE::PART_DURABILITY)
		{
			plus_point += optionInfo.PlusPoint-100;
		}

		itr++;
	}

	if(plus_point != 0)
		maxDur = maxDur * plus_point / 100;

	return min( 65000, maxDur );
}


//----------------------------------------------------------------------
//
//						MBelt
// 
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Set ItemType
//----------------------------------------------------------------------
// ItemType을 설정할때 SlotItemManager도 초기화해야한다.
//----------------------------------------------------------------------
void				
MBelt::SetItemType(TYPE_ITEMTYPE type)		
{ 
	m_ItemType = type; 

	MSlotItemManager::Init( GetPocketNumber() );
}

//----------------------------------------------------------------------
// AddItem ( pItem )
//----------------------------------------------------------------------
// 적절한 slot에 pItem을 추가한다.
//----------------------------------------------------------------------
bool			
MBelt::AddItem(MItem* pItem)
{
	//---------------------------------------------------------------
	// 비어있는 slot을 찾아서 item을 추가한다.
	//---------------------------------------------------------------
	for (int n=0; n<m_Size; n++)
	{
		if (m_ItemSlot[n]==NULL)
		{
			return AddItem( pItem, n );
		}
	}

	return false;
}

//----------------------------------------------------------------------
// AddItem ( pItem, n )
//----------------------------------------------------------------------
// slot(n)에 pItem을 추가한다.
//----------------------------------------------------------------------
bool			
MBelt::AddItem(MItem* pItem, BYTE n)
{
	if (n >= m_Size)
	{
		return false;
	}

	//---------------------------------------------------------------
	// Quick item이어야지 belt에 추가할 수 있다.
	//---------------------------------------------------------------
	if (pItem->IsQuickItem())
	{
		return MSlotItemManager::AddItem( pItem, n );
	}

	return false;	
}

//----------------------------------------------------------------------
// ReplaceItem
//----------------------------------------------------------------------
// pItem을 추가하고 딴게 있다면 Item교환
//----------------------------------------------------------------------
bool			
MBelt::ReplaceItem(MItem* pItem, BYTE n, MItem*& pOldItem)
{
	if (n >= m_Size || pItem == NULL)
	{
		return false;
	}

	//---------------------------------------------------------------
	// 비어 있는 slot이면 그냥 추가
	//---------------------------------------------------------------
	if (m_ItemSlot[n]==NULL)
	{
		return AddItem( pItem, n );
	}
	
	//---------------------------------------------------------------
	// 뭔가 있다면 replace해야 한다.
	// 바꿀려는 Item이 quickItem인 경우만 교체..
	//---------------------------------------------------------------	
	if (pItem->IsQuickItem())
	{
		return MSlotItemManager::ReplaceItem( pItem, n, pOldItem );
	}

	return false;
}


//----------------------------------------------------------------------
// Can ReplaceItem : (n) slot에 pItem을 추가하거나 
//						원래 있던 Item과 교체가 가능한가?
//----------------------------------------------------------------------
bool			
MBelt::CanReplaceItem(MItem* pItem, BYTE n, MItem*& pOldItem)
{
	//---------------------------------------------------------
	// ItemSlot 범위를 넘어가는 경우..
	//---------------------------------------------------------	
	if (n>=m_Size)
	{		
		// NULL로 설정한다.
		pOldItem = NULL;

		return false;
	}

	//---------------------------------------------------------	
	// QuickItem인가?
	//---------------------------------------------------------	
	if (pItem->IsQuickItem())
	{		
		pOldItem = m_ItemSlot[n];

		return true;
	}

	pOldItem = NULL;

	return false;
}

//----------------------------------------------------------------------
// Find Slot To Add Item
//----------------------------------------------------------------------
// return값은 자리가 있느냐(true) / 없느냐(false)
// true인 경우에.. pItem이 들어갈 수 있는 자리가 slot이다.
//----------------------------------------------------------------------
bool			
MBelt::FindSlotToAddItem(MItem* pItem, int &slot) const
{
	if (!pItem->IsQuickItem())
	{
		return false;
	}

	//---------------------------------------------------------
	// 어디에 들어갈 수 있을까?
	//---------------------------------------------------------
	for (int i=0; i<m_Size; i++)
	{
		const MItem* pQuickItem = m_ItemSlot[i];

		//---------------------------------------------------------
		// 아무것도 없는 곳이면 그냥 넣으면 된다.
		//---------------------------------------------------------
		if (pQuickItem==NULL)
		{
			slot = i;
			return true;
		}
		//---------------------------------------------------------
		// 뭔가 있으면.. 그곳에 쌓일 수 있는지 알아본다.
		//---------------------------------------------------------
		if (pQuickItem->GetItemClass()==pItem->GetItemClass()
			&& pQuickItem->GetItemType()==pItem->GetItemType())
		{
			//----------------------------------------------------
			// 더한 개수가 max를 넘지 않아야 한다.
			//----------------------------------------------------
			int addTotal = pQuickItem->GetNumber() + pItem->GetNumber();

			if ( static_cast<TYPE_ITEM_NUMBER>(addTotal) <= pQuickItem->GetMaxNumber() )
			{
				slot = i;
				return true;
			}
		}		
	}

	// 들어갈 곳이 없다.
	return false;
}

//----------------------------------------------------------------------
//
//						MOustersArmsBand
// 
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Set ItemType
//----------------------------------------------------------------------
// ItemType을 설정할때 SlotItemManager도 초기화해야한다.
//----------------------------------------------------------------------
void				
MOustersArmsBand::SetItemType(TYPE_ITEMTYPE type)		
{ 
	m_ItemType = type; 

	MSlotItemManager::Init( GetPocketNumber() );
}

//----------------------------------------------------------------------
// AddItem ( pItem )
//----------------------------------------------------------------------
// 적절한 slot에 pItem을 추가한다.
//----------------------------------------------------------------------
bool			
MOustersArmsBand::AddItem(MItem* pItem)
{
	//---------------------------------------------------------------
	// 비어있는 slot을 찾아서 item을 추가한다.
	//---------------------------------------------------------------
	for (int n=0; n<m_Size; n++)
	{
		if (m_ItemSlot[n]==NULL)
		{
			return AddItem( pItem, n );
		}
	}

	return false;
}

//----------------------------------------------------------------------
// AddItem ( pItem, n )
//----------------------------------------------------------------------
// slot(n)에 pItem을 추가한다.
//----------------------------------------------------------------------
bool			
MOustersArmsBand::AddItem(MItem* pItem, BYTE n)
{
	if (n >= m_Size)
	{
		return false;
	}

	//---------------------------------------------------------------
	// Quick item이어야지 belt에 추가할 수 있다.
	//---------------------------------------------------------------
	if (pItem->IsQuickItem())
	{
		return MSlotItemManager::AddItem( pItem, n );
	}

	return false;	
}

//----------------------------------------------------------------------
// ReplaceItem
//----------------------------------------------------------------------
// pItem을 추가하고 딴게 있다면 Item교환
//----------------------------------------------------------------------
bool			
MOustersArmsBand::ReplaceItem(MItem* pItem, BYTE n, MItem*& pOldItem)
{
	if (n >= m_Size || pItem == NULL)
	{
		return false;
	}

	//---------------------------------------------------------------
	// 비어 있는 slot이면 그냥 추가
	//---------------------------------------------------------------
	if (m_ItemSlot[n]==NULL)
	{
		return AddItem( pItem, n );
	}
	
	//---------------------------------------------------------------
	// 뭔가 있다면 replace해야 한다.
	// 바꿀려는 Item이 quickItem인 경우만 교체..
	//---------------------------------------------------------------	
	if (pItem->IsQuickItem())
	{
		return MSlotItemManager::ReplaceItem( pItem, n, pOldItem );
	}

	return false;
}


//----------------------------------------------------------------------
// Can ReplaceItem : (n) slot에 pItem을 추가하거나 
//						원래 있던 Item과 교체가 가능한가?
//----------------------------------------------------------------------
bool			
MOustersArmsBand::CanReplaceItem(MItem* pItem, BYTE n, MItem*& pOldItem)
{
	//---------------------------------------------------------
	// ItemSlot 범위를 넘어가는 경우..
	//---------------------------------------------------------	
	if (n>=m_Size)
	{		
		// NULL로 설정한다.
		pOldItem = NULL;

		return false;
	}

	//---------------------------------------------------------	
	// QuickItem인가?
	//---------------------------------------------------------	
	if (pItem->IsQuickItem())
	{		
		pOldItem = m_ItemSlot[n];

		return true;
	}

	pOldItem = NULL;

	return false;
}

//----------------------------------------------------------------------
// Find Slot To Add Item
//----------------------------------------------------------------------
// return값은 자리가 있느냐(true) / 없느냐(false)
// true인 경우에.. pItem이 들어갈 수 있는 자리가 slot이다.
//----------------------------------------------------------------------
bool			
MOustersArmsBand::FindSlotToAddItem(MItem* pItem, int &slot) const
{
	if (!pItem->IsQuickItem())
	{
		return false;
	}

	//---------------------------------------------------------
	// 어디에 들어갈 수 있을까?
	//---------------------------------------------------------
	for (int i=0; i<m_Size; i++)
	{
		const MItem* pQuickItem = m_ItemSlot[i];

		//---------------------------------------------------------
		// 아무것도 없는 곳이면 그냥 넣으면 된다.
		//---------------------------------------------------------
		if (pQuickItem==NULL)
		{
			slot = i;
			return true;
		}
		//---------------------------------------------------------
		// 뭔가 있으면.. 그곳에 쌓일 수 있는지 알아본다.
		//---------------------------------------------------------
		if (pQuickItem->GetItemClass()==pItem->GetItemClass()
			&& pQuickItem->GetItemType()==pItem->GetItemType())
		{
			//----------------------------------------------------
			// 더한 개수가 max를 넘지 않아야 한다.
			//----------------------------------------------------
			int addTotal = pQuickItem->GetNumber() + pItem->GetNumber();

			if ( static_cast<TYPE_ITEM_NUMBER>(addTotal) <= pQuickItem->GetMaxNumber() )
			{
				slot = i;
				return true;
			}
		}		
	}

	// 들어갈 곳이 없다.
	return false;
}

//----------------------------------------------------------------------
// MPetItem
//----------------------------------------------------------------------
// The constructor and the durability countdown live here, in the library,
// so a test can drive them through an injected MonotonicClock source; the
// pet's names, which read the creature table, stay in MItemUse.cpp.
//----------------------------------------------------------------------
MPetItem::MPetItem()
	: m_UpdateTime(MonotonicClock::Now())
{
	m_PetKeepedDay = 0;
	m_PetExpRemain = 0;
	m_PetFoodType = 0;
	m_bCanGamble = false;
	m_bCutHead = false;
	m_bCanAttack = false;
}

MPetItem::Minutes
MPetItem::MinutesSinceUpdate() const
{
	// One read of the clock decides the value; a clock that reads before
	// the update point (it cannot, on a monotonic clock, but a test source
	// can) counts as no time elapsed rather than a negative gap.
	const MonotonicClock::TimePoint now = MonotonicClock::Now();
	if (now <= m_UpdateTime)
		return Minutes(0);
	return std::chrono::duration_cast<Minutes>(now - m_UpdateTime);
}

TYPE_ITEM_DURATION
MPetItem::GetRemainingDurability() const
{
	const TYPE_ITEM_DURATION durability = GetCurrentDurability();
	// The elapsed count is 64-bit (Minutes, not std::chrono::minutes); the
	// durability is 32. Compare in the wider type so an elapsed count past
	// the durability's range floors at zero instead of wrapping.
	const long long elapsed = MinutesSinceUpdate().count();
	if (elapsed >= static_cast<long long>(durability))
		return 0;
	return durability - static_cast<TYPE_ITEM_DURATION>(elapsed);
}

//----------------------------------------------------------------------
// MPetItem - UseInventory
//
// As MUsePotionItem's above: the body is the executable's, installed
// through the host, because this library now emits MPetItem's vtable.
//----------------------------------------------------------------------
#ifdef __TEST_SUB_INVENTORY__
void	MPetItem::UseInventory(TYPE_OBJECTID SubInventoryItemID)
#else
void	MPetItem::UseInventory()
#endif
{
	if (s_pHost != NULL && s_pHost->UsePetFromInventory != NULL)
		s_pHost->UsePetFromInventory( this );
}
