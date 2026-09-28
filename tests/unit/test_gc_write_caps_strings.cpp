//----------------------------------------------------------------------
// test_gc_write_caps_strings.cpp
//----------------------------------------------------------------------
//
// Server-to-client packet write()s that put a BYTE-length-prefixed
// string on the wire, over the lengths a length byte cannot express.
// The client only reads these packets live; write() runs in tests and
// goldens, and it must refuse what the server's copy refuses
// (opendarkeden-server src/Core, de::wire::writeString) instead of
// narrowing the length first and emitting a prefix that disagrees with
// the body.
//
// Per field: write() refuses everything above the cap and leaves only
// the fields ahead of it in the ring; a string of exactly the cap emits
// exactly the body size the packet reports; and where read() accepts
// the same length, the field survives a write/read cycle.
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

#include "Gpackets/GCBloodBibleStatus.h"
#include "Gpackets/GCModifyGuildMemberInfo.h"
#include "Gpackets/GCNPCSayDynamic.h"
#include "Gpackets/GCNotifyWin.h"
#include "Gpackets/GCSystemMessage.h"

#include <string>
#include <vector>

namespace {

//----------------------------------------------------------------------
// Streams over a never-used socket (see tests/support/packet_stream_access.h),
// at encrypt code 0 so the bytes are the plain body.
//----------------------------------------------------------------------
struct GCCapOutFixture
{
	Socket				m_Socket;
	SocketEncryptOutputStream	m_Stream;

	GCCapOutFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket)
	{
		m_Stream.setEncryptCode(0);
	}
};

struct GCCapInFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Stream;

	GCCapInFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 4096)
	{
		m_Stream.setEncryptCode(0);
	}
};

//----------------------------------------------------------------------
// The body size an object reports for itself.
//----------------------------------------------------------------------
size_t	BodySize ( const Packet & packet )
{
	return packet.getPacketSize();
}

//----------------------------------------------------------------------
// One string field of one object. m_pFill builds a whole valid object
// with the field under test set to the given string; m_pGet reads that
// same field back. m_Prefix is what write() has already put in the ring
// when it refuses - the fields ahead of this one.
//----------------------------------------------------------------------
template <class T>
struct GCCapCase
{
	void		(*m_pFill)(T&, const std::string&);
	std::string	(*m_pGet)(T&);

	// The longest string write() accepts; at most 255.
	size_t		m_Cap;

	// Bytes in the output ring when it throws.
	size_t		m_Prefix;

	// The longest string read() accepts for this field.
	size_t		m_ReadCap;
};

//----------------------------------------------------------------------
// Writes one object into a fresh ring. Returns whether write() threw
// InvalidProtocolException, and the ring's size either way.
//----------------------------------------------------------------------
template <class T>
bool	WriteThrows ( const T & object, size_t & ringSize )
{
	GCCapOutFixture	out;
	bool		bThrew = false;

	try {
		object.write(out.m_Stream);
	} catch (InvalidProtocolException&) {
		bThrew = true;
	}

	ringSize = (size_t)out.m_Stream.size();
	return bThrew;
}

