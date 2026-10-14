#include "test_framework.h"
#include "DebugKit.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

// The debug kit reads DebugInfo.ini (key : value lines) from the working
// directory: the
// message file's name, three logging flags (on when above 0), the
// authentication key map and the game version, each its default when
// absent; no file, no load. The message-name table loads one name a line
// (up to a comma, past leading blanks; blank lines skipped) and answers
// UNKNOWN_MESSAGE for an ID it lacks.
namespace {

// A scratch directory made the working directory for a test.
struct ScratchDir
{
	std::filesystem::path old = std::filesystem::current_path();
	std::filesystem::path dir = std::filesystem::temp_directory_path() / ("darkeden_debugkit_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
	ScratchDir()
	{
		std::filesystem::create_directories(dir);
		std::filesystem::current_path(dir);
	}
	~ScratchDir()
	{
		std::error_code error;
		std::filesystem::current_path(old, error);
		std::filesystem::remove_all(dir, error);
	}
	void Write(const char* name, const std::string& text) const
	{
		std::ofstream out(dir / name, std::ios::binary | std::ios::trunc);
		out << text;
	}
};

} // namespace

TEST(DebugKit, WithoutItsFileTheKitDoesNotLoad)
{
	ScratchDir scratch;
	CDebugKit kit;
	CHECK(!kit.load());
}

TEST(DebugKit, TheKitReadsItsSettings)
{
	ScratchDir scratch;
	scratch.Write("DebugInfo.ini", "MsgFileName : messages.txt\nMsgOutPutFlag : 1\nMsgDetailFlag : 0\nMsgContentFlag : 2\nAuthKeyMap : 3\nGameVersion : 1207\n");
	CDebugKit kit;
	CHECK(kit.load());
	CHECK(kit.GetMsgFileName() == "messages.txt");
	CHECK(kit.GetMsgOutPutFlag() == TRUE);
	CHECK(kit.GetMsgDetailFlag() == FALSE);
	CHECK(kit.GetMsgContentFlag() == TRUE);
	CHECK_EQ(3, kit.GetAuthKeyMap());
	CHECK_EQ(1207, kit.GetGameVersion());
	// The flags the other way round.
	scratch.Write("DebugInfo.ini", "MsgOutPutFlag : 0\nMsgDetailFlag : 5\nMsgContentFlag : 0\n");
	CDebugKit other;
	CHECK(other.load());
	CHECK(other.GetMsgOutPutFlag() == FALSE && other.GetMsgDetailFlag() == TRUE && other.GetMsgContentFlag() == FALSE);
}

TEST(DebugKit, AbsentSettingsTakeTheirDefaults)
{
	ScratchDir scratch;
	scratch.Write("DebugInfo.ini", "Unrelated : 5\n");
	CDebugKit kit;
	CHECK(kit.load());
	CHECK(kit.GetMsgFileName().empty());
	CHECK(kit.GetMsgOutPutFlag() == FALSE && kit.GetMsgDetailFlag() == FALSE && kit.GetMsgContentFlag() == FALSE);
	CHECK_EQ(0, kit.GetAuthKeyMap());
	CHECK_EQ(0, kit.GetGameVersion());
}

TEST(DebugKit, TheMessageTableNamesWhatItHas)
{
	CMessageStringTable table;
	CHECK(!table.IsExistMessageName(4));
	CHECK(std::string(table.GetMessageName(4)) == "UNKNOWN_MESSAGE");
	std::string name = "GCMove";
	table.SetMessageName(4, name);
	CHECK(table.IsExistMessageName(4));
	CHECK(std::string(table.GetMessageName(4)) == "GCMove");
	table.ClearMessageName();
	CHECK(!table.IsExistMessageName(4));
}

TEST(DebugKit, TheMessageTableLoadsANameALine)
{
	ScratchDir scratch;
	// The last line ends in a newline; a final line without one is not read.
	scratch.Write("messages.txt", "CGConnect, 1\n  GCUpdateInfo,2\n\n   \nLCLogin\nCGMove,3\nCGSay, 4");
	CMessageStringTable table;
	table.LoadFromFile((scratch.dir / "messages.txt").string());
	CHECK(std::string(table.GetMessageName(0)) == "CGConnect");
	CHECK(std::string(table.GetMessageName(1)) == "GCUpdateInfo");
	CHECK(std::string(table.GetMessageName(2)) == "LCLogin");
	CHECK(std::string(table.GetMessageName(3)) == "CGMove");
	CHECK(!table.IsExistMessageName(4));
	// No file: nothing, and no throw.
	CMessageStringTable none;
	none.LoadFromFile((scratch.dir / "missing.txt").string());
	CHECK(!none.IsExistMessageName(0));
	none.SaveToFile((scratch.dir / "out.txt").string());
	CHECK(!std::filesystem::exists(scratch.dir / "out.txt"));
}
