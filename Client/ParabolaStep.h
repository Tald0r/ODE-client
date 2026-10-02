//----------------------------------------------------------------------
// ParabolaStep.h
//----------------------------------------------------------------------
// The arc step of a parabola effect, kept free of game state so that it
// can be tested on its own.
//----------------------------------------------------------------------

#ifndef __PARABOLASTEP_H__
#define __PARABOLASTEP_H__

#include "MathTable.h"
#include <cmath>

//----------------------------------------------------------------------
// ParabolaRadStep
//----------------------------------------------------------------------
// The angle, in MathTable units, by which the arc advances on each move
// of speed pixels along a path len pixels long, so that the arc covers
// FPI (half a turn) by the time the effect reaches its target.
// A path shorter than one move, or a zero speed, counts as one move, so
// the arc completes on the first update. Nonpositive or unordered lengths
// also count as one move; paths beyond the angle resolution have a zero step.
//----------------------------------------------------------------------
inline int
ParabolaRadStep(float len, unsigned speed)
{
	if (speed == 0 || !(len > 0))
		return MathTable::FPI;
	const double steps = std::floor(static_cast<double>(len) / speed);
	if (steps < 1)
		return MathTable::FPI;
	if (steps > MathTable::FPI)
		return 0;

	return MathTable::FPI / static_cast<int>(steps);
}

#endif
