// Phase-driven directional rows of stationary effects.
#ifndef __MBLOODYBREAKEREFFECTGENERATOR_H__
#define __MBLOODYBREAKEREFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MBloodyBreakerEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
};

// Borrowed services. Animation lengths use the initial sprite's blit type even
// as frame IDs change. Queue consumes every effect and returns true only when
// it retains the effect alive. Only an accepted center owns the original target.
struct MBloodyBreakerEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MBloodyBreakerEffectSprite& sprite) = nullptr;
	bool (*MaxFrames)(BYTE blt, TYPE_FRAMEID frameID, int& count) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MBloodyBreakerEffectGenerator : public MEffectGenerator {
	public:
		MBloodyBreakerEffectGenerator() {}
		~MBloodyBreakerEffectGenerator() {}
		static const MBloodyBreakerEffectHost* SetHost(const MBloodyBreakerEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_BLOODY_BREAKER; }
		// Success means the accepted center owns the original target.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MBloodyBreakerEffectSprite& sprite);
		static bool ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MBloodyBreakerEffectHost* s_pHost;
};

#endif
