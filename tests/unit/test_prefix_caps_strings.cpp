//----------------------------------------------------------------------
// test_prefix_caps_strings.cpp
//----------------------------------------------------------------------
//
// The BYTE-length-prefixed strings of the Gpackets, Lpackets and
// Rpackets writers that test_string_write_caps.cpp and
// test_uncapped_string_writes.cpp do not reach: the server-to-client
// and server-to-server packets this client only builds in tests, and
// the client-to-client Rpackets.
//
// Per field: write() refuses every length above its cap and leaves
// only the fields ahead of it behind; a string of exactly the cap
// emits exactly getPacketSize() bytes; and where read() takes the same
// length, the field survives a write/read cycle. The lengths that tell
// a cap checked on the std::string from one checked on the narrowed
// BYTE are 257 and cap + 256: both narrow to a small non-zero length
// that passes every ==0 check and every cap.
//
// The stream packets are driven through the encrypting streams at code
// 0, as in test_string_write_caps.cpp. The datagram packets write into
// a zero-filled Datagram directly, so a refused write is seen as the
// bytes after the expected prefix still being zero.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Datagram.h"
#include "Exception.h"
#include "Packet.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

#include "Gpackets/GCWarScheduleList.h"
#include "Gpackets/GCWhisper.h"
#include "Gpackets/GLIncomingConnectionError.h"
#include "Gpackets/GLIncomingConnectionOK.h"
#include "Lpackets/LCQueryResultCharacterName.h"
#include "Lpackets/LCQueryResultPlayerID.h"
#include "Lpackets/LCReconnect.h"
#include "Lpackets/LCRegisterPlayerOK.h"
#include "Lpackets/LGIncomingConnection.h"
#include "Rpackets/CRConnect.h"
#include "Rpackets/CRRequest.h"
#include "Rpackets/CRWhisper.h"
#include "Rpackets/RCCharacterInfo.h"
#include "Rpackets/RCPositionInfo.h"
#include "Rpackets/RCRequestedFile.h"
#include "Rpackets/RCSay.h"
#include "Rpackets/RCStatusHP.h"

#include <string>
#include <vector>

namespace {

//----------------------------------------------------------------------
// Encrypting streams at code 0 over a never-used socket.
//----------------------------------------------------------------------
struct PrefixOutFixture
{
	Socket				m_Socket;
	SocketEncryptOutputStream	m_Stream;

	PrefixOutFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket)
	{
		m_Stream.setEncryptCode(0);
	}
};

struct PrefixInFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Stream;

	PrefixInFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 4096)
	{
		m_Stream.setEncryptCode(0);
	}
};

//----------------------------------------------------------------------
// One string field of one packet: how to set it (on an otherwise valid
// packet), how to read it back, what write() refuses above, how many
// bytes the fields ahead of it put out, and the longest length read()
// accepts (0 when no round trip applies).
//----------------------------------------------------------------------
template <class PacketT>
struct PrefixCase
{
	void		(*m_pFill)(PacketT&, const std::string&);
	std::string	(*m_pGet)(PacketT&);
	size_t		m_Cap;
	size_t		m_Prefix;
	size_t		m_ReadCap;
};

// Every length above the cap that the checks could confuse: the first
// one over, the ones that narrow to 255, 0 and 1, the one that narrows
// back to the cap itself, and 300.
const size_t	g_OverLengths[] = { 0, 255, 256, 257, 0, 300 };

size_t	OverLength ( size_t i, size_t cap )
{
	if (i == 0)
		return cap + 1;
	if (i == 4)
		return cap + 256;
	return g_OverLengths[i];
}

const size_t	g_NumOverLengths = sizeof(g_OverLengths) / sizeof(g_OverLengths[0]);

