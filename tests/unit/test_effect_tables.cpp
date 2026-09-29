//----------------------------------------------------------------------
// test_effect_tables.cpp
//----------------------------------------------------------------------
//
// The effect and creature sprite tables through their real loaders and
// writers, now that they are gamemodel members (docs/RESTRUCTURING.md
// task 4.1):
//
//   EffectSpriteType.inf        EFFECTSPRITETYPE_TABLE
//     [count:4] then count x [blt:1][frame:2][flags:1][actionFrame:2]
//     [femaleType:2][pairs:1] then pairs x [frame:2]
//     (flags: 0x2 the frame repeats, 0x1 the pair draws behind)
//   ActionEffectSpriteType.inf  MActionEffectSpriteTypeTable
//     [count:4] then count x ([frames:4] then frames x [frame:2])
//   EffectStatus.inf            EFFECTSTATUS_TABLE
//     [count:4] then count x [useSprite:1][attachGround:1][type:2]
//     [color:2][part:1][actionInfo:2][originalActionInfo:2][sound:4]
//   CreatureSprite.inf          CREATURESPRITE_TABLE
//     [count:4] then count x [frame:2][spritePos:4][shadowPos:4]
//     [first:2][last:2][firstShadow:2][lastShadow:2][type:1]
//
// Each loader's own validation is CTypeTable's: a count larger than what
// is left of the file is refused before allocation, and the row loop
// stops at the first failed read. The tests pin the layouts, the round
// trip through each writer, and those refusals.
//
//----------------------------------------------------------------------

#include "test_framework.h"

// The table headers take WORD and BYTE from the precompiled header in the
// library build; a test includes them itself.
#include "Platform.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "MCreatureSpriteTable.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

const char* const	kTempFile = "effect_tables_test.bin";

//----------------------------------------------------------------------
// A little-endian byte builder for the layouts above.
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

// Load a table from the scratch file; true when the stream is still good
// afterwards. atEnd says whether every byte was consumed.
template <class Table>
bool	LoadScratch(Table& table, bool* atEnd = nullptr)
{
	std::ifstream in(kTempFile, std::ios::binary);
	table.LoadFromFile(in);
	const bool good = in.good();
	if (atEnd != nullptr)
		*atEnd = good && in.peek() == std::char_traits<char>::eof();
	return good;
}

template <class Table>
void	SaveScratch(Table& table)
{
	std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
	table.SaveToFile(out);
}

std::vector<int>	Pairs(const EFFECTSPRITETYPETABLE_INFO& info)
{
	return std::vector<int>(info.PairFrameIDList.begin(), info.PairFrameIDList.end());
}

Bytes&	AppendSpriteType(Bytes& b, BLT_TYPE blt, unsigned short frame, unsigned char flags,
			 const std::vector<unsigned short>& pairs)
{
	b.Byte((unsigned char)blt).Word(frame).Byte(flags).Word((unsigned short)(frame + 1)).Word(700);
	b.Byte((unsigned char)pairs.size());
	for (unsigned short p : pairs)
		b.Word(p);
	return b;
}

Bytes&	AppendEffectStatus(Bytes& b, unsigned char useSprite, unsigned char attachGround,
			   unsigned short type, unsigned char part)
{
	b.Byte(useSprite).Byte(attachGround).Word(type).Word(0x7C00).Byte(part);
	return b.Word(55).Word(56).Int(1234);
}

Bytes&	AppendCreatureSprite(Bytes& b, unsigned short frame, unsigned char type)
{
	b.Word(frame).Int(0x1000).Int(0x2000);
	b.Word(10).Word(19).Word(20).Word(29);
	return b.Byte(type);
}

} // namespace

