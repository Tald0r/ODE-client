#include "test_framework.h"
#include "Client/ParabolaStep.h"

TEST(ParabolaStep, APathOfWholeStepsSplitsHalfATurnEvenly)
{
	CHECK_EQ(MathTable::FPI / 3, ParabolaRadStep(30.0f, 10));
	CHECK_EQ(341, ParabolaRadStep(30.0f, 10));
	CHECK_EQ(MathTable::FPI / 30, ParabolaRadStep(480.0f, 16));
	CHECK_EQ(MathTable::FPI, ParabolaRadStep(10.0f, 10));
}

TEST(ParabolaStep, APathShorterThanOneStepCompletesTheArcInOneMove)
{
	CHECK_EQ(MathTable::FPI, ParabolaRadStep(0.0f, 10));
	CHECK_EQ(MathTable::FPI, ParabolaRadStep(5.0f, 10));
}

TEST(ParabolaStep, AZeroSpeedDoesNotDivideByZero)
{
	CHECK_EQ(MathTable::FPI, ParabolaRadStep(100.0f, 0));
}
