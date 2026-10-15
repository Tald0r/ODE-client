#include "test_framework.h"
#include "MZoneSoundManager.h"
#include "MSoundTable.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// The zone's sounds (task 4.227). A sound plays from its sector, heard
// from the player's: panned 128 a sector past 3 either side, 4 steps
// quieter a sector times the volume option, and not at all from 40
// sectors. Its buffer is a duplicate of the cached one, loaded on first
// use. While the player stands still, a playing sound keeps its nearer
// source. Every 5 seconds or when the player moves, the manager plays the
// sounds heard in the player's sector whose time has come, stops one
// outside its hours, and stops looping one no longer heard.
namespace {

using Buffer = LPDIRECTSOUNDBUFFER;
Buffer B(int n) { return reinterpret_cast<Buffer>(static_cast<intptr_t>(n)); }
int N(Buffer buffer) { return static_cast<int>(reinterpret_cast<intptr_t>(buffer)); }

struct Audio
{
	bool ready = true;
	bool soundOn = true;
	int volume = 3;
	int nextBuffer = 100;
	bool loadFails = false;
	bool cacheStores = true;
	int evicted = 0;					// what storing pushes out (0: a free place)
	std::map<TYPE_SOUNDID, int> cache;
	std::map<int, int> duplicates;		// duplicate -> source
	std::vector<std::string> loads;
	std::vector<int> released, stopped;
	std::vector<std::string> events;	// stops and releases, in order
	std::vector<std::pair<int, bool>> played;
	std::vector<std::string> pans;
	std::vector<int> volumes;
	std::map<int, bool> playing;
	bool player = true;
	int px = 10, py = 10;
	bool zone = true;
	SECTORSOUND_LIST sector;
	MonotonicClock::TimePoint now{};
	std::optional<BYTE> hour = BYTE(12);
	std::vector<std::pair<int, int>> sectorsAsked;
};
Audio* audio = nullptr;

const ZoneSoundHost host{
	.AudioReady = []() { return audio->ready; },
	.LoadWav = [](const char* filename) -> Buffer {
		audio->loads.push_back(filename);
		return audio->loadFails ? nullptr : B(audio->nextBuffer++);
	},
	.Duplicate = [](Buffer buffer) { audio->duplicates[audio->nextBuffer] = N(buffer); return B(audio->nextBuffer++); },
	.Release = [](Buffer buffer) {
		audio->released.push_back(N(buffer));
		audio->events.push_back("release " + std::to_string(N(buffer)));
	},
	.IsPlaying = [](Buffer buffer) { return audio->playing[N(buffer)]; },
	.Play = [](Buffer buffer, bool loop) { audio->played.push_back({N(buffer), loop}); audio->playing[N(buffer)] = true; },
	.Stop = [](Buffer buffer) {
		audio->stopped.push_back(N(buffer));
		audio->events.push_back("stop " + std::to_string(N(buffer)));
		audio->playing[N(buffer)] = false;
	},
	.CenterToRightPan = [](Buffer, int pan) { audio->pans.push_back("R" + std::to_string(pan)); },
	.CenterToLeftPan = [](Buffer, int pan) { audio->pans.push_back("L" + std::to_string(pan)); },
	.CenterPan = [](Buffer) { audio->pans.push_back("C"); },
	.SubVolumeFromMax = [](Buffer, int volume) { audio->volumes.push_back(volume); },
	.IsCached = [](TYPE_SOUNDID id) { return audio->cache.count(id) != 0; },
	.Cache = [](TYPE_SOUNDID id, Buffer buffer, Buffer& old) {
		if (!audio->cacheStores) return false;
		audio->cache[id] = N(buffer);
		if (audio->evicted == 0) return false;	// a free place
		old = B(audio->evicted);
		return true;
	},
	.GetCached = [](TYPE_SOUNDID id, Buffer& buffer) {
		const auto entry = audio->cache.find(id);
		if (entry == audio->cache.end()) return false;
		buffer = B(entry->second);
		return true;
	},
	.SoundOn = []() { return audio->soundOn; },
	.SoundVolume = []() { return audio->volume; },
	.PlayerPosition = [](int& x, int& y) {
		if (!audio->player) return false;
		x = audio->px;
		y = audio->py;
		return true;
	},
	.SectorSounds = [](int x, int y) -> const SECTORSOUND_LIST* {
		audio->sectorsAsked.push_back({x, y});
		return audio->zone ? &audio->sector : nullptr;
	},
	.Now = []() { return audio->now; },
	.GameHour = []() { return audio->hour; },
};

struct Fixture
{
	Audio state;
	SOUND_TABLE sounds;
	SOUND_TABLE* oldSounds = g_pSoundTable;
	MZoneSoundTable zoneSounds;
	MZoneSoundTable* oldZoneSounds = g_pZoneSoundTable;
	const ZoneSoundHost* oldHost = ZONESOUND_NODE::SetHost(&host);
	Fixture()
	{
		audio = &state;
		sounds.Init(5);
		sounds.GetMutable(1)->Filename = "bird.wav";
		sounds.GetMutable(2)->Filename = "wind.wav";
		// Sound 3 has no file.
		g_pSoundTable = &sounds;
		g_pZoneSoundTable = &zoneSounds;
	}
	~Fixture()
	{
		ZONESOUND_NODE::SetHost(oldHost);
		g_pSoundTable = oldSounds;
		g_pZoneSoundTable = oldZoneSounds;
		audio = nullptr;
	}
	// A zone sound of sound `sound`, heard in the sector from (x, y).
	ZONESOUND_INFO& ZoneSound(unsigned short id, TYPE_SOUNDID sound, bool loop, int x, int y)
	{
		auto* info = new ZONESOUND_INFO;
		info->ID = id;
		info->SoundID = sound;
		info->Loop = loop;
		info->MinDelay = 1000;
		info->MaxDelay = 1000;
		info->StartHour = 0;
		info->EndHour = 23;
		zoneSounds.AddData(id, info);
		state.sector.push_back(SECTORSOUND_INFO(id, static_cast<unsigned char>(x), static_cast<unsigned char>(y)));
		return *info;
	}
	void Advance(int ms) { state.now += MonotonicClock::Millis(ms); }
};

} // namespace

