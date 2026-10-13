#include "Client_PCH.h"
#include "ZoneStateHost.h"

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

void SetNickName(MCreature* pCreature, BYTE type, char* szNickName)
{
	if (s_Host && s_Host->SetNickName) s_Host->SetNickName(pCreature, type, szNickName);
}

void SetChatString(MCreature* pCreature, const char* str)
{
	if (s_Host && s_Host->SetChatString) s_Host->SetChatString(pCreature, str);
}

void PacketMove(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, BYTE direction)
{
	if (s_Host && s_Host->PacketMove) s_Host->PacketMove(pCreature, x, y, direction);
}

void PacketAttackNormal(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, BYTE direction)
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

}
