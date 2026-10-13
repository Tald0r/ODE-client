#include "Client_PCH.h"
#include "MImageObject.h"
#include "WorldTileGeometry.h"

bool
MImageObject::IsWallTransPosition(int sX, int sY) const
{
		switch (m_X)
		{
			//-------------------------------------------------------------
			// A wall that runs down to the right
			//-------------------------------------------------------------
			case WALL_RIGHTDOWN :
			{
				// The image object's sector X
				int objectSX = WorldTileGeometry::PixelToTileX( m_PixelX );
		
				// x - y
				int value = objectSX - (int)m_Viewpoint;			
				
				if (sX-sY > value)
				{
					return true;
				}

				return false;
			}
			break;

			//-------------------------------------------------------------
			// A wall that runs up to the right
			//-------------------------------------------------------------
			case WALL_RIGHTUP :
			{
				// The image object's sector X
				int objectSX = WorldTileGeometry::PixelToTileX( m_PixelX );
				
				// x + y
				int value = objectSX + (int)m_Viewpoint;			

				if (sX+sY < value)
				{
					return true;
				}

				return false;
			}
			break;
		}

	return true;
}
