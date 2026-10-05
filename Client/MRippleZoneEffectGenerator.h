// One stationary ripple tile ahead of the source.
#ifndef __MRIPPLEZONEEFFECTGENERATOR_H__
#define __MRIPPLEZONEEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MFixedZoneEffectHost.h"

// Borrowed services. Queue consumes every effect and returns true only when
// it retains it alive; ground selects the ground-effect queue.
struct MRippleZoneEffectHost
{
	bool (*Bounds)(TYPE_SECTORPOSITION& width, TYPE_SECTORPOSITION& height) = nullptr;
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect, bool ground) = nullptr;
};

class MRippleZoneEffectGenerator : public MEffectGenerator {
	public:
		static const MRippleZoneEffectHost* SetHost(const MRippleZoneEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_RIPPLE_ZONE; }
		// Both queues transfer the original target only after accepting the effect.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadBounds(TYPE_SECTORPOSITION& width, TYPE_SECTORPOSITION& height);
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect, bool ground);
		static const MRippleZoneEffectHost* s_pHost;
};

#endif
