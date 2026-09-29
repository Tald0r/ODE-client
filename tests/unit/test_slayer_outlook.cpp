//----------------------------------------------------------------------
// test_slayer_outlook.cpp
//----------------------------------------------------------------------
//
// The slayer outlook DWORD of PCSlayerInfo, the record LCPCList carries
// to the character-select screen. Its weapon is two fields. Bits 11-14
// hold the four-bit view: the weapon itself below 16, and above it the
// stand-in slayerWeaponListShape gives it (a cross for cross1, no weapon
// for mace and mace1). Bits 17-18, past the two shield bits at 15-16,
// hold the extension code: 0, or cross1 1, mace 2, mace1 3. A client
// that knows only the four weapon bits keeps bits 0-16 of the DWORD, so
// it reads the four-bit view and never the code; a client that knows
// the code trusts it over the four bits. No weapon value reaches the
// shield.
//
// These are the server repo's PCSlayerInfoOutlook tests
// (tests/packet_login_test.cpp, the server's commit c3b563a8), with
// the same literal tables, so the two repos pin the same DWORDs. Where
// the client's enums run further than the server's (HELMET5, JACKET6,
// SHIELD4) the loops take the client's ranges: every shield the
// two-bit field holds (0..3, where the server's SHIELD_MAX stops at 3),
// and every value each field's enum type can hold.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "PCSlayerInfo.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"

#include <bit>
#include <bitset>
#include <cstdio>
#include <vector>

namespace {

// CHECK_EQ, and on a mismatch the row that failed: the framework's own
// report names only the line, and every check here sits in a loop.
#define CHECK_ROW(expected, actual, what, first, second)			\
	do {									\
		const long long rowExpected = (long long)(expected);		\
		const long long rowActual = (long long)(actual);		\
		CHECK_EQ(rowExpected, rowActual);				\
		if (rowExpected != rowActual)					\
			std::fprintf(stderr, "    %s == %s at %s %lld, %lld\n",	\
				#expected, #actual, what,			\
				(long long)(first), (long long)(second));	\
	} while (0)

// Every value the character list's two-bit shield field holds. The
// client's ShieldType runs to SHIELD4 (the zone view's three-bit
// field); SHIELD4 does not fit here, and the setter's cut is pinned by
// EveryValueAFieldsTypeHoldsStaysInTheField.
const int	kListShields = 4;

// Bits 11-14 of each weapon's outlook with no other field set: for
// 0..15 the weapon in the four-bit layout, and for 16..18 the clamped
// stand-in. These are the DWORDs a client that knows only four weapon
// bits has always been sent, and must go on reading.
const DWORD	kFourBitWeaponDWORDs[WEAPON_MAX] =
{
	0x00000000, 0x00000800, 0x00001000, 0x00001800, 0x00002000, 0x00002800, 0x00003000, 0x00003800,
	0x00004000, 0x00004800, 0x00005000, 0x00005800, 0x00006000, 0x00006800, 0x00007000, 0x00007800,
	0x00007800, // cross1: a cross
	0x00000000, // mace: no weapon
	0x00000000, // mace1: no weapon
};
// Bits 17-18 of each weapon's outlook: no code below 16, then cross1 1,
// mace 2, mace1 3.
const DWORD	kExtensionDWORDs[WEAPON_MAX] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x00020000, // cross1: 1
	0x00040000, // mace: 2
	0x00060000, // mace1: 3
};
const DWORD	kShieldDWORDs[kListShields] = { 0x00000000, 0x00008000, 0x00010000, 0x00018000 };

// A named slayer record with every field zero, so the outlook holds
// nothing but what a test sets. Value-initialised: PCSlayerInfo has no
// constructor, and write() loads the slot as an enum.
PCSlayerInfo	NamedSlayer()
{
	PCSlayerInfo info{};
	info.setName("GoldSlayer");
	info.setSlot(SLOT1);
	return info;
}

// The record exactly as write() puts it on the wire.
std::vector<unsigned char>	WriteRecord(const PCSlayerInfo& info)
{
	Socket				socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketEncryptOutputStream	out(&socket);
	out.setEncryptCode(0);
	info.write(out);
	return SocketOutputStreamTestAccess::Bytes(out);
}

