#include "test_framework.h"
#include "AmbientSoundState.h"
#include "MZoneTable.h"
#include "SoundDef.h"

#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
auto Time(unsigned long long milliseconds) { return MonotonicClock::FromMillis(milliseconds); }
auto Deadline(const AmbientSoundState& state) { return state.GetNextSoundTime().time_since_epoch().count(); }

struct Draws
{
	std::vector<unsigned> values;
	std::size_t used = 0;
	Draws(std::initializer_list<unsigned> input) : values(input) {}
	unsigned Next()
	{
		if (used == values.size()) throw std::runtime_error("Unexpected ambient random draw");
		return values[used++];
	}
	AmbientSoundState::Random Random() { return [this] { return Next(); }; }
	~Draws() { CHECK_EQ(values.size(), used); }
};

void NoActions(const AmbientSoundUpdate& actions)
{
	CHECK(!actions.stop);
	CHECK(!actions.play);
}

void Playback(const AmbientSoundUpdate& actions, TYPE_SOUNDID id, bool loop, int x, int y)
{
	CHECK(actions.play.has_value());
	if (!actions.play) return;
	CHECK_EQ(id, actions.play->id);
	CHECK_EQ(loop, actions.play->loop);
	CHECK_EQ(x, actions.play->x);
	CHECK_EQ(y, actions.play->y);
}
}

TEST(AmbientSounds, FreshStateWaitsUntilAfterTheEpoch)
{
	AmbientSoundState state;
	CHECK(!state.IsPropellerPlaying());
	CHECK_EQ(0, Deadline(state));
	Draws draws{};
	NoActions(state.Update(Time(0), 61, nullptr, std::nullopt, draws.Random()));
}

TEST(AmbientSounds, EveryHelipadStartsOneLoopAtThePlayerWithoutRandomDraws)
{
	for (int zone : {2106, 2004, 2014, 2024})
	{
		AmbientSoundState state;
		Draws draws{};
		const auto first = state.Update(Time(1000), zone, nullptr, AmbientSoundPosition{-8, 17}, draws.Random());
		CHECK(!first.stop);
		Playback(first, SOUND_WORLD_PROPELLER, true, -8, 17);
		CHECK(state.IsPropellerPlaying());
		NoActions(state.Update(Time(2000), zone, nullptr, AmbientSoundPosition{99, 100}, draws.Random()));
		CHECK_EQ(0, Deadline(state));
	}
}

TEST(AmbientSounds, SwitchingBetweenHelipadsKeepsTheSharedLoop)
{
	AmbientSoundState state;
	Draws draws{};
	state.SetNextSoundTime(Time(500));
	state.Update(Time(1000), 2106, nullptr, AmbientSoundPosition{1, 2}, draws.Random());
	for (int zone : {2004, 2014, 2024, 2106})
		NoActions(state.Update(Time(30000), zone, nullptr, AmbientSoundPosition{3, 4}, draws.Random()));
	CHECK(state.IsPropellerPlaying());
	CHECK_EQ(500, Deadline(state));
}

TEST(AmbientSounds, AHelipadWithoutAPlayerCanStartWhenThePlayerArrives)
{
	AmbientSoundState state;
	Draws draws{};
	NoActions(state.Update(Time(1000), 2106, nullptr, std::nullopt, draws.Random()));
	CHECK(!state.IsPropellerPlaying());
	Playback(state.Update(Time(1001), 2106, nullptr, AmbientSoundPosition{1, 2}, draws.Random()),
		SOUND_WORLD_PROPELLER, true, 1, 2);
	NoActions(state.Update(Time(1002), 2106, nullptr, std::nullopt, draws.Random()));
	CHECK(state.IsPropellerPlaying());
}