//----------------------------------------------------------------------
// Stream writers.
//----------------------------------------------------------------------
template <class PacketT>
void	CheckStreamCap ( const PrefixCase<PacketT> & c )
{
	for (size_t i = 0; i < g_NumOverLengths; i++)
	{
		const size_t	len = OverLength(i, c.m_Cap);
		if (len <= c.m_Cap)
			continue;

		PacketT			packet;
		PrefixOutFixture	out;
		bool			bThrew = false;

		c.m_pFill(packet, std::string(len, 'z'));

		try {
			packet.write(out.m_Stream);
		} catch (InvalidProtocolException&) {
			bThrew = true;
		} catch (Throwable&) {
		}

		CHECK(bThrew);
		CHECK_EQ(c.m_Prefix, (size_t)out.m_Stream.size());
	}

	// Exactly the cap goes out whole, at exactly getPacketSize() bytes.
	{
		PacketT			packet;
		PrefixOutFixture	out;

		c.m_pFill(packet, std::string(c.m_Cap, 'z'));
		packet.write(out.m_Stream);

		CHECK_EQ((size_t)packet.getPacketSize(), (size_t)out.m_Stream.size());
	}

	// And the reader takes back the field that went in.
	if (c.m_ReadCap > 0)
	{
		const size_t	len = c.m_ReadCap < c.m_Cap ? c.m_ReadCap : c.m_Cap;
		const std::string value(len, 'z');

		PacketT			src;
		PrefixOutFixture	out;

		c.m_pFill(src, value);
		src.write(out.m_Stream);

		const std::vector<unsigned char> body =
			SocketOutputStreamTestAccess::Bytes(out.m_Stream);

		PacketT			dst;
		PrefixInFixture		in;

		SocketInputStreamTestAccess::Preload(in.m_Stream,
			body.empty() ? NULL : &body[0], (unsigned int)body.size());

		dst.read(in.m_Stream);

		CHECK(c.m_pGet(dst) == value);
	}
}

//----------------------------------------------------------------------
// Datagram writers. Datagram keeps its write offset private, so the
// packet writes into a zero-filled buffer and the bytes from the prefix
// on must still be zero after a refusal: the length byte of the refused
// field is never zero when it is written, because every over-cap length
// that narrows to 0 is refused already.
//----------------------------------------------------------------------
const uint	g_DatagramBufferLen = 4096;

template <class PacketT>
void	CheckDatagramCap ( const PrefixCase<PacketT> & c )
{
	for (size_t i = 0; i < g_NumOverLengths; i++)
	{
		const size_t	len = OverLength(i, c.m_Cap);
		if (len <= c.m_Cap)
			continue;

		PacketT		packet;
		Datagram	datagram;
		bool		bThrew = false;

		datagram.setData(g_DatagramBufferLen);
		c.m_pFill(packet, std::string(len, 'z'));

		try {
			packet.write(datagram);
		} catch (InvalidProtocolException&) {
			bThrew = true;
		} catch (Throwable&) {
		}

		CHECK(bThrew);

		const char*	pData = datagram.getData();
		size_t		written = 0;
		for (size_t j = c.m_Prefix; j < g_DatagramBufferLen; j++)
			if (pData[j] != 0)
				written++;

		CHECK_EQ((size_t)0, written);

		// The fields ahead of the refused one are there.
		if (c.m_Prefix > 0)
			CHECK(pData[c.m_Prefix - 1] != 0);
	}

	// Exactly the cap goes out whole: Datagram::write(const
	// DatagramPacket*) refuses a body that disagrees with getPacketSize().
	{
		PacketT		packet;
		Datagram	datagram;

		c.m_pFill(packet, std::string(c.m_Cap, 'z'));
		datagram.write(&packet);

		CHECK_EQ((size_t)(szPacketHeader + packet.getPacketSize()),
			(size_t)datagram.getLength());
	}

	// And the reader takes back the field that went in.
	if (c.m_ReadCap > 0)
	{
		const size_t	len = c.m_ReadCap < c.m_Cap ? c.m_ReadCap : c.m_Cap;
		const std::string value(len, 'z');

		PacketT		src;
		Datagram	out;

		c.m_pFill(src, value);
		out.setData((uint)src.getPacketSize());
		src.write(out);

		PacketT		dst;
		Datagram	in;

		in.setData(out.getData(), out.getLength());
		dst.read(in);

		CHECK(c.m_pGet(dst) == value);
	}
}

