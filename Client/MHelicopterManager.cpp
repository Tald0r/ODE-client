//----------------------------------------------------------------------
// MHelicopterManager.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "DebugLog.h"
#include "MHelicopterManager.h"
#include "MChaseEffect.h"
#include "EffectSpriteTypeDef.h"
#include "SoundDef.h"
#include "MEffectSpriteTypeTable.h"
#include "WorldTileGeometry.h"

//----------------------------------------------------------------------
// define
//----------------------------------------------------------------------
#define PIXEL_HELICOPTER_MOVE		20

namespace {
const MHelicopterHost* s_pHost = NULL;

MEffect* ZoneEffect(TYPE_OBJECTID id)
{
	return s_pHost != NULL ? s_pHost->GetEffect(id) : NULL;
}
}

const MHelicopterHost*
MHelicopter::SetHost(const MHelicopterHost* pHost)
{
	const MHelicopterHost* pOld = s_pHost;
	s_pHost = pHost;
	return pOld;
}

//----------------------------------------------------------------------
//
//						MHelicopter
//
//----------------------------------------------------------------------
MHelicopter::MHelicopter()
{
	m_CreatureID = OBJECTID_NULL;
	m_EffectID = OBJECTID_NULL;
	m_pSound = NULL;
}

MHelicopter::~MHelicopter()
{
	Release();
}

//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
MHelicopter::Release()
{
	// Its shadow leaves the zone, and its sound stops.
	if (s_pHost != NULL)
	{
		s_pHost->RemoveEffect( m_EffectID );
	}

	if (m_pSound!=NULL)
	{
		if (s_pHost != NULL)
		{
			s_pHost->DeleteSound( m_pSound );
		}
		m_pSound = NULL;
	}

	m_CreatureID = OBJECTID_NULL;
	m_EffectID = OBJECTID_NULL;
}

//----------------------------------------------------------------------
// Set Chase Creature
//----------------------------------------------------------------------
// The helicopter's shadow comes in from 500 pixels right and 300 up of
// the creature and chases it; its propeller loops at (x0, y0).
//----------------------------------------------------------------------
bool
MHelicopter::SetChaseCreature(TYPE_OBJECTID id, int x0, int y0)
{
	int sectorX, sectorY;
	if (s_pHost != NULL && s_pHost->GetCreaturePosition( id, sectorX, sectorY ))
	{
		m_CreatureID = id;

		//-----------------------------------------------------------
		// The helicopter's sound
		//-----------------------------------------------------------
		if (m_pSound!=NULL)
		{
			s_pHost->DeleteSound( m_pSound );
		}

		m_pSound = s_pHost->PlayLoop( SOUND_WORLD_PROPELLER, x0, y0 );

		//-----------------------------------------------------------
		// Chases the creature
		//-----------------------------------------------------------
		int cx, cy, cz;
		cx = WorldTileGeometry::TileToPixelX( sectorX );
		cy = WorldTileGeometry::TileToPixelY( sectorY );
		cz = 0;

		int effectSpriteType = EFFECTSPRITETYPE_SUMMON_HELICOPTER;
		int direction = 2;
		int step = PIXEL_HELICOPTER_MOVE;

		int startX	= cx + 500;
		int	startY	= cy - 300;
		int startZ	= cz;

		BLT_TYPE		bltType = (*g_pEffectSpriteTypeTable)[effectSpriteType].BltType;
		TYPE_FRAMEID	frameID	= (*g_pEffectSpriteTypeTable)[effectSpriteType].FrameID;

		MChaseEffect* pEffect = new MChaseEffect(bltType);

		m_EffectID = pEffect->GetID();

		int maxFrame = s_pHost->GetMaxEffectFrame(bltType, frameID);
		pEffect->SetFrameID( frameID, maxFrame );

		pEffect->SetPixelPosition( startX, startY, startZ );
		pEffect->SetDirection( direction );
		pEffect->SetTraceCreatureID( id );
		pEffect->SetStepPixel( step );

		// The zone owns the effect.
		s_pHost->AddEffect( pEffect );

		return true;
	}

	Release();

	return false;
}

//----------------------------------------------------------------------
// Update Sound
//----------------------------------------------------------------------
void
MHelicopter::UpdateSound(int x, int y)
{
	if (m_pSound!=NULL && s_pHost != NULL)
	{
		s_pHost->MoveSound( m_pSound, x, y );
	}
}

//----------------------------------------------------------------------
//
//						MHelicopterManager
//
//----------------------------------------------------------------------
MHelicopterManager::MHelicopterManager()
{
}

MHelicopterManager::~MHelicopterManager()
{
	Release();
}

