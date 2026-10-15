#include "test_framework.h"
#include "MGuildMarkManager.h"
#include "MGuildInfoMapper.h"
#include "SpriteLib/SpriteLibBackend.h"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// The guild, grade and level marks (task 4.230, VS_UI's since then).
// Grade and level marks come from their packs, two to a mark (large,
// then small). A guild's two marks come from the guild mark pack at the
// sprite ID the guild info mapper gives it, loaded once and cached, a
// failure as well.
namespace {

// A pack of sprites first, first + 1, ... pixels wide, so a sprite shows
// which pack and sprite it is.
struct Packs
{
	std::filesystem::path dir;
	Packs()
	{
		static unsigned sequence = 0;
		dir = std::filesystem::temp_directory_path() / ("darkeden_guild_marks_"
			+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
			+ "_" + std::to_string(sequence++));
		std::filesystem::create_directories(dir);
	}
	~Packs() { std::error_code e; std::filesystem::remove_all(dir, e); }
	// indexCount: the count the index claims; bad: that sprite's row runs
	// past its width (rejected, with the file still readable after it);
	// swap: index entries swapped pairwise from that entry on (the
	// sprites stay in order in the pack).
	std::string Write(const char* name, int count, int first = 1, int indexCount = -1,
		int bad = -1, int swap = -1)
	{
		const std::string spk = (dir / name).string();
		std::ofstream data(spk, std::ios::binary);
		std::ofstream index(spk + "i", std::ios::binary);
		auto put16 = [](std::ofstream& out, std::uint16_t v) { out.put(char(v & 0xFF)); out.put(char(v >> 8)); };
		put16(data, std::uint16_t(count));
		put16(index, std::uint16_t(indexCount < 0 ? count : indexCount));
		std::vector<std::int32_t> offsets;
		for (int i = 0; i < count; ++i)
		{
			offsets.push_back(static_cast<std::int32_t>(data.tellp()));
			const std::uint16_t width = std::uint16_t(first + i);
			const std::uint16_t run = std::uint16_t(i == bad ? width + 5 : width);
			std::vector<std::uint16_t> words = {width, 1, std::uint16_t(3 + run), 1, 0, run};
			for (int x = 0; x < run; ++x) words.push_back(0xFFFF);
			for (std::uint16_t w : words) put16(data, w);
		}
		if (swap >= 0)
			for (int i = swap; i + 1 < count; i += 2) std::swap(offsets[i], offsets[i + 1]);
		for (std::int32_t offset : offsets) index.write(reinterpret_cast<const char*>(&offset), 4);
		return spk;
	}
};

int Width(CSprite* pSprite) { return pSprite != nullptr ? pSprite->GetWidth() : -1; }

struct Scene
{
	Packs packs;
	MGuildInfoMapper mapper;
	MGuildInfoMapper* oldMapper = g_pGuildInfoMapper;
	std::string guilds, slayer, vampire, ousters, levels;
	Scene()
	{
		spritectl_init();
		guilds = packs.Write("guild.spk", 6);			// widths 1-6
		slayer = packs.Write("slayer.spk", 4, 20);		// 20-23
		vampire = packs.Write("vampire.spk", 6, 30);	// 30-35
		ousters = packs.Write("ousters.spk", 2, 40);	// 40-41
		levels = packs.Write("level.spk", 8, 50);		// 50-57
		g_pGuildInfoMapper = &mapper;
		Guild(7, 2);
		Guild(8, 5);				// the last sprite: no small one after it
	}
	~Scene() { g_pGuildInfoMapper = oldMapper; }
	void Guild(WORD id, TYPE_SPRITEID sprite)
	{
		auto* info = new GUILD_INFO;
		info->SetSpriteID(sprite);
		mapper.Set(id, info);
	}
	MGuildMarkManager* Manager(const char* guildPack = nullptr)
	{
		return new MGuildMarkManager(guildPack ? guildPack : guilds.c_str(), "mapper.inf",
			slayer.c_str(), vampire.c_str(), ousters.c_str(), levels.c_str());
	}
};

} // namespace

TEST(GuildMarkManager, GradeAndLevelMarksComeTwoToAMark)
{
	Scene scene;
	std::unique_ptr<MGuildMarkManager> marks(scene.Manager());
	CHECK_EQ(20, Width(marks->GetGradeMark(0, RACE_SLAYER)));
	CHECK_EQ(21, Width(marks->GetGradeMarkSmall(0, RACE_SLAYER)));
	CHECK_EQ(22, Width(marks->GetGradeMark(1, RACE_SLAYER)));
	CHECK_EQ(23, Width(marks->GetGradeMarkSmall(1, RACE_SLAYER)));
	CHECK_EQ(34, Width(marks->GetGradeMark(2, RACE_VAMPIRE)));
	CHECK_EQ(35, Width(marks->GetGradeMarkSmall(2, RACE_VAMPIRE)));
	CHECK_EQ(40, Width(marks->GetGradeMark(0, RACE_OUSTERS)));
	CHECK_EQ(41, Width(marks->GetGradeMarkSmall(0, RACE_OUSTERS)));
	CHECK(marks->GetGradeMark(0, RACE_MAX) == nullptr);
	CHECK(marks->GetGradeMarkSmall(0, RACE_MAX) == nullptr);
	CHECK_EQ(56, Width(marks->GetLevelMark(3)));
	CHECK_EQ(57, Width(marks->GetLevelMarkSmall(3)));
}

