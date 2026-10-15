#include "test_framework.h"
#include "MLoadingSPKWorkNode.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

// The loading thread's sprite-pack jobs (task 4.217, SpriteLib's since
// then). Each loads sprites of a pack from its SPK file, opened through
// the host:
// - Node1 reads a file of (sprite, position) pairs, then loads those
//   sprites;
// - Node2 loads by a pair array it owns;
// - Node3 loads one sprite at a position;
// - Node4 loads a run of sprites from a position.
// Node1-3 skip a sprite already loaded; Node4 loads every one, to keep its
// file position. A job stopped before it starts hands its whole work back
// as a new job (Node2 for 1 and 2, Node4 for 4) of the same type. A file
// that does not open fails the job with nothing left.
namespace {

struct Scratch
{
	std::filesystem::path dir;
	Scratch()
	{
		static unsigned int sequence = 0;
		dir = std::filesystem::temp_directory_path() / ("darkeden_spk_jobs_"
			+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
			+ "_" + std::to_string(sequence++));
		std::filesystem::create_directories(dir);
	}
	~Scratch()
	{
		std::error_code error;
		std::filesystem::remove_all(dir, error);
	}
	std::string Path(const char* name) const { return (dir / name).string(); }
};

// The pack file: three 1-row sprites of widths 1, 2 and 3 at 0, 14 and 30.
const long positions[] = {0, 14, 30};
void WriteSpk(const Scratch& scratch)
{
	std::ofstream out(scratch.Path("pack.spk"), std::ios::binary);
	for (WORD width = 1; width <= 3; ++width)
	{
		std::vector<WORD> words = {width, 1, WORD(3 + width), 1, 0, width};
		for (WORD x = 0; x < width; ++x) words.push_back(0xFFFF);
		for (WORD w : words) { out.put(char(w & 0xFF)); out.put(char(w >> 8)); }
	}
}

void WritePairs(const Scratch& scratch, std::vector<std::pair<TYPE_SPRITEID, long>> pairs)
{
	CSpriteFilePositionArray array;
	array.Init(static_cast<TYPE_SPRITEID>(pairs.size()));
	for (size_t i = 0; i < pairs.size(); ++i)
	{
		array[static_cast<TYPE_SPRITEID>(i)].SpriteID = pairs[i].first;
		array[static_cast<TYPE_SPRITEID>(i)].FilePosition = static_cast<int32_t>(pairs[i].second);
	}
	std::ofstream out(scratch.Path("pack.sfpa"), std::ios::binary);
	array.SaveToFile(out);
}

std::vector<std::string> opened;
const MLoadingSPKHost host{
	.OpenBinary = [](const char* filename, std::ifstream& file) {
		opened.push_back(filename);
		if (file.is_open()) file.close();
		file.open(filename, std::ios::binary);
		return file.is_open();
	},
};

struct Fixture
{
	Scratch scratch;
	CSpritePack pack;
	Fixture() { opened.clear(); SetLoadingSPKHost(&host); WriteSpk(scratch); pack.Init(4); }
	~Fixture() { SetLoadingSPKHost(nullptr); }
	int Width(WORD id) { return pack[id].IsInit() ? pack[id].GetWidth() : 0; }
	std::string Spk() const { return scratch.Path("pack.spk"); }
};

} // namespace

TEST(LoadingSPKWorkNode, ANewJobRuns)
{
	MLoadingSPKWorkNode3 job(1, 14);
	CHECK(job.IsExecute());
}

TEST(LoadingSPKWorkNode, WithoutAHostNoFileOpens)
{
	Fixture fixture;
	SetLoadingSPKHost(nullptr);
	MLoadingSPKWorkNode3 job(1, 14);
	job.SetSPK(&fixture.pack, fixture.Spk().c_str());
	MWorkNode* remain = &job;
	CHECK(!job.Execute(remain));
	CHECK(remain == nullptr);
	CHECK(!job.IsExecute());
	CHECK_EQ(0, fixture.Width(1));
}

TEST(LoadingSPKWorkNode, Node1LoadsByItsPairFile)
{
	Fixture fixture;
	WritePairs(fixture.scratch, {{2, 30}, {0, 0}});
	MLoadingSPKWorkNode1 job(fixture.scratch.Path("pack.sfpa").c_str(), 0);
	job.SetSPK(&fixture.pack, fixture.Spk().c_str());
	MWorkNode* remain = &job;
	CHECK(job.Execute(remain));
	CHECK(remain == nullptr);
	CHECK(!job.IsExecute());
	CHECK_EQ(1, fixture.Width(0));
	CHECK_EQ(0, fixture.Width(1));
	CHECK_EQ(3, fixture.Width(2));
	CHECK(opened.size() == 2 && opened[0] == fixture.scratch.Path("pack.sfpa") && opened[1] == fixture.Spk());
}

