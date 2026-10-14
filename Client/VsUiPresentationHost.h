#pragma once

#include <cstdint>
#include <vector>

class MItem;
class NicknameInfo;
struct UI_GQuestInfo;

namespace VsUiPresentation {

// The UI object's (gC_vs_ui) state and lists that replies read or fill
// directly: which inventory view is open, and the player's nickname list.
// A missing host or entry answers inventory mode 0 and skips the lists.
// AddMail puts a letter on a mail tab, its window width by height, and
// AddMailContents gives a letter its text; both skip without the entry.
// PushGQuestExecuteElement runs a quest's element; UpdateQuestItemInfo
// hands the quest items over to the quest window, which keeps them, and
// answers false when no host entry took them (the caller frees them); an
// entry that has no quest window behind it leaks them, as the game did.
// ModifyQuestManagerInfo hands a quest's new status to the quest window,
// which keeps it when it answers true (except for a quest with no XML
// element, which leaks it); false (or no entry) leaves it with the caller.
// ChangeCustomNaming shows a custom nickname in the naming window, and
// CloseNamingChange closes it. SelectSkill and UnselectSkill set the
// skill in hand. SetPowerjjangPoint sets the player's power points, which
// the power points window shows and gates its gamble on;
// PowerjjangGambleResult shows what a points gamble won;
// RefreshPointExchange reloads the point exchange window.
struct Host
{
	int (*InventoryMode)() = nullptr;
	void (*AddNickNameList)(NicknameInfo* pInfo) = nullptr;
	void (*SetNickNameList)(std::vector<NicknameInfo*>* pList) = nullptr;
	void (*AddMail)(int tabID, std::uint32_t id, int width, int height, const char* sender, const char* title, std::uint32_t date, bool open, int guildID, int guildType) = nullptr;
	void (*AddMailContents)(int tabID, std::uint32_t id, const char* contents) = nullptr;
	void (*PushGQuestExecuteElement)(std::uint32_t questID, std::uint8_t condition, std::uint16_t index) = nullptr;
	void (*UpdateQuestItemInfo)(std::vector<MItem*>& items) = nullptr;
	bool (*ModifyQuestManagerInfo)(UI_GQuestInfo* pInfo, int type) = nullptr;
	void (*ChangeCustomNaming)(int id, const char* name) = nullptr;
	void (*CloseNamingChange)() = nullptr;
	void (*SelectSkill)(int skillID) = nullptr;
	void (*UnselectSkill)() = nullptr;
	void (*SetPowerjjangPoint)(int points) = nullptr;
	void (*PowerjjangGambleResult)(int itemCode) = nullptr;
	void (*RefreshPointExchange)() = nullptr;
	// The quest window, rebuilt from a full quest list: it is closed if it
	// runs, opened (loading its quest XML), given each quest, and the XML
	// released. SetQuestManagerInfo keeps a record it takes and answers
	// false for one it refuses, which stays the caller's; without a host
	// it refuses every record.
	bool (*IsRunningQuestManager)() = nullptr;
	void (*CloseQuestManager)() = nullptr;
	void (*RunQuestManager)() = nullptr;
	bool (*SetQuestManagerInfo)(UI_GQuestInfo* pInfo) = nullptr;
	void (*ReleaseQuestXML)() = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
int InventoryMode();
void AddNickNameList(NicknameInfo* pInfo);
void SetNickNameList(std::vector<NicknameInfo*>* pList);
void AddMail(int tabID, std::uint32_t id, int width, int height, const char* sender, const char* title, std::uint32_t date, bool open, int guildID, int guildType);
void AddMailContents(int tabID, std::uint32_t id, const char* contents);
void PushGQuestExecuteElement(std::uint32_t questID, std::uint8_t condition, std::uint16_t index);
bool UpdateQuestItemInfo(std::vector<MItem*>& items);
bool ModifyQuestManagerInfo(UI_GQuestInfo* pInfo, int type);
void ChangeCustomNaming(int id, const char* name);
void CloseNamingChange();
void SelectSkill(int skillID);
void UnselectSkill();
void SetPowerjjangPoint(int points);
void PowerjjangGambleResult(int itemCode);
void RefreshPointExchange();
bool IsRunningQuestManager();
void CloseQuestManager();
void RunQuestManager();
bool SetQuestManagerInfo(UI_GQuestInfo* pInfo);
void ReleaseQuestXML();

}
