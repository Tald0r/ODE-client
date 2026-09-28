//----------------------------------------------------------------------
// test_packet_records_accessor_widths.cpp
//----------------------------------------------------------------------
//
// Packet accessors are as wide as the field they return: a getter over
// a WORD or DWORD member that the packet reads and writes at full width
// hands back the whole value, and a setter for a BYTE field takes a
// BYTE. Each type here matches the server's copy of the same packet.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Exception.h"
#include "GuildInfo.h"
#include "Packet.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"

#include "Cpackets/CGAbsorbSoul.h"
#include "Gpackets/GCActiveGuildList.h"
#include "Gpackets/GCExecuteElement.h"
#include "Gpackets/GCGuildResponse.h"
#include "Gpackets/GCNoticeEvent.h"
#include "Gpackets/GCRequestFailed.h"
#include "Gpackets/GCSkillToInventoryOK2.h"
#include "Gpackets/GCSkillToTileOK3.h"

#include <type_traits>
#include <vector>

namespace {

//----------------------------------------------------------------------
// An input stream over a never-used socket (see
// tests/support/packet_stream_access.h), preloaded with a body.
//----------------------------------------------------------------------
struct WidthInFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Stream;

	WidthInFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 4096)
	{
		m_Stream.setEncryptCode(0);
	}

	void	Preload ( const std::vector<unsigned char> & body )
	{
		SocketInputStreamTestAccess::Preload(m_Stream,
			body.empty() ? NULL : &body[0], (unsigned int)body.size());
	}
};

//----------------------------------------------------------------------
// write() src and read() the bytes back into dst.
//----------------------------------------------------------------------
template <class PacketT>
void	WriteThenRead ( const PacketT & src, PacketT & dst )
{
	Socket				socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketEncryptOutputStream	out(&socket);
	out.setEncryptCode(0);

	src.write(out);

	WidthInFixture	in;
	in.Preload(SocketOutputStreamTestAccess::Bytes(out));

	dst.read(in.m_Stream);

	CHECK_EQ(0, (long long)in.m_Stream.length());
}

} // namespace

//----------------------------------------------------------------------
// CGAbsorbSoul: the target zone coordinates are ZoneCoord_t on the wire.
//----------------------------------------------------------------------
TEST(PacketRecordsAccessorWidths, AbsorbSoulTargetZoneIsAZoneCoord)
{
	CGAbsorbSoul	packet;
	packet.setTargetZoneX(300);
	packet.setTargetZoneY(511);

	CHECK_EQ(300, (long long)packet.getTargetZoneX());
	CHECK_EQ(511, (long long)packet.getTargetZoneY());
	CHECK((std::is_same<decltype(packet.getTargetZoneX()), ZoneCoord_t>::value));
	CHECK((std::is_same<decltype(packet.getTargetZoneY()), ZoneCoord_t>::value));
}

//----------------------------------------------------------------------
// GCExecuteElement: the quest ID is a DWORD on the wire.
//----------------------------------------------------------------------
TEST(PacketRecordsAccessorWidths, ExecuteElementQuestIDIsADWORD)
{
	// QuestID 0x00010005, condition 1, index 2, little-endian.
	const std::vector<unsigned char> body = {
		0x05, 0x00, 0x01, 0x00,
		0x01,
		0x02, 0x00 };

	WidthInFixture		in;
	in.Preload(body);

	GCExecuteElement	packet;
	packet.read(in.m_Stream);

	CHECK_EQ(0x00010005, (long long)packet.getQuestID());
	CHECK_EQ(1, (long long)packet.getCondition());
	CHECK_EQ(2, (long long)packet.getIndex());
}

//----------------------------------------------------------------------
// GCGuildResponse and GCNoticeEvent: the code is a WORD on the wire,
// and GCNoticeEvent's read() decides on the whole WORD whether a
// parameter follows, so its handler has to dispatch on the same value.
//----------------------------------------------------------------------
TEST(PacketRecordsAccessorWidths, GuildResponseCodeIsAWORD)
{
	GCGuildResponse	src;
	src.setCode(0x0105);
	src.setParameter(9);

	CHECK_EQ(0x0105, (long long)src.getCode());

	GCGuildResponse	dst;
	WriteThenRead(src, dst);

	CHECK_EQ(0x0105, (long long)dst.getCode());
}

TEST(PacketRecordsAccessorWidths, NoticeEventCodeIsAWORD)
{
	GCNoticeEvent	src;
	src.setCode(0x0105);

	CHECK_EQ(0x0105, (long long)src.getCode());

	GCNoticeEvent	dst;
	WriteThenRead(src, dst);

	CHECK_EQ(0x0105, (long long)dst.getCode());
}

//----------------------------------------------------------------------
// GCActiveGuildList: the list count is a WORD on the wire.
//----------------------------------------------------------------------
TEST(PacketRecordsAccessorWidths, ActiveGuildListCountIsAWORD)
{
	GCActiveGuildList	packet;
	for (int i = 0; i < 256; i++)
		packet.addGuildInfo(new GuildInfo());

	CHECK_EQ(256, (long long)packet.getListNum());
}

//----------------------------------------------------------------------
// GCRequestFailed: the code is a BYTE on the wire, so its setter takes
// one instead of narrowing a WORD. No value tells the two apart at run
// time; the type does.
//----------------------------------------------------------------------
TEST(PacketRecordsAccessorWidths, RequestFailedSetCodeTakesABYTE)
{
	CHECK((std::is_same<decltype(&GCRequestFailed::setCode),
		void (GCRequestFailed::*)(BYTE) noexcept>::value));

	GCRequestFailed	packet;
	packet.setCode(REQUEST_FAILED_NULL);
	CHECK_EQ(REQUEST_FAILED_NULL, (long long)packet.getCode());
}

//----------------------------------------------------------------------
// GCSkillToInventoryOK2 and GCSkillToTileOK3: the caster's object ID is
// an ObjectID_t, and the handlers look the creature up by it. The
// server hands out object IDs above 65535 on a long-running zone.
//----------------------------------------------------------------------
TEST(PacketRecordsAccessorWidths, SkillToInventoryOK2ObjectIDIsAnObjectID)
{
	GCSkillToInventoryOK2	src;
	src.setObjectID(65537);
	src.setSkillType(1);
	src.setDuration(2);

	CHECK_EQ(65537, (long long)src.getObjectID());

	GCSkillToInventoryOK2	dst;
	src.setObjectID(70001);
	WriteThenRead(src, dst);

	CHECK_EQ(70001, (long long)dst.getObjectID());
}

TEST(PacketRecordsAccessorWidths, SkillToTileOK3ObjectIDIsAnObjectID)
{
	GCSkillToTileOK3	src;
	src.setObjectID(65537);
	src.setSkillType(1);
	src.setX(2);
	src.setY(3);
	src.setGrade(4);

	CHECK_EQ(65537, (long long)src.getObjectID());

	GCSkillToTileOK3	dst;
	src.setObjectID(70001);
	WriteThenRead(src, dst);

	CHECK_EQ(70001, (long long)dst.getObjectID());
}
