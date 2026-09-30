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
// When the client receives a GCSkillInfo packet from the game server,
// it stores the packet's data in the client; once the data is loaded,
// it sends the game server a CGReady packet.
//
// The skill model is rebuilt by gamemodel's ApplySkillInfo; what stays
// here reaches the executable (MSkillAvailable.cpp).
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
