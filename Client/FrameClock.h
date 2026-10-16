#pragma once

#include "MonotonicClock.h"
#include "../basic/Platform.h"

// The frame clock: the time the current frame began, stamped by the main
// loop each frame (Client.cpp), and again when a wait for packets begins
// and after a zone load. Game logic compares deadlines with it instead of
// reading the clock itself, so code between two stamps sees one time.
extern MonotonicClock::TimePoint	g_FrameNow;

// The frame count, which the main loop advances each frame, and how far
// the drawing is between the last frame and the next (0 to 256), which
// moving creatures' drawn positions interpolate by (task 4.240: the
// executable's until then).
extern DWORD						g_CurrentFrame;
extern DWORD						g_DrawAlphaNum;

// Whether the frame count advanced since the last draw (the main loop sets
// it and clears it once the frame is drawn); the view's fades and
// animations step only then. The executable's until task 4.260.
extern bool							g_bFrameChanged;
