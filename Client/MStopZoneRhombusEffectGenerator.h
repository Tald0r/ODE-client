//----------------------------------------------------------------------
// MStopZoneRhombusEffectGenerator.h
//----------------------------------------------------------------------
#ifndef __MSTOPZONERHOMBUSEFFECTGENERATOR_H__
#define __MSTOPZONERHOMBUSEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MFixedZoneEffectHost.h"

class MStopZoneRhombusEffectGenerator : public MEffectGenerator {
	public:
		MStopZoneRhombusEffectGenerator() {}
		~MStopZoneRhombusEffectGenerator() {}
		static const MFixedZoneEffectHost* SetHost(const MFixedZoneEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_STOP_ZONE_RHOMBUS; }
		// Only slot zero takes the original target; later accepted slots own copies.
		// The result reports slot zero, including when no target was supplied.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MFixedZoneEffectHost* s_pHost;
};

#endif
