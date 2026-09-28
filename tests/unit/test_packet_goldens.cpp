//----------------------------------------------------------------------
// test_packet_goldens.cpp
//----------------------------------------------------------------------
//
// Golden-byte pins and round-trips for representative packets through
// the real stream classes (docs/RESTRUCTURING.md task 2.4; the recipe
// is the server repo's task 1.2).
//
// The .hex files under tests/golden/ ARE the wire contract. Where a
// packet is pinned in both repos, the golden here is a byte-identical
// copy of server/tests/golden/<same name>: this client and that server
// carry hand-maintained copies of every packet class, and a byte that
// moves in one repo breaks live sessions with no compile error
// anywhere. A failing golden is a protocol change to be reviewed, not
// a test to be silenced; diffing the two golden directories is the
// cross-repo check.
//
// Encrypt codes: 0 is the plain branch, 1..5 the __USE_ENCRYPTER__
// branch through every `code % N` case of SHUFFLE_STATEMENT_2.._5
// (EncryptUtility.h) - the shuffle IS part of the wire format, and the
// _4/_5 tables have non-rotation cases that codes 1..3 never reach.
// Packets that never touch the encrypter are pinned at code 0 only,
// and EncrypterFree() asserts both their write() and their read() are
// code-insensitive, so a packet that starts shuffling on either side
// cannot hide behind a single golden. (Known blind spot: the
// encrypter's bool transform is the identity for every code below 129,
// so a packet whose ONLY encrypted field were a bool would pass; none
// exists.)
//
// Fixture values follow the server's canonical-value rules where this
// file owns them: distinct per field and >= 128 in every byte the width
// allows, so a signedness flip, a same-width type swap or two
// transposed fields all move bytes in the golden. The fixtures shared
// with the server (GCMoveOK, CGMove, CGSay, the item-base ObjectIDs)
// keep the server's values, some below 128, because the point of those
// pins is byte identity with the server's files - changing them means
// re-recording both repositories in lockstep.
//
// Set UPDATE_GOLDENS=1 to (re)record instead of compare. Any other
// value compares - UPDATE_GOLDENS=0 must not silently rewrite every
// pin and pass. A recording run is deliberately NOT green: every
// recorded pin is reported as a failure, so a stale UPDATE_GOLDENS=1 in
// a shell or a CI job cannot turn the contract into a rubber stamp.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt), so the
// Packet and stream definitions are identical to the library's.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "CrtCompat.h"
#include "packet_stream_access.h"

#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "Exception.h"

#include "Cpackets/CGMove.h"
#include "Cpackets/CGSay.h"
#include "Cpackets/CGWhisper.h"
#include "Cpackets/CLLogin.h"
#include "Cpackets/CGExchangeBuy.h"
#include "Cpackets/CGAddMouseToZone.h"
#include "Cpackets/CGAddZoneToInventory.h"
#include "Cpackets/CGAddZoneToMouse.h"
#include "Cpackets/CGAttack.h"
#include "Cpackets/CGBloodDrain.h"
#include "Cpackets/CGConnect.h"
#include "Cpackets/CGDissectionCorpse.h"
#include "Cpackets/CGDropMoney.h"
#include "Cpackets/CGNPCAskAnswer.h"
#include "Cpackets/CGPortCheck.h"
#include "Cpackets/CGPickupMoney.h"
#include "Cpackets/CGSkillToInventory.h"
#include "Cpackets/CGSkillToNamed.h"
#include "Cpackets/CGSkillToObject.h"
#include "Cpackets/CGSkillToSelf.h"
#include "Cpackets/CGSkillToTile.h"
#include "Cpackets/CGUseItemFromGear.h"
#include "Cpackets/CGUseItemFromInventory.h"
#include "Cpackets/CGUsePotionFromInventory.h"
#include "Gpackets/GCAddInstalledMineToZone.h"
#include "Gpackets/GCAddNewItemToZone.h"
#include "Gpackets/GCAttack.h"
#include "Gpackets/GCDropItemToZone.h"
#include "Gpackets/GCGetDamage.h"
#include "Gpackets/GCGuildChat.h"
#include "Gpackets/GCMoveError.h"
#include "Gpackets/GCAddItemToItemVerify.h"
#include "Gpackets/GCMoveOK.h"
#include "Gpackets/GCRequestPowerPointResult.h"
#include "Gpackets/GCSay.h"
#include "Gpackets/GCSystemMessage.h"
#include "Gpackets/GCUsePowerPointResult.h"
#include "Gpackets/GCExchangeBuy.h"
#include "Gpackets/GCExchangeList.h"
#include "Rpackets/RCPositionInfo.h"

#include "Datagram.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

const uchar	kEncryptCodes[] = { 0, 1, 2, 3, 4, 5 };
const size_t	kEncryptCodeCount = sizeof(kEncryptCodes) / sizeof(kEncryptCodes[0]);

//----------------------------------------------------------------------
// Streams over a never-used socket (see packet_stream_access.h).
//----------------------------------------------------------------------
struct OutFixture
{
	Socket				m_Socket;
	SocketEncryptOutputStream	m_Stream;

	OutFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket)
	{
	}
};

struct InFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Stream;

	InFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 4096)
	{
	}
};

// The packet body exactly as a session with encrypt code `code` would
// put it on the wire. Nothing is flushed anywhere.
std::vector<unsigned char>	WriteBody(const Packet& packet, uchar code)
{
	OutFixture f;
	f.m_Stream.setEncryptCode(code);
	packet.write(f.m_Stream);
	return SocketOutputStreamTestAccess::Bytes(f.m_Stream);
}

// The packet as SocketOutputStream::write(const Packet*) frames it: id,
// size and the sequence byte, then the body. Pins the header field
// widths, which no body golden can see - a change to PacketID_t or
// PacketSize_t desyncs every packet at byte one.
std::vector<unsigned char>	WriteFramed(const Packet& packet, uchar code)
{
	OutFixture f;
	f.m_Stream.setEncryptCode(code);
	f.m_Stream.write(&packet);
	return SocketOutputStreamTestAccess::Bytes(f.m_Stream);
}

std::string	ToHex(const std::vector<unsigned char>& bytes)
{
	static const char* digits = "0123456789abcdef";
	std::string hex;
	hex.reserve(bytes.size() * 2);
	for (size_t i = 0; i < bytes.size(); i++)
	{
		hex.push_back(digits[bytes[i] >> 4]);
		hex.push_back(digits[bytes[i] & 0x0F]);
	}
	return hex;
}

bool	IsRecording()
{
	const std::optional<std::string> value = Basic::GetEnvironment("UPDATE_GOLDENS");
	return value.has_value() && *value == "1";
}

std::string	GoldenPath(const std::string& name, uchar code)
{
	std::ostringstream path;
	path << WIRE_GOLDEN_DIR << "/" << name << ".code" << (int)code << ".hex";
	return path.str();
}

// Compare `bytes` against tests/golden/<name>.code<code>.hex, or record
// it under UPDATE_GOLDENS=1. A missing golden fails with instructions.
void	ExpectGolden(const std::string& name, uchar code, const std::vector<unsigned char>& bytes)
{
	const std::string path = GoldenPath(name, code);
	const std::string actual = ToHex(bytes);

	if (IsRecording())
	{
		std::ofstream file(path.c_str(), std::ios::trunc | std::ios::binary);
		CHECK(file.good());
		file << actual << "\n";
		std::printf("  recorded golden %s\n", path.c_str());
		// See the file header: a recording run must not pass.
		::testfw::RecordCheck();
		::testfw::RecordFailure(__FILE__, __LINE__,
			"golden recorded (UPDATE_GOLDENS=1) - review the diff and re-run without it");
		return;
	}

	std::ifstream file(path.c_str(), std::ios::binary);
	if (!file.good())
	{
		std::fprintf(stderr,
			"  missing golden %s - run once with UPDATE_GOLDENS=1 to record it,\n"
			"  review the bytes against the packet's read()/write(), and commit it\n",
			path.c_str());
		CHECK(file.good());
		return;
	}
	std::string expected;
	std::getline(file, expected);
	CHECK(!expected.empty());	// an empty pin pins nothing
	if (expected != actual)
		std::fprintf(stderr,
			"  wire bytes changed for %s (encrypt code %d)\n"
			"    golden: %s\n"
			"    actual: %s\n"
			"  this is a protocol-breaking change unless the server repo ships the\n"
			"  identical change; if intended, re-record with UPDATE_GOLDENS=1\n",
			name.c_str(), (int)code, expected.c_str(), actual.c_str());
	CHECK(expected == actual);
}

