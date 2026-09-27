//////////////////////////////////////////////////////////////////////
//
// Filename    : GCFlagWarStatusHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCFlagWarStatus.h"
#include "ClientDef.h"
#include "UIFunction.h"
#include "MonotonicClock.h"

#include <chrono>

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCFlagWarStatusHandler::execute ( GCFlagWarStatus * pGCFlagWarStatus , Player * pPlayer )

{
	(void)pPlayer;
	// The war's end as a point on the monotonic clock: the server sends
	// the seconds remaining.
	const WORD timeRemain = pGCFlagWarStatus->getTimeRemain();
	const MonotonicClock::TimePoint endTime = MonotonicClock::Now() + std::chrono::seconds(timeRemain);
	int		flag_s = (int)pGCFlagWarStatus->getFlagCount( RACE_SLAYER );
	int		flag_v = (int)pGCFlagWarStatus->getFlagCount( RACE_VAMPIRE );
	int		flag_o = (int)pGCFlagWarStatus->getFlagCount( RACE_OUSTERS );

	// Four hours or more is ignored outright (integer hours; upstream's
	// comment said three, and its arithmetic said this).
	if( timeRemain/60/60 > 3 )
		return;

	UI_SetCTFStatus( endTime, flag_s, flag_v, flag_o );
	
}
