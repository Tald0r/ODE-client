#include "test_framework.h"
#include "type_table_access.h"
#include "GameMain.h"
#include "AmbientSoundState.h"
#include "ClientConfig.h"
#include "ClientMode.h"
#include "CDirectInput.h"
#include "CDirectSound.h"
#include "COGGSTREAM.h"
#include "CServerInformation.h"
#include "CSoundPartManager.h"
#include "CrtCompat.h"
#include "DXLibBackend.h"
#include "DXLib/CDirectDraw.h"
#include "FrameClock.h"
#include "GameInitInfo.h"
#include "GuildWarInfo.h"
#include "InputState.h"
#include "EffectSpriteTypeDef.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "MCreatureTable.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "MGameTime.h"
#include "MMusic.h"
#include "MMusicTable.h"
#include "MPlayer.h"
#include "MSoundTable.h"
#include "MTopView.h"
#include "MWarManager.h"
#include "MWeather.h"
#include "MZone.h"
#include "MZoneTable.h"
#include "MonsterNameTable.h"
#include "MonotonicClock.h"
#include "MusicDef.h"
#include "PCConfigTable.h"
#include "Properties.h"
#include "SoundDef.h"
#include "SoundNode.h"
#include "Types/ZoneTypes.h"
#include "UserInformation.h"
#include "UserOption.h"
#include "ZoneFlags.h"
#include <SDL.h>
#ifdef DARKEDEN_TEST_HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

// The game's main functions (task 4.262 moved Client/GameMain.cpp into
// gameclient), group 1: the sounds, the zone's music, the weather and the
// lightning, the activation (Alt+Tab), the screenshot and the character
// window's last slot. None of them asks GameMainHost for anything.
// - PlaySound(id, repeat, x, y) plays the sound table's file for id at the
//   cell, except while an event silences the game, without audio, for an
//   id past the table, with the sound option off or, for a one-shot, with
//   g_SoundPerSecond over MAX_SOUND_PER_SECOND; and only within 24 cells
//   of the player on both axes. A sound the cache does not hold is loaded
//   and cached (the buffer it evicts is freed); a file name is cut at 255
//   characters. Then it is panned ((dx-3)<<7 right of the centre past 3
//   cells right, (|dx+3|)<<7 left past 3 cells left, else the centre),
//   turned down from the volume limit by (distance<<2) * VolumeSound (the
//   motorcycle starting or stopping five cells further), played (a copy beside
//   one still sounding at a good frame rate, g_bGoodFPS) and counted.
// - PlaySoundAtPlayer(id) is the same at the centre and the full volume,
//   once, with no distance and no limit (counted all the same);
//   PlaySoundForce(id) also ignores the silencing event and the option.
// - StopSound(id) stops the cached buffer; nothing without audio, for
//   SOUNDID_NULL or an id the cache does not hold.
// - PlayMusicCurrentZone picks the track for the player's zone (the large
//   one while the player is in it) by the hour, the holy land and a war
//   there (SelectZoneMusic) and, in the game with a zone and the music on,
//   plays it: the wave file through g_pOGG at (VolumeMusic-15)*250, after
//   closing the last g_oggfile, or the MIDI file through g_Music.
// - SetWeather(weather, level) stops the last weather's sound, releases a
//   spotlight weather, and starts level*8 particles: rain with its sound
//   (the heavy one from level 15) looping at the player, snow, spots, or
//   clear skies. The first call's weather is its own previous one.
// - SetLightning(delay) flashes a ready view white (1 to 31 in steps of
//   10) and queues the thunder at the player: the near one up to a second
//   away, the far one after.
// - CheckActivate(active) flushes the input and, for an active
//   application, restores the game (the view, the music: left playing, the
//   zone's in the game, else the theme; the sound unmuted; the buttons and
//   the player's lock released) or stops it (the music off, the sound
//   muted, the player's repeat dropped); in MODE_WAIT_UPDATEINFO the mute
//   is left as it is.
// - MakeScreenShot makes ./ScreenShot and takes the first number from
//   g_ScreenShotNumber whose PATH_SCREENSHOT file is free, saving nothing
//   (recorded defect).
// - SaveLastSelectedCharacter(slot) records the slot for the world and the
//   account (one of 15 characters or more as "DarkEden") and writes the
//   table to FILE_INFO_PLAYER_INFO; SelectLastSelectedCharacter loads that
//   file into an empty table and selects the account's slot.
//
// The audio is the real SDL_mixer on SDL's dummy driver (SDL_AUDIODRIVER
// =dummy initialises headless, as test_audio_owners.cpp does), so the whole
// of each sound function runs; the files are silent WAVs the tests write.
// The pan, the volume and the channel are read from the backend's own
// buffer record. The music is MMusic's state (no MIDI plays) and, for the
// wave music, the OGG stream SDL_mixer plays.
//
// Not observable here: CheckActivate's display calls (SetDisplayMode,
// RestoreDisplayMode, the gamma ramp) and SetWeather's ClearShadowManager
// are no-ops on the SDL backend; gC_vs_ui's RestoreWhenActivateGame and
// SelectCharacter reach no window while the UI is not started (the title
// UI needs the skins and sprite packs), so SelectLastSelectedCharacter's
// lookup after the load, its 12-character guard among it, shows nothing.
//
// Not run (undefined behaviour):
// - SelectLastSelectedCharacter with an empty UserID once its world is in
//   the table: GetString() is NULL and GameMain.cpp:3306 calls strlen on it.
// - PlayMusicCurrentZone with the wave music on, the audio up and a track
//   whose FilenameWav is empty (or past the music table): 2744 builds a
//   std::string_view from NULL.
// - Unchecked reads of a missing object: PlayMusicCurrentZone's zone table
//   (2714, before the mode check), the PlaySound family's event manager,
//   sound table, cache and options (and PlaySound's player), SetWeather's
//   player in the rain and its weather, SetLightning's zone and player,
//   MakeScreenShot's config and file table.
namespace {

//----------------------------------------------------------------------
// The host: group 1 asks it nothing
//----------------------------------------------------------------------
std::string hostCalls;

const GameMainHost recordingHost{
	.InitDraw = []() -> BOOL { hostCalls += "InitDraw;"; return TRUE; },
	.InitSurface = []() -> BOOL { hostCalls += "InitSurface;"; return TRUE; },
	.InitSound = []() -> BOOL { hostCalls += "InitSound;"; return TRUE; },
	.ReleaseSocket = []() { hostCalls += "ReleaseSocket;"; },
};

// Every global a test here changes, given back after; GameMain's
// remembered state reset before and after.
struct Saved
{
	const GameMainHost* host = SetGameMainHost(&recordingHost);
	MEventQueue* events = g_pEventManager;
	SOUND_TABLE* sounds = g_pSoundTable;
	CSoundPartManager* cache = g_pSoundManager;
	UserOption* option = g_pUserOption;
	ClientConfig* config = g_pClientConfig;
	MPlayer* player = g_pPlayer;
	CREATURE_TABLE* creatureTypes = g_pCreatureTable;
	EFFECTSPRITETYPE_TABLE* effectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* effectStatuses = g_pEffectStatusTable;
	MonsterNameTable* monsterNames = g_pMonsterNameTable;
	int soundPerSecond = g_SoundPerSecond;
	bool goodFPS = g_bGoodFPS;
	int previousSound = g_previousSoundID;
	MUSIC_TABLE* music = g_pMusicTable;
	CZoneTable* zones = g_pZoneTable;
	MGameTime* time = g_pGameTime;
	MWarManager* wars = g_pWarManager;
	CLIENT_MODE mode = g_Mode;
	MZone* zone = g_pZone;
	bool inLarge = g_bZonePlayerInLarge;
	int large = g_nZoneLarge, small = g_nZoneSmall;
	COGGSTREAM* ogg = g_pOGG;
	FILE* oggFile = g_oggfile;
	bool midiInit = g_Music.m_bInit, midiLoad = g_Music.m_bLoad, midiPlay = g_Music.m_bPlay, midiPause = g_Music.m_bPause;
	WORD midiVolume = g_Music.m_Volume;
	MWeather* weather = g_pWeather;
	MTopView* view = g_pTopView;
	MonotonicClock::TimePoint now = g_FrameNow;
	CSDLInput* input = g_pSDLInput;
	BOOL activeApp = g_bActiveApp;
	int activeGame = g_bActiveGame;
	bool fullScreen = g_bFullScreen;
	BOOL myFull = g_MyFull;
	BOOL left = g_bLButtonDown, right = g_bRButtonDown, center = g_bCButtonDown;
	Properties* files = g_pFileDef;
	WorldPlayerConfigTable* lastSlots = g_pWorldPlayerConfigTable;
	UserInformation* user = g_pUserInformation;
	CServerInformation* servers = g_pServerInformation;
	int screenShot = g_ScreenShotNumber;
	bool mute = g_SDLAudio.IsMute();
	LONG volumeLimit = g_SDLAudio.GetVolumeLimit();