TEST(ZoneSound, WithoutAHostOrTheAudioNothingPlays)
{
	Fixture f;
	{
		const ZoneSoundHost* old = ZONESOUND_NODE::SetHost(nullptr);
		ZONESOUND_NODE node(1);
		node.Play(12, 10);
		node.Stop();
		node.StopLoop();
		ZONESOUND_NODE::SetHost(old);
	}
	f.state.ready = false;
	ZONESOUND_NODE notReady(1);
	notReady.Play(12, 10);
	f.state.ready = true;
	ZONESOUND_NODE unknown(5);			// past the sound table
	unknown.Play(12, 10);
	f.state.soundOn = false;
	ZONESOUND_NODE muted(1);
	muted.Play(12, 10);
	CHECK(f.state.loads.empty());
	CHECK(f.state.played.empty());
	CHECK_EQ(-1, muted.GetX());
	CHECK_EQ(-1, unknown.GetX());
	CHECK_EQ(-1, notReady.GetX());
}

TEST(ZoneSound, ItLoadsCachesAndPlaysADuplicate)
{
	Fixture f;
	f.state.evicted = 55;
	{
		ZONESOUND_NODE node(1);
		node.Play(17, 12, true);
		CHECK((f.state.loads == std::vector<std::string>{"bird.wav"}));
		CHECK_EQ(100, f.state.cache[1]);
		CHECK((f.state.released == std::vector<int>{55}));	// what the cache pushed out
		CHECK_EQ(100, f.state.duplicates[101]);
		CHECK(node.GetBuffer() == B(101));
		CHECK((f.state.played == std::vector<std::pair<int, bool>>{{101, true}}));
		CHECK_EQ(17, node.GetX());
		CHECK_EQ(12, node.GetY());
		CHECK(node.IsLoop());
		CHECK(node.IsContinueLoop());
		// A second node duplicates the cached buffer without loading.
		ZONESOUND_NODE second(1);
		second.Play(10, 10);
		CHECK_EQ(1u, f.state.loads.size());
		CHECK_EQ(100, f.state.duplicates[102]);
		CHECK(!second.IsLoop());
		f.state.released.clear();
		f.state.events.clear();
	}
	// Each node stops, then releases, its own duplicate.
	CHECK((f.state.events == std::vector<std::string>{"stop 102", "release 102", "stop 101", "release 101"}));
}

