#pragma once

#include <cstdint>
#include "MTypeDef.h"
#include "MEffectStatusDef.h"

class MActionResult;
class MCreature;
class MItem;
class MShop;
class MStatus;

// The zone the player is in, and the creatures in it, as the replies about
// them read and change them. MCreature stays the executable's: replies hold
// the pointer GetCreature returns and pass it back to CreatureState, never
// dereferencing it. Status is the creature as its status array (MStatus);
// GetShop is an NPC's shop, asked only of a creature IsNPC says is one.
// A missing host or entry means no zone and no creature: queries answer
// false, NULL, 0, ACTIONINFO_NULL or an empty name, and actions are skipped.
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
	// The creature lying in the corpse item with this ID; NULL for no item,
	// one that is not a corpse, or a corpse with no creature.
	MCreature* (*GetCorpseCreature)(TYPE_OBJECTID itemID) = nullptr;
	// The item lying in the zone with this ID; NULL for none.
	MItem* (*GetItem)(TYPE_OBJECTID itemID) = nullptr;
	// Takes the item out of the zone and frees it; false when it is not
	// there. MZone's also clears the player's item check buffer when it
	// holds the item, so it needs the player.
	bool (*RemoveItem)(TYPE_OBJECTID itemID) = nullptr;
	// Lays the item in the zone, falling when bDropping; the zone owns it
	// when this answers true.
	bool (*AddItem)(MItem* pItem, bool bDropping) = nullptr;
	// Takes the creature with this ID out of the zone, freeing it unless
	// it is the player; false when the zone does not hold it. MZone's also
	// answers false, and keeps the creature, when no sector holds it.
	bool (*RemoveCreature)(TYPE_OBJECTID id) = nullptr;
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
MCreature* GetCorpseCreature(TYPE_OBJECTID itemID);
MItem* GetItem(TYPE_OBJECTID itemID);
bool RemoveItem(TYPE_OBJECTID itemID);
// Without a host the item is not taken: false, and the caller keeps it.
bool AddItem(MItem* pItem, bool bDropping);
bool RemoveCreature(TYPE_OBJECTID id);

}

