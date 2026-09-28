//----------------------------------------------------------------------
// test_prefix_caps_counts.cpp
//----------------------------------------------------------------------
//
// The BYTE element counts of the Gpackets, Lpackets and Rpackets list
// writers. Each write() narrows the list's size to the count byte and
// then writes every element, so a list of 256 went out behind a count
// of 0 and the reader parsed the entries as the packet that follows.
//
// Per list: write() refuses one over the cap, 256 and 300 entries and
// leaves only the fields ahead of the count behind; a list of exactly
// the cap emits exactly getPacketSize() bytes and reads back whole.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Exception.h"
#include "Packet.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

#include "RaceWarInfo.h"
#include "ServerGroupInfo.h"
#include "WorldInfo.h"
#include "Gpackets/GCWarList.h"
#include "Gpackets/GCWarScheduleList.h"
#include "Lpackets/LCServerList.h"
#include "Lpackets/LCWorldList.h"
#include "Rpackets/CRWhisper.h"
#include "Rpackets/RCRequestedFile.h"

#include <string>
#include <vector>

namespace {

//----------------------------------------------------------------------
// Encrypting streams at code 0 over a never-used socket, as in
// test_string_write_caps.cpp. The input ring is sized for the largest
// at-cap list here, CRWhisper's 255 messages.
//----------------------------------------------------------------------
struct CountOutFixture
{
	Socket				m_Socket;
	SocketEncryptOutputStream	m_Stream;

	CountOutFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket)
	{
		m_Stream.setEncryptCode(0);
	}
};

struct CountInFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Stream;

	CountInFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 8192)
	{
		m_Stream.setEncryptCode(0);
	}
};

//----------------------------------------------------------------------
// One list of one packet: how to build a valid packet with n entries,
// how many entries the reader took back, the cap write() refuses above,
// and the bytes written ahead of the count.
//----------------------------------------------------------------------
template <class PacketT>
struct CountCase
{
	void		(*m_pFill)(PacketT&, size_t);
	size_t		(*m_pCount)(PacketT&);
	size_t		m_Cap;
	size_t		m_Prefix;
};

template <class PacketT>
void	CheckCountCap ( const CountCase<PacketT> & c )
{
	const size_t	counts[] = { c.m_Cap + 1, 256, 300 };

	for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); i++)
	{
		if (counts[i] <= c.m_Cap)
			continue;

		PacketT			packet;
		CountOutFixture		out;
		bool			bThrew = false;

		c.m_pFill(packet, counts[i]);

		try {
			packet.write(out.m_Stream);
		} catch (InvalidProtocolException&) {
			bThrew = true;
		} catch (Throwable&) {
		}

		CHECK(bThrew);
		CHECK_EQ(c.m_Prefix, (size_t)out.m_Stream.size());
	}

	// Exactly the cap goes out whole and reads back whole.
	{
		PacketT			src;
		CountOutFixture		out;

		c.m_pFill(src, c.m_Cap);
		src.write(out.m_Stream);

		CHECK_EQ((size_t)src.getPacketSize(), (size_t)out.m_Stream.size());

		const std::vector<unsigned char> body =
			SocketOutputStreamTestAccess::Bytes(out.m_Stream);

		PacketT			dst;
		CountInFixture		in;

		SocketInputStreamTestAccess::Preload(in.m_Stream,
			body.empty() ? NULL : &body[0], (unsigned int)body.size());

		dst.read(in.m_Stream);

		CHECK_EQ(c.m_Cap, c.m_pCount(dst));
	}
}

} // namespace

// Cap 24, the server's GCWarList::kMaxWars.
TEST(PrefixCapsCounts, GCWarListWars)
{
	const CountCase<GCWarList> c = {
		+[](GCWarList& p, size_t n) {
			for (size_t i = 0; i < n; i++)
				p.addWarInfo(new RaceWarInfo); },
		+[](GCWarList& p) { return (size_t)p.getSize(); },
		24, 0 };

	CheckCountCap(c);
}

