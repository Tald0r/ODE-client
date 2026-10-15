//-----------------------------------------------------------------------------
// ModifyStatusManager.cpp
//-----------------------------------------------------------------------------
// What a change of one of the player's status values does (task 4.235:
// gamemodel's since then). The player, the zone and the UI are reached
// through PlayerState, CreatureState, ZoneState and StatusPresentation.
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "MTypeDef.h"
#include "ModifyStatusManager.h"
#include "Packet/Types/ModifyDef.h"
#include "MSkillManager.h"
#include "ClientMode.h"
#include "../basic/CMessageArray.h"
#include "GameMessages.h"
#include "MMoneyManager.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"
#include "MStatus.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "DebugLog.h"
#include "UserInformation.h"
#include "ExperienceTable.h"
#include "MHelpDef.h"
#include "Alignment.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "StatusPresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "RankBonusHandlerHost.h"
//-----------------------------------------------------------------------------
// Global
//-----------------------------------------------------------------------------
ModifyStatusManager*		g_pModifyStatusManager = NULL;

//-----------------------------------------------------------------------------
// define Functions
//-----------------------------------------------------------------------------
// The player whose value changed (MPlayer::SetStatus runs these).
static MStatus& Player()	{ return *PlayerState::Status(); }

#define	EXTRACT_MODIFY_VALUE( pVoid )							\
																\
			MODIFY_VALUE* pValue	= (MODIFY_VALUE*)pVoid;		\
			[[maybe_unused]] DWORD	oldValue	= pValue->oldValue;	\
			[[maybe_unused]] DWORD	value		= pValue->newValue;	\


//----------------------------------------------------------------------
//
//							Global Functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Can Learn NewDomainSkill
//----------------------------------------------------------------------
bool
CanLearnDomainSkill(int domain)
{
	auto* pSkillDomain = g_pSkillManager->GetMutable(domain);
	if (pSkillDomain == nullptr) return false;
	MSkillDomain& skillDomain = *pSkillDomain;
	int domainLevel = skillDomain.GetDomainLevel();

	//--------------------------------------------------
	// Whether a new skill can be learned
	//--------------------------------------------------
	skillDomain.SetBegin();

	while (skillDomain.IsNotEnd())
	{
		ACTIONINFO					ai		= skillDomain.GetSkillID();
		MSkillDomain::SKILLSTATUS	status	= skillDomain.GetSkillStatus();

		int learnLevel = (*g_pSkillInfoTable)[ai].GetLearnLevel();

		if (learnLevel>=0
			&& status != MSkillDomain::SKILLSTATUS_LEARNED
			&& learnLevel <= domainLevel
			&& ai != SKILL_MAGIC_ELUSION
			&& ai != SKILL_ILLUSION_OF_AVENGE
			&& ai != SKILL_WILL_OF_LIFE
			&& ai != SKILL_POISON_MESH
			&& ai != SKILL_DELEO_EFFICIO
			&& ai != SKILL_REPUTO_FACTUM)
		{
			DEBUG_ADD_FORMAT("[CanLearnNewSkill] skill=%s. learnLev=%d. domainLev=%d", (*g_pSkillInfoTable)[ai].GetName(), learnLevel, domainLevel);

			// A new skill can be learned:
			// knowing that is enough.
			return true;
		}

		skillDomain.Next();
	}

	return false;
}

//----------------------------------------------------------------------
// Set Domain Level
//----------------------------------------------------------------------
// Can a new skill of this domain be learned?
//----------------------------------------------------------------------
void
SetDomainLevel( int domain, int domainLevel )
{
	auto* pSkillDomain = g_pSkillManager->GetMutable(domain);
	if (pSkillDomain == nullptr) return;
	MSkillDomain& skillDomain = *pSkillDomain;
	skillDomain.SetDomainLevel( domainLevel );

	if (g_Mode==MODE_GAME)
	{
		//--------------------------------------------------
		// Say which level the domain reached
		//--------------------------------------------------
		int num1 = domainLevel % 10;
		if (num1==2 || num1==4 || num1==5 || num1==9)
		{
			g_pGameMessage->AddSafeFormat(GetGameString(STRING_MESSAGE_NEW_DOMAIN_LEVEL_1), (*g_pGameStringTable)[SKILLDOMAIN_NAME[domain]].GetString(), domainLevel);
		}
		else
		{
			g_pGameMessage->AddSafeFormat(GetGameString(STRING_MESSAGE_NEW_DOMAIN_LEVEL_2), (*g_pGameStringTable)[SKILLDOMAIN_NAME[domain]].GetString(), domainLevel);
		}

		//--------------------------------------------------
		// Whether a new skill can be learned
		//--------------------------------------------------
		if (CanLearnDomainSkill(domain))
		{
			g_pGameMessage->Add((*g_pGameStringTable)[STRING_MESSAGE_NEW_SKILL_AVAILABLE].GetString());
		}
		// 2004, 5, 6 sobeit add start
		switch(domainLevel)
		{
		case 5:
			GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_LEARN_SKILL);
			break;
		case 30:
			GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_LEVEL_30);
			break;
		case 31:
			GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_LEVEL_31);
			break;
		case 32:
			GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_LEVEL_32);
			break;
		}
		// 2004, 5, 6 sobeit add end
	}
}


