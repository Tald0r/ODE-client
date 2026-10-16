#include "Client_PCH.h"
#include "FrameClock.h"

MonotonicClock::TimePoint	g_FrameNow;
DWORD						g_CurrentFrame = 0;
DWORD						g_DrawAlphaNum = 256;
// The executable's until task 4.260 (CGameUpdate.cpp).
bool						g_bFrameChanged = false;
