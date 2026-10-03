// Borrowed services for horizontal and vertical empty-wall effect patterns.
#ifndef MEMPTYWALLEFFECTHOST_H
#define MEMPTYWALLEFFECTHOST_H

#include "MTypeDef.h"
#include <memory>

class MEffect;

struct MEmptyWallEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

// Queue consumes every effect and returns true only when it retains it alive.
struct MEmptyWallEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MEmptyWallEffectSprite& sprite) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

#endif
