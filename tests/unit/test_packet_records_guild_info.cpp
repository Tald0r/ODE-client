//----------------------------------------------------------------------
// test_packet_records_guild_info.cpp
//----------------------------------------------------------------------
//
// GuildInfo::read() of a zero-length expire date leaves the record
// with an empty expire date, whatever the object held before - as a
// zero-length string read does everywhere else on the wire.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Exception.h"
#include "GuildInfo.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"

#include <string>
#include <vector>

namespace {

//----------------------------------------------------------------------
// The body GuildInfo::write() puts on the wire for this expire date.
//----------------------------------------------------------------------
std::vector<unsigned char>	GuildInfoBody ( const std::string & expireDate )
{
	GuildInfo	info;
	info.setGuildID(7);
	info.setGuildName("Guild");
	info.setGuildMaster("Master");
	info.setGuildMemberCount(3);
	info.setGuildExpireDate(expireDate);

	Socket				socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketEncryptOutputStream	out(&socket);
	out.setEncryptCode(0);

	info.write(out);

	return SocketOutputStreamTestAccess::Bytes(out);
}

} // namespace

TEST(PacketRecordsGuildInfo, ZeroLengthExpireDateClearsAnEarlierOne)
{
	std::vector<unsigned char> wire = GuildInfoBody("20260927");
	const std::vector<unsigned char> second = GuildInfoBody("");
	wire.insert(wire.end(), second.begin(), second.end());

	Socket				socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketEncryptInputStream	in(&socket, 4096);
	in.setEncryptCode(0);

	SocketInputStreamTestAccess::Preload(in, &wire[0], (unsigned int)wire.size());

	GuildInfo	info;

	info.read(in);
	CHECK(info.getGuildExpireDate() == "20260927");

	info.read(in);
	CHECK(info.getGuildExpireDate().empty());
	CHECK_EQ(0, (long long)in.length());
}
