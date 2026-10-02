#pragma once

#include <cstdint>
#include <optional>
#include <string>

class Properties;
class ServerInfoFileParser;

struct LoginEndpoint {
	std::string address;
	unsigned int port = 0;
};

// Inputs owned by the caller. Environment text is read once per attempt;
// random, when installed, has the same nonnegative-result contract as rand.
struct LoginEndpointOptions {
	std::uint64_t attempt = 0;
	std::string launcherAddress{};
	unsigned int launcherPort = 0;
	std::optional<std::string> hostOverride{};
	std::optional<std::string> portOverride{};
	int (*random)() = nullptr;
};

// Select an advertised endpoint without DNS, sockets or game globals.
// The executable advances attempt only after a failed connection.
// Only effective settings are read: environment overrides, then the launcher,
// then configuration. Missing optional address counts use one; a missing range
// base uses the fixed port. Malformed effective values throw ConnectException.
LoginEndpoint SelectLoginEndpoint(const Properties& config, const LoginEndpointOptions& options = {});
LoginEndpoint SelectLoginEndpoint(ServerInfoFileParser& config, int dimension,
	const LoginEndpointOptions& options = {});
