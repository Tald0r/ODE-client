#include "test_framework.h"
#include "LoginEndpoint.h"
#include "Properties.h"
#include "ServerInfoFileParser.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

namespace {

using Settings = std::map<std::string, std::string>;

Settings Defaults()
{
	return {{"MaxLoginServerAddress", "1"}, {"LoginServerAddress", "login.example"},
		{"LoginServerPort", "9999"}};
}

struct ConfigFile {
	std::filesystem::path path;
	explicit ConfigFile(const std::string& text)
	{
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		path = std::filesystem::temp_directory_path()
			/ ("darkeden_login_endpoint_" + std::to_string(stamp) + ".inf");
		std::ofstream output(path, std::ios::binary);
		output << text;
		output.close();
		if (!output.good()) throw std::runtime_error("Cannot write login endpoint fixture");
	}
	~ConfigFile()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}
};

LoginEndpoint Korean(const Settings& values, const LoginEndpointOptions& options)
{
	Properties config;
	for (const auto& [key, value] : values) config.setProperty(key, value);
	return SelectLoginEndpoint(config, options);
}

LoginEndpoint Foreign(const Settings& values, const LoginEndpointOptions& options)
{
	std::string text = "@\n";
	for (const auto& [key, value] : values) text += key + ":" + value + "\n";
	ConfigFile file(text + "@\n");
	ServerInfoFileParser config(file.path.string());
	return SelectLoginEndpoint(config, 0, options);
}

int randomCalls = 0, randomValue = 0;
int Random() { ++randomCalls; return randomValue; }

template<class Select>
bool Rejects(Select select, const Settings& values, const LoginEndpointOptions& options)
{
	try { (void)select(values, options); }
	catch (const ConnectException&) { return true; }
	catch (const Throwable&) { return false; }
	return false;
}

template<class Select>
void CheckEndpoint(Select select, const Settings& values, const LoginEndpointOptions& options,
	const char* address, unsigned int port)
{
	try {
		const auto endpoint = select(values, options);
		CHECK(endpoint.address == address);
		CHECK_EQ(port, endpoint.port);
	} catch (const Throwable&) {
		CHECK(false);
	}
}

} // namespace

TEST(LoginEndpoint, RotatesAcrossTheUnnumberedAndNumberedAddresses)
{
	auto values = Defaults();
	values["MaxLoginServerAddress"] = "3";
	values["LoginServerAddress1"] = "second.example";
	values["LoginServerAddress2"] = "third.example";
	const char* expected[] = {"login.example", "second.example", "third.example"};
	for (auto select : {Korean, Foreign})
	{
		for (int attempt = 0; attempt < 9; ++attempt)
		{
			LoginEndpointOptions options;
			options.attempt = static_cast<std::uint64_t>(attempt);
			const auto endpoint = select(values, options);
			CHECK(endpoint.address == expected[attempt % 3]);
			CHECK_EQ(9999, endpoint.port);
		}
	}
}

TEST(LoginEndpoint, ForeignDimensionsUseTheirOwnAddressAndPortSettings)
{
	ConfigFile file("@\nMaxLoginServerAddress:2\nLoginServerAddress:first.example\n"
		"LoginServerAddress1:first-backup.example\nLoginServerPort:1000\n@\n"
		"@\nMaxLoginServerAddress:3\nLoginServerAddress:second.example\n"
		"LoginServerAddress1:second-backup.example\nLoginServerPort:2000\n@\n");
	ServerInfoFileParser config(file.path.string());
	const auto first = SelectLoginEndpoint(config, 0, {.attempt = 3});
	const auto second = SelectLoginEndpoint(config, 1, {.attempt = 3});
	CHECK(first.address == "first-backup.example");
	CHECK_EQ(1000, first.port);
	CHECK(second.address == "second.example");
	CHECK_EQ(2000, second.port);
}

TEST(LoginEndpoint, MissingKoreanAddressCountDefaultsToOne)
{
	auto values = Defaults();
	values.erase("MaxLoginServerAddress");
	const auto endpoint = Korean(values, {.attempt = 15});
	CHECK(endpoint.address == "login.example");
	CHECK_EQ(9999, endpoint.port);
}

