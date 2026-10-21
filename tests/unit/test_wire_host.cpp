//----------------------------------------------------------------------
// test_wire_host.cpp
//----------------------------------------------------------------------
//
// What the wire layer asks of the program around it
// (docs/reviews/RESTRUCTURING.md task 5.1): three tuning values the receive
// loops and the connection managers used to read straight out of the
// executable's ClientConfig, and the connection a report about a
// malformed packet is sent to.
//
// The point of the seam is that the library answers without the
// executable, so most of what there is to test is what it answers when
// nobody has installed a host - which is the state a test binary, and
// a client whose config file is missing, are both in.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "ClientCommunicationManager.h"
#include "ClientPlayer.h"
#include "RequestServerPlayer.h"
#include "RequestServerPlayerManager.h"
#include "Player.h"
#include "PacketDiagnostics.h"
#include "WireHost.h"

namespace {

int	s_MaxProcessPacket	= 0;
int	s_MaxRequestService	= 0;
uint	s_UDPPort		= 0;
Player*	s_pTarget		= NULL;

ZoneID_t	s_EncryptZoneID		= 0;
int		s_EncryptServerID	= 0;
bool		s_EnglishSeed		= false;

MonotonicClock::TimePoint	s_Now;

// The six file-transfer entries have the same signature in pairs, so a
// host wired to the wrong one would still answer. Recording the name
// makes the test say WHICH was reached.
std::string	s_RequestAsked;

void	Asked(const char* pWhich, const std::string& name)
{
	s_RequestAsked += pWhich;
	s_RequestAsked += "(";
	s_RequestAsked += name;
	s_RequestAsked += ") ";
}

int	HostMaxProcessPacket()		{ return s_MaxProcessPacket; }
int	HostMaxRequestService()		{ return s_MaxRequestService; }
uint	HostUDPPort()			{ return s_UDPPort; }
Player*	HostBugReportTarget()		{ return s_pTarget; }
ZoneID_t	HostEncryptZoneID()	{ return s_EncryptZoneID; }
int	HostEncryptServerID()		{ return s_EncryptServerID; }
bool	HostEnglishSeed()		{ return s_EnglishSeed; }

MonotonicClock::TimePoint	HostCurrentTime()	{ return s_Now; }

bool	HostSendOtherRequest(const std::string& n, RequestServerPlayer*)	{ Asked("SendOther", n); return true; }
bool	HostHasOtherRequest(const std::string& n)			{ Asked("HasOther", n); return true; }
bool	HostRemoveOtherRequest(const std::string& n)			{ Asked("RemoveOther", n); return true; }

// Designated (C++20), as the executable's own s_WireHost is: five of
// the 11 entries share a signature with another on Windows (seven off
// it, where DWORD and uint are the same type), and a positional
// initialiser wired to the wrong slot compiles. The recorder names
// below say which entry was reached; the designators say which entry
// each function was installed in.
const WireHost	s_Host = {
	.MaxProcessPacket		= HostMaxProcessPacket,
	.MaxRequestService		= HostMaxRequestService,
	.ClientCommunicationUDPPort	= HostUDPPort,
	.BugReportTarget		= HostBugReportTarget,
	.EncryptZoneID			= HostEncryptZoneID,
	.EncryptServerID		= HostEncryptServerID,
	.EncryptUsesEnglishSeed		= HostEnglishSeed,
	.CurrentTime			= HostCurrentTime,
	.SendOtherRequest		= HostSendOtherRequest,
	.HasOtherRequest		= HostHasOtherRequest,
	.RemoveOtherRequest		= HostRemoveOtherRequest,
};

// A host that answers nothing, which is not the same as no host. Every
// member value-initialised to NULL.
const WireHost	s_EmptyHost = {};

// SendBugReport asks for a target only once it has decided the report
// is worth sending, so the count is the observable half of a function
// whose other half needs a connection.
int	s_Asked = 0;

Player*	CountingBugReportTarget()	{ s_Asked++; return s_pTarget; }

// s_Host with the counting target in place of the plain one; the
// other 10 entries are the same functions.
const WireHost	s_CountingHost = {
	.MaxProcessPacket		= HostMaxProcessPacket,
	.MaxRequestService		= HostMaxRequestService,
	.ClientCommunicationUDPPort	= HostUDPPort,
	.BugReportTarget		= CountingBugReportTarget,
	.EncryptZoneID			= HostEncryptZoneID,
	.EncryptServerID		= HostEncryptServerID,
	.EncryptUsesEnglishSeed		= HostEnglishSeed,
	.CurrentTime			= HostCurrentTime,
	.SendOtherRequest		= HostSendOtherRequest,
	.HasOtherRequest		= HostHasOtherRequest,
	.RemoveOtherRequest		= HostRemoveOtherRequest,
};

// Puts the library back the way every other test expects it.
struct NoHost
{
	~NoHost()
	{
		Wire::SetHost(NULL);
	}
};

} // namespace

