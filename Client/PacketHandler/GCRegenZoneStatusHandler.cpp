//////////////////////////////////////////////////////////////////////
//
// Filename    : GCRegenZoneStatusHandler.cc
// Written By  : elca
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRegenZoneStatus.h"
#include "ShrineInfoManager.h"
#include "RegenZoneStatusHost.h"

namespace RegenZoneStatus {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

static RegenTowerInfoManager* ReadTable()
{
	return s_Host != nullptr && s_Host->Table != nullptr ? s_Host->Table() : nullptr;
}
}

//////////////////////////////////////////////////////////////////////
//
// Apply the eight server-owned towers and the fixed ownership of later rows.
//
//////////////////////////////////////////////////////////////////////
void GCRegenZoneStatusHandler::execute ( GCRegenZoneStatus * pPacket , Player * pPlayer )

{
	(void)pPlayer;
	auto* table = RegenZoneStatus::ReadTable();
	if (table == nullptr)
		return;

	int i;
	for(i = 0; i < 8 ; i++ )
	{
		if (auto* info = table->GetMutable(i)) {
			info->owner = (int)pPacket->getStatus(i);
		}
	}

	for(;i < table->GetSize(); i++)
	{
		auto* info = table->GetMutable(i);
		if (info == nullptr) continue;
		if( i >= 8 && i <= 11 )
		{
			info->owner = (i&0x1) ? RACE_VAMPIRE : RACE_SLAYER;
		}
		else
			info->owner = RACE_OUSTERS;
	}
}
