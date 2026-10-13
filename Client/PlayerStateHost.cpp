#include "Client_PCH.h"
#include "PlayerStateHost.h"

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

}
