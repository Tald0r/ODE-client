#include "Client_PCH.h"
#include "WorldEventsHost.h"

namespace WorldEvents {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

MItem* AddItemToZone(GCAddItemToZone* pPacket, bool bDropping)
{
	return s_Host && s_Host->AddItemToZone ? s_Host->AddItemToZone(pPacket, bDropping) : nullptr;
}

void AddVampirePortal(int serverID, const char* ownerName, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint32_t delayFrame, int zoneID, TYPE_SECTORPOSITION zoneX, TYPE_SECTORPOSITION zoneY, bool bStartFromMainNode)
{
	if (s_Host && s_Host->AddVampirePortal) s_Host->AddVampirePortal(serverID, ownerName, sX, sY, delayFrame, zoneID, zoneX, zoneY, bStartFromMainNode);
}

void SetBloodBibleSlot(BloodBibleSignInfo* pInfo)
{
	if (s_Host && s_Host->SetBloodBibleSlot) s_Host->SetBloodBibleSlot(pInfo);
}

void SetWeather(int weather, int level)
{
	if (s_Host && s_Host->SetWeather) s_Host->SetWeather(weather, level);
}

void SetLightning(std::uint32_t delay)
{
	if (s_Host && s_Host->SetLightning) s_Host->SetLightning(delay);
}

void SetPetInfo(PetInfo* pPetInfo, TYPE_OBJECTID objectID)
{
	if (s_Host && s_Host->SetPetInfo) s_Host->SetPetInfo(pPetInfo, objectID);
}

void RequestDisconnect(const char* pName)
{
	if (s_Host && s_Host->RequestDisconnect) s_Host->RequestDisconnect(pName);
}

void AddEvent(MEvent& event)
{
	if (s_Host && s_Host->AddEvent) s_Host->AddEvent(event);
}

}
