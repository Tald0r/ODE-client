// Five stationary effects with directional positions and linked target copies.
#ifndef __MBLOODYWALLEFFECTGENERATOR_H__
#define __MBLOODYWALLEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MBloodyWallEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	bool repeatFrame = false;
};

// Borrowed services. The first sprite fixes the blit type and repeat policy;
// frame IDs and animation lengths refresh after every submission attempt.
// Queue consumes every effect and returns true only when it retains it alive.
struct MBloodyWallEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MBloodyWallEffectSprite& sprite) = nullptr;
	bool (*MaxFrames)(BYTE blt, TYPE_FRAMEID frameID, int& count) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MBloodyWallEffectGenerator : public MEffectGenerator {
	public:
		MBloodyWallEffectGenerator() {}
		~MBloodyWallEffectGenerator() {}
		static const MBloodyWallEffectHost* SetHost(const MBloodyWallEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_BLOODY_WALL; }
		// The first accepted effect owns the original target; later ones own copies.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MBloodyWallEffectSprite& sprite);
		static bool ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MBloodyWallEffectHost* s_pHost;
};

#endif
