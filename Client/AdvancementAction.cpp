//----------------------------------------------------------------------
// AdvancementAction.cpp
//----------------------------------------------------------------------
// An advanced (second class) creature draws from its own action sets,
// numbered from ADVANCEMENT_ACTION_START: the conversion of a plain action
// to the advanced one, by race, and each advanced action's frame count
// (task 4.239: MTopViewDraw.cpp's and Client.cpp's until then).
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "AdvancementAction.h"
#include "MCreatureWear.h"

BYTE g_AdvanceVampireActionMaxCount[ ACTION_ADVANCEMENT_MAX - ACTION_ADVANCEMENT_STOP ];
BYTE g_AdvanceSlayerActionMaxCount[ ACTION_ADVANCEMENT_SLAYER_MAX - ACTION_ADVANCEMENT_SLAYER_STOP_SWORD ];
BYTE g_AdvanceOustersActionMaxCount[ ACTION_ADVANCEMENT_OUSTERS_MAX - ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_STOP ];

static const int AdvancementOustersActionConvTable[ ACTION_MAX_SLAYER ] =
{
	ACTION_STAND,		// converted by its own rule (the chakram's)
	ACTION_MOVE,		// converted by its own rule (the chakram's)
	ACTION_ADVANCEMENT_OUSTERS_ATTACK_NORMAL,
	ACTION_ADVANCEMENT_OUSTERS_MAGIC,
	ACTION_ADVANCEMENT_OUSTERS_DAMAGED,
	ACTION_ADVANCEMENT_OUSTERS_DRAINED,
	ACTION_ADVANCEMENT_OUSTERS_DIE,
	ACTION_STAND,		// needs its own rule, which it lacks (standing)
	ACTION_MOVE,		// needs its own rule, which it lacks (walking)
	ACTION_ADVANCEMENT_OUSTERS_SKILL_NORMAL,
	ACTION_ADVANCEMENT_OUSTERS_MAGIC_ATTACK,
	ACTION_ADVANCEMENT_OUSTERS_ABSORB_SOUL,
	ACTION_ADVANCEMENT_OUSTERS_FAST_MOVE_STOP,		// marked upstream for its own rule; already advanced (hovering)
	ACTION_ADVANCEMENT_OUSTERS_FAST_MOVE,		// marked upstream for its own rule; already advanced (flying)
	ACTION_ADVANCEMENT_OUSTERS_ATTACK_SLOW,
	ACTION_ADVANCEMENT_OUSTERS_ATTACK_FAST,
	ACTION_ADVANCEMENT_OUSTERS_SKILL_SLOW,
	ACTION_ADVANCEMENT_OUSTERS_SKILL_FAST,
	-1,
};

static const int AdvancementSlayerActionConvTable[ ACTION_MAX_SLAYER ] =
{
	ACTION_STAND,	// converted by its own rule (the right hand's)
	ACTION_MOVE,	// converted by its own rule (the right hand's)
	-1,	// needs its own rule, which it lacks
	ACTION_ADVANCEMENT_SLAYER_MAGIC,
	ACTION_DAMAGED,	// converted by its own rule (the right hand's)
	ACTION_ADVANCEMENT_SLAYER_DRAINED,
	ACTION_ADVANCEMENT_SLAYER_DIE,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_NORMAL,	// SR
	ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_NORMAL,	// AR
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SWORD_NORMAL,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_BLADE_NORMAL,
	-1,												// throwing a bomb
	ACTION_ADVANCEMENT_SLAYER_BIKE_MOVE,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_NORMAL,	// SG
	ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_NORMAL,	// SMG
	ACTION_ADVANCEMENT_SLAYER_SKILL_SWORD_NORMAL,
	ACTION_ADVANCEMENT_SLAYER_SKILL_BLADE_NORMAL,
	-1,												// throwing holy water
	ACTION_ADVANCEMENT_SLAYER_BIKE_STOP,			// 18
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_SLOW,		//SR
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_FAST,		//SR
	ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_SLOW,		//AR
	ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_FAST,		//AR
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_SLOW,		//SG
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_FAST,		//SG
	ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_SLOW,		//SMG
	ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_FAST,		//SMG
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SWORD_SLOW,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SWORD_FAST,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_BLADE_SLOW,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_BLADE_FAST,
	ACTION_ADVANCEMENT_SLAYER_SKILL_SWORD_SLOW,
	ACTION_ADVANCEMENT_SLAYER_SKILL_SWORD_FAST,
	ACTION_ADVANCEMENT_SLAYER_SKILL_BLADE_SLOW,
	ACTION_ADVANCEMENT_SLAYER_SKILL_BLADE_FAST,
};

