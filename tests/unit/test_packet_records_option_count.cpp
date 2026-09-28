//----------------------------------------------------------------------
// test_packet_records_option_count.cpp
//----------------------------------------------------------------------
//
// The item records that carry a BYTE-counted option list: read() takes
// a count byte and then that many options, so write() has to put the
// count ahead of them for its output to read back, and getPacketSize()
// has to agree with the bytes write() emits.
//
// GCAddItemToInventory is not a packet of its own but the item record
// inside GCMakeItemOK; GCShopBought and GCShopBuyOK are packets.
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

#include "Gpackets/GCAddItemToInventory.h"
#include "Gpackets/GCShopBought.h"
#include "Gpackets/GCShopBuyOK.h"

#include <list>
#include <vector>

namespace {

//----------------------------------------------------------------------
// Streams over a never-used socket (see tests/support/packet_stream_access.h),
// at encryption code 0 so the bytes in the ring are the plain body.
//----------------------------------------------------------------------
struct OptionOutFixture
{
	Socket				m_Socket;
	SocketEncryptOutputStream	m_Stream;

	OptionOutFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket)
	{
		m_Stream.setEncryptCode(0);
	}
};

struct OptionInFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Stream;

	OptionInFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 4096)
	{
		m_Stream.setEncryptCode(0);
	}
};

//----------------------------------------------------------------------
// write() src, check the body is getPacketSize() long, read() it into
// dst, and check read() consumed every byte.
//----------------------------------------------------------------------
template <class RecordT>
void	RoundTrip ( const RecordT & src, RecordT & dst )
{
	OptionOutFixture	out;

	src.write(out.m_Stream);

	const std::vector<unsigned char> body =
		SocketOutputStreamTestAccess::Bytes(out.m_Stream);

	CHECK_EQ((size_t)src.getPacketSize(), body.size());

	OptionInFixture		in;

	SocketInputStreamTestAccess::Preload(in.m_Stream,
		body.empty() ? NULL : &body[0], (unsigned int)body.size());

	try {
		dst.read(in.m_Stream);
	} catch (...) {
		CHECK(false);
	}

	CHECK_EQ(0, (long long)in.m_Stream.length());
}

//----------------------------------------------------------------------
// 256 options is one more than a count byte can say: write() refuses it
// rather than sending a count of 0 followed by 256 option bytes.
//----------------------------------------------------------------------
template <class RecordT>
void	CheckTooManyOptionsRefused ( RecordT & packet )
{
	for (int i = 0; i < 256; i++)
		packet.addOptionType((OptionType_t)(i & 0xff));

	OptionOutFixture	out;
	bool			bThrew = false;

	try {
		packet.write(out.m_Stream);
	} catch (InvalidProtocolException&) {
		bThrew = true;
	}

	CHECK(bThrew);
}

bool	OptionsAre ( const std::list<OptionType_t> & options, OptionType_t a, OptionType_t b )
{
	if (options.size() != 2)
		return false;

	std::list<OptionType_t>::const_iterator itr = options.begin();
	const OptionType_t first = *itr++;
	return first == a && *itr == b;
}

} // namespace

//----------------------------------------------------------------------
// GCAddItemToInventory
//----------------------------------------------------------------------
TEST(PacketRecordsOptionCount, AddItemToInventoryRoundTrips)
{
	GCAddItemToInventory	src;
	src.setObjectID(0x01020304);
	src.setX(5);
	src.setY(6);
	src.setItemClass(11);
	src.setItemType(0x0203);
	src.addOptionType(7);
	src.addOptionType(9);
	src.setDurability(1234);
	src.setItemNum(3);

	GCAddItemToInventory	dst;
	RoundTrip(src, dst);

	CHECK_EQ(0x01020304, dst.getObjectID());
	CHECK_EQ(5, dst.getX());
	CHECK_EQ(6, dst.getY());
	CHECK_EQ(11, dst.getItemClass());
	CHECK_EQ(0x0203, dst.getItemType());
	CHECK_EQ(2, dst.getOptionTypeSize());
	CHECK(OptionsAre(dst.getOptionType(), 7, 9));
	CHECK_EQ(1234, dst.getDurability());
	CHECK_EQ(3, dst.getItemNum());
}

