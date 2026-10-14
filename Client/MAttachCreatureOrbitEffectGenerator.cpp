//----------------------------------------------------------------------
// MAttachCreatureOrbitEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MAttachCreatureOrbitEffectGenerator.h"
#include "MAttachEffect.h"
#include "ZoneStateHost.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MItem.h"
#include "DebugLog.h"
#include "EffectSpriteTypeDef.h"
#include "MAttachOrbitEffect.h"
#include "OrbitEffectPolicy.h"

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
//MAttachCreatureOrbitEffectGenerator	g_AttachCreatureEffectGenerator;

//----------------------------------------------------------------------
// Generate
//----------------------------------------------------------------------
bool
MAttachCreatureOrbitEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	int direction = egInfo.direction;


	MCreature* pCreature = ZoneState::GetCreature( egInfo.creatureID );

	// the creature is gone..
	if (pCreature == NULL)
	{	
		pCreature = ZoneState::GetFakeCreature( egInfo.creatureID );

		if (pCreature==NULL)
		{			
			DEBUG_ADD("AttachCreatureEG - no such Creature");
			return false;		
		}
	}

	int effectPosition = RandomIndex(rand(), MAX_EFFECT_ORBIT_STEP);

	if( ConsultsPreviousOrbitStep(egInfo.effectSpriteType, CreatureState::IsExistAttachEffect( pCreature )) )
	{
		MAttachEffect* pOldEffect = CreatureState::FirstAttachEffect( pCreature );
		if( pOldEffect != NULL && pOldEffect->GetEffectType() == MEffect::EFFECT_ATTACH_ORBIT )
		{
			effectPosition = static_cast<MAttachOrbitEffect*>(pOldEffect)->GetOrbitStep();
		}
	}
	if( ClearsBeforeAttach(egInfo.effectSpriteType) )
		CreatureState::ClearAttachEffect( pCreature );

	// create the effect attached to the creature and take its pointer.
	MAttachOrbitEffect* pEffect = (MAttachOrbitEffect*)CreatureState::CreateAttachEffect( pCreature, egInfo.effectSpriteType,
															egInfo.count, 
															egInfo.linkCount,
															false,	// ground
															MEffect::EFFECT_ATTACH_ORBIT);

	if (pEffect == NULL)
	{
		DEBUG_ADD_WAR("can't CreateAttachEffect");
		return false;
	}
	

	pEffect->SetDirection( direction );

	pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );

	// the character it attaches to
	//pEffect->SetAttachCreatureID( creatureID );		

	// power
	pEffect->SetPower(egInfo.power);
	pEffect->SetOrbitStep(effectPosition);
	if(egInfo.effectSpriteType == EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL_ATTACK 
		|| egInfo.effectSpriteType == EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL_HEAL)
	{
		pEffect->SetOrbitRunning(false);
	}


	// light brightness
	//pEffect->SetLight( light );
	
	return true;
}