//-----------------------------------------------------------------------------
//
// constructor
//
//-----------------------------------------------------------------------------
ModifyStatusManager::ModifyStatusManager()
{
	MFunctionManager::Init( MODIFY_MAX );

	MFunctionManager::SetFunction(MODIFY_CURRENT_HP, Function_MODIFY_CURRENT_HP);
	MFunctionManager::SetFunction(MODIFY_CURRENT_MP, Function_MODIFY_CURRENT_MP);
	MFunctionManager::SetFunction(MODIFY_MAX_HP, Function_MODIFY_MAX_HP);
	MFunctionManager::SetFunction(MODIFY_MAX_MP, Function_MODIFY_MAX_MP);
	MFunctionManager::SetFunction(MODIFY_DURABILITY, Function_MODIFY_DURABILITY);
	MFunctionManager::SetFunction(MODIFY_STR_EXP_REMAIN, Function_MODIFY_STR_EXP_REMAIN);
	MFunctionManager::SetFunction(MODIFY_DEX_EXP_REMAIN, Function_MODIFY_DEX_EXP_REMAIN);
	MFunctionManager::SetFunction(MODIFY_INT_EXP_REMAIN, Function_MODIFY_INT_EXP_REMAIN);
	MFunctionManager::SetFunction(MODIFY_VAMP_EXP_REMAIN, Function_MODIFY_VAMP_EXP_REMAIN);
	MFunctionManager::SetFunction(MODIFY_BULLET, Function_MODIFY_BULLET);
	MFunctionManager::SetFunction(MODIFY_SILVER_DURABILITY, Function_MODIFY_SILVER_DURABILITY);
	MFunctionManager::SetFunction(MODIFY_VISION, Function_MODIFY_VISION);
	MFunctionManager::SetFunction(MODIFY_GOLD, Function_MODIFY_GOLD);
	MFunctionManager::SetFunction(MODIFY_CURRENT_STR, Function_MODIFY_CURRENT_STR);
	MFunctionManager::SetFunction(MODIFY_CURRENT_DEX, Function_MODIFY_CURRENT_DEX);
	MFunctionManager::SetFunction(MODIFY_CURRENT_INT, Function_MODIFY_CURRENT_INT);
	MFunctionManager::SetFunction(MODIFY_FAME, Function_MODIFY_FAME);
	MFunctionManager::SetFunction(MODIFY_TOHIT, Function_MODIFY_TOHIT);
	MFunctionManager::SetFunction(MODIFY_DEFENSE, Function_MODIFY_DEFENSE);
	MFunctionManager::SetFunction(MODIFY_PROTECTION, Function_MODIFY_PROTECTION);
	MFunctionManager::SetFunction(MODIFY_BONUS_POINT, Function_MODIFY_BONUS_POINT);
	MFunctionManager::SetFunction(MODIFY_ATTACK_SPEED, Function_MODIFY_ATTACK_SPEED);
	MFunctionManager::SetFunction(MODIFY_SWORD_DOMAIN_LEVEL, Function_MODIFY_SWORD_DOMAIN_LEVEL);
	MFunctionManager::SetFunction(MODIFY_BLADE_DOMAIN_LEVEL, Function_MODIFY_BLADE_DOMAIN_LEVEL);
	MFunctionManager::SetFunction(MODIFY_ENCHANT_DOMAIN_LEVEL, Function_MODIFY_ENCHANT_DOMAIN_LEVEL);
	MFunctionManager::SetFunction(MODIFY_HEAL_DOMAIN_LEVEL, Function_MODIFY_HEAL_DOMAIN_LEVEL);
	MFunctionManager::SetFunction(MODIFY_GUN_DOMAIN_LEVEL, Function_MODIFY_GUN_DOMAIN_LEVEL);
	MFunctionManager::SetFunction(MODIFY_LEVEL, Function_MODIFY_LEVEL);
	MFunctionManager::SetFunction(MODIFY_ALIGNMENT, Function_MODIFY_ALIGNMENT);
	MFunctionManager::SetFunction(MODIFY_BASIC_STR, Function_MODIFY_BASIC_STR);
	MFunctionManager::SetFunction(MODIFY_MAX_STR, Function_MODIFY_MAX_STR);
	MFunctionManager::SetFunction(MODIFY_BASIC_DEX, Function_MODIFY_BASIC_DEX);
	MFunctionManager::SetFunction(MODIFY_MAX_DEX, Function_MODIFY_MAX_DEX);
	MFunctionManager::SetFunction(MODIFY_BASIC_INT, Function_MODIFY_BASIC_INT);
	MFunctionManager::SetFunction(MODIFY_MAX_INT, Function_MODIFY_MAX_INT);
	MFunctionManager::SetFunction(MODIFY_SWORD_DOMAIN_EXP_REMAIN, Function_MODIFY_SWORD_DOMAIN_EXP_REMAIN);
	MFunctionManager::SetFunction(MODIFY_BLADE_DOMAIN_EXP_REMAIN, Function_MODIFY_BLADE_DOMAIN_EXP_REMAIN);
	MFunctionManager::SetFunction(MODIFY_HEAL_DOMAIN_EXP_REMAIN, Function_MODIFY_HEAL_DOMAIN_EXP_REMAIN);
	MFunctionManager::SetFunction(MODIFY_ENCHANT_DOMAIN_EXP_REMAIN, Function_MODIFY_ENCHANT_DOMAIN_EXP_REMAIN);
	MFunctionManager::SetFunction(MODIFY_GUN_DOMAIN_EXP_REMAIN, Function_MODIFY_GUN_DOMAIN_EXP_REMAIN);
	MFunctionManager::SetFunction(MODIFY_ETC_DOMAIN_EXP_REMAIN, Function_MODIFY_ETC_DOMAIN_EXP_REMAIN);
	MFunctionManager::SetFunction(MODIFY_MIN_DAMAGE, Function_MODIFY_MIN_DAMAGE);
	MFunctionManager::SetFunction(MODIFY_MAX_DAMAGE, Function_MODIFY_MAX_DAMAGE);
	MFunctionManager::SetFunction(MODIFY_SILVER_DAMAGE, Function_MODIFY_SILVER_DAMAGE);
	MFunctionManager::SetFunction(MODIFY_GUILDID, Function_MODIFY_GUILDID);
	MFunctionManager::SetFunction(MODIFY_RANK, Function_MODIFY_RANK);
	MFunctionManager::SetFunction(MODIFY_RANK_EXP_REMAIN, Function_MODIFY_RANK_EXP_REMAIN);
	MFunctionManager::SetFunction(MODIFY_OUSTERS_EXP_REMAIN, Function_MODIFY_OUSTERS_EXP_REMAIN);
	MFunctionManager::SetFunction(MODIFY_SKILL_BONUS_POINT, Function_MODIFY_SKILL_POINT);
	MFunctionManager::SetFunction(MODIFY_ELEMENTAL_FIRE, Function_MODIFY_ELEMENTAL_FIRE);
	MFunctionManager::SetFunction(MODIFY_ELEMENTAL_WATER, Function_MODIFY_ELEMENTAL_WATER);
	MFunctionManager::SetFunction(MODIFY_ELEMENTAL_EARTH, Function_MODIFY_ELEMENTAL_EARTH);
	MFunctionManager::SetFunction(MODIFY_ELEMENTAL_WIND, Function_MODIFY_ELEMENTAL_WIND);
	MFunctionManager::SetFunction(MODIFY_SKILL_LEVEL, Function_MODIFY_SKILL_LEVEL);
	MFunctionManager::SetFunction(MODIFY_SKILL_EXP, Function_MODIFY_SKILL_EXP);
	MFunctionManager::SetFunction(MODIFY_PET_HP, Function_MODIFY_PET_HP);
	MFunctionManager::SetFunction(MODIFY_PET_EXP_REMAIN, Function_MODIFY_PET_EXP_REMAIN);

	MFunctionManager::SetFunction(MODIFY_UNIONID, Function_MODIFY_UNIONID);
	MFunctionManager::SetFunction(MODIFY_UNIONGRADE, Function_MODIFY_UNIONGRADE);

	MFunctionManager::SetFunction(MODIFY_ADVANCEMENT_CLASS_LEVEL, Function_MODIFY_ADVANCEMENT_CLASS_LEVEL);
	MFunctionManager::SetFunction(MODIFY_ADVANCEMENT_CLASS_GOAL_EXP, Function_MODIFY_ADVANCEMENT_CLASS_GOAL_EXP);
}

