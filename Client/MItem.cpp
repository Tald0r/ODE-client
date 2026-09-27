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

#include <fstream>
#include <algorithm>

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
// Get Weight
//----------------------------------------------------------------------
TYPE_ITEM_PRICE
MItem::GetPrice() const
{
	TYPE_ITEM_PRICE price;
	if( GetGrade() > 0 &&  GetGrade() <= 10 && IsGearItem() )
		price = (*g_pItemTable)[GetItemClass()][m_ItemType].Price * ( 100 + (GetGrade()-4)*5 ) / 100;
	else
		price = (*g_pItemTable)[GetItemClass()][m_ItemType].Price;
	return price;
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
// MGearItem::Get MaxDurability
//----------------------------------------------------------------------
int	
MGearItem::GetMaxDurability() const	
{ 
	int maxDur = (*g_pItemTable)[GetItemClass()][m_ItemType].Value1; 

	maxDur += (GetGrade()-4)*1000;

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

	return max( min( 65000, maxDur ), 1000 );
}

int
MWeaponItem::GetMinDamage() const
{
	return max(1, (*g_pItemTable)[GetItemClass()][m_ItemType].Value3 + (GetGrade()-4) );
}	// 최소 공격력

int
MWeaponItem::GetMaxDamage() const
{
	return max(1, (*g_pItemTable)[GetItemClass()][m_ItemType].Value4 + (GetGrade()-4) );
}	// 최대 공격력		

int
MWeaponItem::GetToHit() const
{
	return (*g_pItemTable)[GetItemClass()][m_ItemType].ToHit;
}		// 최대 공격력		

int
MWeaponItem::GetCriticalHit() const
{
	return max( 0, (*g_pItemTable)[GetItemClass()][m_ItemType].CriticalHit + (GetGrade()-4)*2 );
}

int
MArmorItem::GetProtectionValue() const
{
	return max( 0, MGearItem::GetProtectionValue() + (GetGrade()-4) );
}

int
MArmorItem::GetDefenseValue() const
{
	return max( 0, MGearItem::GetDefenseValue() + (GetGrade()-4)*2 );
}
int 
MArmorItem2::GetProtectionValue() const
{
	return max( 0, MGearItem::GetProtectionValue() + ((GetGrade()-4)/2) );
}

int
MArmorItem2::GetDefenseValue() const
{
	return max( 0, MGearItem::GetDefenseValue() + (GetGrade()-4) );
}
int	
MArmorItem2::GetMaxDurability() const	
{ 
	int maxDur = (*g_pItemTable)[GetItemClass()][m_ItemType].Value1; 

	maxDur += (GetGrade()-4)*500;

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

	return max( min( 65000, maxDur ), 1000 );
}

int
MAccessoryItem::GetLucky() const
{
	return  (GetGrade()-4);
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