TEST(LoginEndpoint, AbsentAndSingletonPortRangesKeepTheFixedPortWithoutDrawing)
{
	for (auto select : {Korean, Foreign})
	{
		auto values = Defaults();
		LoginEndpointOptions options;
		options.random = Random;
		randomCalls = 0;
		CHECK_EQ(9999, select(values, options).port);
		for (const char* count : {"0", "1"})
		{
			values["LoginServerPortNum"] = count;
			CHECK_EQ(9999, select(values, options).port);
		}
		CHECK_EQ(0, randomCalls);
	}
}

TEST(LoginEndpoint, RandomPortRangesUseTheBaseAndOneDraw)
{
	auto values = Defaults();
	values["LoginServerPortNum"] = "5";
	values["LoginServerBasePort"] = "20000";
	for (auto select : {Korean, Foreign})
	{
		for (int draw : {0, 1, 4, 5, 1234})
		{
			randomCalls = 0;
			randomValue = draw;
			LoginEndpointOptions options;
			options.random = Random;
			const auto endpoint = select(values, options);
			CHECK_EQ(20000 + draw % 5, endpoint.port);
			CHECK_EQ(1, randomCalls);
		}
	}
}

TEST(LoginEndpoint, MissingKoreanRangeBaseKeepsTheFixedPort)
{
	auto values = Defaults();
	values["LoginServerPortNum"] = "5";
	CHECK_EQ(9999, Korean(values, {}).port);
}

TEST(LoginEndpoint, LauncherEndpointBypassesConfiguredAddressAndPortRange)
{
	auto values = Defaults();
	values.erase("LoginServerAddress");
	values["LoginServerPortNum"] = "5";
	values["LoginServerBasePort"] = "20000";
	for (auto select : {Korean, Foreign})
	{
		LoginEndpointOptions options;
		options.launcherAddress = "launcher.example";
		options.launcherPort = 10000;
		options.random = Random;
		randomCalls = 0;
		const auto endpoint = select(values, options);
		CHECK(endpoint.address == "launcher.example");
		CHECK_EQ(10000, endpoint.port);
		CHECK_EQ(0, randomCalls);
	}
}

TEST(LoginEndpoint, EnvironmentOverridesTakePrecedenceOverTheLauncher)
{
	for (auto select : {Korean, Foreign})
	{
		LoginEndpointOptions options;
		options.launcherAddress = "launcher.example";
		options.launcherPort = 10000;
		options.hostOverride = "environment.example";
		options.portOverride = "65535";
		const auto endpoint = select(Defaults(), options);
		CHECK(endpoint.address == "environment.example");
		CHECK_EQ(65535, endpoint.port);
	}
}

TEST(LoginEndpoint, EnvironmentFieldsAreIndependentAndAnEmptyHostIsIgnored)
{
	for (auto select : {Korean, Foreign})
	{
		LoginEndpointOptions options;
		options.hostOverride = "host-only.example";
		CHECK_EQ(9999, select(Defaults(), options).port);
		options.hostOverride = "";
		for (const char* port : {"1", "42", " +42", "65535"})
		{
			options.portOverride = port;
			const auto endpoint = select(Defaults(), options);
			CHECK(endpoint.address == "login.example");
			CHECK_EQ(std::stoi(port), endpoint.port);
		}
	}
}

TEST(LoginEndpoint, InvalidEnvironmentPortsReportAConnectionError)
{
	for (auto select : {Korean, Foreign})
	{
		for (const char* port : {"", "0", "-1", "65536", "999999999999999999999999", "4x", "4 "})
		{
			LoginEndpointOptions options;
			options.portOverride = port;
			CHECK(Rejects(select, Defaults(), options));
		}
	}
}

TEST(LoginEndpoint, EmptyAddressesFailUnlessTheEnvironmentSuppliesOne)
{
	auto values = Defaults();
	values["LoginServerAddress"] = "";
	for (auto select : {Korean, Foreign})
	{
		CHECK(Rejects(select, values, {}));
		LoginEndpointOptions options;
		options.hostOverride = "override.example";
		CHECK(select(values, options).address == "override.example");
	}
}