ModifyStatusManager::~ModifyStatusManager()
{
}

//-----------------------------------------------------------------------------
// MODIFY_CURRENT_HP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_CURRENT_HP(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	pValue->newValue = value = min(Player().GetMAX_HP()-Player().GetSilverDamage(), value);

	StatusPresentation::SetHP( value, Player().GetMAX_HP() );
	StatusPresentation::SetCharInfoHP( value );

	// Check each skill's MP
	if ((PlayerState::GetRace()==RACE_VAMPIRE))
	{
		g_pSkillAvailable->CheckMP();
	}
}

//-----------------------------------------------------------------------------
// MODIFY_CURRENT_HP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_CURRENT_MP(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	//pValue->newValue = value = min(Player().GetMAX_MP(), value);
	pValue->newValue = value;

	DEBUG_ADD_FORMAT("[UI] Set MP  (%d)", value);
	StatusPresentation::SetMP( value, Player().GetMAX_MP() );
	StatusPresentation::SetCharInfoMP( value );

	// Check each skill's MP
	if (!(PlayerState::GetRace()==RACE_VAMPIRE))
	{
		g_pSkillAvailable->CheckMP();
	}
}

//-----------------------------------------------------------------------------
// MODIFY_MAX_HP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_MAX_HP(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetHP( Player().GetHP(), value );
	StatusPresentation::SetCharInfoHP_max( value );

	if (g_Mode==MODE_GAME)
	{
		int num1 = value % 10;
		if (num1==2 || num1==4 || num1==5 || num1==9)
		{
			g_pGameMessage->AddSafeFormat(GetGameString(STRING_STATUS_HP_MAX_1), value);
		}
		else
		{
			g_pGameMessage->AddSafeFormat(GetGameString(STRING_STATUS_HP_MAX_2), value);
		}
	}
}

//-----------------------------------------------------------------------------
// MODIFY_MAX_MP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_MAX_MP(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetMP( Player().GetMP(), value );
	StatusPresentation::SetCharInfoMP_max( value );

	// Check each skill's MP
	//(*g_pSkillAvailable).CheckMP();

	if (g_Mode==MODE_GAME)
	{
		int num1 = value % 10;
		int stringID;

		if (num1==2 || num1==4 || num1==5 || num1==9)
		{
			stringID = STRING_STATUS_MP_MAX_1;
			if(CreatureState::IsOusters(PlayerState::Creature()))
				stringID = STRING_STATUS_EP_MAX_1;
		}
		else
		{
			stringID = STRING_STATUS_MP_MAX_2;
			if(CreatureState::IsOusters(PlayerState::Creature()))
				stringID = STRING_STATUS_EP_MAX_2;
		}
		g_pGameMessage->AddSafeFormat(GetGameString(stringID), value);
	}
}

//-----------------------------------------------------------------------------
// MODIFY_DURABILITY
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_DURABILITY(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )


	// 2005, 2, 23, sobeit modify start - changed to 1 BYTE | 3 BYTES
	int slotID		= (value & 0xFF000000) >> 24;
	int	durability	= value & 0x00FFFFFF;	// the slot taken off
