#pragma once

#include "MonotonicClock.h"

// The frame clock: the time the current frame began, stamped once per
// frame by the main loop (Client.cpp). Game logic compares deadlines with
// it instead of reading the clock itself, so one frame sees one time.
extern MonotonicClock::TimePoint	g_FrameNow;
