#include "test_framework.h"
#include "AppendPatchInfo.h"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// The append patch (task 4.212): a sprite pack (a 2-byte count, then the
// data) and the packs to append to it in turn. Each node starts writing
// where the file before it ended and adds its own count; ExecutePatch
// copies each append file after its 2-byte count to that position, writes
// the new count, and deletes the append file. A missing append file is
// skipped; a missing original fails. The table saves and loads in the
// AppendPatch.inf layout, and the progress bar is the host's.
namespace {

using Bytes = std::vector<unsigned char>;

struct Scratch
{
	std::filesystem::path dir;
	// Unique per run and per test, so two test processes cannot share it.
	Scratch()
	{
		static unsigned int sequence = 0;
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		dir = std::filesystem::temp_directory_path() / ("darkeden_append_patch_"
			+ std::to_string(stamp) + "_" + std::to_string(sequence++));
		std::error_code error;
		std::filesystem::remove_all(dir, error);
		std::filesystem::create_directories(dir);
	}
	~Scratch()
	{
		std::error_code error;
		std::filesystem::remove_all(dir, error);
	}
	std::string Path(const char* name) const { return (dir / name).string(); }
	void Write(const char* name, const Bytes& bytes) const
	{
		std::ofstream out(dir / name, std::ios::binary | std::ios::trunc);
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
	Bytes Read(const char* name) const
	{
		std::ifstream in(dir / name, std::ios::binary);
		return Bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}
	bool Exists(const char* name) const { return std::filesystem::exists(dir / name); }
};

void Put(Bytes& out, std::uint32_t value, int size)
{
	for (int i = 0; i < size; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}
void PutName(Bytes& out, const std::string& name)
{
	Put(out, static_cast<std::uint32_t>(name.size()), 4);
	out.insert(out.end(), name.begin(), name.end());
}

std::string progress;
const AppendPatchHost recordingHost{
	.UpdateProgress = []() { progress += "step;"; },
	.SetProgressText = [](const char* text) { progress += std::string("text:") + text + ";"; },
};
const std::string patchingText = "text:\xED\x8C\xA8\xEC\xB9\x98 \xED\x99\x94\xEC\x9D\xBC\xEC\x9D\x84 \xEC\xA0\x81\xEC\x9A\xA9\xEC\x8B\x9C\xED\x82\xA4\xEA\xB3\xA0 \xEC\x9E\x88\xEC\x8A\xB5\xEB\x8B\x88\xEB\x8B\xA4.;";

struct Host
{
	Host() { progress.clear(); SetAppendPatchHost(&recordingHost); }
	~Host() { SetAppendPatchHost(nullptr); }
};

} // namespace

TEST(AppendPatch, EachNodeStartsWhereTheFileBeforeItEnded)
{
	AppendPatch patch;
	patch.SetOriginalInfo("org.spk", 1000, 10);
	patch.AddAppendInfo("a1.spk", 302, 3);
	patch.AddAppendInfo("a2.spk", 52, 5);
	CHECK_EQ(2, patch.GetSize());
	CHECK(std::string(patch.GetOriginalFilename()) == "org.spk");
	const auto nodes = patch.GetAppendPatchNode();
	if (nodes.size() != 2) return;
	CHECK(std::string(nodes[0]->appendFilename.GetString()) == "a1.spk");
	CHECK_EQ(302, nodes[0]->appendFilesize);
	CHECK_EQ(3, nodes[0]->appSpkSize);
	CHECK_EQ(1000, nodes[0]->writePosition);
	CHECK_EQ(1300, nodes[0]->afterFilesize);
	CHECK_EQ(13, nodes[0]->afterSpkSize);
	CHECK_EQ(1300, nodes[1]->writePosition);
	CHECK_EQ(1350, nodes[1]->afterFilesize);
	CHECK_EQ(18, nodes[1]->afterSpkSize);
}

TEST(AppendPatch, TheFinalInfoMustAddUp)
{
	AppendPatch patch;
	patch.SetOriginalInfo("org.spk", 1000, 10);
	patch.SetFinalInfo(1000, 10);
	CHECK(patch.CalculateFinalInfo());
	patch.AddAppendInfo("a1.spk", 302, 3);
	patch.AddAppendInfo("a2.spk", 52, 5);
	patch.SetFinalInfo(1350, 18);
	CHECK(patch.CalculateFinalInfo());
	patch.SetFinalInfo(1350, 17);
	CHECK(!patch.CalculateFinalInfo());
	patch.SetFinalInfo(1351, 18);
	CHECK(!patch.CalculateFinalInfo());
}

TEST(AppendPatch, ItSavesAndLoadsTheInfLayout)
{
	Scratch scratch;
	AppendPatch patch;
	patch.SetOriginalInfo("org.spk", 1000, 10);
	patch.AddAppendInfo("a1.spk", 302, 3);
	patch.SetFinalInfo(1300, 13);
	{
		std::ofstream out(scratch.Path("patch.inf"), std::ios::binary);
		patch.SaveToFile(out);
	}
	Bytes want;
	PutName(want, "org.spk");
	Put(want, 1000, 4); Put(want, 10, 2); Put(want, 1300, 4); Put(want, 13, 2);
	Put(want, 1, 4);
	PutName(want, "a1.spk");
	Put(want, 302, 4); Put(want, 3, 2); Put(want, 1000, 4); Put(want, 1300, 4); Put(want, 13, 2);
	CHECK(scratch.Read("patch.inf") == want);

	AppendPatch loaded;
	loaded.AddAppendInfo("stale.spk", 1, 1);
	std::ifstream in(scratch.Path("patch.inf"), std::ios::binary);
	loaded.LoadFromFile(in);
	CHECK(std::string(loaded.GetOriginalFilename()) == "org.spk");
	CHECK_EQ(1, loaded.GetSize());
	CHECK(loaded.CalculateFinalInfo());
	const auto nodes = loaded.GetAppendPatchNode();
	if (nodes.size() != 1) return;
	CHECK(std::string(nodes[0]->appendFilename.GetString()) == "a1.spk");
	CHECK_EQ(302, nodes[0]->appendFilesize);
	CHECK_EQ(1000, nodes[0]->writePosition);
	CHECK_EQ(1300, nodes[0]->afterFilesize);
	CHECK_EQ(13, nodes[0]->afterSpkSize);
}

// A node's sizes and position are 4 bytes in the file, whatever the width
// of long: a node loaded over leftover memory keeps none of it. On LP64
// (Linux, macOS) the loader used to fill only the low half of each long.
TEST(AppendPatch, ANodeLoadsItsFourByteFieldsWhole)
{
	Scratch scratch;
	Bytes bytes;
	PutName(bytes, "a1.spk");
	Put(bytes, 302, 4); Put(bytes, 3, 2); Put(bytes, 1000, 4); Put(bytes, 1300, 4); Put(bytes, 13, 2);
	scratch.Write("node.bin", bytes);

	// What the memory held before: all bits set.
	APPEND_PATCH_NODE node;
	node.appendFilesize = node.writePosition = node.afterFilesize = -1;
	std::ifstream in(scratch.Path("node.bin"), std::ios::binary);
	node.LoadFromFile(in);
	CHECK(std::string(node.appendFilename.GetString()) == "a1.spk");
	CHECK_EQ(302, node.appendFilesize);
	CHECK_EQ(3, node.appSpkSize);
	CHECK_EQ(1000, node.writePosition);
	CHECK_EQ(1300, node.afterFilesize);
	CHECK_EQ(13, node.afterSpkSize);
}

// The 4-byte fields are signed, as long was on Windows: a size the chain
// takes below zero comes back as such.
TEST(AppendPatch, AFourByteFieldKeepsItsSign)
{
	Scratch scratch;
	AppendPatch patch;
	patch.SetOriginalInfo("org.spk", 0, 0);
	patch.AddAppendInfo("a1.spk", 1, 1);	// after: 0 + 1 - 2 = -1
	{
		std::ofstream out(scratch.Path("patch.inf"), std::ios::binary);
		patch.SaveToFile(out);
	}
	AppendPatch loaded;
	std::ifstream in(scratch.Path("patch.inf"), std::ios::binary);
	loaded.LoadFromFile(in);
	const auto nodes = loaded.GetAppendPatchNode();
	if (nodes.size() != 1) return;
	CHECK_EQ(-1, nodes[0]->afterFilesize);
}

TEST(AppendPatch, ReleaseForgetsEverything)
{
	AppendPatch patch;
	patch.SetOriginalInfo("org.spk", 5, 1);
	patch.AddAppendInfo("a1.spk", 4, 1);
	patch.SetFinalInfo(7, 2);
	patch.Release();
	CHECK_EQ(0, patch.GetSize());
	const char* name = patch.GetOriginalFilename();	// null after Release
	CHECK(name == nullptr || *name == '\0');
	// Original and final both zero again.
	CHECK(patch.CalculateFinalInfo());
	patch.SetFinalInfo(0, 1);
	CHECK(!patch.CalculateFinalInfo());
}

TEST(AppendPatch, TheFinalInfoCanBeReadFromAFile)
{
	Scratch scratch;
	scratch.Write("final.spk", {0x02, 0x01, 1, 2, 3, 4, 5, 6, 7, 8});
	AppendPatch patch;
	patch.SetOriginalInfo("org.spk", 10, 0x0102);
	CHECK(patch.SetFinalInfo(scratch.Path("final.spk").c_str()));
	CHECK(patch.CalculateFinalInfo());
	patch.SetOriginalInfo("org.spk", 10, 0x0103);
	CHECK(!patch.CalculateFinalInfo());
	CHECK(!patch.SetFinalInfo(scratch.Path("missing.spk").c_str()));
}

TEST(AppendPatch, ExecutingAppendsRenumbersAndDeletesTheAppendFile)
{
	Scratch scratch;
	Host host;
	scratch.Write("org.spk", {2, 0, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'});
	scratch.Write("add.spk", {3, 0, 'X', 'Y', 'Z'});
	AppendPatch patch;
	patch.SetOriginalInfo(scratch.Path("org.spk").c_str(), 10, 2);
	patch.AddAppendInfo(scratch.Path("add.spk").c_str(), 5, 3);
	CHECK(patch.ExecutePatch());
	CHECK(scratch.Read("org.spk") == Bytes({5, 0, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'X', 'Y', 'Z'}));
	CHECK(!scratch.Exists("add.spk"));
	CHECK(progress == "step;" + patchingText);

	patch.SetFinalInfo(13, 5);
	CHECK(patch.CheckFinalInfo());
	patch.SetFinalInfo(13, 4);
	CHECK(!patch.CheckFinalInfo());
	patch.SetFinalInfo(12, 5);
	CHECK(!patch.CheckFinalInfo());
}

TEST(AppendPatch, AMissingAppendFileIsSkippedAndAMissingOriginalFails)
{
	Scratch scratch;
	Host host;
	scratch.Write("org.spk", {2, 0, 'A', 'B'});
	AppendPatch patch;
	patch.SetOriginalInfo(scratch.Path("org.spk").c_str(), 4, 2);
	patch.AddAppendInfo(scratch.Path("gone.spk").c_str(), 5, 3);
	CHECK(patch.ExecutePatch());
	CHECK(scratch.Read("org.spk") == Bytes({2, 0, 'A', 'B'}));
	CHECK(progress == "step;");

	scratch.Write("add.spk", {3, 0, 'X'});
	AppendPatch orphan;
	orphan.SetOriginalInfo(scratch.Path("missing.spk").c_str(), 4, 2);
	orphan.AddAppendInfo(scratch.Path("add.spk").c_str(), 3, 3);
	CHECK(!orphan.ExecutePatch());
	CHECK(scratch.Exists("add.spk"));
	// Without the original there is nothing to contradict the final info.
	orphan.SetFinalInfo(99, 99);
	CHECK(orphan.CheckFinalInfo());
}

TEST(AppendPatch, ItPatchesWithoutAProgressHost)
{
	Scratch scratch;
	SetAppendPatchHost(nullptr);
	scratch.Write("org.spk", {1, 0, 'A'});
	scratch.Write("add.spk", {1, 0, 'B'});
	AppendPatch patch;
	patch.SetOriginalInfo(scratch.Path("org.spk").c_str(), 3, 1);
	patch.AddAppendInfo(scratch.Path("add.spk").c_str(), 3, 1);
	CHECK(patch.ExecutePatch());
	CHECK(scratch.Read("org.spk") == Bytes({2, 0, 'A', 'B'}));
}

TEST(AppendPatch, TheTableCountsEveryAppend)
{
	AppendPatchTable table;
	table.Init(2);
	AppendPatch* first = table.GetMutable(0);
	AppendPatch* second = table.GetMutable(1);
	CHECK(first != nullptr && second != nullptr);
	if (!first || !second) return;
	first->AddAppendInfo("a.spk", 3, 1);
	first->AddAppendInfo("b.spk", 3, 1);
	second->AddAppendInfo("c.spk", 3, 1);
	CHECK_EQ(3, table.GetAllSize());
}
