 //----------------------------------------------------------------------
// MSkipEffectGenerator.cpp
//----------------------------------------------------------------------
// creates blinking effects joined to the tiles.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MPlayingWithFireEffectGenerator.h"
#include "MEffect.h"
#include "MEffectTarget.h"
#include "MEventQueue.h"
#include "MViewDef.h"
#include "EffectSpriteTypeDef.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include <cstdlib>
#include <utility>

//#define	new			DEBUG_NEW

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
//MAttachZoneEffectGenerator	g_StopZoneEffectGenerator;

const MZoneAttackEffectHost* MPlayingWithFireEffectGenerator::s_pHost = nullptr;

const MZoneAttackEffectHost* MPlayingWithFireEffectGenerator::SetHost(const MZoneAttackEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

// The sprite of the type, with its frames; none past the table.
bool MPlayingWithFireEffectGenerator::ReadSprite(int type, MZoneAttackEffectSprite& sprite)
{
	sprite = {};
	return type >= 0 && s_pHost && s_pHost->Sprite && s_pHost->Sprite(static_cast<TYPE_EFFECTSPRITETYPE>(type), sprite);
}

// Consumes the effect; true only when the zone keeps it.
bool MPlayingWithFireEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

//----------------------------------------------------------------------
// Generate
//----------------------------------------------------------------------
bool
MPlayingWithFireEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	int est[16] = { egInfo.effectSpriteType };
	int effectCount = 1;

	int x = egInfo.x0-TILE_X/2, y = egInfo.y0+TILE_Y/2;

	if( egInfo.pPreviousEffect != NULL )
	{
		
		est[0] = egInfo.pPreviousEffect->GetEst();
		
		m_EffectCount ++;
		if(m_EffectCount>112)
		{
			if( est[0] > EFFECTSPRITETYPE_FIRE_16 )
				est[0] -= (rand()%2)*16;
			else
				est[0] += (rand()%2)*16;
		}
		else
		{
			est[0]++;
			if(est[0]>EFFECTSPRITETYPE_FIRE_32)
				est[0] = EFFECTSPRITETYPE_FIRE_1;
		}
		switch( egInfo.pPreviousEffect->GetEst() )
		{
		case EFFECTSPRITETYPE_FIRE_1:
		case EFFECTSPRITETYPE_FIRE_17:
			x = egInfo.pPreviousEffect->GetPixelX();
			y = egInfo.pPreviousEffect->GetPixelY()+TILE_Y;
			break;

		case EFFECTSPRITETYPE_FIRE_2:
		case EFFECTSPRITETYPE_FIRE_18:
			x = egInfo.pPreviousEffect->GetPixelX()-TILE_X/2;
			y = egInfo.pPreviousEffect->GetPixelY()+TILE_Y;
			break;

		case EFFECTSPRITETYPE_FIRE_3:
		case EFFECTSPRITETYPE_FIRE_19:
			x = egInfo.pPreviousEffect->GetPixelX()-TILE_X;
			y = egInfo.pPreviousEffect->GetPixelY()+TILE_Y;
			break;

		case EFFECTSPRITETYPE_FIRE_4:
		case EFFECTSPRITETYPE_FIRE_20:
			x = egInfo.pPreviousEffect->GetPixelX()-TILE_X;
			y = egInfo.pPreviousEffect->GetPixelY()+TILE_Y/2;
			break;

		case EFFECTSPRITETYPE_FIRE_5:
		case EFFECTSPRITETYPE_FIRE_21:
			x = egInfo.pPreviousEffect->GetPixelX()-TILE_X;
			y = egInfo.pPreviousEffect->GetPixelY();
			break;
			
		case EFFECTSPRITETYPE_FIRE_6:
		case EFFECTSPRITETYPE_FIRE_22:
			x = egInfo.pPreviousEffect->GetPixelX()-TILE_X;
			y = egInfo.pPreviousEffect->GetPixelY()-TILE_Y/2;
			break;
			
		case EFFECTSPRITETYPE_FIRE_7:
		case EFFECTSPRITETYPE_FIRE_23:
			x = egInfo.pPreviousEffect->GetPixelX()-TILE_X;
			y = egInfo.pPreviousEffect->GetPixelY()-TILE_Y;
			break;
			
		case EFFECTSPRITETYPE_FIRE_8:
		case EFFECTSPRITETYPE_FIRE_24:
			x = egInfo.pPreviousEffect->GetPixelX()-TILE_X/2;
			y = egInfo.pPreviousEffect->GetPixelY()-TILE_Y;
			break;
			
		case EFFECTSPRITETYPE_FIRE_9:
		case EFFECTSPRITETYPE_FIRE_25:
			x = egInfo.pPreviousEffect->GetPixelX();
			y = egInfo.pPreviousEffect->GetPixelY()-TILE_Y;
			break;
			
		case EFFECTSPRITETYPE_FIRE_10:
		case EFFECTSPRITETYPE_FIRE_26:
			x = egInfo.pPreviousEffect->GetPixelX()+TILE_X/2;
			y = egInfo.pPreviousEffect->GetPixelY()-TILE_Y;
			break;
			
		case EFFECTSPRITETYPE_FIRE_11:
		case EFFECTSPRITETYPE_FIRE_27:
			x = egInfo.pPreviousEffect->GetPixelX()+TILE_X;
			y = egInfo.pPreviousEffect->GetPixelY()-TILE_Y;
			break;
			
		case EFFECTSPRITETYPE_FIRE_12:
		case EFFECTSPRITETYPE_FIRE_28:
			x = egInfo.pPreviousEffect->GetPixelX()+TILE_X;
			y = egInfo.pPreviousEffect->GetPixelY()-TILE_Y/2;
			break;
			
		case EFFECTSPRITETYPE_FIRE_13:
		case EFFECTSPRITETYPE_FIRE_29:
			x = egInfo.pPreviousEffect->GetPixelX()+TILE_X;
			y = egInfo.pPreviousEffect->GetPixelY();
			break;
			
		case EFFECTSPRITETYPE_FIRE_14:
		case EFFECTSPRITETYPE_FIRE_30:
			x = egInfo.pPreviousEffect->GetPixelX()+TILE_X;
			y = egInfo.pPreviousEffect->GetPixelY()+TILE_Y/2;
			break;
			
		case EFFECTSPRITETYPE_FIRE_15:
		case EFFECTSPRITETYPE_FIRE_31:
			x = egInfo.pPreviousEffect->GetPixelX()+TILE_X;
			y = egInfo.pPreviousEffect->GetPixelY()+TILE_Y;
			break;
			
		case EFFECTSPRITETYPE_FIRE_16:
		case EFFECTSPRITETYPE_FIRE_32:
			x = egInfo.pPreviousEffect->GetPixelX()+TILE_X/2;
			y = egInfo.pPreviousEffect->GetPixelY()+TILE_Y;
			break;
			
		}
	}
	else
	{
		m_EffectCount = 0;
		switch( egInfo.direction )
		{
		case DIRECTION_LEFT:
			x -= TILE_X/2;
			est[0] = EFFECTSPRITETYPE_FIRE_5+(rand()%2)*16;
			break;

		case DIRECTION_LEFTDOWN:
			x -= TILE_X/2;
			y += TILE_Y/2;
			est[0] = EFFECTSPRITETYPE_FIRE_3+(rand()%2)*16;
			break;
			
		case DIRECTION_DOWN:
			y += TILE_Y/2;
			est[0] = EFFECTSPRITETYPE_FIRE_1+(rand()%2)*16;
			break;
			
		case DIRECTION_RIGHTDOWN:
			x += TILE_X/2;
			y += TILE_Y/2;
			est[0] = EFFECTSPRITETYPE_FIRE_15+(rand()%2)*16;
			break;
			
		case DIRECTION_RIGHT:
			x += TILE_X/2;
			est[0] = EFFECTSPRITETYPE_FIRE_13+(rand()%2)*16;
			break;
			
		case DIRECTION_RIGHTUP:
			x += TILE_X/2;
			y -= TILE_Y/2;
			est[0] = EFFECTSPRITETYPE_FIRE_11+(rand()%2)*16;
			break;
			
		case DIRECTION_UP:
			y -= TILE_Y/2;
			est[0] = EFFECTSPRITETYPE_FIRE_9+(rand()%2)*16;
			break;
			
		case DIRECTION_LEFTUP:
			x -= TILE_X/2;
			y -= TILE_Y/2;
			est[0] = EFFECTSPRITETYPE_FIRE_7+(rand()%2)*16;
			break;
			
		}
//		est[0] = EFFECTSPRITETYPE_FIRE_1 + ((egInfo.direction+6)%8)*2 + (rand()%2)*16;
		est[1] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+15)%16+(rand()%2)*16;
		est[2] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+1)%16+(rand()%2)*16;

		est[3] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+2)%16+(rand()%2)*16;
		est[4] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+3)%16+(rand()%2)*16;
		est[5] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+4)%16+(rand()%2)*16;
		est[6] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+5)%16+(rand()%2)*16;
		est[7] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+6)%16+(rand()%2)*16;
		est[8] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+7)%16+(rand()%2)*16;
		est[9] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+8)%16+(rand()%2)*16;
		est[10] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+9)%16+(rand()%2)*16;
		est[11] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+10)%16+(rand()%2)*16;
		est[12] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+11)%16+(rand()%2)*16;
		est[13] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+12)%16+(rand()%2)*16;
		est[14] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+13)%16+(rand()%2)*16;
		est[15] = EFFECTSPRITETYPE_FIRE_1+(est[0]-EFFECTSPRITETYPE_FIRE_1+14)%16+(rand()%2)*16;
		effectCount = 16;

		MEvent event;
		event.eventID = EVENTID_METEOR_SHAKE;
		event.eventType = EVENTTYPE_ZONE;
		event.eventDelay = 2000;
		event.eventFlag = EVENTFLAG_SHAKE_SCREEN;
		event.parameter3 = 3;
		WorldEvents::AddEvent(event);

	}

	// The first sprite type must be in the table.
	MZoneAttackEffectSprite firstSprite;
	if (!ReadSprite( est[0], firstSprite ))
	{
		return false;
	}

	
	int direction = egInfo.direction;


	// add to the zone.

	bool bAdd = false;
	MEffectTarget*	pEffectTarget2;

	for(int i = 0; i < effectCount; i++)
	{
		MZoneAttackEffectSprite sprite;
		if (!ReadSprite( est[i], sprite ))
		{
			continue;
		}
		BLT_TYPE		bltType = static_cast<BLT_TYPE>(sprite.bltType);
		TYPE_FRAMEID	frameID	= sprite.frameID;
		
		int maxFrame = sprite.maxFrames;
		
		MEffect*	pEffect;
		//---------------------------------------------
		// create the effect
		//---------------------------------------------
		pEffect = new MEffect(bltType);
		
		pEffect->SetEst( est[i] );
		pEffect->SetFrameID( frameID, maxFrame );	
		pEffect->SetPixelPosition( x, y, egInfo.z0 );
//		pEffect->SetZ(egInfo.z0);			
		pEffect->SetStepPixel(egInfo.step);		// it does not really move, but the next effect takes it.
		pEffect->SetCount( egInfo.count, egInfo.linkCount );			// the frames it lasts
		
//		pEffect->SetDelayFrame( i*8 );
		
		// the direction
		pEffect->SetDirection( direction );
		
		// power
		pEffect->SetPower(egInfo.power);
		
		// light brightness
		//pEffect->SetLight( light );
		
		// can it overlap
		pEffect->SetMulti(true);

		if(QueueEffect( std::unique_ptr<MEffect>(pEffect)))
		{
			if (!bAdd)
			{
				pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );
				bAdd = true;
			}
			else
			{
				// how the next effect is made
				if (egInfo.pEffectTarget == NULL)
				{
					pEffect->SetLink( egInfo.nActionInfo, NULL );
				}
				else
				{
					pEffectTarget2 = new MEffectTarget(*egInfo.pEffectTarget);
					pEffect->SetLink( egInfo.nActionInfo, pEffectTarget2 );
					pEffectTarget2->Set( x, y, egInfo.z0, egInfo.creatureID );
				}
			}
		}
//		if (bAdd)
//		{
//			// how the next effect is made
//			pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );
//		
//		}
	}
	return true;
}
