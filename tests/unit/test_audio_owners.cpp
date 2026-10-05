#include "test_framework.h"
#include "CSoundPartManager.h"
#include "COGGSTREAM.h"
#include "CDirectSound.h"
#include "DXLibBackend.h"
#include "CrtCompat.h"

#include <SDL.h>
#ifdef DARKEDEN_TEST_HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef DARKEDEN_TEST_HAVE_SDL_MIXER
namespace {
struct AudioDirectory
{
	std::filesystem::path path;
	AudioDirectory()
	{
		static unsigned serial = 0;
		for (unsigned attempt = 0; attempt < 100; ++attempt)
		{
			auto candidate = std::filesystem::temp_directory_path() / ("darkeden_audio_owner_"
				+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
				+ "_" + std::to_string(++serial));
			if (std::filesystem::create_directory(candidate))
			{
				path = std::move(candidate);
				return;
			}
		}
		throw std::runtime_error("Cannot reserve audio fixture directory");
	}
	~AudioDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
};

struct AudioWorld
{
	AudioDirectory directory;
	std::string previousDriver = SDL_getenv("SDL_AUDIODRIVER") ? SDL_getenv("SDL_AUDIODRIVER") : "";
	std::filesystem::path path = directory.path / "fixture.wav";
	bool ready;
	AudioWorld()
	{
		g_SDLAudio.Release();
		dxlib_music_release();
		SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
		ready = g_SDLAudio.Init(nullptr);
		CHECK(ready);
		g_SDLAudio.UnSetMute();
		g_SDLAudio.SetVolumeLimit(DSBVOLUME_MAX);
		// One second of silent mono PCM keeps playback assertions independent
		// of scheduling latency. SDL_mixer decodes these actual fixture bytes.
		std::vector<unsigned char> wav{
			'R','I','F','F', 0x64,0x1f,0,0, 'W','A','V','E',
			'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
			0x40,0x1f,0,0, 0x40,0x1f,0,0, 1,0, 8,0,
			'd','a','t','a', 0x40,0x1f,0,0};
		wav.resize(44 + 8000, 128);
		std::ofstream file(path, std::ios::binary);
		file.write(reinterpret_cast<const char*>(wav.data()), static_cast<std::streamsize>(wav.size()));
		CHECK(file.good());
	}
	~AudioWorld()
	{
		dxlib_music_release();
		g_SDLAudio.Release();
		SDL_setenv("SDL_AUDIODRIVER", previousDriver.c_str(), 1);
	}
	LPDIRECTSOUNDBUFFER LoadSound()
	{
		auto name = path.string();
		auto buffer = g_SDLAudio.LoadWav(name.data());
		CHECK(buffer != nullptr);
		return buffer;
	}
	bool LoadMusic(COGGSTREAM& stream)
	{
		FILE* file = Basic::OpenFile(path.string().c_str(), "rb");
		CHECK(file != nullptr);
		if (!file) return false;
		const bool loaded = stream.streamLoad(file, nullptr);
		std::fclose(file); // Production callers may close it before playback.
		CHECK(loaded);
		return loaded;
	}
};

struct ObservedSoundCache : CSoundPartManager
{
	unsigned released = 0;
	void OnReleaseData(LPDIRECTSOUNDBUFFER& buffer) override
	{
		if (buffer) ++released;
		CSoundPartManager::OnReleaseData(buffer);
		CHECK(buffer == nullptr);
	}
};
}

TEST(SoundCache, StopLeavesCachedBuffersAvailableForReplay)
{
	AudioWorld world;
	if (!world.ready) return;
	CSoundPartManager cache;
	cache.Init(4, 2);
	auto first = world.LoadSound();
	auto second = world.LoadSound();
	cache.SetData(0, first);
	cache.SetData(1, second);
	if (!first || !second) return;
	CHECK(g_SDLAudio.Play(first, true, false));
	CHECK(g_SDLAudio.Play(second, true, false));
	CHECK_EQ(2, Mix_Playing(-1));
	cache.Stop();
	CHECK(!g_SDLAudio.IsPlay(first));
	CHECK(!g_SDLAudio.IsPlay(second));
	CHECK_EQ(0, Mix_Playing(-1));
	LPDIRECTSOUNDBUFFER found = nullptr;
	CHECK(cache.GetData(0, found));
	CHECK(found == first);
	CHECK(g_SDLAudio.Play(found, true, false));
	CHECK(g_SDLAudio.IsPlay(found));
}

