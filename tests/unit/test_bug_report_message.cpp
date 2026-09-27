//----------------------------------------------------------------------
// test_bug_report_message.cpp
//----------------------------------------------------------------------
//
// What a bug report reaches the server as. SendBugReport
// (Client/Packet/WireHost.cpp) formats into a 256-byte buffer, cuts the
// text, prefixes "*bug_report " and sends a CGSay; a CGSay message is
// 128 bytes, the prefix spends 12, so 116 is left for the report.
//
// A Player subclass keeps the packet instead of writing a socket.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Exception.h"
#include "Packet.h"
#include "Player.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"
#include "WireHost.h"
#include "Cpackets/CGSay.h"

#include <string>

namespace {

// Spelled out rather than read through the constant the code under test
// reads, which would not see either number move.
const std::string	PREFIX		= "*bug_report ";

// CGSay::read() and CGSay::write() both throw above this.
const int		SAY_MESSAGE_MAX	= 128;

// What the prefix leaves of it: 128 - 12.
const int		TEXT_MAX	= 116;

// A target that keeps the packet instead of writing a socket.
class CapturingTarget : public Player
{
public:

	CapturingTarget ()
	: m_nSent(0), m_Color(0xFFFFFFFF)
	{
	}

	void sendPacket ( Packet * pPacket )
	{
		m_nSent++;

		if (pPacket == NULL || pPacket->getPacketID() != Packet::PACKET_CG_SAY)
			return;

		const CGSay * pSay = static_cast<const CGSay*>(pPacket);

		m_Message	= pSay->getMessage();
		m_Color		= pSay->getColor();
	}

	int		m_nSent;
	uint		m_Color;
	std::string	m_Message;
};

CapturingTarget *	s_pTarget = NULL;

Player *	HostBugReportTarget ()	{ return s_pTarget; }

// Only the one entry is filled: every other accessor answers its
// documented default with a NULL member, and nothing here asks.
const WireHost	s_Host = {
	.MaxProcessPacket		= NULL,
	.MaxRequestService		= NULL,
	.ClientCommunicationUDPPort	= NULL,
	.BugReportTarget		= HostBugReportTarget,
	.EncryptZoneID			= NULL,
	.EncryptServerID		= NULL,
	.EncryptUsesEnglishSeed		= NULL,
	.CurrentTime			= NULL,
	.SendOtherRequest		= NULL,
	.HasOtherRequest		= NULL,
	.RemoveOtherRequest		= NULL,
};

// Puts the library back however the test leaves.
struct NoHost
{
	~NoHost()	{ Wire::SetHost(NULL); s_pTarget = NULL; }
};

// A stream over a never-used socket (see packet_stream_access.h).
struct OutFixture
{
	Socket			m_Socket;
	SocketOutputStream	m_Stream;

	OutFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket)
	{
	}
};

// The report SendBugReport builds for one text, as the target sees it.
std::string	ReportFor ( const std::string & text )
{
	// Declared first, so NoHost's destructor takes the pointer back
	// before the object it points at goes away.
	CapturingTarget	target;
	NoHost		restore;

	s_pTarget = &target;
	Wire::SetHost(&s_Host);

	SendBugReport("%s", text.c_str());

	CHECK_EQ(1, target.m_nSent);
	CHECK_EQ(0, (int)target.m_Color);

	return target.m_Message;
}

// Whether a CGSay will actually carry that message: one byte over the
// cap is thrown, not truncated.
bool	ACGSayCarries ( const std::string & message )
{
	OutFixture	out;
	CGSay		say;

	say.setColor(0);
	say.setMessage(message);

	try
	{
		say.write(out.m_Stream);
	}
	catch (Throwable&)
	{
		return false;
	}

	return true;
}

// A text of n bytes, distinguishable by position.
std::string	TextOf ( int n )
{
	std::string	text;

	for (int i = 0; i < n; i++)
		text += (char)('a' + (i % 26));

	return text;
}

} // namespace

//----------------------------------------------------------------------
// Nothing worth saying is still not sent
//----------------------------------------------------------------------
TEST(BugReport, AReportOfNoCharactersOrOneIsNotSent)
{
	CapturingTarget	target;
	NoHost		restore;

	// The arithmetic the rest of this file rests on, spelled once.
	CHECK_EQ(12, (int)PREFIX.size());
	CHECK_EQ(TEXT_MAX, SAY_MESSAGE_MAX - (int)PREFIX.size());

	s_pTarget = &target;
	Wire::SetHost(&s_Host);

	// No format at all.
	SendBugReport(NULL);
	CHECK_EQ(0, target.m_nSent);

	// The length test is `<= 1`, so neither of these survives it.
	SendBugReport("");
	SendBugReport("x");
	CHECK_EQ(0, target.m_nSent);

	// Two characters is a report.
	SendBugReport("xy");
	CHECK_EQ(1, target.m_nSent);
	CHECK(target.m_Message == PREFIX + "xy");
}

//----------------------------------------------------------------------
// A report short enough goes out whole
//----------------------------------------------------------------------
TEST(BugReport, AShortReportIsPrefixedAndNotCut)
{
	const std::string	report = ReportFor("something went wrong");

	CHECK(report == PREFIX + "something went wrong");
	CHECK_EQ((int)PREFIX.size() + 20, (int)report.size());
	CHECK(ACGSayCarries(report));

	// The arguments are formatted, not the format string sent.
	CapturingTarget	target;
	NoHost		restore;

	s_pTarget = &target;
	Wire::SetHost(&s_Host);

	SendBugReport("Exceed PacketID:%d", 4321);

	CHECK_EQ(1, target.m_nSent);
	CHECK(target.m_Message == PREFIX + "Exceed PacketID:4321");
}

