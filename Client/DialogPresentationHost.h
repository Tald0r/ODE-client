#pragma once

#include <cstdint>

class MItem;
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
// PopupPCTalkDlg shows the NPC talk box (g_pPCTalkBox) at the centre.
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

}
