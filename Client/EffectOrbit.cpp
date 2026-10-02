#include "EffectOrbit.h"
#include "MathTable.h"
#include "MViewDef.h"
#include <cstdlib>

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
		: step % StepCount;
}

const POINT& EffectOrbit::GetPosition() const
{
	return s_Positions[m_Type][m_Step];
}

void EffectOrbit::NextStep()
{
	m_Step = (m_Step + 1) & 0x3f;
}

void EffectOrbit::Update(bool effectActive)
{
	if (effectActive && m_Running) NextStep();
}
