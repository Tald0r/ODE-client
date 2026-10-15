#pragma once

#include "../basic/Platform.h"

// What the start-up's loading of the game information (GameInitInfo.cpp,
// gameclient's since task 4.252) asks of the executable: the screen fade
// an event starts, the packet items' prototypes, how often the personal
// shops' signs refresh, and the message a failed start-up shows. Without
// a host: no fade, no prototypes, 60 seconds, and no message.
namespace GameInitInfoHost {

struct Host
{
	void (*SetFadeStart)(signed char start, signed char end, signed char step,
		BYTE red, BYTE green, BYTE blue, WORD delay) = nullptr;
	void (*InitPacketItemTable)() = nullptr;
	int (*PersonalShopUpdateTime)() = nullptr;
	void (*InitFailMessage)(const char* message) = nullptr;
	// An error the player is told of (the executable's message box).
	void (*ShowError)(const char* message) = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
void SetFadeStart(signed char start, signed char end, signed char step,
	BYTE red, BYTE green, BYTE blue, WORD delay);
void InitPacketItemTable();
int PersonalShopUpdateTime();
void InitFailMessage(const char* message);
void ShowError(const char* message);

}
