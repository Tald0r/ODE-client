// Stationary wave patterns selected by the target's current phase.
#ifndef __MBLOODYWAVEEFFECTGENERATOR_H__
#define __MBLOODYWAVEEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MBloodyWaveEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	bool repeatFrame = false;
};

// Borrowed services. Queue consumes every effect and returns true only when
// it retains it alive. Frame metadata refreshes after every submission.
struct MBloodyWaveEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MBloodyWaveEffectSprite& sprite) = nullptr;
	bool (*MaxFrames)(BYTE blt, TYPE_FRAMEID frameID, int& count) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MBloodyWaveEffectGenerator : public MEffectGenerator {
	public:
		static const MBloodyWaveEffectHost* SetHost(const MBloodyWaveEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_BLOODY_WAVE; }
		// Only the first accepted effect takes the original target.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MBloodyWaveEffectSprite& sprite);
		static bool ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MBloodyWaveEffectHost* s_pHost;
};

#endif