//----------------------------------------------------------------------
// The lengths that must be refused, and the one that must not.
//----------------------------------------------------------------------
template <class T>
void	CheckGCStringWriteCap ( const GCCapCase<T> & c )
{
	// 256 wraps to 0, 257 to 1, 256 + cap to cap: the window the
	// narrowing hid. 300 and 511 are plain overlong strings.
	const size_t	lengths[] = { c.m_Cap + 1, 256, 257, 256 + c.m_Cap, 300, 511 };

	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
	{
		T	object;
		size_t	ringSize = 0;

		c.m_pFill(object, std::string(lengths[i], 'z'));

		CHECK(WriteThrows(object, ringSize));
		CHECK_EQ(c.m_Prefix, ringSize);
	}

	// Exactly the cap goes out whole, and the body is as long as the
	// object says it is.
	{
		T	object;
		size_t	ringSize = 0;

		c.m_pFill(object, std::string(c.m_Cap, 'z'));

		CHECK(!WriteThrows(object, ringSize));
		CHECK_EQ(BodySize(object), ringSize);
	}

	// And the reader takes the same field back out of those bytes.
	{
		const size_t	len = c.m_ReadCap < c.m_Cap ? c.m_ReadCap : c.m_Cap;
		const std::string value(len, 'z');

		T		src;
		GCCapOutFixture	out;

		c.m_pFill(src, value);
		src.write(out.m_Stream);

		const std::vector<unsigned char> body =
			SocketOutputStreamTestAccess::Bytes(out.m_Stream);

		T		dst;
		GCCapInFixture	in;

		SocketInputStreamTestAccess::Preload(in.m_Stream,
			body.empty() ? NULL : &body[0], (unsigned int)body.size());

		dst.read(in.m_Stream);

		CHECK(c.m_pGet(dst) == value);
	}
}

} // namespace

//----------------------------------------------------------------------
// One string per packet, bounded by the length byte alone
//----------------------------------------------------------------------
TEST(GCStringWriteCaps, GCBloodBibleStatusOwnerName)
{
	const GCCapCase<GCBloodBibleStatus> c = {
		+[](GCBloodBibleStatus& p, const std::string& s) {
			p.setItemType(1); p.setZoneID(2); p.setStorage(0);
			p.setRace(0); p.setShrineRace(1); p.setX(3); p.setY(4);
			p.setOwnerName(s); },
		+[](GCBloodBibleStatus& p) { return std::string(p.getOwnerName()); },
		255,
		szItemType + szZoneID + szStorage + szRace + szRace + szZoneCoord + szZoneCoord,
		255 };

	CheckGCStringWriteCap(c);
}

TEST(GCStringWriteCaps, GCModifyGuildMemberInfoGuildName)
{
	const GCCapCase<GCModifyGuildMemberInfo> c = {
		+[](GCModifyGuildMemberInfo& p, const std::string& s) {
			p.setGuildID(1); p.setGuildMemberRank(2); p.setGuildName(s); },
		+[](GCModifyGuildMemberInfo& p) { return p.getGuildName(); },
		255, szGuildID, 30 };

	CheckGCStringWriteCap(c);
}

TEST(GCStringWriteCaps, GCNPCSayDynamicMessage)
{
	const GCCapCase<GCNPCSayDynamic> c = {
		+[](GCNPCSayDynamic& p, const std::string& s) {
			p.setObjectID(1); p.setMessage(s); },
		+[](GCNPCSayDynamic& p) { return p.getMessage(); },
		255, szObjectID, 255 };

	CheckGCStringWriteCap(c);
}

TEST(GCStringWriteCaps, GCNotifyWinName)
{
	const GCCapCase<GCNotifyWin> c = {
		+[](GCNotifyWin& p, const std::string& s) { p.setGiftID(1); p.setName(s); },
		+[](GCNotifyWin& p) { return p.getName(); },
		255, szDWORD, 255 };

	CheckGCStringWriteCap(c);
}

TEST(GCStringWriteCaps, GCSystemMessageMessage)
{
	const GCCapCase<GCSystemMessage> c = {
		+[](GCSystemMessage& p, const std::string& s) {
			p.setMessage(s); p.setColor(0); p.setType(SYSTEM_MESSAGE_NORMAL); },
		+[](GCSystemMessage& p) { return p.getMessage(); },
		255, 0, 255 };

	CheckGCStringWriteCap(c);
}

//----------------------------------------------------------------------
// An empty message is refused before its length byte goes out.
//----------------------------------------------------------------------
TEST(GCStringWriteCaps, GCSystemMessageEmptyLeavesNothing)
{
	GCSystemMessage	packet;
	size_t		ringSize = 0;

	packet.setMessage("");
	packet.setColor(0);
	packet.setType(SYSTEM_MESSAGE_NORMAL);

	CHECK(WriteThrows(packet, ringSize));
	CHECK_EQ(0, ringSize);
}
