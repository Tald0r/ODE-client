#pragma once

#include <cstdint>

class GCMiniGameScores;
class MCreature;

namespace GameInfoPresentation {

// NPC map, skill teaching, mini-game and prize-notice replies only present
// what they receive. A missing service skips that presentation; a packet
// or string is borrowed for the duration of the call.
struct Host
{
	void (*ClearNPC)() = nullptr;
	void (*SetNPCInfo)(const char* pName, int npcID, int x, int y) = nullptr;
	void (*RunSkillTree)(int domain, int maxLevel) = nullptr;
	void (*MiniGameScores)(GCMiniGameScores* pPacket) = nullptr;
	void (*RunImageNotice)(const char* name, std::uint32_t id) = nullptr;
	// The level the character window shows; 0 without a host.
	int (*GetCharInfoLevel)() = nullptr;
	// Eternity failed: its button waits out its delay.
	void (*SetDelayEternity)() = nullptr;
	// The name of the creature the other-info window shows; NULL for none,
	// or without a host.
	const char* (*GetOtherInfoName)() = nullptr;
	// Shows the creature in the other-info window.
	void (*RunOtherInfo)(MCreature* pCreature) = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
void ClearNPC();
void SetNPCInfo(const char* pName, int npcID, int x, int y);
void RunSkillTree(int domain, int maxLevel);
void MiniGameScores(GCMiniGameScores* pPacket);
void RunImageNotice(const char* name, std::uint32_t id);
int GetCharInfoLevel();
void SetDelayEternity();
const char* GetOtherInfoName();
void RunOtherInfo(MCreature* pCreature);

}
