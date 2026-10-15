#pragma once

#include <cstdint>

// The player's options that game objects read (task 4.242; the options are
// VS_UI's UserOption): whether creatures low on health drip blood,
// whether every chat line shows white, and the colour of the player's own
// chat. Without a host: no blood, own colours, and white.
namespace GameOptions {

struct Host
{
	bool (*BloodDrop)() = nullptr;
	bool (*ChatWhite)() = nullptr;
	std::uint32_t (*ChattingColor)() = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
bool BloodDrop();
bool ChatWhite();
std::uint32_t ChattingColor();

}