// write() src with encrypt code `code`, read() the bytes back into dst
// through an input stream with the same code. Also pins the two
// invariants a packet can silently break: getPacketSize() must equal
// what write() emitted (the framing header advertises it), and read()
// must consume exactly that (a byte left over desyncs every packet
// after it).
template <class PacketT>
void	RoundTrip(const PacketT& src, PacketT& dst, uchar code)
{
	std::vector<unsigned char> body = WriteBody(src, code);
	CHECK_EQ(src.getPacketSize(), body.size());

	InFixture f;
	f.m_Stream.setEncryptCode(code);
	SocketInputStreamTestAccess::Preload(f.m_Stream, body.empty() ? NULL : &body[0],
					     (unsigned int)body.size());
	dst.read(f.m_Stream);
	CHECK(f.m_Stream.isEmpty());

	// Exercise the production framing entry point too. Use the number of bytes
	// actually serialized as the declared size so this check isolates whether
	// read() consumes exactly one body without changing any golden wire bytes.
	std::vector<unsigned char> frame(szPacketHeader);
	const PacketID_t packetID = src.getPacketID();
	const PacketSize_t packetSize = (PacketSize_t)body.size();
	const SequenceSize_t sequence = 0;
	std::memcpy(&frame[0], &packetID, szPacketID);
	std::memcpy(&frame[szPacketID], &packetSize, szPacketSize);
	std::memcpy(&frame[szPacketID + szPacketSize], &sequence, szSequenceSize);
	frame.insert(frame.end(), body.begin(), body.end());

	PacketT framedDst;
	InFixture framed;
	framed.m_Stream.setEncryptCode(code);
	SocketInputStreamTestAccess::Preload(framed.m_Stream, &frame[0],
		(unsigned int)frame.size());
	framed.m_Stream.read(&framedDst);
	CHECK(framed.m_Stream.isEmpty());
	CHECK(WriteBody(framedDst, 0) == WriteBody(src, 0));
}

// Parse `body` into a fresh packet through an input stream set to
// `code`. Throws whatever read() throws.
template <class PacketT>
void	ReadBody(PacketT& dst, const std::vector<unsigned char>& body, uchar code)
{
	InFixture f;
	f.m_Stream.setEncryptCode(code);
	SocketInputStreamTestAccess::Preload(f.m_Stream, body.empty() ? NULL : &body[0],
					     (unsigned int)body.size());
	dst.read(f.m_Stream);
}

// A packet that never calls readEncrypt/writeEncrypt writes the same
// bytes under every code AND parses the same fields from the plain
// bytes under every code. Both sides are checked: a shuffle added to
// read() alone would leave write() and the code-0 golden untouched
// while every live session (nonzero code) mis-parsed. Field equality
// is proved by re-serialising the parsed packet at code 0 - write()
// emits every field, so equal bytes mean equal fields.
template <class PacketT>
bool	EncrypterFree(const PacketT& packet)
{
	const std::vector<unsigned char> plain = WriteBody(packet, 0);
	for (size_t i = 1; i < kEncryptCodeCount; i++)
	{
		if (WriteBody(packet, kEncryptCodes[i]) != plain)
			return false;
		PacketT parsed;
		ReadBody(parsed, plain, kEncryptCodes[i]);
		if (WriteBody(parsed, 0) != plain)
			return false;
	}
	return true;
}

// True when read() refuses `body` with InvalidProtocolException - the
// parsers' bounds checks, which are the whole point of the wire library
// being testable.
template <class PacketT>
bool	Rejects(const std::vector<unsigned char>& body)
{
	try {
		PacketT dst;
		ReadBody(dst, body, 0);
	} catch (InvalidProtocolException&) {
		return true;
	}
	return false;
}

void	Append(std::vector<unsigned char>& out, const void* p, size_t n)
{
	const unsigned char* b = (const unsigned char*)p;
	out.insert(out.end(), b, b + n);
}

// Round-trip + goldens for a fixed-width encrypter packet whose fixture
// is shared with the server: field equality after the round-trip is
// proved by re-serialising both sides at code 0 (write() emits every
// field), so no per-packet getter list is needed.
template <class PacketT>
void	PinSharedEncrypterPacket(const char* name, void (*fill)(PacketT&))
{
	for (size_t i = 0; i < kEncryptCodeCount; i++)
	{
		PacketT src, dst;
		fill(src);
		RoundTrip(src, dst, kEncryptCodes[i]);
		CHECK(WriteBody(dst, 0) == WriteBody(src, 0));
	}
	PacketT packet;
	fill(packet);
	for (size_t i = 0; i < kEncryptCodeCount; i++)
		ExpectGolden(name, kEncryptCodes[i], WriteBody(packet, kEncryptCodes[i]));
}

//----------------------------------------------------------------------
// Fixtures shared with the server repo (values copied from its
// packet_roundtrip_test.cpp / packet_encrypter_test.cpp so the goldens
// are the same files).
//----------------------------------------------------------------------
void	Fill(GCMoveOK& p)	{ p.setX(11); p.setY(22); p.setDir(3); }
void	Fill(CGMove& p)		{ p.setX(101); p.setY(57); p.setDir(6); }
void	Fill(GCMoveError& p)	{ p.setX(0x91); p.setY(0xA3); }
void	Fill(CGSay& p)		{ p.setColor(0x11223344); p.setMessage("hello darkeden"); }
void	Fill(CGWhisper& p)
{
	p.setName("Reiot");
	p.setColor(0xCAFEBABE);
	p.setMessage("wire pin test");
}

// GCAddItemToZone is the abstract base of the three item-to-zone
// packets and the only encrypter layout with a variable-length tail: a
// BYTE-counted option list and a BYTE-counted SubItemInfo list follow
// the shuffled header unencrypted. Two options and one sub-item, so
// both counts are non-trivial and the per-element layout is pinned.
void	FillItemBase(GCAddItemToZone& p)
{
	p.setObjectID(0xE0F10213);
	p.setX(0x87);
	p.setY(0x99);
	p.setItemClass(0xAB);
	p.setItemType(0x8ABC);
	p.addOptionType(0xBD);
	p.addOptionType(0xCF);
	p.setSilver(0x9DEF);
	p.setGrade(-0x12345678);
	p.setDurability(0xF2031425);
	p.setEnchantLevel(-0x37);
	p.setItemNum(0xD1);
	SubItemInfo* pSub = new SubItemInfo();
	pSub->setObjectID(0x03142536);
	pSub->setItemClass(0xE3);
	pSub->setItemType(0x9BCD);
	pSub->setItemNum(0xF5);
	pSub->setSlotID(0x86);
	p.addListElement(pSub);	// the packet's destructor owns it
	p.setListNum(1);
}

void	CheckItemBaseEqual(GCAddItemToZone& a, GCAddItemToZone& b)
{
	CHECK_EQ(a.getObjectID(), b.getObjectID());
	CHECK_EQ(a.getX(), b.getX());
	CHECK_EQ(a.getY(), b.getY());
	CHECK_EQ(a.getItemClass(), b.getItemClass());
	CHECK_EQ(a.getItemType(), b.getItemType());
	CHECK(a.getOptionType() == b.getOptionType());
	CHECK_EQ(a.getSilver(), b.getSilver());
	CHECK_EQ(a.getGrade(), b.getGrade());
	CHECK_EQ(a.getDurability(), b.getDurability());
	CHECK_EQ(a.getEnchantLevel(), b.getEnchantLevel());
	CHECK_EQ(a.getItemNum(), b.getItemNum());
	CHECK_EQ(a.getListNum(), b.getListNum());
	// No const accessor for the sub-item list: pop the single element
	// from each side and compare. popFrontListElement() is front()/
	// pop_front() with no emptiness check, so a parse that yielded no
	// sub-item must be reported here, not turned into undefined
	// behaviour inside the expected failure. Popped elements are ours
	// to free.
	CHECK_EQ(1, a.getListNum());
	CHECK_EQ(1, b.getListNum());
	if (a.getListNum() != 1 || b.getListNum() != 1)
		return;
	SubItemInfo* pa = a.popFrontListElement();
	SubItemInfo* pb = b.popFrontListElement();
	CHECK(pa != NULL && pb != NULL);
	if (pa != NULL && pb != NULL)
	{
		CHECK_EQ(pa->getObjectID(), pb->getObjectID());
		CHECK_EQ(pa->getItemClass(), pb->getItemClass());
		CHECK_EQ(pa->getItemType(), pb->getItemType());
		CHECK_EQ(pa->getItemNum(), pb->getItemNum());
		CHECK_EQ(pa->getSlotID(), pb->getSlotID());
	}
	delete pa;
	delete pb;
}