//----------------------------------------------------------------------
// Without a host
//----------------------------------------------------------------------
TEST(WireHostSeam, TheDefaultsAreTheConfigsOwn)
{
	NoHost	restore;

	Wire::SetHost(NULL);

	// ClientConfig's constructor sets exactly these three, so a
	// library with no host behaves as a client whose config file was
	// missing rather than as one tuned to zero - a receive loop that
	// handled 0 packets a turn would take no packets at all.
	CHECK_EQ(11, Wire::MaxProcessPacket());
	CHECK_EQ(10, Wire::MaxRequestService());
	CHECK_EQ(9858, (int)Wire::ClientCommunicationUDPPort());

	// And there is nowhere to send a report, which is not an error.
	CHECK(Wire::BugReportTarget() == NULL);

	// The encrypt-seed inputs answer zero and the non-English formula,
	// which is what they would be before the player is in a zone.
	CHECK_EQ(0, (int)Wire::EncryptZoneID());
	CHECK_EQ(0, Wire::EncryptServerID());
	CHECK_EQ(false, Wire::EncryptUsesEnglishSeed());

	// A host that answers nothing answers the same way, one entry at a
	// time - the accessors test the pointer, not just the host.
	Wire::SetHost(&s_EmptyHost);
	CHECK_EQ(11, Wire::MaxProcessPacket());
	CHECK_EQ(10, Wire::MaxRequestService());
	CHECK_EQ(9858, (int)Wire::ClientCommunicationUDPPort());
	CHECK(Wire::BugReportTarget() == NULL);
	CHECK_EQ(0, (int)Wire::EncryptZoneID());
	CHECK_EQ(0, Wire::EncryptServerID());
	CHECK_EQ(false, Wire::EncryptUsesEnglishSeed());
}

TEST(WireHostSeam, AHostAnswersForTheProgramAroundIt)
{
	NoHost	restore;

	s_MaxProcessPacket	= 3;
	s_MaxRequestService	= 4;
	s_UDPPort		= 1234;
	s_pTarget		= NULL;

	Wire::SetHost(&s_Host);

	CHECK_EQ(3, Wire::MaxProcessPacket());
	CHECK_EQ(4, Wire::MaxRequestService());
	CHECK_EQ(1234, (int)Wire::ClientCommunicationUDPPort());

	// Read each time, not copied once: the config the executable reads
	// them from is built after start-up and deleted at shutdown.
	s_MaxProcessPacket = 99;
	CHECK_EQ(99, Wire::MaxProcessPacket());

	// Taking the host away puts the defaults back.
	Wire::SetHost(NULL);
	CHECK_EQ(11, Wire::MaxProcessPacket());
}