	Saved()
	{
		ResetGameMainState();
		hostCalls.clear();
	}
	~Saved()
	{
		CHECK(hostCalls.empty());
		ResetGameMainState();
		g_SDLAudio.SetVolumeLimit(volumeLimit);
		if (mute) g_SDLAudio.SetMute(); else g_SDLAudio.UnSetMute();
		g_ScreenShotNumber = screenShot;
		g_pServerInformation = servers;
		g_pUserInformation = user;
		g_pWorldPlayerConfigTable = lastSlots;
		g_pFileDef = files;
		g_bCButtonDown = center;
		g_bRButtonDown = right;
		g_bLButtonDown = left;
		g_MyFull = myFull;
		g_bFullScreen = fullScreen;
		g_bActiveGame = activeGame;
		g_bActiveApp = activeApp;
		g_pSDLInput = input;
		g_FrameNow = now;
		g_pTopView = view;
		g_pWeather = weather;
		g_Music.m_Volume = midiVolume;
		g_Music.m_bPause = midiPause;
		g_Music.m_bPlay = midiPlay;
		g_Music.m_bLoad = midiLoad;
		g_Music.m_bInit = midiInit;
		if (g_oggfile != oggFile && g_oggfile != NULL) std::fclose(g_oggfile);
		g_oggfile = oggFile;
		g_pOGG = ogg;
		g_nZoneSmall = small;
		g_nZoneLarge = large;
		g_bZonePlayerInLarge = inLarge;
		g_pZone = zone;
		g_Mode = mode;
		g_pWarManager = wars;
		g_pGameTime = time;
		g_pZoneTable = zones;
		g_pMusicTable = music;
		g_previousSoundID = previousSound;
		g_bGoodFPS = goodFPS;
		g_SoundPerSecond = soundPerSecond;
		g_pPlayer = player;
		g_pMonsterNameTable = monsterNames;
		g_pEffectStatusTable = effectStatuses;
		g_pEffectSpriteTypeTable = effectSprites;
		g_pCreatureTable = creatureTypes;
		g_pClientConfig = config;
		g_pUserOption = option;
		g_pSoundManager = cache;
		g_pSoundTable = sounds;
		g_pEventManager = events;
		SetGameMainHost(host);
	}
};

// A directory of the test's own, removed with what is in it.
struct TempDir
{
	std::filesystem::path path;
	TempDir()
	{
		static unsigned serial = 0;
		for (unsigned attempt = 0; attempt < 100; ++attempt)
		{
			auto candidate = std::filesystem::temp_directory_path() / ("darkeden_game_main_1_"
				+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
				+ "_" + std::to_string(++serial));
			if (std::filesystem::create_directory(candidate))
			{
				path = candidate;
				return;
			}
		}
		throw std::runtime_error("Cannot reserve a fixture directory");
	}
	~TempDir() { std::error_code error; std::filesystem::remove_all(path, error); }
	std::string File(const std::string& name) const { return (path / name).string(); }
};

std::vector<unsigned char> ReadBytes(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// Four seconds of silent 8-bit mono PCM at 8 kHz: a one-shot is still
// sounding when the test looks.
void WriteWav(const std::string& path)
{
	const std::uint32_t samples = 4 * 8000;
	std::vector<unsigned char> wav{
		'R','I','F','F', 0,0,0,0, 'W','A','V','E',
		'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
		0x40,0x1f,0,0, 0x40,0x1f,0,0, 1,0, 8,0,
		'd','a','t','a', 0,0,0,0};
	auto put = [&wav](std::size_t at, std::uint32_t value) {
		for (int i = 0; i < 4; ++i) wav[at + i] = static_cast<unsigned char>(value >> (8 * i));
	};
	put(4, 36 + samples);
	put(40, samples);
	wav.resize(44 + samples, 128);
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	file.write(reinterpret_cast<const char*>(wav.data()), static_cast<std::streamsize>(wav.size()));
	CHECK(file.good());
}

// The audio up on SDL's dummy driver, as test_audio_owners.cpp brings it
// up; or, asked for none, down.
struct Audio
{
	std::string previousDriver = SDL_getenv("SDL_AUDIODRIVER") ? SDL_getenv("SDL_AUDIODRIVER") : "";
	bool ready = false;
	explicit Audio(bool up)
	{
		g_SDLAudio.Release();
		dxlib_music_release();
		if (!up) return;
		SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
		ready = g_SDLAudio.Init(nullptr);
		CHECK(ready);
		g_SDLAudio.UnSetMute();
		g_SDLAudio.SetVolumeLimit(DSBVOLUME_MAX);
	}
	~Audio()
	{
		dxlib_music_release();
		g_SDLAudio.Release();
		SDL_setenv("SDL_AUDIODRIVER", previousDriver.c_str(), 1);
	}
};

// The client configuration, installed as it is made: a creature's
// constructor and destructor read it.
struct Config : ClientConfig
{
	Config() { g_pClientConfig = this; }
};

// The rest a creature's constructor and destructor read, installed as it is
// made: the creature types (none), the effect tables, the monster names and
// the user's information.
struct CreatureTables
{
	CREATURE_TABLE creatureTypes;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	MonsterNameTable monsterNames;
	UserInformation user;
	CreatureTables()
	{
		g_pCreatureTable = &creatureTypes;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pMonsterNameTable = &monsterNames;
		g_pUserInformation = &user;
	}
};

} // namespace

#ifdef DARKEDEN_TEST_HAVE_SDL_MIXER
// DXLibBackendSDL.cpp's sound buffer, token for token: being the same
// definition it is the same type in the program, so the tests read the
// pan, the volume (both in percent) and the mixer channel the backend
// keeps. An LPDIRECTSOUNDBUFFER points at CSDLAudio's standard-layout
// wrapper, whose first member is this record's handle.
struct dxlib_chunk_ref;
struct dxlib_sound_buffer {
	struct dxlib_chunk_ref* ref;
	int channel;
	int volume;
	int pan;
};

namespace {

//----------------------------------------------------------------------
// The sounds
//----------------------------------------------------------------------
// The sound cache, open to the tests: the buffer it holds for an id,
// read without touching its LRU order.
struct Cache : CSoundPartManager
{
	LPDIRECTSOUNDBUFFER Held(WORD id) const
	{
		if (id >= m_nIndex || m_pPartIndex[id] == m_PartIndexNULL) return nullptr;
		return m_pData[m_pPartIndex[id]];
	}
};

const int kTableSize = SOUND_WORLD_WEATHER_THUNDER_2 + 1;
const int kVolumeSound = 25;

// The player at (100, 100), every id in the table naming its own file
// (none written yet), a cache of 32 parts for the whole table, the sound
// on at volume 25, a limit of 6 a second with none played yet, a good
// frame rate, no event; the audio up unless asked otherwise.
struct Sounds
{
	Saved saved;
	Audio audio;
	TempDir dir;
	MEventQueue events;
	SOUND_TABLE table;
	UserOption option;
	Config config;
	CreatureTables creatures;
	MPlayer player;
	Cache cache;	// last: it frees its buffers while the audio is up

