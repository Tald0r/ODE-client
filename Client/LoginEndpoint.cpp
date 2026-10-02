#include "LoginEndpoint.h"

#include <cerrno>
#include <cstdlib>
#include <limits>

#include "Properties.h"
#include "ServerInfoFileParser.h"

namespace {

template<class ReadString>
std::string Optional(ReadString& readString, const std::string& key)
{
	try {
		return readString(key);
	} catch (NoSuchElementException&) {
		return {};
	}
}

unsigned int Number(const std::string& text, const char* key, unsigned int minimum, unsigned int maximum)
{
	char* end = nullptr;
	errno = 0;
	const long long value = std::strtoll(text.c_str(), &end, 10);
	// Comparing against the full string also rejects embedded NULs. Retain
	// strtol's existing acceptance of a leading plus sign and whitespace.
	if (text.empty() || end == text.c_str() || end != text.c_str() + text.size()
		|| errno == ERANGE || value < minimum || value > maximum)
		throw ConnectException(std::string("Invalid ") + key);
	return static_cast<unsigned int>(value);
}

template<class ReadString>
unsigned int ConfiguredPort(ReadString& readString, const LoginEndpointOptions& options)
{
	const auto countText = Optional(readString, "LoginServerPortNum");
	const unsigned int count = countText.empty() ? 0 : Number(countText, "LoginServerPortNum", 0, 65535);
	if (count > 1)
	{
		const auto baseText = Optional(readString, "LoginServerBasePort");
		if (!baseText.empty())
		{
			const unsigned int base = Number(baseText, "LoginServerBasePort", 1, 65535);
			// Validate the entire interval before addition or drawing a value.
			if (count > 65536u - base)
				throw ConnectException("Login server port range exceeds 65535");
			const int draw = options.random ? options.random() : std::rand();
			if (draw < 0) throw ConnectException("Invalid login server random value");
			return base + static_cast<unsigned int>(draw) % count;
		}
	}
	return Number(Optional(readString, "LoginServerPort"), "LoginServerPort", 1, 65535);
}

template<class ReadString>
LoginEndpoint Select(ReadString readString, const LoginEndpointOptions& options)
{
	LoginEndpoint endpoint;
	if (options.hostOverride && !options.hostOverride->empty())
		endpoint.address = *options.hostOverride;
	else if (options.launcherPort != 0)
		endpoint.address = options.launcherAddress;
	else
	{
		const auto countText = Optional(readString, "MaxLoginServerAddress");
		const unsigned int count = countText.empty() ? 1 : Number(countText,
			"MaxLoginServerAddress", 1, (std::numeric_limits<int>::max)());
		const auto index = options.attempt % count;
		const auto key = index == 0 ? std::string("LoginServerAddress")
			: "LoginServerAddress" + std::to_string(index);
		endpoint.address = Optional(readString, key);
	}

	if (options.portOverride)
		endpoint.port = Number(*options.portOverride, "DARKEDEN_LOGIN_PORT", 1, 65535);
	else if (options.launcherPort != 0)
	{
		if (options.launcherPort > 65535) throw ConnectException("Invalid launcher login port");
		endpoint.port = options.launcherPort;
	}
	else
		endpoint.port = ConfiguredPort(readString, options);

	if (endpoint.address.empty()) throw ConnectException("Login server address is empty");
	if (endpoint.address.find('\0') != std::string::npos)
		throw ConnectException("Login server address contains a NUL byte");
	return endpoint;
}

} // namespace

LoginEndpoint SelectLoginEndpoint(const Properties& config, const LoginEndpointOptions& options)
{
	return Select([&](const std::string& key) { return config.getProperty(key); }, options);
}

LoginEndpoint SelectLoginEndpoint(ServerInfoFileParser& config, int dimension,
	const LoginEndpointOptions& options)
{
	return Select([&](const std::string& key) { return config.getProperty(dimension, key); }, options);
}
