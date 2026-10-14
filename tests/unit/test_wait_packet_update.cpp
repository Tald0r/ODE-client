#include "test_framework.h"
#include "CWaitPacketUpdate.h"
#include "FrameClock.h"
#include <string>

// The loop that waits for the server (task 4.209). Init clears the input
// receivers. SetDelay stamps the frame clock and gives up delay ms later.
// Each Update:
// - past the limit (strictly), goes back to the main menu, disconnected,
//   and carries on;
// - reads the socket and stops there when that fails;
// - enters the queued next mode, clearing it, and stops there;
// - otherwise writes the socket.
namespace {

std::string calls;
bool socketInputOk = true;
const MonotonicClock::TimePoint t0 = MonotonicClock::TimePoint() + MonotonicClock::Millis(100000);

const CWaitPacketHost host{
	.ClearInputReceivers = []() { calls += "clear;"; },
	.StampFrameClock = []() { calls += "stamp;"; g_FrameNow = t0; },
	.SetMode = [](CLIENT_MODE mode) { calls += "mode" + std::to_string(static_cast<int>(mode)) + ";"; },
	.UpdateDisconnected = []() { calls += "disconnected;"; },
	.UpdateSocketInput = []() { calls += "in;"; return socketInputOk; },
	.UpdateSocketOutput = []() { calls += "out;"; return true; },
};

// Saves and restores the clock and the queued mode around a test.
struct Fixture
{
	MonotonicClock::TimePoint frameNow = g_FrameNow;
	CLIENT_MODE modeNext = g_ModeNext;
	Fixture() { calls.clear(); socketInputOk = true; g_ModeNext = MODE_NULL; }
	~Fixture() { g_FrameNow = frameNow; g_ModeNext = modeNext; }
};

const std::string mainMenu = "mode" + std::to_string(static_cast<int>(MODE_MAINMENU)) + ";";

} // namespace

TEST(WaitPacketUpdate, InitClearsTheInputReceivers)
{
	Fixture fixture;
	CWaitPacketUpdate wait(&host);
	wait.Init();
	CHECK(calls == "clear;");
}

TEST(WaitPacketUpdate, WithoutADelayItNeverGivesUp)
{
	Fixture fixture;
	CWaitPacketUpdate wait(&host);
	g_FrameNow = (MonotonicClock::TimePoint::max)();
	wait.Update();
	CHECK(calls == "in;out;");
}

TEST(WaitPacketUpdate, ItGivesUpOnlyPastTheDelay)
{
	Fixture fixture;
	CWaitPacketUpdate wait(&host);
	wait.SetDelay(500);
	CHECK(calls == "stamp;");

	calls.clear();
	g_FrameNow = t0 + MonotonicClock::Millis(500);
	wait.Update();
	CHECK(calls == "in;out;");

	calls.clear();
	g_FrameNow = t0 + MonotonicClock::Millis(501);
	wait.Update();
	CHECK(calls == mainMenu + "disconnected;in;out;");

	// Giving up does not move the limit: still past it, it gives up again.
	calls.clear();
	wait.Update();
	CHECK(calls == mainMenu + "disconnected;in;out;");

	// A new delay counts from the new stamp.
	calls.clear();
	wait.SetDelay(1000);
	g_FrameNow = t0 + MonotonicClock::Millis(1000);
	wait.Update();
	CHECK(calls == "stamp;in;out;");
}

TEST(WaitPacketUpdate, AFailedReadEndsTheFrame)
{
	Fixture fixture;
	CWaitPacketUpdate wait(&host);
	socketInputOk = false;
	g_ModeNext = MODE_GAME;
	wait.Update();
	CHECK(calls == "in;");
	CHECK(g_ModeNext == MODE_GAME);
}

TEST(WaitPacketUpdate, AQueuedModeIsEnteredInsteadOfWriting)
{
	Fixture fixture;
	CWaitPacketUpdate wait(&host);
	g_ModeNext = MODE_GAME;
	wait.Update();
	CHECK(calls == "in;mode" + std::to_string(static_cast<int>(MODE_GAME)) + ";");
	CHECK(g_ModeNext == MODE_NULL);

	calls.clear();
	wait.Update();
	CHECK(calls == "in;out;");

	// Whichever mode is queued.
	calls.clear();
	g_ModeNext = MODE_WAIT_PCLIST;
	wait.Update();
	CHECK(calls == "in;mode" + std::to_string(static_cast<int>(MODE_WAIT_PCLIST)) + ";");
	CHECK(g_ModeNext == MODE_NULL);
}
