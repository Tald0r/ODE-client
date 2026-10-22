//----------------------------------------------------------------------
// test_wire_widths.cpp
//----------------------------------------------------------------------
//
// The C++ `long` is 32 bits under MSVC and 64 bits on LP64 Linux and
// macOS, and the packet protocol was written against the former: every
// `long`/`ulong` field on the wire is four bytes. This pins that on
// every platform, at every place a long reaches bytes - the socket
// streams, the encrypting streams and the datagram - and pins the
// 64-bit types at eight (docs/build/linux-macos-port-assessment-2026-09-07.md,
// area C). A build where any of these moves has changed the protocol,
// which the goldens would also catch, but only for the packets they
// cover; this covers the primitives.
//
// What it cannot pin: `ulong` (unsigned long) is the LP64 uint64_t, so
// readWire(ulong&) is a legal 8-byte scalar there and a rejected type
// under MSVC - the Windows build is what keeps a direct readWire(ulong)
// out of the tree; the narrowing read(ulong&)/write(ulong) overloads
// tested here are the sanctioned path.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Datagram.h"
#include "Socket.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "SocketImpl.h"
#include "WireScalar.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

// The size constants the packet classes size themselves with are the
// 32-bit ones whatever sizeof(long) is.
static_assert(szlong == 4, "szlong is the wire width of long, four bytes");
static_assert(szulong == 4, "szulong is the wire width of ulong, four bytes");
static_assert(sizeof(ulonglong) == 8, "ulonglong is eight bytes");

// ulonglong is `unsigned long long` off MSVC, a different type from the
// LP64 uint64_t (unsigned long) even though both are eight bytes; the
// exchange packets pass it to readWire/writeWire directly.
static_assert(packetwire::WireScalar<ulonglong>);
static_assert(packetwire::WireScalar<long long>);
static_assert(packetwire::WireScalar<unsigned long long>);

namespace {

struct WidthFixture
{
	Socket				m_Socket;
	SocketInputStream		m_Input;
	SocketOutputStream		m_Output;

	WidthFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Input(&m_Socket, 64),
	  m_Output(&m_Socket, 64)
	{
	}
};

struct EncryptedWidthFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Input;
	SocketEncryptOutputStream	m_Output;

	EncryptedWidthFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Input(&m_Socket, 64),
	  m_Output(&m_Socket, 64)
	{
	}
};

void CheckBytes(const std::vector<unsigned char>& actual,
		const unsigned char* expected, std::size_t len)
{
	CHECK_EQ(len, actual.size());
	if (actual.size() != len)
		return;
	for (std::size_t i = 0; i < len; i++)
		CHECK_EQ(expected[i], actual[i]);
}

} // namespace

TEST(WireWidths, StreamLongAndUlongAreFourBytesEachWay)
{
	WidthFixture f;
	f.m_Output.write(static_cast<long>(-2));
	f.m_Output.write(static_cast<ulong>(0xC4D5E6F7u));
	f.m_Output.write(static_cast<long>((std::numeric_limits<std::int32_t>::min)()));

	const std::vector<unsigned char> actual =
		SocketOutputStreamTestAccess::Bytes(f.m_Output);
	const unsigned char expected[] = {
		0xFE, 0xFF, 0xFF, 0xFF,
		0xF7, 0xE6, 0xD5, 0xC4,
		0x00, 0x00, 0x00, 0x80
	};
	CheckBytes(actual, expected, sizeof(expected));

	SocketInputStreamTestAccess::Preload(f.m_Input, expected, sizeof(expected));
	long a = 0;
	ulong b = 0;
	long c = 0;
	f.m_Input.read(a);
	f.m_Input.read(b);
	f.m_Input.read(c);
	CHECK_EQ(-2L, a);
	CHECK_EQ(static_cast<ulong>(0xC4D5E6F7u), b);
	CHECK_EQ(static_cast<long>((std::numeric_limits<std::int32_t>::min)()), c);
	CHECK(f.m_Input.isEmpty());
}

TEST(WireWidths, EncryptedLongAndUlongAreFourBytesEachWay)
{
	EncryptedWidthFixture f;
	f.m_Output.setEncryptCode(0x5A);
	f.m_Output.writeEncrypt(static_cast<long>(0x01020304));
	f.m_Output.writeEncrypt(static_cast<ulong>(0xFFFFFFFFu));

	const std::vector<unsigned char> actual =
		SocketOutputStreamTestAccess::Bytes(f.m_Output);
	CHECK_EQ(std::size_t{8}, actual.size());

	SocketInputStreamTestAccess::Preload(f.m_Input, actual.data(),
		static_cast<unsigned int>(actual.size()));
	f.m_Input.setEncryptCode(0x5A);
	long a = 0;
	ulong b = 0;
	f.m_Input.readEncrypt(a);
	f.m_Input.readEncrypt(b);
	CHECK_EQ(0x01020304L, a);
	CHECK_EQ(static_cast<ulong>(0xFFFFFFFFu), b);
	CHECK(f.m_Input.isEmpty());
}

TEST(WireWidths, DatagramLongAndUlongAreFourBytesEachWay)
{
	Datagram d;
	d.setData(8);
	d.write(static_cast<long>(-1));
	d.write(static_cast<ulong>(0x11223344u));

	const unsigned char expected[] = {
		0xFF, 0xFF, 0xFF, 0xFF,
		0x44, 0x33, 0x22, 0x11
	};
	CHECK_EQ(0, std::memcmp(d.getData(), expected, sizeof(expected)));

	long a = 0;
	ulong b = 0;
	d.read(a);
	d.read(b);
	CHECK_EQ(-1L, a);
	CHECK_EQ(static_cast<ulong>(0x11223344u), b);
}

TEST(WireWidths, UlonglongIsEightBytesEachWay)
{
	WidthFixture f;
	f.m_Output.writeWire(static_cast<ulonglong>(0x8899AABBCCDDEEFFULL));

	const std::vector<unsigned char> actual =
		SocketOutputStreamTestAccess::Bytes(f.m_Output);
	const unsigned char expected[] = {
		0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88
	};
	CheckBytes(actual, expected, sizeof(expected));

	SocketInputStreamTestAccess::Preload(f.m_Input, expected, sizeof(expected));
	ulonglong v = 0;
	f.m_Input.readWire(v);
	CHECK_EQ(static_cast<ulonglong>(0x8899AABBCCDDEEFFULL), v);
	CHECK(f.m_Input.isEmpty());
}
