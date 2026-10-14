#pragma once

#include <cstdint>

class PCSlayerInfo;
class PCVampireInfo;
class PCOustersInfo;

namespace LoginPresentation {

// Login-server replies present what they receive and move the client between
// login modes through this host. Enter* sets the mode now (SetMode); Queue*
// sets the mode the main loop switches to next (g_ModeNext). Sleep stalls the
// caller as the reply always has. A missing service is skipped, and a missing
// mode query answers that the client is not choosing a character.
struct Host
{
	void (*NewCharacterCreateOk)() = nullptr;
	void (*DeleteCharacterOK)() = nullptr;
	void (*AlreadyExistIDMessage)() = nullptr;
	void (*NoAlreadyExistIDMessage)() = nullptr;
	void (*StartCharacterManager)() = nullptr;
	void (*SetSlayerCharacter)(int slot, PCSlayerInfo* pInfo) = nullptr;
	void (*SetVampireCharacter)(int slot, PCVampireInfo* pInfo) = nullptr;
	void (*SetOustersCharacter)(int slot, PCOustersInfo* pInfo) = nullptr;
	void (*RunAskGoBilingPage)(int n) = nullptr;
	void (*PopupExitMessageDlg)(const char* message) = nullptr;
	void (*Sleep)(std::uint32_t milliseconds) = nullptr;
	void (*EnterSelectPCMode)() = nullptr;
	void (*QueueWaitPCListMode)() = nullptr;
	void (*QueueLoginWrongMode)() = nullptr;
	bool (*IsSelectPCMode)() = nullptr;
	void (*PopupErrorMessage)(int errorID) = nullptr;
	void (*QueueWaitSelectPCMode)() = nullptr;
	void (*ShowMessage)(const char* message) = nullptr;
	// Places the player at the cell, facing the direction, for the game.
	void (*InitPlayer)(int x, int y, int direction) = nullptr;
	// Sets the game mode now.
	void (*EnterGameMode)() = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
void NewCharacterCreateOk();
void DeleteCharacterOK();
void AlreadyExistIDMessage();
void NoAlreadyExistIDMessage();
void StartCharacterManager();
void SetSlayerCharacter(int slot, PCSlayerInfo* pInfo);
void SetVampireCharacter(int slot, PCVampireInfo* pInfo);
void SetOustersCharacter(int slot, PCOustersInfo* pInfo);
void RunAskGoBilingPage(int n);
void PopupExitMessageDlg(const char* message);
void Sleep(std::uint32_t milliseconds);
void EnterSelectPCMode();
void QueueWaitPCListMode();
void QueueLoginWrongMode();
bool IsSelectPCMode();
void PopupErrorMessage(int errorID);
void QueueWaitSelectPCMode();
void ShowMessage(const char* message);
void InitPlayer(int x, int y, int direction);
void EnterGameMode();

}
