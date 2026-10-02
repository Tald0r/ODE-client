#include "EffectOrbit.h"
#include "MathTable.h"
#include "MViewDef.h"
#include <cstdlib>

namespace {
int NormalizeStep(int step)
{
	const int remainder = step % EffectOrbit::StepCount;
	return remainder < 0 ? remainder + EffectOrbit::StepCount : remainder;
}
}

POINT EffectOrbit::s_Positions[TypeCount][StepCount]{};

void EffectOrbit::InitializePositions()
{
	const int widths[TypeCount]{TILE_X * 2, TILE_X, TILE_X / 2};
	const int heights[TypeCount]{TILE_Y * 2, TILE_Y, TILE_Y / 2};
	for (int type = 0; type < TypeCount; ++type)
	{
		const int angleStep = MathTable::MAX_ANGLE / (type == 2 ? StepCount : StepCount / 2);
		for (int step = 0; step < StepCount; ++step)
		{
			const int angle = angleStep * step;
			s_Positions[type][step].x = (MathTable::FCos(angle) * widths[type]) >> 16;
			s_Positions[type][step].y = (MathTable::FSin(angle) * heights[type]) >> 16;
		}
	}
}

EffectOrbit::EffectOrbit(int type, int step, const Random& random) : m_Type(type)
{
	m_Step = step == -1 ? static_cast<int>((random ? random() : static_cast<unsigned>(std::rand())) % StepCount)
		: NormalizeStep(step);
}

const POINT& EffectOrbit::GetPosition() const
{
	static const POINT noOffset{};
	if (m_Type < 0 || m_Type >= TypeCount) return noOffset;
	return s_Positions[m_Type][m_Step];
}

void EffectOrbit::SetStep(int step)
{
	m_Step = NormalizeStep(step);
}

void EffectOrbit::NextStep()
{
	m_Step = (m_Step + 1) % StepCount;
}

void EffectOrbit::Update(bool effectActive)
{
	if (effectActive && m_Running) NextStep();
}
