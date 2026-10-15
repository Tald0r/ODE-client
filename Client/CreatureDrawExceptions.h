//----------------------------------------------------------------------
// CreatureDrawExceptions.h
//----------------------------------------------------------------------
// How some creature types are drawn differently from their action (task
// 4.229). A procedure gets the action, frame and direction about to be
// drawn, and the frame counter, and may change them; it reaches the
// creature through CreatureState.
//----------------------------------------------------------------------
#ifndef __CREATURE_DRAW_EXCEPTIONS_H__
#define __CREATURE_DRAW_EXCEPTIONS_H__

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include <map>

class MCreature;

typedef void (*DrawCreatureExceptionProc)( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame );

// Fills the table: each creature type with its procedure.
void	MapCreatureDrawExceptions( std::map< DWORD, DrawCreatureExceptionProc >& procs );

#endif
