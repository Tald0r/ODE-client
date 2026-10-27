//----------------------------------------------------------------------
// test_packet_fuzz_findings.cpp
//----------------------------------------------------------------------
//
// Parsers the client packet-read fuzz targets (tests/fuzz/
// fuzz_client_stream.cpp, the game connection, and
// fuzz_client_login_stream.cpp, the login connection) crashed, each
// pinned with the bytes it crashed on. The same inputs, as the fuzzer
// wrote them, replay in the fuzz_replay_<target> ctests from
// tests/fuzz/regressions/<target>; this file states the contract the
// fix established: the hostile value is refused with
// InvalidProtocolException, the exception every receive loop treats as
// a protocol violation.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Exception.h"
#include "NicknameInfo.h"
#include "Packet.h"
#include "Socket.h"
#include "SocketEncryptInputStream.h"
#include "SocketImpl.h"
#include "StoreInfo.h"
#include "Gpackets/GCActiveGuildList.h"
#include "Cpackets/CGSMSSend.h"
#include "Gpackets/GCAddNewItemToZone.h"
#include "Gpackets/GCAddVampire.h"
#include "Gpackets/GCChangeWeather.h"
#include "Gpackets/GCGoodsList.h"
#include "Gpackets/GCGQuestStatusInfo.h"
#include "Gpackets/GCGQuestStatusModify.h"
#include "Gpackets/GCGuildMemberList.h"
#include "Gpackets/GCHolyLandBonusInfo.h"
#include "Gpackets/GCKickMessage.h"
#include "Gpackets/GCModifyNickname.h"
#include "Gpackets/GCMonsterKillQuestInfo.h"
#include "Gpackets/GCMyStoreInfo.h"
#include "Gpackets/GCNicknameList.h"
#include "Gpackets/GCNPCAskVariable.h"
#include "Gpackets/GCNPCInfo.h"
#include "Gpackets/GCOtherStoreInfo.h"
#include "Gpackets/GCPartyJoined.h"
#include "Gpackets/GCPetStashList.h"
#include "Gpackets/GCShowUnionInfo.h"
#include "Gpackets/GCSkillInfo.h"
#include "Gpackets/GCSMSAddressList.h"
#include "Gpackets/GCStashList.h"
#include "Gpackets/GCSweeperBonusInfo.h"
#include "Gpackets/GCSystemMessage.h"
#include "Gpackets/GCTimeLimitItemInfo.h"
#include "Gpackets/GCTradeAddItem.h"
#include "Gpackets/GCUnionOfferList.h"
#include "Gpackets/GCUpdateInfo.h"
#include "Gpackets/GCWaitGuildList.h"
#include "Gpackets/GCWarList.h"
#include "Gpackets/GCWarScheduleList.h"
#include "Lpackets/LCPCList.h"
#include "Lpackets/LCServerList.h"
#include "Lpackets/LCWorldList.h"
#include "Rpackets/CRWhisper.h"
#include "Rpackets/RCRequestedFile.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ASan's allocator says how many bytes it holds, which is how a leak shows
// in a suite that runs with leak detection off (the game's static tables
// are never freed). MSVC's runtime is not asked.
#if defined(__has_feature) && !defined(_MSC_VER)
#if __has_feature(address_sanitizer)
#define FINDINGS_HEAP_BYTES 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__) && !defined(_MSC_VER) && !defined(FINDINGS_HEAP_BYTES)
#define FINDINGS_HEAP_BYTES 1
#endif
#ifdef FINDINGS_HEAP_BYTES
// The sanitizer runtime's own (sanitizer/allocator_interface.h, which GCC's
// install does not ship), as test_game_ui_1.cpp declares it.
extern "C" size_t __sanitizer_get_current_allocated_bytes(void);
#endif

namespace {

//----------------------------------------------------------------------
// An input stream over a never-used socket (packet_stream_access.h), at
// encrypt code 0, as the fuzz inputs below were found.
//----------------------------------------------------------------------
struct FindingInFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Stream;

	explicit FindingInFixture(const std::vector<unsigned char>& bytes)
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 4096)
	{
		m_Stream.setEncryptCode(0);
		SocketInputStreamTestAccess::Preload(m_Stream,
			bytes.empty() ? NULL : &bytes[0], (unsigned int)bytes.size());
	}
};

// Reads one framed packet. Returns true when read() refused it as a
// protocol violation; `why` receives the refusal's message, or stays
// empty when the frame was read.
bool	FramedReadIsRefused(Packet& packet, const std::vector<unsigned char>& frame,
			    std::string& why)
{
	FindingInFixture f(frame);
	bool bRefused = false;
	why.clear();
	try {
		f.m_Stream.read(&packet);
	} catch (InvalidProtocolException& e) {
		bRefused = true;
		why = e.getMessage();
	}
	// The refused frame is discarded whole, as a completed one is.
	CHECK_EQ(0u, f.m_Stream.length());
	return bRefused;
}

