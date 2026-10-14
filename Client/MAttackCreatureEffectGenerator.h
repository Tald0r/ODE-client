//----------------------------------------------------------------------
// MAttackCreatureEffectGenerator.h
//----------------------------------------------------------------------
// tracing a creature
//----------------------------------------------------------------------

#ifndef	__MATTACKCREATUREEFFECTGENERATOR_H__
#define	__MATTACKCREATUREEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MAttackCreatureParabolaEffectGenerator.h"
#include <memory>

class MAttackCreatureEffectGenerator : public MEffectGenerator {
	public :
		MAttackCreatureEffectGenerator()	{}
		~MAttackCreatureEffectGenerator() {}

		// The services the parabola generator uses: the sprite, the
		// creature, the frames and the queue.
		static const MCreatureParabolaEffectHost* SetHost(const MCreatureParabolaEffectHost* host);

		TYPE_EFFECTGENERATORID		GetID()		{ return EFFECTGENERATORID_ATTACK_CREATURE; }

		// create the effect
		bool	Generate( const EFFECTGENERATOR_INFO& egInfo );

	private :
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MCreatureParabolaEffectHost* s_pHost;
};

#endif
