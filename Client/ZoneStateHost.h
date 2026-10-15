#pragma once

#include <cstdint>
#include "MTypeDef.h"
#include "MEffectStatusDef.h"

class MActionResult;
class MAttachEffect;
class MCreature;
class MObject;
class MItem;
class MPetItem;
class MShop;
class MStatus;
class PCOustersInfo3;
class PCSlayerInfo3;

// The zone the player is in, and the creatures in it, as the replies about
// them read and change them. MCreature stays the executable's: replies hold
// the pointer GetCreature returns and pass it back to CreatureState, never
// dereferencing it. Status is the creature as its status array (MStatus);
// GetShop is an NPC's shop, asked only of a creature IsNPC says is one.
// A missing host or entry means no zone and no creature: queries answer
// false, NULL, 0, ACTIONINFO_NULL or an empty name, and actions are skipped.
// The executable's host reads the zone unguarded in GetCreature: a reply
// asks Exists first where its old code checked g_pZone, and the others (a
// creature's move, say) only arrive in a zone, as they always assumed.
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
	// The ID of the creature with this name (flag as MZone's
	// GetCreatureID); OBJECTID_NULL for none.
	TYPE_OBJECTID (*GetCreatureID)(const char* pName, int flag) = nullptr;
	// The fake creature (a pet, a turret) with this ID; NULL for none.
	MCreature* (*GetFakeCreature)(TYPE_OBJECTID id) = nullptr;
	// A new fake creature looking like pCreature (body, gear, direction,
	// actions) standing at the cell; the caller owns it. NULL without a host.
	MCreature* (*NewFakeCreature)(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) = nullptr;
	// The zone takes the fake creature; false when it has one with that ID
	// already, and the caller still owns it.
	bool (*AddFakeCreature)(MCreature* pFakeCreature) = nullptr;
	// Deletes a fake creature the zone did not take.
	void (*DeleteFakeCreature)(MCreature* pFakeCreature) = nullptr;
	// Marks the zone's cell with a ground effect: a ground elemental's
	// aura, a darkness that cannot be cast there, the fury of gnome.
	void (*SetGroundElemental)(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) = nullptr;
	void (*SetDarknessForbidden)(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) = nullptr;
	void (*SetFuryOfGnome)(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) = nullptr;
	// A new plain creature of the zone, not in it yet; the caller owns it.
	// NULL without a host. AddCreature puts it in; false when the zone
	// refuses it (one with its ID there already, a cell outside the zone or
	// not set, the cell full for its move type, a move type with no
	// sectors), and the caller still owns it. DeleteCreature deletes one
	// the zone did not take, or a corpse's (MCorpse's destructor frees its
	// creature through it; without a host the creature leaks).
	MCreature* (*NewCreature)() = nullptr;
	// The same for a creature that wears gear (MCreatureWear): a bat or a
	// wolf a vampire turns into, say.
	MCreature* (*NewCreatureWear)() = nullptr;
	bool (*AddCreature)(MCreature* pCreature) = nullptr;
	void (*DeleteCreature)(MCreature* pCreature) = nullptr;
	// The creature in the zone's creature list with this ID, not one lying
	// in a corpse (GetCreature finds those too); NULL for none.
	MCreature* (*GetCreatureOnly)(TYPE_OBJECTID id) = nullptr;
	// The ID of the item lying in the cell; OBJECTID_NULL for none.
	TYPE_OBJECTID (*GetItemID)(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) = nullptr;
	// A new corpse item holding the creature, which it owns from then on:
	// freeing the corpse frees the creature. NULL without a host or an item
	// factory; the caller then still owns the creature.
	MItem* (*NewCorpse)(MCreature* pCreature) = nullptr;
	// The same as NewCreature for an NPC (MNPC).
	MCreature* (*NewNPC)() = nullptr;
	// Puts the player in the zone again, under its ID now: the zone's
	// player and the player's zone.
	void (*SetPlayer)() = nullptr;
	// Takes the item of the ID off the zone's ground without freeing it:
	// the player has it now. Asked only with a zone: the executable's
	// host reads it unchecked.
	void (*PickupItem)(TYPE_OBJECTID id) = nullptr;
	// Removes from the zone the creatures out of sight of the cell. Asked
	// only with a zone, as PickupItem.
	void (*KeepObjectInSight)(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, int sight) = nullptr;
	// Whether the creature of the ID has called a helicopter, and whether
	// a corpse of the creature type lies in a cell next to the creature or
	// under it (task 4.237). Both false without a host.
	bool (*HasHelicopter)(TYPE_OBJECTID creatureID) = nullptr;
	bool (*IsExistCorpseFromPlayer)(MCreature* pCreature, int creatureType) = nullptr;
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
// OBJECTID_NULL without a host.
TYPE_OBJECTID GetCreatureID(const char* pName, int flag);
MCreature* GetFakeCreature(TYPE_OBJECTID id);
MCreature* NewFakeCreature(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
bool AddFakeCreature(MCreature* pFakeCreature);
void DeleteFakeCreature(MCreature* pFakeCreature);
void SetGroundElemental(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
void SetDarknessForbidden(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
void SetFuryOfGnome(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
MCreature* NewCreature();
MCreature* NewCreatureWear();
bool AddCreature(MCreature* pCreature);
MCreature* GetCreatureOnly(TYPE_OBJECTID id);
TYPE_OBJECTID GetItemID(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
MItem* NewCorpse(MCreature* pCreature);
MCreature* NewNPC();
void SetPlayer();
void PickupItem(TYPE_OBJECTID id);
void KeepObjectInSight(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, int sight);
void DeleteCreature(MCreature* pCreature);
bool HasHelicopter(TYPE_OBJECTID creatureID);
bool IsExistCorpseFromPlayer(MCreature* pCreature, int creatureType);

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
	// Fills an NPC's shop with its fixed shelf, or its mysterious one, from
	// the shop templates; nothing for a creature that is not an NPC.
	void (*CreateFixedShelf)(MCreature* pCreature, bool mysterious) = nullptr;
	// Whether the creature is in the player's party.
	bool (*IsPlayerParty)(MCreature* pCreature) = nullptr;
	// The creature's guild; 0 without a host.
	int (*GetGuildNumber)(MCreature* pCreature) = nullptr;
	// Marks the creature as in the player's party.
	void (*SetPlayerParty)(MCreature* pCreature) = nullptr;
	// The creature's pet; OBJECTID_NULL for none, or without a host.
	TYPE_OBJECTID (*GetPetID)(MCreature* pCreature) = nullptr;
	void (*SetAction)(MCreature* pCreature, int action) = nullptr;
	// A fake creature's pet item; NULL for another creature, or none.
	MPetItem* (*GetPetItem)(MCreature* pCreature) = nullptr;
	// Moves the creature to the cell at once, over what lies between (a
	// charge, a teleport); server: the server put it there.
	void (*FastMovePosition)(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, bool server) = nullptr;
	// Draws the creature as a ghost trail (flag picks the trail) for delay
	// frames.
	void (*SetGhost)(MCreature* pCreature, int flag, int delay) = nullptr;
	// The direction from the creature to the cell; its current direction
	// for its own cell, and 0 without a host.
	int (*GetDirectionToPosition)(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) = nullptr;
	// The installed turret's (a bike's) direction and its turning count.
	void (*SetInstallTurretDirect)(MCreature* pCreature, int direction) = nullptr;
	void (*SetInstallTurretCount)(MCreature* pCreature, int count) = nullptr;
	// A fake creature runs a fast move to the cell (the afterimage of a
	// fast-moving creature); another creature is left alone.
	void (*SetFakeFastMove)(MCreature* pFakeCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) = nullptr;
	// A fake creature's (a pet's) turret. SetTurretDelay is a Centauro
	// turret's (types 702-704) recoil: it is drawn pulled back by twice that
	// many pixels, one less each frame. SetTurretFinalDirection is the
	// direction such a turret turns towards when its owner, or the owner's
	// last target, is not in the zone (otherwise it turns to that target;
	// 0xFF, unset, turns it towards the way the creature faces), on the
	// turret's own 32-step scale.
	// GCPetUseSkill hands it an 8-way direction, as upstream did. Another
	// creature is left alone.
	void (*SetTurretDelay)(MCreature* pFakeCreature, int delay) = nullptr;
	void (*SetTurretFinalDirection)(MCreature* pFakeCreature, int direction) = nullptr;
	// A creature's personal shop: whether it is open (CurPernalShop; 0
	// without a host), the sign over it in a colour, and the message kept
	// for it. ClearChatString takes the sign (and any chat) away.
	void (*SetPersnalShop)(MCreature* pCreature, int open) = nullptr;
	int (*CurPernalShop)(MCreature* pCreature) = nullptr;
	void (*SetPersnalString)(MCreature* pCreature, const char* sign, std::uint32_t color) = nullptr;
	void (*ClearChatString)(MCreature* pCreature) = nullptr;
	void (*SetPersnalShopMessage)(MCreature* pCreature, const char* message) = nullptr;
	// A new creature's identity and place: its name (never NULL: SetName
	// passes a NULL one as empty), ID, cell and guild (-1 for none).
	void (*SetName)(MCreature* pCreature, const char* name) = nullptr;
	void (*SetID)(MCreature* pCreature, TYPE_OBJECTID id) = nullptr;
	void (*SetPosition)(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) = nullptr;
	void (*SetGuildNumber)(MCreature* pCreature, int guild) = nullptr;
	// A bat's colour set (0xFFFF for its own).
	void (*SetBatColor)(MCreature* pCreature, std::uint16_t colorSet) = nullptr;
	// Whether the creature is a plain MCreature (not one that wears gear,
	// an NPC, a fake or the player); false without a host.
	bool (*IsPlainCreature)(MCreature* pCreature) = nullptr;
	// The creature's height off the ground (a flying one's); 0 without a
	// host.
	int (*GetZ)(MCreature* pCreature) = nullptr;
	// Whether the creature is a plain MCreatureWear (not the player, an NPC
	// or a fake, which wear gear too); false without a host.
	bool (*IsPlainCreatureWear)(MCreature* pCreature) = nullptr;
	// Dresses a creature that wears gear as the slayer pInfo describes: its
	// hair, coat, trousers, helmet, weapon, shield, motorcycle and shoulder,
	// each but the shoulder in its colours (an advanced slayer's coat takes
	// the hair colour where another's takes the skin), and sets its attack
	// speed. Nothing for a creature that wears none.
	void (*SetAddonToSlayer3)(MCreature* pCreature, const PCSlayerInfo3* pInfo) = nullptr;
	// The creature's competence (0 for an operator, as the server marks
	// one).
	void (*SetCompetence)(MCreature* pCreature, int competence) = nullptr;
	// The colour of the master's aura the creature shows (0 for none).
	void (*SetMasterEffectType)(MCreature* pCreature, int type) = nullptr;
	// The guild union the creature's guild belongs to.
	void (*SetUnionGuildID)(MCreature* pCreature, int unionID) = nullptr;
	// Dresses a creature that wears gear as the ouster pInfo describes: its
	// coat and arm, coloured by its hair, coat, boots and arm colours, and
	// sets its attack speed. Nothing for a coat past the ousters' coats, or
	// for a creature that wears none.
	void (*SetAddonToOusters3)(MCreature* pCreature, const PCOustersInfo3* pInfo) = nullptr;
	// Lays the creature down dead, a corpse (its own body's last frame).
	void (*SetCorpse)(MCreature* pCreature) = nullptr;
	// Whether the creature is dead; false without a host.
	bool (*IsDead)(MCreature* pCreature) = nullptr;
	// How many items a corpse holds for looting.
	void (*SetItemCount)(MCreature* pCreature, int count) = nullptr;
	// The NPC ID an NPC answers to (its dialogue and shop); nothing for a
	// creature that is no NPC.
	void (*SetNPCID)(MCreature* pCreature, int npcID) = nullptr;
	// Dresses a creature that wears gear as the slayer NPC of the ID wears
	// its gear; nothing for one that wears none, or an ID past the creature
	// table or with no gear.
	void (*SetAddonToNPC)(MCreature* pCreature, int npcID) = nullptr;
	// Attaches a lasting effect of the sprite type to the creature (one of
	// 0xFFFF frames, linked to no action), facing as the creature does.
	void (*AttachLastingEffect)(MCreature* pCreature, int effectSpriteType) = nullptr;
	// Creates an effect of the sprite type attached to the creature, for
	// the frames given and linked after the link frames, on the ground or
	// not, of the effect type (an MEffect::EFFECT_TYPE); none when the
	// creature cannot take it.
	MAttachEffect* (*CreateAttachEffect)(MCreature* pCreature, int effectSpriteType, std::uint32_t delayFrame, std::uint32_t linkDelayFrame, bool bGroundEffect, int effectType) = nullptr;
	// Whether any effect is attached to the creature, the first attached
	// (only when one is), and clearing them all.
	bool (*IsExistAttachEffect)(MCreature* pCreature) = nullptr;
	MAttachEffect* (*FirstAttachEffect)(MCreature* pCreature) = nullptr;
	void (*ClearAttachEffect)(MCreature* pCreature) = nullptr;
	// Whether the creature has the effect status; false without a host.
	bool (*HasEffectStatus)(MCreature* pCreature, EFFECTSTATUS status) = nullptr;
	// Where the creature is in its current action, and the action's length.
	int (*GetActionCount)(MCreature* pCreature) = nullptr;
	int (*GetActionCountMax)(MCreature* pCreature) = nullptr;
	// How many after-images the creature draws behind it.
	void (*SetShadowCount)(MCreature* pCreature, int count) = nullptr;
	// The creature as the MObject it is (its first base), and an object a
	// sector holds as a creature, back. NULL without a host.
	MObject* (*ObjectOf)(const MCreature* pCreature) = nullptr;
	MCreature* (*CreatureOf)(MObject* pObject) = nullptr;
	// How the creature moves (MOVE_DEVICE_WALK without a host), and
	// whether it stands still (false without a host).
	MOVE_DEVICE (*GetMoveDevice)(MCreature* pCreature) = nullptr;
	bool (*IsStop)(MCreature* pCreature) = nullptr;
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
void CreateFixedShelf(MCreature* pCreature, bool mysterious);
bool IsPlayerParty(MCreature* pCreature);
int GetGuildNumber(MCreature* pCreature);
void SetPlayerParty(MCreature* pCreature);
TYPE_OBJECTID GetPetID(MCreature* pCreature);
void SetAction(MCreature* pCreature, int action);
MPetItem* GetPetItem(MCreature* pCreature);
void FastMovePosition(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, bool server);
void SetGhost(MCreature* pCreature, int flag, int delay);
int GetDirectionToPosition(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
void SetInstallTurretDirect(MCreature* pCreature, int direction);
void SetInstallTurretCount(MCreature* pCreature, int count);
void SetFakeFastMove(MCreature* pFakeCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
void SetTurretDelay(MCreature* pFakeCreature, int delay);
void SetTurretFinalDirection(MCreature* pFakeCreature, int direction);
void SetPersnalShop(MCreature* pCreature, int open);
int CurPernalShop(MCreature* pCreature);
void SetPersnalString(MCreature* pCreature, const char* sign, std::uint32_t color);
void ClearChatString(MCreature* pCreature);
void SetPersnalShopMessage(MCreature* pCreature, const char* message);
void SetName(MCreature* pCreature, const char* name);
void SetID(MCreature* pCreature, TYPE_OBJECTID id);
void SetPosition(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
void SetGuildNumber(MCreature* pCreature, int guild);
void SetBatColor(MCreature* pCreature, std::uint16_t colorSet);
bool IsPlainCreature(MCreature* pCreature);
int GetZ(MCreature* pCreature);
bool IsPlainCreatureWear(MCreature* pCreature);
void SetAddonToSlayer3(MCreature* pCreature, const PCSlayerInfo3* pInfo);
void SetCompetence(MCreature* pCreature, int competence);
void SetMasterEffectType(MCreature* pCreature, int type);
void SetUnionGuildID(MCreature* pCreature, int unionID);
void SetAddonToOusters3(MCreature* pCreature, const PCOustersInfo3* pInfo);
void SetCorpse(MCreature* pCreature);
bool IsDead(MCreature* pCreature);
void SetItemCount(MCreature* pCreature, int count);
void SetNPCID(MCreature* pCreature, int npcID);
void SetAddonToNPC(MCreature* pCreature, int npcID);
void AttachLastingEffect(MCreature* pCreature, int effectSpriteType);
MAttachEffect* CreateAttachEffect(MCreature* pCreature, int effectSpriteType, std::uint32_t delayFrame, std::uint32_t linkDelayFrame, bool bGroundEffect, int effectType);
bool IsExistAttachEffect(MCreature* pCreature);
MAttachEffect* FirstAttachEffect(MCreature* pCreature);
void ClearAttachEffect(MCreature* pCreature);
bool HasEffectStatus(MCreature* pCreature, EFFECTSTATUS status);
int GetActionCount(MCreature* pCreature);
int GetActionCountMax(MCreature* pCreature);
void SetShadowCount(MCreature* pCreature, int count);
MObject* ObjectOf(const MCreature* pCreature);
MCreature* CreatureOf(MObject* pObject);
MOVE_DEVICE GetMoveDevice(MCreature* pCreature);
bool IsStop(MCreature* pCreature);

}