namespace CreatureState {

// How a creature moves. MCreature brings these into its own scope, so
// MCreature::MOVE_DEVICE_X names the same values.
enum MOVE_DEVICE
{
	MOVE_DEVICE_WALK,			// on foot
	MOVE_DEVICE_RIDE,			// a motorcycle
	MOVE_DEVICE_SUMMON_SYLPH,	// riding a summoned sylph
	MOVE_DEVICE_MAX
};

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
	// The words over its head, in a colour (0xBBGGRR).
	void (*SetChatString)(MCreature* pCreature, const char* str, std::uint32_t color) = nullptr;
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
	void (*PacketSpecialActionResult)(MCreature* pCreature, TYPE_ACTIONINFO resultActionInfo, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t grade) = nullptr;
	void (*PacketSpecialActionToInventoryItem)(MCreature* pCreature, TYPE_ACTIONINFO actionInfo) = nullptr;
	bool (*AddEffectStatus)(MCreature* pCreature, EFFECTSTATUS status, std::uint32_t delayFrame) = nullptr;
	void (*SetTurning)(MCreature* pCreature, std::uint16_t count) = nullptr;
	bool (*IsNPC)(MCreature* pCreature) = nullptr;
	MShop* (*GetShop)(MCreature* pCreature) = nullptr;
	// The creature's own visibility, movement kind, type, body colors,
	// casket and Bloody Zenith. MPlayer hides SetUndergroundCreature,
	// SetFlyingCreature and SetGroundCreature, so these reach
	// MCreature's, as any call through an MCreature* did.
	void (*ReleaseMoveBuffer)(MCreature* pCreature) = nullptr;
	void (*SetVisible)(MCreature* pCreature) = nullptr;
	void (*SetInvisible)(MCreature* pCreature) = nullptr;
	bool (*IsUndergroundCreature)(MCreature* pCreature) = nullptr;
	void (*SetUndergroundCreature)(MCreature* pCreature) = nullptr;
	void (*SetFlyingCreature)(MCreature* pCreature) = nullptr;
	void (*SetGroundCreature)(MCreature* pCreature) = nullptr;
	void (*SetCreatureType)(MCreature* pCreature, int creatureType) = nullptr;
	bool (*IsMale)(MCreature* pCreature) = nullptr;
	void (*SetMale)(MCreature* pCreature, bool bMale) = nullptr;
	std::uint16_t (*GetBodyColor1)(MCreature* pCreature) = nullptr;
	std::uint16_t (*GetBodyColor2)(MCreature* pCreature) = nullptr;
	void (*SetBodyColor1)(MCreature* pCreature, std::uint16_t colorSet) = nullptr;
	void (*SetBodyColor2)(MCreature* pCreature, std::uint16_t colorSet) = nullptr;
	TYPE_SECTORPOSITION (*GetServerX)(MCreature* pCreature) = nullptr;
	TYPE_SECTORPOSITION (*GetServerY)(MCreature* pCreature) = nullptr;
	bool (*IsInCasket)(MCreature* pCreature) = nullptr;
	void (*AddCasket)(MCreature* pCreature, int casketType) = nullptr;
	void (*RemoveCasket)(MCreature* pCreature) = nullptr;
	void (*SetBloodyZenith)(MCreature* pCreature, int delay) = nullptr;
	// The special actions take the result queue, as the members do; the
	// forwarders free it when there is no entry to take it.
	void (*PacketSpecialActionToSector)(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, MActionResult* pResult) = nullptr;
	void (*PacketSpecialActionToOther)(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, TYPE_OBJECTID id, MActionResult* pResult) = nullptr;
	void (*PacketSpecialActionToNobody)(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) = nullptr;
	void (*PacketSpecialActionToSelf)(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, MActionResult* pResult) = nullptr;
	void (*SetDrainCreatureID)(MCreature* pCreature, TYPE_OBJECTID id) = nullptr;
	void (*ClearStopBloodDrain)(MCreature* pCreature) = nullptr;
	// The action the creature attacks with by default (its weapon's).
	TYPE_ACTIONINFO (*GetBasicActionInfo)(MCreature* pCreature) = nullptr;
	// Whether the creature had the status; false without a host.
	bool (*RemoveEffectStatus)(MCreature* pCreature, EFFECTSTATUS status) = nullptr;
	// Marks the creature as faking its death.
	void (*SetFakeDie)(MCreature* pCreature) = nullptr;
	// The direction the creature faces now; 0 without a host.
	int (*GetDirection)(MCreature* pCreature) = nullptr;
	// Takes the creature's head off, if it can lose it.
	void (*RemoveHead)(MCreature* pCreature) = nullptr;
	// Turns the creature to a direction (the one it turns to, not the one
	// GetDirection reads until it has turned).
	void (*SetDirection)(MCreature* pCreature, int direction) = nullptr;
	// Fades the creature back in soon after a fade-out.
	void (*SetVisibleSoon)(MCreature* pCreature) = nullptr;
	// Whether the creature wears gear (MCreatureWear); only a wearing
	// creature takes an addon item (SetAddonItem answers false otherwise).
	bool (*IsWear)(MCreature* pCreature) = nullptr;
	// Puts on a copy of the item; the caller keeps pItem.
	bool (*SetAddonItem)(MCreature* pCreature, MItem* pItem) = nullptr;
	void (*SetWeaponSpeed)(MCreature* pCreature, int speed) = nullptr;
	// Takes off the addon in that slot; false for a creature not wearing
	// gear or a slot with nothing in it.
	bool (*RemoveAddon)(MCreature* pCreature, int addon) = nullptr;
	void (*SetMoveDevice)(MCreature* pCreature, MOVE_DEVICE device) = nullptr;
	// The creature table row it is drawn as; 0 without a host.
	int (*GetCreatureType)(MCreature* pCreature) = nullptr;
	// Its operator rank: 0 is an operator; 3 without a host.
	int (*GetCompetence)(MCreature* pCreature) = nullptr;
	// Faces the direction at once (SetDirection turns towards it).
	void (*SetCurrentDirection)(MCreature* pCreature, int direction) = nullptr;
	// The name another race sees under a hallucination or the race mask.
	const char* (*GetHalluName)(MCreature* pCreature) = nullptr;
	// The creature table's race of its type (a monster is none of them).
	bool (*IsSlayer)(MCreature* pCreature) = nullptr;
	bool (*IsOusters)(MCreature* pCreature) = nullptr;
	// Slayer, vampire, or ousters for anything else.
	int (*GetRace)(MCreature* pCreature) = nullptr;
	// Ends a blood drain or a soul absorption the creature was showing.
	void (*SetStopBloodDrain)(MCreature* pCreature) = nullptr;
	void (*SetStopAbsorbSoul)(MCreature* pCreature) = nullptr;
	// The creature table's vampire flag of its type.
	bool (*IsVampire)(MCreature* pCreature) = nullptr;
	// Gives an NPC the shop, which it then owns (MNPC frees the shop it had,
	// so never pass its current one); false (the caller keeps the shop) for
	// a creature that is not an NPC.
	bool (*SetShop)(MCreature* pCreature, MShop* pShop) = nullptr;
	// Whether the creature carries the effect status now.
	bool (*HasEffectStatus)(MCreature* pCreature, EFFECTSTATUS status) = nullptr;
	// Fills an NPC's shop with its fixed shelf, or its mysterious one, from
	// the shop templates; nothing for a creature that is not an NPC.
	void (*CreateFixedShelf)(MCreature* pCreature, bool mysterious) = nullptr;
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
// White unless a colour is given, as MCreature's own default.
void SetChatString(MCreature* pCreature, const char* str, std::uint32_t color = 0xFFFFFF);
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
// grade is the skill's step, for a skill that has steps.
void PacketSpecialActionResult(MCreature* pCreature, TYPE_ACTIONINFO resultActionInfo, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t grade = 0);
void PacketSpecialActionToInventoryItem(MCreature* pCreature, TYPE_ACTIONINFO actionInfo);
bool AddEffectStatus(MCreature* pCreature, EFFECTSTATUS status, std::uint32_t delayFrame);
void SetTurning(MCreature* pCreature, std::uint16_t count);
bool IsNPC(MCreature* pCreature);
MShop* GetShop(MCreature* pCreature);
void ReleaseMoveBuffer(MCreature* pCreature);
void SetVisible(MCreature* pCreature);
void SetInvisible(MCreature* pCreature);
bool IsUndergroundCreature(MCreature* pCreature);
void SetUndergroundCreature(MCreature* pCreature);
void SetFlyingCreature(MCreature* pCreature);
void SetGroundCreature(MCreature* pCreature);
void SetCreatureType(MCreature* pCreature, int creatureType);
bool IsMale(MCreature* pCreature);
void SetMale(MCreature* pCreature, bool bMale);
std::uint16_t GetBodyColor1(MCreature* pCreature);
std::uint16_t GetBodyColor2(MCreature* pCreature);
void SetBodyColor1(MCreature* pCreature, std::uint16_t colorSet);
void SetBodyColor2(MCreature* pCreature, std::uint16_t colorSet);
TYPE_SECTORPOSITION GetServerX(MCreature* pCreature);
TYPE_SECTORPOSITION GetServerY(MCreature* pCreature);
bool IsInCasket(MCreature* pCreature);
void AddCasket(MCreature* pCreature, int casketType);
void RemoveCasket(MCreature* pCreature);
void SetBloodyZenith(MCreature* pCreature, int delay);
// ToSector, ToOther and ToSelf each take pResult, which may be NULL.
void PacketSpecialActionToSector(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, MActionResult* pResult);
void PacketSpecialActionToOther(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, TYPE_OBJECTID id, MActionResult* pResult);
void PacketSpecialActionToNobody(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
void PacketSpecialActionToSelf(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, MActionResult* pResult);
void SetDrainCreatureID(MCreature* pCreature, TYPE_OBJECTID id);
void ClearStopBloodDrain(MCreature* pCreature);
// ACTIONINFO_NULL without a host.
TYPE_ACTIONINFO GetBasicActionInfo(MCreature* pCreature);
bool RemoveEffectStatus(MCreature* pCreature, EFFECTSTATUS status);
void SetFakeDie(MCreature* pCreature);
// 0 without a host.
int GetDirection(MCreature* pCreature);
void RemoveHead(MCreature* pCreature);
void SetDirection(MCreature* pCreature, int direction);
void SetVisibleSoon(MCreature* pCreature);
bool IsWear(MCreature* pCreature);
bool SetAddonItem(MCreature* pCreature, MItem* pItem);
void SetWeaponSpeed(MCreature* pCreature, int speed);
bool RemoveAddon(MCreature* pCreature, int addon);
void SetMoveDevice(MCreature* pCreature, MOVE_DEVICE device);
int GetCreatureType(MCreature* pCreature);
int GetCompetence(MCreature* pCreature);
void SetCurrentDirection(MCreature* pCreature, int direction);
// "" without a host.
const char* GetHalluName(MCreature* pCreature);
bool IsSlayer(MCreature* pCreature);
bool IsOusters(MCreature* pCreature);
// RACE_SLAYER (0) without a host.
int GetRace(MCreature* pCreature);
void SetStopBloodDrain(MCreature* pCreature);
void SetStopAbsorbSoul(MCreature* pCreature);
bool IsVampire(MCreature* pCreature);
// false without a host: the caller keeps the shop.
bool SetShop(MCreature* pCreature, MShop* pShop);
bool HasEffectStatus(MCreature* pCreature, EFFECTSTATUS status);
void CreateFixedShelf(MCreature* pCreature, bool mysterious);

}