//----------------------------------------------------------------------
// The other fifteen encrypter packets, values copied from the server's
// packet_encrypter_test.cpp so the goldens are the same files (the
// server pins all nineteen encrypter users; so does this file now).
//----------------------------------------------------------------------
void	Fill(CGSkillToSelf& p)		{ p.setSkillType(0x8A1B); p.setCEffectID(0x9C2D); }
void	Fill(CGUseItemFromGear& p)	{ p.setObjectID(0xA1B2C3D4); p.setPart(0x85); }
void	Fill(CGAddZoneToMouse& p)	{ p.setObjectID(0xB3C4D5E6); p.setZoneX(0x97); p.setZoneY(0xA9); }
void	Fill(CGNPCAskAnswer& p)		{ p.setObjectID(0xC5D6E7F8); p.setScriptID(0x89ABCDEF); p.setAnswerID(0xB1); }
void	Fill(CGPickupMoney& p)		{ p.setObjectID(0xD7E8F90A); p.setZoneX(0x9B); p.setZoneY(0xAD); }
void	Fill(CGSkillToObject& p)	{ p.setSkillType(0x8E3F); p.setCEffectID(0x9D4A); p.setTargetObjectID(0xE9FA0B1C); }
void	Fill(CGUseItemFromInventory& p)	{ p.setObjectID(0xFB0C1D2E); p.setX(0x93); p.setY(0xA5); }
void	Fill(CGUsePotionFromInventory& p) { p.setObjectID(0x8C9DAEBF); p.setX(0xB5); p.setY(0xC7); }
void	Fill(CGAttack& p)		{ p.setObjectID(0x9EAFB0C1); p.setX(0x83); p.setY(0x95); p.setDir(0xA7); }
void	Fill(CGDissectionCorpse& p)	{ p.setObjectID(0xACBDCEDF); p.setX(0x89); p.setY(0x9B); p.setPet(0xAD); }
void	Fill(CGSkillToTile& p)		{ p.setSkillType(0x8F5B); p.setCEffectID(0x9E6C); p.setX(0xB9); p.setY(0xCB); }
void	Fill(CGAddZoneToInventory& p)
{
	p.setObjectID(0xBACBDCED);
	p.setZoneX(0x8B);
	p.setZoneY(0x9D);
	p.setInvenX(0xAF);
	p.setInvenY(0xC1);
}
void	Fill(CGSkillToInventory& p)
{
	p.setSkillType(0x8D7E);
	p.setObjectID(0xCEDFE0F1);
	p.setX(0x91);
	p.setY(0xA3);
	p.setTargetX(0xB5);
	p.setTargetY(0xC7);
}
void	Fill(CGAddMouseToZone& p)	{ p.setObjectID(0x14253647); }
void	Fill(CGDropMoney& p)		{ p.setAmount(0x8899AABB); }

// CGSkillToNamed: the CGSkillTo* family's encrypter-free member, pinned
// at code 0 only.
void	Fill(CGSkillToNamed& p)
{
	p.setSkillType(0x8C4D);
	p.setCEffectID(0x9F7A);
	p.setTargetName("Reiot");
}

// The combat broadcasts the server pins at code 0 in
// packet_combat_test.cpp: neither reaches the encrypter. Values are
// the server's, so the goldens are byte-identical copies of its files.
void	Fill(GCAttack& p)		{ p.setObjectID(0x81A2B3C4); p.setX(0x85); p.setY(0x96); p.setDir(0xA7); }
void	Fill(GCGetDamage& p)		{ p.setObjectID(0x82A3B4C5); p.setDamage(0x86D7); }

// CGBloodDrain: the client's half of the same combat exchange, not
// pinned by the server. Encrypter-free; only the ObjectID is on the
// wire (X/Y/Dir are commented out of read()/write() upstream).
void	Fill(CGBloodDrain& p)		{ p.setObjectID(0x84A5B6C7); }

//----------------------------------------------------------------------
// Client-authored fixtures: the chat/guild/system-message family the
// code-health review named among the highest-risk parsers (the layout
// pins here; their length bounds are pinned separately below), and the
// login packet whose layout switches on the netmarble flag.
//----------------------------------------------------------------------
void	Fill(GCSay& p)
{
	p.setObjectID(0x81A2C3E4);
	p.setColor(0x95B6D7F8);
	p.setMessage("wire pin say");
}

// type != 0 carries the sending guild's name before the sender.
void	Fill(GCGuildChat& p)
{
	p.setType(0x85);
	p.setSendGuildName("Nosferatu");
	p.setSender("Reiot");
	p.setColor(0xA1B2C3D4);
	p.setMessage("guild wire pin");
}

void	FillType0(GCGuildChat& p)
{
	p.setType(0);
	p.setSender("Reiot");
	p.setColor(0xA1B2C3D4);
	p.setMessage("guild wire pin");
}

void	Fill(GCSystemMessage& p)
{
	p.setMessage("system wire pin");
	p.setColor(0xC1D2E3F4);
	p.setType(SYSTEM_MESSAGE_COMBAT);
}

void	Fill(CLLogin& p)
{
	const BYTE mac[6] = { 0x80, 0x91, 0xA2, 0xB3, 0xC4, 0xD5 };
	p.setID("reiot");
	p.setPassword("wirepin");
	p.setMacAddress(mac);
}

// CGConnect: the server's fixture from packet_gameserver_handshake_test.cpp
// (it builds the instance by reading a crafted image, since its copy has
// no MAC setter; the values are the same), so the golden is a
// byte-identical copy of its file.
void	Fill(CGConnect& p)
{
	const BYTE mac[6] = { 0x8A, 0x9B, 0xAC, 0xBD, 0xCE, 0xDF };
	p.setKey(0xB7A69584);
	p.setPCType(PC_OUSTERS);
	p.setPCName("GoldConnectPC");
	p.setMacAddress(mac);
}

void	Fill(CGExchangeBuy& p)
{
	p.setListingID(0xF9E8D7C6B5A49382ULL);
	p.setIdempotencyKey("idem-key-0123456789abcdef");
}

void	Fill(GCExchangeBuy& p)
{
	p.setSuccess(true);
	p.setMessage("purchase complete");
	p.setOrderID(0x8192A3B4C5D6E7F8ULL);
}

std::string	ExchangeHighBytes(size_t length, unsigned char seed)
{
	std::string value;
	value.reserve(length);
	for (size_t i = 0; i < length; i++)
		value.push_back((char)(unsigned char)(0x80 + ((seed + i) % 0x7F)));
	return value;
}

ExchangeListing	MakeFullExchangeListing()
{
	ExchangeListing listing;
	listing.listingID = static_cast<int64_t>(0x8899AABBCCDDEEFFULL);
	listing.serverID = (int16_t)0x9A8B;
	listing.sellerAccount = "sellerAcct";
	listing.sellerPlayer = "SellerHero";
	listing.sellerRace = 0x81;
	listing.itemClass = 0x92;
	listing.itemType = 0xA3B4;
	listing.itemID = static_cast<int64_t>(0xC5D6E7F899A8B7C6ULL);
	listing.objectID = (int)0xB4C5D6E7;
	listing.pricePoint = (int)0xC6D7E8F9;
	listing.currency = 0xD9;
	listing.status = 0xEA;
	listing.buyerAccount = "buyerAcct";
	listing.buyerPlayer = "BuyerHero";
	listing.taxRate = 0xFB;
	listing.taxAmount = (int)0x8C9DAEBF;
	listing.createdAt = "2026-08-30 12:34:56";
	listing.expireAt = "2026-09-06 12:34:56";
	listing.version = (int)0x9DAEBFC8;
	listing.itemName = "Blood Sword";
	listing.enchantLevel = 0x8D;
	listing.grade = 0xAEBF;
	listing.durability = (int)0xBFC8D9EA;
	listing.silver = 0xC8D9;
	listing.optionType1 = 0xE2;
	listing.optionType2 = 0xF3;
	listing.optionType3 = 0x84;
	listing.optionValue1 = 0x95A6;
	listing.optionValue2 = 0xA6B7;
	listing.optionValue3 = 0xB7C8;
	listing.stackCount = (int)0xD8E9FA8B;
	return listing;
}