// One entry of a GCWarScheduleList with every guild name set to "a".
WarScheduleInfo*	NewGuildWarSchedule ()
{
	WarScheduleInfo*	pInfo = new WarScheduleInfo;

	pInfo->warType = 0;
	pInfo->year = 2026;
	pInfo->month = 9;
	pInfo->day = 28;
	pInfo->hour = 20;
	for (int i = 0; i < 5; ++i)
	{
		pInfo->challengerGuildID[i] = (GuildID_t)(i + 1);
		pInfo->challengerGuildName[i] = "a";
	}
	pInfo->reinforceGuildID = 6;
	pInfo->reinforceGuildName = "a";

	return pInfo;
}

std::string	PopWarScheduleName ( GCWarScheduleList & packet, int index )
{
	WarScheduleInfo*	pInfo = packet.popWarScheduleInfo();
	if (pInfo == NULL)
		return std::string();

	const std::string	name = index < 5
		? pInfo->challengerGuildName[index] : pInfo->reinforceGuildName;

	delete pInfo;
	return name;
}

std::string	PopWhisperMessage ( CRWhisper & packet )
{
	if (packet.getMessageSize() == 0)
		return std::string();

	WHISPER_MESSAGE*	pMessage = packet.popMessage();
	const std::string	msg = pMessage->msg;

	delete pMessage;
	return msg;
}

void	FillWhisper ( CRWhisper & p, const std::string & name,
		      const std::string & target, const std::string & msg )
{
	WHISPER_MESSAGE	m;
	m.msg = msg;
	m.color = 0;

	p.setName(name);
	p.setTargetName(target);
	p.addMessage(m);
	p.setRace(RACE_SLAYER);
	p.setWorldID(1);
}

} // namespace

//----------------------------------------------------------------------
// Gpackets and Lpackets on a stream
//----------------------------------------------------------------------

// Caps 10 and 128, what read() and getPacketMaxSize() allow.
TEST(PrefixCapsStrings, GCWhisperName)
{
	const PrefixCase<GCWhisper> c = {
		+[](GCWhisper& p, const std::string& s) {
			p.setName(s); p.setColor(0); p.setMessage("a"); p.setRace(0); },
		+[](GCWhisper& p) { return p.getName(); },
		10, 0, 10 };

	CheckStreamCap(c);
}

TEST(PrefixCapsStrings, GCWhisperMessage)
{
	// The name and the colour are already in the ring.
	const PrefixCase<GCWhisper> c = {
		+[](GCWhisper& p, const std::string& s) {
			p.setName("a"); p.setColor(0); p.setMessage(s); p.setRace(0); },
		+[](GCWhisper& p) { return p.getMessage(); },
		128, szBYTE + 1 + szuint, 128 };

	CheckStreamCap(c);
}

// Cap 20, read()'s bound.
TEST(PrefixCapsStrings, LCQueryResultCharacterName)
{
	const PrefixCase<LCQueryResultCharacterName> c = {
		+[](LCQueryResultCharacterName& p, const std::string& s) {
			p.setCharacterName(s); p.setExist(true); },
		+[](LCQueryResultCharacterName& p) { return p.getCharacterName(); },
		20, 0, 20 };

	CheckStreamCap(c);
}

// Cap 20, read()'s bound.
TEST(PrefixCapsStrings, LCQueryResultPlayerID)
{
	const PrefixCase<LCQueryResultPlayerID> c = {
		+[](LCQueryResultPlayerID& p, const std::string& s) {
			p.setPlayerID(s); p.setExist(true); },
		+[](LCQueryResultPlayerID& p) { return p.getPlayerID(); },
		20, 0, 20 };

	CheckStreamCap(c);
}

