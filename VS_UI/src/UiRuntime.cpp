#include "UiRuntime.h"
#include "Cpackets/CGExchangeList.h"
#include "Cpackets/CGExchangeBuy.h"

namespace UiRuntime {
namespace {
const Host* activeHost = nullptr;
}

const Host* SetHost(const Host* host)
{
	const auto* previous = activeHost;
	activeHost = host;
	return previous;
}

bool EventFlagActive(std::uint32_t flag)
{
	return activeHost && activeHost->EventFlagActive && activeHost->EventFlagActive(flag);
}

CSprite* GuildMark(std::uint16_t guildID, MarkSize size)
{
	if (!activeHost || !activeHost->FindGuildMark) return nullptr;
	auto* mark = activeHost->FindGuildMark(guildID, size);
	if (mark || !activeHost->LoadGuildMark) return mark;
	activeHost->LoadGuildMark(guildID);
	return activeHost->FindGuildMark(guildID, size);
}

CSprite* GradeMark(std::uint16_t grade, int race, MarkSize size)
{
	return activeHost && activeHost->GradeMark ? activeHost->GradeMark(grade, race, size) : nullptr;
}

std::size_t HornMapCount()
{
	return activeHost && activeHost->HornMapCount ? activeHost->HornMapCount() : 0;
}

HornPortals ReadHornPortals(std::size_t map)
{
	HornPortals portals;
	if (!activeHost || !activeHost->ReadHornPortals || !activeHost->ReadHornPortals(map, portals))
		portals.clear();
	return portals;
}

bool ReadPetProgress(std::uint32_t itemID, PetProgress& progress)
{
	PetProgress next;
	if (!activeHost || !activeHost->ReadPetProgress || !activeHost->ReadPetProgress(itemID, next))
		return false;
	progress = next;
	return true;
}

bool RequestExchangeList(const ExchangeFilter& filter)
{
	if (!activeHost || !activeHost->SendPacket) return false;
	CGExchangeList packet;
	packet.setPage(filter.page);
	packet.setPageSize(filter.pageSize);
	packet.setItemClass(filter.itemClass);
	packet.setItemType(filter.itemType);
	packet.setMinPrice(filter.minPrice);
	packet.setMaxPrice(filter.maxPrice);
	packet.setSellerFilter(filter.sellerFilter);
	return activeHost->SendPacket(packet);
}

bool RequestExchangeBuy(std::uint64_t listingID)
{
	if (!activeHost || !activeHost->SendPacket) return false;
	CGExchangeBuy packet;
	packet.setListingID(listingID);
	return activeHost->SendPacket(packet);
}

void DrawBloodBibleEffect_InGear(int x, int y)
{
	if (activeHost && activeHost->DrawBloodBibleEffect_InGear) activeHost->DrawBloodBibleEffect_InGear(x, y);
}

}
