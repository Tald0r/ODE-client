//----------------------------------------------------------------------
// CreatureShadowDraw.cpp
//----------------------------------------------------------------------
// A creature's shadow (task 4.249: MTopView's members in
// DrawCreatureShadow.cpp until then, gameview's since). The view's packs
// and surface come in a CreatureDrawPacks, the player's shadow option as
// bUserBlendingShadow.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "CreatureShadowDraw.h"
#include "MCreatureWear.h"
#include "MFakeCreature.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MEventManager.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "UserInformation.h"
#include "AdvancementAction.h"
#include "SpriteIDDef.h"
#include "MViewDef.h"
#include "FrameClock.h"
#include "CSpriteSurface.h"

namespace {
	POINT pointTemp;
}

// MCreature.cpp's: the frames of a creature's action.
BYTE GetCreatureActionCountMax( const MCreature* pCreature, int action );

//----------------------------------------------------------------
// Draw CreatureShadow
//----------------------------------------------------------------
// 2D/3D와 관련해서...
// Lock / Unlock은 외부에서 해줘야 한다.
//----------------------------------------------------------------
void
DrawCreatureShadow(const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, bool bUserBlendingShadow)
{
	if(g_pEventManager->GetEventByFlag(EVENTFLAG_NOT_DRAW_CREATURE_SHADOW)
		|| (pCreature->IsGhost(1) && pCreature->IsGhost(2) && pCreature->IsGhost(4)))
		return;

	if(pCreature->IsOusters() && pCreature->IsPlayer() && g_pPlayer->IsOusters() == false && pCreature->IsInGroundElemental())
	{
		return;
	}

	// EFFECTSTATUS_GHOST 에 걸려있으면 남도 못보고 나도 못본다
	if(pCreature->HasEffectStatus(EFFECTSTATUS_GHOST))return;

	// 2004, 6, 7 sobeit add start - 펫일 경우 주인인 다크니스 안에 있으면 안보이게..
	if(pCreature->IsFakeCreature())
	{
		MCreature *OwnerCreature = g_pZone->GetCreature(((MFakeCreature*)pCreature)->GetOwnerID());
		if(OwnerCreature)
		{
			if(OwnerCreature->IsInDarkness())
				return;
		} 
	}
	// 2004, 6, 7 sobeit add end

	if(
		(g_pPlayer->HasEffectStatus(EFFECTSTATUS_YELLOW_POISON_TO_CREATURE) || 
		g_pPlayer->HasEffectStatus(EFFECTSTATUS_FLARE ) ||
		g_pPlayer->HasEffectStatus(EFFECTSTATUS_BLINDNESS))
		&& pCreature != g_pPlayer 
		)
	{
		// 거리에 따라서 출력 여부를 결정한다.
		int sx,sy,ex,ey;

		sx = g_pPlayer->GetX() - 1;
		ex = g_pPlayer->GetX() + 1;

		sy = g_pPlayer->GetY() - 1;
		ey = g_pPlayer->GetY() + 1;

		if( pCreature->GetX() < sx || pCreature->GetX() > ex ||
			pCreature->GetY() < sy || pCreature->GetY() > ey )
			return;
	}


	bool bBlendingShadow = false;
	bool bSlayerPet_ShowTurret = false;

	int body, action, direction, frame, creature_type, frameCount;

	frameCount  = pCreature->GetCreatureFrameCount();

	for(int FrameIndex = 0; FrameIndex< frameCount ; FrameIndex++)
	{
		action		= pCreature->GetAction();		
		body		= pCreature->GetCreatureFrameID(FrameIndex);
		direction	= pCreature->GetDirection();
		frame		= pCreature->GetFrame();
		creature_type = pCreature->GetCreatureType();

		// 2004, 7, 23 sobeit modify start
		switch(creature_type)
		{
		case 672:
			if(action == ACTION_MOVE )
			{
				frame = (g_CurrentFrame + pCreature->GetID()) % 11;	
			}
			break;
		case 717:
		case 721:
		case 723:// 질드레 or or 질드레 분신 or 각성 질드레
			if( pCreature->HasEffectStatus( EFFECTSTATUS_GDR_FLOATING )/* && action == ACTION_MOVE */)
			{
				action = ACTION_VAMPIRE_DRAIN;
				frame = 6 + (frame & 0x07);		// 흡혈 동작에서 반복 frame의 시작이 6이다.		
			}
			if(pCreature->IsDead())
			{
				if(pCreature->GetActionCount()>=pCreature->GetActionCountMax())
				{
					if(creature_type == 723)
						frame = 46; // 무릎꿇은 상태로..나둔당..
				}
			}
			break;
		case 722:// 질드레 얼음 석상 - 그림자 없음
			return;
		case 636:// 생선가게 주인 - 퀘스트용 일단 누워있게..나중에 분석점 하고 수정하자..
			{
				action = 6;
				frame = 14;
			}
			break;
		case 698:// 대지정령
			{
				if(!pCreature->HasEffectStatus(EFFECTSTATUS_COMA) && !pCreature->IsDead())
				{
					action = ACTION_STAND;
				}
				else
					action = ACTION_DIE;
				
				direction = 2;
			}
			break;
		case 433:// 테페즈 관
			{
				if(!pCreature->HasEffectStatus(EFFECTSTATUS_COMA) && !pCreature->IsDead())
					action = ACTION_STAND;
				else
					action = ACTION_DIE;
				
				direction = 1;
			}
			break;
		case 674:
			bBlendingShadow = true;
			break;
		case 675:
			{
				action = ACTION_MAGIC;
				direction = 3;
				frame = 0;
				bBlendingShadow= true;
			}
			break;
		case 702:
		case 703:
		case 704:
			if( action == ACTION_STAND || action == ACTION_MOVE )
				bSlayerPet_ShowTurret = true;
			break;
		case 730:
		case 731:
		case 732: // 웨이포인트 3종
			{
				if(pCreature->IsDead()) return;
				action = ACTION_STAND;
				frame = 0;
				direction = 2;
				bBlendingShadow= true;
			}
			break;
		case 743:
		case 744:
		case 745:
		case 746:
		case 747:
		case 748: // 장애물 5종
			{
				if(pCreature->IsDead())
				{
					action = ACTION_DIE;
					direction = 2;
					if(pCreature->GetActionCount()>=pCreature->GetActionCountMax()) return;
				}
				else
				{
					action = ACTION_STAND;
					frame = 0;
					direction = 2;
				}
			}
			break;
		case 738: // 플레져 스테이션
		case 739: // 랜드 마인
		case 741: // 오토 터렛
			{ 
	//			if(pCreature->IsDead()) return;
	//			if(pCreature->HasEffectStatus(EFFECTSTATUS_HIDE_TO_ATTACKER)) // 공성전시 공격측은 안보임
	//			{
	//				if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_1) || 
	//					g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_2) || 
	//					g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_3) || 
	//					g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_4) || 
	//					g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_5) )
	//					return;
	//			}
				action = ACTION_STAND;
				frame = 0;
				direction = 2;
				bBlendingShadow= true;
			}
			break;
		case 740: // 크레이모어
			{
	//			if(pCreature->IsDead()) return;
	//			if(pCreature->HasEffectStatus(EFFECTSTATUS_HIDE_TO_ATTACKER)) // 공성전시 공격측은 안보임
	//			{
	//				if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_1) || 
	//					g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_2) || 
	//					g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_3) || 
	//					g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_4) || 
	//					g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_5) )
	//					return;
	//			}
				action = ACTION_STAND;
				frame = 0;
				direction = 1;
			}
			[[fallthrough]];
		case 734: // 길드 타워
			{
				direction = 1;
				if(pCreature->IsAlive())
				{
					action = ACTION_STAND;
					frame = 0;
				}
			}
			break;
		case 753:// 서먼 고어 그랜드
			{
				direction = 2;
				if(pCreature->IsDead())
				{
					action = ACTION_DIE;
					if(pCreature->GetActionCount()>=pCreature->GetActionCountMax()) return;
					
				}
			}
			break;
		case 764:
		case 765: // 그레이트 러피언 2차
			if(action == ACTION_DIE && creature_type == 765)
				direction = 2;
			
			if(action == ACTION_DRAINED)
			{
				action = ACTION_ATTACK;
				if(frame>4)
				{
					frame = 4;
					
					if(g_CurrentFrame&0x01)
					{
						direction = ((direction - 1 ) + 8)%8;
						pCreature->SetCurrentDirection(direction);
					}
				}
			}
			break;
		case 767:// 로켓 런쳐
			frame = 0;
			action = ACTION_MOVE;
			break;
		}

		bool isSlayerCharacter = pCreature->IsSlayerCharacter();
		bool isOustersCharacter = pCreature->IsOusters() && pCreature->GetCreatureType() != CREATURETYPE_OUSTERS_OPERATOR
								&&	creature_type != 717 && creature_type != 721 && creature_type != 723;

		//----------------------------------------------------------
		// Hallu상태
		//----------------------------------------------------------
		if (pCreature->IsHallu())
		{
			// 여기서 이번 턴의 그림을 결정한다. - -;
			pCreature->DetermineHalluActionFrame();

			isSlayerCharacter = false;
			isOustersCharacter = false;

			body	= pCreature->GetHalluCreatureFrameID();
			action = pCreature->GetHalluAction();
			frame = pCreature->GetHalluFrame();
			creature_type = pCreature->GetHalluCreatureType();
		}

		// 성물은 무조건 아래, 크리스마스트리
		if(
			(creature_type >= 371 && creature_type <= 376) || 
			(creature_type >= 560 && creature_type <= 563) || 
			creature_type == 482 || 
			(creature_type >= 526 && creature_type <= 549) || 
			(creature_type >= 550 && creature_type <= 557) || 
			creature_type == 650 || 
			(creature_type >= 660 && creature_type <= 669) ||
			creature_type == 672 ||
			creature_type == 673
			)
		{
			direction = 2;
			bBlendingShadow = true;
		}


		// 임시 고정 그림자.
		/*
		if (pCreature->GetID()==g_pPlayer->GetID())
		{
			POINT pointTemp;

			if (pCreature->IsAlive())
			{
				pointTemp.x = pPoint->x - 5;
				pointTemp.y = pPoint->y - 30;
				//pointTemp.y = pPoint->y + cy + pCreature->GetSY();
				
				//m_pSurface->BltSprite(&pointTemp, &m_EtcSPK[ 1 ]);
				if (true)
				{	
					int sprite = 0;

					m_pSurface->Unlock();
					// EffectTexture 설정
					//// CDirect3D::GetDevice()->SetTexture() removed (SDL2)

					CSpriteSurface* pSurface = m_pCreatureShadowManager->GetTexture(sprite);
					// CDirect3D::GetDevice()->SetTexture() removed (SDL2)

					
					RECT rect = { pointTemp.x, 
									pointTemp.y, 
									pointTemp.x + m_pCreatureShadowManager->GetWidth(sprite),
									pointTemp.y + m_pCreatureShadowManager->GetHeight(sprite)
					};


					m_TextureEffect.DrawEffect2D( &rect );
					
					m_pSurface->Lock();			
				}
				else
				{	
					int sprite = 0;
					m_pSurface->BltShadowSpriteDarkness( &pointTemp, &m_CreatureSSPK[sprite], 1 );
				}
			}

			return;
		}
		*/	

		bool bPlayerVampire = 
				(*g_pCreatureSpriteTable)[(*g_pCreatureTable)[pCreature->GetCreatureType()].SpriteTypes[0]].IsPlayerVampireOnlySprite()
	//			&& (pCreature==g_pPlayer || pCreature->HasEffectStatus(EFFECTSTATUS_COMA))
				&& pCreature->GetActionCount()>=pCreature->GetActionCountMax()	
				&& !pCreature->IsExistAttachEffect()
				&& pCreature->IsDead();

		bool bTeenVersion = 
				(
					(g_pUserInformation->GoreLevel == false
					&& !(
					(creature_type >= 526 && creature_type <= 549) || 
					(
					(creature_type >= 371 && creature_type <= 376) || 
					(creature_type >= 560 && creature_type <= 563)
					) || 
					creature_type == 482 || 
					creature_type == 650 || 
					creature_type == 670 || 
					creature_type == 672 ||
					creature_type == 673 ||
					creature_type == 730 || // 웨이 포인트1
					creature_type == 731 || // 웨이 포인트2
					creature_type == 732 || // 웨이 포인트3
					creature_type == 636 // 생선 가게 아저씨
					))	// 성물 // 크리스마스트리
					|| (creature_type >= 377 && creature_type <= 386)
					|| creature_type == 480 // 다크 가디언은 먼지로 표시
				)
				&&
				(
					pCreature->GetActionCount()>=pCreature->GetActionCountMax()/2
					&& pCreature->IsDead()
				)
				&&
				(
					creature_type < 431		// 바토리
					|| creature_type > 436	// 테페즈
				);
		

		//---------------------------------------------------------
		// PC Vampire인 경우 죽으면 '먼지'로 표현한다.
		//
		// player는 죽어도 아이템으로 변하지 않기 때문에..
		// 틴버전에선 모든 시체를 '먼지'로 표현한다.
		//---------------------------------------------------------	
		if (bPlayerVampire || bTeenVersion)
		{
			if(pCreature->IsSlayer())
			{
				CSprite *pSprite = &(*packs.pEtcSPK)[ SPRITEID_SLAYER_CROSS_SHADOW ];
				// 좌표 보정
				POINT pointTemp;
				pointTemp.x = pPoint->x - 5;
				pointTemp.y = pPoint->y;				
				packs.pSurface->BltSprite(&pointTemp, pSprite);
			}


			return;		
		}

		//------------------------------------------------
		// Slayer인 경우 Darkness 안을 볼 수 없다.
		//------------------------------------------------
		if (pCreature->IsInDarkness() && !pCreature->IsNPC() && !g_pPlayer->IsVampire()
			&& g_pPlayer!=pCreature && (!g_pPlayer->HasEffectStatus( EFFECTSTATUS_LIGHTNESS)|| g_pZone->GetID() == 3001) )
		{

			return;
		}
			
		POINT pointTemp;


		//------------------------------------------------
		// 땅 속에 묻힌 애들은 그림자 없다.
		//// 투명 상태에서는 그림자 없다 
		//------------------------------------------------
		bool bPerfectCasket = pCreature->IsInCasket() && pCreature->GetCasketCount()==0;
		if (pCreature->IsUndergroundCreature()
			// 포탈 이동
			|| pCreature->IsCutHeight()
			// 관에 완전히 들어가는 경우
			|| bPerfectCasket
			//|| pCreature->GetInvisibleCount()==64
			)
		{

			//----------------------------------------------------
			// 관 출력
			//----------------------------------------------------
			if (bPerfectCasket)
			{
				// 그림자는 +1
				int casketType = pCreature->GetCasketType();
				int casketID = SPRITEID_CASKET_1 + pCreature->GetCasketType()*2 + 1;

				if (static_cast<DWORD>(casketID) < packs.pEtcSPK->GetSize())
				{
					CSprite* pSpriteImage = &(*packs.pEtcSPK)[casketID-1];
					CSprite* pSprite = &(*packs.pEtcSPK)[casketID];

					if (pSprite!=NULL)
					{
						// tile의 중심에서 세운다.
						pointTemp.x = pPoint->x + 24 - (pSpriteImage->GetWidth()>>1);

						pointTemp.y = pPoint->y + TILE_Y - pSpriteImage->GetHeight();

						const POINT shadowPoint[] =
						{
							{ -27, 28 },
							{ -27, 23 },
							{ -31, 28 }
						};

						pointTemp.x += shadowPoint[casketType].x;
						pointTemp.y += shadowPoint[casketType].y;

						packs.pSurface->BltSprite(&pointTemp, pSprite);
					}
				}
			}

			

			return;
		}

		//register int i;

		//----------------------------------------------------------
		// 머리가 없는 경우
		//----------------------------------------------------------
		if (!pCreature->HasHead() 
			&& pCreature->GetActionCount()==pCreature->GetActionCountMax())
		{	
			if (action==ACTION_DIE)
			{
				// 실제로는..마지막 frame + 1이다.
				//frame	= (*g_pCreatureTable)[creature_type].GetActionCount( action );
				frame	= GetCreatureActionCountMax( pCreature, action );
			}
		}
		
		// Teen버전인 경우 죽는 동작 애니메이션 안함
		// 처음엔 마지막프레임전까지의 동작만 0으로 세팅했는데 저 위쪽에서 재로 찍을 경우 return해버려서 의미없음
		if(g_pUserInformation->GoreLevel == false && action == ACTION_DIE
				&&
				(
					creature_type < 431		// 바토리
					|| creature_type > 436	// 테페즈
				)
			)
		{
			frame = 0;
		}

		// Creature가 존재하는 높이만큼 빼준다.

		// 그림자는 안 빼준다.
		//pPoint->y -= pCreature->GetZ();

		//------------------------------------------------
		//
		//       일반적인 Creature 출력
		//
		//------------------------------------------------
			
		//------------------------------------------------
		//
		//         Body출력
		//
		//------------------------------------------------
		if (body!=FRAMEID_NULL)
		{
			bool bBat = pCreature->GetCreatureType() == CREATURETYPE_BAT;
			bool bWolf = pCreature->GetCreatureType() == CREATURETYPE_WOLF;
			bool bWerWolf = pCreature->GetCreatureType() == CREATURETYPE_WER_WOLF;

			if( pCreature->IsAdvancementClass() && !bBat && !bWolf && !bWerWolf )
				DrawShadowAdvancementClassVampireCharacter( packs, pPoint, pCreature, action, direction, frame, body, bBlendingShadow, bSlayerPet_ShowTurret );
			else
				DrawShadowVampireCharacter( packs, pPoint, pCreature, action, direction, frame, body, bBlendingShadow, bSlayerPet_ShowTurret );
			
		}	
		
		//----------------------------------------
		//
		//            복장 출력
		//
		//----------------------------------------
		// 모든 입을 수 있는 종류의 옷 중에서 
		// 입고 있는 옷을 확인해서 출력해줘야 한다.
		
		//if (pCreature->IsWear())
		if (isSlayerCharacter)
		{
			if( pCreature->IsAdvancementClass() )
				DrawShadowAdvancementClassSlayerCharacter( packs, pPoint, pCreature, action, direction, frame, bUserBlendingShadow );
			else
				DrawShadowSlayerCharacter( packs, pPoint, pCreature, action, direction, frame, bUserBlendingShadow );
		}
		else if(isOustersCharacter)
		{
			if( pCreature->IsAdvancementClass() )
				DrawShadowAdvancementClassOustersCharacter( packs, pPoint, pCreature, action, direction, frame, bUserBlendingShadow );
			else
				DrawShadowOustersCharacter( packs, pPoint, pCreature, action, direction, frame, bUserBlendingShadow );
			
		}
	}

	
}

