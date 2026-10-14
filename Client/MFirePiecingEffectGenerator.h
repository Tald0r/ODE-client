//----------------------------------------------------------------------
// MFirePiecingEffectGenerator.h
//----------------------------------------------------------------------
// created at a point in the zone
//----------------------------------------------------------------------

#ifndef	__MFIREPIECINGEFFECTGENERATOR_H__
#define	__MFIREPIECINGEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MAttackZoneEffectGenerator.h"
#include <memory>

class MFirePiecingEffectGenerator : public MEffectGenerator {
	public :
		// The zone attack's services: the sprite with its frames, and the queue.
		static const MZoneAttackEffectHost* SetHost(const MZoneAttackEffectHost* host);

		MFirePiecingEffectGenerator() {}
		~MFirePiecingEffectGenerator() {}

		TYPE_EFFECTGENERATORID		GetID()		{ return EFFECTGENERATORID_FIRE_PIECING; }

		// create the effect
		bool	Generate( const EFFECTGENERATOR_INFO& egInfo );

	private :
		static bool ReadSprite(int type, MZoneAttackEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MZoneAttackEffectHost* s_pHost;
};

//
//extern MStopZoneEffectGenerator	g_StopZoneEffectGenerator;

#endif