	explicit Sounds(bool audible = true) : audio(audible)
	{
		g_pEventManager = &events;
		table.Init(kTableSize);
		for (int id = 0; id < kTableSize; ++id)
			testfw::MutableRow(table, id).Filename = Name(id).c_str();
		g_pSoundTable = &table;
		cache.Init(kTableSize, 32);
		g_pSoundManager = &cache;
		option.PlaySound = TRUE;
		option.VolumeSound = kVolumeSound;
		g_pUserOption = &option;
		config.MAX_SOUND_PER_SECOND = 6;
		// The cell alone: MCreature::SetPosition also checks the darkness.
		player.MObject::SetPosition(100, 100);
		g_pPlayer = &player;
		g_SoundPerSecond = 0;
		g_bGoodFPS = true;
		g_previousSoundID = SOUNDID_NULL;
	}
	~Sounds() { g_pClientConfig = &config; }
	std::string Name(int id) const { return dir.File("sound" + std::to_string(id) + ".wav"); }
	// The id's file written: it loads.
	void Wav(int id) { WriteWav(Name(id)); }
	// The silencing event.
	void Silence()
	{
		MEvent event;
		event.eventID = EVENTID_NULL;
		event.eventFlag = EVENTFLAG_NOT_PLAY_SOUND;
		events.AddEvent(event);
		CHECK(events.GetEventByFlag(EVENTFLAG_NOT_PLAY_SOUND) != NULL);
	}
};

// The sound at dx, dy cells from the player.
void PlayAt(int id, int dx, int dy, bool repeat = false)
{
	PlaySound(TYPE_SOUNDID(id), repeat, 100 + dx, 100 + dy);
}

// The volume the backend holds for an attenuation below the limit 0.
int Percent(int attenuation) { return AudioVolumeToPercent(-attenuation); }

struct Heard { int channel = -1, volume = -1, pan = 0; bool playing = false; };

Heard Hear(LPDIRECTSOUNDBUFFER buffer)
{
	Heard heard;
	if (buffer == nullptr) return heard;
	const dxlib_sound_t sound = *reinterpret_cast<const dxlib_sound_t*>(buffer);
	heard.channel = sound->channel;
	heard.volume = sound->volume;
	heard.pan = sound->pan;
	heard.playing = g_SDLAudio.IsPlay(buffer);
	return heard;
}

// How many mixer channels sound.
int Sounding() { return Mix_Playing(-1); }

} // namespace

// The probe reads what CSDLAudio set: a layout that drifted from the
// backend's would fail here first.
TEST(GameMain1Audio, TheProbeReadsTheBackendsPanVolumeAndChannel)
{
	Sounds world;
	if (!world.audio.ready) return;
	world.Wav(1);
	std::string name = world.Name(1);
	LPDIRECTSOUNDBUFFER buffer = g_SDLAudio.LoadWav(name.data());
	CHECK(buffer != nullptr);
	if (buffer == nullptr) return;
	CHECK(g_SDLAudio.ChangePan(buffer, -3000));
	CHECK(g_SDLAudio.SubVolumeFromMax(buffer, 1000));
	CHECK_EQ(-1, Hear(buffer).channel);
	CHECK(g_SDLAudio.Play(buffer, true, false));
	const Heard heard = Hear(buffer);
	CHECK_EQ(-30, heard.pan);
	CHECK_EQ(Percent(1000), heard.volume);
	CHECK(heard.channel >= 0);
	CHECK(heard.playing);
	CHECK_EQ(1, Mix_Playing(heard.channel));
	CHECK_EQ(1, Sounding());
	g_SDLAudio.Release(buffer);
	CHECK_EQ(0, Sounding());
}

//----------------------------------------------------------------------
// PlaySound
//----------------------------------------------------------------------
TEST(GameMain1PlaySound, AFirstPlayLoadsCachesPansFadesPlaysAndCounts)
{
	Sounds world;
	if (!world.audio.ready) return;
	world.Wav(1);
	PlayAt(1, 10, 2);
	const Heard heard = Hear(world.cache.Held(1));
	CHECK(world.cache.Held(1) != nullptr);
	CHECK(heard.playing);
	CHECK_EQ(8, heard.pan);								// (10-3)<<7 = 896
	CHECK_EQ(Percent(10 * 4 * kVolumeSound), heard.volume);
	CHECK_EQ(1, Sounding());
	CHECK_EQ(1, g_SoundPerSecond);
}

// Each row a sound of its own, loaded: the pan by the column, the volume
// by the larger of the two distances.
TEST(GameMain1PlaySound, ThePanFollowsTheColumnAndTheVolumeTheDistance)
{
	Sounds world;
	if (!world.audio.ready) return;
	struct Row { int dx, dy, pan, distance; };
	const Row rows[] = {
		{0, 0, 0, 0}, {3, 0, 0, 3}, {4, 0, 1, 4}, {-3, 1, 0, 3}, {-4, 0, -1, 4},
		{-10, -2, -8, 10}, {2, 10, 0, 10}, {0, -10, 0, 10}, {24, 0, 26, 24}, {-24, 24, -26, 24},
	};
	int id = 1;
	for (const Row& row : rows)
	{
		world.Wav(id);
		g_SoundPerSecond = 0;
		PlayAt(id, row.dx, row.dy);
		const Heard heard = Hear(world.cache.Held(TYPE_SOUNDID(id)));
		CHECK(heard.playing);
		CHECK_EQ(row.pan, heard.pan);
		CHECK_EQ(Percent(row.distance * 4 * kVolumeSound), heard.volume);
		CHECK_EQ(1, g_SoundPerSecond);
		++id;
	}
	CHECK_EQ(10, Sounding());
}

// A cached sound is panned and faded again for where it is now.
TEST(GameMain1PlaySound, ACachedSoundIsPannedAndFadedAgainForWhereItIsNow)
{
	Sounds world;
	if (!world.audio.ready) return;
	world.Wav(1);
	PlayAt(1, 10, 0);
	LPDIRECTSOUNDBUFFER buffer = world.cache.Held(1);
	g_SDLAudio.Stop(buffer);
	PlayAt(1, -6, 12);
	CHECK(world.cache.Held(1) == buffer);
	Heard heard = Hear(buffer);
	CHECK(heard.playing);
	CHECK_EQ(-3, heard.pan);							// -(|-6+3|<<7) = -384
	CHECK_EQ(Percent(12 * 4 * kVolumeSound), heard.volume);
	g_SDLAudio.Stop(buffer);
	PlayAt(1, 3, -1);
	heard = Hear(buffer);
	CHECK_EQ(0, heard.pan);
	CHECK_EQ(Percent(3 * 4 * kVolumeSound), heard.volume);
	CHECK_EQ(3, g_SoundPerSecond);
}

// The motorcycle starting and stopping is five cells further, loaded or
// cached; running, and the sounds around, are not.
TEST(GameMain1PlaySound, TheMotorcycleSoundsFiveCellsFurther)
{
	Sounds world;
	if (!world.audio.ready) return;
	const int ids[] = {SOUND_WORLD_BIKE_GO - 1, SOUND_WORLD_BIKE_GO, SOUND_WORLD_BIKE_RUN, SOUND_WORLD_BIKE_STOP, SOUND_WORLD_BIKE_STOP + 1};
	const int extra[] = {0, 5, 0, 5, 0};
	for (int i = 0; i < 5; ++i)
	{
		world.Wav(ids[i]);
		g_SoundPerSecond = 0;
		PlayAt(ids[i], 6, 0);
		LPDIRECTSOUNDBUFFER buffer = world.cache.Held(TYPE_SOUNDID(ids[i]));
		CHECK_EQ(Percent((6 + extra[i]) * 4 * kVolumeSound), Hear(buffer).volume);
		g_SDLAudio.Stop(buffer);
		PlayAt(ids[i], 0, 2);
		CHECK_EQ(Percent((2 + extra[i]) * 4 * kVolumeSound), Hear(buffer).volume);
		CHECK(Hear(buffer).playing);
	}
}

TEST(GameMain1PlaySound, ItIsHeardOnlyWithin24CellsOnBothAxes)
{
	Sounds world;
	if (!world.audio.ready) return;
	world.Wav(1);
	PlayAt(1, 25, 0);
	PlayAt(1, 0, -25);
	PlayAt(1, -25, 25);
	CHECK(world.cache.Held(1) == nullptr);
	CHECK_EQ(0, g_SoundPerSecond);
	CHECK_EQ(0, Sounding());
	PlayAt(1, -24, 24);
	CHECK(Hear(world.cache.Held(1)).playing);
	CHECK_EQ(1, g_SoundPerSecond);
	// Cached, it is still not heard from 25 cells.
	g_SDLAudio.Stop(world.cache.Held(1));
	PlayAt(1, 0, 25);
	CHECK(!Hear(world.cache.Held(1)).playing);
	CHECK_EQ(1, g_SoundPerSecond);
}

// MAX_SOUND_PER_SECOND one-shots may still start at the limit, none over
// it; a repeated sound starts regardless.
TEST(GameMain1PlaySound, TheLimitStopsAOneShotOverItButNotARepeat)
{
	Sounds world;
	if (!world.audio.ready) return;
	world.Wav(1);
	world.Wav(2);
	world.Wav(3);
	g_SoundPerSecond = 6;
	PlayAt(1, 0, 0);
	CHECK(Hear(world.cache.Held(1)).playing);
	CHECK_EQ(7, g_SoundPerSecond);
	PlayAt(2, 0, 0);
	CHECK(world.cache.Held(2) == nullptr);
	CHECK_EQ(7, g_SoundPerSecond);
	PlayAt(3, 0, 0, true);
	CHECK(Hear(world.cache.Held(3)).playing);
	CHECK_EQ(8, g_SoundPerSecond);
	CHECK_EQ(2, Sounding());
}

TEST(GameMain1PlaySound, NothingWhileSilencedPastTheTableOrWithTheSoundOff)
{
	{
		Sounds world;
		if (!world.audio.ready) return;
		world.Wav(1);
		world.Silence();
		PlayAt(1, 0, 0, true);
		CHECK(world.cache.Held(1) == nullptr);
		CHECK_EQ(0, g_SoundPerSecond);
	}
	{
		Sounds world;
		world.option.PlaySound = FALSE;
		world.Wav(1);
		PlayAt(1, 0, 0, true);
		CHECK(world.cache.Held(1) == nullptr);
		CHECK_EQ(0, g_SoundPerSecond);
	}
	{
		// The id the table ends at, its file there all the same.
		Sounds world;
		world.Wav(kTableSize);
		PlayAt(kTableSize, 0, 0, true);
		CHECK_EQ(0, g_SoundPerSecond);
		CHECK_EQ(0, Sounding());
	}
}

TEST(GameMain1PlaySound, NothingWithoutAudio)
{
	Sounds world(false);
	CHECK(!g_SDLAudio.IsInit());
	world.Wav(1);
	// No table: LoadWav fails without audio anyway, so only the audio check
	// keeps the table from being read.
	g_pSoundTable = NULL;
	PlayAt(1, 0, 0, true);
	CHECK(world.cache.Held(1) == nullptr);
	CHECK_EQ(0, g_SoundPerSecond);
}

// No file name, or a file that does not load: nothing cached, played or
// counted.
TEST(GameMain1PlaySound, ASoundWithoutAFileOrWhoseFileFailsIsSkipped)
{
	Sounds world;
	if (!world.audio.ready) return;
	testfw::MutableRow(world.table, 1).Filename = "";
	CHECK(world.table[1].Filename.GetString() == nullptr);
	PlayAt(1, 0, 0);
	PlayAt(2, 0, 0);	// its file never written
	CHECK(world.cache.Held(1) == nullptr);
	CHECK(world.cache.Held(2) == nullptr);
	CHECK_EQ(0, g_SoundPerSecond);
	CHECK_EQ(0, Sounding());
}

// The name is cut to its first 255 characters, which is the file loaded.
TEST(GameMain1PlaySound, AFileNameIsCutAt255Characters)
{
	Sounds world;
	if (!world.audio.ready) return;
	const std::string directory = world.dir.path.string() + static_cast<char>(std::filesystem::path::preferred_separator);
	CHECK(directory.size() < 200);
	if (directory.size() >= 200) return;
	const std::string kept = directory + std::string(255 - directory.size(), 'k');
	CHECK_EQ(255, static_cast<int>(kept.size()));
	WriteWav(kept);
	testfw::MutableRow(world.table, 1).Filename = (kept + "-and-the-rest.wav").c_str();
	PlayAt(1, 0, 0);
	CHECK(Hear(world.cache.Held(1)).playing);
	CHECK_EQ(1, g_SoundPerSecond);
}

// At a good frame rate a copy plays beside the cached buffer still
// sounding, which keeps its channel; at a poor one the buffer itself is
// started again on another channel.
TEST(GameMain1PlaySound, AGoodFrameRatePlaysACopyBesideASoundStillSounding)
{
	Sounds world;
	if (!world.audio.ready) return;
	world.Wav(1);
	world.Wav(2);
	PlayAt(1, 0, 0, true);
	const int first = Hear(world.cache.Held(1)).channel;
	PlayAt(1, 0, 0, true);
	CHECK_EQ(first, Hear(world.cache.Held(1)).channel);
	CHECK_EQ(2, Sounding());
	g_bGoodFPS = false;
	PlayAt(2, 0, 0, true);
	const int second = Hear(world.cache.Held(2)).channel;
	PlayAt(2, 0, 0, true);
	CHECK(Hear(world.cache.Held(2)).channel != second);
	CHECK(Hear(world.cache.Held(2)).playing);
	CHECK_EQ(4, Sounding());
	CHECK_EQ(4, g_SoundPerSecond);
}

// A cache of one: the second sound evicts the first, whose buffer is freed
// (its loop ends).
TEST(GameMain1PlaySound, ALoadThatEvictsASoundFreesItsBuffer)
{
	Sounds world;
	if (!world.audio.ready) return;
	world.cache.Init(kTableSize, 1);
	world.Wav(1);
	world.Wav(2);
	PlayAt(1, 0, 0, true);
	CHECK_EQ(1, Sounding());
	PlayAt(2, 0, 0);
	CHECK(world.cache.Held(1) == nullptr);
	CHECK(Hear(world.cache.Held(2)).playing);
	CHECK_EQ(1, Sounding());
	CHECK_EQ(2, g_SoundPerSecond);
}

// Recorded defect: an id in the table but past the cache is loaded and
// played on every call, but SetData stores nothing for it and returns
// 0xFFFF, so the buffer is never kept, never freed (it leaks) and cannot
// be stopped (GameMain.cpp:2370, the same at 2523 and 2618).
TEST(GameMain1PlaySound, AnIdPastTheCacheIsPlayedButNeverKept)
{
	Sounds world;
	if (!world.audio.ready) return;
	world.cache.Init(10, 4);
	world.Wav(20);
	PlayAt(20, 0, 0);
	PlayAt(20, 0, 0);
	CHECK(world.cache.Held(20) == nullptr);
	CHECK_EQ(2, Sounding());
	CHECK_EQ(2, g_SoundPerSecond);
	StopSound(20);
	CHECK_EQ(2, Sounding());
}

//----------------------------------------------------------------------
// PlaySoundAtPlayer and PlaySoundForce
//----------------------------------------------------------------------
namespace {
typedef void (*PlayFunction)(TYPE_SOUNDID);
struct Variant { const char* name; PlayFunction play; };
const Variant kPlayers[] = {{"AtPlayer", PlaySoundAtPlayer}, {"Force", PlaySoundForce}};
}

// Loaded or cached, at the centre and the volume limit wherever the player
// is (there need be none), beyond the limit, and counted.
TEST(GameMain1PlayAtPlayer, BothPlayAtTheCentreAndFullVolumeWithNoLimit)
{
	for (const Variant& each : kPlayers)
	{
		Sounds world;
		if (!world.audio.ready) return;
		world.Wav(1);
		world.Wav(2);
		g_pPlayer = NULL;
		g_SoundPerSecond = 50;
		g_SDLAudio.SetVolumeLimit(-1000);
		each.play(1);
		Heard heard = Hear(world.cache.Held(1));
		CHECK(heard.playing);
		CHECK_EQ(0, heard.pan);
		CHECK_EQ(Percent(1000), heard.volume);
		CHECK_EQ(51, g_SoundPerSecond);
		// Cached after a positional play: back to the centre and the limit.
		g_pPlayer = &world.player;
		PlayAt(2, 10, 0, true);
		g_SDLAudio.Stop(world.cache.Held(2));
		g_pPlayer = NULL;
		each.play(2);
		heard = Hear(world.cache.Held(2));
		CHECK(heard.playing);
		CHECK_EQ(0, heard.pan);
		CHECK_EQ(Percent(1000), heard.volume);
		CHECK_EQ(53, g_SoundPerSecond);
		CHECK_EQ(2, Sounding());
	}
}

// Only PlaySoundForce plays while silenced and with the sound option off.
TEST(GameMain1PlayAtPlayer, OnlyTheForcedOnePlaysSilencedOrWithTheSoundOff)
{
	for (const Variant& each : kPlayers)
	{
		const bool forced = each.play == PlaySoundForce;
		{
			Sounds world;
			if (!world.audio.ready) return;
			world.Wav(1);
			world.Silence();
			each.play(1);
			CHECK_EQ(forced, world.cache.Held(1) != nullptr);
			CHECK_EQ(forced ? 1 : 0, g_SoundPerSecond);
		}
		{
			Sounds world;
			world.option.PlaySound = FALSE;
			world.Wav(1);
			each.play(1);
			CHECK_EQ(forced, world.cache.Held(1) != nullptr);
			CHECK_EQ(forced ? 1 : 0, g_SoundPerSecond);
		}
	}
}

TEST(GameMain1PlayAtPlayer, NeitherPlaysWithoutAudioPastTheTableOrWithoutAFile)
{
	for (const Variant& each : kPlayers)
	{
		{
			Sounds world(false);
			world.Wav(1);
			g_pSoundTable = NULL;	// as in NothingWithoutAudio
			each.play(1);
			CHECK(world.cache.Held(1) == nullptr);
			CHECK_EQ(0, g_SoundPerSecond);
		}
		Sounds world;
		if (!world.audio.ready) return;
		world.Wav(kTableSize);
		each.play(TYPE_SOUNDID(kTableSize));
		testfw::MutableRow(world.table, 1).Filename = "";
		each.play(1);
		each.play(2);	// its file never written
		CHECK(world.cache.Held(1) == nullptr);
		CHECK(world.cache.Held(2) == nullptr);
		CHECK_EQ(0, g_SoundPerSecond);
		CHECK_EQ(0, Sounding());
	}
}

TEST(GameMain1PlayAtPlayer, BothFreeTheBufferTheyEvictAndCopyAtAGoodFrameRate)
{
	for (const Variant& each : kPlayers)
	{
		Sounds world;
		if (!world.audio.ready) return;
		world.cache.Init(kTableSize, 1);
		world.Wav(1);
		world.Wav(2);
		PlayAt(1, 0, 0, true);
		each.play(2);
		CHECK(world.cache.Held(1) == nullptr);
		CHECK_EQ(1, Sounding());
		const int first = Hear(world.cache.Held(2)).channel;
		each.play(2);
		CHECK_EQ(first, Hear(world.cache.Held(2)).channel);
		CHECK_EQ(2, Sounding());
		g_bGoodFPS = false;
		each.play(2);
		CHECK(Hear(world.cache.Held(2)).channel != first);
		CHECK_EQ(3, Sounding());
		CHECK_EQ(4, g_SoundPerSecond);
	}
}

//----------------------------------------------------------------------
// StopSound
//----------------------------------------------------------------------
TEST(GameMain1StopSound, ItStopsTheCachedSoundAndNoOther)
{
	Sounds world;
	if (!world.audio.ready) return;
	world.Wav(1);
	world.Wav(2);
	PlayAt(1, 0, 0, true);
	PlayAt(2, 0, 0, true);
	StopSound(1);
	CHECK(!Hear(world.cache.Held(1)).playing);
	CHECK_EQ(-1, Hear(world.cache.Held(1)).channel);
	CHECK(Hear(world.cache.Held(2)).playing);
	CHECK_EQ(1, Sounding());
	// A copy that plays beside it is not its to stop (a zone change's
	// ReleaseDuplicateBuffer ends it).
	PlayAt(2, 0, 0, true);
	CHECK_EQ(2, Sounding());
	StopSound(2);
	CHECK(!Hear(world.cache.Held(2)).playing);
	CHECK_EQ(1, Sounding());
}

TEST(GameMain1StopSound, NothingForAnIdTheCacheDoesNotHold)
{
	Sounds world;
	if (!world.audio.ready) return;
	world.cache.Init(10, 4);
	world.Wav(1);
	PlayAt(1, 0, 0, true);
	StopSound(2);
	StopSound(10);
	StopSound(SOUNDID_NULL);
	CHECK(Hear(world.cache.Held(1)).playing);
	// The null id is refused before the cache is read.
	g_pSoundManager = NULL;
	StopSound(SOUNDID_NULL);
	g_pSoundManager = &world.cache;
	CHECK(Hear(world.cache.Held(1)).playing);
}

// Without audio nothing is read, the cache included.
TEST(GameMain1StopSound, WithoutAudioItReadsNothing)
{
	Sounds world(false);
	g_pSoundManager = NULL;
	StopSound(1);
	CHECK(!g_SDLAudio.IsInit());
}
#endif // DARKEDEN_TEST_HAVE_SDL_MIXER

//----------------------------------------------------------------------
// PlayMusicCurrentZone
//----------------------------------------------------------------------
namespace {

// The game time at an hour.
struct GameTime : MGameTime
{
	void Hour(BYTE hour) { m_Hour = hour; }
};

// A zone whose delayed sounds the tests read.
struct ZoneProbe : MZone
{
	ZoneProbe() : MZone(8, 8) {}
	std::vector<SOUND_NODE> Queued(MonotonicClock::TimePoint after)
	{
		std::vector<SOUND_NODE> queued;
		m_DelayedSounds.Update(after, [&queued](const SOUND_NODE& node) { queued.push_back(node); });
		return queued;
	}
};

const int kPlainZone = 60, kHolyZone = 1500, kOctavusZone = 1201;

// In the game in zone 60 (small), the large zone 1500 a holy land, 1201
// one too; at hour 7; the MIDI music on (g_Music ready, idle) at volume
// 15; the music table naming music<id>.mid and music<id>.wav, none
// written; no war, no event, no audio, no OGG stream.
struct Music
{
	TempDir dir;	// first: removed after Saved closes g_oggfile (Windows keeps an open file)
	Saved saved;
	Audio quiet{false};	// down: the wave music then only closes its file
	MEventQueue events;
	CZoneTable zones;
	MUSIC_TABLE music;
	GameTime time;
	MWarManager wars;
	ZoneProbe zone;
	UserOption option;

