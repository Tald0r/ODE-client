#include "Client_PCH.h"
#include "ZoneLoadingHost.h"

namespace ZoneLoading {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

void RemoveZoneEvents()
{
	if (s_Host && s_Host->RemoveZoneEvents) s_Host->RemoveZoneEvents();
}

void LoadAllAddonSprites()
{
	if (s_Host && s_Host->LoadAllAddonSprites) s_Host->LoadAllAddonSprites();
}

Race InterfaceRace()
{
	return s_Host && s_Host->InterfaceRace ? s_Host->InterfaceRace() : RACE_SLAYER;
}

void ChangeInterfaceRace(Race race)
{
	if (s_Host && s_Host->ChangeInterfaceRace) s_Host->ChangeInterfaceRace(race);
}

void InitSkillTree()
{
	if (s_Host && s_Host->InitSkillTree) s_Host->InitSkillTree();
}

void SetSMSCharge(int charge)
{
	if (s_Host && s_Host->SetSMSCharge) s_Host->SetSMSCharge(charge);
}

void SetPowerjjangPoint(int point)
{
	if (s_Host && s_Host->SetPowerjjangPoint) s_Host->SetPowerjjangPoint(point);
}

void CloseWindows()
{
	if (s_Host && s_Host->CloseWindows) s_Host->CloseWindows();
}

void StartProgress(int zoneID)
{
	if (s_Host && s_Host->StartProgress) s_Host->StartProgress(zoneID);
}

void DrawProgress(int percent)
{
	if (s_Host && s_Host->DrawProgress) s_Host->DrawProgress(percent);
}

void EndProgress()
{
	if (s_Host && s_Host->EndProgress) s_Host->EndProgress();
}

void InitTopView()
{
	if (s_Host && s_Host->InitTopView) s_Host->InitTopView();
}

bool PreloadsMonsters()
{
	return s_Host && s_Host->PreloadsMonsters ? s_Host->PreloadsMonsters() : false;
}

void LoadCreature(int spriteType)
{
	if (s_Host && s_Host->LoadCreature) s_Host->LoadCreature(spriteType);
}

bool HasLargeZone()
{
	return s_Host && s_Host->HasLargeZone ? s_Host->HasLargeZone() : false;
}

void AddMonsterSpriteTypes(const COrderedList<int>& types)
{
	if (s_Host && s_Host->AddMonsterSpriteTypes) s_Host->AddMonsterSpriteTypes(types);
}

void ReleaseCreatureSpritesExcept(const COrderedList<int>& types)
{
	if (s_Host && s_Host->ReleaseCreatureSpritesExcept) s_Host->ReleaseCreatureSpritesExcept(types);
}

void MoveZone(int zoneID)
{
	if (s_Host && s_Host->MoveZone) s_Host->MoveZone(zoneID);
}

void LoadZone(int zoneID)
{
	if (s_Host && s_Host->LoadZone) s_Host->LoadZone(zoneID);
}

void SetEncryptCode()
{
	if (s_Host && s_Host->SetEncryptCode) s_Host->SetEncryptCode();
}

void StampFrameClock()
{
	if (s_Host && s_Host->StampFrameClock) s_Host->StampFrameClock();
}

std::uint32_t MillisecondsToFrames(std::uint32_t ms)
{
	return s_Host && s_Host->MillisecondsToFrames ? s_Host->MillisecondsToFrames(ms) : 0;
}

void AdvanceFrame(int frames)
{
	if (s_Host && s_Host->AdvanceFrame) s_Host->AdvanceFrame(frames);
}

void SetMyBatColorSet(std::uint16_t colorSet)
{
	if (s_Host && s_Host->SetMyBatColorSet) s_Host->SetMyBatColorSet(colorSet);
}

void ShowPlayerHP()
{
	if (s_Host && s_Host->ShowPlayerHP) s_Host->ShowPlayerHP();
}

void EnterWaitSetPositionMode()
{
	if (s_Host && s_Host->EnterWaitSetPositionMode) s_Host->EnterWaitSetPositionMode();
}

}