// Cap 15, read()'s bound: a dotted IPv4 address.
TEST(PrefixCapsStrings, LCReconnectGameServerIP)
{
	const PrefixCase<LCReconnect> c = {
		+[](LCReconnect& p, const std::string& s) {
			p.setGameServerIP(s); p.setGameServerPort(9998); p.setKey(1); },
		+[](LCReconnect& p) { return p.getGameServerIP(); },
		15, 0, 15 };

	CheckStreamCap(c);
}

// Cap 20, what getPacketMaxSize() allows and the server's writeString
// {1, 20} enforces.
TEST(PrefixCapsStrings, LCRegisterPlayerOKGroupName)
{
	const PrefixCase<LCRegisterPlayerOK> c = {
		+[](LCRegisterPlayerOK& p, const std::string& s) {
			p.setGroupName(s); p.setAdult(true); },
		+[](LCRegisterPlayerOK& p) { return p.getGroupName(); },
		20, 0, 20 };

	CheckStreamCap(c);
}

TEST(PrefixCapsStrings, LCRegisterPlayerOKRefusesAnEmptyGroupName)
{
	LCRegisterPlayerOK	packet;
	PrefixOutFixture	out;
	bool			bThrew = false;

	packet.setGroupName("");
	packet.setAdult(true);

	try {
		packet.write(out.m_Stream);
	} catch (InvalidProtocolException&) {
		bThrew = true;
	} catch (Throwable&) {
	}

	CHECK(bThrew);
	CHECK_EQ((size_t)0, (size_t)out.m_Stream.size());
}

// Cap 16 per guild name, the server's kMaxGuildNameLength. The count
// and the entry's fixed fields are already in the ring, and the guild
// ID of the refused name.
TEST(PrefixCapsStrings, GCWarScheduleListChallengerGuildName)
{
	const PrefixCase<GCWarScheduleList> c = {
		+[](GCWarScheduleList& p, const std::string& s) {
			WarScheduleInfo* pInfo = NewGuildWarSchedule();
			pInfo->challengerGuildName[0] = s;
			p.addWarScheduleInfo(pInfo); },
		+[](GCWarScheduleList& p) { return PopWarScheduleName(p, 0); },
		16, szBYTE + (szBYTE + szWORD + szBYTE + szBYTE + szBYTE) + szGuildID, 16 };

	CheckStreamCap(c);
}

TEST(PrefixCapsStrings, GCWarScheduleListLastChallengerGuildName)
{
	const PrefixCase<GCWarScheduleList> c = {
		+[](GCWarScheduleList& p, const std::string& s) {
			WarScheduleInfo* pInfo = NewGuildWarSchedule();
			pInfo->challengerGuildName[4] = s;
			p.addWarScheduleInfo(pInfo); },
		+[](GCWarScheduleList& p) { return PopWarScheduleName(p, 4); },
		16, szBYTE + (szBYTE + szWORD + szBYTE + szBYTE + szBYTE)
			+ (szGuildID + szBYTE + 1) * 4 + szGuildID, 16 };

	CheckStreamCap(c);
}

TEST(PrefixCapsStrings, GCWarScheduleListReinforceGuildName)
{
	const PrefixCase<GCWarScheduleList> c = {
		+[](GCWarScheduleList& p, const std::string& s) {
			WarScheduleInfo* pInfo = NewGuildWarSchedule();
			pInfo->reinforceGuildName = s;
			p.addWarScheduleInfo(pInfo); },
		+[](GCWarScheduleList& p) { return PopWarScheduleName(p, 5); },
		16, szBYTE + (szBYTE + szWORD + szBYTE + szBYTE + szBYTE)
			+ (szGuildID + szBYTE + 1) * 5 + szGuildID, 16 };

	CheckStreamCap(c);
}

//----------------------------------------------------------------------
// Rpackets on a stream
//----------------------------------------------------------------------

// Caps 10 / 10 / 128, read()'s bounds.
TEST(PrefixCapsStrings, CRWhisperName)
{
	const PrefixCase<CRWhisper> c = {
		+[](CRWhisper& p, const std::string& s) { FillWhisper(p, s, "b", "m"); },
		+[](CRWhisper& p) { return p.getName(); },
		10, 0, 10 };

	CheckStreamCap(c);
}

