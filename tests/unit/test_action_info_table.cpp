//----------------------------------------------------------------------
// test_action_info_table.cpp
//----------------------------------------------------------------------
//
// The action table (Action.inf) through its real loader and writer,
// now that MActionInfoTable is a gamemodel member (docs/RESTRUCTURING.md
// task 4.1). Every skill and action the client shows is a row: its
// casting frames, its target and packet kind, its effect status and the
// chain of effect nodes the effect generator walks.
//
// The file layout, which the tests build byte by byte:
//
//   table   [minResult:4][maxResult:4][count:4] then count rows
//   row     [name: MString][action:1][effectSprite:2][femaleSprite:2]
//           [useRepeatFrame:1] 3 x ([start:4][castingStart:4]
//           [castingFrames:4][repeatStart:4][repeatEnd:4])
//           [repeatLimit:2][castingEffectToSelf:1][castingActionInfo:4]
//           [castingAction:1][range:1][target:1][start:1][user:1]
//           [weapon:2][currentWeapon:1][option:1][plusActionInfo:4]
//           [packetType:1][delay:2][value:4][sound:2][mainNode:4]
//           [resultID:2][resultValue:4][effectStatus:2][attack:1]
//           [selectCreature:1][flags:1] (5 x [actionStep:2] when flags
//           has 0x2) [parent:2][masteryStep:1][ignoreFailDelay:1]
//           then the node table: [count:4] then count nodes
//   node    [generator:2][effectSprite:2][step:2][count:2][linkCount:2]
//           [sound:2][delayNode:1][resultTime:1]
//
// The loader validates two things itself, both through CTypeTable: a
// row or node count larger than what is left of the file is refused
// before anything is allocated, and the row loop stops at the first
// failed read. The flag byte decides whether the five action steps are
// on disk at all. Those are what these tests pin.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MActionInfoTable.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

const char* const	kTempFile = "action_info_table_test.bin";

//----------------------------------------------------------------------
// A little-endian byte builder for the file layout above.
//----------------------------------------------------------------------
struct Bytes
{
	std::vector<unsigned char>	data;

	Bytes&	Int(int v)			{ return Raw(&v, sizeof(v)); }
	Bytes&	Byte(unsigned char v)		{ data.push_back(v); return *this; }
	Bytes&	Word(unsigned short v)		{ return Raw(&v, sizeof(v)); }
	Bytes&	Raw(const void* p, size_t n)
	{
		const unsigned char* c = static_cast<const unsigned char*>(p);
		data.insert(data.end(), c, c + n);
		return *this;
	}
	// MString's on-disk form: a 4-byte length, then the characters.
	Bytes&	Str(const char* s)
	{
		Int((int)std::strlen(s));
		return Raw(s, std::strlen(s));
	}
};

void	WriteScratch(const Bytes& b)
{
	std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
	if (!b.data.empty())
		out.write((const char*)&b.data[0], (std::streamsize)b.data.size());
}

void	RemoveScratch()
{
	std::remove(kTempFile);
}

bool	StrEq(const char* actual, const char* expected)
{
	return actual != NULL && std::strcmp(actual, expected) == 0;
}

//----------------------------------------------------------------------
// One row with every field set to a value the constructor does not
// choose, so a field the loader skipped cannot pass by default. The
// action-step block is present only when useActionStep is set, as the
// flag byte says.
//----------------------------------------------------------------------
struct RowSpec
{
	bool	useActionStep;
	BYTE	packetType;
	WORD	effectStatus;
	int	nodeCount;
};

