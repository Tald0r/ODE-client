#include "Client_PCH.h"
#include "ZoneInfoData.h"

bool ZoneInfoData::LoadFromFile(std::ifstream& file, int expectedWidth, int expectedHeight)
{
	file.read(reinterpret_cast<char*>(&width), 2);
	file.read(reinterpret_cast<char*>(&height), 2);
	if (width != expectedWidth || height != expectedHeight) return false;

	int numPortal = 0;
	file.read(reinterpret_cast<char*>(&numPortal), 4);
	portals.clear();
	MPortal portal;
	for (int i = 0; i < numPortal; ++i)
	{
		portal.LoadFromFile(file);
		portals.push_back(portal);
	}

	int numSafe = 0;
	file.read(reinterpret_cast<char*>(&numSafe), 4);
	safetyZones.clear();
	B_RECT rect{};
	for (int i = 0; i < numSafe; ++i)
	{
		file.read(reinterpret_cast<char*>(&rect), SIZE_B_RECT);
		safetyZones.push_back(rect);
	}
	return true;
}