//	int slotID		= (value & 0xFFFF0000) >> 16;
//	int	durability	= value & 0x0000FFFF;	// the slot taken off
	// 2005, 2, 23, sobeit modify end

	#ifdef	OUTPUT_DEBUG
		DEBUG_ADD_FORMAT("[MODIFY_DURABILITY] slot=%d, value=%d", (int)slotID, (int)durability);

		bool bModified;
	#endif


	switch(PlayerState::GetRace())
	{
	case RACE_SLAYER:
	{
		//-------------------------------------------------
		// The weapon is in the right hand,
		// but the server sometimes says the left.
		//-------------------------------------------------
		const MItem* pItem = g_pSlayerGear->GetItem( (MSlayerGear::GEAR_SLAYER)slotID );

		if (pItem!=NULL)
		{
			// A two-handed weapon: always the right hand's slot
			if (pItem->GetGearSlot()==MItem::SLOT_TWOHAND)
			{
				slotID = (BYTE)MSlayerGear::GEAR_SLAYER_RIGHTHAND;
			}

			#ifdef	OUTPUT_DEBUG
				bModified =
			#endif

			//pItem->GetCurrentDurability();

			g_pSlayerGear->ModifyDurability(slotID, durability);
		}
	}
	break;

	case RACE_VAMPIRE:
	{
		//-------------------------------------------------
		// The weapon is in the right hand,
		// but the server sometimes says the left.
		//-------------------------------------------------
		const MItem* pItem = g_pVampireGear->GetItem( (MVampireGear::GEAR_VAMPIRE)slotID );

		if (pItem!=NULL)
		{
			// A two-handed weapon: always the right hand's slot
			if (pItem->GetGearSlot()==MItem::SLOT_VAMPIRE_TWOHAND)
			{
				slotID = (BYTE)MVampireGear::GEAR_VAMPIRE_RIGHTHAND;
			}

			#ifdef	OUTPUT_DEBUG
				bModified =
			#endif

			//pItem->GetCurrentDurability();

			g_pVampireGear->ModifyDurability(slotID, durability);
		}
	}
	break;

	case RACE_OUSTERS:
		{
			//-------------------------------------------------
			// The weapon is in the right hand,
			// but the server sometimes says the left.
			//-------------------------------------------------
			const MItem* pItem = g_pOustersGear->GetItem( (MOustersGear::GEAR_OUSTERS)slotID );

			if (pItem!=NULL)
			{
				// A two-handed weapon: always the right hand's slot
				if (pItem->GetGearSlot()==MItem::SLOT_OUSTERS_TWOHAND)
				{
					slotID = (BYTE)MOustersGear::GEAR_OUSTERS_RIGHTHAND;
				}

#ifdef	OUTPUT_DEBUG
				bModified =
#endif

					//pItem->GetCurrentDurability();

					g_pOustersGear->ModifyDurability(slotID, durability);
			}
		}
		break;

	default:
		break;
	}

	#ifdef	OUTPUT_DEBUG
		if (!bModified)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Can't Modify Durability. slot=%d, value=%d", (int)slotID, (int)durability);
		}
	#endif
}

//-----------------------------------------------------------------------------
// MODIFY_STR_EXP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_STR_EXP_REMAIN(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetSheet(StatusPresentation::SHEET_STR_EXP_REMAIN, value);
}

//-----------------------------------------------------------------------------
// MODIFY_DEX_EXP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_DEX_EXP_REMAIN(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetSheet(StatusPresentation::SHEET_DEX_EXP_REMAIN, value);
}

//-----------------------------------------------------------------------------
// MODIFY_INT_EXP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_INT_EXP_REMAIN(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetSheet(StatusPresentation::SHEET_INT_EXP_REMAIN, value);
}

//-----------------------------------------------------------------------------
// MODIFY_VAMP_EXP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_VAMP_EXP_REMAIN(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetSheet(StatusPresentation::SHEET_EXP_REMAIN, value);
}

//-----------------------------------------------------------------------------
// MODIFY_BULLET
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_BULLET(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	if (g_pCurrentMagazine!=NULL)
	{
		g_pCurrentMagazine->SetNumber( value );
	}
}

