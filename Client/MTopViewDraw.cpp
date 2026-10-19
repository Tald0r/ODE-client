//#define __3D_IMAGE_OBJECT__					// by sonee
//----------------------------------------------------------------------
// MTopViewDraw.cpp
//----------------------------------------------------------------------
//
// Only the complicated Draw functions are split out separately.
// Because working on them was very inconvenient.
//----------------------------------------------------------------------
#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif
#include "Client_PCH.h"
// DX3D.h removed (SDL2) - Direct3D has been replaced with SDL2
#include <math.h>
#include <list>
#include <stdio.h>
#include <fstream>
#include "MZone.h"
#include "MCreature.h"
#include "MFakeCreature.h"
#include "MPlayer.h"
#include "MItem.h"
#include "MImageObject.h"
#include "MAnimationObject.h"
#include "MShadowAnimationObject.h"
#include <assert.h>

#include "MMovingEffect.h"
#include "MScreenEffect.h"
#include "MTopView.h"
#include "MItemTable.h"
#include "MCreatureTable.h"
#include "MWeather.h"
#include "../basic/CMessageArray.h"
#include "DXLib/DXLib.h"
#include "SP.h"
#include "Fl2.h"
//#include "2D.h"
#include "ClientConfig.h"
#include "VS_UI_Base.h"
#include "VS_UI_mouse_pointer.h"
#include "UIFunction.h"
#include "AddonDef.h"
#include "MZoneTable.h"
#include "MGameTime.h"
#include "UserOption.h"
#include "UserInformation.h"
#include "SpriteIDDef.h"
#include "MGameStringTable.h"
//#include "MZoneInfo.h"
#include "MObjectSelector.h"
#include "MPortal.h"
#include "EffectSpriteTypeDef.h"
#include "MScreenEffectManager.h"
#include "MEffectSpriteTypeTable.h"
#include "Profiler.h"
//#include "MFileDef.h"
#include "Properties.h"
#include "MEventManager.h"
#include "DebugLog.h"
#include "FrameClock.h"
#include "MItemOptionTable.h"
#include "SkillDef.h"
#include "VS_UI_item.h"
#include "MTopViewDraw.inl"
#include "AdvancementAction.h"
#include "CreatureDivineGuidanceDraw.h"
#include "CreatureShadowDraw.h"
#include "CreatureEffectDraw.h"

#define	SHIFT_SNIPPING			2

#ifdef __METROTECH_TEST__
	extern bool g_bLight;
#endif

#ifdef OUTPUT_DEBUG
	int g_WipeValue = 0;
	int g_WipeStep = 0;
#endif


static int g_nRocketPosValue[4] = {0,2,4,2};
extern int defaultSkinColor;
extern int defaultCoatColor;
extern int defaultTrouserColor;

namespace {
	POINT pointTemp;
	RECT rect;

	void DrawInvisibleVampireSprite(CSpriteSurface* surface, POINT* point,
		CIndexSprite* sprite, MCreature* creature)
	{
		const int wipe = creature->GetInvisibleCount();
		if (wipe >= 64) return;
		WORD tint = creature->GetAttachEffectColor();
		if (creature->GetCreatureType() == CREATURETYPE_BAT && creature->GetBatColor() != 0xffff)
			tint = creature->GetBatColor();
		int color1 = tint < MAX_COLORSET ? tint : creature->GetBodyColor1();
		int color2 = tint < MAX_COLORSET ? tint : creature->GetBodyColor2();
		if (color1 == QUEST_ITEM_COLOR || color1 == UNIQUE_ITEM_COLOR)
			color1 = MItem::GetSpecialColorItemColorset(color1);
		if (color2 == QUEST_ITEM_COLOR || color2 == UNIQUE_ITEM_COLOR)
			color2 = MItem::GetSpecialColorItemColorset(color2);
		CIndexSprite::SetUsingColorSet(color1, color2);
		// Supply this creature's fade on every draw, including tinted bodies
		// and weapons; other effects also use the shared value.
		CSpriteSurface::s_Value1 = wipe;
		CIndexSprite::SetEffect(CIndexSprite::EFFECT_WIPE_OUT);
		surface->BltIndexSpriteEffect(point, sprite);
	}
};

BYTE GetCreatureActionCountMax( const MCreature* pCreature, int action );

// The packs and surface the creature drawing in gameview reads.
CreatureDrawPacks
MTopView::GetCreatureDrawPacks()
{
	CreatureDrawPacks packs;
	packs.pSurface = m_pSurface;
	packs.pCreatureFPK = &m_CreatureFPK;
	packs.pCreatureSPK = &m_CreatureSPK;
	packs.pAddonFPK = &m_AddonFPK;
	packs.pAddonSPK = &m_AddonSPK;
	packs.pOustersFPK = &m_OustersFPK;
	packs.pOustersSPK = &m_OustersSPK;
	packs.pAdvancementSlayerManFPK = &m_AdvancementSlayerManFPK;
	packs.pAdvancementSlayerManSPK = &m_AdvancementSlayerManSPK;
	packs.pAdvancementSlayerWomanFPK = &m_AdvancementSlayerWomanFPK;
	packs.pAdvancementSlayerWomanSPK = &m_AdvancementSlayerWomanSPK;
	packs.pAdvancementVampireManFPK = &m_AdvancementVampireManFPK;
	packs.pAdvancementVampireManSPK = &m_AdvancementVampireManSPK;
	packs.pAdvancementVampireWomanFPK = &m_AdvancementVampireWomanFPK;
	packs.pAdvancementVampireWomanSPK = &m_AdvancementVampireWomanSPK;
	packs.pAdvancementOustersFPK = &m_AdvancementOustersFPK;
	packs.pAdvancementOustersSPK = &m_AdvancementOustersSPK;
	packs.pCreatureShadowFPK = &m_CreatureShadowFPK;
	packs.pCreatureSSPK = &m_CreatureSSPK;
	packs.pAddonShadowFPK = &m_AddonShadowFPK;
	packs.pAddonSSPK = &m_AddonSSPK;
	packs.pOustersShadowFPK = &m_OustersShadowFPK;
	packs.pOustersSSPK = &m_OustersSSPK;
	packs.pAdvancementSlayerManShadowFPK = &m_AdvancementSlayerManShadowFPK;
	packs.pAdvancementSlayerManSSPK = &m_AdvancementSlayerManSSPK;
	packs.pAdvancementSlayerWomanShadowFPK = &m_AdvancementSlayerWomanShadowFPK;
	packs.pAdvancementSlayerWomanSSPK = &m_AdvancementSlayerWomanSSPK;
	packs.pAdvancementVampireManShadowFPK = &m_AdvancementVampireManShadowFPK;
	packs.pAdvancementVampireManSSPK = &m_AdvancementVampireManSSPK;
	packs.pAdvancementVampireWomanShadowFPK = &m_AdvancementVampireWomanShadowFPK;
	packs.pAdvancementVampireWomanSSPK = &m_AdvancementVampireWomanSSPK;
	packs.pAdvancementOustersShadowFPK = &m_AdvancementOustersShadowFPK;
	packs.pAdvancementOustersSSPK = &m_AdvancementOustersSSPK;
	packs.pEtcSPK = &m_EtcSPK;
	packs.selectedCreatureID = m_SelectCreatureID;
	packs.pOutlines = &m_SOM;
	return packs;
}

// A creature's shadow, gameview's (task 4.249), with the player's option.
void
MTopView::DrawCreatureShadow(POINT* pPoint, MCreature* pCreature)
{
	::DrawCreatureShadow(GetCreatureDrawPacks(), pPoint, pCreature, g_pUserOption->BlendingShadow);
}

bool IsEscapeDrawCreatureFunction( MCreature* pCreature )
{
	bool InGroundElemental = pCreature->IsOusters() && pCreature->IsPlayer() && g_pPlayer->IsOusters() == false
							&& pCreature->IsInGroundElemental();

	bool bGhost = pCreature->HasEffectStatus(EFFECTSTATUS_GHOST) || g_pEventManager->GetEventByFlag(EVENTFLAG_NOT_DRAW_CREATURE)
		|| (pCreature->IsGhost(1) && pCreature->IsGhost(2) && pCreature->IsGhost(4));

		
	// 2004, 6, 7 sobeit add start - for a pet, hide it when its owner is inside Darkness..
	bool bPet1 = false;
	if(pCreature->IsFakeCreature())
	{
		MCreature *OwnerCreature = g_pZone->GetCreature(((MFakeCreature*)pCreature)->GetOwnerID());
		if(NULL != OwnerCreature)
		{
			if(OwnerCreature->IsInDarkness())
				bPet1 = true;
		}
	}

	bool bBlind = false;
	if(
		(g_pPlayer->HasEffectStatus(EFFECTSTATUS_YELLOW_POISON_TO_CREATURE) || 
		g_pPlayer->HasEffectStatus(EFFECTSTATUS_FLARE )||
		g_pPlayer->HasEffectStatus(EFFECTSTATUS_BLINDNESS))
		&& pCreature != g_pPlayer 
		)
	{
		// Decide whether to draw according to distance.
		int sx,sy,ex,ey;

		sx = g_pPlayer->GetX() - 1;
		ex = g_pPlayer->GetX() + 1;

		sy = g_pPlayer->GetY() - 1;
		ey = g_pPlayer->GetY() + 1;

		if( pCreature->GetX() < sx || pCreature->GetX() > ex ||
			pCreature->GetY() < sy || pCreature->GetY() > ey )
			bBlind = true;
	}


	return InGroundElemental || bGhost || bPet1 || bBlind;
}