static const int AdvancementVampireActionConvTable[ ACTION_MAX_SLAYER ] =
{
	ACTION_ADVANCEMENT_STOP,
	ACTION_ADVANCEMENT_MOVE,
	ACTION_ADVANCEMENT_ATTACK_NORMAL,
	ACTION_ADVANCEMENT_MAGIC,
	ACTION_ADVANCEMENT_DAMAGED,
	ACTION_ADVANCEMENT_DRAINED,
	ACTION_ADVANCEMENT_DIE,
	ACTION_ADVANCEMENT_DRAIN,
	ACTION_ADVANCEMENT_ATTACK_SLOW,
	ACTION_ADVANCEMENT_ATTACK_FAST,
	ACTION_ADVANCEMENT_SKILL_SLOW,		// 10
	ACTION_ADVANCEMENT_SKILL_NORMAL,	// 11
	ACTION_ADVANCEMENT_SKILL_FAST,		// 12
	ACTION_ADVANCEMENT_MAGIC_ATTACK,
	ACTION_ADVANCEMENT_SPECIAL,
	ACTION_ADVANCEMENT_CREATE_WEAPON,
	ACTION_ADVANCEMENT_DESTROY_WEAPON,	// 16
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1, // 30
	-1,
	-1,
	-1,
	-1,
};

int GetAdvancementVampireActionFromVampireAction( int CurAction, const MCreature* pCreature )
{
	(void)pCreature;
	if( CurAction >= ADVANCEMENT_ACTION_START )
		return CurAction;

	return AdvancementVampireActionConvTable[ CurAction ];
}

static int	GetAdvancementSlayerStandActionFromItemClass( ITEM_CLASS itemclass )
{
	switch( itemclass )
	{
	case ITEM_CLASS_SWORD :
		return ACTION_ADVANCEMENT_SLAYER_STOP_SWORD;

	case ITEM_CLASS_BLADE :
		return ACTION_ADVANCEMENT_SLAYER_STOP_BLADE;

	case ITEM_CLASS_SG :
	case ITEM_CLASS_SMG :
	case ITEM_CLASS_AR :
	case ITEM_CLASS_SR :
		return ACTION_ADVANCEMENT_SLAYER_STOP_GUN;

	default:
		break;
	}

	return ACTION_ADVANCEMENT_SLAYER_STOP_MACE_AND_CROSS;
}

static int GetAdvancementSlayerMoveActionFromItemClass( ITEM_CLASS itemclass )
{
	switch( itemclass )
	{
	case ITEM_CLASS_SWORD :
		return ACTION_ADVANCEMENT_SLAYER_MOVE_SWORD;

	case ITEM_CLASS_BLADE :
		return ACTION_ADVANCEMENT_SLAYER_MOVE_BLADE;

	case ITEM_CLASS_SG :
	case ITEM_CLASS_SMG :
	case ITEM_CLASS_AR :
	case ITEM_CLASS_SR :
		return ACTION_ADVANCEMENT_SLAYER_MOVE_GUN;

	default:
		break;
	}

	return ACTION_ADVANCEMENT_SLAYER_MOVE_MACE_AND_CROSS;
}

static int GetAdvancementSlayerDamagedActionFromItemClass( ITEM_CLASS itemclass )
{
	switch( itemclass )
	{
	case ITEM_CLASS_SWORD :
		return ACTION_ADVANCEMENT_SLAYER_DAMAGED_SWORD;

	case ITEM_CLASS_BLADE :
		return ACTION_ADVANCEMENT_SLAYER_DAMAGED_BLADE;

	case ITEM_CLASS_SG :
	case ITEM_CLASS_SMG :
	case ITEM_CLASS_AR :
	case ITEM_CLASS_SR :
		return ACTION_ADVANCEMENT_SLAYER_DAMAGED_GUN;

	default:
		break;
	}

	return ACTION_ADVANCEMENT_SLAYER_DAMAGED_CROSS_MACE;
}

int ConvAdvancementSlayerAction( int CurAction, ITEM_CLASS rightHand )
{
	if( ADVANCEMENT_ACTION_START <= CurAction )
		return CurAction;

	switch( CurAction )
	{
	case ACTION_DAMAGED :
		return GetAdvancementSlayerDamagedActionFromItemClass( rightHand );

	case ACTION_STAND :
		return GetAdvancementSlayerStandActionFromItemClass( rightHand );

	case ACTION_MOVE :
		return GetAdvancementSlayerMoveActionFromItemClass( rightHand );
	}

	return AdvancementSlayerActionConvTable[ CurAction ];
}

