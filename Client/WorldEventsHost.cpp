#include "Client_PCH.h"
#include "WorldEventsHost.h"
#include "MActionResult.h"

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

void RemoveEvent(int eventID)
{
	if (s_Host && s_Host->RemoveEvent) s_Host->RemoveEvent(eventID);
}

void PlayMusicCurrentZone()
{
	if (s_Host && s_Host->PlayMusicCurrentZone) s_Host->PlayMusicCurrentZone();
}

void PlaySound(TYPE_SOUNDID soundID)
{
	if (s_Host && s_Host->PlaySound) s_Host->PlaySound(soundID);
}

void PlaySoundAt(TYPE_SOUNDID soundID, bool repeat, int x, int y)
{
	if (s_Host && s_Host->PlaySoundAt) s_Host->PlaySoundAt(soundID, repeat, x, y);
}

void SetDarkBits(int darkBits)
{
	if (s_Host && s_Host->SetDarkBits) s_Host->SetDarkBits(darkBits);
}

void MoveIntoPortal(MCreature* pCreature)
{
	if (s_Host && s_Host->MoveIntoPortal) s_Host->MoveIntoPortal(pCreature);
}

void SetDragonTornado(int effectStatus, std::uint32_t objectID, int x, int y)
{
	if (s_Host && s_Host->SetDragonTornado) s_Host->SetDragonTornado(effectStatus, objectID, x, y);
}

void SkillCrossCounter(MCreature* pUser, MCreature* pTarget, int skillID)
{
	if (s_Host && s_Host->SkillCrossCounter) s_Host->SkillCrossCounter(pUser, pTarget, skillID);
}

void ExecuteActionInfoFromMainNode(TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction, TYPE_OBJECTID targetID, TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame, MActionResult* pResult, bool bStartFromMainNode, int sX, int sY)
{
	if (s_Host && s_Host->ExecuteActionInfoFromMainNode)
		s_Host->ExecuteActionInfoFromMainNode(actionInfo, startX, startY, startZ, direction, targetID, targetX, targetY, targetZ, delayFrame, pResult, bStartFromMainNode, sX, sY);
	else
		delete pResult;
}

void SkillToSector(TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int direction, std::uint32_t delayFrame, MActionResult* pResult)
{
	if (s_Host && s_Host->SkillToSector)
		s_Host->SkillToSector(actionInfo, sX, sY, direction, delayFrame, pResult);
	else
		delete pResult;
}

}