Bytes&	AppendRowHead(Bytes& b, const RowSpec& spec)
{
	b.Str("Bloody Nail");
	b.Byte(7);				// action
	b.Word(120).Word(121);			// effect sprite, female
	b.Byte(1);				// use repeat frame
	for (int i = 0; i < 3; i++)
	{
		b.Int(10 + i).Int(20 + i).Int(30 + i);	// start, casting start, casting frames
		b.Int(40 + i).Int(50 + i);		// repeat start, repeat end
	}
	b.Word(9);				// repeat limit
	b.Byte(0);				// casting effect to self
	b.Int(301);				// casting action info
	b.Byte(1);				// casting action
	b.Byte(5);				// range
	b.Byte(FLAG_ACTIONINFO_TARGET_OTHER);
	b.Byte(FLAG_ACTIONINFO_START_SKY);
	b.Byte(FLAG_ACTIONINFO_USER_VAMPIRE);
	b.Word(FLAG_ACTIONINFO_WEAPON_SWORD | FLAG_ACTIONINFO_WEAPON_BLADE);
	b.Byte(FLAG_ACTIONINFO_CURRENT_WEAPON_DELAY);
	b.Byte(FLAG_ACTIONINFO_OPTION_USE_WITH_BLESS);
	b.Int(3);				// plus action info
	b.Byte(spec.packetType);
	b.Word(900);				// delay
	b.Int(-4);				// value
	b.Word(66);				// sound
	b.Int(1);				// main node
	b.Word(12).Int(77);			// result id, result value
	b.Word(spec.effectStatus);
	b.Byte(0);				// attack
	b.Byte(FLAG_ACTIONINFO_SELECT_PARTY);
	b.Byte(spec.useActionStep ? 0x7 : 0x5);	// grade, (step), attach self
	if (spec.useActionStep)
		for (int i = 0; i < MAX_ACTION_STEP; i++)
			b.Word((unsigned short)(200 + i));
	b.Word(250);				// parent
	b.Byte(2);				// mastery step
	b.Byte(1);				// ignore fail delay
	return b.Int(spec.nodeCount);
}

Bytes&	AppendNode(Bytes& b, unsigned short generator, bool delayNode, bool resultTime)
{
	b.Word(generator).Word((unsigned short)(generator + 1)).Word(3).Word(16).Word(8).Word(70);
	b.Byte(delayNode ? 1 : 0);
	return b.Byte(resultTime ? 1 : 0);
}

Bytes	TableHead(int count)
{
	Bytes b;
	return b.Int(100).Int(150).Int(count);
}

void	CheckRow(const MActionInfo& info, bool useActionStep)
{
	CHECK(StrEq(info.GetName(), "Bloody Nail"));
	CHECK_EQ(7, (int)info.GetAction());
	CHECK_EQ(120, (int)info.GetActionEffectSpriteType());
	CHECK_EQ(121, (int)info.GetActionEffectSpriteTypeFemale());
	CHECK(info.IsUseRepeatFrame() != 0);
	for (int i = 0; i < 3; i++)
	{
		CHECK_EQ(10 + i, info.GetStartFrame(i));
		CHECK_EQ(20 + i, info.GetCastingStartFrame(i));
		CHECK_EQ(30 + i, info.GetCastingFrames(i));
		CHECK_EQ(40 + i, info.GetRepeatStartFrame(i));
		CHECK_EQ(50 + i, info.GetRepeatEndFrame(i));
	}
	CHECK_EQ(9, (int)info.GetRepeatLimit());
	CHECK(!info.IsCastingEffectToSelf());
	CHECK_EQ(301, (int)info.GetCastingActionInfo());
	CHECK(info.IsCastingAction());
	CHECK_EQ(5, (int)info.GetRange());
	CHECK_EQ(FLAG_ACTIONINFO_TARGET_OTHER, (int)info.GetTarget());
	CHECK(info.IsStartSky() != 0);
	CHECK_EQ(FLAG_ACTIONINFO_USER_VAMPIRE, (int)info.GetUser());
	CHECK_EQ(FLAG_ACTIONINFO_WEAPON_SWORD | FLAG_ACTIONINFO_WEAPON_BLADE, (int)info.GetWeaponType());
	CHECK(info.IsAffectCurrentWeaponDelay() != 0);
	CHECK(info.IsOptionUseWithBless() != 0);
	CHECK_EQ(3, info.GetAffectCurrentWeaponActionInfoPlus());
	CHECK_EQ(900, (int)info.GetDelay());
	CHECK_EQ(-4, info.GetValue());
	CHECK_EQ(66, (int)info.GetSoundID());
	CHECK_EQ(1, info.GetMainNode());
	CHECK_EQ(12, (int)info.GetActionResultID());
	CHECK_EQ(77, info.GetActionResultValue());
	CHECK(!info.IsAttack());
	CHECK_EQ(FLAG_ACTIONINFO_SELECT_PARTY, (int)info.GetActionTarget());
	CHECK(info.IsUseActionGrade());
	CHECK(info.IsAttachSelf());
	CHECK_EQ(useActionStep, info.IsUseActionStep());
	CHECK_EQ(useActionStep ? 202 : (int)ACTIONINFO_NULL, (int)info.GetActionStep(2));
	CHECK_EQ(250, (int)info.GetParentActionInfo());
	CHECK_EQ(2, (int)info.GetMasterySkillStep());
	CHECK(info.IsIgnoreSkillFailDelay());
}

} // namespace

