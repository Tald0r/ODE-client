#pragma once

#include "MonotonicClock.h"

// The frame clock: the time the current frame began, stamped by the main
// loop each frame (Client.cpp), and again after a wait for packets and
// after a zone load. Game logic compares deadlines with it instead of
// reading the clock itself, so code between two stamps sees one time.
extern MonotonicClock::TimePoint	g_FrameNow;