// True when `why` contains `reason`.
bool	RefusedFor(const std::string& why, const char* reason)
{
	return why.find(reason) != std::string::npos;
}

// NicknameInfo's body: id (WORD, little-endian) and type, then the
// type's own fields.
bool	NicknameReadIsRefused(const std::vector<unsigned char>& body)
{
	FindingInFixture f(body);
	NicknameInfo info;
	try {
		info.read(f.m_Stream);
	} catch (InvalidProtocolException&) {
		return true;
	}
	return false;
}

} // namespace

//----------------------------------------------------------------------
// NicknameInfo::read ended its type switch in a libc assert(false),
// which aborts the client in every build without NDEBUG, on a type byte
// any GCModifyNickname, GCAddSlayer, GCAddVampire, GCAddOusters,
// GCAddNickname, GCNicknameList or GCUpdateInfo carries.
//----------------------------------------------------------------------
TEST(PacketFuzzFindings, NicknameTypePastTheLastIsRefused)
{
	CHECK(NicknameReadIsRefused({ 0x34, 0x12, 6 }));
	CHECK(NicknameReadIsRefused({ 0x34, 0x12, 0x7f }));
	CHECK(NicknameReadIsRefused({ 0x34, 0x12, 0xff }));
}

TEST(PacketFuzzFindings, EveryNicknameTypeInRangeStillReads)
{
	// NICK_NONE: nothing follows.
	CHECK(!NicknameReadIsRefused({ 0x34, 0x12, NicknameInfo::NICK_NONE }));
	// The three index types: a WORD follows.
	CHECK(!NicknameReadIsRefused({ 0x34, 0x12, NicknameInfo::NICK_BUILT_IN, 7, 0 }));
	CHECK(!NicknameReadIsRefused({ 0x34, 0x12, NicknameInfo::NICK_QUEST, 7, 0 }));
	CHECK(!NicknameReadIsRefused({ 0x34, 0x12, NicknameInfo::NICK_FORCED, 7, 0 }));
	// The two string types: a length byte, then the string.
	CHECK(!NicknameReadIsRefused({ 0x34, 0x12, NicknameInfo::NICK_CUSTOM_FORCED, 2, 'h', 'i' }));
	CHECK(!NicknameReadIsRefused({ 0x34, 0x12, NicknameInfo::NICK_CUSTOM, 0 }));
}

// The first frame of the fuzzer's input (tests/fuzz/regressions/
// client_stream/GCModifyNickname-type-0xff.hex), byte for byte, without
// the encrypt code byte before it and the 8 zero bytes after it: one
// GCModifyNickname frame of 30 bytes whose nickname type is 0xff.
// The check names the type refusal: in a build with NDEBUG (Release)
// the unfixed code's assert compiles out, the parse stops after the
// type byte, and the framed read refuses the 23 bytes left unread
// ("packet parser did not consume declared body"), so a bare "refused"
// would pass on the unfixed code there.
TEST(PacketFuzzFindings, ModifyNicknameFrameWithTypeFfIsRefused)
{
	std::vector<unsigned char> frame = {
		0x15, 0x01,			// id 0x0115, GCModifyNickname
		0x1e, 0x00, 0x00, 0x00,		// body size 30
		0x00,				// sequence
		0x00, 0x00, 0xff, 0xff,		// object id
		0xff, 0xff,			// nickname id
		0xff,				// nickname type
		0xff, 0xff,			// unread: the type ended the parse
	};
	frame.resize(7 + 30, 0x00);

	GCModifyNickname packet;
	std::string why;
	CHECK(FramedReadIsRefused(packet, frame, why));
	CHECK(RefusedFor(why, "nickname type out of range"));
}

//----------------------------------------------------------------------
// StoreInfo::read took an item count from a wire byte (up to 255) and
// read that many items into m_Items, which the constructor sizes to
// MAX_ITEM_NUM (20): a count of 21 or more wrote StoreItemInfo fields
// past the end of the vector's heap block. GCMyStoreInfo and
// GCOtherStoreInfo carry a StoreInfo.
//----------------------------------------------------------------------
namespace {

// A StoreInfo body as read(iStream, toOther) takes it: open, an empty
// sign, the count, then `present` items that are all absent (one zero
// byte each).
std::vector<unsigned char>	StoreBody(unsigned char count, unsigned int present)
{
	std::vector<unsigned char> body = { 1, 0, count };
	body.resize(body.size() + present, 0x00);
	return body;
}

// read()s `body` into a fresh StoreInfo. True when refused; `unread` is
// what the stream still held afterwards.
bool	StoreReadIsRefused(const std::vector<unsigned char>& body, bool toOther,
			   unsigned int& unread)
{
	FindingInFixture f(body);
	StoreInfo info;
	bool bRefused = false;
	try {
		info.read(f.m_Stream, toOther);
	} catch (InvalidProtocolException&) {
		bRefused = true;
	}
	unread = f.m_Stream.length();
	return bRefused;
}

} // namespace