// The outlook as the client receives it: the DWORD the record writes
// just before its colors and its advancement level.
DWORD	OutlookOnTheWire(const PCSlayerInfo& info)
{
	const std::vector<unsigned char> body = WriteRecord(info);
	const size_t tail = szColor * PCSlayerInfo::SLAYER_COLOR_MAX + szLevel;
	CHECK(body.size() >= szDWORD + tail);
	if (body.size() < szDWORD + tail)
		return 0xFFFFFFFF;
	const size_t at = body.size() - tail - szDWORD;
	return (DWORD)body[at] | ((DWORD)body[at + 1] << 8) | ((DWORD)body[at + 2] << 16) | ((DWORD)body[at + 3] << 24);
}

// write() the record, and read() the bytes back into a fresh one.
PCSlayerInfo	ThroughTheWire(const PCSlayerInfo& info)
{
	const std::vector<unsigned char> body = WriteRecord(info);
	Socket				socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketEncryptInputStream	in(&socket, 4096);
	in.setEncryptCode(0);
	SocketInputStreamTestAccess::Preload(in, &body[0], (unsigned int)body.size());
	PCSlayerInfo parsed{};
	parsed.read(in);
	CHECK(in.isEmpty());
	return parsed;
}

// The outlook DWORD read into a record, as read() and setShapeInfo()
// both do.
PCSlayerInfo	FromOutlook(DWORD outlook)
{
	PCSlayerInfo info = NamedSlayer();
	Color_t colors[PCSlayerInfo::SLAYER_COLOR_MAX] = {};
	info.setShapeInfo(outlook, colors);
	return info;
}

// What a client that knows only four weapon bits makes of an outlook:
// its read() keeps bits 0-16 in a bitset<17>, and its getters mask four
// weapon bits at 11 and two shield bits at 15.
struct FourBitReading
{
	DWORD	outlook;
	int	weapon;
	int	shield;
};
FourBitReading	ReadAsAFourBitClient(DWORD wire)
{
	const DWORD outlook = (DWORD)std::bitset<17>(wire).to_ulong();
	return { outlook, (int)((outlook >> 11) & 15), (int)((outlook >> 15) & 3) };
}

// The weapon a four-bit reading shows: below 16 the weapon itself, a
// cross for cross1, and no weapon for mace and mace1.
int	ClampedWeapon(int weapon)
{
	if (weapon < 16)
		return weapon;
	return weapon == WEAPON_CROSS1 ? (int)WEAPON_CROSS : (int)WEAPON_NONE;
}

} // namespace

//----------------------------------------------------------------------
// Every weapon with every shield, set in either order, reads back as
// itself, in the record and after the wire.
//----------------------------------------------------------------------
TEST(PCSlayerInfoOutlook, EveryWeaponAndEveryShieldReadBackInEitherOrder)
{
	for (int weapon = WEAPON_NONE; weapon < WEAPON_MAX; weapon++)
	{
		for (int shield = SHIELD_NONE; shield < kListShields; shield++)
		{
			PCSlayerInfo weaponFirst = NamedSlayer();
			weaponFirst.setWeaponType((WeaponType)weapon);
			weaponFirst.setShieldType((ShieldType)shield);
			PCSlayerInfo shieldFirst = NamedSlayer();
			shieldFirst.setShieldType((ShieldType)shield);
			shieldFirst.setWeaponType((WeaponType)weapon);
			for (const PCSlayerInfo* pInfo : { &weaponFirst, &shieldFirst })
			{
				CHECK_ROW(weapon, pInfo->getWeaponType(), "weapon, shield", weapon, shield);
				CHECK_ROW(shield, pInfo->getShieldType(), "weapon, shield", weapon, shield);
			}

			const PCSlayerInfo parsed = ThroughTheWire(shieldFirst);
			CHECK_ROW(weapon, parsed.getWeaponType(), "on the wire: weapon, shield", weapon, shield);
			CHECK_ROW(shield, parsed.getShieldType(), "on the wire: weapon, shield", weapon, shield);
		}
	}
}

//----------------------------------------------------------------------
// A weapon written over another leaves no bit of the old one, and the
// shield stays.
//----------------------------------------------------------------------
TEST(PCSlayerInfoOutlook, ReplacingAWeaponLeavesNoBitOfTheOldOne)
{
	for (int from = WEAPON_NONE; from < WEAPON_MAX; from++)
	{
		for (int to = WEAPON_NONE; to < WEAPON_MAX; to++)
		{
			PCSlayerInfo info = NamedSlayer();
			info.setShieldType(SHIELD2);
			info.setWeaponType((WeaponType)from);
			info.setWeaponType((WeaponType)to);
			CHECK_ROW(to, info.getWeaponType(), "from, to", from, to);
			CHECK_ROW(SHIELD2, info.getShieldType(), "from, to", from, to);
			CHECK_ROW(kFourBitWeaponDWORDs[to] | kExtensionDWORDs[to] | kShieldDWORDs[SHIELD2],
				OutlookOnTheWire(info), "from, to", from, to);
		}
	}
}

