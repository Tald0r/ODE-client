#pragma once

#include <cstdint>
#include "MTypeDef.h"

class BloodBibleSignInfo;
class EffectInfo;
class GCAddItemToZone;
class GCAddMonster;
class MActionResult;
class MCreature;
class MEvent;
class MItem;
class PetInfo;

namespace WorldEvents {

// What a reply hands on to the executable's world and connections whole:
// an item or portal appearing in the zone, the blood bible slots, the
// weather and lightning, a pet's details, a peer asking to disconnect, and
// an event for the event manager to run (a timed logout, say) or to drop,
// the current zone's music played again, a sound played, and an action's
// effect started from the effect generator's main node; a cross counter
// shown as a fading copy of the user striking back; a skill's effect
// started at a cell with nobody casting it; a sound heard from a cell.
// A missing host or entry adds no item (NULL) and skips the rest.
struct Host
{
	MItem* (*AddItemToZone)(GCAddItemToZone* pPacket, bool bDropping) = nullptr;
	void (*AddVampirePortal)(int serverID, const char* ownerName, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint32_t delayFrame, int zoneID, TYPE_SECTORPOSITION zoneX, TYPE_SECTORPOSITION zoneY, bool bStartFromMainNode) = nullptr;
	void (*SetBloodBibleSlot)(BloodBibleSignInfo* pInfo) = nullptr;
	void (*SetWeather)(int weather, int level) = nullptr;
	void (*SetLightning)(std::uint32_t delay) = nullptr;
	void (*SetPetInfo)(PetInfo* pPetInfo, TYPE_OBJECTID objectID) = nullptr;
	void (*RequestDisconnect)(const char* pName) = nullptr;
	void (*AddEvent)(MEvent& event) = nullptr;
	void (*RemoveEvent)(int eventID) = nullptr;
	void (*PlayMusicCurrentZone)() = nullptr;
	void (*PlaySound)(TYPE_SOUNDID soundID) = nullptr;
	// Starts actionInfo's effect from (startX, startY, startZ) towards the
	// target; the effect takes pResult and runs it when it lands.
	void (*ExecuteActionInfoFromMainNode)(TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction, TYPE_OBJECTID targetID, TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame, MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) = nullptr;
	void (*SkillCrossCounter)(MCreature* pUser, MCreature* pTarget, int skillID) = nullptr;
	// The skill's effect starting at a cell with nobody casting it (a mine
	// going off, an unseen caster's skill), carrying the result queue.
	void (*SkillToSector)(TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int direction, std::uint32_t delayFrame, MActionResult* pResult) = nullptr;
	// A sound heard from a cell (the listener's distance sets its volume).
	void (*PlaySoundAt)(TYPE_SOUNDID soundID, bool repeat, int x, int y) = nullptr;
	// How dark the view draws (0-15; the view ignores 16-255, all a
	// packet's byte can hold beyond; the adapter takes the low byte).
	void (*SetDarkBits)(int darkBits) = nullptr;
	// The creature goes into a portal (the vampire portal's effect).
	void (*MoveIntoPortal)(MCreature* pCreature) = nullptr;
	// A dragon tornado (or its child) of the object at the cell.
	void (*SetDragonTornado)(int effectStatus, std::uint32_t objectID, int x, int y) = nullptr;
	// An inventory item's effect after delayFrame frames, value being the
	// action's own (an option, say). For an enchant's option gained or lost,
	// a broken item or a sex change it is the item's change too: when it
	// lands, the host removes a broken item, changes an option or the sex,
	// and ends the item-to-item check, freeing the item in hand. (An option
	// lost on a pet item changes nothing: the check ends at once.) Other
	// actions only show.
	void (*AddNewInventoryEffect)(TYPE_OBJECTID id, TYPE_ACTIONINFO actionInfo, std::uint32_t delayFrame, std::uint32_t value) = nullptr;
	// The effect statuses a packet lists for a creature (the pairs of
	// status and duration EffectInfo holds): each is put on it for its
	// duration less delayedFrame, the frames already spent loading, and one
	// with no time left is skipped. A coma also makes the creature a
	// corpse; a trap's, Back Stab 3's, the ice of soul stone's and the
	// ground elemental centre's start their action. The host empties the
	// list, so read it first; pEffectInfo may be NULL.
	void (*SetEffectInfo)(MCreature* pCreature, EffectInfo* pEffectInfo, int delayedFrame) = nullptr;
	// Gilles de Rais's map effect (1 or 2), shown as the boss appears.
	void (*AddGDREffect)(int effect, bool bAppearBossMonster) = nullptr;
	// The creature comes up out of a portal (its body cut from the top).
	void (*ComeFromPortal)(MCreature* pCreature) = nullptr;
	// A monster in a slayer's or an ouster's shape. One the zone does not
	// know yet is made from the packet as a creature dressed for that race
	// and added to the zone; one it knows takes the packet's cell,
	// direction and health in place. A sacrifice (793, 795) is wounded.
	void (*AddRaceSlayerMonster)(GCAddMonster* pPacket) = nullptr;
	void (*AddRaceOustersMonster)(GCAddMonster* pPacket) = nullptr;
	// Skills shown with copies or summoned creatures, the user striking the
	// target: Shadow Dancing's and Illendue's fading copies (Illendue's
	// copy using skillID), Blaze Walk's strikes at its grade, the plasma
	// rocket launcher's rocket, and the wild wolf running at the target
	// (to eat it, when it is a corpse). Either creature may be NULL; then
	// nothing is shown.
	void (*SkillShadowDancing)(MCreature* pUser, MCreature* pTarget, int skillID) = nullptr;
	void (*SkillIllendue)(MCreature* pUser, MCreature* pTarget, int skillID) = nullptr;
	void (*SkillBlazeWalk)(MCreature* pUser, MCreature* pTarget, int skillID, std::uint8_t grade) = nullptr;
	void (*AddRocketLauncher)(MCreature* pUser, MCreature* pTarget) = nullptr;
	void (*AddWildWolf)(MCreature* pUser, MCreature* pTarget, bool bEatCorpse) = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
MItem* AddItemToZone(GCAddItemToZone* pPacket, bool bDropping);
void AddVampirePortal(int serverID, const char* ownerName, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint32_t delayFrame, int zoneID, TYPE_SECTORPOSITION zoneX, TYPE_SECTORPOSITION zoneY, bool bStartFromMainNode);
void SetBloodBibleSlot(BloodBibleSignInfo* pInfo);
void SetWeather(int weather, int level);
void SetLightning(std::uint32_t delay);
void SetPetInfo(PetInfo* pPetInfo, TYPE_OBJECTID objectID);
void RequestDisconnect(const char* pName);
void AddEvent(MEvent& event);
void RemoveEvent(int eventID);
void PlayMusicCurrentZone();
void PlaySound(TYPE_SOUNDID soundID);
void PlaySoundAt(TYPE_SOUNDID soundID, bool repeat, int x, int y);
void SetDarkBits(int darkBits);
void MoveIntoPortal(MCreature* pCreature);
void SetDragonTornado(int effectStatus, std::uint32_t objectID, int x, int y);
void AddNewInventoryEffect(TYPE_OBJECTID id, TYPE_ACTIONINFO actionInfo, std::uint32_t delayFrame, std::uint32_t value);
void SetEffectInfo(MCreature* pCreature, EffectInfo* pEffectInfo, int delayedFrame);
void AddGDREffect(int effect, bool bAppearBossMonster);
void ComeFromPortal(MCreature* pCreature);
void AddRaceSlayerMonster(GCAddMonster* pPacket);
void AddRaceOustersMonster(GCAddMonster* pPacket);
void SkillShadowDancing(MCreature* pUser, MCreature* pTarget, int skillID);
void SkillIllendue(MCreature* pUser, MCreature* pTarget, int skillID);
void SkillBlazeWalk(MCreature* pUser, MCreature* pTarget, int skillID, std::uint8_t grade);
void AddRocketLauncher(MCreature* pUser, MCreature* pTarget);
void AddWildWolf(MCreature* pUser, MCreature* pTarget, bool bEatCorpse);
// Either creature may be NULL; then nothing is shown.
void SkillCrossCounter(MCreature* pUser, MCreature* pTarget, int skillID);
// Takes pResult; without an entry it is freed unrun.
void ExecuteActionInfoFromMainNode(TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction, TYPE_OBJECTID targetID, TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame, MActionResult* pResult, bool bStartFromMainNode = true, int sX = 0, int sY = 0);
// Takes pResult; without an entry it is freed unrun.
void SkillToSector(TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int direction, std::uint32_t delayFrame, MActionResult* pResult);

}
