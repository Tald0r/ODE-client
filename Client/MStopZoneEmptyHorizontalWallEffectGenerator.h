//----------------------------------------------------------------------
// MStopZoneEmptyHorizontalWallEffectGenerator.h
//----------------------------------------------------------------------
#ifndef __MSTOPZONEEMPTYHORIZONTALWALLEEFFECTGENERATOR_H__
#define __MSTOPZONEEMPTYHORIZONTALWALLEEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MEmptyWallEffectHost.h"

class MStopZoneEmptyHorizontalWallEffectGenerator : public MEffectGenerator {
	public:
		MStopZoneEmptyHorizontalWallEffectGenerator() {}
		~MStopZoneEmptyHorizontalWallEffectGenerator() {}
		static const MEmptyWallEffectHost* SetHost(const MEmptyWallEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_STOP_ZONE_EMPTY_HORIZONTAL_WALL; }
		// The first accepted effect takes the original target; later ones get copies.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MEmptyWallEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MEmptyWallEffectHost* s_pHost;
};

#endif