//======================================================================
// EffectSpriteType.inf
//======================================================================
TEST(EffectSpriteTypeTable, LoadReadsTheLayoutAndDecodesTheFlagByte)
{
	Bytes b;
	b.Int(3);
	AppendSpriteType(b, BLT_NORMAL, 100, 0x0, {});
	AppendSpriteType(b, BLT_SCREEN, 200, 0x2, { 201, 202 });
	AppendSpriteType(b, BLT_SHADOW, 300, 0x3, { 301 });
	WriteScratch(b);

	EFFECTSPRITETYPE_TABLE table;
	bool atEnd = false;
	CHECK(LoadScratch(table, &atEnd));
	RemoveScratch();
	CHECK(atEnd);

	CHECK_EQ(3, table.GetSize());
	CHECK_EQ((int)BLT_NORMAL, (int)table[0].BltType);
	CHECK_EQ(100, (int)table[0].FrameID);
	CHECK(!table[0].RepeatFrame);
	CHECK(!table[0].bPairFrameBack);
	CHECK_EQ(101, (int)table[0].ActionEffectFrameID);
	CHECK_EQ(700, (int)table[0].FemaleEffectSpriteType);
	CHECK(table[0].PairFrameIDList.empty());

	CHECK_EQ((int)BLT_SCREEN, (int)table[1].BltType);
	CHECK(table[1].RepeatFrame);
	CHECK(!table[1].bPairFrameBack);
	CHECK((Pairs(table[1]) == std::vector<int>{ 201, 202 }));

	CHECK_EQ((int)BLT_SHADOW, (int)table[2].BltType);
	CHECK(table[2].RepeatFrame);
	CHECK(table[2].bPairFrameBack);
	CHECK((Pairs(table[2]) == std::vector<int>{ 301 }));
}

TEST(EffectSpriteTypeTable, SaveThenLoadRoundTrips)
{
	{
		EFFECTSPRITETYPE_TABLE source;
		source.Init(2);
		for (int i = 0; i < 2; i++)
		{
			EFFECTSPRITETYPETABLE_INFO* info = source.GetMutable(i);
			CHECK(info != nullptr);
			if (info == nullptr)
				return;
			info->BltType = i == 0 ? BLT_EFFECT : BLT_SCREEN;
			info->FrameID = (TYPE_FRAMEID)(40 + i);
			info->RepeatFrame = i == 0;
			info->bPairFrameBack = i == 1;
			info->ActionEffectFrameID = (TYPE_FRAMEID)(50 + i);
			info->FemaleEffectSpriteType = (TYPE_EFFECTSPRITETYPE)(60 + i);
			if (i == 1)
			{
				info->PairFrameIDList.push_back(70);
				info->PairFrameIDList.push_back(71);
				info->PairFrameIDList.push_back(72);
			}
		}
		SaveScratch(source);
	}

	EFFECTSPRITETYPE_TABLE loaded;
	bool atEnd = false;
	CHECK(LoadScratch(loaded, &atEnd));
	RemoveScratch();
	CHECK(atEnd);

	CHECK_EQ(2, loaded.GetSize());
	CHECK_EQ((int)BLT_EFFECT, (int)loaded[0].BltType);
	CHECK_EQ(40, (int)loaded[0].FrameID);
	CHECK(loaded[0].RepeatFrame);
	CHECK(!loaded[0].bPairFrameBack);
	CHECK_EQ(50, (int)loaded[0].ActionEffectFrameID);
	CHECK_EQ(60, (int)loaded[0].FemaleEffectSpriteType);
	CHECK(loaded[0].PairFrameIDList.empty());
	CHECK_EQ((int)BLT_SCREEN, (int)loaded[1].BltType);
	CHECK(!loaded[1].RepeatFrame);
	CHECK(loaded[1].bPairFrameBack);
	CHECK((Pairs(loaded[1]) == std::vector<int>{ 70, 71, 72 }));
}

TEST(EffectSpriteTypeTable, LoadRefusesACountLargerThanTheFile)
{
	Bytes b;
	b.Int(100000);
	AppendSpriteType(b, BLT_NORMAL, 100, 0, {});
	WriteScratch(b);

	EFFECTSPRITETYPE_TABLE table;
	CHECK(!LoadScratch(table));
	RemoveScratch();
	CHECK_EQ(0, table.GetSize());
}

// A record whose pair list runs past the end of the file fails the
// stream; the records before it keep what they read.
TEST(EffectSpriteTypeTable, LoadFailsOnAPairListCutByTheEndOfTheFile)
{
	Bytes b;
	b.Int(3);
	AppendSpriteType(b, BLT_EFFECT, 100, 0x2, { 101 });
	AppendSpriteType(b, BLT_NORMAL, 200, 0x0, { 201 });
	b.data[b.data.size() - 3] = 4;		// the second record claims four pairs, one is there
	WriteScratch(b);

	EFFECTSPRITETYPE_TABLE table;
	CHECK(!LoadScratch(table));
	RemoveScratch();

	CHECK_EQ(3, table.GetSize());
	CHECK_EQ((int)BLT_EFFECT, (int)table[0].BltType);
	CHECK_EQ(100, (int)table[0].FrameID);
	CHECK((Pairs(table[0]) == std::vector<int>{ 101 }));
	CHECK_EQ(200, (int)table[1].FrameID);
	CHECK(!table[1].PairFrameIDList.empty());
	CHECK_EQ(201, *table[1].PairFrameIDList.begin());
}