//----------------------------------------------------------------------
// A file built to the layout above loads field for field, including the
// optional action-step block and the node table.
//----------------------------------------------------------------------
TEST(ActionInfoTable, LoadReadsEveryFieldOfTheLayout)
{
	Bytes b = TableHead(2);
	AppendRowHead(b, RowSpec{ true, ACTIONINFO_PACKET_OTHER, EFFECTSTATUS_MAX - 1, 2 });
	AppendNode(b, 11, true, false);
	AppendNode(b, 13, false, true);
	AppendRowHead(b, RowSpec{ false, ACTIONINFO_PACKET_ZONE, EFFECTSTATUS_NULL, 0 });
	WriteScratch(b);

	MActionInfoTable table;
	bool good = false;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		table.LoadFromFile(in);
		good = in.good();
		// Every byte was consumed, and not one more.
		CHECK_EQ(std::char_traits<char>::eof(), in.peek());
	}
	RemoveScratch();

	CHECK(good);
	CHECK_EQ(100, (int)table.GetMinResultActionInfo());
	CHECK_EQ(150, (int)table.GetMaxResultActionInfo());
	CHECK_EQ(2, table.GetSize());

	const MActionInfo& first = table[0];
	CheckRow(first, true);
	CHECK_EQ((int)ACTIONINFO_PACKET_OTHER, (int)first.GetPacketType());
	CHECK_EQ((int)EFFECTSTATUS_MAX - 1, (int)first.GetEffectStatus());
	CHECK_EQ(2, first.GetSize());
	CHECK_EQ(11, (int)first[0].EffectGeneratorID);
	CHECK_EQ(12, (int)first[0].EffectSpriteType);
	CHECK_EQ(3, (int)first[0].Step);
	CHECK_EQ(16, (int)first[0].Count);
	CHECK_EQ(8, (int)first[0].LinkCount);
	CHECK_EQ(70, (int)first[0].SoundID);
	CHECK(first[0].bDelayNode);
	CHECK(!first[0].bResultTime);
	CHECK(!first[1].bDelayNode);
	CHECK(first[1].bResultTime);

	const MActionInfo& second = table[1];
	CheckRow(second, false);
	CHECK_EQ((int)ACTIONINFO_PACKET_ZONE, (int)second.GetPacketType());
	CHECK_EQ((int)EFFECTSTATUS_NULL, (int)second.GetEffectStatus());
	CHECK_EQ(0, second.GetSize());
}

