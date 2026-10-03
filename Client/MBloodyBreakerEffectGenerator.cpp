//----------------------------------------------------------------------
// MBloodyBreakerEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MBloodyBreakerEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"
#include "WorldTileGeometry.h"
#include <utility>
#include <vector>

const MBloodyBreakerEffectHost* MBloodyBreakerEffectGenerator::s_pHost = nullptr;

const MBloodyBreakerEffectHost* MBloodyBreakerEffectGenerator::SetHost(const MBloodyBreakerEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MBloodyBreakerEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MBloodyBreakerEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MBloodyBreakerEffectGenerator::ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count)
{
	count = 0;
	return s_pHost && s_pHost->MaxFrames && s_pHost->MaxFrames(blt, frameID, count);
}

bool MBloodyBreakerEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

char bloody_breaker_map[8][13][13] = { { {0, } } };

struct tempBreaker
{
	int x;
	int y;
	BYTE center;
};

void	MakeMap(BYTE dic, std::vector<tempBreaker> &v_cp, int p)
{
	const char width[7] = { 0, 0, 1, 1, 2, 2, 2 };
	
    POINT mask[8];
    mask[0].x = -1;
    mask[0].y =  0;
    mask[1].x = -1;
    mask[1].y =  1;
    mask[2].x =  0;
    mask[2].y =  1;
    mask[3].x =  1;
    mask[3].y =  1;
    mask[4].x =  1;
    mask[4].y =  0;
    mask[5].x =  1;
    mask[5].y = -1;
    mask[6].x =  0;
    mask[6].y = -1;
    mask[7].x = -1;
    mask[7].y = -1;	
    
	for ( int i = 1; i <= 6; i++ )
	{
		int x = 0;
		int y = 0;
		
		for ( int j = 0; j <= width[i]; j++ )
		{
			x = mask[dic].x * i;
			y = mask[dic].y * i;				
			
			
			if ( j == 0)
			{
				if( p == i )
				{
					tempBreaker pt= { x,y,1};
					v_cp.push_back(pt);
				}
			}
			else
			{
				int left  = ( (dic&0x1) == 0 ? (( dic + 2 )&0x7) : (( dic + 3 )&0x7) );
				int right = ( (dic&0x1) == 0 ? (( dic + 6 )&0x7) : (( dic + 5 )&0x7) );
				
				int xl = x + ( mask[left].x * j );
				int yl = y + ( mask[left].y * j );
				
				int xr = x + ( mask[right].x * j );
				int yr = y + ( mask[right].y * j );
				
				if( p == i )
				{
					tempBreaker pt = { xl,yl,0};					
					v_cp.push_back(pt);
					pt.x = xr;
					pt.y = yr;
					v_cp.push_back(pt);
				}
			}
		}
	}
}


bool
MBloodyBreakerEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	int est = egInfo.effectSpriteType;
	
	MBloodyBreakerEffectSprite sprite;
	if (!ReadSprite(static_cast<TYPE_EFFECTSPRITETYPE>(est), sprite)) return false;
	const BYTE bltType = sprite.bltType;
	TYPE_FRAMEID frameID = sprite.frameID;
	int maxFrame;
	if (!ReadMaxFrames(bltType, frameID, maxFrame)) return false;

	int currentPhase = egInfo.pEffectTarget != NULL ?egInfo.pEffectTarget->GetCurrentPhase() : -1;

	unsigned char currentDirection = egInfo.direction;
	std::vector<tempBreaker> v_cp;

	MakeMap(currentDirection, v_cp, currentPhase );

	if(v_cp.empty())
		return false;

	TYPE_SECTORPOSITION	tX, tY;
	tX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x0));
	tY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y0));
	bool targetTransferred = false;

	
	for(int i=0;static_cast<size_t>(i)<v_cp.size();i++)
	{
		auto effect = std::make_unique<MEffect>(bltType);
		MEffect* pEffect = effect.get();

		pEffect->SetFrameID( frameID, static_cast<BYTE>(maxFrame) );
		int tempy =0, tempx =0;
		if(currentPhase != -1)
		{
			if(currentDirection == 7 || currentDirection == 6 || currentDirection == 5)
				tempy+=(currentPhase-1);
			if(currentDirection == 1 || currentDirection == 2 || currentDirection == 3)
				tempy-=(currentPhase-1);
			if(currentDirection == 0 || currentDirection == 1 || currentDirection == 7)
				tempx+=(currentPhase-1);
			if(currentDirection == 5 || currentDirection == 4 || currentDirection == 3)
				tempx-=(currentPhase-1);
		}
		pEffect->SetPosition(static_cast<TYPE_SECTORPOSITION>(v_cp[i].x+tX+tempx),
			static_cast<TYPE_SECTORPOSITION>(v_cp[i].y+tY+tempy));
		pEffect->SetZ(egInfo.z0);
		pEffect->SetStepPixel(egInfo.step);
		pEffect->SetCount(egInfo.count, egInfo.linkCount);
		pEffect->SetDirection( egInfo.direction );
		pEffect->SetPower(egInfo.power);
		pEffect->SetMulti( true );

		if(QueueEffect(std::move(effect)))
		{
			if(v_cp[i].center == 1 && egInfo.pEffectTarget != NULL) 
			{			
				pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );
				targetTransferred = true;
				
			} else
			{
				pEffect->SetLink( egInfo.nActionInfo, NULL );
			}
		}
		if (est>=EFFECTSPRITETYPE_BLOODY_WALL_1
			&& est<=EFFECTSPRITETYPE_BLOODY_WALL_3)
		{
			if (++est > EFFECTSPRITETYPE_BLOODY_WALL_3)
			{
				est = EFFECTSPRITETYPE_BLOODY_WALL_1;
			}
		}
		// Refresh after every attempt, including the last, retaining the initial blit type.
		if (!ReadSprite(static_cast<TYPE_EFFECTSPRITETYPE>(est), sprite)) return targetTransferred;
		frameID = sprite.frameID;
		if (!ReadMaxFrames(bltType, frameID, maxFrame)) return targetTransferred;
	}
	
	return true;
}