//----------------------------------------------------------------------
// Draw Creature : draw the character
//----------------------------------------------------------------------
// Draw pCreature at pPoint of pSurface.
//----------------------------------------------------------------
void	
MTopView::DrawCreature(POINT* pPoint, MCreature* pCreature)
{	
	// With EFFECTSTATUS_GHOST, others cannot see me and I cannot see others
	//DEBUG_ADD_FORMAT("[DrawCreature] Start %s %s %s", pCreature->IsGhost(1)?"true":"false", pCreature->IsGhost(2)?"true":"false", pCreature->IsGhost(4)?"true":"false");
	if( IsEscapeDrawCreatureFunction( pCreature ) )
		return;

	// 2004, 6, 7 sobeit add end
	__BEGIN_PROFILE("DrawCreature");

	int body, action, direction, frame, creature_type, frameCount;

	frameCount  = pCreature->GetCreatureFrameCount();

	for(int FrameIndex = 0; FrameIndex< frameCount ; FrameIndex++)
	{
		body		= pCreature->GetCreatureFrameID(FrameIndex);
		action		= pCreature->GetAction();
		direction	= pCreature->GetDirection();
		frame		= pCreature->GetFrame();//%m_CreatureFPK[body][action][direction].GetCount();
		creature_type = pCreature->GetCreatureType();

	//	if(action>ACTION_OUSTERS_MAGIC_ATTACK)
	//		int i = 0;
		//----------------------------------------------------------
		// Character under Charm..
		// Expressed with the being-drained motion..
		//----------------------------------------------------------
		if (pCreature->HasEffectStatus(EFFECTSTATUS_CHARM))
		{
			action = ACTION_DRAINED;
			frame = 6 + (frame & 0x07);		// In the drain motion the repeating frames start at 6.
		}
		if(pCreature->HasEffectStatus(EFFECTSTATUS_BIKE_CRASH))
			frame = 0;
		
		bool isSlayerCharacter	= pCreature->IsSlayerCharacter();
		//bool isOustersCharacter = pCreature->IsOusters();

		bool isOustersCharacter = (pCreature->IsOusters() && pCreature->GetCreatureType() != CREATURETYPE_OUSTERS_OPERATOR
								&&	creature_type != 717 && creature_type != 721 && creature_type != 723);
							
		
		// If hallucination is applied, the action frames etc. are changed... but not in wolf or bat form.
		// Do not change them. For now it has been commented out at the design team's request.
		if (pCreature->IsHallu()
	//		&& !pCreature->HasEffectStatus( EFFECTSTATUS_TRANSFORM_TO_BAT )
	//		&& !pCreature->HasEffectStatus( EFFECTSTATUS_TRANSFORM_TO_WOLF )
			&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
	#ifdef __METROTECH_TEST__
			&& !g_bLight
	#endif
			)
		{
			isSlayerCharacter = false;
			isOustersCharacter = false;

			body	= pCreature->GetHalluCreatureFrameID();
			action	= pCreature->GetHalluAction();
			frame	= pCreature->GetHalluFrame();
			creature_type = pCreature->GetHalluCreatureType();
		}

		bool bGildreDead = false;
		bool bSlayerPet_ShowTurret = false;
		GENERATE_OPTION  OutLineOption = GENERATE_ALL;
		// 2004, 7, 23 sobeit modify start

		// 2004.12.9 created exception processing function by sonee
		std::map< DWORD, DrawCreatureExceptionProc >::iterator itr = m_ExceptionProcMap.find( creature_type );

		if( m_ExceptionProcMap.end() != itr )
			itr->second( pCreature, action, frame, direction, g_CurrentFrame );

		switch(creature_type)
		{		
		case 717:
		case 721:
			if(pCreature->IsDead())
			{
				if(pCreature->GetActionCount()>=pCreature->GetActionCountMax())
					bGildreDead = true;
			}
			break;
		case 702:
		case 703:
		case 704:
			if( action == ACTION_STAND || action == ACTION_MOVE )
				bSlayerPet_ShowTurret = true;
			break;
		case 726: // 4 kinds of castle gate
		case 727:
		case 728:
		case 729:
			{
				MCreature* TempCreature = g_pZone->GetCreature(m_SelectCreatureID);
				if(TempCreature != NULL)
				{
					if(	TempCreature->GetCreatureType() == 726 ||
						TempCreature->GetCreatureType() == 727 ||
						TempCreature->GetCreatureType() == 728 ||
						TempCreature->GetCreatureType() == 729 )
					{
						if(creature_type == 726)
							OutLineOption = GENERATE_EXCEPT_RIGHT;
						else if(creature_type == 727 || creature_type == 728 )
							OutLineOption = GENERATE_EXCEPT_SIDE;
						else if(creature_type == 729)
							OutLineOption = GENERATE_EXCEPT_LEFT;
					}
				}
			}
			break;
		case 753 :
			if(pCreature->IsDead())
			{
				action = ACTION_DIE;
				if(pCreature->GetActionCount()>=pCreature->GetActionCountMax()) return;
			}
			break;

		case 767:// rocket launcher
			{
				if(direction == 0 || direction == 4)
				{
					pPoint->y -= g_nRocketPosValue[frame];
//					pPoint->y -= sin((double)pCreature->GetMoveCount()*3.141592/(double)pCreature->GetMoveCountMax())*5;
				}
				else 
				{
					pPoint->x -= g_nRocketPosValue[frame];
//					pPoint->x -= sin((double)pCreature->GetMoveCount()*3.141592/(double)pCreature->GetMoveCountMax())*5;
				}
			}
			frame = 0;
			action = ACTION_MOVE;
		
			break;
		case 769:
			if(pCreature->IsDead())
				return;
			break;

		}
		// 2004, 7, 23 sobeit modify end
		
		// 2004, 11, 22, sobeit add start - set afire related
		if(pCreature->GetID() != g_pPlayer->GetID())
		{
			if(pCreature->GetSpecialActionInfo() == SKILL_SET_AFIRE)
			{
				if(action == ACTION_ATTACK || action == ACTION_VAMPIRE_ATTACK_SLOW || action == ACTION_VAMPIRE_ATTACK_FAST)
				{
					action = ACTION_ATTACK;
					frame = 3+((frame)%4);
				}
			}
		}
		else
		{
			if(pCreature->GetUsedActionInfo() == SKILL_SET_AFIRE)
			{
				if(action == ACTION_ATTACK || action == ACTION_VAMPIRE_ATTACK_SLOW || action == ACTION_VAMPIRE_ATTACK_FAST)
				{
					action = ACTION_ATTACK;
					frame = 3+((frame)%4);
				}
			}
		}
		// 2004, 11, 22, sobeit add end - set afire related
		// Relics always go below, Christmas tree


		if(pCreature->IsFakeDie())
			frame = 0;
	
		int	creatureID			= pCreature->GetID();
		bool bPlayerVampire = 
				(*g_pCreatureSpriteTable)[(*g_pCreatureTable)[creature_type].SpriteTypes[0]].IsPlayerVampireOnlySprite()
	//			&& (pCreature==g_pPlayer || pCreature->HasEffectStatus(EFFECTSTATUS_COMA))
				&& (pCreature->GetActionCount()>=pCreature->GetActionCountMax()	)
				&& !pCreature->IsExistAttachEffect()
				&& pCreature->IsDead();

		bool bTeenVersion = 
				(
					(g_pUserInformation->GoreLevel == false
					&& !(
					(creature_type >= 526 && creature_type <= 549) || 
					(creature_type >= 371 && creature_type <= 376) || 
					(creature_type >= 560 && creature_type <= 563) || 
					creature_type == 482 || 
					creature_type == 650 || 
					creature_type == 670 || 
					creature_type == 672 ||
					creature_type == 673 ||
					creature_type == 730 || // waypoint 1
					creature_type == 731 || // waypoint 2
					creature_type == 732 || // waypoint 3
					creature_type == 636 // fish shop man

					))	// relic // Christmas tree
					|| (creature_type >= 377 && creature_type <= 386)	// Dark Guardians are shown as dust
					|| creature_type == 480 
				)
				&&
				(
					pCreature->GetActionCount()>=pCreature->GetActionCountMax()/2
					&& pCreature->IsDead()
				)
				&&
				(
					creature_type < 431		// Bathory
					|| creature_type > 436	// Tepes
				);

		//---------------------------------------------------------
		// A PC Vampire is shown as 'dust' when dead.
		//
		// because a player does not turn into an item when dead..
		// In the Teen version every corpse is shown as 'dust'.
		//---------------------------------------------------------	
		if (bPlayerVampire || bTeenVersion || bGildreDead )
		{
			{
				CSprite* pSprite;
				
				if(pCreature->IsSlayer())
				{
					pSprite = &m_EtcSPK[ SPRITEID_SLAYER_CROSS ];
					// coordinate correction
					pointTemp.x = pPoint->x - 5+20;
					pointTemp.y = pPoint->y - 30;				
				}
				//else if((*g_pCreatureSpriteTable)[(*g_pCreatureTable)[creature_type].SpriteType].IsPlayerVampireOnlySprite())
				else if((*g_pCreatureSpriteTable)[(*g_pCreatureTable)[creature_type].SpriteTypes[0]].IsPlayerVampireOnlySprite())
				{
					pSprite = &m_EtcSPK[ SPRITEID_VAMPIRE_DUST ];
					// coordinate correction
					pointTemp.x = pPoint->x - 5;
					pointTemp.y = pPoint->y;				
				}
				else
				{
					pSprite = &m_EtcSPK[ SPRITEID_MONSTER_DUST ];
					// coordinate correction
					pointTemp.x = pPoint->x - 5;
					pointTemp.y = pPoint->y;				
				}


				// If it is the selected one
				if (m_SelectItemID == static_cast<TYPE_OBJECTID>(creatureID)
					|| m_SelectCreatureID == static_cast<TYPE_OBJECTID>(creatureID))
				{
					m_SOMOutlineColor = m_ColorOutlineItem;
					
					m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );
					
					m_SOM.Generate();

					m_pSurface->BltSpriteOutline( &m_SOM, m_SOMOutlineColor );
					
					//---------------------------------------- 	
					// Set the coordinates for drawing the name
					//---------------------------------------- 	
					const int FontHeight = g_pClientConfig->FONT_HEIGHT;
					const int FontHeight2 = FontHeight << 1;
					
					pointTemp.x = pPoint->x;			
					if (pointTemp.x<0) pointTemp.x=0;
					
					//---------------------------------------- 	
					// Also calculate where to draw the Level Name
					//---------------------------------------- 	
					if (pCreature->HasLevelName())
					{
						pointTemp.y = pPoint->y - FontHeight2;
						
						if (pointTemp.y < FontHeight2)
						{
							pointTemp.y = FontHeight2;
						}
					}
					//---------------------------------------- 	
					// When drawing just the name
					//---------------------------------------- 	
					else
					{
						pointTemp.y = pPoint->y - FontHeight;
						
						if (pointTemp.y < FontHeight) 
						{
							pointTemp.y = FontHeight;
						}
					}
					
					m_pointChatString	= pointTemp;
					m_pSelectedCreature = pCreature;
				}
				else
				{
					m_pSurface->BltSprite(&pointTemp, pSprite);	
				}			

				//------------------------------------------------
				// For a dead character the selection rectangle
				// is limited to the size of one Tile.
				//------------------------------------------------
				RECT rect;
				rect.left	= pPoint->x;
				rect.top	= pPoint->y;
				rect.right	= pPoint->x + TILE_X;
				rect.bottom = pPoint->y + TILE_Y;
				if(FrameIndex ==0 )	
					pCreature->SetScreenRect( &rect );
				
	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD("EndDrawCreature-S");
	#endif
				
				
				if(pCreature->GetActionCount()>=pCreature->GetActionCountMax()/2)
				{
					if (pCreature->IsExistAttachEffect())
					{	
	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
						DEBUG_ADD("DrawAttachEffect");
	#endif
						
						pointTemp = *pPoint;
						//pointTemp.x += pCreature->GetSX();
						//pointTemp.y += pCreature->GetSY();
						
						DrawAttachEffect(&pointTemp, 
							pCreature->GetAttachEffectIterator(), 
							pCreature->GetAttachEffectSize(),
							pCreature,
							1);	// foreground output
						
	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
						DEBUG_ADD("DAE ok");
	#endif
					}
				}
				m_pSurface->Unlock();
				const int firstY = DrawChatString(&pointTemp, pCreature, pCreature->GetChatColor());
				pointTemp.y = firstY;
				DrawCreatureHPModify(&pointTemp, pCreature);
				m_pSurface->Lock();	
				return;
			}
		}

		//------------------------------------------------
		// A Slayer cannot see inside Darkness.
		//------------------------------------------------
		if (
			!( (creature_type >= 526 && creature_type <= 549) || 
			   creature_type == 670 || 
			   creature_type == 672 ||
			   creature_type == 673 ||
			   (creature_type >= 371 && creature_type <= 376) || 
			   (creature_type >= 560 && creature_type <= 563)
			 ) &&
			pCreature->IsInDarkness() && !pCreature->IsNPC() && !g_pPlayer->IsVampire() && g_pPlayer!=pCreature
			&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_LIGHTNESS )
			&& g_pZone->GetID() != 3001
			&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
	#ifdef __METROTECH_TEST__
			&& !g_bLight
	#endif
			)
		{
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD("EndDrawCreature-D");
			#endif

			return;
		}

		//------------------------------------------------
		//
		// Those buried underground use a fixed picture..
		//
		//------------------------------------------------
		// 2004, 04, 24 sobeit modify - the Gilles Lair ghost is an underground creature but does not use a fixed picture..^^;
		if (pCreature->IsUndergroundCreature() && pCreature->GetCreatureType() != CREATURETYPE_GHOST)
		{
			DrawUndergroundCreature(pPoint, pCreature );
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD( "End DrawCreature" );
			#endif		

			return;
		}

		//----------------------------------------------------------
		// When there is no head
		//----------------------------------------------------------
		if (!pCreature->HasHead()
			&& pCreature->GetActionCount()==pCreature->GetActionCountMax()
			&& g_pUserInformation->GoreLevel == true)
		{
			if (action==ACTION_DIE)
			{
				// Actually.. it is the last frame + 1.
				//frame	= (*g_pCreatureTable)[creature_type].GetActionCount( action );
				frame	= GetCreatureActionCountMax( pCreature, action);
			}
		}

		// In the Teen version the death animation is not played
		// At first only the motion up to before the last frame was set to 0, but when drawn as ash up there it returns early, so that was pointless
		if(g_pUserInformation->GoreLevel == false && action == ACTION_DIE
			&&	(
					creature_type < 431		// Bathory
					|| creature_type > 436	// Tepes
				)
			)
		{
			frame = 0;
		}
		
		// Subtract the height at which the Creature exists.
		pPoint->y -= pCreature->GetZ();

		if(!pCreature->IsNPC() &&  pCreature->HasEffectStatus(EFFECTSTATUS_DIVINE_GUIDANCE))
		{	
			DrawDivineGuidanceCharacter( GetCreatureDrawPacks(), pPoint, pCreature, direction, body, isSlayerCharacter, isOustersCharacter );
		}

		//----------------------------------------
		// Draw Effects attached to the ground
		//----------------------------------------
		if (pCreature->IsExistGroundAttachEffect())
		{				
			pointTemp = *pPoint;
			//pointTemp.x += pCreature->GetSX();
			//pointTemp.y += pCreature->GetSY();

			DrawAttachEffect(&pointTemp, pCreature->GetGroundAttachEffectIterator(), pCreature->GetGroundAttachEffectSize(), pCreature);
		}

		//---------------------------------------------------------------
		//
		//                  Draw Background AttachEffectNode
		//
		//---------------------------------------------------------------
		if (pCreature->IsExistAttachEffect())
		{				
	//		DEBUG_ADD("BackAttachEffect");

			pointTemp = *pPoint;
			//pointTemp.x += pCreature->GetSX();
			//pointTemp.y += pCreature->GetSY();

			DrawAttachEffect(&pointTemp, 
								pCreature->GetAttachEffectIterator(), 
								pCreature->GetAttachEffectSize(),
								pCreature,
								2);	// background output

	//		DEBUG_ADD("BAE ok");
		}

		//---------------------------------------------------------------
		//
		//                  Selected with the Mouse --> outline
		//
		//---------------------------------------------------------------

		//------------------------------------------------
		// Creature the Mouse is pointing at
		//------------------------------------------------
		
		if (m_SelectCreatureID == static_cast<TYPE_OBJECTID>(creatureID) || OutLineOption)
		{
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD("Sel Creature");
			#endif

			// Add to SpriteOutlineManager
			if ((pCreature->IsNPC()							// if it is an NPC
				// Not allowed for Bathory. Hardcoded - -;;
				&& !(creature_type==217))
				|| (IsRequestMode()								// if in trade mode
					// Only players are allowed.
					//pCreature->GetCreatureType()<=CREATURETYPE_VAMPIRE_FEMALE
				//	&& (*g_pCreatureSpriteTable)[(*g_pCreatureTable)[creature_type].SpriteType].IsPlayerOnlySprite()
				&& (*g_pCreatureSpriteTable)[(*g_pCreatureTable)[creature_type].SpriteTypes[0]].IsPlayerOnlySprite())
				|| (!IsRequestMode()							// if it is a creature being chased in order to trade
					&& g_pPlayer->IsRequestMode()
					&& g_pPlayer->IsTraceCreature()
					&& g_pPlayer->GetTraceID()==static_cast<TYPE_OBJECTID>(creatureID)))
			{
				m_SOMOutlineColor = m_ColorOutlineNPC;
			}
			else if (g_pObjectSelector->CanAttack( pCreature )
				|| creature_type==217)		// if it is Bathory. Hardcoded - -;;
			{
				m_SOMOutlineColor = m_ColorOutlineAttackPossible;
			}
			else
			{
				m_SOMOutlineColor = m_ColorOutlineAttackImpossible;
			}

			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD("drawBody");
			#endif

			//----------------------------------------
			//
			//         Drawn when there is a body
			//
			//----------------------------------------
			if (body!=FRAMEID_NULL)
			{
				// body needs fixing
				if( pCreature->IsAdvancementClass()
					)		//by viva Selected Vampire
					DrawSelectedAdvancementVampireCreature( pPoint, pCreature, action, direction, frame, 0, FrameIndex );
				else
					DrawSelectedVampireCreature( pPoint, pCreature, action, direction, frame, body, FrameIndex );
			}
			else
			{
				//----------------------------------------------------
				// Clear the character's selection rectangle area.
				//----------------------------------------------------
				pCreature->ClearScreenRect();
			}

			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD("draw clothes");
			#endif

			//----------------------------------------
			//
			//            Draw the outfit
			//
			//----------------------------------------
			// Among all kinds of wearable clothes
			// check which are being worn and draw them.
			if (isSlayerCharacter)
			{		
				// 2004, 9, 15, sobeit add start  - install turret
				if(pCreature->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
					DrawInstallTurret( GetCreatureDrawPacks(), pPoint, pCreature, action, direction, frame, FrameIndex );
				else
				{
					if( pCreature->IsAdvancementClass() )
						DrawSelectedAdvancementSlayerCreature(pPoint, pCreature, action, direction, frame );
					else
						DrawSelectedSlayerCreature( pPoint, pCreature, action, direction, frame );
				}
			}
			else if(isOustersCharacter)
			{
				if( pCreature->IsAdvancementClass() )
					DrawSelectedAdvancementOustersCreature(pPoint, pCreature, action, direction, frame );
				else
					DrawSelectedOustersCreature( pPoint, pCreature, action, direction, frame );
			}
			
			// A Slayer pet needs its turret drawn -ㅅ-;;;; hard hard.. aah -_-/~
			if( bSlayerPet_ShowTurret )
			{
				DrawCentauroTurret( GetCreatureDrawPacks(), pPoint, pCreature, action, direction, frame , body);
			}

			// FIX: Draw the actual sprites before drawing the outline
			// BltSpriteOutline only draws the outline, not the sprites themselves
			// So we need to draw the sprites first
			const std::vector<OutlineEntry>& outlines = m_SOM.GetOutlines();
			for (const auto& entry : outlines) {
				if (entry.pIndexSprite) {
					POINT spritePoint = { entry.x, entry.y };
					m_pSurface->BltIndexSprite(&spritePoint, entry.pIndexSprite);
				} else if (entry.pSprite) {
					POINT spritePoint = { entry.x, entry.y };
					m_pSurface->BltSprite(&spritePoint, entry.pSprite);
				}
			}

			m_SOM.Generate(OutLineOption);
			
	//			// 2004, 8, 18 sobeit add start - added somehow because of siege-war gate targeting..
	//		enum GENERATE_OPTION{
	//			GENERATE_ALL = 0,
	//			GENERATE_EXCEPT_LEFT,	// exclude the left outline
	//			GENERATE_EXCEPT_RIGHT,  // exclude the right outline
	//			GENERATE_EXCEPT_SIDE,	// exclude the outlines on both sides
	//			// -_- do I need to make top and bottom too..-_-;
	//		};
	//		// 2004, 8, 18 sobeit add start
			//if (true || 
			//	DarkBits==0 || 
			//	g_pPlayer->GetID()==pCreature->GetID())
			//{	
			
				if (pCreature->IsInvisible() || pCreature->IsFakeDie()
	//				#ifdef OUTPUT_DEBUG
	//					||	g_pSDLInput->KeyDown(DIK_RCONTROL)
	//				#endif
					)
				{
					// snipping, the Slayer's invisible
					if (pCreature->HasEffectStatus(EFFECTSTATUS_SNIPPING_MODE))
					{
						m_pSurface->BltSpriteOutlineDarkness( &m_SOM,  m_SOMOutlineColor, SHIFT_SNIPPING );
					}
					m_pSurface->BltSpriteOutlineOnly( &m_SOM,  m_SOMOutlineColor );
				}
				else
				{
					// Draw darkened
					if (pCreature->IsFade())
					{
						m_pSurface->BltSpriteOutlineDarkness( &m_SOM,  m_SOMOutlineColor, 1 );
					}
					else
					{
						m_pSurface->BltSpriteOutline( &m_SOM,  m_SOMOutlineColor );
					}
				}
				//m_pSurface->BltSpriteOutlineOnly( &m_SOM,  0xF800 );
			//}
			//else
			//{
			//	m_pSurface->BltSpriteOutlineDarkness( &m_SOM, m_SOMOutlineColor, DarkBits );
			//}		
				m_SOM.Clear();
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD("drawSelOK");
			#endif
		}
		//------------------------------------------------
		//
		//       Normal Creature output
		//
		//------------------------------------------------
		else
		{
			
			//------------------------------------------------
			//
			//         Body output
			//
			//------------------------------------------------
			if (body!=FRAMEID_NULL)
			{
				bool bBat = pCreature->GetCreatureType() == CREATURETYPE_BAT;
				bool bWolf = pCreature->GetCreatureType() == CREATURETYPE_WOLF;
				bool bWerWolf = pCreature->GetCreatureType() == CREATURETYPE_WER_WOLF;

				if( pCreature->IsAdvancementClass() && !bBat && !bWolf && !bWerWolf
					) //by viva
					DrawAdvancementClassVampireCharacter( pPoint, pCreature, action, direction, frame, body, FrameIndex );
				else
					DrawVampireCharacter( pPoint, pCreature, action, direction, frame, body, FrameIndex );			
				
			}
			else
			{
				//----------------------------------------------------
				// Clear the character's selection rectangle area.
				//----------------------------------------------------
				pCreature->ClearScreenRect();		
			}
			
			//----------------------------------------
			//
			//            Draw the outfit
			//
			//----------------------------------------
			// Among all kinds of wearable clothes
			// check which are being worn and draw them.
			//WORD colorSet = pCreature->GetAttachEffectColor();
							
			//if (pCreature->IsWear())
			if (isSlayerCharacter)
			{
				if( pCreature->IsAdvancementClass() )   //by viva
					DrawAdvancementClassSlayerCharacter( pPoint, pCreature, action, direction, frame, FrameIndex );
				else
					DrawSlayerCharacter( pPoint, pCreature, action, direction, frame, FrameIndex );
			}
			else if(isOustersCharacter)
			{
				if( pCreature->IsAdvancementClass() )
					DrawAdvancementClassOustersCharacter( pPoint, pCreature, action, direction, frame );
				else
					DrawOustersCharacter( pPoint, pCreature, action, direction, frame );
			}

			// A Slayer pet needs its turret drawn -ㅅ-;;;; hard hard.. aah -_-/~
			if( bSlayerPet_ShowTurret )
			{
				DrawCentauroTurret( GetCreatureDrawPacks(), pPoint, pCreature, action, direction, frame , body);
			}
		}


		//---------------------------------------------------------------
		//
		//                  AttachEffectNode output
		//
		//---------------------------------------------------------------
		if (pCreature->IsExistAttachEffect())
		{	
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD("DrawAttachEffect");
			#endif

			pointTemp = *pPoint;
			//pointTemp.x += pCreature->GetSX();
			//pointTemp.y += pCreature->GetSY();

			DrawAttachEffect(&pointTemp, 
								pCreature->GetAttachEffectIterator(), 
								pCreature->GetAttachEffectSize(),
								pCreature,
								1);	// foreground output

			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD("DAE ok");
			#endif
		}



		//pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
		//pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
		bool bPlayerParty = (pCreature->IsPlayerParty() 
								&& pCreature!=g_pPlayer
								// Under hallucination you cannot recognize party members.
								&& !g_pPlayer->HasEffectStatus(EFFECTSTATUS_HALLUCINATION));	

		//------------------------------------------------
		// Chat String output
		//------------------------------------------------
		// If the character is selected with the Mouse..
		if (m_SelectCreatureID == static_cast<TYPE_OBJECTID>(creatureID))
		{
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD("SelectedCreature");
			#endif

			//----------------------------------------
			// Set the coordinates for drawing the name
			//----------------------------------------
			const int FontHeight = g_pClientConfig->FONT_HEIGHT;
			const int FontHeight2 = FontHeight << 1;

			pointTemp.x = pPoint->x;
			if (pointTemp.x<0) pointTemp.x=0;

			pointTemp.y = pPoint->y - pCreature->GetHeight();

			//----------------------------------------
			// Also calculate where to draw the Level Name
			//----------------------------------------
			if (pCreature->HasLevelName())
			{
				if (pointTemp.y < FontHeight2)
				{
					pointTemp.y = FontHeight2;
				}
			}
			//----------------------------------------
			// When drawing just the name
			//----------------------------------------
			else
			{
				if (pointTemp.y < FontHeight)
				{
					pointTemp.y = FontHeight;
				}
			}

			m_pointChatString	= pointTemp;
			m_pSelectedCreature = pCreature;

		}
		else
		{		
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD("noSelCre");
			#endif

			pointTemp.x = pPoint->x;
			pointTemp.y = pPoint->y - pCreature->GetHeight();
				
			//------------------------------------------------
			// For a party member, draw the HP.
			//------------------------------------------------
			if (bPlayerParty)
			{			
				DrawPartyHP( &pointTemp, pCreature );

				pointTemp.y = pPoint->y - pCreature->GetHeight() - m_EtcSPK[SPRITEID_PARTY_HP].GetHeight();
			}
			else
			{
				pointTemp.y = pPoint->y - pCreature->GetHeight();
			}

			pointTemp.x = pPoint->x;			

			//
			// This puts quite a load... tsk~~
			//
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD("DrawChat");
			#endif

			m_pSurface->Unlock();
			const int firstY = DrawChatString(&pointTemp, pCreature, pCreature->GetChatColor());
			pointTemp.y = firstY;
			DrawCreatureHPModify(&pointTemp, pCreature);
			m_pSurface->Lock();	

			// 2004, 8, 18, sobeit add start - siege-war guild mark display
			DrawGuildMarkInSiegeWar(pCreature,pointTemp.y);
			// 2004, 8, 18, sobeit add end
		}	

		//------------------------------------------------
		// Show the party member's sight.
		//------------------------------------------------
		if (bPlayerParty)
		{
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD("partySight");
			#endif

			int pX = pCreature->GetPixelX() - m_FirstZonePixel.x + 24;
			int pY = pCreature->GetPixelY() - m_FirstZonePixel.y - TILE_Y;

			//int addLight = (pCreature->GetCreatureType()==CREATURETYPE_BAT ? 3 : 0);
	//		int addLight = (pCreature->IsFlyingCreature() ? 3 : 0);
			// Assume the same sight as the player. - -;
			int creatureLight = g_pPlayer->GetLightSight();// + addLight);

			// SDL2: Unified 2D lighting path for all platforms
			AddLightFilter2D( pX,
								pY - pCreature->GetZ(),
								creatureLight,
								false,	// screenPixel coordinates
								true);	// light that must always be drawn

			// motorcycle light
			ADD_MOTORCYCLE_LIGHT_XY_2D( pCreature, pX, pY, true );
		}
		//------------------------------------------------
		// For non-party members...
		//------------------------------------------------
		else
		{
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD("noPParty?");
			#endif

			// motorcycle light
			ADD_MOTORCYCLE_LIGHT_2D( pCreature, false );
		}

		if(gpC_item != NULL && pCreature != NULL && pCreature->GetHeadSkin() != 0)
		{
			int spriteID = pCreature->GetHeadSkin();
			int pX = pCreature->GetPixelX() - m_FirstZonePixel.x + 24-gpC_item->GetWidth(spriteID)/2;
			int pY = pCreature->GetPixelY() - m_FirstZonePixel.y - TILE_Y -18-gpC_item->GetHeight(spriteID);
			CIndexSprite::SetUsingColorSet(377, 0);
			gpC_item->BltLocked(pX, pY, spriteID);
		}
	}

	__END_PROFILE("DrawCreature")

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "End DrawCreature" );
	#endif
