#pragma once

#include <vector>

class NicknameInfo;

namespace VsUiPresentation {

// The UI object's (gC_vs_ui) state and lists that replies read or fill
// directly: which inventory view is open, and the player's nickname list.
// A missing host or entry answers inventory mode 0 and skips the lists.
struct Host
{
	int (*InventoryMode)() = nullptr;
	void (*AddNickNameList)(NicknameInfo* pInfo) = nullptr;
	void (*SetNickNameList)(std::vector<NicknameInfo*>* pList) = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
int InventoryMode();
void AddNickNameList(NicknameInfo* pInfo);
void SetNickNameList(std::vector<NicknameInfo*>* pList);

}