TEST(PacketFuzzFindings, StoreItemCountPastTheVectorIsRefusedBeforeAnyItem)
{
	const unsigned char counts[] = { (unsigned char)(MAX_ITEM_NUM + 1), 0x80, 0xff };
	for (unsigned char count : counts)
	{
		for (bool toOther : { false, true })
		{
			// Enough absent items that the stream never runs out first.
			unsigned int unread = 0;
			CHECK(StoreReadIsRefused(StoreBody(count, count), toOther, unread));
			// Refused at the count byte: not one item was read.
			CHECK_EQ((unsigned int)count, unread);
		}
	}
}

TEST(PacketFuzzFindings, StoreItemCountUpToTheVectorStillReads)
{
	const unsigned char counts[] = { 0, 1, (unsigned char)MAX_ITEM_NUM };
	for (unsigned char count : counts)
	{
		unsigned int unread = 1;
		CHECK(!StoreReadIsRefused(StoreBody(count, count), false, unread));
		CHECK_EQ(0u, unread);
	}
}

// The fuzzer's input (tests/fuzz/regressions/client_stream/
// GCMyStoreInfo-item-count-0xa9.hex) without its encrypt code byte:
// one GCMyStoreInfo frame of 64 bytes, count 169, one item present.
// The check names the count refusal: without it the parse writes past
// m_Items and then runs off the 64-byte body, which the framed read
// also refuses ("packet parser underflowed declared body"), so a bare
// "refused" would pass on the unfixed code in a build without ASan.
TEST(PacketFuzzFindings, MyStoreInfoFrameWithCountA9IsRefused)
{
	std::vector<unsigned char> frame = {
		0x1f, 0x01,			// id 0x011f, GCMyStoreInfo
		0x40, 0x00, 0x00, 0x00,		// body size 64
		0x00,				// sequence
		0x00,				// open UI
		0x00,				// open
		0x00,				// sign length
		0xa9,				// item count 169
		0x5a, 0x53, 0x6b, 0x01,		// item 0 present, then its fields
	};
	frame.resize(7 + 64, 0x00);

	GCMyStoreInfo packet;
	std::string why;
	CHECK(FramedReadIsRefused(packet, frame, why));
	CHECK(RefusedFor(why, "store item count out of range"));
}

// The same count through GCOtherStoreInfo, the other packet that
// carries a StoreInfo: an open store with 21 absent items.
TEST(PacketFuzzFindings, OtherStoreInfoFrameWithCount21IsRefused)
{
	std::vector<unsigned char> body = {
		0x04, 0x03, 0x02, 0x01,		// object id
		0x01,				// requested
	};
	const std::vector<unsigned char> store =
		StoreBody((unsigned char)(MAX_ITEM_NUM + 1), MAX_ITEM_NUM + 1);
	body.insert(body.end(), store.begin(), store.end());

	std::vector<unsigned char> frame = {
		0x2d, 0x01,			// id 0x012d, GCOtherStoreInfo
		(unsigned char)body.size(), 0x00, 0x00, 0x00,
		0x00,				// sequence
	};
	frame.insert(frame.end(), body.begin(), body.end());

	GCOtherStoreInfo packet;
	std::string why;
	CHECK(FramedReadIsRefused(packet, frame, why));
	CHECK(RefusedFor(why, "store item count out of range"));
}