int ConvAdvancementOustersActionFromOusterAction( int CurAction, bool bChakram )
{
	if( ADVANCEMENT_ACTION_START <= CurAction )
		return CurAction;

	switch( CurAction )
	{
	case ACTION_STAND :
		if( bChakram )
			return ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_STOP;
		else
			return ACTION_ADVANCEMENT_OUSTERS_WRISTLET_STOP;

	case ACTION_MOVE :
		if( bChakram )
			return ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_MOVE;
		else
			return ACTION_ADVANCEMENT_OUSTERS_WRISTLET_MOVE;
	}

	if( CurAction > ACTION_OUSTERS_CHAKRAM_FAST )
		return -1;

	return AdvancementOustersActionConvTable[ CurAction ];
}

//----------------------------------------------------------------------
// The parts and actions the creature drawing picks (task 4.248:
// MTopViewDraw.cpp's until then).
//----------------------------------------------------------------------
//add by viva
int GetNewVampireActionFromVampireAction( int CurAction )
{
	int Action=CurAction;
	if(CurAction==13)
		Action=16;
	else if(CurAction==12)
		Action=7;
	return Action;
}
int GetAdvancementPartFromItemClass( ITEM_CLASS itemClass , TYPE_FRAMEID frameID)
{
//	assert(false && "님하 즐드333");

	switch( itemClass )
	{
	case ITEM_CLASS_MOTORCYCLE :
		{
			if(frameID == AC_BIKE_2)
				return AC_BIKE_2;
			return AC_BIKE_1;
		}

	case ITEM_CLASS_SWORD :
		return AC_SWORD;

	case ITEM_CLASS_BLADE :
		return AC_BLADE;

	case ITEM_CLASS_AR :
		return AC_GUN_AR;

	case ITEM_CLASS_SG :
	case ITEM_CLASS_SMG :
	case ITEM_CLASS_SR :
		return AC_GUN_SR;

	case ITEM_CLASS_MACE :
		return AC_MACE;

	case ITEM_CLASS_CROSS :
		return AC_CROSS;

	case ITEM_CLASS_SHIELD :
		return AC_SHIELD;

	case ITEM_CLASS_HELM :
		return AC_HELMET;

	case ITEM_CLASS_COAT :
	case ITEM_CLASS_TROUSER :
		return AC_BODY;

	case ITEM_CLASS_SHOULDER_ARMOR :
		return AC_SHOULDER;

	default:
		break;
	}

	return -1;
}

