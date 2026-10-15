#pragma once

#include "../basic/Platform.h"
#include <fstream>

class CSoundPartManager;

// The sound parts' manager InitInfomation makes (GameMain.cpp's until
// task 4.252).
extern CSoundPartManager*	g_pSoundManager;

// The start-up's loading of the game information (task 4.252: the
// executable's until then, gameclient's since): the client's settings,
// the player's options, and every table, manager and help the game reads
// from the files the FileDef properties name. Each answers FALSE when it
// cannot go on.
BOOL	InitClientConfig();
BOOL	InitUserOption();
BOOL	InitInfomation();

// Opens a data file for binary reading, the path as the game spells it
// resolved to the disk's; when it cannot, says so through the host's
// failure message and answers false.
bool	FileOpenBinary(const char* filename, std::ifstream& file);
