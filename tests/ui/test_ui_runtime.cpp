#include "test_framework.h"
#include "UiRuntime.h"
#include "CSprite565.h"
#include "Cpackets/CGExchangeList.h"
#include "Cpackets/CGExchangeBuy.h"
#include <string>
#include <utility>
#include <vector>

namespace {
struct HostScope {
	const UiRuntime::Host* previous;
	explicit HostScope(const UiRuntime::Host* host) : previous(UiRuntime::SetHost(host)) {}
	~HostScope() { UiRuntime::SetHost(previous); }
};
struct Capture {
	std::vector<std::string> marks;
	CSprite* small = nullptr;
	CSprite* large = nullptr;
	bool loaded = false;
	bool publishMark = true;
	std::uint32_t flags = 0;
	std::uint32_t lastFlag = 0;
	std::uint16_t grade = 0;
	int race = -1;
	UiRuntime::MarkSize gradeSize = UiRuntime::MarkSize::Large;
	std::size_t mapCount = 0;
	std::size_t lastMap = 0;
	UiRuntime::HornPortals portals;
	bool readable = true;
	std::uint32_t petID = 0;
	UiRuntime::PetProgress pet;
	std::vector<int> packetIDs;
	UiRuntime::ExchangeFilter filter;
	std::uint64_t listingID = 0;
	std::string seller, idempotencyKey;
	bool sent = true;
} capture;

bool EventFlag(std::uint32_t flag)
{
	capture.lastFlag = flag;
	return (capture.flags & flag) != 0;
}
CSprite* FindMark(std::uint16_t id, UiRuntime::MarkSize size)
{
	capture.marks.push_back("find:" + std::to_string(id)
		+ (size == UiRuntime::MarkSize::Small ? ":small" : ":large"));
	return !capture.loaded ? nullptr : size == UiRuntime::MarkSize::Small ? capture.small : capture.large;
}
void LoadMark(std::uint16_t id)
{
	capture.marks.push_back("load:" + std::to_string(id));
	capture.loaded = capture.publishMark;
}
CSprite* Grade(std::uint16_t grade, int race, UiRuntime::MarkSize size)
{
	capture.grade = grade;
	capture.race = race;
	capture.gradeSize = size;
	return size == UiRuntime::MarkSize::Small ? capture.small : capture.large;
}
std::size_t MapCount() { return capture.mapCount; }
bool ReadPortals(std::size_t map, UiRuntime::HornPortals& portals)
{
	capture.lastMap = map;
	portals = capture.portals;
	return capture.readable;
}
bool ReadPet(std::uint32_t item, UiRuntime::PetProgress& progress)
{
	capture.petID = item;
	progress = capture.pet;
	return capture.readable;
}
bool Send(Packet& packet)
{
	capture.packetIDs.push_back(packet.getPacketID());
	if (packet.getPacketID() == Packet::PACKET_CG_EXCHANGE_LIST) {
		const auto& list = static_cast<const CGExchangeList&>(packet);
		capture.filter = {list.getPage(), list.getPageSize(), list.getItemClass(),
			list.getItemType(), list.getMinPrice(), list.getMaxPrice(), {}};
		capture.seller = list.getSellerFilter();
	} else if (packet.getPacketID() == Packet::PACKET_CG_EXCHANGE_BUY) {
		const auto& buy = static_cast<const CGExchangeBuy&>(packet);
		capture.listingID = buy.getListingID();
		capture.idempotencyKey = buy.getIdempotencyKey();
	}
	return capture.sent;
}
const UiRuntime::Host host{
	.EventFlagActive = EventFlag,
	.FindGuildMark = FindMark,
	.LoadGuildMark = LoadMark,
	.GradeMark = Grade,
	.HornMapCount = MapCount,
	.ReadHornPortals = ReadPortals,
	.ReadPetProgress = ReadPet,
	.SendPacket = Send,
};
}