//	DEBUG_ADD("[DrawCreature] End");

}

void	MTopView::DrawSelectedSlayerCreature( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
	// Draw the add-ons matching the Creature's Action.
	//action = pCreature->GetAction();	
	WORD clothes;
	BYTE clothesType;
	
	for (int i=0; i<ADDON_MAX; i++)
	{
		// Depending on the Creature's current direction...
		// the order in which clothes are drawn may differ.
		clothesType = MCreatureWear::s_AddonOrder[pCreature->GetDirection()][i];
		
		// If it is wearing the i-th kind of clothes, draw it.
		const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(clothesType);
		
		if (addonInfo.bAddon)
		{
			clothes = addonInfo.FrameID;
			
			FRAME_ARRAY &FA = m_AddonFPK[clothes][action][direction];
			
			// If the motion exists
			if (FA.GetSize() > frame)
			{
				CFrame &Frame = FA[frame];					
				int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
				int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
				int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
				
				pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
				pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
				
				CIndexSprite* pSprite = &m_AddonSPK[ sprite ];
				
				// If the outfit Sprite has not been initialized
				//						if (pSprite->IsNotInit())
				//						{
				//							LoadFromFileAddonSPK( clothes, action );
				//							//continue;
				//						}
				
				//---------------------------------------- 
				// Set the character selection rectangle area
				//---------------------------------------- 	
				rect.left	= pointTemp.x;
				rect.top	= pointTemp.y;
				rect.right	= rect.left + pSprite->GetWidth();
				rect.bottom = rect.top + pSprite->GetHeight();
				pCreature->AddScreenRect( &rect );
				
				// Add to SpriteOutlineManager
				//CIndexSprite::SetUsingColorSet((pCreature->GetID()+3*(clothesType,1),7)%MAX_COLORSET, ((pCreature->GetID()*pCreature->GetID(),7*(clothesType+2)))%MAX_COLORSET);
				//CIndexSprite::SetUsingColorSet((c1+3*(clothesType+1)+7)%MAX_COLORSET, ((c2*c2+7*(clothesType+2)))%MAX_COLORSET);
				//---------------------------------------- 
				// When a part of the character changes color
				//---------------------------------------- 
				if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
				{
					m_SOM.Add( pointTemp.x, pointTemp.y, pSprite, addonInfo.EffectColorSet );
				}
				//---------------------------------------- 
				// In the original colors..
				//---------------------------------------- 
				else
				{
					int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
					if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
					{
						if(colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )
							colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
						
						if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
							colorSet1 = colorSet2;
					}							
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );
				}
			}
		}
	}
}



void	MTopView::DrawSelectedOustersCreature( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	//  There is a chakram
	MCreatureWear *pCreatureWear = (MCreatureWear *)pCreature;
	
	const MCreatureWear::ADDON_INFO& addonInfoChakram = pCreatureWear->GetAddonInfo(ADDON_RIGHTHAND);
	bool bChakram = addonInfoChakram.bAddon && addonInfoChakram.ItemClass == ITEM_CLASS_OUSTERS_CHAKRAM;
	
	int tempAction = action;
	if(bChakram == false)
	{
		switch(tempAction)
		{
		case ACTION_STAND:
			tempAction = ACTION_OUSTERS_STAND;
			break;
			
		case ACTION_MOVE:
			tempAction = ACTION_OUSTERS_MOVE;
			break;
		}
	}
	
	// Draw the body, then draw the chakram
	const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(ADDON_COAT);
	const MCreatureWear::ADDON_INFO& bootsAddonInfo = pCreatureWear->GetAddonInfo(ADDON_TROUSER);
	
	if (addonInfo.bAddon && !pCreatureWear->IsGhost(1))
	{
		int clothes = addonInfo.FrameID;
		
		
		FRAME_ARRAY &FA = m_OustersFPK[clothes][tempAction][direction];
		
		// If the motion exists
		if (FA.GetSize() > frame)
		{
			CFrame &Frame = FA[frame];					
			int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
			int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
			int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
			
			CIndexSprite* pSprite = &m_OustersSPK[ sprite ];					
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			//---------------------------------------- 
			// Set the character selection rectangle area
			//---------------------------------------- 	
			rect.left	= pointTemp.x;
			rect.top	= pointTemp.y;
			rect.right	= rect.left + pSprite->GetWidth();
			rect.bottom = rect.top + pSprite->GetHeight();
			pCreature->AddScreenRect( &rect );
			
			//---------------------------------------- 
			// When a part of the character changes color
			//---------------------------------------- 
			if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
			{
				m_SOM.Add( pointTemp.x, pointTemp.y, pSprite, addonInfo.EffectColorSet );
			}
			//---------------------------------------- 
			// Normal character output
			//---------------------------------------- 
			else
			{
				CIndexSprite::SetUsingColorSetOnly( 0, pCreature->GetBodyColor1() );	// hair color
				
				int coatColor = addonInfo.ColorSet2, Colorset;
				if( coatColor == QUEST_ITEM_COLOR || coatColor == UNIQUE_ITEM_COLOR )
					coatColor = MItem::GetSpecialColorItemColorset( coatColor );						
				CIndexSprite::SetUsingColorSetOnly( 1, coatColor );	// clothes color
				if(bChakram == false)	// if wearing a wristlet
				{
					if(addonInfoChakram.bAddon)
					{
						Colorset = addonInfoChakram.ColorSet2;
						if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
							Colorset = MItem::GetSpecialColorItemColorset( Colorset );
						CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arm
					}
					else
					{
						Colorset = defaultCoatColor;								
						if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
							Colorset = MItem::GetSpecialColorItemColorset( Colorset );
						
						CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arm
					}
				}
				else
				{
					Colorset = defaultCoatColor;
					if(addonInfoChakram.bAddon)
						Colorset = addonInfoChakram.ColorSet2;
					if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arm
				}
				
				if(bootsAddonInfo.bAddon)
				{
					Colorset = bootsAddonInfo.ColorSet2;
					if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					CIndexSprite::SetUsingColorSetOnly( 3, Colorset );	// boots color
				}
				else
				{
					Colorset = defaultCoatColor;
					
					if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					CIndexSprite::SetUsingColorSetOnly( 3, Colorset );	// arm
				}
				
				m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );
			}						
			
		}
	}
	
	if (bChakram && !pCreatureWear->IsGhost(2))
	{
		int clothes = addonInfoChakram.FrameID;
		
		FRAME_ARRAY &FA = m_OustersFPK[clothes][tempAction][direction];
		
		// If the motion exists
		if (FA.GetSize() > frame)
		{
			CFrame &Frame = FA[frame];					
			int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
			int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
			int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
			
			CIndexSprite* pSprite = &m_OustersSPK[ sprite ];					
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			//---------------------------------------- 
			// When a part of the character changes color
			//---------------------------------------- 
			if (addonInfoChakram.bEffectColor)	//colorSet < MAX_COLORSET)
			{
				m_SOM.Add( pointTemp.x, pointTemp.y, pSprite, addonInfo.EffectColorSet );
			}
			//---------------------------------------- 
			// Normal character output
			//---------------------------------------- 
			else
			{
				int colorset1, colorset2;
				
				colorset1 = addonInfoChakram.ColorSet1;
				colorset2 = addonInfoChakram.ColorSet2;
				if( colorset1 == QUEST_ITEM_COLOR || colorset1 == UNIQUE_ITEM_COLOR )
					colorset1 = MItem::GetSpecialColorItemColorset( colorset1 );
				if( colorset2 == QUEST_ITEM_COLOR || colorset2 == UNIQUE_ITEM_COLOR )
					colorset2 = MItem::GetSpecialColorItemColorset( colorset2 );
				
				
				CIndexSprite::SetUsingColorSet(colorset1, colorset2);
				m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );
			}						
			
		}
	}
}

