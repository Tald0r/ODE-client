#include "Client_PCH.h"
#include "PlayerPresentationHost.h"

namespace PlayerPresentation {

namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

bool GetInventoryOpenState()
{
	return s_Host && s_Host->GetInventoryOpenState ? s_Host->GetInventoryOpenState() : false;
}

bool GetGearOpenState()
{
	return s_Host && s_Host->GetGearOpenState ? s_Host->GetGearOpenState() : false;
}

void RunTraceWindow()
{
	if (s_Host && s_Host->RunTraceWindow) s_Host->RunTraceWindow();
}

void SetCannotTrace()
{
	if (s_Host && s_Host->SetCannotTrace) s_Host->SetCannotTrace();
}

void RunElevator()
{
	if (s_Host && s_Host->RunElevator) s_Host->RunElevator();
}

void FinishRequestDie()
{
	if (s_Host && s_Host->FinishRequestDie) s_Host->FinishRequestDie();
}

void FinishRequestResurrect()
{
	if (s_Host && s_Host->FinishRequestResurrect) s_Host->FinishRequestResurrect();
}

void CloseRequestShrineMinimap()
{
	if (s_Host && s_Host->CloseRequestShrineMinimap) s_Host->CloseRequestShrineMinimap();
}

bool IsInstallMineProgress()
{
	return s_Host && s_Host->IsInstallMineProgress ? s_Host->IsInstallMineProgress() : false;
}

void StartInstallMineProgress(int gridX, int gridY)
{
	if (s_Host && s_Host->StartInstallMineProgress) s_Host->StartInstallMineProgress(gridX, gridY);
}

void StartCreateMineProgress(int gridX, int gridY)
{
	if (s_Host && s_Host->StartCreateMineProgress) s_Host->StartCreateMineProgress(gridX, gridY);
}

void StartCreateBombProgress(int gridX, int gridY)
{
	if (s_Host && s_Host->StartCreateBombProgress) s_Host->StartCreateBombProgress(gridX, gridY);
}

void EndInstallMineProgress()
{
	if (s_Host && s_Host->EndInstallMineProgress) s_Host->EndInstallMineProgress();
}

const char* GetInputString()
{
	return s_Host && s_Host->GetInputString ? s_Host->GetInputString() : "";
}

void AddInputString(const char* pString)
{
	if (s_Host && s_Host->AddInputString) s_Host->AddInputString(pString);
}

MItem* GetPickUpItem()
{
	return s_Host && s_Host->GetPickUpItem ? s_Host->GetPickUpItem() : nullptr;
}

void SetCursorAttack()
{
	if (s_Host && s_Host->SetCursorAttack) s_Host->SetCursorAttack();
}

bool ForceFeelReady()
{
	return s_Host && s_Host->ForceFeelReady ? s_Host->ForceFeelReady() : false;
}

void ForceAction(int soundID)
{
	if (s_Host && s_Host->ForceAction) s_Host->ForceAction(soundID);
}

void SetChangeVampireTime(int time)
{
	if (s_Host && s_Host->SetChangeVampireTime) s_Host->SetChangeVampireTime(time);
}

void SetSilverDamage(int minDamage, int maxDamage)
{
	if (s_Host && s_Host->SetSilverDamage) s_Host->SetSilverDamage(minDamage, maxDamage);
}

void RunPetInfo(PETINFO* pPetInfo)
{
	if (s_Host && s_Host->RunPetInfo) s_Host->RunPetInfo(pPetInfo);
}

void RemoveDescriptor(void* pPtr)
{
	if (s_Host && s_Host->RemoveDescriptor) s_Host->RemoveDescriptor(pPtr);
}

void LockItem()
{
	if (s_Host && s_Host->LockItem) s_Host->LockItem();
}

void LockItemTrade()
{
	if (s_Host && s_Host->LockItemTrade) s_Host->LockItemTrade();
}

void SetCharInfoDAM(int value, int value2)
{
	if (s_Host && s_Host->SetCharInfoDAM) s_Host->SetCharInfoDAM(value, value2);
}

void RunPartyCancel(const char* pName)
{
	if (s_Host && s_Host->RunPartyCancel) s_Host->RunPartyCancel(pName);
}

void RunExchangeCancel(const char* pName)
{
	if (s_Host && s_Host->RunExchangeCancel) s_Host->RunExchangeCancel(pName);
}

void CloseSelectWayPoint()
{
	if (s_Host && s_Host->CloseSelectWayPoint) s_Host->CloseSelectWayPoint();
}

void AddEffectStatus(int status, std::uint32_t delayFrame)
{
	if (s_Host && s_Host->AddEffectStatus) s_Host->AddEffectStatus(status, delayFrame);
}

void RemoveEffectStatus(int status)
{
	if (s_Host && s_Host->RemoveEffectStatus) s_Host->RemoveEffectStatus(status);
}

void FinishLevelUpButton()
{
	if (s_Host && s_Host->FinishLevelUpButton) s_Host->FinishLevelUpButton();
}

void SetHotKey(int hotkey, int id)
{
	if (s_Host && s_Host->SetHotKey) s_Host->SetHotKey(hotkey, id);
}

}
