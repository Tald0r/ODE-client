//////////////////////////////////////////////////////////////////////
//
// Filename    : GCUseOKHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCUseOK.h"
#include "AffectModifyInfo.h"
#include "PlayerStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCUseOKHandler::execute ( GCUseOK * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	// message

	// Hm.. a cut & paste masterpiece.. - -;;

	PlayerState::UseItemOK();

	//------------------------------------------------------------------
	//
	//						Change the status values.
	//
	//------------------------------------------------------------------
	MStatus* pPlayerStatus = PlayerState::Status();
	if (pPlayerStatus != NULL)
	{
		AffectModifyInfo( pPlayerStatus, pPacket );
	}


	//------------------------------------------------------------------
	// Change what the UI shows.
	// Isn't this faster than comparing.. hm.. - -;
	//------------------------------------------------------------------
	//UI_SetHP( g_pPlayer->GetHP(), g_pPlayer->GetMAX_HP() );
	//UI_SetMP( g_pPlayer->GetMP(), g_pPlayer->GetMAX_MP() );


	__END_CATCH
}

void GCUseSkillCardOKHandler::execute ( GCUseSkillCardOK * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;
		

	// message

	// Hm.. a cut & paste masterpiece.. - -;;

	PlayerState::UseSkillCardOK(pPacket->getCardType());

	//------------------------------------------------------------------
	//
	//						Change the status values.
	//
	//------------------------------------------------------------------


	//------------------------------------------------------------------
	// Change what the UI shows.
	// Isn't this faster than comparing.. hm.. - -;
	//------------------------------------------------------------------
	//UI_SetHP( g_pPlayer->GetHP(), g_pPlayer->GetMAX_HP() );
	//UI_SetMP( g_pPlayer->GetMP(), g_pPlayer->GetMAX_MP() );


	__END_CATCH
}