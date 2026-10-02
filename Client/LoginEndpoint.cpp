#include "LoginEndpoint.h"
#include "Properties.h"
#include "ServerInfoFileParser.h"
#include "SafeFormat.h"

#include <cstdlib>

namespace {

template<class ReadString, class ReadInt>
LoginEndpoint Select(ReadString readString, ReadInt readInt, const LoginEndpointOptions& options)
{
	int maxAddress = 1;
	try {
		maxAddress = std::atoi(readString("MaxLoginServerAddress").c_str());
	} catch (NoSuchElementException&) {
	}

	const int index = options.attempt % maxAddress;
	LoginEndpoint endpoint;
	endpoint.port = readInt("LoginServerPort");
	if (options.launcherPort == 0)
	{
		std::string key = "LoginServerAddress";
		if (index != 0)
		{
			char number[10];
			SafeFormat::Format(number, "%d", index);
			key += number;
		}
		endpoint.address = readString(key);

		try {
			const int portCount = readInt("LoginServerPortNum");
			if (portCount > 1)
			{
				endpoint.port = readInt("LoginServerBasePort")
					+ (options.random ? options.random() : std::rand()) % portCount;
			}
		} catch (NoSuchElementException&) {
		}
	}
	else
	{
		endpoint.address = options.launcherAddress;
		endpoint.port = options.launcherPort;
	}

	if (options.hostOverride && !options.hostOverride->empty())
		endpoint.address = *options.hostOverride;
	if (options.portOverride)
	{
		char* end = nullptr;
		const long value = std::strtol(options.portOverride->c_str(), &end, 10);
		if (options.portOverride->empty() || *end || value < 1 || value > 65535)
			throw ConnectException("Invalid DARKEDEN_LOGIN_PORT");
		endpoint.port = static_cast<unsigned int>(value);
	}
	if (endpoint.address.empty()) throw ConnectException("Login server address is empty");
	return endpoint;
}

} // namespace

LoginEndpoint SelectLoginEndpoint(const Properties& config, const LoginEndpointOptions& options)
{
	return Select([&](const std::string& key) { return config.getProperty(key); },
		[&](const std::string& key) { return config.getPropertyInt(key); }, options);
}

LoginEndpoint SelectLoginEndpoint(ServerInfoFileParser& config, int dimension,
	const LoginEndpointOptions& options)
{
	return Select([&](const std::string& key) { return config.getProperty(dimension, key); },
		[&](const std::string& key) { return config.getPropertyInt(dimension, key); }, options);
}