//----------------------------------------------------------------------
// LCPCList::read stored each PC info at m_pPCInfos[getSlot()], and
// PCSlayerInfo::read, PCVampireInfo::read and PCOustersInfo::read took
// that slot from a wire byte (up to 255) and cast it to Slot unchecked.
// A slot of 3 (SLOT_MAX) wrote the info's pointer one entry past the
// three-entry array, inside the heap-allocated packet's neighbour; a
// slot of 4 or more is no Slot value at all (Clang's UBSan stops at
// getSlot()'s load) and wrote up to 2 KB past it. The login server
// sends LCPCList after a login and after every character change.
//----------------------------------------------------------------------
namespace {

// tests/golden/LCPCList.code0.hex, the list the server writes for three
// characters: the type tags 'S', 'V' and 'O', then a PCSlayerInfo in
// slot 0, a PCVampireInfo in slot 1 and a PCOustersInfo in slot 2.
// Each info starts with its name's length and the name; the slot byte
// follows the name.
const char	kPCListGoldenHex[] =
	"53564f0a476f6c64536c6179657200bdac9b8a81079207a3078bbfae9d8cc0af"
	"9e8dc1b09f8e1b8a2c8b3d8c4e8dc2b1a08f8081828384859d7c0100118a228b"
	"338c448d558e668f77908d0f476f6c6456616d706972654e616d6501bcad9e8f"
	"008891999204aa9381079207a307bb94cc959697cbbaa998ccbbaa99dd9a9b0b"
	"476f6c644f75737465727302bfae8d9c01119d229e339f44a00c81079207a307"
	"55a166a277a388a4a5a6dac9b8a7dbcab9a8eea9ffaaab";

std::vector<unsigned char>	FromHex(const char* hex)
{
	std::vector<unsigned char> bytes;
	for (const char* p = hex; p[0] != '\0' && p[1] != '\0'; p += 2)
	{
		const char digits[3] = { p[0], p[1], '\0' };
		bytes.push_back((unsigned char)std::strtoul(digits, NULL, 16));
	}
	return bytes;
}

std::vector<unsigned char>	PCListGolden()
{
	return FromHex(kPCListGoldenHex);
}

// The offset of the byte after `name` in the golden body: the info's
// slot byte.
std::size_t	SlotOffsetAfter(const std::vector<unsigned char>& body, const char* name)
{
	const std::size_t len = std::strlen(name);
	const auto it = std::search(body.begin(), body.end(), name, name + len);
	CHECK(it != body.end());
	return (std::size_t)(it - body.begin()) + len;
}

std::size_t	SlayerSlot(const std::vector<unsigned char>& body)	{ return SlotOffsetAfter(body, "GoldSlayer"); }
std::size_t	VampireSlot(const std::vector<unsigned char>& body)	{ return SlotOffsetAfter(body, "GoldVampireName"); }
std::size_t	OustersSlot(const std::vector<unsigned char>& body)	{ return SlotOffsetAfter(body, "GoldOusters"); }

// One LCPCList frame (id 446) around `body`, sequence 0.
std::vector<unsigned char>	PCListFrame(const std::vector<unsigned char>& body)
{
	std::vector<unsigned char> frame = {
		0xbe, 0x01,			// id 0x01be, LCPCList
		(unsigned char)body.size(), (unsigned char)(body.size() >> 8), 0x00, 0x00,
		0x00,				// sequence
	};
	frame.insert(frame.end(), body.begin(), body.end());
	return frame;
}

// read()s the bytes from `begin` to `end` of the golden body into a
// fresh Info. True when refused; `why` receives the message.
template <class Info>
bool	PCInfoReadIsRefused(const std::vector<unsigned char>& body, std::size_t begin,
			    std::size_t end, std::string& why)
{
	const std::vector<unsigned char> bytes(body.begin() + begin, body.begin() + end);
	FindingInFixture f(bytes);
	Info info;
	why.clear();
	try {
		info.read(f.m_Stream);
	} catch (InvalidProtocolException& e) {
		why = e.getMessage();
		return true;
	}
	return false;
}

} // namespace

// Each info's read, alone, refuses the slot byte: 3 is the first value
// past the array, 0x7f and 0xff are not Slot values.
TEST(PacketFuzzFindings, PCInfoSlotPastTheLastIsRefused)
{
	const unsigned char slots[] = { (unsigned char)SLOT_MAX, 0x7f, 0xff };
	for (unsigned char slot : slots)
	{
		std::vector<unsigned char> body = PCListGolden();
		const std::size_t vampireStart = VampireSlot(body) - 1 - std::strlen("GoldVampireName");
		const std::size_t oustersStart = OustersSlot(body) - 1 - std::strlen("GoldOusters");
		body[SlayerSlot(body)] = slot;
		body[VampireSlot(body)] = slot;
		body[OustersSlot(body)] = slot;

		std::string why;
		CHECK(PCInfoReadIsRefused<PCSlayerInfo>(body, 3, vampireStart, why));
		CHECK(RefusedFor(why, "pc slot out of range"));
		CHECK(PCInfoReadIsRefused<PCVampireInfo>(body, vampireStart, oustersStart, why));
		CHECK(RefusedFor(why, "pc slot out of range"));
		CHECK(PCInfoReadIsRefused<PCOustersInfo>(body, oustersStart, body.size(), why));
		CHECK(RefusedFor(why, "pc slot out of range"));
	}
}

// The fuzzer's input (tests/fuzz/regressions/client_login_stream/
// LCPCList-slot-0xbc.hex) without its encrypt code byte, byte for
// byte: one LCPCList frame of 183 bytes, a mutation of the golden list
// whose Slayer is named "GoldSlaye\0" and has slot byte 188. On the
// unfixed code the read stores the Slayer at m_pPCInfos[188], 1.5 KB
// past the packet.
TEST(PacketFuzzFindings, PCListFrameWithSlayerSlotBcIsRefused)
{
	const std::vector<unsigned char> frame = FromHex(
		"be01b70000000053564f0a476f6c64536c61796500bc0100000000009207a3"
		"078bbfae9d8cc0af9e8dc1b09f8e1b8a2c8b3d8c4e8dc2b1a08f808182838485"
		"9d040500118a228b338c448d558e668f77908d0f476f6c6456616d706972654e"
		"616d6501bcad9e8f008891999204aa9381079207a307bb94cc959697cbbaa998"
		"ccbbaa99dd9a9b0b476f6c644f75737465727302bfae8d9c01119d229e339f44"
		"a00c81079207a30755a166a277a388a4a5a6dac9b8a7dbcab9a8eea9ffaaab");
	CHECK_EQ(7u + 183u, (unsigned int)frame.size());
	CHECK_EQ(0xbc, (int)frame[7 + 3 + 1 + 10]);	// types, name length, name

	LCPCList packet;
	std::string why;
	CHECK(FramedReadIsRefused(packet, frame, why));
	CHECK(RefusedFor(why, "pc slot out of range"));
}

