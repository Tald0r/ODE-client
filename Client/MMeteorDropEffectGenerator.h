// Generate a falling meteor and its accepted-effect fade event.
#ifndef __MMETEORDROPEFFECTGENERATOR_H__
#define __MMETEORDROPEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MFixedZoneEffectHost.h"

class MEvent;

// Borrowed services. Queue consumes on every call and returns true only when
// it retains the effect alive. AddEvent borrows the event for the call only.
struct MMeteorDropEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
	void (*AddEvent)(MEvent& event) = nullptr;
};

class MMeteorDropEffectGenerator : public MEffectGenerator {
	public:
		MMeteorDropEffectGenerator() {}
		~MMeteorDropEffectGenerator() {}
		static const MMeteorDropEffectHost* SetHost(const MMeteorDropEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_METEOR_DROP; }
		// Acceptance transfers the original target before the event is submitted.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static void AddEvent(MEvent& event);
		static const MMeteorDropEffectHost* s_pHost;
};

#endif