//----------------------------------------------------------------------
// The report itself
//----------------------------------------------------------------------
TEST(WireHostSeam, AReportIsBuiltOnlyWhenThereIsSomethingToSay)
{
	NoHost	restore;

	//------------------------------------------------------------------
	// This is the function whose definition lived in the executable
	// while the wire layer called it - the seam only a failed link
	// could see, and unreachable from a test binary until it moved.
	//
	// It asks the host for a target only after it has decided the
	// report is worth sending, so counting the asks says which reports
	// get that far without needing a Player to send one to.
	//------------------------------------------------------------------
	s_pTarget = NULL;
	s_Asked = 0;
	Wire::SetHost(&s_CountingHost);

	// Nothing to format.
	SendBugReport(NULL);
	CHECK_EQ(0, s_Asked);

	// A report that says nothing, and one that says one character:
	// both dropped before a packet is built (the length test is
	// `<= 1`, so a single character does not survive it either).
	SendBugReport("");
	SendBugReport("x");
	CHECK_EQ(0, s_Asked);

	// Two characters is a report.
	SendBugReport("xy");
	CHECK_EQ(1, s_Asked);

	// So is a formatted one, and a plain one.
	SendBugReport("%d %s", 7, "formatted");
	SendBugReport("a report with no connection to send it on");
	CHECK_EQ(3, s_Asked);

	// Longer than the 256-byte buffer it formats into: truncated
	// rather than overrun, and still a report.
	char	shout[600];
	for (int i = 0; i < 599; i++)
		shout[i] = 'A';
	shout[599] = '\0';
	SendBugReport("%s", shout);
	CHECK_EQ(4, s_Asked);

	// Every one of those was asked for a target and told there is
	// none, which is where they stopped.
	CHECK(Wire::BugReportTarget() == NULL);
	CHECK_EQ(5, s_Asked);		// the line above asked too
}

//----------------------------------------------------------------------
// The library's own diagnostic seam, which used to run through the
// executable and back
//----------------------------------------------------------------------
TEST(WireHostSeam, AWireDiagnosticReachesTheReporterWithNoHookInstalled)
{
	NoHost	restore;

	PacketDiagnostics::BugReportFn	pPrevious = PacketDiagnostics::getBugReportHook();

	s_pTarget = NULL;
	s_Asked = 0;
	Wire::SetHost(&s_CountingHost);
	PacketDiagnostics::setBugReportHook(NULL);

	// With no hook, a report from inside the library used to go
	// nowhere at all: the hook was how it left, and the executable
	// installed one that called straight back into this library. It
	// goes to SendBugReport directly now.
	PacketDiagnostics::reportBug("too large PacketSize ID)%d %d/%d", 1, 2, 3);
	CHECK_EQ(1, s_Asked);

	// A hook still intercepts, and nothing reaches the reporter then.
	PacketDiagnostics::setBugReportHook(pPrevious);
}

//----------------------------------------------------------------------
// The file the seam let into the library
//----------------------------------------------------------------------
TEST(WireHostSeam, TheCommunicationManagersObjectIsInTheLibrary)
{
	// Same link proof as DatagramSocket's, and for the same reason: a
	// static library hands the linker only the objects that resolve
	// something. Its constructor opens a UDP socket on the configured
	// port, so nothing here builds one.
	void (ClientCommunicationManager::*pUpdate)() = &ClientCommunicationManager::Update;
	void (ClientCommunicationManager::*pSend)(Datagram*) = &ClientCommunicationManager::sendDatagram;

	CHECK(pUpdate != NULL);
	CHECK(pSend != NULL);
}

TEST(WireHostSeam, AHostAnswersTheEncryptSeedInputs)
{
	NoHost	restore;

	s_EncryptZoneID		= 0x1234;
	s_EncryptServerID	= 7;
	s_EnglishSeed		= true;

	Wire::SetHost(&s_Host);

	CHECK_EQ(0x1234, (int)Wire::EncryptZoneID());
	CHECK_EQ(7, Wire::EncryptServerID());
	CHECK_EQ(true, Wire::EncryptUsesEnglishSeed());

	// Read each time. The zone changes under the client while the
	// connection stays open, so a value copied once would be the wrong
	// one for every zone after the first.
	s_EncryptZoneID = 0x0009;
	CHECK_EQ(9, (int)Wire::EncryptZoneID());
}

