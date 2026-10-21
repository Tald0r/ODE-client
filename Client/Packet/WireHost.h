//----------------------------------------------------------------------
// WireHost.h
//----------------------------------------------------------------------
//
// What the wire layer needs from the program around it
// (docs/reviews/RESTRUCTURING.md task 5.1).
//
// The receive loops and the connection managers under Client/Packet are
// otherwise self-contained, but they read three tuning values out of
// the executable's config and send a bug report through its own
// executable's socket. Both are seams to code the library sits below,
// so the library asks for them instead: the executable fills a WireHost
// in and installs it, exactly as MItemHost and MPriceHost work for
// gamemodel.
//
// Every accessor answers without a host. The defaults are the ones
// ClientConfig's own constructor sets, so a binary that never installs
// a host - a test binary - behaves as a client whose config file was
// missing rather than as one whose tuning is zero.
//
//----------------------------------------------------------------------

#ifndef __WIREHOST_H__
#define __WIREHOST_H__

#include "SafeFormat.h"
#include "Types.h"
#include "Types/ZoneTypes.h"

#include <string>

#include "MonotonicClock.h"
#include "Exception.h"

class Player;
class RequestServerPlayer;

//----------------------------------------------------------------------
// The answers with no host, which are ClientConfig's own constructor
// values. Named so that the executable's accessors can fall back to
// the same numbers rather than writing them a second time.
//----------------------------------------------------------------------
enum {
	WIRE_DEFAULT_MAX_PROCESS_PACKET		= 11,
	WIRE_DEFAULT_MAX_REQUEST_SERVICE	= 10,
	WIRE_DEFAULT_UDP_PORT			= 9858
};

//----------------------------------------------------------------------
// The host
//----------------------------------------------------------------------
struct WireHost {

	// How many packets one turn of a receive loop handles.
	int	(*MaxProcessPacket)();

	// How many request-service connections may be open at once.
	int	(*MaxRequestService)();

	// The UDP port the client communication manager binds.
	uint	(*ClientCommunicationUDPPort)();

	// Where a report about a malformed packet is sent. NULL while
	// there is no connection, which is not an error - the report is
	// dropped.
	Player*	(*BugReportTarget)();

	// The two values ClientPlayer::setEncryptCode() derives the stream
	// cipher's seed from: the zone the player is in, and the account's
	// server number.
	ZoneID_t	(*EncryptZoneID)();
	int		(*EncryptServerID)();

	// Which of the two seed formulas to use. The English client spaces
	// the server number differently; every other region shares one
	// formula. See WireEncryptSeed below.
	bool		(*EncryptUsesEnglishSeed)();

	//------------------------------------------------------------------
	// The request-service family (docs/reviews/RESTRUCTURING.md task 5.1's
	// fourth slice) - the peer-to-peer side, where another client dials
	// this one to fetch a profile file or deliver a whisper. Inbound
	// only: the outbound half, this client dialling peers, was compiled
	// out upstream and is deleted (task 5.2, eighth slice), and with it
	// the in-game test, the logged-in character's name, the three
	// "my request" file-transfer calls and the profile notification the
	// fifth slice had put here.
	//------------------------------------------------------------------

	// The frame stamp its timeouts are measured against.
	MonotonicClock::TimePoint	(*CurrentTime)();

	// The peer file-transfer manager, which stays executable-side: it
	// writes into the profile directory and reads the UI's own state.
	// What the wire layer needs of it is three calls - a request server
	// player hands itself to it when a peer asks for a file and takes
	// itself back out when the connection ends.
	//
	// Every one answers false with no host, which is what an
	// unregistered transfer looks like, so a receive loop that asks
	// about one cleans up rather than waiting.
	bool		(*SendOtherRequest)(const std::string& name, RequestServerPlayer* pPlayer);
	bool		(*HasOtherRequest)(const std::string& name);
	bool		(*RemoveOtherRequest)(const std::string& name);

};