//-----------------------------------------------------------------------------
// MODIFY_SILVER_DURABILITY
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_SILVER_DURABILITY(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	if ((PlayerState::GetRace()==RACE_SLAYER))
	{
		const MItem* pWeapon = g_pSlayerGear->GetItem( MSlayerGear::GEAR_SLAYER_RIGHTHAND );

		if (pWeapon!=NULL)
		{
			MItem* pModifyWeapon = g_pSlayerGear->GetItemToModify( pWeapon->GetID() );

			if (pModifyWeapon!=NULL)
			{
				pModifyWeapon->SetSilver( value );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// MODIFY_VISION
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_VISION(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	//--------------------------------------------------------
	// Remove from the zone the creatures out of the player's sight
	// from (m_ServerX, m_ServerY).
	//--------------------------------------------------------
	PlayerState::SetSight( static_cast<char>(value) );
	ZoneState::KeepObjectInSight(CreatureState::GetServerX(PlayerState::Creature()),
								CreatureState::GetServerY(PlayerState::Creature()),
								PlayerState::GetSight());
}

//-----------------------------------------------------------------------------
// MODIFY_GOLD
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_GOLD(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	g_pMoneyManager->SetMoney( value );

}

//-----------------------------------------------------------------------------
// MODIFY_CURRENT_STR
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_CURRENT_STR(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	DEBUG_ADD_FORMAT("MODIFY_CURRENT_STR = %d", value);

	if (g_Mode==MODE_GAME)
	{
		int num1 = value % 10;
		if (num1==2 || num1==4 || num1==5 || num1==9)
		{
			g_pGameMessage->AddSafeFormat(GetGameString(STRING_STATUS_STR_1), value);
		}
		else
		{
			g_pGameMessage->AddSafeFormat(GetGameString(STRING_STATUS_STR_2), value);
		}
	}

	//C_VS_UI_SLAYER_PDS::m_str		= value;
	//C_VS_UI_SLAYER_PDS::m_str_max	= value;
	StatusPresentation::SetCharInfoSTR( value );

	//------------------------------------------
	// Recheck what the change affects
	//------------------------------------------
	MItemManager* pGear = PlayerState::GetGear();

	g_pInventory->CheckAffectStatusAll();
	pGear->CheckAffectStatusAll();

	//------------------------------------------
	// Recompute the derived values
	//------------------------------------------
	PlayerState::CalculateStatus();
}

//-----------------------------------------------------------------------------
// MODIFY_CURRENT_DEX
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_CURRENT_DEX(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	if (g_Mode==MODE_GAME)
	{
		int num1 = value % 10;
		if (num1==2 || num1==4 || num1==5 || num1==9)
		{
			g_pGameMessage->AddSafeFormat(GetGameString(STRING_STATUS_DEX_1), value);
		}
		else
		{
			g_pGameMessage->AddSafeFormat(GetGameString(STRING_STATUS_DEX_2), value);
		}
	}
	//C_VS_UI_SLAYER_PDS::m_dex		= value;
	//C_VS_UI_SLAYER_PDS::m_dex_max	= value;
	StatusPresentation::SetCharInfoDEX( value );

	//------------------------------------------
	// Recheck what the change affects
	//------------------------------------------
	MItemManager* pGear = PlayerState::GetGear();

	g_pInventory->CheckAffectStatusAll();
	pGear->CheckAffectStatusAll();

	//------------------------------------------
	// Recompute the derived values
	//------------------------------------------
	PlayerState::CalculateStatus();
}

//-----------------------------------------------------------------------------
// MODIFY_CURRENT_INT
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_CURRENT_INT(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	if (g_Mode==MODE_GAME)
	{
		int num1 = value % 10;
		if (num1==2 || num1==4 || num1==5 || num1==9)
		{
			g_pGameMessage->AddSafeFormat(GetGameString(STRING_STATUS_INT_1), value);
		}
		else
		{
			g_pGameMessage->AddSafeFormat(GetGameString(STRING_STATUS_INT_2), value);
		}
	}
	//C_VS_UI_SLAYER_PDS::m_int		= value;
	//C_VS_UI_SLAYER_PDS::m_int_max	= value;
	StatusPresentation::SetCharInfoINT( value );

	//------------------------------------------
	// Recheck what depends on INT
	//------------------------------------------
	switch(PlayerState::GetRace())
	{
		case RACE_SLAYER:
			//------------------------------------------
			// Recheck which items can be used.
			//------------------------------------------
			g_pInventory->CheckAffectStatusAll();
			g_pSlayerGear->CheckAffectStatusAll();
			break;

		case RACE_VAMPIRE:
			g_pInventory->CheckAffectStatusAll();
			g_pVampireGear->CheckAffectStatusAll();
			break;

		case RACE_OUSTERS:
			g_pInventory->CheckAffectStatusAll();
			g_pOustersGear->CheckAffectStatusAll();
			break;

		default:
			break;
	}

	//------------------------------------------
	// Recompute the derived values
	//------------------------------------------
	PlayerState::CalculateStatus();

	// A vampire's skill costs fall as its current INT rises
	// (MSkillInfoTable::GetVampireConsumeMP), so which skills it can use
	// changes with INT as well as with HP.
	if ((PlayerState::GetRace()==RACE_VAMPIRE))
	{
		g_pSkillAvailable->CheckMP();
	}
}

//-----------------------------------------------------------------------------
// MODIFY_FAME
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_FAME(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetCharInfoFAME( value );
}

//-----------------------------------------------------------------------------
// MODIFY_TOHIT
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_TOHIT(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetCharInfoTOHIT( value );
}

//-----------------------------------------------------------------------------
// MODIFY_DEFENSE
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_DEFENSE(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetCharInfoDefense( value );
}

//-----------------------------------------------------------------------------
// MODIFY_PROTECTION
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_PROTECTION(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetCharInfoProtection( value );
}

//-----------------------------------------------------------------------------
// MODIFY_BONUS_POINT
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_BONUS_POINT(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	if (value!=0)
	{
		StatusPresentation::LevelUp();
	}

	StatusPresentation::SetBonusPoint( value );

	// [help] bonus point
//	__BEGIN_HELP_EVENT
//		ExecuteHelpEvent( HE_STATUS_BONUS_POINT );
//	__END_HELP_EVENT
}

//-----------------------------------------------------------------------------
// MODIFY_ATTACK_SPEED
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_ATTACK_SPEED(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	CreatureState::SetWeaponSpeed( PlayerState::Creature(), value );
}

//-----------------------------------------------------------------------------
// MODIFY_SWORD_DOMAIN_LEVEL
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_SWORD_DOMAIN_LEVEL(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	SetDomainLevel( SKILLDOMAIN_SWORD, value );
}

//-----------------------------------------------------------------------------
// MODIFY_BLADE_DOMAIN_LEVEL
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_BLADE_DOMAIN_LEVEL(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	SetDomainLevel( SKILLDOMAIN_BLADE, value );
}

//-----------------------------------------------------------------------------
// MODIFY_ENCHANT_DOMAIN_LEVEL
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_ENCHANT_DOMAIN_LEVEL(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	SetDomainLevel( SKILLDOMAIN_ENCHANT, value );
}

//-----------------------------------------------------------------------------
// MODIFY_HEAL_DOMAIN_LEVEL
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_HEAL_DOMAIN_LEVEL(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	SetDomainLevel( SKILLDOMAIN_HEAL, value );
}


//-----------------------------------------------------------------------------
// MODIFY_GUN_DOMAIN_LEVEL
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_GUN_DOMAIN_LEVEL(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	SetDomainLevel( SKILLDOMAIN_GUN, value );
}

//-----------------------------------------------------------------------------
// MODIFY_LEVEL
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_LEVEL(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	if (auto* entry = g_pSkillManager->GetMutable(SKILLDOMAIN_VAMPIRE)) {
		entry->SetDomainLevel( value );
	}

	if (g_Mode==MODE_GAME)
	{
		g_pGameMessage->AddSafeFormat(GetGameString(STRING_STATUS_LEVEL), value);

		if (CanLearnDomainSkill(SKILLDOMAIN_VAMPIRE))
		{
			g_pGameMessage->Add((*g_pGameStringTable)[STRING_MESSAGE_NEW_SKILL_AVAILABLE].GetString());
		}
		// 2004, 5, 6 sobeit add start
		switch(value)
		{
		case 5:
			GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_LEARN_SKILL);
			break;
		case 30:
			GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_LEVEL_30);
			break;
		case 31:
			GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_LEVEL_31);
			break;
		case 32:
			GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_LEVEL_32);
			break;
		}
		// 2004, 5, 6 sobeit add end

		StatusPresentation::SetCharInfoLevel( value );
	}

	//------------------------------------------
	// Recheck which items can be used.
	//------------------------------------------
	MItemManager* pGear = PlayerState::GetGear();

	g_pInventory->CheckAffectStatusAll();
	pGear->CheckAffectStatusAll();

}