TEST(ZoneSound, WithoutAFileOrALoadOrACacheEntryThereIsNoBuffer)
{
	Fixture f;
	ZONESOUND_NODE noFile(3);
	noFile.Play(12, 10);
	CHECK(f.state.loads.empty());
	CHECK(noFile.GetBuffer() == nullptr);
	CHECK_EQ(12, noFile.GetX());					// the place is kept

	f.state.loadFails = true;
	ZONESOUND_NODE failed(1);
	failed.Play(12, 10);
	CHECK_EQ(1u, f.state.loads.size());
	CHECK(failed.GetBuffer() == nullptr);
	CHECK(f.state.played.empty());

	// Not stored in the cache: nothing pushed out is released, and the
	// loaded buffer is still duplicated.
	f.state.loadFails = false;
	f.state.cacheStores = false;
	ZONESOUND_NODE uncached(2);
	uncached.Play(12, 10);
	CHECK(f.state.released.empty());
	CHECK(uncached.GetBuffer() != nullptr);
}

TEST(ZoneSound, ItIsPannedAndQuietenedByDistance)
{
	Fixture f;
	const struct { int x, y; const char* pan; int volume; } cases[] = {
		{17, 12, "R512", 7 * 4 * 3},	// 7 right
		{14, 10, "R128", 4 * 4 * 3},
		{13, 10, "C", 3 * 4 * 3},
		{10, 10, "C", 0},
		{7, 10, "C", 3 * 4 * 3},
		{6, 10, "L128", 4 * 4 * 3},
		{2, 4, "L640", 8 * 4 * 3},		// 8 left, 6 up
		{49, 10, "R4608", 39 * 4 * 3},	// still heard at 39
	};
	for (const auto& c : cases)
	{
		f.state.pans.clear();
		f.state.volumes.clear();
		ZONESOUND_NODE node(1);
		node.Play(c.x, c.y);
		CHECK((f.state.pans == std::vector<std::string>{c.pan}));
		CHECK((f.state.volumes == std::vector<int>{c.volume}));
	}
	// From 40 sectors it is not heard, and gets no buffer.
	f.state.pans.clear();
	ZONESOUND_NODE far(1);
	far.Play(50, 10);
	CHECK(far.GetBuffer() == nullptr);
	CHECK(f.state.pans.empty());
	CHECK_EQ(50, far.GetX());
	ZONESOUND_NODE farUp(1);
	farUp.Play(10, 50);
	CHECK(farUp.GetBuffer() == nullptr);
}

TEST(ZoneSound, APlayingSoundKeepsItsNearerSourceWhileThePlayerStands)
{
	Fixture f;
	ZONESOUND_NODE node(1);
	node.Play(15, 10);					// 5 away
	node.Play(20, 10);					// 10 away: ignored
	CHECK_EQ(15, node.GetX());
	CHECK_EQ(1u, f.state.pans.size());
	node.Play(5, 10);					// 5 away too: taken
	CHECK_EQ(5, node.GetX());
	node.Play(12, 10);					// nearer: taken
	CHECK_EQ(12, node.GetX());
	// Once playing, it is not started again.
	CHECK_EQ(1u, f.state.played.size());
	// The player moved: the farther one is taken, also when only along y.
	f.state.px = 11;
	node.Play(30, 10);
	CHECK_EQ(30, node.GetX());
	f.state.py = 11;
	node.Play(40, 11);
	CHECK_EQ(40, node.GetX());
	// The old source's distance counts along y as well.
	ZONESOUND_NODE upward(1);
	upward.Play(11, 21);					// 10 away, along y
	upward.Play(16, 11);					// 5 away: taken
	CHECK_EQ(16, upward.GetX());
}

