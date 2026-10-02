//----------------------------------------------------------------------
// MFallingEffectGenerator.h
//----------------------------------------------------------------------
// Generate an effect falling from above its destination.
//----------------------------------------------------------------------

#ifndef	__MFALLINGEFFECTGENERATOR_H__
#define	__MFALLINGEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MFallingEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

// Borrowed services. The queue consumes the effect on every call and returns
// true only if it retains the effect alive. The target transfers only on success.
struct MFallingEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MFallingEffectSprite& sprite) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MFallingEffectGenerator : public MEffectGenerator {
	public :
		MFallingEffectGenerator()	{}
		~MFallingEffectGenerator() {}
		static const MFallingEffectHost* SetHost(const MFallingEffectHost* host);

		TYPE_EFFECTGENERATORID		GetID()		{ return EFFECTGENERATORID_FALLING; }

		// Rejection leaves the caller owning egInfo.pEffectTarget.
		bool	Generate( const EFFECTGENERATOR_INFO& egInfo );

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MFallingEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MFallingEffectHost* s_pHost;
};

#endif

