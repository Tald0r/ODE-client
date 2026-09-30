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
	return duration * 100;
}
