#include "test_framework.h"
#include "MWeather.h"

#include <initializer_list>

namespace {

struct Environment {
	bool player = true;
	int x = 120, y = 240;
	int width = 80, height = 60;
	int originReads = 0, widthReads = 0, heightReads = 0;
} environment;

bool ReadOrigin(int& x, int& y)
{
	++environment.originReads;
	if (!environment.player) return false;
	x = environment.x;
	y = environment.y;
	return true;
}

int Width() { ++environment.widthReads; return environment.width; }
int Height() { ++environment.heightReads; return environment.height; }

const MWeatherHost host = {.ReadOrigin = ReadOrigin, .Width = Width, .Height = Height};

struct HostScope {
	const MWeatherHost* previous;
	explicit HostScope(const MWeatherHost* installed = &host)
		: previous(MWeather::SetHost(installed)) { environment = {}; }
	~HostScope() { MWeather::SetHost(previous); }
};

// Seed a known phase to isolate progression from the legacy constructor's
// partially initialized particles. The public start/resize contracts get
// separate coverage, and constructor fixes follow the extraction.
class WeatherProbe : public MWeather {
public:
	void Prepare(BYTE count, BYTE phase)
	{
		Init(count);
		for (int i = 0; i < count; ++i)
			m_pMapEffect[i].Set(phase, 0, 12, 34, 0, 0, 0);
	}
	MAP_EFFECT& Particle(BYTE index) { return m_pMapEffect[index]; }
	void Generate(BYTE kind, BYTE index)
	{
		if (kind == WEATHER_RAIN) GenerateRain(index);
		else if (kind == WEATHER_SNOW) GenerateSnow(index);
		else GenerateSpot(index);
	}
};

void Start(MWeather& weather, BYTE kind, BYTE count)
{
	if (kind == MWeather::WEATHER_RAIN) weather.SetRain(count);
	else if (kind == MWeather::WEATHER_SNOW) weather.SetSnow(count);
	else weather.SetSpot(count);
}

void CheckPosition(const MAP_EFFECT& effect, int x, int y)
{
	CHECK_EQ(x, effect.GetX());
	CHECK_EQ(y, effect.GetY());
}

} // namespace

TEST(MapEffect, MovesExactlyItsLifetimeAndKeepsTheMaximumCount)
{
	MAP_EFFECT effect;
	effect.Set(MAP_EFFECT::MAP_EFFECT_FALL, 7, 12, 34, 2, 3, 2);
	CHECK_EQ(7, effect.GetSpriteID());
	CHECK(effect.Move());
	CheckPosition(effect, 14, 37);
	CHECK_EQ(1, effect.GetCount());
	CHECK_EQ(2, effect.GetMaxCount());
	effect.SetSX(4);
	effect.SetSY(5);
	CHECK(effect.Move());
	CheckPosition(effect, 18, 42);
	CHECK(!effect.IsActive());
	CHECK(!effect.Move());
	CheckPosition(effect, 18, 42);
	CHECK_EQ(2, effect.GetMaxCount());
}

TEST(MapEffect, AssignmentCopiesMotionAndLifetimeIndependently)
{
	MAP_EFFECT first, second;
	first.Set(MAP_EFFECT::MAP_EFFECT_ARRIVE2, 13, 10, 20, 1, 2, 5);
	second = first;
	CHECK_EQ(first.GetType(), second.GetType());
	CHECK_EQ(first.GetSpriteID(), second.GetSpriteID());
	CHECK_EQ(5, second.GetCount());
	CHECK_EQ(5, second.GetMaxCount());
	CHECK(second.Move());
	CheckPosition(second, 11, 22);
	CheckPosition(first, 10, 20);
	CHECK_EQ(5, first.GetCount());
}

TEST(Weather, MissingHostsAndOriginCallbacksSkipNewWeather)
{
	const MWeatherHost empty;
	for (const MWeatherHost* installed : {static_cast<const MWeatherHost*>(nullptr), &empty})
	{
		HostScope scope(installed);
		MWeather weather;
		for (BYTE kind : {MWeather::WEATHER_RAIN, MWeather::WEATHER_SNOW, MWeather::WEATHER_SPOT})
		{
			Start(weather, kind, 8);
			CHECK(!weather.IsActive());
			CHECK_EQ(0, weather.GetSize());
		}
		weather.Action();
	}
}