//======================================================================
// ActionEffectSpriteType.inf: one frame array per effect sprite type,
// one frame per action.
//======================================================================
TEST(ActionEffectSpriteTypeTable, ANewFrameArrayHoldsOneFramePerSlayerAction)
{
	ACTION_FRAMEID_ARRAY frames;
	CHECK_EQ(ACTION_MAX_SLAYER, frames.GetSize());
	CHECK_EQ(0, (int)frames[0].FrameID);
	CHECK_EQ(0, (int)frames[ACTION_MAX_SLAYER - 1].FrameID);
}

TEST(ActionEffectSpriteTypeTable, LoadSizesEachArrayByItsOwnCount)
{
	Bytes b;
	b.Int(2);
	b.Int(3).Word(11).Word(12).Word(13);
	b.Int(1).Word(21);
	WriteScratch(b);

	MActionEffectSpriteTypeTable table;
	bool atEnd = false;
	CHECK(LoadScratch(table, &atEnd));
	RemoveScratch();
	CHECK(atEnd);

	CHECK_EQ(2, table.GetSize());
	CHECK_EQ(3, table[0].GetSize());
	CHECK_EQ(11, (int)table[0][0].FrameID);
	CHECK_EQ(13, (int)table[0][2].FrameID);
	CHECK_EQ(1, table[1].GetSize());
	CHECK_EQ(21, (int)table[1][0].FrameID);
	// Past an array: the default frame, never memory beyond it.
	CHECK_EQ(0, (int)table[1][5].FrameID);
}

TEST(ActionEffectSpriteTypeTable, SaveThenLoadRoundTrips)
{
	{
		MActionEffectSpriteTypeTable source;
		source.Init(2);
		for (int i = 0; i < 2; i++)
		{
			ACTION_FRAMEID_ARRAY* frames = source.GetMutable(i);
			CHECK(frames != nullptr);
			if (frames == nullptr)
				return;
			for (int a = 0; a < frames->GetSize(); a++)
				frames->GetMutable(a)->FrameID = (TYPE_FRAMEID)(i * 100 + a);
		}
		SaveScratch(source);
	}

	MActionEffectSpriteTypeTable loaded;
	bool atEnd = false;
	CHECK(LoadScratch(loaded, &atEnd));
	RemoveScratch();
	CHECK(atEnd);

	CHECK_EQ(2, loaded.GetSize());
	CHECK_EQ(ACTION_MAX_SLAYER, loaded[1].GetSize());
	CHECK_EQ(0, (int)loaded[0][0].FrameID);
	CHECK_EQ(ACTION_MAX_SLAYER - 1, (int)loaded[0][ACTION_MAX_SLAYER - 1].FrameID);
	CHECK_EQ(100 + 7, (int)loaded[1][7].FrameID);
}

TEST(ActionEffectSpriteTypeTable, LoadRefusesAFrameCountLargerThanTheFile)
{
	Bytes b;
	b.Int(2);
	b.Int(2).Word(11).Word(12);
	b.Int(5000).Word(21);
	WriteScratch(b);

	MActionEffectSpriteTypeTable table;
	CHECK(!LoadScratch(table));
	RemoveScratch();

	CHECK_EQ(2, table.GetSize());
	CHECK_EQ(12, (int)table[0][1].FrameID);
	// The refused array keeps the frames it was constructed with.
	CHECK_EQ(ACTION_MAX_SLAYER, table[1].GetSize());
	CHECK_EQ(0, (int)table[1][0].FrameID);
}