TEST(PrefixCapsStrings, CRWhisperTargetName)
{
	const PrefixCase<CRWhisper> c = {
		+[](CRWhisper& p, const std::string& s) { FillWhisper(p, "a", s, "m"); },
		+[](CRWhisper& p) { return p.getTargetName(); },
		10, szBYTE + 1, 10 };

	CheckStreamCap(c);
}

TEST(PrefixCapsStrings, CRWhisperMessage)
{
	// Both names and the message count are already in the ring.
	const PrefixCase<CRWhisper> c = {
		+[](CRWhisper& p, const std::string& s) { FillWhisper(p, "a", "b", s); },
		+[](CRWhisper& p) { return PopWhisperMessage(p); },
		128, (szBYTE + 1) * 2 + szBYTE, 128 };

	CheckStreamCap(c);
}

// Cap 10 per name, what getPacketMaxSize() allows.
TEST(PrefixCapsStrings, CRConnectRequestServerName)
{
	const PrefixCase<CRConnect> c = {
		+[](CRConnect& p, const std::string& s) {
			p.setRequestServerName(s.c_str()); p.setRequestClientName("b"); },
		+[](CRConnect& p) { return p.getRequestServerName(); },
		10, 0, 10 };

	CheckStreamCap(c);
}

TEST(PrefixCapsStrings, CRConnectRequestClientName)
{
	const PrefixCase<CRConnect> c = {
		+[](CRConnect& p, const std::string& s) {
			p.setRequestServerName("a"); p.setRequestClientName(s.c_str()); },
		+[](CRConnect& p) { return p.getRequestClientName(); },
		10, szBYTE + 1, 10 };

	CheckStreamCap(c);
}

// Cap 20, read()'s bound. The request code is already in the ring.
TEST(PrefixCapsStrings, CRRequestRequestName)
{
	const PrefixCase<CRRequest> c = {
		+[](CRRequest& p, const std::string& s) {
			p.setCode(CR_REQUEST_FILE_PROFILE); p.setRequestName(s.c_str()); },
		+[](CRRequest& p) { return p.getRequestName(); },
		20, szBYTE, 20 };

	CheckStreamCap(c);
}

// Cap 255, what getPacketMaxSize() budgets the one file name. The file
// type and the version are already in the ring.
TEST(PrefixCapsStrings, RCRequestedFileInfoFilename)
{
	const PrefixCase<RCRequestedFileInfo> c = {
		+[](RCRequestedFileInfo& p, const std::string& s) {
			p.setRequestFileType(REQUEST_FILE_PROFILE); p.setVersion(1);
			p.setFilename(s); p.setFileSize(2); },
		+[](RCRequestedFileInfo& p) { return p.getFilename(); },
		255, szBYTE + szDWORD, 255 };

	CheckStreamCap(c);
}

//----------------------------------------------------------------------
// Datagram packets
//----------------------------------------------------------------------

// Cap 79 per field, read()'s `>= 80` bound.
TEST(PrefixCapsStrings, GLIncomingConnectionErrorMessage)
{
	const PrefixCase<GLIncomingConnectionError> c = {
		+[](GLIncomingConnectionError& p, const std::string& s) {
			p.setMessage(s); p.setPlayerID("b"); },
		+[](GLIncomingConnectionError& p) { return p.getMessage(); },
		79, 0, 79 };

	CheckDatagramCap(c);
}

TEST(PrefixCapsStrings, GLIncomingConnectionErrorPlayerID)
{
	const PrefixCase<GLIncomingConnectionError> c = {
		+[](GLIncomingConnectionError& p, const std::string& s) {
			p.setMessage("a"); p.setPlayerID(s); },
		+[](GLIncomingConnectionError& p) { return p.getPlayerID(); },
		79, szBYTE + 1, 79 };

	CheckDatagramCap(c);
}