//----------------------------------------------------------------------
// The whole DWORD of every weapon with every shield: the four-bit view
// at 11-14, the extension code at 17-18, and nothing else. Weapons
// below 16 carry no code, so their DWORD is the four-bit layout's
// exactly.
//----------------------------------------------------------------------
TEST(PCSlayerInfoOutlook, EveryWeaponWritesItsFourBitViewAndItsExtensionCode)
{
	for (int weapon = WEAPON_NONE; weapon < WEAPON_MAX; weapon++)
	{
		for (int shield = SHIELD_NONE; shield < kListShields; shield++)
		{
			PCSlayerInfo info = NamedSlayer();
			info.setWeaponType((WeaponType)weapon);
			info.setShieldType((ShieldType)shield);
			CHECK_ROW(kFourBitWeaponDWORDs[weapon] | kExtensionDWORDs[weapon] | kShieldDWORDs[shield],
				OutlookOnTheWire(info), "weapon, shield", weapon, shield);
		}
	}
}

//----------------------------------------------------------------------
// Old client, new server: a client that knows only four weapon bits
// reads bits 0-16, and they are the four-bit table's DWORD exactly,
// weapon and shield alike: below 16 the weapon, from 16 the clamped
// stand-in.
//----------------------------------------------------------------------
TEST(PCSlayerInfoOutlook, AFourBitClientReadsTheFourBitView)
{
	for (int weapon = WEAPON_NONE; weapon < WEAPON_MAX; weapon++)
	{
		for (int shield = SHIELD_NONE; shield < kListShields; shield++)
		{
			PCSlayerInfo info = NamedSlayer();
			info.setWeaponType((WeaponType)weapon);
			info.setShieldType((ShieldType)shield);
			const FourBitReading reading = ReadAsAFourBitClient(OutlookOnTheWire(info));
			CHECK_ROW(kFourBitWeaponDWORDs[weapon] | kShieldDWORDs[shield], reading.outlook,
				"weapon, shield", weapon, shield);
			CHECK_ROW(ClampedWeapon(weapon), reading.weapon, "weapon, shield", weapon, shield);
			CHECK_ROW(shield, reading.shield, "weapon, shield", weapon, shield);
		}
	}
	CHECK_EQ(WEAPON_CROSS, ReadAsAFourBitClient(0x00007800 | 0x00020000).weapon);
	CHECK_EQ(WEAPON_NONE, ReadAsAFourBitClient(0x00040000).weapon);
	CHECK_EQ(WEAPON_NONE, ReadAsAFourBitClient(0x00060000).weapon);
}

//----------------------------------------------------------------------
// New client, old server, or a Slayer.Shape row written without the
// code: bits 17-18 are clear, so the outlook reads back as its four
// bits say: weapons 0..15 as themselves, and the clamped DWORDs of
// cross1, mace and mace1 as a cross, no weapon and no weapon.
//----------------------------------------------------------------------
TEST(PCSlayerInfoOutlook, AnOutlookWithoutTheCodeReadsAsItsFourBits)
{
	for (int weapon = WEAPON_NONE; weapon < WEAPON_MAX; weapon++)
	{
		for (int shield = SHIELD_NONE; shield < kListShields; shield++)
		{
			const PCSlayerInfo stored = FromOutlook(kFourBitWeaponDWORDs[weapon] | kShieldDWORDs[shield]);
			CHECK_ROW(ClampedWeapon(weapon), stored.getWeaponType(), "weapon, shield", weapon, shield);
			CHECK_ROW(shield, stored.getShieldType(), "weapon, shield", weapon, shield);
		}
	}
}

