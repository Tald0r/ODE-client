//----------------------------------------------------------------------
// MStopZoneEffectGenerator.h
//----------------------------------------------------------------------
#ifndef __MSTOPZONEEFFECTGENERATOR_H__
#define __MSTOPZONEEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

class MEvent;

struct MStopZoneEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	bool repeatFrame = false;
};

// Borrowed services. Sprite selection precedes the optional meteor event;
// MaxFrames resolves the final frame ID afterward. Queue consumes every
// effect and returns true only if it retains the submitted effect alive.
struct MStopZoneEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MStopZoneEffectSprite& sprite) = nullptr;
	bool (*MaxFrames)(BYTE blt, TYPE_FRAMEID frameID, int& count) = nullptr;
	void (*AddEvent)(MEvent& event) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MStopZoneEffectGenerator : public MEffectGenerator {
	public:
		MStopZoneEffectGenerator() {}
		~MStopZoneEffectGenerator() {}
		static const MStopZoneEffectHost* SetHost(const MStopZoneEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_STOP_ZONE; }
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MStopZoneEffectSprite& sprite);
		static bool ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count);
		static void AddEvent(MEvent& event);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MStopZoneEffectHost* s_pHost;
};

#endif