//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
MHelicopterManager::Release()
{
	CTypeMap2<MHelicopter>::Release();
}

//----------------------------------------------------------------------
// Update
//----------------------------------------------------------------------
// A helicopter whose shadow is gone goes. A chasing one moves its sound
// with its shadow; when it reaches the player, the way-point selection
// opens; when one sent away (chasing no one) arrives, it goes.
//----------------------------------------------------------------------
void
MHelicopterManager::Update()
{
	iterator iHelicopter = begin();

	while (iHelicopter != end())
	{
		MHelicopter* pHelicopter = iHelicopter->second;

		bool bDeleteHelicopter = false;

		MEffect* pEffect = ZoneEffect( pHelicopter->GetEffectID() );

		if (pEffect==NULL)
		{
			bDeleteHelicopter = true;
		}
		else
		{
			if (pEffect->GetEffectType()==MEffect::EFFECT_CHASE)
			{
				MChaseEffect *pChaseEffect = (MChaseEffect*)pEffect;

				pHelicopter->UpdateSound( pEffect->GetX(), pEffect->GetY() );

				if (pChaseEffect->GetTraceCreatureID()==s_pHost->PlayerID())
				{
					if (pChaseEffect->IsChaseOver())
					{
						if (!s_pHost->IsSelectingWayPoint())
						{
							s_pHost->SelectWayPoint();
						}
					}
				}
				else if (pChaseEffect->GetTraceCreatureID()==OBJECTID_NULL
					&& pChaseEffect->IsChaseOver())
				{
					bDeleteHelicopter = true;
				}
			}
		}

		if (bDeleteHelicopter)
		{
			delete pHelicopter;

			iterator iTemp = iHelicopter;
			iHelicopter ++;

			erase( iTemp );
		}
		else
		{
			iHelicopter ++;
		}
	}
}

//----------------------------------------------------------------------
// Add Helicopter
//----------------------------------------------------------------------
// One per creature: a new one replaces the old.
//----------------------------------------------------------------------
bool
MHelicopterManager::AddHelicopter(MHelicopter* pHelicopter)
{
	TYPE_OBJECTID creatureID = pHelicopter->GetCreatureID();

	iterator iHelicopter = find( creatureID );

	if (iHelicopter != end())
	{
		delete iHelicopter->second;
		erase( iHelicopter );
	}

	insert(TYPE_MAP::value_type(creatureID, pHelicopter));

	return true;
}

//----------------------------------------------------------------------
// Get Helicopter
//----------------------------------------------------------------------
MHelicopter*
MHelicopterManager::GetHelicopter(TYPE_OBJECTID creatureID) const
{
	const_iterator iHelicopter = find( creatureID );

	if (iHelicopter == end())
	{
		return NULL;
	}

	return iHelicopter->second;
}

//----------------------------------------------------------------------
// Remove Helicopter Soon
//----------------------------------------------------------------------
// Deletes it at once.
//----------------------------------------------------------------------
void
MHelicopterManager::RemoveHelicopterSoon(TYPE_OBJECTID creatureID)
{
	iterator iHelicopter = find( creatureID );

	if (iHelicopter != end())
	{
		delete iHelicopter->second;
		erase( iHelicopter );
	}
}

//----------------------------------------------------------------------
// Remove Helicopter
//----------------------------------------------------------------------
// Sends it away: it stops chasing and flies 1000 pixels left and 600
// down, and Update deletes it when it arrives. Without a shadow, it is
// deleted at once.
//----------------------------------------------------------------------
void
MHelicopterManager::RemoveHelicopter(TYPE_OBJECTID creatureID)
{
	iterator iHelicopter = find( creatureID );

	if (iHelicopter != end())
	{
		MHelicopter* pHelicopter = iHelicopter->second;

		MEffect* pEffect = ZoneEffect( pHelicopter->GetEffectID() );

		if (pEffect==NULL)
		{
			delete pHelicopter;
			erase( iHelicopter );
		}
		else
		{
			if (pEffect->GetEffectType()==MEffect::EFFECT_CHASE)
			{
				MChaseEffect *pChaseEffect = (MChaseEffect*)pEffect;

				if (pChaseEffect->GetTraceCreatureID()!=OBJECTID_NULL)
				{
					pChaseEffect->SetTraceCreatureID( OBJECTID_NULL );

					int x = pEffect->GetPixelX() - 1000;
					int y = pEffect->GetPixelY() + 600;

					pChaseEffect->SetTarget(x, y, 0, PIXEL_HELICOPTER_MOVE);
				}
			}
		}
	}
}
