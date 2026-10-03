//----------------------------------------------------------------------
// MStopZoneEmptyVerticalEffectGenerator.h
//----------------------------------------------------------------------
#ifndef __MSTOPZONEEMPTYVERTICALWALLEEFFECTGENERATOR_H__
#define __MSTOPZONEEMPTYVERTICALWALLEEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MEmptyWallEffectHost.h"

class MStopZoneEmptyVerticalWallEffectGenerator : public MEffectGenerator {
	public:
		MStopZoneEmptyVerticalWallEffectGenerator() {}
		~MStopZoneEmptyVerticalWallEffectGenerator() {}
		static const MEmptyWallEffectHost* SetHost(const MEmptyWallEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_STOP_ZONE_EMPTY_VERTICAL_WALL; }
		// The first accepted effect takes the original target; later ones get copies.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MEmptyWallEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MEmptyWallEffectHost* s_pHost;
};

#endif
