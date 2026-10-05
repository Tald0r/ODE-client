// Ground-effect generation using borrowed sprite and queue services.
#ifndef __MATTACKZONERECTEFFECTGENERATOR_H__
#define __MATTACKZONERECTEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MFixedZoneEffectHost.h"

class MAttackZoneRectEffectGenerator : public MEffectGenerator
{
public:
	static const MFixedZoneEffectHost* SetHost(const MFixedZoneEffectHost* host);
	TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_ATTACK_ZONE_RECT; }
	// True means a retained effect owns the original target, or an effect was
	// retained when no target was supplied. Rejection leaves it with the caller.
	bool Generate(const EFFECTGENERATOR_INFO& egInfo);

private:
	static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite);
	static bool QueueEffect(std::unique_ptr<MEffect> effect);
	static const MFixedZoneEffectHost* s_pHost;
};

#endif