//----------------------------------------------------------------------
// The stream cipher's seed
//----------------------------------------------------------------------
//
// These are the bytes the server has to agree with on every login, and
// the expectations below were computed from the expression that stood
// in ClientPlayer.cpp before it moved.
//
// This comment used to say the opposite - that nothing exercises the
// seed, because no target defines __USE_ENCRYPTER__. Encrypter.h
// defines it, ClientPlayer.cpp pulls that in two lines before its first
// #ifdef, and GCUpdateInfoHandler calls setEncryptCode() right after
// MoveZone/LoadZone. The slice that wrote the claim also collapsed four
// branches into two on the strength of it; the collapse happens to be
// correct, and TheThreeNonEnglishRegionsAgreedAndStillDo is what makes
// that a checked fact rather than a lucky one.
//----------------------------------------------------------------------
TEST(WireEncryptSeed, MatchesTheExpressionItWasExtractedFrom)
{
	// ((zone >> 8) ^ zone) ^ ((server + 1) << 4)
	// zone 0x1234: 0x12 ^ 0x34 = 0x26; server 7: 8 << 4 = 0x80
	CHECK_EQ(0xA6, (int)WireEncryptSeed(0x1234, 7, false));

	// The English formula multiplies by 51 instead: 8 * 51 = 408,
	// truncated to a uchar after the xor with 0x26.
	CHECK_EQ((int)(uchar)(0x26 ^ 408), (int)WireEncryptSeed(0x1234, 7, true));

	// Zone 0 and server 0 - the answer with no host installed.
	CHECK_EQ(0x10, (int)WireEncryptSeed(0, 0, false));
	CHECK_EQ(51, (int)WireEncryptSeed(0, 0, true));
}

TEST(WireEncryptSeed, TheThreeNonEnglishRegionsAgreedAndStillDo)
{
	// The function this replaced wrote four branches - Netmarble,
	// Chinese, English and a default - of which only the English one
	// differed. Collapsing them is only safe if that is true, so it is
	// asserted rather than asserted-in-a-comment: for every server
	// number a byte can hold, the non-English answer is the shift form.
	for (int server = 0; server < 256; ++server)
	{
		const uchar expected = (uchar)( ( ( 0xBEEF >> 8 ) ^ 0xBEEF ) ^ ( ( server + 1 ) << 4 ) );

		CHECK_EQ((int)expected, (int)WireEncryptSeed(0xBEEF, server, false));
	}
}

//----------------------------------------------------------------------
// The file this slice let into the library
//----------------------------------------------------------------------
TEST(WireHostSeam, TheGameConnectionsObjectIsInTheLibrary)
{
	// The link proof for ClientPlayer.cpp, whose last two game-code
	// includes went behind the host above. Constructing one would open
	// a socket, so this takes addresses instead - but of NON-VIRTUAL
	// members. A pointer to a virtual member is a vtable index and need
	// not reference the defining object at all, so `processCommand`
	// and `disconnect`, which is what this test reached for first,
	// would have proved nothing. Verified by taking the file back out
	// of tests/arch/packetwire_files.txt: with these two the link
	// fails, with the virtual pair it succeeded.
	void (ClientPlayer::*pStatus)(PlayerStatus) = &ClientPlayer::setPlayerStatus;
	Packet* (ClientPlayer::*pOldPacket)(uint) = &ClientPlayer::getOldPacket;

	CHECK(pStatus != NULL);
	CHECK(pOldPacket != NULL);
}