//----------------------------------------------------------------------
// Every combination of the two weapon fields decodes to a weapon a
// table of WEAPON_MAX entries can hold: the code when it is set,
// whatever the four bits say, and the four bits otherwise. Each is
// tried under every shield, alone and with every other bit of the
// DWORD set. GameUI.cpp indexes its 19-entry weapon tables with the
// result unchecked.
//----------------------------------------------------------------------
TEST(PCSlayerInfoOutlook, EveryOutlookDecodesToAWeaponBelowTheMax)
{
	const DWORD kWeaponFields = 0x00007800 | 0x00060000;
	const DWORD kShieldField = 0x00018000;
	for (DWORD low = 0; low < 16; low++)
	{
		for (DWORD extension = 0; extension < 4; extension++)
		{
			const int expected = extension != 0 ? (int)(WEAPON_CROSS + extension) : (int)low;
			for (int shield = SHIELD_NONE; shield < kListShields; shield++)
			{
				for (DWORD others : { (DWORD)0, (DWORD)~(kWeaponFields | kShieldField) })
				{
					const DWORD outlook = (low << 11) | (extension << 17) | kShieldDWORDs[shield] | others;
					const PCSlayerInfo info = FromOutlook(outlook);
					CHECK_ROW(expected, info.getWeaponType(), "outlook, shield", outlook, shield);
					CHECK_ROW(true, (int)info.getWeaponType() < (int)WEAPON_MAX, "outlook, shield", outlook, shield);
					CHECK_ROW(shield, info.getShieldType(), "outlook, shield", outlook, shield);
				}
			}
		}
	}
}

//----------------------------------------------------------------------
// A value past WEAPON_MACE1 is no weapon: both weapon fields cleared,
// nothing spilled into the shield or past the code, whatever was held.
// WeaponType's largest enumerator is WEAPON_MAX (19), so the type holds
// 0..31 and every value here is one of its values.
//----------------------------------------------------------------------
TEST(PCSlayerInfoOutlook, AValuePastMaceOneWritesNoWeapon)
{
	for (int past = WEAPON_MAX; past < 32; past++)
	{
		for (int from = WEAPON_NONE; from < WEAPON_MAX; from++)
		{
			for (int shield = SHIELD_NONE; shield < kListShields; shield++)
			{
				PCSlayerInfo info = NamedSlayer();
				info.setShieldType((ShieldType)shield);
				info.setWeaponType((WeaponType)from);
				info.setWeaponType((WeaponType)past);
				const int row = past * 100 + from;
				CHECK_ROW(WEAPON_NONE, info.getWeaponType(), "past*100+from, shield", row, shield);
				CHECK_ROW(shield, info.getShieldType(), "past*100+from, shield", row, shield);
				CHECK_ROW(kShieldDWORDs[shield], OutlookOnTheWire(info), "past*100+from, shield", row, shield);
			}
		}
	}
}

//----------------------------------------------------------------------
// weaponBits, which setWeaponType writes through, against the tables
// and against the DWORD the setter puts on the wire; past the enum, any
// value writes nothing.
//----------------------------------------------------------------------
TEST(PCSlayerInfoOutlook, WeaponBitsMatchTheTablesAndTheSetter)
{
	for (DWORD weapon = WEAPON_NONE; weapon < (DWORD)WEAPON_MAX; weapon++)
	{
		CHECK_ROW(kFourBitWeaponDWORDs[weapon] | kExtensionDWORDs[weapon], PCSlayerInfo::weaponBits(weapon),
			"weapon", weapon, 0);
		PCSlayerInfo info = NamedSlayer();
		info.setWeaponType((WeaponType)weapon);
		CHECK_ROW(PCSlayerInfo::weaponBits(weapon), OutlookOnTheWire(info), "weapon", weapon, 0);
	}
	for (DWORD past : { (DWORD)WEAPON_MAX, (DWORD)31, (DWORD)32, (DWORD)255, (DWORD)0x10011, (DWORD)0xFFFFFFFF })
		CHECK_ROW(0, PCSlayerInfo::weaponBits(past), "past", past, 0);
	CHECK_EQ(0x00007800u | 0x00060000u, PCSlayerInfo::kWeaponBitsMask);
}

//----------------------------------------------------------------------
// The four-bit view: every weapon's stand-in fits the four bits, and
// only cross1, mace and mace1 are replaced.
//----------------------------------------------------------------------
TEST(SlayerWeaponShape, CharacterListFitsFourBits)
{
	for (int shape = WEAPON_NONE; shape < WEAPON_MAX; shape++)
		CHECK_ROW(true, slayerWeaponListShape((WeaponType)shape) < 16, "shape", shape, 0);
	CHECK_EQ(WEAPON_SR3, slayerWeaponListShape(WEAPON_SR3));
	CHECK_EQ(WEAPON_CROSS, slayerWeaponListShape(WEAPON_CROSS));
	CHECK_EQ(WEAPON_CROSS, slayerWeaponListShape(WEAPON_CROSS1));
	CHECK_EQ(WEAPON_NONE, slayerWeaponListShape(WEAPON_MACE));
	CHECK_EQ(WEAPON_NONE, slayerWeaponListShape(WEAPON_MACE1));
}

