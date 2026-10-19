#include "InputState.h"
#include "MTypeDef.h"

int		g_x = 400;
int		g_y = 300;
BOOL	g_bLButtonDown = FALSE;
BOOL	g_bRButtonDown = FALSE;
POINT	g_SelectSector = { SECTORPOSITION_NULL, SECTORPOSITION_NULL };
bool	g_bNetStatusGood = true;
BOOL	g_bUIInput = FALSE;
