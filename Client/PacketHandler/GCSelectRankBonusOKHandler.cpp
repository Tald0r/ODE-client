//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSelectRankBonusOK1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSelectRankBonusOK.h"
#include "RankBonusTable.h"
#include "TempInformation.h"
#include "RankBonusDef.h"
#include "MPlayer.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSelectRankBonusOKHandler::execute ( GCSelectRankBonusOK * pGCSelectRankBonusOK , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	g_pTempInformation->SetMode(TempInformation::MODE_NULL);
	const int type = pGCSelectRankBonusOK->getRankBonusType();

	if(type < g_pRankBonusTable->GetSize())
	{
		if (auto* entry = g_pRankBonusTable->GetMutable(type)) {
			entry->SetStatus(RankBonusInfo::STATUS_LEARNED);
		}
		
		const int level = (*g_pRankBonusTable)[type].GetLevel();
		int i;
		
		for(i = type-1; i >= 0 && (*g_pRankBonusTable)[i].GetLevel() == level; i--)
			if (auto* entry = g_pRankBonusTable->GetMutable(i)) {
				entry->SetStatus(RankBonusInfo::STATUS_CANNOT_LEARN);
			}
		
		for(i = type+1; i < g_pRankBonusTable->GetSize() && (*g_pRankBonusTable)[i].GetLevel() == level; i++)
			if (auto* entry = g_pRankBonusTable->GetMutable(i)) {
				entry->SetStatus(RankBonusInfo::STATUS_CANNOT_LEARN);
			}
	}

	if(type == RANK_BONUS_URANUS_BLESS)
		g_pPlayer->CheckRegen();
		
	__END_CATCH
}
