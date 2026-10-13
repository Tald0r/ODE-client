#pragma once

#include <cstdint>
#include "MTypeDef.h"
#include "MEffectStatusDef.h"

class MCreature;
class MShop;
class MStatus;

// The zone the player is in, and the creatures in it, as the replies about
// them read and change them. MCreature stays the executable's: replies hold
// the pointer GetCreature returns and pass it back to CreatureState, never
// dereferencing it. Status is the creature as its status array (MStatus);
// GetShop is an NPC's shop, asked only of a creature IsNPC says is one.
// A missing host or entry means no zone and no creature: queries answer
// false, NULL, 0 or an empty name, and actions are skipped.
namespace ZoneState {

struct Host
{
	bool (*Exists)() = nullptr;
	MCreature* (*GetCreature)(TYPE_OBJECTID id) = nullptr;
	bool (*RemoveTileEffect)(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int effectStatus, int serverID) = nullptr;
	bool (*AddHelicopter)(TYPE_OBJECTID creatureID, int x0, int y0) = nullptr;
	void (*RemoveHelicopter)(TYPE_OBJECTID creatureID) = nullptr;
	bool (*AddCorpseFromCreature)(TYPE_OBJECTID id) = nullptr;
	// The zone's ID; asked only when the zone exists.
	int (*GetID)() = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
bool Exists();
MCreature* GetCreature(TYPE_OBJECTID id);
bool RemoveTileEffect(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int effectStatus, int serverID);
bool AddHelicopter(TYPE_OBJECTID creatureID, int x0, int y0);
void RemoveHelicopter(TYPE_OBJECTID creatureID);
bool AddCorpseFromCreature(TYPE_OBJECTID id);
// ZONEID_NULL without a host.
int GetID();

}

namespace CreatureState {

struct Host
{
	MStatus* (*Status)(MCreature* pCreature) = nullptr;
	TYPE_OBJECTID (*GetID)(MCreature* pCreature) = nullptr;
	const char* (*GetName)(MCreature* pCreature) = nullptr;
	bool (*IsAlive)(MCreature* pCreature) = nullptr;
	void (*SetDead)(MCreature* pCreature) = nullptr;
	void (*StopRecoveryHP)(MCreature* pCreature) = nullptr;
	void (*SetRecoveryHP)(MCreature* pCreature, int amount, int times, std::uint32_t delay) = nullptr;
	void (*SetNickName)(MCreature* pCreature, std::uint8_t type, char* szNickName) = nullptr;
	void (*SetChatString)(MCreature* pCreature, const char* str) = nullptr;
	void (*PacketMove)(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, std::uint8_t direction) = nullptr;
	void (*PacketAttackNormal)(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, std::uint8_t direction) = nullptr;
	void (*SetDirectionToPosition)(MCreature* pCreature, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) = nullptr;
	void (*SetStop)(MCreature* pCreature) = nullptr;
	bool (*MovePosition)(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) = nullptr;
	void (*SetServerPosition)(MCreature* pCreature, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) = nullptr;
	bool (*KnockBackPosition)(MCreature* pCreature, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) = nullptr;
	TYPE_SECTORPOSITION (*GetX)(MCreature* pCreature) = nullptr;
	TYPE_SECTORPOSITION (*GetY)(MCreature* pCreature) = nullptr;
	void (*SetEffectDelayFrame)(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, std::uint32_t frames) = nullptr;
	void (*PacketSpecialActionResult)(MCreature* pCreature, TYPE_ACTIONINFO resultActionInfo, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) = nullptr;
	void (*PacketSpecialActionToInventoryItem)(MCreature* pCreature, TYPE_ACTIONINFO actionInfo) = nullptr;
	bool (*AddEffectStatus)(MCreature* pCreature, EFFECTSTATUS status, std::uint32_t delayFrame) = nullptr;
	void (*SetTurning)(MCreature* pCreature, std::uint16_t count) = nullptr;
	bool (*IsNPC)(MCreature* pCreature) = nullptr;
	MShop* (*GetShop)(MCreature* pCreature) = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
MStatus* Status(MCreature* pCreature);
TYPE_OBJECTID GetID(MCreature* pCreature);
const char* GetName(MCreature* pCreature);
bool IsAlive(MCreature* pCreature);
void SetDead(MCreature* pCreature);
void StopRecoveryHP(MCreature* pCreature);
void SetRecoveryHP(MCreature* pCreature, int amount, int times, std::uint32_t delay);
void SetNickName(MCreature* pCreature, std::uint8_t type, char* szNickName);
void SetChatString(MCreature* pCreature, const char* str);
void PacketMove(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, std::uint8_t direction);
void PacketAttackNormal(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, std::uint8_t direction);
void SetDirectionToPosition(MCreature* pCreature, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
void SetStop(MCreature* pCreature);
bool MovePosition(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
void SetServerPosition(MCreature* pCreature, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
bool KnockBackPosition(MCreature* pCreature, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
// Sets one value in the creature's status array; skipped without a status.
void SetStatus(MCreature* pCreature, std::uint32_t n, std::uint32_t value);
TYPE_SECTORPOSITION GetX(MCreature* pCreature);
TYPE_SECTORPOSITION GetY(MCreature* pCreature);
void SetEffectDelayFrame(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, std::uint32_t frames);
void PacketSpecialActionResult(MCreature* pCreature, TYPE_ACTIONINFO resultActionInfo, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
void PacketSpecialActionToInventoryItem(MCreature* pCreature, TYPE_ACTIONINFO actionInfo);
bool AddEffectStatus(MCreature* pCreature, EFFECTSTATUS status, std::uint32_t delayFrame);
void SetTurning(MCreature* pCreature, std::uint16_t count);
bool IsNPC(MCreature* pCreature);
MShop* GetShop(MCreature* pCreature);

}