//----------------------------------------------------------------------
// The stream cipher's seed.
//----------------------------------------------------------------------
// Pulled out of ClientPlayer::setEncryptCode() so that it can be
// tested. THIS IS LIVE CODE. The slice that moved it said the opposite
// - that nothing defines __USE_ENCRYPTER__, so the encrypted streams
// are never constructed - and that was wrong: Encrypter.h defines it,
// and ClientPlayer.cpp includes SocketEncryptInputStream.h two lines
// before its first #ifdef on it. tests/unit/test_packet_goldens.cpp had
// the truth written down the whole time ("0 is the plain branch, 1..5
// the __USE_ENCRYPTER__ branch"); the claim was made without checking
// against it. Both reviewers of the following slice found it.
//
// So the seed is on the live connection path: GCUpdateInfoHandler calls
// setEncryptCode() right after MoveZone/LoadZone, and the byte it
// derives has to agree with the server's exactly.
//
// The original wrote four branches - Netmarble, Chinese, English, and a
// default - of which three computed the identical expression. Only the
// English one differs, multiplying the server number by 51 where the
// others shift it left by four. That collapse is behaviour-preserving,
// which a test asserts across every server number a byte can hold
// rather than leaving to this comment.
//----------------------------------------------------------------------
uchar	WireEncryptSeed ( ZoneID_t zoneID , int serverID , bool bEnglishSeed ) noexcept;

//----------------------------------------------------------------------
// The wire layer's view of it
//----------------------------------------------------------------------
class Wire {

public :

	// Installed once at start-up; NULL puts every default back.
	static void	SetHost ( const WireHost * pHost ) noexcept { s_pHost = pHost; }

	static int	MaxProcessPacket ();
	static int	MaxRequestService ();
	static uint	ClientCommunicationUDPPort ();
	static Player *	BugReportTarget ();
	static ZoneID_t	EncryptZoneID ();
	static int	EncryptServerID ();
	static bool	EncryptUsesEnglishSeed ();

	static MonotonicClock::TimePoint	CurrentTime ();

	// NOT nothrow, and the omission is deliberate. The file-transfer
	// manager behind it reads the profile file and writes the peer
	// socket, and throwing is how a transfer ends: RequestFileManager::
	// SendOtherRequest throws ConnectException("No File to Send"), and
	// RequestServerPlayer::send propagates ProtocolException and Error.
	// The call site sits outside processCommand's try, so the exception
	// unwinds to RequestServerPlayerManager::Update's catch (Throwable&),
	// which disconnects that peer - the designed teardown. A throw() here
	// would make that path undefined under MSVC and std::terminate under
	// C++17 or on clang/gcc. (Neither are the two below, for the same
	// reason: the manager locks.)
	static bool	SendOtherRequest ( const std::string & name , RequestServerPlayer * pPlayer );

	static bool	HasOtherRequest ( const std::string & name );
	static bool	RemoveOtherRequest ( const std::string & name );

private :

	static const WireHost *	s_pHost;

};

//----------------------------------------------------------------------
// Report a malformed packet to the server, as a chat message.
//----------------------------------------------------------------------
// The executable defined this and the wire layer called it, which is a
// seam no include rule and no ratchet over globals can see - only a
// failed link found it. It lives here now, and takes its target from
// the host.
//----------------------------------------------------------------------
void SendBugReportArgs(const DiagnosticSite* site, const char* format,
	const SafeFormat::Arg* args, size_t count);

template <typename... Args>
void SendBugReport(const char* format, Args... args)
{
	const SafeFormat::Arg packed[] = {SafeFormat::MakeArg(args)..., SafeFormat::Arg()};
	SendBugReportArgs(nullptr, format, packed, sizeof...(args));
}

// The same report prefixed with the caller's captured file and line.
template <typename... Args>
void SendBugReportAt(const DiagnosticSite& site, const char* format, Args... args)
{
	const SafeFormat::Arg packed[] = {SafeFormat::MakeArg(args)..., SafeFormat::Arg()};
	SendBugReportArgs(&site, format, packed, sizeof...(args));
}

#endif	// __WIREHOST_H__