void	MTopView::DrawVampireCharacter( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame, int body, int frameindex )
{
	//------------------------------------------------
	// Coffin output information
	//------------------------------------------------
	int casketValue = pCreature->GetCasketCount();
	int creature_type = pCreature->GetCreatureType();  //add by viva
	//int creature_type = 814;
	bool bInCasket = pCreature->IsInCasket();
	bool bCasketOnly = (bInCasket && casketValue==0);
	
	//----------------------------------------------------
	// When only the coffin is drawn completely, the body is not drawn.
	//----------------------------------------------------
	if (!bCasketOnly)
	{
		FRAME_ARRAY& FA = m_CreatureFPK[body][action][direction];
	//	try{	
		if (FA.GetSize() > frame)
		{
			CFrame& Frame =	FA[frame];
			int sprite = Frame.GetSpriteID(),	//m_CreatureFPK[body][action][direction][frame].GetSpriteID(),
				cx		= Frame.GetCX(),	//m_CreatureFPK[body][action][direction][frame].GetCX(),
				cy		= Frame.GetCY();	//m_CreatureFPK[body][action][direction][frame].GetCY();
			
			// coordinate correction
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			CIndexSprite* pSprite = &m_CreatureSPK[ sprite ];
			
			//---------------------------------------- 
			// Set the character selection rectangle area
			//---------------------------------------- 	
			rect.left	= pointTemp.x;
			rect.top	= pointTemp.y;
			rect.right	= rect.left + pSprite->GetWidth();
			rect.bottom = rect.top + pSprite->GetHeight();
			if(frameindex==0 )
				pCreature->SetScreenRect( &rect );
			
			// Mark the Character the Player is chasing
			//if (g_pPlayer->GetTraceCreatureID()==pCreature->GetID())			
			
			//CIndexSprite::SetUsingColorSet(c1%MAX_COLORSET, c2%MAX_COLORSET);
			
			//-----------------------------------------------------------
			// CutHeight
			//-----------------------------------------------------------
			bool bCutHeight = pCreature->IsCutHeight();
			if (bCutHeight)
			{
				// SDL2: Clipping is automatic in SDL backend
				// previousClipBottom = m_pSurface->GetClipBottom();
				// m_pSurface->SetClipBottom( rect.bottom - 24 );
				
				pointTemp.y += pCreature->GetCutHeightCount() - 24;
			}
			
			//-----------------------------------------------------------
			// Draw the transparent state
			//-----------------------------------------------------------
			// vampire
			if (pCreature->IsFakeDie())
			{
				WORD colorSet = pCreature->GetAttachEffectColor();
				
				if( pCreature->GetCreatureType() == CREATURETYPE_BAT && pCreature->GetBatColor() != 0xFFFF)
					colorSet = pCreature->GetBatColor();
				
				//---------------------------------------- 
				// When the whole character changes color
				//---------------------------------------- 
				if (colorSet < MAX_COLORSET)
				{
					// Put EFFECT into IndexSprite too...
					// It should appear/disappear gradually, smoothly~ - -;
					m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, colorSet);
				}
				else
				{
					int colorSet1 = pCreature->GetBodyColor1();
					int colorSet2 = pCreature->GetBodyColor2();
					
					if( colorSet1 == QUEST_ITEM_COLOR || colorSet1 == UNIQUE_ITEM_COLOR )
						colorSet1 = MItem::GetSpecialColorItemColorset( colorSet1 );
					
					if( colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )
						colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					
					int netValue = pCreature->GetFakeDieCount()/5;
					
					CSpriteSurface::s_Value1 = netValue;
					
					CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_NET );
					CIndexSprite::SetEffect( CIndexSprite::EFFECT_NET );
					
					m_pSurface->BltIndexSpriteEffect(&pointTemp, pSprite);
					
				}
			}
			else if (pCreature->IsInvisible())
			{
				DrawInvisibleVampireSprite(m_pSurface, &pointTemp, pSprite, pCreature);
			}					
			else
			{	
				//-----------------------------------------------------------
				// Moving fast
				//-----------------------------------------------------------
				if (pCreature->IsFastMove() || pCreature->IsBloodyZenith())
				{
					int colorSet1 = pCreature->GetBodyColor1();
					int colorSet2 = pCreature->GetBodyColor2();
					
					if( colorSet1 == QUEST_ITEM_COLOR || colorSet1 == UNIQUE_ITEM_COLOR )
						colorSet1 = MItem::GetSpecialColorItemColorset( colorSet1 );
					
					if( colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )
						colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
					
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					
					POINT pointTemp2;
					
					//CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_NET );
					//CIndexSprite::SetEffect( CIndexSprite::EFFECT_NET );
					
					int fastMoveShadowMax = 2;
					
					if(creature_type == CREATURETYPE_WILD_WOLF)
						fastMoveShadowMax = 6;
					if(pCreature->IsBloodyZenith())
					{
						fastMoveShadowMax = pCreature->GetBloodyZenithCount();
						pCreature->UpdateBloodyZenith();
					}
					for(int fastMoveShadow = fastMoveShadowMax; fastMoveShadow > 0; fastMoveShadow--)
					{
						// 1 pixel skip
						pointTemp2.x = pointTemp.x - (pCreature->GetCX() >> 1)*fastMoveShadow;
						pointTemp2.y = pointTemp.y - (pCreature->GetCY() >> 1)*fastMoveShadow;
						if(pCreature->IsBloodyZenith())
						{
							pointTemp2.y = static_cast<LONG>(pointTemp2.y - sin((double)max(0, pCreature->GetMoveCount()-fastMoveShadow)*3.141592/(double)pCreature->GetMoveCountMax())*100);
						}
						
						//CSpriteSurface::s_Value1 = 1;
						//m_pSurface->BltIndexSpriteEffect(&pointTemp2, pSprite);
						m_pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, min(2, fastMoveShadow));
					}
					
				}
				
				if(pCreature->IsBloodyZenith())
				{
					pointTemp.y = static_cast<LONG>(pointTemp.y - sin((double)pCreature->GetMoveCount()*3.141592/(double)pCreature->GetMoveCountMax())*100);
				}
				WORD colorSet = pCreature->GetAttachEffectColor();
				
				if( pCreature->GetCreatureType() == CREATURETYPE_BAT && pCreature->GetBatColor() != 0xFFFF)
					colorSet = pCreature->GetBatColor();
				
				//---------------------------------------- 
				// When the whole character changes color
				//---------------------------------------- 
				if (colorSet < MAX_COLORSET)
				{
					m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, colorSet);
				}
				//---------------------------------------- 
				// Normal color output
				//---------------------------------------- 					
				else
				{				
					int colorSet1 = pCreature->GetBodyColor1();
					int colorSet2 = pCreature->GetBodyColor2();
					
					if( colorSet1 == QUEST_ITEM_COLOR || colorSet1 == UNIQUE_ITEM_COLOR )
						colorSet1 = MItem::GetSpecialColorItemColorset( colorSet1 );
					
					if( colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )
						colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					
					int shadowCount = pCreature->GetShadowCount();
					
					//----------------------------------------
					// Shadow output
					//----------------------------------------
					if (shadowCount)
					{
						bool bDrawShadowFirst = (direction<=4) || creature_type == 764 ||  creature_type == 765;
						if (!bDrawShadowFirst)
						{
							if (pCreature->IsFade())
							{
								m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
							}
							else
							{
								m_pSurface->BltIndexSprite(&pointTemp, pSprite);
							}
						}
						
						// 0, 1, 2, 3, 4, 5
						const int darkbits[6] =
						{
							0, 1, 1, 2, 2, 3
						};
						
						POINT pointTemp2;
						
						// If moving, the coordinates change too
						int cxStep = 0, cyStep = 0, cxInc =0 , cyInc = 0;
						if (action==ACTION_MOVE)
						{
							cxInc = -pCreature->GetCX();
							cyInc = -pCreature->GetCY();
						}
						
						
						int faNum = FA.GetSize();
						for (int f=1; f<=shadowCount; f++)
						{
							int frame_f = frame-f;
							
							//---------------------------------------- 
							// f frames before
							//---------------------------------------- 
							if (frame_f < 0)
							{
								frame_f += faNum;
							}
							
							CFrame &Frame_f = FA[frame_f];
							int sprite_f	= Frame_f.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
							
							CIndexSprite* pSprite_f = &m_CreatureSPK[ sprite_f ];
							
							//									if (pSprite_f->IsNotInit())
							//									{
							//										LoadFromFileCreatureActionSPK( body, action );
							//									}
							
							int cx_f		= Frame_f.GetCX();
							int cy_f		= Frame_f.GetCY();
							
							pointTemp2.x = pPoint->x + cx_f + cxStep;
							pointTemp2.y = pPoint->y + cy_f + cyStep;
							cxStep += cxInc;
							cyStep += cyInc;
							
							m_pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite_f, darkbits[f]);																		
						}
						
						if (bDrawShadowFirst)
						{
							if (pCreature->IsFade())
							{
								m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
							}
							else
							{
								m_pSurface->BltIndexSprite(&pointTemp, pSprite);
							}
						}
					}
					//----------------------------------------
					// Draw the body only
					//----------------------------------------
					else
					{
						// Draw darkened
						if (pCreature->IsFade())
						{
							m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
						}
						else
						{
							m_pSurface->BltIndexSprite(&pointTemp, pSprite);
							
						}
					}
				}
			}

			if (bCutHeight)
			{
				// SDL2: Clipping is automatic in SDL backend
				// m_pSurface->SetClipBottom( previousClipBottom );
			}

			// 2004, 10, 28, sobeit add start  - mark the target monster of a monster-kill quest.
			if(pCreature->IsAlive() && (g_CurrentFrame&0x04)&&UI_IsMonsterKillQuest_Monster((*g_pCreatureTable)[pCreature->GetCreatureType()].SpriteTypes[0]))
			//if(pCreature->IsAlive() && (g_CurrentFrame&0x04))
			{
				UI_ShowTargetArrow(pPoint->x + 12 ,pPoint->y- pCreature->GetHeight()-20);
			}
		// 2004, 10, 28, sobeit add end
		

		}
//		}catch(...){} 
	}
		
	//----------------------------------------------------
	// Coffin output
	//----------------------------------------------------
	if (bInCasket)
	{
		// Must be *2 because of the shadow.
		int casketID = SPRITEID_CASKET_1 + pCreature->GetCasketType()*2;
		
		if (static_cast<DWORD>(casketID) < m_EtcSPK.GetSize())
		{
			CSprite* pSprite = &m_EtcSPK[casketID];
			
			if (pSprite!=NULL)
			{
				// Set it up at the center of the tile.
				pointTemp.x = pPoint->x + 24 - (pSprite->GetWidth()>>1);
				pointTemp.y = pPoint->y + TILE_Y - pSprite->GetHeight();
				
				//---------------------------------------- 
				// Set the character selection rectangle area
				//---------------------------------------- 	
				rect.left	= pointTemp.x;
				rect.top	= pointTemp.y;
				rect.right	= rect.left + pSprite->GetWidth();
				rect.bottom = rect.top + pSprite->GetHeight();
				if(frameindex==0 )
					pCreature->SetScreenRect( &rect );
				
				if (casketValue==0)
				{
					m_pSurface->BltSprite(&pointTemp, pSprite);						
				}
				else if (casketValue==64)
				{
					CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_WIPE_OUT );
					
					m_pSurface->BltSpriteEffect(&pointTemp, pSprite);
				}
				else
				{
					CSpriteSurface::s_Value1 = casketValue;
					
					CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_WIPE_OUT );
					
					m_pSurface->BltSpriteEffect(&pointTemp, pSprite);
				}
			}
		}
	}		
}

void	MTopView::DrawOustersCharacter( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	//  There is a chakram
	MCreatureWear *pCreatureWear = (MCreatureWear *)pCreature;
	
	const MCreatureWear::ADDON_INFO& addonInfoChakram = pCreatureWear->GetAddonInfo(ADDON_RIGHTHAND);
	bool bChakram = addonInfoChakram.bAddon && addonInfoChakram.ItemClass == ITEM_CLASS_OUSTERS_CHAKRAM;
	
	int tempAction = action;
	if(bChakram == false)
	{
		switch(tempAction)
		{
		case ACTION_STAND:
			tempAction = ACTION_OUSTERS_STAND;
			break;
			
		case ACTION_MOVE:
			tempAction = ACTION_OUSTERS_MOVE;
			break;
		}
	}
	
	// Draw the body, then draw the chakram
	const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(ADDON_COAT);
	const MCreatureWear::ADDON_INFO& bootsAddonInfo = pCreatureWear->GetAddonInfo(ADDON_TROUSER);
	
	if (addonInfo.bAddon && !pCreatureWear->IsGhost(1))
	{
		//				DEBUG_ADD("[DrawCreature] DrawCloth");
		int clothes = addonInfo.FrameID;
		
		
		FRAME_ARRAY &FA = m_OustersFPK[clothes][tempAction][direction];
		
		// If the motion exists
		if (FA.GetSize() > frame)
		{
			CFrame &Frame = FA[frame];					
			int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
			int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
			int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
			
			CIndexSprite* pSprite = &m_OustersSPK[ sprite ];					
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			//---------------------------------------- 
			// Set the character selection rectangle area
			//---------------------------------------- 	
			rect.left	= pointTemp.x;
			rect.top	= pointTemp.y;
			rect.right	= rect.left + pSprite->GetWidth();
			rect.bottom = rect.top + pSprite->GetHeight();
			pCreature->AddScreenRect( &rect );
			
			//---------------------------------------- 
			// When a part of the character changes color
			//---------------------------------------- 
			if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
			{
				m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, 
					addonInfo.EffectColorSet);
			}
			//---------------------------------------- 
			// Normal character output
			//---------------------------------------- 
			else
			{
				CIndexSprite::SetUsingColorSetOnly( 0, pCreature->GetBodyColor1() );	// hair color
				
				int Colorset;
				
				Colorset = addonInfo.ColorSet2;
				
				if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
					Colorset = MItem::GetSpecialColorItemColorset( Colorset );
				CIndexSprite::SetUsingColorSetOnly( 1, Colorset );	// clothes color
				
				if(bChakram == false)	// if wearing a wristlet
				{
					if(addonInfoChakram.bAddon)
					{
						Colorset = addonInfoChakram.ColorSet2;
						if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
							Colorset = MItem::GetSpecialColorItemColorset( Colorset );
						
						CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arm
					}
					else
					{
						Colorset = defaultCoatColor;
						if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
							Colorset = MItem::GetSpecialColorItemColorset( Colorset );
						
						CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arm
					}
				}
				else
				{
					Colorset = defaultCoatColor;
					if(addonInfoChakram.bAddon)
						Colorset = addonInfoChakram.ColorSet2;
					if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					
					CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arm
					
				}
				
				if(bootsAddonInfo.bAddon)
				{
					Colorset = bootsAddonInfo.ColorSet2;
					if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					
					CIndexSprite::SetUsingColorSetOnly( 3, Colorset );	// boots color
				}
				else
				{
					Colorset = defaultCoatColor ;
					if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					
					CIndexSprite::SetUsingColorSetOnly( 3, Colorset );	// arm
				}
				
				// Draw darkened
				if (pCreature->IsFade())
				{
					m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
				}
				else
				{
					m_pSurface->BltIndexSprite(&pointTemp, pSprite);
				}
			}			
		}
	}
	
	if (bChakram && !pCreatureWear->IsGhost(2))
	{
		//				DEBUG_ADD("[DrawCreature] DrawChakram");
		int clothes = addonInfoChakram.FrameID;
		
		FRAME_ARRAY &FA = m_OustersFPK[clothes][tempAction][direction];
		
		// If the motion exists
		if (FA.GetSize() > frame)
		{
			//					DEBUG_ADD_FORMAT("[DrawCreature] FA.GetSize() > frame %d %d", FA.GetSize(), frame);
			CFrame &Frame = FA[frame];					
			int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
			int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
			int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
			
			CIndexSprite* pSprite = &m_OustersSPK[ sprite ];					
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			//---------------------------------------- 
			// When a part of the character changes color
			//---------------------------------------- 
			if (addonInfoChakram.bEffectColor)	//colorSet < MAX_COLORSET)
			{
				m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, 
					addonInfoChakram.EffectColorSet);
			}
			//---------------------------------------- 
			// Normal character output
			//---------------------------------------- 
			else
			{
				int colorset1, colorset2;
				colorset1 = addonInfoChakram.ColorSet1;
				colorset2 = addonInfoChakram.ColorSet2;
				
				if( colorset1 == QUEST_ITEM_COLOR || colorset1 == UNIQUE_ITEM_COLOR )
					colorset1 = MItem::GetSpecialColorItemColorset( colorset1 );
				if( colorset2 == QUEST_ITEM_COLOR || colorset2 == UNIQUE_ITEM_COLOR )
					colorset2 = MItem::GetSpecialColorItemColorset( colorset2 );
				CIndexSprite::SetUsingColorSet( colorset1, colorset2 );
				// Draw darkened
				if (pCreature->IsFade())
				{
					m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
				}
				else
				{
					m_pSurface->BltIndexSprite(&pointTemp, pSprite);
				}
			}						
			
		}
		else
		{
			DEBUG_ADD_FORMAT("[DrawCreature] FA.GetSize() <= frame %d %d %d %d", FA.GetSize(), frame, pCreatureWear->GetActionCount(), pCreatureWear->GetActionCountMax());
			//					DEBUG_ADD_FORMAT("[DrawCreature] FA.GetSize() <= frame %d %d %d", clothes, tempAction, direction);
		}
	}
	