//======================================================================
// EffectStatus.inf
//======================================================================
TEST(EffectStatusTable, ANewNodeUsesNoSpriteColourOrAction)
{
	EFFECTSTATUS_NODE node;
	CHECK(node.bUseEffectSprite);
	CHECK(!node.bAttachGround);
	CHECK_EQ((int)EFFECTSPRITETYPE_NULL, (int)node.EffectSpriteType);
	CHECK_EQ(0xFFFF, (int)node.EffectColor);
	CHECK_EQ((int)ADDON_NULL, (int)node.EffectColorPart);
	CHECK_EQ((int)ACTIONINFO_NULL, (int)node.ActionInfo);
	CHECK_EQ((int)ACTIONINFO_NULL, (int)node.OriginalActionInfo);
	CHECK_EQ(SOUNDID_NULL, node.SoundID);
}

TEST(EffectStatusTable, LoadReadsTheLayout)
{
	Bytes b;
	b.Int(2);
	AppendEffectStatus(b, 1, 0, 400, ADDON_COAT);
	AppendEffectStatus(b, 0, 1, EFFECTSPRITETYPE_NULL, 0xFF);
	WriteScratch(b);

	EFFECTSTATUS_TABLE table;
	bool atEnd = false;
	CHECK(LoadScratch(table, &atEnd));
	RemoveScratch();
	CHECK(atEnd);

	CHECK_EQ(2, table.GetSize());
	CHECK(table[0].bUseEffectSprite);
	CHECK(!table[0].bAttachGround);
	CHECK_EQ(400, (int)table[0].EffectSpriteType);
	CHECK_EQ(0x7C00, (int)table[0].EffectColor);
	CHECK_EQ((int)ADDON_COAT, (int)table[0].EffectColorPart);
	CHECK_EQ(55, (int)table[0].ActionInfo);
	CHECK_EQ(56, (int)table[0].OriginalActionInfo);
	CHECK_EQ(1234, table[0].SoundID);
	CHECK(!table[1].bUseEffectSprite);
	CHECK(table[1].bAttachGround);
	// The part is one byte on disk: ADDON_NULL (0xFFFF) cannot be
	// written, and its byte 0xFF reads back as 0xFF. Every byte is a
	// value ADDON can hold (its range reaches 0xFFFF).
	CHECK_EQ(0xFF, (int)table[1].EffectColorPart);
}

TEST(EffectStatusTable, SaveThenLoadRoundTrips)
{
	{
		EFFECTSTATUS_TABLE source;
		source.Init(2);
		EFFECTSTATUS_NODE* node = source.GetMutable(1);
		CHECK(node != nullptr);
		if (node == nullptr)
			return;
		node->bUseEffectSprite = false;
		node->bAttachGround = true;
		node->EffectSpriteType = 321;
		node->EffectColor = 0x03E0;
		node->EffectColorPart = ADDON_HAIR;
		node->ActionInfo = 77;
		node->OriginalActionInfo = 78;
		node->SoundID = 99;
		SaveScratch(source);
	}

	EFFECTSTATUS_TABLE loaded;
	bool atEnd = false;
	CHECK(LoadScratch(loaded, &atEnd));
	RemoveScratch();
	CHECK(atEnd);

	CHECK_EQ(2, loaded.GetSize());
	// Row 0 was the constructor's; ADDON_NULL narrows to its low byte.
	CHECK(loaded[0].bUseEffectSprite);
	CHECK_EQ((int)EFFECTSPRITETYPE_NULL, (int)loaded[0].EffectSpriteType);
	CHECK_EQ(0xFF, (int)loaded[0].EffectColorPart);
	CHECK_EQ(SOUNDID_NULL, loaded[0].SoundID);
	CHECK(!loaded[1].bUseEffectSprite);
	CHECK(loaded[1].bAttachGround);
	CHECK_EQ(321, (int)loaded[1].EffectSpriteType);
	CHECK_EQ(0x03E0, (int)loaded[1].EffectColor);
	CHECK_EQ((int)ADDON_HAIR, (int)loaded[1].EffectColorPart);
	CHECK_EQ(77, (int)loaded[1].ActionInfo);
	CHECK_EQ(78, (int)loaded[1].OriginalActionInfo);
	CHECK_EQ(99, loaded[1].SoundID);
}

TEST(EffectStatusTable, LoadRefusesACountLargerThanTheFile)
{
	Bytes b;
	b.Int(64);
	AppendEffectStatus(b, 1, 0, 400, ADDON_COAT);
	WriteScratch(b);

	EFFECTSTATUS_TABLE table;
	CHECK(!LoadScratch(table));
	RemoveScratch();
	CHECK_EQ(0, table.GetSize());
}

