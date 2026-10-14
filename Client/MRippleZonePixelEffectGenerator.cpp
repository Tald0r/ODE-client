//----------------------------------------------------------------------
// MRippleZonePixelEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MRippleZonePixelEffectGenerator.h"
#include "MEffect.h"
#include "MEffectTarget.h"
#include "EffectSpriteTypeDef.h"
#include "UserInformation.h"
#include "WorldTileGeometry.h"
#include "ZoneStateHost.h"
#include <cmath>
#include <cstdlib>
#include <utility>

//#include "DebugInfo.h"
//#define	new			DEBUG_NEW
//#define	delete		DEBUG_DELETE

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
//MRippleZonePixelEffectGenerator	g_RippleZoneEffectGenerator;

const MRippleZoneEffectHost* MRippleZonePixelEffectGenerator::s_pHost = nullptr;

const MRippleZoneEffectHost* MRippleZonePixelEffectGenerator::SetHost(const MRippleZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

// Consumes the effect; true only when the zone keeps it.
bool MRippleZonePixelEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect, bool ground)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect), ground);
}

//----------------------------------------------------------------------
// Generate
//----------------------------------------------------------------------
bool
MRippleZonePixelEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	//------------------------------------------------------------
	// this time the effect is drawn at the start (sx,sy)
	// and next time at (tx,ty).
	// changing the MEffectTarget makes it so.
	//------------------------------------------------------------
	MEffectTarget* pTarget = egInfo.pEffectTarget;

	// the start
	int sx = egInfo.x0;
	int sy = egInfo.y0;

	// the target
	int tx = egInfo.x1; 
	int ty = egInfo.y1;
	int tz = 0;//egInfo.z1;

	int tx2 = tx;
	int ty2 = ty;

	int est = egInfo.effectSpriteType;

	//------------------------------------------------------------
	// hard-coded.. heh
	//------------------------------------------------------------
	bool bPixelStepMove = false;
	bool bGroundEffect = false;
	bool bResetTarget = true;
	int startMoveStep = 0;

	//------------------------------------------------------------
	// Power of Land
	//------------------------------------------------------------
	if (est==EFFECTSPRITETYPE_POWER_OF_LAND_STONE_1)
	{
		// pick one of 1 to 3.
		est = EFFECTSPRITETYPE_POWER_OF_LAND_STONE_2 + rand()%3;
		
		bGroundEffect = true;
		bPixelStepMove = true;
		startMoveStep = 1;
		bResetTarget = false;
	}
	//------------------------------------------------------------
	// Earthquake
	//------------------------------------------------------------
	else if (est==EFFECTSPRITETYPE_EARTHQUAKE_1)
	{
		// pick one of 1 to 3.
		est = EFFECTSPRITETYPE_EARTHQUAKE_1 + rand()%3;
		
		bGroundEffect = true;
		bPixelStepMove = true;
		startMoveStep = 3;
	}
	//------------------------------------------------------------
	// Transfusion
	//------------------------------------------------------------
	else if (est==EFFECTSPRITETYPE_TRANSFUSION_1 || est==EFFECTSPRITETYPE_GREEN_TRANSFUSION_1)
	{
		MCreature* pCreature = ZoneState::GetCreature( egInfo.creatureID );

		int targetX, targetY;

		// if the effect (egInfo.xy0 + size)
		// meets the character (pt + size),
		// stop drawing the effect..
		POINT pt;

 		if (pCreature==NULL)
		{
			targetX = egInfo.x1;
			targetY = egInfo.y1;

//			pt.x = targetX;
//			pt.y = targetY;
		}
		else
		{
			targetX = CreatureState::GetX( pCreature );
			targetY = CreatureState::GetY( pCreature );

		}
		pt.x = WorldTileGeometry::TileToPixelX( targetX );
		pt.y = WorldTileGeometry::TileToPixelY( targetY );

		RECT rectEffect = 
		{
			egInfo.x0 + (24>>1),
			egInfo.y0 + (24>>1),
			egInfo.x0 + 24,
			egInfo.y0 + 24,
		};
		
		RECT rectChar =
		{
			pt.x,
			pt.y,
			pt.x + 24,
			pt.y + 24
		};

		if (rectChar.left <= rectEffect.right
			&& rectChar.right >= rectEffect.left
			&& rectChar.top <= rectEffect.bottom
			&& rectChar.bottom >= rectEffect.top)
		{
			return false;
		}

		// pick one of 1 to 3.
		if(g_pUserInformation->GoreLevel == false)
			est = EFFECTSPRITETYPE_GREEN_TRANSFUSION_1 + rand()%3;
		else
			est = EFFECTSPRITETYPE_TRANSFUSION_1 + rand()%3;

		bGroundEffect = true;
		bPixelStepMove = true;
		startMoveStep = 1;
	}

	if (bPixelStepMove)
	{
		// not to the target.. but a set number of pixels?
		// one step..
		int movePixel = egInfo.step;// * egInfo.count;

		int cx = sx-tx;
		int cy = sy-ty;

		if (bResetTarget && (cx==0 || cy==0))
		{
			//---------------------------------------------
			// the pixel position to the map's.
			//---------------------------------------------
			TYPE_SECTORPOSITION	sX, sY;
			sX = WorldTileGeometry::PixelToTileX( sx );
			sY = WorldTileGeometry::PixelToTileY( sy );

			//---------------------------------------------
			// the next position in the direction it goes.
			//---------------------------------------------
			TYPE_SECTORPOSITION x=sX, y=sY;
			WorldTileGeometry::Step(x,y, egInfo.direction);

			//---------------------------------------------
			// (x,y) back to pixels.
			//---------------------------------------------
			tx = WorldTileGeometry::TileToPixelX( x );
			ty = WorldTileGeometry::TileToPixelY( y );

			// work it out again..
			cx = sx - tx;
			cy = sy - ty;
		}

		int currentPixel = static_cast<int>(sqrt(cx*cx + cy*cy));

		if (currentPixel==0)
		{			
		}
		else
		{
			movePixel = static_cast<int>((float)movePixel * (1.0f - fabs((float)cy / (float)(2.0f*currentPixel))));
		
			if (pTarget!=NULL && pTarget->GetCurrentPhase()==1)
			{
				movePixel *= startMoveStep;
			}

			int movePixel2 = movePixel<<1;
			
			tx = sx - (cx * movePixel / currentPixel);
			ty = sy - (cy * movePixel / currentPixel);

			tx2 = sx - (cx * movePixel2 / currentPixel);
			ty2 = sy - (cy * movePixel2 / currentPixel);
		}
	}

	//-----------------------------------------------------
	// the MEffectTarget's target must be set again.
	//-----------------------------------------------------	
	if (pTarget!=NULL && bResetTarget)
	{
		pTarget->Set(tx2, ty2, tz, pTarget->GetID());		
	}


	// The sprite, with its frames; none past the table.
	MFixedZoneEffectSprite sprite;
	if (est < 0 || s_pHost == NULL || s_pHost->Sprite == NULL || !s_pHost->Sprite( static_cast<TYPE_EFFECTSPRITETYPE>(est), sprite ))
	{
		return false;
	}
	BLT_TYPE		bltType = static_cast<BLT_TYPE>(sprite.bltType);
	TYPE_FRAMEID	frameID	= sprite.frameID;

	int maxFrame = sprite.maxFrames;

	MEffect*	pEffect;
	//---------------------------------------------
	// create the effect
	//---------------------------------------------
	pEffect = new MEffect(bltType);

	pEffect->SetFrameID( frameID, maxFrame );	

	//pEffect->SetPosition(x, y);		// sector position (probably meaningless)

	pEffect->SetPixelPosition(tx, ty, tz);		// pixel position
	
	// the direction
	pEffect->SetDirection( egInfo.direction );

	//pEffect->SetZ( sz );			
	pEffect->SetStepPixel(egInfo.step);		// it does not really move, but the next effect takes it.
	pEffect->SetCount( egInfo.count, egInfo.linkCount );			// the frames it lasts

	// power
	pEffect->SetPower(egInfo.power);

	// light brightness
	//pEffect->SetLight( light );

	// add to the zone, on the ground or not.
	bool bAdd = QueueEffect( std::unique_ptr<MEffect>(pEffect), bGroundEffect );

	if (bAdd)
	{
		// how the next effect is made
		pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );	
	}

	return bAdd;
}