#ifdef OUTPUT_DEBUG_DRAW_PROCESS
	DEBUG_ADD("dcOK");
#endif
}

void	MTopView::DrawAdvancementClassOustersCharacter( 
				POINT* pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	//  There is a chakram
	MCreatureWear *pCreatureWear = (MCreatureWear *)pCreature;
	
	const MCreatureWear::ADDON_INFO& addonInfoChakram = pCreatureWear->GetAddonInfo(ADDON_RIGHTHAND);
	bool bChakram = addonInfoChakram.bAddon && addonInfoChakram.ItemClass == ITEM_CLASS_OUSTERS_CHAKRAM;
	
	int tempAction = ConvAdvancementOustersActionFromOusterAction( action, bChakram );

	if( tempAction == -1 )
		return;
	else
		tempAction -= ADVANCEMENT_ACTION_START;
	
	// Draw the body, then draw the chakram
	const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(ADDON_COAT);
	const MCreatureWear::ADDON_INFO& bootsAddonInfo = pCreatureWear->GetAddonInfo(ADDON_TROUSER);
	
	if (addonInfo.bAddon && !pCreatureWear->IsGhost(1))
	{
		//				DEBUG_ADD("[DrawCreature] DrawCloth");
		
		
		//FRAME_ARRAY &FA = m_OustersFPK[clothes][tempAction][direction];
		FRAME_ARRAY &FA = m_AdvancementOustersFPK[ 1 ][ tempAction ][ direction ];
		
		// If the motion exists
		if (FA.GetSize() > frame)
		{
			CFrame &Frame = FA[frame];					
			int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
			int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
			int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
			
			//CIndexSprite* pSprite = &m_OustersSPK[ sprite ];					
			CIndexSprite* pSprite = &m_AdvancementOustersSPK[ sprite ];
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			//---------------------------------------- 
			// Set the character selection rectangle area
			//---------------------------------------- 	
			rect.left	= pointTemp.x;
			rect.top	= pointTemp.y;
			rect.right	= rect.left + pSprite->GetWidth();
			rect.bottom = rect.top + pSprite->GetHeight();
			pCreature->AddScreenRect( &rect );
			
			//---------------------------------------- 
			// When a part of the character changes color
			//---------------------------------------- 
			if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
			{
				m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, 
					addonInfo.EffectColorSet);
			}
			//---------------------------------------- 
			// Normal character output
			//---------------------------------------- 
			else
			{
				CIndexSprite::SetUsingColorSetOnly( 0, pCreature->GetBodyColor1() );	// hair color
				
				int Colorset;
				
				Colorset = addonInfo.ColorSet2;
				
				if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
					Colorset = MItem::GetSpecialColorItemColorset( Colorset );
				CIndexSprite::SetUsingColorSetOnly( 1, Colorset );	// clothes color
				
				if(bChakram == false)	// if wearing a wristlet
				{
					if(addonInfoChakram.bAddon)
					{
						Colorset = addonInfoChakram.ColorSet2;
						if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
							Colorset = MItem::GetSpecialColorItemColorset( Colorset );
						
						CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arm
					}
					else
					{
						Colorset = defaultCoatColor;
						if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
							Colorset = MItem::GetSpecialColorItemColorset( Colorset );
						
						CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arm
					}
				}
				else
				{
					Colorset = defaultCoatColor;
					if(addonInfoChakram.bAddon)
						Colorset = addonInfoChakram.ColorSet2;
					if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					
					CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arm
					
				}
				
				if(bootsAddonInfo.bAddon)
				{
					Colorset = bootsAddonInfo.ColorSet2;
					if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					
					CIndexSprite::SetUsingColorSetOnly( 3, Colorset );	// boots color
				}
				else
				{
					Colorset = defaultCoatColor ;
					if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					
					CIndexSprite::SetUsingColorSetOnly( 3, Colorset );	// arm
				}
				
				// Draw darkened
				if (pCreature->IsFade())
				{
					m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
				}
				else
				{
					m_pSurface->BltIndexSprite(&pointTemp, pSprite);
				}
			}			
		}
	}
	
	if (bChakram && !pCreatureWear->IsGhost(2))
	{
		//				DEBUG_ADD("[DrawCreature] DrawChakram");
		
		//FRAME_ARRAY &FA = m_OustersFPK[clothes][tempAction][direction];
		FRAME_ARRAY &FA = m_AdvancementOustersFPK[ 0 ][ tempAction ][ direction ];
		
		// If the motion exists
		if (FA.GetSize() > frame)
		{
			//					DEBUG_ADD_FORMAT("[DrawCreature] FA.GetSize() > frame %d %d", FA.GetSize(), frame);
			CFrame &Frame = FA[frame];					
			int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
			int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
			int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
			
			//CIndexSprite* pSprite = &m_OustersSPK[ sprite ];					
			CIndexSprite* pSprite = &m_AdvancementOustersSPK[ sprite ];
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			//---------------------------------------- 
			// When a part of the character changes color
			//---------------------------------------- 
			if (addonInfoChakram.bEffectColor)	//colorSet < MAX_COLORSET)
			{
				m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, 
					addonInfoChakram.EffectColorSet);
			}
			//---------------------------------------- 
			// Normal character output
			//---------------------------------------- 
			else
			{
				int colorset1, colorset2;
				colorset1 = addonInfoChakram.ColorSet1;
				colorset2 = addonInfoChakram.ColorSet2;
				
				if( colorset1 == QUEST_ITEM_COLOR || colorset1 == UNIQUE_ITEM_COLOR )
					colorset1 = MItem::GetSpecialColorItemColorset( colorset1 );
				if( colorset2 == QUEST_ITEM_COLOR || colorset2 == UNIQUE_ITEM_COLOR )
					colorset2 = MItem::GetSpecialColorItemColorset( colorset2 );
				CIndexSprite::SetUsingColorSet( colorset1, colorset2 );
				// Draw darkened
				if (pCreature->IsFade())
				{
					m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
				}
				else
				{
					m_pSurface->BltIndexSprite(&pointTemp, pSprite);
				}
			}						
			
		}
		else
		{
			DEBUG_ADD_FORMAT("[DrawCreature] FA.GetSize() <= frame %d %d %d %d", FA.GetSize(), frame, pCreatureWear->GetActionCount(), pCreatureWear->GetActionCountMax());
			//					DEBUG_ADD_FORMAT("[DrawCreature] FA.GetSize() <= frame %d %d %d", clothes, tempAction, direction);
		}
	}
	
#ifdef OUTPUT_DEBUG_DRAW_PROCESS
	DEBUG_ADD("dcOK");
#endif
}


void	MTopView::DrawSlayerCharacter( POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, int FrameIndex )
{
	if(pCreature->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
		DrawInstallTurret( GetCreatureDrawPacks(), pPoint, pCreature, action, direction, frame, FrameIndex );
	else
	{
		MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
		
		// Draw the add-ons matching the Creature's Action.
		//action = pCreature->GetAction();
		
		WORD clothes;
		BYTE clothesType;
		
		//------------------------------------------------------------
		// Fade Out output
		//------------------------------------------------------------
		if (pCreature->HasEffectStatus( EFFECTSTATUS_FADE_OUT ))
		{
			DrawFadeOut( GetCreatureDrawPacks(), pPoint, pCreature, action, direction, frame);
		}
		
		//------------------------------------------------------------
		// Fast Move output
		//------------------------------------------------------------
		if (pCreature->IsFastMove())
		{
			DrawFastMove( GetCreatureDrawPacks(), pPoint, pCreature, action, direction, frame );
		}
		//------------------------------------------------------------
		// Invisible  - a Slayer is transparent in snipping mode.
		//------------------------------------------------------------
		else if (pCreature->IsInvisible())
		{
			DrawInvisible( GetCreatureDrawPacks(), pPoint, pCreature, action, direction, frame );
		}
		//------------------------------------------------------------
		// Weapon afterimage skill
		//------------------------------------------------------------
		else if (
			(pCreature->HasEffectStatus( EFFECTSTATUS_DANCING_SWORD )
			|| pCreature->HasEffectStatus( EFFECTSTATUS_GHOST_BLADE )
			|| (pCreature->HasEffectStatus( EFFECTSTATUS_REDIANCE ) && pCreature->GetAction() == ACTION_STAND)
			)
			)
		{
			DrawWeaponFadeOut( GetCreatureDrawPacks(), pPoint, pCreature, action, direction, frame );
		}
		//------------------------------------------------------------
		// General output
		//------------------------------------------------------------
		else
		{
			for (int i=0; i<ADDON_MAX; i++)
			{
				// Depending on the Creature's current direction...
				// the order in which clothes are drawn may differ.
				clothesType = MCreatureWear::s_AddonOrder[pCreature->GetDirection()][i];
				
				// If it is wearing the i-th kind of clothes, draw it.
				const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(clothesType);
				//by viva NewSlayer
				if (addonInfo.bAddon)
				{
					clothes = addonInfo.FrameID;

					FRAME_ARRAY &FA = m_AddonFPK[clothes][action][direction];
					
					// If the motion exists
					if (FA.GetSize() > frame)
					{
						CFrame &Frame = FA[frame];					
						int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
						int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
						int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
						
						CIndexSprite* pSprite = &m_AddonSPK[ sprite ];
								
						pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
						pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
						
						//---------------------------------------- 
						// Set the character selection rectangle area
						//---------------------------------------- 	
						rect.left	= pointTemp.x;
						rect.top	= pointTemp.y;
						rect.right	= rect.left + pSprite->GetWidth();
						rect.bottom = rect.top + pSprite->GetHeight();
						pCreature->AddScreenRect( &rect );
						
						//---------------------------------------- 
						// When a part of the character changes color
						//---------------------------------------- 
						if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
						{
							m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, 
								addonInfo.EffectColorSet);
						}
						//---------------------------------------- 
						// Normal character output
						//---------------------------------------- 
						else
						{
							int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
							if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
							{
								// if it is a unique item
								//									if(colorSet2 == QUEST_ITEM_COLOR)
								//										colorSet2 = MItem::GetQuestItemColorset();
								//									else
								//										colorSet2 = MItem::GetUniqueItemColorset();										
								
								colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
								if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
									colorSet1 = colorSet2;
							}
							
							CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
							
							// Draw darkened
							if (pCreature->IsFade())
							{
								m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
							}
							else
							{
								m_pSurface->BltIndexSprite(&pointTemp, pSprite);
							}
						}						
						
						// Normal output
						//CIndexSprite::SetUsingColorSet( addonInfo.ColorSet1, addonInfo.ColorSet2 );
						//m_pSurface->BltIndexSprite(&pointTemp, pSprite);
						
					}
				}
			}
		}
	}
#ifdef OUTPUT_DEBUG_DRAW_PROCESS
DEBUG_ADD("dcOK");
#endif
}

