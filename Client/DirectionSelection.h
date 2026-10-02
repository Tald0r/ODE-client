#ifndef CLIENT_DIRECTION_SELECTION_H
#define CLIENT_DIRECTION_SELECTION_H

#include "Platform.h"
#include "MTypeDef.h"

// Eight-way facing for the client's screen-coordinate slope thresholds.
// Coincident positions face down; threshold comparisons retain float rounding.
BYTE SelectFacingDirection(int originX, int originY, int destX, int destY);

#endif