TEST(Weather, OriginsAreCapturedForEachNewWeatherKind)
{
	HostScope scope;
	MWeather weather;
	weather.SetSpot(1);
	CHECK_EQ(120, weather.GetStartX());
	CHECK_EQ(240, weather.GetStartY());
	environment.x = 500;
	environment.y = 700;
	weather.SetSpot(1);
	CHECK_EQ(1, environment.originReads);
	CHECK_EQ(120, weather.GetStartX());
	// Reusing this initialized one-particle allocation is safe in the
	// original implementation, even before the constructor follow-up.
	weather.SetRain(1);
	CHECK_EQ(500, weather.GetStartX());
	CHECK_EQ(700, weather.GetStartY());
	environment.x = 900;
	weather.SetSnow(1);
	CHECK_EQ(900, weather.GetStartX());
	CHECK_EQ(3, environment.originReads);
}

TEST(Weather, ADisappearingPlayerPreservesTheExistingSimulation)
{
	HostScope scope;
	MWeather weather;
	weather.SetSpot(1);
	const int count = weather[0].GetCount();
	environment.player = false;
	weather.SetRain(4);
	weather.SetSnow(4);
	CHECK_EQ(MWeather::WEATHER_SPOT, weather.GetWeatherType());
	CHECK_EQ(1, weather.GetSize());
	CHECK_EQ(count, weather[0].GetCount());
	CHECK_EQ(120, weather.GetStartX());
	CHECK_EQ(240, weather.GetStartY());
}

TEST(Weather, RespawnUsesLiveViewportDimensionsAndTheWeatherSpriteRange)
{
	HostScope scope;
	WeatherProbe weather;
	for (BYTE kind : {MWeather::WEATHER_RAIN, MWeather::WEATHER_SNOW, MWeather::WEATHER_SPOT})
	{
		const BYTE terminal = kind == MWeather::WEATHER_RAIN
			? MAP_EFFECT::MAP_EFFECT_ARRIVE4 : MAP_EFFECT::MAP_EFFECT_ARRIVE6;
		for (int size : {1, 37, 123})
		{
			environment.width = size;
			environment.height = size + 1;
			weather.Prepare(1, terminal);
			const int widthReads = environment.widthReads;
			const int heightReads = environment.heightReads;
			weather.Generate(kind, 0);
			const auto& effect = weather[0];
			CHECK_EQ(MAP_EFFECT::MAP_EFFECT_FALL, effect.GetType());
			CHECK(effect.GetX() >= 0 && effect.GetX() < size);
			CHECK(effect.GetY() >= 0 && effect.GetY() < size + 1);
			CHECK_EQ(widthReads + 1, environment.widthReads);
			CHECK_EQ(heightReads + 1, environment.heightReads);
			CHECK_EQ(effect.GetCount(), effect.GetMaxCount());
			if (kind == MWeather::WEATHER_RAIN)
			{
				CHECK(effect.GetSpriteID() <= 2);
				CHECK(effect.GetCount() >= 0 && effect.GetCount() < 25);
			}
			else if (kind == MWeather::WEATHER_SNOW)
			{
				CHECK(effect.GetSpriteID() >= 7 && effect.GetSpriteID() <= 11);
				CHECK(effect.GetCount() >= 0 && effect.GetCount() < 120);
			}
			else
			{
				CHECK(effect.GetSpriteID() >= 18 && effect.GetSpriteID() <= 23);
				CHECK(effect.GetCount() >= 30 && effect.GetCount() < 60);
			}
		}
	}
}

TEST(Weather, MissingAndInvalidViewportDimensionsUseTheDefaultSize)
{
	const MWeatherHost originOnly = {.ReadOrigin = ReadOrigin};
	for (const MWeatherHost* installed : {&originOnly, &host})
	{
		HostScope scope(installed);
		environment.width = 0;
		environment.height = -5;
		MWeather weather;
		weather.SetSpot(1);
		CHECK(weather[0].GetX() >= 0 && weather[0].GetX() < 800);
		CHECK(weather[0].GetY() >= 0 && weather[0].GetY() < 600);
	}
}