void	MTopView::DrawAdvancementClassSlayerCharacter( POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, int FrameIndex )
{
	//add by viva
	int action_viva = action;
	if(pCreature->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
		DrawInstallTurret( GetCreatureDrawPacks(), pPoint, pCreature, action, direction, frame, FrameIndex );
	else
	{
		// ToT no time.. hard coding.. by sonee
		action = ConvAdvancementSlayerActionFromSlayerAction( action, dynamic_cast< MCreatureWear* >(pCreature) );
		
		if( action == -1 )
			return;
		else
			action -= ADVANCEMENT_ACTION_START;
		
		
		MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
		
		// Draw the add-ons matching the Creature's Action.
		//action = pCreature->GetAction();
		
		int clothes;
		BYTE clothesType;
		
		//------------------------------------------------------------
		// Fade Out output
		//------------------------------------------------------------
		if (pCreature->HasEffectStatus( EFFECTSTATUS_FADE_OUT ))
		{
			DrawFadeOutForACSlayer( GetCreatureDrawPacks(), pPoint, pCreature, action_viva, direction, frame);	//by viva	modifyed	try
		}
		
		//------------------------------------------------------------
		// Fast Move output
		//------------------------------------------------------------
		if (pCreature->IsFastMove())
		{
			DrawFastMoveForACSlayer( GetCreatureDrawPacks(), pPoint, pCreature, action_viva, direction, frame );	//by viva	modifyed try
		}
		//------------------------------------------------------------
		// Invisible  - a Slayer is transparent in snipping mode.
		//------------------------------------------------------------
		else if (pCreature->IsInvisible())
		{
			DrawInvisibleForACSlayer( GetCreatureDrawPacks(), pPoint, pCreature, action_viva, direction, frame );		//by viva	modifyed try
		}
		//------------------------------------------------------------
		// Weapon afterimage skill
		//------------------------------------------------------------
		else if (
			(pCreature->HasEffectStatus( EFFECTSTATUS_DANCING_SWORD )
			|| pCreature->HasEffectStatus( EFFECTSTATUS_GHOST_BLADE )
			|| (pCreature->HasEffectStatus( EFFECTSTATUS_REDIANCE ) && pCreature->GetAction() == ACTION_STAND)
			)
			)
		{
			DrawWeaponFadeOutForACSlayer( GetCreatureDrawPacks(), pPoint, pCreature, action_viva, direction, frame );	//by viva	modifyed try
		}
		//------------------------------------------------------------
		// General output
		//------------------------------------------------------------
		else
		{
			//by viva AdvancemnetSlayer
			action_viva = ConvNewSlayerActionFromSlayerAction(action_viva, dynamic_cast< MCreatureWear* >(pCreature));
//			if(action_viva>32)	
//				action_viva=36;
			
			CCreatureFramePack& slayerFPK = pCreature->IsMale() ? m_AdvancementSlayerManFPK : m_AdvancementSlayerWomanFPK;
			CIndexSpritePack& addonISPK = pCreature->IsMale() ? m_AdvancementSlayerManSPK : m_AdvancementSlayerWomanSPK;
			
			for( int i = 0; i < AC_ADDON_MAX; ++i )
			{
				clothesType = MCreatureWear::s_ACAddonOrder[ direction ][ i ];
				
				const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetACAddonInfo( clothesType );
				//if(addonInfo.ItemClass==ITEM_CLASS_MOTORCYCLE) break;
				if( addonInfo.bAddon )
				{	
					if((addonInfo.ItemClass==ITEM_CLASS_COAT && addonInfo.ItemType>=28)			//11
						||	(addonInfo.ItemClass==ITEM_CLASS_SWORD && addonInfo.ItemType>=16)		//14
						||	(addonInfo.ItemClass==ITEM_CLASS_BLADE && addonInfo.ItemType>=16)		//15
						||	(addonInfo.ItemClass==ITEM_CLASS_SHIELD && addonInfo.ItemType>=15)	//16
						||  (addonInfo.ItemClass==ITEM_CLASS_CROSS && addonInfo.ItemType>=14)		//17
						||	(addonInfo.ItemClass==ITEM_CLASS_HELM && addonInfo.ItemType>=15)		//19
						||  (addonInfo.ItemClass==ITEM_CLASS_AR && addonInfo.ItemType>=16)		//22
						||  (addonInfo.ItemClass==ITEM_CLASS_SR && addonInfo.ItemType>=16)		//23
						||  (addonInfo.ItemClass==ITEM_CLASS_MACE && addonInfo.ItemType>=14)		//35
//						||	addonInfo.ItemClass==ITEM_CLASS_MOTORCYCLE	//1
						)
					{
						int k = 1;
						if(clothesType == AC_ADDON_BODY)	k = 2;
						for(int i=0;i<k;i++)
						{	
						//	if(addonInfo.ItemClass==ITEM_CLASS_MOTORCYCLE)
						//		clothes = 14;
						//	else
								clothes=addonInfo.FrameID + i;
							if( clothes == -1 ) continue;
							if( clothes > slayerFPK.GetSize() -1 ) continue;
							if(action_viva > slayerFPK[clothes].GetSize() -1) continue;
							try{
							FRAME_ARRAY& FA = slayerFPK[ clothes ][ action_viva ][ direction ];
							
							if( FA.GetSize() > frame )
							{
								CFrame &Frame = FA[frame];					
								int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
								int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
								int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
								
								CIndexSprite* pSprite = &addonISPK[ sprite ];
								pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
								pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
								
								//---------------------------------------- 
								// Set the character selection rectangle area
								//---------------------------------------- 	
								rect.left	= pointTemp.x;
								rect.top	= pointTemp.y;
								rect.right	= rect.left + pSprite->GetWidth();
								rect.bottom = rect.top + pSprite->GetHeight();
								pCreature->AddScreenRect( &rect );
								
								//---------------------------------------- 
								// When a part of the character changes color
								//---------------------------------------- 
								if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
								{
									m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, 
										addonInfo.EffectColorSet);
								}
								//---------------------------------------- 
								// Normal character output
								//---------------------------------------- 
								else
								{
									if( addonInfo.bAddon )
									{
										int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
										if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
										{
											colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
											if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
												colorSet1 = colorSet2;
										}
										CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
									}
									else
										CIndexSprite::SetUsingColorSet( 375, 375 );
									
									// Draw darkened
									if (pCreature->IsFade())
									{
										m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
									}
									else
									{
										m_pSurface->BltIndexSprite(&pointTemp, pSprite);
									}
								}						
							}
							}catch(...){}
						}
					}
					else
					{
						
						// For now the part is used as it is. When clothes are added later, set and use the following variables.
						// Put the clothes information into addonInfo.FrameID and use it.
						
						clothes = GetAdvancementPartFromItemClass( addonInfo.ItemClass, addonInfo.FrameID );
						
						if( clothes == -1 ) continue;
//						clothes =14;
						try{
						FRAME_ARRAY& FA = slayerFPK[ clothes ][ action ][ direction ];
						
						
						if( FA.GetSize() > frame )
						{
							CFrame &Frame = FA[frame];					
							int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
							int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
							int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
							
							CIndexSprite* pSprite = &addonISPK[ sprite ];
							pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
							pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
							
							//---------------------------------------- 
							// Set the character selection rectangle area
							//---------------------------------------- 	
							rect.left	= pointTemp.x;
							rect.top	= pointTemp.y;
							rect.right	= rect.left + pSprite->GetWidth();
							rect.bottom = rect.top + pSprite->GetHeight();
							pCreature->AddScreenRect( &rect );
							
							//---------------------------------------- 
							// When a part of the character changes color
							//---------------------------------------- 
							if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
							{
								m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, 
									addonInfo.EffectColorSet);
							}
							//---------------------------------------- 
							// Normal character output
							//---------------------------------------- 
							else
							{
								if( addonInfo.bAddon )
								{
									int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
									if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
									{
										colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
										if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
											colorSet1 = colorSet2;
									}
									CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
								}
								else
									CIndexSprite::SetUsingColorSet( 375, 375 );
								
								// Draw darkened
								if (pCreature->IsFade())
								{
									m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
								}
								else
								{
									m_pSurface->BltIndexSprite(&pointTemp, pSprite);
								}
							}						
						}
						}catch(...){}
					}
				}
			}
		}
	}
}

void	MTopView::DrawAdvancementClassVampireCharacter( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame, int body, int frameindex )
{
	(void)body;
	//------------------------------------------------
	// Coffin output information
	//------------------------------------------------
	int casketValue = pCreature->GetCasketCount();
	int creature_type = pCreature->GetCreatureType();
	bool bInCasket = pCreature->IsInCasket();
	bool bCasketOnly = (bInCasket && casketValue==0);

	action = GetAdvancementVampireActionFromVampireAction( action, pCreature );

	if( action == -1 )
		return;
	else
		action -= ADVANCEMENT_ACTION_START;
//add by viva

	//CCreatureFramePack& advanceVampireFPK = pCreature->IsMale() ? m_AdvancementVampireManFPK : m_AdvancementVampireWomanFPK;
	//CIndexSpritePack& advanceVampireSPK = pCreature->IsMale() ? m_AdvancementVampireManSPK : m_AdvancementVampireWomanSPK;
	CCreatureFramePack& advanceVampireFPK = (creature_type>=807&&creature_type<=812) ? m_AdvancementVampireManFPK : pCreature->IsMale() ? m_AdvancementVampireManFPK : m_AdvancementVampireWomanFPK;
	CIndexSpritePack& advanceVampireSPK = (creature_type>=807&&creature_type<=812) ? m_AdvancementVampireManSPK : pCreature->IsMale() ? m_AdvancementVampireManSPK : m_AdvancementVampireWomanSPK;

//end

	if(creature_type>=807&&creature_type<=812)
		action = GetNewVampireActionFromVampireAction(action);
	//----------------------------------------------------
	// When only the coffin is drawn completely, the body is not drawn.
	//----------------------------------------------------
	if (!bCasketOnly)
	{
		//add by viva
		int viva_body=0;
		if(creature_type==807)
			viva_body=2;
		else if(creature_type==808)
			viva_body=3;
		else if(creature_type==809)
			viva_body=4;
		else if(creature_type==810)
			viva_body=5;
		else if(creature_type==811)
			viva_body=6;
		else if(creature_type==812)
			viva_body=7;
		else
			viva_body=0;

		if(action>advanceVampireFPK[viva_body].GetSize()-1) return;
		FRAME_ARRAY& FA = advanceVampireFPK[viva_body][action][direction];
	
		//end
		
		if (FA.GetSize() > frame)
		{
			CFrame& Frame =	FA[frame];
			int sprite = Frame.GetSpriteID(),	//m_CreatureFPK[body][action][direction][frame].GetSpriteID(),
				cx		= Frame.GetCX(),	//m_CreatureFPK[body][action][direction][frame].GetCX(),
				cy		= Frame.GetCY();	//m_CreatureFPK[body][action][direction][frame].GetCY();
			
			// coordinate correction
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			CIndexSprite* pSprite = &advanceVampireSPK[ sprite ];
			
			//---------------------------------------- 
			// Set the character selection rectangle area
			//---------------------------------------- 	
			rect.left	= pointTemp.x;
			rect.top	= pointTemp.y;
			rect.right	= rect.left + pSprite->GetWidth();
			rect.bottom = rect.top + pSprite->GetHeight();
			if(frameindex==0 )
				pCreature->SetScreenRect( &rect );
			//-----------------------------------------------------------
			// CutHeight
			//-----------------------------------------------------------
			bool bCutHeight = pCreature->IsCutHeight();
			if (bCutHeight)
			{
				// SDL2: Clipping is automatic

				pointTemp.y += pCreature->GetCutHeightCount() - 24;
			}
			
			//-----------------------------------------------------------
			// Draw the transparent state
			//-----------------------------------------------------------
			// vampire
			if (pCreature->IsFakeDie())
			{
				WORD colorSet = pCreature->GetAttachEffectColor();
				
				if( pCreature->GetCreatureType() == CREATURETYPE_BAT && pCreature->GetBatColor() != 0xFFFF)
					colorSet = pCreature->GetBatColor();
				
				//---------------------------------------- 
				// When the whole character changes color
				//---------------------------------------- 
				if (colorSet < MAX_COLORSET)
				{
					m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, colorSet);
				}
				else
				{
					int colorSet1 = pCreature->GetBodyColor1();
					int colorSet2 = pCreature->GetBodyColor2();
					
					if( colorSet1 == QUEST_ITEM_COLOR || colorSet1 == UNIQUE_ITEM_COLOR )
						colorSet1 = MItem::GetSpecialColorItemColorset( colorSet1 );
					
					if( colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )
						colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					
					int netValue = pCreature->GetFakeDieCount()/5;
					
					CSpriteSurface::s_Value1 = netValue;
					
					CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_NET );
					CIndexSprite::SetEffect( CIndexSprite::EFFECT_NET );
					
					m_pSurface->BltIndexSpriteEffect(&pointTemp, pSprite);
					
				}
			}
			else if (pCreature->IsInvisible())
			{
				DrawInvisibleVampireSprite(m_pSurface, &pointTemp, pSprite, pCreature);
			}					
			else
			{	
				//-----------------------------------------------------------
				// Moving fast
				//-----------------------------------------------------------
				if (pCreature->IsFastMove() || pCreature->IsBloodyZenith())
				{
					int colorSet1 = pCreature->GetBodyColor1();
					int colorSet2 = pCreature->GetBodyColor2();
					
					if( colorSet1 == QUEST_ITEM_COLOR || colorSet1 == UNIQUE_ITEM_COLOR )
						colorSet1 = MItem::GetSpecialColorItemColorset( colorSet1 );
					
					if( colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )
						colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
					
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					
					POINT pointTemp2;
					
					int fastMoveShadowMax = 2;
					if(pCreature->IsBloodyZenith())
					{
						fastMoveShadowMax = pCreature->GetBloodyZenithCount();
						pCreature->UpdateBloodyZenith();
					}
					for(int fastMoveShadow = fastMoveShadowMax; fastMoveShadow > 0; fastMoveShadow--)
					{
						// 1 pixel skip
						pointTemp2.x = pointTemp.x - (pCreature->GetCX() >> 1)*fastMoveShadow;
						pointTemp2.y = pointTemp.y - (pCreature->GetCY() >> 1)*fastMoveShadow;
						if(pCreature->IsBloodyZenith())
						{
							pointTemp2.y = static_cast<LONG>(pointTemp2.y - sin((double)max(0, pCreature->GetMoveCount()-fastMoveShadow)*3.141592/(double)pCreature->GetMoveCountMax())*100);
						}
						
						m_pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, min(2, fastMoveShadow));
					}
					
				}
				
				if(pCreature->IsBloodyZenith())
				{
					pointTemp.y = static_cast<LONG>(pointTemp.y - sin((double)pCreature->GetMoveCount()*3.141592/(double)pCreature->GetMoveCountMax())*100);
				}
				WORD colorSet = pCreature->GetAttachEffectColor();
				
				if( pCreature->GetCreatureType() == CREATURETYPE_BAT && pCreature->GetBatColor() != 0xFFFF)
					colorSet = pCreature->GetBatColor();
				
				//---------------------------------------- 
				// When the whole character changes color
				//---------------------------------------- 
				if (colorSet < MAX_COLORSET)
				{
					m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, colorSet);
				}
				//---------------------------------------- 
				// Normal color output
				//---------------------------------------- 					
				else
				{				
					int colorSet1 = pCreature->GetBodyColor1();
					int colorSet2 = pCreature->GetBodyColor2();
					
					if( colorSet1 == QUEST_ITEM_COLOR || colorSet1 == UNIQUE_ITEM_COLOR )
						colorSet1 = MItem::GetSpecialColorItemColorset( colorSet1 );
					
					if( colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )
						colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					
					int shadowCount = pCreature->GetShadowCount();
					
					//----------------------------------------
					// Shadow output
					//----------------------------------------
					if (shadowCount)
					{
						bool bDrawShadowFirst = (direction<=4) || creature_type == 764 ||  creature_type == 765;
						if (!bDrawShadowFirst)
						{
							if (pCreature->IsFade())
							{
								m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
							}
							else
							{
								m_pSurface->BltIndexSprite(&pointTemp, pSprite);
							}
						}
						
						// 0, 1, 2, 3, 4, 5
						const int darkbits[6] =
						{
							0, 1, 1, 2, 2, 3
						};
						
						POINT pointTemp2;
						
						// If moving, the coordinates change too
						int cxStep = 0, cyStep = 0, cxInc =0 , cyInc = 0;
						if (action==ACTION_MOVE)
						{
							cxInc = -pCreature->GetCX();
							cyInc = -pCreature->GetCY();
						}
						
						
						int faNum = FA.GetSize();
						for (int f=1; f<=shadowCount; f++)
						{
							int frame_f = frame-f;
							
							//---------------------------------------- 
							// f frames before
							//---------------------------------------- 
							if (frame_f < 0)
							{
								frame_f += faNum;
							}
							
							CFrame &Frame_f = FA[frame_f];
							int sprite_f	= Frame_f.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
							
							CIndexSprite* pSprite_f = &m_CreatureSPK[ sprite_f ];
							
							int cx_f		= Frame_f.GetCX();
							int cy_f		= Frame_f.GetCY();
							
							pointTemp2.x = pPoint->x + cx_f + cxStep;
							pointTemp2.y = pPoint->y + cy_f + cyStep;
							cxStep += cxInc;
							cyStep += cyInc;
							
							m_pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite_f, darkbits[f]);																		
						}
						
						if (bDrawShadowFirst)
						{
							if (pCreature->IsFade())
							{
								m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
							}
							else
							{
								m_pSurface->BltIndexSprite(&pointTemp, pSprite);
							}
						}
					}
					//----------------------------------------
					// Draw the body only
					//----------------------------------------
					else
					{
						// Draw darkened
						if (pCreature->IsFade())
						{
							m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
						}
						else
						{
							m_pSurface->BltIndexSprite(&pointTemp, pSprite);
							
						}
					}
				}
			}

			if (bCutHeight)
			{
				// SDL2: Clipping is automatic - no restore needed
			}

			// 2004, 10, 28, sobeit add start  - mark the target monster of a monster-kill quest.
			if(pCreature->IsAlive() && (g_CurrentFrame&0x04)&&UI_IsMonsterKillQuest_Monster((*g_pCreatureTable)[pCreature->GetCreatureType()].SpriteTypes[0]))
			//if(pCreature->IsAlive() && (g_CurrentFrame&0x04))
			{
				UI_ShowTargetArrow(pPoint->x + 12 ,pPoint->y- pCreature->GetHeight()-20);
			}
		// 2004, 10, 28, sobeit add end
		}

		bool bWeapon = true;

		bool bBat = pCreature->GetCreatureType() == CREATURETYPE_BAT;
		bool bWolf = pCreature->GetCreatureType() == CREATURETYPE_WOLF;
		bool bWerWolf = pCreature->GetCreatureType() == CREATURETYPE_WER_WOLF;

		if( bWeapon && !bBat && !bWolf && !bWerWolf )
		{

			FRAME_ARRAY& FAWEAPON = advanceVampireFPK[1][action][direction];
			if (FAWEAPON.GetSize() > frame)
			{
				CFrame& Frame =	FAWEAPON[frame];
				int sprite = Frame.GetSpriteID(),	//m_CreatureFPK[body][action][direction][frame].GetSpriteID(),
					cx		= Frame.GetCX(),	//m_CreatureFPK[body][action][direction][frame].GetCX(),
					cy		= Frame.GetCY();	//m_CreatureFPK[body][action][direction][frame].GetCY();
				
				// coordinate correction
				pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
				pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
				
				CIndexSprite* pSprite = &advanceVampireSPK[ sprite ];
				
				//---------------------------------------- 
				// Set the character selection rectangle area
				//---------------------------------------- 	
				rect.left	= pointTemp.x;
				rect.top	= pointTemp.y;
				rect.right	= rect.left + pSprite->GetWidth();
				rect.bottom = rect.top + pSprite->GetHeight();
//				if(frameindex==0 )
//					pCreature->SetScreenRect( &rect );
				//-----------------------------------------------------------
				// CutHeight
				//-----------------------------------------------------------
				bool bCutHeight = pCreature->IsCutHeight();
			if (bCutHeight)
			{
				// SDL2: Clipping is automatic in SDL backend
				// previousClipBottom = m_pSurface->GetClipBottom();
				// m_pSurface->SetClipBottom( rect.bottom - 24 );

				pointTemp.y += pCreature->GetCutHeightCount() - 24;
			}
				
				//-----------------------------------------------------------
				// Draw the transparent state
				//-----------------------------------------------------------
				// vampire
				if (pCreature->IsFakeDie())
				{
					WORD colorSet = pCreature->GetAttachEffectColor();
					
					//---------------------------------------- 
					// When the whole character changes color
					//---------------------------------------- 
					if (colorSet < MAX_COLORSET)
					{
						m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, colorSet);
					}
					else
					{
						int netValue = pCreature->GetFakeDieCount()/5;
						
						CSpriteSurface::s_Value1 = netValue;
						
						CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_NET );
						CIndexSprite::SetEffect( CIndexSprite::EFFECT_NET );
						
						m_pSurface->BltIndexSpriteEffect(&pointTemp, pSprite);
						
					}
				}
				else if (pCreature->IsInvisible())
				{
					DrawInvisibleVampireSprite(m_pSurface, &pointTemp, pSprite, pCreature);
				}					
				else
				{	
					//-----------------------------------------------------------
					// Moving fast
					//-----------------------------------------------------------
					if (pCreature->IsFastMove() || pCreature->IsBloodyZenith())
					{
						POINT pointTemp2;
						
						int fastMoveShadowMax = 2;
						if(pCreature->IsBloodyZenith())
						{
							fastMoveShadowMax = pCreature->GetBloodyZenithCount();
							pCreature->UpdateBloodyZenith();
						}
						for(int fastMoveShadow = fastMoveShadowMax; fastMoveShadow > 0; fastMoveShadow--)
						{
							// 1 pixel skip
							pointTemp2.x = pointTemp.x - (pCreature->GetCX() >> 1)*fastMoveShadow;
							pointTemp2.y = pointTemp.y - (pCreature->GetCY() >> 1)*fastMoveShadow;
							if(pCreature->IsBloodyZenith())
							{
								pointTemp2.y = static_cast<LONG>(pointTemp2.y - sin((double)max(0, pCreature->GetMoveCount()-fastMoveShadow)*3.141592/(double)pCreature->GetMoveCountMax())*100);
							}
							
							m_pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, min(2, fastMoveShadow));
						}
						
					}
					
					if(pCreature->IsBloodyZenith())
					{
						pointTemp.y = static_cast<LONG>(pointTemp.y - sin((double)pCreature->GetMoveCount()*3.141592/(double)pCreature->GetMoveCountMax())*100);
					}
					WORD colorSet = pCreature->GetAttachEffectColor();
					
					//---------------------------------------- 
					// When the whole character changes color
					//---------------------------------------- 
					if (colorSet < MAX_COLORSET)
					{
						m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, colorSet);
					}
					//---------------------------------------- 
					// Normal color output
					//---------------------------------------- 					
					else
					{				
						int shadowCount = pCreature->GetShadowCount();
						
						//----------------------------------------
						// Shadow output
						//----------------------------------------
						if (shadowCount)
						{
							bool bDrawShadowFirst = (direction<=4) || creature_type == 764 ||  creature_type == 765;
							if (!bDrawShadowFirst)
							{
								if (pCreature->IsFade())
								{
									m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
								}
								else
								{
									m_pSurface->BltIndexSprite(&pointTemp, pSprite);
								}
							}
							
							// 0, 1, 2, 3, 4, 5
							const int darkbits[6] =
							{
								0, 1, 1, 2, 2, 3
							};
							
							POINT pointTemp2;
							
							// If moving, the coordinates change too
							int cxStep = 0, cyStep = 0, cxInc =0 , cyInc = 0;
							if (action==ACTION_MOVE)
							{
								cxInc = -pCreature->GetCX();
								cyInc = -pCreature->GetCY();
							}
							
							
							int faNum = FA.GetSize();
							for (int f=1; f<=shadowCount; f++)
							{
								int frame_f = frame-f;
								
								//---------------------------------------- 
								// f frames before
								//---------------------------------------- 
								if (frame_f < 0)
								{
									frame_f += faNum;
								}
								
								CFrame &Frame_f = FA[frame_f];
								int sprite_f	= Frame_f.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
								
								CIndexSprite* pSprite_f = &m_CreatureSPK[ sprite_f ];
								
								int cx_f		= Frame_f.GetCX();
								int cy_f		= Frame_f.GetCY();
								
								pointTemp2.x = pPoint->x + cx_f + cxStep;
								pointTemp2.y = pPoint->y + cy_f + cyStep;
								cxStep += cxInc;
								cyStep += cyInc;
								
								m_pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite_f, darkbits[f]);																		
							}
							
							if (bDrawShadowFirst)
							{
								if (pCreature->IsFade())
								{
									m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
								}
								else
								{
									m_pSurface->BltIndexSprite(&pointTemp, pSprite);
								}
							}
						}
						//----------------------------------------
						// Draw the body only
						//----------------------------------------
						else
						{
							// Draw darkened
							if (pCreature->IsFade())
							{
								m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
							}
							else
							{
								m_pSurface->BltIndexSprite(&pointTemp, pSprite);
								
							}
						}
					}
				}
			}
		}
	}
		
	//----------------------------------------------------
	// Coffin output
	//----------------------------------------------------
	if (bInCasket)
	{
		// Must be *2 because of the shadow.
		int casketID = SPRITEID_CASKET_1 + pCreature->GetCasketType()*2;
		
		if (static_cast<DWORD>(casketID) < m_EtcSPK.GetSize())
		{
			CSprite* pSprite = &m_EtcSPK[casketID];
			
			if (pSprite!=NULL)
			{
				// Set it up at the center of the tile.
				pointTemp.x = pPoint->x + 24 - (pSprite->GetWidth()>>1);
				pointTemp.y = pPoint->y + TILE_Y - pSprite->GetHeight();
				
				//---------------------------------------- 
				// Set the character selection rectangle area
				//---------------------------------------- 	
				rect.left	= pointTemp.x;
				rect.top	= pointTemp.y;
				rect.right	= rect.left + pSprite->GetWidth();
				rect.bottom = rect.top + pSprite->GetHeight();
				if(frameindex==0 )
					pCreature->SetScreenRect( &rect );
				
				if (casketValue==0)
				{
					m_pSurface->BltSprite(&pointTemp, pSprite);						
				}
				else if (casketValue==64)
				{
					CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_WIPE_OUT );
					
					m_pSurface->BltSpriteEffect(&pointTemp, pSprite);
				}
				else
				{
					CSpriteSurface::s_Value1 = casketValue;
					
					CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_WIPE_OUT );
					
					m_pSurface->BltSpriteEffect(&pointTemp, pSprite);
				}
			}
		}
	}
}

