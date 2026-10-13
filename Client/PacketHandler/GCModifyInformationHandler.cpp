//////////////////////////////////////////////////////////////////////
//
// Filename    : GCModifyInformationHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCModifyInformation.h"
#include "AffectModifyInfo.h"
#include "ConvertDuration.h"
#include "MStatus.h"
#include "PlayerStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCModifyInformationHandler::execute ( GCModifyInformation * pPacket, Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	// message

	//------------------------------------------------------------------
	// 상태값을 바꾼다.
	//------------------------------------------------------------------
	AffectModifyInfo(PlayerState::Status(), pPacket);

	//------------------------------------------------------------------
	// effect status를 적용시킨다.
	//------------------------------------------------------------------
	if (PlayerState::Status()->GetEFFECT_STAT()!=EFFECTSTATUS_NULL)
	{
		int esDelayFrame = ConvertDurationToFrame( PlayerState::Status()->GetDURATION() );

		// effect를 붙인다.
		PlayerState::AddEffectStatus((EFFECTSTATUS)PlayerState::Status()->GetEFFECT_STAT(), esDelayFrame);	
		
		PlayerState::Status()->SetStatus( MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL );
	}

	//------------------------------------------------------------------
	// UI에 보이는 것을 바꿔준다.
	// 비교연산하는거보다 이게 더 빠르지 않을까.. 음.. - -;
	//------------------------------------------------------------------
	//UI_SetHP( g_pPlayer->GetHP(), g_pPlayer->GetMAX_HP() );
	//UI_SetMP( g_pPlayer->GetMP(), g_pPlayer->GetMAX_MP() );


	__END_CATCH
}