ExchangeListing	MakeEmptyStringExchangeListing()
{
	ExchangeListing listing;
	listing.listingID = static_cast<int64_t>(0x91A2B3C4D5E6F788ULL);
	listing.serverID = (int16_t)0xABCC;
	listing.sellerRace = 0x83;
	listing.itemClass = 0x94;
	listing.itemType = 0xA5B6;
	listing.itemID = static_cast<int64_t>(0xA2B3C4D5E6F78899ULL);
	listing.objectID = (int)0xB3C4D5E6;
	listing.pricePoint = (int)0xC4D5E6F7;
	listing.currency = 0xDB;
	listing.status = 0xEC;
	listing.taxRate = 0xFD;
	listing.taxAmount = (int)0x8E9FA8B1;
	listing.version = (int)0x9FA8B1C2;
	listing.enchantLevel = 0x8F;
	listing.grade = 0xB8C1;
	listing.durability = (int)0xC1D2E3F4;
	listing.silver = 0xD2E3;
	listing.optionType1 = 0xE4;
	listing.optionType2 = 0xF5;
	listing.optionType3 = 0x86;
	listing.optionValue1 = 0x97A8;
	listing.optionValue2 = 0xA8B9;
	listing.optionValue3 = 0xB9CA;
	listing.stackCount = (int)0xE8F19293;
	return listing;
}

ExchangeListing	MakeMaxStringExchangeListing()
{
	ExchangeListing listing = MakeFullExchangeListing();
	const size_t length = GCExchangeList::kMaxListingString;
	listing.sellerAccount = ExchangeHighBytes(length, 0x01);
	listing.sellerPlayer = ExchangeHighBytes(length, 0x11);
	listing.buyerAccount = ExchangeHighBytes(length, 0x21);
	listing.buyerPlayer = ExchangeHighBytes(length, 0x31);
	listing.createdAt = ExchangeHighBytes(length, 0x41);
	listing.expireAt = ExchangeHighBytes(length, 0x51);
	listing.itemName = ExchangeHighBytes(length, 0x61);
	return listing;
}

void	Fill(GCExchangeList& p)
{
	p.setPage((int)0x8A9BACBD);
	p.setPageSize((int)0x9BACBDCE);
	p.setTotal((int)0xACBDCEDF);
	std::vector<ExchangeListing> listings;
	listings.push_back(MakeFullExchangeListing());
	listings.push_back(MakeEmptyStringExchangeListing());
	p.setListings(listings);
}

void	CheckExchangeListingEqual(const ExchangeListing& expected,
	const ExchangeListing& actual)
{
	CHECK_EQ(expected.listingID, actual.listingID);
	CHECK_EQ(expected.serverID, actual.serverID);
	CHECK(expected.sellerAccount == actual.sellerAccount);
	CHECK(expected.sellerPlayer == actual.sellerPlayer);
	CHECK_EQ(expected.sellerRace, actual.sellerRace);
	CHECK_EQ(expected.itemClass, actual.itemClass);
	CHECK_EQ(expected.itemType, actual.itemType);
	CHECK_EQ(expected.itemID, actual.itemID);
	CHECK_EQ(expected.objectID, actual.objectID);
	CHECK_EQ(expected.pricePoint, actual.pricePoint);
	CHECK_EQ(expected.currency, actual.currency);
	CHECK_EQ(expected.status, actual.status);
	CHECK(expected.buyerAccount == actual.buyerAccount);
	CHECK(expected.buyerPlayer == actual.buyerPlayer);
	CHECK_EQ(expected.taxRate, actual.taxRate);
	CHECK_EQ(expected.taxAmount, actual.taxAmount);
	CHECK(expected.createdAt == actual.createdAt);
	CHECK(expected.expireAt == actual.expireAt);
	CHECK_EQ(expected.version, actual.version);
	CHECK(expected.itemName == actual.itemName);
	CHECK_EQ(expected.enchantLevel, actual.enchantLevel);
	CHECK_EQ(expected.grade, actual.grade);
	CHECK_EQ(expected.durability, actual.durability);
	CHECK_EQ(expected.silver, actual.silver);
	CHECK_EQ(expected.optionType1, actual.optionType1);
	CHECK_EQ(expected.optionType2, actual.optionType2);
	CHECK_EQ(expected.optionType3, actual.optionType3);
	CHECK_EQ(expected.optionValue1, actual.optionValue1);
	CHECK_EQ(expected.optionValue2, actual.optionValue2);
	CHECK_EQ(expected.optionValue3, actual.optionValue3);
	CHECK_EQ(expected.stackCount, actual.stackCount);
}

void	CheckExchangeListEqual(const GCExchangeList& expected,
	const GCExchangeList& actual)
{
	CHECK_EQ(expected.getPage(), actual.getPage());
	CHECK_EQ(expected.getPageSize(), actual.getPageSize());
	CHECK_EQ(expected.getTotal(), actual.getTotal());
	CHECK_EQ(expected.getListings().size(), actual.getListings().size());
	if (expected.getListings().size() == actual.getListings().size())
	{
		for (size_t i = 0; i < expected.getListings().size(); i++)
			CheckExchangeListingEqual(expected.getListings()[i], actual.getListings()[i]);
	}
}

} // namespace

//----------------------------------------------------------------------
// GCMoveOK - fixed-width fields through SHUFFLE_STATEMENT_3
//----------------------------------------------------------------------
TEST(GCMoveOK, RoundTripsForEveryEncryptCode)
{
	for (size_t i = 0; i < kEncryptCodeCount; i++)
	{
		GCMoveOK src, dst;
		Fill(src);
		RoundTrip(src, dst, kEncryptCodes[i]);
		CHECK_EQ(src.getX(), dst.getX());
		CHECK_EQ(src.getY(), dst.getY());
		CHECK_EQ(src.getDir(), dst.getDir());
	}
}

TEST(GCMoveOK, BodyBytesMatchTheSharedGoldenForEveryEncryptCode)
{
	GCMoveOK packet;
	Fill(packet);
	for (size_t i = 0; i < kEncryptCodeCount; i++)
		ExpectGolden("GCMoveOK", kEncryptCodes[i], WriteBody(packet, kEncryptCodes[i]));
}

// The one framed golden: header widths (ushort id, uint size, BYTE
// sequence) plus the body, the same file the server pins.
TEST(GCMoveOK, FramedBytesMatchTheSharedGolden)
{
	GCMoveOK packet;
	Fill(packet);
	ExpectGolden("GCMoveOK.framed", 0, WriteFramed(packet, 0));
}

//----------------------------------------------------------------------
// CGMove - the client-to-server twin
//----------------------------------------------------------------------
TEST(CGMove, RoundTripsForEveryEncryptCode)
{
	for (size_t i = 0; i < kEncryptCodeCount; i++)
	{
		CGMove src, dst;
		Fill(src);
		RoundTrip(src, dst, kEncryptCodes[i]);
		CHECK_EQ(src.getX(), dst.getX());
		CHECK_EQ(src.getY(), dst.getY());
		CHECK_EQ(src.getDir(), dst.getDir());
	}
}

TEST(CGMove, BodyBytesMatchTheSharedGoldenForEveryEncryptCode)
{
	CGMove packet;
	Fill(packet);
	for (size_t i = 0; i < kEncryptCodeCount; i++)
		ExpectGolden("CGMove", kEncryptCodes[i], WriteBody(packet, kEncryptCodes[i]));
}

//----------------------------------------------------------------------
// GCMoveError - SHUFFLE_STATEMENT_2
//----------------------------------------------------------------------
TEST(GCMoveError, RoundTripsForEveryEncryptCode)
{
	for (size_t i = 0; i < kEncryptCodeCount; i++)
	{
		GCMoveError src, dst;
		Fill(src);
		RoundTrip(src, dst, kEncryptCodes[i]);
		CHECK_EQ(src.getX(), dst.getX());
		CHECK_EQ(src.getY(), dst.getY());
	}
}

TEST(GCMoveError, BodyBytesMatchTheSharedGoldenForEveryEncryptCode)
{
	GCMoveError packet;
	Fill(packet);
	for (size_t i = 0; i < kEncryptCodeCount; i++)
		ExpectGolden("GCMoveError", kEncryptCodes[i], WriteBody(packet, kEncryptCodes[i]));
}