TEST(LoginEndpoint, AdvertisedHostnamesAndAddressesAreReturnedWithoutResolution)
{
	for (auto select : {Korean, Foreign})
	{
		for (const char* address : {"192.0.2.123", "login.example", "2001:db8::1"})
		{
			auto values = Defaults();
			values["LoginServerAddress"] = address;
			CHECK(select(values, {}).address == address);
		}
	}
}

TEST(LoginEndpoint, ConfiguredPortsMustBeCompleteDecimalValuesInTheTCPRange)
{
	for (auto select : {Korean, Foreign})
	{
		for (const char* port : {"", "0", "-1", "65536", "2147483648", "4294967295",
			"not-a-port", "9999suffix", "999999999999999999999999"})
		{
			auto values = Defaults();
			values["LoginServerPort"] = port;
			CHECK(Rejects(select, values, {}));
		}
		for (const char* port : {"1", "65535"})
		{
			auto values = Defaults();
			values["LoginServerPort"] = port;
			CHECK_EQ(std::stoi(port), select(values, {}).port);
		}
	}
}

TEST(LoginEndpoint, TheWholeConfiguredPortRangeMustBeValid)
{
	struct Range { const char* count; const char* base; };
	const Range invalid[] = {
		{"2", "65535"}, {"65535", "2"}, {"3", "0"}, {"3", "-1"},
		{"3", "65536"}, {"2147483647", "1"}, {"-1", "20000"},
		{"2x", "20000"}, {"999999999999999999999999", "20000"},
		{"3", "not-a-port"}, {"3", "2147483647"}
	};
	for (auto select : {Korean, Foreign})
	{
		for (const auto& range : invalid)
		{
			auto values = Defaults();
			values["LoginServerPortNum"] = range.count;
			values["LoginServerBasePort"] = range.base;
			randomValue = 0; // Reject invalid ranges even when the first draw fits.
			LoginEndpointOptions options;
			options.random = Random;
			CHECK(Rejects(select, values, options));
		}
	}
}

TEST(LoginEndpoint, ValidRangeBoundariesDoNotRequireAnUnusedFixedPort)
{
	for (auto select : {Korean, Foreign})
	{
		for (int base : {1, 65534})
		{
			auto values = Defaults();
			values.erase("LoginServerPort");
			const int count = 65536 - base;
			values["LoginServerPortNum"] = std::to_string(count);
			values["LoginServerBasePort"] = std::to_string(base);
			for (int draw : {0, count - 1})
			{
				randomValue = draw;
				randomCalls = 0;
				LoginEndpointOptions options;
				options.random = Random;
				CheckEndpoint(select, values, options, "login.example", base + draw);
				CHECK_EQ(1, randomCalls);
			}
		}
	}
}

TEST(LoginEndpoint, BothFormatsFallBackWhenTheOptionalRangeBaseIsAbsentOrEmpty)
{
	for (auto select : {Korean, Foreign})
	{
		auto values = Defaults();
		values["LoginServerPortNum"] = "3";
		LoginEndpointOptions options;
		options.random = Random;
		randomValue = 0;
		CheckEndpoint(select, values, options, "login.example", 9999);
		values["LoginServerBasePort"] = "";
		CheckEndpoint(select, values, options, "login.example", 9999);
	}
}

TEST(LoginEndpoint, EffectiveOverridesIgnoreUnusedMissingSettings)
{
	for (auto select : {Korean, Foreign})
	{
		auto values = Defaults();
		values.erase("LoginServerPort");
		LoginEndpointOptions options;
		options.launcherAddress = "launcher.example";
		options.launcherPort = 12000;
		CheckEndpoint(select, values, options, "launcher.example", 12000);
		options.launcherPort = 0;
		options.hostOverride = "environment.example";
		options.portOverride = "13000";
		CheckEndpoint(select, values, options, "environment.example", 13000);
	}
}

TEST(LoginEndpoint, LauncherPortsAreValidatedUnlessTheEnvironmentReplacesThem)
{
	for (auto select : {Korean, Foreign})
	{
		for (unsigned int port : {65536u, (std::numeric_limits<unsigned int>::max)()})
		{
			LoginEndpointOptions options;
			options.launcherAddress = "launcher.example";
			options.launcherPort = port;
			CHECK(Rejects(select, Defaults(), options));
			options.portOverride = "12345";
			CheckEndpoint(select, Defaults(), options, "launcher.example", 12345);
		}
	}
}

