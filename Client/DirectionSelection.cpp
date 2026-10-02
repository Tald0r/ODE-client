#include "DirectionSelection.h"
#include "MViewDef.h"

BYTE
SelectFacingDirection(int originX, int originY, int destX, int destY)
{
	int	stepX = destX - originX,
		stepY = destY - originY;

	// 0일 때 check
	float	k	= (stepX==0)? 0 : (float)(stepY) / stepX;	// 기울기


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
