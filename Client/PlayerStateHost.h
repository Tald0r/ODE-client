#pragma once

#include <cstdint>
#include "MTypeDef.h"
#include "MEffectStatusDef.h"
#include "RaceType.h"

class MItem;
class MItemManager;
class MStatus;

namespace PlayerState {

// What the player is waiting for the server to verify. MPlayer brings these
// into its own scope, so MPlayer::WAIT_VERIFY_X names the same values.
enum WAIT_VERIFY
{
	WAIT_VERIFY_NULL,
	WAIT_VERIFY_MOTORCYCLE_GETON,
	WAIT_VERIFY_MOTORCYCLE_GETOFF,
	WAIT_VERIFY_SKILL_SUCCESS,
	WAIT_VERIFY_ITEM_USE,
	WAIT_VERIFY_NPC_ASK,
	WAIT_VERIFY_TRADE,
	WAIT_VERIFY_PARTY,
	WAIT_VERIFY_PARTY_LEAVE,
	WAIT_VERIFY_RESURRECT,
	WAIT_VERIFY_LOVE_CHAIN,
	WAIT_VERIFY_LOGOUT,
	WAIT_VERIFY_SYLPH_SUMMON_GETOFF,
};

// The client's own player, which replies about it read and change through
// this host. Status is the player as its status array (MStatus, whose
// SetStatus the player overrides); NULL when there is no player. A missing
// host or entry means no player: no status, ID 0, nothing awaited, a slayer
// wearing nothing, false from every action, and actions skipped.
struct Host
{
	MStatus* (*Status)() = nullptr;
	TYPE_OBJECTID (*GetID)() = nullptr;
	WAIT_VERIFY (*GetWaitVerify)() = nullptr;
	void (*SetWaitVerifyNULL)() = nullptr;
	Race (*GetRace)() = nullptr;
	MItemManager* (*GetGear)() = nullptr;
	bool (*SetAddonItem)(MItem* pItem) = nullptr;
	bool (*AddEffectStatus)(EFFECTSTATUS status, std::uint32_t delayFrame) = nullptr;
	void (*SetRecoveryHP)(int amount, int times, std::uint32_t delay) = nullptr;
	void (*SetRecoveryMP)(int amount, int times, std::uint32_t delay) = nullptr;
	void (*StopRecoveryHP)() = nullptr;
	void (*StopRecoveryMP)() = nullptr;
	void (*SetVisible)() = nullptr;
	void (*SetServerPosition)(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) = nullptr;
	void (*PacketMoveOK)(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t direction) = nullptr;
	void (*PacketMoveNO)() = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
MStatus* Status();
TYPE_OBJECTID GetID();
WAIT_VERIFY GetWaitVerify();
void SetWaitVerifyNULL();
Race GetRace();
MItemManager* GetGear();
bool SetAddonItem(MItem* pItem);
bool AddEffectStatus(EFFECTSTATUS status, std::uint32_t delayFrame);
void SetRecoveryHP(int amount, int times, std::uint32_t delay);
void SetRecoveryMP(int amount, int times, std::uint32_t delay);
void StopRecoveryHP();
void StopRecoveryMP();
void SetVisible();
void SetServerPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
void PacketMoveOK(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t direction);
void PacketMoveNO();

}
