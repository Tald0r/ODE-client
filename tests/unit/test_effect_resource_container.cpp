#include "test_framework.h"
#include "EffectResourceContainer.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

// The effect resource container (task 4.216, framelib's since then): the
// effect frame, sprite and palette packs of a data directory, by blt type.
// Load reads each pack it finds and warns about the rest: it succeeds even
// with none, and once loaded it does not load again. The alpha packs take
// the first of their names that exists. Every getter answers null past its
// pack, and for a blt type that has no such pack. (Every BLT_TYPE value is
// handled; a number outside the enum would be undefined behaviour, which
// Clang's UBSan stops on, so the switches' defaults are not tested.)
namespace {

struct Scratch
{
	std::filesystem::path dir;
	Scratch()
	{
		static unsigned int sequence = 0;
		dir = std::filesystem::temp_directory_path() / ("darkeden_effect_resources_"
			+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
			+ "_" + std::to_string(sequence++));
		std::filesystem::create_directories(dir);
	}
	~Scratch()
	{
		std::error_code error;
		std::filesystem::remove_all(dir, error);
	}
	std::string Base() const { return dir.string() + "/"; }
	std::string Path(const char* name) const { return (dir / name).string(); }
};

// A frame pack of two types: type 1 has one direction of two frames.
void SaveFramePack(const Scratch& scratch, const char* name, TYPE_SPRITEID firstSprite)
{
	CEffectFramePack pack;
	pack.Init(2);
	pack[1].Init(1);
	pack[1][0].Init(2);
	pack[1][0][0].Set(firstSprite, 3, 4, 0, false);
	pack[1][0][1].Set(firstSprite + 1, 5, 6, 0, false);
	std::ofstream data(scratch.Path(name), std::ios::binary);
	std::ofstream index(scratch.Path("index.tmp"), std::ios::binary);
	pack.SaveToFile(data, index);
}

// A pack of two loaded 2x1 sprites.
void SaveSpritePack(const Scratch& scratch, const char* name)
{
	const WORD words[] = {2, 1, 5, 1, 0, 2, 0xFFFF, 0x07E0};
	{
		std::ofstream out(scratch.Path("sprite.bin"), std::ios::binary);
		for (WORD w : words) { out.put(char(w & 0xFF)); out.put(char(w >> 8)); }
	}
	CSpritePack pack;
	pack.Init(2);
	for (WORD i = 0; i < 2; ++i)
	{
		std::ifstream in(scratch.Path("sprite.bin"), std::ios::binary);
		pack[i].LoadFromFile(in);
	}
	std::ofstream data(scratch.Path(name), std::ios::binary);
	std::ofstream index(scratch.Path("index.tmp"), std::ios::binary);
	pack.SaveToFile(data, index);
}

template <typename Pack>
void SavePack(const Scratch& scratch, const char* name, WORD size)
{
	Pack pack;
	pack.Init(size);
	std::ofstream data(scratch.Path(name), std::ios::binary);
	std::ofstream index(scratch.Path("index.tmp"), std::ios::binary);
	pack.SaveToFile(data, index);
}

} // namespace

TEST(EffectResourceContainer, AnEmptyContainerAnswersNull)
{
	EffectResourceContainer resources;
	CHECK(!resources.IsLoaded());
	CHECK(resources.GetEffectFrame(BLT_NORMAL, 0, 0, 0) == nullptr);
	CHECK(resources.GetSprite(BLT_NORMAL, 0) == nullptr);
	CHECK(resources.GetAlphaSprite(0) == nullptr);
	CHECK(resources.GetShadowSprite(0) == nullptr);
	CHECK(resources.GetPalette(BLT_EFFECT, 0) == nullptr);
	CHECK(resources.GetPalette(BLT_SCREEN, 0) == nullptr);
	CHECK(resources.GetPalette(BLT_NORMAL, 0) == nullptr);
}

