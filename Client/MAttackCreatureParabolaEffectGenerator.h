// Generate a parabolic projectile toward a sampled creature position.
#ifndef __MATTACKCREATUREPARABOLAEFFECTGENERATOR_H__
#define __MATTACKCREATUREPARABOLAEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MCreatureParabolaEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
};

struct MCreatureParabolaPosition
{
	TYPE_SECTORPOSITION x = 0, y = 0;
	short z = 0;
};

// Borrowed services. Animation length is read after the creature position.
// Queue consumes every effect, returning true only when it retains it alive.
struct MCreatureParabolaEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MCreatureParabolaEffectSprite& sprite) = nullptr;
	bool (*Creature)(TYPE_OBJECTID id, MCreatureParabolaPosition& position) = nullptr;
	bool (*MaxFrames)(BYTE blt, TYPE_FRAMEID frameID, int& count) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MAttackCreatureParabolaEffectGenerator : public MEffectGenerator {
	public:
		MAttackCreatureParabolaEffectGenerator() {}
		~MAttackCreatureParabolaEffectGenerator() {}
		static const MCreatureParabolaEffectHost* SetHost(const MCreatureParabolaEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_ATTACK_CREATURE_PARABOLA; }
		// Only successful submission transfers the original target.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MCreatureParabolaEffectSprite& sprite);
		static bool ReadCreature(TYPE_OBJECTID id, MCreatureParabolaPosition& position);
		static bool ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MCreatureParabolaEffectHost* s_pHost;
};

#endif
