//----------------------------------------------------------------------
// MCreature.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MObject.h"
#include "MCreature.h"
#include "WorldTileGeometry.h"
#include "MActionInfoTable.h"
#include "MEffectGeneratorTable.h"
#include "MCreatureTable.h"
#include "MSoundTable.h"
#include "MEffectStatusTable.h"
#include "ClientConfig.h"
#include "TextUtf8.h"
#include "TextWrap.h"
#include "SkillDef.h"
#include "MLevelNameTable.h"
#include "EffectSpriteTypeDef.h"
#include "SoundDef.h"
#include "MParty.h"
#include "MonsterNameTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MAttachOrbitEffect.h"
#include "UserInformation.h"
#include "MSkillManager.h"	// [new skill3]
#include "MEventManager.h"
#include "RankBonusTable.h"
#include "DebugLog.h"
#include "MItem.h"
#include "MSector.h"
#include "MEffect.h"
#include "MViewDef.h"
#include "Alignment.h"
#include "AdvancementAction.h"
#include "DirectionSelection.h"
#include "FrameClock.h"
#include "ZoneFlags.h"
#include "GameMessages.h"
#include "../basic/ColorSetDef.h"
#include "../basic/TextEncoding.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "StatusPresentationHost.h"
#include "DialogPresentationHost.h"
#include "VsUiPresentationHost.h"
#include "GameOptionsHost.h"
#include "RankBonusDef.h"
#include "MTimeItemManager.h"

#include "domain/Formulas.h"
#include "SystemAvailabilities.h"
#include "MGameStringTable.h"

#define ATTACHEFFECTCOLOR_NULL	0xFFFF

// White, as the UI's RGB_WHITE (VS_UI_Base.h).
#define RGB_WHITE	RGB(255, 255, 255)

// The creature allocator's pool (task 4.242: MemoryPool.cpp's until then,
// which keeps the pools of the executable's creature classes).
MemoryPool g_CreatureMemoryPool( sizeof( MCreature ), 30 );

namespace {
// The player's cell (the regen tower check, the move logs); (0, 0)
// without a player.
int PlayerX() { MCreature* pPlayer = PlayerState::Creature(); return pPlayer != NULL ? pPlayer->GetX() : 0; }
int PlayerY() { MCreature* pPlayer = PlayerState::Creature(); return pPlayer != NULL ? pPlayer->GetY() : 0; }
// The view's cell-to-pixel conversion (MTopView's statics).
POINT MapToPixel(int x, int y) { POINT p; p.x = WorldTileGeometry::TileToPixelX(x); p.y = WorldTileGeometry::TileToPixelY(y); return p; }
}

//#define	new			DEBUG_NEW
#define				MAX_DARKNESS_COUNT		13

#ifdef __METROTECH_TEST__
	extern bool g_bLight;
#endif

//----------------------------------------------------------------------
// Get ArmageddonSprite
//----------------------------------------------------------------------
// This should also be put in as a member function.. but for now, because of compile time -_-;
//----------------------------------------------------------------------
//TYPE_EFFECTSPRITETYPE
//GetArmageddonSprite(MCreature* pCreature)
//{
//	DWORD aHP = pCreature->GetArmageddon();
//
//	if (aHP > 300)
//	{
//		return EFFECTSPRITETYPE_ARMAGEDDON_BACK_0_2;
//	}
//	else if (aHP > 200)
//	{
//		return EFFECTSPRITETYPE_ARMAGEDDON_BACK_1_2;
//	}
//	else if (aHP > 100)
//	{
//		return EFFECTSPRITETYPE_ARMAGEDDON_BACK_2_2;
//	}
//	
//	return EFFECTSPRITETYPE_ARMAGEDDON_BACK_3_2;	
//}
//
//int
//IsChangedArmageddonSprite(DWORD previousHP, DWORD currentHP)
//{
//	if (previousHP > 300)
//	{
//		if (currentHP <= 300)
//		{
//			return 1;
//		}
//
//		return 0;
//	}
//	else if (previousHP > 200)
//	{
//		if (currentHP <= 200)
//		{
//			return 2;
//		}
//
//		return 0;
//	}
//	else if (previousHP > 100)
//	{
//		if (currentHP <= 100)
//		{
//			return 3;
//		}
//
//		return 0;
//	}
//	
//	if (currentHP == 0)
//	{
//		return 4;
//	}
//	
//	return 0;
//}

//----------------------------------------------------------------------
// define
//----------------------------------------------------------------------
// Should just make it a function.. but temporarily, since I don't want to change the header and recompile - -;
//DEBUG_ADD_FORMAT("[ Apply Buffering Move ] [ID=%d] Current(%d, %d) Dir(%d) --> Next(%d, %d) Dir(%d)",
//															m_ID,
//															m_X, m_Y, m_Direction,
//															m_NextX,m_NextY, m_NextDirection);
		
#define MoveNextPosition()					\
											\
		MovePosition( m_NextX, m_NextY );	\
		if (m_NextDirection != DIRECTION_NULL)	\
		{									\
			int dir = m_Direction;			\
											\
			SetAction( m_MoveAction );		\
			m_Direction = m_NextDirection;	\
			m_DirectionMove = m_NextDirection;	\
			ActionMove();					\
			m_Direction = dir;				\
		}									\
											\
		m_NextX = SECTORPOSITION_NULL;		\
		m_NextY = SECTORPOSITION_NULL;		\
		m_NextDirection = DIRECTION_NULL;

#define	ActionMoveNextPosition()					\
		if (!m_bFastMove)							\
		{											\
			if (m_NextX != SECTORPOSITION_NULL)		\
			{										\
				MoveNextPosition();					\
			}										\
			else if (m_NextAction==m_MoveAction)	\
			{										\
				SetAction( m_NextAction );			\
				ActionMove();						\
			}										\
		}

#define SetServerPositionWithBlock(sX, sY)							\
		{															\
			CLASS_TYPE classType = GetClassType();					\
																	\
			if (classType!=CLASS_PLAYER && classType!=CLASS_FAKE	\
				&& (m_ServerX!=sX || m_ServerY!=sY))				\
			{														\
				ZoneState::UnSetServerBlockIn(m_pZone, m_MoveType, m_ServerX, m_ServerY);	\
																	\
				m_ServerX = static_cast<TYPE_SECTORPOSITION>(sX);	\
				m_ServerY = static_cast<TYPE_SECTORPOSITION>(sY);	\
																	\
				if (m_bAlive)										\
				{													\
					ZoneState::SetServerBlockIn(m_pZone, m_MoveType, static_cast<TYPE_SECTORPOSITION>(sX), static_cast<TYPE_SECTORPOSITION>(sY));	\
				}													\
			}														\
		}

//----------------------------------------------------------------------
// info about the pixels moved at one time
// Adding it ACTIONCOUNT_MOVE times in the direction of sXTable makes the sXTable value 0.
// For each of the 8 directions, as many as the Move count..
//----------------------------------------------------------------------
/*
int MCreature::m_cXTableFrame8[8][8] = 
{
	{ -6, -6, -6, -6, -6, -6, -6, -6 },		// LEFT
	{ -6, -6, -6, -6, -6, -6, -6, -6 },		// LEFTDOWN
	{  0,  0,  0,  0,  0,  0,  0,  0 },		// DOWN
	{  6,  6,  6,  6,  6,  6,  6,  6 },		// RIGHTDOWN
	{  6,  6,  6,  6,  6,  6,  6,  6 },		// RIGHT
	{  6,  6,  6,  6,  6,  6,  6,  6 },		// RIGHTUP
	{  0,  0,  0,  0,  0,  0,  0,  0 },		// UP
	{ -6, -6, -6, -6, -6, -6, -6, -6 }		// LEFTUP
};

int MCreature::m_cXTableFrame8[8][8] = 
{
	{  0,  0,  0,  0,  0,  0,  0,  0 },		// LEFT
	{  3,  3,  3,  3,  3,  3,  3,  3 },		// LEFTDOWN
	{  3,  3,  3,  3,  3,  3,  3,  3 },		// DOWN
	{  3,  3,  3,  3,  3,  3,  3,  3 },		// RIGHTDOWN
	{  0,  0,  0,  0,  0,  0,  0,  0 },		// RIGHT
	{ -3, -3, -3, -3, -3, -3, -3, -3 },		// RIGHTUP
	{ -3, -3, -3, -3, -3, -3, -3, -3 },		// UP
	{ -3, -3, -3, -3, -3, -3, -3, -3 }		// LEFTUP
};

int MCreature::m_cXTableFrame4[8][4] = 
{
	{ -12, -12, -12, -12 },		// LEFT
	{ -12, -12, -12, -12 },		// LEFTDOWN
	{   0,   0,   0,   0 },		// DOWN
	{  12,  12,  12,  12 },		// RIGHTDOWN
	{  12,  12,  12,  12 },		// RIGHT
	{  12,  12,  12,  12 },		// RIGHTUP
	{   0,   0,   0,   0 },		// UP
	{ -12, -12, -12, -12 }		// LEFTUP
};

int MCreature::m_cXTableFrame4[8][4] = 
{
	{  0,  0,  0,  0 },		// LEFT
	{  6,  6,  6,  6 },		// LEFTDOWN
	{  6,  6,  6,  6 },		// DOWN
	{  6,  6,  6,  6 },		// RIGHTDOWN
	{  0,  0,  0,  0 },		// RIGHT
	{ -6, -6, -6, -6 },		// RIGHTUP
	{ -6, -6, -6, -6 },		// UP
	{ -6, -6, -6, -6 }		// LEFTUP
};
*/

//------------------------------------------------------------------
// Slayer action speed
//------------------------------------------------------------------
int MCreature::s_SlayerActionSpeed[ACTION_MAX_SLAYER][3] =
{
	{ ACTION_STAND, ACTION_STAND, ACTION_STAND },
	{ ACTION_MOVE, ACTION_MOVE, ACTION_MOVE },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_MAGIC, ACTION_MAGIC, ACTION_MAGIC },
	{ ACTION_DAMAGED, ACTION_DAMAGED, ACTION_DAMAGED },
	{ ACTION_DRAINED, ACTION_DRAINED, ACTION_DRAINED },
	{ ACTION_DIE, ACTION_DIE, ACTION_DIE },

	{ ACTION_SLAYER_GUN_SR_SLOW, ACTION_SLAYER_GUN_SR, ACTION_SLAYER_GUN_SR_FAST },
	{ ACTION_SLAYER_GUN_AR_SLOW, ACTION_SLAYER_GUN_AR, ACTION_SLAYER_GUN_AR_FAST },
	{ ACTION_SLAYER_SWORD_SLOW, ACTION_SLAYER_SWORD, ACTION_SLAYER_SWORD_FAST },
	{ ACTION_SLAYER_BLADE_SLOW, ACTION_SLAYER_BLADE, ACTION_SLAYER_BLADE_FAST },
	{ ACTION_SLAYER_THROW_WEAPON, ACTION_SLAYER_THROW_WEAPON, ACTION_SLAYER_THROW_WEAPON },
	{ ACTION_SLAYER_MOTOR_MOVE, ACTION_SLAYER_MOTOR_MOVE, ACTION_SLAYER_MOTOR_MOVE },
	{ ACTION_SLAYER_GUN_SG_SLOW, ACTION_SLAYER_GUN_SG, ACTION_SLAYER_GUN_SG_FAST },
	{ ACTION_SLAYER_GUN_SMG_SLOW, ACTION_SLAYER_GUN_SMG, ACTION_SLAYER_GUN_SMG_FAST },
	{ ACTION_SLAYER_SWORD_2_SLOW, ACTION_SLAYER_SWORD_2, ACTION_SLAYER_SWORD_2_FAST },
	{ ACTION_SLAYER_BLADE_2_SLOW, ACTION_SLAYER_BLADE_2, ACTION_SLAYER_BLADE_2_FAST },
	{ ACTION_SLAYER_THROW_POTION, ACTION_SLAYER_THROW_POTION, ACTION_SLAYER_THROW_POTION },
	{ ACTION_SLAYER_MOTOR_STAND, ACTION_SLAYER_MOTOR_STAND, ACTION_SLAYER_MOTOR_STAND },
};

//------------------------------------------------------------------
// vampire action speed
//------------------------------------------------------------------
int MCreature::s_VampireActionSpeed[ACTION_MAX_SLAYER][3] = //ACTION_MAX_VAMPIRE][3] =
{
	{ ACTION_STAND, ACTION_STAND, ACTION_STAND },
	{ ACTION_MOVE, ACTION_MOVE, ACTION_MOVE },
	{ ACTION_VAMPIRE_ATTACK_SLOW, ACTION_ATTACK, ACTION_VAMPIRE_ATTACK_FAST },
	{ ACTION_MAGIC, ACTION_MAGIC, ACTION_MAGIC },
	{ ACTION_DAMAGED, ACTION_DAMAGED, ACTION_DAMAGED },
	{ ACTION_DRAINED, ACTION_DRAINED, ACTION_DRAINED },
	{ ACTION_DIE, ACTION_DIE, ACTION_DIE },

	{ ACTION_VAMPIRE_DRAIN, ACTION_VAMPIRE_DRAIN, ACTION_VAMPIRE_DRAIN },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },	
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK }
};

//------------------------------------------------------------------
// ousters action speed
//------------------------------------------------------------------
int MCreature::s_OustersActionSpeed[ACTION_MAX_SLAYER][3] = //ACTION_MAX_OUSTERS][3] =
{
	{ ACTION_STAND, ACTION_STAND, ACTION_STAND },
	{ ACTION_MOVE, ACTION_MOVE, ACTION_MOVE },
	{ ACTION_OUSTERS_ATTACK_SLOW, ACTION_ATTACK, ACTION_OUSTERS_ATTACK_FAST },
	{ ACTION_MAGIC, ACTION_MAGIC, ACTION_MAGIC },
	{ ACTION_DAMAGED, ACTION_DAMAGED, ACTION_DAMAGED },
	{ ACTION_DRAINED, ACTION_DRAINED, ACTION_DRAINED },
	{ ACTION_DIE, ACTION_DIE, ACTION_DIE },
	
	{ ACTION_OUSTERS_STAND, ACTION_OUSTERS_STAND, ACTION_OUSTERS_STAND },
	{ ACTION_OUSTERS_MOVE, ACTION_OUSTERS_MOVE, ACTION_OUSTERS_MOVE },	
	{ ACTION_OUSTERS_CHAKRAM_SLOW, ACTION_OUSTERS_CHAKRAM, ACTION_OUSTERS_CHAKRAM_FAST },
	{ ACTION_OUSTERS_MAGIC_ATTACK, ACTION_OUSTERS_MAGIC_ATTACK, ACTION_OUSTERS_MAGIC_ATTACK },
	{ ACTION_OUSTERS_DRAIN, ACTION_OUSTERS_DRAIN, ACTION_OUSTERS_DRAIN },
	{ ACTION_OUSTERS_FAST_MOVE_STAND, ACTION_OUSTERS_FAST_MOVE_STAND, ACTION_OUSTERS_FAST_MOVE_STAND },
	{ ACTION_OUSTERS_FAST_MOVE_MOVE, ACTION_OUSTERS_FAST_MOVE_MOVE, ACTION_OUSTERS_FAST_MOVE_MOVE },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK },
	{ ACTION_ATTACK, ACTION_ATTACK, ACTION_ATTACK }
};

//----------------------------------------------------------------------
// Pixel count of one Tile move
//----------------------------------------------------------------------
int MCreature::m_sXTable[MAX_DIRECTION] = 
{
	TILE_X, TILE_X, 0, -TILE_X, -TILE_X, -TILE_X, 0, TILE_X
};

int MCreature::m_sYTable[MAX_DIRECTION] = 
{
	0, -TILE_Y, -TILE_Y, -TILE_Y, 0, TILE_Y, TILE_Y, TILE_Y
};

int *MCreature::m_cXTable[MAX_FRAME_MOVE][MAX_DIRECTION] = { { NULL, } };
int *MCreature::m_cYTable[MAX_FRAME_MOVE][MAX_DIRECTION] = { { NULL, } };

//----------------------------------------------------------------------
// change value per direction
//----------------------------------------------------------------------
POINT g_DirectionValue[MAX_DIRECTION] =
{
	{ -1, 0 },
	{ -1, 1 },
	{ 0, 1 },
	{ 1, 1 },
	{ 1, 0 },
	{ 1, -1 },
	{ 0, -1 },
	{ -1, -1 }
};

bool			IsCreatureMove( MCreature *pCreature )
{
	if( pCreature == NULL )
		return false;

	BYTE Action = pCreature->GetAction();
	if( Action == ACTION_MOVE )
		return true;

	if( pCreature->IsSlayer() )
	{		
		if( Action == ACTION_SLAYER_MOTOR_MOVE )
			return true;
	} else
	if( pCreature->IsVampire() )
	{				
	} else
	if( pCreature->IsOusters() )
	{
		if( Action == ACTION_OUSTERS_MOVE || Action == ACTION_OUSTERS_FAST_MOVE_MOVE )
			return true;
	}

	return false;
}


BYTE GetCreatureActionCountMax( const MCreature* pCreature, int action )
{
	bool bBat = pCreature->GetCreatureType() == CREATURETYPE_BAT;
	bool bWolf = pCreature->GetCreatureType() == CREATURETYPE_WOLF;
	bool bWerWolf = pCreature->GetCreatureType() == CREATURETYPE_WER_WOLF;
	// 2005, 1, 7, sobeit add start
	bool bInstallTurret = pCreature->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET);
	// 2005, 1, 7, sobeit add end
	if( !pCreature->IsAdvancementClass() || !WorldEvents::ViewExists() || bBat || bWolf || bWerWolf || bInstallTurret)
	{
		return (*g_pCreatureTable)[pCreature->GetCreatureType()].GetActionCount( action );
	}

	BYTE* refActionMaxTable = NULL;
	switch( pCreature->GetRace() )
	{
	case RACE_SLAYER :
		{
			ITEM_CLASS rightHand;
			CreatureState::GetAddonItemClass( const_cast<MCreature*>(pCreature), ADDON_RIGHTHAND, &rightHand );
			action = ConvAdvancementSlayerAction( action, rightHand );
		}
		refActionMaxTable = g_AdvanceSlayerActionMaxCount;
		break;
		
	case RACE_VAMPIRE :
		action = GetAdvancementVampireActionFromVampireAction( action, pCreature );
		refActionMaxTable = g_AdvanceVampireActionMaxCount;
		break;
		
	case RACE_OUSTERS :
		{
			ITEM_CLASS rightHand;
			const bool bAddon = CreatureState::GetAddonItemClass( const_cast<MCreature*>(pCreature), ADDON_RIGHTHAND, &rightHand );
			bool bChakram = bAddon && rightHand == ITEM_CLASS_OUSTERS_CHAKRAM;
			
			action = ConvAdvancementOustersActionFromOusterAction( action, bChakram );
			refActionMaxTable = g_AdvanceOustersActionMaxCount;
		}
		break;

	default:
		break;
	}
	
	action -= ADVANCEMENT_ACTION_START;

	if( refActionMaxTable != NULL && action >= 0 )
		return refActionMaxTable[ action ];
	
	return 0;
}

bool			IsCreatureActionAttack( MCreature *pCreature )
{
	if( pCreature == NULL )
		return false;

	BYTE Action = pCreature->GetAction();
	
	if( Action == ACTION_ATTACK || Action == ACTION_MAGIC )
		return true;

	if( pCreature->IsSlayer() )
	{
		if ( Action == ACTION_SLAYER_GUN_SR || Action == ACTION_SLAYER_GUN_AR ||
			Action == ACTION_SLAYER_SWORD || Action == ACTION_SLAYER_BLADE ||
			Action == ACTION_SLAYER_THROW_WEAPON || Action == ACTION_SLAYER_GUN_SG ||
			Action == ACTION_SLAYER_GUN_SMG || Action == ACTION_SLAYER_SWORD_2 ||
			Action == ACTION_SLAYER_BLADE_2	|| Action == ACTION_SLAYER_THROW_POTION	||
			(Action >= ACTION_SLAYER_GUN_SR_SLOW && Action <= ACTION_SLAYER_BLADE_2_FAST )
			)
			return true;
	} 
	else if( pCreature->IsOusters() )
	{
		if ( Action == ACTION_OUSTERS_CHAKRAM || Action == ACTION_OUSTERS_MAGIC_ATTACK ||
			Action == ACTION_OUSTERS_ATTACK_SLOW || Action == ACTION_OUSTERS_ATTACK_FAST ||
			Action == ACTION_OUSTERS_CHAKRAM_SLOW || Action == ACTION_OUSTERS_CHAKRAM_FAST )
			return true;
	}
	else if (pCreature->IsVampire() )
	{		
		if( Action == ACTION_VAMPIRE_ATTACK_FAST || Action == ACTION_VAMPIRE_ATTACK_SLOW)
			return true;
	}

	return false;
}

bool			HasEffectStatusSummonSylph( MCreature* pCreature )
{
	if( 
		pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH ) ||
		pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_GREEN ) ||
		pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_ORANGE ) ||
		pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_RED ) ||
		pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_YELLOW ) ||
		pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_WHITE ) ||
		pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_BROWN ) ||
		pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_LILAC ) ||
		pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_BLACK ) 
		)
		return true;

	return false;
}

void			RemoveEffectStatusSummonSylph( MCreature* pCreature )
{
	pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH );
	pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_GREEN );
	pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_ORANGE );
	pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_RED );
	pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_YELLOW );
	pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_WHITE );
	pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_BROWN );
	pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_LILAC );
	pCreature->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH_BLACK );
}

//----------------------------------------------------------------------
// Init MoveTable
//----------------------------------------------------------------------
// Initialize the cX,cYtable.
// [8 maxFrame kinds][8 directions][maxFrame frames]
//----------------------------------------------------------------------
void
MCreature::InitMoveTable()
{
	for (int maxFrame=0; maxFrame<MAX_FRAME_MOVE; maxFrame++)
	{
		int maxFrame_plus_1	= maxFrame + 1;

		for (int d=0; d<MAX_DIRECTION; d++)
		{			
			// maxFrame frames
			if (m_cXTable[maxFrame][d]!=NULL)
			{
				delete [] m_cXTable[maxFrame][d];			
			}
			m_cXTable[maxFrame][d] = new int [maxFrame_plus_1];

			if (m_cYTable[maxFrame][d]!=NULL)
			{
				delete [] m_cYTable[maxFrame][d];				
			}
			m_cYTable[maxFrame][d] = new int [maxFrame_plus_1];
			
												// when maxFrame is 8..
			int totalX	= -m_sXTable[d];				// 48
			int totalY	= -m_sYTable[d];				// 24

			int		pX	= 0, pY = 0;			// previous x,y value
			float	fX	= 0, fY = 0;			// x,y value added so far
			float	cX	= (float)totalX / maxFrame_plus_1;	// 6
			float	cY	= (float)totalY / maxFrame_plus_1;	// 3

			// Initialize each Frame.
			// Adding maxFrame of them should give totalX, totalY.
			for (int f=0; f<maxFrame; f++)
			{
				// x, y value added so far
				fX += cX;
				fY += cY;

				// Subtract the previous value from the current value to get the difference.
				// Rounding fX seems fine too.
				m_cXTable[maxFrame][d][f] = static_cast<int>(fX - pX);
				m_cYTable[maxFrame][d][f] = static_cast<int>(fY - pY);

				// Remember the current value.
				pX = static_cast<int>(fX);
				pY = static_cast<int>(fY);
			}

			m_cXTable[maxFrame][d][maxFrame] = totalX - pX;
			m_cYTable[maxFrame][d][maxFrame] = totalY - pY;
		}		
	}	
}	

//----------------------------------------------------------------------
// Release MoveTable
//----------------------------------------------------------------------
// Remove the cX,cYTable.
//----------------------------------------------------------------------
void
MCreature::ReleaseMoveTable()
{
	for (int maxFrame=0; maxFrame<MAX_FRAME_MOVE; maxFrame++)
	{
		for (int d=0; d<MAX_DIRECTION; d++)
		{
			if (m_cXTable[maxFrame][d]!=NULL)
			{
				delete [] m_cXTable[maxFrame][d];
				m_cXTable[maxFrame][d] = NULL;
			}

			if (m_cYTable[maxFrame][d]!=NULL)
			{
				delete [] m_cYTable[maxFrame][d];
				m_cYTable[maxFrame][d] = NULL;
			}
		}
	}
}

		
//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------
MCreature::MCreature()
{
	m_PrevPixelX = 0;
	m_PrevPixelY = 0;
	m_DrawSnapFrame = 0;

	m_PersonalShopOpenTime = 0;
	m_HeadSkin = 0;
	m_DelayLastAction = 0;
	m_ObjectType	= TYPE_CREATURE;

	m_CreatureType	= CREATURETYPE_NULL;

	m_CreatureFrameIDs.Init(0);
	m_bExistBody	= FALSE;

	m_MoveType = CREATURE_GROUND;

	// way of moving and the movement unit for it
	m_MoveDevice = MOVE_DEVICE_WALK;
	m_MoveAction = ACTION_MOVE;

	//m_ppCXTable = m_cXTableDeviceNull;
	//m_ppCYTable = m_cYTableDeviceNull;

	m_ColorBody1 = 0;
	m_ColorBody2 = 0;
	m_ChangeColorSet = ATTACHEFFECTCOLOR_NULL;	// not used by default.

	m_ID = OBJECTID_NULL;

	m_pZone = NULL;

	m_X = SECTORPOSITION_NULL;
	m_Y = SECTORPOSITION_NULL;
	
	m_sX = 0;	
	m_sY = 0;

	m_cX = 0;
	m_cY = 0;

	m_Action = ACTION_STAND;				// ID of the Action
	m_Direction = DIRECTION_LEFTDOWN;			// direction to face
	m_DirectionMove = m_Direction;
	m_DirectionMoved = m_Direction;
	m_CurrentDirection = DIRECTION_LEFTDOWN;	// direction currently facing

	//  action
	m_NextAction		= ACTION_STAND;


	m_ActionCount		= 0;				// Current Frame
	m_ActionCountMax	= 0;

	m_MoveCount			= 0;
	m_MoveCountMax		= 0;
	m_NextMoveCount		= 0;

	// height
	m_Z		= 0;
	
	//-------------------------------------------------------
	// next direction to move
	//-------------------------------------------------------
	m_NextX			= SECTORPOSITION_NULL;
	m_NextY			= SECTORPOSITION_NULL;
	m_NextDirection	= DIRECTION_NULL;

	//-------------------------------------------------------
	// coordinate of the last verified Sector
	//-------------------------------------------------------
	m_ServerX	= SECTORPOSITION_NULL;
	m_ServerY	= SECTORPOSITION_NULL;


	//-------------------------------------------------------
	// Chatting String
	//-------------------------------------------------------
	int i;
	m_ChatString = new char* [g_pClientConfig->MAX_CHATSTRING];
	for (i=0; i<g_pClientConfig->MAX_CHATSTRING; i++)
	{
		m_ChatString[i] = new char [g_pClientConfig->MAX_CHATSTRINGLENGTH_PLUS1];
	}

	m_ChatStringCurrent = 0;
	for (i=0; i<g_pClientConfig->MAX_CHATSTRING; i++)
	{
		m_ChatString[i][0] = '\0';
	}
	m_ChatColor = RGB_WHITE;//0xFFFF;
	m_OriChatColor = RGB_WHITE;

	m_ChatTime = 0;

	//-------------------------------------------------------
	// skill type
	//-------------------------------------------------------
	m_nBasicActionInfo		= SKILL_ATTACK_MELEE;
	m_nSpecialActionInfo	= ACTIONINFO_NULL;
	m_nUsedActionInfo		= ACTIONINFO_NULL;

	//-------------------------------------------------------
	// type of Effect attached to the character
	//-------------------------------------------------------
	m_bAttachEffect = new bool [(*g_pEffectSpriteTypeTable).GetSize()];
	for (i=0; i<(*g_pEffectSpriteTypeTable).GetSize(); i++)
	{
		m_bAttachEffect[i] = false;
	}

	m_bEffectStatus = new bool [(*g_pEffectStatusTable).GetSize()];
	for (i=0; i<(*g_pEffectStatusTable).GetSize(); i++)
	{
		m_bEffectStatus[i] = false;
	}

	m_AttachEffectColor = ATTACHEFFECTCOLOR_NULL;

//	m_MaxEffectLight = 0;	// maximum brightness

	//-------------------------------------------------------
	// info about the target
	//-------------------------------------------------------
	SetTraceID( OBJECTID_NULL );
	m_TraceX				= SECTORPOSITION_NULL;
	m_TraceY				= SECTORPOSITION_NULL;
	m_TraceZ				= 0;

	// glowing Effects
	//m_nAlphaEffect = 0;

	// no chat string yet
	m_NextChatFadeTime = g_FrameNow;

	m_pActionResult = NULL;

	m_pName = NULL;

	m_bAlive = true;

	m_bNextAction = false;
		
	m_nNextUsedActionInfo = ACTIONINFO_NULL;

	m_ItemCount = 0;

	m_EffectDelayFrame = 0;

	m_InvisibleCount = 0;
	m_InvisibleStep = 0;

	//-------------------------------------------------------
	// HP
	//-------------------------------------------------------
	MStatus::ClearStatus();

	SetStatus( MODIFY_CURRENT_HP, 100 );
	SetStatus( MODIFY_MAX_HP, 100 );
	SetStatus( MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL );

	m_GuildNumber = -1;
	m_nUnionGuildID = 0;

	m_bFastMove = false;
	m_bKnockBack = 0;

	m_RepeatCount = 0;


	m_WeaponSpeed = WEAPON_SPEED_NORMAL;

	
	m_bHasHead = true;

	m_RecoveryHPAmount = 0; 
	m_RecoveryHPTimes = 0;
	m_RecoveryHPNextTime = MonotonicClock::TimePoint();
	m_RecoveryHPDelayTime = 0;	
	m_RecoveryMPAmount = 0; 
	m_RecoveryMPTimes = 0;
	m_RecoveryMPNextTime = MonotonicClock::TimePoint();
	m_RecoveryMPDelayTime = 0;	
	//m_RecoveryPart = MODIFY_CURRENT_HP;

	m_NextBloodingTime  = MonotonicClock::TimePoint();

	m_DelayActionInfo = ACTIONINFO_NULL;

	m_bMale = true;

	m_ShadowCount = 0;

	m_bInCasket = false;
	m_CasketCount = 0;
	m_CasketType = 0;

	m_bCutHeight = false;
	m_CutHeightCount = 0;
	m_CutHeightCountInc = 0;

	m_bStopBloodDrain = 0;
	m_bStopAbsorbSoul = 0;

	// if -1, not in darkness.
	m_DarknessCount = -1;
	m_DarknessCountInc = 0;

	m_bPlayerParty = false;

	m_bHallu = false;
	m_NameSelection.SelectHallucinationName(*g_pMonsterNameTable, rand());

	m_RegenDelayTime = 0;
	m_RegenNextTime = MonotonicClock::TimePoint();
	m_RegenAmount = 0;
	m_RegenBonusDelayTime = 0;
	m_RegenBonusNextTime = MonotonicClock::TimePoint();
	m_RegenBonusAmount = 0;

	m_Competence = 3;

	m_bTurning = false;
	m_TurningCount = 0;

	m_bCauseCriticalWounds = false;
	m_CauseCriticalWoundsCount = 0;

	m_bBloodyZenith = false;
	m_BloodyZenithCount = 0;

	m_bGunShotGuidance = false;
	m_GunShotGuidanceCount = 0;
	m_fGhost = 0;
	m_GhostCount = 0;

	m_bFakeDie = false;
	m_FakeDieCount = 0;
	m_GradeActionInfo = 0;
	m_TraceTimer = MonotonicClock::TimePoint();
	m_BatColorSet = 0xFFFF;

	m_PetID = OBJECTID_NULL;
	m_ElementalID = OBJECTID_NULL;

	m_bSiegeWar = 0;

	m_bBurningSolCount = 0;
	m_bBurningSol = false;
	m_current_sprite_index = 0;
	m_Input_Chat_Flag = false;
	m_IsSkipMotorStand = false;

	m_Persnal_flag = false;

	m_MasterEffectType = 0;
}

MCreature::~MCreature()
{
	//----------------------------------------
	// remove pet
	//----------------------------------------
	if(GetPetID() != OBJECTID_NULL && ZoneState::Exists())
	{
		ZoneState::RemoveFakeCreature(GetPetID());
	}

	//----------------------------------------
	// remove spirit
	//----------------------------------------
	if(GetElementalID() != OBJECTID_NULL && ZoneState::Exists())
	{
		ZoneState::RemoveFakeCreature(GetElementalID());
	}

	ClearAttachEffect();

	//-------------------------------------------------------
	// remove name
	//-------------------------------------------------------
	if (m_pName!=NULL)
	{
		delete [] m_pName;
		m_pName = NULL;
	}

	//-------------------------------------------------------
	// Chatting String
	//-------------------------------------------------------
	if (m_ChatString != NULL)
	{
		for (int i=0; i<g_pClientConfig->MAX_CHATSTRING; i++)
		{
			delete [] m_ChatString[i];
		}

		delete [] m_ChatString;		
		m_ChatString = NULL;
	}

	//-------------------------------------------------------
	// effectspritetype
	//-------------------------------------------------------
	if (m_bAttachEffect != NULL)
	{
		delete [] m_bAttachEffect;
		m_bAttachEffect = NULL;
	}

	if (m_bEffectStatus != NULL)
	{
		delete [] m_bEffectStatus;
		m_bEffectStatus = NULL;
	}

	if (m_pActionResult!=NULL)
	{
		delete m_pActionResult;
		m_pActionResult = NULL;
	}
	
	ReleaseMoveBuffer();
}

bool IsCreatureTypeVampirePC(int type)
{
	return type == CREATURETYPE_VAMPIRE_MALE1 ||
		type == CREATURETYPE_VAMPIRE_MALE2 ||
		type == CREATURETYPE_VAMPIRE_MALE3 ||
		type == CREATURETYPE_VAMPIRE_FEMALE1 ||
		type == CREATURETYPE_VAMPIRE_FEMALE2 ||
		type == CREATURETYPE_VAMPIRE_FEMALE3 ||
		type == CREATURETYPE_WER_WOLF	||	
				// add by Coffee 2006.11.24  add Ghost clan Yama appearance
		type == CREATURETYPE_VAMPIRE_FEMALE4 ||
		type == CREATURETYPE_VAMPIRE_MALE4	||
		//add by viva
		type == CREATURETYPE_VAMPIRE_MALE5	||
		type == CREATURETYPE_VAMPIRE_FEMALE5	||
		type == CREATURETYPE_VAMPIRE_MALE6	||
		type == CREATURETYPE_VAMPIRE_FEMALE6;

}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Set Creature Type
//----------------------------------------------------------------------
void	
MCreature::SetCreatureType(TYPE_CREATURETYPE type)
{ 
	if (type < (*g_pCreatureTable).GetSize())
	{

	}
	else
	{
		LOG_ERROR("[Error] CreatureType is Wrong. type=%d", (int)type);
		
		type = 0;
	}
//	type = 812;//add by viva
	m_CreatureType		= type;   

	m_ActionCount		= 0;
	m_ActionCountMax	= 0; 
	m_MoveCount			= 0;
	m_MoveCountMax		= 0; 
	m_Action		= (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND;
	m_sX=0; 
	m_sY=0;
	
	// also remove the next action
	m_bNextAction = false;
	m_NextX = SECTORPOSITION_NULL;
	m_NextY = SECTORPOSITION_NULL;

//	int spriteType = (*g_pCreatureTable)[m_CreatureType].SpriteTypes[0];
//
//	m_CreatureFrameID	= (*g_pCreatureSpriteTable)[spriteType].FrameID;
//	m_bExistBody		= m_CreatureFrameID!=FRAMEID_NULL;

		//by csm
	int spriteType;
	m_CreatureFrameIDs.Init((*g_pCreatureTable)[m_CreatureType].SpriteTypes.GetSize());
	for(int framecnt = 0;  framecnt < (*g_pCreatureTable)[m_CreatureType].SpriteTypes.GetSize(); framecnt++ )
	{
		spriteType = (*g_pCreatureTable)[m_CreatureType].SpriteTypes[framecnt];
		m_CreatureFrameIDs.Set(framecnt, (*g_pCreatureSpriteTable)[spriteType].FrameID);
	}
	
	
	m_bExistBody		= m_CreatureFrameIDs[0]!=FRAMEID_NULL;


	
	// color - set only for mobs..
	if(!IsCreatureTypeVampirePC(m_CreatureType))
	{
		m_ColorBody1		= (*g_pCreatureTable)[m_CreatureType].ColorSet;
		m_ColorBody2		= m_ColorBody1;
	}
	
	// remove dark drawing
	m_bFade				= false;

	// Originally made to emit an effect when the creature type changes.
	// Let's do this only when transforming between Slayer<->Vampire
	// remove attached effect
//	ClearAttachEffect();

	//by csm
	spriteType = (*g_pCreatureTable)[m_CreatureType].SpriteTypes[0];
	//---------------------------------------------------------------------------
	// set skill
	//---------------------------------------------------------------------------
	// when there is no basic attack action..
	if (!(*g_pCreatureSpriteTable)[spriteType].IsPlayerSprite()
		//&& (*g_pCreatureTable)[m_CreatureType].GetActionCount( ACTION_ATTACK )==0)
		&& GetCreatureActionCountMax( this, ACTION_ATTACK )==0)
	{
		m_nBasicActionInfo = ACTIONINFO_NULL;
	}
	else
	{
		m_nBasicActionInfo = SKILL_ATTACK_MELEE;
	}

//	m_nSpecialActionInfo	= ACTIONINFO_NULL;
	m_nUsedActionInfo		= ACTIONINFO_NULL;

	//m_WeaponSpeed			= WEAPON_SPEED_NORMAL;

	//---------------------------------------------------------------------------
	// when it is a mob whose color changes completely
	//---------------------------------------------------------------------------
	int changeColor = (*g_pCreatureTable)[m_CreatureType].ChangeColorSet;
	if (changeColor < MAX_COLORSET)
	{
		m_ChangeColorSet = changeColor;
		m_AttachEffectColor = changeColor;
	}
	else
	{
		// ATTACHEFFECTCOLOR_NULLthen it is the normal color.

		if ((*g_pCreatureSpriteTable)[spriteType].IsMonsterSprite())
		{
			m_ChangeColorSet = g_ZoneCreatureColorSet;//ATTACHEFFECTCOLOR_NULL;
			m_AttachEffectColor = g_ZoneCreatureColorSet;//ATTACHEFFECTCOLOR_NULL;
		}
		else
		{
			m_ChangeColorSet = ATTACHEFFECTCOLOR_NULL;
			m_AttachEffectColor = ATTACHEFFECTCOLOR_NULL;
		}
	}

	//---------------------------------------------------------------------------
	// set gender by default(?)..
	// for a wolf or bat, the gender cannot be known from creaturetype..
	//---------------------------------------------------------------------------
	if( m_CreatureType != CREATURETYPE_WER_WOLF )
		m_bMale = (*g_pCreatureTable)[m_CreatureType].bMale;

	//---------------------------------------------------------------------------
	// body afterimage
	//---------------------------------------------------------------------------
	m_ShadowCount = (*g_pCreatureTable)[m_CreatureType].ShadowCount;

	m_bInCasket = false;
	m_CasketCount = 0;

	//---------------------------------------------------------------------------
	// remove Effect
	//---------------------------------------------------------------------------
	if (HasEffectStatus(EFFECTSTATUS_TRANSFORM_TO_BAT)
		&& m_CreatureType!=CREATURETYPE_BAT)
	{
		RemoveEffectStatus(EFFECTSTATUS_TRANSFORM_TO_BAT);
	}

	if (HasEffectStatus(EFFECTSTATUS_TRANSFORM_TO_WOLF)
		&& m_CreatureType!=CREATURETYPE_WOLF)
	{
		RemoveEffectStatus(EFFECTSTATUS_TRANSFORM_TO_WOLF);
	}

	if (HasEffectStatus(EFFECTSTATUS_TRANSFORM_TO_WERWOLF)
		&& m_CreatureType != CREATURETYPE_WER_WOLF)
	{
		RemoveEffectStatus(EFFECTSTATUS_TRANSFORM_TO_WERWOLF);
	}

	// 2004, 9, 13, sobeit add start - Gun Slayer 130 skill
	if (HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET)
		&& m_CreatureType != CREATURETYPE_INSTALL_TURRET)
	{
		RemoveEffectStatus(EFFECTSTATUS_INSTALL_TURRET);
	}
	// 2004, 9, 13, sobeit add end - Gun Slayer 130 skill
	
	// Set the Weapon Speed again.
	int attackSpeed = m_Status[MODIFY_ATTACK_SPEED];

	// If 0, assume the value was not set. Especially for mobs.
	if (attackSpeed!=0)	
	{
		SetWeaponSpeed( attackSpeed );
	}

	CheckRegen();
}

//----------------------------------------------------------------------
// Set BodyColor1
//----------------------------------------------------------------------
void		
MCreature::SetBodyColor1(WORD set)		
{ 
	m_ColorBody1 = set; 
}

//----------------------------------------------------------------------
// Set BodyColor2
//----------------------------------------------------------------------
void		
MCreature::SetBodyColor2(WORD set)		
{ 
	m_ColorBody2 = set; 

	if (m_CreatureType==CREATURETYPE_VAMPIRE_OPERATOR
		|| m_CreatureType==CREATURETYPE_SLAYER_OPERATOR
		|| m_CreatureType==CREATURETYPE_OUSTERS_OPERATOR)
	{
		m_ColorBody1 = set;
	}
}

//----------------------------------------------------------------------
// Clear AttachEffect
//----------------------------------------------------------------------
void
MCreature::ClearAttachEffect()
{
	//-------------------------------------------------------
	// Remove the Effects attached to the Creature from memory.
	//-------------------------------------------------------
	ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();

	while (iEffect != m_listEffect.end())
	{
		MEffect* pEffect = *iEffect;
		delete pEffect;
		
		iEffect++;
	}

	m_listEffect.clear();

	//-------------------------------------------------------
	// Effect attached to the body
	//-------------------------------------------------------
	iEffect = m_listGroundEffect.begin();

	while (iEffect != m_listGroundEffect.end())
	{
		MEffect* pEffect = *iEffect;
		delete pEffect;
		
		iEffect++;
	}

	m_listGroundEffect.clear();

	//-------------------------------------------------------
	// remove check value
	//-------------------------------------------------------
	if (g_pEffectSpriteTypeTable!=NULL)
	{
		for (int i=0; i<(*g_pEffectSpriteTypeTable).GetSize(); i++)
		{
			m_bAttachEffect[i] = false;
		}
	}

	if (g_pEffectStatusTable!=NULL)
	{
		for (int i=0; i<(*g_pEffectStatusTable).GetSize(); i++)
		{
			m_bEffectStatus[i] = false;
		}	
	}

	UnSetHalluCreature();
}
void
MCreature::RemoveGlacierEffect()
{
	SetStop();
	StopCauseCriticalWounds();
	ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();

	while (iEffect != m_listEffect.end())
	{
		MAttachEffect* pEffect = *iEffect;
		TYPE_EFFECTSPRITETYPE Type = pEffect->GetEffectSpriteType();
		if(	EFFECTSPRITETYPE_GDR_GLACIER_SLAYER_START == Type ||
			EFFECTSPRITETYPE_GDR_GLACIER_SLAYER_LOOP == Type ||
			EFFECTSPRITETYPE_GDR_GLACIER_SLAYER_END == Type ||
			EFFECTSPRITETYPE_GDR_GLACIER_VAMPIRE_START == Type ||
			EFFECTSPRITETYPE_GDR_GLACIER_VAMPIRE_LOOP == Type ||
			EFFECTSPRITETYPE_GDR_GLACIER_VAMPIRE_END == Type ||
			EFFECTSPRITETYPE_GDR_GLACIER_OUSTERS_START == Type ||
			EFFECTSPRITETYPE_GDR_GLACIER_OUSTERS_LOOP == Type ||
			EFFECTSPRITETYPE_GDR_GLACIER_OUSTERS_END == Type 

			||EFFECTSPRITETYPE_CAUSE_CRITICAL_WOUND_2 == Type 
			||EFFECTSPRITETYPE_CAUSE_CRITICAL_WOUND_1 == Type 
			||EFFECTSPRITETYPE_CAUSE_CRITICAL_WOUND_3 == Type 
			||EFFECTSPRITETYPE_CAUSE_CRITICAL_WOUND_4 == Type 
			)
		{
			// free the memory
			delete pEffect;

			// remove from the list; erase() hands back the following
			// element, so the loop must not step forward again below.
			iEffect = m_listEffect.erase( iEffect );

			m_bAttachEffect[Type] = false;

			continue;
		}
		
		iEffect++;

	}
	m_bEffectStatus[EFFECTSTATUS_GLACIER]  = false;
}

void
MCreature::Remove_Curse_Paralsis_Effect()
{
	ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();
	while (iEffect != m_listEffect.end())
	{
		MAttachEffect* pEffect = *iEffect;
		TYPE_EFFECTSPRITETYPE Type = pEffect->GetEffectSpriteType();
		if(	EFFECTSPRITETYPE_PURPLE_WALL_2 == Type )
		{
			pEffect->SetCount( 0, 0 );
		}
		iEffect++;
	}
}

//----------------------------------------------------------------------
// Remove EffectStatus
//----------------------------------------------------------------------
// Remove a specific EffectSpriteType.
//----------------------------------------------------------------------
bool
MCreature::RemoveEffectStatus(EFFECTSTATUS status)
{
	DEBUG_ADD_FORMAT("[RemoveEffectStatus] id=%d, status=%d", m_ID, (int)status);
	
	if (status>=g_pEffectStatusTable->GetSize())
	{
		DEBUG_ADD_FORMAT_ERR("[Error]EffectStatus exceed Max : %d", (int)status);
		
		return false;
	}

	if (!m_bEffectStatus[status])
	{
		// [new skill7]
		DEBUG_ADD_FORMAT("NotEffectStatus(%d)", (int)status);
		return false;
	}

	m_bEffectStatus[status] = false;

	bool bUseEffectSprite = (*g_pEffectStatusTable)[status].bUseEffectSprite;

	int pTypes = (*g_pEffectStatusTable)[status].EffectSpriteType;
	//------------------------------------------------------------
	// according to effectStatus.
	//------------------------------------------------------------
	switch (status)
	{
		case EFFECTSTATUS_SUMMON_SYLPH_GREEN :
		case EFFECTSTATUS_SUMMON_SYLPH_ORANGE :
		case EFFECTSTATUS_SUMMON_SYLPH_RED :
		case EFFECTSTATUS_SUMMON_SYLPH_YELLOW :
		case EFFECTSTATUS_SUMMON_SYLPH_WHITE :
		case EFFECTSTATUS_SUMMON_SYLPH_BROWN :
		case EFFECTSTATUS_SUMMON_SYLPH_LILAC :
		case EFFECTSTATUS_SUMMON_SYLPH_BLACK :
		case EFFECTSTATUS_SUMMON_SYLPH:
			SetMoveDevice(MOVE_DEVICE_WALK);
			if(IsAdvancementClass() && status==EFFECTSTATUS_SUMMON_SYLPH)
			{
				pTypes = EFFECTSPRITETYPE_OUSTERS_FASTMOVE_LOOP;
				WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_ADVANCEMENT_SUMMON_SYLPH_END,
					GetX(), GetY(), 0,GetDirection(),	GetID(),	
					GetX(), GetY(), 0, 8, NULL, false);
			}
		break;
		case EFFECTSTATUS_BIKE_CRASH:
			SetMoveDevice(MOVE_DEVICE_WALK);
		break;

		//------------------------------------------------------------
		// when paralysis wears off
		//------------------------------------------------------------
		case EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS :
		case EFFECTSTATUS_EXPLOSION_WATER :
			if (m_bAlive)
			{
				SetStop();
				StopCauseCriticalWounds();
	#ifdef OUTPUT_DEBUG					
			DEBUG_ADD_FORMAT("[CCW] Stop");
	#endif
			}			
		break;

		case EFFECTSTATUS_CURSE_PARALYSIS :
			if (m_bAlive)
			{
				SetStop();			
				Remove_Curse_Paralsis_Effect();
			}			
		break;

		//------------------------------------------------------------
		// when paralysis wears off
		//------------------------------------------------------------
		// add by Coffee 2007-3-21
		case EFFECTSTATUS_SATELLITE_BOMB_AIM :
		// end	
		case EFFECTSTATUS_GUN_SHOT_GUIDANCE_AIM :
			if (m_bAlive)
			{
				SetStop();
				StopGunShotGuidance();
	#ifdef OUTPUT_DEBUG					
			DEBUG_ADD_FORMAT("[GSG] Stop");
	#endif
			}			
		break;

		//------------------------------------------------------------
		// EFFECTSTATUS_SUMMON_CASKET [new skill]
		//------------------------------------------------------------
		case EFFECTSTATUS_CASKET :
			// make it invisible immediately.
			RemoveCasket();
		break;


		//------------------------------------------------------------
		// remove Invisible
		//------------------------------------------------------------
		case EFFECTSTATUS_INVISIBILITY :
		case EFFECTSTATUS_SNIPPING_MODE :	
			// remove the invisible.
			SetVisible();
		break;

		//------------------------------------------------------------
		// remove COMA
		//------------------------------------------------------------
		case EFFECTSTATUS_COMA :
			SetAlive();
			if(IsVampire())
			{
				WorldEvents::ExecuteActionInfoFromMainNode(
							BLOOD_RESURRECT,										// skill number used
						
							m_X, m_Y, 0,
							(int)m_Direction,														// direction used
							
							m_ID,												// info about the target
							m_X, m_Y, 0,
							
							2*16, //5*16, 
							
							NULL,
							
							false,
							
							(int)m_sX, (int)m_sY);	
			}
		break;

		case EFFECTSTATUS_GHOST:
			if (!(*g_pCreatureTable)[m_CreatureType].bFlyingCreature)	// when it is a bat
				SetGroundCreature();
			break;
		case EFFECTSTATUS_WILL_OF_LIFE :
			if( IsVampire() )
				CheckRegen();
			break;

		case EFFECTSTATUS_GLACIER :
			RemoveGlacierEffect();
			break;
		case EFFECTSTATUS_SIEGE_DEFENDERL:
		case EFFECTSTATUS_SIEGE_REINFORCE:
		case EFFECTSTATUS_SIEGE_ATTACKER_1:
		case EFFECTSTATUS_SIEGE_ATTACKER_2:
		case EFFECTSTATUS_SIEGE_ATTACKER_3:
		case EFFECTSTATUS_SIEGE_ATTACKER_4:
		case EFFECTSTATUS_SIEGE_ATTACKER_5:
			SetSiegeWar(0);
			break;
//		case EFFECTSTATUS_BURNING_SOL_CHARGE_1:
//		case EFFECTSTATUS_BURNING_SOL_CHARGE_2:
//		case EFFECTSTATUS_BURNING_SOL_CHARGE_3:
//		case EFFECTSTATUS_BURNING_SOL_CHARGE_4:
//			StopBurningSol();
//			break;
		case EFFECTSTATUS_INSTALL_TURRET:
			WorldEvents::ExecuteActionInfoFromMainNode(RESULT_SKILL_INSTALL_TURRET,GetX(), GetY(), 0,0,GetID()	,
				GetX(), GetY(), 0, 0, NULL, false);
			SetAction( ACTION_STAND );
			SetInstallTurretCount(0);
			SetInstallTurretDirect(2);
			SetDirection( 2 );
			SetCurrentDirection( 2 );
			break;

		case EFFECTSTATUS_GRAND_MASTER_SLAYER:
		case EFFECTSTATUS_GRAND_MASTER_VAMPIRE:
		case EFFECTSTATUS_GRAND_MASTER_OUSTERS:
		case EFFECTSTATUS_GRAND_MASTER_SLAYER_130:
		case EFFECTSTATUS_GRAND_MASTER_SLAYER_150:
		case EFFECTSTATUS_GRAND_MASTER_VAMPIRE_130:
		case EFFECTSTATUS_GRAND_MASTER_VAMPIRE_150:
		case EFFECTSTATUS_GRAND_MASTER_OUSTERS_130:
		case EFFECTSTATUS_GRAND_MASTER_OUSTERS_150:
			{
				pTypes = GetMasterEffectType(status);
			}
			break;
		default:
			break;
	}


	//------------------------------------------------------------
	//
	//			when expressed with EffectSprite
	//
	//------------------------------------------------------------
	if (bUseEffectSprite)
	{
//		int* pTypes = NULL;
		
//		if (status==EFFECTSTATUS_SPIRIT_GUARD_1 || 
//			status==EFFECTSTATUS_SPIRIT_GUARD_2 || 
//			status==EFFECTSTATUS_SPIRIT_GUARD_3 || 
//			status==EFFECTSTATUS_SPIRIT_GUARD_4)
//		{
//			numTypes = EFFECTSTATUS_SPIRIT_GUARD_4 - EFFECTSTATUS_SPIRIT_GUARD_1 +1;
//
//			pTypes = new int[numTypes];
//
//			for(int i = 0; i < numTypes; i++)
//				pTypes[i] = EFFECTSPRITETYPE_SPIRIT_GUARD;
//		}
//		else
//		if (status==EFFECTSTATUS_ARMAGEDDON)
//		{			
//			numTypes = 4;
//			pTypes = new int[numTypes];
//
//			pTypes[0] = EFFECTSPRITETYPE_ARMAGEDDON_BACK_0_2;
//			pTypes[1] = EFFECTSPRITETYPE_ARMAGEDDON_BACK_1_2;
//			pTypes[2] = EFFECTSPRITETYPE_ARMAGEDDON_BACK_2_2;
//			pTypes[3] = EFFECTSPRITETYPE_ARMAGEDDON_BACK_3_2;
//		}
//		else
		{
//			pTypes = new int[numTypes];

			//pTypes[0] = (*g_pEffectStatusTable)[status].EffectSpriteType;
			
		}		

		//for (int i=0; i<numTypes; i++)
		{
		//	TYPE_EFFECTSPRITETYPE type = pTypes[i];
			TYPE_EFFECTSPRITETYPE type = pTypes;
			TYPE_EFFECTSPRITETYPE type2 = EFFECTSPRITETYPE_NULL;

			if(type == EFFECTSPRITETYPE_FAST_MOVE_STOP)
				type2 = EFFECTSPRITETYPE_FAST_MOVE_FLY;
			
			if( type >= EFFECTSPRITETYPE_SUMMON_SYLPH_GREEN_STOP &&
				type <= EFFECTSPRITETYPE_SUMMON_SYLPH_BLACK_STOP &&
				((type - EFFECTSPRITETYPE_SUMMON_SYLPH_GREEN_STOP)&0x1) == 0
				)
				type2 = type - 1;

			//-------------------------------------------------------
			// Among the Effects attached to the Creature
			// remove those whose EffectSpriteType is type.
			//-------------------------------------------------------
			if (type < g_pEffectSpriteTypeTable->GetSize())
			{
				if (IsFemale()
					&& (*g_pEffectSpriteTypeTable)[type].FemaleEffectSpriteType!=EFFECTSPRITETYPE_NULL)
				{
					type = (*g_pEffectSpriteTypeTable)[type].FemaleEffectSpriteType;
				}
				if (type >= g_pEffectSpriteTypeTable->GetSize() || m_bAttachEffect == NULL)
					return false;
				

				if (m_bAttachEffect[type])
				{
					//-------------------------------------------------------
					// attached to the body
					//-------------------------------------------------------
					ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();

					bool re = false;
					while (iEffect != m_listEffect.end())
					{
						MAttachEffect* pEffect = *iEffect;
						
						//-------------------------------------------------------
						// find the same type.
						//-------------------------------------------------------
						if (pEffect->IsEffectSprite() 
							&& (pEffect->GetEffectSpriteType() == type || (type2 != EFFECTSPRITETYPE_NULL &&
							type2 == pEffect->GetEffectSpriteType())) )
						{
							// free the memory
							delete pEffect;

							// remove from the list; erase() hands back the following
							// element, so the loop must not step forward again below.
							iEffect = m_listEffect.erase( iEffect );

							m_bAttachEffect[type] = false;

							re = true;
							continue;
						}
						
						iEffect++;

					}

					if(re)
					{
					//	delete [] pTypes;
						return true;
					}

					//-------------------------------------------------------
					// attached to the ground
					//-------------------------------------------------------
					iEffect = m_listGroundEffect.begin();

					while (iEffect != m_listGroundEffect.end())
					{
						MAttachEffect* pEffect = *iEffect;
						
						//-------------------------------------------------------
						// find the same type.
						//-------------------------------------------------------
						if ((pEffect->IsEffectSprite() && pEffect->GetEffectSpriteType() == type)
							|| (type2 != EFFECTSPRITETYPE_NULL && type2 == pEffect->GetEffectSpriteType()) )
						{
							// remove from memory
							delete pEffect;

							// remove from list
							m_listGroundEffect.erase( iEffect );

							m_bAttachEffect[type] = false;
					
						//	delete [] pTypes;
							return true;
						}
						
						iEffect++;
					}
				}
			}
		}

	//	delete [] pTypes;
	}
	//------------------------------------------------------------
	//
	//			when expressed with EffectColor
	//
	//------------------------------------------------------------
	else
	{
		WORD colorSet = (*g_pEffectStatusTable)[status].EffectColor;

		//-------------------------------------------------------
		// Among the Effects attached to the Creature
		// remove those whose EffectColor is color.
		//-------------------------------------------------------
		ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();

		while (iEffect != m_listEffect.end())
		{
			MAttachEffect* pEffect = *iEffect;
			
			//-------------------------------------------------------
			// find the same type.
			//-------------------------------------------------------
			if (pEffect->IsEffectColor() && pEffect->GetEffectColor() == colorSet)
			{
				// remove from memory
				delete pEffect;

				// remove from list
				m_listEffect.erase( iEffect );

				return true;
			}
			
			iEffect++;
		}		

		// assume there is no GroundEffect.
	}
	
	return false;
}

//----------------------------------------------------------------------
// Add EffectStatus
//----------------------------------------------------------------------
// 
// effect showing a special state
//
// Expression of Effects shown attached to a Creature
//
// since stacking is not allowed..
// only add Effects that are not yet attached.
//
// However, there are cases where the time must be extended.
//----------------------------------------------------------------------
bool
MCreature::AddEffectStatus(enum EFFECTSTATUS status, DWORD delayFrame)
{
	LOG_DEBUG("[AddEffectStatus] id=%d, status=%d, delay=%d", m_ID, (int)status, delayFrame);
//	if( status == EFFECTSTATUS_VIEW_HP )
//	{
//		int a = 0;
//	}
	
	//----------------------------------------------------------
	// when already dead
	//----------------------------------------------------------
	if (IsDead() && status != EFFECTSTATUS_COMA && 
		!(
		(GetCreatureType() >= 371 && GetCreatureType() <= 376) || (GetCreatureType() >= 560 && GetCreatureType() <= 563)) && 
		GetCreatureType() != 482 && GetCreatureType() != 650 && !(GetCreatureType() >= 526 && GetCreatureType() <= 549) &&
		GetCreatureType() != 670 && GetCreatureType() != 672 && GetCreatureType() != 673
		)	// attach an effect for a holy object
	{
		DEBUG_ADD("Don't AddEffectStatus : Already Dead");
		
		return false;
	}

	if (status>=g_pEffectStatusTable->GetSize())
	{
		LOG_ERROR("[Error]EffectStatus exceed Max : %d", (int)status);
		
		return false;
	}

	int eCount = 0; // number of effects
	int orbit_type = 0;

	//------------------------------------------------------------
	// according to effectStatus.
	//------------------------------------------------------------
	MEffect::EFFECT_TYPE eType = MEffect::EFFECT_ATTACH;
	TYPE_EFFECTSPRITETYPE type = (*g_pEffectStatusTable)[status].EffectSpriteType;
	switch (status)
	{
	case EFFECTSTATUS_SUMMON_SYLPH_GREEN :
	case EFFECTSTATUS_SUMMON_SYLPH_ORANGE :
	case EFFECTSTATUS_SUMMON_SYLPH_RED :
	case EFFECTSTATUS_SUMMON_SYLPH_YELLOW :
	case EFFECTSTATUS_SUMMON_SYLPH_WHITE :
	case EFFECTSTATUS_SUMMON_SYLPH_BROWN :
	case EFFECTSTATUS_SUMMON_SYLPH_LILAC :
	case EFFECTSTATUS_SUMMON_SYLPH_BLACK :
	case EFFECTSTATUS_SUMMON_SYLPH:
		{
			SetMoveDevice(MOVE_DEVICE_SUMMON_SYLPH);
			/********  Edit By sonic remove the ball-change effect after the Magic Spirit second transfer********/
			
			if(IsAdvancementClass() && status==EFFECTSTATUS_SUMMON_SYLPH)
			{
				type = EFFECTSPRITETYPE_OUSTERS_FASTMOVE_LOOP;
				WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_ADVANCEMENT_SUMMON_SYLPH_START,GetX(), GetY(), 0,GetDirection(),	GetID(),
				GetX(), GetY(), 0, 8, NULL, false);
			}
			/********End *******************************************/
		}
		break;

		//------------------------------------------------------------
		// EFFECTSTATUS_INVISIBILITY
		//------------------------------------------------------------
		case EFFECTSTATUS_INVISIBILITY :
			// make it invisible immediately.
			SetInvisibleSoon();
		break;

		//------------------------------------------------------------
		// EFFECTSTATUS_SUMMON_CASKET [new skill]
		//------------------------------------------------------------
		case EFFECTSTATUS_CASKET :
			// make it invisible immediately.
			AddCasketSoon(0);
		break;

		//------------------------------------------------------------
		// EFFECTSTATUS_GUN_SHOT_GUIDANCE_AIM
		//------------------------------------------------------------

		// add by Coffee 2007-3-21
		case EFFECTSTATUS_SATELLITE_BOMB_AIM :
		// end		
		case EFFECTSTATUS_GUN_SHOT_GUIDANCE_AIM :
#ifdef OUTPUT_DEBUG					
				DEBUG_ADD_FORMAT("[GSG] Set");
#endif
//				AffectMoveBufferAll();
//				ActionMoveNextPosition();
			if(!IsVampire())
			{
				SetAction((*g_pActionInfoTable)[m_nBasicActionInfo].GetAction());
				m_ActionCount = 0;
				SetGunShotGuidance(delayFrame);
			}
			else
			{
				return false;
			}
			break;

		//------------------------------------------------------------
		// EFFECTSTATUS_CURSE_PARALYSIS
		//------------------------------------------------------------
		case EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS :
#ifdef OUTPUT_DEBUG					
				DEBUG_ADD_FORMAT("[CCW] Set");
#endif
//				AffectMoveBufferAll();
//				ActionMoveNextPosition();
			// For CauseCriticalWounds, the Slayer does not call AddEffect. Only damage, without attaching an effect
			// 2005, 1, 18, sobeit modify start // g_pZone->GetID() == 4002 added - quest Slayer npc under cause_critical_wounds
			if(!IsSlayer() || GetCreatureType() == 793) 
			// 2005, 1, 18, sobeit modify end
			{
				SetAction(ACTION_DRAINED);
				m_ActionCount = 0;
				SetCauseCriticalWounds(delayFrame);
			}
			else
			{
				SetAction(ACTION_DAMAGED);
				return false;
			}
			break;

		case EFFECTSTATUS_EXPLOSION_WATER:
			SetAction(ACTION_DRAINED);
			m_ActionCount = 0;
			SetCauseCriticalWounds(delayFrame);
			break;
			
		case EFFECTSTATUS_CURSE_PARALYSIS :
			// move all of them.
			AffectMoveBufferAll();
			
			ActionMoveNextPosition();
		break;

		case EFFECTSTATUS_FIRE_ELEMENTAL:
			if(GetClassType() == CLASS_FAKE)
			{
				delayFrame = 0xFFFF;
				eType = MEffect::EFFECT_ATTACH_ORBIT;
				eCount = 1;
				orbit_type = 2;
			}
			else
			{
				WorldEvents::AddOustersElemental( this, 699, status );
				delayFrame = 0;
			}
			break;

		case EFFECTSTATUS_FIRE_ELEMENTAL_ATTACK:
			if(GetClassType() == CLASS_FAKE)
			{
//				ClearAttachEffect();
				delayFrame = 0;
				WorldEvents::ExecuteActionInfoFromMainNode(
							SKILL_CLIENT_FIRE_ELEMENTAL_ATTACK,										// skill number used
						
							m_X, m_Y, 0,
							(int)m_Direction,														// direction used
							
							m_ID,												// info about the target
							m_X, m_Y, 0,
							
							0xFFFF, //5*16, 
							
							NULL,
							
							false,
							
							(int)m_sX, (int)m_sY);	
			}
			else
			{
				MCreature *pFakeCreature = ZoneState::GetFakeCreature(GetElementalID());
				if(pFakeCreature != NULL)
				{
					pFakeCreature->AddEffectStatus(status, 0xFFFF);
				}
				return false;
			}
			break;

		case EFFECTSTATUS_WATER_ELEMENTAL:
			if(GetClassType() == CLASS_FAKE)
			{
				eType = MEffect::EFFECT_ATTACH_ORBIT;
				eCount = 1;
				orbit_type = 2;
				delayFrame = 0xFFFF;
			}
			else
			{
				WorldEvents::AddOustersElemental( this, 699, status );
				delayFrame = 0;
			}
			break;

		case EFFECTSTATUS_WATER_ELEMENTAL_HEAL:
			if(GetClassType() == CLASS_FAKE)
			{
//				ClearAttachEffect();
				delayFrame = 0;
				WorldEvents::ExecuteActionInfoFromMainNode(
							SKILL_CLIENT_WATER_ELEMENTAL_HEAL,										// skill number used
						
							m_X, m_Y, 0,
							(int)m_Direction,														// direction used
							
							m_ID,												// info about the target
							m_X, m_Y, 0,
							
							0xFFFF, //5*16, 
							
							NULL,
							
							false,
							
							(int)m_sX, (int)m_sY);	
			}
			else
			{
				MCreature *pFakeCreature = ZoneState::GetFakeCreature(GetElementalID());
				if(pFakeCreature != NULL)
				{
					pFakeCreature->AddEffectStatus(status, 0xFFFF);
				}
				return false;
			}
			break;

		//------------------------------------------------------------
		// EFFECTSTATUS_COMA
		//------------------------------------------------------------
		case EFFECTSTATUS_COMA :
			SetDead();
		break;

		case EFFECTSTATUS_FIRE_ELEMENTAL_DAMAGED:
			{
				TYPE_EFFECTSPRITETYPE type = (*g_pEffectStatusTable)[status].EffectSpriteType;
				if(type < g_pEffectSpriteTypeTable->GetSize())
				{
					TYPE_FRAMEID	frameID = (*g_pEffectSpriteTypeTable)[type].FrameID;
					BYTE			maxFrame;
					
					// BLT_NORMAL		
					BLT_TYPE bltType = (*g_pEffectSpriteTypeTable)[type].BltType;
					maxFrame = WorldEvents::GetMaxEffectFrame(bltType, frameID);

					delayFrame = maxFrame;
				}
			}
			break;

		case EFFECTSTATUS_WATER_ELEMENTAL_HEALED:
			{
				TYPE_EFFECTSPRITETYPE type = (*g_pEffectStatusTable)[status].EffectSpriteType;
				if(type < g_pEffectSpriteTypeTable->GetSize())
				{
					TYPE_FRAMEID	frameID = (*g_pEffectSpriteTypeTable)[type].FrameID;
					BYTE			maxFrame;
					
					// BLT_NORMAL		
					BLT_TYPE bltType = (*g_pEffectSpriteTypeTable)[type].BltType;
					maxFrame = WorldEvents::GetMaxEffectFrame(bltType, frameID);

					delayFrame = maxFrame;
				}
			}
			break;

		//------------------------------------------------------------
		// EFFECTSTATUS_ARMAGEDDON
		//------------------------------------------------------------
		case EFFECTSTATUS_ARMAGEDDON :
		case EFFECTSTATUS_GROUND_ELEMENTAL_CENTER:
			delayFrame = 0xFFFF;		// infinite..
		break;

		//------------------------------------------------------------
		// holy object related
		//------------------------------------------------------------
		case EFFECTSTATUS_HAS_BLOOD_BIBLE_GREGORI :         // 151			
		case EFFECTSTATUS_HAS_BLOOD_BIBLE_NEMA :         // 152				
		case EFFECTSTATUS_HAS_BLOOD_BIBLE_LEGIOS :         // 153				
		case EFFECTSTATUS_HAS_BLOOD_BIBLE_MIHOLE :         // 154				
		case EFFECTSTATUS_HAS_BLOOD_BIBLE_AROSA :         // 155				
		case EFFECTSTATUS_HAS_BLOOD_BIBLE_ARMEGA :         // 156				
		case EFFECTSTATUS_HAS_BLOOD_BIBLE_INI :         // 157				
		case EFFECTSTATUS_HAS_BLOOD_BIBLE_JAVE :         // 158				
		case EFFECTSTATUS_HAS_BLOOD_BIBLE_CHASPA :         // 159				
		case EFFECTSTATUS_HAS_BLOOD_BIBLE_CONCILIA :         // 160				
		case EFFECTSTATUS_HAS_BLOOD_BIBLE_KIRO :         // 161				
		case EFFECTSTATUS_HAS_BLOOD_BIBLE_HILLEL :         // 162						
		case EFFECTSTATUS_SHRINE_GUARD_GREGORI :				// 163				
		case EFFECTSTATUS_SHRINE_GUARD_NEMA :				// 164				
		case EFFECTSTATUS_SHRINE_GUARD_LEGIOS :				// 165				
		case EFFECTSTATUS_SHRINE_GUARD_MIHOLE :				// 166				
		case EFFECTSTATUS_SHRINE_GUARD_AROSA :				// 167				
		case EFFECTSTATUS_SHRINE_GUARD_ARMEGA :				// 168				
		case EFFECTSTATUS_SHRINE_GUARD_INI :				// 169				
		case EFFECTSTATUS_SHRINE_GUARD_JAVE :				// 170				
		case EFFECTSTATUS_SHRINE_GUARD_CHASPA :				// 171				
		case EFFECTSTATUS_SHRINE_GUARD_CONCILIA :				// 172				
		case EFFECTSTATUS_SHRINE_GUARD_KIRO :				// 173				
		case EFFECTSTATUS_SHRINE_GUARD_HILLEL :				// 174						
		case EFFECTSTATUS_SHRINE_HOLY_GREGORI :				// 175				
		case EFFECTSTATUS_SHRINE_HOLY_NEMA :				// 176				
		case EFFECTSTATUS_SHRINE_HOLY_LEGIOS :				// 177				
		case EFFECTSTATUS_SHRINE_HOLY_MIHOLE :				// 178				
		case EFFECTSTATUS_SHRINE_HOLY_AROSA :				// 179				
		case EFFECTSTATUS_SHRINE_HOLY_ARMEGA :				// 180				
		case EFFECTSTATUS_SHRINE_HOLY_INI :				// 181				
		case EFFECTSTATUS_SHRINE_HOLY_JAVE :				// 182				
		case EFFECTSTATUS_SHRINE_HOLY_CHASPA :				// 183				
		case EFFECTSTATUS_SHRINE_HOLY_CONCILIA :			// 184				
		case EFFECTSTATUS_SHRINE_HOLY_KIRO :			// 185				
		case EFFECTSTATUS_SHRINE_HOLY_HILLEL :			// 186				
		case EFFECTSTATUS_SHRINE_SHIELD:
		case EFFECTSTATUS_SLAYER_RELIC:
//		case EFFECTSTATUS_DROP_BLOOD_BIBLE:			
		case EFFECTSTATUS_VAMPIRE_RELIC:
			delayFrame = 0xFFFF;		// infinite..
			break;

		//------------------------------------------------------------
		// EFFECTSTATUS_CURE_CRITICAL_WOUNDS
		//------------------------------------------------------------
		case EFFECTSTATUS_CURE_CRITICAL_WOUNDS :
			eType = MEffect::EFFECT_ATTACH_ORBIT;
			eCount = 1;
			orbit_type = 0;
			break;
			
		case EFFECTSTATUS_SPIRIT_GUARD_1 :
			eType = MEffect::EFFECT_ATTACH_ORBIT;
			eCount = 1;
			orbit_type = 1;
			break;
			
		case EFFECTSTATUS_SPIRIT_GUARD_2 :
			eType = MEffect::EFFECT_ATTACH_ORBIT;
			eCount = 2;
			orbit_type = 1;
			break;
			
		case EFFECTSTATUS_SPIRIT_GUARD_3 :
			eType = MEffect::EFFECT_ATTACH_ORBIT;
			eCount = 3;
			orbit_type = 1;
			break;
			
		case EFFECTSTATUS_SPIRIT_GUARD_4 :
			eType = MEffect::EFFECT_ATTACH_ORBIT;
			eCount = 4;
			orbit_type = 1;
			break;

		case EFFECTSTATUS_HALLUCINATION:
			{
				if(this == PlayerState::Creature())
				{
					MEvent event;
					event.eventID = EVENTID_HALLUCINATION;
					event.eventType = EVENTTYPE_EFFECT;
					event.eventFlag = EVENTFLAG_FADE_SCREEN;
					event.eventDelay = delayFrame/g_pClientConfig->FPS*1000;
					event.parameter1 = EFFECTSTATUS_HALLUCINATION;
					event.parameter2 = (30 << 16) | (0 << 8) | 30;	// alpha depth, r, g, b
					g_pEventManager->AddEvent(event);
				}
			}
			break;

		case EFFECTSTATUS_GHOST:
			SetFlyingCreature();
			break;

		case EFFECTSTATUS_LEVELUP_SLAYER:
			delayFrame = 30;
			WorldEvents::PlaySound(SOUND_SLAYER_ENCHANT_PFA);
			break;

		case EFFECTSTATUS_DISTANCE_BLITZ:
			delayFrame = 10;
			break;

		case EFFECTSTATUS_LEVELUP_VAMPIRE:
			delayFrame = 24;
			WorldEvents::PlaySound(SOUND_SLAYER_ENCHANT_PFA);
			break;

		case EFFECTSTATUS_WATER_SHIELD:
			delayFrame = 12;
			break;

		case EFFECTSTATUS_WARP_BLOOD_BIBLE_FROM_ME :// 195 effect stamped on the head when the Blood Bible I was holding is warped -_-
		case EFFECTSTATUS_WARP_BLOOD_BIBLE_FROM_TILE :		// 196 effect stamped on that tile when a Blood Bible lying on the ground is warped
		case EFFECTSTATUS_SHRINE_GUARD_WARP :			// 187		when warping from the Guard Shrine
		case EFFECTSTATUS_SHRINE_HOLY_WARP :			// 188		when warping from the Holy Land Shrine
		case EFFECTSTATUS_WARP_BLOOD_BIBLE_SLAYER:
		case EFFECTSTATUS_WARP_BLOOD_BIBLE_VAMPIRE:
			delayFrame = 14;				
			break;
		case EFFECTSTATUS_WILL_OF_LIFE :
			delayFrame = 84;			
			break;
		case EFFECTSTATUS_ILLUSION_OF_AVENGE :
			delayFrame = 32;
			break;
		case EFFECTSTATUS_DENIAL_MAGIC_DAMAGED :
			delayFrame = 14;
			break;
		case EFFECTSTATUS_BACK_STAB :
		case EFFECTSTATUS_BACK_STAB_2 :
			delayFrame = 12;
			break;
		case EFFECTSTATUS_FIRE_OF_SOUL_STONE :
			delayFrame = 16;
			break;
		case EFFECTSTATUS_SAND_OF_SOUL_STONE :
			delayFrame = 10;
			break;
		case EFFECTSTATUS_HAS_FLAG :
			delayFrame = 0xFFFF;
			break;
		case EFFECTSTATUS_FLAG_INSERT :
			delayFrame = 0xFFFF;
			break;
		case EFFECTSTATUS_STONE_SKIN_DAMAGED :
			delayFrame = 8;
			break;
		case EFFECTSTATUS_HAS_SWEEPER :                       // 292
		case EFFECTSTATUS_HAS_SWEEPER_2 :                     // 293
		case EFFECTSTATUS_HAS_SWEEPER_3 :                     // 294
		case EFFECTSTATUS_HAS_SWEEPER_4 :                     // 295
		case EFFECTSTATUS_HAS_SWEEPER_5 :                     // 296
		case EFFECTSTATUS_HAS_SWEEPER_6 :                     // 297
		case EFFECTSTATUS_HAS_SWEEPER_7 :                     // 298
		case EFFECTSTATUS_HAS_SWEEPER_8 :                     // 299
		case EFFECTSTATUS_HAS_SWEEPER_9 :                     // 300
		case EFFECTSTATUS_HAS_SWEEPER_10 :                    // 301
		case EFFECTSTATUS_HAS_SWEEPER_11 :                    // 302
		case EFFECTSTATUS_HAS_SWEEPER_12 :                    // 303
		case EFFECTSTATUS_KEEP_SWEEPER :                      // 304
		case EFFECTSTATUS_KEEP_SWEEPER_2 :                    // 305
		case EFFECTSTATUS_KEEP_SWEEPER_3 :                    // 306
		case EFFECTSTATUS_KEEP_SWEEPER_4 :                    // 307
		case EFFECTSTATUS_KEEP_SWEEPER_5 :                    // 308
		case EFFECTSTATUS_KEEP_SWEEPER_6 :                    // 309
		case EFFECTSTATUS_KEEP_SWEEPER_7 :                    // 310
		case EFFECTSTATUS_KEEP_SWEEPER_8 :                    // 311
		case EFFECTSTATUS_KEEP_SWEEPER_9 :                    // 312
		case EFFECTSTATUS_KEEP_SWEEPER_10 :                   // 313
		case EFFECTSTATUS_KEEP_SWEEPER_11 :                   // 314
		case EFFECTSTATUS_KEEP_SWEEPER_12 :                   // 315			
		case EFFECTSTATUS_SLAYER_REGEN_ZONE :                 // 316
		case EFFECTSTATUS_VAMPIRE_REGEN_ZONE :                // 317
		case EFFECTSTATUS_OUSTERS_REGEN_ZONE :                // 318
		case EFFECTSTATUS_DEFAULT_REGEN_ZONE :                // 319
		case EFFECTSTATUS_SLAYER_TRYING_1 :                   // 320
		case EFFECTSTATUS_SLAYER_TRYING_2 :                   // 321
		case EFFECTSTATUS_SLAYER_TRYING_3 :                   // 322
		case EFFECTSTATUS_VAMPIRE_TRYING_1 :                  // 323
		case EFFECTSTATUS_VAMPIRE_TRYING_2 :                  // 324
		case EFFECTSTATUS_VAMPIRE_TRYING_3 :                  // 325
		case EFFECTSTATUS_OUSTERS_TRYING_1 :                  // 326
		case EFFECTSTATUS_OUSTERS_TRYING_2 :                  // 327
		case EFFECTSTATUS_OUSTERS_TRYING_3 :                  // 328
			delayFrame = 0xFFFF;
			break;
		case EFFECTSTATUS_CAPTURE_REGEN_ZONE :                // 331
			delayFrame = 36;
			break;

		// 2004, 6 ,12 sobeit add start - on a 130,150 master effect, erase the previous master effect.
		case EFFECTSTATUS_GRAND_MASTER_SLAYER:
		case EFFECTSTATUS_GRAND_MASTER_VAMPIRE:
		case EFFECTSTATUS_GRAND_MASTER_OUSTERS:
		case EFFECTSTATUS_GRAND_MASTER_SLAYER_130:
		case EFFECTSTATUS_GRAND_MASTER_SLAYER_150:
		case EFFECTSTATUS_GRAND_MASTER_VAMPIRE_130:
		case EFFECTSTATUS_GRAND_MASTER_VAMPIRE_150:
		case EFFECTSTATUS_GRAND_MASTER_OUSTERS_130:
		case EFFECTSTATUS_GRAND_MASTER_OUSTERS_150:
			{
				delayFrame = 0xFFFF;
				type = GetMasterEffectType(status);
				if(0 == type || IsDead()) 
					return false;
			}
			break;
		// 2004, 6 ,12 sobeit add end - on a 130,150 master effect, erase the previous master effect.
//		case EFFECTSTATUS_GDR_FLOATING:
//			//SetAction(ACTION_DRAINED);
//		//	m_MoveAction = ACTION_DRAINED;
//			if(GetCreatureType() == 717) // when it is Zildre
//			{ 
//				
//			}
//			break;
		case EFFECTSTATUS_BLINDNESS:
			WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_BLINDNESS,GetX(), GetY(), 0,GetDirection(),	GetID(),
								GetX(), GetY(), 0, delayFrame, NULL, false);
			break;	
		case EFFECTSTATUS_GLACIER:
			if(IsAlive() /*&& !HasEffectStatus(EFFECTSTATUS_GLACIER)*/)
			{
				
				SetAction(ACTION_DRAINED);
				m_ActionCount = 0;
				SetCauseCriticalWounds(delayFrame);
			//	if(!HasEffectStatus(EFFECTSTATUS_GLACIER))
				{
					// Twenty frames shorter, never below 0 (the frames are unsigned).
					const DWORD glacierFrames = delayFrame > 20 ? delayFrame - 20 : 0;
					if(IsSlayer())
						WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_GDR_GLACIER_SLAYER,GetX(), GetY(), 0,GetDirection(),	GetID(),
										GetX(), GetY(), 0, glacierFrames, NULL, false);
					else if(IsVampire())
						WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_GDR_GLACIER_VAMPIRE,GetX(), GetY(), 0,GetDirection(),	GetID(),
										GetX(), GetY(), 0, glacierFrames, NULL, false);
					else if(IsOusters())
						WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_GDR_GLACIER_OUSTERS,GetX(), GetY(), 0,GetDirection(),	GetID(),
										GetX(), GetY(), 0, glacierFrames, NULL, false);
				}
			}
		//	m_bEffectStatus[status] = true;
		//	return true;
			break;
		case EFFECTSTATUS_SUMMON_GDR:
			delayFrame = 34;
			break;
//		case EFFECTSTATUS_ICICLE_DROP:
//			delayFrame = 16;
//			break;
//		case EFFECTSTATUS_ICICLE_AUGER:
//			delayFrame = 14;
//			break;
//		case EFFECTSTATUS_ICICLE_LARGE_AUGER:
//			delayFrame = 12;
//			break;
		
		case EFFECTSTATUS_GOLD_MEDAL:
			delayFrame = 21;
			break;
		case EFFECTSTATUS_SIEGE_DEFENDERL:
		case EFFECTSTATUS_SIEGE_REINFORCE:
		case EFFECTSTATUS_SIEGE_ATTACKER_1:
		case EFFECTSTATUS_SIEGE_ATTACKER_2:
		case EFFECTSTATUS_SIEGE_ATTACKER_3:
		case EFFECTSTATUS_SIEGE_ATTACKER_4:
		case EFFECTSTATUS_SIEGE_ATTACKER_5:
			SetSiegeWar(status - EFFECTSTATUS_SIEGE_DEFENDERL + 1);
			break;
		case EFFECTSTATUS_REPUTO_FACTUM:
			delayFrame = 0;
			WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_HIT_REPUTO_FACTUM, GetX(), GetY(), 0,GetDirection(),	GetID(),
								GetX(), GetY(), 0, 8, NULL, false);
			break;
		case EFFECTSTATUS_BURNING_SOL_CHARGE_1:
			SetAction(ACTION_SLAYER_BLADE);
			SetBurningSol(0);
			break;
		case EFFECTSTATUS_BURNING_SOL_CHARGE_2:
//			if(HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1))
//			{
//				MCreature::RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1);
//			}
//			else
			if(!HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1))
			{
				SetAction(ACTION_SLAYER_BLADE);
				SetBurningSol(0);
			}
			
			break;
		case EFFECTSTATUS_BURNING_SOL_CHARGE_3:
//			if(HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2))
//			{
//				MCreature::RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1);
//				MCreature::RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2);
//			}
//			else
			if(!HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1)&&
				!HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2))
			{
				SetAction(ACTION_SLAYER_BLADE);
				SetBurningSol(0);
			}
			break;
		case EFFECTSTATUS_BURNING_SOL_CHARGE_4:
//			if(HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_3))
//			{
//				MCreature::RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1);
//				MCreature::RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2);
//				MCreature::RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_3);
//			}
//			else
			if(!HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1)&&
				!HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2)&&
				!HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_3))
			{
				SetAction(ACTION_SLAYER_BLADE);
				SetBurningSol(0);
			}
			break;
		case EFFECTSTATUS_INSTALL_TURRET: // Gun Slayer 130 Install Turret
			{
				SetAction( ACTION_STAND );
				SetInstallTurretCount(0);
				SetInstallTurretDirect(2);
//				SetCreatureType( CREATURETYPE_INSTALL_TURRET );
//				SetGroundCreature();
//				
				SetDirection( 2 );
				SetCurrentDirection( 2 );
//				//--------------------------------------------------
//				// result of transforming into a wolf
//				//--------------------------------------------------
//				MActionResult* pResult = new MActionResult;
//
//				pResult->Add( new MActionResultNodeChangeCreatureType( GetID(), CREATURETYPE_INSTALL_TURRET ) );

//					
//				ExecuteActionInfoFromMainNode(
//					RESULT_SKILL_INSTALL_TURRET,										// skill number used
//				
//					GetX(), GetY(), 0,
//					GetDirection(),														// direction used
//					
//					OBJECTID_NULL,												// info about the target
//					GetX(), GetY(), 0, 
//					
//					0,													// (remaining) duration of the skill
//					
//					NULL, //NULL,
//					
//					false);			// start the skill from the beginning.
			}
			break;
			// 2004, 10, 25, sobeit add start
		case EFFECTSTATUS_DRAGON_EYES:
			delayFrame = 0xffff;
			break;
			// 2004, 10, 25, sobeit add end

		case EFFECTSTATUS_SET_AFIRE_ATTACK:
			delayFrame = 20;
			break;

		case EFFECTSTATUS_ICE_HORIZON:
				delayFrame = 22;
			break;
			
		case EFFECTSTATUS_FIERCE_FLAME:
			WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_FIERCE_FLAME,GetX(), GetY(), 0,GetDirection(),	GetID(),
					GetX(), GetY(), 0, delayFrame, NULL, false);
			break;
		// add by Coffee 2007-5-3 inserted
		case EFFECTSTATUS_DUMMY_DRAKE:
			WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_DUMMY_DRAKE,GetX(), GetY(), 0,GetDirection(),	GetID(),
					GetX(), GetY(), 0, delayFrame, NULL, false);
			break;
		case EFFECTSTATUS_HYDRO_CONVERGENCE: ////water magic
			WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_HYDRO_CONVERGENCE,GetX(), GetY(), 0,GetDirection(),	GetID(),
					GetX(), GetY(), 0, delayFrame, NULL, false);
			break;
		case EFFECTSTATUS_HETER_CHAKRAM: ////agile combat
			WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_SKILL_DUMMY_DRAKE,GetX(), GetY(), 0,GetDirection(),	GetID(),
					GetX(), GetY(), 0, delayFrame, NULL, false);
			break;	
		case EFFECTSTATUS_BLOOD_CURSE: // //vampire skill
			WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_BLOOD_CURSE,GetX(), GetY(), 0,GetDirection(),	GetID(),
					GetX(), GetY(), 0, delayFrame, NULL, false);
			break;
		// end 	
		case EFFECTSTATUS_PASSING_HEAL:
				delayFrame = 20;
			break;
		case EFFECTSTATUS_DONATION_200501:
			WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_CANDLE,GetX(), GetY(), 0,GetDirection(),	GetID(),
								GetX(), GetY(), 0, delayFrame, NULL, false);
			break;	
		default:
			break;
			
	}

	m_bEffectStatus[status] = true;

	bool bUseEffectSprite = (*g_pEffectStatusTable)[status].bUseEffectSprite;	

	//------------------------------------------------------------
	//
	//			when expressed with EffectSprite
	//
	//------------------------------------------------------------		
	if (bUseEffectSprite)
	{
		if( (status == EFFECTSTATUS_GRAND_MASTER_OUSTERS ||
			status == EFFECTSTATUS_GRAND_MASTER_SLAYER ||
			status == EFFECTSTATUS_GRAND_MASTER_VAMPIRE ) &&
			!g_pSystemAvailableManager->IsAvailableGrandMasterEffectSystem() )
			return false;

		DEBUG_ADD("[Use EffectSprite]");
		
		//TYPE_EFFECTSPRITETYPE type = (*g_pEffectStatusTable)[status].EffectSpriteType;

		if (type>=g_pEffectSpriteTypeTable->GetSize())//EFFECTSPRITETYPE_NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error]EffectSpriteType of EffectStatus exceed Max : %d", (int)type);
			
			return false;
		}

		if (IsFemale()
			&& (*g_pEffectSpriteTypeTable)[type].FemaleEffectSpriteType!=EFFECTSPRITETYPE_NULL)
		{
			type = (*g_pEffectSpriteTypeTable)[type].FemaleEffectSpriteType;
		}
		if (type >= g_pEffectSpriteTypeTable->GetSize() || m_bAttachEffect == NULL)
			return false;

//		//------------------------------------------------------------
//		// EFFECTSTATUS_ARMAGEDDON
//		//------------------------------------------------------------
//		if (status==EFFECTSTATUS_ARMAGEDDON)
//		{
//			type = GetArmageddonSprite( this );			
//		}

		TYPE_FRAMEID	frameID = (*g_pEffectSpriteTypeTable)[type].FrameID;

		if (frameID==FRAMEID_NULL)
		{
			DEBUG_ADD_ERR("[Error]FrameID of EffectSpriteType is NULL");
			
			return false;
		}

		//------------------------------------------------------------
		// when it already exists
		//------------------------------------------------------------
		if (m_bAttachEffect[type])
		{		
			ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();

			//------------------------------------------------------
			// attached to the body
			//------------------------------------------------------
			// find the same Effect.
			while (iEffect != m_listEffect.end())
			{
				MAttachEffect*	pEffect = *iEffect;
				
				// find the same type.
				if (pEffect->IsEffectSprite() && pEffect->GetEffectSpriteType() == type)
				{
					// extend the time only if the one being added ends later.
					if (g_CurrentFrame+delayFrame > pEffect->GetEndFrame()) 
					{
						pEffect->SetCount( delayFrame );
					}

					return true;
				}

				iEffect++;
			}

			//------------------------------------------------------
			// attached to the ground
			//------------------------------------------------------
			iEffect = m_listGroundEffect.begin();

			// find the same Effect.
			while (iEffect != m_listGroundEffect.end())
			{
				MAttachEffect*	pEffect = *iEffect;
				
				// find the same type.
				if (pEffect->IsEffectSprite() && pEffect->GetEffectSpriteType() == type)
				{
					// extend the time only if the one being added ends later.
					if (g_CurrentFrame+delayFrame > pEffect->GetEndFrame()) 
					{
						pEffect->SetCount( delayFrame );
					}

					return true;
				}

				iEffect++;
			}

			return false;
		}

		//------------------------------------------------------------
		// when it doesn't exist, add it.
		//------------------------------------------------------------
		MAttachEffect*	pEffect = NULL;
		
		if (eType==MEffect::EFFECT_ATTACH_ORBIT)
		{
			for(int i = 0; i < eCount; i++)
			{
				int effectPosition = 0;
				if( eCount > 1 )
				{
					effectPosition = (MAX_EFFECT_ORBIT_STEP/(eCount*2))*i;
				}
				else
				{
					effectPosition = -1;
				}
				pEffect = new MAttachOrbitEffect(type, delayFrame, orbit_type, effectPosition);
				
				pEffect->SetAttachCreature( this );
				
				// increase the count of glowing Effects?
				if (pEffect->GetBltType()==BLT_EFFECT)
				{
					// if it is the first glowing Effect added, add the sight.
					//if (m_nAlphaEffect==0)
					{
						pEffect->SetPosition( m_X, m_Y );
						//g_pZone->SetLight(m_X, m_Y, pEffect->GetLight());
					}
					
					//m_nAlphaEffect ++;		
				}
				
				//------------------------------------------------------------
				// if it is attached to the ground..
				//------------------------------------------------------------
				if ((*g_pEffectStatusTable)[status].bAttachGround)
				{
					m_listGroundEffect.push_back( pEffect );
				}
				//------------------------------------------------------------
				// attached to the body..
				//------------------------------------------------------------
				else
				{
					m_listEffect.push_back( pEffect );
				}
				
				m_bAttachEffect[type] = true;
				
			}
		}
		else
		{
			pEffect = new MAttachEffect(type, delayFrame);
			
			pEffect->SetAttachCreature( this );
			
			// increase the count of glowing Effects?
			if (pEffect->GetBltType()==BLT_EFFECT)
			{
				// if it is the first glowing Effect added, add the sight.
				//if (m_nAlphaEffect==0)
				{
					pEffect->SetPosition( m_X, m_Y );
					//g_pZone->SetLight(m_X, m_Y, pEffect->GetLight());
				}
				
				//m_nAlphaEffect ++;		
			}
			
			//------------------------------------------------------------
			// if it is attached to the ground..
			//------------------------------------------------------------
			if ((*g_pEffectStatusTable)[status].bAttachGround)
			{
				m_listGroundEffect.push_back( pEffect );
			}
			//------------------------------------------------------------
			// attached to the body..
			//------------------------------------------------------------
			else
			{
				m_listEffect.push_back( pEffect );
			}

			// Check array bounds before setting flag
			if (type >= (*g_pEffectSpriteTypeTable).GetSize()) {
				LOG_ERROR("[EFFECT ERROR] type=%d out of bounds! table size=%d",
				         type, (*g_pEffectSpriteTypeTable).GetSize());
			} else {
				m_bAttachEffect[type] = true;
			}
			
		}
	
	}
	//------------------------------------------------------------
	//
	//			when expressed with EffectColor
	//
	//------------------------------------------------------------
//	else
	if((*g_pEffectStatusTable)[status].EffectColor != 0xFFFF)
	{
		DEBUG_ADD("[Use EffectColor]");
		
		WORD colorSet = (*g_pEffectStatusTable)[status].EffectColor;

		if (colorSet>=MAX_COLORSET)
		{
			DEBUG_ADD_FORMAT_ERR("[Error]EffectColor of EffectStatus exceed Max : ", (int)colorSet);
			
			return false;
		}

		//------------------------------------------------------------
		// check whether it already exists.
		// since it must be checked by color.. the whole list must be searched. T_T;
		//------------------------------------------------------------
		DEBUG_ADD_FORMAT("listEffect size = %d", m_listEffect.size());
		
		ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();

		//----------------------------------------------------------------
		// attached to the body
		//----------------------------------------------------------------
		// find the same Effect.
		while (iEffect != m_listEffect.end())
		{
			MAttachEffect*	pEffect = *iEffect;
			
			// find the same color.
			if (pEffect->IsEffectColor() && pEffect->GetEffectColor() == colorSet)
			{
				// extend the time only if the one being added ends later.
				if (g_CurrentFrame+delayFrame > pEffect->GetEndFrame()) 
				{
					pEffect->SetCount( delayFrame );
				}

				return true;
			}

			iEffect++;
		}

		//----------------------------------------------------------------
		// attached to the ground
		//----------------------------------------------------------------
		// assume there is none.

		DEBUG_ADD("[Create New AttachEffect]");
		
		//------------------------------------------------------------
		// when it doesn't exist, add it.
		//------------------------------------------------------------
		MAttachEffect*	pEffect = new MAttachEffect(EFFECTSPRITETYPE_NULL, delayFrame);

		DEBUG_ADD("[Create New AttachEffect OK]");
		
		pEffect->SetAttachCreature( this );

		DEBUG_ADD("[Set AttachCreature]");
		
		// set color
		pEffect->SetEffectColor( colorSet );

		ADDON	part = (*g_pEffectStatusTable)[status].EffectColorPart;

		//------------------------------------------------------------
		// there are also cases where only a specific part changes color.
		//------------------------------------------------------------
		//if (part<ADDON_MAX)
		//{
			pEffect->SetEffectColorPart( part );
		//}

		DEBUG_ADD("[if (pEffect->GetBltType()==BLT_EFFECT)]");
		
		// increase the count of glowing Effects?
		if (pEffect->GetBltType()==BLT_EFFECT)
		{
			// if it is the first glowing Effect added, add the sight.
			//if (m_nAlphaEffect==0)
			{
				pEffect->SetPosition( m_X, m_Y );
				//g_pZone->SetLight(m_X, m_Y, pEffect->GetLight());
			}

			//m_nAlphaEffect ++;		
		}

		//------------------------------------------------------
		// things that change color are simply added to the ones attached to the body.
		//------------------------------------------------------
		m_listEffect.push_back( pEffect );				
	}

	DEBUG_ADD("[AddEffectStatus - OK]");
	
	return true;
}

//----------------------------------------------------------------------
// Set Name - set character name
//----------------------------------------------------------------------
void	
MCreature::SetName(const char* pName)
{
	if (m_pName!=NULL)
	{
		delete [] m_pName;
	}

	const size_t nName = strlen(pName) + 1;
	m_pName = new char [nName];

	memcpy(m_pName, pName, nName);
}

//----------------------------------------------------------------------
// Is Stop
//----------------------------------------------------------------------
BOOL	
MCreature::IsStop()
{ 
	return (m_MoveCount>=m_MoveCountMax 
			&& m_ActionCount>=GetActionCountMax()) 
			|| (m_Action==ACTION_STAND && m_MoveAction==ACTION_MOVE)
			|| (m_Action==ACTION_SLAYER_MOTOR_STAND && m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)
			|| (m_Action==ACTION_OUSTERS_FAST_MOVE_STAND && m_MoveAction==ACTION_OUSTERS_FAST_MOVE_MOVE);
}

//----------------------------------------------------------------------
// Set Server Position
//----------------------------------------------------------------------
void			
MCreature::SetServerPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)	
{ 
	//------------------------------------------------
	// erase all existing MoveBuffer
	//------------------------------------------------
	ReleaseMoveBuffer();

	SetServerPositionWithBlock(sX, sY);
}

//----------------------------------------------------------------------
// Is Slayer Character
//----------------------------------------------------------------------
// was the slayer image used?
//----------------------------------------------------------------------
bool			
MCreature::IsSlayerCharacter() const
{
	return (*g_pCreatureTable)[m_CreatureType].SpriteTypes[0] <= 1;
	//return m_CreatureType==CREATURETYPE_SLAYER_MALE
	//	||  m_CreatureType==CREATURETYPE_SLAYER_FEMALE;
}

//----------------------------------------------------------------------
// Decide the way of moving and the movement unit for it.
//----------------------------------------------------------------------
void		
MCreature::SetMoveDevice(MOVE_DEVICE md)
{	
	//--------------------------------------------------------
	// apply all
	//--------------------------------------------------------
	AffectMoveBufferAll();

	//--------------------------------------------------
	// can be changed only for Slayer.
	//--------------------------------------------------
	if (IsSlayer())
	{
		m_MoveDevice = md; 

//		SetAction(ACTION_STAND);
		switch (md)
		{
			case MOVE_DEVICE_WALK :
			{
				m_MoveAction = ACTION_MOVE;

				if (m_Action==ACTION_SLAYER_MOTOR_STAND)
				{
					SetAction( ACTION_STAND );
				}				
			}
			break;

			case MOVE_DEVICE_RIDE :
			{
				m_MoveAction = ACTION_SLAYER_MOTOR_MOVE;

				if (m_Action==ACTION_STAND)
				{
					SetAction( ACTION_SLAYER_MOTOR_STAND );
				}
			}
			break;

			default:
			break;
		}	
	}
	//--------------------------------------------------
	// otherwise..
	//--------------------------------------------------
	else if( IsOusters() )
	{
		m_MoveDevice = md; 
		if(m_MoveDevice==MOVE_DEVICE_SUMMON_SYLPH)
		{	
			m_MoveAction = ACTION_OUSTERS_FAST_MOVE_MOVE;
		}
		else
		{
			m_MoveAction = ACTION_MOVE;
		}
		
		SetAction(ACTION_STAND);
	} else
	{
		m_MoveDevice = MOVE_DEVICE_WALK;
		m_MoveAction = ACTION_MOVE;
	}
}

//----------------------------------------------------------------------
// Set the Creature's position and stop the movement it was doing.
//----------------------------------------------------------------------
void	
MCreature::SetPosition(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)	
{ 
	//------------------------------------------------
	// erase all existing MoveBuffer
	//------------------------------------------------
	ReleaseMoveBuffer();

	if (m_X!=SECTORPOSITION_NULL
		&& m_Y!=SECTORPOSITION_NULL)
	{
		MovePosition( x, y );
	}
	else
	{
		m_X	= x; 
		m_Y	= y; 	
	}

	m_sX = 0;
	m_sY = 0;
	m_ActionCount		= 0;
	m_ActionCountMax	= 0;
	m_MoveCount			= 0;
	m_MoveCountMax		= 0;

	CheckInDarkness();	
}

//----------------------------------------------------------------------
// Set Weapon Speed
//----------------------------------------------------------------------
void				
MCreature::SetWeaponSpeed(int speed)
{
	//-----------------------------------------------------------
	// bats and wolves have no separate attack speed..
	//-----------------------------------------------------------
	if (m_CreatureType==CREATURETYPE_BAT ||
		m_CreatureType==CREATURETYPE_WOLF ||
		m_CreatureType==CREATURETYPE_WER_WOLF)
	{
		m_WeaponSpeed = WEAPON_SPEED_NORMAL;
	}
	//-----------------------------------------------------------
	// calculate attack speed..
	//-----------------------------------------------------------
	else
	{
		if (speed <= 33)
		{
			m_WeaponSpeed = WEAPON_SPEED_SLOW;
		}
		else if (speed <= 66)
		{
			m_WeaponSpeed = WEAPON_SPEED_NORMAL;
		}
		else
		{
			m_WeaponSpeed = WEAPON_SPEED_FAST;
		}
	}
	if(GetClassType() == CLASS_PLAYER)
	{
		StatusPresentation::SetWeaponSpeed(speed, m_WeaponSpeed);
	}
}

//----------------------------------------------------------------------
// Make the Creature stop.
//----------------------------------------------------------------------
void
MCreature::SetStop()
{ 
	
	m_bFastMove = false;
	m_bKnockBack = 0;	

	//------------------------------------------------
	// erase all existing MoveBuffer
	//------------------------------------------------
	AffectMoveBufferAll();
	//ReleaseMoveBuffer();

	// 2001.11.8 - make sure the coordinates are corrected when stopping..
	ActionMoveNextPosition();

	// stop Action
	m_sX=0; 
	m_sY=0;

	int action = (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_MOVE : ACTION_STAND;
	if( IsOusters() && HasEffectStatusSummonSylph( this ))
	{
		if (action==ACTION_STAND)
		{
			action = ACTION_OUSTERS_FAST_MOVE_STAND;
		} 
		else if( action == ACTION_MOVE )
		{
			action = ACTION_OUSTERS_FAST_MOVE_MOVE;
		}
	}
	m_Action		= action;
	// 2004, 11, 3, sobeit modify start - set m_ActionCount to the max count of the changed action - because of the Ousters ghost bug..-_-;
	m_ActionCount	= GetActionCountMax(); 
	//m_ActionCount	= (*g_pCreatureTable)[m_CreatureType].GetActionCount( m_Action );
	// 2004, 11, 3, sobeit modify end
	m_MoveCount		= m_MoveCountMax;

	// also remove the next action
	m_bNextAction = false;
	m_NextX = SECTORPOSITION_NULL;
	m_NextY = SECTORPOSITION_NULL;

	
	//m_nNextUsedActionInfo = ACTIONINFO_NULL;
}

//----------------------------------------------------------------------
// Draw-phase interpolation (60 fps rendering between 62 ms logic ticks)
//----------------------------------------------------------------------
// g_DrawAlphaNum is how far the wall clock has advanced into the current
// 62 ms tick, as a 0..256 fixed-point fraction. g_bInterpolateDraw is set
// only around the world draw in CGameUpdate::UpdateDraw, so game logic
// always reads exact tick positions. The clock is FrameClock's (task 4.240);
// the flag is MObjectScreen.cpp's.
extern bool		g_bInterpolateDraw;

// A creature never moves more than a tile's worth of pixels per tick; a
// larger gap means teleport/respawn and must snap, not glide.
static const int DRAW_LERP_SNAP_DISTANCE = 96;

//----------------------------------------------------------------------
// Snapshot Draw State
//----------------------------------------------------------------------
// Called once per logic tick from MZone::UpdateAllCreature, before this
// creature's Action() moves it. Stores the pre-tick pixel position that
// GetPixelX/Y interpolate from during the draw phase.
//----------------------------------------------------------------------
void
MCreature::SnapshotDrawState()
{
	m_PrevPixelX = WorldTileGeometry::TileToPixelX(m_X) + m_sX;
	m_PrevPixelY = WorldTileGeometry::TileToPixelY(m_Y) + m_sY;
	m_DrawSnapFrame = g_CurrentFrame;
}

//----------------------------------------------------------------------
// Get PixelX - PixelX coordinate of the Creature
//----------------------------------------------------------------------
int
MCreature::GetPixelX() const
{
	int cur = WorldTileGeometry::TileToPixelX(m_X) + m_sX;

	if (!g_bInterpolateDraw || m_DrawSnapFrame != g_CurrentFrame)
		return cur;

	int d = cur - m_PrevPixelX;
	if (d > DRAW_LERP_SNAP_DISTANCE || d < -DRAW_LERP_SNAP_DISTANCE)
		return cur;

	return m_PrevPixelX + (int)(((LONGLONG)d * (LONGLONG)g_DrawAlphaNum) >> 8);
}

//----------------------------------------------------------------------
// Get PixelY - PixelY coordinate of the Creature
//----------------------------------------------------------------------
int
MCreature::GetPixelY() const
{
	int cur = WorldTileGeometry::TileToPixelY(m_Y) + m_sY;// + ((m_MoveType == CREATURE_FLYING)<<2);

	if (!g_bInterpolateDraw || m_DrawSnapFrame != g_CurrentFrame)
		return cur;

	int d = cur - m_PrevPixelY;
	if (d > DRAW_LERP_SNAP_DISTANCE || d < -DRAW_LERP_SNAP_DISTANCE)
		return cur;

	return m_PrevPixelY + (int)(((LONGLONG)d * (LONGLONG)g_DrawAlphaNum) >> 8);
}

//----------------------------------------------------------------------
// Get Draw Gap - interpolated minus exact pixel position
//----------------------------------------------------------------------
// The camera in MTopView::DrawZone follows the player's exact position, so
// UpdateDraw biases it by this gap (through the Draw(x, y) offset
// parameters) to scroll smoothly between ticks.
//----------------------------------------------------------------------
int
MCreature::GetDrawGapX() const
{
	if (m_DrawSnapFrame != g_CurrentFrame)
		return 0;

	int cur = WorldTileGeometry::TileToPixelX(m_X) + m_sX;
	int d = cur - m_PrevPixelX;
	if (d > DRAW_LERP_SNAP_DISTANCE || d < -DRAW_LERP_SNAP_DISTANCE)
		return 0;

	return (int)(((LONGLONG)d * (LONGLONG)g_DrawAlphaNum) >> 8) - d;
}

int
MCreature::GetDrawGapY() const
{
	if (m_DrawSnapFrame != g_CurrentFrame)
		return 0;

	int cur = WorldTileGeometry::TileToPixelY(m_Y) + m_sY;
	int d = cur - m_PrevPixelY;
	if (d > DRAW_LERP_SNAP_DISTANCE || d < -DRAW_LERP_SNAP_DISTANCE)
		return 0;

	return (int)(((LONGLONG)d * (LONGLONG)g_DrawAlphaNum) >> 8) - d;
}

//----------------------------------------------------------------------
// Set Level Name
//----------------------------------------------------------------------
void				
MCreature::SetLevelName(int ln)
{
	(void)ln;
	m_NameSelection.SelectLevelName(*g_pLevelNameTable, rand());
}

//----------------------------------------------------------------------
// Get Level Name
//----------------------------------------------------------------------
const char*			
MCreature::GetLevelName() const
{
	return m_NameSelection.GetLevelName(*g_pLevelNameTable);
}

//----------------------------------------------------------------------
// Move Position
//----------------------------------------------------------------------
// in the Zone, at the (m_X, m_Y) Sector
// must move this Creature to (x, y).
//----------------------------------------------------------------------
bool
MCreature::MovePosition(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	if (m_pZone==NULL)
	{
		return false;
	}

	// differs depending on Creature Type.
	switch (m_MoveType)
	{
		case CREATURE_UNDERGROUND : 
			if (ZoneState::MoveUndergroundCreatureIn(m_pZone, this, m_X, m_Y, x, y))
			{
				// when there is an Effect

				// if moved, set the new coordinate
				m_X	= x;
				m_Y	= y;		
				
				CheckInDarkness();
				
				return true;
			}
		break;

		case CREATURE_GROUND : 
			if (ZoneState::MoveGroundCreatureIn(m_pZone, this, m_X, m_Y, x, y))
			{
				// when there is an Effect

				// if moved, set the new coordinate
				m_X	= x;
				m_Y	= y;

				CheckInDarkness();

				return true;
			}
		break;

		case CREATURE_FLYING : 
			if (ZoneState::MoveFlyingCreatureIn(m_pZone, this, m_X, m_Y, x, y))
			{
				// when there is an Effect

				// if moved, set the new coordinate
				m_X	= x;
				m_Y	= y;

				CheckInDarkness();

				return true;
			}
		break;

		// fake just moves.
		case CREATURE_FAKE_NO_BLOCK :
		case CREATURE_FAKE_UNDERGROUND :
		case CREATURE_FAKE_GROUND :
		case CREATURE_FAKE_FLYING :
			m_X = x;
			m_Y = y;
		return true;
		break;
	}

	return false;
}

//----------------------------------------------------------------------
// Step the supplied sector position in the selected direction.
//----------------------------------------------------------------------
void
MCreature::GetPositionToDirection(TYPE_SECTORPOSITION &x, TYPE_SECTORPOSITION &y, BYTE direction)
{
	WorldTileGeometry::Step(x, y, direction);
}

//----------------------------------------------------------------------
// Add Effect
//----------------------------------------------------------------------
// Expression of Effects shown attached to a Creature
//
// since stacking is not allowed..
// only add Effects that are not yet attached.
//
// However, there are cases where the time must be extended.
//----------------------------------------------------------------------
MAttachEffect*		
MCreature::CreateAttachEffect(TYPE_EFFECTSPRITETYPE type, 
							  DWORD delayFrame, 
							  DWORD linkDelayFrame, 
							  BOOL bGroundEffect,
							  MEffect::EFFECT_TYPE eType)
{
	DEBUG_ADD_FORMAT("CreateAttachEffect. type=%d, delayf=%d", type, delayFrame);

	// when dead... remove the delay.
	if (!m_bAlive)
	{
		// - -;;
		delayFrame = min( DWORD(10), delayFrame );
	}
	bool	IsMulti = false;
	int		orbitType = 0;
	// 2004, 08, 05 sobeit add start - castle gate, guild tower debris splash
	if(GetCreatureType() >= 726 && GetCreatureType() <= 729)
	{	
		if(type < EFFECTSPRITETYPE_CASTLE_GATE_DUST_1 ||
		   type > EFFECTSPRITETYPE_CASTLE_GATE_DAMAGED) // if it is not a castle gate effect, always splash debris..haha
		{
			int currentHP		= max(0,int(GetHP()));
			int frame = 4 -(currentHP*5/GetMAX_HP());
			
			if(m_NickNameType != frame) // not keen on adding a variable.. the castle gate doesn't use m_NickNameType, so use it here temporarily -_-;
			{
				m_NickNameType = frame;
				if(frame<1) frame = 1;
				type = EFFECTSPRITETYPE_CASTLE_GATE_DUST_1 + rand()%(5-frame);
				delayFrame = 32;
			}
			else
			{
				if(frame<1) frame = 1;
				type = EFFECTSPRITETYPE_CASTLE_GATE_PIECE_1 + rand()%(5-frame);
				delayFrame = 14;
			}
			// it is a castle gate hit effect.. better without it..heh
//			ExecuteActionInfoFromMainNode(SKILL_CLIENT_CASTLE_GATE_DAMAGED,GetX(), GetY(), 0,GetDirection(),	GetID(),	
//				GetX(), GetY(), 0, 8, NULL, false);
		}
		else
		{
			if(type == EFFECTSPRITETYPE_CASTLE_GATE_LARGE_DUST)
				delayFrame = 44;
			else if(type == EFFECTSPRITETYPE_CASTLE_GATE_DAMAGED)
				delayFrame = 8;
			else 
				delayFrame = 32;
		}
	}
	else if(GetCreatureType() == 734) // guild tower
	{
		if(type < EFFECTSPRITETYPE_CASTLE_GATE_DAMAGED ||
		   type > EFFECTSPRITETYPE_GUILD_TOWER_PROTECT) // if it is not a guild tower effect, always splash debris..haha
		{
			type = EFFECTSPRITETYPE_CASTLE_GATE_DAMAGED;
			delayFrame = 8;
//			ExecuteActionInfoFromMainNode(SKILL_CLIENT_GUILD_TOWER_PROTECT,GetX(), GetY(), 0,GetDirection(),	GetID(),	
//				GetX(), GetY(), 0, 32, NULL, false);
		}
		else
		{
			delayFrame = 32;
		}
	}
//	else if(GetCreatureType() == 723) // awakened Zildre
//	{
//		if(type == EFFECTSPRITETYPE_GDR_DEAD)
//			delayFrame = 218;
//
//	}
	else
	{
//		if(GetCreatureType() == 734&&type == EFFECTSPRITETYPE_GUILD_TOWER_DESTROY) // guild tower
//			delayFrame = 32;
	// 2004, 08, 05 sobeit add end - castle gate, guild tower debris splash

		//------------------------------------------------
		// for blood, randomize the position
		//------------------------------------------------
		if (type==EFFECTSPRITETYPE_BLOOD_GUN_1_1
			|| type==EFFECTSPRITETYPE_BLOOD_GUN_2_1
			|| type==EFFECTSPRITETYPE_BLOOD_GUN_3_1
			|| type==EFFECTSPRITETYPE_GREEN_BLOOD_GUN_1_1
			|| type==EFFECTSPRITETYPE_GREEN_BLOOD_GUN_2_1
			|| type==EFFECTSPRITETYPE_GREEN_BLOOD_GUN_3_1)
		{
			// depending on height.. if over 100... (golem)
			if ((*g_pCreatureTable)[m_CreatureType].Height > 100)
			{
				// type + 0~6
				type += rand()%7;
			}
			else
			{
				// type + 1~6
				type += rand()%6 + 1;
			}		
		}

		//------------------------------------------------
		// for blood, randomize the position
		//------------------------------------------------
		if (type==EFFECTSPRITETYPE_BULLET_OF_LIGHT_START_1)
		{
			// depending on height.. if over 100... (golem)
			if ((*g_pCreatureTable)[m_CreatureType].Height > 100)
			{
				// type + 0~6
				type += (rand()%7)*2;
			}
			else
			{
				// type + 1~6
				type += (rand()%6)*2;
			}		
		}
		//------------------------------------------------
		// blood from a sword hit (vertical)
		//------------------------------------------------
		if (type==EFFECTSPRITETYPE_BLOOD_VERTICAL_1 
			|| type==EFFECTSPRITETYPE_BLOOD_HORIZONTAL_1)
		{
			if (rand()%2)
			{
				type = EFFECTSPRITETYPE_BLOOD_VERTICAL_1 + rand()%4;
			}
			else
			{
				type = EFFECTSPRITETYPE_BLOOD_HORIZONTAL_1 + rand()%3;
			}		
		}
		if (type==EFFECTSPRITETYPE_GREEN_BLOOD_VERTICAL_1 
			|| type==EFFECTSPRITETYPE_GREEN_BLOOD_HORIZONTAL_1)
		{
			if (rand()%2)
			{
				type = EFFECTSPRITETYPE_GREEN_BLOOD_VERTICAL_1 + rand()%4;
			}
			else
			{
				type = EFFECTSPRITETYPE_GREEN_BLOOD_HORIZONTAL_1 + rand()%3;
			}		
		}
		//------------------------------------------------
		// case of smoke from being hit by acid
		//------------------------------------------------
		else if (type==EFFECTSPRITETYPE_SMOKE)
		{
			type += rand()%5;
		}
		
		//------------------------------------------------
		// Typhoon back [new skill]
		//------------------------------------------------
		else if (type==EFFECTSPRITETYPE_TYPHOON_BACK)
		{	
			SetTurning( 16 );
		}


		if (type>=g_pEffectSpriteTypeTable->GetSize())//EFFECTSPRITETYPE_NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error]EffectSpriteType of EffectStatus exceed Max : ", (int)type);
			
			return NULL;
		}

		//------------------------------------------------
		// when using the female EffectSpriteType
		//------------------------------------------------
		if (IsFemale()
			&& (*g_pEffectSpriteTypeTable)[type].FemaleEffectSpriteType!=EFFECTSPRITETYPE_NULL)
		{
			type = (*g_pEffectSpriteTypeTable)[type].FemaleEffectSpriteType;

			if (type>=g_pEffectSpriteTypeTable->GetSize())//EFFECTSPRITETYPE_NULL)
			{
				DEBUG_ADD_FORMAT_ERR("[Error]EffectSpriteType of EffectStatus exceed Max : ", (int)type);
				
				return NULL;
			}
		}
	

		// distinguish effects differently by attack speed.
		switch( type )
		{
		case EFFECTSPRITETYPE_FLOURISH_NORMAL :
		case EFFECTSPRITETYPE_FLOURISH_NORMAL_2 :
		case EFFECTSPRITETYPE_FLOURISH_NORMAL_3 :
		case EFFECTSPRITETYPE_NEW_BLOODY_NAIL_FEMALE_NORMAL :
		case EFFECTSPRITETYPE_NEW_ACID_TOUCH_FEMALE_NORMAL :	
		case EFFECTSPRITETYPE_NEW_BLOODY_NAIL_MALE_NORMAL :
		case EFFECTSPRITETYPE_NEW_ACID_TOUCH_MALE_NORMAL :	
		case EFFECTSPRITETYPE_REDIANCE_MALE_NORMAL_ATTACK_NORMAL :
		case EFFECTSPRITETYPE_REDIANCE_FEMALE_NORMAL_ATTACK_NORMAL :
		case EFFECTSPRITETYPE_TALON_OF_CROW_MALE_NORMAL :
		case EFFECTSPRITETYPE_TALON_OF_CROW_FEMALE_NORMAL :
			IsMulti = true;
			switch( GetWeaponSpeed() )
			{
			case WEAPON_SPEED_SLOW :
				delayFrame = 15;
				++type;
				break;
			case WEAPON_SPEED_NORMAL :
				delayFrame = 13;
				break;
			case WEAPON_SPEED_FAST :
				delayFrame = 11;
				--type;			
				break;
			}
			break;
		case EFFECTSPRITETYPE_REDIANCE_MALE_NORMAL :
		case EFFECTSPRITETYPE_REDIANCE_FEMALE_NORMAL :
			
	//	case EFFECTSPRITETYPE_THROW_CHACRAM_21:
	//	case EFFECTSPRITETYPE_THROW_CHACRAM_23:
	//	case EFFECTSPRITETYPE_THROW_CHACRAM_25:
			IsMulti = true;
			switch( GetWeaponSpeed() )
			{
			case WEAPON_SPEED_SLOW :
				delayFrame = 25;
				++type;
				break;
			case WEAPON_SPEED_NORMAL :
				delayFrame = 23;
				break;
			case WEAPON_SPEED_FAST :
				delayFrame = 21;
				--type;			
				break;
			}
	//		if(type == EFFECTSPRITETYPE_THROW_CHACRAM_23
	//			|| type == EFFECTSPRITETYPE_THROW_CHACRAM_21
	//			|| type == EFFECTSPRITETYPE_THROW_CHACRAM_25)
	//		{
	//			SetGhost(2, delayFrame-2);
	//			IsMulti = true;
	//		}
			break;

		case EFFECTSPRITETYPE_THROW_CHACRAM_21:
		case EFFECTSPRITETYPE_THROW_CHACRAM_23:
		case EFFECTSPRITETYPE_THROW_CHACRAM_25:
			IsMulti = true;
			switch( GetWeaponSpeed() )
			{
			case WEAPON_SPEED_SLOW :
				delayFrame = 25;
				type = EFFECTSPRITETYPE_THROW_CHACRAM_25;
				break;
			case WEAPON_SPEED_NORMAL :
				delayFrame = 23;
				type = EFFECTSPRITETYPE_THROW_CHACRAM_23;
				break;
			case WEAPON_SPEED_FAST :
				delayFrame = 21;
				type = EFFECTSPRITETYPE_THROW_CHACRAM_21;			
				break;
			}
			//SetGhost(2, delayFrame);
			break;

		case EFFECTSPRITETYPE_THROW_CHACRAM_21_MASTERY:
		case EFFECTSPRITETYPE_THROW_CHACRAM_23_MASTERY:
		case EFFECTSPRITETYPE_THROW_CHACRAM_25_MASTERY:
			IsMulti = true;
			switch( GetWeaponSpeed() )
			{
			case WEAPON_SPEED_SLOW :
				delayFrame = 25;
				type = EFFECTSPRITETYPE_THROW_CHACRAM_25_MASTERY;
				break;
			case WEAPON_SPEED_NORMAL :
				delayFrame = 23;
				type = EFFECTSPRITETYPE_THROW_CHACRAM_23_MASTERY;
				break;
			case WEAPON_SPEED_FAST :
				delayFrame = 21;
				type = EFFECTSPRITETYPE_THROW_CHACRAM_21_MASTERY;
				break;
			}
			SetGhost(2, delayFrame);
			break;

		
		case EFFECTSPRITETYPE_DISTANCE_BLITZ_12:
		case EFFECTSPRITETYPE_DISTANCE_BLITZ_3:
		case EFFECTSPRITETYPE_DISTANCE_BLITZ_4:
		case EFFECTSPRITETYPE_DISTANCE_BLITZ_5:
		case EFFECTSPRITETYPE_DISTANCE_BLITZ_6:
//		case EFFECTSPRITETYPE_THROW_CHACRAM_21:
//		case EFFECTSPRITETYPE_THROW_CHACRAM_23:
//		case EFFECTSPRITETYPE_THROW_CHACRAM_25:
//		case EFFECTSPRITETYPE_THROW_CHACRAM_21_MASTERY:
//		case EFFECTSPRITETYPE_THROW_CHACRAM_23_MASTERY:
//		case EFFECTSPRITETYPE_THROW_CHACRAM_25_MASTERY:
			{
				delayFrame = 25;
				SetGhost(2, delayFrame-2);
				IsMulti = true;
			}
			break;

		case EFFECTSPRITETYPE_PIERCING_FRONT :
		case EFFECTSPRITETYPE_PIERCING_BACK :
		case EFFECTSPRITETYPE_JABBING_VEIN_REAR_1 :
		case EFFECTSPRITETYPE_JABBING_VEIN_REAR_2 :
		case EFFECTSPRITETYPE_JABBING_VEIN_REAR_3 :
		case EFFECTSPRITETYPE_JABBING_VEIN_REAR_4 :
		case EFFECTSPRITETYPE_JABBING_VEIN_FRONT_1 :
		case EFFECTSPRITETYPE_JABBING_VEIN_FRONT_2 :
		case EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3 :
		case EFFECTSPRITETYPE_JABBING_VEIN_FRONT_4 :

		case EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW:
		case EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW_SHADOW:
			IsMulti = true;
			break;

		case EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL:
		case EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL_ATTACK:
		case EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL:
		case EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL_HEAL:
			orbitType = 2;
			break;
		
		}

		if( type >= EFFECTSPRITETYPE_LAR_SLASH_MALE_FAST && type <= EFFECTSPRITETYPE_LAR_SLASH_FEMALE_SLOW )
			delayFrame = 0xFFFF;
	}	
	TYPE_FRAMEID	frameID = (*g_pEffectSpriteTypeTable)[type].FrameID;

	if (frameID==FRAMEID_NULL)
	{
		DEBUG_ADD_ERR("[Error]FrameID of EffectSpriteType is NULL");
		
		return NULL;
	}


	//------------------------------
	// when it already exists
	//------------------------------
	if (m_bAttachEffect[type] && !IsMulti)
	{	
		//------------------------------------------------------
		// check the ones attached to the body
		//------------------------------------------------------
		ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();

		// find the same Effect.
		while (iEffect != m_listEffect.end())
		{
			MAttachEffect*	pEffect = *iEffect;
			
			// find the same type.
			if (pEffect->IsEffectSprite() && pEffect->GetEffectSpriteType() == type)
			{
				// extend the time only if the one being added ends later.
				if (g_CurrentFrame+delayFrame > pEffect->GetEndFrame()) 
				{
					pEffect->SetCount( delayFrame );
				}

				DEBUG_ADD("find same AttachEffect");
				return pEffect;
			}

			iEffect++;
		}

		//------------------------------------------------------
		// check the ones attached to the ground
		//------------------------------------------------------
		iEffect = m_listGroundEffect.begin();

		// find the same Effect.
		while (iEffect != m_listGroundEffect.end())
		{
			MAttachEffect*	pEffect = *iEffect;
			
			// find the same type.
			if (pEffect->IsEffectSprite() && pEffect->GetEffectSpriteType() == type)
			{
				// extend the time only if the one being added ends later.
				if (g_CurrentFrame+delayFrame > pEffect->GetEndFrame()) 
				{
					pEffect->SetCount( delayFrame );
				}

				DEBUG_ADD("find same GroundAttachEffect");
				return pEffect;
			}

			iEffect++;
		}

		return NULL;
	}

	//------------------------------
	// when it doesn't exist, add it.
	//------------------------------
	MAttachEffect*	pEffect = NULL;
	
	if (eType==MEffect::EFFECT_ATTACH_ORBIT)
	{
		pEffect = new MAttachOrbitEffect(type, delayFrame, orbitType, 0, linkDelayFrame);
	}
	else	// EFFECT_ATTACH
	{
		pEffect = new MAttachEffect(type, delayFrame, linkDelayFrame);
	}

	pEffect->SetAttachCreatureID( m_ID );

	// increase the count of glowing Effects?
	if (pEffect->GetBltType()==BLT_EFFECT)
	{
		// if it is the first glowing Effect added, add the sight.
		//if (m_nAlphaEffect==0)
		{
			pEffect->SetPosition( m_X, m_Y );
			//g_pZone->SetLight(m_X, m_Y, pEffect->GetLight());
		}

		//m_nAlphaEffect ++;		
	}

	//------------------------------------------------------
	// attached to the ground
	//------------------------------------------------------
	if (bGroundEffect)
	{
		m_listGroundEffect.push_back( pEffect );
	}
	//------------------------------------------------------
	// attached to the body
	//------------------------------------------------------
	else
	{
		m_listEffect.push_back( pEffect );
	}

	m_bAttachEffect[type] = true;

	// Set LightSight
	DEBUG_ADD_FORMAT("NewEffect : Type=%d, Light=%d, (%d, %d)", pEffect->GetFrameID(), pEffect->GetLight(), pEffect->GetX(), pEffect->GetY());			
	
	return pEffect;
}

//----------------------------------------------------------------------
// Update Effect
//----------------------------------------------------------------------
// Change the Frame of all Effects attached to the Creature, and..
// if any have ended, delete them from the list and memory.
//----------------------------------------------------------------------
void		
MCreature::UpdateAttachEffect()
{
	ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();
	ATTACHEFFECT_LIST::iterator iEffectTemp;

	// brightness is 0
	//m_MaxEffectLight = 0;

	//---------------------------------------------------------------------
	// color that changes the character color..
	//---------------------------------------------------------------------
	// it is a NULL value.. must be checked from outside..
	m_AttachEffectColor = m_ChangeColorSet;	//ATTACHEFFECTCOLOR_NULL;
	int bShowColor =  (HasEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS)
						|| g_CurrentFrame % g_pClientConfig->FRAME_DRAW_ORIGINAL_SPRITE);	// show the original color once every few frames.
	int numColors = 0;
	
	//---------------------------------------------------------------------
	// Update all Effects.
	//---------------------------------------------------------------------
	while (iEffect != m_listEffect.end())
	{
		MAttachEffect*	pEffect = *iEffect;
		
		int x		= pEffect->GetX();
		int y		= pEffect->GetY();
		int light = pEffect->GetLight();		
		bool		bErase = false;

		//---------------------------------------------------------------------
		// Update
		//---------------------------------------------------------------------
		// message : find path
		//#ifdef	OUTPUT_DEBUG
			//sprintf(g_pDebugMessage->GetCurrent(), "Before UpdateEffect : Type=%d", pEffect->GetFrameID());	
			//g_pDebugMessage->Next();
		//#endif


		// for fast movement the move and stop effects differ.... hard-coded..-_-;
		if( pEffect->GetEffectSpriteType() == EFFECTSPRITETYPE_FAST_MOVE_FLY ||
			pEffect->GetEffectSpriteType() == EFFECTSPRITETYPE_FAST_MOVE_STOP ||
			(
			pEffect->GetEffectSpriteType() == EFFECTSPRITETYPE_SUMMON_SYLPH_GREEN_FLY &&
			pEffect->GetEffectSpriteType() == EFFECTSPRITETYPE_SUMMON_SYLPH_BLACK_STOP
			)
			)
		{		
			BLT_TYPE		bltType = (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_FAST_MOVE_STOP].BltType;
			TYPE_EFFECTSPRITETYPE		EffectSpriteType = pEffect->GetEffectSpriteType();
			bool			bNewSummonSylphEffect = false;
			bool			bNewSummonSylphEffectStop = false;
			
			if( EffectSpriteType >= EFFECTSPRITETYPE_SUMMON_SYLPH_GREEN_FLY &&
				EffectSpriteType <= EFFECTSPRITETYPE_SUMMON_SYLPH_BLACK_STOP )
			{
				bNewSummonSylphEffect = true;

				if( (EffectSpriteType - EFFECTSPRITETYPE_SUMMON_SYLPH_GREEN_FLY) & 0x1 )
					bNewSummonSylphEffectStop = true;
			}
			
			if( bNewSummonSylphEffect && !bNewSummonSylphEffectStop )
				bltType = (*g_pEffectSpriteTypeTable)[EffectSpriteType + 1].BltType;

			if( GetAction() == ACTION_OUSTERS_FAST_MOVE_STAND && (
				EffectSpriteType != EFFECTSPRITETYPE_FAST_MOVE_STOP ||
				( bNewSummonSylphEffect && !bNewSummonSylphEffectStop )
				)
				)
			{
				TYPE_FRAMEID	frameID	= (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_FAST_MOVE_STOP].FrameID;
				
				if( bNewSummonSylphEffect )
					frameID = (*g_pEffectSpriteTypeTable)[ EffectSpriteType + 1 ].FrameID;

				int maxFrame = WorldEvents::GetMaxEffectFrame(bltType, frameID);
			
				pEffect->SetEffectSprite( EFFECTSPRITETYPE_FAST_MOVE_STOP );
				if( bNewSummonSylphEffect )
					pEffect->SetEffectSprite( EffectSpriteType + 1 );
				pEffect->SetFrameID( frameID, maxFrame );
			}
			else
			if( GetAction() == ACTION_OUSTERS_FAST_MOVE_MOVE && 
				( pEffect->GetEffectSpriteType() != EFFECTSPRITETYPE_FAST_MOVE_FLY ||
				( bNewSummonSylphEffect && bNewSummonSylphEffectStop )
			  )
			  )
			{
				TYPE_FRAMEID	frameID	= (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_FAST_MOVE_FLY].FrameID;
				if( bNewSummonSylphEffect )
					frameID = (*g_pEffectSpriteTypeTable)[ EffectSpriteType - 1 ].FrameID;

				int maxFrame = WorldEvents::GetMaxEffectFrame(bltType, frameID);

				pEffect->SetEffectSprite( EFFECTSPRITETYPE_FAST_MOVE_FLY );

				if( bNewSummonSylphEffect )
					pEffect->SetEffectSprite( EffectSpriteType - 1 );
				pEffect->SetFrameID( frameID, maxFrame );
			}
		}
		else if( pEffect->GetEffectSpriteType() >= EFFECTSPRITETYPE_LAR_SLASH_MALE_FAST && pEffect->GetEffectSpriteType() <= EFFECTSPRITETYPE_LAR_SLASH_FEMALE_SLOW )
		{
			if(!(GetAction() == ACTION_SLAYER_SWORD_2 || GetAction() == ACTION_SLAYER_SWORD_2_SLOW || GetAction() == ACTION_SLAYER_SWORD_2_FAST ) 
				&&	m_RepeatCount <= 0 )
			{
				pEffect->SetCount(0);
			}
		}
		else if (pEffect->GetEffectSpriteType() >= EFFECTSPRITETYPE_REGEN_TOWER_THROW_LIGHT_START &&
			pEffect->GetEffectSpriteType() <= EFFECTSPRITETYPE_REGEN_TOWER_THROW_LIGHT_END &&
			!HasEffectStatus( EFFECTSTATUS_TRYING )
			)
		{
			bErase = true;
		}
		

		if (bErase == false && pEffect->Update())
		{
			// do not change direction in the case of soul absorption.
			if( pEffect->GetEffectSpriteType() != EFFECTSPRITETYPE_ABSORB_SOUL &&
				pEffect->GetEffectSpriteType() != EFFECTSPRITETYPE_PIERCING_FRONT &&
				pEffect->GetEffectSpriteType() != EFFECTSPRITETYPE_PIERCING_BACK )
				pEffect->SetDirection( m_CurrentDirection );

			// save the brightness of the Effect with the highest brightness.
//			if (m_MaxEffectLight < pEffect->GetLight())
//			{
//				m_MaxEffectLight = pEffect->GetLight();
//			}

			// set position
			pEffect->SetPosition(m_X, m_Y);

			//---------------------------------------------------------------------
			// the position changed, or
			// when the light size (sight) changed
			//---------------------------------------------------------------------
			if (x!=pEffect->GetX() || y!=pEffect->GetY()
				|| light != pEffect->GetLight())
			{				
				// change sight
//				g_pZone->UnSetLight(x, y, light);
//				g_pZone->SetLight(pEffect->GetX(), pEffect->GetY(), pEffect->GetLight());

			}

			//---------------------------------------------------------------------
			// if it is an Effect that changes the character color
			// must choose one.
			//---------------------------------------------------------------------
			if (bShowColor && pEffect->IsEffectColor())
			{
				numColors++;
				//if (m_AttachEffectColor == ATTACHEFFECTCOLOR_NULL || rand()%numColors==0)
				if (m_AttachEffectColor == m_ChangeColorSet || rand()%numColors==0)
				{
					m_AttachEffectColor = pEffect->GetEffectColor();
				}
			}

			//-----------------------------------------------
			//
			// Before this Effect ends, by LinkCount
			// if there is a next linked Effect, it must be created.
			//
			// when the current Frame has passed EndLinkFrame
			//
			//-----------------------------------------------
			if (g_CurrentFrame >= pEffect->GetEndLinkFrame()
				&& pEffect->GetLinkSize() != 0)
			{
				// in GenerateNext
				// because it sets the EffectTarget of pEffect to NULL
				// there is no need to delete it here.
				g_pEffectGeneratorTable->GenerateNext( pEffect );

				// pEffect must still exist, so do not delete it.
			}

			// proper case
			iEffect++;
		}
		//---------------------------------------------------------------------
		// when it ends because time ran out
		//---------------------------------------------------------------------
		else
		{
			
			bool bUseEffectSprite = pEffect->IsEffectSprite();

			// remove flag
			if (bUseEffectSprite)
			{
				m_bAttachEffect[pEffect->GetEffectSpriteType()] = false;	// remove flag
			}
		
			//---------------------------------------------------------------------
			//
			// if there is a next linked Effect, it must be created.
			//
			//---------------------------------------------------------------------
//			// 2004, 8, 16, sobeit add start test - Blade Slayer 130 skill
//			if(pEffect->GetEffectSpriteType() == EFFECTSPRITETYPE_BURNING_SOL_CHARGING)
//			{
//				// 	if still charging, the flying one must not appear..^^
//			}
//			// 2004, 8, 16, sobeit add end test - Blade Slayer 130 skill
			if (pEffect->GetLinkSize() != 0)
			{				
				(*g_pEffectGeneratorTable).GenerateNext( pEffect );
			}

			// if it is a glowing Effect, the sight must be removed.
			//if (pEffect->GetBltType()==BLT_EFFECT)
			{
				//m_nAlphaEffect --;

				// if all glowing Effects are gone, remove the sight.
				//if (m_nAlphaEffect==0)
				{
//					g_pZone->UnSetLight(x, y, light);

				}
			}

			DEBUG_ADD_FORMAT("[DeleteAttachEffect] id=%d, esType=%d", m_ID, pEffect->GetEffectSpriteType());
			
			// delete memory, delete list
			delete pEffect;						// remove memory
			
			// saved temporarily to delete from the list..
			iEffectTemp = iEffect;

			iEffect++;
			m_listEffect.erase( iEffectTemp );	// remove from list
		}		
	}


	//---------------------------------------------------------------------
	// effect attached to the ground
	//---------------------------------------------------------------------
	iEffect = m_listGroundEffect.begin();
	
	//---------------------------------------------------------------------
	// Update all Effects.
	//---------------------------------------------------------------------
	while (iEffect != m_listGroundEffect.end())
	{
		MAttachEffect*	pEffect = *iEffect;
		
		int x		= pEffect->GetX();
		int y		= pEffect->GetY();
		int light = pEffect->GetLight();		

		//---------------------------------------------------------------------
		// Update
		//---------------------------------------------------------------------
		// message : find path
		//#ifdef	OUTPUT_DEBUG
			//sprintf(g_pDebugMessage->GetCurrent(), "Before UpdateEffect : Type=%d", pEffect->GetFrameID());	
			//g_pDebugMessage->Next();
		//#endif

		if (pEffect->Update())
		{
			// save the brightness of the Effect with the highest brightness.
//			if (m_MaxEffectLight < pEffect->GetLight())
//			{
//				m_MaxEffectLight = pEffect->GetLight();
//			}

			// set position
			pEffect->SetPosition(m_X, m_Y);

			//---------------------------------------------------------------------
			// the position changed, or
			// when the light size (sight) changed
			//---------------------------------------------------------------------
			if (x!=pEffect->GetX() || y!=pEffect->GetY()
				|| light != pEffect->GetLight())
			{				
				// change sight
//				g_pZone->UnSetLight(x, y, light);
//				g_pZone->SetLight(pEffect->GetX(), pEffect->GetY(), pEffect->GetLight());

			}

			//---------------------------------------------------------------------
			// if it is an Effect that changes the character color
			// must choose one.
			//---------------------------------------------------------------------
			/*
			// ones laid on the ground have no color change.
			if (bShowColor && pEffect->IsEffectColor())
			{
				numColors++;
				if (m_AttachEffectColor == ATTACHEFFECTCOLOR_NULL || rand()%numColors==0)
				{
					m_AttachEffectColor = pEffect->GetEffectColor();
				}
			}
			*/
			//-----------------------------------------------
			//
			// Before this Effect ends, by LinkCount
			// if there is a next linked Effect, it must be created.
			//
			// when the current Frame has passed EndLinkFrame
			//
			//-----------------------------------------------
			if (g_CurrentFrame >= pEffect->GetEndLinkFrame()
				&& pEffect->GetLinkSize() != 0)
			{
				// in GenerateNext
				// because it sets the EffectTarget of pEffect to NULL
				// there is no need to delete it here.
				g_pEffectGeneratorTable->GenerateNext( pEffect );

				// pEffect must still exist, so do not delete it.
			}

			// proper case
			iEffect++;
		}
		//---------------------------------------------------------------------
		// when it ends because time ran out
		//---------------------------------------------------------------------
		else
		{
			bool bUseEffectSprite = pEffect->IsEffectSprite();

			// remove flag
			if (bUseEffectSprite)
			{
				m_bAttachEffect[pEffect->GetEffectSpriteType()] = false;	// remove flag
			}
		
			//---------------------------------------------------------------------
			//
			// if there is a next linked Effect, it must be created.
			//
			//---------------------------------------------------------------------
			if (pEffect->GetLinkSize() != 0)
			{				
				(*g_pEffectGeneratorTable).GenerateNext( pEffect );
			}

			// if it is a glowing Effect, the sight must be removed.
			//if (pEffect->GetBltType()==BLT_EFFECT)
			{
				//m_nAlphaEffect --;

				// if all glowing Effects are gone, remove the sight.
				//if (m_nAlphaEffect==0)
				{
//					g_pZone->UnSetLight(x, y, light);

				}
			}

			
			DEBUG_ADD_FORMAT("[DeleteAttachEffect] id=%d, esType=%d", m_ID, pEffect->GetEffectSpriteType());
			
			// delete memory, delete list
			delete pEffect;						// remove memory
			
			// saved temporarily to delete from the list..
			iEffectTemp = iEffect;

			iEffect++;
			m_listGroundEffect.erase( iEffectTemp );	// remove from list
		}		
	}
}

//----------------------------------------------------------------------
// UnSet LightSight AttachEffect
//----------------------------------------------------------------------
// Delete the sight of all Effects attached to the Creature.
//----------------------------------------------------------------------
/*
void		
MCreature::UnSetLightSightAttachEffect()
{
	ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();
	
	// Update all Effects.
	while (iEffect != m_listEffect.end())
	{
		MAttachEffect*	pEffect = *iEffect;
		
		// delete sight
//		g_pZone->UnSetLight(pEffect->GetX(), pEffect->GetY(), pEffect->GetLight());

		// next one
		iEffect++;
	}
}
*/

//----------------------------------------------------------------------
// Set LightSight AttachEffect
//----------------------------------------------------------------------
// Add the sight of all Effects attached to the Creature.
//----------------------------------------------------------------------
/*
void		
MCreature::SetLightSightAttachEffect()
{
	ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();
	
	// Update all Effects.
	while (iEffect != m_listEffect.end())
	{
		MAttachEffect*	pEffect = *iEffect;
		
		// delete sight
//		g_pZone->SetLight(pEffect->GetX(), pEffect->GetY(), pEffect->GetLight());

		// next one
		iEffect++;
	}
}
*/


//----------------------------------------------------------------------
// Set Dead
//----------------------------------------------------------------------
// Show the dying actionInfo and die.
//----------------------------------------------------------------------
void
MCreature::SetDead()
{
	// Kill only if alive. -_-;;
	if (m_bAlive)
	{	
//		if(HasEffectStatus(EFFECTSTATUS_HIDE_TO_ATTACKER))
//		{
//			if(GetCreatureType() == 738) // Pleasure Station
//				ExecuteActionInfoFromMainNode(SKILL_PLEASURE_EXPLOSION,GetX(), GetY(), 0,GetDirection(),	GetID(),	
//								GetX(), GetY(), 0, 20, NULL, false);			
//			else if(GetCreatureType() == 739) // Land Mine
//				ExecuteActionInfoFromMainNode(SKILL_LAND_MINE_EXPLOSION,GetX(), GetY(), 0,GetDirection(),	GetID(),	
//								GetX(), GetY(), 0, 0, NULL, false);	
//			else if(GetCreatureType() == 740) // Claymore
//				ExecuteActionInfoFromMainNode(SKILL_CLAYMORE_EXPLOSION,GetX(), GetY(), 0,GetDirection(),	GetID(),	
//								GetX(), GetY(), 0, 20, NULL, false);	
//		}
		//if(HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH ) )
		if(GetCreatureType() == 723) // awakened Zildre
			WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_GDR_DEAD, GetX(), GetY(), 0,GetDirection(),	GetID(),
								GetX(), GetY(), 0, 20, NULL, false);			

		if(HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
		{
			RemoveEffectStatus(EFFECTSTATUS_INSTALL_TURRET);
			SetAction( ACTION_STAND );
		}

		if( HasEffectStatusSummonSylph( this ) )
		{
			RemoveEffectStatusSummonSylph( this );
			SetAction( ACTION_STAND );
		}
		if(HasEffectStatus(EFFECTSTATUS_GLACIER))
		{ 
			RemoveEffectStatus( EFFECTSTATUS_GLACIER );
			SetAction(ACTION_STAND);
			ClearAttachEffect();
		}
		StopDrain();		
		StopAbsorb();

		//-------------------------------------------------------`
		// remove the server block coordinate
		//-------------------------------------------------------
		ZoneState::UnSetServerBlockIn(m_pZone, m_MoveType, m_ServerX, m_ServerY);
	
		// 2002.3.22 added
		SetGroundCreature();

		MovePosition( m_ServerX, m_ServerY );
		SetStop();

		// apply other results.
		AffectUsedActionInfo(m_nUsedActionInfo);		
		m_nUsedActionInfo	= ACTIONINFO_NULL;
		m_nSpecialActionInfo = ACTIONINFO_NULL;

		// ActionInfo when dying
		m_nNextUsedActionInfo	= (*g_pCreatureTable)[m_CreatureType].DeadActionInfo;	

		MActionResultNode* pActionResultNode = CreateActionResultNode(this, m_nNextUsedActionInfo);
		if(pActionResultNode != NULL)
		{
			if(m_pActionResult != NULL)
			{
				delete m_pActionResult;
				m_pActionResult = NULL;
			}
			
			m_pActionResult = new MActionResult;
			
			m_pActionResult->Add(pActionResultNode);
		}

			
		
		if (m_nNextUsedActionInfo!=ACTIONINFO_NULL)
		{		
			// action when dying
			SetNextAction( GetActionInfoAction(m_nNextUsedActionInfo) );

			// action done to me
			SetTraceID( m_ID );
			m_TraceX				= m_X;
			m_TraceY				= m_Y;
			m_TraceZ				= 0;
		}

		//-------------------------------------------------------
		// make visible
		//-------------------------------------------------------
		SetVisibleSoon();

		// HP
		StopRecoveryHP();
		StopRecoveryMP();
		SetStatus( MODIFY_CURRENT_HP, 0 );

		m_Z = 0;


		// died.
		m_bAlive = false;	
		m_bInCasket = false;

		//-------------------------------------------------------
		// remove Effect
		//-------------------------------------------------------
		ClearAttachEffect();

		//-------------------------------------------------------
		// remove Effect states
		//-------------------------------------------------------
		if (g_pEffectStatusTable!=NULL)
		{
			for (int e=0; e<g_pEffectStatusTable->GetSize(); e++)
			{
				if (e!=EFFECTSTATUS_BLOOD_DRAIN)
				{
					RemoveEffectStatus( (EFFECTSTATUS)e );
				}
			}
		}

		// leave darkness..
		m_DarknessCount = -1;
		m_DarknessCountInc = 0;

	}	
}

//----------------------------------------------------------------------
// Set Corpse
//----------------------------------------------------------------------
// Make it dead. --> straight to a corpse..
//----------------------------------------------------------------------
void
MCreature::SetCorpse()
{	
	//-------------------------------------------------------
	// remove the server block coordinate
	//-------------------------------------------------------
	ZoneState::UnSetServerBlockIn(m_pZone, m_MoveType, m_ServerX, m_ServerY);

	// stop
	SetStop();

	// dying appearance
	ACTIONINFO deadAI = (enum ACTIONINFO)(*g_pCreatureTable)[m_CreatureType].DeadActionInfo;
	
	// action when dying
	SetAction( GetActionInfoAction(deadAI) );

	// 2002.3.22 added
	SetGroundCreature();
	
	// perform all actions..
	m_ActionCount	= GetActionCountMax(); 
	m_MoveCount		= m_MoveCountMax;
	
	// died.
	m_bAlive = false;

	// leave darkness..
	m_DarknessCount = -1;
	m_DarknessCountInc = 0;
}

//----------------------------------------------------------------------
// Set Alive
//----------------------------------------------------------------------
void
MCreature::SetAlive()
{
	// Comes back to life.
	m_bAlive = true;

	m_bHasHead = true;

	SetAction( ACTION_STAND );

	//-------------------------------------------------------
	// remove Effect states
	//-------------------------------------------------------
	/*
	if (g_pEffectStatusTable!=NULL)
	{
		for (int e=0; e<g_pEffectStatusTable->GetSize(); e++)
		{
			RemoveEffectStatus( (EFFECTSTATUS)e );
		}
	}
	*/

	if ((*g_pCreatureTable)[m_CreatureType].bFlyingCreature)	// for a bat, change m_MoveType again because SetGroundCreature was done
		m_MoveType = CREATURE_FLYING;

	if (m_MoveType == CREATURE_FLYING)
	{
		m_Z=(*g_pCreatureTable)[m_CreatureType].FlyingHeight;
	}
}

//----------------------------------------------------------------------
// Set NextAction
//----------------------------------------------------------------------
// Set the next action to perform.
//----------------------------------------------------------------------
void			
MCreature::SetNextAction(BYTE action)
{
	m_NextAction = action;
}

//----------------------------------------------------------------------
// Set NextAction to Move
//----------------------------------------------------------------------
// Move on the next action.
//----------------------------------------------------------------------
void	
MCreature::SetNextActionToMove()
{
	m_NextAction = m_MoveAction;
}

//----------------------------------------------------------------------
// Set CutHeight From
//----------------------------------------------------------------------
void				
MCreature::SetCutHeightFrom(int startCount, int lastCount, int inc)
{
	m_bCutHeight = true;
	m_CutHeightCountLast = lastCount;
	m_CutHeightCount = startCount;
	m_CutHeightCountInc = inc;
}

//----------------------------------------------------------------------
// Update CutHeight
//----------------------------------------------------------------------
void				
MCreature::UpdateCutHeight()
{
	if (m_bCutHeight)
	{
		m_CutHeightCount += m_CutHeightCountInc;

		if (m_CutHeightCountInc < 0 && m_CutHeightCount <= m_CutHeightCountLast)
		{
			m_bCutHeight = false;
			m_CutHeightCount = 0;
			m_CutHeightCountInc = 0;
		}

		else if (m_CutHeightCountInc > 0 && m_CutHeightCount >= m_CutHeightCountLast)
		{
			m_bCutHeight = false;
			m_CutHeightCount = m_CutHeightCountLast;
			m_CutHeightCountInc = 0;
		}	
		
		// spin it around.
		m_Direction = ((m_Direction+1) & 0x07);		

		if( GetCreatureType() == 702 || GetCreatureType() == 703 || GetCreatureType() == 704 )
		{
			CreatureState::SyncTurretDirection( this );
		}
	}
}

//----------------------------------------------------------------------
// Update Turning
//----------------------------------------------------------------------
void				
MCreature::UpdateTurning()
{
	if (m_bTurning)
	{
		if (--m_TurningCount <= 0)
		{
			m_bTurning = false;
		}
		else
		{
			// spin it around.
			m_Direction = ((m_Direction+1) & 0x07);
		}
	}
}

//----------------------------------------------------------------------
// Update CauseCriticalWounds
//----------------------------------------------------------------------
void				
MCreature::UpdateCauseCriticalWounds()
{
	if(m_bCauseCriticalWounds)
	{
		if(IsDead())
		{
			StopCauseCriticalWounds();
			SetStop();
			return;
		}
		if(--m_CauseCriticalWoundsCount < 0)
		{
			StopCauseCriticalWounds();
			SetStop();
		}
		if(m_CauseCriticalWoundsCount < 10 && m_CauseCriticalWoundsCount >= 0)
			m_ActionCount = m_CauseCriticalWoundsCount;
		else if(m_ActionCount < 10)
			m_ActionCount++;

		if(m_CreatureType == 793) // Slayer sacrifice -_-;
			m_ActionCount = 20;

	#ifdef OUTPUT_DEBUG					
		DEBUG_ADD_FORMAT("[UpdateCCW] bCCW : %d CCWCount : %d Action : %d ActionCount : %d", m_bCauseCriticalWounds, m_CauseCriticalWoundsCount, m_Action, m_ActionCount);
	#endif
	}
}

//----------------------------------------------------------------------
// Update Ghost
//----------------------------------------------------------------------
void				
MCreature::UpdateGhost()
{
//	DEBUG_ADD_FORMAT("[UpdateGhost] %s, %d", m_fGhost?"true":"false", m_GhostCount);
	if(m_fGhost)
	{
		if(--m_GhostCount < 0)
		{
			StopGhost();
			//SetStop();
		}
	}
}

bool MCreature::IsBloodyZenith()
{
//	bool bActionAttack = 
//	{
//		m_Action == ACTION_ATTACK ||
//		m_Action == ACTION_VAMPIRE_ATTACK_FAST ||
//		m_Action == ACTION_VAMPIRE_ATTACK_SLOW
//	};

	return m_bBloodyZenith;//  && bActionAttack; 
}

int MCreature::GetBloodyZenithCount()
{
	return m_BloodyZenithCount;
}

//----------------------------------------------------------------------
// Update BloodyZenith
//----------------------------------------------------------------------
void				
MCreature::UpdateBloodyZenith()
{
	if(IsBloodyZenith() && !IsFastMove())
	{
		if(--m_BloodyZenithCount < 0)
		{
			StopBloodyZenith();
//			char TempBuffer[256];
//			sprintf(TempBuffer,"Bloody Zenith end a:%d, n:%d, n:%d, z:%d,s:%d", m_nUsedActionInfo, m_NextAction, m_nNextUsedActionInfo,MAGIC_BLOODY_ZENITH,SKILL_SET_AFIRE);
//			
//			g_pSystemMessage->Add(TempBuffer);
//			SetStop();
		}
	}
}

//----------------------------------------------------------------------
// Update GunShotGuidance
//----------------------------------------------------------------------
void				
MCreature::UpdateGunShotGuidance()
{
	if(m_bGunShotGuidance)
	{
		if(IsDead())
		{
			StopGunShotGuidance();
			SetStop();
			return;
		}
		if(--m_GunShotGuidanceCount < 0)
		{
			StopGunShotGuidance();
			SetStop();
		}
		if(m_GunShotGuidanceCount < 8 && m_GunShotGuidanceCount >= 0)
			m_ActionCount = m_GunShotGuidanceCount;
		else if(m_ActionCount < 8)
			m_ActionCount++;
		
	#ifdef OUTPUT_DEBUG					
		DEBUG_ADD_FORMAT("[UpdateCCW] bCCW : %d CCWCount : %d Action : %d ActionCount : %d", m_bGunShotGuidance, m_GunShotGuidanceCount, m_Action, m_ActionCount);
	#endif
	}
}

//----------------------------------------------------------------------
// Set FakeDie
//----------------------------------------------------------------------
void				
MCreature::SetFakeDie()
{
	m_bFakeDie = true;
	m_FakeDieCount = 0;
}

//----------------------------------------------------------------------
// Update CauseCriticalWounds
//----------------------------------------------------------------------
void				
MCreature::UpdateFakeDie()
{
	if(m_bFakeDie)
	{
		++m_FakeDieCount;
//		if( < 0)
//		{
//			StopFakeDie();
//		}
	}
}

//----------------------------------------------------------------------
// Set Invisible
//----------------------------------------------------------------------
void				
MCreature::SetInvisible()
{
	m_InvisibleCount = 0;
	m_InvisibleStep = 1;
}

//----------------------------------------------------------------------
// Set Invisible
//----------------------------------------------------------------------
void				
MCreature::SetInvisibleSoon()
{
	m_InvisibleCount = 64;
	m_InvisibleStep = 0;
}

//----------------------------------------------------------------------
// Set Visible
//----------------------------------------------------------------------
void				
MCreature::SetVisible()
{
	m_InvisibleCount = 64;
	m_InvisibleStep = -1;
}

//----------------------------------------------------------------------
// Set Visible
//----------------------------------------------------------------------
void				
MCreature::SetVisibleSoon()
{
	m_InvisibleCount = 0;
	m_InvisibleStep = 0;
}

//----------------------------------------------------------------------
// Set SameBody
//----------------------------------------------------------------------
void		
MCreature::SetSameBody(const MCreature* pCreature)
{
	SetCreatureType( pCreature->GetCreatureType() );

	if (GetClassType()!=CLASS_FAKE)
	{
		m_MoveType = pCreature->m_MoveType;
	}

//	if(pCreature->IsAdvancementClass())
//		SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, pCreature->GetAdvancementClassLevel() );
	m_Z	= pCreature->m_Z;

	m_ColorBody1 = pCreature->m_ColorBody1;
	m_ColorBody2 = pCreature->m_ColorBody2;
}

//----------------------------------------------------------------------
// Set Action
//----------------------------------------------------------------------
// The number of Frames will differ for each Action..
// It would be better to move this into a file and keep the info there
// must be read from FRAME_PACK.
//----------------------------------------------------------------------
void
MCreature::SetAction(BYTE action)
{ 	
//	if ( m_nUsedActionInfo == MAGIC_BLOODY_ZENITH && m_Action == ACTION_ATTACK &&
//		m_ActionCount >= m_ActionCountMax && GetActionDelay() > 0)
//	{
//		SetActionDelay( GetActionDelay() - 1 );
//		m_ActionCount = m_ActionCountMax - 2;
//		return;
//	}

	// turn it off unconditionally.
	m_bFastMove = false;

	if (!m_bAlive
		&& action!=ACTION_DIE)
	{		
		return;
	}
	if(HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET) && action != ACTION_STAND && action != ACTION_ATTACK)
		return;

	
	if(GetCreatureType() == 782 && action == ACTION_ATTACK)
		action = ACTION_VAMPIRE_DRAIN;
//	if(action>ACTION_OUSTERS_MAGIC_ATTACK)
//		int i = 0;
	//-----------------------------------------------------------------
	// when it matches the action of the current appearance. 2001.10.5
	//-----------------------------------------------------------------
	if (action < GetActionMax())
	{
		
		//-----------------------------------------------------
		// when riding a motorcycle
		//-----------------------------------------------------
		if (m_MoveDevice==MOVE_DEVICE_RIDE)
		{
			if (IsSlayer())
			{
				if (action==ACTION_STAND)
				{
					action = ACTION_SLAYER_MOTOR_STAND;
				}
				else if (action==ACTION_MOVE)
				{
					action = ACTION_SLAYER_MOTOR_MOVE;
				}
			}
		} else
			//-----------------------------------------------------
			// when walking around
			//-----------------------------------------------------
		{
			if (IsSlayer())
			{
				if (action==ACTION_SLAYER_MOTOR_STAND)
				{
					action = ACTION_STAND;
				}
				else if (action==ACTION_SLAYER_MOTOR_MOVE)
				{
					action = ACTION_MOVE;
				}
			}
		}
		if( IsOusters() && (HasEffectStatusSummonSylph( this ) || m_MoveDevice==MOVE_DEVICE_SUMMON_SYLPH))
		{
			if (action==ACTION_STAND)
			{
				action = ACTION_OUSTERS_FAST_MOVE_STAND;
			} else
			if( action == ACTION_MOVE )
			{
				action = ACTION_OUSTERS_FAST_MOVE_MOVE;
			}
		}
				
		m_bNextAction = false;
			
		//------------------------------------------------
		// for Move
		//------------------------------------------------
		if (action==m_MoveAction)//ACTION_MOVE || action==ACTION_SLAYER_MOTOR_MOVE)
		{
			// if just standing... make it the end action.
			// to mark that the Action has ended..
			if (m_Action==ACTION_STAND 
				|| (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND))
			{			
				m_ActionCount = 0;
				m_ActionCountMax = 0;			
			}		

			// only when the Action has ended, change Action to Move.
			// otherwise.. it changes to Move after the Action ends.
			if (m_ActionCount>=GetActionCountMax())
			{
				m_Action = action;
			}		

			// stop
			m_sX = 0;
			m_sY = 0;

			// move to the position verified by the Server.
			//MovePosition( m_ServerX, m_ServerY );
			
			// [ TEST CODE ]
			// remove the Action.
			// --> if not removed.. Move is performed after the Action ends.
			// However, Move may fail and the coordinate may go off. Hmm!!
			//m_ActionCount = 0;
			//m_ActionCountMax = 0;

			m_RepeatCount = 0;

			// first Move action
			m_MoveCount = 0;
			m_NextMoveCount = 0;
			
			//m_MoveCountMax = (*g_pCreatureTable)[m_CreatureType].GetActionCount( action );
			m_MoveCountMax = GetCreatureActionCountMax( this, action );
				//g_pTopView->m_CreatureFPK[(*g_pCreatureTable)[m_CreatureType].FrameID]
				//							[action][m_CurrentDirection].GetSize();//ACTIONCOUNT_MOVE;

		}
		//------------------------------------------------
		// for Action
		//------------------------------------------------
		else
		{
			//------------------------------------------------
			// extreme is released at a specific action.
			//------------------------------------------------
//			if (HasEffectStatus(EFFECTSTATUS_EXTREME))
//			{
//				if (action==ACTION_MAGIC
//					|| action==ACTION_DIE
//					|| action==ACTION_VAMPIRE_DRAIN
//					|| action==ACTION_DRAINED)
//				{
//					RemoveEffectStatus(EFFECTSTATUS_EXTREME);
//				}
//			}

			if (// do not show the action while riding a motorcycle.
				(m_MoveDevice==MOVE_DEVICE_RIDE && action!=ACTION_SLAYER_MOTOR_STAND)
				// for damaged..
				|| (action==ACTION_DAMAGED 						
						&& (// if not being attacked or not a stop action, do not show the being-attacked appearance
							m_Action!=ACTION_DAMAGED && m_Action!=ACTION_STAND && m_Action!=ACTION_MOVE
						//&& (// during a blood-drained action, or
						//	m_Action==ACTION_DRAINED
							// when a vampire, do not show damaged during the blood-sucking action.
						//	|| IsVampire() && m_Action==ACTION_VAMPIRE_DRAIN
							))
				)
			{
				m_NextAction = (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND;
				return;
			}

			
			// 2005, 1, 6, sobeit add start - if a stop arrives while motor ride move is in progress, ignore it once..
			if (IsAdvancementClass() && m_MoveDevice==MOVE_DEVICE_RIDE && action == ACTION_SLAYER_MOTOR_STAND&& m_Action==ACTION_SLAYER_MOTOR_MOVE)
			{
				if(!m_IsSkipMotorStand)
				{
					m_NextAction = (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND;
					m_IsSkipMotorStand = true;
					return;
				}
				else
					m_IsSkipMotorStand = false;
			}
			// 2005, 1, 6, sobeit add end
			m_ActionCountMax = GetCreatureActionCountMax( this, action );//(*g_pCreatureTable)[m_CreatureType].GetActionCount( action );

			if (m_ActionCountMax==0)
			{
				// when there is no action
				m_Action = (m_MoveDevice==MOVE_DEVICE_WALK)? ACTION_STAND : ACTION_SLAYER_MOTOR_STAND;
				m_ActionCount = 0;		
				m_RepeatCount = 0;
				//m_ActionCountMax = (*g_pCreatureTable)[m_CreatureType].GetActionCount( m_Action );
				m_ActionCountMax = GetCreatureActionCountMax( this, m_Action );
				
				// It was done below so I do it for now.. I don't know what it is - -; 2002.1.10
				m_nSpecialActionInfo = ACTIONINFO_NULL;
			}
			else
			{
				m_Action = action;			

				//---------------------------------------------
				// when repeating the action
				//---------------------------------------------
				// show from the start of the repeating action.
				// However, since we don't know when the next repeat packet will arrive..
				// ActionCounMax is always set to the end frame.
				//---------------------------------------------
//				BOOL bSlayer = IsSlayer();

				if (m_RepeatCount!=0	//m_Action == action			
					&& action!=ACTION_STAND
					&& (!IsSlayer() || (IsSlayer() && action!=ACTION_SLAYER_MOTOR_STAND))
					//&& (*g_pActionInfoTable)[m_nUsedActionInfo].IsUseRepeatFrame())
					)
				{
					// set count..
					
					m_ActionCount = GetActionInfoRepeatStartFrame( m_nSpecialActionInfo );
//					char Msg[128];
//					sprintf(Msg, "%d" , m_ActionCount);
//					g_pSystemMessage->Add(Msg);	
				}
				//---------------------------------------------
				// general case..
				//---------------------------------------------
				else
				{			
					// set count..
					m_ActionCount = 0;		
					m_RepeatCount = 0;

					// 2004, 11, 12, sobeit modify start - block
					// What is this? I don't know why it was done.. - -;; 2002.1.10
				//	m_nSpecialActionInfo = ACTIONINFO_NULL;	
					// 2004, 11, 12, sobeit modify end - block
				}				
			}
		}
	}
	// 2004, 11, 12, sobeit add start - block
	if(m_Action == ACTION_STAND)	
		m_nSpecialActionInfo = ACTIONINFO_NULL;	
	// 2004, 11, 12, sobeit add end - block

	// Effects additionally attached when an action starts.
	// should be moved somewhere.-_-
	if( HasEffectStatus( EFFECTSTATUS_REDIANCE ) )
	{
		if( 
			( m_Action == ACTION_SLAYER_SWORD_2 || m_Action == ACTION_SLAYER_SWORD_2_SLOW || m_Action == ACTION_SLAYER_SWORD_2_FAST ) 
			)
		{
			MAttachEffect *pEffect = CreateAttachEffect( EFFECTSPRITETYPE_REDIANCE_MALE_NORMAL, 0, 0 );
			if( pEffect != NULL )
			{
				pEffect->SetDirection( GetDirection() );
				pEffect->SetLink( ACTIONINFO_NULL, NULL );							
				pEffect->SetAttachCreatureID( GetID() );
			}
		} 
		else if(
			( m_Action == ACTION_SLAYER_SWORD || m_Action == ACTION_SLAYER_SWORD_FAST || m_Action == ACTION_SLAYER_SWORD_SLOW ) )
		{
			MAttachEffect *pEffect = CreateAttachEffect( EFFECTSPRITETYPE_REDIANCE_MALE_NORMAL_ATTACK_NORMAL, 0, 0 );
			if( pEffect != NULL )
			{
				pEffect->SetDirection( GetDirection() );
				pEffect->SetLink( ACTIONINFO_NULL, NULL );							
				pEffect->SetAttachCreatureID( GetID() );
			}
		}				
	}

//	// 2004, 9, 8, sobeit add start - Blade Slayer 130 skill related
//	else if(HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1)||
//			HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2)||
//			HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_3)||
//			HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_4)
//			)
//	{
//		if(m_Action == ACTION_SLAYER_BLADE)
//		{
//			if(m_ActionCount>4)
//				m_ActionCount = 5;
//		}
//		else
//			m_Action = ACTION_SLAYER_BLADE;
//	
//	}
//	// 2004, 9, 8, sobeit add end - Blade Slayer 130 skill related
	m_NextAction = (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND;
}

//----------------------------------------------------------------------
// Clear Chat String
//----------------------------------------------------------------------
void
MCreature::ClearChatString()
{
	// Chatting String
	m_ChatStringCurrent = 0;
	for (int i=0; i<g_pClientConfig->MAX_CHATSTRING; i++)
	{
		m_ChatString[i][0] = '\0';
	}
	m_HeadSkin = 0;

//	if( CurPernalShop() == 1 && m_NextChatFadeTime != 0 )
	if( CurPernalShop() == 1 && GetInputChatting() == true)
	{
		SetInputChatting(false);
		
		if( m_PersnalMessage.empty() )
			SetPersnalString( (*g_pGameStringTable)[UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE].GetString() );
		else
			SetPersnalString( m_PersnalMessage.c_str(), GameOptions::ChattingColor() );
	}
}

// All supported configurations allocate at least four data bytes per row.
// Keep storage clipping separate from the splitter's layout budget: even a
// decorated tree name is copied only through a complete UTF-8 scalar.
void MCreature::AppendChatRow(std::string_view text)
{
	if (g_pClientConfig->MAX_CHATSTRINGLENGTH_PLUS1 <= 0 || g_pClientConfig->MAX_CHATSTRING <= 0)
		return;
	const size_t capacity = static_cast<size_t>(g_pClientConfig->MAX_CHATSTRINGLENGTH_PLUS1);
	const size_t count = TextSystem::Utf8PrefixBytes(text, capacity - 1);
	char* const destination = m_ChatString[m_ChatStringCurrent];
	if (count != 0) memcpy(destination, text.data(), count);
	destination[count] = '\0';
	if (++m_ChatStringCurrent == g_pClientConfig->MAX_CHATSTRING) m_ChatStringCurrent = 0;
}

void
MCreature::SetPersnalString(const char* input, COLORREF color)
{
	if (input == NULL)
	{
		DEBUG_ADD("[SetPersnalString] str == NULL");
		return;
	}

	// Copy before clearing rows; the supplied sign can belong to the creature.
	const std::string text = TextEncoding::NormalizeUtf8(input);
	DEBUG_ADD_FORMAT("[SetPersnalString] %s", text.c_str());
	ClearChatString();
	m_NextChatFadeTime = MonotonicClock::TimePoint();
	SetInputChatting(false);

	const size_t column = static_cast<size_t>(max(0, g_pClientConfig->MAX_CHATSTRING_LENGTH));
	for (const auto& row : TextSystem::WrapUtf8Lines(text, column, {.skipSeamSpace = false}))
		AppendChatRow(row);

	m_ChatColor = color;
	m_OriChatColor |= 0xFF000000;
	m_ChatTime = g_CurrentFrame;

	DEBUG_ADD("[SetPersnalString] Excute ok");
	for (int i = 0; i < m_ChatStringCurrent; i++)
		DEBUG_ADD_FORMAT("%s", m_ChatString[i]);
	DEBUG_ADD("[SetPersnalString] Excute ok 2");
}
//----------------------------------------------------------------------
// Set ChatString
//----------------------------------------------------------------------
// Add a String. At the end~..
//----------------------------------------------------------------------
void		
MCreature::SetChatString(const char *input, COLORREF color)
{
	if (input==NULL)
	{
		DEBUG_ADD("[SetChatString] str == NULL");
		return;
	}

	// Normalize complete input before any byte cuts, and keep tree parsing local.
	std::string mutableString = TextEncoding::NormalizeUtf8(input);
	char* str = mutableString.data();

//	if(CurPernalShop() == 1)
//		return;

	DEBUG_ADD_FORMAT("[SetChatString] %s", str);

	if(GameOptions::ChatWhite())
		color = RGB_WHITE;

	SetInputChatting(false);
	ClearChatString();
	SetInputChatting(true);



	// Christmas-tree recipient, message and sender fields.
	char *szTreeFrom = NULL;
	if( GetCreatureType() == 482 || GetCreatureType() == 650 )
	{
		if(GetCreatureType() == 482)
		{
			DEBUG_ADD("[SetChatString] Tree!!!");
			
			char *find = strchr(str, '%');
			if(find == NULL)
			{
				DEBUG_ADD("[SetChatString] find == NULL");
				
				return;
			}
			
			*find = '\0';
			
			AppendChatRow(std::string("Dear. ") + str);

			str = find+1;
		}
		char *find2 = strchr(str, '%');
		if(find2 == NULL)
		{
			DEBUG_ADD("[SetChatString] find2 == NULL");

			return;
		}

		*find2 = '\0';

		szTreeFrom = find2+1;
	}    
	

	
	if(strstr(str, "배철숩니다") != NULL)
	{
		m_HeadSkin = 215;
	}
	else if(strstr(str, "해골바가지") != NULL)
	{
		m_HeadSkin = 149;
	}
	else if(strstr(str, "늑대인간") != NULL)
	{
		m_HeadSkin = 397;
	}
	else if(strstr(str, "닭대가리") != NULL)
	{
		m_HeadSkin = 221;
	}
	else if(strstr(str, "가면라이더") != NULL)
	{
		m_HeadSkin = 316;
	}


	const size_t column = static_cast<size_t>(max(0, g_pClientConfig->MAX_CHATSTRING_LENGTH));
	const bool isTree = GetCreatureType() == 482 || GetCreatureType() == 650;
	// Ordinary chat keeps embedded newlines as before; tree messages split them.
	for (const auto& row : TextSystem::WrapUtf8Lines(str, column,
		{.skipSeamSpace = false, .splitNewlines = isTree}))
		AppendChatRow(row);
	DEBUG_ADD("[SetChatString] while ok");

	// Hard-coded for the Christmas tree
	if(GetCreatureType() == 482 || GetCreatureType() == 650)
	{
		DEBUG_ADD("[SetChatString] Tree code");

		std::string sender = std::string("From. ") + szTreeFrom;
		if (sender.size() < column) sender.insert(0, column - sender.size(), ' ');
		AppendChatRow(sender);

		DEBUG_ADD("[SetChatString] Tree code ok");
	}
	

	// when the chat string starts to fade
	m_NextChatFadeTime = g_FrameNow + MonotonicClock::Duration(g_pClientConfig->DELAY_CHATSTRING_KEEP);

	// chat color
	m_ChatColor = color;//RGB_WHITE;//0xFFFF;
	m_OriChatColor = m_ChatColor;
	
	// hard coding for the Christmas tree
	if(GetCreatureType() == 482 || GetCreatureType() == 650)
	{
		m_NextChatFadeTime = MonotonicClock::TimePoint();
	}
	else
	{
		m_OriChatColor |= 0xFF000000;
	}

	// set spoken time
	m_ChatTime = g_CurrentFrame;
	

	DEBUG_ADD("[SetChatString] Excute ok");
	for(int i = 0; i < m_ChatStringCurrent; i++)
	{
		DEBUG_ADD_FORMAT("%s", m_ChatString[i]);
	}
	DEBUG_ADD("[SetChatString] Excute ok 2");
}

//----------------------------------------------------------------------
// Fade ChatString
//----------------------------------------------------------------------
// Darken the ChatString.
//----------------------------------------------------------------------
void		
MCreature::FadeChatString()
{
	/*
	m_ChatString[m_ChatStringCurrent][0] = NULL;	

	m_ChatStringCurrent++;
	if (m_ChatStringCurrent==g_pClientConfig->MAX_CHATSTRING) m_ChatStringCurrent=0;

	// Specify the time the chat String is delayed.
	m_NextChatFadeTime = g_CurrentTime + DELAY_FADECHATSTRING;
	*/

//	MCreature* pCreature = g_pZone->GetCreature(GetID());
//	if(CurPernalShop() == 1 && m_NextChatFadeTime+3000 >= g_CurrentTime)
//	{
//		if(pCreature->GetPersnalShopMessage().length()>0)
//			pCreature->SetPersnalString((char*)g_pPlayer->GetPersnalShopMessage().c_str(),g_pUserOption->ChattingColor);
//		else
//			pCreature->SetPersnalString((*g_pGameStringTable)[UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE].GetString(),g_pUserOption->ChattingColor);
//	}
//	else if(CurPernalShop() == 1)
//	{
//		return;
//	}

//	if (m_NextChatFadeTime >= g_CurrentTime || ( CurPernalShop() == 1 && m_NextChatFadeTime == 0 ) )
	if (m_NextChatFadeTime >= g_FrameNow || ( CurPernalShop() == 1 && GetInputChatting() == false ) )
		return;

	BYTE b = (m_OriChatColor & 0xFF000000) >> 24;

	BYTE color[3] ={ static_cast<BYTE>(m_OriChatColor & 0xFF), static_cast<BYTE>((m_OriChatColor & 0xFF00)>>8), static_cast<BYTE>((m_OriChatColor & 0xFF0000)>>16) };


	if (b>=g_pClientConfig->MIN_CHATSTRING_COLOR256)
	{
		// hard coding for the Christmas tree
		if(GetCreatureType() == 482 || GetCreatureType() == 650)
		{
			b -= g_pClientConfig->VALUE_CHATSTRING_FADE*4;
		}
		else
		{
			b -= g_pClientConfig->VALUE_CHATSTRING_FADE;
			if(b > 0)
			{
				color[0] = color[0]*b/0xFF;
				color[1] = color[1]*b/0xFF;
				color[2] = color[2]*b/0xFF;
			}

		}
	}
	else
	{
		// hard coding for the Christmas tree
		if(GetCreatureType() == 482 || GetCreatureType() == 650)
		{
			const int treeColorMax = 18;
			static COLORREF treeColor[treeColorMax] = 
			{
				RGB(255, 150, 150),
				RGB(150, 255, 150),
				RGB(150, 150, 255),
				RGB(255, 255, 150),
				RGB(150, 255, 255),
				RGB(255, 150, 255),

				RGB(255, 200, 150),
				RGB(200, 255, 150),
				RGB(200, 150, 255),
				RGB(255, 255, 200),
				RGB(200, 255, 255),
				RGB(255, 200, 255),

				RGB(255, 150, 200),
				RGB(150, 255, 200),
				RGB(150, 200, 255),

				RGB(255, 200, 200),
				RGB(200, 255, 200),
				RGB(200, 200, 255),
			};

			// chat color
			COLORREF color = 0;
			
			do
			{
				color = treeColor[rand()%treeColorMax];
			}while(color == (m_OriChatColor&0x00FFFFFF));
			
			m_ChatColor = color;//RGB_WHITE;//0xFFFF;
			m_OriChatColor = m_ChatColor;
			m_OriChatColor |= 0xFF000000;
			if(GetCreatureType() == 650 )
				m_NextChatFadeTime = g_FrameNow + MonotonicClock::Millis(0xffffff);
			
			return;
		}
		else
		{
			ClearChatString();
		}
	}

	m_NextChatFadeTime = g_FrameNow + MonotonicClock::Duration(g_pClientConfig->DELAY_CHATSTRING_FADE);
	
	m_ChatColor = RGB(color[0], color[1], color[2]);
	m_OriChatColor = (m_OriChatColor&0xFFFFFF) | (b<<24);
}


//----------------------------------------------------------------------
// Get ChatString
//----------------------------------------------------------------------
// 0 ~ g_pClientConfig->MAX_CHATSTRING-1
// 0 is the oldest String and g_pClientConfig->MAX_CHATSTRING-1 should be the most recent one
// must be returned.
//----------------------------------------------------------------------
const char*	
MCreature::GetChatString(BYTE i)
{ 
	//                          i   = the value that should actually be returned
	//m_ChatStringCurrent - (3-[0]) = m_ChatStringCurrent;
	//m_ChatStringCurrent - (3-[1]) = m_ChatStringCurrent - 2;
	//m_ChatStringCurrent - (3-[2]) = m_ChatStringCurrent - 1;

	BYTE gap = g_pClientConfig->MAX_CHATSTRING - i;

	if (gap > m_ChatStringCurrent)
	{
		return m_ChatString[m_ChatStringCurrent + g_pClientConfig->MAX_CHATSTRING - gap];
	}
	
	return m_ChatString[m_ChatStringCurrent - gap];	
}

//----------------------------------------------------------------------
// Check AffectStatus
//----------------------------------------------------------------------
// Sets whether this creature can use pItem. The rule is the item's
// (MItem::IsUsableBy, in gamemodel); this reads the creature's race,
// current stats and sex for it.
//----------------------------------------------------------------------
void	
MCreature::CheckAffectStatus(MItem* pItem)
{
	MItemUser user;
	user.race = GetRace();
	user.str = static_cast<int>(GetSTR());
	user.dex = static_cast<int>(GetDEX());
	user.inte = static_cast<int>(GetINT());
	user.level = static_cast<int>(GetLEVEL());
	user.bMale = IsMale();

	if (pItem->IsUsableBy(user))
	{
		pItem->SetAffectStatus();
	}
	else
	{
		pItem->UnSetAffectStatus();
	}
}

//----------------------------------------------------------------------
// Set Direction To Creature
//----------------------------------------------------------------------
// Look toward another Creature.
//----------------------------------------------------------------------
void	
MCreature::SetDirectionToPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	// if the same coordinate.. keep the current direction.
	if (m_X==sX && m_Y==sY)
	{
		m_Direction = m_CurrentDirection;
	}
	else
	{
		m_Direction = SelectFacingDirection(m_X, m_Y, sX, sY);
	}
}

//----------------------------------------------------------------------
// Change Near Direction
//----------------------------------------------------------------------
// Smoothly change m_CurrentDirection to m_Direction.
// However, the current moving direction (m_DirectionMoved) takes priority.
//----------------------------------------------------------------------
bool	
MCreature::ChangeNearDirection()
{
	int toDir;
	
	// if showing an action.. set direction
	if (m_ActionCount<GetActionCountMax())
	{
		toDir = m_Direction;
	}
	// if moving, m_DirectionMoved
	// However, for KnockBack or FastMove... ignore direction..
	else if (m_MoveCount>0 && m_MoveCount<m_MoveCountMax
		&& !m_bKnockBack
		&& !m_bFastMove)		
	{
		toDir = m_DirectionMoved;
	}
	// otherwise.. m_Direction
	else
	{
		toDir = m_Direction;
	}

	// if the same, no need to change.
	if (m_CurrentDirection==toDir)
	{
		return false;
	}

	// if different..
	// must turn left or turn right.

	// To make it positive.. and.. for speed.. & was used.
	int gap = (((m_CurrentDirection+MAX_DIRECTION) - toDir) & 0x07);//% MAX_DIRECTION;

	// 4, 7
	if (gap==4)
	{
		// turns randomly left or right.
		if (rand() & 0x01)
		{
			m_CurrentDirection ++;			
		}
		else
		{
			m_CurrentDirection += 7;			
		}
	}
	// 
	else if (gap<4)
	{
		m_CurrentDirection += 7;		
	}
	else
	{
		m_CurrentDirection ++;		
	}

	// keep it within 0~7.
	m_CurrentDirection &= 0x07;

	return true;	
}

//----------------------------------------------------------------------
// Get Counter Direction
//----------------------------------------------------------------------
// Get the direction opposite to the heading.
// Hmm, this should be made static or d should be removed... - -
//----------------------------------------------------------------------
int
MCreature::GetCounterDirection( int d )
{
	// opposite direction
	d += 4;

	// limit to 0~7.
	while (d < 8)
	{
		d -= 8;
	}

	return d;
}

//----------------------------------------------------------------------
// Get Direction To Creature
//----------------------------------------------------------------------
// Decide the direction of looking from the current position (m_X,m_Y) toward (sX,sY).
//----------------------------------------------------------------------
BYTE	
MCreature::GetDirectionToPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	// if the same coordinate.. keep the current direction.
	if (m_X==sX && m_Y==sY)
	{
		return m_CurrentDirection;
	}

	return SelectFacingDirection(m_X, m_Y, sX, sY);
}

//----------------------------------------------------------------------
// Get NextPosition()
//----------------------------------------------------------------------
// position after moving in the direction from the current Creature position
//----------------------------------------------------------------------
void
MCreature::GetNextPosition(BYTE direction, POINT &next)
{
	// set the target for the next direction from the current position
	next.x = m_X;
	next.y = m_Y;
	
	switch (direction)
	{
		case DIRECTION_LEFTDOWN		: next.x--;	next.y++;	break;
		case DIRECTION_RIGHTUP		: next.x++;	next.y--;	break;
		case DIRECTION_LEFTUP		: next.x--;	next.y--;	break;
		case DIRECTION_RIGHTDOWN	: next.x++;	next.y++;	break;
		case DIRECTION_LEFT			: next.x--;			break;
		case DIRECTION_DOWN			:		next.y++;	break;
		case DIRECTION_UP			:		next.y--;	break;
		case DIRECTION_RIGHT		: next.x++;			break;
	}
}

//----------------------------------------------------------------------
// Get NextPosition()
//----------------------------------------------------------------------
// position after moving in the m_CurrentDirection direction from the current Creature position
//----------------------------------------------------------------------
/*
void
MCreature::GetNextPosition(POINT &next)
{
	// set the target for the next direction from the current position
	next.x = m_X;
	next.y = m_Y;
	
	switch (m_CurrentDirection)
	{
		case DIRECTION_LEFTDOWN		: next.x--;	next.y++;	break;
		case DIRECTION_RIGHTUP		: next.x++;	next.y--;	break;
		case DIRECTION_LEFTUP		: next.x--;	next.y--;	break;
		case DIRECTION_RIGHTDOWN	: next.x++;	next.y++;	break;
		case DIRECTION_LEFT			: next.x--;			break;
		case DIRECTION_DOWN			:		next.y++;	break;
		case DIRECTION_UP			:		next.y--;	break;
		case DIRECTION_RIGHT		: next.x++;			break;
	}
}
*/

//----------------------------------------------------------------------
// Get NextPosition()
//----------------------------------------------------------------------
// position after moving in the m_CurrentDirection direction from the current Creature position
//----------------------------------------------------------------------
void
MCreature::GetNextPosition(TYPE_SECTORPOSITION &sX, TYPE_SECTORPOSITION &sY)
{
	// set the target for the next direction from the current position
	sX = m_X;
	sY = m_Y;
	
	switch (m_Direction)
	{
		case DIRECTION_LEFTDOWN		: sX--;	sY++;	break;
		case DIRECTION_RIGHTUP		: sX++;	sY--;	break;
		case DIRECTION_LEFTUP		: sX--;	sY--;	break;
		case DIRECTION_RIGHTDOWN	: sX++;	sY++;	break;
		case DIRECTION_LEFT			: sX--;			break;
		case DIRECTION_DOWN			:		sY++;	break;
		case DIRECTION_UP			:		sY--;	break;
		case DIRECTION_RIGHT		: sX++;			break;
	}
}

//----------------------------------------------------------------------
// Move
//----------------------------------------------------------------------
//
// Move one Frame in the current direction (m_CurrentDirection).
//
// Not moving a whole Sector, but the middle of moving between Sector and Sector
// moving one Frame.
//
// The question is how many Frames to move from Sector --> Sector??...
//    = assign the pixel unit moved at one time to cX,cY, and
//    = input the pixel unit moved at one time into sX,sY.
//
// In Move()
//    if cX,cY is 0, regenerate the cX,cY, sX,sY values, and
//    if not 0, sX
//----------------------------------------------------------------------
void	
MCreature::ActionMove()
{
	if( GetCreatureType() == 672 )
		return;

	if( HasEffectStatus( EFFECTSTATUS_TRYING ) )
	{
		std::vector<const MEffect*> groundEffects;
		ZoneState::GetGroundEffects( groundEffects );
		bool bFind = false;

		for( const MEffect* pEffect : groundEffects )
		{
			if( PlayerX() == pEffect->GetX() &&
				PlayerY() == pEffect->GetY() &&
				pEffect->GetFrameID() == EFFECTSPRITETYPE_REGEN_TOWER_GROUND )
			{
				bFind = true;
				break;
			}
		}
		if( bFind == false )
			RemoveEffectStatus( EFFECTSTATUS_TRYING );
	}

	// when the destination Sector is reached
	if (m_MoveCount==0)//m_sX==0 && m_sY==0)// && m_ActionCount==0)
	{
		// advance one Sector in the current direction, and
		// cX,cY, sX,sY must be set again.

		//--------------------------------------------
		// check whether movement is possible
		//--------------------------------------------
		TYPE_SECTORPOSITION	x, y;

		//--------------------------------------------
		// for fast move, the coordinate is in nextX, nextY.
		//--------------------------------------------
		if (m_bFastMove)
		{
			x = m_NextX;
			y = m_NextY;

			m_NextX = SECTORPOSITION_NULL;					

			//m_CurrentDirection = 
			m_DirectionMove = m_Direction = GetDirectionToPosition( x, y );
		}
		else
		{
			// decide the coordinate when moved in the current direction.
			x = m_X; 
			y = m_Y;
			GetPositionToDirection( x, y, m_DirectionMove );
		}

		//--------------------------------------------
		// when it can move
		//--------------------------------------------
		//
		// must move even if it cannot move.
		// because it is information that came from the Server..
		// 
		//--------------------------------------------

		//#ifdef	OUTPUT_DEBUG
			TYPE_SECTORPOSITION ox = m_X;
			TYPE_SECTORPOSITION oy = m_Y;
		//#endif
		
		{
			// change the sector info of the zone.
			// also set m_X and m_Y.
			if (MovePosition( x, y ))
			{
				#ifdef OUTPUT_DEBUG
					DEBUG_ADD_FORMAT("[Move] [ID=%d] (%d,%d) --> (%d,%d)",
									m_ID,
									ox, oy,
									m_X, m_Y);					

					if (m_DirectionMove >= 8)
					{
						DEBUG_ADD_FORMAT_ERR("[Error] Direction exceed MAX : dir=%d", (int)m_DirectionMove);
						return;
					}
				#endif
				//--------------------------------------------
				// if there are 5 Frames
				//--------------------------------------------
				//
				// [1] when UP,DOWN,LEFT,RIGHT,
				//
				//     X change : 0 16 32 48 64   (+-16)
				//     Y change : 0  8 16 24 32   (+-8)
				// 
				//
				// [2] when moving diagonally (LEFTUP,LEFTDOWN,RIGHTUP,RIGHTDOWN),
				//
				//     X change : 0  8 16 24 32   (+-8)
				//     Y change : 0  4  8 12 16   (+-4)
				//
				//--------------------------------------------
				// sX,sY : total pixels to move (one TILE)
				// cX,cY : unit pixel of movement
				//--------------------------------------------

				//--------------------------------------------
				// for Fast Move
				//--------------------------------------------
				if (m_bFastMove)
				{
					// pixel distance when moving in a straight line
					// ex) left 2 --> 1 : ( 2 - 1 ) * TILE_X
					//     up 2 --> 1 : ( 2 - 1 ) * TILE_Y
					m_sX = (ox - m_X) * TILE_X;
					m_sY = (oy - m_Y) * TILE_Y;
					//------------------------------------------------
					// moves to the destination at half the speed of actually moving one tile.
					//------------------------------------------------
					// number of Frames when moving one tile
					int moveTimes_div_2 = (*g_pCreatureTable)[m_CreatureType].MoveTimes >> 1;

					// 2005, 1, 5, sobeit add start
					if(HasEffectStatus(EFFECTSTATUS_BIKE_CRASH))
					{
//						moveTimes_div_2 = moveTimes_div_2/2;
						m_MoveCountMax = m_MoveCountMax*2;
					}
					// 2005, 1, 5, sobeit add end

					m_cX = -m_sX / moveTimes_div_2;
					m_cY = -m_sY / moveTimes_div_2;

					m_MoveCount += 2;

					if (m_MoveCount > m_MoveCountMax)
					{
						m_MoveCount = m_MoveCountMax;
					}
				}
				//--------------------------------------------	
				// normal move
				//--------------------------------------------
				else
				{				
					m_DirectionMoved = m_DirectionMove;
					m_sX = m_sXTable[m_DirectionMoved];
					m_sY = m_sYTable[m_DirectionMoved];
					
					//????????????????????????????????????????????????????????
					//???                                                  ???
					//???   Should one Frame be moved...?? or not   ???
					//???                                                  ???
					//????????????????????????????????????????????????????????
					int moveTimes_1;

					#ifdef OUTPUT_DEBUG
						if (m_CreatureType >= g_pCreatureTable->GetSize())
						{
							DEBUG_ADD_FORMAT_ERR("[Error] CreatureType is exceed MAX. id=%d, type=%d", m_ID, m_CreatureType);

							SetStop();

							return;
						}
					#endif
				
					if (m_MoveDevice==MOVE_DEVICE_WALK)
					{
						moveTimes_1 = (*g_pCreatureTable)[m_CreatureType].MoveTimes-1;
					}
					else if (m_MoveDevice==MOVE_DEVICE_RIDE)
					{
						moveTimes_1 = (*g_pCreatureTable)[m_CreatureType].MoveTimesMotor-1;
					}
					else if (m_MoveDevice==MOVE_DEVICE_SUMMON_SYLPH || HasEffectStatusSummonSylph( this ))
					{
						moveTimes_1 = (*g_pCreatureTable)[m_CreatureType].MoveTimesMotor-1;
					}
					else
					{
						DEBUG_ADD_FORMAT_ERR("[Error] What is MoveType of this Creature??? id=%d", m_ID);
						
						SetStop();

						return;
					}

					m_MoveTableCount = 0;
					m_cX = m_cXTable[moveTimes_1][m_DirectionMoved][m_MoveTableCount];
					m_cY = m_cYTable[moveTimes_1][m_DirectionMoved][m_MoveTableCount];

					//m_ActionCount++;
					m_MoveCount++;
				}
				
				m_sX += m_cX;
				m_sY += m_cY;	
				
				// Specify the count for the next move.
				// cases where only the frame changes, and
				// cases where the frame changes and it moves.
				m_NextMoveCount = (*g_pCreatureTable)[m_CreatureType].MoveRatio;				

				//------------------------------------------------
				// Play the Sound that matches the character's MoveAction.
				//------------------------------------------------
				TYPE_SOUNDID soundID = (*g_pCreatureTable)[m_CreatureType].GetActionSound( m_MoveAction );

				if (soundID!=SOUNDID_NULL  )
				{
					WorldEvents::PlaySoundAt( soundID,
								false,
								m_X,
								m_Y);
				}
			}
			// when it could not move
			else
			{
				// stop
				SetAction( ACTION_STAND );	
			}			
		}
		//--------------------------------------------
		// when it cannot move
		//--------------------------------------------
	}
	// when the next Sector has not yet been fully reached
	else
	{
		// Move one Frame.
		/*
		switch (m_MoveDevice)
		{
			case MOVE_DEVICE_NULL :			
				m_sX += m_cXTableDeviceNull[m_CurrentDirection][m_ActionCount];
				m_sY += m_cYTableDeviceNull[m_CurrentDirection][m_ActionCount];
			break;

			case MOVE_DEVICE_MOTOR1 :						
				m_sX += m_cXTableDeviceMotor1[m_CurrentDirection][m_ActionCount];
				m_sY += m_cYTableDeviceMotor1[m_CurrentDirection][m_ActionCount];
			break;
		}
		*/
		//m_sX += m_cX;
		//m_sY += m_cY;

		// move only when the move count is reached.
		if (m_MoveCount>=m_NextMoveCount)
		{			
			//--------------------------------------------
			// for Fast Move
			//--------------------------------------------
			if (m_bFastMove)
			{
				m_sX += m_cX;
				m_sY += m_cY;

				m_MoveCount += 2;

				if (m_MoveCount > m_MoveCountMax)
				{
					m_MoveCount = m_MoveCountMax;
				}

				// Specify the count for the next move.
				m_NextMoveCount += (*g_pCreatureTable)[m_CreatureType].MoveRatio;

				//----------------------------------------------------------
				// hit once after fast move.. - -
				// Flash Sliding
				//----------------------------------------------------------
				if (m_MoveCount+1 >= m_MoveCountMax
					&& m_nNextUsedActionInfo!=m_nBasicActionInfo)
				{
					MCreature* pCreature = ZoneState::GetCreature( m_TraceID );

					if (pCreature!=NULL 
						&& m_nSpecialActionInfo != MAGIC_RAPID_GLIDING 
						&& m_nSpecialActionInfo != SKILL_TELEPORT 
						&& m_nSpecialActionInfo != SKILL_CHARGING_ATTACK 
						&& !HasEffectStatus(EFFECTSTATUS_BIKE_CRASH))
					{
						//---------------------------------------------------
						// appearance of the target being hit
						//---------------------------------------------------
						MActionResult* pResult = new MActionResult;
						
						int ActionInfo = GetBasicActionInfo();//m_nBasicActionInfo;

						if(m_nSpecialActionInfo == SKILL_BLITZ_SLIDING || m_nSpecialActionInfo == SKILL_BLAZE_WALK )
						{
							ActionInfo = SKILL_BLITZ_SLIDING_ATTACK;

							if( m_nSpecialActionInfo == SKILL_BLAZE_WALK )
								ActionInfo = SKILL_BLAZE_WALK_ATTACK;
							
							SetDirection( SelectFacingDirection( GetX(), GetY(), pCreature->GetX(), pCreature->GetY() ) );
						}
						else if(m_nSpecialActionInfo == SKILL_SET_AFIRE)
						{
							ActionInfo = RESULT_SKILL_SET_AFIRE;
						}
//						if(m_nSpecialActionInfo == MAGIC_BLOODY_ZENITH)
//						{
//							SetBloodyZenith(16);
//						}						

						// show result
						SetDirection( SelectFacingDirection( GetX(), GetY(), pCreature->GetX(), pCreature->GetY() ) );
						pResult->Add( new MActionResultNodeActionInfo( 
														ActionInfo,
														m_ID,
														pCreature->GetID(), 
														pCreature->GetX(),
														pCreature->GetY(),
														0 ) 
										);	
					
						PacketSpecialActionToOther( ActionInfo, 
													m_TraceID, 
													pResult );						
					}
				}
			}
			//--------------------------------------------
			// for ordinary movement
			//--------------------------------------------
			else
			{
				int moveTimes_1;
				
				if (m_MoveDevice==MOVE_DEVICE_WALK)
				{
					moveTimes_1 = (*g_pCreatureTable)[m_CreatureType].MoveTimes-1;
				}
				else if (m_MoveDevice==MOVE_DEVICE_RIDE)
				{
					moveTimes_1 = (*g_pCreatureTable)[m_CreatureType].MoveTimesMotor-1;
				}
				else if (m_MoveDevice==MOVE_DEVICE_SUMMON_SYLPH || HasEffectStatusSummonSylph( this ))
				{
					moveTimes_1 = (*g_pCreatureTable)[m_CreatureType].MoveTimesMotor-1;
				} 
				else
				{
					DEBUG_ADD_FORMAT_ERR("[Error] MoveTimes Error. MCreature.cpp Lines:%d",__LINE__);
					return;
				}

				m_MoveTableCount++;
				if(m_MoveTableCount > moveTimes_1)
					m_MoveTableCount = moveTimes_1;
				
				m_cX = m_cXTable[moveTimes_1][m_DirectionMoved][m_MoveTableCount];
				m_cY = m_cYTable[moveTimes_1][m_DirectionMoved][m_MoveTableCount];

				m_sX += m_cX;
				m_sY += m_cY;

				// Specify the count for the next move.
				m_NextMoveCount += (*g_pCreatureTable)[m_CreatureType].MoveRatio;

				m_MoveCount++;
			}
		
		}
		else
		{
			//m_ActionCount++;
			m_MoveCount++;
		}	


		// Change the frame. If it reached the last Frame, change to the 0th Frame.
		//if (++m_ActionCount == (*m_pFrames)[m_Action][m_CurrentDirection].GetCount())
		//	m_ActionCount = 0;
	}
}

//----------------------------------------------------------------------
// Attach CastingEffect
//----------------------------------------------------------------------
void	
MCreature::AttachCastingEffect(TYPE_ACTIONINFO nUsedActionInfo, BOOL bForceAttach)
{
	#ifdef OUTPUT_DEBUG					
    	DEBUG_ADD_FORMAT("[CheckError] AttachCastingEffect(%d, %d)", nUsedActionInfo, bForceAttach);	
		if (g_bCheckError)
		{
			DEBUG_ADD_FORMAT("[CheckError] AttachCastingEffect(%d, %d)", nUsedActionInfo, bForceAttach);
		}
	#endif

	if (nUsedActionInfo!=ACTIONINFO_NULL)
	{
		int		castingStartFrame = GetActionInfoCastingStartFrame(nUsedActionInfo);
		BOOL	bCastingEffect	= bForceAttach || (castingStartFrame==m_ActionCount);

		//-------------------------------------------------------------
		// Casting Effect
		//-------------------------------------------------------------
		if (bCastingEffect)
		{
			if (GetActionInfoActionEffectSpriteType(nUsedActionInfo) != EFFECTSPRITETYPE_NULL)
			{
				int castingFrames = GetActionInfoCastingFrames(nUsedActionInfo);
			
				// if not set.. match the action.
				if (castingFrames==0)
				{
					int actionInfoAction = GetActionInfoAction(nUsedActionInfo);
						
					//-------------------------------------------------
					// when riding a motorcycle
					//-------------------------------------------------				
					if (m_MoveDevice==MOVE_DEVICE_RIDE)
					{
						//castingFrames = (*g_pCreatureTable)[m_CreatureType].GetActionCount( actionInfoAction );					
						m_ActionCountMax = GetCreatureActionCountMax( this, actionInfoAction );
					}					
					else
					{	
						//-------------------------------------------------
						// when attacked during the blood-sucking action
						//-------------------------------------------------
						if (actionInfoAction==ACTION_DAMAGED
							//&& (// during a blood-drained action, or
								//m_Action==ACTION_DRAINED
								// when a vampire, do not show damaged during the blood-sucking action.
								//|| IsVampire() && m_Action==ACTION_VAMPIRE_DRAIN
							//	)
							)
						{
							//castingFrames = (*g_pCreatureTable)[m_CreatureType].GetActionCount( ACTION_DAMAGED );
							castingFrames = GetCreatureActionCountMax( this, ACTION_DAMAGED );
						}
						else
						{
							castingFrames = (int)GetActionCountMax() - (int)castingStartFrame;

							// temporarily..
							// what if a negative number comes out here.. what to do.. - -;
							if (castingFrames < 0)
							{
								castingFrames = 16;
							}
						}
					}					
				}

				TYPE_EFFECTSPRITETYPE effectSpriteType = GetActionInfoActionEffectSpriteType(nUsedActionInfo);				

				MCreature* targetCreature = NULL;

				//-------------------------------------------------
				// who does the casting attach to?
				//-------------------------------------------------
				if(nUsedActionInfo != ACTIONINFO_NULL)
				{
					if ((*g_pActionInfoTable)[nUsedActionInfo].IsCastingEffectToSelf())
					{
						targetCreature = this;
					}
					else
					{
						targetCreature = ZoneState::GetCreature( m_TraceID );
					}
				}


				if (targetCreature!=NULL)
				{
					// [ TEST CODE ]
					// create an Effect to attach to the Creature and receive the pointer.
					MAttachEffect* pEffect = targetCreature->CreateAttachEffect( 
																effectSpriteType, 
																//(*g_pActionInfoTable)[nUsedActionInfo].GetDelay() 
																castingFrames,
																castingFrames
															);
		
					if (pEffect != NULL)
					{
						pEffect->SetDirection( targetCreature->GetDirection() );

						pEffect->SetLink( nUsedActionInfo, NULL );

						// character it must attach to
						pEffect->SetAttachCreatureID( targetCreature->GetID() );
						
						#ifdef OUTPUT_DEBUG
						
							if (m_MoveDevice==MOVE_DEVICE_RIDE)
							{
								
							}
							else
							{	
								//-------------------------------------------------
								// when attacked during the blood-sucking action
								//-------------------------------------------------
								if (GetActionInfoAction(nUsedActionInfo)==ACTION_DAMAGED)
								{
									DEBUG_ADD_FORMAT("[ATTACH] damaged. id=%d, ai=%d, est=%d, frames=%d", m_ID, nUsedActionInfo, (int)effectSpriteType, (int)castingFrames);
									DEBUG_ADD_FORMAT("Action[%d]=%d/%d, Move=%d/%d, Frame=%d", m_Action, m_ActionCount, GetActionCountMax(), m_MoveCount, m_MoveCountMax, GetFrame());

									g_bCheckError = true;
								}
							}
						#endif
					}
				}
			}

			//-------------------------------------------------------------
			// show Casting ActionInfo
			//-------------------------------------------------------------
			TYPE_ACTIONINFO castingActionInfo = (*g_pActionInfoTable)[nUsedActionInfo].GetCastingActionInfo();
			if (castingActionInfo!=ACTIONINFO_NULL)
			{
				#ifdef OUTPUT_DEBUG
					
					DEBUG_ADD_FORMAT("AffectUsedActionInfo(%d)", (int)castingActionInfo);
					
					AffectUsedActionInfo(castingActionInfo);					

					DEBUG_ADD("castingActionInfo OK");

				#else

					AffectUsedActionInfo(castingActionInfo);

				#endif
			}

			//-------------------------------------------------------------
			// sound matching the casting action
			//-------------------------------------------------------------
			WorldEvents::PlaySoundAt( GetCastingSoundID(nUsedActionInfo) ,
						false,
						m_X, m_Y);
		}
	}

	#ifdef OUTPUT_DEBUG					
		if (g_bCheckError)
		{
			DEBUG_ADD_FORMAT("[CheckError] AttachCastingEffect OK~~");
		}
	#endif
}

//----------------------------------------------------------------------
// Set Underground Creature
//----------------------------------------------------------------------
void	
MCreature::SetUndergroundCreature()		
{ 
	if (m_pZone!=NULL)
	{
		ZoneState::UnSetServerBlockIn(m_pZone, m_MoveType, m_ServerX, m_ServerY);
	}

	m_MoveType = CREATURE_UNDERGROUND; 
	m_Z = 0;

	CheckRegen();
}

//----------------------------------------------------------------------
// Set Ground Creature
//----------------------------------------------------------------------
void	
MCreature::SetGroundCreature()				
{ 
	if (m_pZone!=NULL)
	{
		ZoneState::UnSetServerBlockIn(m_pZone, m_MoveType, m_ServerX, m_ServerY);
	}

	m_MoveType = CREATURE_GROUND; 
	m_Z=0; 

	CheckRegen();
}

//----------------------------------------------------------------------
// Set Flying Creature
//----------------------------------------------------------------------
void	
MCreature::SetFlyingCreature()				
{ 
	if (m_pZone!=NULL)
	{
		ZoneState::UnSetServerBlockIn(m_pZone, m_MoveType, m_ServerX, m_ServerY);
	}

	m_MoveType = CREATURE_FLYING; 
	m_Z = (*g_pCreatureTable)[m_CreatureType].FlyingHeight; 
	
	CheckRegen();
}

//----------------------------------------------------------------------
// Action Effect
//----------------------------------------------------------------------
void	
MCreature::ActionEffect()
{	
	//-------------------------------------------------------------
	// when the skill to use is set..
	//-------------------------------------------------------------
	if (m_nUsedActionInfo!=ACTIONINFO_NULL)
	{
		BOOL	bActionStand	= (m_Action==ACTION_STAND || (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND) || (IsOusters() && m_Action==ACTION_OUSTERS_FAST_MOVE_STAND));
		int		actionCountMax_1 = GetActionCountMax()-1;
		BOOL	bStartAction	= (m_ActionCount==0);
		BOOL	bEndAction		= (m_ActionCount==actionCountMax_1) || bActionStand;
		int		StartFrame		= (*g_pActionInfoTable)[m_nUsedActionInfo].GetStartFrame( m_WeaponSpeed );

		// Cases when the Effect starts..
		// (1) when it is StartFrame
		// (2) when it is the last ActionFrame
		BOOL bStartEffect = m_ActionCount==StartFrame || 
							(StartFrame >= GetActionCountMax() && bEndAction);

		bStartAction |= (*g_pActionInfoTable)[m_nUsedActionInfo].IsUseRepeatFrame()
					&& m_RepeatCount!=0
					&& m_ActionCount == GetActionInfoRepeatStartFrame( m_nUsedActionInfo );

			
		//BOOL	bCastingEffect	= GetActionInfoCastingStartFrame(m_nUsedActionInfo)==m_ActionCount;

		//-------------------------------------------------------------
		// start action
		//-------------------------------------------------------------
		if (bStartAction)
		{
			//------------------------------------------------
			// play the sound matching the skill's action.
			//------------------------------------------------
			//g_Sound.Play( g_SoundTable[(*g_pActionInfoTable)[m_nUsedActionInfo].GetSoundID()].pDSBuffer );
			//PlaySound( (*g_pActionInfoTable)[m_nUsedActionInfo].GetSoundID() ,
			//			false,
			//			m_X, m_Y);

			//------------------------------------------------
			// play the Sound matching the character's Action.
			//------------------------------------------------
			WorldEvents::PlaySoundAt(
				(*g_pCreatureTable)[m_CreatureType].GetActionSound( 
					GetActionInfoAction(m_nUsedActionInfo)
				),
				false,
				m_X, m_Y
			);
		}

		//-------------------------------------------------------------
		// Casting Effect
		//-------------------------------------------------------------
		AttachCastingEffect( m_nUsedActionInfo );	

		#ifdef OUTPUT_DEBUG					
			if (g_bCheckError)
			{
				DEBUG_ADD("[CheckError] ActionEffect");
			}
		#endif


		//-------------------------------------------------------------
		// if it is a stop action..
		// show the effect immediately.
		//-------------------------------------------------------------
		if (bActionStand)
		{
			m_ActionCount = GetActionCountMax()-1;
		}

		//-------------------------------------------------------------
		// when the skill must be shown
		//-------------------------------------------------------------
		if (bStartEffect)
		{
			AffectUsedActionInfo(m_nUsedActionInfo);
			m_nUsedActionInfo	= ACTIONINFO_NULL;
		}
	}
	else
	{
		//if (m_ActionCount==m_ActionCountMax-1)		
		{
			//--------------------------------------------------------
			// end of Action
			//--------------------------------------------------------	
			//m_Action			= ACTION_STAND;
		}
	}

	m_ActionCount++;

	//--------------------------------------------------------	
	// temporarily stopping the blood-sucking action..
	//--------------------------------------------------------	
	if (m_bStopBloodDrain)
	{
		StopBloodDrain();
	}
	if (m_bStopAbsorbSoul)
	{
		StopAbsorbSoul();
	}

	#ifdef OUTPUT_DEBUG					
		if (g_bCheckError)
		{
			DEBUG_ADD("[CheckError] ActionEffect OK");
		}
	#endif
}

//----------------------------------------------------------------------
// Affect Used ActionInfo
//----------------------------------------------------------------------
// Express that a skill was used.
//----------------------------------------------------------------------
void
MCreature::AffectUsedActionInfo(TYPE_ACTIONINFO nUsedActionInfo)
{
	bool	bMonster = !IsPlayerOnly();
	bool	SpecialID = false;
	#ifdef OUTPUT_DEBUG					
		if (g_bCheckError)
		{
			DEBUG_ADD_FORMAT("[CheckError] AffectUsedActionInfo(%d)", nUsedActionInfo);
		}
	#endif

	if (nUsedActionInfo==ACTIONINFO_NULL)
	{
		#ifdef OUTPUT_DEBUG					
			if (g_bCheckError)
			{
				DEBUG_ADD("[CheckError] AffectUsedActionInfo NULL -> return");
			}
		#endif

		return;
	}

	DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d %d",__LINE__,nUsedActionInfo);

	if ((*g_pActionInfoTable)[nUsedActionInfo].GetSize()!=0)
	{
		POINT point;	

		//--------------------------------------------------------
		// Is it a Casting ActionInfo?
		//--------------------------------------------------------
		BOOL bCastingAction = (*g_pActionInfoTable)[nUsedActionInfo].IsCastingAction();
		DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);


		//--------------------------------------------------------
		// target position Pixel coordinate
		//--------------------------------------------------------
		point = MapToPixel(m_TraceX, m_TraceY);
		//point.x += m_sX;
		//point.y += m_sY;
		//point.x += TILE_X_HALF;

		//--------------------------------------------------------
		//
		//					set Effect target
		//
		//--------------------------------------------------------
		MEffectTarget* pEffectTarget = NULL;

		//--------------------------------------------------------
		// for a casting action
		//--------------------------------------------------------
		if (bCastingAction)
		{
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
			pEffectTarget = new MEffectTarget( (*g_pActionInfoTable)[nUsedActionInfo].GetSize() );
			pEffectTarget->NewEffectID();		
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);

			if( nUsedActionInfo == CASTING_FIRE || nUsedActionInfo == CASTING_WATER ||
				nUsedActionInfo == CASTING_GROUND || nUsedActionInfo == CASTING_FIRE2 ||
				nUsedActionInfo == CASTING_GROUND2 || nUsedActionInfo == CASTING_WATER2 )
			{
				pEffectTarget->Set( point.x, point.y, m_TraceZ, GetID() );
			} else
			{
				
				pEffectTarget->Set( point.x, point.y, m_TraceZ, m_TraceID );				
			}
			pEffectTarget->SetDelayFrame( m_EffectDelayFrame );

			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);

			// no result handling is needed in castingAction.
		}
		//--------------------------------------------------------
		// when it is not a casting action.. an actual skill..
		//--------------------------------------------------------
		else
		{
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
			pEffectTarget = new MEffectTarget( (*g_pActionInfoTable)[nUsedActionInfo].GetSize() );
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d %d",__LINE__,(*g_pActionInfoTable)[nUsedActionInfo].GetSize());

			pEffectTarget->NewEffectID();
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d %d",__LINE__,m_nUsedActionInfo);

			//pEffectTarget->Set( point.x, point.y, m_TraceZ, m_TraceID );
			
//			if(m_nUsedActionInfo == MAGIC_ACID_TOUCH || m_nUsedActionInfo == MAGIC_BLOODY_NAIL 
//				|| m_nUsedActionInfo == SKILL_FLOURISH || m_nUsedActionInfo == SKILL_ABSORB_SOUL)
			if(m_nUsedActionInfo != ACTIONINFO_NULL && (*g_pActionInfoTable)[m_nUsedActionInfo].IsAttachSelf() )
			{
				SpecialID = true;
//				if(!bMonster)
				{
					DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
					pEffectTarget->Set( point.x, point.y, m_TraceZ, GetID() );					
				}
//				else
//					pEffectTarget->Set( point.x, point.y, m_TraceZ, m_TraceID );

				DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
			}
			else
			{
				DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
				pEffectTarget->Set( point.x, point.y, m_TraceZ, m_TraceID );
			}
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);

			//--------------------------------------------------------
			// set duration
			//--------------------------------------------------------

			if (m_DelayActionInfo==nUsedActionInfo)
			{
				DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
				pEffectTarget->SetDelayFrame( m_EffectDelayFrame );
				m_DelayActionInfo	= ACTIONINFO_NULL;
				m_EffectDelayFrame	= 0;
				DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
			}
			
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
			// set result: leave result handling to EffectGenerator.
			pEffectTarget->SetResult( m_pActionResult );
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
			
			// clear the set result.
			m_pActionResult = NULL;
		}

		//--------------------------------------------------------
		//
		//					Decide the start position.
		//
		// Default to the target position. An entry whose start flag is
		// only "sky" (MAGIC_METEOR, RESULT_MAGIC_METEOR and SKILL_ERUPTION
		// in the shipped Action.inf) matches neither branch below, and
		// used to hand uninitialised x/y to the effect generator.
		//
		//--------------------------------------------------------
		int x = point.x, y = point.y, z = m_Z, direction;

		DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
		//--------------------------------------------------------
		// Start at the user's position
		//--------------------------------------------------------
		if ((*g_pActionInfoTable)[nUsedActionInfo].IsStartUser())
		{
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
			point = MapToPixel(m_X, m_Y);
			point.x += m_sX;
			point.y += m_sY;
			//point.x += TILE_X_HALF;

			x			= point.x;
			y			= point.y;
			z			= m_Z;//+60;			
		}
		//--------------------------------------------------------
		// Start at the target's position
		//--------------------------------------------------------
		else if ((*g_pActionInfoTable)[nUsedActionInfo].IsStartTarget())
		{
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
			x			= point.x;
			y			= point.y;
			z			= m_Z;//+60;			
		}

		//--------------------------------------------------------
		// Start in the sky
		//--------------------------------------------------------
		if ((*g_pActionInfoTable)[nUsedActionInfo].IsStartSky())
		{
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
			z	= m_Z + (*g_pActionInfoTable)[nUsedActionInfo].GetValue();

			direction	= DIRECTION_DOWN;
		}
		//--------------------------------------------------------
		// Start on the ground
		//--------------------------------------------------------
		else
		{
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
			direction	= m_Direction;
		}

		//--------------------------------------------------------
		//
		//                   Effect creation
		//
		//--------------------------------------------------------
		
		DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
		(*g_pEffectGeneratorTable).Generate(
				x,y,z,				// start position
				direction, 		// direction
		//		1,					// power
				SpecialID ? bMonster ? 2 : 1 : 1,
				nUsedActionInfo,		//	ActionInfoTable type,
				pEffectTarget,		// target info
				GetActionGrade()	
		);
		DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
		ClearActionGrade();		

		DEBUG_ADD_FORMAT("[Creature::AffectUsedActionInfo] ai=%d, id=%d", nUsedActionInfo, m_ID);		
	}
	else
	{
		//------------------------------------------------------------
		// Is it time to handle the result? - assume of course *_*;
		//------------------------------------------------------------				
		DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
		if (m_pActionResult != NULL)
		{					
			// (!) the m_pActionResult value can change in Execute
			//		it must be saved and then deleted.
			MActionResult* pOldResult = m_pActionResult;

			m_pActionResult = NULL;
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);

			pOldResult->Execute();
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);

			delete pOldResult;	
			DEBUG_ADD_FORMAT("[AffectUsedActionInfo] %d",__LINE__);
		}				
	}

	//--------------------------------------------------------
	// end of Action
	//--------------------------------------------------------	
	//m_Action			= ACTION_STAND;	


	#ifdef OUTPUT_DEBUG					
		if (g_bCheckError)
		{
			DEBUG_ADD_FORMAT("[CheckError] AffectUsedActionInfo OKOK ~~");
		}
	#endif
}

//----------------------------------------------------------------------
// Affect Buffered ActionInfo
//----------------------------------------------------------------------
// If there is NextUsedActionInfo
// Apply all of UsedActionInfo --> NextUsedActionInfo.
//
// Do not handle the case with only UsedActionInfo.
//----------------------------------------------------------------------
void
MCreature::AffectBufferedActionInfo()
{
	DEBUG_ADD_FORMAT("[AffectBufferedActionInfo] ai=%d, nai=%d, action=%d-%d/%d", m_nUsedActionInfo, m_nNextUsedActionInfo, (int)m_Action, (int)m_ActionCount, (int)GetActionCountMax());
	
	//--------------------------------------------------------
	// if there is a skill to use next... apply it immediately.
	//--------------------------------------------------------
	if (m_nNextUsedActionInfo!=ACTIONINFO_NULL)
	{	
		TYPE_ACTIONINFO nextActionInfo = m_nNextUsedActionInfo;
		DEBUG_ADD("[AffectBufferedActionInfo] Into Here");
		
		//--------------------------------------------------------
		// when there is a skill to use now
		//--------------------------------------------------------
		if (m_nUsedActionInfo!=ACTIONINFO_NULL)
		{
			DEBUG_ADD("[AffectBufferedActionInfo] AttachCastingEffect" );
			
			m_nNextUsedActionInfo = ACTIONINFO_NULL;
		
			// apply m_nUsedActionInfo
			// --> m_nNextUsedActionInfo may change inside.

	// In this case a CastingEffect must always (!) be attached.
			AttachCastingEffect( m_nUsedActionInfo, TRUE );

			#ifdef OUTPUT_DEBUG					
				if (g_bCheckError)
				{
					DEBUG_ADD("[CheckError] AffectBufferedActionInfo");
				}
			#endif

			AffectUsedActionInfo( m_nUsedActionInfo );

			#ifdef OUTPUT_DEBUG					
				if (g_bCheckError)
				{
					DEBUG_ADD("[CheckError] AffectUsedActionInfo OK");
				}
			#endif

			m_nUsedActionInfo	= ACTIONINFO_NULL;			

			// if after affect.. a skill to use appears..
			if (m_nNextUsedActionInfo!=ACTIONINFO_NULL)
			{	
				#ifdef OUTPUT_DEBUG					
					if (g_bCheckError)
					{
						DEBUG_ADD_FORMAT("[CheckError] AttachCastingEffect( %d )", m_nNextUsedActionInfo);

						AttachCastingEffect( m_nNextUsedActionInfo, TRUE );

						DEBUG_ADD_FORMAT("[CheckError] AffectUsedActionInfo( %d )", m_nNextUsedActionInfo);

						AffectUsedActionInfo( m_nNextUsedActionInfo );

						DEBUG_ADD_FORMAT("[CheckError] m_nNextUsedActionInfo!=ACTIONINFO_NULL OK");
					}
					else
					{
						AttachCastingEffect( m_nNextUsedActionInfo, TRUE );
						AffectUsedActionInfo( m_nNextUsedActionInfo );
					}
				#else
					AttachCastingEffect( m_nNextUsedActionInfo, TRUE );
					AffectUsedActionInfo( m_nNextUsedActionInfo );
				#endif
			}
		}
		
		//--------------------------------------------------------
		// apply the skill to use next
		//--------------------------------------------------------
		m_nNextUsedActionInfo = ACTIONINFO_NULL;

		// --> m_nNextUsedActionInfo may change inside.
		DEBUG_ADD("~~~~~zzzz");
		AttachCastingEffect( nextActionInfo, TRUE );
		DEBUG_ADD("~~~~~zzzz2");
		AffectUsedActionInfo( nextActionInfo );
			
		// if after affect.. a skill to use appears..
		if (m_nNextUsedActionInfo!=ACTIONINFO_NULL)
		{	
			DEBUG_ADD("~~~~~zzzz3");
			AttachCastingEffect( m_nNextUsedActionInfo, TRUE );
			AffectUsedActionInfo( m_nNextUsedActionInfo );
		}

		m_nUsedActionInfo = ACTIONINFO_NULL;
		
		m_nNextUsedActionInfo = ACTIONINFO_NULL;

		#ifdef OUTPUT_DEBUG					
			if (g_bCheckError)
			{
				DEBUG_ADD("[CheckError] AffectBufferedActionInfo OKOK -_-;");
			}
		#endif
	}
	//--------------------------------------------------------
	// Is it the case with only the skill to use now?
	//--------------------------------------------------------
	else if (m_nUsedActionInfo!=ACTIONINFO_NULL)
	{
		DEBUG_ADD("[AFFE] USEDACTIONINFO != NULL ");
		m_nNextUsedActionInfo = ACTIONINFO_NULL;
	
		// apply m_nUsedActionInfo
		// --> m_nNextUsedActionInfo may change inside.

		// In this case a CastingEffect must always (!) be attached.
		DEBUG_ADD("[AFFE] CASTING EFF");
		AttachCastingEffect( m_nUsedActionInfo, TRUE );
		DEBUG_ADD("[AFFE] AFFECT USED");
		AffectUsedActionInfo( m_nUsedActionInfo );

		m_nUsedActionInfo	= ACTIONINFO_NULL;			

		// if after affect.. a skill to use appears..
		if (m_nNextUsedActionInfo!=ACTIONINFO_NULL)
		{			
			DEBUG_ADD("[AFFE] ZZZZZZ ");
			AttachCastingEffect( m_nNextUsedActionInfo, TRUE );
			AffectUsedActionInfo( m_nNextUsedActionInfo );
		}	

		//--------------------------------------------------------
		// apply the skill to use next
		//--------------------------------------------------------
		m_nUsedActionInfo = ACTIONINFO_NULL;
		m_nNextUsedActionInfo = ACTIONINFO_NULL;
	}
}

//----------------------------------------------------------------------
// Is Recovery HP
//----------------------------------------------------------------------
inline BOOL				
MCreature::IsRecoveryHP()	
{ 
	return m_RecoveryHPTimes > 0;
	//return m_RecoveryPart==MODIFY_CURRENT_HP; 
}

//----------------------------------------------------------------------
// Is Recovery MP
//----------------------------------------------------------------------
inline BOOL				
MCreature::IsRecoveryMP()	
{ 
	return m_RecoveryMPTimes > 0;
	//return m_RecoveryPart==MODIFY_CURRENT_MP;
}		

//----------------------------------------------------------------------
// Set Recovery
//----------------------------------------------------------------------
// Recover amount, times times, every delay.
//----------------------------------------------------------------------
void				
MCreature::SetRecoveryHP(int amount, int times, DWORD delay)
{ 
	if (times > 0)
	{
		// recover once immediately

		if(! IsRecoveryHP() )					// if current HP is not filling
		{
			SetStatus( MODIFY_CURRENT_HP, GetHP()+amount );
			m_RecoveryHPTimes = times - 1; 
		} else
			m_RecoveryHPTimes = times;

		m_RecoveryHPAmount = amount; 
		m_RecoveryHPDelayTime = delay;
		//m_RecoveryHPPart = MODIFY_CURRENT_HP;

		// the next recovery time
		m_RecoveryHPNextTime = g_FrameNow + MonotonicClock::Millis(m_RecoveryHPDelayTime);
	}
}

//----------------------------------------------------------------------
// Set Recovery
//----------------------------------------------------------------------
// Recover amount, times times, every delay.
//----------------------------------------------------------------------
void				
MCreature::SetRecoveryMP(int amount, int times, DWORD delay)
{ 
	// 2004, 9, 3, sobeit add start - if hit by the Pleasure Station, MP doesn't fill...-_-
	if(HasEffectStatus(EFFECTSTATUS_PLEASURE_EXPLOSION))
		return;
	// 2004, 9, 3, sobeit add end
	if (times > 0)
	{
		// recover once immediately
		if(! IsRecoveryMP() )
		{
			SetStatus( MODIFY_CURRENT_MP, GetMP()+amount );
			m_RecoveryMPTimes = times-1; 
		} else
			m_RecoveryMPTimes = times; 

		m_RecoveryMPAmount = amount; 		
		m_RecoveryMPDelayTime = delay;
		//m_RecoveryMPPart = MODIFY_CURRENT_MP;

		// the next recovery time
		m_RecoveryMPNextTime = g_FrameNow + MonotonicClock::Millis(m_RecoveryMPDelayTime);
	}
}

//----------------------------------------------------------------------
// Check Drop Blood
//----------------------------------------------------------------------
// When HP is low.. bleed
//----------------------------------------------------------------------
void				
MCreature::CheckDropBlood()
{
	if (!HasEffectStatus(EFFECTSTATUS_COMA))
	{
		//----------------------------------------------------------
		// time to bleed?
		//----------------------------------------------------------
		if (GameOptions::BloodDrop()
			&& g_FrameNow > m_NextBloodingTime)
		{
			int currentHP = m_Status[MODIFY_CURRENT_HP];
			int maxHP = m_Status[MODIFY_MAX_HP];

			int percentHP = ((maxHP==0)? 0 : currentHP*100 / maxHP);

			//----------------------------------------------------------
			// bleed when HP is 30% or below.
			//----------------------------------------------------------
			if (percentHP <= g_pClientConfig->BLOOD_DROP_HP_PERCENT)
			{
				WorldEvents::ExecuteActionInfoFromMainNode(
							BLOOD_DROP_GROUND,										// skill number used
						
							m_X, m_Y, 0,
							(int)m_Direction,														// direction used
							
							m_ID,												// info about the target
							m_X, m_Y, 0,
							
							80, //5*16, 
							
							NULL,
							
							false,
							
							(int)m_sX, (int)m_sY);	
			}

			//----------------------------------------------------------
			// set the next time bleeding is possible.
			// since checking a bit late doesn't matter, bleed or not..extend time
			//----------------------------------------------------------
			// The product is taken before the remainder, as it always was (0 to
			// 19 ms more); in 64 bits, since rand() reaches 2^31-1 off Windows.
			DWORD timeGap = 300 //+ 1000	//g_pClientConfig->BLOOD_DROP_GAP_TIME
							+ static_cast<DWORD>(static_cast<long long>(percentHP<<2) * rand() % 20);//g_pClientConfig->BLOOD_DROP_RANDOM_TIME;

			//----------------------------------------------------------
			// if blood-drained, bleed a bit less.
			//----------------------------------------------------------
			if (HasEffectStatus(EFFECTSTATUS_BLOOD_DRAIN))
			{
				// bleeds about five times less
				m_NextBloodingTime = g_FrameNow + MonotonicClock::Millis(timeGap*5);
			}
			else
			{			
				m_NextBloodingTime = g_FrameNow + MonotonicClock::Millis(timeGap);
			}
		}
	}
}

//----------------------------------------------------------------------
// Add Casket Soon
//----------------------------------------------------------------------
void				
MCreature::AddCasketSoon(int type)
{
	m_bInCasket = true;
	m_CasketCount = 0;
	m_CasketCountInc = 0;
	m_CasketType = type;

	//SetInvisibleSoon();

	// [new skill3] -_-; it would be better to make it virtual..
	if (GetClassType()==CLASS_PLAYER)
	{		
		g_pSkillAvailable->SetAvailableSkills();
	}

	// [new skill7]
	m_bEffectStatus[EFFECTSTATUS_CASKET] = true;
	
	CheckRegen();

	if(PlayerState::Creature() == this)	// For Player, cannot change clothes during casket summon.
	{
		VsUiPresentation::LockGear();
	}
}

//----------------------------------------------------------------------
// Remove Casket Soon
//----------------------------------------------------------------------
void				
MCreature::RemoveCasketSoon()
{
	m_bInCasket = false;
	m_CasketCount = 64;
	m_CasketCountInc = 0;

	//SetVisibleSoon();

	// [new skill7]
	m_bEffectStatus[EFFECTSTATUS_CASKET] = false;
	
	CheckRegen();

	if(PlayerState::Creature() == this)	// For Player, lift the restriction on changing clothes during casket summon.
	{
		DialogPresentation::UnlockGear();
	}
}

//----------------------------------------------------------------------
// Add Casket
//----------------------------------------------------------------------
// The casket gradually appears.
//----------------------------------------------------------------------
void				
MCreature::AddCasket(int type)
{
	AddCasketSoon(type);
//	m_bInCasket = true;
	m_CasketCount = 64;
	m_CasketCountInc = -2;
//	m_CasketType = type;
//
//	//SetInvisible();
//
//	// [new skill3] -_-; it would be better to make it virtual..
//	if (GetClassType()==CLASS_PLAYER)
//	{		
//		g_pSkillAvailable->SetAvailableSkills();
//	}
//
//	// [new skill7]
//	m_bEffectStatus[EFFECTSTATUS_CASKET] = true;
//	
//	CheckRegen();
}

//----------------------------------------------------------------------
// Remove Casket
//----------------------------------------------------------------------
// The casket gradually disappears.
//----------------------------------------------------------------------
void				
MCreature::RemoveCasket()
{
	RemoveCasketSoon();
	m_bInCasket = true;		// must be changed to false when it reaches 64.
	m_CasketCount = 0;
	m_CasketCountInc = 2;

	//SetVisible();

//	// [new skill7]
//	m_bEffectStatus[EFFECTSTATUS_CASKET] = false;
//
//	CheckRegen();
}

//----------------------------------------------------------------------
// Update Casket
//----------------------------------------------------------------------
void				
MCreature::UpdateCasket()
{
	if (m_bInCasket)
	{		
		int incValue = m_CasketCountInc;

		if (m_CasketCount < 48)
		{
			if (m_CasketCount < 32)
			{
				incValue *= 3;
			}
			else
			{
				incValue *= 2;
			}
		}
		m_CasketCount += incValue;
	
		if (m_CasketCount<0)
		{
			m_CasketCount = 0;
			m_CasketCountInc = 0;	
			
			CheckRegen();
		}
		else if (m_CasketCount>64)
		{
			m_bInCasket = false;
			m_CasketCount = 64;
			m_CasketCountInc = 0;

			// [new skill3] - -;;
			if (GetClassType()==CLASS_PLAYER)
			{		
				g_pSkillAvailable->SetAvailableSkills();
			}

			CheckRegen();
		}		
	}
}

//----------------------------------------------------------------------
// Update Status
//----------------------------------------------------------------------
// For now.. HP / MP change
//----------------------------------------------------------------------
void
MCreature::UpdateStatus()
{
	//--------------------------------------------------------
	// do not raise HP in coma state.
	//--------------------------------------------------------
	if (!m_bAlive 
		|| HasEffectStatus(EFFECTSTATUS_COMA))
	{
		return;
	}

	bool bChangedHP = false;

	//--------------------------------------------------------
	// HP recovery
	//--------------------------------------------------------
	if (m_RecoveryHPTimes > 0 
		&& g_FrameNow >= m_RecoveryHPNextTime)
	{		
		m_RecoveryHPTimes--;

		// recovery
		SetStatus( MODIFY_CURRENT_HP, GetStatus(MODIFY_CURRENT_HP)+m_RecoveryHPAmount );

		bChangedHP = true;
		
		// the next recovery time
		m_RecoveryHPNextTime += MonotonicClock::Millis(m_RecoveryHPDelayTime);		
	}

	//--------------------------------------------------------
	// MP recovery
	//--------------------------------------------------------
	if (m_RecoveryMPTimes > 0 
		&& g_FrameNow >= m_RecoveryMPNextTime)
	{		
		m_RecoveryMPTimes--;

		// recovery
		SetStatus( MODIFY_CURRENT_MP, GetStatus(MODIFY_CURRENT_MP)+m_RecoveryMPAmount );

		// the next recovery time
		m_RecoveryMPNextTime += MonotonicClock::Millis(m_RecoveryMPDelayTime);		
	}

	//--------------------------------------------------------
	// automatic recovery for vampires
	//--------------------------------------------------------
	if (m_RegenAmount > 0)
	{
		//--------------------------------------------------------
		// basic recovery
		//--------------------------------------------------------
		if (g_FrameNow >= m_RegenNextTime)
		{		
			// [new skill4] HP regen doesn't work when mephisto is applied.

			//if ( !m_bEffectStatus[EFFECTSTATUS_MEPHISTO] || !m_bEffectStatus[EFFECTSTATUS_MEPHISTO]&&IsInCasket() )
			{
				// when inside a casket, recover starting from silver damage.
				if( IsInCasket() )
				{
					DWORD SilverDamage = GetStatus( MODIFY_SILVER_DAMAGE );
					DWORD Value = m_RegenAmount;
					
					if( SilverDamage > 0 )
					{
						if( SilverDamage < Value )
							Value = SilverDamage;
						
						SetStatus( MODIFY_SILVER_DAMAGE, SilverDamage - Value );
					} else
					{
						SetStatus( MODIFY_CURRENT_HP, GetStatus( MODIFY_CURRENT_HP ) + Value );
					}
				}
				else if( !m_bEffectStatus[EFFECTSTATUS_MEPHISTO] )
					SetStatus( MODIFY_CURRENT_HP, GetStatus(MODIFY_CURRENT_HP)+m_RegenAmount );

				bChangedHP = true;
			}

			m_RegenNextTime += MonotonicClock::Millis(m_RegenDelayTime);		
			

			//--------------------------------------------------------
			// the bonus recovery
			//--------------------------------------------------------
			if (g_FrameNow >= m_RegenBonusNextTime)
			{		
				if( GetStatus( MODIFY_SILVER_DAMAGE ) > 0 && IsInCasket() )
				{
					// Down to none: the damage is unsigned, so the difference is taken signed.
					const long long left = static_cast<long long>(GetStatus( MODIFY_SILVER_DAMAGE )) - m_RegenBonusAmount;
					SetStatus( MODIFY_SILVER_DAMAGE, left > 0 ? static_cast<DWORD>(left) : 0 );
				}
				else
					SetStatus( MODIFY_CURRENT_HP, GetStatus(MODIFY_CURRENT_HP)+m_RegenBonusAmount );

				//bChangedHP = true;

				m_RegenBonusNextTime += MonotonicClock::Millis(m_RegenBonusDelayTime);		
			}
		}		
	}

	//--------------------------------------------------------
	// in a party, set HP again.
	//--------------------------------------------------------
	if (bChangedHP)
	{	
		if (IsPlayerParty())
		{
			PARTY_INFO* pInfo = g_pParty->GetMemberInfo( GetName() );
				
			// correct the coordinate.
			if (pInfo!=NULL)
			{
				pInfo->HP = static_cast<WORD>(m_Status[MODIFY_CURRENT_HP]);
			}
		}
	}

}

//----------------------------------------------------------------------
// Update Invisible
//----------------------------------------------------------------------
void
MCreature::UpdateInvisible()
{
	if (m_InvisibleStep)
	{
		m_InvisibleCount += m_InvisibleStep;

		if (m_InvisibleCount>=64 || m_InvisibleCount<=0)
		{
			m_InvisibleStep = 0;
		}
	}
}

//----------------------------------------------------------------------
// Take the next action.
//----------------------------------------------------------------------
void
MCreature::Action()
{	
	#ifdef OUTPUT_DEBUG						
		g_bCheckError = false;


		//if (rand()%8==5)
		{
		//	PlaySound( rand()%g_pSoundTable->GetSize() );
		}
	#endif

	//--------------------------------------------------------
	// when the chat string darkens
	//--------------------------------------------------------
	if (m_NextChatFadeTime < g_FrameNow)
	{
		FadeChatString();		
	}
	
	//--------------------------------------------------------
	// Status change	- HP, MP change
	//--------------------------------------------------------
	UpdateStatus();
	
//	// 2004, 08, 05 sobeit add start
//	if( GetCreatureType() == 726 ||  GetCreatureType() == 727 || GetCreatureType() == 728 || GetCreatureType() == 729) // castle gate
//		return; // going further down has little to do.
//	// 2004, 08, 05 sobeit add end

	//--------------------------------------------------------
	// special effects
	//--------------------------------------------------------
//	if(UpDateInstallTurret())
//		return;
	UpDateInstallTurret();

	UpdateInvisible();
	UpdateCasket();
	UpdateCutHeight();
	UpdateTurning();
	UpdateCauseCriticalWounds();
	UpdateGhost();
//	UpdateBloodyZenith();
	UpdateGunShotGuidance();
	UpdateFakeDie();
	UpdateBurningSol();

	UpdateBikeCrash();
	if( !HasEffectStatus( EFFECTSTATUS_FIRE_ELEMENTAL ) && !HasEffectStatus( EFFECTSTATUS_WATER_ELEMENTAL ) && GetElementalID() != OBJECTID_NULL && ZoneState::Exists())
	{
		ZoneState::RemoveFakeCreature(GetElementalID());
	}
	
	if( GetCreatureType() == 672 )
	{
		if( HasEffectStatus( EFFECTSTATUS_KEEP_SWEEPER ) ||
			HasEffectStatus( EFFECTSTATUS_KEEP_SWEEPER_2 ) ||
			HasEffectStatus( EFFECTSTATUS_KEEP_SWEEPER_3 ) ||
			HasEffectStatus( EFFECTSTATUS_KEEP_SWEEPER_4 ) ||
			HasEffectStatus( EFFECTSTATUS_KEEP_SWEEPER_5 ) ||
			HasEffectStatus( EFFECTSTATUS_KEEP_SWEEPER_6 ) ||
			HasEffectStatus( EFFECTSTATUS_KEEP_SWEEPER_7 ) ||
			HasEffectStatus( EFFECTSTATUS_KEEP_SWEEPER_8 ) ||
			HasEffectStatus( EFFECTSTATUS_KEEP_SWEEPER_9 ) ||
			HasEffectStatus( EFFECTSTATUS_KEEP_SWEEPER_10 ) ||
			HasEffectStatus( EFFECTSTATUS_KEEP_SWEEPER_11 ) ||
			HasEffectStatus( EFFECTSTATUS_KEEP_SWEEPER_12 )
			)
		{
			if( GetAction() != ACTION_MOVE )
			{
				m_Action = ACTION_MOVE;				
			}
			if(GetActionCountMax() != 0)
			{
				m_ActionCount = g_CurrentFrame % GetActionCountMax();
			}
		}
		else
		{
			m_Action = ACTION_STAND;
			m_ActionCount = 0;
		}
		
		return;
	} 
	else if( GetCreatureType() == 675 )
	{
		m_Action = ACTION_MAGIC;
		m_ActionCount = 0;		
	}
	//UpdateDarkness();
	
	//--------------------------------------------------------
	// make direction change natural
	//--------------------------------------------------------
	if (!ChangeNearDirection())
	{
			//--------------------------------------------------------
			// when there was no need to change direction..
			// change direction once in a while when idle.. - -;
			//--------------------------------------------------------			
			if (// not a Player, and
//				m_CreatureType >= 4
				!IsCreatureTypeVampirePC(m_CreatureType)
				&& m_CreatureType != CREATURETYPE_SLAYER_OPERATOR
				&& m_CreatureType != CREATURETYPE_VAMPIRE_OPERATOR
				&& m_CreatureType != CREATURETYPE_OUSTERS_OPERATOR
				&& m_CreatureType != CREATURETYPE_SLAYER_MALE
				&& m_CreatureType != CREATURETYPE_SLAYER_FEMALE
				&& m_CreatureType != CREATURETYPE_OUSTERS
				// not an air Bathory, and
				&& m_CreatureType != 431
				// not an NPC... (because not all directions have pictures -_-;)
				&& !IsNPC()
				// alive, and..
				&& m_bAlive
				// stopped, and
				&& m_Action==ACTION_STAND
				// has nowhere to move, and
				&& m_listMoveBuffer.size()==0
				// at the end of the stop action..
				&& m_ActionCount>=GetActionCountMax()-1
				// randomly.. - -;
				&& (rand() % 5)==0
				&& !HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET)
				)
			{
				m_Direction = ((int)m_Direction + 8 + rand()%5-2) % 8;
			}
	}
	//m_CurrentDirection = m_Direction;

	//--------------------------------------------------------
	// if some effect is applied, it moves 2 times slower.
	//--------------------------------------------------------
	if (HasEffectStatus( EFFECTSTATUS_HAS_SLAYER_RELIC ) || HasEffectStatus( EFFECTSTATUS_HAS_VAMPIRE_RELIC ) || 
		HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_GREGORI )|| HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_NEMA ) ||
		HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_LEGIOS ) || HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_MIHOLE ) ||
		HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_AROSA )  || HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_ARMEGA ) ||
		HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_INI )    || HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_JAVE ) ||
		HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_CHASPA ) || HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_CONCILIA ) ||
		HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_KIRO )   || HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_HILLEL ) ||
		HasEffectStatus( EFFECTSTATUS_AMBER_OF_GUARD ) || HasEffectStatus( EFFECTSTATUS_AMBER_OF_AVENGER ) ||
		HasEffectStatus( EFFECTSTATUS_AMBER_OF_IMMORTAL ) || HasEffectStatus( EFFECTSTATUS_AMBER_OF_CURSE ) ||
		HasEffectStatus( EFFECTSTATUS_HAS_CASTLE_SYMBOL_5 ) || HasEffectStatus( EFFECTSTATUS_HAS_CASTLE_SYMBOL_6 ) ||
		(IsCreatureMove( this ) && HasEffectStatus( EFFECTSTATUS_ICE_FIELD_TO_CREATURE )) ||
		(IsCreatureMove( this ) && HasEffectStatus( EFFECTSTATUS_FROZEN_ARMOR_TO_ENEMY )) ||
		(IsCreatureActionAttack( this ) && (	HasEffectStatus( EFFECTSTATUS_ICE_OF_SOUL_STONE ) || (IsInFuryOfGnome() && !IsOusters()/*(IsSlayer() || IsVampire())*/)	)) ||
		(IsCreatureMove( this ) && HasEffectStatus( EFFECTSTATUS_JABBING_VEIN )) ||
		HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER ) || HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_2 ) ||
		HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_3 ) || HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_4 ) ||
		HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_5 ) || HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_6 ) ||
		HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_7 ) || HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_8 ) ||
		HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_9 ) || HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_10 ) ||
		HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_11 ) || HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_12 )
		)
	{
		if (g_CurrentFrame & 0x01)
		{
			return;
		}
	}

//	BOOL bSlayer = IsSlayer();

	//--------------------------------------------------------
	// KnockBack handling 2001.10.9
	//--------------------------------------------------------
	if (m_bKnockBack > 0)
	{
		m_sX += m_cX;
		m_sY += m_cY;

		// when knockBack is all done, stop action.
		if (--m_bKnockBack==0)
		{
			m_sX = 0;
			m_sY = 0;

			SetAction( ACTION_STAND );

			return;
		}			
	}
	//--------------------------------------------------------
	// do not take an action during FastMove.
	//--------------------------------------------------------
	else if (!m_bFastMove)
	{
		if (m_nNextUsedActionInfo!=ACTIONINFO_NULL)
		{	
			int bOldNextAction = m_NextAction;

			// after running affectUsedActionInfo
			// NextActionInfo may change.
			TYPE_ACTIONINFO nextUsedActionInfo = m_nNextUsedActionInfo;
			TYPE_ACTIONINFO nextNextUsedActionInfo = ACTIONINFO_NULL;

			//--------------------------------------------------------
			// if there was a skill in use before... apply it immediately.
			//--------------------------------------------------------
			if (m_nUsedActionInfo!=ACTIONINFO_NULL)
			{	
				m_nNextUsedActionInfo = ACTIONINFO_NULL;

				// if castingEffect has not been output yet, output it.
				if (GetActionInfoCastingStartFrame(m_nUsedActionInfo) >= m_ActionCount)
				{
					AttachCastingEffect( m_nUsedActionInfo, TRUE );
				}

				AffectUsedActionInfo( m_nUsedActionInfo );
				m_nUsedActionInfo	= ACTIONINFO_NULL;

				nextNextUsedActionInfo = m_nNextUsedActionInfo;
			}

			m_nUsedActionInfo = nextUsedActionInfo;
			m_nNextUsedActionInfo = nextNextUsedActionInfo;

			// 2001.9.30 added
			if (m_nUsedActionInfo!=ACTIONINFO_NULL)
			{
				int actionInfoAction = GetActionInfoAction(m_nUsedActionInfo);

				// if it is a being-hit action.. blood splatter..
				// 2002.3.19 comment removed.. let's show the effect for any action for now
				//if (actionInfoAction==ACTION_DAMAGED)
				{
					if //(m_Action!=ACTION_STAND 
						//&& (!IsSlayer() || IsSlayer && m_Action!=ACTION_SLAYER_MOTOR_STAND)
						//&& 
						(GetActionInfoCastingStartFrame(m_nUsedActionInfo) <= m_ActionCount)
					{
						AttachCastingEffect( m_nUsedActionInfo, TRUE );

						AffectUsedActionInfo( m_nUsedActionInfo );
				
						// output sound
						WorldEvents::PlaySoundAt(
							(*g_pCreatureTable)[m_CreatureType].GetActionSound( 
								GetActionInfoAction(m_nUsedActionInfo)
							),
							false,
							m_X, m_Y
						);

						m_nUsedActionInfo = ACTIONINFO_NULL;					
					}
				}

				// 2001.05.21 added
				// ACTION_STAND is not shown in skill actions.
				if (actionInfoAction!=ACTION_STAND)
				{
					SetAction( actionInfoAction );
				}
			}				

			//--------------------------------------------------------
			// when trying to move but getting hit and so running a different action
			// there were cases where movement did not happen.
			// Countermeasure --> remember NextAction and, if it is a move.. perform the move
			// 2001.9.24
			//--------------------------------------------------------
			if (bOldNextAction==m_MoveAction)
			{
				SetNextAction( m_MoveAction );
			}	 			
		}
		//--------------------------------------------------------
		// remembered next action...
		//--------------------------------------------------------
		else if (m_bNextAction 
				|| (m_NextAction!=ACTION_STAND 
				&& (!IsOusters() || (IsOusters() && m_NextAction!=ACTION_OUSTERS_FAST_MOVE_STAND))
				&& (!IsSlayer() || (IsSlayer() && m_NextAction!=ACTION_SLAYER_MOTOR_STAND))))
		{
			SetAction( m_NextAction );
		}	 
	}

	//--------------------------------------------------------
	// when paralyzed..
	//--------------------------------------------------------
	if(HasEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS) && !IsSlayer())
	{
		//--------------------------------------------------------
		// when trying to use a skill
		//--------------------------------------------------------
		if (m_nUsedActionInfo!=ACTIONINFO_NULL)
		{
			AffectUsedActionInfo(m_nUsedActionInfo);
			m_nUsedActionInfo	= ACTIONINFO_NULL;
		}
	}
	else
	if (HasEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS)
		|| HasEffectStatus(EFFECTSTATUS_EXPLOSION_WATER)
		|| HasEffectStatus(EFFECTSTATUS_GLACIER)
		)
	{			
		//--------------------------------------------------------
		// when trying to use a skill
		//--------------------------------------------------------
		if (m_nUsedActionInfo!=ACTIONINFO_NULL)
		{
			AffectUsedActionInfo(m_nUsedActionInfo);
			m_nUsedActionInfo	= ACTIONINFO_NULL;
		}

		if (m_ActionCount>=GetActionCountMax())
		{			
			//--------------------------------------------------------
			// upon arriving at the target tile..
			// if it keeps standing.. or has finished walking..
			// show it swaying in place.
			//--------------------------------------------------------
			if (m_MoveCount>=m_MoveCountMax)
			{		
				BOOL bSlayer = IsSlayer();

				if (m_bNextAction 
					|| (m_NextAction!=ACTION_STAND 
					&& (!IsOusters() || (IsOusters() && m_NextAction!=ACTION_OUSTERS_FAST_MOVE_STAND))
					&& (!bSlayer || (bSlayer && m_NextAction!=ACTION_SLAYER_MOTOR_STAND))))
				{
					m_Action = ((m_MoveDevice==MOVE_DEVICE_RIDE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND);
					if(IsOusters() && HasEffectStatusSummonSylph( this ))
						m_Action = ACTION_OUSTERS_FAST_MOVE_STAND;

					SetAction( m_NextAction );
				}	 
				else
				{
					SetAction( ACTION_STAND );
				}
			}
		}
	}
	//--------------------------------------------------------
	// general case
	//--------------------------------------------------------
	else
	{
		//--------------------------------------------------------
		// check bleeding
		//--------------------------------------------------------
		CheckDropBlood();

		//--------------------------------------------------------
		// Action handling
		//--------------------------------------------------------
		if (m_ActionCount < GetActionCountMax())
		{		
			//--------------------------------------------------------
			// when trying to use a skill
			//--------------------------------------------------------
			if (m_nUsedActionInfo!=ACTIONINFO_NULL)
			{
				ActionEffect();
			}
			//--------------------------------------------------------
			// action..
			//--------------------------------------------------------
			else
			{
				// perform the appropriate Action
				//switch (m_Action)
				if (m_Action==ACTION_STAND 
					|| (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND))
				{
					m_ActionCount++;
				}

				/*
				else if (m_Action==ACTION_MOVE
					|| IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_MOVE)
				{
						// the walking action is handled separately.
						//ActionMove();
				}
				*/
				
				//	default :
				else 
				{
					ActionEffect();
				}
			}
		}
		
		//--------------------------------------------------------
		// only if alive
		//--------------------------------------------------------
		if (m_bAlive)
		{
			//--------------------------------------------------------
			// when the Action has ended...
			//--------------------------------------------------------
			if (m_ActionCount>=GetActionCountMax())
			{			
				//--------------------------------------------------------
				// upon arriving at the target tile..
				// if it keeps standing.. or has finished walking..
				// show it swaying in place.
				//--------------------------------------------------------
				if (m_MoveCount>=m_MoveCountMax)
				{		
					//#ifdef CONNECT_SERVER
					//--------------------------------------------------------
					// if there is no next move.. stop it.
					//--------------------------------------------------------
					AffectMoveBuffer();

					BOOL bSlayer = IsSlayer();

					if (m_bNextAction 
						|| (m_NextAction!=ACTION_STAND 
						&& (!IsOusters() || (IsOusters() && m_NextAction!=ACTION_OUSTERS_FAST_MOVE_STAND))
						&& (!bSlayer || (bSlayer && m_NextAction!=ACTION_SLAYER_MOTOR_STAND))))
					{
						m_Action = ((m_MoveDevice==MOVE_DEVICE_RIDE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND);
						if(IsOusters() && HasEffectStatusSummonSylph( this ))
							m_Action = ACTION_OUSTERS_FAST_MOVE_STAND;
						SetAction( m_NextAction );
					}	 
					else
					{
						SetAction( ACTION_STAND );				
					}
					//#else
					//	SetAction( m_MoveAction );				
					//#endif
				}
				//--------------------------------------------------------
				// it showed another action while walking or was walking continuously.
				// walk again.
				//--------------------------------------------------------
				else 
				{			
					m_Action = m_MoveAction;				
					
					ActionMove();

					if (m_MoveCount>=m_MoveCountMax)
					{
						// if there is a next place to move..
						if (m_NextX != SECTORPOSITION_NULL
							&& m_NextDirection != DIRECTION_NULL)
						{		
							MoveNextPosition();
						}
						else
						{
							//#ifdef CONNECT_SERVER

							//--------------------------------------------------------
							// next move
							//--------------------------------------------------------
							AffectMoveBuffer();

							BOOL bSlayer = IsSlayer();

							if (m_bNextAction 
								|| (m_NextAction!=ACTION_STAND 
								&& (!IsOusters() || (IsOusters() && m_NextAction!=ACTION_OUSTERS_FAST_MOVE_STAND) )
								&& (!bSlayer || (bSlayer && m_NextAction!=ACTION_SLAYER_MOTOR_STAND))))
							{
								m_Action = ((m_MoveDevice==MOVE_DEVICE_RIDE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND);
								if(IsOusters() && HasEffectStatusSummonSylph( this ))
									m_Action = ACTION_OUSTERS_FAST_MOVE_STAND;
								SetAction( m_NextAction );
							}	 
							else
							{
								SetAction( ACTION_STAND );				
							}
							//#else
							//	SetAction( m_MoveAction );				
							//#endif
						}
					}
				}
			}
			//--------------------------------------------------------
			// even while showing an Action..
			// if there is something to move?, make it move too. 2001.10.10
			//--------------------------------------------------------
			else
			{		
				/*
				// 2001.11.8 - commented out.. apparently it looks bad.. - -;;
				if (// not KnockBack or FastMove
					!m_bFastMove && m_bKnockBack==0	&& m_MoveCount>0)
				{
					if (m_MoveCount<m_MoveCountMax)
					{
						ActionMove();
					}
					else if (m_MoveCount>=m_MoveCountMax)
					{
						// if there is a next place to move..
						if (m_NextX != SECTORPOSITION_NULL)
						{		
							MoveNextPosition();
						}
					}
				}
				*/
			}
		}
	}

	#ifdef OUTPUT_DEBUG					
		if (g_bCheckError)
		{
			DEBUG_ADD_FORMAT("[CheckError] Action OK");
		}
	#endif		
}

//----------------------------------------------------------------------
// Packet Move (x, y, d)
//----------------------------------------------------------------------
// since this Creature received Move
//
// //0. move it considering coordinate collision with the Player.
//
// move one cell from (x,y) in direction d.
//----------------------------------------------------------------------
void		
MCreature::PacketMove(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, BYTE direction)
{
//	// 2004, 9, 16, sobeit add start - change turret direction, should I remove it..-_-;
//	if(HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
//	{
//		SetCurrentDirection(direction);
//		return;
//	}
//	// 2004, 9, 16, sobeit add end
	//---------------------------------------------------
	// add new movement info to the buffer.
	//---------------------------------------------------
	MoveNode* pNode = new MoveNode;
	pNode->x = x;
	pNode->y = y;
	pNode->direction = direction;

	if (IsPlayerParty())
	{
		PARTY_INFO* pInfo = g_pParty->GetMemberInfo( GetName() );		
			
		// correct the coordinate.
		if (pInfo!=NULL)
		{
			pInfo->zoneID = (g_bZonePlayerInLarge?g_nZoneLarge : g_nZoneSmall);
			pInfo->zoneX = static_cast<BYTE>(x);
			pInfo->zoneY = static_cast<BYTE>(y);
		}
	}

	//---------------------------------------------------
	// apply the Server coordinate immediately.
	//---------------------------------------------------
	///*
	int oldX = m_X;
	int oldY = m_Y;

	m_X = x;
	m_Y = y;

	POINT next;
	GetNextPosition( direction, next );

	m_X = oldX;
	m_Y = oldY;

	// remember server coordinate
	// 2001.8.6
	// 2002.3.22 restored again.
	//m_ServerX	= next.x;
	//m_ServerY	= next.y;
	
	SetServerPositionWithBlock( next.x, next.y );
	//*/

	m_listMoveBuffer.push_back( pNode );

	//-----------------------------------------------------------
	// if stopped, apply immediately.
	//-----------------------------------------------------------
	if (m_Action==ACTION_STAND 
		|| (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND))
	{
		AffectMoveBuffer();
	}
	else
	{
		//---------------------------------------------------
		// apply all MoveBuffer beyond the limit.
		//---------------------------------------------------
		while (m_listMoveBuffer.size() > static_cast<size_t>(g_pClientConfig->MAX_CREATURE_MOVE_BUFFER))
		{
			AffectMoveBuffer();
		}		
	}
}

//----------------------------------------------------------------------
// Release MoveBuffer
//----------------------------------------------------------------------
// erase all existing MoveBuffer
//----------------------------------------------------------------------
void
MCreature::ReleaseMoveBuffer()
{
	MOVENODE_LIST::iterator iNode = m_listMoveBuffer.begin();

	while (iNode != m_listMoveBuffer.end())
	{
		MoveNode* pNode = *iNode;

		delete pNode;

		iNode++;
	}

	m_listMoveBuffer.clear();
}

//----------------------------------------------------------------------
// Affect MoveBuffer All
//----------------------------------------------------------------------
// Apply all MoveBuffer.
//----------------------------------------------------------------------
void
MCreature::AffectMoveBufferAll()
{
	while (m_listMoveBuffer.size() != 0)
	{
		AffectMoveBuffer();
	}
}

//----------------------------------------------------------------------
// Affect Move Buffer
//----------------------------------------------------------------------
// Set one piece of buffered next movement info.
//----------------------------------------------------------------------
bool	
MCreature::AffectMoveBuffer()
{
	//-------------------------------------------
	// when there is no movement info
	//-------------------------------------------
	if (m_listMoveBuffer.size()==0)
	{
		return false;
	}

	//-------------------------------------------
	// read the very first one.
	//-------------------------------------------
	MoveNode* pNode = m_listMoveBuffer.front();
	m_listMoveBuffer.pop_front();

	//-------------------------------------------
	// set value
	//-------------------------------------------
	// originally in PacketMove()
	// cut & paste when adding move buffering - -;;
	// will it work well..
	//-------------------------------------------
	int x = pNode->x;
	int y = pNode->y;
	int direction = pNode->direction;

	delete pNode;

	// when dead
	if (!m_bAlive)
	{
		return false;
	}


	LOG_DEBUG("AffectMoveBuffer : [ID=%d] From(%d,%d) Direction(%d)", m_ID, x,y, direction);

	// remember the previous coordinate.
	int oldX = m_X;
	int oldY = m_Y;

	// temporarily.. set the new coordinate
	m_X = x;
	m_Y = y;	// m_X=x; m_Y=y;  and.. Etc...
		
	// get the coordinate moved in the direction sent from the Server.
	POINT next;
	GetNextPosition( direction, next );

	// check whether it goes out of bounds . 2001.10.7
	if (next.x < 0 
		|| next.y < 0 
		|| next.x >= ZoneState::GetWidth()
		|| next.y >= ZoneState::GetHeight())
	{
		m_X = oldX;
		m_Y = oldY;

		return false;
	}

	// remember server coordinate
	// 2001.8.6
	// comment 2002.3.22... moved to PacketMove.
	//SetServerPositionWithBlock( next.x, next.y );	

	// return to the original coordinate.
	m_X = oldX;
	m_Y = oldY;

	//-----------------------------------------------------
	// when it is a place that cannot be reached...
	//-----------------------------------------------------
	// Wrong info came from the Server.. - -;;
	// a timing problem, shall we say?
	#ifdef OUTPUT_DEBUG
		//if (!m_pZone->CanMove(m_MoveType, next.x, next.y))
		{
		//	DEBUG_ADD_FORMAT("[ Collide ] Creature[ID=%d] Can't Move to (%d,%d) - But, Go!", m_ID, next.x, next.y);				
			
			// let's try moving anyway~
			//return;
		}
	#endif

	//--------------------------------------------------
	// set the coordinate verified by the Server.
	//--------------------------------------------------
	//m_ServerX = x;//next.x;
	//m_ServerY = y;//next.y;

	// if the previous info has not been updated yet...
	if (m_NextX != SECTORPOSITION_NULL)
		// && m_NextY != SECTORPOSITION_NULL
	{
		// message
		DEBUG_ADD_FORMAT("[ SlowUpdate ] [ID=%d] : (%d, %d) --> (%d, %d),  Player=(%d, %d)", 
							m_ID, 
							m_X, m_Y, 
							m_NextX, m_NextY, 
							PlayerX(), PlayerY());
		
		// update the previous info immediately.
		MovePosition(m_NextX, m_NextY);
		if (m_NextDirection != DIRECTION_NULL)
		{
			m_Direction = m_NextDirection;
			m_DirectionMove = m_NextDirection;
		}

		// move from (m_X, m_Y) in m_CurrentDirection.
		SetNextAction( m_MoveAction );

		m_NextX = x;
		m_NextY = y;
		m_NextDirection = direction;	

		return true;
	}

	//--------------------------------------------------
	// no need to move if already at the destination.
	//--------------------------------------------------		
	//else 
	if (m_X==next.x && m_Y==next.y)
	{	
		// message
		DEBUG_ADD_FORMAT("[ Stand ] [ID=%d] : Already Reached (%d, %d) ,  Player=(%d, %d)", m_ID, oldX, oldY, PlayerX(), PlayerY());
	}
	//--------------------------------------------------
	// if not at the destination --> move.
	//--------------------------------------------------
	else
	{		
		//--------------------------------------------------
		// if moving from the previous position..
		//--------------------------------------------------
		if (m_X==x && m_Y==y)
		{
			// if the moving action has finished..
			if (m_MoveCount>=m_MoveCountMax)
			{
				DEBUG_ADD_FORMAT("[ MoveSetting ] [ID=%d] From(%d, %d) to Direction(%d) ,  Player=(%d, %d)", m_ID, x,y, direction, PlayerX(), PlayerY());
				
				m_Direction = direction;
				m_DirectionMove = direction;
				SetNextAction( m_MoveAction );
				//m_bNextAction = true;
			}
			// if still moving..
			else
			{
				DEBUG_ADD_FORMAT("[ MoveBuffering ][ID=%d] Current(%d, %d), Next(%d, %d) to Direction(%d)", m_ID, m_X, m_Y, x,y, direction);						
				
				m_NextX = x;
				m_NextY = y;
				m_NextDirection = direction;
			}
		}
		//--------------------------------------------------
		// when it ends up jumping...
		//--------------------------------------------------
		// remove the previous Creature position.
		else
		{
			// move from the previous coordinate (m_X, m_Y) to the new coordinate (x,y).
			MovePosition( x, y );			
			
			// must move in direction (direction) from the new coordinate.
			m_Direction = direction;
			m_DirectionMove = direction;
			m_NextDirection = direction;	// 2001.11.6
			SetNextAction( m_MoveAction );

			// message
			DEBUG_ADD_FORMAT("[ Jump ] [ID=%d] : (%d, %d) --> (%d, %d),  Player=(%d, %d)", m_ID, m_X, m_Y, next.x, next.y, PlayerX(), PlayerY());
		}			
		
		// decide the direction and make it move.
		//m_CurrentDirection = direction;
		//SetNextAction( m_MoveAction );
	}	

	return true;
}

//----------------------------------------------------------------------
// Get ActionInfo's Action
//----------------------------------------------------------------------
// Find the ACTION appropriate for nActionInfo.
//----------------------------------------------------------------------
int
MCreature::GetActionInfoAction(TYPE_ACTIONINFO nActionInfo, bool IsSelfAction)
{
	// 2001.9.30
	if (nActionInfo>=g_pActionInfoTable->GetSize())
	{
		return 0;
	}

	int action;
	
	
	// 2004, 11, 23, sobeit add start - set afire
//	if(nActionInfo == SKILL_SET_AFIRE && !IsFastMove())
//		return ACTION_ATTACK;
	// 2004, 11, 23, sobeit add end
	//-------------------------------------------------------------
	// Is the basic action applied?
	//-------------------------------------------------------------
	if ((*g_pActionInfoTable)[nActionInfo].IsAffectCurrentWeaponAction())
	{
		action = (*g_pActionInfoTable)[m_nBasicActionInfo].GetAction();
	}
	else
	{
		action = (*g_pActionInfoTable)[nActionInfo].GetAction();	
	}

	if(action == ACTION_MAGIC && IsAdvancementClass())
	{
		if(!(*g_pActionInfoTable)[nActionInfo].IsTargetSelf() && !IsSelfAction)
		{
			if(IsSlayer())
				return ACTION_ADVANCEMENT_SLAYER_MAGIC_ATTACK;
			else if(IsOusters())
				return ACTION_ADVANCEMENT_OUSTERS_MAGIC_ATTACK;
			else 
				return ACTION_ADVANCEMENT_MAGIC_ATTACK;

		}
	}
	//-------------------------------------------------------------
	// action differs depending on weapon speed.
	//-------------------------------------------------------------
	if (IsSlayer())
	{
		if (m_CreatureType==CREATURETYPE_SLAYER_OPERATOR)
		{
			if (action>ACTION_DAMAGED
				|| action==ACTION_ATTACK)
			{
				return ACTION_ATTACK;
			}
		}
		
		//-----------------------------------------------------
		// Lightning Handis always ACTION_SLAYER_SWORD_2. - -;d
		//-----------------------------------------------------
		if (nActionInfo==SKILL_LIGHTNING_HAND || nActionInfo == SKILL_LARSLASH )
		{
			return ACTION_SLAYER_SWORD_2;
		} 
		else
		if (nActionInfo == SKILL_BLITZ_SLIDING_ATTACK)
		{
			return ACTION_SLAYER_SWORD_2_FAST;
		} else
		if ( nActionInfo==SKILL_BLAZE_WALK_ATTACK )
		{
			return ACTION_SLAYER_BLADE_2_FAST;
		}
		else 
		if( nActionInfo == SKILL_PLASMA_ROCKET_LAUNCHER )
		{
			return s_SlayerActionSpeed[action][1];
		}
		return s_SlayerActionSpeed[action][m_WeaponSpeed];
	} 
	else if( IsOusters() )
	{
		if(nActionInfo == SKILL_DISTANCE_BLITZ)
			return action;

		// test code
//		if(nActionInfo == SKILL_FATAL_SNICK || nActionInfo == SKILL_FATAL_SNICK_MASTERY)
//			action = ACTION_OUSTERS_CHAKRAM;
//		
		if(m_CreatureType==CREATURETYPE_OUSTERS)
			return s_OustersActionSpeed[action][m_WeaponSpeed];		
	}

	if (m_CreatureType==CREATURETYPE_WOLF)		
	{
		if (action==ACTION_ATTACK)
		{
			return ACTION_ATTACK;
		}
	}

	if (m_CreatureType==CREATURETYPE_VAMPIRE_OPERATOR || m_CreatureType==CREATURETYPE_SLAYER_OPERATOR || m_CreatureType==CREATURETYPE_OUSTERS_OPERATOR)
	{
		if (action>ACTION_DAMAGED
			|| action==ACTION_ATTACK)
		{
			return ACTION_ATTACK;
		}		
	}
	// 2004, 7, 27 sobeit add start
	if (m_CreatureType==717 || m_CreatureType==721 || m_CreatureType==723) // Zildre, Zildre clone, awakened Zildre
	{
		if(action <ACTION_MAX_OUSTERS)
			return action;
	}
	// 2004, 7, 27 sobeit add end
	if(m_CreatureType == 755 || m_CreatureType == 756 || m_CreatureType == 757 || m_CreatureType == 758 ||
		m_CreatureType == 759 || m_CreatureType == 760 || m_CreatureType == 761 || m_CreatureType == 762 || m_CreatureType == 763)
	{
		if(action==ACTION_ATTACK)
		{
			if (rand()%2)
				action = ACTION_MAGIC;
			else
				action = ACTION_ATTACK;
		}
	}
	

//	if (IsCreatureTypeVampirePC(m_CreatureType))
//		return s_VampireActionSpeed[action][m_WeaponSpeed];	


	return s_VampireActionSpeed[action][m_WeaponSpeed];	
//	return action;
}

//----------------------------------------------------------------------
// Get ActionInfo's CastingStartFrame
//----------------------------------------------------------------------
// Find the CastingStartFrame appropriate for nActionInfo.
//----------------------------------------------------------------------
int			
MCreature::GetActionInfoCastingStartFrame(TYPE_ACTIONINFO nActionInfo)
{
	// 2001.9.30
	if (nActionInfo>=g_pActionInfoTable->GetSize())
	{
		return 0;
	}

	//-------------------------------------------------------------
	// Is the basic action applied?
	//-------------------------------------------------------------
	if ((*g_pActionInfoTable)[nActionInfo].IsAffectCurrentWeaponCastingStartFrame())
	{
//		if(nActionInfo == SKILL_PLASMA_ROCKET_LAUNCHER)
//		{
//			switch(m_nBasicActionInfo)
//			{
//			case ACTION_SLAYER_GUN_SR :
//			case ACTION_SLAYER_GUN_SR_SLOW :
//			case ACTION_SLAYER_GUN_SR_FAST :
//				return 7;
//			case ACTION_SLAYER_GUN_AR:
//			case ACTION_SLAYER_GUN_AR_SLOW:
//			case ACTION_SLAYER_GUN_AR_FAST:
//				return 4;
//			case ACTION_SLAYER_GUN_SG:
//			case ACTION_SLAYER_GUN_SG_SLOW:
//			case ACTION_SLAYER_GUN_SG_FAST:
//				return 4;
//			case ACTION_SLAYER_GUN_SMG:
//			case ACTION_SLAYER_GUN_SMG_SLOW:
//			case ACTION_SLAYER_GUN_SMG_FAST:
//				return 4;
//			}
//		}
		return (*g_pActionInfoTable)[m_nBasicActionInfo].GetCastingStartFrame( m_WeaponSpeed );
	}
	
	return (*g_pActionInfoTable)[nActionInfo].GetCastingStartFrame( m_WeaponSpeed );	
}

//----------------------------------------------------------------------
// Get ActionInfo's RepeatStartFrame
//----------------------------------------------------------------------
// Find the RepeatStartFrame appropriate for nActionInfo.
//----------------------------------------------------------------------
int			
MCreature::GetActionInfoRepeatStartFrame(TYPE_ACTIONINFO nActionInfo)
{
	// 2001.9.30
	if (nActionInfo>=g_pActionInfoTable->GetSize())
	{
		return GetActionCountMax();
	}

	int repeat;

	//-------------------------------------------------------------
	// Is the basic action applied?
	//-------------------------------------------------------------
	if ((*g_pActionInfoTable)[nActionInfo].IsAffectCurrentWeaponCastingStartFrame())
	{
//		if(nActionInfo == SKILL_PLASMA_ROCKET_LAUNCHER)
//		{
//			switch(m_nBasicActionInfo)
//			{
//			case ACTION_SLAYER_GUN_SR :
//			case ACTION_SLAYER_GUN_SR_SLOW :
//			case ACTION_SLAYER_GUN_SR_FAST :
//				return 7;
//			case ACTION_SLAYER_GUN_AR:
//			case ACTION_SLAYER_GUN_AR_SLOW:
//			case ACTION_SLAYER_GUN_AR_FAST:
//				return 4;
//			case ACTION_SLAYER_GUN_SG:
//			case ACTION_SLAYER_GUN_SG_SLOW:
//			case ACTION_SLAYER_GUN_SG_FAST:
//				return 4;
//			case ACTION_SLAYER_GUN_SMG:
//			case ACTION_SLAYER_GUN_SMG_SLOW:
//			case ACTION_SLAYER_GUN_SMG_FAST:
//				return 4;
//			}
//		}
		if (m_nBasicActionInfo < g_pActionInfoTable->GetSize())
		{
			repeat = (*g_pActionInfoTable)[m_nBasicActionInfo].GetRepeatStartFrame( m_WeaponSpeed );
		}
		else
		{
			repeat = GetActionCountMax();
		}
	}
	else
	{
		repeat = (*g_pActionInfoTable)[nActionInfo].GetRepeatStartFrame( m_WeaponSpeed );
	}

	
	if (repeat==-1)
	{
		return 0;
	}

	return repeat;
}
		
//----------------------------------------------------------------------
// Get ActionInfo's RepeatEndFrame
//----------------------------------------------------------------------
// Find the RepeatEndFrame appropriate for nActionInfo.
//----------------------------------------------------------------------
int			
MCreature::GetActionInfoRepeatEndFrame(TYPE_ACTIONINFO nActionInfo)
{
	// 2001.9.30
	if (nActionInfo>=g_pActionInfoTable->GetSize())
	{
		return GetActionCountMax();
	}

	int repeat;
	//-------------------------------------------------------------
	// Is the basic action applied?
	//-------------------------------------------------------------
	if ((*g_pActionInfoTable)[nActionInfo].IsAffectCurrentWeaponCastingStartFrame())
	{
//		if(nActionInfo == SKILL_PLASMA_ROCKET_LAUNCHER)
//		{
//			switch(m_nBasicActionInfo)
//			{
//			case ACTION_SLAYER_GUN_SR :
//			case ACTION_SLAYER_GUN_SR_SLOW :
//			case ACTION_SLAYER_GUN_SR_FAST :
//				return 17;
//			case ACTION_SLAYER_GUN_AR:
//			case ACTION_SLAYER_GUN_AR_SLOW:
//			case ACTION_SLAYER_GUN_AR_FAST:
//				return 14;
//			case ACTION_SLAYER_GUN_SG:
//			case ACTION_SLAYER_GUN_SG_SLOW:
//			case ACTION_SLAYER_GUN_SG_FAST:
//				return 14;
//			case ACTION_SLAYER_GUN_SMG:
//			case ACTION_SLAYER_GUN_SMG_SLOW:
//			case ACTION_SLAYER_GUN_SMG_FAST:
//				return 14;
//			}
//		}
		if (m_nBasicActionInfo < g_pActionInfoTable->GetSize())
		{
			repeat = (*g_pActionInfoTable)[m_nBasicActionInfo].GetRepeatEndFrame( m_WeaponSpeed );		
		}
		else
		{
			repeat = GetActionCountMax();
		}
	}
	else
	{
		repeat = (*g_pActionInfoTable)[nActionInfo].GetRepeatEndFrame( m_WeaponSpeed );
	}

	if (repeat==-1)
	{
		return GetCreatureActionCountMax( this, m_Action );//(*g_pCreatureTable)[m_CreatureType].GetActionCount( m_Action );
	}

	return repeat;
}

//----------------------------------------------------------------------
// Get ActionInfo's CastingFrames
//----------------------------------------------------------------------
// Find the CastingFrames appropriate for nActionInfo.
//----------------------------------------------------------------------
int			
MCreature::GetActionInfoCastingFrames(TYPE_ACTIONINFO nActionInfo)
{
	// 2001.9.30
	if (nActionInfo>=g_pActionInfoTable->GetSize())
	{
		return 0;
	}

	//-------------------------------------------------------------
	// Is the basic action applied?
	//-------------------------------------------------------------
	if ((*g_pActionInfoTable)[nActionInfo].IsAffectCurrentWeaponCastingFrames())
	{
		return (*g_pActionInfoTable)[m_nBasicActionInfo].GetCastingFrames( m_WeaponSpeed );
	}
	
	return (*g_pActionInfoTable)[nActionInfo].GetCastingFrames( m_WeaponSpeed );	
}

//----------------------------------------------------------------------
// Get ActionInfo's Delay
//----------------------------------------------------------------------
// Find the Delay appropriate for nActionInfo.
//----------------------------------------------------------------------
int
MCreature::GetActionInfoDelay(TYPE_ACTIONINFO nActionInfo)
{
	// 2001.9.30
	if (nActionInfo>=g_pActionInfoTable->GetSize())
	{
		return 0;
	}


//	if(nActionInfo == SKILL_FATAL_SNICK || nActionInfo == SKILL_FATAL_SNICK_MASTERY)
//	{
//		switch( GetWeaponSpeed() )
//		{
//		case WEAPON_SPEED_SLOW :
//			return 25 * 1000 / 16;
//		case WEAPON_SPEED_NORMAL :
//			return 23 * 1000 / 16;
//		case WEAPON_SPEED_FAST :
//			return 21 * 1000 / 16;
//		}
//	} 
	//-------------------------------------------------------------
	// Is the basic action applied?
	//-------------------------------------------------------------
	if ((*g_pActionInfoTable)[nActionInfo].IsAffectCurrentWeaponDelay())
	{
		return (*g_pActionInfoTable)[m_nBasicActionInfo].GetDelay();
	}
	
	return (*g_pActionInfoTable)[nActionInfo].GetDelay();	
}

//----------------------------------------------------------------------
// Get Casting SoundID
//----------------------------------------------------------------------
// Find the ACTION appropriate for nActionInfo.
//----------------------------------------------------------------------
TYPE_SOUNDID
MCreature::GetCastingSoundID(TYPE_ACTIONINFO nActionInfo)
{
	//-------------------------------------------------------------
	// Is the basic weapon applied?
	//-------------------------------------------------------------
	if ((*g_pActionInfoTable)[nActionInfo].IsAffectCurrentWeaponSound())
	{
		return (*g_pActionInfoTable)[m_nBasicActionInfo].GetSoundID();
	}
	
	return (*g_pActionInfoTable)[nActionInfo].GetSoundID();	
}

//----------------------------------------------------------------------
// Get ActionInfo ActionEffectSpriteType
//----------------------------------------------------------------------
// Find the ActionEffectSpriteType appropriate for nActionInfo.
//----------------------------------------------------------------------
int
MCreature::GetActionInfoActionEffectSpriteType(TYPE_ACTIONINFO nActionInfo)
{
	// 2001.9.30
	if (nActionInfo>=g_pActionInfoTable->GetSize())
	{
		return 0;
	}

	int useActionInfo;


	//-------------------------------------------------------------
	// Is the basic weapon applied?
	//-------------------------------------------------------------
	if ((*g_pActionInfoTable)[nActionInfo].IsAffectCurrentWeaponActionEffectSpriteType())
	{
		useActionInfo = GetBasicActionInfo(); //m_nBasicActionInfo;
	}
	else
	{
		useActionInfo = nActionInfo;
	}

	// add plus.
	// Temporary patch~~ but.. it probably won't be fixed in the future either - -;
	useActionInfo += (*g_pActionInfoTable)[nActionInfo].GetAffectCurrentWeaponActionInfoPlus();


	TYPE_EFFECTSPRITETYPE re_est = EFFECTSPRITETYPE_NULL;

	//------------------------------------------------------------
	// for male
	//------------------------------------------------------------
	if (IsMale())
	{
		re_est = (*g_pActionInfoTable)[useActionInfo].GetActionEffectSpriteType();
		if(nActionInfo == SKILL_BULLET_OF_LIGHT)
		{
			switch(re_est)
			{
			case EFFECTSPRITETYPE_GUN_AR_MALE:
				re_est = EFFECTSPRITETYPE_BULLET_OF_LIGHT_AR_MALE;
				break;

			case EFFECTSPRITETYPE_GUN_SMG_MALE:
				re_est = EFFECTSPRITETYPE_BULLET_OF_LIGHT_SMG_MALE;
				break;

			case EFFECTSPRITETYPE_GUN_SR_MALE:
				re_est = EFFECTSPRITETYPE_BULLET_OF_LIGHT_SR_MALE;
				break;

			case EFFECTSPRITETYPE_GUN_SG_MALE:
				re_est = EFFECTSPRITETYPE_BULLET_OF_LIGHT_SG_MALE;
				break;
			}
		} 
		else
		if(nActionInfo == SKILL_JABBING_VEIN)
		{
			switch(re_est)
			{
			case EFFECTSPRITETYPE_GUN_AR_MALE :
				re_est = EFFECTSPRITETYPE_JABBING_VEIN_GUN_AR_MALE;
				break;
			case EFFECTSPRITETYPE_GUN_SMG_MALE :
				re_est = EFFECTSPRITETYPE_JABBING_VEIN_GUN_SMG_MALE;
				break;
			case EFFECTSPRITETYPE_GUN_SR_MALE :
				re_est = EFFECTSPRITETYPE_JABBING_VEIN_GUN_SR_MALE;
				break;
			case EFFECTSPRITETYPE_GUN_SG_MALE :
				re_est = EFFECTSPRITETYPE_JABBING_VEIN_GUN_SG_MALE;
				break;
			}
		}
		return re_est;
	}
	
	//------------------------------------------------------------
	// female Effect check
	//------------------------------------------------------------	
	TYPE_EFFECTSPRITETYPE effectSpriteTypeFemale = (*g_pActionInfoTable)[useActionInfo].GetActionEffectSpriteTypeFemale();

	if (effectSpriteTypeFemale!=EFFECTSPRITETYPE_NULL)
	{
		re_est = effectSpriteTypeFemale;
		if(nActionInfo == SKILL_BULLET_OF_LIGHT)
		{
			switch(re_est)
			{
			case EFFECTSPRITETYPE_GUN_AR_FEMALE:
				re_est = EFFECTSPRITETYPE_BULLET_OF_LIGHT_AR_FEMALE;
				break;

			case EFFECTSPRITETYPE_GUN_SMG_FEMALE:
				re_est = EFFECTSPRITETYPE_BULLET_OF_LIGHT_SMG_FEMALE;
				break;

			case EFFECTSPRITETYPE_GUN_SR_FEMALE:
				re_est = EFFECTSPRITETYPE_BULLET_OF_LIGHT_SR_FEMALE;
				break;

			case EFFECTSPRITETYPE_GUN_SG_FEMALE:
				re_est = EFFECTSPRITETYPE_BULLET_OF_LIGHT_SG_FEMALE;
				break;
			}
		} 
		else
		if(nActionInfo == SKILL_JABBING_VEIN)
		{
			switch(re_est)
			{
			case EFFECTSPRITETYPE_GUN_AR_FEMALE :
				re_est = EFFECTSPRITETYPE_JABBING_VEIN_GUN_AR_FEMALE;
				break;
			case EFFECTSPRITETYPE_GUN_SMG_FEMALE :
				re_est = EFFECTSPRITETYPE_JABBING_VEIN_GUN_SMG_FEMALE;
				break;
			case EFFECTSPRITETYPE_GUN_SR_FEMALE :
				re_est = EFFECTSPRITETYPE_JABBING_VEIN_GUN_SR_FEMALE;
				break;
			case EFFECTSPRITETYPE_GUN_SG_FEMALE :
				re_est = EFFECTSPRITETYPE_JABBING_VEIN_GUN_SG_FEMALE;
				break;
			}
		}
		return re_est;
	}
	
	// if none, use the male one..
	re_est = (*g_pActionInfoTable)[useActionInfo].GetActionEffectSpriteType();
	if(nActionInfo == SKILL_BULLET_OF_LIGHT)
	{
		switch(re_est)
		{
		case EFFECTSPRITETYPE_GUN_AR_MALE:
			re_est = EFFECTSPRITETYPE_BULLET_OF_LIGHT_AR_MALE;
			break;
			
		case EFFECTSPRITETYPE_GUN_SMG_MALE:
			re_est = EFFECTSPRITETYPE_BULLET_OF_LIGHT_SMG_MALE;
			break;
			
		case EFFECTSPRITETYPE_GUN_SR_MALE:
			re_est = EFFECTSPRITETYPE_BULLET_OF_LIGHT_SR_MALE;
			break;
			
		case EFFECTSPRITETYPE_GUN_SG_MALE:
			re_est = EFFECTSPRITETYPE_BULLET_OF_LIGHT_SG_MALE;
			break;
		}
	} 
	else if(nActionInfo == SKILL_JABBING_VEIN)
	{
		switch(re_est)
		{
		case EFFECTSPRITETYPE_GUN_AR_MALE :
			re_est = EFFECTSPRITETYPE_JABBING_VEIN_GUN_AR_MALE;
			break;
		case EFFECTSPRITETYPE_GUN_SMG_MALE :
			re_est = EFFECTSPRITETYPE_JABBING_VEIN_GUN_SMG_MALE;
			break;
		case EFFECTSPRITETYPE_GUN_SR_MALE :
			re_est = EFFECTSPRITETYPE_JABBING_VEIN_GUN_SR_MALE;
			break;
		case EFFECTSPRITETYPE_GUN_SG_MALE :
			re_est = EFFECTSPRITETYPE_JABBING_VEIN_GUN_SG_MALE;
			break;
		}
	}
	return re_est;	
}


//----------------------------------------------------------------------
// Packet SpecialAction Result
//----------------------------------------------------------------------
// since this Creature received Damage
//
// 0. stop all actions and..
// 1. Action --> action of receiving Damage
// 2. show Damage
//----------------------------------------------------------------------
void		
MCreature::PacketSpecialActionResult(TYPE_ACTIONINFO nResultActionInfo, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, BYTE temp)
{
	//--------------------------------------------------
	// if dead, return
	//--------------------------------------------------
	/*
	// if we return here, the hit effect on death is not shown.
	if (!m_bAlive)
	{
		// when dead, nothing but the death effect should be shown.
		if (nResultActionInfo!=(*g_pCreatureTable)[m_CreatureType].DeadActionInfo)
			return;
	}
	*/

	//----------------------------------------------------------------------
	// when it is not a result action..  - does it mean anything?? Hmm..
	//----------------------------------------------------------------------
	if (nResultActionInfo < (*g_pActionInfoTable).GetMinResultActionInfo()
		// do not show an action during fast move, knockback... // 2001.10.9
		|| m_bFastMove || m_bKnockBack || nResultActionInfo == ACTIONINFO_NULL)
	{
		return;
	}


	//----------------------------------------------------------------------
	// shield effect.. - -;
	//----------------------------------------------------------------------
	if (GetActionInfoAction( nResultActionInfo )==ACTION_DAMAGED)
	{
		// do not show the hit action when dead..
		// apparently it gets hit because of tile skills.. - -;
		// 2002.3.9
		if (!m_bAlive)
		{
			return;
		}

		//if (HasEffectStatus(EFFECTSTATUS_AURA_PRISM))
		{
		//	nResultActionInfo = REFLECT_AURA_PRISM;
		}
		//else 
			if (HasEffectStatus(EFFECTSTATUS_AURA_SHIELD))
		{
			nResultActionInfo = REFLECT_AURA_PRISM;//REFLECT_AURA_SHIELD;
		}

//		#ifndef CONNECT_SERVER
//			if (HasEffectStatus(EFFECTSTATUS_ARMAGEDDON))
//			{				
//				int hp = GetArmageddonHP()-30;
//				hp = max(0, hp);
//
//				SetStatus( MODIFY_ARMAGEDDON_HP, hp );
//			}
//		#endif
	}


	// stop moving.
	////m_sX = 0;	
	////m_sY = 0;	
	//SetStop();

	//m_ActionCount = 0; 
	//------------------------------------------------------
	// check when NextUsedACtionInfo already exists
	//------------------------------------------------------
	AffectBufferedActionInfo();
	
	ActionMoveNextPosition();

	// must find the ActionInfo corresponding to nResultActionInfo.
	// just do original ActionInfo + MIN_RESULT_ACTIONINFO.
	m_nNextUsedActionInfo	= nResultActionInfo; // +(*g_pActionInfoTable).GetMinResultActionInfo()

	if( temp != 0)
		SetActionGrade( temp );

	if (m_bAlive)
	{
		BYTE nextAction = GetActionInfoAction(m_nNextUsedActionInfo);
		SetNextAction( nextAction );

		if (nextAction!=ACTION_STAND)
		{
			m_bNextAction = true;
		}
	}

	// used on itself?..
	SetTraceID( id );
	m_TraceX	= sX;
	m_TraceY	= sY;

	// Test Code
	/*
	if (rand()%2)
		SetChatString( "Shit!" );
	else
		SetChatString( "Oops!" );
	*/
}


//----------------------------------------------------------------------
// Packet AttackNormal (x,y, d)
//----------------------------------------------------------------------
// this Creature takes the Attack action.
//
// 0. stop all actions and..
// 1. Action Attack
//----------------------------------------------------------------------
void		
MCreature::PacketAttackNormal(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, BYTE direction)
{
	//--------------------------------------------------
	// if dead, return
	//--------------------------------------------------
	if (!m_bAlive)
		return;

	// stop moving.
	m_sX = 0;	
	m_sY = 0;
	
	// set the info sent from the server..
	{
		// same as the current coordinate.. no need to move.
		if (m_X==sX && m_Y==sY)
		{
		}
		// if the coordinate differs.. must move first and then Attack.
		else
		{
			// move
			MovePosition(sX, sY);
			
			// message
			DEBUG_ADD_FORMAT("[ Move To Attack ] : (%d, %d) --> (%d, %d),  Player=(%d, %d)", m_X, m_Y, sX, sY, PlayerX(), PlayerY());
		}	

		// set the info sent from the Server..
		m_Direction = direction;
	}		

	m_ActionCount = 0; 

	// attacking motion
	m_nNextUsedActionInfo	= SKILL_ATTACK_MELEE;
	SetNextAction( GetActionInfoAction(m_nNextUsedActionInfo) );	
}

//----------------------------------------------------------------------
// Packet SpecialAction To Sector
//----------------------------------------------------------------------
// this Creature takes the action according to nActionInfo.
//
// 0. stop all actions and..
// 1. SpecialAction
//----------------------------------------------------------------------
void		

MCreature::PacketSpecialActionToSector(TYPE_ACTIONINFO nActionInfo, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, MActionResult* pActionResult)
{
	DEBUG_ADD_FORMAT("PacketSpecialActionToSector ai : %d", nActionInfo );
	//--------------------------------------------------
	// if dead, return
	//--------------------------------------------------
	if (!m_bAlive || (
		nActionInfo>=(*g_pActionInfoTable).GetMinResultActionInfo() &&
		!(nActionInfo>=STEP_SKILL_FLOURISH_2 && nActionInfo <MAGIC_ENCHANT_OPTION_NULL )
		)
		)
	{
		//DEBUG_ADD("delete?");
		if (pActionResult!=NULL)
		{	
			DEBUG_ADD("delete");
			delete pActionResult;
		}
//		DEBUG_ADD("return");
		return;
	}

	//------------------------------------------------------
	// when it is a skill used on a Sector
	//------------------------------------------------------
	//DEBUG_ADD("fuck");
	if ((*g_pActionInfoTable)[nActionInfo].IsTargetZone() ||
		nActionInfo == SKILL_TRANSFUSION || nActionInfo == SKILL_PIERCING || nActionInfo == SKILL_VIOLENT_PHANTOM 
		|| nActionInfo == SKILL_ICE_LANCE_MASTERY || nActionInfo == SKILL_ICE_LANCE
		|| nActionInfo == SKILL_DESTRUCTION_SPEAR_MASTERY || nActionInfo == SKILL_DESTRUCTION_SPEAR
		|| nActionInfo == SKILL_SPIT_STREAM || nActionInfo == SKILL_ROTTEN_APPLE || nActionInfo == SKILL_VAMPIRE_INNATE_DEADLY_CLAW
		)
	{
	//	DEBUG_ADD("hmhm");
		//---------------------------------------------------------------
		// yum yum.. 2001.10.28
		//---------------------------------------------------------------
		AffectMoveBufferAll();
	//	DEBUG_ADD("3");

		ActionMoveNextPosition();
	//	DEBUG_ADD("2");

		// stop moving.
	//	m_sX = 0;	
	//	m_sY = 0;
		if (nActionInfo==m_nSpecialActionInfo && m_nSpecialActionInfo!=ACTIONINFO_NULL)
		{
			m_RepeatCount ++;
		}
		else
		{
			m_RepeatCount = 0;
			m_ActionCount = 0;
		}

		m_nSpecialActionInfo = nActionInfo;
		//------------------------------------------------------
		// check when NextUsedACtionInfo already exists
		//------------------------------------------------------
		AffectBufferedActionInfo();
	//	DEBUG_ADD("1");

		// set the info sent from the Server..
		m_nNextUsedActionInfo	= nActionInfo;
		m_TraceX		= sX;
		m_TraceY		= sY;
		m_TraceZ		= 0;	// is the z value.. meaningful..
		// 2004, 11, 4, sobeit modify start - because of Great Ruffian...
		if(GetCreatureType() == 764 || GetCreatureType() == 765)
		{
			SetTraceID( GetID() );
			if(nActionInfo == SKILL_CHAIN_THROWING_AXE)
				SetShadowCount(3);
		}
		else
			SetTraceID( OBJECTID_NULL );
		// 2004, 11, 4, sobeit modify end

		
		// look toward the target.
		//SetDirectionToPosition(sX, sY);
		
	//	m_ActionCount = 0;

		// [ TEST CODE ]	
	//	DEBUG_ADD("0");
		BYTE nextAction = GetActionInfoAction(nActionInfo);

	//	DEBUG_ADD("9");
		SetNextAction( nextAction );
	//	DEBUG_ADD("8");

		if (nextAction!=ACTION_STAND)
		{
			m_bNextAction = true;
		}

		//---------------------------------------------------------------
		// when there is already a result to show..
		//---------------------------------------------------------------
	//	DEBUG_ADD("7");
		if (m_pActionResult != NULL)
		{
			DEBUG_ADD("Execute Old Result");

			// handle the result that already existed.
			// [ TEST CODE ]
			// result handling..
			//
			// (!) the m_pActionResult value can change in Execute
			//		it must be saved and then deleted.
			MActionResult* pOldResult = m_pActionResult;

			m_pActionResult = NULL;

			pOldResult->Execute();

			delete pOldResult;			
		}

	//	DEBUG_ADD("6");
		// set the result
		m_pActionResult = pActionResult;

	//	DEBUG_ADD("5");
	}
	//------------------------------------------------------
	// otherwise
	//------------------------------------------------------
	else
	{
		if (pActionResult!=NULL)
		{	
			delete pActionResult;
		}
		DEBUG_ADD_FORMAT("[Skill : %s] Target is Not Sector", (*g_pActionInfoTable)[nActionInfo].GetName());					
	}
}

//----------------------------------------------------------------------
// Packet SpecialAction To Other
//----------------------------------------------------------------------
// this Creature takes the action according to nActionInfo.
//
// 0. stop all actions and..
// 1. SpecialAction
//----------------------------------------------------------------------
void		
MCreature::PacketSpecialActionToOther(TYPE_ACTIONINFO nActionInfo, TYPE_OBJECTID id, MActionResult* pActionResult)
{
	//--------------------------------------------------
	// if dead, return
	//--------------------------------------------------
	if (!m_bAlive || 
		(nActionInfo>=(*g_pActionInfoTable).GetMinResultActionInfo() &&
		!( nActionInfo >= STEP_SKILL_FLOURISH_2 && nActionInfo < MAGIC_ENCHANT_OPTION_NULL ))
		)
	{
		if (pActionResult!=NULL)
		{	
			delete pActionResult;
		}
		return;
	}
	//------------------------------------------------------
	// when it is a skill used on others
	//------------------------------------------------------
	if ((*g_pActionInfoTable)[nActionInfo].IsTargetOther())
	{
		// get the target creature.
		MCreature* pCreature = ZoneState::GetCreature( id );
		
		if (pCreature==NULL)
		{
			// when there is no such creature
			DEBUG_ADD_FORMAT("[Skill : %s] There's no such creature(%d)", (*g_pActionInfoTable)[nActionInfo].GetName(), id);

			if (pActionResult!=NULL)
			{	
				delete pActionResult;
			}
			
			return;
		}

		//---------------------------------------------------------------
		// yum yum.. 2001.10.28
		//---------------------------------------------------------------
		AffectMoveBufferAll();

		ActionMoveNextPosition();

		// stop moving.
		//m_sX = 0;	
		//m_sY = 0;

		//------------------------------------------------------
		// when the same as the previous action
		// 2001.7.23 added
		//------------------------------------------------------
		if (nActionInfo==m_nSpecialActionInfo && m_nSpecialActionInfo!=ACTIONINFO_NULL)
		{
			m_RepeatCount ++;
		}
		else
		{
			m_RepeatCount = 0;
			m_ActionCount = 0;
		}

		m_nSpecialActionInfo = nActionInfo;

		//------------------------------------------------------
		// check when NextUsedACtionInfo already exists
		//------------------------------------------------------
		AffectBufferedActionInfo();
				
		// set the info sent from the Server..
		m_nNextUsedActionInfo	= nActionInfo;
		m_TraceX		= pCreature->GetX();
		m_TraceY		= pCreature->GetY();
		m_TraceZ		= pCreature->GetZ();
		SetTraceID ( id );

		// look toward the target.
		//SetDirectionToPosition(pCreature->GetX(), pCreature->GetY());
		
		//m_ActionCount = 0;

		// [ TEST CODE ]	
		BYTE nextAction = GetActionInfoAction(nActionInfo);
		SetNextAction( nextAction );

		if (nextAction!=ACTION_STAND)
		{
			m_bNextAction = true;
		}

		//---------------------------------------------------------------
		// when there is already a result to show..
		//---------------------------------------------------------------		
		if (m_pActionResult != NULL)
		{
			DEBUG_ADD("Execute Old Result");

			// handle the result that already existed.
			// [ TEST CODE ]
			// result handling..
			// (!) the m_pActionResult value can change in Execute
			//		it must be saved and then deleted.
			MActionResult* pOldResult = m_pActionResult;

			m_pActionResult = NULL;

			pOldResult->Execute();

			delete pOldResult;	
		}

		// set the result
		m_pActionResult = pActionResult;

	}
	//------------------------------------------------------
	// otherwise
	//------------------------------------------------------
	else
	{
		if (pActionResult!=NULL)
		{	
			delete pActionResult;
		}

		DEBUG_ADD_FORMAT("[Skill : %s] Target is Not Other", (*g_pActionInfoTable)[nActionInfo].GetName());							
	}
}

//----------------------------------------------------------------------
// Packet SpecialAction To InventoryItem
//----------------------------------------------------------------------
// only the casting action is shown.
//----------------------------------------------------------------------
void		
MCreature::PacketSpecialActionToInventoryItem(TYPE_ACTIONINFO nActionInfo)
{
	//--------------------------------------------------
	// if dead, return
	//--------------------------------------------------
	if (!m_bAlive || nActionInfo>=(*g_pActionInfoTable).GetMinResultActionInfo())
		return;

	//------------------------------------------------------
	// when it is a skill used on others
	//------------------------------------------------------
	if ((*g_pActionInfoTable)[nActionInfo].IsTargetItem())
	{		
		//------------------------------------------------------
		// check when NextUsedACtionInfo already exists
		//------------------------------------------------------
		AffectBufferedActionInfo();
				
		// set the info sent from the Server..
		m_nNextUsedActionInfo	= nActionInfo;
		m_TraceX		= m_X;
		m_TraceY		= m_Y;
		m_TraceZ		= m_Z;
		SetTraceID( m_ID );

		BYTE nextAction = GetActionInfoAction(nActionInfo);
		SetNextAction( nextAction );

		if (nextAction!=ACTION_STAND)
		{
			m_bNextAction = true;
		}

		//---------------------------------------------------------------
		// when there is already a result to show..
		//---------------------------------------------------------------
		if (m_pActionResult != NULL)
		{
			DEBUG_ADD("Execute Old Result");

			// handle the result that already existed.
			// [ TEST CODE ]
			// result handling..
			// (!) the m_pActionResult value can change in Execute
			//		it must be saved and then deleted.
			MActionResult* pOldResult = m_pActionResult;

			m_pActionResult = NULL;

			pOldResult->Execute();

			delete pOldResult;	
		}
	}
	//------------------------------------------------------
	// otherwise
	//------------------------------------------------------
	else
	{
		DEBUG_ADD_FORMAT("[Skill : %s] Target is Not Item", (*g_pActionInfoTable)[nActionInfo].GetName());							
	}
}

//----------------------------------------------------------------------
// Packet SpecialAction To Nobody
//----------------------------------------------------------------------
//
// when the target is not in the Zone...
//
//----------------------------------------------------------------------
void		
MCreature::PacketSpecialActionToNobody(TYPE_ACTIONINFO nActionInfo, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	//--------------------------------------------------
	// if dead, return
	//--------------------------------------------------
	if (!m_bAlive || nActionInfo>=(*g_pActionInfoTable).GetMinResultActionInfo())
		return;

	//------------------------------------------------------
	// when it is a skill used on others
	//------------------------------------------------------
	if ((*g_pActionInfoTable)[nActionInfo].IsTargetOther())
	{
		//---------------------------------------------------------------
		// yum yum.. 2001.10.28
		//---------------------------------------------------------------
		AffectMoveBufferAll();

		ActionMoveNextPosition();

		//------------------------------------------------------
		// check when NextUsedACtionInfo already exists
		//------------------------------------------------------
		AffectBufferedActionInfo();
				
		// set the info sent from the Server..
		m_nNextUsedActionInfo	= nActionInfo;
		m_TraceX		= x;
		m_TraceY		= y;
		m_TraceZ		= 0;
		SetTraceID( OBJECTID_NULL );

		// look toward the target.
		//SetDirectionToPosition(pCreature->GetX(), pCreature->GetY());
		
		//m_ActionCount = 0;

		// [ TEST CODE ]	
		BYTE nextAction = GetActionInfoAction(nActionInfo);
		SetNextAction( nextAction );

		if (nextAction!=ACTION_STAND)
		{
			m_bNextAction = true;
		}
	}
	//------------------------------------------------------
	// otherwise
	//------------------------------------------------------
	else
	{
		DEBUG_ADD_FORMAT("[Skill : %s] Target is Not Other", (*g_pActionInfoTable)[nActionInfo].GetName());	
	}
}

//----------------------------------------------------------------------
// Packet SpecialAction To Self
//----------------------------------------------------------------------
// this Creature takes the action according to nActionInfo.
//
// 0. stop all actions and..
// 1. SpecialAction
//----------------------------------------------------------------------
void		
MCreature::PacketSpecialActionToSelf(TYPE_ACTIONINFO nActionInfo, MActionResult* pActionResult)
{
	//--------------------------------------------------
	// if dead, return
	//--------------------------------------------------
	if (!m_bAlive || nActionInfo>=(*g_pActionInfoTable).GetMinResultActionInfo() || nActionInfo == ACTIONINFO_NULL)
	{
		if (pActionResult!=NULL)
		{	
			delete pActionResult;
		}
		return;
	}

	//------------------------------------------------------
	// when it is a skill used on oneself
	//------------------------------------------------------
	if ((*g_pActionInfoTable)[nActionInfo].IsTargetSelf())
	{
		//---------------------------------------------------------------
		// yum yum.. 2001.10.28
		//---------------------------------------------------------------
		AffectMoveBufferAll();

		ActionMoveNextPosition();

		// stop moving.
	//	m_sX = 0;	
	//	m_sY = 0;
		//------------------------------------------------------
		// check when NextUsedACtionInfo already exists
		//------------------------------------------------------
		AffectBufferedActionInfo();
				
		// set the info sent from the Server..
		m_nNextUsedActionInfo	= nActionInfo;
		m_TraceX		= m_X;
		m_TraceY		= m_Y;
		m_TraceZ		= m_Z;	// is the z value.. meaningful..
		SetTraceID( m_ID ) ;
		
	//	m_ActionCount = 0;

		// [ TEST CODE ]	
		BYTE nextAction = GetActionInfoAction(nActionInfo, true);
		SetNextAction( nextAction );

		if (nextAction!=ACTION_STAND)
		{
			m_bNextAction = true;
		}

		//---------------------------------------------------------------
		// when there is already a result to show..
		//---------------------------------------------------------------
		if (m_pActionResult != NULL)
		{
			DEBUG_ADD("Execute Old Result");

			// handle the result that already existed.
			// [ TEST CODE ]
			// result handling..
			// (!) the m_pActionResult value can change in Execute
			//		it must be saved and then deleted.
			MActionResult* pOldResult = m_pActionResult;

			m_pActionResult = NULL;

			pOldResult->Execute();

			delete pOldResult;	
		}

		// set the result
		m_pActionResult = pActionResult;
	}
	//------------------------------------------------------
	// otherwise
	//------------------------------------------------------
	else
	{
		if (pActionResult!=NULL)
		{	
			delete pActionResult;
		}
		DEBUG_ADD_FORMAT("[Skill : %s] Target is Not Self", (*g_pActionInfoTable)[nActionInfo].GetName());							
	}
}

//----------------------------------------------------------------------
// Set Status
//----------------------------------------------------------------------
void	
MCreature::SetStatus(DWORD n, DWORD value)
{
	if(n >= MAX_MODIFY)
		return;

	DWORD oldValue = m_Status[n];

	m_Status[n] = value; 

	switch (n)
	{
		//--------------------------------------------------
		// Guild ID changed
		//--------------------------------------------------
		case MODIFY_GUILDID :
		{
			m_Status[n] = value;
			SetGuildNumber( value );
		}
		break;
		case MODIFY_UNIONID :
		{
			SetUnionGuildID( value );
		}
		break;

		// 2004, 10, 19, sobeit add start

		// 2004, 10, 19, sobeit add end
		//--------------------------------------------------
		// alignment changed
		//--------------------------------------------------
		case MODIFY_ALIGNMENT :			// alignment
		{
			//--------------------------------------------------
			// convert -10000 ~ 10000 to 0~4.
			//--------------------------------------------------
			value = ConvertAlignment( value );
			m_Status[n] = value;	
		}
		break;

		//--------------------------------------------------
		// when HP changes
		//--------------------------------------------------
		case MODIFY_CURRENT_HP :
			{
				const int localValue = min(GetMAX_HP()-GetSilverDamage(), value);
				AddHPModify( localValue - oldValue );

				// so as not to exceed max
				m_Status[n] = localValue;	

				// in a party, set HP again.
				if (IsPlayerParty())
				{
					PARTY_INFO* pInfo = g_pParty->GetMemberInfo( GetName() );
						
					// correct the coordinate.
					if (pInfo!=NULL)
					{
						pInfo->HP = localValue;
					}
				}
			}
		break;

		//--------------------------------------------------
		// when MaxHP changes
		//--------------------------------------------------
		case MODIFY_MAX_HP :
			// in a party, set HP again.
			if (IsPlayerParty())
			{
				PARTY_INFO* pInfo = g_pParty->GetMemberInfo( GetName() );
					
				// correct the coordinate.
				if (pInfo!=NULL)
				{
					pInfo->MaxHP = static_cast<WORD>(value);
				}
			}
		break;

//		//--------------------------------------------------
//		// when Armageddon HP changes
//		//--------------------------------------------------
//		case MODIFY_ARMAGEDDON_HP :
//		{
//			int changeValue = IsChangedArmageddonSprite(oldValue, value);
//
//			if (HasEffectStatus(EFFECTSTATUS_ARMAGEDDON)
//				&& changeValue)
//			{
//				RemoveEffectStatus(EFFECTSTATUS_ARMAGEDDON);
//
//				// add again
//				if (value!=0)
//				{
//					AddEffectStatus(EFFECTSTATUS_ARMAGEDDON, 0xFFFF);
//				}
//
//				// show the breaking part
//				int skillID = ARMAGEDDON_CRASH_1 + (changeValue-1);
//				
//				//--------------------------------------------------
//				// breaking Effect
//				//--------------------------------------------------		
//				ExecuteActionInfoFromMainNode(
//					skillID,
//				
//					m_X, m_Y, 0,
//					m_Direction,
//					
//					m_ID,												// info about the target
//					m_X, m_Y, 0, 
//					
//					0,													// (remaining) duration of the skill
//					
//					NULL,
//					
//					true);
//			}
//		}
//		break;
	}
}

//----------------------------------------------------------------------
// Change To Slayer
//----------------------------------------------------------------------
// transforms into slayer..
// if already slayer.. return false
//----------------------------------------------------------------------
bool	
MCreature::ChangeToSlayer()
{
	//-----------------------------------------------------
	// if already slayer..
	//-----------------------------------------------------
	if (IsSlayer())
	{
		return false;
	}

	ClearAttachEffect();

	//-----------------------------------------------------
	// set info
	//-----------------------------------------------------
	SetMoveDevice( MOVE_DEVICE_WALK );
	SetBasicActionInfo( SKILL_ATTACK_MELEE );

	//-----------------------------------------------------
	// stop it.
	//-----------------------------------------------------
	SetStop();

	//-----------------------------------------------------
	// male, female ?
	//-----------------------------------------------------
	if (IsMale())
	{
		SetCreatureType( CREATURETYPE_SLAYER_MALE );
	}
	else
	{
		SetCreatureType( CREATURETYPE_SLAYER_FEMALE );
	}

	//-----------------------------------------------------
	// flash
	//-----------------------------------------------------
	//g_pTopView->SetFadeStart(1, 31, 6,  5,5,31);

	return true;
}

//----------------------------------------------------------------------
// Change To Vampire
//----------------------------------------------------------------------
// transforms into vampire..
// if already vampire.. return false
//----------------------------------------------------------------------
bool
MCreature::ChangeToVampire()
{
	//-----------------------------------------------------
	// if already vampire..
	//-----------------------------------------------------
	if (IsVampire())
	{
		return false;
	}

	ClearAttachEffect();

	DEBUG_ADD("MCreature::ChangeToVampire - RemoveEffectStatus OK");
	
	//-----------------------------------------------------
	// set info
	//-----------------------------------------------------
	SetMoveDevice( MOVE_DEVICE_WALK );
	SetBasicActionInfo( SKILL_ATTACK_MELEE );

	//-----------------------------------------------------
	// stop it.
	//-----------------------------------------------------
	SetStop();

	//-----------------------------------------------------
	// male, female ?
	//-----------------------------------------------------
	if (IsMale())
	{
		// transformation sound
		WorldEvents::PlaySoundAt(SOUND_VAMPIRE_CHANGE_MALE,
					false,
					m_X,
					m_Y);

		SetCreatureType( CREATURETYPE_VAMPIRE_MALE1 );
	}
	else
	{
		// transformation sound
		WorldEvents::PlaySoundAt(SOUND_VAMPIRE_CHANGE_FEMALE,
					false,
					m_X,
					m_Y);

		SetCreatureType( CREATURETYPE_VAMPIRE_FEMALE1 );
	}

	//-----------------------------------------------------
	// remove outfit
	//-----------------------------------------------------
	// no need to do it.
	//for (int i=0; i<ADDON_MAX; i++)
	//{
	//	ClearAddonInfo( i );
	//}

	//-----------------------------------------------------
	// flash
	//-----------------------------------------------------
	//g_pTopView->SetFadeStart(1, 31, 6,  31,0,0);

	DEBUG_ADD("MCreature::ChangeToVampire OK");
	
	return true;
}

//----------------------------------------------------------------------
// Fast Move Position
//----------------------------------------------------------------------
// (m_X, m_Y) --> (x, y)
//----------------------------------------------------------------------
bool		
MCreature::FastMovePosition(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y,bool server)
{
	(void)server;
	//if (m_X!=x || m_Y!=y)
	{
		//------------------------------------------------
		// stop
		//------------------------------------------------
		SetStop();

		m_nNextUsedActionInfo = ACTIONINFO_NULL;
		m_bNextAction = false;
		
		//------------------------------------------------
		// set fast move
		//------------------------------------------------	
		SetAction( m_MoveAction );

		//MovePosition( x, y );

		m_NextX = x;
		m_NextY = y;
		SetServerPositionWithBlock( x, y );

		// must be set after SetAction.
		m_bFastMove = true;

		return true;
	}

	// anyway, assume the server coordinate has been verified.
	//m_ServerX = x;
	//m_ServerY = y;

	return false;
}

//----------------------------------------------------------------------
// KnockBackPosition
//----------------------------------------------------------------------
bool
MCreature::KnockBackPosition(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, BYTE Action)
{
	//if (m_X!=x || m_Y!=y)
	{
		// Whoa, this isn't bool.. yet it starts with m_b - -; too lazy to recompile  --;

		if( Action == 0 )
		{
			//int knockBackCount = (*g_pCreatureTable)[m_CreatureType].GetActionCount( ACTION_DAMAGED );
			int knockBackCount = GetCreatureActionCountMax( this, ACTION_DAMAGED );
			
			if (knockBackCount<=0)
			{
				// there is no being-hit action - -;
				return false;
			}
			
			//------------------------------------------------
			// stop
			//------------------------------------------------
			SetStop();
			
			m_nNextUsedActionInfo = ACTIONINFO_NULL;
			m_bNextAction = false;

			//------------------------------------------------
			// set damaged with the KnockBack action
			//------------------------------------------------	
			SetAction( ACTION_DAMAGED );
			
			//------------------------------------------------
			// play the Sound matching the character's Action.
			//------------------------------------------------
			TYPE_SOUNDID soundID = (*g_pCreatureTable)[m_CreatureType].GetActionSound( ACTION_DAMAGED );
			
			if (soundID!=SOUNDID_NULL)
			{
				WorldEvents::PlaySoundAt( soundID,
					false,
					m_X,
					m_Y);
			}
			m_bKnockBack = knockBackCount;
		}
		else
		{
			m_bKnockBack = 3;
			AddEffectStatus( EFFECTSTATUS_FADE_OUT, 8 );
		}
		
		
		//------------------------------------------------	
		// correct the coordinate with sx, sy
		//
		// move from the current position (m_X, m_Y) to the KnockBack position (x, y).
		// 
		//------------------------------------------------	
		int oldX = m_X;
		int oldY = m_Y;
		int newX = x;
		int newY = y;
		POINT	oldPoint = MapToPixel(oldX, oldY);
		POINT	newPoint = MapToPixel(newX, newY);
		
		// calculate the number of pixels to move
		int gapX = newPoint.x - oldPoint.x; //+ m_sX);
		int gapY = newPoint.y - oldPoint.y; //+ m_sY);

		m_sX = -gapX;
		m_sY = -gapY;

		// the number of pixels to move at once --> put in (m_cX, m_cY).
		m_cX = gapX / m_bKnockBack;
		m_cY = gapY / m_bKnockBack;

		
		// only if the coordinate changed..
		if (newX!=oldX || newY!=oldY)
		{
			//------------------------------------------------	
			// set the KnockBack direction
			// direction looking from the new coordinate to the old coordinate
			//------------------------------------------------	
			m_Direction = SelectFacingDirection(newX, newY, oldX, oldY);
		
			//------------------------------------------------	
			// coordinate moves immediately.
			//------------------------------------------------			
			MovePosition( x, y );
		}

		SetServerPositionWithBlock( x, y );

		if( Action == 0 )
		{
			//------------------------------------------------			
			// blood attached to the body
			//------------------------------------------------			
			MAttachEffect* pEffect = NULL;
			if(g_pUserInformation->GoreLevel == false)
			{
				pEffect = CreateAttachEffect( EFFECTSPRITETYPE_GREEN_BLOOD, 6, 6);		
			}
			else
			{
				pEffect = CreateAttachEffect( EFFECTSPRITETYPE_BLOOD, 6, 6);		
			}
			
			if (pEffect != NULL)
			{
				pEffect->SetDirection( m_Direction );
				pEffect->SetAttachCreatureID( m_ID );
			}						
			
			//------------------------------------------------			
			// 3 drops of blood spilled on the ground(-_-;)
			//------------------------------------------------			
			if (GameOptions::BloodDrop())
			{
				int num = rand()%3 + 3;
				for (int i=0; i<num; i++)
				{
					WorldEvents::ExecuteActionInfoFromMainNode(
						BLOOD_CRITICAL_HIT,										// skill number used
						
						oldX, oldY, 0,
						(int)m_Direction,														// direction used
						
						m_ID,												// info about the target
						oldX, oldY, 0,
						
						50 + rand()%30, //5*16, 
						
						NULL,
						
						false);	
				}
			}
		}
		SetServerPosition( x,y);

		return true;
	}

	// anyway, assume the server coordinate has been verified.
	//m_ServerX = x;
	//m_ServerY = y;

	return false;
}

//----------------------------------------------------------------------
// Stop Fast Move
//----------------------------------------------------------------------
void				
MCreature::StopFastMove()
{
	m_sX = 0;
	m_sY = 0;
	m_bFastMove = false;

	// 2001.8.10 added
	// Ugh.. but this function isn't even used.. - -;;
	m_MoveCount = m_MoveCountMax;
}

//----------------------------------------------------------------------
// Find Enemy
//----------------------------------------------------------------------
int
MCreature::FindEnemy()
{
	// It is used only in MPlayer now.
	// for the future...
	return OBJECTID_NULL;
}

//----------------------------------------------------------------------
// Set ShadowCount
//----------------------------------------------------------------------
void				
MCreature::SetShadowCount(int n)
{ 
	// up to 5.. - -;
	m_ShadowCount = min(5, n); 
}

//----------------------------------------------------------------------
// Stop Blood Drain
//----------------------------------------------------------------------
// m_bStopBloodDrainthen this must be called.
//----------------------------------------------------------------------
void	
MCreature::StopBloodDrain()
{
	if (m_Action==ACTION_VAMPIRE_DRAIN
		&& IsVampire()
		&& GetActionCountMax()>60)
	{
		if( m_ActionCount>9 && m_ActionCount<70 && GetCreatureType() == CREATURETYPE_WER_WOLF )
		{
			m_ActionCount = 70;
			UnSetStopBloodDrain();
		} else
		if (m_ActionCount>9 && m_ActionCount<60)
		{
			m_ActionCount = 60;

			UnSetStopBloodDrain();
		}	
	}
	else
	{
		//UnSetStopBloodDrain();
	}
}

//----------------------------------------------------------------------
// Stop Absorb Soul
//----------------------------------------------------------------------
// m_bStopAbsorbSoulthen this must be called.
//----------------------------------------------------------------------
void	
MCreature::StopAbsorbSoul()
{
	if (m_Action==ACTION_OUSTERS_DRAIN
		&& IsOusters()
		&& GetActionCountMax()>30)
	{
		if (m_ActionCount>9 && m_ActionCount<30)
		{
			m_ActionCount = 30;

			ATTACHEFFECT_LIST::iterator itr = m_listEffect.begin();
			ATTACHEFFECT_LIST::iterator endItr = m_listEffect.end();

			while(itr != endItr)
			{
				MAttachEffect*	pEffect = *itr;
				
				if(pEffect->GetEffectSpriteType() == EFFECTSPRITETYPE_ABSORB_SOUL)
				{
					bool bUseEffectSprite = pEffect->IsEffectSprite();
					
					// remove flag
					if (bUseEffectSprite)
					{
						m_bAttachEffect[pEffect->GetEffectSpriteType()] = false;	// remove flag
					}

					delete pEffect;
					m_listEffect.erase(itr);
					break;
				}
				itr++;
			}
			
			UnSetStopAbsorbSoul();
		}	
	}
	else
	{
		//UnSetStopBloodDrain();
	}
}

//----------------------------------------------------------------------
// Update Darkness
//----------------------------------------------------------------------
// simply change only the count.
//----------------------------------------------------------------------
void	
MCreature::UpdateDarkness()
{
	if (!IsVampire() || (IsVampire() && ZoneState::GetID() == 3001
		&& !PlayerState::HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		))
	{
		if (m_DarknessCountInc==0)
		{
			if (m_ActionCount>=GetActionCountMax()-1)
			{
				CheckInDarkness();
			}
		}
		else
		{
			m_DarknessCount += m_DarknessCountInc;

			// when entering darkness
			if (m_DarknessCount < 0)
			{
				m_DarknessCount = 0;
				m_DarknessCountInc = 0;
			}
			// when leaving darkness
			else if (m_DarknessCount >= MAX_DARKNESS_COUNT)
			{
				m_DarknessCount = -1;
				m_DarknessCountInc = 0;
			}
		}
	}
	else
	{
		// not related to Darkness unless Slayer.
		// for checking
		m_DarknessCount = -1;
		m_DarknessCountInc = 0;
	}
}

//----------------------------------------------------------------------
// Check In Darkness
//----------------------------------------------------------------------
// the moment darkness appears/disappears
//            just check the moment the player moves.
//
// not the player, so sight doesn't matter.
//----------------------------------------------------------------------
void
MCreature::CheckInDarkness()
{
	if (ZoneState::Exists())
	{
		//if (IsSlayer())
		{
			//-------------------------------------------------------
			// check whether it entered Darkness
			//-------------------------------------------------------
			// the current coordinate looks better than the server coordinate.. lol..
			if (m_X < ZoneState::GetWidth()
				&& m_Y < ZoneState::GetHeight())
			{
				const MSector& sector = *ZoneState::GetSector( m_X, m_Y );

				if (m_bAlive && (sector.HasDarkness()))
				{
					// when in darkness
					//PlaceInDarkness();
					m_DarknessCount = MAX_DARKNESS_COUNT;
					m_DarknessCountInc = 0;
				}
				else
				{		
					// darkness has disappeared.
					//PlaceNotInDarkness();
					m_DarknessCount = -1;
					m_DarknessCountInc = 0;
				}
			}
		}
	}
}

BOOL
MCreature::IsInGroundElemental() const
{
	if (ZoneState::Exists())
	{
		//if (IsSlayer())
		{
			//-------------------------------------------------------
			// check whether it entered Darkness
			//-------------------------------------------------------
			// the current coordinate looks better than the server coordinate.. lol..
			if (m_X < ZoneState::GetWidth()
				&& m_Y < ZoneState::GetHeight())
			{
				const MSector& sector = *ZoneState::GetSector( m_X, m_Y );

				if (m_bAlive && sector.HasGroundElemental())
				{
					return true;
				}
			}
		}
	}
	return false;
}

BOOL
MCreature::IsInFuryOfGnome() const
{
	if (ZoneState::Exists())
	{
		//if (IsSlayer())
		{
			//-------------------------------------------------------
			// check whether it entered Fury Of Gnome
			//-------------------------------------------------------
			// the current coordinate looks better than the server coordinate.. lol..
			if (m_X < ZoneState::GetWidth()
				&& m_Y < ZoneState::GetHeight())
			{
				const MSector& sector = *ZoneState::GetSector( m_X, m_Y );

				if (m_bAlive && sector.HasFuryOfGnome())
				{
					return true;
				}
			}
		}
	}
	return false;
}

//----------------------------------------------------------------------
// Place In Darkness
//----------------------------------------------------------------------
// the player is in darkness.
//----------------------------------------------------------------------
void
MCreature::PlaceInDarkness(bool IsBlindness)
{
	if (
		  (
				(IsBlindness || (!IsVampire() || (IsVampire() && ZoneState::GetID() == 3001)))
				
				&& !PlayerState::HasEffectStatus( EFFECTSTATUS_GHOST )
			)
		  
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		)
	{
		// when it was not in darkness
		if (m_DarknessCount < 0)
		{
			m_DarknessCount = MAX_DARKNESS_COUNT;		
			m_DarknessCountInc = -1;
		}	
		else if (m_DarknessCount > 0)
		{
			m_DarknessCountInc = -1;
		}
		else if (m_DarknessCount==0)
		{
			m_DarknessCountInc = 0;
		}
	}
}

//----------------------------------------------------------------------
// Move In Darkness
//----------------------------------------------------------------------
// the player is not in darkness.
//----------------------------------------------------------------------
void
MCreature::PlaceNotInDarkness()
{
	if (!IsVampire() || (IsVampire() && ZoneState::GetID() == 3001
		&& !PlayerState::HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		))
	{
		// when it was in darkness
		if (m_DarknessCount >= 0 && m_DarknessCount < MAX_DARKNESS_COUNT)
		{
			m_DarknessCountInc = 1;
		}	
	}
}

//----------------------------------------------------------------------
// Show In Darkness
//----------------------------------------------------------------------
// visible because not in darkness, or
// visible by distance even when in darkness?
//----------------------------------------------------------------------
BOOL	
MCreature::ShowInDarkness(int sX, int sY) const
{

//	DEBUG_ADD_FORMAT("[ShowInDarkness] g_bLight %d", g_bLight);
	// when already in darkness
	if (//!(m_CreatureType >= 526 && m_CreatureType <= 549 || m_CreatureType >= 371 && m_CreatureType <= 376 || m_CreatureType >= 560 && m_CreatureType <= 563) &&
//		!IsNPC() &&
		m_DarknessCount >= 0 &&
		(!HasEffectStatus( EFFECTSTATUS_LIGHTNESS ) || ZoneState::GetID() == 3001 )
		&& !PlayerState::HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
	)
	{
		// can see within darknessCount.
		if ((max(abs(m_X-sX), abs(m_Y-sY))) <= m_DarknessCount)
		{
			return TRUE;
		}

		return FALSE;
	}

	return TRUE;
}

//----------------------------------------------------------------------
// Set HalluCreature
//----------------------------------------------------------------------
// for hallu... it is shown as some mob.
//----------------------------------------------------------------------
void				
MCreature::SetHalluCreature(TYPE_CREATURETYPE type)
{
	// NPCs look correct even when Hallu is applied.
	if (IsNPC())
	{
		m_bHallu = false;
		return;
	}

	m_HalluCreatureType = type;
	
	m_HalluCreatureFrameID	= (*g_pCreatureSpriteTable)[(*g_pCreatureTable)[type].SpriteTypes[0]].FrameID;
	
	// color - set only for mobs..
	m_HalluColorBody	= (*g_pCreatureTable)[m_CreatureType].ColorSet;

	m_bHallu = true;
}

//----------------------------------------------------------------------
// Determine HalluAction
//----------------------------------------------------------------------
void			
MCreature::DetermineHalluActionFrame()
{
	//------------------------------------------------------
	// select the appropriate action.
	//------------------------------------------------------
	if (m_Action <= ACTION_DIE)
	{
		m_HalluAction = m_Action;
	}
	else if (m_Action==ACTION_SLAYER_MOTOR_STAND)
	{
		m_HalluAction = ACTION_STAND;
	}
	else if (m_Action==ACTION_SLAYER_MOTOR_MOVE)
	{
		m_HalluAction = ACTION_MOVE;
	}
	else
	{
		m_HalluAction = ACTION_ATTACK;
	}

	BYTE frame = GetFrame();

	//------------------------------------------------------
	// decide the number of frames of the action.
	//------------------------------------------------------
	int countMax = (*g_pCreatureTable)[m_HalluCreatureType].GetActionCount( m_HalluAction );

	if (countMax==0)
	{
		// if there is no action..
		if (m_HalluAction!=ACTION_ATTACK)
		{
			m_HalluAction = ACTION_ATTACK;

			countMax = (*g_pCreatureTable)[m_HalluCreatureType].GetActionCount( m_HalluAction );
		}

		// if there is no action, stop..
		if (countMax==0)
		{
			m_HalluAction = ACTION_STAND;

			countMax = (*g_pCreatureTable)[m_HalluCreatureType].GetActionCount( m_HalluAction );						
		}
	}
	
	if (countMax==0)	// compare compare.. - -;
	{
		m_HalluFrame = 0;
	}
	else
	{
		m_HalluFrame = frame % countMax;
	}
}

//----------------------------------------------------------------------
// Get HalluName
//----------------------------------------------------------------------
const char*			
MCreature::GetHalluName() const
{ 
	return m_NameSelection.GetHallucinationName(GetName(),
		(*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME], g_pMonsterNameTable);
}

//----------------------------------------------------------------------
// Set Regen
//----------------------------------------------------------------------
void				
MCreature::SetRegen(int amount, DWORD delay)
{ 
	// The player is this creature: its own type and dexterity.
	if(this == PlayerState::Creature() && IsVampire() && GetCreatureType() != CREATURETYPE_BAT)
	{
		// The server's HP regeneration bonus from the basic DEX
		// (VampireStat.cpp), 1 from DEX 51 up to 7 from DEX 451.
		amount += decore::vampireDexHPRegenBonus( (int)GetBASIC_DEX() );

		if((*g_pRankBonusTable)[RANK_BONUS_URANUS_BLESS].GetStatus() == RankBonusInfo::STATUS_LEARNED)
			amount += (*g_pRankBonusTable)[RANK_BONUS_URANUS_BLESS].GetPoint();

		// Will of Life adds the HP its cast costs (the server's
		// EffectWillOfLife bonus), at the level current when this runs. The
		// server keeps the bonus of the level the skill was cast at. That is
		// the same while only the cast's CheckRegen has run: a level change
		// (Function_MODIFY_LEVEL) calls no CheckRegen. If another call (an
		// effect status added: a removal calls it only for Will of Life's
		// own, a creature type change, a basic DEX
		// change, a rank bonus packet) follows a level change across a
		// multiple of seven while the effect lasts, the prediction is off by
		// 1 HP a tick until the server's HP updates correct it.
		if( HasEffectStatus( EFFECTSTATUS_WILL_OF_LIFE ) )
			amount += GetWillOfLifeHP( (int)GetLEVEL() );
	}
	m_RegenAmount = amount; 
	
	m_RegenDelayTime = delay; 
	m_RegenNextTime = g_FrameNow; 
}

//----------------------------------------------------------------------
// Set Regen Bonus
//----------------------------------------------------------------------
void				
MCreature::SetRegenBonus(int amount, DWORD delay)
{ 
	m_RegenBonusAmount = amount; 
	m_RegenBonusDelayTime = delay; 
	m_RegenBonusNextTime = g_FrameNow; 
}

//----------------------------------------------------------------------
// CheckRegen - Regen setting
//----------------------------------------------------------------------
void
MCreature::CheckRegen()
{
	//-------------------------------------------------------
	// do not Regen unless a vampire.
	//-------------------------------------------------------
	if (!IsVampire())
	{
		SetRegen( 0, 0 );
		return;
	}

	//-------------------------------------------------------
	// Burrow state
	//-------------------------------------------------------
	if (IsUndergroundCreature())
	{
		SetRegen( g_pClientConfig->REGEN_AMOUNT_BURROW, 1000 );
		return;
	}

	//-------------------------------------------------------
	// Casket state
	//-------------------------------------------------------
	if (IsInCasket())
	{
		SetRegen( g_pClientConfig->REGEN_AMOUNT_CASKET, 1000 );
		return;
	}

	//-------------------------------------------------------
	// depending on Creature Type..
	//-------------------------------------------------------
	switch (m_CreatureType)
	{
		//---------------------------------------------------------------
		// vampire, wolf
		//---------------------------------------------------------------
		case CREATURETYPE_VAMPIRE_MALE1 :
		case CREATURETYPE_VAMPIRE_FEMALE1 :
		case CREATURETYPE_VAMPIRE_MALE2 :
		case CREATURETYPE_VAMPIRE_FEMALE2 :
		case CREATURETYPE_VAMPIRE_MALE3 :
		case CREATURETYPE_VAMPIRE_FEMALE3 :
		case CREATURETYPE_VAMPIRE_OPERATOR :
		case CREATURETYPE_WOLF :
		case CREATURETYPE_WER_WOLF :
		// add by coffee 2006.11.24  
		case CREATURETYPE_VAMPIRE_FEMALE4:
		case CREATURETYPE_VAMPIRE_MALE4:
		//add by viva
		case CREATURETYPE_VAMPIRE_MALE5:
		case CREATURETYPE_VAMPIRE_FEMALE5:
		case CREATURETYPE_VAMPIRE_MALE6:
		case CREATURETYPE_VAMPIRE_FEMALE6:
		// add end
			SetRegen( g_pClientConfig->REGEN_AMOUNT_VAMPIRE, 1000 );	// 2 every 1 second
		break;

		//---------------------------------------------------------------
		// other cases..
		//---------------------------------------------------------------
		default :
			SetRegen( 0, 0 );	// no regen.
	}
}

//----------------------------------------------------------------------
// Stop the blood-sucking action of the Creature that was sucking my blood
//----------------------------------------------------------------------
void 
MCreature::StopDrain()
{
	if(m_Action == ACTION_DRAINED)
	{
		MCreature *pCreature = ZoneState::GetCreature(GetDrainCreatureID());
		if(pCreature != NULL)
		{
			pCreature->StopBloodDrain();
		}
	}
}

//----------------------------------------------------------------------
// Stop the soul-absorbing action of the Creature that was absorbing my soul
//----------------------------------------------------------------------
void 
MCreature::StopAbsorb()
{
	if(m_Action == ACTION_DRAINED)
	{
		MCreature *pCreature = ZoneState::GetCreature(GetAbsorbCreatureID());
		if(pCreature != NULL)
		{
			pCreature->StopAbsorb();
		}
	}
}

void
MCreature::AddHPModify(const int modify)
{
	m_HPModifyList.push_back(HPModify(modify, MonotonicClock::Now()));
	if(m_HPModifyList.size() > g_pClientConfig->HPModifyListMax)m_HPModifyList.pop_front(); 
}

void 
MCreature::SetTraceID(TYPE_OBJECTID id)
{
	m_TraceID = id;	
}

TYPE_ACTIONINFO		MCreature::GetBasicActionInfo() const
{ 	
	return m_nBasicActionInfo; 
}

int MCreature::GetHeadSkin() const

{
	return m_HeadSkin;
}


bool MCreature::UpdateBurningSol()
{
	if(m_bBurningSol)
	{
		if(IsDead())
		{
			StopBurningSol();
			SetStop();
			return false ;
		}
		else if(HasEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS))
		{
			StopBurningSol();
			SetStop();
			RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1);
			RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2);
			RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_3);
			RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_4);
			return false;
		}
		int bCheckFrame = 0;
		if(!IsAdvancementClass())
			bCheckFrame = IsMale()?7:11;
		else
			bCheckFrame = 7;

		m_ActionCount = m_bBurningSolCount;

		m_bBurningSolCount++;
		if(	HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1)||
			HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2)||
			HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_3)||
			HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_4))
		{
			if(m_bBurningSolCount>=bCheckFrame)
				m_bBurningSolCount = bCheckFrame;
		}
		if(m_ActionCount>=GetActionCountMax())
			StopBurningSol();
		return true;
	#ifdef OUTPUT_DEBUG					
		DEBUG_ADD_FORMAT("[UpdateBurningSol] ActionCount : %d", m_bBurningSolCount);
	#endif
	}
	return false;
}
void MCreature::InstallTurretStopAttack()
{
	m_nUsedActionInfo = ACTIONINFO_NULL;
	if(m_Action == ACTION_ATTACK)
	{
		if(m_ActionCount<14)
			m_ActionCount = 54 - m_ActionCount;
		else if(m_ActionCount<20)
			m_ActionCount = 20;
		m_ActionCountMax = 54;
		WorldEvents::PlaySound( SOUND_SLAYER_TURRET_FIRE_END );
		
//		if(m_ActionCount<28)
//			m_ActionCount = 75 - m_ActionCount;
//		else if(m_ActionCount<42)
//			m_ActionCount = 42;
//		m_ActionCountMax = 75;
	}
}
bool MCreature::UpDateInstallTurret()
{
	if(HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
	{
		if(IsAlive() && (m_Action == ACTION_STAND))
		{
			if (m_bInstallTurretDirect != m_Direction)
			{
				if( ((m_bInstallTurretDirect - m_Direction + 8)%8) < 4 )
				{
					m_bInstallTurretCount = ((m_bInstallTurretCount-1)+16)%16;
				}
				else
				{
					m_bInstallTurretCount = (m_bInstallTurretCount+1)%16;
				}

				if(!(m_bInstallTurretCount&0x01))
					m_bInstallTurretDirect = ((m_bInstallTurretCount /2) +2 )%8;

			}
			else
			{
				if(!(g_CurrentFrame%100))
					SetDirection((m_bInstallTurretDirect + 2+ rand()%5)%8);
			}
			return true;
		}
		else if( m_Action == ACTION_ATTACK)
		{
			if(m_ActionCount<20)
				m_ActionCount = 20;
	
			m_ActionCountMax = 54;
		}

//		
		SetInstallTurretDirect(m_CurrentDirection);
		SetInstallTurretCount(((m_CurrentDirection+6)%8)*2);

	}
	return false;

}

BYTE		MCreature::GetActionCountMax() const
{ 
	//if( IsAdvancementClass() )
	//	return GetCreatureActionCountMax(( this, GetAction() );
	return m_ActionCountMax; 
}
// 2005, 1, 5, sobeit add start
void		MCreature::UpdateBikeCrash()
{
	if(HasEffectStatus(EFFECTSTATUS_BIKE_CRASH))
	{
		int direction = (GetDirection() + 1)%8;
		int TempDir = GetInstallTurretDirect();
		
		if(GetAction() == ACTION_SLAYER_MOTOR_STAND)
		{
			if(direction == ((TempDir+3)%8))
			{
				WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_BIKE_CRASH, GetX(), GetY(), 0, GetDirection(),	GetID(),
					GetX(), GetY(), 0, 5000, NULL, false);	
			}
			SetDirection(direction);
			SetCurrentDirection(direction);
			if(direction == TempDir)
			{
				m_bInstallTurretCount ++;
				if(m_bInstallTurretCount == 3)
				{
					RemoveEffectStatus(EFFECTSTATUS_BIKE_CRASH);
					if(PlayerState::GetID() == GetID())
						PlayerState::SetDelay(0);
				}
			}
		}
//		else
//		{
//			if(direction < ((TempDir+3)%8))
//			{
//				SetDirection(direction);
//				SetCurrentDirection(direction);
//			}
//		}
	}
	
}
// 2005, 1, 5, sobeit add end	

void 
MCreature::RemoveCauseCriticalWoundsEffect()
{
	SetStop();
	StopCauseCriticalWounds();
	ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();

	while (iEffect != m_listEffect.end())
	{
		MAttachEffect* pEffect = *iEffect;
		TYPE_EFFECTSPRITETYPE Type = pEffect->GetEffectSpriteType();
		if(	
			EFFECTSPRITETYPE_CAUSE_CRITICAL_WOUND_2 == Type 
			||EFFECTSPRITETYPE_CAUSE_CRITICAL_WOUND_1 == Type 
			||EFFECTSPRITETYPE_CAUSE_CRITICAL_WOUND_3 == Type 
			||EFFECTSPRITETYPE_CAUSE_CRITICAL_WOUND_4 == Type 
			)
		{
			// free the memory
			delete pEffect;

			// remove from the list; erase() hands back the following
			// element, so the loop must not step forward again below.
			iEffect = m_listEffect.erase( iEffect );

			m_bAttachEffect[Type] = false;

			continue;
		}
		
		iEffect++;

	}
	iEffect = m_listGroundEffect.begin();

	while (iEffect != m_listGroundEffect.end())
	{
		MAttachEffect* pEffect = *iEffect;
		TYPE_EFFECTSPRITETYPE Type = pEffect->GetEffectSpriteType();
		if(	
			EFFECTSPRITETYPE_CAUSE_CRITICAL_WOUND_2 == Type 
			||EFFECTSPRITETYPE_CAUSE_CRITICAL_WOUND_1 == Type 
			||EFFECTSPRITETYPE_CAUSE_CRITICAL_WOUND_3 == Type 
			||EFFECTSPRITETYPE_CAUSE_CRITICAL_WOUND_4 == Type 
			)
		{
			// free the memory
			delete pEffect;

			// remove from the list; erase() hands back the following
			// element, so the loop must not step forward again below.
			iEffect = m_listGroundEffect.erase( iEffect );

			m_bAttachEffect[Type] = false;

			continue;
		}
		
		iEffect++;

	}
	m_bEffectStatus[EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS]  = false;

}

// 2005, 1, 27, sobeit add start- max action count;
int
MCreature::GetActionMax() const
{
	if(!IsAdvancementClass())
		 return (*g_pCreatureTable)[m_CreatureType].GetActionMax(); 

	// for a promoted character
	if(IsSlayer())
		return ACTION_ADVANCEMENT_SLAYER_MAX/* - ACTION_ADVANCEMENT_SLAYER_STOP_SWORD*/;
	else if(IsVampire())
		return ACTION_ADVANCEMENT_MAX/* - ACTION_ADVANCEMENT_STOP*/;
	else if(IsOusters())
		return ACTION_ADVANCEMENT_OUSTERS_MAX/* - ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_STOP*/;

	return 0;
}

// 2005, 1, 27, sobeit add end

// 2005, 2, 22, sobeit add start
int 
MCreature::GetMasterEffectType(DWORD Status)
{
	TYPE_EFFECTSPRITETYPE type = (*g_pEffectStatusTable)[Status].EffectSpriteType;
	int MarketType = GetMasterEffectType();
	
	//if(MarketType>3) return 0;
	
	switch(Status)
	{
		case EFFECTSTATUS_GRAND_MASTER_SLAYER:
			{
				if(MarketType) 
					type =  EFFECTSPRITETYPE_MARKET_MASTER_SLAYER_100_FEAR + MarketType - 1;
			}
			break;
		case EFFECTSTATUS_GRAND_MASTER_VAMPIRE:
			{
				if(MarketType) 
					type =  EFFECTSPRITETYPE_MARKET_MASTER_VAMPIRE_100_FEAR + MarketType - 1;
			}
			break;
		case EFFECTSTATUS_GRAND_MASTER_OUSTERS:
			{
				if(MarketType) 
					type =  EFFECTSPRITETYPE_MARKET_MASTER_OUSTERS_100_FEAR + MarketType - 1;
			}
			break;
			
		case EFFECTSTATUS_GRAND_MASTER_SLAYER_130:
			{
				RemoveEffectStatus(EFFECTSTATUS_GRAND_MASTER_SLAYER);
				if(IsDead()) return 0; //ignore effects on the dead
				if(MarketType) 
					type =  EFFECTSPRITETYPE_MARKET_MASTER_SLAYER_130_FEAR + MarketType - 1;
			}
			break;
		case EFFECTSTATUS_GRAND_MASTER_SLAYER_150:
			{
				RemoveEffectStatus(EFFECTSTATUS_GRAND_MASTER_SLAYER_130);
				if(IsAdvancementClass())
				{
					switch(MarketType)
					{
					//case 0:
					case 1:
					case 2:
					case 3:
						type =  EFFECTSPRITETYPE_MARKET_MASTER_SLAYER_150_FEAR + MarketType - 1;
					    break;
					case 4:
						type =  EFFECTSPRITETYPE_MARKET_MASTER_SLAYER_150_ADVANCE_NEW_4;
						break;
					}
					// commented out because promoted characters were told not to apply the Market Master Effect..
					// if they change their mind later, just uncomment.
//					if(MarketType) 
//						type =  EFFECTSPRITETYPE_MARKET_MASTER_SLAYER_150_ADVANCE_FEAR + MarketType - 1;
//					else
					// edit By Sonic 2006.10.28 remove the ineffective light on the character after the second transfer

					//	type = EFFECTSPRITETYPE_ADVANCEMENT_MASTER_SLAYER;
// 					if(MarketType)
// 						type =  EFFECTSPRITETYPE_MARKET_MASTER_SLAYER_150_FEAR + MarketType - 1;
					// end  By Sonic 2006.10.28
				}
				else
				{
					if(MarketType) 
						type =  EFFECTSPRITETYPE_MARKET_MASTER_SLAYER_150_FEAR + MarketType - 1;
				}
			}
			break;
		case EFFECTSTATUS_GRAND_MASTER_VAMPIRE_130:
			{
				RemoveEffectStatus(EFFECTSTATUS_GRAND_MASTER_VAMPIRE);
				if(MarketType) 
					type =  EFFECTSPRITETYPE_MARKET_MASTER_VAMPIRE_130_FEAR + MarketType - 1;
			}
			break;
		case EFFECTSTATUS_GRAND_MASTER_VAMPIRE_150:
			{
				RemoveEffectStatus(EFFECTSTATUS_GRAND_MASTER_VAMPIRE_130);
				if(IsAdvancementClass())
				{
					switch(MarketType)
					{
					//case 0:
					case 1:
					case 2:
					case 3:
						type =  EFFECTSPRITETYPE_MARKET_MASTER_VAMPIRE_150_FRONT_FEAR + MarketType - 1;
					    break;
					case 4:
						type =  EFFECTSPRITETYPE_MARKET_MASTER_VAMPIRE_150_ADVANCE_NEW_4;
						break;	
					}
					// commented out because promoted characters were told not to apply the Market Master Effect..
					// if they change their mind later, just uncomment.
//					if(MarketType) 
//						type =  EFFECTSPRITETYPE_MARKET_MASTER_VAMPIRE_150_FRONT_ADVANCE_FEAR + MarketType - 1;
//					else
						// edit by sonic 2006.10.28   remove the fire color effect on the body that cannot be seen after the second transfer
							//type =  EFFECTSPRITETYPE_ADVANCEMENT_MASTER_VAMPIRE_BACK;
// 							if(MarketType)
// 								type =  EFFECTSPRITETYPE_MARKET_MASTER_VAMPIRE_150_FRONT_FEAR + MarketType - 1;
						// end by sonic 2006.10.28
						
				}
				else
				{
					if(MarketType) 
						type =  EFFECTSPRITETYPE_MARKET_MASTER_VAMPIRE_150_FRONT_FEAR + MarketType - 1;
				}
			}
			break;
		case EFFECTSTATUS_GRAND_MASTER_OUSTERS_130:
			{
				RemoveEffectStatus(EFFECTSTATUS_GRAND_MASTER_OUSTERS);
				if(MarketType) 
					type =  EFFECTSPRITETYPE_MARKET_MASTER_OUSTERS_130_FEAR + MarketType - 1;
			}
			break;
		case EFFECTSTATUS_GRAND_MASTER_OUSTERS_150:
			{
				RemoveEffectStatus(EFFECTSTATUS_GRAND_MASTER_OUSTERS_130);
	
				if(IsAdvancementClass())
				{
					switch(MarketType)
					{
					//case 0:
					case 1:
					case 2:
					case 3:
						type =  EFFECTSPRITETYPE_MARKET_MASTER_OUSTERS_150_FEAR + MarketType - 1;
					    break;
					case 4:
						type =  EFFECTSPRITETYPE_MARKET_MASTER_OUSTERS_150_ADVANCE_NEW_4;
						break;	
					}	
					// commented out because promoted characters were told not to apply the Market Master Effect..
					// if they change their mind later, just uncomment.
//					if(MarketType) 
//						type =  EFFECTSPRITETYPE_MARKET_MASTER_OUSTERS_150_ADVANCE_FEAR + MarketType - 1;
//					else
					// edit by sonic 2006.10.28  fix the fire color problem of the character after the second transfer
						//type =  EFFECTSPRITETYPE_ADVANCEMENT_MASTER_OUSTER;
// 					if(MarketType)
// 					type =  EFFECTSPRITETYPE_MARKET_MASTER_OUSTERS_150_FEAR + MarketType - 1;

					// end by sonic 2006.10.28
				}
				else
				{
					if(MarketType) 
						type =  EFFECTSPRITETYPE_MARKET_MASTER_OUSTERS_150_FEAR + MarketType - 1;
				}

			}
			break;
	}
	
	return type;
}

void 
MCreature::ChangeMasterEffectType(int MarketEffect)
{
	int status = 0;

	if(IsSlayer())
	{
		if(HasEffectStatus(EFFECTSTATUS_GRAND_MASTER_SLAYER))		status = EFFECTSTATUS_GRAND_MASTER_SLAYER;
		else if(HasEffectStatus(EFFECTSTATUS_GRAND_MASTER_SLAYER_130))	status = EFFECTSTATUS_GRAND_MASTER_SLAYER_130;
		else if(HasEffectStatus(EFFECTSTATUS_GRAND_MASTER_SLAYER_150))	status = EFFECTSTATUS_GRAND_MASTER_SLAYER_150;
	}
	else if(IsVampire())
	{
		if(HasEffectStatus(EFFECTSTATUS_GRAND_MASTER_VAMPIRE))		status = EFFECTSTATUS_GRAND_MASTER_VAMPIRE;
		else if(HasEffectStatus(EFFECTSTATUS_GRAND_MASTER_VAMPIRE_130))	status = EFFECTSTATUS_GRAND_MASTER_VAMPIRE_130;
		else if(HasEffectStatus(EFFECTSTATUS_GRAND_MASTER_VAMPIRE_150))	status = EFFECTSTATUS_GRAND_MASTER_VAMPIRE_150;
	}
	else if(IsOusters())
	{
		if(HasEffectStatus(EFFECTSTATUS_GRAND_MASTER_OUSTERS))		status = EFFECTSTATUS_GRAND_MASTER_OUSTERS;
		else if(HasEffectStatus(EFFECTSTATUS_GRAND_MASTER_OUSTERS_130))	status = EFFECTSTATUS_GRAND_MASTER_OUSTERS_130;
		else if(HasEffectStatus(EFFECTSTATUS_GRAND_MASTER_OUSTERS_150))	status = EFFECTSTATUS_GRAND_MASTER_OUSTERS_150;
	}

	if( 0 == status )
		return;
		
	RemoveEffectStatus((EFFECTSTATUS)status);

	SetMasterEffectType(MarketEffect);

	AddEffectStatus((EFFECTSTATUS)status, 0xffff);
	
		
//	ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();
//
//	bool re = false;
//	while (iEffect != m_listEffect.end())
//	{
//		MAttachEffect* pEffect = *iEffect;
//		TYPE_EFFECTSPRITETYPE Type = pEffect->GetEffectSpriteType();
//		if(	
//			Type >= EFFECTSPRITETYPE_GRAND_MASTER_ALPHA_VAMPIRE_100_NEW && Type <= EFFECTSPRITETYPE_GRAND_MASTER_ALPHA_OUSTERS_150_NEW
//			|| Type >= EFFECTSPRITETYPE_ADVANCEMENT_MASTER_VAMPIRE_BACK && Type <= EFFECTSPRITETYPE_ADVANCEMENT_MASTER_OUSTER
//			|| Type >= EFFECTSPRITETYPE_MARKET_MASTER_VAMPIRE_100_FEAR && Type <= EFFECTSPRITETYPE_MARKET_MASTER_OUSTERS_150_ADVANCE_HOPE
//			)
//		{
//			// remove from memory
//			delete pEffect;
//
//			ATTACHEFFECT_LIST::iterator dEffect = iEffect;
//			iEffect--;
//
//			// remove from list
//			m_listEffect.erase( dEffect );
//
//			m_bAttachEffect[Type] = false;
//	
//			re = true;
//		}
//		
//		iEffect++;
//
//	}
//	iEffect = m_listGroundEffect.begin();
//
//	while (iEffect != m_listGroundEffect.end())
//	{
//		MAttachEffect* pEffect = *iEffect;
//		TYPE_EFFECTSPRITETYPE Type = pEffect->GetEffectSpriteType();
//		if(	
//			Type >= EFFECTSPRITETYPE_GRAND_MASTER_ALPHA_VAMPIRE_100_NEW && Type <= EFFECTSPRITETYPE_GRAND_MASTER_ALPHA_OUSTERS_150_NEW
//			|| Type >= EFFECTSPRITETYPE_ADVANCEMENT_MASTER_VAMPIRE_BACK && Type <= EFFECTSPRITETYPE_ADVANCEMENT_MASTER_OUSTER
//			|| Type >= EFFECTSPRITETYPE_MARKET_MASTER_VAMPIRE_100_FEAR && Type <= EFFECTSPRITETYPE_MARKET_MASTER_OUSTERS_150_ADVANCE_HOPE
//			)
//		{
//			// remove from memory
//			delete pEffect;
//
//			ATTACHEFFECT_LIST::iterator dEffect = iEffect;
//			iEffect--;
//
//			// remove from list
//			m_listGroundEffect.erase( dEffect );
//
//			m_bAttachEffect[Type] = false;
//	
//			re = true;
//		}
//		
//		iEffect++;
//
//	}
//	m_bEffectStatus[Status]  = false;

}
// 2005, 2, 22, sobeit add end