void	
MTopView::DrawSelectedVampireCreature( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame, int body, int frameindex )
{
	//------------------------------------------------
	// Coffin output information
	//------------------------------------------------
	int casketValue = pCreature->GetCasketCount();
	bool bInCasket = pCreature->IsInCasket();
	bool bCasketOnly = (bInCasket && casketValue==0);
	
	if (!bCasketOnly)
	{
		FRAME_ARRAY& FA = m_CreatureFPK[body][action][direction];
		
		if (FA.GetSize() > frame)
		{			
			CFrame&	Frame = FA[frame];
			
			int sprite = Frame.GetSpriteID(),	//m_CreatureFPK[body][action][direction][frame].GetSpriteID(),
				cx		= Frame.GetCX(),	//m_CreatureFPK[body][action][direction][frame].GetCX(),
				cy		= Frame.GetCY();	//m_CreatureFPK[body][action][direction][frame].GetCY();
			
			CIndexSprite* pSprite = &m_CreatureSPK[ sprite ];
			
			//					if (pSprite->IsNotInit())
			//					{
			//						LoadFromFileCreatureActionSPK( body, action );
			//					}
			
			// coordinate correction
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();			
			
			//---------------------------------------- 		
			// Set the character selection rectangle area
			//---------------------------------------- 	
			rect.left	= pointTemp.x;
			rect.top	= pointTemp.y;
			rect.right	= rect.left + pSprite->GetWidth();
			rect.bottom = rect.top + pSprite->GetHeight();
			if(frameindex ==0 )
				pCreature->SetScreenRect( &rect );
			
			WORD colorSet = pCreature->GetAttachEffectColor();
			
			if( pCreature->GetCreatureType() == CREATURETYPE_BAT && pCreature->GetBatColor() != 0xFFFF)
				colorSet = pCreature->GetBatColor();
			
			//---------------------------------------- 
			// When the whole character changes color
			//---------------------------------------- 
			if (colorSet < MAX_COLORSET)
			{
				m_SOM.Add( pointTemp.x, pointTemp.y, pSprite, colorSet );
			}					
			//---------------------------------------- 
			// Normal output
			//---------------------------------------- 										
			else
			{					
				int colorSet1 = pCreature->GetBodyColor1();
				int colorSet2 = pCreature->GetBodyColor2();
				
				if ( colorSet1 == UNIQUE_ITEM_COLOR || colorSet1 == QUEST_ITEM_COLOR )
					colorSet1 = MItem::GetSpecialColorItemColorset( colorSet1 );
				if ( colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR )
					colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
				
				CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
				
				m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );
			}
			
			
			int shadowCount = pCreature->GetShadowCount();
			
			//----------------------------------------
			// Shadow output
			//----------------------------------------
			if (shadowCount)
			{
				// 0, 1, 2, 3, 4, 5
				const int darkbits[6] =
				{
					0, 1, 1, 2, 2, 3
				};
				
				POINT pointTemp2;
				
				// If moving, the coordinates change too
				int cxStep = 0, cyStep = 0, cxInc =0 , cyInc = 0;
				if (action==ACTION_MOVE)
				{
					cxInc = -pCreature->GetCX();
					cyInc = -pCreature->GetCY();
				}
				
				
				int faNum = FA.GetSize();
				for (int f=1; f<=shadowCount; f++)
				{
					int frame_f = frame-f;
					
					//---------------------------------------- 
					// f frames before
					//---------------------------------------- 
					if (frame_f < 0)
					{
						frame_f += faNum;
					}
					
					CFrame &Frame_f = FA[frame_f];
					int sprite_f	= Frame_f.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
					
					CIndexSprite* pSprite_f = &m_CreatureSPK[ sprite_f ];
					
					//							if (pSprite_f->IsNotInit())
					//							{	
					//								LoadFromFileCreatureActionSPK(body, action);
					//							}
					
					int cx_f		= Frame_f.GetCX();
					int cy_f		= Frame_f.GetCY();
					
					pointTemp2.x = pPoint->x + cx_f + cxStep;
					pointTemp2.y = pPoint->y + cy_f + cyStep;
					cxStep += cxInc;
					cyStep += cyInc;
					
					m_pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite_f, darkbits[f]);																
				}
			}	// shadowcount			
		}
	}

	//----------------------------------------------------
	// Coffin output
	//----------------------------------------------------
	if (bInCasket)
	{
		// *2 because of the shadow.
		int casketID = SPRITEID_CASKET_1 + pCreature->GetCasketType()*2;
		
		if (static_cast<DWORD>(casketID) < m_EtcSPK.GetSize())
		{
			CSprite* pSprite = &m_EtcSPK[casketID];
			
			if (pSprite!=NULL)
			{
				// Set it up at the center of the tile.
				pointTemp.x = pPoint->x + 24 - (pSprite->GetWidth()>>1);
				pointTemp.y = pPoint->y + TILE_Y - pSprite->GetHeight();
				
				//---------------------------------------- 
				// Set the character selection rectangle area
				//---------------------------------------- 	
				rect.left	= pointTemp.x;
				rect.top	= pointTemp.y;
				rect.right	= rect.left + pSprite->GetWidth();
				rect.bottom = rect.top + pSprite->GetHeight();
				if(frameindex ==0 )
					pCreature->SetScreenRect( &rect );
				
				//---------------------------------------- 	
				// Draw everything unconditionally
				//---------------------------------------- 	
				m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );						
			}
		}
	}
}

