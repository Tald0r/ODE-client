#include "Client_PCH.h"
#include "ParabolaEffectMotion.h"
#include "LinearEffectMotion.h"
#include "MathTable.h"
#include "ParabolaStep.h"
#include <cmath>
#include <cstdint>

void ParabolaEffectMotion::Reset(const LinearEffectMotion& path, WORD speed)
{
	m_RadStep = ParabolaRadStep(path.GetPathLength(), speed);
	m_RadCurrent = 0;
	m_HalfTurnReached = false;
}

void ParabolaEffectMotion::Advance(LinearEffectMotion& path, float& pixelX,
	float& pixelY, float& pixelZ, WORD speed)
{
	pixelX += path.m_StepX;
	pixelY += path.m_StepY;
	pixelZ += path.m_StepZ;
	m_RadCurrent += m_RadStep;
	m_HalfTurnReached = m_HalfTurnReached || m_RadCurrent >= MathTable::FPI;
	m_RadCurrent &= MathTable::MAX_ANGLE_1;
	const auto heightStep = (static_cast<std::int64_t>(MathTable::FCos(m_RadCurrent)) * speed) >> 16;
	pixelZ += static_cast<float>(heightStep);
}

bool ParabolaEffectMotion::FinishStep(LinearEffectMotion& path, float& pixelX,
	float& pixelY, float& pixelZ, WORD arrivalDistance) const
{
	if ((std::fabs(pixelX - path.m_TargetX) < arrivalDistance &&
		std::fabs(pixelY - path.m_TargetY) < arrivalDistance &&
		m_HalfTurnReached) || pixelZ < path.m_TargetZ)
	{
		pixelX = static_cast<float>(path.m_TargetX);
		pixelY = static_cast<float>(path.m_TargetY);
		pixelZ = static_cast<float>(path.m_TargetZ);
		path.m_StepX = 0;
		path.m_StepY = 0;
		path.m_StepZ = 0;
		return true;
	}
	return false;
}
