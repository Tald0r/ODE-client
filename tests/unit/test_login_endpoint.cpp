#include "test_framework.h"
#include "LoginEndpoint.h"
#include "Properties.h"
#include "ServerInfoFileParser.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <initializer_list>
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
	return false;
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
			const auto endpoint = select(values, {.attempt = attempt});
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