//----------------------------------------------------------------------
// The GCAddItemToZone family - shuffled header + unencrypted lists
//----------------------------------------------------------------------
TEST(GCAddNewItemToZone, RoundTripsForEveryEncryptCode)
{
	for (size_t i = 0; i < kEncryptCodeCount; i++)
	{
		GCAddNewItemToZone src, dst;
		FillItemBase(src);
		RoundTrip(src, dst, kEncryptCodes[i]);
		CheckItemBaseEqual(src, dst);
	}
}

TEST(GCAddNewItemToZone, BodyBytesMatchTheSharedGoldenForEveryEncryptCode)
{
	GCAddNewItemToZone packet;
	FillItemBase(packet);
	for (size_t i = 0; i < kEncryptCodeCount; i++)
		ExpectGolden("GCAddNewItemToZone", kEncryptCodes[i], WriteBody(packet, kEncryptCodes[i]));
}

namespace {

// A short transport fragment must wait for more bytes. A complete frame whose
// declared body is a shortened valid body must be rejected, even when a nested
// legacy decoder logs and swallows its read error. Follow every malformed frame
// with a valid one to check both alignment and recovery on the same stream.
template <class PacketT>
void CheckItemBodyPrefixes(uchar code)
{
	PacketT source;
	FillItemBase(source);
	const std::vector<unsigned char> valid = WriteFramed(source, code);
	const PacketSize_t bodySize = source.getPacketSize();
	CHECK_EQ(szPacketHeader + bodySize, valid.size());

	for (PacketSize_t prefix = 0; prefix < bodySize; ++prefix)
	{
		InFixture input;
		input.m_Stream.setEncryptCode(code);
		const uint head = input.m_Stream.capacity() - szPacketHeader - 2;
		const uint fragmentSize = szPacketHeader + prefix;
		SocketInputStreamTestAccess::Preload(input.m_Stream, valid.data(), fragmentSize, head);
		PacketT fragment;
		bool incomplete = false;
		try { input.m_Stream.read(&fragment); }
		catch (InsufficientDataException&) { incomplete = true; }
		CHECK(incomplete);
		CHECK_EQ(fragmentSize, input.m_Stream.length());
		std::vector<char> unchanged(fragmentSize);
		CHECK(input.m_Stream.peek(unchanged.data(), fragmentSize));
		CHECK_EQ(0, std::memcmp(unchanged.data(), valid.data(), fragmentSize));

		std::vector<unsigned char> wire(valid.begin(), valid.begin() + fragmentSize);
		std::memcpy(wire.data() + szPacketID, &prefix, szPacketSize);
		wire.insert(wire.end(), valid.begin(), valid.end());
		SocketInputStreamTestAccess::Preload(input.m_Stream, wire.data(), (uint)wire.size(), head);
		PacketT malformed;
		bool rejected = false;
		try { input.m_Stream.read(&malformed); }
		catch (InvalidProtocolException&) { rejected = true; }
		if (!rejected)
			std::fprintf(stderr, "accepted short item body: packet=%u code=%u bytes=%u/%u\n",
				(unsigned)source.getPacketID(), (unsigned)code, (unsigned)prefix, (unsigned)bodySize);
		CHECK(rejected);
		CHECK_EQ(valid.size(), input.m_Stream.length());
		PacketT following;
		input.m_Stream.read(&following);
		CHECK(input.m_Stream.isEmpty());
		CHECK(WriteFramed(following, code) == valid);
	}
}

} // namespace

TEST(GCAddNewItemToZone, EveryShortBodyIsRejectedAndTransportFragmentsArePreserved)
{
	for (uchar code : kEncryptCodes)
		CheckItemBodyPrefixes<GCAddNewItemToZone>(code);
}

TEST(GCAddInstalledMineToZone, EveryShortBodyIsRejectedAndTransportFragmentsArePreserved)
{
	for (uchar code : kEncryptCodes)
		CheckItemBodyPrefixes<GCAddInstalledMineToZone>(code);
}

TEST(GCAddInstalledMineToZone, RoundTripsForEveryEncryptCode)
{
	for (size_t i = 0; i < kEncryptCodeCount; i++)
	{
		GCAddInstalledMineToZone src, dst;
		FillItemBase(src);
		RoundTrip(src, dst, kEncryptCodes[i]);
		CheckItemBaseEqual(src, dst);
	}
}

TEST(GCAddInstalledMineToZone, BodyBytesMatchTheSharedGoldenForEveryEncryptCode)
{
	GCAddInstalledMineToZone packet;
	FillItemBase(packet);
	for (size_t i = 0; i < kEncryptCodeCount; i++)
		ExpectGolden("GCAddInstalledMineToZone", kEncryptCodes[i], WriteBody(packet, kEncryptCodes[i]));
}

// GCDropItemToZone appends a pet ObjectID after the base layout. The
// server repo pins only its write(): its read() still consumes a
// leading flag byte that write() no longer emits (server RESTRUCTURING
// 1.2). This copy's read() consumes exactly what write() emits, so the
// client also round-trips - and the shared golden proves the client
// parses what the server actually sends.
TEST(GCDropItemToZone, RoundTripsForEveryEncryptCode)
{
	for (size_t i = 0; i < kEncryptCodeCount; i++)
	{
		GCDropItemToZone src, dst;
		FillItemBase(src);
		src.setDropPetOID(0x47586970);
		RoundTrip(src, dst, kEncryptCodes[i]);
		CheckItemBaseEqual(src, dst);
		CHECK_EQ(src.getDropPetOID(), dst.getDropPetOID());
	}
}

TEST(GCDropItemToZone, BodyBytesMatchTheSharedGoldenForEveryEncryptCode)
{
	GCDropItemToZone packet;
	FillItemBase(packet);
	packet.setDropPetOID(0x47586970);
	for (size_t i = 0; i < kEncryptCodeCount; i++)
		ExpectGolden("GCDropItemToZone", kEncryptCodes[i], WriteBody(packet, kEncryptCodes[i]));
}

//----------------------------------------------------------------------
// CGSay / CGWhisper - BYTE-length-prefixed strings, encrypter-free
//----------------------------------------------------------------------
TEST(CGSay, RoundTripsAndMatchesTheSharedGolden)
{
	CGSay src, dst;
	Fill(src);
	CHECK(EncrypterFree(src));
	RoundTrip(src, dst, 0);
	CHECK_EQ(src.getColor(), dst.getColor());
	CHECK(src.getMessage() == dst.getMessage());
	ExpectGolden("CGSay", 0, WriteBody(src, 0));
}

TEST(CGWhisper, RoundTripsAndMatchesTheSharedGolden)
{
	CGWhisper src, dst;
	Fill(src);
	CHECK(EncrypterFree(src));
	RoundTrip(src, dst, 0);
	CHECK(src.getName() == dst.getName());
	CHECK_EQ(src.getColor(), dst.getColor());
	CHECK(src.getMessage() == dst.getMessage());
	ExpectGolden("CGWhisper", 0, WriteBody(src, 0));
}

