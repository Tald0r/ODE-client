//--------------------------------------------------------------------------------
//
// Filename    : GCAddressListVerifyHandler.cpp
// Written By  : elca, Reiot
// Description :
//
//--------------------------------------------------------------------------------
#include "Client_PCH.h"
// include files
#include "Gpackets/GCAddressListVerify.h"

	#include "ClientPlayer.h"

#include "TempInformation.h"
#include "CommunicationPresentationHost.h"
#include "MGameStringTable.h"
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCAddressListVerifyHandler::execute ( GCAddressListVerify * pPacket , Player * pPlayer )

{
	__BEGIN_TRY /*__BEGIN_DEBUG_EX*/
	(void)pPlayer;
	/*__BEGIN_DEBUG*/
		
	
	if(NULL == pPacket)
		return;

	switch(pPacket->getCode())
	{
	case GCAddressListVerify::ADDRESS_LIST_ADD_OK:
		if (g_pTempInformation->GetMode()==TempInformation::MODE_SMS_ADD_ADDRESS)
			CommunicationPresentation::AddSMSList(pPacket->getParameter(),
						  (char*)g_pTempInformation->StrValue1.c_str(),
						  (char*)g_pTempInformation->StrValue2.c_str(),
						  (char*)g_pTempInformation->StrValue3.c_str());
		break;
	case GCAddressListVerify::ADDRESS_LIST_DELETE_OK:
		if (g_pTempInformation->GetMode()==TempInformation::MODE_SMS_DELETE_ADDRESS)
			CommunicationPresentation::DeleteSMSList(static_cast<DWORD>(g_pTempInformation->Value1)); 
		break;
	case GCAddressListVerify::ADDRESS_LIST_ADD_FAIL:
		if(pPacket->getParameter())
			CommunicationPresentation::SMS_ERROR(pPacket->getParameter());
		else
			CommunicationPresentation::PopupMessage(UI_STRING_MESSAGE_SMS_ADD_FAIL);
		break;
	case GCAddressListVerify::ADDRESS_LIST_DELETE_FAIL:
		if(pPacket->getParameter())
			CommunicationPresentation::SMS_ERROR(pPacket->getParameter());
		else
			CommunicationPresentation::PopupMessage(UI_STRING_MESSAGE_SMS_DELETE_FAIL);
		break;
	case GCAddressListVerify::SMS_SEND_OK:
		CommunicationPresentation::SMS_Set_Charge(pPacket->getParameter());
		CommunicationPresentation::PopupMessage(UI_STRING_MESSAGE_SMS_SEND_OK);
		break;
	case GCAddressListVerify::SMS_SEND_FAIL:
		if(pPacket->getParameter())
			CommunicationPresentation::SMS_ERROR(pPacket->getParameter());
		else
			CommunicationPresentation::PopupMessage(UI_STRING_MESSAGE_SMS_SEND_FAIL);
		break;
	case GCAddressListVerify::SMS_CHARGE_OK: 
		CommunicationPresentation::SMS_Set_Charge(pPacket->getParameter());
		break;
	};

	g_pTempInformation->SetMode(TempInformation::MODE_NULL); 



	/*__END_DEBUG*/
	/*__END_DEBUG_EX*/ __END_CATCH
}