TEST(LoginEndpoint, EmbeddedNullsCannotHideTrailingOverrideText)
{
	for (auto select : {Korean, Foreign})
	{
		LoginEndpointOptions options;
		options.portOverride = std::string("9999") + '\0' + "suffix";
		CHECK(Rejects(select, Defaults(), options));
		options.portOverride.reset();
		options.hostOverride = std::string("login.example") + '\0' + ".other";
		CHECK(Rejects(select, Defaults(), options));
		options.hostOverride.reset();
		options.launcherAddress = std::string("login.example") + '\0' + ".other";
		options.launcherPort = 9999;
		CHECK(Rejects(select, Defaults(), options));
	}
}

TEST(LoginEndpoint, RequiredMissingFieldsReportConnectionErrorsInBothFormats)
{
	for (auto select : {Korean, Foreign})
	{
		for (const char* missing : {"LoginServerAddress", "LoginServerPort"})
		{
			auto values = Defaults();
			values.erase(missing);
			CHECK(Rejects(select, values, {}));
		}
	}
}

TEST(LoginEndpoint, AddressSuffixesKeepEveryDigit)
{
	auto values = Defaults();
	values["MaxLoginServerAddress"] = "2147483647";
	values["LoginServerAddress2147483646"] = "last.example";
	LoginEndpointOptions options;
	options.attempt = 2147483646;
	for (auto select : {Korean, Foreign})
		CheckEndpoint(select, values, options, "last.example", 9999);
}

TEST(LoginEndpoint, RetryIndicesRetainTheirFullWidth)
{
	auto values = Defaults();
	values["MaxLoginServerAddress"] = "3";
	values["LoginServerAddress1"] = "second.example";
	values["LoginServerAddress2"] = "third.example";
	const char* expected[] = {"login.example", "second.example", "third.example"};
	for (auto select : {Korean, Foreign})
	{
		for (std::uint64_t attempt : {std::uint64_t(1) << 31, std::uint64_t(1) << 32,
			(std::numeric_limits<std::uint64_t>::max)()})
		{
			LoginEndpointOptions options;
			// Exercise the old narrow API as well as its widened replacement.
			options.attempt = static_cast<decltype(options.attempt)>(attempt);
			CheckEndpoint(select, values, options, expected[attempt % 3], 9999);
		}
	}
}

TEST(LoginEndpoint, InvalidAddressCountsAreRejectedBeforeDivision)
{
	for (auto select : {Korean, Foreign})
	{
		for (const char* count : {"-1", "-2", "0", "not-a-count", "3suffix", "2147483648",
			"999999999999999999999999"})
		{
			auto values = Defaults();
			values["MaxLoginServerAddress"] = count;
			CHECK(Rejects(select, values, {}));
		}
	}
}

TEST(LoginEndpoint, MissingForeignAddressCountsUseTheSameSingleAddressDefault)
{
	auto values = Defaults();
	values.erase("MaxLoginServerAddress");
	LoginEndpointOptions options;
	options.attempt = 15;
	CheckEndpoint(Foreign, values, options, "login.example", 9999);
}

TEST(LoginEndpoint, CompleteOverridesSkipInvalidUnusedCountsAndRanges)
{
	auto values = Defaults();
	values["MaxLoginServerAddress"] = "0";
	values["LoginServerPort"] = "not-a-port";
	values["LoginServerPortNum"] = "not-a-count";
	values.erase("LoginServerAddress");
	for (auto select : {Korean, Foreign})
	{
		LoginEndpointOptions options;
		options.launcherAddress = "launcher.example";
		options.launcherPort = 12000;
		options.random = Random;
		randomCalls = 0;
		CheckEndpoint(select, values, options, "launcher.example", 12000);
		options.launcherPort = 0;
		options.hostOverride = "environment.example";
		options.portOverride = "13000";
		CheckEndpoint(select, values, options, "environment.example", 13000);
		CHECK_EQ(0, randomCalls);
	}
}
