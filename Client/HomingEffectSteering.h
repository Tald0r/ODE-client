#ifndef HOMING_EFFECT_STEERING_H
#define HOMING_EFFECT_STEERING_H

#include "Platform.h"

// Uses MathTable's shared tables, initialized by GameInit. Inputs use the
// executable's degree angles and integer screen coordinates.
class HomingEffectSteering
{
public:
	HomingEffectSteering(int currentDegrees, int turnDegrees);
	void TurnToward(int pixelX, int pixelY, int targetX, int targetY);
	POINT Advance(WORD speed);
	void StopTurning() { m_RadStep = 0; }
	int GetAngle() const { return m_RadCurrent; }
	int GetTurnStep() const { return m_RadStep; }

private:
	int m_RadCurrent;
	int m_RadStep;
};

#endif
