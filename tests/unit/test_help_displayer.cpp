#include "test_framework.h"
#include "MHelpDisplayer.h"
#include "MHelpStringTable.h"
#include "CMessageArray.h"
#include "FrameClock.h"
#include "GameMessages.h"
#include <string>

// A help message goes to the help log unless the last one shown is still
// up (10 seconds, by the frame clock), in which case it is dropped, not
// queued. A help already shown is never shown again until the table's
// display history is cleared, and a help past the table is ignored.
namespace {

const MonotonicClock::TimePoint kStart = MonotonicClock::TimePoint() + MonotonicClock::Millis(50000);

MonotonicClock::TimePoint At(DWORD millis) { return kStart + MonotonicClock::Millis(millis); }

struct World
{
	MHelpStringTable table;
	CMessageArray log;
	MHelpStringTable* oldTable = g_pHelpStringTable;
	CMessageArray* oldLog = g_pHelpMessage;
	MonotonicClock::TimePoint oldNow = g_FrameNow;
	MHelpDisplayer displayer;
	World()
	{
		table.Init(3);
		*table.GetMutable(HO_ALT_TO_PICKUP_ITEM) = "Alt picks items up";
		*table.GetMutable(HO_ALT_TO_TRADE) = "Alt trades";
		*table.GetMutable(HO_ATTACK_SLAYER_TO_DRAIN) = "Attack a slayer to drain";
		log.Init(8, 128);
		g_pHelpStringTable = &table;
		g_pHelpMessage = &log;
		g_FrameNow = kStart;
	}
	~World()
	{
		g_FrameNow = oldNow;
		g_pHelpMessage = oldLog;
		g_pHelpStringTable = oldTable;
	}
	std::string Newest() { return log[log.GetSize() - 1]; }
	std::string Before() { return log[log.GetSize() - 2]; }
	void Show(HELP_OUTPUT help, DWORD at)
	{
		g_FrameNow = At(at);
		displayer.OutputHelp(help);
	}
};

} // namespace

TEST(HelpDisplayer, AHelpIsShownAndKeepsTheNextBackForTenSeconds)
{
	World world;
	world.Show(HO_ALT_TO_PICKUP_ITEM, 0);
	CHECK(world.Newest() == "Alt picks items up");
	CHECK(world.table.IsDisplayed(HO_ALT_TO_PICKUP_ITEM));

	// Within the window, and at its very end, the next help is dropped and
	// stays unshown.
	world.Show(HO_ALT_TO_TRADE, 9999);
	world.Show(HO_ALT_TO_TRADE, 10000);
	CHECK(world.Newest() == "Alt picks items up");
	CHECK(!world.table.IsDisplayed(HO_ALT_TO_TRADE));

	// Past it, the help is shown and starts a window of its own.
	world.Show(HO_ALT_TO_TRADE, 10001);
	CHECK(world.Newest() == "Alt trades");
	CHECK(world.Before() == "Alt picks items up");
	world.Show(HO_ATTACK_SLAYER_TO_DRAIN, 20001);
	CHECK(world.Newest() == "Alt trades");
	world.Show(HO_ATTACK_SLAYER_TO_DRAIN, 20002);
	CHECK(world.Newest() == "Attack a slayer to drain");
}

TEST(HelpDisplayer, AHelpAlreadyShownIsSkippedWithoutHoldingTheNextBack)
{
	World world;
	world.Show(HO_ALT_TO_PICKUP_ITEM, 0);
	world.Show(HO_ALT_TO_PICKUP_ITEM, 20000);
	CHECK(world.Before() != "Alt picks items up");
	// The skipped help started no window: another is shown at once.
	world.Show(HO_ALT_TO_TRADE, 20000);
	CHECK(world.Newest() == "Alt trades");
	CHECK(world.Before() == "Alt picks items up");

	// Clearing the display history lets it be shown again.
	world.table.ClearDisplayed();
	world.Show(HO_ALT_TO_PICKUP_ITEM, 40000);
	CHECK(world.Newest() == "Alt picks items up");
}

TEST(HelpDisplayer, AHelpPastTheTableIsIgnored)
{
	World world;
	world.Show(HO_BELT_TOGGLE_KEY, 0);
	world.Show(MAX_HELP_OUTPUT, 0);
	CHECK(world.Newest().empty());
	// It started no window either.
	world.Show(HO_ALT_TO_TRADE, 0);
	CHECK(world.Newest() == "Alt trades");
}
