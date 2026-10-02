#include "test_framework.h"
#include "DelayedSoundQueue.h"
#include "SoundDef.h"

#include <limits>
#include <stdexcept>
#include <vector>

namespace {
auto Time(unsigned long long milliseconds) { return MonotonicClock::FromMillis(milliseconds); }
auto Milliseconds(const SOUND_NODE& sound) { return sound.GetPlayTime().time_since_epoch().count(); }
SOUND_NODE Sound(TYPE_SOUNDID id, DWORD delay = 0)
{
	return SOUND_NODE(id, delay, int(id) + 10, int(id) + 20, Time(1000));
}
}

TEST(DelayedSounds, RecordsUseTheSuppliedFrameTimestampAndPosition)
{
	SOUND_NODE sound(61, 234, -8, 17, Time(5000));
	CHECK_EQ(5234, Milliseconds(sound));
	CHECK_EQ(61, sound.GetSoundID());
	CHECK_EQ(-8, sound.GetX()); CHECK_EQ(17, sound.GetY());
}

TEST(DelayedSounds, ResetReplacesAllRecordFieldsFromTheNewTimestamp)
{
	auto sound = Sound(1, 20);
	sound.Set(62, 5, 91, -40, Time(10000));
	CHECK_EQ(10005, Milliseconds(sound));
	CHECK_EQ(62, sound.GetSoundID());
	CHECK_EQ(91, sound.GetX()); CHECK_EQ(-40, sound.GetY());
}

TEST(DelayedSounds, FullDwordDelaysDoNotWrapAtTheLegacyTickBoundary)
{
	SOUND_NODE sound(SOUNDID_NULL, 0xffffffffu, -1, -2, Time(0xfffffff0u));
	CHECK_EQ(0x1ffffffefuLL, Milliseconds(sound));
	CHECK_EQ(SOUNDID_NULL, sound.GetSoundID());
}

TEST(DelayedSounds, RecordCopiesOwnTheirTimingAndFullRangeCoordinates)
{
	const int low = (std::numeric_limits<int>::min)();
	const int high = (std::numeric_limits<int>::max)();
	SOUND_NODE original(62, 30, low, high, Time(9000));
	SOUND_NODE copy = original;
	original.Set(63, 0, 1, 2, Time(1));
	CHECK_EQ(9030, Milliseconds(copy));
	CHECK_EQ(62, copy.GetSoundID());
	CHECK_EQ(low, copy.GetX()); CHECK_EQ(high, copy.GetY());
}

TEST(DelayedSounds, ThunderUsesTheCloseTrackThroughOneSecond)
{
	for (DWORD delay : {0u, 999u, 1000u, 1001u, 0xffffffffu})
	{
		const auto sound = MakeThunderSound(delay, -8, 17, Time(5000));
		CHECK_EQ(delay <= 1000 ? SOUND_WORLD_WEATHER_THUNDER_1 : SOUND_WORLD_WEATHER_THUNDER_2,
			sound.GetSoundID());
		CHECK_EQ(5000uLL + delay, Milliseconds(sound));
		CHECK_EQ(-8, sound.GetX()); CHECK_EQ(17, sound.GetY());
	}
}

TEST(DelayedSounds, AnEmptyQueueDoesNotCallPlayback)
{
	DelayedSoundQueue sounds;
	CHECK_EQ(0, sounds.GetSize());
	int played = 0;
	sounds.Update(Time(1000), [&](const SOUND_NODE&) { ++played; });
	CHECK_EQ(0, played);
	sounds.Clear();
	CHECK_EQ(0, sounds.GetSize());
}

TEST(DelayedSounds, ADeadlineMustBeStrictlyEarlierThanTheUpdateFrame)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1, 20));
	std::vector<TYPE_SOUNDID> played;
	auto play = [&](const SOUND_NODE& sound) { played.push_back(sound.GetSoundID()); };
	sounds.Update(Time(1019), play);
	sounds.Update(Time(1020), play);
	CHECK(played.empty()); CHECK_EQ(1, sounds.GetSize());
	sounds.Update(Time(1021), play);
	CHECK(played == std::vector<TYPE_SOUNDID>({1}));
	CHECK_EQ(0, sounds.GetSize());
	sounds.Update(Time(2000), play);
	CHECK_EQ(1, played.size());
}

TEST(DelayedSounds, ZeroDelayStillWaitsUntilTheFollowingFrame)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1));
	int played = 0;
	sounds.Update(Time(1000), [&](const SOUND_NODE&) { ++played; });
	CHECK_EQ(0, played); CHECK_EQ(1, sounds.GetSize());
	sounds.Update(Time(1001), [&](const SOUND_NODE&) { ++played; });
	CHECK_EQ(1, played); CHECK_EQ(0, sounds.GetSize());
}

TEST(DelayedSounds, ReadyRecordsPlayInInsertionOrderRatherThanDeadlineOrder)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1, 50));
	sounds.Add(Sound(2, 0));
	sounds.Add(Sound(3, 25));
	std::vector<TYPE_SOUNDID> played;
	sounds.Update(Time(1100), [&](const SOUND_NODE& sound) { played.push_back(sound.GetSoundID()); });
	CHECK(played == std::vector<TYPE_SOUNDID>({1, 2, 3}));
	CHECK_EQ(0, sounds.GetSize());
}