//----------------------------------------------------------------------
// What the writer writes, the loader reads back.
//----------------------------------------------------------------------
TEST(ActionInfoTable, SaveThenLoadRoundTrips)
{
	{
		MActionInfoTable source;
		source.SetMinResultActionInfo(100);
		source.SetMaxResultActionInfo(150);
		source.Init(1);
		MActionInfo* info = source.GetMutable(0);
		CHECK(info != nullptr);
		if (info == nullptr)
			return;
		info->Set("Bloody Nail", 7, 120, 5, FLAG_ACTIONINFO_TARGET_OTHER,
			FLAG_ACTIONINFO_START_SKY, 900, -4);
		info->SetActionEffectSpriteTypeFemale(121);
		info->UseRepeatFrame();
		for (int i = 0; i < 3; i++)
		{
			info->SetStartFrame(i, 10 + i);
			info->SetCastingStartFrame(i, 20 + i);
			info->SetCastingFrames(i, 30 + i);
			info->SetRepeatFrame(i, 40 + i, 11);	// end = start + frames - 1 = 50 + i
		}
		info->SetRepeatLimit(9);
		info->SetCastingEffectToOther();
		info->SetCastingActionInfo(301);
		info->SetCastingAction();
		info->SetUser(FLAG_ACTIONINFO_USER_VAMPIRE);
		info->SetWeaponType(FLAG_ACTIONINFO_WEAPON_SWORD | FLAG_ACTIONINFO_WEAPON_BLADE);
		info->SetAffectCurrentWeaponDelay();
		info->SetOptionUseWithBless();
		info->SetAffectCurrentWeaponActionInfoPlus(3);
		info->SetPacketType(ACTIONINFO_PACKET_OTHER);
		info->SetSoundID(66);
		info->SetMainNode(1);
		info->SetActionResult(12, 77);
		info->SetEffectStatus(EFFECTSTATUS_NULL);
		info->UnSetAttack();
		info->SetSelectCreature(FLAG_ACTIONINFO_SELECT_PARTY);
		info->SetUseActionGrade();
		info->SetUseActionStep();
		for (int i = 0; i < MAX_ACTION_STEP; i++)
			info->SetActionStep((BYTE)i, (TYPE_ACTIONINFO)(200 + i));
		info->SetAttachSelf();
		info->SetParentActionInfo(250);
		info->SetMasterySkillStep(2);
		info->SetSkillFailDelay();

		info->Init(1);
		ACTION_INFO_NODE* node = info->GetMutable(0);
		CHECK(node != nullptr);
		if (node == nullptr)
			return;
		node->EffectGeneratorID = 11;
		node->EffectSpriteType = 12;
		node->Step = 3;
		node->Count = 16;
		node->LinkCount = 8;
		node->SoundID = 70;
		node->SetDelayNode();
		node->SetResultTime();

		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		source.SaveToFile(out);
		CHECK(out.good());
	}

	MActionInfoTable loaded;
	bool good = false;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		loaded.LoadFromFile(in);
		good = in.good();
		CHECK_EQ(std::char_traits<char>::eof(), in.peek());
	}
	RemoveScratch();

	CHECK(good);
	CHECK_EQ(100, (int)loaded.GetMinResultActionInfo());
	CHECK_EQ(150, (int)loaded.GetMaxResultActionInfo());
	CHECK_EQ(1, loaded.GetSize());
	CheckRow(loaded[0], true);
	CHECK_EQ((int)ACTIONINFO_PACKET_OTHER, (int)loaded[0].GetPacketType());
	CHECK_EQ((int)EFFECTSTATUS_NULL, (int)loaded[0].GetEffectStatus());
	CHECK_EQ(1, loaded[0].GetSize());
	CHECK_EQ(11, (int)loaded[0][0].EffectGeneratorID);
	CHECK_EQ(70, (int)loaded[0][0].SoundID);
	CHECK(loaded[0][0].bDelayNode);
	CHECK(loaded[0][0].bResultTime);
}

//----------------------------------------------------------------------
// A row count the file cannot hold is refused before allocation: the
// stream fails and the table stays empty.
//----------------------------------------------------------------------
TEST(ActionInfoTable, LoadRefusesARowCountLargerThanTheFile)
{
	Bytes b = TableHead(1000000);
	AppendRowHead(b, RowSpec{ false, ACTIONINFO_PACKET_NONE, EFFECTSTATUS_NULL, 0 });
	WriteScratch(b);

	MActionInfoTable table;
	bool failed = false;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		table.LoadFromFile(in);
		failed = in.fail();
	}
	RemoveScratch();

	CHECK(failed);
	CHECK_EQ(0, table.GetSize());
	// Out of range reads the constructor's row, never memory.
	CHECK_EQ((int)ACTIONINFO_NULL, (int)table[0].GetCastingActionInfo());
}

