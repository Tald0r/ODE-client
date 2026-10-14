//----------------------------------------------------------------------
// MAttachCreatureEffectGroundGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MAttachCreatureEffectGroundGenerator.h"
#include "MAttachEffect.h"
#include "ZoneStateHost.h"
#include "EffectSpriteTypeDef.h"
#include "MEffectSpriteTypeTable.h"

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
//MAttachCreatureEffectGroundGenerator	g_AttachCreatureEffectGenerator;

//----------------------------------------------------------------------
// Generate
//----------------------------------------------------------------------
bool
MAttachCreatureEffectGroundGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	int est = egInfo.effectSpriteType;

	int direction = egInfo.direction;

	//-----------------------------------------------------------
	// temporary (-_-;) code for Sword Wave..
	//-----------------------------------------------------------
	if (est==EFFECTSPRITETYPE_SWORD_WAVE_1)
	{
		direction = (direction + 7) % 8;	// +8 - 1
	}
	

	MCreature* pCreature = ZoneState::GetCreature( egInfo.creatureID );

	// the creature is gone..
	if (pCreature == NULL)
	{	
		pCreature = ZoneState::GetFakeCreature( egInfo.creatureID );

		if (pCreature==NULL)
		{
			return false;
		}
	}

	// create the effect attached to the creature and take its pointer.
	MAttachEffect* pEffect = CreatureState::CreateAttachEffect( pCreature,
											egInfo.effectSpriteType, 
											egInfo.count, 
											egInfo.linkCount, 
											true,	// attached to the ground.
											MEffect::EFFECT_ATTACH );

	if (pEffect == NULL)
	{
		return false;
	}
	

	pEffect->SetDirection( direction );

	pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );

	// the character it attaches to
	//pEffect->SetAttachCreatureID( creatureID );		

	// power
	pEffect->SetPower(egInfo.power);

	// light brightness
	//pEffect->SetLight( light );
	
	return true;
}