// A file cut inside a record fails the stream; the records before the
// cut keep what they read.
TEST(EffectStatusTable, LoadStopsAtAFileCutInsideARecord)
{
	Bytes b;
	b.Int(3);
	AppendEffectStatus(b, 0, 1, 400, ADDON_COAT);
	AppendEffectStatus(b, 1, 1, 401, ADDON_HAIR);
	b.data.resize(b.data.size() - 6);	// cut inside the second record's action infos
	WriteScratch(b);

	EFFECTSTATUS_TABLE table;
	CHECK(!LoadScratch(table));
	RemoveScratch();

	CHECK_EQ(3, table.GetSize());
	CHECK(!table[0].bUseEffectSprite);
	CHECK_EQ(400, (int)table[0].EffectSpriteType);
	CHECK_EQ(1234, table[0].SoundID);
	CHECK_EQ(401, (int)table[1].EffectSpriteType);
	// The third record was never read.
	CHECK_EQ((int)EFFECTSPRITETYPE_NULL, (int)table[2].EffectSpriteType);
}

//======================================================================
// CreatureSprite.inf
//======================================================================
TEST(CreatureSpriteTable, LoadReadsTheLayoutAndLeavesTheLoadedFlagUnset)
{
	Bytes b;
	b.Int(2);
	AppendCreatureSprite(b, 30, FLAG_CREATURESPRITE_PLAYER_VAMPIRE);
	AppendCreatureSprite(b, 31, FLAG_CREATURESPRITE_MONSTER_ALL);
	WriteScratch(b);

	CREATURESPRITE_TABLE table;
	bool atEnd = false;
	CHECK(LoadScratch(table, &atEnd));
	RemoveScratch();
	CHECK(atEnd);

	CHECK_EQ(2, table.GetSize());
	CHECK_EQ(30, (int)table[0].FrameID);
	CHECK_EQ(10, (int)table[0].FirstSpriteID);
	CHECK_EQ(19, (int)table[0].LastSpriteID);
	CHECK_EQ(20, (int)table[0].FirstShadowSpriteID);
	CHECK_EQ(29, (int)table[0].LastShadowSpriteID);
	CHECK_EQ(FLAG_CREATURESPRITE_PLAYER_VAMPIRE, (int)table[0].CreatureType);
	CHECK(table[0].IsPlayerSprite() != 0);
	CHECK(table[0].IsVampireSprite() != 0);
	CHECK(table[0].IsPlayerVampireOnlySprite() != 0);
	CHECK(table[0].IsPlayerOnlySprite() != 0);
	CHECK(table[0].IsMonsterSprite() == 0);
	// Not on disk: the sprites of a freshly loaded row are not loaded.
	CHECK_EQ(FALSE, table[0].bLoad);

	CHECK_EQ(31, (int)table[1].FrameID);
	CHECK(table[1].IsMonsterSprite() != 0);
	CHECK(table[1].IsSlayerSprite() != 0);
	CHECK(table[1].IsOustersSprite() != 0);
	CHECK(table[1].IsPlayerSprite() == 0);
	CHECK(table[1].IsPlayerOnlySprite() == 0);
}

TEST(CreatureSpriteTable, SaveThenLoadRoundTrips)
{
	{
		CREATURESPRITE_TABLE source;
		source.Init(1);
		CREATURESPRITETABLE_INFO* info = source.GetMutable(0);
		CHECK(info != nullptr);
		if (info == nullptr)
			return;
		info->FrameID = 44;
		info->SpriteFilePosition = 0x1234;
		info->SpriteShadowFilePosition = 0x5678;
		info->FirstSpriteID = 1;
		info->LastSpriteID = 2;
		info->FirstShadowSpriteID = 3;
		info->LastShadowSpriteID = 4;
		info->CreatureType = FLAG_CREATURESPRITE_NPC_SLAYER;
		info->bLoad = TRUE;
		SaveScratch(source);
	}

	CREATURESPRITE_TABLE loaded;
	bool atEnd = false;
	CHECK(LoadScratch(loaded, &atEnd));
	RemoveScratch();
	CHECK(atEnd);

	CHECK_EQ(1, loaded.GetSize());
	CHECK_EQ(44, (int)loaded[0].FrameID);
	CHECK(loaded[0].SpriteFilePosition == 0x1234L);
	CHECK(loaded[0].SpriteShadowFilePosition == 0x5678L);
	CHECK_EQ(1, (int)loaded[0].FirstSpriteID);
	CHECK_EQ(2, (int)loaded[0].LastSpriteID);
	CHECK_EQ(3, (int)loaded[0].FirstShadowSpriteID);
	CHECK_EQ(4, (int)loaded[0].LastShadowSpriteID);
	CHECK_EQ(FLAG_CREATURESPRITE_NPC_SLAYER, (int)loaded[0].CreatureType);
	CHECK(loaded[0].IsNPCSprite() != 0);
	CHECK_EQ(FALSE, loaded[0].bLoad);
}

