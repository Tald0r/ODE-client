//////////////////////////////////////////////////////////////////////
//
// Filename    : GCUseBonusPointOKHandler.cc
// Written By  : crazydog
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCUseBonusPointOK.h"
#include "Cpackets/CGUseBonusPoint.h"
#include "TempInformation.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "MStatus.h"
#include "PlayerStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCUseBonusPointOKHandler::execute ( GCUseBonusPointOK * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		


	//------------------------------------------------------------------
	// Apply the changed status values.
	//------------------------------------------------------------------
	AffectModifyInfo(PlayerState::Status(), pPacket);

	//------------------------------------------------------------
	// Was this the answer to spending a bonus point?
	//------------------------------------------------------------
	if (g_pTempInformation->GetMode()==TempInformation::MODE_BONUSPOINT_USE)
	{
		//------------------------------------------------------------
		// Clear the request.
		//------------------------------------------------------------
		g_pTempInformation->SetMode(TempInformation::MODE_NULL);

		int bonusPoint = PlayerState::Status()->GetBonusPoint();

		//------------------------------------------------------------
		// Take one bonus point away.
		//------------------------------------------------------------
		if (bonusPoint-1 >= 0)
		{
			PlayerState::Status()->SetStatus( MODIFY_BONUS_POINT, bonusPoint-1 );
		}
		else
		{
			DEBUG_ADD_ERR("[Error] Minus BonusPoint");
		}
	}


	__END_CATCH
}
