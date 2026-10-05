//----------------------------------------------------------------------
//
// Filename    : GCRankBonusInfoHandler.cpp
// Written By  : elca
// Description : 
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRankBonusInfo.h"
#include "RankBonusTable.h"
#include "RankBonusHandlerHost.h"

namespace RankBonusHandlers {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

void CheckRegen()
{
	if (s_Host != nullptr && s_Host->CheckRegen != nullptr)
		s_Host->CheckRegen();
}
}

void GCRankBonusInfoHandler::execute ( GCRankBonusInfo * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;
	for(int i = 0; i < g_pRankBonusTable->GetSize(); i++)
		if (auto* entry = g_pRankBonusTable->GetMutable(i)) {
			entry->SetStatus(RankBonusInfo::STATUS_NULL);
		}

	DWORD type = 0;

	while((type = pPacket->popFrontListElement()) != EndOfRankBonus)
	{
		if(type < static_cast<DWORD>(g_pRankBonusTable->GetSize()))
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
	}

	RankBonusHandlers::CheckRegen();

	__END_CATCH
}
