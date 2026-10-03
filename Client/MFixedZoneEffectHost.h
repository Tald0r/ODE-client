// Borrowed services for fixed zone-effect patterns.
#ifndef MFIXEDZONEEFFECTHOST_H
#define MFIXEDZONEEFFECTHOST_H

#include "MTypeDef.h"
#include <memory>

class MEffect;

struct MFixedZoneEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

// Queue consumes every effect and returns true only when it retains it alive.
struct MFixedZoneEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

#endif