TEST(AmbientSounds, LeavingAHelipadStopsOnceEvenWithoutMetadataOrPlayer)
{
	AmbientSoundState state;
	state.SetNextSoundTime(Time(5000));
	Draws draws{};
	state.Update(Time(1000), 2106, nullptr, AmbientSoundPosition{1, 2}, draws.Random());
	const auto actions = state.Update(Time(2000), 61, nullptr, std::nullopt, draws.Random());
	CHECK(actions.stop.has_value());
	if (actions.stop) CHECK_EQ(SOUND_WORLD_PROPELLER, *actions.stop);
	CHECK(!actions.play);
	CHECK(!state.IsPropellerPlaying());
	NoActions(state.Update(Time(3000), 61, nullptr, std::nullopt, draws.Random()));
	CHECK_EQ(5000, Deadline(state));
}

TEST(AmbientSounds, NonHelipadIdsDoNotAliasThroughNarrowing)
{
	for (int zone : {-1, 0, 2003, 2005, 2013, 2015, 2023, 2025, 2105, 2107, 65536 + 2106})
	{
		AmbientSoundState state;
		state.SetNextSoundTime(Time(5000));
		Draws draws{};
		NoActions(state.Update(Time(1000), zone, nullptr, AmbientSoundPosition{1, 2}, draws.Random()));
		CHECK(!state.IsPropellerPlaying());
	}
}

TEST(AmbientSounds, ADeadlineMustBeStrictlyEarlierThanTheCurrentFrame)
{
	AmbientSoundState state;
	state.SetNextSoundTime(Time(1000));
	ZONETABLE_INFO zone;
	zone.SoundIDList = {41, 42, 43};
	Draws draws{4, 1, 14, 0, 11, 9};
	const AmbientSoundPosition player{100, 200};
	NoActions(state.Update(Time(999), 61, &zone, player, draws.Random()));
	NoActions(state.Update(Time(1000), 61, &zone, player, draws.Random()));
	CHECK_EQ(0, draws.used);
	const auto actions = state.Update(Time(1001), 61, &zone, player, draws.Random());
	CHECK(!actions.stop);
	Playback(actions, 42, false, 127, 179);
	CHECK_EQ(16001, Deadline(state));
	NoActions(state.Update(Time(1001), 61, &zone, player, draws.Random()));
	NoActions(state.Update(Time(16001), 61, &zone, player, draws.Random()));
}

TEST(AmbientSounds, SelectionCoordinatesAndDelayConsumeTheSuppliedSequenceInOrder)
{
	AmbientSoundState state;
	ZONETABLE_INFO zone;
	zone.SoundIDList = {41, 42, 43};
	Draws draws{2, 0, 0, 1, 0, 0, 3, 3, 29, 2, 23, 19};
	Playback(state.Update(Time(1000), 61, &zone, AmbientSoundPosition{100, 200}, draws.Random()),
		43, false, 87, 210);
	CHECK_EQ(7000, Deadline(state));
	CHECK_EQ(6, draws.used);
	Playback(state.Update(Time(7001), 61, &zone, AmbientSoundPosition{100, 200}, draws.Random()),
		41, false, 127, 179);
	CHECK_EQ(22001, Deadline(state));
}

TEST(AmbientSounds, EmptySoundListsKeepTheNullRequestAndCoordinateDraws)
{
	AmbientSoundState state;
	ZONETABLE_INFO zone;
	Draws draws{0, 0, 1, 0, 0};
	const auto actions = state.Update(Time(1000), 61, &zone, AmbientSoundPosition{100, 200}, draws.Random());
	CHECK(!actions.stop);
	Playback(actions, SOUNDID_NULL, false, 87, 210);
	CHECK_EQ(7000, Deadline(state));
}

TEST(AmbientSounds, MissingMetadataOrPlayerOnlyDrawsTheNextDelay)
{
	ZONETABLE_INFO zone;
	zone.SoundIDList = {41};
	for (bool metadata : {false, true})
		for (bool hasPlayer : {false, true})
		{
			if (metadata && hasPlayer) continue;
			AmbientSoundState state;
			Draws draws{9};
			const auto player = hasPlayer ? std::optional<AmbientSoundPosition>{{100, 200}} : std::nullopt;
			NoActions(state.Update(Time(1000), 61, metadata ? &zone : nullptr, player, draws.Random()));
			CHECK_EQ(16000, Deadline(state));
		}
}

