#pragma once

#include <cstdint>
#include "MTypeDef.h"

class BloodBibleSignInfo;
class GCAddItemToZone;
class MEvent;
class MItem;
class PetInfo;

namespace WorldEvents {

// What a reply hands on to the executable's world and connections whole:
// an item or portal appearing in the zone, the blood bible slots, the
// weather and lightning, a pet's details, a peer asking to disconnect, and
// an event for the event manager to run (a timed logout, say) or to drop,
// the current zone's music played again, and a sound played.
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

}
