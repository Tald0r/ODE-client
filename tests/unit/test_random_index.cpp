#include "test_framework.h"
#include "Client/OrbitEffectPolicy.h"
#include <cstdlib>

// MAttachOrbitEffect's step count and MFakeCreature's turret direction
// count, the two callers of RandomIndex.
static const int ORBIT_STEP_COUNT = 64;
static const int TURRET_DIRECTION_COUNT = 32;

TEST(RandomIndex, ZeroMapsToTheFirstIndex)
{
	CHECK_EQ(RandomIndex(0, ORBIT_STEP_COUNT), 0);
	CHECK_EQ(RandomIndex(0, TURRET_DIRECTION_COUNT), 0);
}

TEST(RandomIndex, HalfOfRandMaxMapsBelowTheMiddle)
{
	CHECK_EQ(RandomIndex(RAND_MAX/2, ORBIT_STEP_COUNT), ORBIT_STEP_COUNT/2 - 1);
	CHECK_EQ(RandomIndex(RAND_MAX/2, TURRET_DIRECTION_COUNT), TURRET_DIRECTION_COUNT/2 - 1);
}

TEST(RandomIndex, RandMaxMapsToTheLastIndex)
{
	CHECK_EQ(RandomIndex(RAND_MAX, ORBIT_STEP_COUNT), ORBIT_STEP_COUNT - 1);
	CHECK_EQ(RandomIndex(RAND_MAX, TURRET_DIRECTION_COUNT), TURRET_DIRECTION_COUNT - 1);
}

TEST(RandomIndex, EveryIndexCoversAnEqualBand)
{
	// RAND_MAX + 1 is a power of two on every supported C library, so the
	// bands divide it exactly. The width is computed as long long because
	// count * width is 2^31 where RAND_MAX is 2^31 - 1.
	const int counts[] = { ORBIT_STEP_COUNT, TURRET_DIRECTION_COUNT };
	for (int count : counts)
	{
		const long long width = ((long long)RAND_MAX + 1) / count;
		CHECK_EQ(width * count, (long long)RAND_MAX + 1);
		for (int index = 0; index < count; index++)
		{
			const long long first = width * index;
			const long long last = first + width - 1;
			CHECK_EQ(RandomIndex((int)first, count), index);
			CHECK_EQ(RandomIndex((int)last, count), index);
		}
	}
}
