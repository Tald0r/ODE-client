#pragma once

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
	int attempt = 0;
	std::string launcherAddress{};
	unsigned int launcherPort = 0;
	std::optional<std::string> hostOverride{};
	std::optional<std::string> portOverride{};
	int (*random)() = nullptr;
};

// Select an advertised endpoint without DNS, sockets or process globals.
// The executable advances attempt only after a failed connection.
LoginEndpoint SelectLoginEndpoint(const Properties& config, const LoginEndpointOptions& options = {});
LoginEndpoint SelectLoginEndpoint(ServerInfoFileParser& config, int dimension,
	const LoginEndpointOptions& options = {});
