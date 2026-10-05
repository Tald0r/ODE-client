#include "test_framework.h"
#include "Platform.h"
#include "MCrashReportManager.h"
#include "Profiler.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
struct DiagnosticFile
{
	std::filesystem::path directory;
	std::filesystem::path path;
	DiagnosticFile()
	{
		static unsigned serial = 0;
		for (unsigned attempt = 0; attempt < 100; ++attempt)
		{
			auto candidate = std::filesystem::temp_directory_path() / ("darkeden_diagnostic_"
				+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
				+ "_" + std::to_string(++serial));
			if (std::filesystem::create_directory(candidate))
			{
				directory = std::move(candidate);
				path = directory / "fixture.bin";
				return;
			}
		}
		throw std::runtime_error("Cannot reserve diagnostic fixture directory");
	}
	~DiagnosticFile() { std::error_code error; std::filesystem::remove_all(directory, error); }
	std::string Read() const
	{
		std::ifstream file(path, std::ios::binary);
		return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	}
};

void Populate(MCrashReport& report)
{
	report.SetExcutableTime("E");
	report.SetVersion(0x1234);
	report.SetAddress("A");
	report.SetOS("O");
	report.SetCallStack("C");
	report.SetMessage("M");
}

unsigned long long clockMilliseconds;
auto ReadClock() { return MonotonicClock::FromMillis(clockMilliseconds); }
}

TEST(CrashReports, SavesTheProductionFieldOrderAndWidths)
{
	DiagnosticFile fixture;
	MCrashReport report;
	Populate(report);
	{
		std::ofstream file(fixture.path, std::ios::binary);
		report.SaveToFile(file);
		CHECK(file.good());
	}
	const unsigned char expected[]{1, 0, 0, 0, 'E', 0x34, 0x12,
		1, 0, 0, 0, 'A', 1, 0, 0, 0, 'O', 1, 0, 0, 0, 'C', 1, 0, 0, 0, 'M'};
	CHECK(fixture.Read() == std::string(reinterpret_cast<const char*>(expected), sizeof(expected)));
}

