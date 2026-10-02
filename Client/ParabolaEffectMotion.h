#ifndef PARABOLA_EFFECT_MOTION_H
#define PARABOLA_EFFECT_MOTION_H

#include "Platform.h"

class LinearEffectMotion;

class ParabolaEffectMotion
{
public:
	void Reset(const LinearEffectMotion& path, WORD speed);
	void Advance(LinearEffectMotion& path, float& pixelX, float& pixelY,
		float& pixelZ, WORD speed);
	// The caller can emit smoke at the advanced position before this snaps
	// an arrived effect to its target and clears the linear velocity.
	bool FinishStep(LinearEffectMotion& path, float& pixelX, float& pixelY,
		float& pixelZ, WORD arrivalDistance) const;

private:
	int m_RadCurrent = 0;
	int m_RadStep = 0;
	bool m_HalfTurnReached = false;
};

#endif