void	MTopView::DrawSelectedAdvancementVampireCreature( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame, int body, int frameindex )
{
		//------------------------------------------------
	// Coffin output information
	//------------------------------------------------
	int casketValue = pCreature->GetCasketCount();
	bool bInCasket = pCreature->IsInCasket();
	bool bCasketOnly = (bInCasket && casketValue==0);
	int creature_type = pCreature->GetCreatureType();	//add by viva
	action = GetAdvancementVampireActionFromVampireAction( action, pCreature );

	if( action == -1 )
		return;
	else
		action -= ADVANCEMENT_ACTION_START;
//by viva selected vampire
	//CCreatureFramePack& advanceVampireFPK = pCreature->IsMale() ? m_AdvancementVampireManFPK : m_AdvancementVampireWomanFPK;
	//CIndexSpritePack& advanceVampireSPK = pCreature->IsMale() ? m_AdvancementVampireManSPK : m_AdvancementVampireWomanSPK;	
	
	CCreatureFramePack& advanceVampireFPK = creature_type>=807&&creature_type<=812 ? m_AdvancementVampireManFPK : pCreature->IsMale() ? m_AdvancementVampireManFPK : m_AdvancementVampireWomanFPK;
	CIndexSpritePack& advanceVampireSPK = (creature_type>=807&&creature_type<=812) ? m_AdvancementVampireManSPK : pCreature->IsMale() ? m_AdvancementVampireManSPK : m_AdvancementVampireWomanSPK;

//end

	//----------------------------------------------------
	// When only the coffin is drawn completely, the body is not drawn.
	//----------------------------------------------------

	if (!bCasketOnly)
	{	
		for( int i = body; i <= body+1; ++i )
		{
			// 0:body 1: to be modified later depending on weapon attach/detach

			//add by viva	select vampire
			int viva_body=i;
			if(creature_type==807)
				viva_body=2;
			else if(creature_type==808)
				viva_body=3;
			else if(creature_type==809)
				viva_body=4;
			else if(creature_type==810)
				viva_body=5;
			else if(creature_type==811)
				viva_body=6;
			else if(creature_type==812)
				viva_body=7;
			else
				viva_body=i;

			FRAME_ARRAY& FA = advanceVampireFPK[viva_body][action][direction];
		//	FRAME_ARRAY& FA = advanceVampireFPK[i][action][direction];
			//end
			if (FA.GetSize() > frame)
			{			
				CFrame&	Frame = FA[frame];
				
				int sprite = Frame.GetSpriteID(),	//m_CreatureFPK[body][action][direction][frame].GetSpriteID(),
					cx		= Frame.GetCX(),	//m_CreatureFPK[body][action][direction][frame].GetCX(),
					cy		= Frame.GetCY();	//m_CreatureFPK[body][action][direction][frame].GetCY();
				
				CIndexSprite* pSprite = &advanceVampireSPK[ sprite ];
				
				//					if (pSprite->IsNotInit())
				//					{
				//						LoadFromFileCreatureActionSPK( body, action );
				//					}
				
				// coordinate correction
				pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
				pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();			
				
				//---------------------------------------- 		
				// Set the character selection rectangle area
				//---------------------------------------- 	
				rect.left	= pointTemp.x;
				rect.top	= pointTemp.y;
				rect.right	= rect.left + pSprite->GetWidth();
				rect.bottom = rect.top + pSprite->GetHeight();
				if(frameindex ==0 && i == body)
					pCreature->SetScreenRect( &rect );
				
				WORD colorSet = pCreature->GetAttachEffectColor();
				
				if( pCreature->GetCreatureType() == CREATURETYPE_BAT && pCreature->GetBatColor() != 0xFFFF)
					colorSet = pCreature->GetBatColor();
				
				//---------------------------------------- 
				// When the whole character changes color
				//---------------------------------------- 
				if (colorSet < MAX_COLORSET)
				{
					m_SOM.Add( pointTemp.x, pointTemp.y, pSprite, colorSet );
				}					
				//---------------------------------------- 
				// Normal output
				//---------------------------------------- 										
				else
				{					
					int colorSet1 = pCreature->GetBodyColor1();
					int colorSet2 = pCreature->GetBodyColor2();
					
					if ( colorSet1 == UNIQUE_ITEM_COLOR || colorSet1 == QUEST_ITEM_COLOR )
						colorSet1 = MItem::GetSpecialColorItemColorset( colorSet1 );
					if ( colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR )
						colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					
					m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );
				}
				
				
				int shadowCount = pCreature->GetShadowCount();
				
				//----------------------------------------
				// Shadow output
				//----------------------------------------
				if (shadowCount)
				{
					// 0, 1, 2, 3, 4, 5
					const int darkbits[6] =
					{
						0, 1, 1, 2, 2, 3
					};
					
					POINT pointTemp2;
					
					// If moving, the coordinates change too
					int cxStep = 0, cyStep = 0, cxInc =0 , cyInc = 0;
					if (action==ACTION_MOVE)
					{
						cxInc = -pCreature->GetCX();
						cyInc = -pCreature->GetCY();
					}
					
					
					int faNum = FA.GetSize();
					for (int f=1; f<=shadowCount; f++)
					{
						int frame_f = frame-f;
						
						//---------------------------------------- 
						// f frames before
						//---------------------------------------- 
						if (frame_f < 0)
						{
							frame_f += faNum;
						}
						
						CFrame &Frame_f = FA[frame_f];
						int sprite_f	= Frame_f.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
						
						CIndexSprite* pSprite_f = &advanceVampireSPK[ sprite_f ];
						
						//							if (pSprite_f->IsNotInit())
						//							{	
						//								LoadFromFileCreatureActionSPK(body, action);
						//							}
						
						int cx_f		= Frame_f.GetCX();
						int cy_f		= Frame_f.GetCY();
						
						pointTemp2.x = pPoint->x + cx_f + cxStep;
						pointTemp2.y = pPoint->y + cy_f + cyStep;
						cxStep += cxInc;
						cyStep += cyInc;
						
						m_pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite_f, darkbits[f]);																
					}
				}	// shadowcount			
			}
		}
	}

	//----------------------------------------------------
	// Coffin output
	//----------------------------------------------------
	if (bInCasket)
	{
		// *2 because of the shadow.
		int casketID = SPRITEID_CASKET_1 + pCreature->GetCasketType()*2;
		
		if (static_cast<DWORD>(casketID) < m_EtcSPK.GetSize())
		{
			CSprite* pSprite = &m_EtcSPK[casketID];
			
			if (pSprite!=NULL)
			{
				// Set it up at the center of the tile.
				pointTemp.x = pPoint->x + 24 - (pSprite->GetWidth()>>1);
				pointTemp.y = pPoint->y + TILE_Y - pSprite->GetHeight();
				
				//---------------------------------------- 
				// Set the character selection rectangle area
				//---------------------------------------- 	
				rect.left	= pointTemp.x;
				rect.top	= pointTemp.y;
				rect.right	= rect.left + pSprite->GetWidth();
				rect.bottom = rect.top + pSprite->GetHeight();
				if(frameindex ==0 )
					pCreature->SetScreenRect( &rect );
				
				//---------------------------------------- 	
				// Draw everything unconditionally
				//---------------------------------------- 	
				m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );						
			}
		}
	}
}

void	MTopView::DrawSelectedAdvancementSlayerCreature( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	int action_viva=action;
	action = ConvAdvancementSlayerActionFromSlayerAction( action, dynamic_cast< MCreatureWear* >(pCreature) );
	
	if( action == -1 )
		return;
	else
		action -= ADVANCEMENT_ACTION_START;
	
	MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
	// Draw the add-ons matching the Creature's Action.
	//action = pCreature->GetAction();	
	int clothes;
	BYTE clothesType;
	
	//by viva AdvancemnetSlayer
	action_viva = ConvNewSlayerActionFromSlayerAction(action_viva, dynamic_cast< MCreatureWear* >(pCreature));
	
	
	CCreatureFramePack& slayerFPK = pCreature->IsMale() ? m_AdvancementSlayerManFPK : m_AdvancementSlayerWomanFPK;
	CIndexSpritePack& addonISPK = pCreature->IsMale() ? m_AdvancementSlayerManSPK : m_AdvancementSlayerWomanSPK;
	
	for( int i = 0; i < AC_ADDON_MAX; ++i )
	{
		clothesType = MCreatureWear::s_ACAddonOrder[ direction ][ i ];
		
		const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetACAddonInfo( clothesType );
		
		if( addonInfo.bAddon )
		{	
			if((addonInfo.ItemClass==ITEM_CLASS_COAT && addonInfo.ItemType>=28)			//11
				||	(addonInfo.ItemClass==ITEM_CLASS_SWORD && addonInfo.ItemType>=16)		//14
				||	(addonInfo.ItemClass==ITEM_CLASS_BLADE && addonInfo.ItemType>=16)		//15
				||	(addonInfo.ItemClass==ITEM_CLASS_SHIELD && addonInfo.ItemType>=15)	//16
				||  (addonInfo.ItemClass==ITEM_CLASS_CROSS && addonInfo.ItemType>=14)		//17
				||	(addonInfo.ItemClass==ITEM_CLASS_HELM && addonInfo.ItemType>=15)		//19
				||  (addonInfo.ItemClass==ITEM_CLASS_AR && addonInfo.ItemType>=16)		//22
				||  (addonInfo.ItemClass==ITEM_CLASS_SR && addonInfo.ItemType>=16)		//23
				||  (addonInfo.ItemClass==ITEM_CLASS_MACE && addonInfo.ItemType>=14)		//35
				)
			{
				int k = 1;
				if(clothesType == AC_ADDON_BODY)	k = 2;
				for(int i=0;i<k;i++)
				{							
					clothes=addonInfo.FrameID + i;
					if( clothes == -1 ) continue;
					if( clothes > slayerFPK.GetSize() -1 ) continue;
					if(action_viva > slayerFPK[clothes].GetSize() -1) continue;
					
					FRAME_ARRAY& FA = slayerFPK[ clothes ][ action_viva ][ direction ];
					// If the motion exists
					if (FA.GetSize() > frame)
					{
						CFrame &Frame = FA[frame];					
						int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
						int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
						int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
						
						pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
						pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
						
						CIndexSprite* pSprite = &addonISPK[ sprite ];
						
						// If the outfit Sprite has not been initialized
						//						if (pSprite->IsNotInit())
						//						{
						//							LoadFromFileAddonSPK( clothes, action );
						//							//continue;
						//						}
						
						//---------------------------------------- 
						// Set the character selection rectangle area
						//---------------------------------------- 	
						rect.left	= pointTemp.x;
						rect.top	= pointTemp.y;
						rect.right	= rect.left + pSprite->GetWidth();
						rect.bottom = rect.top + pSprite->GetHeight();
						pCreature->AddScreenRect( &rect );
						
						// Add to SpriteOutlineManager
						//CIndexSprite::SetUsingColorSet((pCreature->GetID()+3*(clothesType,1),7)%MAX_COLORSET, ((pCreature->GetID()*pCreature->GetID(),7*(clothesType+2)))%MAX_COLORSET);
						//CIndexSprite::SetUsingColorSet((c1+3*(clothesType+1)+7)%MAX_COLORSET, ((c2*c2+7*(clothesType+2)))%MAX_COLORSET);
						//---------------------------------------- 
						// When a part of the character changes color
						//---------------------------------------- 
						if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
						{
							m_SOM.Add( pointTemp.x, pointTemp.y, pSprite, addonInfo.EffectColorSet );
						}
						//---------------------------------------- 
						// In the original colors..
						//---------------------------------------- 
						else
						{
							int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
							if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
							{
								if(colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )
									colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
								
								if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
									colorSet1 = colorSet2;
							}							
							
							CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
							m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );
						}
					}
					
				}
			}
			else
			{		
				// For now the part is used as it is. When clothes are added later, set and use the following variables.
				// Put the clothes information into addonInfo.FrameID and use it.
				
				clothes = GetAdvancementPartFromItemClass( addonInfo.ItemClass , addonInfo.FrameID);
				
				if( clothes == -1 )
					continue;
				
				FRAME_ARRAY &FA = slayerFPK[clothes][action][direction];
				
				// If the motion exists
				if (FA.GetSize() > frame)
				{
					CFrame &Frame = FA[frame];					
					int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
					int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
					int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
					
					pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
					pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
					
					CIndexSprite* pSprite = &addonISPK[ sprite ];
					
					// If the outfit Sprite has not been initialized
					//						if (pSprite->IsNotInit())
					//						{
					//							LoadFromFileAddonSPK( clothes, action );
					//							//continue;
					//						}
					
					//---------------------------------------- 
					// Set the character selection rectangle area
					//---------------------------------------- 	
					rect.left	= pointTemp.x;
					rect.top	= pointTemp.y;
					rect.right	= rect.left + pSprite->GetWidth();
					rect.bottom = rect.top + pSprite->GetHeight();
					pCreature->AddScreenRect( &rect );
					
					// Add to SpriteOutlineManager
					//CIndexSprite::SetUsingColorSet((pCreature->GetID()+3*(clothesType,1),7)%MAX_COLORSET, ((pCreature->GetID()*pCreature->GetID(),7*(clothesType+2)))%MAX_COLORSET);
					//CIndexSprite::SetUsingColorSet((c1+3*(clothesType+1)+7)%MAX_COLORSET, ((c2*c2+7*(clothesType+2)))%MAX_COLORSET);
					//---------------------------------------- 
					// When a part of the character changes color
					//---------------------------------------- 
					if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
					{
						m_SOM.Add( pointTemp.x, pointTemp.y, pSprite, addonInfo.EffectColorSet );
					}
					//---------------------------------------- 
					// In the original colors..
					//---------------------------------------- 
					else
					{
						int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
						if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
						{
							if(colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )
								colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
							
							if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
								colorSet1 = colorSet2;
						}							
						
						CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
						m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );
					}
				}
			}
		}
	}
}

void	MTopView::DrawSelectedAdvancementOustersCreature( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	MCreatureWear *pCreatureWear = (MCreatureWear *)pCreature;
	
	const MCreatureWear::ADDON_INFO& addonInfoChakram = pCreatureWear->GetAddonInfo(ADDON_RIGHTHAND);
	bool bChakram = addonInfoChakram.bAddon && addonInfoChakram.ItemClass == ITEM_CLASS_OUSTERS_CHAKRAM;

	int tempAction = ConvAdvancementOustersActionFromOusterAction( action, bChakram );

	if( tempAction == -1 )
		return;
	else
		tempAction -= ADVANCEMENT_ACTION_START;

	const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(ADDON_COAT);
	const MCreatureWear::ADDON_INFO& bootsAddonInfo = pCreatureWear->GetAddonInfo(ADDON_TROUSER);
	
	if (addonInfo.bAddon && !pCreatureWear->IsGhost(1))
	{
		int clothes = 1;		
		
		FRAME_ARRAY &FA = m_AdvancementOustersFPK[clothes][tempAction][direction];
		
		// If the motion exists
		if (FA.GetSize() > frame)
		{
			CFrame &Frame = FA[frame];					
			int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
			int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
			int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
			
			CIndexSprite* pSprite = &m_AdvancementOustersSPK[ sprite ];					
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			//---------------------------------------- 
			// Set the character selection rectangle area
			//---------------------------------------- 	
			rect.left	= pointTemp.x;
			rect.top	= pointTemp.y;
			rect.right	= rect.left + pSprite->GetWidth();
			rect.bottom = rect.top + pSprite->GetHeight();
			pCreature->AddScreenRect( &rect );
			
			//---------------------------------------- 
			// When a part of the character changes color
			//---------------------------------------- 
			if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
			{
				m_SOM.Add( pointTemp.x, pointTemp.y, pSprite, addonInfo.EffectColorSet );
			}
			//---------------------------------------- 
			// Normal character output
			//---------------------------------------- 
			else
			{
				CIndexSprite::SetUsingColorSetOnly( 0, pCreature->GetBodyColor1() );	// hair color
				
				int coatColor = addonInfo.ColorSet2, Colorset;
				if( coatColor == QUEST_ITEM_COLOR || coatColor == UNIQUE_ITEM_COLOR )
					coatColor = MItem::GetSpecialColorItemColorset( coatColor );						
				CIndexSprite::SetUsingColorSetOnly( 1, coatColor );	// clothes color
				if(bChakram == false)	// if wearing a wristlet
				{
					if(addonInfoChakram.bAddon)
					{
						Colorset = addonInfoChakram.ColorSet2;
						if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
							Colorset = MItem::GetSpecialColorItemColorset( Colorset );
						CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arm
					}
					else
					{
						Colorset = defaultCoatColor;								
						if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
							Colorset = MItem::GetSpecialColorItemColorset( Colorset );
						
						CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arm
					}
				}
				else
				{
					Colorset = defaultCoatColor;
					if(addonInfoChakram.bAddon)
						Colorset = addonInfoChakram.ColorSet2;
					if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arm
				}
				
				if(bootsAddonInfo.bAddon)
				{
					Colorset = bootsAddonInfo.ColorSet2;
					if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					CIndexSprite::SetUsingColorSetOnly( 3, Colorset );	// boots color
				}
				else
				{
					Colorset = defaultCoatColor;
					
					if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					CIndexSprite::SetUsingColorSetOnly( 3, Colorset );	// arm
				}
				
				m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );
			}						
			
		}
	}
	
	if (bChakram && !pCreatureWear->IsGhost(2))
	{
		int clothes = 0;
		
		FRAME_ARRAY &FA = m_AdvancementOustersFPK[clothes][tempAction][direction];
		
		// If the motion exists
		if (FA.GetSize() > frame)
		{
			CFrame &Frame = FA[frame];					
			int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
			int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
			int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
			
			CIndexSprite* pSprite = &m_AdvancementOustersSPK[ sprite ];					
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			//---------------------------------------- 
			// When a part of the character changes color
			//---------------------------------------- 
			if (addonInfoChakram.bEffectColor)	//colorSet < MAX_COLORSET)
			{
				m_SOM.Add( pointTemp.x, pointTemp.y, pSprite, addonInfo.EffectColorSet );
			}
			//---------------------------------------- 
			// Normal character output
			//---------------------------------------- 
			else
			{
				int colorset1, colorset2;
				
				colorset1 = addonInfoChakram.ColorSet1;
				colorset2 = addonInfoChakram.ColorSet2;
				if( colorset1 == QUEST_ITEM_COLOR || colorset1 == UNIQUE_ITEM_COLOR )
					colorset1 = MItem::GetSpecialColorItemColorset( colorset1 );
				if( colorset2 == QUEST_ITEM_COLOR || colorset2 == UNIQUE_ITEM_COLOR )
					colorset2 = MItem::GetSpecialColorItemColorset( colorset2 );
				
				
				CIndexSprite::SetUsingColorSet(colorset1, colorset2);
				m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );
			}						
			
		}
	}
}
