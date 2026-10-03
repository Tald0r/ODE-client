// MStopZoneRectEffectGenerator.h
#ifndef __MSTOPZONERECTEFFECTGENERATOR_H__
#define __MSTOPZONERECTEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MRectZoneEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
};

struct MRectZoneEffectBounds
{
	TYPE_SECTORPOSITION width = 0;
	TYPE_SECTORPOSITION height = 0;
};

// Borrowed services. MaxFrames resolves the selected center frame once;
// Bounds follows center submission. Queue consumes every effect and returns
// true only when it retains it alive, including delayed effects.
struct MRectZoneEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MRectZoneEffectSprite& sprite) = nullptr;
	bool (*MaxFrames)(BYTE blt, TYPE_FRAMEID frameID, int& count) = nullptr;
	bool (*Bounds)(MRectZoneEffectBounds& bounds) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect, DWORD delay) = nullptr;
};

class MStopZoneRectEffectGenerator : public MEffectGenerator {
	public:
		MStopZoneRectEffectGenerator() {}
		~MStopZoneRectEffectGenerator() {}
		static const MRectZoneEffectHost* SetHost(const MRectZoneEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_STOP_ZONE_RECT; }
		// The first accepted effect owns the original target; later ones get copies.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MRectZoneEffectSprite& sprite);
		static bool ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count);
		static bool ReadBounds(MRectZoneEffectBounds& bounds);
		static bool QueueEffect(std::unique_ptr<MEffect> effect, DWORD delay);
		static const MRectZoneEffectHost* s_pHost;
};

#endif
