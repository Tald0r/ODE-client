#pragma once

#include <cstdint>

class PCSlayerInfo;
class PCVampireInfo;
class PCOustersInfo;

namespace LoginPresentation {

// Login-server replies present what they receive and move the client between
// login modes through this host; the game server's first reply also places
// the player and enters the game here. Enter* sets the mode now (SetMode); Queue*
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
	// Keeps the login ID for the next login's ID box.
	void (*BackupLoginID)(const char* id) = nullptr;
	// Sets the mode that waits for the world list now.
	void (*EnterWaitWorldListMode)() = nullptr;
	// The server group the account joined, kept for the world screens.
	void (*SetServerGroupName)(const char* name) = nullptr;
	// Shows blood and gore, or hides them.
	void (*SetGoreLevel)(bool gore) = nullptr;
	// The player's option for the teen version (no gore); false without a
	// host.
	bool (*UseTeenVersion)() = nullptr;
	// Sends what the game connection has waiting to go (UpdateSocketOutput).
	void (*FlushSocketOutput)() = nullptr;
	// Enters the mode that waits for the character list.
	void (*EnterWaitPCListMode)() = nullptr;
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
void BackupLoginID(const char* id);
void EnterWaitWorldListMode();
void SetServerGroupName(const char* name);
void SetGoreLevel(bool gore);
bool UseTeenVersion();
void FlushSocketOutput();
void EnterWaitPCListMode();

}
