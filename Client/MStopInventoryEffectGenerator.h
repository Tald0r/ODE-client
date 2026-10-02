//----------------------------------------------------------------------
// MStopInventoryEffectGenerator.h
//----------------------------------------------------------------------
// Generate a stationary effect at an inventory cell.
//----------------------------------------------------------------------

#ifndef	__MSTOPINVENTORYEFFECTGENERATOR_H__
#define	__MSTOPINVENTORYEFFECTGENERATOR_H__

#include "MEffectGenerator.h"

class MScreenEffectManager;

struct MInventoryEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

struct MInventoryEffectPlacement
{
	POINT basis{}, cell{};
	int itemWidth = 1, itemHeight = 1;
	int cellWidth = 0, cellHeight = 0;
};

// Borrowed services. Failed reads leave the caller's effect target untouched.
struct MInventoryEffectHost
{
	bool (*Placement)(int x, int y, MInventoryEffectPlacement& placement) = nullptr;
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MInventoryEffectSprite& sprite) = nullptr;
	MScreenEffectManager* (*Manager)() = nullptr;
};

class MStopInventoryEffectGenerator : public MEffectGenerator {
	public :
		MStopInventoryEffectGenerator() {}
		~MStopInventoryEffectGenerator() {}
		static const MInventoryEffectHost* SetHost(const MInventoryEffectHost* host);

		TYPE_EFFECTGENERATORID		GetID()		{ return EFFECTGENERATORID_STOP_INVENTORY; }

		// A successful generation transfers the target into the queued effect.
		bool	Generate( const EFFECTGENERATOR_INFO& egInfo );

	private:
		static bool ReadPlacement(int x, int y, MInventoryEffectPlacement& placement);
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MInventoryEffectSprite& sprite);
		static MScreenEffectManager* ReadManager();
		static const MInventoryEffectHost* s_pHost;
};

#endif