void	DrawShadowSlayerCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, bool bUserBlendingShadow )
{
	// Check if m_AddonShadowFPK is initialized (fix crash in SDL backend)
	// CCreatureFramePack doesn't have IsNotInit(), so we check m_Size
	if (packs.pAddonShadowFPK->GetSize() == 0) {
		return;
	}

	if(pCreature->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
	{
		//frame = pCreature->GetInstallTurretCount();
		if(ACTION_STAND == action)
		{
			direction = 2;
			frame = pCreature->GetInstallTurretCount();
		}
		FRAME_ARRAY& FA = (*packs.pCreatureShadowFPK)[222][action][direction];
		
		if (FA.GetSize() > frame)
		{			
			CFrame&	Frame = FA[frame];
			
			int sprite = Frame.GetSpriteID(),	//m_CreatureFPK[body][action][direction][frame].GetSpriteID(),
				cx		= Frame.GetCX(),	//m_CreatureFPK[body][action][direction][frame].GetCX(),
				cy		= Frame.GetCY();	//m_CreatureFPK[body][action][direction][frame].GetCY();
			
			CShadowSprite* pSprite = &(*packs.pCreatureSSPK)[ sprite ];
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();			
			
			packs.pSurface->BltShadowSprite(&pointTemp, pSprite);
			
		}
	}
	else
	{
		MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
		
		// Creature의 Action에 맞는 add-on을 출력한다.
		//action = pCreature->GetAction();
		
		
		//int id = pCreature->GetID();
		
		WORD clothes;
		BYTE clothesType;
		for (int i=0; i<ADDON_MAX; i++)
		{
			// Creature의 현재 방향에 따라서...
			// 옷을 출력해주는 순서가 다를 수 있다.
			clothesType = MCreatureWear::s_AddonOrder[pCreature->GetDirection()][i];
			
			// i번째 종류의 옷을 입고 있다면 출력해 준다.
			const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(clothesType);
			
			if (addonInfo.bAddon)
			{
				clothes = addonInfo.FrameID;
				
				
				FRAME_ARRAY &FA = (*packs.pAddonShadowFPK)[clothes][action][direction];
				
				// 있는 동작인 경우
				if (FA.GetSize() > frame)
				{
					CFrame &Frame = FA[frame];
					
					int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
					int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
					int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
					
					//CIndexSprite* pSprite = &(*pAddonSPK)[ sprite ];					
					
					// 복장Sprite가 초기화 되지 않은 경우
					//if (pSprite->IsNotInit())
					//	continue;
					
					pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
					pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
					CShadowSprite* pSprite = &(*packs.pAddonSSPK)[ sprite ];
						
					if (bUserBlendingShadow)
					{
						packs.pSurface->BltShadowSpriteDarkness( &pointTemp, pSprite, 1 );
					}
					else
					{
						packs.pSurface->BltShadowSprite( &pointTemp, pSprite );
					}
				}
			}
		}
	}
}

void	DrawShadowVampireCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, int body, bool bBlendingShadow , bool bSlayerPet_ShowTurret )
{
	FRAME_ARRAY &FA = (*packs.pCreatureShadowFPK)[body][action][direction];
	
	if (FA.GetSize() > frame)
	{
		CFrame &Frame = FA[frame];
		
		int sprite = Frame.GetSpriteID(),
			cx		= Frame.GetCX(),
			cy		= Frame.GetCY();
		
		// 좌표 보정
		pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
		pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
		CShadowSprite* pSprite = &(*packs.pCreatureSSPK)[sprite];
		
		//				if (pSprite->IsNotInit())
		//				{
		//					LoadFromFileCreatureActionSPK( body, action );
		//				}
		
		if (bBlendingShadow)
		{
			packs.pSurface->BltShadowSpriteDarkness( &pointTemp, pSprite, 1 );
		}
		else
		{
			packs.pSurface->BltShadowSprite( &pointTemp, pSprite );
		}
	}		
	
	// 슬레 펫인 경우는 터렛 찍어줘야 된다-ㅅ-;;;; 하드하드..아아-_-/~
	if( bSlayerPet_ShowTurret )
	{
		MFakeCreature *pFakeCreature = (MFakeCreature *)pCreature;
		BYTE direct = pFakeCreature->GetTurretDirection();
		
		FRAME_ARRAY& FA = (*packs.pCreatureShadowFPK)[body][ACTION_VAMPIRE_DRAIN][2];
		
		CFrame& Frame =	FA[direct];
		int sprite = Frame.GetSpriteID(),	//m_CreatureFPK[body][action][direction][frame].GetSpriteID(),
			cx		= Frame.GetCX(),	//m_CreatureFPK[body][action][direction][frame].GetCX(),
			cy		= Frame.GetCY();	//m_CreatureFPK[body][action][direction][frame].GetCY();
		
		if( pCreature->GetCreatureType() == 702 )
		{
			POINT pointGap[8] = 
			{
				{ 11, 6 },	// 좌
				{ 8, 2 },	// 좌하
				{ 0, 0 },	// 하
				{ -7, 2 },	// 우하
				{ -10, 6 },	// 우
				{ -7, 10 },	// 우상
				{ 0, 12 },	// 상
				{ 8, 10 },	// 좌상
			};
			cx += pointGap[pFakeCreature->GetDirection()].x;
			cy += pointGap[pFakeCreature->GetDirection()].y;
		}
		if( pFakeCreature->GetTurretDelay() > 0 )
		{
			extern POINT g_DirectionValue[MAX_DIRECTION];
			BYTE direct = (pFakeCreature->GetTurretDirection()/4+2)%8;
			cx -= g_DirectionValue[direct].x*pFakeCreature->GetTurretDelay()*2;
			cy -= g_DirectionValue[direct].y*pFakeCreature->GetTurretDelay()*2;
		}
		
		// 좌표 보정
		pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
		pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
		
		CShadowSprite* pSprite = &(*packs.pCreatureSSPK)[ sprite ];
		
		if (bBlendingShadow)
		{
			packs.pSurface->BltShadowSpriteDarkness( &pointTemp, pSprite, 1 );
		}
		else
		{
			packs.pSurface->BltShadowSprite( &pointTemp, pSprite );
		}
	}
}

