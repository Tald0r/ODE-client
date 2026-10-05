// Skip-draw ground patterns using borrowed sprite and consuming queue services.
#ifndef __MSKIPEFFECTGENERATOR_H__
#define __MSKIPEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MFixedZoneEffectHost.h"

class MSkipEffectGenerator : public MEffectGenerator
{
public:
	static const MFixedZoneEffectHost* SetHost(const MFixedZoneEffectHost* host);
	TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_SKIP_DRAW; }
	// The first retained effect owns the original target. Rejection leaves it
	// with the caller; later retained effects own independent visual copies.
	bool Generate(const EFFECTGENERATOR_INFO& egInfo);

private:
	static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite);
	static bool QueueEffect(std::unique_ptr<MEffect> effect);
	static const MFixedZoneEffectHost* s_pHost;
};

#endif
