#include "Client_PCH.h"
#include "FrameClock.h"

MonotonicClock::TimePoint	g_FrameNow;
DWORD						g_CurrentFrame = 0;
DWORD						g_DrawAlphaNum = 256;
