#include "Client_PCH.h"
#include "PlayerStateHost.h"
#include "MActionResult.h"

namespace PlayerState {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

MStatus* Status()
{
	return s_Host && s_Host->Status ? s_Host->Status() : nullptr;
}

TYPE_OBJECTID GetID()
{
	return s_Host && s_Host->GetID ? s_Host->GetID() : 0;
}

WAIT_VERIFY GetWaitVerify()
{
	return s_Host && s_Host->GetWaitVerify ? s_Host->GetWaitVerify() : WAIT_VERIFY_NULL;
}

Race GetRace()
{
	return s_Host && s_Host->GetRace ? s_Host->GetRace() : RACE_SLAYER;
}

MItemManager* GetGear()
{
	return s_Host && s_Host->GetGear ? s_Host->GetGear() : nullptr;
}

bool SetAddonItem(MItem* pItem)
{
	return s_Host && s_Host->SetAddonItem ? s_Host->SetAddonItem(pItem) : false;
}

bool AddEffectStatus(EFFECTSTATUS status, std::uint32_t delayFrame)
{
	return s_Host && s_Host->AddEffectStatus ? s_Host->AddEffectStatus(status, delayFrame) : false;
}

void SetWaitVerifyNULL()
{
	if (s_Host && s_Host->SetWaitVerifyNULL) s_Host->SetWaitVerifyNULL();
}

void SetRecoveryHP(int amount, int times, std::uint32_t delay)
{
	if (s_Host && s_Host->SetRecoveryHP) s_Host->SetRecoveryHP(amount, times, delay);
}

void SetRecoveryMP(int amount, int times, std::uint32_t delay)
{
	if (s_Host && s_Host->SetRecoveryMP) s_Host->SetRecoveryMP(amount, times, delay);
}

void StopRecoveryHP()
{
	if (s_Host && s_Host->StopRecoveryHP) s_Host->StopRecoveryHP();
}

void StopRecoveryMP()
{
	if (s_Host && s_Host->StopRecoveryMP) s_Host->StopRecoveryMP();
}

void SetVisible()
{
	if (s_Host && s_Host->SetVisible) s_Host->SetVisible();
}

void SetServerPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	if (s_Host && s_Host->SetServerPosition) s_Host->SetServerPosition(sX, sY);
}

void PacketMoveOK(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t direction)
{
	if (s_Host && s_Host->PacketMoveOK) s_Host->PacketMoveOK(sX, sY, direction);
}

void PacketMoveNO()
{
	if (s_Host && s_Host->PacketMoveNO) s_Host->PacketMoveNO();
}

void SetGuildNumber(int guildNumber)
{
	if (s_Host && s_Host->SetGuildNumber) s_Host->SetGuildNumber(guildNumber);
}

bool HasEffectStatus(EFFECTSTATUS status)
{
	return s_Host && s_Host->HasEffectStatus ? s_Host->HasEffectStatus(status) : false;
}

MCreature* Creature()
{
	return s_Host && s_Host->Creature ? s_Host->Creature() : nullptr;
}

bool IsOperator()
{
	return s_Host && s_Host->IsOperator ? s_Host->IsOperator() : false;
}

int GetSight()
{
	return s_Host && s_Host->GetSight ? s_Host->GetSight() : 0;
}

void SetUndergroundCreature()
{
	if (s_Host && s_Host->SetUndergroundCreature) s_Host->SetUndergroundCreature();
}

bool IsItemCheckBufferItemToItem()
{
	return s_Host && s_Host->IsItemCheckBufferItemToItem ? s_Host->IsItemCheckBufferItemToItem() : false;
}

MItem* GetItemCheckBuffer()
{
	return s_Host && s_Host->GetItemCheckBuffer ? s_Host->GetItemCheckBuffer() : nullptr;
}

void ClearItemCheckBuffer()
{
	if (s_Host && s_Host->ClearItemCheckBuffer) s_Host->ClearItemCheckBuffer();
}

bool PacketAddActionResult(std::uint16_t effectID, MActionResult* pResult)
{
	if (s_Host && s_Host->PacketAddActionResult) return s_Host->PacketAddActionResult(effectID, pResult);
	delete pResult;
	return false;
}

void SetCurrentMagazineNumber(int number)
{
	if (s_Host && s_Host->SetCurrentMagazineNumber) s_Host->SetCurrentMagazineNumber(number);
}

void SetGroundCreature()
{
	if (s_Host && s_Host->SetGroundCreature) s_Host->SetGroundCreature();
}

void SetTempSelectPos(int x, int y)
{
	if (s_Host && s_Host->SetTempSelectPos) s_Host->SetTempSelectPos(x, y);
}

void SetDelay(std::uint32_t milliseconds)
{
	if (s_Host && s_Host->SetDelay) s_Host->SetDelay(milliseconds);
}

ITEM_CHECK_BUFFER GetItemCheckBufferStatus()
{
	return s_Host && s_Host->GetItemCheckBufferStatus ? s_Host->GetItemCheckBufferStatus() : ITEM_CHECK_BUFFER_NULL;
}

void SetCannotTrace()
{
	if (s_Host && s_Host->SetCannotTrace) s_Host->SetCannotTrace();
}

void SetTimeLightSight(int sight)
{
	if (s_Host && s_Host->SetTimeLightSight) s_Host->SetTimeLightSight(sight);
}

bool RemoveAddonItem(MItem* pItem)
{
	return s_Host && s_Host->RemoveAddonItem ? s_Host->RemoveAddonItem(pItem) : false;
}

void CalculateStatus()
{
	if (s_Host && s_Host->CalculateStatus) s_Host->CalculateStatus();
}

void UseItemOK()
{
	if (s_Host && s_Host->UseItemOK) s_Host->UseItemOK();
}

void UseSkillCardOK(int cardType)
{
	if (s_Host && s_Host->UseSkillCardOK) s_Host->UseSkillCardOK(cardType);
}

void SendInfoToParty()
{
	if (s_Host && s_Host->SendInfoToParty) s_Host->SendInfoToParty();
}

bool IsTraceCreature()
{
	return s_Host && s_Host->IsTraceCreature && s_Host->IsTraceCreature();
}

void TraceNULL()
{
	if (s_Host && s_Host->TraceNULL) s_Host->TraceNULL();
}

}