TEST(ZoneSound, DistanceAlongYCountsAboveAndBelow)
{
	Fixture f;
	f.state.px = 10;
	f.state.py = 30;						// off the diagonal
	ZONESOUND_NODE node(1);
	node.Play(10, 12);						// 18 north
	CHECK((f.state.pans == std::vector<std::string>{"C"}));
	CHECK((f.state.volumes == std::vector<int>{18 * 4 * 3}));
	node.Play(15, 30);						// 5 east: nearer, taken
	CHECK_EQ(15, node.GetX());
	ZONESOUND_NODE south(1);
	south.Play(11, 52);						// 22 south
	CHECK_EQ(22 * 4 * 3, f.state.volumes.back());
	// 45 north is out of hearing.
	f.state.py = 50;
	ZONESOUND_NODE far(1);
	far.Play(10, 5);
	CHECK(far.GetBuffer() == nullptr);
}

TEST(ZoneSound, StoppingAndEndingALoop)
{
	Fixture f;
	ZONESOUND_NODE node(1);
	node.Stop();							// no buffer
	CHECK(f.state.stopped.empty());
	node.Play(12, 10, true);
	node.StopLoop();
	CHECK((f.state.played.back() == std::pair<int, bool>{101, false}));
	CHECK(!node.IsContinueLoop());
	node.SetContinueLoop();
	node.Stop();
	CHECK((f.state.stopped == std::vector<int>{101}));
	CHECK(!node.IsContinueLoop());
	// Not playing: neither calls the device.
	node.SetContinueLoop();
	node.Stop();
	const size_t played = f.state.played.size();
	node.StopLoop();
	CHECK_EQ(1u, f.state.stopped.size());
	CHECK_EQ(played, f.state.played.size());
	CHECK(!node.IsContinueLoop());
}

TEST(ZoneSoundManager, NothingWithoutAPlayerOrAZone)
{
	Fixture f;
	f.ZoneSound(7, 1, true, 12, 10);
	MZoneSoundManager manager;
	f.state.player = false;
	manager.UpdateSound();
	CHECK(f.state.sectorsAsked.empty());
	f.state.player = true;
	f.state.zone = false;
	manager.UpdateSound();
	CHECK(f.state.played.empty());
	f.state.zone = true;
	manager.UpdateSound();					// the same moment: not skipped
	CHECK_EQ(1u, f.state.played.size());
}

TEST(ZoneSoundManager, ItPlaysTheSectorsSoundsEveryFiveSecondsOrOnAMove)
{
	Fixture f;
	ZONESOUND_INFO& bird = f.ZoneSound(7, 1, false, 12, 10);
	MZoneSoundManager manager;
	f.Advance(100);
	manager.UpdateSound();
	CHECK((f.state.sectorsAsked == std::vector<std::pair<int, int>>{{10, 10}}));
	ZONESOUND_NODE* pNode = manager.GetData(7);
	CHECK(pNode != nullptr);
	if (pNode == nullptr) return;
	CHECK_EQ(1, pNode->GetSoundID());
	CHECK_EQ(12, pNode->GetX());
	CHECK(!pNode->IsLoop());
	CHECK(bird.NextPlayTime == f.state.now);	// its next time is set (no delay gap: now)
	// Within 5 seconds, standing still: nothing.
	f.Advance(5000);
	manager.UpdateSound();
	CHECK_EQ(1u, f.state.sectorsAsked.size());
	// Past 5 seconds: again.
	f.Advance(1);
	manager.UpdateSound();
	CHECK_EQ(2u, f.state.sectorsAsked.size());
	CHECK(manager.GetData(7) == pNode);		// the same node
	// A move: at once; then standing there, not again.
	f.state.py = 11;
	manager.UpdateSound();
	CHECK((f.state.sectorsAsked.back() == std::pair<int, int>{10, 11}));
	manager.UpdateSound();
	CHECK_EQ(3u, f.state.sectorsAsked.size());
	f.state.px = 11;
	manager.UpdateSound();
	CHECK_EQ(4u, f.state.sectorsAsked.size());
	manager.UpdateSound();
	CHECK_EQ(4u, f.state.sectorsAsked.size());
}