// Cap MAX_WAR_NUM (20), what getPacketMaxSize() sizes the list for.
TEST(PrefixCapsCounts, GCWarScheduleListSchedules)
{
	const CountCase<GCWarScheduleList> c = {
		+[](GCWarScheduleList& p, size_t n) {
			for (size_t i = 0; i < n; i++)
			{
				WarScheduleInfo* pInfo = new WarScheduleInfo;
				pInfo->warType = 1;
				pInfo->year = 2026;
				pInfo->month = 9;
				pInfo->day = 28;
				pInfo->hour = 20;
				p.addWarScheduleInfo(pInfo);
			} },
		+[](GCWarScheduleList& p) {
			size_t n = 0;
			while (WarScheduleInfo* pInfo = p.popWarScheduleInfo())
			{
				delete pInfo;
				n++;
			}
			return n; },
		MAX_WAR_NUM, 0 };

	CheckCountCap(c);
}

// Cap 37, the multiplier in ServerGroupInfo::getMaxSize(). The current
// group ID is already in the ring.
TEST(PrefixCapsCounts, LCServerListGroups)
{
	const CountCase<LCServerList> c = {
		+[](LCServerList& p, size_t n) {
			p.setCurrentServerGroupID(1);
			for (size_t i = 0; i < n; i++)
			{
				ServerGroupInfo* pInfo = new ServerGroupInfo;
				pInfo->setGroupID((ServerGroupID_t)i);
				pInfo->setGroupName("a");
				pInfo->setStat(0);
				p.addListElement(pInfo);
			} },
		+[](LCServerList& p) { return (size_t)p.getListNum(); },
		37, szServerGroupID };

	CheckCountCap(c);
}

// Cap 37, the multiplier in WorldInfo::getMaxSize(). The current world
// ID is already in the ring.
TEST(PrefixCapsCounts, LCWorldListWorlds)
{
	const CountCase<LCWorldList> c = {
		+[](LCWorldList& p, size_t n) {
			p.setCurrentWorldID(1);
			for (size_t i = 0; i < n; i++)
			{
				WorldInfo* pInfo = new WorldInfo;
				pInfo->setID((WorldID_t)i);
				pInfo->setName("a");
				pInfo->setStat(0);
				p.addListElement(pInfo);
			} },
		+[](LCWorldList& p) { return (size_t)p.getListNum(); },
		37, szWorldID };

	CheckCountCap(c);
}

// Cap 255, all a count byte can say. Both names are already in the ring.
TEST(PrefixCapsCounts, CRWhisperMessages)
{
	const CountCase<CRWhisper> c = {
		+[](CRWhisper& p, size_t n) {
			WHISPER_MESSAGE m;
			m.msg = "a";
			m.color = 0;
			p.setName("a");
			p.setTargetName("b");
			for (size_t i = 0; i < n; i++)
				p.addMessage(m);
			p.setRace(RACE_SLAYER);
			p.setWorldID(1); },
		+[](CRWhisper& p) { return (size_t)p.getMessageSize(); },
		255, (szBYTE + 1) * 2 };

	CheckCountCap(c);
}

// Cap 1, what getPacketMaxSize() budgets and the one producer sends.
TEST(PrefixCapsCounts, RCRequestedFileInfos)
{
	const CountCase<RCRequestedFile> c = {
		+[](RCRequestedFile& p, size_t n) {
			for (size_t i = 0; i < n; i++)
			{
				RCRequestedFileInfo* pInfo = new RCRequestedFileInfo;
				pInfo->setRequestFileType(REQUEST_FILE_PROFILE);
				pInfo->setVersion(1);
				pInfo->setFilename("a");
				pInfo->setFileSize(2);
				p.addInfo(pInfo);
			} },
		+[](RCRequestedFile& p) { return (size_t)p.getListNum(); },
		1, 0 };

	CheckCountCap(c);
}
