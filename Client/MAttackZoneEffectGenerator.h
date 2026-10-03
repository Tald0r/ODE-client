//----------------------------------------------------------------------
// MAttackZoneEffectGenerator.h
//----------------------------------------------------------------------
// Generate a projectile toward a position in the zone.
//----------------------------------------------------------------------

#ifndef	__MATTACKZONEEFFECTGENERATOR_H__
#define	__MATTACKZONEEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MZoneAttackEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

// Borrowed services. Sprite resolution requires the executable table and view.
// Queue consumes every effect and returns true only if it retains it alive.
struct MZoneAttackEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MZoneAttackEffectSprite& sprite) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MAttackZoneEffectGenerator : public MEffectGenerator {
	public :
		MAttackZoneEffectGenerator() {}
		~MAttackZoneEffectGenerator() {}
		static const MZoneAttackEffectHost* SetHost(const MZoneAttackEffectHost* host);

		TYPE_EFFECTGENERATORID		GetID()		{ return EFFECTGENERATORID_ATTACK_ZONE; }

		// Rejection leaves target ownership with the caller.
		bool	Generate( const EFFECTGENERATOR_INFO& egInfo );

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MZoneAttackEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MZoneAttackEffectHost* s_pHost;
};

#endif

