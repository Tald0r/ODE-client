#pragma once

#include <cstdint>
#include <vector>

class MItem;
class MStorage;
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
	// released. SetQuestManagerInfo keeps a record when it answers true
	// (except for a quest with no XML element, which leaks it, as
	// ModifyQuestManagerInfo does); false (no game window, or no host or
	// entry) leaves it with the caller.
	bool (*IsRunningQuestManager)() = nullptr;
	void (*CloseQuestManager)() = nullptr;
	void (*RunQuestManager)() = nullptr;
	bool (*SetQuestManagerInfo)(UI_GQuestInfo* pInfo) = nullptr;
	void (*ReleaseQuestXML)() = nullptr;
	// The personal shop window. Its mode says why a shop's contents come
	// (1 the player opening its own shop to stock it, 2 opening it for
	// sale, 3 closing it; 0 otherwise); SetPersnalShop shows a storage's
	// items, borrowed. SetInventoryMode switches the inventory to one
	// beside a shop (2). Without a host the window is not running, its
	// mode is 0, and the rest is skipped.
	int (*GetPersnalShoptoUI)() = nullptr;
	void (*SetPersnalShoptoUI)(int mode) = nullptr;
	bool (*IsRunningPersnalShop)() = nullptr;
	void (*RunPersnalShop)() = nullptr;
	void (*ClosePersnalShop)() = nullptr;
	void (*SetPersnalShop)(MStorage* pStorage) = nullptr;
	void (*SetInventoryMode)(int mode) = nullptr;
	// An operator's "*C2G on" puts each item it then creates ("*create") on
	// at once: IsCreatingToGear says one is pending, and MoveCreatedToGear
	// moves the item at the inventory cell to the gear (one fewer pending).
	// Nothing is pending without a host.
	bool (*IsCreatingToGear)() = nullptr;
	void (*MoveCreatedToGear)(int x, int y) = nullptr;
	// What using an item opens (task 4.237): LockGear keeps the belt or
	// arms band from being taken off while a quick slot item is in use;
	// the mixing forge for a class (FORGE_CLASS) and a type (0 for its
	// first, A); the bulletin board, the Christmas card and the quest
	// inventory windows for the item. All are skipped without a host.
	void (*LockGear)() = nullptr;
	void (*RunMixingForge)(int forgeClass, int forgeType) = nullptr;
	void (*RunBulletinBoardWindow)(const MItem* pItem) = nullptr;
	void (*RunXmasCardWindow)(const MItem* pItem) = nullptr;
	void (*RunQuestInventory)(const MItem* pItem) = nullptr;
};

// The mixing forge's item classes, in C_VS_UI_MIXING_FORGE's order.
enum FORGE_CLASS
{
	FORGE_CLASS_WEAPON,
	FORGE_CLASS_ARMOR,
	FORGE_CLASS_ACCESSORY,
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
int GetPersnalShoptoUI();
void SetPersnalShoptoUI(int mode);
bool IsRunningPersnalShop();
void RunPersnalShop();
void ClosePersnalShop();
void SetPersnalShop(MStorage* pStorage);
void SetInventoryMode(int mode);
bool IsCreatingToGear();
void MoveCreatedToGear(int x, int y);
void LockGear();
void RunMixingForge(int forgeClass, int forgeType);
void RunBulletinBoardWindow(const MItem* pItem);
void RunXmasCardWindow(const MItem* pItem);
void RunQuestInventory(const MItem* pItem);

}
