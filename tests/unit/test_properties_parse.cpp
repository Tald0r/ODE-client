//----------------------------------------------------------------------
// test_properties_parse.cpp
//----------------------------------------------------------------------
//
// Properties::load finds the key, the separator and the value with the
// std::string search functions, which return std::string::npos when they
// find nothing. Those positions must be held in std::string::size_type:
// in a 32-bit type npos is truncated and never compares equal to npos on
// a 64-bit build, so a blank line was parsed instead of skipped and a
// line without a separator was accepted instead of rejected.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "Properties.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
	struct TempProperties
	{
		std::filesystem::path path;

		explicit TempProperties(const std::string& text)
		{
			const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
			path = std::filesystem::temp_directory_path()
				/ ("darkeden_properties_parse_" + std::to_string(stamp) + ".inf");
			std::ofstream(path, std::ios::binary) << text;
		}

		~TempProperties()
		{
			std::error_code error;
			std::filesystem::remove(path, error);
		}
	};
}

TEST(PropertiesParse, WhitespaceOnlyLineIsSkipped)
{
	TempProperties file("First : 1\n   \t \nSecond : 2\n");

	Properties properties;
	bool threw = false;
	try
	{
		properties.load(file.path.string());
	}
	catch (...)
	{
		threw = true;
	}

	CHECK_EQ(false, threw);
	if (!threw)
	{
		CHECK_EQ(1, properties.getPropertyInt("First"));
		CHECK_EQ(2, properties.getPropertyInt("Second"));
	}
}

TEST(PropertiesParse, LineWithoutSeparatorIsRejected)
{
	TempProperties file("First : 1\nNoSeparatorHere\n");

	Properties properties;
	bool threw = false;
	try
	{
		properties.load(file.path.string());
	}
	catch (...)
	{
		threw = true;
	}

	CHECK_EQ(true, threw);
}
