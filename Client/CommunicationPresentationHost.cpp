#include "Client_PCH.h"
#include "CommunicationPresentationHost.h"

namespace CommunicationPresentation {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

void AddChatToHistory(const char* message, const char* sender, int chatType, std::uint32_t color)
{
	if (s_Host && s_Host->AddChatToHistory) s_Host->AddChatToHistory(message, sender, chatType, color);
}

void PopupMessage(int gameStringID)
{
	if (s_Host && s_Host->PopupMessage) s_Host->PopupMessage(gameStringID);
}

void SetSMSList(void* pAddressList)
{
	if (s_Host && s_Host->SetSMSList) s_Host->SetSMSList(pAddressList);
}

void AddSMSList(std::uint32_t element, char* name, char* id, char* num)
{
	if (s_Host && s_Host->AddSMSList) s_Host->AddSMSList(element, name, id, num);
}

void DeleteSMSList(std::uint32_t id)
{
	if (s_Host && s_Host->DeleteSMSList) s_Host->DeleteSMSList(id);
}

void SMS_Set_Charge(int charge)
{
	if (s_Host && s_Host->SMS_Set_Charge) s_Host->SMS_Set_Charge(charge);
}

void SMS_ERROR(std::uint32_t parameter)
{
	if (s_Host && s_Host->SMS_ERROR) s_Host->SMS_ERROR(parameter);
}

void FriendChatting_UpdateFriendList(GCFriendChatting* pPacket)
{
	if (s_Host && s_Host->FriendChatting_UpdateFriendList) s_Host->FriendChatting_UpdateFriendList(pPacket);
}

void FriendChatting_Message(GCFriendChatting* pPacket)
{
	if (s_Host && s_Host->FriendChatting_Message) s_Host->FriendChatting_Message(pPacket);
}

void RunFriendRequest(GCFriendChatting* pPacket)
{
	if (s_Host && s_Host->RunFriendRequest) s_Host->RunFriendRequest(pPacket);
}

void RunFriendRefuse(GCFriendChatting* pPacket)
{
	if (s_Host && s_Host->RunFriendRefuse) s_Host->RunFriendRefuse(pPacket);
}

void RunFriendWait(GCFriendChatting* pPacket)
{
	if (s_Host && s_Host->RunFriendWait) s_Host->RunFriendWait(pPacket);
}

void RunFriendOK(GCFriendChatting* pPacket)
{
	if (s_Host && s_Host->RunFriendOK) s_Host->RunFriendOK(pPacket);
}

void RunFriendExist(GCFriendChatting* pPacket)
{
	if (s_Host && s_Host->RunFriendExist) s_Host->RunFriendExist(pPacket);
}

void RunFriendBlack(GCFriendChatting* pPacket)
{
	if (s_Host && s_Host->RunFriendBlack) s_Host->RunFriendBlack(pPacket);
}

void RunFriendDelete(GCFriendChatting* pPacket)
{
	if (s_Host && s_Host->RunFriendDelete) s_Host->RunFriendDelete(pPacket);
}

void SetRangerChatString(const char* message)
{
	if (s_Host && s_Host->SetRangerChatString) s_Host->SetRangerChatString(message);
}

bool HideHolyLandMessages()
{
	return s_Host && s_Host->HideHolyLandMessages ? s_Host->HideHolyLandMessages() : false;
}

bool HideLairMessages()
{
	return s_Host && s_Host->HideLairMessages ? s_Host->HideLairMessages() : false;
}

bool HideWarMessages()
{
	return s_Host && s_Host->HideWarMessages ? s_Host->HideWarMessages() : false;
}

void CloseParty()
{
	if (s_Host && s_Host->CloseParty) s_Host->CloseParty();
}

void ClosePartyCancel()
{
	if (s_Host && s_Host->ClosePartyCancel) s_Host->ClosePartyCancel();
}

void ClosePartyAsk()
{
	if (s_Host && s_Host->ClosePartyAsk) s_Host->ClosePartyAsk();
}

void ClosePartyRequest()
{
	if (s_Host && s_Host->ClosePartyRequest) s_Host->ClosePartyRequest();
}

void RunPartyRequest(std::uint32_t inviterID)
{
	if (s_Host && s_Host->RunPartyRequest) s_Host->RunPartyRequest(inviterID);
}

}