TEST(CrashReports, LoadsAdjacentRecordsAndPreservesEmptyStrings)
{
	DiagnosticFile fixture;
	const unsigned char bytes[]{1, 0, 0, 0, 'E', 0x34, 0x12,
		1, 0, 0, 0, 'A', 1, 0, 0, 0, 'O', 1, 0, 0, 0, 'C', 1, 0, 0, 0, 'M',
		0, 0, 0, 0, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	{
		std::ofstream file(fixture.path, std::ios::binary);
		file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
	}
	std::ifstream file(fixture.path, std::ios::binary);
	MCrashReport report;
	report.LoadFromFile(file);
	CHECK(file.good());
	CHECK_EQ(27, file.tellg());
	CHECK(std::string(report.GetExecutableTime()) == "E");
	CHECK_EQ(0x1234, report.GetVersion());
	CHECK(std::string(report.GetAddress()) == "A");
	CHECK(std::string(report.GetOS()) == "O");
	CHECK(std::string(report.GetCallStack()) == "C");
	CHECK(std::string(report.GetMessage()) == "M");
	report.LoadFromFile(file);
	CHECK(file.good());
	CHECK_EQ(sizeof(bytes), file.tellg());
	CHECK_EQ(65535, report.GetVersion());
	CHECK(std::string(report.GetExecutableTime()).empty());
	CHECK(std::string(report.GetAddress()).empty());
	CHECK(std::string(report.GetOS()).empty());
	CHECK(std::string(report.GetCallStack()).empty());
	CHECK(std::string(report.GetMessage()).empty());
}

TEST(CrashReports, TruncatedMessageReportsFailureWithoutReplacingThePreviousMessage)
{
	DiagnosticFile fixture;
	MCrashReport report;
	Populate(report);
	{
		std::ofstream file(fixture.path, std::ios::binary);
		report.SaveToFile(file);
	}
	std::filesystem::resize_file(fixture.path, 26);
	report.SetMessage("retained");
	std::ifstream file(fixture.path, std::ios::binary);
	report.LoadFromFile(file);
	CHECK(file.fail());
	CHECK(std::string(report.GetMessage()) == "retained");
}

TEST(CrashReports, TableRoundTripUsesTheSameRecordsAsCrashCollectionAndSending)
{
	DiagnosticFile fixture;
	MCrashReportManager reports;
	reports.Init(2);
	Populate(*reports.GetMutable(0));
	Populate(*reports.GetMutable(1));
	reports.GetMutable(1)->SetVersion(9);
	reports.GetMutable(1)->SetMessage("second failure");
	{
		std::ofstream file(fixture.path, std::ios::binary);
		reports.SaveToFile(file);
		CHECK(file.good());
	}
	MCrashReportManager loaded;
	std::ifstream file(fixture.path, std::ios::binary);
	loaded.LoadFromFile(file);
	CHECK(file.good());
	CHECK_EQ(2, loaded.GetSize());
	if (loaded.GetSize() != 2) return;
	CHECK_EQ(0x1234, loaded[0].GetVersion());
	CHECK_EQ(9, loaded[1].GetVersion());
	CHECK(std::string(loaded[1].GetMessage()) == "second failure");
}

TEST(Profiler, NamedPassesAccumulateOnlyClosedIntervals)
{
	MonotonicClock::ScopedTestSource source(ReadClock);
	Profiler profiler;
	CHECK_EQ(0, profiler.GetNumber());
	CHECK(!profiler.HasProfileInfo("render"));
	profiler.End("missing");
	CHECK_EQ(0, profiler.GetNumber());
	CHECK_EQ(0, profiler.GetTimes("missing"));
	CHECK_EQ(0, profiler.GetTotalTime("missing"));
	CHECK(profiler.GetAverageTime("missing") == 0.0f);
	clockMilliseconds = 100;
	profiler.Begin("render");
	clockMilliseconds = 110;
	profiler.Begin("sound");
	clockMilliseconds = 130;
	profiler.End("render");
	clockMilliseconds = 160;
	profiler.End("sound");
	profiler.End("render");
	CHECK_EQ(1, profiler.GetTimes("render"));
	CHECK_EQ(30, profiler.GetTotalTime("render"));
	CHECK_EQ(50, profiler.GetTotalTime("sound"));
	clockMilliseconds = 200;
	profiler.Begin("render");
	clockMilliseconds = 210;
	profiler.End("render");
	CHECK_EQ(2, profiler.GetTimes("render"));
	CHECK_EQ(40, profiler.GetTotalTime("render"));
	CHECK(profiler.GetAverageTime("render") == 20.0f);
	CHECK_EQ(2, profiler.GetNumber());
	CHECK(profiler.GetBegin()->first == "render");
}

TEST(Profiler, RepeatedBeginRestartsTheOpenPassAndClearDiscardsIt)
{
	MonotonicClock::ScopedTestSource source(ReadClock);
	ProfilerInfo info;
	CHECK_EQ(0, info.GetTimes());
	CHECK_EQ(0, info.GetTotalTime());
	clockMilliseconds = 100;
	info.Begin();
	clockMilliseconds = 200;
	info.Begin();
	clockMilliseconds = 215;
	info.End();
	CHECK_EQ(1, info.GetTimes());
	CHECK_EQ(15, info.GetTotalTime());
	CHECK(info.GetAverageTime() == 15.0f);
	info.Begin();
	info.Clear();
	clockMilliseconds = 300;
	info.End();
	CHECK_EQ(0, info.GetTimes());
	CHECK_EQ(0, info.GetTotalTime());
}

TEST(Profiler, ReleaseIsRepeatableAndNamesCanBeReused)
{
	MonotonicClock::ScopedTestSource source(ReadClock);
	Profiler profiler;
	clockMilliseconds = 100;
	profiler.Begin("render");
	profiler.Release();
	profiler.Release();
	CHECK_EQ(0, profiler.GetNumber());
	CHECK(!profiler.HasProfileInfo("render"));
	profiler.Begin("render");
	clockMilliseconds = 107;
	profiler.End("render");
	CHECK_EQ(1, profiler.GetTimes("render"));
	CHECK_EQ(7, profiler.GetTotalTime("render"));
}

TEST(Profiler, FileOutputContainsNamedStatisticsAndHonorsAppend)
{
	DiagnosticFile fixture;
	MonotonicClock::ScopedTestSource source(ReadClock);
	Profiler profiler;
	clockMilliseconds = 100;
	profiler.Begin("render");
	clockMilliseconds = 125;
	profiler.End("render");
	profiler.WriteToFile(fixture.path.string().c_str());
	const auto first = fixture.Read();
	CHECK(first.find("ProfilerName") != std::string::npos);
	CHECK(first.find("LoopTimes   TotalTime   AverageTime") != std::string::npos);
	CHECK(first.find("render") != std::string::npos);
	CHECK(first.find("25.000") != std::string::npos);
	profiler.WriteToFile(fixture.path.string().c_str(), true);
	CHECK(fixture.Read() == first + first);
	profiler.WriteToFile(fixture.path.string().c_str());
	CHECK(fixture.Read() == first);
}