//----------------------------------------------------------------------
// The cut is where the packet ends, not before it
//----------------------------------------------------------------------
TEST(BugReport, TheTextIsCutOnlyWhereACGSayMessageEnds)
{
	// One byte under the headroom, exactly on it, and one over.
	const std::string	under	= TextOf(TEXT_MAX - 1);
	const std::string	exact	= TextOf(TEXT_MAX);
	const std::string	over	= TextOf(TEXT_MAX + 1);

	CHECK(ReportFor(under) == PREFIX + under);
	CHECK(ReportFor(exact) == PREFIX + exact);
	CHECK(ReportFor(over) == PREFIX + exact);

	// Which is a full CGSay message, to the byte.
	CHECK_EQ(SAY_MESSAGE_MAX, (int)ReportFor(exact).size());
	CHECK_EQ(SAY_MESSAGE_MAX, (int)ReportFor(over).size());

	// A long report keeps the first TEXT_MAX bytes and nothing after.
	const std::string	report = ReportFor(TextOf(200));

	CHECK_EQ(SAY_MESSAGE_MAX, (int)report.size());
	CHECK(report == PREFIX + TextOf(TEXT_MAX));
	CHECK(report.compare(0, PREFIX.size(), PREFIX) == 0);
}

//----------------------------------------------------------------------
// And never further than the packet reaches
//----------------------------------------------------------------------
TEST(BugReport, NoReportOutgrowsThePacketThatCarriesIt)
{
	// Either side of every boundary in the path.
	const int	lengths[] = { 2, 3, 50, 114, 115, 116, 117, 118,
					127, 128, 129, 200, 254, 255 };

	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
	{
		const std::string	report = ReportFor(TextOf(lengths[i]));

		CHECK(report.size() <= (size_t)SAY_MESSAGE_MAX);
		CHECK(ACGSayCarries(report));
	}
}

//----------------------------------------------------------------------
// A report at a site carries the site in front
//----------------------------------------------------------------------
TEST(BugReport, AReportAtASiteCarriesTheSiteInFront)
{
	CapturingTarget	target;
	NoHost		restore;

	s_pTarget = &target;
	Wire::SetHost(&s_Host);

	// An explicit site: the "[file,line] " the callers used to format.
	SendBugReportAt(DiagnosticSite("Some.cpp", 42), "%d,%d", 7, 9);

	CHECK_EQ(1, target.m_nSent);
	CHECK(target.m_Message == PREFIX + "[Some.cpp,42] 7,9");

	// The defaulted site is the caller's line, not the header's.
	const int line = __LINE__ + 1;
	SendBugReportAt(DiagnosticSite(), "%d", 10);

	CHECK_EQ(2, target.m_nSent);
	CHECK(target.m_Message == PREFIX + "[" + __FILE__ + "," + std::to_string(line) + "] 10");

	// Nothing to format: nothing sent, as SendBugReport does.
	SendBugReportAt(DiagnosticSite("Some.cpp", 42), NULL);
	CHECK_EQ(2, target.m_nSent);
}

//----------------------------------------------------------------------
// The cut counts the prefix, and the short-text rule reads the text
//----------------------------------------------------------------------
TEST(BugReport, AReportAtASiteIsCutWithThePrefixInTheCount)
{
	CapturingTarget	target;
	NoHost		restore;

	s_pTarget = &target;
	Wire::SetHost(&s_Host);

	// "[x,1] " is six bytes of the TEXT_MAX the message carries.
	SendBugReportAt(DiagnosticSite("x", 1), "%s", TextOf(200).c_str());

	CHECK_EQ(1, target.m_nSent);
	CHECK_EQ(SAY_MESSAGE_MAX, (int)target.m_Message.size());
	CHECK(target.m_Message == PREFIX + "[x,1] " + TextOf(TEXT_MAX - 6));
	CHECK(ACGSayCarries(target.m_Message));

	// A text of no characters or one is not sent, prefix or none.
	SendBugReportAt(DiagnosticSite("x", 1), "");
	SendBugReportAt(DiagnosticSite("x", 1), "x");
	CHECK_EQ(1, target.m_nSent);
	SendBugReportAt(DiagnosticSite("x", 1), "xy");
	CHECK_EQ(2, target.m_nSent);
	CHECK(target.m_Message == PREFIX + "[x,1] xy");
}

TEST(BugReport, BothTypedEntryPointsRefuseUnprovidedArgumentsBeforePacketDelivery)
{
	CapturingTarget target;
	NoHost restore;
	s_pTarget = &target;
	Wire::SetHost(&s_Host);
	SendBugReport("%s %d %s", 42);
	CHECK(target.m_Message == PREFIX + "%s 42 %s");
	SendBugReportAt(DiagnosticSite("where.cpp", 7), "%s", "literal %s %n %%");
	CHECK(target.m_Message == PREFIX + "[where.cpp,7] literal %s %n %%");
	CHECK_EQ(2, target.m_nSent);
	CHECK(ACGSayCarries(target.m_Message));
}
