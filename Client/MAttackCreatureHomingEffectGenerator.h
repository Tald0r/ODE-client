//----------------------------------------------------------------------
// MAttackCreatureHomingEffectGenerator.h
//----------------------------------------------------------------------
// tracing a creature on a curve
//----------------------------------------------------------------------

#ifndef	__MATTACKCREATUREHOMINGEFFECTGENERATOR_H__
#define	__MATTACKCREATUREHOMINGEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MAttackCreatureParabolaEffectGenerator.h"
#include <memory>

class MAttackCreatureHomingEffectGenerator : public MEffectGenerator {
	public :
		MAttackCreatureHomingEffectGenerator()	{}
		~MAttackCreatureHomingEffectGenerator() {}

		// The services the parabola generator uses: the sprite, the
		// creature, the frames and the queue.
		static const MCreatureParabolaEffectHost* SetHost(const MCreatureParabolaEffectHost* host);

		TYPE_EFFECTGENERATORID		GetID()		{ return EFFECTGENERATORID_ATTACK_CREATURE_HOMING; }

		// create the effect
		bool	Generate( const EFFECTGENERATOR_INFO& egInfo );

	private :
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MCreatureParabolaEffectHost* s_pHost;
};

#endif
