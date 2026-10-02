#ifndef LINEAR_EFFECT_MOTION_H
#define LINEAR_EFFECT_MOTION_H

#include "Platform.h"

// The calling effect owns the current position and speed. Retargeting calculates
// a fixed velocity; advancing uses the caller's current arrival distance.
// Effect subclasses retain access to the trajectory for their curved paths.
class LinearEffectMotion
{
	friend class ParabolaEffectMotion;
public:
	void SetLinearTarget(float pixelX, float pixelY, float pixelZ,
		int targetX, int targetY, int targetZ, WORD stepPixel);
	bool AdvanceLinear(float& pixelX, float& pixelY, float& pixelZ,
		WORD arrivalDistance);
	float GetPathLength() const { return m_Len; }

protected:
	int m_TargetX = 0;
	int m_TargetY = 0;
	int m_TargetZ = 0;
	float m_StepX = 0;
	float m_StepY = 0;
	float m_StepZ = 0;
	float m_Len = 0;
};

#endif
