#include "Client_PCH.h"
#include "ZoneStateHost.h"
#include "MActionResult.h"
#include "MStatus.h"

namespace ZoneState {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

bool Exists()
{
	return s_Host && s_Host->Exists ? s_Host->Exists() : false;
}

MCreature* GetCreature(TYPE_OBJECTID id)
{
	return s_Host && s_Host->GetCreature ? s_Host->GetCreature(id) : nullptr;
}

bool RemoveTileEffect(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int effectStatus, int serverID)
{
	return s_Host && s_Host->RemoveTileEffect ? s_Host->RemoveTileEffect(sX, sY, effectStatus, serverID) : false;
}

bool AddHelicopter(TYPE_OBJECTID creatureID, int x0, int y0)
{
	return s_Host && s_Host->AddHelicopter ? s_Host->AddHelicopter(creatureID, x0, y0) : false;
}

void RemoveHelicopter(TYPE_OBJECTID creatureID)
{
	if (s_Host && s_Host->RemoveHelicopter) s_Host->RemoveHelicopter(creatureID);
}

bool AddCorpseFromCreature(TYPE_OBJECTID id)
{
	return s_Host && s_Host->AddCorpseFromCreature ? s_Host->AddCorpseFromCreature(id) : false;
}

int GetID()
{
	return s_Host && s_Host->GetID ? s_Host->GetID() : ZONEID_NULL;
}

}

