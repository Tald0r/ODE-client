#pragma once

#include <cstdint>
#include "MTypeDef.h"
#include "MEffectStatusDef.h"
#include "RaceType.h"

class BloodBibleSignInfo;
class ExtraInfo;
class GearInfo;
class InventoryInfo;
class MActionResult;
class MCreature;
class MItem;
class MItemManager;
class MStatus;
class PCOustersInfo2;
class PCSlayerInfo2;
class PCVampireInfo2;

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
	// The player's sight by the time of day (the light around it).
	void (*SetTimeLightSight)(int sight) = nullptr;
	// The worn item no longer adds to the player (SetAddonItem undone);
	// false when it did not.
	bool (*RemoveAddonItem)(MItem* pItem) = nullptr;
	// Works the player's status out again from what it wears and has.
	void (*CalculateStatus)() = nullptr;
	// The server allowed the item use, or the skill card of that type,
	// that the player asked for: it is spent.
	void (*UseItemOK)() = nullptr;
	void (*UseSkillCardOK)(int cardType) = nullptr;
	// Tells the party what the player is, where, and how it fares.
	void (*SendInfoToParty)() = nullptr;
	// Whether the player is following a creature (to attack or talk to
	// it); TraceNULL stops it. False without a host.
	bool (*IsTraceCreature)() = nullptr;
	void (*TraceNULL)() = nullptr;
	// Whether the player is repeating an action (attacking or using a
	// skill again and again); false without a host.
	bool (*IsRepeatAction)() = nullptr;
	// Turns the player into a slayer or a vampire (its body, its gear
	// and what it can do).
	void (*ChangeToSlayer)() = nullptr;
	void (*ChangeToVampire)() = nullptr;
	// The player's basic attack: its action and its reach in cells.
	void (*SetBasicAttack)(TYPE_ACTIONINFO actionInfo, int distance) = nullptr;
	// Takes the player's details from a slayer's, a vampire's or an
	// ouster's info: its look, its stats and what it carries in the UI.
	void (*SetPCSlayerInfo)(PCSlayerInfo2* pInfo) = nullptr;
	void (*SetPCVampireInfo)(PCVampireInfo2* pInfo) = nullptr;
	void (*SetPCOustersInfo)(PCOustersInfo2* pInfo) = nullptr;
	// Fills the player's gear (and its blood bible signs, if given), its
	// inventory, and the item held at the mouse from the infos.
	void (*SetGearInfo)(GearInfo* pGearInfo, BloodBibleSignInfo* pBloodBibleInfo) = nullptr;
	void (*SetInventoryInfo)(InventoryInfo* pInventoryInfo) = nullptr;
	void (*SetExtraInfo)(ExtraInfo* pExtraInfo) = nullptr;
	// Marks an item the player now has usable or not for the player's
	// race, stats and sex (MCreature::CheckAffectStatus).
	void (*CheckAffectStatus)(MItem* pItem) = nullptr;
	// Puts an item in the player's check buffer, to wait in that status
	// for the server's answer.
	void (*SetItemCheckBuffer)(MItem* pItem, ITEM_CHECK_BUFFER status) = nullptr;
	// Makes the player wait on that verification from the server.
	void (*SetWaitVerify)(WAIT_VERIFY verify) = nullptr;
	// The special action (skill) chosen, kept across a new player.
	int (*GetSpecialActionInfo)() = nullptr;
	void (*SetSpecialActionInfo)(int actionInfo) = nullptr;
	// The normal attack mode.
	void (*SetAttackModeNormal)() = nullptr;
	// Deletes the player and makes a new one.
	void (*Recreate)() = nullptr;
	// The player's sector position, X and Y set alone.
	void (*SetXY)(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) = nullptr;
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
void SetTimeLightSight(int sight);
// false without a host.
bool RemoveAddonItem(MItem* pItem);
void CalculateStatus();
void UseItemOK();
void UseSkillCardOK(int cardType);
void SendInfoToParty();
bool IsTraceCreature();
void TraceNULL();
bool IsRepeatAction();
void ChangeToSlayer();
void ChangeToVampire();
void SetBasicAttack(TYPE_ACTIONINFO actionInfo, int distance);
void SetPCSlayerInfo(PCSlayerInfo2* pInfo);
void SetPCVampireInfo(PCVampireInfo2* pInfo);
void SetPCOustersInfo(PCOustersInfo2* pInfo);
void SetGearInfo(GearInfo* pGearInfo, BloodBibleSignInfo* pBloodBibleInfo);
void SetInventoryInfo(InventoryInfo* pInventoryInfo);
void SetExtraInfo(ExtraInfo* pExtraInfo);
void CheckAffectStatus(MItem* pItem);
void SetItemCheckBuffer(MItem* pItem, ITEM_CHECK_BUFFER status);
void SetWaitVerify(WAIT_VERIFY verify);
int GetSpecialActionInfo();
void SetSpecialActionInfo(int actionInfo);
void SetAttackModeNormal();
void Recreate();
void SetXY(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);

}