// Slot 3, a valid Slot enumerator and one entry past the array, in the
// last info: the frame is otherwise the golden, so on the unfixed code
// it reads to the end and only the stray pointer tells.
TEST(PacketFuzzFindings, PCListFrameWithOustersSlot3IsRefused)
{
	std::vector<unsigned char> body = PCListGolden();
	body[OustersSlot(body)] = (unsigned char)SLOT_MAX;

	LCPCList packet;
	std::string why;
	CHECK(FramedReadIsRefused(packet, PCListFrame(body), why));
	CHECK(RefusedFor(why, "pc slot out of range"));
}

// Slots 0 to 2 still read, in any order, each info landing in the slot
// its byte names.
TEST(PacketFuzzFindings, PCListSlotsInRangeStillRead)
{
	std::vector<unsigned char> body = PCListGolden();
	body[SlayerSlot(body)] = (unsigned char)SLOT3;
	body[VampireSlot(body)] = (unsigned char)SLOT1;
	body[OustersSlot(body)] = (unsigned char)SLOT2;

	LCPCList packet;
	std::string why;
	CHECK(!FramedReadIsRefused(packet, PCListFrame(body), why));
	CHECK_EQ((int)PC_VAMPIRE, (int)packet.getPCInfo(SLOT1)->getPCType());
	CHECK_EQ((int)PC_OUSTERS, (int)packet.getPCInfo(SLOT2)->getPCType());
	CHECK_EQ((int)PC_SLAYER, (int)packet.getPCInfo(SLOT3)->getPCType());
}

// A slot named twice keeps the later info, as it always has, and the slot
// no character names stays empty. This test cannot see whether the earlier
// info is freed: no leak check runs in the unit suite, and this passed on
// the unfixed code, which leaked it (ReadPCInfo now deletes it).
TEST(PacketFuzzFindings, PCListSlotNamedTwiceKeepsTheLaterInfo)
{
	std::vector<unsigned char> body = PCListGolden();
	body[SlayerSlot(body)] = (unsigned char)SLOT2;
	body[VampireSlot(body)] = (unsigned char)SLOT2;

	LCPCList packet;
	std::string why;
	CHECK(!FramedReadIsRefused(packet, PCListFrame(body), why));
	CHECK_EQ((int)PC_VAMPIRE, (int)packet.getPCInfo(SLOT2)->getPCType());
	CHECK_EQ((int)PC_OUSTERS, (int)packet.getPCInfo(SLOT3)->getPCType());
	bool bEmpty = false;
	try {
		packet.getPCInfo(SLOT1);
	} catch (NoSuchElementException&) {
		bEmpty = true;
	}
	CHECK(bEmpty);
}

//----------------------------------------------------------------------
// Found by the packet-handler targets (tests/fuzz/
// fuzz_client_stream_handlers.cpp), which run the handler after the
// read: GCChangeWeather, GCKickMessage and GCSystemMessage stored a wire
// byte in an enum with no fixed type, whose values end at the bit width
// of its last enumerator, and the handler's getter loaded a byte past
// that, which is undefined (Clang's UBSan halts on the load; GCC's does
// not check enums). Each byte now reads back as sent, and the handlers'
// switches and the weather host's skip what they do not name.
//----------------------------------------------------------------------
namespace {

// A frame of the packet id with this body, at sequence 0.
std::vector<unsigned char>	Frame(PacketID_t id, const std::vector<unsigned char>& body)
{
	std::vector<unsigned char> frame = {
		(unsigned char)(id & 0xff), (unsigned char)(id >> 8),
		(unsigned char)body.size(), (unsigned char)(body.size() >> 8),
		(unsigned char)(body.size() >> 16), (unsigned char)(body.size() >> 24),
		0x00,
	};
	frame.insert(frame.end(), body.begin(), body.end());
	return frame;
}

} // namespace

TEST(PacketFuzzFindings, WeatherPastTheEnumReadsBackAsSent)
{
	for (unsigned char weather : { (unsigned char)WEATHER_MAX, (unsigned char)33, (unsigned char)0xff })
	{
		GCChangeWeather packet;
		std::string why;
		CHECK(!FramedReadIsRefused(packet, Frame(Packet::PACKET_GC_CHANGE_WEATHER, { weather, 2 }), why));
		CHECK_EQ((int)weather, (int)packet.getWeather());
		CHECK_EQ(2, (int)packet.getWeatherLevel());
	}
}

TEST(PacketFuzzFindings, KickMessageTypePastTheEnumReadsBackAsSent)
{
	for (unsigned char type : { (unsigned char)KICK_MESSAGE_MAX, (unsigned char)0xfe })
	{
		GCKickMessage packet;
		std::string why;
		CHECK(!FramedReadIsRefused(packet, Frame(Packet::PACKET_GC_KICK_MESSAGE, { type, 30, 0, 0, 0 }), why));
		CHECK_EQ((int)type, (int)packet.getType());
		CHECK_EQ(30u, packet.getSeconds());
	}
}

