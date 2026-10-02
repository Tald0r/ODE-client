#include "DirectionSelection.h"
#include "MViewDef.h"

#include <cstdint>

BYTE
SelectFacingDirection(int originX, int originY, int destX, int destY)
{
	const std::int64_t stepX = static_cast<std::int64_t>(destX) - originX;
	const std::int64_t stepY = static_cast<std::int64_t>(destY) - originY;

	// 0일 때 check
	const float k = stepX == 0 ? 0.0f
		: static_cast<float>(stepY) / static_cast<float>(stepX);


	//--------------------------------------------------
	// 방향을 정해야 한다.
	//--------------------------------------------------
	if (stepY == 0)
	{
		// X축
		// - -;;
		if (stepX == 0)
			return DIRECTION_DOWN;
		else if (stepX > 0)
			return DIRECTION_RIGHT;
		else
			return DIRECTION_LEFT;
	}
	else
	if (stepY < 0)	// UP쪽으로
	{
		// y축 위
		if (stepX == 0)
		{
			return DIRECTION_UP;
		}
		// 1사분면
		else if (stepX > 0)
		{
			if (k < -BASIS_DIRECTION_HIGH)
				return DIRECTION_UP;
			else if (k <= -BASIS_DIRECTION_LOW)
				return DIRECTION_RIGHTUP;
			else
				return DIRECTION_RIGHT;
		}
		// 2사분면
		else
		{
			if (k > BASIS_DIRECTION_HIGH)
				return DIRECTION_UP;
			else if (k >= BASIS_DIRECTION_LOW)
				return DIRECTION_LEFTUP;
			else
				return DIRECTION_LEFT;
		}
	}
	// 아래쪽
	else
	{
		// y축 아래
		if (stepX == 0)
		{
			return DIRECTION_DOWN;
		}
		// 4사분면
		else if (stepX > 0)
		{
			if (k > BASIS_DIRECTION_HIGH)
				return DIRECTION_DOWN;
			else if (k >= BASIS_DIRECTION_LOW)
				return DIRECTION_RIGHTDOWN;
			else
				return DIRECTION_RIGHT;
		}
		// 3사분면
		else
		{
			if (k < -BASIS_DIRECTION_HIGH)
				return DIRECTION_DOWN;
			else if (k <= -BASIS_DIRECTION_LOW)
				return DIRECTION_LEFTDOWN;
			else
				return DIRECTION_LEFT;
		}
	}
}
