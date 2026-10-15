#include "test_framework.h"
#include "MMusic.h"

// The MIDI player's state (task 4.214). Nothing plays: Init and Play
// report failure, but the state moves as the game expects. Before Init,
// every call but SetVolume refuses and changes nothing.
namespace {

bool State(const MMusic& music, bool load, bool play, bool pause)
{
	return music.m_bLoad == load && music.m_bPlay == play && music.m_bPause == pause;
}

} // namespace

TEST(Music, ANewPlayerIsIdle)
{
	MMusic music;
	CHECK(!music.IsInit());
	CHECK(State(music, false, false, false));
	CHECK_EQ(0, music.GetVolume());
}

TEST(Music, BeforeInitEveryCallRefuses)
{
	MMusic music;
	CHECK(!music.Play("music.mid"));
	CHECK(!music.Stop());
	CHECK(!music.Pause());
	CHECK(!music.Resume());
	CHECK(!music.RePlay());
	music.UnInit();
	CHECK(State(music, false, false, false));
	music.SetVolume(0x1234);
	CHECK_EQ(0x1234, music.GetVolume());
}

TEST(Music, InitFailsButInitialises)
{
	MMusic music;
	music.m_bPause = true;
	CHECK(!music.Init(nullptr));
	CHECK(music.IsInit());
	CHECK(!music.m_bPause);
}

TEST(Music, PlayPauseResumeAndStop)
{
	MMusic music;
	music.Init(nullptr);
	CHECK(!music.Play("music.mid"));		// nothing plays, yet:
	CHECK(State(music, true, true, false));
	CHECK(music.IsPlay());
	CHECK(!music.IsPause());

	CHECK(music.Pause());
	CHECK(State(music, true, false, true));
	CHECK(!music.IsPlay());
	CHECK(music.IsPause());
	CHECK(music.Pause());					// not playing: nothing to do
	CHECK(State(music, true, false, true));

	CHECK(music.Resume());
	CHECK(State(music, true, true, false));
	CHECK(!music.Resume());					// already playing

	CHECK(music.Stop());
	CHECK(State(music, false, false, false));
	CHECK(music.Stop());					// not playing: nothing to do
	CHECK(!music.Resume());					// nothing loaded
	CHECK(music.Pause());					// nothing loaded: nothing to do
	CHECK(State(music, false, false, false));
}

TEST(Music, ReplayNeedsAFileAndPlayRestarts)
{
	MMusic music;
	music.Init(nullptr);
	CHECK(!music.RePlay());
	music.Play("one.mid");
	music.Pause();
	CHECK(music.RePlay());
	CHECK(State(music, true, true, false));
	// A paused file is not stopped by Stop (it is not playing); Play over
	// it starts afresh.
	music.Pause();
	CHECK(music.Stop());
	CHECK(State(music, true, false, true));
	music.Play("two.mid");
	CHECK(State(music, true, true, false));
}

// Paused means a loaded file is paused.
TEST(Music, PausedNeedsALoadedFile)
{
	MMusic music;
	music.m_bPause = true;
	CHECK(!music.IsPause());
	music.m_bLoad = true;
	CHECK(music.IsPause());
}

TEST(Music, UnInitStopsALoadedFile)
{
	MMusic music;
	music.Init(nullptr);
	music.Play("music.mid");
	music.UnInit();
	CHECK(State(music, false, false, false));
	CHECK(music.IsInit());
}