TEST(PacketFuzzFindings, SystemMessageTypePastTheEnumReadsBackAsSent)
{
	for (unsigned char type : { (unsigned char)SYSTEM_MESSAGE_MAX, (unsigned char)101, (unsigned char)0xff })
	{
		GCSystemMessage packet;
		std::string why;
		CHECK(!FramedReadIsRefused(packet, Frame(Packet::PACKET_GC_SYSTEM_MESSAGE, { 1, 'x', 0, 0, 0, 0, type }), why));
		CHECK_EQ((int)type, (int)packet.getType());
		CHECK(packet.getMessage() == "x");
	}
}

// GCAddVampire's vampire info (PCVampireInfo3, and the other PC infos the
// same way) stores its sex byte in Sex, an enum of two values; the handler
// target's input below sent 5, and GCAddVampireHandler's getSex() load
// halted under Clang's UBSan. The byte reads back as sent, and the handler
// tests it against MALE.
TEST(PacketFuzzFindings, VampireSexPastTheEnumReadsBackAsSent)
{
	// The frame of tests/fuzz/regressions/client_stream_handlers/
	// GCAddVampire-sex-0x05.hex, without its encrypt code byte. The sex is
	// the body's byte after the id, the name and the cell and direction.
	std::vector<unsigned char> frame = {
		0xc3, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x0d,
		0xff, 0xff, 0x1e, 0x1e, 0x40, 0x00, 0x2f, 0x01, 0x05, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x69, 0x05, 0x00, 0x00, 0x00, 0x2a, 0x00, 0xff, 0x00,
		0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0x1e, 0x1e, 0x40, 0x00, 0x00, 0x00, 0x2f, 0x01, 0x05, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x69, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	const size_t kSexAt = 7 + 4 + 1 + 13 + 3;
	for (unsigned char sex : { (unsigned char)5, (unsigned char)2, (unsigned char)0xff })
	{
		frame[kSexAt] = sex;
		GCAddVampire packet;
		std::string why;
		CHECK(!FramedReadIsRefused(packet, frame, why));
		CHECK_EQ((int)sex, (int)packet.getVampireInfo().getSex());
		CHECK(packet.getVampireInfo().getSex() != MALE);
	}
}

//----------------------------------------------------------------------
// Found by reading: the same conversions where nothing loaded the value
// for UBSan to see, or loaded it only after the read. A wire value past
// the values of an enum with no fixed type is undefined as soon as it is
// converted, and Clang's UBSan checks loads, not conversions.
//----------------------------------------------------------------------

// RCRequestedFileInfo::read stored a peer's file type byte in
// REQUEST_FILE_TYPE, whose values end at 7, as its last enumerator, 4,
// needs three bits; only a getter load lets UBSan see it. The byte reads
// back as sent.
TEST(PacketFuzzFindings, RequestedFileTypePastTheEnumReadsBackAsSent)
{
	for (unsigned char type : { (unsigned char)8, (unsigned char)0xff })
	{
		// The type, version 1, the filename "f", size 9.
		FindingInFixture f({ type, 1, 0, 0, 0, 1, 'f', 9, 0, 0, 0 });
		RCRequestedFileInfo info;
		info.read(f.m_Stream);
		CHECK_EQ(0u, f.m_Stream.length());
		CHECK_EQ((int)type, (int)info.getRequestFileType());
		CHECK(info.getFilename() == "f");
		CHECK_EQ(9u, (unsigned int)info.getFileSize());
	}
}

// GCWarList::read switched on its war type byte cast to WarType, whose
// values end at 3, before the default could refuse it. 3 is a value of
// the enum and names no war; 4 and 0xff are not values. Each is refused,
// as it was: a regression guard, since no sanitizer sees the cast.
TEST(PacketFuzzFindings, WarTypePastTheLastIsRefused)
{
	for (unsigned char type : { (unsigned char)(WAR_LEVEL + 1), (unsigned char)4, (unsigned char)0xff })
	{
		// One war, of this type.
		FindingInFixture f({ 1, type });
		GCWarList packet;
		bool bRefused = false;
		try {
			packet.read(f.m_Stream);
		} catch (Error&) {
			bRefused = true;
		}
		CHECK(bRefused);
		CHECK(packet.isEmpty());
	}
}

// PCOustersInfo::toString, compiled in every build, named the sex with
// Sex2String[m_Sex], an array of two names, for any byte the read
// stored: a read past the array. A byte that names no sex is printed as
// its number; the golden Ousters is MALE.
TEST(PacketFuzzFindings, OustersInfoStringPrintsASexItHasNoNameForAsItsNumber)
{
	std::vector<unsigned char> body = PCListGolden();
	const std::size_t begin = OustersSlot(body) - 1 - std::strlen("GoldOusters");
	// The slot, then the alignment, then the sex.
	const std::size_t sexAt = OustersSlot(body) + 1 + szAlignment;
	CHECK_EQ((int)MALE, (int)body[sexAt]);
	const struct { unsigned char sex; const char* named; } rows[] = {
		{ (unsigned char)FEMALE, ",Sex:FEMALE," },
		{ (unsigned char)MALE, ",Sex:MALE," },
		{ 2, ",Sex:2," },
		{ 5, ",Sex:5," },
		{ 0xff, ",Sex:255," },
	};
	for (const auto& row : rows)
	{
		body[sexAt] = row.sex;
		FindingInFixture f(std::vector<unsigned char>(body.begin() + begin, body.end()));
		PCOustersInfo info;
		info.read(f.m_Stream);
		CHECK_EQ(0u, f.m_Stream.length());
		CHECK_EQ((int)row.sex, (int)info.getSex());
		CHECK(info.toString().find(row.named) != std::string::npos);
	}
}

//----------------------------------------------------------------------
// Found by reading: a list read allocated an entry, read it, and only
// then handed it to the list, so an entry whose read threw (its bytes
// past the frame, a bad value) was leaked. LCServerList and LCWorldList
// were the review's row; the sweep for a `new` in a read found the same
// shape in the packets below. Each is read from frames whose bodies
// are every length up to 255 of one byte, so some read stops inside an
// entry; the refusals must leave ASan's allocator holding nothing more.
// Without ASan only the refusals run: nothing else can see a leak.
//----------------------------------------------------------------------
namespace {

// The bytes the allocator holds now; 0 where nothing can say.
long long	HeldHeapBytes()
{
#ifdef FINDINGS_HEAP_BYTES
	return (long long)__sanitizer_get_current_allocated_bytes();
#else
	return 0;
#endif
}

// What the packet does not own: the SMS list's address units are the
// handler's to hand to the executable's SMS list, which keeps them, and
// the packet frees none, refused or read whole (a follow-up in
// docs/reviews/NEXT.md). The test frees what it still holds, so what it
// measures there is the unit whose read throws.
void	FreeWhatThePacketDoesNotOwn(Packet&) {}
void	FreeWhatThePacketDoesNotOwn(GCSMSAddressList& packet)
{
	for (AddressUnit* pUnit : packet.getAddresses())
		delete pUnit;
	packet.getAddresses().clear();
}

// One pass of reads of PacketT, a fresh packet per frame; any refusal is
// caught, as the receive loop catches it.
template <class PacketT>
void	ReadShortBodies(unsigned char fill)
{
	for (std::size_t n = 0; n <= 255; ++n)
	{
		PacketT packet;
		FindingInFixture f(Frame(packet.getPacketID(), std::vector<unsigned char>(n, fill)));
		try {
			f.m_Stream.read(&packet);
		} catch (Throwable&) {
		}
		FreeWhatThePacketDoesNotOwn(packet);
	}
}

// What a second pass leaves held: the first warms whatever the reads
// allocate once and keep.
template <class PacketT>
long long	HeapHeldByShortBodies()
{
	for (unsigned char fill : { (unsigned char)1, (unsigned char)2 })
		ReadShortBodies<PacketT>(fill);
	const long long before = HeldHeapBytes();
	for (unsigned char fill : { (unsigned char)1, (unsigned char)2 })
		ReadShortBodies<PacketT>(fill);
	return HeldHeapBytes() - before;
}

} // namespace

#define CHECK_SHORT_BODIES_LEAK_NOTHING(PacketT) \
	do { \
		const long long held = HeapHeldByShortBodies<PacketT>(); \
		if (held != 0) std::printf("      %s holds %lld bytes more\n", #PacketT, held); \
		CHECK_EQ(0LL, held); \
	} while (0)

TEST(PacketFuzzFindings, LoginListReadsThatThrowLeakNothing)
{
	CHECK_SHORT_BODIES_LEAK_NOTHING(LCServerList);
	CHECK_SHORT_BODIES_LEAK_NOTHING(LCWorldList);
}

TEST(PacketFuzzFindings, GameListReadsThatThrowLeakNothing)
{
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCActiveGuildList);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCAddNewItemToZone);	// GCAddItemToZone::read, for the three that share it
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCGoodsList);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCGQuestStatusInfo);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCGQuestStatusModify);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCGuildMemberList);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCHolyLandBonusInfo);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCMonsterKillQuestInfo);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCNicknameList);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCNPCAskVariable);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCNPCInfo);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCPartyJoined);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCPetStashList);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCShowUnionInfo);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCSkillInfo);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCSMSAddressList);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCStashList);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCSweeperBonusInfo);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCTradeAddItem);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCUnionOfferList);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCUpdateInfo);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCWaitGuildList);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCWarList);
	CHECK_SHORT_BODIES_LEAK_NOTHING(GCWarScheduleList);
}

