// Ground-effect generation using borrowed sprite and queue services.
#ifndef __MATTACHZONESELECTABLEEFFECTGENERATOR_H__
#define __MATTACHZONESELECTABLEEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MFixedZoneEffectHost.h"

class MAttachZoneSelectableEffectGenerator : public MEffectGenerator
{
public:
	static const MFixedZoneEffectHost* SetHost(const MFixedZoneEffectHost* host);
	TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_ATTACH_ZONE_SELECTABLE; }
	// True means a retained effect owns the original target, or an effect was
	// retained when no target was supplied. Rejection leaves it with the caller.
	bool Generate(const EFFECTGENERATOR_INFO& egInfo);

private:
	static int OffsetCoordinate(int coordinate, int offset);
	static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite);
	static bool QueueEffect(std::unique_ptr<MEffect> effect);
	static const MFixedZoneEffectHost* s_pHost;
};

#endif