//----------------------------------------------------------------------
// A node count past the end of the file is refused the same way, inside
// the row: the row keeps no nodes and the table load stops there.
//----------------------------------------------------------------------
TEST(ActionInfoTable, LoadRefusesANodeCountLargerThanTheFile)
{
	Bytes b = TableHead(2);
	AppendRowHead(b, RowSpec{ false, ACTIONINFO_PACKET_SELF, EFFECTSTATUS_NULL, 50 });
	AppendNode(b, 11, false, false);
	WriteScratch(b);

	MActionInfoTable table;
	bool failed = false;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		table.LoadFromFile(in);
		failed = in.fail();
	}
	RemoveScratch();

	CHECK(failed);
	CHECK_EQ(2, table.GetSize());
	CheckRow(table[0], false);
	CHECK_EQ(0, table[0].GetSize());
	// The second row was never read: it is the constructor's.
	CHECK(table[1].GetName() == NULL);
	CHECK_EQ((int)ACTIONINFO_PACKET_NONE, (int)table[1].GetPacketType());
}

//----------------------------------------------------------------------
// A file cut inside a row's node table fails the stream; the rows read
// before the cut keep what they read, and the table keeps the size the
// file declared.
//----------------------------------------------------------------------
TEST(ActionInfoTable, LoadStopsAtAFileCutInsideTheNodeTable)
{
	Bytes b = TableHead(3);
	AppendRowHead(b, RowSpec{ false, ACTIONINFO_PACKET_ITEM, 5, 1 });
	AppendNode(b, 11, true, true);
	AppendRowHead(b, RowSpec{ true, ACTIONINFO_PACKET_ZONE, 6, 2 });
	AppendNode(b, 21, false, false);
	b.Word(31).Word(32);			// the second node, cut after 4 of 14 bytes
	WriteScratch(b);

	MActionInfoTable table;
	bool failed = false;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		table.LoadFromFile(in);
		failed = in.fail();
	}
	RemoveScratch();

	CHECK(failed);
	CHECK_EQ(3, table.GetSize());
	CheckRow(table[0], false);
	CHECK_EQ(1, table[0].GetSize());
	CHECK_EQ(11, (int)table[0][0].EffectGeneratorID);
	CHECK_EQ((int)ACTIONINFO_PACKET_ITEM, (int)table[0].GetPacketType());
	CHECK_EQ(5, (int)table[0].GetEffectStatus());
	// The cut row read its head and its first node.
	CheckRow(table[1], true);
	CHECK_EQ(2, table[1].GetSize());
	CHECK_EQ(21, (int)table[1][0].EffectGeneratorID);
	// The third row was never read.
	CHECK(table[2].GetName() == NULL);
}

