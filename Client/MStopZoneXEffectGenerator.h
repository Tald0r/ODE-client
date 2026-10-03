//----------------------------------------------------------------------
// MStopZoneXEffectGenerator.h
//----------------------------------------------------------------------
#ifndef __MSTOPZONEXEFFECTGENERATOR_H__
#define __MSTOPZONEXEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MFixedZoneEffectHost.h"

class MStopZoneXEffectGenerator : public MEffectGenerator {
	public:
		MStopZoneXEffectGenerator() {}
		~MStopZoneXEffectGenerator() {}
		static const MFixedZoneEffectHost* SetHost(const MFixedZoneEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_STOP_ZONE_X; }
		// Only slot zero takes the original target; later accepted slots own copies.
		// The result reports slot zero, including when no target was supplied.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MFixedZoneEffectHost* s_pHost;
};

#endif
