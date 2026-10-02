//----------------------------------------------------------------------
// MRisingEffectGenerator.h
//----------------------------------------------------------------------
// Generate rising projectiles and firework patterns.
//----------------------------------------------------------------------

#ifndef	__MRISINGEFFECTGENERATOR_H__
#define	__MRISINGEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MRisingEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

// Borrowed services. Queue consumes the effect on every call and returns true
// only if it retains the effect alive. Targets attach after successful submission.
struct MRisingEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MRisingEffectSprite& sprite) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MRisingEffectGenerator : public MEffectGenerator {
	public :
		MRisingEffectGenerator() {}
		~MRisingEffectGenerator() {}
		static const MRisingEffectHost* SetHost(const MRisingEffectHost* host);

		TYPE_EFFECTGENERATORID		GetID()		{ return EFFECTGENERATORID_RISING; }

		// True means the caller target transferred, or (without a target)
		// at least one shot was accepted.
		bool	Generate( const EFFECTGENERATOR_INFO& egInfo );

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MRisingEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MRisingEffectHost* s_pHost;
};

#endif