//----------------------------------------------------------------------
// The request-service family's seams
//----------------------------------------------------------------------
TEST(WireHostSeam, TheRequestSeamsAnswerConservativelyWithNoHost)
{
	NoHost	restore;

	Wire::SetHost(NULL);

	// The epoch.
	CHECK_EQ(0, (long long)Wire::CurrentTime().time_since_epoch().count());

	// And no file transfer is registered, which is what a caller
	// asking whether it still has one needs to hear so that it cleans
	// up instead of waiting on a manager that is not there.
	CHECK_EQ(false, Wire::SendOtherRequest("peer", NULL));
	CHECK_EQ(false, Wire::HasOtherRequest("peer"));
	CHECK_EQ(false, Wire::RemoveOtherRequest("peer"));

	// A host that answers nothing answers the same way, one entry at a
	// time - the accessors test the pointer, not just the host. All
	// four, because with s_pHost NULL the first half of each guard
	// short-circuits and the member test never runs: a guard written
	// as `s_pHost->HasOtherRequest==NULL` inside Wire::RemoveOtherRequest
	// would pass everything above. The review round of the fourth slice
	// found half of its eight were only covered the short-circuiting
	// way.
	Wire::SetHost(&s_EmptyHost);
	CHECK_EQ(0, (long long)Wire::CurrentTime().time_since_epoch().count());
	CHECK_EQ(false, Wire::SendOtherRequest("peer", NULL));
	CHECK_EQ(false, Wire::HasOtherRequest("peer"));
	CHECK_EQ(false, Wire::RemoveOtherRequest("peer"));
}

TEST(WireHostSeam, AHostAnswersTheRequestSeamsAndIsAskedTheRightOne)
{
	NoHost	restore;

	s_Now		= MonotonicClock::FromMillis(4321);
	s_RequestAsked.clear();

	Wire::SetHost(&s_Host);

	CHECK_EQ(4321, (long long)Wire::CurrentTime().time_since_epoch().count());

	// Read each time. The request timeouts are differences against this
	// clock, so a value copied once would freeze every one of them.
	s_Now = MonotonicClock::FromMillis(9999);
	CHECK_EQ(9999, (long long)Wire::CurrentTime().time_since_epoch().count());

	// The three file-transfer calls are near-identical in shape (two
	// share a signature), which is exactly how one gets wired to the
	// wrong host entry. Each records its own name, so the test says
	// which was reached rather than only that something was.
	//
	// Read what that does and does not cover. It proves WireHost.cpp's
	// forwarders each call their matching member. It CANNOT see the
	// initialiser actually at risk - s_WireHost in Client/GameInit.cpp -
	// because unit_tests never links the executable. Since the fifth
	// slice that initialiser is designated (C++20), so a transposition
	// there is a compile error rather than something checked by hand;
	// the fourth slice's reviewers had checked its fifteen entries by
	// reading.
	Wire::SendOtherRequest("d", NULL);
	Wire::HasOtherRequest("e");
	Wire::RemoveOtherRequest("f");

	CHECK(s_RequestAsked == "SendOther(d) HasOther(e) RemoveOther(f) ");
}

//----------------------------------------------------------------------
// The files the fourth slice let into the library - the two the
// eighth slice left, the inbound peer side
//----------------------------------------------------------------------
TEST(WireHostSeam, TheRequestServiceObjectsAreInTheLibrary)
{
	// Non-virtual members whose definitions are in the .cpp files, for
	// the reason the ClientPlayer proof below records: a pointer to a
	// virtual member is a vtable index and need not reference the
	// defining object, and an inline member in the header is not in the
	// object at all. Constructing any of these would open a socket.
	uint (RequestServerPlayer::*pSend)(const char*, uint) = &RequestServerPlayer::send;
	void (RequestServerPlayerManager::*pInit)(int) = &RequestServerPlayerManager::Init;
	void (RequestServerPlayerManager::*pWait)() = &RequestServerPlayerManager::WaitRequest;

	CHECK(pSend != NULL);
	CHECK(pInit != NULL);
	CHECK(pWait != NULL);
}
