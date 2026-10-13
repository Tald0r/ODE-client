#pragma once

#include <cstdint>

class MStorage;

namespace DialogPresentation {

// Replies that end in a message box or release a locked item only present
// what they receive. A missing service skips that presentation, and a
// missing swap query answers that no advancement-item swap is running.
// RunPetStorage opens the pet storage window, SetPetStorage shows a
// storage in it (borrowed for the call: the window copies what it shows).
// EmptyMove replays the mouse position once, so the UI under it refreshes.
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

}
