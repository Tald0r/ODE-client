//----------------------------------------------------------------------
// test_packet_records_pc_info_sizes.cpp
//----------------------------------------------------------------------
//
// PCOustersInfo2 and PCVampireInfo2: getSize() is the number of bytes
// write() emits, and getMaxSize() is that number for the longest name
// (20 bytes) and guild name (30 bytes) write() accepts. getSize() is
// what GCUpdateInfo and GCMorph1 add up for their framing header.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "PCOustersInfo2.h"
#include "PCVampireInfo2.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptOutputStream.h"

#include <string>

namespace {

//----------------------------------------------------------------------
// The number of bytes info.write() emits.
//----------------------------------------------------------------------
template <class InfoT>
long long	WrittenSize ( const InfoT & info )
{
	Socket				socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketEncryptOutputStream	out(&socket);
	out.setEncryptCode(0);

	info.write(out);

	return (long long)out.size();
}

//----------------------------------------------------------------------
// Both sizes of one PC info class, over a short and a maximal record.
// Value-initialised, so every field is zero before the names are set.
//----------------------------------------------------------------------
template <class InfoT>
void	CheckPCInfoSizes ()
{
	{
		InfoT	info{};
		info.setSex(MALE);
		info.setName("a");
		info.setGuildName("g");

		CHECK_EQ((long long)info.getSize(), WrittenSize(info));
	}

	{
		InfoT	info{};
		info.setSex(FEMALE);
		info.setName(std::string(20, 'n'));
		info.setGuildName(std::string(30, 'g'));

		CHECK_EQ((long long)info.getSize(), WrittenSize(info));
		CHECK_EQ((long long)InfoT::getMaxSize(), WrittenSize(info));
	}
}

} // namespace

TEST(PacketRecordsPCInfoSizes, OustersSizesMatchWrite)
{
	CheckPCInfoSizes<PCOustersInfo2>();
}

TEST(PacketRecordsPCInfoSizes, VampireSizesMatchWrite)
{
	CheckPCInfoSizes<PCVampireInfo2>();
}
