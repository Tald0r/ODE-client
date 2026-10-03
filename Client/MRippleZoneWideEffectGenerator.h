// Generate a widening stationary row one tile ahead of the source.
#ifndef __MRIPPLEZONEWIDEEFFECTGENERATOR_H__
#define __MRIPPLEZONEWIDEEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MFixedZoneEffectHost.h"

struct MWideRippleEffectBounds
{
	TYPE_SECTORPOSITION width = 0, height = 0;
};

// Borrowed services. Bounds are refreshed for each candidate. Queue consumes
// every effect, returning true only when it retains the effect alive.
struct MWideRippleEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite) = nullptr;
	bool (*Bounds)(MWideRippleEffectBounds& bounds) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MRippleZoneWideEffectGenerator : public MEffectGenerator {
	public:
		MRippleZoneWideEffectGenerator() {}
		~MRippleZoneWideEffectGenerator() {}
		static const MWideRippleEffectHost* SetHost(const MWideRippleEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_RIPPLE_ZONE_WIDE; }
		// Only center acceptance transfers the original target and returns true.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite);
		static bool ReadBounds(MWideRippleEffectBounds& bounds);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MWideRippleEffectHost* s_pHost;
};

#endif
