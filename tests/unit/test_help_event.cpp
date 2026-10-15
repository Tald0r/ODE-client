#include "test_framework.h"
#include "HelpEvent.h"
#include "ClientMode.h"
#include <vector>

// A help event (task 4.222): in game mode it adds the event's help mail
// through the host; in any other mode, or without a host, nothing.
namespace {

std::vector<int> mails;
const MHelpHost host{ .AddHelpMail = [](HELP_EVENT he) { mails.push_back(he); } };

struct Fixture
{
	CLIENT_MODE mode = g_Mode;
	Fixture() { mails.clear(); SetHelpHost(&host); }
	~Fixture() { SetHelpHost(nullptr); g_Mode = mode; }
};

} // namespace

TEST(HelpEvent, InGameModeItAddsTheEventsMail)
{
	Fixture fixture;
	g_Mode = MODE_GAME;
	ExecuteHelpEvent(HELP_EVENT_INTERFACE);
	ExecuteHelpEvent(HELP_EVENT_USE_POTION);
	CHECK((mails == std::vector<int>{HELP_EVENT_INTERFACE, HELP_EVENT_USE_POTION}));
}

TEST(HelpEvent, OutsideGameModeNothing)
{
	Fixture fixture;
	g_Mode = MODE_WAIT_SELECTPC;
	ExecuteHelpEvent(HELP_EVENT_INTERFACE);
	g_Mode = MODE_QUIT;
	ExecuteHelpEvent(HELP_EVENT_INTERFACE);
	CHECK(mails.empty());
}

TEST(HelpEvent, WithoutAHostNothing)
{
	Fixture fixture;
	g_Mode = MODE_GAME;
	SetHelpHost(nullptr);
	ExecuteHelpEvent(HELP_EVENT_INTERFACE);
	const MHelpHost empty{};
	SetHelpHost(&empty);
	ExecuteHelpEvent(HELP_EVENT_INTERFACE);
	CHECK(mails.empty());
}
