#pragma once

#include <cstdint>

class MItem;
class MShop;
class MStorage;

namespace DialogPresentation {

// Replies that end in a message box or release a locked item only present
// what they receive. A missing service skips that presentation, and a
// missing swap query answers that no advancement-item swap is running.
// RunPetStorage opens the pet storage window, SetPetStorage shows a
// storage in it. The window keeps the pointer and draws from it every
// frame, so the storage must outlive it (the handler's is g_pStorage).
// EmptyMove replays the mouse position once, so the UI under it refreshes.
// RunStorage and SetStorage do for the stash window what the pet ones do
// for the pet storage window, which also keeps the storage's pointer.
// PickUpItem puts an item back on the mouse; GetMouseItem is the item on
// it (NULL for none, or without a host), and DropItem lets it go (the
// window that took it keeps it). UnlockGear lifts the gear window's lock
// (every item check locks it); CloseMixingForge closes the mixing forge.
// PopupPCTalkDlg shows the NPC talk box (g_pPCTalkBox) at the centre;
// ShowPCTalkDlg shows it again as it was. RunStorageBuy asks whether to buy
// a storage at the price. OpenInventoryToSell opens the inventory to sell
// to a shop. CloseRequestResurrectWindow closes the window asking to be
// resurrected. SetShop names the shop the shop window shows; RunShop
// opens that window. IsLockInput answers whether the game's input is
// locked (the player is talking to an NPC); false without a host.
struct Host
{
	void (*PopupFreeMessageDlg)(const char* message) = nullptr;
	void (*UnlockItem)() = nullptr;
	void (*UnlockItemTrade)() = nullptr;
	void (*RemoveItemFromItemShop)(std::uint32_t id) = nullptr;
	bool (*IsRunningSwapAdvancementItem)() = nullptr;
	void (*RunPetStorage)() = nullptr;
	void (*SetPetStorage)(MStorage* pStorage) = nullptr;
	void (*EmptyMove)() = nullptr;
	void (*RunStorage)() = nullptr;
	void (*SetStorage)(MStorage* pStorage) = nullptr;
	void (*PickUpItem)(MItem* pItem) = nullptr;
	MItem* (*GetMouseItem)() = nullptr;
	void (*DropItem)() = nullptr;
	void (*UnlockGear)() = nullptr;
	void (*CloseMixingForge)() = nullptr;
	void (*PopupPCTalkDlg)() = nullptr;
	void (*ShowPCTalkDlg)() = nullptr;
	void (*RunStorageBuy)(int price) = nullptr;
	void (*OpenInventoryToSell)() = nullptr;
	void (*CloseRequestResurrectWindow)() = nullptr;
	void (*SetShop)(MShop* pShop) = nullptr;
	void (*RunShop)() = nullptr;
	bool (*IsLockInput)() = nullptr;
	// The mixing forge's two items made one (parameter, the server's): when
	// the first is in the inventory and both items' options match it, the
	// host gives the first the second's option and the higher grade, and
	// takes the second out of the inventory and frees it; otherwise it
	// shows an error and frees nothing. OkRemoveOption shows an option taken
	// off an item, which stays the inventory's.
	void (*OkMixingForge)(std::uint32_t parameter, MItem* pItem, MItem* pItem2) = nullptr;
	void (*OkRemoveOption)(std::uint32_t parameter, MItem* pItem) = nullptr;
	// Closes the NPC talk window; UnSetLockInputPCTalk lets the player act
	// again while it talks.
	void (*ClosePCTalkDlg)() = nullptr;
	void (*UnSetLockInputPCTalk)() = nullptr;
	// The windows an NPC talk choice waits for (task 4.238): whether the
	// stash, the shop, the bookcase, the repair, the sell or the party
	// window runs (false without a host); CloseAllDialog closes the game's
	// other windows before a talk box opens.
	bool (*IsRunningStorage)() = nullptr;
	bool (*IsRunningShop)() = nullptr;
	bool (*IsRunningBookcase)() = nullptr;
	bool (*IsRunningRepair)() = nullptr;
	bool (*IsRunningSell)() = nullptr;
	bool (*IsRunningParty)() = nullptr;
	void (*CloseAllDialog)() = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
void PopupFreeMessageDlg(const char* message);
void UnlockItem();
void UnlockItemTrade();
void RemoveItemFromItemShop(std::uint32_t id);
bool IsRunningSwapAdvancementItem();
void RunPetStorage();
void SetPetStorage(MStorage* pStorage);
void EmptyMove();
void RunStorage();
void SetStorage(MStorage* pStorage);
void PickUpItem(MItem* pItem);
MItem* GetMouseItem();
void DropItem();
void UnlockGear();
void CloseMixingForge();
void PopupPCTalkDlg();
void ShowPCTalkDlg();
void RunStorageBuy(int price);
void OpenInventoryToSell();
void CloseRequestResurrectWindow();
void SetShop(MShop* pShop);
void RunShop();
bool IsLockInput();
void OkMixingForge(std::uint32_t parameter, MItem* pItem, MItem* pItem2);
void OkRemoveOption(std::uint32_t parameter, MItem* pItem);
void ClosePCTalkDlg();
void UnSetLockInputPCTalk();
bool IsRunningStorage();
bool IsRunningShop();
bool IsRunningBookcase();
bool IsRunningRepair();
bool IsRunningSell();
bool IsRunningParty();
void CloseAllDialog();

}
