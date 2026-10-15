#include "test_framework.h"
#include "Netmarble.h"
#include <string>

// The Netmarble launch check (task 4.220). Without an argument after a
// space, or with one of 4096 bytes or more, it is no Netmarble launch:
// false. Otherwise the launch is refused through the host (the game shows
// a message and exits), and without one simply false.
namespace {

int refused = 0;
const NetmarbleHost host{ .RefuseLaunch = []() { ++refused; } };

struct Fixture
{
	Fixture() { refused = 0; SetNetmarbleHost(&host); }
	~Fixture() { SetNetmarbleHost(nullptr); }
};

} // namespace

TEST(Netmarble, NoArgumentIsNoNetmarbleLaunch)
{
	Fixture fixture;
	CHECK(!ParsingNetmarble(""));
	CHECK(!ParsingNetmarble("DarkEden.exe"));
	CHECK_EQ(0, refused);
}

TEST(Netmarble, AnArgumentIsRefused)
{
	Fixture fixture;
	CHECK(!ParsingNetmarble("DarkEden.exe 2,20,id,key"));
	CHECK_EQ(1, refused);
	// Even an empty one after the space.
	CHECK(!ParsingNetmarble("DarkEden.exe "));
	CHECK_EQ(2, refused);
}

TEST(Netmarble, AnArgumentOf4096BytesIsNotOne)
{
	Fixture fixture;
	const std::string longest = "DarkEden.exe " + std::string(4095, 'a');
	CHECK(!ParsingNetmarble(longest.c_str()));
	CHECK_EQ(1, refused);
	const std::string tooLong = "DarkEden.exe " + std::string(4096, 'a');
	CHECK(!ParsingNetmarble(tooLong.c_str()));
	CHECK_EQ(1, refused);
}

TEST(Netmarble, TheArgumentStartsAtTheFirstSpace)
{
	Fixture fixture;
	// 4096 bytes after the first space, though few after the last.
	const std::string line = "DarkEden.exe " + std::string(4096, 'a') + " x";
	CHECK(!ParsingNetmarble(line.c_str()));
	CHECK_EQ(0, refused);
}

TEST(Netmarble, WithoutAHostARefusalOnlyReturns)
{
	refused = 0;
	SetNetmarbleHost(nullptr);
	CHECK(!ParsingNetmarble("DarkEden.exe 2,20,id,key"));
	const NetmarbleHost empty{};
	SetNetmarbleHost(&empty);
	CHECK(!ParsingNetmarble("DarkEden.exe 2,20,id,key"));
	SetNetmarbleHost(nullptr);
	CHECK_EQ(0, refused);
	CHECK(g_CpCookie.empty());
}