//-----------------------------------------------------------------------------
// MODIFY_ALIGNMENT
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_ALIGNMENT(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	const int numAlignment = 5;

	//#ifdef	OUTPUT_DEBUG
	//	if (g_pGameMessage!=NULL)
	//		g_pGameMessage->AddFormat("oldValue=%d. newAlignment=%d, modifyAlignment=%d", oldValue, value, -(~(short)value + 1));
	//#endif

	//--------------------------------------------------
	// Set it in the UI
	//--------------------------------------------------
	short uiAlignment = static_cast<short>(value);
	if (uiAlignment > 10000)
	{
		uiAlignment = - (short)(~uiAlignment + 1);
	}
	StatusPresentation::SetSheet(StatusPresentation::SHEET_ALIGNMENT_NUM, uiAlignment);

	//--------------------------------------------------
	// -10000 ~ 10000 to the steps 0~4.
	//--------------------------------------------------
	int alignment;
	int oldAlignment;

	if (value > 10000)
	{
		pValue->newValue = value = - (short)(~value + 1);
	}

	if (oldValue > 10000)
	{
		oldValue = - (short)(~oldValue + 1);
	}

	alignment = ConvertAlignment( value );
	oldAlignment = ConvertAlignment( oldValue );

	//--------------------------------------------------
	// The message that the alignment moved a little
	//--------------------------------------------------
	if (g_Mode==MODE_GAME)
	{
		if (value > oldValue)
		{
			g_pGameMessage->Add((*g_pGameStringTable)[STRING_CHANGE_TO_GOOD_ALIGNMENT].GetString());
		}
		else if (value < oldValue)
		{
			g_pGameMessage->Add((*g_pGameStringTable)[STRING_CHANGE_TO_BAD_ALIGNMENT].GetString());

			// 2004, 5, 6 sobeit add start
			GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_FAME);
			// 2004, 5, 6 sobeit add end
		}
	}


	//#ifdef	OUTPUT_DEBUG
	//	if (g_pGameMessage!=NULL)
	//		g_pGameMessage->AddFormat("newValue=%d", alignment);
	//#endif

	//--------------------------------------------------
	// When the alignment's step changed..
	//--------------------------------------------------
	if (oldAlignment != alignment)
	{
		const char* alignmentName[numAlignment] =
		{
			"Evil-",
			"Evil",
			"Neutral",
			"Good",
			"Good+"
		};

		int stringSelect[numAlignment] =
		{
			STRING_CHANGE_ALIGNMENT2,
			STRING_CHANGE_ALIGNMENT2,
			STRING_CHANGE_ALIGNMENT2,
			STRING_CHANGE_ALIGNMENT,
			STRING_CHANGE_ALIGNMENT
		};

		if (g_Mode==MODE_GAME)
		{
			char str[256];
			SafeFormat::Format(str, GetGameString(stringSelect[alignment]), alignmentName[alignment]);
			g_pGameMessage->Add(str);

			StatusPresentation::SetSheet(StatusPresentation::SHEET_ALIGNMENT, alignment);
		}
	}
}

//-----------------------------------------------------------------------------
// MODIFY_BASIC_STR
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_BASIC_STR(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetSheet(StatusPresentation::SHEET_STR_PURE, value);
}

//-----------------------------------------------------------------------------
// MODIFY_MAX_STR
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_MAX_STR(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	DEBUG_ADD_FORMAT("MODIFY_MAX_STR = %d", value);

	StatusPresentation::SetSheet(StatusPresentation::SHEET_STR_MAX, value);
}

//-----------------------------------------------------------------------------
// MODIFY_BASIC_DEX
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_BASIC_DEX(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetSheet(StatusPresentation::SHEET_DEX_PURE, value);

	RankBonusHandlers::CheckRegen();
}

