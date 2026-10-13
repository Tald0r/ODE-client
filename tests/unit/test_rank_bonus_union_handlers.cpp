#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"
#include "TempInformation.h"
#include "VsUiPresentationHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCSelectRankBonusFailed.h"
#include "Gpackets/GCUnionOfferList.h"
#include <string>
#include <vector>

// A refused rank bonus ends the pending request, says the skill is not
// supported and replays the mouse once. The guild-union offers become
// letters on the union mail tab: a guild applying to join or to leave,
// from its master, on its date; an offer of any other kind stops the rest.
namespace {

std::vector<std::string> calls;

std::string N(long long value) { return std::to_string(value); }

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.PopupFreeMessageDlg = [](const char* message) { calls.push_back("popup|" + std::string(message)); };
	host.EmptyMove = []() { calls.push_back("emptymove"); };
	return host;
}();

const VsUiPresentation::Host ui = []() {
	VsUiPresentation::Host host{};
	host.AddMail = [](int tabID, std::uint32_t id, int width, int height, const char* sender, const char* title,
		std::uint32_t date, bool open, int guildID, int guildType) {
		calls.push_back("mail|" + N(tabID) + "|" + N(id) + "|" + N(width) + "x" + N(height) + "|" + sender + "|" + title + "|"
			+ N(date) + "|" + N(open) + "|" + N(guildID) + "|" + N(guildType));
	};
	host.AddMailContents = [](int tabID, std::uint32_t id, const char* contents) {
		calls.push_back("contents|" + N(tabID) + "|" + N(id) + "|" + contents);
	};
	return host;
}();

struct World : GameModelWorld
{
	TempInformation temp;
	TempInformation* oldTemp = g_pTempInformation;
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const VsUiPresentation::Host* oldUi = VsUiPresentation::SetHost(&ui);
	World()
	{
		InitGameStringTable();
		g_pTempInformation = &temp;
		calls.clear();
	}
	~World()
	{
		VsUiPresentation::SetHost(oldUi);
		DialogPresentation::SetHost(oldDialog);
		g_pTempInformation = oldTemp;
	}
};

void ReceiveRankBonusFailed()
{
	GCSelectRankBonusFailed packet;
	GCSelectRankBonusFailedHandler::execute(&packet, nullptr);
}

struct Offer
{
	BYTE wireType;
	GuildID_t guild;
	const char* name;
	const char* master;
	DWORD date;
};

void ReceiveOffers(const std::vector<Offer>& offers)
{
	GCUnionOfferList original;
	for (const Offer& offer : offers)
	{
		SingleGuildUnionOffer* single = new SingleGuildUnionOffer;
		single->setGuildType(offer.wireType);
		single->setGuildID(offer.guild);
		single->setGuildName(offer.name);
		single->setGuildMaster(offer.master);
		single->setDate(offer.date);
		original.addUnionOfferList(single);
	}
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 2048);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	GCUnionOfferList packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	GCUnionOfferListHandler::execute(&packet, nullptr);
}

std::string Letter(const std::string& master, const std::string& title, DWORD date, GuildID_t guild, int type)
{
	return "mail|0|9|500x250|" + master + "|" + title + "|" + N(date) + "|0|" + N(guild) + "|" + N(type);
}

} // namespace

TEST(RankBonusUnionHandlers, ARefusedRankBonusEndsTheRequestSaysWhyAndReplaysTheMouse)
{
	World world;
	world.temp.SetMode(TempInformation::MODE_BONUSPOINT_USE);
	ReceiveRankBonusFailed();
	CHECK_EQ(static_cast<int>(TempInformation::MODE_NULL), static_cast<int>(world.temp.GetMode()));
	CHECK(calls == std::vector<std::string>({"popup|That skill is not supported yet.", "emptymove"}));
}

TEST(RankBonusUnionHandlers, EachUnionOfferBecomesALetterFromTheGuildMaster)
{
	World world;
	// On the wire a joining guild is kind 1 and a leaving one kind 2.
	ReceiveOffers({{1, 31, "Lanterns", "Vanda", 20261013}, {2, 32, "Moths", "Nyx", 20261014}});
	const std::string join = "The Lanterns guild is applying to join the union.";
	const std::string leave = "The Moths guild is applying to leave the union.";
	CHECK(calls == std::vector<std::string>({Letter("Vanda", join, 20261013, 31, SingleGuildUnionOffer::JOIN),
		"contents|0|9|" + join, Letter("Nyx", leave, 20261014, 32, SingleGuildUnionOffer::QUIT), "contents|0|9|" + leave}));
}

TEST(RankBonusUnionHandlers, AnOfferOfAnotherKindStopsTheRest)
{
	World world;
	ReceiveOffers({{1, 31, "Lanterns", "Vanda", 1}, {3, 33, "Odd", "Odd", 2}, {1, 34, "Late", "Late", 3}});
	const std::string join = "The Lanterns guild is applying to join the union.";
	CHECK(calls == std::vector<std::string>({Letter("Vanda", join, 1, 31, SingleGuildUnionOffer::JOIN), "contents|0|9|" + join}));
	calls.clear();
	ReceiveOffers({{0, 35, "Zero", "Zero", 4}});
	CHECK(calls.empty());
}

TEST(RankBonusUnionHandlers, MissingServicesSkipThePresentationNotTheRequest)
{
	World world;
	DialogPresentation::SetHost(nullptr);
	VsUiPresentation::SetHost(nullptr);
	world.temp.SetMode(TempInformation::MODE_BONUSPOINT_USE);
	ReceiveRankBonusFailed();
	ReceiveOffers({{1, 31, "Lanterns", "Vanda", 1}});
	CHECK_EQ(static_cast<int>(TempInformation::MODE_NULL), static_cast<int>(world.temp.GetMode()));
	const DialogPresentation::Host emptyDialog{};
	const VsUiPresentation::Host emptyUi{};
	DialogPresentation::SetHost(&emptyDialog);
	VsUiPresentation::SetHost(&emptyUi);
	ReceiveRankBonusFailed();
	ReceiveOffers({{1, 31, "Lanterns", "Vanda", 1}});
	CHECK(calls.empty());
}