TEST(DelayedSounds, FutureRecordsDoNotBlockReadyRecordsBehindThem)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1, 100));
	sounds.Add(Sound(2));
	sounds.Add(Sound(3, 200));
	sounds.Add(Sound(4));
	std::vector<TYPE_SOUNDID> played;
	auto play = [&](const SOUND_NODE& sound) { played.push_back(sound.GetSoundID()); };
	sounds.Update(Time(1001), play);
	CHECK(played == std::vector<TYPE_SOUNDID>({2, 4})); CHECK_EQ(2, sounds.GetSize());
	sounds.Update(Time(1300), play);
	CHECK(played == std::vector<TYPE_SOUNDID>({2, 4, 1, 3})); CHECK_EQ(0, sounds.GetSize());
}

TEST(DelayedSounds, DistinctRequestsWithIdenticalValuesBothPlay)
{
	DelayedSoundQueue sounds;
	const auto sound = Sound(1);
	sounds.Add(sound); sounds.Add(sound);
	std::vector<TYPE_SOUNDID> played;
	sounds.Update(Time(1001), [&](const SOUND_NODE& ready) { played.push_back(ready.GetSoundID()); });
	CHECK(played == std::vector<TYPE_SOUNDID>({1, 1}));
}

TEST(DelayedSounds, QueuingACopyPreservesTheScheduledPositionAndTimestamp)
{
	DelayedSoundQueue sounds;
	auto sound = Sound(1, 20);
	sounds.Add(sound);
	sound.Set(2, 0, 99, 100, Time(0));
	int played = 0;
	sounds.Update(Time(1021), [&](const SOUND_NODE& ready) {
		++played;
		CHECK_EQ(1, ready.GetSoundID()); CHECK_EQ(1020, Milliseconds(ready));
		CHECK_EQ(11, ready.GetX()); CHECK_EQ(21, ready.GetY());
	});
	CHECK_EQ(1, played);
}

TEST(DelayedSounds, ClearingDropsReadyAndFutureSoundsAndAllowsReuse)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1)); sounds.Add(Sound(2, 2000));
	sounds.Clear();
	CHECK_EQ(0, sounds.GetSize());
	sounds.Add(Sound(3));
	std::vector<TYPE_SOUNDID> played;
	sounds.Update(Time(4000), [&](const SOUND_NODE& sound) { played.push_back(sound.GetSoundID()); });
	CHECK(played == std::vector<TYPE_SOUNDID>({3}));
}

TEST(DelayedSounds, MissingPlaybackStillConsumesReadySounds)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1)); sounds.Add(Sound(2, 100));
	sounds.Update(Time(1001), {});
	CHECK_EQ(1, sounds.GetSize());
	std::vector<TYPE_SOUNDID> played;
	sounds.Update(Time(1101), [&](const SOUND_NODE& sound) { played.push_back(sound.GetSoundID()); });
	CHECK(played == std::vector<TYPE_SOUNDID>({2}));
}

TEST(DelayedSounds, ReadySoundsAppendedByPlaybackRunInTheSameUpdate)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1)); sounds.Add(Sound(2));
	std::vector<TYPE_SOUNDID> played;
	sounds.Update(Time(1100), [&](const SOUND_NODE& sound) {
		played.push_back(sound.GetSoundID());
		if (sound.GetSoundID() == 1) sounds.Add(Sound(3));
		if (sound.GetSoundID() == 3) sounds.Add(Sound(4));
	});
	CHECK(played == std::vector<TYPE_SOUNDID>({1, 2, 3, 4}));
	CHECK_EQ(0, sounds.GetSize());
}

TEST(DelayedSounds, ABackwardsFrameDoesNotPlayOrReschedulePendingSounds)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1, 100));
	int played = 0;
	auto play = [&](const SOUND_NODE&) { ++played; };
	sounds.Update(Time(100), play);
	sounds.Update(Time(1100), play);
	CHECK_EQ(0, played); CHECK_EQ(1, sounds.GetSize());
	sounds.Update(Time(1101), play);
	CHECK_EQ(1, played); CHECK_EQ(0, sounds.GetSize());
}

TEST(DelayedSounds, PlaybackSeesOnlyTheSoundsThatRemainPending)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1)); sounds.Add(Sound(2));
	sounds.Update(Time(1001), [&](const SOUND_NODE& sound) {
		CHECK_EQ(sound.GetSoundID() == 1 ? 1 : 0, sounds.GetSize());
	});
}

