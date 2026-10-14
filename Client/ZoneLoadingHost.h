#pragma once

#include <cstdint>
#include "COrderedList.h"
#include "RaceType.h"

namespace ZoneLoading {

// What loading the player's information and zone needs from the program
// (GCUpdateInfo): sprites, windows, the progress bar, the view, the zone,
// the connection's encryption and the frame clock. A missing service is
// skipped and a missing answer is the default.
struct Host
{
	// The zone's events, dropped when the player's information arrives.
	void (*RemoveZoneEvents)() = nullptr;
	// Loads every addon sprite not loaded yet.
	void (*LoadAllAddonSprites)() = nullptr;
	// The interface's race, and changing it.
	Race (*InterfaceRace)() = nullptr;
	void (*ChangeInterfaceRace)(Race race) = nullptr;
	// Rebuilds the skill tree.
	void (*InitSkillTree)() = nullptr;
	// The character window's SMS charge and Powerjjang points.
	void (*SetSMSCharge)(int charge) = nullptr;
	void (*SetPowerjjangPoint)(int point) = nullptr;
	// Unlocks the gear, the items and the trade, and closes the shop, bookcase,
	// exchange, repair, sale, storage, party, trace, talk and message windows.
	void (*CloseWindows)() = nullptr;
	// The loading progress: started for a zone, drawn at a percent, ended.
	void (*StartProgress)(int zoneID) = nullptr;
	void (*DrawProgress)(int percent) = nullptr;
	void (*EndProgress)() = nullptr;
	// Initialises the view, unless it is already.
	void (*InitTopView)() = nullptr;
	// Whether monster sprites load up front (a user option), and loading one.
	bool (*PreloadsMonsters)() = nullptr;
	void (*LoadCreature)(int spriteType) = nullptr;
	// Whether a large zone is held.
	bool (*HasLargeZone)() = nullptr;
	// The sprite types a zone uses: added to the view's, or kept while every
	// other creature sprite is released.
	void (*AddMonsterSpriteTypes)(const COrderedList<int>& types) = nullptr;
	void (*ReleaseCreatureSpritesExcept)(const COrderedList<int>& types) = nullptr;
	// Moves to the zone from the game, or loads it.
	void (*MoveZone)(int zoneID) = nullptr;
	void (*LoadZone)(int zoneID) = nullptr;
	// Turns on the game connection's packet encryption.
	void (*SetEncryptCode)() = nullptr;
	// Stamps the frame clock, converts milliseconds to frames, and advances
	// the frame count.
	void (*StampFrameClock)() = nullptr;
	std::uint32_t (*MillisecondsToFrames)(std::uint32_t ms) = nullptr;
	void (*AdvanceFrame)(int frames) = nullptr;
	// The player's bat colour set.
	void (*SetMyBatColorSet)(std::uint16_t colorSet) = nullptr;
	// Shows the player's HP again.
	void (*ShowPlayerHP)() = nullptr;
	// Waits for the player's position.
	void (*EnterWaitSetPositionMode)() = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
void RemoveZoneEvents();
void LoadAllAddonSprites();
Race InterfaceRace();
void ChangeInterfaceRace(Race race);
void InitSkillTree();
void SetSMSCharge(int charge);
void SetPowerjjangPoint(int point);
void CloseWindows();
void StartProgress(int zoneID);
void DrawProgress(int percent);
void EndProgress();
void InitTopView();
bool PreloadsMonsters();
void LoadCreature(int spriteType);
bool HasLargeZone();
void AddMonsterSpriteTypes(const COrderedList<int>& types);
void ReleaseCreatureSpritesExcept(const COrderedList<int>& types);
void MoveZone(int zoneID);
void LoadZone(int zoneID);
void SetEncryptCode();
void StampFrameClock();
std::uint32_t MillisecondsToFrames(std::uint32_t ms);
void AdvanceFrame(int frames);
void SetMyBatColorSet(std::uint16_t colorSet);
void ShowPlayerHP();
void EnterWaitSetPositionMode();

}
