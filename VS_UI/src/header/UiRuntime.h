#pragma once

#include "PortalFlag.h"
#include "GuildMarkTypes.h"
#include <cstddef>
#include <cstdint>
#include <list>
#include <string>

class CSprite;
class Packet;

namespace UiRuntime {
enum class MarkSize { Large, Small };
using HornPortals = std::list<UI_PORTAL_FLAG>;

struct PetProgress {
	int experienceRemaining = 0;
	int level = 0;
};

struct ExchangeFilter {
	int page = 1;
	int pageSize = 20;
	std::uint8_t itemClass = 0xff;
	std::uint16_t itemType = 0xffff;
	int minPrice = 0;
	int maxPrice = 0;
	// A part of the seller's player name; empty matches every seller.
	std::string sellerFilter;
};

// Borrowed callbacks, installed before UI initialization and cleared at shutdown.
// Sprites remain owned by the renderer; portal and pet data are copied.
// SendPacket borrows its packet synchronously and must not retain its address.
struct Host {
	bool (*EventFlagActive)(std::uint32_t) = nullptr;
	CSprite* (*FindGuildMark)(std::uint16_t, MarkSize) = nullptr;
	void (*LoadGuildMark)(std::uint16_t) = nullptr;
	CSprite* (*GradeMark)(std::uint16_t, int race, MarkSize) = nullptr;
	std::size_t (*HornMapCount)() = nullptr;
	bool (*ReadHornPortals)(std::size_t, HornPortals&) = nullptr;
	bool (*ReadPetProgress)(std::uint32_t itemID, PetProgress&) = nullptr;
	bool (*SendPacket)(Packet&) = nullptr;
	// The blood bible's effect behind its item in the gear window, at the
	// screen point (the view draws it).
	void (*DrawBloodBibleEffect_InGear)(int x, int y) = nullptr;
};

const Host* SetHost(const Host* host);
bool EventFlagActive(std::uint32_t flag);
CSprite* GuildMark(std::uint16_t guildID, MarkSize size);
CSprite* GradeMark(std::uint16_t grade, int race, MarkSize size);
std::size_t HornMapCount();
HornPortals ReadHornPortals(std::size_t map);
// Failure leaves the caller's last complete progress unchanged.
bool ReadPetProgress(std::uint32_t itemID, PetProgress& progress);
bool RequestExchangeList(const ExchangeFilter& filter);
bool RequestExchangeBuy(std::uint64_t listingID);
void DrawBloodBibleEffect_InGear(int x, int y);
}
