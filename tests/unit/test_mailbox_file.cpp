#include "test_framework.h"
#include "VS_UI_GameCommon2.h"
#include "VS_UI_GlobalResource.h"
#include "MString.h"
#include "pi_core.h"
#include "u_window.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

// The mailbox's file: a flag, a tab count, then each tab's mail count and
// mails. The box has three tabs; a count past them, or a mail count past
// the end of the file, loads what the file holds. What the box holds shows
// in what it saves: the flag, the two tabs it keeps (mail and help), each
// with its count and mails.
namespace {

const char* kPath = "mailbox_file_test.bin";

std::string Int(unsigned value)
{
	char bytes[4];
	std::memcpy(bytes, &value, 4);
	return std::string(bytes, 4);
}

// A mail: its id, mark, sender, title, contents, date and window size.
std::string Mail(unsigned id)
{
	{
		std::ofstream out(kPath, std::ios::binary | std::ios::trunc);
		for (int i = 0; i < 4; i++) MString().SaveToFile(out);
	}
	std::ifstream in(kPath, std::ios::binary);
	const std::string strings((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	return Int(id) + std::string(1, '\0') + strings + std::string(sizeof(SIZE), '\0');
}

// A window needs the UI's process runner and window manager, which the
// UI's Init makes; made for the test where the UI has none.
struct WindowScope
{
	bool ownRunner = gpC_process_runner == NULL;
	bool ownManager = gpC_window_manager == NULL;
	WindowScope()
	{
		if (ownRunner) gpC_process_runner = new PI_ProcessRunner;
		if (ownManager) gpC_window_manager = new WindowManager;
	}
	~WindowScope()
	{
		if (ownManager) { delete gpC_window_manager; gpC_window_manager = NULL; }
		if (ownRunner) { delete gpC_process_runner; gpC_process_runner = NULL; }
	}
};

// The box loaded from these bytes, as it saves itself.
std::string LoadedAndSaved(const std::string& bytes)
{
	WindowScope windows;
	C_VS_UI_MAILBOX box;
	{
		std::ofstream out(kPath, std::ios::binary | std::ios::trunc);
		out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	}
	{
		std::ifstream in(kPath, std::ios::binary);
		box.LoadFromFile(in);
	}
	{
		std::ofstream out(kPath, std::ios::binary | std::ios::trunc);
		box.SaveToFile(out);
	}
	std::ifstream in(kPath, std::ios::binary);
	const std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	in.close();
	std::remove(kPath);
	return saved;
}

unsigned CountAt(const std::string& saved, size_t at)
{
	unsigned count = 0;
	if (saved.size() >= at + 4) std::memcpy(&count, saved.data() + at, 4);
	return count;
}

} // namespace

TEST(MailboxFile, AMailCountPastTheFileKeepsTheMailItHolds)
{
	// The mail tab says a thousand and holds one.
	const std::string saved = LoadedAndSaved(Int(0) + std::string(1, '\x01') + Int(1000) + Mail(7));
	// Flag, tab count, then the mail tab's count.
	CHECK_EQ(1u, CountAt(saved, 5));
	CHECK_EQ(5 + 4 + Mail(7).size() + 4, saved.size());
}

TEST(MailboxFile, ATabCountPastTheBoxReadsItsThreeTabs)
{
	// Five tabs, each with one mail: the fourth and fifth have no place
	// in the box.
	std::string bytes = Int(0) + std::string(1, '\x05');
	for (unsigned tab = 0; tab < 5; tab++) bytes += Int(1) + Mail(10 + tab);
	const std::string saved = LoadedAndSaved(bytes);
	CHECK_EQ(1u, CountAt(saved, 5));
	CHECK_EQ(1u, CountAt(saved, 5 + 4 + Mail(10).size()));
}

TEST(MailboxFile, AnEmptyFileLoadsNoMail)
{
	const std::string saved = LoadedAndSaved("");
	CHECK_EQ(0u, CountAt(saved, 5));
	CHECK_EQ(0u, CountAt(saved, 9));
}

// A mail's date: the DWORD the server sends, over a hundred, is YYMMDD, and
// the box stores it as "YY/MM/DD". GCUnionOfferList's date and
// GCNoticeEvent's parameter (through UI_RunNotice) both reach AddMail.
namespace {

// The date the box stores for a new mail of this date, as it saves it, or
// "threw" when AddMail let an exception out: nothing between a packet
// handler and the UI catches a std::exception.
std::string DateStored(DWORD date)
{
	WindowScope windows;
	C_GLOBAL_RESOURCE* oldResource = gpC_global_resource;
	std::string saved;
	bool threw = false;
	{
		C_GLOBAL_RESOURCE resource;	// Start's close button
		gpC_global_resource = &resource;
		C_VS_UI_MAILBOX box;
		const SIZE size = {0, 0};
		try
		{
			box.AddMail(C_VS_UI_MAILBOX::TAB_MAIL_ID, 7, size, "s", "t", date, false);
		}
		catch (const std::exception&)
		{
			threw = true;
		}
		if (box.Running()) box.Finish();
		{
			std::ofstream out(kPath, std::ios::binary | std::ios::trunc);
			box.SaveToFile(out);
		}
		std::ifstream in(kPath, std::ios::binary);
		saved.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}
	std::remove(kPath);
	gpC_global_resource = oldResource;
	if (threw) return "threw";
	// Flag, tab count, mail count, then the mail's id and mark; its
	// sender, title and contents come before the date.
	size_t at = 4 + 1 + 4 + 4 + 1;
	for (int i = 0; i < 3; i++) at += 4 + CountAt(saved, at);
	return saved.substr(at + 4, CountAt(saved, at));
}

} // namespace

TEST(MailboxDate, ADateUnderSixDigitsIsZeroPaddedNotRefused)
{
	// Under 10000 the year, month and day are all zero: the date was two
	// or three characters, and the second '/' went in past its end.
	CHECK(DateStored(0) == "00/00/00");
	CHECK(DateStored(999) == "00/00/09");
	CHECK(DateStored(9999) == "00/00/99");
	// The year 2000 is four digits; it was stored as "01/20/5".
	CHECK(DateStored(120512) == "00/12/05");
}

TEST(MailboxDate, ASixDigitDateIsStoredAsBefore)
{
	CHECK(DateStored(3120512) == "03/12/05");
	CHECK(DateStored(23120512) == "23/12/05");
	CHECK(DateStored(4294967295u) == "42/94/9672");
}
