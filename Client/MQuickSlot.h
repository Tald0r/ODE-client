//----------------------------------------------------------------------
// MQuickSlot.h
//----------------------------------------------------------------------
// QuickSlot은 Gear에 들어있는 Belt를 의미한다.
//
// Gear에 Belt를 착용하는 순간 g_pQuickSlot이 바뀌게 된다.
// 반대로, 제거하는 순간 g_pQuickSlot은 NULL이 된다.
//----------------------------------------------------------------------

#ifndef	__MQUICKSLOT_H__
#define	__MQUICKSLOT_H__

#include "MItem.h"

extern MBelt*					g_pQuickSlot;
extern MOustersArmsBand*		g_pArmsBand1;
extern MOustersArmsBand*		g_pArmsBand2;

//----------------------------------------------------------------------
// Can Pickup Item To Quickslot
//----------------------------------------------------------------------
// Whether an item picked up from the zone may go straight into the
// belt (slayer) or an armsband (ousters). The caller passes the
// executable-side state: whether the belt or an armsband is worn,
// whether the item check buffer is empty and whether no temporary
// mode is pending.
//----------------------------------------------------------------------
bool	CanPickupItemToQuickslot(Race race, bool bHasBelt, bool bHasArmsBand,
								bool bItemCheckBufferNULL, bool bTempModeNULL,
								const MItem* pItem);

#endif