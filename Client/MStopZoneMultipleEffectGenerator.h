// MStopZoneMultipleEffectGenerator.h
#ifndef __MSTOPZONEMULTIPLEEFFECTGENERATOR_H__
#define __MSTOPZONEMULTIPLEEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

class MEvent;

struct MStopMultipleEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

// Borrowed services. The optional event follows metadata and precedes all
// random positions. Queue consumes every effect and returns true only when
// it retains the submitted effect alive.
struct MStopMultipleEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MStopMultipleEffectSprite& sprite) = nullptr;
	void (*AddEvent)(MEvent& event) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MStopZoneMultipleEffectGenerator : public MEffectGenerator {
	public:
		MStopZoneMultipleEffectGenerator() {}
		~MStopZoneMultipleEffectGenerator() {}
		static const MStopMultipleEffectHost* SetHost(const MStopMultipleEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_STOP_ZONE_MULTIPLE; }
		// The first accepted effect owns the original target; later ones get copies.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MStopMultipleEffectSprite& sprite);
		static void AddEvent(MEvent& event);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MStopMultipleEffectHost* s_pHost;
};

#endif