	Music()
	{
		g_pEventManager = &events;
		Zone(kPlainZone, false);
		Zone(kHolyZone, true);
		Zone(kOctavusZone, true);
		g_pZoneTable = &zones;
		music.Init(MAX_MUSIC);
		for (int id = 0; id < MAX_MUSIC; ++id)
		{
			testfw::MutableRow(music, id).Filename = dir.File("music" + std::to_string(id) + ".mid").c_str();
			testfw::MutableRow(music, id).FilenameWav = Wave(id).c_str();
		}
		g_pMusicTable = &music;
		time.Hour(7);
		g_pGameTime = &time;
		g_pWarManager = &wars;
		g_pZone = &zone;
		g_Mode = MODE_GAME;
		g_bZonePlayerInLarge = false;
		g_nZoneSmall = kPlainZone;
		g_nZoneLarge = kHolyZone;
		option.PlayMusic = TRUE;
		option.PlayWaveMusic = FALSE;
		option.VolumeMusic = 15;
		g_pUserOption = &option;
		g_Music.m_bInit = true;
		g_Music.m_bLoad = g_Music.m_bPlay = g_Music.m_bPause = false;
		g_pOGG = NULL;
		g_oggfile = NULL;
	}
	void Zone(int id, bool holy)
	{
		ZONETABLE_INFO* pInfo = new ZONETABLE_INFO;
		pInfo->ID = TYPE_ZONEID(id);
		pInfo->HolyLand = holy;
		CHECK(zones.Add(pInfo));
	}
	std::string Wave(int id) const { return dir.File("music" + std::to_string(id) + ".wav"); }
	// Only this track's wave file there.
	void Only(int id)
	{
		for (int other = 0; other < MAX_MUSIC; ++other)
		{
			std::error_code error;
			std::filesystem::remove(Wave(other), error);
		}
		WriteWav(Wave(id));
	}
	void War(int zoneID)
	{
		GuildWarInfo* pInfo = new GuildWarInfo;
		pInfo->setCastleID(ZoneID_t(zoneID));
		wars.SetWar(pInfo);
		CHECK(wars.IsExist(ZoneID_t(zoneID)));
	}
};

bool MidiPlaying() { return g_Music.m_bLoad && g_Music.m_bPlay && !g_Music.m_bPause; }
bool MidiIdle() { return !g_Music.m_bLoad && !g_Music.m_bPlay && !g_Music.m_bPause; }

} // namespace

TEST(GameMain1Music, InTheGameTheZonesTrackPlaysAsMIDI)
{
	Music world;
	PlayMusicCurrentZone();
	CHECK(MidiPlaying());
	CHECK(g_oggfile == NULL);
}

TEST(GameMain1Music, NothingSilencedOutsideTheGameWithoutAZoneOrWithTheMusicOff)
{
	for (int condition = 0; condition < 4; ++condition)
	{
		Music world;
		if (condition == 0)
		{
			MEvent event;
			event.eventID = EVENTID_NULL;
			event.eventFlag = EVENTFLAG_NOT_PLAY_SOUND;
			world.events.AddEvent(event);
		}
		if (condition == 1) g_Mode = MODE_WAIT_SETPOSITION;
		if (condition == 2) g_pZone = NULL;
		if (condition == 3) world.option.PlayMusic = FALSE;
		// The wave music's file, which a play would close first.
		world.option.PlayWaveMusic = TRUE;
		FILE* file = Basic::OpenFile(world.dir.File("open.wav").c_str(), "wb");
		CHECK(file != NULL);
		g_oggfile = file;
		PlayMusicCurrentZone();
		CHECK(g_oggfile == file);
		world.option.PlayWaveMusic = FALSE;
		PlayMusicCurrentZone();
		CHECK(MidiIdle());
	}
}

// Without the audio the wave music only closes the last file.
TEST(GameMain1Music, WithoutAudioTheWaveMusicOnlyClosesTheLastFile)
{
	Music world;
	CHECK(!g_SDLAudio.IsInit());
	world.option.PlayWaveMusic = TRUE;
	world.Only(MUSIC_TREASURE);
	g_oggfile = Basic::OpenFile(world.dir.File("open.wav").c_str(), "wb");
	CHECK(g_oggfile != NULL);
	PlayMusicCurrentZone();		// g_pOGG is NULL: not read
	CHECK(g_oggfile == NULL);
	CHECK(MidiIdle());
}

#ifdef DARKEDEN_TEST_HAVE_SDL_MIXER
namespace {
const int kUnknownZone = 4321;	// in no zone table

// The wave music's scene, the audio up and the stream ready.
struct WaveMusic : Music
{
	Audio audio{true};
	COGGSTREAM stream{nullptr, nullptr, 0, 0, 0, 0};
	WaveMusic()
	{
		option.PlayWaveMusic = TRUE;
		g_pOGG = &stream;
	}
	~WaveMusic() { stream.streamClose(); }
	// The track played: its file the only one there, it is open and
	// streaming.
	bool Plays(int id)
	{
		// The last track's file closed (its stream holds a copy), or Windows
		// could not delete it and a replay of it would pass.
		if (g_oggfile != NULL) std::fclose(g_oggfile);
		g_oggfile = NULL;
		Only(id);
		PlayMusicCurrentZone();
		return g_oggfile != NULL && dxlib_music_is_playing() != 0 && MidiIdle();
	}
};
}

// Outside the holy land the track follows the hour (none: hour 0).
TEST(GameMain1WaveMusic, OutsideTheHolyLandTheTrackFollowsTheHour)
{
	WaveMusic world;
	if (!world.audio.ready) return;
	CHECK(world.Plays(MUSIC_TREASURE));					// hour 7
	world.time.Hour(23);
	CHECK(world.Plays(MUSIC_HOLLOWEEN));
	g_pGameTime = NULL;
	CHECK(world.Plays(MUSIC_LIVE_OR_DEAD));
	g_pGameTime = &world.time;
	g_nZoneSmall = kUnknownZone;						// not a holy land either
	CHECK(world.Plays(MUSIC_HOLLOWEEN));
}

TEST(GameMain1WaveMusic, InTheHolyLandItIsTheHolyTrackOrTheWarsOrTheZonesOwn)
{
	WaveMusic world;
	if (!world.audio.ready) return;
	g_bZonePlayerInLarge = true;						// 1500
	CHECK(world.Plays(MUSIC_HOLYLAND));
	world.War(kHolyZone);
	CHECK(world.Plays(MUSIC_HOLYLAND_WAR));
	g_pWarManager = NULL;
	CHECK(world.Plays(MUSIC_HOLYLAND));
	g_pWarManager = &world.wars;
	g_nZoneLarge = kOctavusZone;
	world.War(kOctavusZone);
	CHECK(world.Plays(MUSIC_OCTAVUS));
	g_bZonePlayerInLarge = false;						// back in zone 60
	CHECK(world.Plays(MUSIC_TREASURE));
}

// The volume is (VolumeMusic-15)*250 hundredths of a dB, below -1.
TEST(GameMain1WaveMusic, TheVolumeIsVolumeMusicLess15Times250)
{
	WaveMusic world;
	if (!world.audio.ready) return;
	const int volumes[] = {15, 14, 11, 5, 0};
	for (int volume : volumes)
	{
		world.option.VolumeMusic = volume;
		CHECK(world.Plays(MUSIC_TREASURE));
		const long attenuation = volume == 15 ? -1 : (volume - 15) * 250L;
		CHECK_EQ(AudioVolumeToPercent(attenuation) * MIX_MAX_VOLUME / 100, Mix_VolumeMusic(-1));
	}
}

// The last track stops even when the new one's file is missing.
TEST(GameMain1WaveMusic, TheLastTrackStopsEvenWhenTheNewFileIsMissing)
{
	WaveMusic world;
	if (!world.audio.ready) return;
	CHECK(world.Plays(MUSIC_TREASURE));
	world.Only(MUSIC_TREASURE);
	world.time.Hour(23);
	PlayMusicCurrentZone();
	CHECK(g_oggfile == NULL);
	CHECK(!dxlib_music_is_playing());
}
#endif // DARKEDEN_TEST_HAVE_SDL_MIXER

//----------------------------------------------------------------------
// SetWeather
//----------------------------------------------------------------------
#ifdef DARKEDEN_TEST_HAVE_SDL_MIXER
namespace {

struct WeatherProbe : MWeather
{
	BYTE Count() const { return m_nMapEffect; }
};

const MWeatherHost weatherHost{
	.ReadOrigin = [](int& x, int& y) { x = 640; y = 1200; return true; },
	.Width = []() { return 800; },
	.Height = []() { return 600; },
};

// The sounds' scene, the player at (20, 50), a weather that may start, no
// view; the weather's sounds written.
struct WeatherScene : Sounds
{
	WeatherProbe weather;
	const MWeatherHost* previousHost = MWeather::SetHost(&weatherHost);
	WeatherScene()
	{
		player.MObject::SetPosition(20, 50);
		g_pWeather = &weather;
		g_pTopView = NULL;
		Wav(SOUND_WORLD_WEATHER_RAIN_1);
		Wav(SOUND_WORLD_WEATHER_RAIN_2);
	}
	~WeatherScene() { MWeather::SetHost(previousHost); }
	Heard Rain(int id) { return Hear(cache.Held(TYPE_SOUNDID(id))); }
};

} // namespace

// The rain's sound loops at the player (centre, full volume), over the
// per-second limit too; the heavy one from level 15.
TEST(GameMain1Weather, RainFallsWithItsSoundLoopingAtThePlayer)
{
	WeatherScene world;
	if (!world.audio.ready) return;
	g_SoundPerSecond = 7;
	SetWeather(WEATHER_RAINY, 14);
	CHECK_EQ(int(MWeather::WEATHER_RAIN), int(world.weather.GetWeatherType()));
	CHECK_EQ(112, int(world.weather.Count()));
	CHECK_EQ(int(SOUND_WORLD_WEATHER_RAIN_1), g_previousSoundID);
	const Heard heard = world.Rain(SOUND_WORLD_WEATHER_RAIN_1);
	CHECK(heard.playing);
	CHECK_EQ(0, heard.pan);
	CHECK_EQ(100, heard.volume);
	CHECK_EQ(8, g_SoundPerSecond);
	// Heavier: the light sound stops, the heavy one plays.
	SetWeather(WEATHER_RAINY, 15);
	CHECK_EQ(120, int(world.weather.Count()));
	CHECK_EQ(int(SOUND_WORLD_WEATHER_RAIN_2), g_previousSoundID);
	CHECK(!world.Rain(SOUND_WORLD_WEATHER_RAIN_1).playing);
	CHECK(world.Rain(SOUND_WORLD_WEATHER_RAIN_2).playing);
	CHECK_EQ(1, Sounding());
	CHECK_EQ(int(MWeather::WEATHER_RAIN), int(world.weather.GetWeatherType()));
}

TEST(GameMain1Weather, ClearSkiesEndTheRainAndItsSound)
{
	WeatherScene world;
	if (!world.audio.ready) return;
	SetWeather(WEATHER_RAINY, 5);
	CHECK_EQ(40, int(world.weather.Count()));
	SetWeather(WEATHER_CLEAR, 9);
	CHECK_EQ(int(MWeather::WEATHER_STOP), int(world.weather.GetWeatherType()));	// the drops still falling
	CHECK_EQ(SOUNDID_NULL, g_previousSoundID);
	CHECK(!world.Rain(SOUND_WORLD_WEATHER_RAIN_1).playing);
	CHECK_EQ(0, Sounding());
	CHECK_EQ(1, g_SoundPerSecond);
}

TEST(GameMain1Weather, SnowAndSpotsFallSilently)
{
	WeatherScene world;
	if (!world.audio.ready) return;
	SetWeather(WEATHER_RAINY, 5);
	SetWeather(WEATHER_SNOWY, 3);
	CHECK_EQ(int(MWeather::WEATHER_SNOW), int(world.weather.GetWeatherType()));
	CHECK_EQ(24, int(world.weather.Count()));
	CHECK_EQ(SOUNDID_NULL, g_previousSoundID);
	CHECK_EQ(0, Sounding());
	SetWeather(WEATHER_SPOT, 2);
	CHECK_EQ(int(MWeather::WEATHER_SPOT), int(world.weather.GetWeatherType()));
	CHECK_EQ(16, int(world.weather.Count()));
	CHECK_EQ(SOUNDID_NULL, g_previousSoundID);
}

// After spots the weather is released first: clear skies are then nothing
// at all, and a level 0 (no particles) leaves nothing either.
TEST(GameMain1Weather, TheWeatherAfterSpotsStartsFromNothing)
{
	WeatherScene world;
	if (!world.audio.ready) return;
	SetWeather(WEATHER_SPOT, 2);
	SetWeather(WEATHER_CLEAR, 0);
	CHECK_EQ(int(MWeather::WEATHER_NULL), int(world.weather.GetWeatherType()));
	CHECK_EQ(0, int(world.weather.Count()));
	SetWeather(WEATHER_SPOT, 2);
	SetWeather(WEATHER_SNOWY, 0);
	CHECK_EQ(int(MWeather::WEATHER_NULL), int(world.weather.GetWeatherType()));
	// Snow is not released: clear skies leave its flakes falling, and a
	// level 0 leaves the snow.
	SetWeather(WEATHER_SNOWY, 2);
	SetWeather(WEATHER_SNOWY, 0);
	CHECK_EQ(int(MWeather::WEATHER_SNOW), int(world.weather.GetWeatherType()));
	SetWeather(WEATHER_CLEAR, 0);
	CHECK_EQ(int(MWeather::WEATHER_STOP), int(world.weather.GetWeatherType()));
}

// The first call (after ResetGameMainState) is its own previous weather:
// only spots then release what was there.
TEST(GameMain1Weather, TheFirstWeatherIsItsOwnPrevious)
{
	WeatherScene world;
	if (!world.audio.ready) return;
	world.weather.SetRain(40);
	SetWeather(WEATHER_SNOWY, 0);
	CHECK_EQ(int(MWeather::WEATHER_RAIN), int(world.weather.GetWeatherType()));
	ResetGameMainState();
	SetWeather(WEATHER_SPOT, 0);
	CHECK_EQ(int(MWeather::WEATHER_NULL), int(world.weather.GetWeatherType()));
}

// A weather past the four stops the last sound, but keeps its id, and the
// weather as it was.
TEST(GameMain1Weather, AnUnknownWeatherOnlyStopsTheSound)
{
	WeatherScene world;
	if (!world.audio.ready) return;
	SetWeather(WEATHER_RAINY, 5);
	SetWeather(WEATHER_MAX, 9);
	CHECK(!world.Rain(SOUND_WORLD_WEATHER_RAIN_1).playing);
	CHECK_EQ(int(SOUND_WORLD_WEATHER_RAIN_1), g_previousSoundID);
	CHECK_EQ(int(MWeather::WEATHER_RAIN), int(world.weather.GetWeatherType()));
	CHECK_EQ(40, int(world.weather.Count()));
}
#endif // DARKEDEN_TEST_HAVE_SDL_MIXER

//----------------------------------------------------------------------
// SetLightning
//----------------------------------------------------------------------
namespace {

// The view, ready or not, open to the tests.
struct ViewProbe : MTopView
{
	void Ready() { m_bInit = true; }
	~ViewProbe() { m_bInit = false; }
};

const MonotonicClock::TimePoint kNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(5000000);

// A ready view, the player at (30, 40) in a zone, the frame at kNow.
struct Lightning
{
	Saved saved;
	Config config;
	CreatureTables creatures;
	ZoneProbe zone;
	MPlayer player;
	ViewProbe view;
	Lightning()
	{
		g_pZone = &zone;
		player.MObject::SetPosition(30, 40);
		g_pPlayer = &player;
		view.Ready();
		g_pTopView = &view;
		g_FrameNow = kNow;
	}
	// The thunder queued: one sound at the player, delay ms on.
	bool Thunder(TYPE_SOUNDID id, DWORD delay)
	{
		const std::vector<SOUND_NODE> queued = zone.Queued(kNow + MonotonicClock::Millis(3600000));
		return queued.size() == 1 && queued[0].GetSoundID() == id && queued[0].GetX() == 30 && queued[0].GetY() == 40
			&& queued[0].GetPlayTime() == kNow + MonotonicClock::Millis(delay);
	}
};

} // namespace

// The flash: white, from 1 to 31 in steps of 10 a tick.
TEST(GameMain1Lightning, ItFlashesTheViewAndQueuesTheThunderAtThePlayer)
{
	Lightning world;
	SetLightning(1000);
	CHECK(world.view.IsFade());
	CHECK_EQ(CSDLGraphics::Color(31, 31, 31), world.view.m_FadeColor);
	const int values[] = {1, 11, 21, 31};
	for (int value : values)
	{
		CHECK(world.view.m_Fade.IsActive());
		CHECK_EQ(value, int(world.view.m_Fade.Value()));
		world.view.m_Fade.Advance(1, true);
	}
	CHECK(!world.view.m_Fade.IsActive());
	CHECK(world.Thunder(SOUND_WORLD_WEATHER_THUNDER_1, 1000));
}

TEST(GameMain1Lightning, PastASecondTheThunderIsTheFarOne)
{
	Lightning world;
	SetLightning(1001);
	CHECK(world.Thunder(SOUND_WORLD_WEATHER_THUNDER_2, 1001));
	SetLightning(0);
	CHECK(world.Thunder(SOUND_WORLD_WEATHER_THUNDER_1, 0));
}

TEST(GameMain1Lightning, AViewNotReadyOrNoneIsNotFlashedButItThunders)
{
	Lightning world;
	ViewProbe idle;
	g_pTopView = &idle;
	SetLightning(10);
	CHECK(!idle.IsFade());
	CHECK(world.Thunder(SOUND_WORLD_WEATHER_THUNDER_1, 10));
	g_pTopView = NULL;
	SetLightning(20);
	CHECK(world.Thunder(SOUND_WORLD_WEATHER_THUNDER_1, 20));
	CHECK(!world.view.IsFade());
}

//----------------------------------------------------------------------
// CheckActivate
//----------------------------------------------------------------------
namespace {

// The game on the main menu, active, the sound muted, every button down,
// the mouse wheel turned and a click pending, a ready view waiting to be
// drawn, the player locked and repeating, the MIDI music idle (the theme
// is MUSIC_TREASURE), a zone as for the music.
struct Activation : Music
{
	CSDLInput input;
	ViewProbe view;
	Properties files;
	Config config;
	CreatureTables creatures;
	MPlayer player;
	Activation()
	{
		input.m_mouse_z = 3;
		input.m_lb_down = TRUE;
		input.m_rb_up = TRUE;
		g_pSDLInput = &input;
		view.Ready();
		view.m_bDrawRequest = true;
		view.m_bFirstTileDraw = false;
		g_pTopView = &view;
		// RestoreSurface reloads the light filter: there is none.
		files.setProperty("FILE_FILTER_LIGHT2D", dir.File("light.ftp"));
		g_pFileDef = &files;
		config.MUSIC_THEME = MUSIC_TREASURE;
		player.SetLockMode();
		player.SetRepeatAction();
		g_pPlayer = &player;
		g_bActiveApp = TRUE;
		g_bActiveGame = FALSE;
		g_bFullScreen = false;
		g_Mode = MODE_MAINMENU;
		g_SDLAudio.SetMute();
		g_bLButtonDown = g_bRButtonDown = g_bCButtonDown = TRUE;
	}
	~Activation() { g_pClientConfig = &config; }
	bool InputFlushed() const { return input.m_mouse_z == 0 && !input.m_lb_down && !input.m_rb_up; }
	bool ViewRestored() const { return !view.m_bDrawRequest && view.m_bFirstTileDraw; }
	bool ButtonsDown() const { return g_bLButtonDown && g_bRButtonDown && g_bCButtonDown; }
};

} // namespace

TEST(GameMain1Activate, ActivatingRestoresTheViewTheMusicTheSoundTheButtonsAndThePlayer)
{
	Activation world;
	CheckActivate(TRUE);
	CHECK_EQ(TRUE, g_bActiveGame);
	CHECK(world.InputFlushed());
	CHECK(world.ViewRestored());
	CHECK(MidiPlaying());								// the theme
	CHECK(!g_SDLAudio.IsMute());
	CHECK(!g_bLButtonDown && !g_bRButtonDown && !g_bCButtonDown);
	CHECK(!world.player.IsLockMode());
	CHECK(world.player.IsRepeatAction());
}

// Without the input, a view, a player, the options or the configuration
// the rest is still done; a view not ready is left.
TEST(GameMain1Activate, WhatIsMissingIsSkipped)
{
	Activation world;
	ViewProbe idle;
	idle.m_bDrawRequest = true;
	idle.m_bFirstTileDraw = false;
	g_pTopView = &idle;
	g_pSDLInput = NULL;
	g_pPlayer = NULL;
	g_pUserOption = NULL;
	CheckActivate(TRUE);
	CHECK(idle.m_bDrawRequest && !idle.m_bFirstTileDraw);
	CHECK(MidiIdle());
	CHECK(!g_SDLAudio.IsMute());
	CHECK(!g_bLButtonDown);
	CHECK_EQ(TRUE, g_bActiveGame);
	g_pTopView = NULL;
	g_pUserOption = &world.option;
	g_pClientConfig = NULL;
	g_SDLAudio.SetMute();
	CheckActivate(TRUE);
	CHECK(MidiIdle());
	CHECK(!g_SDLAudio.IsMute());
}

// The theme only with the music on, a theme and a music table.
TEST(GameMain1Activate, OutsideTheGameTheThemeStartsIfThereIsOne)
{
	for (int condition = 0; condition < 3; ++condition)
	{
		Activation world;
		if (condition == 0) world.option.PlayMusic = FALSE;
		if (condition == 1) world.config.MUSIC_THEME = MUSICID_NULL;
		if (condition == 2) g_pMusicTable = NULL;
		CheckActivate(TRUE);
		CHECK(MidiIdle());
		CHECK(world.ViewRestored());
	}
}

// In the game it is the zone's music: none without a zone.
TEST(GameMain1Activate, InTheGameTheZonesMusicStarts)
{
	Activation world;
	g_Mode = MODE_GAME;
	g_pZone = NULL;
	CheckActivate(TRUE);
	CHECK(MidiIdle());
	g_pZone = &world.zone;
	CheckActivate(TRUE);
	CHECK(MidiPlaying());
}

// Music still playing is left alone (Resume does nothing to it): the
// zone's is not started over it. The wave music shows it without audio,
// the zone's play closing the last file.
TEST(GameMain1Activate, MusicStillPlayingIsLeftAlone)
{
	Activation world;
	g_Mode = MODE_GAME;
	world.option.PlayWaveMusic = TRUE;
	FILE* file = Basic::OpenFile(world.dir.File("open.wav").c_str(), "wb");
	g_oggfile = file;
	g_Music.Play("playing.mid");
	CheckActivate(TRUE);
	CHECK(g_oggfile == file);
	CHECK(MidiPlaying());
	g_Music.Stop();
	CheckActivate(TRUE);
	CHECK(g_oggfile == NULL);
}

// In MODE_WAIT_UPDATEINFO the mute stays as it is, either way.
TEST(GameMain1Activate, WaitingForTheGameInfoTheMuteIsLeft)
{
	Activation world;
	g_Mode = MODE_WAIT_UPDATEINFO;
	CheckActivate(TRUE);
	CHECK(g_SDLAudio.IsMute());
	g_SDLAudio.UnSetMute();
	CheckActivate(FALSE);
	CHECK(!g_SDLAudio.IsMute());
}

// Deactivated: the music stops, the sound is muted, the player stops
// repeating; the view, the buttons and the lock are left.
TEST(GameMain1Activate, DeactivatingStopsTheMusicMutesAndEndsTheRepeat)
{
	Activation world;
	g_bActiveGame = TRUE;
	g_SDLAudio.UnSetMute();
	g_Music.Play("playing.mid");
	CheckActivate(FALSE);
	CHECK_EQ(FALSE, g_bActiveGame);
	CHECK(world.InputFlushed());
	CHECK(MidiIdle());
	CHECK(g_SDLAudio.IsMute());
	CHECK(!world.player.IsRepeatAction());
	CHECK(world.player.IsLockMode());
	CHECK(world.ButtonsDown());
	CHECK(world.view.m_bDrawRequest);
	g_pPlayer = NULL;
	CheckActivate(FALSE);
	CHECK(g_SDLAudio.IsMute());
}

// An inactive application only has its input flushed and the game marked
// inactive.
TEST(GameMain1Activate, AnInactiveApplicationOnlyFlushesTheInput)
{
	for (BOOL active : {TRUE, FALSE})
	{
		Activation world;
		g_bActiveApp = FALSE;
		g_bActiveGame = TRUE;
		g_Music.Play("playing.mid");
		CheckActivate(active);
		CHECK_EQ(FALSE, g_bActiveGame);
		CHECK(world.InputFlushed());
		CHECK(MidiPlaying());
		CHECK(g_SDLAudio.IsMute());
		CHECK(world.ButtonsDown());
		CHECK(world.player.IsLockMode() && world.player.IsRepeatAction());
		CHECK(world.view.m_bDrawRequest && !world.view.m_bFirstTileDraw);
	}
}

//----------------------------------------------------------------------
// MakeScreenShot
//----------------------------------------------------------------------
namespace {

// The working directory a temporary one, five screenshots allowed, named
// ScreenShot/shot<nnn>.jpg, the counter at 0.
struct ScreenShots
{
	Saved saved;
	TempDir dir;
	std::filesystem::path cwd = std::filesystem::current_path();
	ClientConfig config;
	Properties files;
	ScreenShots()
	{
		std::filesystem::current_path(dir.path);
		config.MAX_SCREENSHOT = 5;
		g_pClientConfig = &config;
		files.setProperty("PATH_SCREENSHOT", "ScreenShot/shot");
		g_pFileDef = &files;
		g_ScreenShotNumber = 0;
	}
	~ScreenShots() { std::filesystem::current_path(cwd); }
	std::filesystem::path Folder() const { return dir.path / "ScreenShot"; }
	void Take(int number)
	{
		char name[32];
		std::snprintf(name, sizeof(name), "shot%03d.jpg", number);
		std::ofstream(Folder() / name) << "jpg";
	}
	// The folder's files, by name.
	std::string Listed() const
	{
		std::vector<std::string> names;
		for (const auto& entry : std::filesystem::directory_iterator(Folder()))
			names.push_back(entry.path().filename().string());
		std::sort(names.begin(), names.end());
		std::string listed;
		for (const std::string& name : names) listed += name + ";";
		return listed;
	}
};

} // namespace

// Recorded defect: the number is taken but nothing is saved (the save was
// never ported to the SDL surfaces; GameMain.cpp:2262-2275).
TEST(GameMain1ScreenShot, ItTakesTheFirstFreeNumberAndSavesNothing)
{
	ScreenShots world;
	MakeScreenShot();
	CHECK(std::filesystem::is_directory(world.Folder()));
	CHECK_EQ(1, g_ScreenShotNumber);
	CHECK(world.Listed().empty());
	world.Take(1);
	world.Take(2);
	world.Take(4);
	MakeScreenShot();
	CHECK_EQ(4, g_ScreenShotNumber);
	MakeScreenShot();
	CHECK_EQ(5, g_ScreenShotNumber);
	CHECK(world.Listed() == "shot001.jpg;shot002.jpg;shot004.jpg;");
}

// The counter starts where it is: the numbers below it are not looked at.
TEST(GameMain1ScreenShot, TheSearchStartsAtTheCounter)
{
	ScreenShots world;
	world.config.MAX_SCREENSHOT = 100;
	std::filesystem::create_directory(world.Folder());
	world.Take(10);
	g_ScreenShotNumber = 10;
	MakeScreenShot();
	CHECK_EQ(12, g_ScreenShotNumber);
}

// Every number taken: the counter stops at the limit and stays there; the
// folder is made all the same, and no file name is asked for.
TEST(GameMain1ScreenShot, AtTheLimitNoNumberIsTaken)
{
	ScreenShots world;
	std::filesystem::create_directory(world.Folder());
	for (int number = 0; number < 5; ++number) world.Take(number);
	MakeScreenShot();
	CHECK_EQ(5, g_ScreenShotNumber);
	std::filesystem::remove_all(world.Folder());
	Properties none;
	g_pFileDef = &none;
	MakeScreenShot();
	CHECK_EQ(5, g_ScreenShotNumber);
	CHECK(std::filesystem::is_directory(world.Folder()));
}

//----------------------------------------------------------------------
// SaveLastSelectedCharacter and SelectLastSelectedCharacter
//----------------------------------------------------------------------
namespace {

struct Servers : CServerInformation
{
	void World(unsigned int id) { m_ServerGroupID = id; }
};

// The file's bytes, built.
struct Bytes
{
	std::vector<unsigned char> data;
	Bytes& U32(std::uint32_t value)
	{
		for (int i = 0; i < 4; ++i) data.push_back(static_cast<unsigned char>(value >> (8 * i)));
		return *this;
	}
	Bytes& Account(const std::string& name, unsigned char slot, std::uint32_t recent)
	{
		data.push_back(static_cast<unsigned char>(name.size()));
		data.insert(data.end(), name.begin(), name.end());
		data.push_back(slot);
		return U32(recent);
	}
};

// The account "slayer01" in world 7, the table file in a temporary
// directory, no table yet.
struct LastSlot
{
	Saved saved;
	TempDir dir;
	Properties files;
	UserInformation user;
	Servers servers;
	LastSlot()
	{
		files.setProperty("FILE_INFO_PLAYER_INFO", Path());
		g_pFileDef = &files;
		user.UserID = "slayer01";
		g_pUserInformation = &user;
		servers.World(7);
		g_pServerInformation = &servers;
		g_pWorldPlayerConfigTable = NULL;
	}
	~LastSlot() { delete g_pWorldPlayerConfigTable; }
	std::string Path() const { return dir.File("player.inf"); }
	std::vector<unsigned char> File() const { return ReadBytes(Path()); }
	void Write(const Bytes& bytes) const
	{
		std::ofstream file(Path(), std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<const char*>(bytes.data.data()), static_cast<std::streamsize>(bytes.data.size()));
	}
	int Slot(int world, const char* account) const
	{
		if (g_pWorldPlayerConfigTable == NULL) return -1;
		PlayerConfigTable* pTable = g_pWorldPlayerConfigTable->GetPlayerConfigTable(world);
		PlayerConfig* pConfig = pTable != NULL ? pTable->GetPlayerConfig(account) : NULL;
		return pConfig != NULL ? pConfig->GetLastSlot() : -1;
	}
};

} // namespace

TEST(GameMain1LastSlot, SaveMakesTheTableTheWorldAndTheAccountAndWritesTheFile)
{
	LastSlot world;
	SaveLastSelectedCharacter(2);
	CHECK(g_pWorldPlayerConfigTable != NULL);
	CHECK_EQ(2, world.Slot(7, "slayer01"));
	CHECK(Bytes().U32(2).U32(1).U32(7).U32(1).Account("slayer01", 2, 1).data == world.File());
}

// The same account again: its slot replaced; another account beside it
// (the first one aging); another world after.
TEST(GameMain1LastSlot, LaterSavesUpdateTheSlotAndAddAccountsAndWorlds)
{
	LastSlot world;
	SaveLastSelectedCharacter(2);
	SaveLastSelectedCharacter(0);
	CHECK(Bytes().U32(2).U32(1).U32(7).U32(1).Account("slayer01", 0, 1).data == world.File());
	world.user.UserID = "vamp02";
	SaveLastSelectedCharacter(1);
	CHECK(Bytes().U32(2).U32(1).U32(7).U32(2).Account("slayer01", 0, 2).Account("vamp02", 1, 1).data == world.File());
	world.servers.World(9);
	SaveLastSelectedCharacter(2);
	CHECK(Bytes().U32(2).U32(2)
		.U32(7).U32(2).Account("slayer01", 0, 3).Account("vamp02", 1, 2)
		.U32(9).U32(1).Account("vamp02", 2, 1).data == world.File());
}

// An account of 15 characters or more, or none, is saved as "DarkEden";
// 14 are kept.
TEST(GameMain1LastSlot, ALongOrMissingAccountIsSavedAsDarkEden)
{
	{
		LastSlot world;
		world.user.UserID = "abcdefghijklmn";
		SaveLastSelectedCharacter(1);
		CHECK(Bytes().U32(2).U32(1).U32(7).U32(1).Account("abcdefghijklmn", 1, 1).data == world.File());
	}
	{
		LastSlot world;
		world.user.UserID = "abcdefghijklmno";
		SaveLastSelectedCharacter(1);
		CHECK(Bytes().U32(2).U32(1).U32(7).U32(1).Account("DarkEden", 1, 1).data == world.File());
		// Not found under its own name, it is a new "DarkEden" each time.
		SaveLastSelectedCharacter(2);
		CHECK(Bytes().U32(2).U32(1).U32(7).U32(1).Account("DarkEden", 2, 1).data == world.File());
	}
	{
		LastSlot world;
		world.user.UserID = "";
		SaveLastSelectedCharacter(0);
		CHECK(Bytes().U32(2).U32(1).U32(7).U32(1).Account("DarkEden", 0, 1).data == world.File());
	}
}

// A slot past the three is not taken, but the file is written.
TEST(GameMain1LastSlot, ASlotOutOfRangeIsIgnoredButSaved)
{
	LastSlot world;
	SaveLastSelectedCharacter(2);
	SaveLastSelectedCharacter(3);
	CHECK(Bytes().U32(2).U32(1).U32(7).U32(1).Account("slayer01", 2, 2).data == world.File());
	SaveLastSelectedCharacter(-1);
	CHECK(Bytes().U32(2).U32(1).U32(7).U32(1).Account("slayer01", 2, 3).data == world.File());
}

// Without the user's or the server's information the table is made, but
// nothing is read, recorded or written.
TEST(GameMain1LastSlot, WithoutTheUserOrServerOnlyTheTableIsMade)
{
	for (int missing = 0; missing < 2; ++missing)
	{
		LastSlot world;
		world.Write(Bytes().U32(2).U32(1).U32(7).U32(1).Account("slayer01", 1, 1));
		const std::vector<unsigned char> before = world.File();
		if (missing == 0) g_pUserInformation = NULL; else g_pServerInformation = NULL;
		SaveLastSelectedCharacter(2);
		CHECK(g_pWorldPlayerConfigTable != NULL && g_pWorldPlayerConfigTable->empty());
		CHECK(before == world.File());
		delete g_pWorldPlayerConfigTable;
		g_pWorldPlayerConfigTable = NULL;
		SelectLastSelectedCharacter();
		CHECK(g_pWorldPlayerConfigTable != NULL && g_pWorldPlayerConfigTable->empty());
	}
}

// Saving does not read the file: an empty table writes only itself (in the
// game SetMode(MODE_WAIT_SELECTPC) has selected, and so loaded, first).
TEST(GameMain1LastSlot, SaveDoesNotReadTheFile)
{
	LastSlot world;
	world.Write(Bytes().U32(2).U32(1).U32(3).U32(1).Account("other", 1, 1));
	SaveLastSelectedCharacter(2);
	CHECK(Bytes().U32(2).U32(1).U32(7).U32(1).Account("slayer01", 2, 1).data == world.File());
}

// Selecting loads the file into an empty table, once; a save after keeps
// what was loaded.
TEST(GameMain1LastSlot, SelectLoadsTheFileIntoAnEmptyTableOnce)
{
	LastSlot world;
	world.Write(Bytes().U32(2).U32(2)
		.U32(3).U32(1).Account("other", 1, 4)
		.U32(7).U32(1).Account("slayer01", 1, 5));
	SelectLastSelectedCharacter();
	CHECK_EQ(1, world.Slot(3, "other"));
	CHECK_EQ(1, world.Slot(7, "slayer01"));
	SaveLastSelectedCharacter(2);
	CHECK(Bytes().U32(2).U32(2)
		.U32(3).U32(1).Account("other", 1, 5)
		.U32(7).U32(1).Account("slayer01", 2, 1).data == world.File());
	// The table not empty, the file is not read again.
	world.Write(Bytes().U32(2).U32(1).U32(4).U32(1).Account("newer", 0, 1));
	SelectLastSelectedCharacter();
	CHECK_EQ(-1, world.Slot(4, "newer"));
	CHECK_EQ(2, world.Slot(7, "slayer01"));
}

// Without a file, or with an account the world does not know, selecting
// leaves the table as it was.
TEST(GameMain1LastSlot, SelectWithNoFileOrNoAccountChangesNothing)
{
	LastSlot world;
	SelectLastSelectedCharacter();
	CHECK(g_pWorldPlayerConfigTable != NULL && g_pWorldPlayerConfigTable->empty());
	CHECK(!std::filesystem::exists(world.Path()));
	world.Write(Bytes().U32(2).U32(1).U32(7).U32(1).Account("other", 1, 1));
	world.user.UserID = "abcdefghijkl";	// 12: not looked up
	SelectLastSelectedCharacter();
	CHECK_EQ(1, world.Slot(7, "other"));
	CHECK_EQ(1, int(g_pWorldPlayerConfigTable->size()));
}