//----------------------------------------------------------------------
// The other fifteen encrypter packets, shared goldens (see Fill above)
//----------------------------------------------------------------------
#define SHARED_ENCRYPTER_PIN(Name)						\
	TEST(Name, RoundTripsAndMatchesTheSharedGoldenForEveryEncryptCode)	\
	{								\
		PinSharedEncrypterPacket<Name>(#Name, &Fill);		\
	}
SHARED_ENCRYPTER_PIN(CGSkillToSelf)
SHARED_ENCRYPTER_PIN(CGUseItemFromGear)
SHARED_ENCRYPTER_PIN(CGAddZoneToMouse)
SHARED_ENCRYPTER_PIN(CGNPCAskAnswer)
SHARED_ENCRYPTER_PIN(CGPickupMoney)
SHARED_ENCRYPTER_PIN(CGSkillToObject)
SHARED_ENCRYPTER_PIN(CGUseItemFromInventory)
SHARED_ENCRYPTER_PIN(CGUsePotionFromInventory)
SHARED_ENCRYPTER_PIN(CGAttack)
SHARED_ENCRYPTER_PIN(CGDissectionCorpse)
SHARED_ENCRYPTER_PIN(CGSkillToTile)
SHARED_ENCRYPTER_PIN(CGAddZoneToInventory)
SHARED_ENCRYPTER_PIN(CGSkillToInventory)
SHARED_ENCRYPTER_PIN(CGAddMouseToZone)
SHARED_ENCRYPTER_PIN(CGDropMoney)
#undef SHARED_ENCRYPTER_PIN

//----------------------------------------------------------------------
// CGSkillToNamed - the CGSkillTo* family's encrypter-free member
//----------------------------------------------------------------------
TEST(CGSkillToNamed, RoundTripsAndMatchesGolden)
{
	CGSkillToNamed src, dst;
	Fill(src);
	CHECK(EncrypterFree(src));
	RoundTrip(src, dst, 0);
	CHECK_EQ(src.getSkillType(), dst.getSkillType());
	CHECK_EQ(src.getCEffectID(), dst.getCEffectID());
	CHECK(src.getTargetName() == dst.getTargetName());
	ExpectGolden("CGSkillToNamed", 0, WriteBody(src, 0));
}

//----------------------------------------------------------------------
// The melee combat exchange: CGBloodDrain up, GCAttack and GCGetDamage
// down. The GC pair's goldens are the server's; the CG one is
// client-authored. Pinned ahead of their typed-wire migration.
//----------------------------------------------------------------------
TEST(GCAttack, RoundTripsAndMatchesTheSharedGolden)
{
	GCAttack src, dst;
	Fill(src);
	CHECK(EncrypterFree(src));
	RoundTrip(src, dst, 0);
	CHECK_EQ(src.getObjectID(), dst.getObjectID());
	CHECK_EQ(src.getX(), dst.getX());
	CHECK_EQ(src.getY(), dst.getY());
	CHECK_EQ(src.getDir(), dst.getDir());
	ExpectGolden("GCAttack", 0, WriteBody(src, 0));
}

TEST(GCGetDamage, RoundTripsAndMatchesTheSharedGolden)
{
	GCGetDamage src, dst;
	Fill(src);
	CHECK(EncrypterFree(src));
	RoundTrip(src, dst, 0);
	CHECK_EQ(src.getObjectID(), dst.getObjectID());
	CHECK_EQ(src.getDamage(), dst.getDamage());
	ExpectGolden("GCGetDamage", 0, WriteBody(src, 0));
}

TEST(CGBloodDrain, RoundTripsAndMatchesGolden)
{
	CGBloodDrain src, dst;
	Fill(src);
	CHECK(EncrypterFree(src));
	RoundTrip(src, dst, 0);
	CHECK_EQ(src.getObjectID(), dst.getObjectID());
	ExpectGolden("CGBloodDrain", 0, WriteBody(src, 0));
}

//----------------------------------------------------------------------
// Client-authored pins: chat, guild chat, system message, login
//----------------------------------------------------------------------
TEST(GCSay, RoundTripsAndMatchesGolden)
{
	GCSay src, dst;
	Fill(src);
	CHECK(EncrypterFree(src));
	RoundTrip(src, dst, 0);
	CHECK_EQ(src.getObjectID(), dst.getObjectID());
	CHECK_EQ(src.getColor(), dst.getColor());
	CHECK(src.getMessage() == dst.getMessage());
	ExpectGolden("GCSay", 0, WriteBody(src, 0));
}

TEST(GCGuildChat, RoundTripsAndMatchesGoldenWithAndWithoutTheGuildName)
{
	{
		GCGuildChat src, dst;
		Fill(src);
		CHECK(EncrypterFree(src));
		RoundTrip(src, dst, 0);
		CHECK_EQ(src.getType(), dst.getType());
		CHECK(src.getSendGuildName() == dst.getSendGuildName());
		CHECK(src.getSender() == dst.getSender());
		CHECK_EQ(src.getColor(), dst.getColor());
		CHECK(src.getMessage() == dst.getMessage());
		ExpectGolden("GCGuildChat", 0, WriteBody(src, 0));
	}
	{
		GCGuildChat src, dst;
		FillType0(src);
		RoundTrip(src, dst, 0);
		CHECK_EQ(src.getType(), dst.getType());
		CHECK(std::string() == dst.getSendGuildName());
		CHECK(src.getSender() == dst.getSender());
		CHECK(src.getMessage() == dst.getMessage());
		ExpectGolden("GCGuildChat.type0", 0, WriteBody(src, 0));
	}
}

TEST(GCSystemMessage, RoundTripsAndMatchesGolden)
{
	GCSystemMessage src, dst;
	Fill(src);
	CHECK(EncrypterFree(src));
	RoundTrip(src, dst, 0);
	CHECK(src.getMessage() == dst.getMessage());
	CHECK_EQ(src.getColor(), dst.getColor());
	CHECK_EQ((int)src.getType(), (int)dst.getType());
	ExpectGolden("GCSystemMessage", 0, WriteBody(src, 0));
}

// GCExchangeList is a representative span migration: its body contains two
// raw 64-bit values and seven length-prefixed strings copied through a fixed
// staging buffer. Pin both parsing and every emitted byte before changing the
// stream calls.
TEST(GCExchangeList, SpanMigrationPreservesTheWireLayout)
{
	GCExchangeList src, dst;
	Fill(src);
	CHECK(EncrypterFree(src));
	RoundTrip(src, dst, 0);
	CheckExchangeListEqual(src, dst);
	ExpectGolden("GCExchangeList", 0, WriteBody(src, 0));
}

TEST(GCExchangeList, SpanMigrationPreservesMaxLengthStrings)
{
	GCExchangeList src, dst;
	Fill(src);
	std::vector<ExchangeListing> listings;
	listings.push_back(MakeMaxStringExchangeListing());
	src.setListings(listings);

	RoundTrip(src, dst, 0);
	CheckExchangeListEqual(src, dst);
	CHECK_EQ(1, dst.getListings().size());
	if (dst.getListings().size() == 1)
	{
		CHECK_EQ(GCExchangeList::kMaxListingString,
			dst.getListings()[0].sellerAccount.size());
		CHECK_EQ(GCExchangeList::kMaxListingString,
			dst.getListings()[0].sellerPlayer.size());
		CHECK_EQ(GCExchangeList::kMaxListingString,
			dst.getListings()[0].buyerAccount.size());
		CHECK_EQ(GCExchangeList::kMaxListingString,
			dst.getListings()[0].buyerPlayer.size());
		CHECK_EQ(GCExchangeList::kMaxListingString,
			dst.getListings()[0].createdAt.size());
		CHECK_EQ(GCExchangeList::kMaxListingString,
			dst.getListings()[0].expireAt.size());
		CHECK_EQ(GCExchangeList::kMaxListingString,
			dst.getListings()[0].itemName.size());
	}
}

TEST(CGExchangeBuy, TypedScalarMigrationPreservesSharedGolden)
{
	CGExchangeBuy src, dst;
	Fill(src);
	CHECK(EncrypterFree(src));
	RoundTrip(src, dst, 0);
	CHECK_EQ(src.getListingID(), dst.getListingID());
	CHECK(src.getIdempotencyKey() == dst.getIdempotencyKey());
	ExpectGolden("CGExchangeBuy", 0, WriteBody(src, 0));
}

TEST(GCExchangeBuy, TypedScalarMigrationPreservesSharedGolden)
{
	GCExchangeBuy src, dst;
	Fill(src);
	CHECK(EncrypterFree(src));
	RoundTrip(src, dst, 0);
	CHECK_EQ(src.isSuccess(), dst.isSuccess());
	CHECK(src.getMessage() == dst.getMessage());
	CHECK_EQ(src.getOrderID(), dst.getOrderID());
	ExpectGolden("GCExchangeBuy", 0, WriteBody(src, 0));
}

// CLLogin is written by this client and read by the login server, whose
// read() consumes id, password, MAC and a trailing login-mode byte -
// exactly what write() here emits, and what the plain golden pins. The
// netmarble layout - a 4-byte ID length and no password - is pinned by
// golden only: neither repo's read() parses it. Both variants can be
// recorded because the flag is a packet member now (task 2.4 cut the
// g_pUserInformation read out of the wire class).
TEST(CLLogin, PlainLayoutMatchesGolden)
{
	CLLogin packet;
	Fill(packet);
	CHECK(EncrypterFree(packet));
	CHECK(!packet.isNetmarble());
	const std::vector<unsigned char> body = WriteBody(packet, 0);
	CHECK_EQ(packet.getPacketSize(), body.size());
	ExpectGolden("CLLogin", 0, body);
}

// Stated as a fact so its eventual change is deliberate: this repo's
// CLLogin::read() is a stale mirror of the server's and stops one byte
// short - it never consumes the login-mode byte write() emits and the
// server reads. Harmless today (the client never reads a CLLogin), so
// the test pins the asymmetry rather than a round-trip; when read() is
// brought in step, flip this to RoundTrip() and CHECK the fields.
TEST(CLLogin, ClientSideReadStopsOneByteShortOfWrite)
{
	CLLogin src, dst;
	Fill(src);
	std::vector<unsigned char> body = WriteBody(src, 0);

	InFixture f;
	SocketInputStreamTestAccess::Preload(f.m_Stream, &body[0], (unsigned int)body.size());
	dst.read(f.m_Stream);
	CHECK(src.getID() == dst.getID());
	CHECK(src.getPassword() == dst.getPassword());
	CHECK(std::memcmp(src.getMacAddress(), dst.getMacAddress(), 6) == 0);
	CHECK_EQ(1, f.m_Stream.length());	// the login-mode byte
}

TEST(CLLogin, NetmarbleLayoutMatchesGolden)
{
	CLLogin packet;
	Fill(packet);
	packet.setNetmarble(true);
	const std::vector<unsigned char> body = WriteBody(packet, 0);
	CHECK_EQ(packet.getPacketSize(), body.size());
	ExpectGolden("CLLogin.netmarble", 0, body);
}

// CGConnect is the game-server handshake: key, PC type, name, then the
// six MAC bytes as a raw array. Both sides consume the same bytes, so
// it round-trips here, and the golden is the server's. Pinned ahead of
// the MAC array's move to a span.
TEST(CGConnect, RoundTripsAndMatchesTheSharedGolden)
{
	CGConnect src, dst;
	Fill(src);
	CHECK(EncrypterFree(src));
	RoundTrip(src, dst, 0);
	CHECK_EQ(src.getKey(), dst.getKey());
	CHECK_EQ((int)src.getPCType(), (int)dst.getPCType());
	CHECK(src.getPCName() == dst.getPCName());
	CHECK(std::memcmp(src.getMacAddress(), dst.getMacAddress(), 6) == 0);
	ExpectGolden("CGConnect", 0, WriteBody(src, 0));
}

//----------------------------------------------------------------------
// GCAddItemToItemVerify: a code byte, then 0, 1 or 2 uint parameters
// depending on the code. The server (the writing side, so the
// authority) sends ONE parameter - the new grade - for UP_GRADE_OK and
// two for THREE_ENCHANT_OK; this repo's read() and getPacketSize() took
// two for both until task 2.5's triage, so a successful item upgrade
// over-read into the next packet. The bytes below are what the server's
// write() puts on the wire for each shape; write() here agreed all
// along, only read()/getPacketSize() had drifted.
//----------------------------------------------------------------------
namespace {

std::vector<unsigned char>	VerifyBody(BYTE code, const uint* params, size_t count)
{
	std::vector<unsigned char> body;
	body.push_back(code);
	for (size_t i = 0; i < count; i++)
		Append(body, &params[i], sizeof(uint));
	return body;
}

// Parse `body` and require read() to consume exactly it, then hand the
// packet back for field checks.
void	ReadExactly(GCAddItemToItemVerify& dst, const std::vector<unsigned char>& body)
{
	InFixture f;
	SocketInputStreamTestAccess::Preload(f.m_Stream, &body[0], (unsigned int)body.size());
	dst.read(f.m_Stream);
	CHECK(f.m_Stream.isEmpty());
	CHECK_EQ(body.size(), dst.getPacketSize());
}

} // namespace

TEST(GCAddItemToItemVerify, UpgradeOKCarriesTheOneParameterTheServerSends)
{
	const uint grade = 7;
	const std::vector<unsigned char> body = VerifyBody(ADD_ITEM_TO_ITEM_VERIFY_UP_GRADE_OK, &grade, 1);

	GCAddItemToItemVerify dst;
	ReadExactly(dst, body);
	CHECK_EQ(ADD_ITEM_TO_ITEM_VERIFY_UP_GRADE_OK, dst.getCode());
	CHECK_EQ(grade, dst.getParameter());
	CHECK_EQ(0, dst.getParameter2());	// never on the wire for this code

	GCAddItemToItemVerify src;
	src.setCode(ADD_ITEM_TO_ITEM_VERIFY_UP_GRADE_OK);
	src.setParameter(grade);
	CHECK(EncrypterFree(src));
	CHECK(WriteBody(src, 0) == body);
	ExpectGolden("GCAddItemToItemVerify.upgrade", 0, body);
}

TEST(GCAddItemToItemVerify, ThreeEnchantOKCarriesTwoParameters)
{
	const uint params[2] = { 0x11223344, 0x55667788 };
	const std::vector<unsigned char> body = VerifyBody(ADD_ITEM_TO_ITEM_VERIFY_THREE_ENCHANT_OK, params, 2);

	GCAddItemToItemVerify dst;
	ReadExactly(dst, body);
	CHECK_EQ(ADD_ITEM_TO_ITEM_VERIFY_THREE_ENCHANT_OK, dst.getCode());
	CHECK_EQ(params[0], dst.getParameter());
	CHECK_EQ(params[1], dst.getParameter2());

	GCAddItemToItemVerify src;
	src.setCode(ADD_ITEM_TO_ITEM_VERIFY_THREE_ENCHANT_OK);
	src.setParameter(params[0]);
	src.setParameter2(params[1]);
	CHECK(WriteBody(src, 0) == body);
	ExpectGolden("GCAddItemToItemVerify.threeenchant", 0, body);
}

TEST(GCAddItemToItemVerify, EnchantFailCarriesNoParameter)
{
	const std::vector<unsigned char> body = VerifyBody(ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_FAIL, NULL, 0);

	GCAddItemToItemVerify dst;
	ReadExactly(dst, body);
	CHECK_EQ(ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_FAIL, dst.getCode());

	GCAddItemToItemVerify src;
	src.setCode(ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_FAIL);
	CHECK(WriteBody(src, 0) == body);
	ExpectGolden("GCAddItemToItemVerify.fail", 0, body);
}

// A fresh packet carries no parameter until read() supplies one: the
// handler reads getParameter2() for codes the server sends without it.
TEST(GCAddItemToItemVerify, FreshPacketHasZeroParameters)
{
	GCAddItemToItemVerify packet;
	CHECK_EQ(0, packet.getParameter());
	CHECK_EQ(0, packet.getParameter2());
}

//----------------------------------------------------------------------
// Bounds of the chat-family parsers: the checks that make a hostile
// length byte a disconnect instead of an overrun. Each hostile body is
// built from a good one so only the byte under test differs.
//----------------------------------------------------------------------
TEST(GCSay, RejectsAnEmptyAndAnOverlongMessage)
{
	GCSay good;
	Fill(good);
	std::vector<unsigned char> body = WriteBody(good, 0);
	const size_t lenAt = 8;	// ObjectID(4) + color(4), then the length byte
	CHECK_EQ(good.getMessage().size(), body[lenAt]);

	std::vector<unsigned char> empty(body.begin(), body.begin() + lenAt);
	empty.push_back(0);
	CHECK(Rejects<GCSay>(empty));

	std::vector<unsigned char> overlong(body.begin(), body.begin() + lenAt);
	overlong.push_back(129);	// the parser's cap is 128
	overlong.insert(overlong.end(), 129, 'x');
	CHECK(Rejects<GCSay>(overlong));

	// The boundary itself is accepted: 128 bytes parse.
	std::vector<unsigned char> atCap(body.begin(), body.begin() + lenAt);
	atCap.push_back(128);
	atCap.insert(atCap.end(), 128, 'x');
	CHECK(!Rejects<GCSay>(atCap));
}

TEST(GCGuildChat, RejectsAnOverlongSenderAndGuildName)
{
	// type 0, then szSender 11 - one past the parser's cap of 10.
	std::vector<unsigned char> longSender;
	longSender.push_back(0);
	longSender.push_back(11);
	longSender.insert(longSender.end(), 11, 's');
	CHECK(Rejects<GCGuildChat>(longSender));

	// type 1 carries a guild name first; 31 is one past its cap of 30.
	std::vector<unsigned char> longGuild;
	longGuild.push_back(1);
	longGuild.push_back(31);
	longGuild.insert(longGuild.end(), 31, 'g');
	CHECK(Rejects<GCGuildChat>(longGuild));

	// An empty sender is refused too (szSender == 0).
	std::vector<unsigned char> emptySender;
	emptySender.push_back(0);
	emptySender.push_back(0);
	CHECK(Rejects<GCGuildChat>(emptySender));
}

//----------------------------------------------------------------------
// The power-point results refuse a code past the last enumerator of the
// list it names, as the server's copies do. The last enumerator itself
// parses; one past it is refused. Each hostile body differs from the
// good one in the byte under test only.
//----------------------------------------------------------------------
TEST(GCUsePowerPointResult, RejectsAResultOrItemCodePastTheLastEnumerator)
{
	const BYTE lastResult = GCUsePowerPointResult::kLastResultCode;
	const BYTE lastItem = GCUsePowerPointResult::kLastItemCode;
	CHECK_EQ((int)GCUsePowerPointResult::NOT_ENOUGH_INVENTORY_SPACE, (int)lastResult);
	CHECK_EQ((int)GCUsePowerPointResult::BLACK_RICE_CAKE_SOUP, (int)lastItem);

	GCUsePowerPointResult good;
	good.setErrorCode(lastResult);
	good.setItemCode(lastItem);
	good.setPowerPoint(0xC0D1E2F3u);	// wider than a BYTE, so the setter keeps all four
	const std::vector<unsigned char> body = WriteBody(good, 0);
	CHECK_EQ((size_t)(szBYTE + szBYTE + szDWORD), body.size());	// code, item, points
	CHECK(!Rejects<GCUsePowerPointResult>(body));

	GCUsePowerPointResult parsed;
	ReadBody(parsed, body, 0);
	CHECK_EQ((int)lastResult, (int)parsed.getErrorCode());
	CHECK_EQ((int)lastItem, (int)parsed.getItemCode());
	CHECK_EQ(0xC0D1E2F3u, parsed.getPowerPoint());

	std::vector<unsigned char> badResult = body;
	badResult[0] = (unsigned char)(lastResult + 1);
	CHECK(Rejects<GCUsePowerPointResult>(badResult));
	badResult[0] = 0xFF;
	CHECK(Rejects<GCUsePowerPointResult>(badResult));

	std::vector<unsigned char> badItem = body;
	badItem[1] = (unsigned char)(lastItem + 1);
	CHECK(Rejects<GCUsePowerPointResult>(badItem));
	badItem[1] = 0xFF;
	CHECK(Rejects<GCUsePowerPointResult>(badItem));
}

TEST(GCRequestPowerPointResult, RejectsAResultCodePastTheLastEnumerator)
{
	const BYTE lastResult = GCRequestPowerPointResult::kLastResultCode;
	CHECK_EQ((int)GCRequestPowerPointResult::CONNECT_ERROR, (int)lastResult);

	GCRequestPowerPointResult good;
	good.setErrorCode(lastResult);
	good.setSumPowerPoint(1234);
	good.setRequestPowerPoint(56);
	const std::vector<unsigned char> body = WriteBody(good, 0);
	CHECK_EQ((size_t)(szBYTE + szint + szint), body.size());	// code, sum, request
	CHECK(!Rejects<GCRequestPowerPointResult>(body));

	GCRequestPowerPointResult parsed;
	ReadBody(parsed, body, 0);
	CHECK_EQ((int)lastResult, (int)parsed.getErrorCode());
	CHECK_EQ(1234, parsed.getSumPowerPoint());
	CHECK_EQ(56, parsed.getRequestPowerPoint());

	std::vector<unsigned char> bad = body;
	bad[0] = (unsigned char)(lastResult + 1);
	CHECK(Rejects<GCRequestPowerPointResult>(bad));
	bad[0] = 0xFF;
	CHECK(Rejects<GCRequestPowerPointResult>(bad));
}

// Type != 0 with an empty guild name: write() emits the zero length
// byte and no name, read() skips the name read on zero - the one
// asymmetric branch in the parser, pinned so it stays a round-trip.
TEST(GCGuildChat, EmptyGuildNameWithNonZeroTypeRoundTrips)
{
	GCGuildChat src, dst;
	Fill(src);
	src.setSendGuildName("");
	RoundTrip(src, dst, 0);
	CHECK_EQ(src.getType(), dst.getType());
	CHECK(dst.getSendGuildName().empty());
	CHECK(src.getSender() == dst.getSender());
	CHECK(src.getMessage() == dst.getMessage());
}

#include "Gpackets/GCSkillInfo.h"

TEST(GCSkillInfo, OustersLoginFrameIncludesSkillLevelsAndPreservesNextPacket)
{
    GCSkillInfo packet;
    packet.setPCType(PC_OUSTERS);
    auto* skills = new OustersSkillInfo();
    skills->setListNum(2);
    for (int i = 0; i < 2; ++i) {
        auto* skill = new SubOustersSkillInfo();
        skill->setSkillType(247 - i);
        skill->setExpLevel(1);
        skill->setSkillTurn(5);
        skill->setCastingTime(3);
        skills->addListElement(skill);
    }
    packet.addListElement(skills);
    const std::vector<unsigned char> expected = {
        0x69, 0x01, 28, 0, 0, 0, 0, // GCSkillInfo, body size, sequence
        2, 1, 0, 2,                // Ousters, one list, learn flag, two skills
        247, 0, 1, 0, 5, 0, 0, 0, 3, 0, 0, 0,
        246, 0, 1, 0, 5, 0, 0, 0, 3, 0, 0, 0
    };
    CHECK_EQ(28u, packet.getPacketSize());
    CHECK_EQ(12u, SubOustersSkillInfo::getMaxSize());
    CHECK(WriteFramed(packet, 0) == expected);
    auto joined = expected;
    joined.insert(joined.end(), expected.begin(), expected.end());
    InFixture input;
    input.m_Stream.setEncryptCode(0);
    SocketInputStreamTestAccess::Preload(input.m_Stream, joined.data(), static_cast<unsigned int>(joined.size()));
    for (int pass = 0; pass < 2; ++pass) {
        GCSkillInfo decoded;
        input.m_Stream.read(&decoded);
        CHECK_EQ(PC_OUSTERS, decoded.getPCType());
        CHECK_EQ(1, decoded.getListNum());
        CHECK_EQ(28u, decoded.getPacketSize());
        CHECK(WriteBody(decoded, 0) == std::vector<unsigned char>(expected.begin() + 7, expected.end()));
        CHECK_EQ(pass == 0 ? expected.size() : 0, input.m_Stream.length());
    }
}

//----------------------------------------------------------------------
// The datagram frame: Datagram::write(const DatagramPacket*) puts the
// packet id and the declared size in front of the body, at their
// szPacketID and szPacketSize widths, and sizes the datagram at
// szPacketHeader + body - one byte more than it writes, the slot the
// stream's sequence byte occupies. That byte is not written by this
// code, so the goldens pin the id, the size and the body and the test
// pins the length separately; the pad byte's value is the fix that
// follows this pin. CGPortCheck is what the client sends the login
// server; RCPositionInfo is what it sends and receives from a peer.
//----------------------------------------------------------------------
void	Fill(CGPortCheck& p)		{ p.setPCName("WirePin"); }
void	Fill(RCPositionInfo& p)
{
	p.setName("Nosferatu");
	p.setZoneID(0x8A9B);
	p.setZoneX(0xC5);
	p.setZoneY(0xD6);
}

// The bytes Datagram::write put down: the header it writes and the
// body, without the unwritten pad slot.
template <class PacketT>
std::vector<unsigned char>	DatagramWritten(const PacketT& packet, Datagram& datagram)
{
	datagram.write(&packet);
	const unsigned char* data = (const unsigned char*)datagram.getData();
	const size_t written = szPacketID + szPacketSize + packet.getPacketSize();
	CHECK_EQ(szPacketHeader + packet.getPacketSize(), datagram.getLength());
	return std::vector<unsigned char>(data, data + written);
}

TEST(Datagram, CGPortCheckFrameMatchesGolden)
{
	CGPortCheck packet;
	Fill(packet);
	Datagram datagram;
	ExpectGolden("CGPortCheck.datagram", 0, DatagramWritten(packet, datagram));
}

TEST(Datagram, RCPositionInfoFrameMatchesGolden)
{
	RCPositionInfo packet;
	Fill(packet);
	Datagram datagram;
	ExpectGolden("RCPositionInfo.datagram", 0, DatagramWritten(packet, datagram));
}