TEST(UiRuntime, MissingAndEmptyHostsHaveNoLiveStateOrTransport)
{
	const UiRuntime::Host empty{};
	for (const auto* value : {static_cast<const UiRuntime::Host*>(nullptr), &empty}) {
		HostScope scope(value);
		CHECK(!UiRuntime::EventFlagActive(0x8000));
		CHECK(UiRuntime::GuildMark(4, UiRuntime::MarkSize::Small) == nullptr);
		CHECK(UiRuntime::GradeMark(4, 0, UiRuntime::MarkSize::Large) == nullptr);
		CHECK_EQ(0, UiRuntime::HornMapCount());
		CHECK(UiRuntime::ReadHornPortals(0).empty());
		UiRuntime::PetProgress progress{42, 7};
		CHECK(!UiRuntime::ReadPetProgress(17, progress));
		CHECK_EQ(42, progress.experienceRemaining);
		CHECK_EQ(7, progress.level);
		CHECK(!UiRuntime::RequestExchangeList({}));
		CHECK(!UiRuntime::RequestExchangeBuy(17));
	}
}

TEST(UiRuntime, GuildCacheMissLoadsOnceAndThenRepeatsTheSameLookup)
{
	capture = {};
	CSprite565 small, large;
	capture.small = &small;
	capture.large = &large;
	HostScope scope(&host);
	CHECK(UiRuntime::GuildMark(37, UiRuntime::MarkSize::Small) == &small);
	CHECK((capture.marks == std::vector<std::string>{"find:37:small", "load:37", "find:37:small"}));
	capture.marks.clear();
	CHECK(UiRuntime::GuildMark(37, UiRuntime::MarkSize::Large) == &large);
	CHECK((capture.marks == std::vector<std::string>{"find:37:large"}));
	capture.loaded = false;
	capture.publishMark = false;
	capture.marks.clear();
	CHECK(UiRuntime::GuildMark(91, UiRuntime::MarkSize::Large) == nullptr);
	CHECK((capture.marks == std::vector<std::string>{"find:91:large", "load:91", "find:91:large"}));
	const UiRuntime::Host noLoader{.FindGuildMark = FindMark};
	HostScope partial(&noLoader);
	capture.marks.clear();
	CHECK(UiRuntime::GuildMark(91, UiRuntime::MarkSize::Small) == nullptr);
	CHECK((capture.marks == std::vector<std::string>{"find:91:small"}));
}

TEST(UiRuntime, EventAndGradeQueriesReadCurrentHostValues)
{
	capture = {};
	CSprite565 small, large;
	capture.small = &small;
	capture.large = &large;
	HostScope scope(&host);
	CHECK(!UiRuntime::EventFlagActive(0x8000));
	capture.flags = 0x8000;
	CHECK(UiRuntime::EventFlagActive(0x8000));
	CHECK_EQ(0x8000, capture.lastFlag);
	CHECK(!UiRuntime::EventFlagActive(0x4000));
	CHECK_EQ(0x4000, capture.lastFlag);
	CHECK(UiRuntime::GradeMark(49, 2, UiRuntime::MarkSize::Small) == &small);
	CHECK_EQ(49, capture.grade);
	CHECK_EQ(2, capture.race);
	CHECK(capture.gradeSize == UiRuntime::MarkSize::Small);
	CHECK(UiRuntime::GradeMark(1, 0, UiRuntime::MarkSize::Large) == &large);
	CHECK_EQ(1, capture.grade);
	CHECK_EQ(0, capture.race);
}