TEST(PacketFuzzFindings, PeerListReadsThatThrowLeakNothing)
{
	CHECK_SHORT_BODIES_LEAK_NOTHING(CRWhisper);
	CHECK_SHORT_BODIES_LEAK_NOTHING(RCRequestedFile);
}


//----------------------------------------------------------------------
// Found by the game fuzz target: CGSMSSend::read and
// GCTimeLimitItemInfo::read bounded their counts and lengths with
// Assert, which throws AssertionError, no protocol refusal, in a build
// without NDEBUG and compiles out in one with it, where the values were
// then accepted. Each bound is a protocol refusal now, in every build,
// at the value its Assert stopped at. CGSMSSend is client-to-server; the
// client reads it only because its factory is registered.
//----------------------------------------------------------------------
namespace {

// A CGSMSSend body: the receivers, each a length and that many digits,
// then the caller's number and the message.
std::vector<unsigned char>	SMSBody(std::size_t receivers, std::size_t receiverLength,
				std::size_t callerLength, std::size_t messageLength)
{
	std::vector<unsigned char> body = { (unsigned char)receivers };
	for (std::size_t i = 0; i < receivers; ++i)
	{
		body.push_back((unsigned char)receiverLength);
		body.insert(body.end(), receiverLength, (unsigned char)'1');
	}
	body.push_back((unsigned char)callerLength);
	body.insert(body.end(), callerLength, (unsigned char)'2');
	body.push_back((unsigned char)messageLength);
	body.insert(body.end(), messageLength, (unsigned char)'m');
	return body;
}

bool	SMSSendIsRefused(const std::vector<unsigned char>& body, std::string& why)
{
	CGSMSSend packet;
	return FramedReadIsRefused(packet, Frame(packet.getPacketID(), body), why);
}

// GCTimeLimitItemInfo's count, then that many object ids, each its own
// index, and times.
std::vector<unsigned char>	TimeLimitBody(std::size_t count)
{
	std::vector<unsigned char> body = { (unsigned char)count };
	for (std::size_t i = 0; i < count; ++i)
	{
		const unsigned char entry[] = { (unsigned char)i, (unsigned char)(i >> 8), 0, 0, 60, 0, 0, 0 };
		body.insert(body.end(), entry, entry + sizeof(entry));
	}
	return body;
}

} // namespace

