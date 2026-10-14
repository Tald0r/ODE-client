#include "test_framework.h"
#include "gamemodel_world.h"
#include "CommunicationPresentationHost.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"
#include "VsUiPresentationHost.h"
#include "Gpackets/GCExchangeBuy.h"
#include "Gpackets/GCRequestPowerPointResult.h"
#include "Gpackets/GCUsePowerPointResult.h"
#include <string>
#include <vector>

// Power points asked for: on success the player's points are the new sum
// and a message says how many were added; each failure has its message.
// Power points spent on a gamble: on success the points are what is left
// and the gamble's prize is shown; a full inventory or too few points
// have their messages. A point exchange purchase shows the server's
// message, or a general error when it failed without one, and a success
// reloads the exchange window.
namespace {

std::vector<std::string> calls;

std::string N(long long value) { return std::to_string(value); }

const VsUiPresentation::Host vsui = []() {
	VsUiPresentation::Host host{};
	host.SetPowerjjangPoint = [](int points) { calls.push_back("points|" + N(points)); };
	host.PowerjjangGambleResult = [](int itemCode) { calls.push_back("prize|" + N(itemCode)); };
	host.RefreshPointExchange = []() { calls.push_back("exchange reloaded"); };
	return host;
}();

const CommunicationPresentation::Host communication = []() {
	CommunicationPresentation::Host host{};
	host.PopupMessage = [](int id) { calls.push_back("popup|" + N(id)); };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.PopupFreeMessageDlg = [](const char* message) { calls.push_back(std::string("message|") + message); };
	return host;
}();

struct World : GameModelWorld
{
	const VsUiPresentation::Host* oldVsUi = VsUiPresentation::SetHost(&vsui);
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	World()
	{
		InitGameStringTable();
		calls.clear();
	}
	~World()
	{
		DialogPresentation::SetHost(oldDialog);
		CommunicationPresentation::SetHost(oldCommunication);
		VsUiPresentation::SetHost(oldVsUi);
	}
};

void ReceiveRequested(int error, int sum = 0, int requested = 0)
{
	GCRequestPowerPointResult packet;
	packet.setErrorCode(static_cast<BYTE>(error));
	packet.setSumPowerPoint(sum);
	packet.setRequestPowerPoint(requested);
	GCRequestPowerPointResultHandler::execute(&packet, nullptr);
}

void ReceiveSpent(int error, unsigned int left = 0, int prize = 0)
{
	GCUsePowerPointResult packet;
	packet.setErrorCode(static_cast<BYTE>(error));
	packet.setPowerPoint(left);
	packet.setItemCode(static_cast<BYTE>(prize));
	GCUsePowerPointResultHandler::execute(&packet, nullptr);
}

void ReceiveExchange(bool success, const std::string& message)
{
	GCExchangeBuy packet;
	packet.setSuccess(success);
	packet.setMessage(message);
	GCExchangeBuyHandler::execute(&packet, nullptr);
}

std::string Popup(int id) { return "popup|" + N(id); }

} // namespace

TEST(PowerPointReplies, RequestedPointsAreAddedAndSaid)
{
	World world;
	ReceiveRequested(GCRequestPowerPointResult::_NO_ERROR, 1500, 300);
	char said[128];
	SafeFormat::Format(said, GetGameString(UI_STRING_POWER_JJANG_REQUEST_OK), 300);
	CHECK(std::string(said).find("300") != std::string::npos);
	CHECK(calls == std::vector<std::string>({"points|1500", std::string("message|") + said}));
}

TEST(PowerPointReplies, EachRequestFailureHasItsMessage)
{
	World world;
	const struct { int error; int text; } failures[] = {
		{GCRequestPowerPointResult::SERVER_ERROR, UI_STRING_POWER_JJANG_ERROR_SERVER_ERROR},
		{GCRequestPowerPointResult::PROCESS_ERROR, UI_STRING_POWER_JJANG_ERROR_PROCESS_ERROR},
		{GCRequestPowerPointResult::NO_MEMBER, UI_STRING_POWER_JJANG_ERROR_NO_MEMBER},
		{GCRequestPowerPointResult::NO_POINT, UI_STRING_POWER_JJANG_ERROR_NO_POINT},
		{GCRequestPowerPointResult::NO_MATCHING, UI_STRING_POWER_JJANG_ERROR_NO_MATCHING},
		{GCRequestPowerPointResult::CONNECT_ERROR, UI_STRING_POWER_JJANG_ERROR_CONNECT},
		{250, STRING_ERROR_ETC_ERROR}};
	for (const auto& f : failures)
	{
		calls.clear();
		ReceiveRequested(f.error, 1500, 300);
		CHECK(calls == std::vector<std::string>{Popup(f.text)});
	}
}

TEST(PowerPointReplies, SpentPointsShowThePrizeOrWhyNot)
{
	World world;
	ReceiveSpent(GCUsePowerPointResult::_NO_ERROR, 700, 12);
	CHECK(calls == std::vector<std::string>({"points|700", "prize|12"}));
	calls.clear();
	ReceiveSpent(GCUsePowerPointResult::NOT_ENOUGH_INVENTORY_SPACE, 700, 12);
	ReceiveSpent(GCUsePowerPointResult::NOT_ENOUGH_POWER_POINT, 700, 12);
	ReceiveSpent(250, 700, 12);
	CHECK(calls == std::vector<std::string>({Popup(UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST),
		Popup(UI_STRING_POWER_JJANG_ERROR_NO_POINT), Popup(STRING_ERROR_ETC_ERROR)}));
}

TEST(PowerPointReplies, AnExchangePurchaseSaysWhatTheServerSaid)
{
	World world;
	ReceiveExchange(true, "Bought.");
	CHECK(calls == std::vector<std::string>({"message|Bought.", "exchange reloaded"}));
	calls.clear();
	ReceiveExchange(true, "");
	CHECK(calls == std::vector<std::string>{"exchange reloaded"});
	calls.clear();
	ReceiveExchange(false, "Sold out.");
	CHECK(calls == std::vector<std::string>{"message|Sold out."});
	calls.clear();
	ReceiveExchange(false, "");
	CHECK(calls == std::vector<std::string>{Popup(STRING_ERROR_ETC_ERROR)});
}

TEST(PowerPointReplies, WithoutAHostNothingIsShown)
{
	World world;
	VsUiPresentation::SetHost(nullptr);
	ReceiveSpent(GCUsePowerPointResult::_NO_ERROR, 700, 12);
	ReceiveExchange(true, "");
	const VsUiPresentation::Host bare{};
	VsUiPresentation::SetHost(&bare);
	ReceiveSpent(GCUsePowerPointResult::_NO_ERROR, 700, 12);
	ReceiveRequested(GCRequestPowerPointResult::_NO_ERROR, 1500, 300);
	ReceiveExchange(true, "");
	CHECK(calls.size() == 1 && calls[0].rfind("message|", 0) == 0);
}
