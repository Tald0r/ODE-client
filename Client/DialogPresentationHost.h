#pragma once

#include <cstdint>

namespace DialogPresentation {

// Replies that end in a message box or release a locked item only present
// what they receive. A missing service skips that presentation, and a
// missing swap query answers that no advancement-item swap is running.
struct Host
{
	void (*PopupFreeMessageDlg)(const char* message) = nullptr;
	void (*UnlockItem)() = nullptr;
	void (*UnlockItemTrade)() = nullptr;
	void (*RemoveItemFromItemShop)(std::uint32_t id) = nullptr;
	bool (*IsRunningSwapAdvancementItem)() = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
void PopupFreeMessageDlg(const char* message);
void UnlockItem();
void UnlockItemTrade();
void RemoveItemFromItemShop(std::uint32_t id);
bool IsRunningSwapAdvancementItem();

}
