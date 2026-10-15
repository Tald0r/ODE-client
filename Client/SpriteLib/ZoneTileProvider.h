#ifndef __ZONE_TILEPROVIDER_H__
#define __ZONE_TILEPROVIDER_H__

#include "TileRenderer.h"

/**
 * ZoneTileProvider - adapts a zone to the ITileDataProvider interface, so
 * TileRenderer can draw its tiles (task 4.223; MZoneTileProvider before).
 *
 * Zone needs GetWidth(), GetHeight() and GetSector(x, y), whose result
 * has GetSpriteID(). The game uses it with MZone. A sector's sprite 0 or
 * SPRITEID_NULL, a cell off the zone, or no zone at all is the null tile
 * (-1).
 */
template <class Zone>
class ZoneTileProvider : public ITileDataProvider
{
public:
	// We don't own the zone, so we don't delete it.
	void SetZone(Zone* pZone) { m_pZone = pZone; }

	virtual int GetSpriteID(int sectorX, int sectorY) override
	{
		if (!IsInBounds(sectorX, sectorY))
			return -1;  // no zone, or out of bounds

		const int spriteID = m_pZone->GetSector(sectorX, sectorY).GetSpriteID();
		if (spriteID == 0 || spriteID == SPRITEID_NULL)
			return -1;

		return spriteID;
	}

	virtual int GetWidth() const override
	{
		if (m_pZone == nullptr)
			return 0;
		return m_pZone->GetWidth();
	}

	virtual int GetHeight() const override
	{
		if (m_pZone == nullptr)
			return 0;
		return m_pZone->GetHeight();
	}

private:
	bool IsInBounds(int x, int y) const
	{
		if (m_pZone == nullptr)
			return false;
		return (x >= 0 && y >= 0 && x < m_pZone->GetWidth() && y < m_pZone->GetHeight());
	}

private:
	Zone* m_pZone = nullptr;
};

#endif // __ZONE_TILEPROVIDER_H__
