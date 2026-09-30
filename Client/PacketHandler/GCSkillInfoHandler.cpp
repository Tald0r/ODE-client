//----------------------------------------------------------------------
//
// Filename    : GCSkillInfoHandler.cpp
// Written By  : elca
// Description : 
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillInfo.h"
#include "ApplySkillInfo.h"
#include "MSkillManager.h"

//----------------------------------------------------------------------
// Rebuilds the player's skill model from the packet (gamemodel's
// ApplySkillInfo), then does the part that reaches the executable
// (MSkillAvailable.cpp): it clears the sweeper bonus skills and asks
// g_pSkillAvailable which skills can be used now. It sends nothing.
//----------------------------------------------------------------------
void GCSkillInfoHandler::execute ( GCSkillInfo * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	ApplySkillInfo( pPacket );

	//--------------------------------------------------
	// Reset on a zone move for the Holy Land bonus
	//--------------------------------------------------
//	for(int i = 0; i < HOLYLAND_BONUS_MAX; i++)
//	{
//		g_abHolyLandBonusSkills[i] = false;
//	}
	
	for( int i = 0 ; i < SWEEPER_BONUS_MAX; i ++ )
		g_abSweeperBonusSkills[i] = false;

	//--------------------------------------------------
	// Check again which skills can be used now.
	//--------------------------------------------------

	g_pSkillAvailable->SetAvailableSkills();

	__END_CATCH
}
