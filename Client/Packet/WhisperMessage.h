//////////////////////////////////////////////////////////////////////
//
// Filename    : WhisperMessage.h
// Description : One whisper line as it travels in CRWhisper - wire
//               vocabulary shared by the packet and WhisperManager.
//               Lived in WhisperManager.h until docs/reviews/RESTRUCTURING.md
//               task 2.4 moved the packet classes into packetwire,
//               which may not reach the game-side manager.
//
//////////////////////////////////////////////////////////////////////

#ifndef __WHISPER_MESSAGE_H__
#define __WHISPER_MESSAGE_H__

#include "Types.h"

#include <string>

struct WHISPER_MESSAGE
{
	std::string msg;
	DWORD color;
};

#endif // __WHISPER_MESSAGE_H__
