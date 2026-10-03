//----------------------------------------------------------------------
// MAttackZoneParabolaEffectGenerator.h
//----------------------------------------------------------------------
#ifndef __MATTACKZONEPARABOLAEFFECTGENERATOR_H__
#define __MATTACKZONEPARABOLAEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MZoneParabolaEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

// Borrowed metadata requires the executable table and view. Queue consumes
// every effect and returns true only if it retains the submitted effect alive.
struct MZoneParabolaEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MZoneParabolaEffectSprite& sprite) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MAttackZoneParabolaEffectGenerator : public MEffectGenerator {
	public:
		MAttackZoneParabolaEffectGenerator() {}
		~MAttackZoneParabolaEffectGenerator() {}
		static const MZoneParabolaEffectHost* SetHost(const MZoneParabolaEffectHost* host);

		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_ATTACK_ZONE_PARABOLA; }
		// Rejection leaves target ownership with the caller.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MZoneParabolaEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MZoneParabolaEffectHost* s_pHost;
};

#endif
