#pragma once

#include <cstdint>

class MItem;
struct PETINFO;

namespace PlayerPresentation {

// What the player (MPlayer, gamemodel's since task 4.247) asks of the UI
// object (gC_vs_ui), the mouse pointer, the force feedback device and the
// UI functions of GameUI.cpp. A missing host or entry does nothing and
// answers false, NULL or "".
struct Host
{
	// gC_vs_ui's: whether the inventory or the gear window is open; the
	// trace window run, or marked unable to trace; the elevator window
	// run; the death and resurrection requests finished and the shrine's
	// minimap closed; the mine and bomb progress bars (started at an
	// inventory grid cell, asked about, ended); the chat input line read
	// and added to.
	bool (*GetInventoryOpenState)() = nullptr;
	bool (*GetGearOpenState)() = nullptr;
	void (*RunTraceWindow)() = nullptr;
	void (*SetCannotTrace)() = nullptr;
	void (*RunElevator)() = nullptr;
	void (*FinishRequestDie)() = nullptr;
	void (*FinishRequestResurrect)() = nullptr;
	void (*CloseRequestShrineMinimap)() = nullptr;
	bool (*IsInstallMineProgress)() = nullptr;
	void (*StartInstallMineProgress)(int gridX, int gridY) = nullptr;
	void (*StartCreateMineProgress)(int gridX, int gridY) = nullptr;
	void (*StartCreateBombProgress)(int gridX, int gridY) = nullptr;
	void (*EndInstallMineProgress)() = nullptr;
	const char* (*GetInputString)() = nullptr;
	void (*AddInputString)(const char* pString) = nullptr;
	// The mouse pointer: the item it carries, and its attack cursor.
	MItem* (*GetPickUpItem)() = nullptr;
	void (*SetCursorAttack)() = nullptr;
	// Force feedback: whether the option is on and a device present, and
	// the effect of a sound played on it.
	bool (*ForceFeelReady)() = nullptr;
	void (*ForceAction)(int soundID) = nullptr;
	// The character window's slot: when the player turns vampire, and its
	// silver damage (the least, the most).
	void (*SetChangeVampireTime)(int time) = nullptr;
	void (*SetSilverDamage)(int minDamage, int maxDamage) = nullptr;
	// GameUI.cpp's UI functions: a pet's window, an item's descriptor
	// taken away, the items and the trade locked, the damage shown (the
	// most, the least), a party member's or a trade's cancel asked, the
	// way point selection closed, an effect status shown and taken away,
	// the level-up button finished (UIMessageManager's).
	void (*RunPetInfo)(PETINFO* pPetInfo) = nullptr;
	void (*RemoveDescriptor)(void* pPtr) = nullptr;
	void (*LockItem)() = nullptr;
	void (*LockItemTrade)() = nullptr;
	void (*SetCharInfoDAM)(int value, int value2) = nullptr;
	void (*RunPartyCancel)(const char* pName) = nullptr;
	void (*RunExchangeCancel)(const char* pName) = nullptr;
	void (*CloseSelectWayPoint)() = nullptr;
	void (*AddEffectStatus)(int status, std::uint32_t delayFrame) = nullptr;
	void (*RemoveEffectStatus)(int status) = nullptr;
	void (*FinishLevelUpButton)() = nullptr;
	// The quick slot's hot key to the item or skill id.
	void (*SetHotKey)(int hotkey, int id) = nullptr;
};

// Borrowed until replaced; each call reads the current host.
const Host* SetHost(const Host* host);

bool GetInventoryOpenState();
bool GetGearOpenState();
void RunTraceWindow();
void SetCannotTrace();
void RunElevator();
void FinishRequestDie();
void FinishRequestResurrect();
void CloseRequestShrineMinimap();
bool IsInstallMineProgress();
void StartInstallMineProgress(int gridX, int gridY);
void StartCreateMineProgress(int gridX, int gridY);
void StartCreateBombProgress(int gridX, int gridY);
void EndInstallMineProgress();
const char* GetInputString();
void AddInputString(const char* pString);
MItem* GetPickUpItem();
void SetCursorAttack();
bool ForceFeelReady();
void ForceAction(int soundID);
void SetChangeVampireTime(int time);
void SetSilverDamage(int minDamage, int maxDamage);
void RunPetInfo(PETINFO* pPetInfo);
void RemoveDescriptor(void* pPtr);
void LockItem();
void LockItemTrade();
void SetCharInfoDAM(int value, int value2);
void RunPartyCancel(const char* pName);
void RunExchangeCancel(const char* pName);
void CloseSelectWayPoint();
void AddEffectStatus(int status, std::uint32_t delayFrame);
void RemoveEffectStatus(int status);
void FinishLevelUpButton();
void SetHotKey(int hotkey, int id);

}
