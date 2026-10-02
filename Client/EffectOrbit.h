#pragma once

#include "Platform.h"
#include <functional>

class EffectOrbit
{
public:
	static constexpr int TypeCount = 3;
	static constexpr int StepCount = 64;
	using Random = std::function<unsigned()>;

	// GameInit builds the math tables before initializing these cached paths.
	static void InitializePositions();
	// Only -1 requests a random starting step; other steps use the cycle.
	EffectOrbit(int type, int step = -1, const Random& random = {});

	const POINT& GetPosition() const;
	int GetStep() const { return m_Step; }
	void SetStep(int step) { m_Step = step; }
	bool IsRunning() const { return m_Running; }
	void SetRunning(bool running) { m_Running = running; }
	void NextStep();
	void Update(bool effectActive);

private:
	static POINT s_Positions[TypeCount][StepCount];
	int m_Type;
	int m_Step;
	bool m_Running = true;
};