//======================================================================
// Bytes the writer never emits.
//
// The loader read seven flags straight into bool storage (the row's
// useRepeatFrame, castingEffectToSelf, castingAction and
// ignoreFailDelay, the attack flag through a bool local, and each
// node's delayNode and resultTime) and cast the packet-type byte and the
// effect-status word to their enums unchecked. A bool holding 2 is
// neither true nor false - Clang's -fsanitize=bool traps its load, and
// Clang tests only the low bit where MSVC and GCC test for non-zero - and
// an enum value outside the enumeration's range is undefined, which
// -fsanitize=enum traps (ACTIONINFO_PACKET holds 0..15 and EFFECTSTATUS
// 0..511). The contract: a non-zero flag byte reads as true and a zero
// byte as false, and a packet type or effect status past the last
// enumerator reads as the constructor's NONE / EFFECTSTATUS_NULL.
//======================================================================
namespace {

unsigned char	StorageByte(const bool& b)
{
	unsigned char c = 0;
	std::memcpy(&c, &b, 1);
	return c;
}

// The row's flags are protected; the test reads their storage bytes.
struct ActionInfoFlags : public MActionInfo
{
	unsigned char	UseRepeatFrame() const		{ return StorageByte(m_bUseRepeatFrame); }
	unsigned char	CastingEffectToSelf() const	{ return StorageByte(m_bCastingEffectToSelf); }
	unsigned char	CastingAction() const		{ return StorageByte(m_bCastingAction); }
	unsigned char	IgnoreFailDelay() const		{ return StorageByte(m_bIgnoreFailDelay); }
	BOOL		Attack() const			{ return m_bAttack; }
	int		PacketType() const		{ int v = 0; std::memcpy(&v, &m_PacketType, sizeof(m_PacketType) < sizeof(v) ? sizeof(m_PacketType) : sizeof(v)); return v; }
	int		EffectStatus() const		{ int v = 0; std::memcpy(&v, &m_EffectStatus, sizeof(m_EffectStatus) < sizeof(v) ? sizeof(m_EffectStatus) : sizeof(v)); return v; }
};

// Offsets into a row built by AppendRowHead (name "Bloody Nail").
const size_t	kNameBytes		= 4 + 11;
const size_t	kUseRepeatFrame		= kNameBytes + 1 + 2 + 2;
const size_t	kCastingEffectToSelf	= kUseRepeatFrame + 1 + 3 * 20 + 2;
const size_t	kCastingActionInfo	= kCastingEffectToSelf + 1;
const size_t	kCastingAction		= kCastingActionInfo + 4;
const size_t	kPacketType		= kCastingAction + 1 + 1 + 1 + 1 + 1 + 2 + 1 + 1 + 4;
const size_t	kEffectStatus		= kPacketType + 1 + 2 + 4 + 2 + 4 + 2 + 4;
const size_t	kAttack			= kEffectStatus + 2;
// Without the action-step block: parent, mastery step, ignore fail delay.
const size_t	kIgnoreFailDelay	= kAttack + 1 + 1 + 1 + 2 + 1;

// Load one row, built without the action-step block, into a row object.
bool	LoadOneRow(ActionInfoFlags& info, const Bytes& b)
{
	WriteScratch(b);
	bool good = false;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		info.LoadFromFile(in);
		good = in.good();
	}
	RemoveScratch();
	return good;
}

} // namespace

TEST(ActionInfoTable, LoadReadsANonZeroFlagByteAsTrue)
{
	for (unsigned char byte : { (unsigned char)2, (unsigned char)0x80, (unsigned char)0xFF })
	{
		Bytes b;
		AppendRowHead(b, RowSpec{ false, ACTIONINFO_PACKET_SELF, EFFECTSTATUS_NULL, 1 });
		AppendNode(b, 11, false, false);
		b.data[kUseRepeatFrame] = byte;
		b.data[kCastingEffectToSelf] = byte;
		b.data[kCastingAction] = byte;
		b.data[kAttack] = byte;
		b.data[kIgnoreFailDelay] = byte;
		b.data[b.data.size() - 2] = byte;	// the node's delayNode
		b.data[b.data.size() - 1] = byte;	// the node's resultTime

		ActionInfoFlags info;
		CHECK(LoadOneRow(info, b));
		CHECK_EQ(1, (int)info.UseRepeatFrame());
		CHECK_EQ(1, (int)info.CastingEffectToSelf());
		CHECK_EQ(1, (int)info.CastingAction());
		CHECK_EQ(1, (int)info.IgnoreFailDelay());
		CHECK_EQ(TRUE, info.Attack());
		CHECK_EQ(1, info.GetSize());
		const ACTION_INFO_NODE* node = info.GetMutable(0);
		CHECK(node != nullptr);
		if (node == nullptr)
			continue;
		CHECK_EQ(1, (int)StorageByte(node->bDelayNode));
		CHECK_EQ(1, (int)StorageByte(node->bResultTime));
	}
}

TEST(ActionInfoTable, LoadReadsAZeroFlagByteAsFalse)
{
	Bytes b;
	AppendRowHead(b, RowSpec{ false, ACTIONINFO_PACKET_SELF, EFFECTSTATUS_NULL, 0 });
	b.data[kUseRepeatFrame] = 0;
	b.data[kCastingAction] = 0;
	b.data[kIgnoreFailDelay] = 0;
	b.data[kCastingEffectToSelf] = 0;

	ActionInfoFlags info;
	CHECK(LoadOneRow(info, b));
	CHECK_EQ(0, (int)info.UseRepeatFrame());
	CHECK_EQ(0, (int)info.CastingEffectToSelf());
	CHECK_EQ(0, (int)info.CastingAction());
	CHECK_EQ(0, (int)info.IgnoreFailDelay());
	CHECK_EQ(FALSE, info.Attack());
}