//-----------------------------------------------------------------------------
// MODIFY_MAX_DEX
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_MAX_DEX(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetSheet(StatusPresentation::SHEET_DEX_MAX, value);
}

//-----------------------------------------------------------------------------
// MODIFY_BASIC_INT
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_BASIC_INT(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetSheet(StatusPresentation::SHEET_INT_PURE, value);
}

//-----------------------------------------------------------------------------
// MODIFY_MAX_INT
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_MAX_INT(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetSheet(StatusPresentation::SHEET_INT_MAX, value);
}

//-----------------------------------------------------------------------------
// MODIFY_SWORD_DOMAIN_EXP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_SWORD_DOMAIN_EXP_REMAIN(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	if (auto* entry = g_pSkillManager->GetMutable(SKILLDOMAIN_SWORD)) {
		entry->SetDomainExpRemain( value );
	}
}

//-----------------------------------------------------------------------------
// MODIFY_BLADE_DOMAIN_EXP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_BLADE_DOMAIN_EXP_REMAIN(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	if (auto* entry = g_pSkillManager->GetMutable(SKILLDOMAIN_BLADE)) {
		entry->SetDomainExpRemain( value );
	}
}

//-----------------------------------------------------------------------------
// MODIFY_HEAL_DOMAIN_EXP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_HEAL_DOMAIN_EXP_REMAIN(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	if (auto* entry = g_pSkillManager->GetMutable(SKILLDOMAIN_HEAL)) {
		entry->SetDomainExpRemain( value );
	}
}

//-----------------------------------------------------------------------------
// MODIFY_ENCHANT_DOMAIN_EXP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_ENCHANT_DOMAIN_EXP_REMAIN(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	if (auto* entry = g_pSkillManager->GetMutable(SKILLDOMAIN_ENCHANT)) {
		entry->SetDomainExpRemain( value );
	}
}

//-----------------------------------------------------------------------------
// MODIFY_GUN_DOMAIN_EXP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_GUN_DOMAIN_EXP_REMAIN(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	if (auto* entry = g_pSkillManager->GetMutable(SKILLDOMAIN_GUN)) {
		entry->SetDomainExpRemain( value );
	}
}

//-----------------------------------------------------------------------------
// MODIFY_ETC_DOMAIN_EXP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_ETC_DOMAIN_EXP_REMAIN(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	if (auto* entry = g_pSkillManager->GetMutable(SKILLDOMAIN_ETC)) {
		entry->SetDomainExpRemain( value );
	}
}

//-----------------------------------------------------------------------------
// MODIFY_MIN_DAMAGE
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_MIN_DAMAGE(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	// This should compute only the damage..
	// (for now the whole status, so as not to forget it)
	PlayerState::CalculateStatus();
}

//-----------------------------------------------------------------------------
// MODIFY_MAX_DAMAGE
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_MAX_DAMAGE(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	PlayerState::CalculateStatus();
}

//-----------------------------------------------------------------------------
// MODIFY_SILVER_DAMAGE
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_SILVER_DAMAGE(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetCharInfoSilverDamage( value );

	StatusPresentation::SetHP( Player().GetHP(), Player().GetMAX_HP() );
}

//-----------------------------------------------------------------------------
// MODIFY_GUILDID
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_GUILDID(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	PlayerState::SetGuildNumber(value);

	StatusPresentation::SetSheet(StatusPresentation::SHEET_GUILD_ID, value);
}

//-----------------------------------------------------------------------------
// MODIFY_RANK
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_RANK(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetSheet(StatusPresentation::SHEET_GRADE, value);

	if(value > oldValue)
		g_pGameMessage->Add((*g_pGameStringTable)[STRING_MESSAGE_UP_TO_GRADE].GetString());
}

//-----------------------------------------------------------------------------
// MODIFY_RANK_EXP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_RANK_EXP_REMAIN(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

//	int accum = 0;
//	accum = g_pExperienceTable->GetRankInfo(g_char_slot_ingame.GRADE, PlayerState::GetRace()).AccumExp;
//	g_char_slot_ingame.GRADE_EXP_REMAIN = accum-value;
	StatusPresentation::SetSheet(StatusPresentation::SHEET_GRADE_EXP_REMAIN, value);
}

//-----------------------------------------------------------------------------
// MODIFY_OUSTERS_EXP
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_OUSTERS_EXP_REMAIN(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetSheet(StatusPresentation::SHEET_EXP_REMAIN, value);
}


//-----------------------------------------------------------------------------
// MODIFY_BONUS_POINT
//-----------------------------------------------------------------------------
void
ModifyStatusManager::Function_MODIFY_SKILL_POINT(void* pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

//		if (value!=0)
//		{
//			StatusPresentation::LevelUp();
//		}

		StatusPresentation::SetSkillPoint( value );

		// [help] bonus point
//		__BEGIN_HELP_EVENT
//			ExecuteHelpEvent( HE_STATUS_BONUS_POINT );
//		__END_HELP_EVENT
}

void
ModifyStatusManager::Function_MODIFY_ELEMENTAL_FIRE(void *pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetElementalValue(ITEMTABLE_INFO::ELEMENTAL_TYPE_FIRE, value);
}

void
ModifyStatusManager::Function_MODIFY_ELEMENTAL_WATER(void *pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetElementalValue(ITEMTABLE_INFO::ELEMENTAL_TYPE_WATER, value);
}

void
ModifyStatusManager::Function_MODIFY_ELEMENTAL_EARTH(void *pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetElementalValue(ITEMTABLE_INFO::ELEMENTAL_TYPE_EARTH, value);
}