void	DrawShadowOustersCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, bool bUserBlendingShadow )
{
	//  챠크람이 있다
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
	
	// 몸을 찍고 챠크람을 찍는다
	const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(ADDON_COAT);
	
	if (addonInfo.bAddon && !pCreatureWear->IsGhost(1))
	{
		int clothes = addonInfo.FrameID;
		
		FRAME_ARRAY &FA = (*packs.pOustersShadowFPK)[clothes][tempAction][direction];
		
		// 있는 동작인 경우
		if (FA.GetSize() > frame)
		{
			CFrame &Frame = FA[frame];
			
			int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
			int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
			int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			//				if(true)
			//				{
			//					m_pSurface->BltShadowSprite( &pointTemp, &(m_pOustersShadowManager->GetSprite(sprite)) );
			//				}
			//				else
			{
				CShadowSprite* pSprite = &(*packs.pOustersSSPK)[ sprite ];
				
				if (bUserBlendingShadow)
				{
					packs.pSurface->BltShadowSpriteDarkness( &pointTemp, pSprite, 1 );
				}
				else
				{
					packs.pSurface->BltShadowSprite( &pointTemp, pSprite );
				}
			}
		}
	}
	if (bChakram && !pCreatureWear->IsGhost(2))
	{
		int clothes = addonInfoChakram.FrameID;
		
		FRAME_ARRAY &FA = (*packs.pOustersShadowFPK)[clothes][tempAction][direction];
		
		// 있는 동작인 경우
		if (FA.GetSize() > frame)
		{
			CFrame &Frame = FA[frame];					
			int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
			int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
			int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
			
			CShadowSprite* pSprite = &(*packs.pOustersSSPK)[ sprite ];
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			//				if(true)
			//				{
			//					m_pSurface->BltShadowSprite( &pointTemp, &(m_pOustersShadowManager->GetSprite(sprite)) );
			//				}
			//				else
			{
				
				if (bUserBlendingShadow)
				{
					packs.pSurface->BltShadowSpriteDarkness( &pointTemp, pSprite, 1 );
				}
				else
				{
					packs.pSurface->BltShadowSprite( &pointTemp, pSprite );
				}
			}
			
		}
	}
}

void	DrawShadowAdvancementClassSlayerCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, bool bUserBlendingShadow )
{
	if(pCreature->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
	{
		//frame = pCreature->GetInstallTurretCount();
		if(ACTION_STAND == action)
		{
			direction = 2;
			frame = pCreature->GetInstallTurretCount();
		}
		FRAME_ARRAY& FA = (*packs.pCreatureShadowFPK)[222][action][direction];
		
		if (FA.GetSize() > frame)
		{			
			CFrame&	Frame = FA[frame];
			
			int sprite = Frame.GetSpriteID(),	//m_CreatureFPK[body][action][direction][frame].GetSpriteID(),
				cx		= Frame.GetCX(),	//m_CreatureFPK[body][action][direction][frame].GetCX(),
				cy		= Frame.GetCY();	//m_CreatureFPK[body][action][direction][frame].GetCY();
			
			CShadowSprite* pSprite = &(*packs.pCreatureSSPK)[ sprite ];
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();			
			
			packs.pSurface->BltShadowSprite(&pointTemp, pSprite);
			
		}
	}
	else
	{
		MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
		
		action = ConvAdvancementSlayerActionFromSlayerAction( action, dynamic_cast< MCreatureWear* >(pCreature) );

		if( action == -1 )
			return;
		else
			action -= ADVANCEMENT_ACTION_START;
		
		int clothes;
		BYTE clothesType;

		CCreatureFramePack& slayerFPK = pCreature->IsMale() ? (*packs.pAdvancementSlayerManShadowFPK) : (*packs.pAdvancementSlayerWomanShadowFPK);
		CShadowSpritePack& addonSSPK = pCreature->IsMale() ? (*packs.pAdvancementSlayerManSSPK) : (*packs.pAdvancementSlayerWomanSSPK);

		for (int i=0; i<AC_ADDON_MAX; i++)
		{
			// Creature의 현재 방향에 따라서...
			// 옷을 출력해주는 순서가 다를 수 있다.
			clothesType = MCreatureWear::s_ACAddonOrder[ direction ][ i ];
			
			// i번째 종류의 옷을 입고 있다면 출력해 준다.
			const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetACAddonInfo(clothesType);
			
			if (addonInfo.bAddon || i == AC_ADDON_BODY )
			{
				clothes = GetAdvancementPartFromItemClass( addonInfo.ItemClass , addonInfo.FrameID);		

				if( clothes == -1 )
					continue;
				
				FRAME_ARRAY &FA = slayerFPK[clothes][action][direction];
				
				// 있는 동작인 경우
				if (FA.GetSize() > frame)
				{
					CFrame &Frame = FA[frame];
					
					int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
					int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
					int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
					
					//CIndexSprite* pSprite = &(*pAddonSPK)[ sprite ];					
					
					// 복장Sprite가 초기화 되지 않은 경우
					//if (pSprite->IsNotInit())
					//	continue;
					
					pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
					pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
					CShadowSprite* pSprite = &addonSSPK[ sprite ];
						
					if (bUserBlendingShadow)
					{
						packs.pSurface->BltShadowSpriteDarkness( &pointTemp, pSprite, 1 );
					}
					else
					{
						packs.pSurface->BltShadowSprite( &pointTemp, pSprite );
					}
				}
			}
		}
	}
}

void	DrawShadowAdvancementClassVampireCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, int body, bool bBlendingShadow , bool bSlayerPet_ShowTurret )
{		
	CCreatureFramePack& advanceVampireFPK = pCreature->IsMale() ? (*packs.pAdvancementVampireManShadowFPK) : (*packs.pAdvancementVampireWomanShadowFPK);
	CShadowSpritePack& advanceVampireSSPK = pCreature->IsMale() ? (*packs.pAdvancementVampireManSSPK) : (*packs.pAdvancementVampireWomanSSPK);

	action = GetAdvancementVampireActionFromVampireAction( action, pCreature );

	if( action == -1 )
		return;
	else
		action -= ADVANCEMENT_ACTION_START;


	FRAME_ARRAY &FA = advanceVampireFPK[0][action][direction];
	
	if (FA.GetSize() > frame)
	{
		CFrame &Frame = FA[frame];
		
		int sprite	= Frame.GetSpriteID(),
			cx		= Frame.GetCX(),
			cy		= Frame.GetCY();
		
		// 좌표 보정
		pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
		pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
		CShadowSprite* pSprite = &advanceVampireSSPK[sprite];
		
		//				if (pSprite->IsNotInit())
		//				{
		//					LoadFromFileCreatureActionSPK( body, action );
		//				}
		
		if (bBlendingShadow)
		{
			packs.pSurface->BltShadowSpriteDarkness( &pointTemp, pSprite, 1 );
		}
		else
		{
			packs.pSurface->BltShadowSprite( &pointTemp, pSprite );
		}
	}		

	bool bWeapon = false;
	
	bool bBat = pCreature->GetCreatureType() == CREATURETYPE_BAT;
	bool bWolf = pCreature->GetCreatureType() == CREATURETYPE_WOLF;
	bool bWerWolf = pCreature->GetCreatureType() == CREATURETYPE_WER_WOLF;

	if( bWeapon && !bBat && !bWolf && !bWerWolf )
	{
		FRAME_ARRAY &FAW = advanceVampireFPK[1][action][direction];
	
		if (FAW.GetSize() > frame)
		{
			CFrame &Frame = FAW[frame];
			
			int sprite	= Frame.GetSpriteID(),
				cx		= Frame.GetCX(),
				cy		= Frame.GetCY();
			
			// 좌표 보정
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			CShadowSprite* pSprite = &advanceVampireSSPK[sprite];
			
			//				if (pSprite->IsNotInit())
			//				{
			//					LoadFromFileCreatureActionSPK( body, action );
			//				}
			
			if (bBlendingShadow)
			{
				packs.pSurface->BltShadowSpriteDarkness( &pointTemp, pSprite, 1 );
			}
			else
			{
				packs.pSurface->BltShadowSprite( &pointTemp, pSprite );
			}
		}
	}
	
	// 슬레 펫인 경우는 터렛 찍어줘야 된다-ㅅ-;;;; 하드하드..아아-_-/~
	if( bSlayerPet_ShowTurret )
	{
		MFakeCreature *pFakeCreature = (MFakeCreature *)pCreature;
		BYTE direct = pFakeCreature->GetTurretDirection();
		
		FRAME_ARRAY& FA = (*packs.pCreatureShadowFPK)[body][ACTION_VAMPIRE_DRAIN][2];
		
		CFrame& Frame =	FA[direct];
		int sprite = Frame.GetSpriteID(),	//m_CreatureFPK[body][action][direction][frame].GetSpriteID(),
			cx		= Frame.GetCX(),	//m_CreatureFPK[body][action][direction][frame].GetCX(),
			cy		= Frame.GetCY();	//m_CreatureFPK[body][action][direction][frame].GetCY();
		
		if( pCreature->GetCreatureType() == 702 )
		{
			POINT pointGap[8] = 
			{
				{ 11, 6 },	// 좌
				{ 8, 2 },	// 좌하
				{ 0, 0 },	// 하
				{ -7, 2 },	// 우하
				{ -10, 6 },	// 우
				{ -7, 10 },	// 우상
				{ 0, 12 },	// 상
				{ 8, 10 },	// 좌상
			};
			cx += pointGap[pFakeCreature->GetDirection()].x;
			cy += pointGap[pFakeCreature->GetDirection()].y;
		}
		if( pFakeCreature->GetTurretDelay() > 0 )
		{
			extern POINT g_DirectionValue[MAX_DIRECTION];
			BYTE direct = (pFakeCreature->GetTurretDirection()/4+2)%8;
			cx -= g_DirectionValue[direct].x*pFakeCreature->GetTurretDelay()*2;
			cy -= g_DirectionValue[direct].y*pFakeCreature->GetTurretDelay()*2;
		}
		
		// 좌표 보정
		pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
		pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
		
		CShadowSprite* pSprite = &(*packs.pCreatureSSPK)[ sprite ];
		
		if (bBlendingShadow)
		{
			packs.pSurface->BltShadowSpriteDarkness( &pointTemp, pSprite, 1 );
		}
		else
		{
			packs.pSurface->BltShadowSprite( &pointTemp, pSprite );
		}
	}
}

void	DrawShadowAdvancementClassOustersCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, bool bUserBlendingShadow )
{
		//  챠크람이 있다
	MCreatureWear *pCreatureWear = (MCreatureWear *)pCreature;
	
	const MCreatureWear::ADDON_INFO& addonInfoChakram = pCreatureWear->GetAddonInfo(ADDON_RIGHTHAND);
	bool bChakram = addonInfoChakram.bAddon && addonInfoChakram.ItemClass == ITEM_CLASS_OUSTERS_CHAKRAM;
	
	int tempAction = ConvAdvancementOustersActionFromOusterAction( action, bChakram );		

	if( tempAction == -1 )
		return;
	else
		tempAction -= ADVANCEMENT_ACTION_START;
	
	// 몸을 찍고 챠크람을 찍는다
	const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(ADDON_COAT);
	
	if (addonInfo.bAddon && !pCreatureWear->IsGhost(1))
	{
		FRAME_ARRAY &FA = (*packs.pAdvancementOustersShadowFPK)[1][tempAction][direction];
		
		// 있는 동작인 경우
		if (FA.GetSize() > frame)
		{
			CFrame &Frame = FA[frame];
			
			int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
			int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
			int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			CShadowSprite* pSprite = &(*packs.pAdvancementOustersSSPK)[ sprite ];
			
			if (bUserBlendingShadow)
			{
				packs.pSurface->BltShadowSpriteDarkness( &pointTemp, pSprite, 1 );
			}
			else
			{
				packs.pSurface->BltShadowSprite( &pointTemp, pSprite );
			}
		}
	}
	if (bChakram && !pCreatureWear->IsGhost(2))
	{
		FRAME_ARRAY &FA = (*packs.pAdvancementOustersShadowFPK)[0][tempAction][direction];
		
		// 있는 동작인 경우
		if (FA.GetSize() > frame)
		{
			CFrame &Frame = FA[frame];					
			int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
			int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
			int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
			
			CShadowSprite* pSprite = &(*packs.pAdvancementOustersSSPK)[ sprite ];
			
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
			
			if (bUserBlendingShadow)
			{
				packs.pSurface->BltShadowSpriteDarkness( &pointTemp, pSprite, 1 );
			}
			else
			{
				packs.pSurface->BltShadowSprite( &pointTemp, pSprite );
			}
		}
	}
}
