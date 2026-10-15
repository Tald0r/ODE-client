#ifndef __NETMARBLE_H__
#define __NETMARBLE_H__

#include <string>

// Netmarble's login ID; always empty (see Netmarble.cpp).
extern std::string g_CpCookie;

// The game tells the player to launch from Netmarble's site and exits
// (task 4.220). Without a host a refused launch only returns false.
struct NetmarbleHost
{
	void (*RefuseLaunch)();
};

void	SetNetmarbleHost(const NetmarbleHost* pHost);

// A Netmarble launch: false when the command line has no space, or what
// follows the first space is 4096 bytes or more. Otherwise the launch is
// refused through the host (in the game it does not return), then false.
bool	ParsingNetmarble(const char* pCommandLine);

#endif
