#pragma once

#include <cstdint>

class GCFriendChatting;

namespace CommunicationPresentation {

// Friend, SMS address-book and whisper replies only present what they
// receive. A missing service skips that presentation. A packet is borrowed
// for the call, but SetSMSList's list holds the packet's AddressUnits, which
// the executable's SMS list keeps after the call. The Hide* queries are
// the player's options to hide holy-land, master-lair and war system
// messages; a missing query hides nothing.
struct Host
{
	void (*AddChatToHistory)(const char* message, const char* sender, int chatType, std::uint32_t color) = nullptr;
	void (*PopupMessage)(int gameStringID) = nullptr;
	void (*SetSMSList)(void* pAddressList) = nullptr;
	void (*AddSMSList)(std::uint32_t element, char* name, char* id, char* num) = nullptr;
	void (*DeleteSMSList)(std::uint32_t id) = nullptr;
	void (*SMS_Set_Charge)(int charge) = nullptr;
	void (*SMS_ERROR)(std::uint32_t parameter) = nullptr;
	void (*FriendChatting_UpdateFriendList)(GCFriendChatting* pPacket) = nullptr;
	void (*FriendChatting_Message)(GCFriendChatting* pPacket) = nullptr;
	void (*RunFriendRequest)(GCFriendChatting* pPacket) = nullptr;
	void (*RunFriendRefuse)(GCFriendChatting* pPacket) = nullptr;
	void (*RunFriendWait)(GCFriendChatting* pPacket) = nullptr;
	void (*RunFriendOK)(GCFriendChatting* pPacket) = nullptr;
	void (*RunFriendExist)(GCFriendChatting* pPacket) = nullptr;
	void (*RunFriendBlack)(GCFriendChatting* pPacket) = nullptr;
	void (*RunFriendDelete)(GCFriendChatting* pPacket) = nullptr;
	void (*SetRangerChatString)(const char* message) = nullptr;
	bool (*HideHolyLandMessages)() = nullptr;
	bool (*HideLairMessages)() = nullptr;
	bool (*HideWarMessages)() = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
void AddChatToHistory(const char* message, const char* sender, int chatType, std::uint32_t color);
void PopupMessage(int gameStringID);
void SetSMSList(void* pAddressList);
void AddSMSList(std::uint32_t element, char* name, char* id, char* num);
void DeleteSMSList(std::uint32_t id);
void SMS_Set_Charge(int charge);
void SMS_ERROR(std::uint32_t parameter);
void FriendChatting_UpdateFriendList(GCFriendChatting* pPacket);
void FriendChatting_Message(GCFriendChatting* pPacket);
void RunFriendRequest(GCFriendChatting* pPacket);
void RunFriendRefuse(GCFriendChatting* pPacket);
void RunFriendWait(GCFriendChatting* pPacket);
void RunFriendOK(GCFriendChatting* pPacket);
void RunFriendExist(GCFriendChatting* pPacket);
void RunFriendBlack(GCFriendChatting* pPacket);
void RunFriendDelete(GCFriendChatting* pPacket);
void SetRangerChatString(const char* message);
bool HideHolyLandMessages();
bool HideLairMessages();
bool HideWarMessages();

}