TEST(Weather, RainTraversesFourStationarySplashFrames)
{
	HostScope scope;
	WeatherProbe weather;
	weather.Prepare(1, MAP_EFFECT::MAP_EFFECT_FALL);
	for (int stage = 1; stage <= 4; ++stage)
	{
		weather.Generate(MWeather::WEATHER_RAIN, 0);
		const auto& effect = weather[0];
		CHECK_EQ(stage, effect.GetType());
		CHECK_EQ(stage + 2, effect.GetSpriteID());
		CHECK_EQ(1, effect.GetCount());
		CHECK(weather.Particle(0).Move());
		CheckPosition(effect, 12, 34);
	}
}

TEST(Weather, SnowTraversesSixStationaryLandingFrames)
{
	HostScope scope;
	WeatherProbe weather;
	weather.Prepare(1, MAP_EFFECT::MAP_EFFECT_FALL);
	for (int stage = 1; stage <= 6; ++stage)
	{
		weather.Generate(MWeather::WEATHER_SNOW, 0);
		const auto& effect = weather[0];
		CHECK_EQ(stage, effect.GetType());
		CHECK_EQ(stage + 11, effect.GetSpriteID());
		CHECK(effect.GetCount() >= 2 && effect.GetCount() <= 3);
		while (weather.Particle(0).Move()) {}
		CheckPosition(effect, 12, 34);
	}
}

TEST(Weather, NewSpotsRampInBatchesOfFiveWithTheExistingDelays)
{
	HostScope scope;
	MWeather weather;
	weather.SetSpot(12);
	CHECK_EQ(1, weather.GetSize());
	weather.Action();
	weather.Action();
	CHECK_EQ(1, weather.GetSize());
	weather.Action();
	CHECK_EQ(6, weather.GetSize());
	for (int frame = 0; frame < 10; ++frame) weather.Action();
	CHECK_EQ(6, weather.GetSize());
	weather.Action();
	CHECK_EQ(11, weather.GetSize());
	for (int frame = 0; frame < 11; ++frame) weather.Action();
	CHECK_EQ(12, weather.GetSize());
	for (int frame = 0; frame < 100; ++frame) weather.Action();
	CHECK_EQ(12, weather.GetSize());
}

TEST(Weather, StoppingLetsActiveParticlesFinishWithoutRespawning)
{
	HostScope scope;
	WeatherProbe weather;
	weather.SetSpot(4);
	for (int frame = 0; frame < 3; ++frame) weather.Action();
	CHECK_EQ(4, weather.GetSize());
	for (BYTE i = 0; i < 4; ++i)
		weather.Particle(i).Set(MAP_EFFECT::MAP_EFFECT_FALL, 18, 100, 100, 1, 2, i + 1);
	weather.Stop();
	for (int frame = 0; frame < 4; ++frame) weather.Action();
	for (BYTE i = 0; i < 4; ++i)
	{
		CHECK(!weather[i].IsActive());
		CheckPosition(weather[i], 101 + i, 102 + 2 * i);
	}
	weather.Action();
	CHECK(!weather.IsActive());
}

TEST(Weather, ReleaseEmptiesTheSimulationAndAllowsAnotherStart)
{
	HostScope scope;
	MWeather weather;
	weather.SetSpot(4);
	weather.Release();
	CHECK(!weather.IsActive());
	CHECK_EQ(0, weather.GetSize());
	weather.Release();
	weather.Action();
	weather.SetSpot(1);
	CHECK_EQ(MWeather::WEATHER_SPOT, weather.GetWeatherType());
	CHECK_EQ(1, weather.GetSize());
}

TEST(Weather, ZeroCountsDoNotChangeTheCurrentWeather)
{
	HostScope scope;
	MWeather weather;
	weather.SetSpot(1);
	const int count = weather[0].GetCount();
	for (BYTE kind : {MWeather::WEATHER_RAIN, MWeather::WEATHER_SNOW, MWeather::WEATHER_SPOT})
	{
		Start(weather, kind, 0);
		CHECK_EQ(MWeather::WEATHER_SPOT, weather.GetWeatherType());
		CHECK_EQ(1, weather.GetSize());
		CHECK_EQ(count, weather[0].GetCount());
	}
}