TEST(UiRuntime, SnapshotReadsDoNotExposeHostStorageOrPublishFailedPetReads)
{
	capture = {};
	capture.mapCount = 3;
	capture.portals.push_back({71, 20, 30, 4, 5});
	capture.pet = {100, 9};
	HostScope scope(&host);
	CHECK_EQ(3, UiRuntime::HornMapCount());
	capture.mapCount = 6;
	CHECK_EQ(6, UiRuntime::HornMapCount());
	auto portals = UiRuntime::ReadHornPortals(2);
	CHECK_EQ(2, capture.lastMap);
	CHECK_EQ(1, portals.size());
	if (!portals.empty()) CHECK_EQ(71, portals.front().zone_id);
	capture.portals.front().zone_id = 72;
	if (!portals.empty()) CHECK_EQ(71, portals.front().zone_id);
	CHECK_EQ(72, UiRuntime::ReadHornPortals(2).front().zone_id);
	UiRuntime::PetProgress progress{42, 7};
	CHECK(UiRuntime::ReadPetProgress(0xfedcba98, progress));
	CHECK_EQ(0xfedcba98, capture.petID);
	CHECK_EQ(100, progress.experienceRemaining);
	CHECK_EQ(9, progress.level);
	capture.readable = false;
	capture.pet = {0, 0};
	CHECK(UiRuntime::ReadHornPortals(99).empty());
	CHECK_EQ(99, capture.lastMap);
	CHECK(!UiRuntime::ReadPetProgress(4, progress));
	CHECK_EQ(100, progress.experienceRemaining);
	CHECK_EQ(9, progress.level);
}

TEST(UiRuntime, ExchangeRequestsBuildRealPacketsAndBorrowThemForOneSend)
{
	capture = {};
	HostScope scope(&host);
	const UiRuntime::ExchangeFilter filter{3, 50, 12, 345, 100, 9000, {}};
	CHECK(UiRuntime::RequestExchangeList(filter));
	CHECK_EQ(3, capture.filter.page);
	CHECK_EQ(50, capture.filter.pageSize);
	CHECK_EQ(12, capture.filter.itemClass);
	CHECK_EQ(345, capture.filter.itemType);
	CHECK_EQ(100, capture.filter.minPrice);
	CHECK_EQ(9000, capture.filter.maxPrice);
	CHECK(capture.seller.empty());
	CHECK(UiRuntime::RequestExchangeBuy(0xfedcba9876543210ULL));
	CHECK_EQ(0xfedcba9876543210ULL, capture.listingID);
	CHECK(capture.idempotencyKey.empty());
	CHECK((capture.packetIDs == std::vector<int>{Packet::PACKET_CG_EXCHANGE_LIST, Packet::PACKET_CG_EXCHANGE_BUY}));
	capture.sent = false;
	CHECK(!UiRuntime::RequestExchangeList({}));
	CHECK(!UiRuntime::RequestExchangeBuy(4));
	CHECK_EQ(4, capture.packetIDs.size());
}

TEST(UiRuntime, ExchangeListCarriesTheSellerFilterItWasGiven)
{
	capture = {};
	HostScope scope(&host);
	UiRuntime::ExchangeFilter filter;
	filter.sellerFilter = "Kain";
	CHECK(UiRuntime::RequestExchangeList(filter));
	CHECK(capture.seller == "Kain");
	CHECK_EQ(1, capture.filter.page);
	CHECK_EQ(0xff, capture.filter.itemClass);
	CHECK_EQ(0xffff, capture.filter.itemType);
	// An empty filter sends an empty seller: no filter at all.
	CHECK(UiRuntime::RequestExchangeList({}));
	CHECK(capture.seller.empty());
	CHECK_EQ(2, capture.packetIDs.size());
}

TEST(UiRuntime, TheBloodBiblesGearEffectIsTheViews)
{
	// Task 4.236: the gear window draws it through the host (the view
	// draws behind the item at the point).
	static std::vector<std::pair<int, int>> drawn;
	drawn.clear();
	UiRuntime::DrawBloodBibleEffect_InGear(1, 2);
	UiRuntime::Host drawing{};
	drawing.DrawBloodBibleEffect_InGear = [](int x, int y) { drawn.push_back({x, y}); };
	{
		HostScope scope(&drawing);
		UiRuntime::DrawBloodBibleEffect_InGear(30, 47);
	}
	UiRuntime::Host none{};
	{
		HostScope scope(&none);
		UiRuntime::DrawBloodBibleEffect_InGear(5, 6);
	}
	CHECK((drawn == std::vector<std::pair<int, int>>{{30, 47}}));
}
