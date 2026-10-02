//----------------------------------------------------------------------
// MMultipleFallingEffectGenerator.h
//----------------------------------------------------------------------
// Generate phases of four falling projectiles.
//----------------------------------------------------------------------

#ifndef	__MMULTIPLEFALLINGEFFECTGENERATOR_H__
#define	__MMULTIPLEFALLINGEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MMultipleFallingEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

// Borrowed services. Queue consumes each effect and returns true only when it
// retains the effect alive. The first accepted shot receives the caller target.
struct MMultipleFallingEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MMultipleFallingEffectSprite& sprite) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MMultipleFallingEffectGenerator : public MEffectGenerator {
	public :
		MMultipleFallingEffectGenerator() {}
		~MMultipleFallingEffectGenerator() {}
		static const MMultipleFallingEffectHost* SetHost(const MMultipleFallingEffectHost* host);

		TYPE_EFFECTGENERATORID		GetID()		{ return EFFECTGENERATORID_MULTIPLE_FALLING; }

		// Rejection leaves the caller owning egInfo.pEffectTarget.
		bool	Generate( const EFFECTGENERATOR_INFO& egInfo );

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MMultipleFallingEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MMultipleFallingEffectHost* s_pHost;
};

#endif

