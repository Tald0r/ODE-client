//----------------------------------------------------------------------
// MRisingEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MRisingEffectGenerator.h"
#include "MLinearEffect.h"
#include "SkillDef.h"
#include <cmath>
#include <utility>

#define PI 3.141592f

const MRisingEffectHost* MRisingEffectGenerator::s_pHost = nullptr;

const MRisingEffectHost* MRisingEffectGenerator::SetHost(const MRisingEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MRisingEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MRisingEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MRisingEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MRisingEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	MRisingEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const BYTE bltType = sprite.bltType;
	const TYPE_FRAMEID frameID = sprite.frameID;
	const BYTE maxFrame = static_cast<BYTE>(sprite.maxFrames);
	MEffectTarget* pTarget = egInfo.pEffectTarget;
	bool generated = false;

	if((egInfo.nActionInfo >= SKILL_FIRE_CRACKER_VOLLEY_1 &&
		egInfo.nActionInfo <= SKILL_FIRE_CRACKER_WIDE_VOLLEY_4) ||
		egInfo.nActionInfo ==SKILL_DRAGON_FIRE_CRACKER)
	{
		// Three-shot volley.
		int angle = 18;
		int i;
		int tx[3],tz[3],step[3];
		int coord_z = egInfo.step * egInfo.count;

		for(i=0;i<3;i++)
		{
			tx[i] = egInfo.x0;
			tz[i] = egInfo.z0 + coord_z;
			step[i] = egInfo.step;
		}

		float Radian = float( angle ) * ( PI / 180.0f );
		float sinValue = float(coord_z) * sinf( Radian );
		float cosValue = float(coord_z) * cosf( Radian );
		int step_count = static_cast<int>(sqrt(int(sinValue * sinValue)+ int(cosValue)*int(cosValue)) / egInfo.count);

		tx[0] = egInfo.x0 + int(sinValue);
		tz[0] = egInfo.z0 + int(cosValue);
		step[0] = step_count;
		tx[2] = egInfo.x0 - int(sinValue);
		tz[2] = egInfo.z0 + int(cosValue);
		step[2] = step_count;

		for(i=0;i<3;i++)
		{
			auto effect = std::make_unique<MLinearEffect>(bltType);
			MLinearEffect* pEffect = effect.get();

			pEffect->SetFrameID( frameID, maxFrame );
			pEffect->SetPixelPosition( egInfo.x0, egInfo.y0, egInfo.z0 );
			pEffect->SetDirection( 2 );
			pEffect->SetTarget( tx[i], egInfo.y0, tz[i], step[i] );
			pEffect->SetCount( egInfo.count, egInfo.linkCount );
			pEffect->SetPower( egInfo.power );

			if(QueueEffect(std::move(effect)) )
			{
				if(pTarget == NULL )
				{
					pEffect->SetLink( egInfo.nActionInfo, NULL );
					generated = true;
				} else
				{
					if( i == 1 )
					{
						pEffect->SetLink( egInfo.nActionInfo, pTarget );
						pTarget->Set( tx[i], egInfo.y0, tz[i], egInfo.creatureID );
						generated = true;
					} else
					{
						MEffectTarget *pEffectTarget = new MEffectTarget( *pTarget );
						pEffect->SetLink( egInfo.nActionInfo, pEffectTarget );
						pEffectTarget->Set( tx[i], egInfo.y0, tz[i], egInfo.creatureID );
					}
				}
			}
		}
		return generated;
	}
	else
	if(egInfo.nActionInfo == SKILL_FIRE_CRACKER_STORM)
	{
		int angle1 = 10, angle2 = 30;
		int i;
		int tx[4],tz[4],step[4];
		int coord_z = egInfo.step * egInfo.count;

		for(i=0;i<4;i++)
		{
			tx[i] = egInfo.x0;
			tz[i] = egInfo.z0 + coord_z;
			step[i] = egInfo.step;
		}

		float Radian = float( angle1 ) * ( PI / 180.0f );
		float sinValue = float(coord_z) * sinf( Radian );
		float cosValue = float(coord_z) * cosf( Radian );
		int step_count = static_cast<int>(sqrt(int(sinValue * sinValue)+ int(cosValue)*int(cosValue)) / egInfo.count);

		tx[0] = egInfo.x0 + int(sinValue);
		tz[0] = egInfo.z0 + int(cosValue);
		step[0] = step_count;
		tx[3] = egInfo.x0 - int(sinValue);
		tz[3] = egInfo.z0 + int(cosValue);
		step[3] = step_count;

		Radian = float( angle2 ) * (PI / 180.0f);
		sinValue = float(coord_z) * sinf( Radian );
		cosValue = float(coord_z) * cosf( Radian );
		step_count = static_cast<int>(sqrt(int(sinValue * sinValue) + int(cosValue)*int(cosValue)) / egInfo.count);

		tx[1] = egInfo.x0 + int(sinValue);
		tz[1] = egInfo.z0 + int(cosValue);
		tx[2] = egInfo.x0 - int(sinValue);
		tz[2] = egInfo.z0 + int(cosValue);
		step[1] = step[2] = step_count;

		for(i=0;i<4;i++)
		{
			auto effect = std::make_unique<MLinearEffect>(bltType);
			MLinearEffect* pEffect = effect.get();
			pEffect->SetFrameID( frameID, maxFrame );
			pEffect->SetPixelPosition( egInfo.x0, egInfo.y0, egInfo.z0 );
			pEffect->SetDirection( 2 );
			pEffect->SetTarget( tx[i], egInfo.y0, tz[i], step[i] );
			pEffect->SetCount( egInfo.count, egInfo.linkCount );
			pEffect->SetPower( egInfo.power );

			if(QueueEffect(std::move(effect)) )
			{
				if(pTarget == NULL )
				{
					pEffect->SetLink( egInfo.nActionInfo, NULL );
					generated = true;
				} else
				{
					if( i == 1 )
					{
						pEffect->SetLink( egInfo.nActionInfo, pTarget );
						pTarget->Set( tx[i], egInfo.y0, tz[i], egInfo.creatureID );
						generated = true;
					} else
					{
						MEffectTarget *pEffectTarget = new MEffectTarget( *pTarget );
						pEffect->SetLink( egInfo.nActionInfo, pEffectTarget );
						pEffectTarget->Set( tx[i], egInfo.y0, tz[i], egInfo.creatureID );
					}
				}
			}
		}
		return generated;
	}
	else
	{
		auto effect = std::make_unique<MLinearEffect>(bltType);
		MLinearEffect* pEffect = effect.get();

		pEffect->SetFrameID( frameID, maxFrame );
		// Begin at the source pixel position.
		pEffect->SetPixelPosition( egInfo.x0, egInfo.y0, egInfo.z0 );
		// Linear target selection computes the final facing.
		pEffect->SetDirection( egInfo.direction );
		// Rise by speed times duration.
		pEffect->SetTarget( egInfo.x0, egInfo.y0, egInfo.z0+egInfo.step*egInfo.count, egInfo.step );

		// Keep finite lifetime and an independent link deadline.
		pEffect->SetCount( egInfo.count, egInfo.linkCount );

		// Preserve power.
		pEffect->SetPower(egInfo.power);

		if (QueueEffect(std::move(effect)))
		{
			pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );

			return true;
		}
	}

	return false;
}