void
ModifyStatusManager::Function_MODIFY_ELEMENTAL_WIND(void *pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	StatusPresentation::SetElementalValue(ITEMTABLE_INFO::ELEMENTAL_TYPE_WIND, value);
}

void
ModifyStatusManager::Function_MODIFY_SKILL_LEVEL(void *pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	SkillType_t		SkillID = HIWORD( value );
	WORD			SkillLev = LOWORD( value );

	if( SkillID < MAX_ACTIONINFO && g_pSkillInfoTable != NULL)
	{
		if (auto* entry = g_pSkillInfoTable->GetMutable(SkillID)) {
			entry->SetExpLevel( SkillLev );
		}

		if (g_Mode==MODE_GAME)
		{
			int num1 = SkillLev % 10;
			if (num1==2 || num1==4 || num1==5 || num1==9)
			{
				g_pGameMessage->AddSafeFormat(GetGameString(STRING_MESSAGE_MODIFY_SKILL_LEVEL_2),
					(*g_pSkillInfoTable)[SkillID].GetHName(), SkillLev );
			}
			else
			{
				g_pGameMessage->AddSafeFormat(GetGameString(STRING_MESSAGE_MODIFY_SKILL_LEVEL_1),
					(*g_pSkillInfoTable)[SkillID].GetHName(), SkillLev);
			}
		}
	}
}

void
ModifyStatusManager::Function_MODIFY_SKILL_EXP(void *pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	SkillType_t		SkillID = HIWORD( value );
	int				SkillExp = LOWORD( value ) * 10;

	if( SkillID < MAX_ACTIONINFO && g_pSkillInfoTable != NULL)
		if (auto* entry = g_pSkillInfoTable->GetMutable(SkillID)) {
			entry->SetSkillExp( SkillExp );
		}
}

void
ModifyStatusManager::Function_MODIFY_PET_HP(void *pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )
	TYPE_OBJECTID id = PlayerState::GetID();
	if(id == OBJECTID_NULL)
		return;
	MCreature* pPet = ZoneState::GetFakeCreature(CreatureState::GetPetID(PlayerState::Creature()));
	if(pPet == NULL)
		return;
	MPetItem* pPetItem = CreatureState::GetPetItem(pPet);
	if(pPetItem == NULL)
		return;
	if(pPetItem != NULL)
	{
		pPetItem->SetCurrentDurability(value);

		// The pet dying
		if(value == 60)
		{
			std::string petName = MItem::PetName(pPetItem).c_str();
			char szTemp[512];
			SafeFormat::Format(szTemp, GetGameString(UI_STRING_MESSAGE_HOUR), 1);

			g_pSystemMessage->AddSafeFormat(GetGameString(STRING_MESSAGE_PET_DIE_WARNING), petName.c_str(), szTemp);
			g_pSystemMessage->Add((*g_pGameStringTable)[STRING_MESSAGE_PET_REQUEST_REFILL].GetString());
		}
		else if(value == 10)
		{
			std::string petName = MItem::PetName(pPetItem).c_str();
			char szTemp[512];
			SafeFormat::Format(szTemp, GetGameString(UI_STRING_MESSAGE_MINUTE), 10);

			g_pSystemMessage->AddSafeFormat(GetGameString(STRING_MESSAGE_PET_DIE_WARNING), petName.c_str(), szTemp);
			g_pSystemMessage->Add((*g_pGameStringTable)[STRING_MESSAGE_PET_REQUEST_REFILL].GetString());
		}
		else if(value == 0)
		{
			TYPE_OBJECTID petID = CreatureState::GetPetID(PlayerState::Creature());
			if(petID != OBJECTID_NULL)
			{
				CreatureState::SetDead(ZoneState::GetFakeCreature(petID));
			}
		}
	}
}

void
ModifyStatusManager::Function_MODIFY_PET_EXP_REMAIN(void *pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )
	TYPE_OBJECTID id = PlayerState::GetID();
	if(id == OBJECTID_NULL)
		return;
	MCreature* pPet = ZoneState::GetFakeCreature(CreatureState::GetPetID(PlayerState::Creature()));
	if(pPet == NULL)
		return;
	MPetItem* pPetItem = CreatureState::GetPetItem(pPet);
	if(pPetItem == NULL)
		return;
	if(pPetItem != NULL)
	{
		int accum = 0;
		accum = g_pExperienceTable->GetPetExp(pPetItem->GetNumber()).AccumExp;
		pPetItem->SetPetExpRemain(accum-value);
	}
}


void
ModifyStatusManager::Function_MODIFY_UNIONID(void *pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )

	if (PlayerState::Status() == NULL)
		return;
	CreatureState::SetUnionGuildID( PlayerState::Creature(), value );
	g_pUserInformation->dwUnionID = value;

}

void
ModifyStatusManager::Function_MODIFY_UNIONGRADE(void *pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )
	if (PlayerState::Status() == NULL)
		return;

	g_pUserInformation->bUnionGrade = static_cast<BYTE>(value);

}


void
ModifyStatusManager::Function_MODIFY_ADVANCEMENT_CLASS_LEVEL(void *pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )
	if (PlayerState::Status() == NULL)
		return;


	StatusPresentation::SetSheet(StatusPresentation::SHEET_ADVANCEMENT_LEVEL, value);
}

void
ModifyStatusManager::Function_MODIFY_ADVANCEMENT_CLASS_GOAL_EXP(void *pVoid)
{
	EXTRACT_MODIFY_VALUE( pVoid )
	if (PlayerState::Status() == NULL)
		return;

	StatusPresentation::SetSheet(StatusPresentation::SHEET_EXP_REMAIN, value);


}