TEST(AmbientSounds, LeavingAnOverdueHelipadReturnsStopAndRandomPlaybackTogether)
{
	AmbientSoundState state;
	ZONETABLE_INFO zone;
	zone.SoundIDList = {41};
	Draws draws{0, 0, 0, 1, 0, 0};
	state.Update(Time(1000), 2106, &zone, AmbientSoundPosition{100, 200}, draws.Random());
	const auto actions = state.Update(Time(90000), 61, &zone, AmbientSoundPosition{100, 200}, draws.Random());
	CHECK(actions.stop.has_value());
	if (actions.stop) CHECK_EQ(SOUND_WORLD_PROPELLER, *actions.stop);
	Playback(actions, 41, false, 87, 210);
	CHECK(!state.IsPropellerPlaying());
	CHECK_EQ(96000, Deadline(state));
	NoActions(state.Update(Time(90000), 61, &zone, AmbientSoundPosition{100, 200}, draws.Random()));
}

TEST(AmbientSounds, ResettingTheDeadlineDoesNotForgetAPlayingPropeller)
{
	AmbientSoundState state;
	Draws draws{};
	state.Update(Time(1000), 2106, nullptr, AmbientSoundPosition{1, 2}, draws.Random());
	state.SetNextSoundTime(Time(5000));
	CHECK(state.IsPropellerPlaying());
	NoActions(state.Update(Time(2000), 2106, nullptr, AmbientSoundPosition{3, 4}, draws.Random()));
	CHECK_EQ(5000, Deadline(state));
}

TEST(AmbientSounds, StoppingAllAudioAllowsTheLoopToRestartAndKeepsTheDeadline)
{
	AmbientSoundState state;
	Draws draws{};
	state.SetNextSoundTime(Time(5000));
	state.Update(Time(1000), 2106, nullptr, AmbientSoundPosition{1, 2}, draws.Random());
	state.OnSoundsStopped();
	CHECK(!state.IsPropellerPlaying());
	CHECK_EQ(5000, Deadline(state));
	Playback(state.Update(Time(2000), 2106, nullptr, AmbientSoundPosition{3, 4}, draws.Random()),
		SOUND_WORLD_PROPELLER, true, 3, 4);
}

TEST(AmbientSounds, ZoneLoadDelaysSpanTenThroughFourteenSeconds)
{
	AmbientSoundState state;
	Draws draws{};
	state.Update(Time(1000), 2106, nullptr, AmbientSoundPosition{1, 2}, draws.Random());
	for (unsigned sample = 0; sample < 20; ++sample)
	{
		state.DelayAfterZoneLoad(Time(0xfffffff0u), sample);
		CHECK_EQ(0xfffffff0uLL + 10000 + (sample % 5) * 1000, Deadline(state));
		CHECK(state.IsPropellerPlaying());
	}
	state.DelayAfterZoneLoad(Time(1000), (std::numeric_limits<unsigned>::max)());
	CHECK_EQ(11000 + ((std::numeric_limits<unsigned>::max)() % 5) * 1000, Deadline(state));
}

TEST(AmbientSounds, RecurringDelaysSpanSixThroughFifteenSecondsAcrossTheOldTickWrap)
{
	for (unsigned sample = 0; sample < 30; ++sample)
	{
		AmbientSoundState state;
		Draws draws{sample};
		NoActions(state.Update(Time(0xfffffff0u), 61, nullptr, std::nullopt, draws.Random()));
		CHECK_EQ(0xfffffff0uLL + 6000 + (sample % 10) * 1000, Deadline(state));
	}
}

TEST(AmbientSounds, ABackwardsFrameLeavesTheDeadlineUntouched)
{
	AmbientSoundState state;
	state.SetNextSoundTime(Time(10000));
	Draws draws{};
	NoActions(state.Update(Time(0), 61, nullptr, std::nullopt, draws.Random()));
	CHECK_EQ(10000, Deadline(state));
}

