#pragma once

#include <cstdint>

namespace NoticePresentation {

// What the server's event notices show beyond the event queue and the
// message logs: notice windows, popups, a few game windows, and the two
// switches the notices flip. A missing service skips it.
struct Host
{
	// A notice window of the kind (1 the beginner zone, 2 the flag war, 3
	// and 4 the level war, 5 the race war, 7 the pet quest, 8 an SMS) with
	// its parameter; DeleteNotice closes the kind's window (6 the race
	// war's end).
	void (*RunNotice)(std::uint32_t kind, std::uint32_t parameter) = nullptr;
	void (*DeleteNotice)(std::uint32_t kind) = nullptr;
	void (*RunWelcomeMessage)() = nullptr;
	void (*LotteryResult)(bool bSuccess) = nullptr;
	// The horn's window for the zone the player is in.
	void (*RunHorn)(int zoneID) = nullptr;
	// The master's lair opening, closing or counting down (type 0, 1, 2).
	void (*MasterLairMessage)(std::uint8_t type, short value, std::uint16_t zoneID) = nullptr;
	// A popup of the game string, or of the text (large or normal).
	void (*PopupMessage)(int gameStringID) = nullptr;
	void (*PopupText)(const char* text, bool bLarge) = nullptr;
	void (*RunMinigame)(int game) = nullptr;
	void (*CloseCTFStatusWindow)() = nullptr;
	// Sends the crash report the last run left, if any.
	void (*SendCrashReport)() = nullptr;
	// Shows the national flags of a holiday, or hides them.
	void (*SetFlagTo)(bool bOn) = nullptr;
	// The quest's ending starts.
	void (*StartQuestEnding)() = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
void RunNotice(std::uint32_t kind, std::uint32_t parameter);
void DeleteNotice(std::uint32_t kind);
void RunWelcomeMessage();
void LotteryResult(bool bSuccess);
void RunHorn(int zoneID);
void MasterLairMessage(std::uint8_t type, short value, std::uint16_t zoneID);
void PopupMessage(int gameStringID);
void PopupText(const char* text, bool bLarge);
void RunMinigame(int game);
void CloseCTFStatusWindow();
void SendCrashReport();
void SetFlagTo(bool bOn);
void StartQuestEnding();

}
