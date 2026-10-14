//----------------------------------------------------------------------
// MAttachZoneEffectGenerator.h
//----------------------------------------------------------------------
// created at a point in the zone
//----------------------------------------------------------------------

#ifndef	__MATTACHZONEEFFECTGENERATOR_H__
#define	__MATTACHZONEEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include "MFixedZoneEffectHost.h"
#include <memory>

class MAttachZoneEffectGenerator : public MEffectGenerator {
	public :
		// The ground's services: the sprite with its frames, and the queue.
		static const MFixedZoneEffectHost* SetHost(const MFixedZoneEffectHost* host);

		MAttachZoneEffectGenerator()	{}
		~MAttachZoneEffectGenerator() {}

		TYPE_EFFECTGENERATORID		GetID()		{ return EFFECTGENERATORID_ATTACH_ZONE; }

		// create the effect
		bool	Generate( const EFFECTGENERATOR_INFO& egInfo );

	private :
		// Mercy Ground's nine tiles.
		static bool MercyGroundEffect( const EFFECTGENERATOR_INFO& egInfo );
		static bool ReadSprite(int type, MFixedZoneEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MFixedZoneEffectHost* s_pHost;
};

//
//extern MAttachZoneEffectGenerator	g_StopZoneEffectGenerator;

#endif