TEST(EffectResourceContainer, LoadSucceedsWithNothingToLoadAndOnlyOnce)
{
	Scratch scratch;
	EffectResourceContainer resources;
	CHECK(resources.Load(scratch.Base().c_str()));
	CHECK(resources.IsLoaded());
	CHECK_EQ(0u, resources.m_EffectNormalSPK.GetSize());

	// Loaded already: a pack that appears now is not read.
	SaveSpritePack(scratch, "Effect.spk");
	CHECK(resources.Load(scratch.Base().c_str()));
	CHECK_EQ(0u, resources.m_EffectNormalSPK.GetSize());
	// A fresh container reads it.
	EffectResourceContainer fresh;
	CHECK(fresh.Load(scratch.Base().c_str()));
	CHECK_EQ(2u, fresh.m_EffectNormalSPK.GetSize());
}

TEST(EffectResourceContainer, FramesAreFoundByTypeDirectionAndFrame)
{
	Scratch scratch;
	SaveFramePack(scratch, "NormalEffect.efpk", 10);
	SaveFramePack(scratch, "ShadowEffect.efpk", 20);
	SaveFramePack(scratch, "EffectScreen.efpk", 30);
	SaveFramePack(scratch, "EffectAlpha.efpk", 40);		// the alpha pack's third name
	EffectResourceContainer resources;
	CHECK(resources.Load(scratch.Base().c_str()));

	CEffectFrame* frame = resources.GetEffectFrame(BLT_NORMAL, 1, 0, 1);
	CHECK(frame != nullptr);
	if (frame) CHECK_EQ(11, frame->GetSpriteID());
	frame = resources.GetEffectFrame(BLT_SHADOW, 1, 0, 0);
	if (frame) CHECK_EQ(20, frame->GetSpriteID());
	CHECK(frame != nullptr);
	frame = resources.GetEffectFrame(BLT_SCREEN, 1, 0, 0);
	if (frame) CHECK_EQ(30, frame->GetSpriteID());
	CHECK(frame != nullptr);
	frame = resources.GetEffectFrame(BLT_EFFECT, 1, 0, 0);
	if (frame) CHECK_EQ(40, frame->GetSpriteID());
	CHECK(frame != nullptr);

	CHECK(resources.GetEffectFrame(BLT_NORMAL, 2, 0, 0) == nullptr);		// past the types
	CHECK(resources.GetEffectFrame(BLT_NORMAL, 0, 0, 0) == nullptr);		// a type without directions
	CHECK(resources.GetEffectFrame(BLT_NORMAL, 1, 1, 0) == nullptr);		// past the directions
	CHECK(resources.GetEffectFrame(BLT_NORMAL, 1, 0, 2) == nullptr);		// past the frames
}

TEST(EffectResourceContainer, AnAlphaPackTakesItsFirstNameThatExists)
{
	Scratch scratch;
	SaveFramePack(scratch, "Effect.efpk", 50);
	SaveFramePack(scratch, "EffectAlpha.efpk", 60);
	EffectResourceContainer resources;
	resources.Load(scratch.Base().c_str());
	CEffectFrame* frame = resources.GetEffectFrame(BLT_EFFECT, 1, 0, 0);
	CHECK(frame != nullptr);
	if (frame) CHECK_EQ(50, frame->GetSpriteID());
}

TEST(EffectResourceContainer, AnEffectSpritePackLoads)
{
	Scratch scratch;
	SaveSpritePack(scratch, "Effect.spk");
	SavePack<MPalettePack>(scratch, "Effect.ppk", 6);
	SavePack<MPalettePack>(scratch, "EffectAlpha.ppk", 8);	// a later name: not read
	SavePack<MPalettePack>(scratch, "EffectScreen.ppk", 7);
	EffectResourceContainer resources;
	CHECK(resources.Load(scratch.Base().c_str()));
	CHECK_EQ(2u, resources.m_EffectNormalSPK.GetSize());
	CHECK(resources.GetSprite(BLT_NORMAL, 1) == &resources.m_EffectNormalSPK[1]);
	CHECK_EQ(6u, resources.m_EffectAlphaPPK.GetSize());
	CHECK_EQ(7u, resources.m_EffectScreenPPK.GetSize());
}

