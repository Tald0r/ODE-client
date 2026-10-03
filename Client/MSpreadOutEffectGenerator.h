// Eight-direction linear effect generation.
#ifndef __MSPREADOUTEFFECTGENERATOR_H__
#define __MSPREADOUTEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MFixedZoneEffectHost.h"

class MSpreadOutEffectGenerator : public MEffectGenerator {
	public:
		MSpreadOutEffectGenerator() {}
		~MSpreadOutEffectGenerator() {}
		static const MFixedZoneEffectHost* SetHost(const MFixedZoneEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_SPREAD_OUT; }
		// Only slot zero takes the original target and determines the result.
		// Accepted targets are moved to their direction's computed destination.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MFixedZoneEffectHost* s_pHost;
};

#endif
