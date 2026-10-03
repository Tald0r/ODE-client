//----------------------------------------------------------------------
// MAttackZoneBombEffectGenerator.h
//----------------------------------------------------------------------
#ifndef __MATTACKZONEBOMBEFFECTGENERATOR_H__
#define __MATTACKZONEBOMBEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MZoneBombEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

// Borrowed metadata requires the executable table and view. Queue consumes
// every effect and returns true only if it retains the submitted effect alive.
struct MZoneBombEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MZoneBombEffectSprite& sprite) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MAttackZoneBombEffectGenerator : public MEffectGenerator {
	public:
		MAttackZoneBombEffectGenerator() {}
		~MAttackZoneBombEffectGenerator() {}
		static const MZoneBombEffectHost* SetHost(const MZoneBombEffectHost* host);

		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_ATTACK_ZONE_BOMB; }
		// Rejection leaves target ownership with the caller.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MZoneBombEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MZoneBombEffectHost* s_pHost;
};

#endif