TEST(SoundCache, ReinitializationAndReleaseCallTheProductionCleanupForEveryBuffer)
{
	AudioWorld world;
	if (!world.ready) return;
	ObservedSoundCache cache;
	cache.Init(4, 2);
	cache.SetData(0, world.LoadSound());
	cache.SetData(1, world.LoadSound());
	cache.Init(6, 3);
	CHECK_EQ(2, cache.released);
	CHECK_EQ(0, cache.GetUsed());
	CHECK_EQ(6, cache.GetMaxIndex());
	CHECK_EQ(3, cache.GetMaxPartIndex());
	cache.SetData(2, world.LoadSound());
	cache.Release();
	CHECK_EQ(3, cache.released);
	CHECK_EQ(0, cache.GetUsed());
	cache.Release();
	cache.Stop();
	CHECK_EQ(3, cache.released);
}

TEST(SoundCache, DestructionStopsTheRealPlayingBuffers)
{
	AudioWorld world;
	if (!world.ready) return;
	{
		CSoundPartManager cache;
		cache.Init(2, 1);
		auto buffer = world.LoadSound();
		cache.SetData(0, buffer);
		if (!buffer) return;
		CHECK(g_SDLAudio.Play(buffer, true, false));
		CHECK_EQ(1, Mix_Playing(-1));
	}
	CHECK_EQ(0, Mix_Playing(-1));
}
#endif // DARKEDEN_TEST_HAVE_SDL_MIXER

TEST(OggStream, DefaultsAndMissingInputsDoNotStartPlayback)
{
	COGGSTREAM stream(nullptr, nullptr, 0, 0, 0, 0);
	CHECK(!stream.streamPlay(SOUND_PLAY_ONCE));
	CHECK(!stream.streamUpdate(nullptr));
	CHECK(!stream.streamLoad(nullptr, nullptr));
	stream.streamClose();
	stream.streamClose();
	CHECK_EQ(DSBVOLUME_MIN, stream.streamVolume(DSBVOLUME_MIN - 1));
	CHECK_EQ(DSBVOLUME_MAX, stream.streamVolume(DSBVOLUME_MAX + 1));
	CHECK_EQ(-1000, stream.streamVolume(-1000));
}

#ifdef DARKEDEN_TEST_HAVE_SDL_MIXER
TEST(OggStream, OwnsDecodedInputAfterTheCallerClosesItsFile)
{
	AudioWorld world;
	if (!world.ready) return;
	COGGSTREAM stream(nullptr, nullptr, 0, 0, 0, 0);
	CHECK_EQ(-1000, stream.streamVolume(-1000));
	if (!world.LoadMusic(stream)) return;
	CHECK(stream.streamPlay(SOUND_PLAY_REPEAT));
	CHECK(stream.streamUpdate(nullptr));
	CHECK(dxlib_music_is_playing());
	CHECK_EQ(AudioVolumeToPercent(-1000) * MIX_MAX_VOLUME / 100, Mix_VolumeMusic(-1));
	CHECK_EQ(DSBVOLUME_MIN, stream.streamVolume(DSBVOLUME_MIN - 1));
	CHECK_EQ(0, Mix_VolumeMusic(-1));
	stream.streamClose();
	CHECK(!dxlib_music_is_playing());
	CHECK(!stream.streamUpdate(nullptr));
	CHECK(!stream.streamPlay(SOUND_PLAY_REPEAT));
}

TEST(OggStream, ReloadStopsThePreviousTrackAndDestructionStopsTheReplacement)
{
	AudioWorld world;
	if (!world.ready) return;
	{
		COGGSTREAM stream(nullptr, nullptr, 0, 0, 0, 0);
		if (!world.LoadMusic(stream)) return;
		CHECK(stream.streamPlay(SOUND_PLAY_REPEAT));
		if (!world.LoadMusic(stream)) return;
		CHECK(!stream.streamUpdate(nullptr));
		CHECK(!dxlib_music_is_playing());
		CHECK(stream.streamPlay(SOUND_PLAY_ONCE));
		CHECK(stream.streamUpdate(nullptr));
	}
	CHECK(!dxlib_music_is_playing());
}

TEST(OggStream, FailedReloadDiscardsThePreviouslyPlayingTrack)
{
	AudioWorld world;
	if (!world.ready) return;
	COGGSTREAM stream(nullptr, nullptr, 0, 0, 0, 0);
	if (!world.LoadMusic(stream)) return;
	CHECK(stream.streamPlay(SOUND_PLAY_REPEAT));
	CHECK(!stream.streamLoad(nullptr, nullptr));
	CHECK(!dxlib_music_is_playing());
	CHECK(!stream.streamUpdate(nullptr));
	CHECK(!stream.streamPlay(SOUND_PLAY_ONCE));
}
#endif // DARKEDEN_TEST_HAVE_SDL_MIXER
