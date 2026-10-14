 //----------------------------------------------------------------------
// MAttachZoneEffectGenerator.cpp
//----------------------------------------------------------------------
// creates effects joined to the tiles.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MAttachZoneEffectGenerator.h"
#include "MEffect.h"
#include "MEffectTarget.h"
#include "MEventQueue.h"
#include "MViewDef.h"
#include "EffectSpriteTypeDef.h"
#include "DebugLog.h"
#include "WorldEventsHost.h"
#include "WorldTileGeometry.h"
#include <cstdlib>
#include <utility>
#include <vector>
//#define	new			DEBUG_NEW

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
//MAttachZoneEffectGenerator	g_StopZoneEffectGenerator;

const MFixedZoneEffectHost* MAttachZoneEffectGenerator::s_pHost = nullptr;

const MFixedZoneEffectHost* MAttachZoneEffectGenerator::SetHost(const MFixedZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

// The sprite of the type, with its frames; none past the table.
bool MAttachZoneEffectGenerator::ReadSprite(int type, MFixedZoneEffectSprite& sprite)
{
	sprite = {};
	return type >= 0 && s_pHost && s_pHost->Sprite && s_pHost->Sprite(static_cast<TYPE_EFFECTSPRITETYPE>(type), sprite);
}

// Consumes the effect; true only when the zone keeps it on the ground.
bool MAttachZoneEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool	MAttachZoneEffectGenerator::MercyGroundEffect( const EFFECTGENERATOR_INFO& egInfo )
{	
	POINT pt ={egInfo.x0, egInfo.y0};

	int est = egInfo.effectSpriteType;		
	
	for(int j=-1;j<=1;j++)
	{
		for(int i=-1;i<=1;i++)
		{
			MFixedZoneEffectSprite sprite;
			if (!ReadSprite( est, sprite ))
			{
				est++;
				continue;
			}
			BLT_TYPE		bltType = static_cast<BLT_TYPE>(sprite.bltType);
			TYPE_FRAMEID	frameID	= sprite.frameID;
			int maxFrame = sprite.maxFrames;
			
			MEffect*	pEffect;
			pEffect = new MEffect(bltType);
			
			pEffect->SetFrameID( frameID, maxFrame );				
			pEffect->SetPixelPosition(
				pt.x , 
				pt.y , 
				egInfo.z0);		// pixel position
			
			pEffect->SetStepPixel(egInfo.step);		// it does not really move, but the next effect takes it.
			pEffect->SetCount( egInfo.count, egInfo.linkCount );			// the frames it lasts
			pEffect->SetDirection( egInfo.direction );			
			pEffect->SetPower(egInfo.power);			
			if (QueueEffect( std::unique_ptr<MEffect>(pEffect) ))
			{
				if( egInfo.effectSpriteType == est )
					pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );				
				else
				{
					MEffectTarget *pTarget = egInfo.pEffectTarget;
					
					if( pTarget != NULL )
					{
						MEffectTarget *pEffectTarget = new MEffectTarget( *pTarget );
						pEffect->SetLink( egInfo.nActionInfo, pEffectTarget );						
					}
				}
			}		
			est++;
		}
	}

	return true;
}