namespace CreatureState {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

MStatus* Status(MCreature* pCreature)
{
	return s_Host && s_Host->Status ? s_Host->Status(pCreature) : nullptr;
}

TYPE_OBJECTID GetID(MCreature* pCreature)
{
	return s_Host && s_Host->GetID ? s_Host->GetID(pCreature) : 0;
}

const char* GetName(MCreature* pCreature)
{
	return s_Host && s_Host->GetName ? s_Host->GetName(pCreature) : "";
}

bool IsAlive(MCreature* pCreature)
{
	return s_Host && s_Host->IsAlive ? s_Host->IsAlive(pCreature) : false;
}

void SetDead(MCreature* pCreature)
{
	if (s_Host && s_Host->SetDead) s_Host->SetDead(pCreature);
}

void StopRecoveryHP(MCreature* pCreature)
{
	if (s_Host && s_Host->StopRecoveryHP) s_Host->StopRecoveryHP(pCreature);
}

void SetRecoveryHP(MCreature* pCreature, int amount, int times, std::uint32_t delay)
{
	if (s_Host && s_Host->SetRecoveryHP) s_Host->SetRecoveryHP(pCreature, amount, times, delay);
}

void SetNickName(MCreature* pCreature, std::uint8_t type, char* szNickName)
{
	if (s_Host && s_Host->SetNickName) s_Host->SetNickName(pCreature, type, szNickName);
}

void SetChatString(MCreature* pCreature, const char* str)
{
	if (s_Host && s_Host->SetChatString) s_Host->SetChatString(pCreature, str);
}

void PacketMove(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, std::uint8_t direction)
{
	if (s_Host && s_Host->PacketMove) s_Host->PacketMove(pCreature, x, y, direction);
}

void PacketAttackNormal(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, std::uint8_t direction)
{
	if (s_Host && s_Host->PacketAttackNormal) s_Host->PacketAttackNormal(pCreature, x, y, direction);
}

void SetDirectionToPosition(MCreature* pCreature, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	if (s_Host && s_Host->SetDirectionToPosition) s_Host->SetDirectionToPosition(pCreature, sX, sY);
}

void SetStop(MCreature* pCreature)
{
	if (s_Host && s_Host->SetStop) s_Host->SetStop(pCreature);
}

bool MovePosition(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	return s_Host && s_Host->MovePosition ? s_Host->MovePosition(pCreature, x, y) : false;
}

void SetServerPosition(MCreature* pCreature, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	if (s_Host && s_Host->SetServerPosition) s_Host->SetServerPosition(pCreature, sX, sY);
}

bool KnockBackPosition(MCreature* pCreature, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	return s_Host && s_Host->KnockBackPosition ? s_Host->KnockBackPosition(pCreature, sX, sY) : false;
}

void SetStatus(MCreature* pCreature, std::uint32_t n, std::uint32_t value)
{
	if (MStatus* pStatus = Status(pCreature)) pStatus->SetStatus(n, value);
}

TYPE_SECTORPOSITION GetX(MCreature* pCreature)
{
	return s_Host && s_Host->GetX ? s_Host->GetX(pCreature) : 0;
}

TYPE_SECTORPOSITION GetY(MCreature* pCreature)
{
	return s_Host && s_Host->GetY ? s_Host->GetY(pCreature) : 0;
}

void SetEffectDelayFrame(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, std::uint32_t frames)
{
	if (s_Host && s_Host->SetEffectDelayFrame) s_Host->SetEffectDelayFrame(pCreature, actionInfo, frames);
}

void PacketSpecialActionResult(MCreature* pCreature, TYPE_ACTIONINFO resultActionInfo, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t grade)
{
	if (s_Host && s_Host->PacketSpecialActionResult) s_Host->PacketSpecialActionResult(pCreature, resultActionInfo, id, sX, sY, grade);
}

void PacketSpecialActionToInventoryItem(MCreature* pCreature, TYPE_ACTIONINFO actionInfo)
{
	if (s_Host && s_Host->PacketSpecialActionToInventoryItem) s_Host->PacketSpecialActionToInventoryItem(pCreature, actionInfo);
}

bool AddEffectStatus(MCreature* pCreature, EFFECTSTATUS status, std::uint32_t delayFrame)
{
	return s_Host && s_Host->AddEffectStatus ? s_Host->AddEffectStatus(pCreature, status, delayFrame) : false;
}

void SetTurning(MCreature* pCreature, std::uint16_t count)
{
	if (s_Host && s_Host->SetTurning) s_Host->SetTurning(pCreature, count);
}

bool IsNPC(MCreature* pCreature)
{
	return s_Host && s_Host->IsNPC ? s_Host->IsNPC(pCreature) : false;
}

MShop* GetShop(MCreature* pCreature)
{
	return s_Host && s_Host->GetShop ? s_Host->GetShop(pCreature) : nullptr;
}

void ReleaseMoveBuffer(MCreature* pCreature)
{
	if (s_Host && s_Host->ReleaseMoveBuffer) s_Host->ReleaseMoveBuffer(pCreature);
}

void SetVisible(MCreature* pCreature)
{
	if (s_Host && s_Host->SetVisible) s_Host->SetVisible(pCreature);
}

void SetInvisible(MCreature* pCreature)
{
	if (s_Host && s_Host->SetInvisible) s_Host->SetInvisible(pCreature);
}

bool IsUndergroundCreature(MCreature* pCreature)
{
	return s_Host && s_Host->IsUndergroundCreature ? s_Host->IsUndergroundCreature(pCreature) : false;
}

void SetUndergroundCreature(MCreature* pCreature)
{
	if (s_Host && s_Host->SetUndergroundCreature) s_Host->SetUndergroundCreature(pCreature);
}

void SetFlyingCreature(MCreature* pCreature)
{
	if (s_Host && s_Host->SetFlyingCreature) s_Host->SetFlyingCreature(pCreature);
}

void SetGroundCreature(MCreature* pCreature)
{
	if (s_Host && s_Host->SetGroundCreature) s_Host->SetGroundCreature(pCreature);
}

void SetCreatureType(MCreature* pCreature, int creatureType)
{
	if (s_Host && s_Host->SetCreatureType) s_Host->SetCreatureType(pCreature, creatureType);
}

bool IsMale(MCreature* pCreature)
{
	return s_Host && s_Host->IsMale ? s_Host->IsMale(pCreature) : false;
}

void SetMale(MCreature* pCreature, bool bMale)
{
	if (s_Host && s_Host->SetMale) s_Host->SetMale(pCreature, bMale);
}

std::uint16_t GetBodyColor1(MCreature* pCreature)
{
	return s_Host && s_Host->GetBodyColor1 ? s_Host->GetBodyColor1(pCreature) : 0;
}

std::uint16_t GetBodyColor2(MCreature* pCreature)
{
	return s_Host && s_Host->GetBodyColor2 ? s_Host->GetBodyColor2(pCreature) : 0;
}

void SetBodyColor1(MCreature* pCreature, std::uint16_t colorSet)
{
	if (s_Host && s_Host->SetBodyColor1) s_Host->SetBodyColor1(pCreature, colorSet);
}

void SetBodyColor2(MCreature* pCreature, std::uint16_t colorSet)
{
	if (s_Host && s_Host->SetBodyColor2) s_Host->SetBodyColor2(pCreature, colorSet);
}

TYPE_SECTORPOSITION GetServerX(MCreature* pCreature)
{
	return s_Host && s_Host->GetServerX ? s_Host->GetServerX(pCreature) : 0;
}

TYPE_SECTORPOSITION GetServerY(MCreature* pCreature)
{
	return s_Host && s_Host->GetServerY ? s_Host->GetServerY(pCreature) : 0;
}

bool IsInCasket(MCreature* pCreature)
{
	return s_Host && s_Host->IsInCasket ? s_Host->IsInCasket(pCreature) : false;
}

void AddCasket(MCreature* pCreature, int casketType)
{
	if (s_Host && s_Host->AddCasket) s_Host->AddCasket(pCreature, casketType);
}

void RemoveCasket(MCreature* pCreature)
{
	if (s_Host && s_Host->RemoveCasket) s_Host->RemoveCasket(pCreature);
}

void SetBloodyZenith(MCreature* pCreature, int delay)
{
	if (s_Host && s_Host->SetBloodyZenith) s_Host->SetBloodyZenith(pCreature, delay);
}

void PacketSpecialActionToSector(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, MActionResult* pResult)
{
	if (s_Host && s_Host->PacketSpecialActionToSector) s_Host->PacketSpecialActionToSector(pCreature, actionInfo, sX, sY, pResult);
	else delete pResult;
}

void PacketSpecialActionToOther(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, TYPE_OBJECTID id, MActionResult* pResult)
{
	if (s_Host && s_Host->PacketSpecialActionToOther) s_Host->PacketSpecialActionToOther(pCreature, actionInfo, id, pResult);
	else delete pResult;
}

void PacketSpecialActionToNobody(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	if (s_Host && s_Host->PacketSpecialActionToNobody) s_Host->PacketSpecialActionToNobody(pCreature, actionInfo, x, y);
}

void PacketSpecialActionToSelf(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, MActionResult* pResult)
{
	if (s_Host && s_Host->PacketSpecialActionToSelf) s_Host->PacketSpecialActionToSelf(pCreature, actionInfo, pResult);
	else delete pResult;
}

void SetDrainCreatureID(MCreature* pCreature, TYPE_OBJECTID id)
{
	if (s_Host && s_Host->SetDrainCreatureID) s_Host->SetDrainCreatureID(pCreature, id);
}

void ClearStopBloodDrain(MCreature* pCreature)
{
	if (s_Host && s_Host->ClearStopBloodDrain) s_Host->ClearStopBloodDrain(pCreature);
}

TYPE_ACTIONINFO GetBasicActionInfo(MCreature* pCreature)
{
	return s_Host && s_Host->GetBasicActionInfo ? s_Host->GetBasicActionInfo(pCreature) : ACTIONINFO_NULL;
}

}
