//----------------------------------------------------------------------
// MQuickSlot.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MQuickSlot.h"

MBelt*		g_pQuickSlot = NULL;
MOustersArmsBand*	g_pArmsBand1 = NULL;
MOustersArmsBand*	g_pArmsBand2 = NULL;

//----------------------------------------------------------------------
// - -;;
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Can Pickup Item To Quickslot
//----------------------------------------------------------------------
bool
CanPickupItemToQuickslot(Race race, bool bHasBelt, bool bHasArmsBand,
							bool bItemCheckBufferNULL, bool bTempModeNULL,
							const MItem* pItem)
{
	const bool bSlayer	= race==RACE_SLAYER;
	const bool bVampire	= race==RACE_VAMPIRE;
	const bool bOusters	= race==RACE_OUSTERS;

	return (bHasBelt && bSlayer) || ((bHasArmsBand && bOusters)
		&& bItemCheckBufferNULL
		&& bTempModeNULL
		&& ((pItem->IsSlayerItem() && bSlayer) || 
		(pItem->IsVampireItem() && bVampire) || 
		(pItem->IsOustersItem() && bOusters)));
}
