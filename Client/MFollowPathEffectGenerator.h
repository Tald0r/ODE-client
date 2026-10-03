// Generate a linear effect for the target's current path phase.
#ifndef __MFOLLOWPATHEFFECTGENERATOR_H__
#define __MFOLLOWPATHEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MFixedZoneEffectHost.h"

class MFollowPathEffectGenerator : public MEffectGenerator {
	public:
		MFollowPathEffectGenerator() {}
		~MFollowPathEffectGenerator() {}
		static const MFixedZoneEffectHost* SetHost(const MFixedZoneEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_FOLLOW_PATH; }
		// Only acceptance transfers and retargets the original target.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MFixedZoneEffectHost* s_pHost;
};

#endif