int GetAdvancementSlayerAttackActionFromItemClass( ITEM_CLASS itemClass, MCreature* pCreature )
{
	switch( itemClass )
	{
	case ITEM_CLASS_AR :
		return static_cast<int>(ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_SLOW) + static_cast<int>(pCreature->GetWeaponSpeed());

	case ITEM_CLASS_SG :
	case ITEM_CLASS_SMG :
	case ITEM_CLASS_SR :
		return static_cast<int>(ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_SLOW) + static_cast<int>(pCreature->GetWeaponSpeed());

	default:
		break;
	}

	return -1;
}
//add by viva
int ConvNewSlayerActionFromSlayerAction(int CurAction, MCreatureWear* pCreatureWear)
{
	assert(pCreatureWear!=NULL);
	int Action=CurAction;
	if(CurAction==ACTION_STAND)
	{
		const MCreatureWear::ADDON_INFO& addon = pCreatureWear->GetAddonInfo( ADDON_RIGHTHAND );
		if(	addon.ItemClass==ITEM_CLASS_SG
		||addon.ItemClass==ITEM_CLASS_SMG
		||addon.ItemClass==ITEM_CLASS_AR
		||addon.ItemClass==ITEM_CLASS_SR
		)
			return ACTION_NEW_SLAYER_STOP_GUN;
	}
	else if(CurAction==ACTION_MOVE)
	{
		const MCreatureWear::ADDON_INFO& addon = pCreatureWear->GetAddonInfo( ADDON_RIGHTHAND );
		if(	addon.ItemClass==ITEM_CLASS_SG
		||addon.ItemClass==ITEM_CLASS_SMG
		||addon.ItemClass==ITEM_CLASS_AR
		||addon.ItemClass==ITEM_CLASS_SR
		)
			return ACTION_NEW_SLAYER_MOVE_GUN;
	}
	else if(CurAction==ACTION_SLAYER_GUN_SG)	//9
		Action=ACTION_NEW_SLAYER_GUN_SG;
	else if(CurAction==ACTION_SLAYER_GUN_SMG)	//10
		Action=ACTION_NEW_SLAYER_GUN_SMG;
	else if(CurAction==ACTION_SLAYER_SWORD)	//11
		Action=ACTION_NEW_SLAYER_SWORD;
	else if(CurAction==ACTION_SLAYER_BLADE)	//12
		Action=ACTION_NEW_SLAYER_BLADE;
	else if(CurAction==ACTION_SLAYER_SWORD_2)	//13
		Action=ACTION_NEW_SLAYER_SWORD_2;
	else if(CurAction==ACTION_SLAYER_BLADE_2)	//14
		Action=ACTION_NEW_SLAYER_BLADE_2;
	else if(CurAction==ACTION_SLAYER_MOTOR_MOVE)	//15
		Action=ACTION_NEW_SLAYER_MOTOR_MOVE;
	else if(CurAction==ACTION_SLAYER_MOTOR_STAND)	//16
		Action=ACTION_NEW_SLAYER_MOTOR_STAND;
	else if(CurAction==ACTION_SLAYER_GUN_SR_SLOW)	//17
		Action=ACTION_NEW_SLAYER_GUN_SR_SLOW;
	else if(CurAction==ACTION_SLAYER_GUN_SR_FAST)	//18
		Action=ACTION_NEW_SLAYER_GUN_SR_FAST;
	else if(CurAction==ACTION_SLAYER_GUN_AR_SLOW)	//19
		Action=ACTION_NEW_SLAYER_GUN_AR_SLOW;
	else if(CurAction==ACTION_SLAYER_GUN_AR_FAST)	//20
		Action=ACTION_NEW_SLAYER_GUN_AR_FAST;
	else if(CurAction==ACTION_SLAYER_GUN_SG_SLOW)	//21
		Action=ACTION_NEW_SLAYER_GUN_SG_SLOW;
	else if(CurAction==ACTION_SLAYER_GUN_SG_FAST)	//22
		Action=ACTION_NEW_SLAYER_GUN_SG_FAST;
	else if(CurAction==ACTION_SLAYER_GUN_SMG_SLOW)	//23
		Action=ACTION_NEW_SLAYER_GUN_SMG_SLOW;
	else if(CurAction==ACTION_SLAYER_GUN_SMG_FAST)	//24
		Action=ACTION_NEW_SLAYER_GUN_SMG_FAST;
	else if(CurAction==ACTION_SLAYER_SWORD_SLOW)	//25
		Action=ACTION_NEW_SLAYER_SWORD_SLOW;
	else if(CurAction==ACTION_SLAYER_SWORD_FAST)	//26
		Action=ACTION_NEW_SLAYER_SWORD_FAST;
	else if(CurAction==ACTION_SLAYER_BLADE_SLOW)	//27
		Action=ACTION_NEW_SLAYER_BLADE_SLOW;
	else if(CurAction==ACTION_SLAYER_BLADE_FAST)	//28
		Action=ACTION_NEW_SLAYER_BLADE_FAST;
	else if(CurAction==ACTION_SLAYER_SWORD_2_SLOW)	//29
		Action=ACTION_NEW_SLAYER_SWORD_2_SLOW;
	else if(CurAction==ACTION_SLAYER_SWORD_2_FAST)	//30
		Action=ACTION_NEW_SLAYER_SWORD_2_FAST;
	else if(CurAction==ACTION_SLAYER_BLADE_2_SLOW)	//31
		Action=ACTION_NEW_SLAYER_BLADE_2_SLOW;
	else if(CurAction==ACTION_SLAYER_BLADE_2_FAST)	//32
		Action=ACTION_NEW_SLAYER_BLADE_2_FAST;
	else if(CurAction==130)	// (garbled upstream comment)
		Action=37;
	return Action;
}
// The slayer's conversion by the right hand's item class.
int ConvAdvancementSlayerActionFromSlayerAction( int CurAction, MCreatureWear* pCreatureWear )
{
	assert( pCreatureWear != NULL );

	return ConvAdvancementSlayerAction( CurAction, pCreatureWear->GetAddonInfo( ADDON_RIGHTHAND ).ItemClass );
}