TEST(PacketRecordsOptionCount, AddItemToInventoryRefusesTooManyOptions)
{
	GCAddItemToInventory	packet;
	CheckTooManyOptionsRefused(packet);
}

//----------------------------------------------------------------------
// GCShopBought
//----------------------------------------------------------------------
TEST(PacketRecordsOptionCount, ShopBoughtRoundTrips)
{
	GCShopBought	src;
	src.setObjectID(0x01020304);
	src.setShopVersion(0x05060708);
	src.setShopType(2);
	src.setShopIndex(9);
	src.setItemObjectID(0x0a0b0c0d);
	src.setItemClass(11);
	src.setItemType(0x0203);
	src.addOptionType(3);
	src.addOptionType(7);
	src.setDurability(1234);
	src.setSilver(567);
	src.setGrade(4);
	src.setEnchantLevel(8);

	GCShopBought	dst;
	RoundTrip(src, dst);

	CHECK_EQ(0x01020304, dst.getObjectID());
	CHECK_EQ(0x05060708, dst.getShopVersion());
	CHECK_EQ(2, dst.getShopType());
	CHECK_EQ(9, dst.getShopIndex());
	CHECK_EQ(0x0a0b0c0d, dst.getItemObjectID());
	CHECK_EQ(11, dst.getItemClass());
	CHECK_EQ(0x0203, dst.getItemType());
	CHECK(OptionsAre(dst.getOptionType(), 3, 7));
	CHECK_EQ(1234, dst.getDurability());
	CHECK_EQ(567, dst.getSilver());
	CHECK_EQ(4, dst.getGrade());
	CHECK_EQ(8, dst.getEnchantLevel());
}

TEST(PacketRecordsOptionCount, ShopBoughtRefusesTooManyOptions)
{
	GCShopBought	packet;
	CheckTooManyOptionsRefused(packet);
}

//----------------------------------------------------------------------
// GCShopBuyOK
//----------------------------------------------------------------------
TEST(PacketRecordsOptionCount, ShopBuyOKRoundTrips)
{
	GCShopBuyOK	src;
	src.setObjectID(0x01020304);
	src.setShopVersion(0x05060708);
	src.setItemObjectID(0x0a0b0c0d);
	src.setItemClass(11);
	src.setItemType(0x0203);
	src.addOptionType(3);
	src.addOptionType(7);
	src.setDurability(1234);
	src.setItemNum(6);
	src.setSilver(567);
	src.setGrade(4);
	src.setEnchantLevel(8);
	src.setPrice(0x11223344);

	GCShopBuyOK	dst;
	RoundTrip(src, dst);

	CHECK_EQ(0x01020304, dst.getObjectID());
	CHECK_EQ(0x05060708, dst.getShopVersion());
	CHECK_EQ(0x0a0b0c0d, dst.getItemObjectID());
	CHECK_EQ(11, dst.getItemClass());
	CHECK_EQ(0x0203, dst.getItemType());
	CHECK(OptionsAre(dst.getOptionType(), 3, 7));
	CHECK_EQ(1234, dst.getDurability());
	CHECK_EQ(6, dst.getItemNum());
	CHECK_EQ(567, dst.getSilver());
	CHECK_EQ(4, dst.getGrade());
	CHECK_EQ(8, dst.getEnchantLevel());
	CHECK_EQ(0x11223344, dst.getPrice());
}

TEST(PacketRecordsOptionCount, ShopBuyOKRefusesTooManyOptions)
{
	GCShopBuyOK	packet;
	CheckTooManyOptionsRefused(packet);
}
