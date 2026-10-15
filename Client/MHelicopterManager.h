//----------------------------------------------------------------------
// MHelicopterManager.h
//----------------------------------------------------------------------
// An MHelicopter is shown as an effect that chases one player (its
// shadow) with a looping propeller sound. MHelicopterManager handles every
// MHelicopter on the screen (task 4.226, gamemodel's since then). What
// they ask of the game goes through MHelicopterHost.
//----------------------------------------------------------------------

#ifndef __MHELICOPTERMANAGER_H__
#define __MHELICOPTERMANAGER_H__

#include "MTypeDef.h"
#include "CTypeMap2.h"
#include "MEffect.h"
class ZONESOUND_NODE;

struct MHelicopterHost
{
	// The zone: a creature's sector, and the effects it owns.
	bool (*GetCreaturePosition)(TYPE_OBJECTID id, int& sectorX, int& sectorY) = nullptr;
	void (*AddEffect)(MEffect* pEffect) = nullptr;
	MEffect* (*GetEffect)(TYPE_OBJECTID id) = nullptr;
	void (*RemoveEffect)(TYPE_OBJECTID id) = nullptr;
	int (*GetMaxEffectFrame)(BLT_TYPE bltType, TYPE_FRAMEID frameID) = nullptr;
	TYPE_OBJECTID (*PlayerID)() = nullptr;
	// The way-point selection the player's helicopter opens on arrival.
	bool (*IsSelectingWayPoint)() = nullptr;
	void (*SelectWayPoint)() = nullptr;
	// A looping sound at a sector, moved, and deleted.
	ZONESOUND_NODE* (*PlayLoop)(TYPE_SOUNDID id, int x, int y) = nullptr;
	void (*MoveSound)(ZONESOUND_NODE* pSound, int x, int y) = nullptr;
	void (*DeleteSound)(ZONESOUND_NODE* pSound) = nullptr;
};

//----------------------------------------------------------------------
// MHelicopter
//----------------------------------------------------------------------
class MHelicopter {
	public :
		MHelicopter();
		~MHelicopter();

		// Without a host there is no zone: no creature to chase, and no
		// helicopter's effect.
		static const MHelicopterHost*	SetHost(const MHelicopterHost* pHost);

		void			Release();

		bool			SetChaseCreature(TYPE_OBJECTID id, int x0, int y0);		

		TYPE_OBJECTID	GetCreatureID() const	{ return m_CreatureID; }
		TYPE_OBJECTID	GetEffectID() const		{ return m_EffectID; }

		void			UpdateSound(int x, int y);

	protected :
		TYPE_OBJECTID			m_CreatureID;	// who called it
		TYPE_OBJECTID			m_EffectID;		// the ID of its shadow effect
		ZONESOUND_NODE*			m_pSound;		// its propeller sound
};

//----------------------------------------------------------------------
// MHelicopterManager - a map of MHelicopter*
//----------------------------------------------------------------------
class MHelicopterManager : private CTypeMap2<MHelicopter> {
	public :
		MHelicopterManager();
		~MHelicopterManager();

		void			Release();

		bool			AddHelicopter(MHelicopter* pHelicopter);
		MHelicopter*	GetHelicopter(TYPE_OBJECTID creatureID) const;
		void			RemoveHelicopter(TYPE_OBJECTID creatureID);
		void			RemoveHelicopterSoon(TYPE_OBJECTID creatureID);

		void			Update();		
};

#endif
