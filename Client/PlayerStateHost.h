#pragma once

#include <cstdint>
#include "MTypeDef.h"
#include "MEffectStatusDef.h"
#include "RaceType.h"

class MActionResult;
class MCreature;
class MItem;
class MItemManager;
class MStatus;

namespace PlayerState {

// What an item waiting on the server is for. MPlayer brings these into
// its own scope, so MPlayer::ITEM_CHECK_BUFFER_X names the same values.
	enum ITEM_CHECK_BUFFER
	{
		ITEM_CHECK_BUFFER_NULL = 0,

		// picking an item up
		ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY,
		ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE,
		ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT,
		ITEM_CHECK_BUFFER_PICKUP_MONEY,

		// using an item
		ITEM_CHECK_BUFFER_USE_FROM_INVENTORY,
		ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT,
		
		// putting an item into another
		ITEM_CHECK_BUFFER_INSERT_FROM_INVENTORY,	
		ITEM_CHECK_BUFFER_INSERT_FROM_GEAR,	
		ITEM_CHECK_BUFFER_INSERT_FROM_QUICKSLOT,			
		
		ITEM_CHECK_BUFFER_DROP_TO_INVENTORY,
		ITEM_CHECK_BUFFER_PICKUP_FROM_INVENTORY,

		ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY,	// picking up only some of a stack

		// trading
		ITEM_CHECK_BUFFER_TRADE_ADD,
		ITEM_CHECK_BUFFER_TRADE_REMOVE,

		// a skill on an item
		ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY,

		// a skill from an item
		ITEM_CHECK_BUFFER_SKILL_FROM_ITEM,

		// dropped from the mouse to the zone
		ITEM_CHECK_BUFFER_DROP_TO_ZONE,			

		ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE,			

		// adding an item to another
		ITEM_CHECK_BUFFER_ITEM_TO_ITEM,

		// merging trees
		ITEM_CHECK_BUFFER_TREE_MERGE,
		ITEM_CHECK_BUFFER_USE_FROM_GEAR,
		
		ITEM_CHECK_BUFFER_USE_MIXING_ITEM,
		ITEM_CHECK_BUFFER_DROP_TO_CREATURE,			
		
		ITEM_CHECK_BUFFER_USE_FROM_GQUEST_INVENTORY,
	};

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
	void (*SetGuildNumber)(int guildNumber) = nullptr;
	bool (*HasEffectStatus)(EFFECTSTATUS status) = nullptr;
	MCreature* (*Creature)() = nullptr;
	// Whether the player is an operator (a game master's character).
	bool (*IsOperator)() = nullptr;
	// How far the player sees, in cells.
	int (*GetSight)() = nullptr;
	// MPlayer's own burrowing, which hides the creature's.
	void (*SetUndergroundCreature)() = nullptr;
	// The item held while the server checks its use, and whether it is
	// being used on another item.
	bool (*IsItemCheckBufferItemToItem)() = nullptr;
	MItem* (*GetItemCheckBuffer)() = nullptr;
	void (*ClearItemCheckBuffer)() = nullptr;
	// Hands the player's current action its result queue, which the player
	// keeps for the action or runs and frees at once.
	bool (*PacketAddActionResult)(std::uint16_t effectID, MActionResult* pResult) = nullptr;
	// The bullets left in the gun's current magazine, if it has one. The
	// magazine (MMagazine) keeps its vtable in the executable.
	void (*SetCurrentMagazineNumber)(int number) = nullptr;
	// MPlayer's own landing, which hides the creature's.
	void (*SetGroundCreature)() = nullptr;
	// The cell a skill was cast at, for a skill that charges before it
	// lands there (Burning Soul).
	void (*SetTempSelectPos)(int x, int y) = nullptr;
	// The player acts again only after this many milliseconds (MPlayer's
	// own delay; MCreature has none).
	void (*SetDelay)(std::uint32_t milliseconds) = nullptr;
	// What the item waiting on the server is for; NULL without a player.
	ITEM_CHECK_BUFFER (*GetItemCheckBufferStatus)() = nullptr;
	// Soul Chain failed: the player cannot trace its target.
	void (*SetCannotTrace)() = nullptr;
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
void SetGuildNumber(int guildNumber);
bool HasEffectStatus(EFFECTSTATUS status);
// The player as a creature, for CreatureState's entries; NULL without a player.
MCreature* Creature();
// False without a player.
bool IsOperator();
// 0 without a player.
int GetSight();
void SetUndergroundCreature();
bool IsItemCheckBufferItemToItem();
// NULL without a player.
MItem* GetItemCheckBuffer();
void ClearItemCheckBuffer();
// Takes pResult; without an entry it is freed unrun, and the answer is false.
bool PacketAddActionResult(std::uint16_t effectID, MActionResult* pResult);
void SetCurrentMagazineNumber(int number);
void SetGroundCreature();
void SetTempSelectPos(int x, int y);
void SetDelay(std::uint32_t milliseconds);
ITEM_CHECK_BUFFER GetItemCheckBufferStatus();
void SetCannotTrace();

}
