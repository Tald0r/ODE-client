//----------------------------------------------------------------------
// MStopZoneEmptyCrossEffectGenerator.h
//----------------------------------------------------------------------
#ifndef __MSTOPZONEEMPTYCROSSEFFECTGENERATOR_H__
#define __MSTOPZONEEMPTYCROSSEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MFixedZoneEffectHost.h"

class MStopZoneEmptyCrossEffectGenerator : public MEffectGenerator {
	public:
		MStopZoneEmptyCrossEffectGenerator() {}
		~MStopZoneEmptyCrossEffectGenerator() {}
		static const MFixedZoneEffectHost* SetHost(const MFixedZoneEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_STOP_ZONE_EMPTY_CROSS; }
		// Slot zero takes the original target; later accepted arms own retargeted copies.
		// The result reports slot zero, including when no target was supplied.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MFixedZoneEffectHost* s_pHost;
};

#endif
