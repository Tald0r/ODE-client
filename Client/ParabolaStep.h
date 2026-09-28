//----------------------------------------------------------------------
// ParabolaStep.h
//----------------------------------------------------------------------
// The arc step of a parabola effect, kept free of game state so that it
// can be tested on its own.
//----------------------------------------------------------------------

#ifndef __PARABOLASTEP_H__
#define __PARABOLASTEP_H__

#include "MathTable.h"

//----------------------------------------------------------------------
// ParabolaRadStep
//----------------------------------------------------------------------
// The angle, in MathTable units, by which the arc advances on each move
// of speed pixels along a path len pixels long, so that the arc covers
// FPI (half a turn) by the time the effect reaches its target.
//----------------------------------------------------------------------
inline int
ParabolaRadStep(float len, unsigned speed)
{
	int steps = static_cast<int>(len) / static_cast<int>(speed);	// moves needed to reach the target

	return static_cast<int>(MathTable::FPI / (float)steps);
}

#endif
