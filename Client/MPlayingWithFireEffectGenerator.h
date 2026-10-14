//----------------------------------------------------------------------
// MAttachZoneEffectGenerator.h
//----------------------------------------------------------------------
// created at a point in the zone
//----------------------------------------------------------------------

#ifndef	__MPLAYINGWITHFIREEFFECTGENERATOR_H__
#define	__MPLAYINGWITHFIREEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MAttackZoneEffectGenerator.h"
#include <memory>

class MPlayingWithFireEffectGenerator : public MEffectGenerator {
	public :
		// The zone attack's services: the sprite with its frames, and the queue.
		static const MZoneAttackEffectHost* SetHost(const MZoneAttackEffectHost* host);

		MPlayingWithFireEffectGenerator()	{}
		~MPlayingWithFireEffectGenerator() {}

		TYPE_EFFECTGENERATORID		GetID()		{ return EFFECTGENERATORID_PLAYING_WITH_FIRE; }

		int m_EffectCount;
		// create the effect
		bool	Generate( const EFFECTGENERATOR_INFO& egInfo );

	private :
		static bool ReadSprite(int type, MZoneAttackEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MZoneAttackEffectHost* s_pHost;
};

//
//extern MAttachZoneEffectGenerator	g_StopZoneEffectGenerator;

#endif