TEST(DelayedSounds, ThrowingPlaybackConsumesItsRecordAndKeepsLaterSounds)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1)); sounds.Add(Sound(2)); sounds.Add(Sound(3, 1000));
	bool threw = false;
	try
	{
		sounds.Update(Time(1001), [](const SOUND_NODE&) { throw std::runtime_error("playback failed"); });
	}
	catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(2, sounds.GetSize());
	std::vector<TYPE_SOUNDID> played;
	sounds.Update(Time(1001), [&](const SOUND_NODE& sound) { played.push_back(sound.GetSoundID()); });
	CHECK(played == std::vector<TYPE_SOUNDID>({2}));
	CHECK_EQ(1, sounds.GetSize());
}

TEST(DelayedSounds, PlaybackCanClearTheQueueWithoutInvalidatingItsRecord)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1)); sounds.Add(Sound(2));
	int played = 0;
	sounds.Update(Time(1001), [&](const SOUND_NODE& sound) {
		++played;
		sounds.Clear();
		CHECK_EQ(1, sound.GetSoundID());
		CHECK_EQ(11, sound.GetX()); CHECK_EQ(21, sound.GetY());
	});
	CHECK_EQ(1, played); CHECK_EQ(0, sounds.GetSize());
}

TEST(DelayedSounds, PlaybackCanReplaceTheQueueWithReadyAndFutureSounds)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1)); sounds.Add(Sound(2));
	std::vector<TYPE_SOUNDID> played;
	auto play = [&](const SOUND_NODE& sound) {
		played.push_back(sound.GetSoundID());
		if (sound.GetSoundID() == 1)
		{
			sounds.Clear();
			sounds.Add(Sound(3)); sounds.Add(Sound(4, 100));
			CHECK_EQ(1, sound.GetSoundID());
		}
	};
	sounds.Update(Time(1001), play);
	CHECK(played == std::vector<TYPE_SOUNDID>({1, 3})); CHECK_EQ(1, sounds.GetSize());
	sounds.Update(Time(1101), play);
	CHECK(played == std::vector<TYPE_SOUNDID>({1, 3, 4})); CHECK_EQ(0, sounds.GetSize());
}

TEST(DelayedSounds, RecursiveUpdatesDoNotReplayTheActiveRecord)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1)); sounds.Add(Sound(2)); sounds.Add(Sound(3));
	std::vector<TYPE_SOUNDID> played;
	DelayedSoundQueue::Play play;
	bool recursed = false;
	play = [&](const SOUND_NODE& sound) {
		played.push_back(sound.GetSoundID());
		if (!recursed)
		{
			recursed = true;
			sounds.Update(Time(1001), play);
			CHECK_EQ(1, sound.GetSoundID());
		}
	};
	sounds.Update(Time(1001), play);
	CHECK(played == std::vector<TYPE_SOUNDID>({1, 2, 3}));
	CHECK_EQ(0, sounds.GetSize());
}

TEST(DelayedSounds, ARecursiveLaterFrameCanConsumeTheOuterUpdatesFutureSounds)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1)); sounds.Add(Sound(2, 100)); sounds.Add(Sound(3, 200));
	std::vector<TYPE_SOUNDID> played;
	DelayedSoundQueue::Play play;
	play = [&](const SOUND_NODE& sound) {
		played.push_back(sound.GetSoundID());
		if (sound.GetSoundID() == 1) sounds.Update(Time(1300), play);
	};
	sounds.Update(Time(1001), play);
	CHECK(played == std::vector<TYPE_SOUNDID>({1, 2, 3}));
	CHECK_EQ(0, sounds.GetSize());
}

TEST(DelayedSounds, TheOuterUpdateRecoversAfterACaughtRecursivePlaybackException)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1)); sounds.Add(Sound(2)); sounds.Add(Sound(3));
	std::vector<TYPE_SOUNDID> played;
	DelayedSoundQueue::Play play;
	bool caught = false;
	play = [&](const SOUND_NODE& sound) {
		played.push_back(sound.GetSoundID());
		if (sound.GetSoundID() == 1)
		{
			try { sounds.Update(Time(1001), play); }
			catch (const std::runtime_error&) { caught = true; }
		}
		else if (sound.GetSoundID() == 2) throw std::runtime_error("nested playback failed");
	};
	sounds.Update(Time(1001), play);
	CHECK(caught);
	CHECK(played == std::vector<TYPE_SOUNDID>({1, 2, 3}));
	CHECK_EQ(0, sounds.GetSize());
}

TEST(DelayedSounds, SoundsAppendedBeforePlaybackThrowsRemainPendingInOrder)
{
	DelayedSoundQueue sounds;
	sounds.Add(Sound(1)); sounds.Add(Sound(2));
	bool caught = false;
	try
	{
		sounds.Update(Time(1001), [&](const SOUND_NODE&) {
			sounds.Add(Sound(3));
			throw std::runtime_error("playback failed after appending");
		});
	}
	catch (const std::runtime_error&) { caught = true; }
	CHECK(caught); CHECK_EQ(2, sounds.GetSize());
	std::vector<TYPE_SOUNDID> played;
	sounds.Update(Time(1001), [&](const SOUND_NODE& sound) { played.push_back(sound.GetSoundID()); });
	CHECK(played == std::vector<TYPE_SOUNDID>({2, 3})); CHECK_EQ(0, sounds.GetSize());
}