TEST(LoadingSPKWorkNode, ALoadedSpriteIsSkipped)
{
	Fixture fixture;
	// Entry 0 already holds the 2-wide sprite.
	MLoadingSPKWorkNode3 first(0, 14);
	first.SetSPK(&fixture.pack, fixture.Spk().c_str());
	MWorkNode* remain = &first;
	CHECK(first.Execute(remain));
	CHECK_EQ(2, fixture.Width(0));

	WritePairs(fixture.scratch, {{0, 0}, {1, 30}});
	MLoadingSPKWorkNode1 job(fixture.scratch.Path("pack.sfpa").c_str(), 0);
	job.SetSPK(&fixture.pack, fixture.Spk().c_str());
	CHECK(job.Execute(remain));
	CHECK_EQ(2, fixture.Width(0));
	CHECK_EQ(3, fixture.Width(1));

	// Node3 on a loaded sprite does not even open the file.
	opened.clear();
	MLoadingSPKWorkNode3 again(0, 0);
	again.SetSPK(&fixture.pack, fixture.Spk().c_str());
	CHECK(again.Execute(remain));
	CHECK(remain == nullptr);
	CHECK(opened.empty());
	CHECK_EQ(2, fixture.Width(0));
}

TEST(LoadingSPKWorkNode, AMissingFileFailsTheJob)
{
	Fixture fixture;
	MLoadingSPKWorkNode1 noPairs(fixture.scratch.Path("missing.sfpa").c_str(), 0);
	noPairs.SetSPK(&fixture.pack, fixture.Spk().c_str());
	MWorkNode* remain = &noPairs;
	CHECK(!noPairs.Execute(remain));
	CHECK(remain == nullptr);
	CHECK(!noPairs.IsExecute());

	WritePairs(fixture.scratch, {{1, 14}});
	const std::string missing = fixture.scratch.Path("missing.spk");
	MLoadingSPKWorkNode1 noPack(fixture.scratch.Path("pack.sfpa").c_str(), 0);
	noPack.SetSPK(&fixture.pack, missing.c_str());
	remain = &noPack;
	CHECK(!noPack.Execute(remain));
	CHECK(remain == nullptr);

	MLoadingSPKWorkNode4 run(0, 3, 0);
	run.SetSPK(&fixture.pack, missing.c_str());
	remain = &run;
	CHECK(!run.Execute(remain));
	CHECK(remain == nullptr);
	CHECK_EQ(0, fixture.Width(0));
}

TEST(LoadingSPKWorkNode, AStoppedPairJobHandsBackItsWork)
{
	Fixture fixture;
	WritePairs(fixture.scratch, {{2, 30}, {0, 0}});
	MLoadingSPKWorkNode1 job(fixture.scratch.Path("pack.sfpa").c_str(), 0);
	job.SetSPK(&fixture.pack, fixture.Spk().c_str());
	job.SetType(7);
	job.Stop();
	MWorkNode* remain = nullptr;
	CHECK(!job.Execute(remain));
	std::unique_ptr<MWorkNode> rest(remain);
	auto* node2 = dynamic_cast<MLoadingSPKWorkNode2*>(remain);
	CHECK(node2 != nullptr);
	CHECK_EQ(0, fixture.Width(2));
	if (!node2) return;
	CHECK_EQ(7, node2->GetType());
	CHECK(node2->IsExecute());

	// The rest runs as a job of its own, and stopped again hands back the
	// same work.
	node2->Stop();
	MWorkNode* again = nullptr;
	CHECK(!node2->Execute(again));
	std::unique_ptr<MWorkNode> restAgain(again);
	auto* next = dynamic_cast<MLoadingSPKWorkNode2*>(again);
	CHECK(next != nullptr);
	if (!next) return;
	CHECK_EQ(7, next->GetType());
	MWorkNode* done = next;
	CHECK(next->Execute(done));
	CHECK(done == nullptr);
	CHECK_EQ(3, fixture.Width(2));
	CHECK_EQ(1, fixture.Width(0));
}

TEST(LoadingSPKWorkNode, Node4LoadsARunAndHandsBackItsPlace)
{
	Fixture fixture;
	MLoadingSPKWorkNode4 run(1, 3, 14);
	run.SetSPK(&fixture.pack, fixture.Spk().c_str());
	run.SetType(3);
	run.Stop();
	MWorkNode* remain = nullptr;
	CHECK(!run.Execute(remain));
	std::unique_ptr<MWorkNode> rest(remain);
	auto* node4 = dynamic_cast<MLoadingSPKWorkNode4*>(remain);
	CHECK(node4 != nullptr);
	CHECK_EQ(0, fixture.Width(1));
	if (!node4) return;
	CHECK_EQ(3, node4->GetType());
	MWorkNode* done = node4;
	CHECK(node4->Execute(done));
	CHECK(done == nullptr);
	// Entries 1 and 2 from position 14: the 2- and 3-wide sprites.
	CHECK_EQ(0, fixture.Width(0));
	CHECK_EQ(2, fixture.Width(1));
	CHECK_EQ(3, fixture.Width(2));
	CHECK_EQ(0, fixture.Width(3));

	// A run stops before its last sprite ID, though the file goes on.
	CSpritePack fresh;
	fresh.Init(4);
	MLoadingSPKWorkNode4 firstTwo(0, 2, 0);
	firstTwo.SetSPK(&fresh, fixture.Spk().c_str());
	CHECK(firstTwo.Execute(done));
	CHECK(fresh[0].IsInit() && fresh[1].IsInit());
	CHECK(!fresh[2].IsInit());

	// Node4 reloads even a loaded entry, to keep its place in the file.
	MLoadingSPKWorkNode4 over(1, 2, 30);
	over.SetSPK(&fixture.pack, fixture.Spk().c_str());
	CHECK(over.Execute(done));
	CHECK_EQ(3, fixture.Width(1));
}