//----------------------------------------------------------------------
// Generate
//----------------------------------------------------------------------
bool
MAttachZoneEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	int est = egInfo.effectSpriteType;
	
	if( est == EFFECTSPRITETYPE_MERCY_GROUND_1 )
	{
		return MercyGroundEffect( egInfo );
	}
	
	// 2001.10.6
	POINT pixelPoint = { egInfo.x0, egInfo.y0 };
	
	
	//----------------------------------------------------------------
	// the kind of blood on the ground at random.. hard-coded.. sigh~
	//----------------------------------------------------------------
	if (est==EFFECTSPRITETYPE_GREEN_BLOOD_GROUND_2_1)
	{
		est = EFFECTSPRITETYPE_GREEN_BLOOD_GROUND_2_1 + rand()%4;
		
		pixelPoint.x += (rand()%TILE_X_HALF) - (TILE_X_HALF>>1);
		pixelPoint.y += (rand()%24) - (24>>1);
	}
	else if (est==EFFECTSPRITETYPE_GREEN_BLOOD_GROUND_1_1)
	{
		est = EFFECTSPRITETYPE_GREEN_BLOOD_GROUND_1_1 + rand()%5;
		
		pixelPoint.x += (rand()%TILE_X_HALF) - (TILE_X_HALF>>1);
		pixelPoint.y += (rand()%24) - (24>>1);
	}
	if (est==EFFECTSPRITETYPE_BLOOD_GROUND_2_1)
	{
		est = EFFECTSPRITETYPE_BLOOD_GROUND_2_1 + rand()%4;
		
		pixelPoint.x += (rand()%TILE_X_HALF) - (TILE_X_HALF>>1);
		pixelPoint.y += (rand()%24) - (24>>1);
	}
	else if (est==EFFECTSPRITETYPE_BLOOD_GROUND_1_1)
	{
		est = EFFECTSPRITETYPE_BLOOD_GROUND_1_1 + rand()%5;
		
		pixelPoint.x += (rand()%TILE_X_HALF) - (TILE_X_HALF>>1);
		pixelPoint.y += (rand()%24) - (24>>1);
	}
	else if( est == EFFECTSPRITETYPE_GREAT_RUFFIAN_1_AXE_GROUND)
	{
		MEvent event;
		event.eventID = EVENTID_METEOR_SHAKE;
		event.eventType = EVENTTYPE_ZONE;
		event.eventDelay = 500;
		event.eventFlag = EVENTFLAG_SHAKE_SCREEN;
		event.parameter3 = 3;
		WorldEvents::AddEvent(event);
	}
	else if(est == EFFECTSPRITETYPE_QUEST_MAGIC_ZONE)
	{
		pixelPoint.x -= 26;
		pixelPoint.y -= 14;
	}
	
	MFixedZoneEffectSprite sprite;
	if (!ReadSprite( est, sprite ))
	{
		return false;
	}
	BLT_TYPE		bltType = static_cast<BLT_TYPE>(sprite.bltType);
	TYPE_FRAMEID	frameID	= sprite.frameID;
	
	//---------------------------------------------
	// read MaxFrame.
	//---------------------------------------------
	int maxFrame = sprite.maxFrames;
	
	// sob.. -.-
	if( egInfo.temp1 == 0 )				// UseActionGrade
	{
		MEffect*	pEffect;
		//---------------------------------------------
		// create the effect
		//---------------------------------------------
		pEffect = new MEffect(bltType);
		
		pEffect->SetFrameID( frameID, maxFrame );	
		
		pEffect->SetPixelPosition(pixelPoint.x, pixelPoint.y, egInfo.z0);		// pixel position
		
		pEffect->SetStepPixel(egInfo.step);		// it does not really move, but the next effect takes it.
		
		pEffect->SetCount( egInfo.count, egInfo.linkCount );			// the frames it lasts
		
		// the direction
		pEffect->SetDirection( egInfo.direction );
		
		// power
		pEffect->SetPower(egInfo.power);
		
		// light brightness
		//pEffect->SetLight( light );
		
		
		// as a ground effect..
		// add to the zone.
		if (QueueEffect( std::unique_ptr<MEffect>(pEffect) ))
		{
			// how the next effect is made
			pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );
			
			return true;
		}
	} else
	{
		// with ActionGrade.
		std::vector<POINT> effectlist;
		POINT pt = { WorldTileGeometry::PixelToTileX( pixelPoint.x ), WorldTileGeometry::PixelToTileY( pixelPoint.y ) };
		
		if( egInfo.temp1 == 1 )
		{
			for( int i=-1;i<=1;i++)
			{
				POINT point = pt;
				point.x += i;
				effectlist.push_back( point );
			}
		} 
		else if( egInfo.temp1 == 2 )
		{
			for( int i=-1;i<=1;i++)
			{
				if( i == 0)
					continue;
				POINT point = pt;
				point.x += i;				
				effectlist.push_back( point );
				point.x -= i;
				point.y += i;
				effectlist.push_back( point );
			}
			effectlist.push_back( pt );
		} 
		else if( egInfo.temp1 == 3 )
		{
			for(int i=-1;i<=1;i++)
			{
				for(int j=-1;j<=1;j++)
				{
					POINT point = pt;
					point.x += i;
					point.y += j;
					effectlist.push_back( point );
				}
			}
		}		
		for(int i=0;static_cast<size_t>(i)<effectlist.size();i++)
		{
			MEffect*	pEffect;
			pEffect = new MEffect(bltType);
			
			pEffect->SetFrameID( frameID, maxFrame );				
			pEffect->SetPixelPosition(WorldTileGeometry::TileToPixelX( effectlist[i].x), WorldTileGeometry::TileToPixelY(effectlist[i].y), egInfo.z0);		// pixel position
			pEffect->SetStepPixel(egInfo.step);		// it does not really move, but the next effect takes it.
			pEffect->SetCount( egInfo.count, egInfo.linkCount );			// the frames it lasts
			pEffect->SetDirection( egInfo.direction );			
			pEffect->SetPower(egInfo.power);			
			// A refused effect is gone; only a kept one is linked (the first
			// to the target, the others to copies).
			const bool bKept = QueueEffect( std::unique_ptr<MEffect>(pEffect) );
			if (bKept && i == 0)
			{
				pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );				
			}  else if (bKept)
			{
				MEffectTarget *pTarget = egInfo.pEffectTarget;
				
				if( pTarget != NULL )
				{
					MEffectTarget *pEffectTarget = new MEffectTarget( *pTarget );
					pEffect->SetLink( egInfo.nActionInfo, pEffectTarget );						
				}
			}
		}
		//		egInfo.temp1= 0;
		return true;
	}
	return false;	
}
