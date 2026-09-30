//----------------------------------------------------------------------
// ConvertDuration.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "ConvertDuration.h"
#include "ClientConfig.h"

//-----------------------------------------------------------------------------
// Convert Duration To Frame
//-----------------------------------------------------------------------------
DWORD
ConvertDurationToFrame(int duration)
{
	// 16 frames a second
	// 1.6 frames per 0.1 second
	// 1 --> 0.1 second
	// 10 --> 1 second
	return duration * g_pClientConfig->FPS / 10;
}

//-----------------------------------------------------------------------------
// Convert Duration To Millisecond
//-----------------------------------------------------------------------------
DWORD
ConvertDurationToMillisecond(int duration)
{
	// 1 --> 0.1 second
	// 1 --> 100
	// In DWORD arithmetic: a wire turn past INT_MAX arrives here
	// negative, and the product of an int would overflow. Modulo 2^32,
	// which is the value every build computed before.
	return (DWORD)duration * 100;
}