TEST(CreatureSpriteTable, LoadRefusesACountLargerThanTheFile)
{
	Bytes b;
	b.Int(20);
	AppendCreatureSprite(b, 30, FLAG_CREATURESPRITE_NPC_ALL);
	WriteScratch(b);

	CREATURESPRITE_TABLE table;
	CHECK(!LoadScratch(table));
	RemoveScratch();
	CHECK_EQ(0, table.GetSize());
}

TEST(CreatureSpriteTable, LoadStopsAtAFileCutInsideARecord)
{
	Bytes b;
	b.Int(2);
	AppendCreatureSprite(b, 30, FLAG_CREATURESPRITE_NPC_ALL);
	AppendCreatureSprite(b, 31, FLAG_CREATURESPRITE_NPC_ALL);
	b.data.resize(b.data.size() - 1);	// the second record's type byte is missing
	WriteScratch(b);

	CREATURESPRITE_TABLE table;
	CHECK(!LoadScratch(table));
	RemoveScratch();

	CHECK_EQ(2, table.GetSize());
	CHECK_EQ(30, (int)table[0].FrameID);
	CHECK_EQ(FLAG_CREATURESPRITE_NPC_ALL, (int)table[0].CreatureType);
	CHECK_EQ(31, (int)table[1].FrameID);
	// The constructor's type, since its byte was never read.
	CHECK_EQ(0, (int)table[1].CreatureType);
}

//======================================================================
// EffectStatus.inf: bytes the writer never emits.
//
// The two flags were read straight into bool storage, so a byte other
// than 0 or 1 made a bool that is neither true nor false (Clang's
// -fsanitize=bool traps its load). The contract: a non-zero byte reads
// as true, and the flag's storage holds 1.
//======================================================================
namespace {

unsigned char	StorageByte(const bool& b)
{
	unsigned char c = 0;
	std::memcpy(&c, &b, 1);
	return c;
}

} // namespace

TEST(EffectStatusTable, LoadReadsANonZeroFlagByteAsTrue)
{
	Bytes b;
	b.Int(3);
	AppendEffectStatus(b, 2, 0x80, 400, ADDON_COAT);
	AppendEffectStatus(b, 0xFF, 0, 401, ADDON_COAT);
	AppendEffectStatus(b, 0, 1, 402, ADDON_COAT);
	WriteScratch(b);

	EFFECTSTATUS_TABLE table;
	CHECK(LoadScratch(table));
	RemoveScratch();

	CHECK_EQ(3, table.GetSize());
	CHECK_EQ(1, (int)StorageByte(table[0].bUseEffectSprite));
	CHECK_EQ(1, (int)StorageByte(table[0].bAttachGround));
	CHECK_EQ(1, (int)StorageByte(table[1].bUseEffectSprite));
	CHECK_EQ(0, (int)StorageByte(table[1].bAttachGround));
	CHECK_EQ(0, (int)StorageByte(table[2].bUseEffectSprite));
	CHECK_EQ(1, (int)StorageByte(table[2].bAttachGround));
	CHECK_EQ(402, (int)table[2].EffectSpriteType);
}

// A record cut before its colour part keeps the part it held: the part
// used to be assigned from a local the failed read never wrote.
// Uninitialised stack contents make this a regression guard.
TEST(EffectStatusTable, LoadKeepsTheColourPartOfARecordCutBeforeIt)
{
	Bytes b;
	b.Int(1);
	AppendEffectStatus(b, 1, 0, 400, ADDON_COAT);
	b.data.resize(4 + 6);			// the flags, the type and the colour

	EFFECTSTATUS_TABLE table;
	WriteScratch(b);
	CHECK(!LoadScratch(table));
	RemoveScratch();

	CHECK_EQ(1, table.GetSize());
	CHECK_EQ(0x7C00, (int)table[0].EffectColor);
	CHECK_EQ((int)ADDON_NULL, (int)table[0].EffectColorPart);
}