//----------------------------------------------------------------------
// The outlook's other multi-bit fields
//----------------------------------------------------------------------
namespace {

// How many values an enum with no fixed underlying type and no negative
// enumerator can hold: those of the narrowest bit-field that fits its
// largest enumerator. A value past them is not a value of the type, and
// Clang's UBSan (the macos-asan preset) stops on its load before any
// setter could cut it.
constexpr DWORD	EnumValueCount(DWORD largestEnumerator)
{
	return std::bit_ceil(largestEnumerator + 1);
}

// Each field as its setter writes it: the first bit and the mask,
// spelled as literals so the layout is pinned too, and how many values
// the field's enum type can hold. The client's types run past two of
// their fields: HelmetType to HELMET_MAX (6), so it holds 0..7 in a
// two-bit field, and ShieldType to SHIELD_MAX (5), so it holds 0..7 in
// a two-bit field too. Their extra values (HELMET4, HELMET5, SHIELD3,
// SHIELD4) come from the zone view, PCSlayerInfo3, whose helmet and
// shield fields are three bits wide.
struct OutlookField
{
	const char*	name;
	int		first;
	DWORD		mask;
	DWORD		values;
	void		(*set)(PCSlayerInfo&, DWORD);
	DWORD		(*get)(const PCSlayerInfo&);
};

const OutlookField	kOutlookFields[] =
{
	{ "hair style", 1, 3, EnumValueCount(HAIR_STYLE3),
	  [](PCSlayerInfo& info, DWORD value) { info.setHairStyle((HairStyle)value); },
	  [](const PCSlayerInfo& info) { return (DWORD)info.getHairStyle(); } },
	{ "helmet", 3, 3, EnumValueCount(HELMET_MAX),
	  [](PCSlayerInfo& info, DWORD value) { info.setHelmetType((HelmetType)value); },
	  [](const PCSlayerInfo& info) { return (DWORD)info.getHelmetType(); } },
	{ "jacket", 5, 7, EnumValueCount(JACKET_MAX),
	  [](PCSlayerInfo& info, DWORD value) { info.setJacketType((JacketType)value); },
	  [](const PCSlayerInfo& info) { return (DWORD)info.getJacketType(); } },
	{ "pants", 8, 7, EnumValueCount(PANTS_MAX),
	  [](PCSlayerInfo& info, DWORD value) { info.setPantsType((PantsType)value); },
	  [](const PCSlayerInfo& info) { return (DWORD)info.getPantsType(); } },
	{ "shield", 15, 3, EnumValueCount(SHIELD_MAX),
	  [](PCSlayerInfo& info, DWORD value) { info.setShieldType((ShieldType)value); },
	  [](const PCSlayerInfo& info) { return (DWORD)info.getShieldType(); } },
};

} // namespace

//----------------------------------------------------------------------
// Every value a field's type can hold is written into that field alone:
// a value wider than the field keeps only the field's low bits, and no
// bit reaches a neighbour. Unmasked, a helmet of 4..7 lands on the
// jacket and a shield of 4..7 on the weapon extension code. Every
// weapon is held while the field is written, so the whole DWORD must be
// that weapon's, with the field's bits and nothing else.
//----------------------------------------------------------------------
TEST(PCSlayerInfoOutlook, EveryValueAFieldsTypeHoldsStaysInTheField)
{
	CHECK_EQ(4, EnumValueCount(HAIR_STYLE3));
	CHECK_EQ(8, EnumValueCount(HELMET_MAX));
	CHECK_EQ(8, EnumValueCount(SHIELD_MAX));
	for (const OutlookField& field : kOutlookFields)
	{
		for (DWORD value = 0; value < field.values; value++)
		{
			for (int weapon = WEAPON_NONE; weapon < WEAPON_MAX; weapon++)
			{
				PCSlayerInfo info = NamedSlayer();
				info.setWeaponType((WeaponType)weapon);
				field.set(info, field.mask);
				field.set(info, value);
				CHECK_ROW(weapon, info.getWeaponType(), field.name, value, weapon);
				CHECK_ROW(value & field.mask, field.get(info), field.name, value, weapon);
				CHECK_ROW(kFourBitWeaponDWORDs[weapon] | kExtensionDWORDs[weapon] | ((value & field.mask) << field.first),
					OutlookOnTheWire(info), field.name, value, weapon);
			}
		}
	}
}
