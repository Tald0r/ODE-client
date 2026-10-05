// Ground-effect ring and Ruffian axe patterns.
#ifndef __MATTACHZONEAROUNDEFFECTGENERATOR_H__
#define __MATTACHZONEAROUNDEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MFixedZoneEffectHost.h"
class MEvent;

// Borrowed services. Queue consumes every effect and returns true only when
// it retains it alive. AddEvent receives the shake after the axe submissions.
struct MAroundGroundEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
	void (*AddEvent)(MEvent& event) = nullptr;
};

class MAttachZoneAroundEffectGenerator : public MEffectGenerator
{
public:
	static const MAroundGroundEffectHost* SetHost(const MAroundGroundEffectHost* host);
	TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_ATTACH_ZONE_AROUND; }
	// True means a retained effect owns the original target, or an effect was
	// retained when no target was supplied. Rejection leaves it with the caller.
	bool Generate(const EFFECTGENERATOR_INFO& egInfo);

private:
	static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite);
	static bool QueueEffect(std::unique_ptr<MEffect> effect);
	static void AddEvent(MEvent& event);
	static const MAroundGroundEffectHost* s_pHost;
};

#endif
