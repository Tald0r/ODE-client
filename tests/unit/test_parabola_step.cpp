#include "test_framework.h"
#include "Client/ParabolaStep.h"
#include <limits>

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

TEST(ParabolaStep, VeryLongLengthsHaveNoRepresentableArcStep)
{
	CHECK_EQ(0, ParabolaRadStep((std::numeric_limits<float>::max)(), 1));
	CHECK_EQ(0, ParabolaRadStep(static_cast<float>((std::numeric_limits<int>::max)()), 10));
}

TEST(ParabolaStep, ResolutionBoundariesKeepWholeMoveTruncation)
{
	CHECK_EQ(1, ParabolaRadStep(10239.5f, 10));
	CHECK_EQ(1, ParabolaRadStep(10240.0f, 10));
	CHECK_EQ(1, ParabolaRadStep(10249.5f, 10));
	CHECK_EQ(0, ParabolaRadStep(10250.0f, 10));
}

TEST(ParabolaStep, FullUnsignedSpeedsAreNotConvertedToSignedIntegers)
{
	const unsigned speed = (std::numeric_limits<unsigned>::max)();
	CHECK_EQ(MathTable::FPI, ParabolaRadStep(100, speed));
	CHECK_EQ(MathTable::FPI, ParabolaRadStep(4294967296.0f, speed));
	CHECK_EQ(MathTable::FPI / 2, ParabolaRadStep(8589934592.0f, speed));
}

TEST(ParabolaStep, NonpositiveAndNonfiniteLengthsHaveDefinedSteps)
{
	const float infinity = std::numeric_limits<float>::infinity();
	CHECK_EQ(MathTable::FPI, ParabolaRadStep(-10, 10));
	CHECK_EQ(MathTable::FPI, ParabolaRadStep(-infinity, 10));
	CHECK_EQ(MathTable::FPI, ParabolaRadStep(std::numeric_limits<float>::quiet_NaN(), 10));
	CHECK_EQ(0, ParabolaRadStep(infinity, 10));
	CHECK_EQ(MathTable::FPI, ParabolaRadStep(infinity, 0));
}