TEST(EffectResourceContainer, TheAlphaPaletteFallsBackToItsThirdName)
{
	Scratch scratch;
	SavePack<MPalettePack>(scratch, "EffectAlpha.ppk", 8);
	EffectResourceContainer resources;
	CHECK(resources.Load(scratch.Base().c_str()));
	CHECK_EQ(8u, resources.m_EffectAlphaPPK.GetSize());
}

TEST(EffectResourceContainer, SpritesAndPalettesAreFoundByBltTypeWithinTheirPacks)
{
	EffectResourceContainer resources;
	resources.m_EffectNormalSPK.Init(2);
	resources.m_EffectAlphaSPK.Init(3);
	resources.m_EffectShadowSPK.Init(4);
	resources.m_EffectScreenSPK.Init(5);
	resources.m_EffectAlphaPPK.Init(6);
	resources.m_EffectScreenPPK.Init(7);

	CHECK(resources.GetSprite(BLT_NORMAL, 0) == &resources.m_EffectNormalSPK[0]);
	CHECK(resources.GetSprite(BLT_NORMAL, 1) == &resources.m_EffectNormalSPK[1]);
	CHECK(resources.GetSprite(BLT_NORMAL, 2) == nullptr);
	CHECK(resources.GetSprite(BLT_NORMAL, -1) == nullptr);
	CHECK(resources.GetSprite(BLT_SHADOW, 0) == &resources.m_EffectShadowSPK[0]);
	CHECK(resources.GetSprite(BLT_SCREEN, 0) == &resources.m_EffectScreenSPK[0]);
	CHECK(resources.GetAlphaSprite(0) == &resources.m_EffectAlphaSPK[0]);
	CHECK(resources.GetShadowSprite(0) == &resources.m_EffectShadowSPK[0]);
	CHECK(resources.GetSprite(BLT_SHADOW, 3) == &resources.m_EffectShadowSPK[3]);
	CHECK(resources.GetSprite(BLT_SHADOW, 4) == nullptr);
	CHECK(resources.GetSprite(BLT_SHADOW, -1) == nullptr);
	CHECK(resources.GetSprite(BLT_SCREEN, 4) == &resources.m_EffectScreenSPK[4]);
	CHECK(resources.GetSprite(BLT_SCREEN, 5) == nullptr);
	CHECK(resources.GetSprite(BLT_SCREEN, -1) == nullptr);
	CHECK(resources.GetSprite(BLT_EFFECT, 0) == nullptr);		// alpha sprites have their own getter

	CHECK(resources.GetAlphaSprite(2) == &resources.m_EffectAlphaSPK[2]);
	CHECK(resources.GetAlphaSprite(3) == nullptr);
	CHECK(resources.GetAlphaSprite(-1) == nullptr);
	CHECK(resources.GetShadowSprite(3) == &resources.m_EffectShadowSPK[3]);
	CHECK(resources.GetShadowSprite(4) == nullptr);
	CHECK(resources.GetShadowSprite(-1) == nullptr);

	CHECK(resources.GetPalette(BLT_EFFECT, 5) == &resources.m_EffectAlphaPPK[5]);
	CHECK(resources.GetPalette(BLT_EFFECT, 6) == nullptr);
	CHECK(resources.GetPalette(BLT_SCREEN, 6) == &resources.m_EffectScreenPPK[6]);
	CHECK(resources.GetPalette(BLT_SCREEN, 7) == nullptr);
	CHECK(resources.GetPalette(BLT_NORMAL, 0) == nullptr);
	CHECK(resources.GetPalette(BLT_SHADOW, 0) == nullptr);
}
