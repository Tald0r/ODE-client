//----------------------------------------------------------------------
// MRippleZonePixelEffectGenerator.h
//----------------------------------------------------------------------
// created a tile on from a point in the zone, in its direction
//----------------------------------------------------------------------

#ifndef	__MRIPPLEZONEPIXELEFFECTGENERATOR_H__
#define	__MRIPPLEZONEPIXELEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MRippleZoneEffectGenerator.h"
#include <memory>

class MRippleZonePixelEffectGenerator : public MEffectGenerator {
	public :
		// The ripple's services: the sprite with its frames, and the queue on
		// the ground or not (its bounds are not used).
		static const MRippleZoneEffectHost* SetHost(const MRippleZoneEffectHost* host);

		MRippleZonePixelEffectGenerator() {}
		~MRippleZonePixelEffectGenerator() {}

		TYPE_EFFECTGENERATORID		GetID()		{ return EFFECTGENERATORID_RIPPLE_ZONE_PIXEL; }

		// create the effect
		bool	Generate( const EFFECTGENERATOR_INFO& egInfo );

	private :
		static bool QueueEffect(std::unique_ptr<MEffect> effect, bool ground);
		static const MRippleZoneEffectHost* s_pHost;
};

//
//extern MRippleZoneEffectGenerator	g_RippleZoneEffectGenerator;

#endif

