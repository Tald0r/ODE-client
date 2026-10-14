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
		std::error_code error;
		std::filesystem::remove_all(dir, error);	// what a crashed run left
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
}

TEST(DebugKit, EachFlagIsOnAboveZeroAndReadsItsOwnKey)
{
	ScratchDir scratch;
	// Each flag sees 1, 0 and -1 across the three files, in a different
	// order from the others.
	const char* files[] = {
		"MsgOutPutFlag : 1\nMsgDetailFlag : 0\nMsgContentFlag : -1\n",
		"MsgOutPutFlag : -1\nMsgDetailFlag : 1\nMsgContentFlag : 0\n",
		"MsgOutPutFlag : 0\nMsgDetailFlag : -1\nMsgContentFlag : 1\n",
	};
	for (int i = 0; i < 3; i++)
	{
		scratch.Write("DebugInfo.ini", files[i]);
		CDebugKit kit;
		CHECK(kit.load());
		CHECK(kit.GetMsgOutPutFlag() == (i == 0 ? TRUE : FALSE));
		CHECK(kit.GetMsgDetailFlag() == (i == 1 ? TRUE : FALSE));
		CHECK(kit.GetMsgContentFlag() == (i == 2 ? TRUE : FALSE));
	}
}

TEST(DebugKit, AMalformedFileDoesNotLoad)
{
	ScratchDir scratch;
	scratch.Write("DebugInfo.ini", "MsgOutPutFlag 1\n");
	CDebugKit kit;
	CHECK(!kit.load());
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
	scratch.Write("messages.txt", "CGConnect, 1\n  GCUpdateInfo,2\n\n   \nLCLogin\n\tCGMove,3\nCGSay, 4");
	CMessageStringTable table;
	table.LoadFromFile("messages.txt");
	CHECK(std::string(table.GetMessageName(0)) == "CGConnect");
	CHECK(std::string(table.GetMessageName(1)) == "GCUpdateInfo");
	CHECK(std::string(table.GetMessageName(2)) == "LCLogin");
	CHECK(std::string(table.GetMessageName(3)) == "CGMove");
	CHECK(!table.IsExistMessageName(4));
	// No file: nothing, and no throw.
	CMessageStringTable none;
	none.LoadFromFile("missing.txt");
	CHECK(!none.IsExistMessageName(0));
	none.SaveToFile("out.txt");
	CHECK(!std::filesystem::exists("out.txt"));
}