//======================================================================
// EffectSpriteType.inf: bytes the writer never emits.
//
// The draw type byte was cast to BLT_TYPE unchecked. BLT_TYPE holds 0..3,
// so a byte of 4 or more is a value the enum cannot represent: undefined,
// and Clang's -fsanitize=enum traps its load (GameInitInfo logs the first
// ten rows' draw types at start-up, and every effect generator reads it).
// Such a row now draws as BLT_EFFECT, the draw type MAttachEffect already
// gives an effect sprite type the table does not describe.
//======================================================================
TEST(EffectSpriteTypeTable, LoadReadsADrawTypePastTheLastEnumeratorAsEffect)
{
	Bytes b;
	b.Int(3);
	AppendSpriteType(b, BLT_SCREEN, 100, 0, {});
	AppendSpriteType(b, BLT_NORMAL, 200, 0, {});
	AppendSpriteType(b, BLT_NORMAL, 300, 0, {});
	b.data[4 + 9] = 4;			// the second row's draw type
	b.data[4 + 9 + 9] = 0xFF;		// the third row's
	WriteScratch(b);

	EFFECTSPRITETYPE_TABLE table;
	CHECK(LoadScratch(table));
	RemoveScratch();

	CHECK_EQ(3, table.GetSize());
	CHECK_EQ((int)BLT_SCREEN, (int)table[0].BltType);
	CHECK_EQ((int)BLT_EFFECT, (int)table[1].BltType);
	CHECK_EQ(200, (int)table[1].FrameID);
	CHECK_EQ((int)BLT_EFFECT, (int)table[2].BltType);
	CHECK_EQ(300, (int)table[2].FrameID);
}

// A pair list that claims more pairs than the file holds keeps only the
// pairs that were read. The loop used to push one entry per pair the
// count claimed, repeating the last frame read for every failed read.
TEST(EffectSpriteTypeTable, LoadKeepsOnlyThePairsTheFileHolds)
{
	Bytes b;
	b.Int(2);
	AppendSpriteType(b, BLT_EFFECT, 100, 0, {});
	AppendSpriteType(b, BLT_NORMAL, 200, 0, { 201 });
	b.data[b.data.size() - 3] = 200;	// the second record claims 200 pairs
	WriteScratch(b);

	EFFECTSPRITETYPE_TABLE table;
	CHECK(!LoadScratch(table));
	RemoveScratch();

	CHECK_EQ(2, table.GetSize());
	CHECK_EQ(200, (int)table[1].FrameID);
	CHECK((Pairs(table[1]) == std::vector<int>{ 201 }));
}

//======================================================================
// CreatureSprite.inf: the two sprite-pack file positions are four bytes
// on disk and a `long` in memory - four bytes on Windows, eight on
// macOS and Linux. The loader read the four bytes into the long's first
// half and left the second as it was (the constructor does not set it),
// so off Windows a position read back as whatever the upper half held.
// The contract: the position is the file's signed 32-bit value on every
// platform, as Windows has always read it.
//======================================================================
TEST(CreatureSpriteTable, LoadReadsTheFilePositionsAsSigned32BitValues)
{
	Bytes b;
	AppendCreatureSprite(b, 30, FLAG_CREATURESPRITE_NPC_ALL);
	b.data[2] = 0x78; b.data[3] = 0x56; b.data[4] = 0x34; b.data[5] = 0x12;
	b.data[6] = 0xFF; b.data[7] = 0xFF; b.data[8] = 0xFF; b.data[9] = 0xFF;
	WriteScratch(b);

	CREATURESPRITETABLE_INFO info;
	// What the row held before: every bit set, as a reused or never
	// zeroed row may hold.
	info.SpriteFilePosition = -1;
	info.SpriteShadowFilePosition = 0x7FFFFFFF;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		info.LoadFromFile(in);
		CHECK(in.good());
	}
	RemoveScratch();

	CHECK(info.SpriteFilePosition == 0x12345678L);
	CHECK(info.SpriteShadowFilePosition == -1L);
	CHECK_EQ(30, (int)info.FrameID);
}