TEST(AmbientSounds, ThrowingRandomDrawsDoNotPartiallyPublishStopOrDeadlineState)
{
	ZONETABLE_INFO zone;
	zone.SoundIDList = {41};
	for (unsigned failingDraw = 0; failingDraw < 6; ++failingDraw)
	{
		AmbientSoundState state;
		state.SetNextSoundTime(Time(500));
		Draws none{};
		state.Update(Time(1000), 2106, nullptr, AmbientSoundPosition{1, 2}, none.Random());
		unsigned consumed = 0;
		bool threw = false;
		try
		{
			state.Update(Time(2000), 61, &zone, AmbientSoundPosition{1, 2}, [&]() -> unsigned {
				if (consumed++ == failingDraw) throw std::runtime_error("Random source failed");
				return 0;
			});
		}
		catch (const std::runtime_error&) { threw = true; }
		CHECK(threw);
		CHECK_EQ(failingDraw + 1, consumed);
		CHECK(state.IsPropellerPlaying());
		CHECK_EQ(500, Deadline(state));
		Draws retry{0, 0, 0, 0, 0, 0};
		const auto actions = state.Update(Time(2000), 61, &zone, AmbientSoundPosition{1, 2}, retry.Random());
		CHECK(actions.stop.has_value());
		Playback(actions, 41, false, -12, -8);
		CHECK(!state.IsPropellerPlaying());
		CHECK_EQ(8000, Deadline(state));
	}
}

TEST(AmbientSounds, FullUnsignedDrawsStayWithinTheSoundPositionAndDelayRanges)
{
	const unsigned high = (std::numeric_limits<unsigned>::max)();
	AmbientSoundState state;
	ZONETABLE_INFO zone;
	zone.SoundIDList = {41, 42};
	Draws draws{high, high, high, high, high, high};
	Playback(state.Update(Time(1000), 61, &zone, AmbientSoundPosition{100, 200}, draws.Random()),
		42, false, 113 + static_cast<int>(high % 15), 210 + static_cast<int>(high % 12));
	CHECK_EQ(7000 + (high % 10) * 1000, Deadline(state));
}

TEST(AmbientSounds, DefaultRandomSourceReturnsAValidZoneRequest)
{
	AmbientSoundState state;
	ZONETABLE_INFO zone;
	zone.SoundIDList = {41};
	const auto actions = state.Update(Time(1000), 61, &zone, AmbientSoundPosition{100, 200});
	CHECK(!actions.stop);
	CHECK(actions.play.has_value());
	if (actions.play)
	{
		CHECK_EQ(41, actions.play->id);
		CHECK(!actions.play->loop);
		const int dx = actions.play->x - 100;
		const int dy = actions.play->y - 200;
		CHECK((dx >= 13 && dx <= 27) || (dx >= -27 && dx <= -13));
		CHECK((dy >= 10 && dy <= 21) || (dy >= -21 && dy <= -10));
	}
	CHECK(Deadline(state) >= 7000 && Deadline(state) <= 16000);
}

TEST(AmbientSoundMetadata, EmptyListsReturnTheNullIdWithEitherSelectionEntryPoint)
{
	ZONETABLE_INFO zone;
	CHECK_EQ(SOUNDID_NULL, zone.GetRandomSoundID());
	CHECK_EQ(SOUNDID_NULL, zone.GetRandomSoundID(0));
	CHECK_EQ(SOUNDID_NULL, zone.GetRandomSoundID((std::numeric_limits<unsigned>::max)()));
}

TEST(AmbientSoundMetadata, SuppliedDrawsSelectByOrdinalIncludingDuplicateAndNullIds)
{
	ZONETABLE_INFO zone;
	zone.SoundIDList = {41, 41, SOUNDID_NULL, 42};
	const TYPE_SOUNDID expected[]{41, 41, SOUNDID_NULL, 42};
	for (unsigned draw = 0; draw < 16; ++draw)
		CHECK_EQ(expected[draw % 4], zone.GetRandomSoundID(draw));
	CHECK_EQ(42, zone.GetRandomSoundID((std::numeric_limits<unsigned>::max)()));
	zone.SoundIDList = {43};
	CHECK_EQ(43, zone.GetRandomSoundID());
}
