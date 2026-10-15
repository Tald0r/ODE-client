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

void AddNewInventoryEffect(TYPE_OBJECTID id, TYPE_ACTIONINFO actionInfo, std::uint32_t delayFrame, std::uint32_t value)
{
	if (s_Host && s_Host->AddNewInventoryEffect) s_Host->AddNewInventoryEffect(id, actionInfo, delayFrame, value);
}

void SetEffectInfo(MCreature* pCreature, EffectInfo* pEffectInfo, int delayedFrame)
{
	if (s_Host && s_Host->SetEffectInfo) s_Host->SetEffectInfo(pCreature, pEffectInfo, delayedFrame);
}

void AddGDREffect(int effect, bool bAppearBossMonster)
{
	if (s_Host && s_Host->AddGDREffect) s_Host->AddGDREffect(effect, bAppearBossMonster);
}

void ComeFromPortal(MCreature* pCreature)
{
	if (s_Host && s_Host->ComeFromPortal) s_Host->ComeFromPortal(pCreature);
}

void AddRaceSlayerMonster(GCAddMonster* pPacket)
{
	if (s_Host && s_Host->AddRaceSlayerMonster) s_Host->AddRaceSlayerMonster(pPacket);
}

void AddRaceOustersMonster(GCAddMonster* pPacket)
{
	if (s_Host && s_Host->AddRaceOustersMonster) s_Host->AddRaceOustersMonster(pPacket);
}

void SkillShadowDancing(MCreature* pUser, MCreature* pTarget, int skillID)
{
	if (s_Host && s_Host->SkillShadowDancing) s_Host->SkillShadowDancing(pUser, pTarget, skillID);
}

void SkillIllendue(MCreature* pUser, MCreature* pTarget, int skillID)
{
	if (s_Host && s_Host->SkillIllendue) s_Host->SkillIllendue(pUser, pTarget, skillID);
}

void SkillBlazeWalk(MCreature* pUser, MCreature* pTarget, int skillID, std::uint8_t grade)
{
	if (s_Host && s_Host->SkillBlazeWalk) s_Host->SkillBlazeWalk(pUser, pTarget, skillID, grade);
}

void AddRocketLauncher(MCreature* pUser, MCreature* pTarget)
{
	if (s_Host && s_Host->AddRocketLauncher) s_Host->AddRocketLauncher(pUser, pTarget);
}

void AddWildWolf(MCreature* pUser, MCreature* pTarget, bool bEatCorpse)
{
	if (s_Host && s_Host->AddWildWolf) s_Host->AddWildWolf(pUser, pTarget, bEatCorpse);
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

void MuteSound(bool bMute)
{
	if (s_Host && s_Host->MuteSound) s_Host->MuteSound(bMute);
}

bool ViewExists()
{
	return s_Host && s_Host->ViewExists && s_Host->ViewExists();
}

int GetMaxEffectFrame(int bltType, std::uint16_t frameID)
{
	return s_Host && s_Host->GetMaxEffectFrame ? s_Host->GetMaxEffectFrame(bltType, frameID) : 0;
}

void AddOustersElemental(MCreature* pOwner, int creatureType, int status)
{
	if (s_Host && s_Host->AddOustersElemental) s_Host->AddOustersElemental(pOwner, creatureType, status);
}

POINT GetFirstSector()
{
	if (s_Host && s_Host->GetFirstSector) return s_Host->GetFirstSector();
	POINT none = { 0, 0 };
	return none;
}

void StopSound(TYPE_SOUNDID soundID)
{
	if (s_Host && s_Host->StopSound) s_Host->StopSound(soundID);
}

int GetRandomMonsterTypeInZone()
{
	return s_Host && s_Host->GetRandomMonsterTypeInZone ? s_Host->GetRandomMonsterTypeInZone() : 0;
}

void SetFirstDraw()
{
	if (s_Host && s_Host->SetFirstDraw) s_Host->SetFirstDraw();
}

int GetTileSpriteCount()
{
	return s_Host && s_Host->GetTileSpriteCount ? s_Host->GetTileSpriteCount() : 0;
}

int GetImageObjectFrameCount(int frameID)
{
	return s_Host && s_Host->GetImageObjectFrameCount ? s_Host->GetImageObjectFrameCount(frameID) : 0;
}

std::uint16_t GetImageObjectFrameSprite(int frameID, int frame)
{
	return s_Host && s_Host->GetImageObjectFrameSprite ? s_Host->GetImageObjectFrameSprite(frameID, frame) : std::uint16_t(0xFFFF);
}

void SetSelectedSectorNULL()
{
	if (s_Host && s_Host->SetSelectedSectorNULL) s_Host->SetSelectedSectorNULL();
}

void SetSelectedNULL()
{
	if (s_Host && s_Host->SetSelectedNULL) s_Host->SetSelectedNULL();
}

void SetSelectedCreature(TYPE_OBJECTID creatureID)
{
	if (s_Host && s_Host->SetSelectedCreature) s_Host->SetSelectedCreature(creatureID);
}

POINT GetSelectedSector(int screenX, int screenY)
{
	if (s_Host && s_Host->GetSelectedSector) return s_Host->GetSelectedSector(screenX, screenY);
	POINT none = { SECTORPOSITION_NULL, SECTORPOSITION_NULL };
	return none;
}

POINT ScreenToPixel(int screenX, int screenY)
{
	if (s_Host && s_Host->ScreenToPixel) return s_Host->ScreenToPixel(screenX, screenY);
	POINT origin = { 0, 0 };
	return origin;
}

int GetViewRequestMode()
{
	return s_Host && s_Host->GetViewRequestMode ? s_Host->GetViewRequestMode() : 0;
}

bool IsViewRequestMode()
{
	return s_Host && s_Host->IsViewRequestMode && s_Host->IsViewRequestMode();
}

void SetFadeStart(signed char start, signed char end, signed char step, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint16_t delay)
{
	if (s_Host && s_Host->SetFadeStart) s_Host->SetFadeStart(start, end, step, r, g, b, delay);
}

void SetFadeEnd()
{
	if (s_Host && s_Host->SetFadeEnd) s_Host->SetFadeEnd();
}

}

void PlaySound(TYPE_SOUNDID soundID)
{
	WorldEvents::PlaySound(soundID);
}
