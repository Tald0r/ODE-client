//----------------------------------------------------------------------
// MAttachCreatureEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MAttachCreatureEffectGenerator.h"
#include "MAttachEffect.h"
#include "ZoneStateHost.h"
#include "MEffectSpriteTypeTable.h"
#include "MItem.h"
#include "DebugLog.h"
#include "EffectSpriteTypeDef.h"
#include "SkillDef.h"
#include "WorldTileGeometry.h"
#include <algorithm>
#include <cstdlib>

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
//MAttachCreatureEffectGenerator	g_AttachCreatureEffectGenerator;

//----------------------------------------------------------------------
// Generate
//----------------------------------------------------------------------
bool
MAttachCreatureEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	int direction = egInfo.direction;

	//-----------------------------------------------------------
	// temporary (-_-;) code for Sword Wave..
	//-----------------------------------------------------------
	/*
	if (frameID==EFFECTSPRITETYPE_SWORD_WAVE_1)
	{
		direction = (direction + 1) % 8;	// +1
	}
	*/
	int			plusZ=0;
	
	if( egInfo.nActionInfo == SKILL_EMISSION_WATER || egInfo.nActionInfo == STEP_SKILL_EMISSION_WATER_2 || egInfo.nActionInfo == STEP_SKILL_EMISSION_WATER_3 )
	{
		plusZ = egInfo.step;		
	}


	MCreature* pCreature = ZoneState::GetCreature( egInfo.creatureID );

	// the creature is gone..
	if (pCreature == NULL)
	{	
		pCreature = ZoneState::GetFakeCreature( egInfo.creatureID );

		if (pCreature==NULL)
		{
			MItem* pItem = ZoneState::GetItem( egInfo.creatureID );

			if (pItem==NULL)
			{
				DEBUG_ADD("AttachCreatureEG - no such Creature");
				return false;
			}

			// The creature lying in a corpse; none for another item.
			pCreature = ZoneState::GetCorpseCreature( egInfo.creatureID );
		}
	}
	TYPE_EFFECTSPRITETYPE EffectType = egInfo.effectSpriteType;
	
	switch( EffectType )
	{
	case EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3 :
		if( pCreature != NULL && ( CreatureState::GetCreatureType( pCreature ) == 104 ||
			(CreatureState::GetCreatureType( pCreature ) >= 111 && CreatureState::GetCreatureType( pCreature ) <= 119) ||
			CreatureState::GetCreatureType( pCreature ) == 358 ||
			CreatureState::GetCreatureType( pCreature ) == 359 ||
			CreatureState::GetCreatureType( pCreature ) == 467 ||
			CreatureState::GetCreatureType( pCreature ) == 582 ) )
			EffectType = EFFECTSPRITETYPE_JABBING_VEIN_FRONT_GOLEMER;
		else if( pCreature != NULL && (CreatureState::GetCreatureType( pCreature ) == 106 ||
			(CreatureState::GetCreatureType( pCreature ) >= 129 && CreatureState::GetCreatureType( pCreature ) <= 137) ) )
			EffectType = EFFECTSPRITETYPE_JABBING_VEIN_FRONT_ALCAN;
		
		break;
	case EFFECTSPRITETYPE_JABBING_VEIN_REAR_3 :
		if( pCreature != NULL && ( CreatureState::GetCreatureType( pCreature ) == 104 ||
			(CreatureState::GetCreatureType( pCreature ) >= 111 && CreatureState::GetCreatureType( pCreature ) <= 119) ||
			CreatureState::GetCreatureType( pCreature ) == 358 ||
			CreatureState::GetCreatureType( pCreature ) == 359 ||
			CreatureState::GetCreatureType( pCreature ) == 467 ||
			CreatureState::GetCreatureType( pCreature ) == 582 ) )
			EffectType = EFFECTSPRITETYPE_JABBING_VEIN_REAR_GOLEMER;
		else if( pCreature != NULL && (CreatureState::GetCreatureType( pCreature ) == 106 ||
			(CreatureState::GetCreatureType( pCreature ) >= 129 && CreatureState::GetCreatureType( pCreature ) <= 137) ) )
			EffectType = EFFECTSPRITETYPE_JABBING_VEIN_REAR_ALCAN;
		break;
	case EFFECTSPRITETYPE_TRIDENT_BOMB_1 :
		EffectType = EFFECTSPRITETYPE_TRIDENT_BOMB_1 + (rand()%10);
		break;
	case EFFECTSPRITETYPE_PIERCING_FRONT :
		// ToT the direction is.. -.- reversed~
		direction = (direction+4)%8;
		break;
	case EFFECTSPRITETYPE_QUICK_FIRE_BOMB_1 :
		EffectType = EFFECTSPRITETYPE_QUICK_FIRE_BOMB_1 + (rand()%5);
		break;
	case EFFECTSPRITETYPE_STONE_SKIN_DAMAGED_1 :
		EffectType = EFFECTSPRITETYPE_STONE_SKIN_DAMAGED_1 + rand()%3;
		break;
		
	case EFFECTSPRITETYPE_DISTANCE_BLITZ_12:
		{
			int sx = WorldTileGeometry::PixelToTileX(egInfo.x0);
			int sy = WorldTileGeometry::PixelToTileY(egInfo.y0);
			int dx = WorldTileGeometry::PixelToTileX(egInfo.x1);
			int dy = WorldTileGeometry::PixelToTileY(egInfo.y1);
			EffectType += (std::min)(6, (std::max)(2, (std::max)(std::abs(dx-sx), std::abs(dy-sy))))-2;
		}
		break;
	}

	if(pCreature!= NULL && CreatureState::GetCreatureType( pCreature ) == 185 &&
		EffectType >=EFFECTSPRITETYPE_JABBING_VEIN_FRONT_1 && EffectType <= EFFECTSPRITETYPE_JABBING_VEIN_FRONT_BAT)
			EffectType = EFFECTSPRITETYPE_JABBING_VEIN_FRONT_BAT;

	if(pCreature!= NULL && CreatureState::GetCreatureType( pCreature ) == 185 &&
		EffectType >=EFFECTSPRITETYPE_JABBING_VEIN_REAR_1 && EffectType <= EFFECTSPRITETYPE_JABBING_VEIN_REAR_BAT)
			EffectType = EFFECTSPRITETYPE_JABBING_VEIN_REAR_BAT;
	
	// create the effect attached to the creature and take its pointer.
	MAttachEffect* pEffect = CreatureState::CreateAttachEffect( pCreature, EffectType, egInfo.count, egInfo.linkCount, false, MEffect::EFFECT_ATTACH );

	if (pEffect == NULL)
	{
		DEBUG_ADD_WAR("can't CreateAttachEffect");
		return false;
	}
	
	pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );
	pEffect->SetZ( plusZ );

	// the character it attaches to
	//pEffect->SetAttachCreatureID( creatureID );		

	// power
	pEffect->SetPower(egInfo.power);

	// light brightness
	//pEffect->SetLight( light );
	pEffect->SetDirection( direction );
	
	return true;
}
