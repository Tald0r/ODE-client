//----------------------------------------------------------------------
// MStopZoneEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "DebugLog.h"
#include "MStopZoneEffectGenerator.h"
#include "MEffect.h"
#include "WorldTileGeometry.h"
#include "EffectSpriteTypeDef.h"
#include "MEventQueue.h"
#include "SkillDef.h"
#include <cstdlib>
#include <utility>

const MStopZoneEffectHost* MStopZoneEffectGenerator::s_pHost = nullptr;

const MStopZoneEffectHost* MStopZoneEffectGenerator::SetHost(const MStopZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MStopZoneEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MStopZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MStopZoneEffectGenerator::ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count)
{
	count = 0;
	return s_pHost && s_pHost->MaxFrames && s_pHost->MaxFrames(blt, frameID, count);
}

void MStopZoneEffectGenerator::AddEvent(MEvent& event)
{
	if (s_pHost && s_pHost->AddEvent) s_pHost->AddEvent(event);
}

bool MStopZoneEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MStopZoneEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	int est = egInfo.effectSpriteType;

	TYPE_FRAMEID newFrameID = FRAMEID_NULL;

	//-----------------------------------------------------------
	// 다크니스의 경우 다양하게 찍어주기...
	// 임시 땜빵 코드.. 케케~
	//-----------------------------------------------------------
	// frameID가 아니고 effectSpriteType으로 해야되는데
	// 일단은 alphaSprite라서 수치가 맞아떨어진다 - -;;
	if ((est>=EFFECTSPRITETYPE_DARKNESS_1_1
		&& est<=EFFECTSPRITETYPE_DARKNESS_3_5) ||
		(est>=EFFECTSPRITETYPE_GRAY_DARKNESS_1_1 &&
		est<=EFFECTSPRITETYPE_GRAY_DARKNESS_3_5))
	{
		if (egInfo.pPreviousEffect!=NULL)
		{
			newFrameID = 5 + (egInfo.pPreviousEffect)->GetFrameID();
		}
		else
		{
			if (est>=EFFECTSPRITETYPE_DARKNESS_2_1
				&& est<=EFFECTSPRITETYPE_DARKNESS_2_5)
			{
				est = EFFECTSPRITETYPE_DARKNESS_2_1 + rand()%5;
			}
			else if (est>=EFFECTSPRITETYPE_DARKNESS_1_1
				&& est<=EFFECTSPRITETYPE_DARKNESS_1_5)
			{
				est = EFFECTSPRITETYPE_DARKNESS_1_1 + rand()%5;
			}
			else if (est>=EFFECTSPRITETYPE_DARKNESS_3_1
				&& est<=EFFECTSPRITETYPE_DARKNESS_3_5)
			{
				est = EFFECTSPRITETYPE_DARKNESS_3_1 + rand()%5;
			}
			else if (est>=EFFECTSPRITETYPE_GRAY_DARKNESS_1_1 &&
				est<=EFFECTSPRITETYPE_GRAY_DARKNESS_1_5 )
			{
				est = EFFECTSPRITETYPE_GRAY_DARKNESS_1_1 + rand()%5;
			}
			else if (est>=EFFECTSPRITETYPE_GRAY_DARKNESS_2_1 &&
				est<=EFFECTSPRITETYPE_GRAY_DARKNESS_2_5 )
			{
				est = EFFECTSPRITETYPE_GRAY_DARKNESS_2_1 + rand()%5;
			}
			else if (est>=EFFECTSPRITETYPE_GRAY_DARKNESS_3_1 &&
				est<=EFFECTSPRITETYPE_GRAY_DARKNESS_3_5 )
			{
				est = EFFECTSPRITETYPE_DARKNESS_3_1 + rand()%5;
			}
		}
	}
	else if(est == EFFECTSPRITETYPE_INFINITY_THUNDERBOLT_CENTER)
	{
		est = EFFECTSPRITETYPE_INFINITY_THUNDERBOLT_CENTER + rand()%3;
	}

	MStopZoneEffectSprite sprite;
	if (!ReadSprite(static_cast<TYPE_EFFECTSPRITETYPE>(est), sprite)) return false;
	const BYTE bltType = sprite.bltType;
	TYPE_FRAMEID frameID = sprite.frameID;
	const bool repeatFrame = sprite.repeatFrame;

	if (newFrameID != FRAMEID_NULL)
	{
		frameID = newFrameID;
	}

	int direction = egInfo.direction;


	//-----------------------------------------------------------
	// Sword Wave를 위한 임시(-_-;) 코드..
	//-----------------------------------------------------------
	if (est==EFFECTSPRITETYPE_SWORD_WAVE_1)
	{
		direction = (direction + 1) % 8;	// +1
	}
	else if (est==EFFECTSPRITETYPE_SWORD_WAVE_2)
	{
		direction = (direction + 6) % 8;	// + 8 - 2
	}
	else if (est==EFFECTSPRITETYPE_SWORD_WAVE_3)
	{
		direction = (direction + 1) % 8;
	}


	//-----------------------------------------------------------
	// 메테오를 위한 임시(-_-;) 코드..
	//-----------------------------------------------------------
	if(est == EFFECTSPRITETYPE_METEOR_ROCK)
	{
		MEvent event;
		event.eventID = EVENTID_METEOR_SHAKE;
		event.eventType = EVENTTYPE_ZONE;
		event.eventDelay = 63;
		event.eventFlag = EVENTFLAG_SHAKE_SCREEN;
		event.parameter3 = 3;
		AddEvent(event);
	}

	//---------------------------------------------
	// pixel좌표를 Map의 좌표로 바꿔준다.
	//---------------------------------------------
	TYPE_SECTORPOSITION	sX, sY;
	sX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x0));
	sY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y0));

	int maxFrame;
	if (!ReadMaxFrames(bltType, frameID, maxFrame)) return false;

	// Zone에 추가한다.

	if( egInfo.nActionInfo == RESULT_SKILL_STONE_AUGER ||
		egInfo.nActionInfo == RESULT_STEP_SKILL_STONE_AUGER_2 ||
		egInfo.nActionInfo == RESULT_STEP_SKILL_STONE_AUGER_3 )
	{
		// 십자 모양으로.
		POINT pt[] =
		{
			{ 0, 1 },
			{ 1, 0 },
			{ 0, 0 },
			{ 0, -1 },
			{ -1, 0 }
		};

		sX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x1));
		sY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y1));

		bool targetTransferred = false;
		bool anyAccepted = false;
		for(int i=0;static_cast<size_t>(i)<sizeof(pt)/sizeof(POINT);i++)
		{
			auto effect = std::make_unique<MEffect>(bltType);
			MEffect* pEffect = effect.get();

			pEffect->SetFrameID(frameID, static_cast<BYTE>(maxFrame));

			pEffect->SetPosition(static_cast<TYPE_SECTORPOSITION>(sX+ pt[i].x), static_cast<TYPE_SECTORPOSITION>(sY+pt[i].y) );
			pEffect->SetZ( egInfo.z0 );

			pEffect->SetStepPixel( egInfo.step );
			pEffect->SetCount( egInfo.count, egInfo.linkCount );
			pEffect->SetDirection( direction );
			pEffect->SetPower( egInfo.power);
			pEffect->SetMulti( true );

			if (QueueEffect(std::move(effect)))
			{
				anyAccepted = true;
				if( egInfo.pEffectTarget != NULL )
				{
					if( i == 0 )
					{
						pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget);
						targetTransferred = true;
					}
					else
					{
						MEffectTarget *pTarget = new MEffectTarget(*egInfo.pEffectTarget);
						pEffect->SetLink( egInfo.nActionInfo, pTarget );
					}
				}

				if (repeatFrame && maxFrame > 0)
				{
					int num = rand() % maxFrame;

					for (int nf=0; nf<num; nf++)
					{
						pEffect->NextFrame();
					}
				}
			}
		}
		return egInfo.pEffectTarget ? targetTransferred : anyAccepted;

	} else
	{
			auto effect = std::make_unique<MEffect>(bltType);
			MEffect* pEffect = effect.get();

			// Debug: Log effect creation
			DEBUG_ADD_FORMAT("StopZoneEffect: Created pEffect=%p, bltType=%d", pEffect, bltType);

			pEffect->SetFrameID(frameID, static_cast<BYTE>(maxFrame));

			// Debug: Verify effect state before AddEffect
			#ifdef __SANITIZE_ADDRESS__
			// With ASAN, add extra validation
			if (pEffect) {
				TYPE_FRAMEID testFrameID = pEffect->GetFrameID();
				DEBUG_ADD_FORMAT("StopZoneEffect: pEffect=%p FrameID=%d before AddEffect", pEffect, testFrameID);
			}
			#endif
			if((est == EFFECTSPRITETYPE_FIRE_CRACKER_1 ||
				est == EFFECTSPRITETYPE_FIRE_CRACKER_2 ||
				est == EFFECTSPRITETYPE_FIRE_CRACKER_3 ) ||
				(est >= EFFECTSPRITETYPE_DRAGON_FIRE_CRACKER && est <= EFFECTSPRITETYPE_FIRE_CRACKER_4) ||
				est == EFFECTSPRITETYPE_PET_SLAYER_3TH_ATTACK_1 || est == EFFECTSPRITETYPE_PET_SLAYER_3TH_ATTACK_2
				)
				pEffect->SetPixelPosition( egInfo.x0, egInfo.y0, egInfo.z0 );
			else
				pEffect->SetPosition(sX, sY);		// Sector 좌표
			pEffect->SetZ(egInfo.z0);
			pEffect->SetStepPixel(egInfo.step);		// 실제로 움직이지는 않지만, 다음 Effect를 위해서 대입해준다.
			pEffect->SetCount( egInfo.count, egInfo.linkCount );			// 지속되는 Frame

			// 방향 설정
			pEffect->SetDirection( direction );

			// 위력
			pEffect->SetPower(egInfo.power);

			// 빛의 밝기
			//pEffect->SetLight( light );

			// 중복 가능한가
			if(
				(est == EFFECTSPRITETYPE_FIRE_CRACKER_1 ||
				est == EFFECTSPRITETYPE_FIRE_CRACKER_2 ||
				est == EFFECTSPRITETYPE_FIRE_CRACKER_3 ) ||
				(est >= EFFECTSPRITETYPE_DRAGON_FIRE_CRACKER && est <= EFFECTSPRITETYPE_FIRE_CRACKER_4) ||
				est == EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW ||
				est == EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW_SHADOW ||
				est == EFFECTSPRITETYPE_GREAT_RUFFIAN_1_AXE_GROUND ||
				est == EFFECTSPRITETYPE_GREAT_RUFFIAN_1_AXE_WAVE||
				est == EFFECTSPRITETYPE_NEW_PLASMA_ROCKET_LAUNCHER_BLOW
//				|| est == EFFECTSPRITETYPE_ACID_ERUPTION_1
//				|| est == EFFECTSPRITETYPE_ACID_ERUPTION_2
//				|| est == EFFECTSPRITETYPE_ACID_ERUPTION_3
//				|| est == EFFECTSPRITETYPE_ACID_ERUPTION_4
//				|| est == EFFECTSPRITETYPE_ACID_ERUPTION_5
				)
				pEffect->SetMulti(true);
		bool bAdd = QueueEffect(std::move(effect));

		if (bAdd)
		{
			// 다음 Effect 생성 정보
			pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );

			//---------------------------------------------
			// 반복되는 frame이면..
			// 시작 frame을 다르게 한다.
			//---------------------------------------------
			if (repeatFrame && maxFrame > 0)
			{
				int num = rand() % maxFrame;

				for (int nf=0; nf<num; nf++)
				{
					pEffect->NextFrame();
				}
			}
		}
		return bAdd;
	}
	return false;
}
