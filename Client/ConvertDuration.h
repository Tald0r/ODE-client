//----------------------------------------------------------------------
// ConvertDuration.h
//----------------------------------------------------------------------
// The server sends durations in tenths of a second (a "duration" of 10
// is one second). These convert one into the client's two time units.
// Kept free of game state, other than the configured frame rate, so
// that the model code which reads a duration off a packet can be
// tested on its own. ClientDef.h includes this header, which is where
// the packet handlers find the two functions.
//----------------------------------------------------------------------

#ifndef __CONVERTDURATION_H__
#define __CONVERTDURATION_H__

#include "Platform.h"

//----------------------------------------------------------------------
// ConvertDurationToFrame
//----------------------------------------------------------------------
// The number of frames a duration lasts at g_pClientConfig->FPS frames
// a second: duration * FPS / 10. g_pClientConfig must exist.
//----------------------------------------------------------------------
extern DWORD	ConvertDurationToFrame(int duration);

//----------------------------------------------------------------------
// ConvertDurationToMillisecond
//----------------------------------------------------------------------
// The duration in milliseconds: duration * 100, taken as a DWORD, so
// modulo 2^32 for a duration past INT_MAX / 100 or below 0.
//----------------------------------------------------------------------
extern DWORD	ConvertDurationToMillisecond(int duration);

#endif