// Cap 20, read()'s bound.
TEST(PrefixCapsStrings, GLIncomingConnectionOKPlayerID)
{
	const PrefixCase<GLIncomingConnectionOK> c = {
		+[](GLIncomingConnectionOK& p, const std::string& s) {
			p.setPlayerID(s); p.setTCPPort(9999); p.setKey(1); },
		+[](GLIncomingConnectionOK& p) { return p.getPlayerID(); },
		20, 0, 20 };

	CheckDatagramCap(c);
}

// Caps 20 / 20 / 15, read()'s bounds.
TEST(PrefixCapsStrings, LGIncomingConnectionPlayerID)
{
	const PrefixCase<LGIncomingConnection> c = {
		+[](LGIncomingConnection& p, const std::string& s) {
			p.setPlayerID(s); p.setPCName("b"); p.setClientIP("c"); },
		+[](LGIncomingConnection& p) { return p.getPlayerID(); },
		20, 0, 20 };

	CheckDatagramCap(c);
}

TEST(PrefixCapsStrings, LGIncomingConnectionPCName)
{
	const PrefixCase<LGIncomingConnection> c = {
		+[](LGIncomingConnection& p, const std::string& s) {
			p.setPlayerID("a"); p.setPCName(s); p.setClientIP("c"); },
		+[](LGIncomingConnection& p) { return p.getPCName(); },
		20, szBYTE + 1, 20 };

	CheckDatagramCap(c);
}

TEST(PrefixCapsStrings, LGIncomingConnectionClientIP)
{
	const PrefixCase<LGIncomingConnection> c = {
		+[](LGIncomingConnection& p, const std::string& s) {
			p.setPlayerID("a"); p.setPCName("b"); p.setClientIP(s); },
		+[](LGIncomingConnection& p) { return p.getClientIP(); },
		15, (szBYTE + 1) * 2, 15 };

	CheckDatagramCap(c);
}

// Cap 20 each, read()'s bound. An empty name stays allowed.
TEST(PrefixCapsStrings, RCCharacterInfoName)
{
	const PrefixCase<RCCharacterInfo> c = {
		+[](RCCharacterInfo& p, const std::string& s) {
			p.setName(s); p.setGuildID(7); },
		+[](RCCharacterInfo& p) { return p.getName(); },
		20, 0, 20 };

	CheckDatagramCap(c);
}

TEST(PrefixCapsStrings, RCPositionInfoName)
{
	const PrefixCase<RCPositionInfo> c = {
		+[](RCPositionInfo& p, const std::string& s) {
			p.setName(s); p.setZoneID(1); p.setZoneX(2); p.setZoneY(3); },
		+[](RCPositionInfo& p) { return p.getName(); },
		20, 0, 20 };

	CheckDatagramCap(c);
}

TEST(PrefixCapsStrings, RCStatusHPName)
{
	const PrefixCase<RCStatusHP> c = {
		+[](RCStatusHP& p, const std::string& s) {
			p.setName(s); p.setMaxHP(100); p.setCurrentHP(50); },
		+[](RCStatusHP& p) { return p.getName(); },
		20, 0, 20 };

	CheckDatagramCap(c);
}

// Caps 20 / 128, read()'s bounds. This is the one writer here with a
// live sender: the party 'say' command.
TEST(PrefixCapsStrings, RCSayName)
{
	const PrefixCase<RCSay> c = {
		+[](RCSay& p, const std::string& s) {
			p.setName(s); p.setMessage("m"); p.setColor(0); },
		+[](RCSay& p) { return p.getName(); },
		20, 0, 20 };

	CheckDatagramCap(c);
}

TEST(PrefixCapsStrings, RCSayMessage)
{
	const PrefixCase<RCSay> c = {
		+[](RCSay& p, const std::string& s) {
			p.setName("a"); p.setMessage(s); p.setColor(0); },
		+[](RCSay& p) { return p.getMessage(); },
		128, szBYTE + 1, 128 };

	CheckDatagramCap(c);
}