TEST(GuildMarkManager, AGuildsMarksLoadOnceFromItsSpriteID)
{
	Scene scene;
	std::unique_ptr<MGuildMarkManager> marks(scene.Manager());
	CHECK(marks->IsInit());
	CHECK(!marks->HasGuildMark(7));
	CHECK(marks->GetGuildMark(7) == nullptr);
	CHECK(marks->LoadGuildMark(7));
	CHECK(marks->HasGuildMark(7));
	CHECK_EQ(3, Width(marks->GetGuildMark(7)));			// sprite 2
	CHECK_EQ(4, Width(marks->GetGuildMarkSmall(7)));	// sprite 3
	// Cached: the pack is not read again.
	CSprite* pLarge = marks->GetGuildMark(7);
	std::filesystem::remove(scene.guilds);
	CHECK(marks->LoadGuildMark(7));
	CHECK(marks->GetGuildMark(7) == pLarge);
	marks->Release();
	CHECK(!marks->HasGuildMark(7));
	CHECK(!marks->LoadGuildMark(7));					// gone from disk now
}

TEST(GuildMarkManager, AFailedLoadIsCachedEmpty)
{
	Scene scene;
	std::unique_ptr<MGuildMarkManager> marks(scene.Manager());
	CHECK(!marks->LoadGuildMark(9));					// not in the mapper
	CHECK(marks->HasGuildMark(9));
	CHECK(marks->GetGuildMark(9) == nullptr);
	CHECK(marks->GetGuildMarkSmall(9) == nullptr);
	scene.Guild(9, 0);
	CHECK(!marks->LoadGuildMark(9));					// still the cached failure
	CHECK(!marks->LoadGuildMark(8));					// the pack's last sprite
	CHECK(marks->GetGuildMark(8) == nullptr);
	scene.Guild(10, 6);
	CHECK(!marks->LoadGuildMark(10));					// past the pack
	scene.Guild(11, 4);
	CHECK(marks->LoadGuildMark(11));					// sprites 4 and 5
	CHECK_EQ(6, Width(marks->GetGuildMarkSmall(11)));
}

TEST(GuildMarkManager, NoMapperOrPackNoGuildMarks)
{
	Scene scene;
	{
		std::unique_ptr<MGuildMarkManager> marks(scene.Manager());
		g_pGuildInfoMapper = nullptr;
		CHECK(!marks->LoadGuildMark(7));
		g_pGuildInfoMapper = &scene.mapper;
	}
	{
		// An index that disagrees with its pack.
		const std::string bad = scene.packs.Write("bad.spk", 6, 1, 5);
		std::unique_ptr<MGuildMarkManager> marks(scene.Manager(bad.c_str()));
		CHECK(!marks->LoadGuildMark(7));
	}
	{
		const std::string missing = (scene.packs.dir / "missing.spk").string();
		std::unique_ptr<MGuildMarkManager> marks(scene.Manager(missing.c_str()));
		CHECK(!marks->LoadGuildMark(7));
	}
	{
		std::unique_ptr<MGuildMarkManager> marks(scene.Manager(""));
		CHECK(!marks->LoadGuildMark(7));
	}
	{
		// An empty pack: no pair at any sprite ID.
		scene.Guild(12, 0);
		const std::string empty = scene.packs.Write("empty.spk", 0);
		std::unique_ptr<MGuildMarkManager> marks(scene.Manager(empty.c_str()));
		CHECK(!marks->LoadGuildMark(12));
	}
}

TEST(GuildMarkManager, AHalfLoadedPairIsNotPublished)
{
	// Sprite 5, the small one of the pair at 4, is malformed.
	Scene scene;
	scene.Guild(13, 4);
	const std::string badSmall = scene.packs.Write("badsmall.spk", 6, 1, -1, 5);
	std::unique_ptr<MGuildMarkManager> marks(scene.Manager(badSmall.c_str()));
	CHECK(!marks->LoadGuildMark(13));
	CHECK(marks->HasGuildMark(13));
	CHECK(marks->GetGuildMark(13) == nullptr);
	CHECK(marks->GetGuildMarkSmall(13) == nullptr);
	// The pair before it loads.
	CHECK(marks->LoadGuildMark(7));
	CHECK_EQ(3, Width(marks->GetGuildMark(7)));
	// The large one malformed, the small one whole.
	scene.Guild(14, 4);
	const std::string badLarge = scene.packs.Write("badlarge.spk", 6, 1, -1, 4);
	std::unique_ptr<MGuildMarkManager> others(scene.Manager(badLarge.c_str()));
	CHECK(!others->LoadGuildMark(14));
	CHECK(others->GetGuildMark(14) == nullptr);
	CHECK(others->GetGuildMarkSmall(14) == nullptr);
}

TEST(GuildMarkManager, EachMarkIsReadAtItsOwnIndexEntry)
{
	// Entries 2 and 3 swapped: the pair at 2 is sprite 3 large and sprite
	// 2 small, wherever reading the large one leaves the file.
	Scene scene;
	const std::string swapped = scene.packs.Write("swapped.spk", 6, 1, -1, false, 2);
	std::unique_ptr<MGuildMarkManager> marks(scene.Manager(swapped.c_str()));
	CHECK(marks->LoadGuildMark(7));
	CHECK_EQ(4, Width(marks->GetGuildMark(7)));
	CHECK_EQ(3, Width(marks->GetGuildMarkSmall(7)));
}

TEST(GuildMarkManager, ItIsInitWithAPackOrAMapperName)
{
	Scene scene;
	MGuildMarkManager none("", nullptr, "", "", "", "");
	CHECK(!none.IsInit());
	MGuildMarkManager pack("guild.spk", nullptr, "", "", "", "");
	CHECK(pack.IsInit());
	MGuildMarkManager mapper("", "mapper.inf", "", "", "", "");
	CHECK(mapper.IsInit());
}