TEST(ActionInfoTable, LoadReadsAPacketTypePastTheLastEnumeratorAsNone)
{
	// The last enumerator loads as itself; one past it (still inside the
	// enum's range), the first byte outside the range and 0xFF read as NONE.
	const int cases[][2] = {
		{ ACTIONINFO_PACKET_ABSORB_SOUL, ACTIONINFO_PACKET_ABSORB_SOUL },
		{ ACTIONINFO_PACKET_ABSORB_SOUL + 1, ACTIONINFO_PACKET_NONE },
		{ 16, ACTIONINFO_PACKET_NONE },
		{ 0xFF, ACTIONINFO_PACKET_NONE },
	};
	for (const auto& c : cases)
	{
		Bytes b;
		AppendRowHead(b, RowSpec{ false, (BYTE)c[0], EFFECTSTATUS_NULL, 0 });

		ActionInfoFlags info;
		CHECK(LoadOneRow(info, b));
		CHECK_EQ(c[1], info.PacketType());
	}
}

TEST(ActionInfoTable, LoadReadsAnEffectStatusPastTheLastEnumeratorAsNull)
{
	// EFFECTSTATUS_NULL is the writer's "none" and loads as itself, as
	// does every status below it; past it (500 is still inside the
	// enum's range, 512 and 0xFFFF are not) reads as EFFECTSTATUS_NULL.
	const int cases[][2] = {
		{ 0, 0 },
		{ EFFECTSTATUS_MAX - 1, EFFECTSTATUS_MAX - 1 },
		{ EFFECTSTATUS_NULL, EFFECTSTATUS_NULL },
		{ 500, EFFECTSTATUS_NULL },
		{ 512, EFFECTSTATUS_NULL },
		{ 0xFFFF, EFFECTSTATUS_NULL },
	};
	for (const auto& c : cases)
	{
		Bytes b;
		AppendRowHead(b, RowSpec{ false, ACTIONINFO_PACKET_SELF, (WORD)c[0], 0 });

		ActionInfoFlags info;
		CHECK(LoadOneRow(info, b));
		CHECK_EQ(c[1], info.EffectStatus());
	}
}

// The casting action info is four bytes on disk and two in memory. The
// loader read all four into the member, so the upper two landed on the
// castingAction flag stored after it and on the padding behind that;
// the flag's own byte overwrote them only when the file went on. A file
// that ends after the field left the flag holding the field's third
// byte.
TEST(ActionInfoTable, LoadKeepsTheCastingActionFieldOutOfTheFlagAfterIt)
{
	Bytes b;
	AppendRowHead(b, RowSpec{ false, ACTIONINFO_PACKET_SELF, EFFECTSTATUS_NULL, 0 });
	b.data[kCastingActionInfo + 2] = 2;
	b.data[kCastingActionInfo + 3] = 7;
	b.data.resize(kCastingAction);		// the file ends after the field

	ActionInfoFlags info;
	CHECK(!LoadOneRow(info, b));
	CHECK_EQ(301, (int)info.GetCastingActionInfo());
	// The constructor's value: the flag's byte was never read.
	CHECK_EQ(0, (int)info.CastingAction());
}

// A row cut before its packet type, effect status and attack flag reads
// them as the constructor's values. Before the fix they came from
// uninitialised locals, which a test can only catch by chance: this is a
// regression guard.
TEST(ActionInfoTable, LoadKeepsTheDefaultsOfFieldsPastACut)
{
	Bytes b;
	AppendRowHead(b, RowSpec{ false, ACTIONINFO_PACKET_OTHER, 7, 0 });
	b.data.resize(kPacketType);

	ActionInfoFlags info;
	CHECK(!LoadOneRow(info, b));
	CHECK_EQ((int)ACTIONINFO_PACKET_NONE, info.PacketType());
	CHECK_EQ((int)EFFECTSTATUS_NULL, info.EffectStatus());
	CHECK_EQ(TRUE, info.Attack());
}
