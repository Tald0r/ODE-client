#include "Client_PCH.h"
#include "HomingEffectSteering.h"
#include "MathTable.h"
#include <cstdint>
#include <cstdlib>

HomingEffectSteering::HomingEffectSteering(int currentDegrees, int turnDegrees)
	: m_RadCurrent(MathTable::GetAngle360(currentDegrees)),
	  m_RadStep(MathTable::GetAngle360(turnDegrees))
{
}

void HomingEffectSteering::TurnToward(int pixelX, int pixelY, int targetX, int targetY)
{
	const int targetAngle = MathTable::GetAngleToTarget(pixelX, pixelY, targetX, targetY);
	const int direction = MathTable::GetAngleDir(m_RadCurrent, targetAngle);
	m_RadStep = direction * std::abs(m_RadStep);
}

POINT HomingEffectSteering::Advance(WORD speed)
{
	m_RadCurrent += m_RadStep;
	m_RadCurrent &= MathTable::MAX_ANGLE_1;
	return {
		static_cast<LONG>((static_cast<std::int64_t>(MathTable::FCos(m_RadCurrent)) * speed) >> 16),
		static_cast<LONG>(-((static_cast<std::int64_t>(MathTable::FSin(m_RadCurrent)) * speed) >> 16))
	};
}