TEST(ZoneSoundManager, ASoundWaitsForItsTimeAndHours)
{
	Fixture f;
	ZONESOUND_INFO& bird = f.ZoneSound(7, 1, false, 12, 10);
	bird.NextPlayTime = f.state.now + MonotonicClock::Millis(3000);
	f.ZoneSound(15, 1, false, 12, 10);		// no zone sound 15 in the table:
	f.zoneSounds.RemoveData(15);				// skipped
	MZoneSoundManager manager;
	manager.UpdateSound();
	CHECK(manager.GetData(7) == nullptr);	// not yet
	CHECK(manager.GetData(15) == nullptr);
	f.Advance(6000);
	manager.UpdateSound();
	ZONESOUND_NODE* pNode = manager.GetData(7);
	CHECK(pNode != nullptr);
	if (pNode == nullptr) return;
	// Outside its hours it stops.
	bird.StartHour = 20;
	bird.EndHour = 22;
	f.state.px = 11;
	manager.UpdateSound();
	CHECK((f.state.stopped == std::vector<int>{101}));
	// No game hour yet: outside every window.
	bird.StartHour = 0;
	bird.EndHour = 23;
	f.state.hour.reset();
	f.state.px = 12;
	manager.UpdateSound();
	CHECK_EQ(1u, f.state.stopped.size());	// already stopped
	CHECK_EQ(1u, f.state.played.size());
}

TEST(ZoneSoundManager, ALoopNoLongerHeardStopsLooping)
{
	Fixture f;
	f.ZoneSound(7, 1, true, 12, 10);
	f.ZoneSound(8, 2, true, 8, 10);
	MZoneSoundManager manager;
	manager.UpdateSound();
	CHECK_EQ(2u, f.state.played.size());
	// Moving to a sector where only the first is heard.
	f.state.sector.pop_back();
	f.state.px = 11;
	manager.UpdateSound();
	ZONESOUND_NODE* pWind = manager.GetData(8);
	CHECK(pWind != nullptr);
	if (pWind == nullptr) return;
	CHECK_EQ(2, pWind->GetSoundID());		// its zone sound's sound
	CHECK(!pWind->IsContinueLoop());
	CHECK((f.state.played.back() == std::pair<int, bool>{N(pWind->GetBuffer()), false}));
	CHECK(manager.GetData(7)->IsContinueLoop());
}

TEST(ZoneSoundManager, ALoopWithinItsHoursKeepsLoopingBetweenShows)
{
	Fixture f;
	ZONESOUND_INFO& wind = f.ZoneSound(8, 2, true, 8, 10);
	MZoneSoundManager manager;
	manager.UpdateSound();
	// Its table entry stops looping, but the node still loops: within
	// its hours and not yet due, it is told to continue.
	wind.Loop = false;
	wind.NextPlayTime = f.state.now + MonotonicClock::Millis(60000);
	f.state.px = 11;
	const size_t played = f.state.played.size();
	manager.UpdateSound();
	CHECK(manager.GetData(8)->IsContinueLoop());
	CHECK_EQ(played, f.state.played.size());
	// A sound that does not loop is not told to continue.
	ZONESOUND_INFO& bird = f.ZoneSound(7, 1, false, 12, 10);
	f.state.px = 12;
	manager.UpdateSound();
	bird.NextPlayTime = f.state.now + MonotonicClock::Millis(60000);
	f.state.px = 13;
	manager.UpdateSound();
	CHECK(manager.GetData(7) != nullptr && !manager.GetData(7)->IsContinueLoop());
}