TEST(PacketFuzzFindings, SMSSendCountsAndLengthsAtTheirBoundsAreRefused)
{
	std::string why;
	CHECK(SMSSendIsRefused(SMSBody(MAX_RECEVIER_NUM, 3, 3, 3), why));
	CHECK(RefusedFor(why, "too many sms receivers"));
	CHECK(SMSSendIsRefused(SMSBody(0xff, 3, 3, 3), why));
	CHECK(RefusedFor(why, "too many sms receivers"));
	CHECK(SMSSendIsRefused(SMSBody(1, MAX_NUMBER_LENGTH, 3, 3), why));
	CHECK(RefusedFor(why, "too long sms receiver number"));
	CHECK(SMSSendIsRefused(SMSBody(1, 3, MAX_NUMBER_LENGTH, 3), why));
	CHECK(RefusedFor(why, "too long sms caller number"));
	CHECK(SMSSendIsRefused(SMSBody(1, 3, 3, MAX_MESSAGE_LENGTH), why));
	CHECK(RefusedFor(why, "too long sms message"));
}

TEST(PacketFuzzFindings, SMSSendJustInsideItsBoundsStillReads)
{
	CGSMSSend packet;
	std::string why;
	CHECK(!FramedReadIsRefused(packet, Frame(packet.getPacketID(),
		SMSBody(MAX_RECEVIER_NUM - 1, MAX_NUMBER_LENGTH - 1, MAX_NUMBER_LENGTH - 1, MAX_MESSAGE_LENGTH - 1)), why));
	CHECK_EQ((std::size_t)MAX_NUMBER_LENGTH - 1, packet.getCallerNumber().size());
	CHECK_EQ((std::size_t)MAX_MESSAGE_LENGTH - 1, packet.getMessage().size());
}

TEST(PacketFuzzFindings, TimeLimitItemCountPastItsBoundIsRefused)
{
	for (std::size_t count : { (std::size_t)MAX_TIME_LIMIT_ITEM_INFO + 1, (std::size_t)0xff })
	{
		GCTimeLimitItemInfo packet;
		std::string why;
		CHECK(FramedReadIsRefused(packet, Frame(packet.getPacketID(), TimeLimitBody(count)), why));
		CHECK(RefusedFor(why, "too many time limit items"));
		CHECK(packet.m_TimeLimitItemInfos.empty());
	}
	GCTimeLimitItemInfo packet;
	std::string why;
	CHECK(!FramedReadIsRefused(packet, Frame(packet.getPacketID(), TimeLimitBody(MAX_TIME_LIMIT_ITEM_INFO)), why));
	CHECK_EQ((std::size_t)MAX_TIME_LIMIT_ITEM_INFO, packet.m_TimeLimitItemInfos.size());
}
