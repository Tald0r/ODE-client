//----------------------------------------------------------------------
// AffectModifyInfo - the one function every ModifyInfo packet's status
// changes go through (gamemodel since task 4.14; it was in the
// executable's PacketFunction.cpp).
//
// The packets are real: built from wire bytes through ModifyInfo::read
// (and the GC packets' own factories and readers), then applied to a
// real MStatus. The creature and player overrides of SetStatus are
// executable-side; BoundedStatus below bounds its index the way
// MCreature::SetStatus does, so the out-of-range tests can show what
// the function itself passes on.
//----------------------------------------------------------------------
#include "test_framework.h"
#include "packet_stream_access.h"

#include "Client/AffectModifyInfo.h"
#include "Client/MStatus.h"

#include "SocketInputStream.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "ModifyInfo.h"
#include "Gpackets/GCAttackArmsOK1.h"
#include "Gpackets/GCAttackArmsOK2.h"
#include "Gpackets/GCAttackMeleeOK1.h"
#include "Gpackets/GCAttackMeleeOK2.h"
#include "Gpackets/GCBloodDrainOK1.h"
#include "Gpackets/GCBloodDrainOK2.h"
#include "Gpackets/GCCrossCounterOK1.h"
#include "Gpackets/GCCrossCounterOK2.h"
#include "Gpackets/GCKnocksTargetBackOK1.h"
#include "Gpackets/GCKnocksTargetBackOK2.h"
#include "Gpackets/GCMakeItemFail.h"
#include "Gpackets/GCMakeItemOK.h"
#include "Gpackets/GCMineExplosionOK1.h"
#include "Gpackets/GCModifyInformation.h"
#include "Gpackets/GCOtherModifyInfo.h"
#include "Gpackets/GCSkillFailed1.h"
#include "Gpackets/GCSkillToInventoryOK1.h"
#include "Gpackets/GCSkillToObjectOK1.h"
#include "Gpackets/GCSkillToObjectOK2.h"
#include "Gpackets/GCSkillToObjectOK6.h"
#include "Gpackets/GCSkillToSelfOK1.h"
#include "Gpackets/GCSkillToTileOK1.h"
#include "Gpackets/GCSkillToTileOK2.h"
#include "Gpackets/GCSkillToTileOK6.h"
#include "Gpackets/GCThrowBombOK1.h"
#include "Gpackets/GCThrowBombOK2.h"
#include "Gpackets/GCThrowItemOK2.h"
#include "Gpackets/GCUseBonusPointOK.h"
#include "Gpackets/GCUseOK.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
	//------------------------------------------------------------------
	// A small ring over a never-used socket (test_packetwire_parsers.cpp
	// has the reasoning).
	//------------------------------------------------------------------
	struct StreamFixture
	{
		Socket				m_Socket;
		SocketInputStream	m_Stream;

		explicit StreamFixture(uint bufferLen = 1024)
		: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
		  m_Stream(&m_Socket, bufferLen)
		{
		}

		void Preload(const std::vector<unsigned char>& bytes)
		{
			SocketInputStreamTestAccess::Preload(m_Stream, bytes.data(), (unsigned int)bytes.size());
		}
	};

	// ModifyInfo itself is abstract (Packet's getPacketID is pure).
	class TestModifyInfo : public ModifyInfo
	{
	public:
		PacketID_t getPacketID() const { return 0; }
#ifdef __DEBUG_OUTPUT__
		std::string getPacketName() const { return "TestModifyInfo"; }
#endif
	};

	struct Entry
	{
		BYTE type;
		DWORD value;
	};

	//------------------------------------------------------------------
	// The ModifyInfo wire layout: [shortCount:1][(type:1, value:2 LE)...]
	// [longCount:1][(type:1, value:4 LE)...]
	//------------------------------------------------------------------
	std::vector<unsigned char> ModifyInfoBytes(const std::vector<Entry>& shorts, const std::vector<Entry>& longs)
	{
		std::vector<unsigned char> bytes;
		bytes.push_back((unsigned char)shorts.size());
		for (const Entry& e : shorts)
		{
			bytes.push_back(e.type);
			bytes.push_back((unsigned char)(e.value & 0xFF));
			bytes.push_back((unsigned char)((e.value >> 8) & 0xFF));
		}
		bytes.push_back((unsigned char)longs.size());
		for (const Entry& e : longs)
		{
			bytes.push_back(e.type);
			for (int b = 0; b < 4; ++b)
				bytes.push_back((unsigned char)((e.value >> (8 * b)) & 0xFF));
		}
		return bytes;
	}

	// Reads the bytes into a fresh ModifyInfo, as the receive loop does.
	void ReadInto(ModifyInfo& info, const std::vector<unsigned char>& bytes)
	{
		StreamFixture f;
		f.Preload(bytes);
		info.read(f.m_Stream);
		CHECK(f.m_Stream.isEmpty());
	}

	// A status whose every slot starts at a known value.
	class FilledStatus : public MStatus
	{
	public:
		explicit FilledStatus(DWORD fill = 0x5A5A5A5Au)
		{
			for (DWORD n = 0; n < MAX_MODIFY; ++n)
				m_Status[n] = fill;
		}
	};

	//------------------------------------------------------------------
	// Records every SetStatus it is handed, then applies it with the
	// same bound MCreature::SetStatus and MPlayer::SetStatus put first.
	//------------------------------------------------------------------
	class BoundedStatus : public FilledStatus
	{
	public:
		void SetStatus(DWORD n, DWORD value) override
		{
			calls.push_back(Entry{(BYTE)n, value});
			rawTypes.push_back(n);
			if (n >= MAX_MODIFY)
				return;
			MStatus::SetStatus(n, value);
		}

		std::vector<Entry> calls;
		std::vector<DWORD> rawTypes;
	};

	const DWORD kFill = 0x5A5A5A5Au;
}

//----------------------------------------------------------------------
// The two entry kinds
//----------------------------------------------------------------------

TEST(AffectModifyInfo, ShortEntriesLandInTheirSlots)
{
	TestModifyInfo info;
	ReadInto(info, ModifyInfoBytes({ { MODIFY_CURRENT_HP, 120 }, { MODIFY_MAX_HP, 300 } }, {}));

	FilledStatus status;
	AffectModifyInfo(&status, &info);

	CHECK_EQ(120u, status.GetHP());
	CHECK_EQ(300u, status.GetMAX_HP());
	CHECK_EQ(kFill, status.GetMP());
}

TEST(AffectModifyInfo, LongEntriesLandInTheirSlots)
{
	TestModifyInfo info;
	ReadInto(info, ModifyInfoBytes({}, { { MODIFY_GOLD, 2000000000u }, { MODIFY_VAMP_EXP_REMAIN, 70000 } }));

	FilledStatus status;
	AffectModifyInfo(&status, &info);

	CHECK_EQ(2000000000u, status.GetGold());
	CHECK_EQ(70000u, status.GetVampExp());
	CHECK_EQ(kFill, status.GetFAME());
}

TEST(AffectModifyInfo, EveryInRangeTypeAsAShortEntry)
{
	std::vector<Entry> shorts;
	for (DWORD n = 0; n < MAX_MODIFY; ++n)
		shorts.push_back(Entry{(BYTE)n, 0x100u + n});

	TestModifyInfo info;
	ReadInto(info, ModifyInfoBytes(shorts, {}));

	FilledStatus status;
	AffectModifyInfo(&status, &info);

	for (DWORD n = 0; n < MAX_MODIFY; ++n)
		CHECK_EQ(0x100u + n, status.GetStatus(n));
}

TEST(AffectModifyInfo, EveryInRangeTypeAsALongEntry)
{
	std::vector<Entry> longs;
	for (DWORD n = 0; n < MAX_MODIFY; ++n)
		longs.push_back(Entry{(BYTE)n, 0x01000000u + n});

	TestModifyInfo info;
	ReadInto(info, ModifyInfoBytes({}, longs));

	FilledStatus status;
	AffectModifyInfo(&status, &info);

	for (DWORD n = 0; n < MAX_MODIFY; ++n)
		CHECK_EQ(0x01000000u + n, status.GetStatus(n));
}

TEST(AffectModifyInfo, EachEntryKeepsItsWholeWidth)
{
	// A short value is two wire bytes, a long one four; the top of each
	// range arrives unchanged, and a long 0xFFFFFFFF is stored even though
	// it equals MODIFY_NULL.
	TestModifyInfo info;
	ReadInto(info, ModifyInfoBytes({ { MODIFY_CURRENT_MP, 0xFFFF }, { MODIFY_MAX_MP, 0 } },
		{ { MODIFY_GOLD, 0xFFFFFFFFu }, { MODIFY_FAME, 0x80000000u } }));

	FilledStatus status;
	AffectModifyInfo(&status, &info);

	CHECK_EQ(0xFFFFu, status.GetMP());
	CHECK_EQ(0u, status.GetMAX_MP());
	CHECK_EQ(0xFFFFFFFFu, status.GetGold());
	CHECK_EQ(0x80000000u, status.GetFAME());
}

//----------------------------------------------------------------------
// Order and draining
//----------------------------------------------------------------------

TEST(AffectModifyInfo, EntriesApplyInWireOrderShortsFirst)
{
	// A type in both lists ends with its long value; a type repeated in
	// one list ends with its later entry.
	TestModifyInfo info;
	ReadInto(info, ModifyInfoBytes({ { MODIFY_LEVEL, 10 }, { MODIFY_ALIGNMENT, 1 }, { MODIFY_ALIGNMENT, 2 } },
		{ { MODIFY_LEVEL, 11 }, { MODIFY_BULLET, 30 } }));

	BoundedStatus status;
	AffectModifyInfo(&status, &info);

	CHECK_EQ((size_t)5, status.calls.size());
	if (status.calls.size() == 5)
	{
		CHECK_EQ((BYTE)MODIFY_LEVEL, status.calls[0].type);
		CHECK_EQ(10u, status.calls[0].value);
		CHECK_EQ((BYTE)MODIFY_ALIGNMENT, status.calls[1].type);
		CHECK_EQ(1u, status.calls[1].value);
		CHECK_EQ((BYTE)MODIFY_ALIGNMENT, status.calls[2].type);
		CHECK_EQ(2u, status.calls[2].value);
		CHECK_EQ((BYTE)MODIFY_LEVEL, status.calls[3].type);
		CHECK_EQ(11u, status.calls[3].value);
		CHECK_EQ((BYTE)MODIFY_BULLET, status.calls[4].type);
		CHECK_EQ(30u, status.calls[4].value);
	}
	CHECK_EQ(11u, status.GetLEVEL());
	CHECK_EQ(2u, status.GetAlignment());
	CHECK_EQ(30u, status.GetBullet());
}

TEST(AffectModifyInfo, DrainsThePacketSoASecondCallChangesNothing)
{
	TestModifyInfo info;
	ReadInto(info, ModifyInfoBytes({ { MODIFY_CURRENT_HP, 5 } }, { { MODIFY_GOLD, 6 } }));

	BoundedStatus status;
	AffectModifyInfo(&status, &info);
	CHECK_EQ(0, (int)info.getShortCount());
	CHECK_EQ(0, (int)info.getLongCount());
	CHECK_EQ((size_t)2, status.calls.size());

	AffectModifyInfo(&status, &info);
	CHECK_EQ((size_t)2, status.calls.size());
	CHECK_EQ(5u, status.GetHP());
	CHECK_EQ(6u, status.GetGold());
}

TEST(AffectModifyInfo, AnEmptyPacketChangesNothing)
{
	TestModifyInfo info;
	ReadInto(info, ModifyInfoBytes({}, {}));

	BoundedStatus status;
	AffectModifyInfo(&status, &info);

	CHECK(status.calls.empty());
	for (DWORD n = 0; n < MAX_MODIFY; ++n)
		CHECK_EQ(kFill, status.GetStatus(n));
}

//----------------------------------------------------------------------
// Out-of-range types from the wire
//----------------------------------------------------------------------

TEST(AffectModifyInfo, PassesAnOutOfRangeWireTypeOnUnchecked)
{
	// The type is a wire byte (0-255) and MAX_MODIFY is far below 255.
	// AffectModifyInfo does not check it: the SetStatus it calls must.
	// Every live caller passes the player or a creature, whose overrides
	// refuse it; this status bounds the same way, so the slots stay put.
	TestModifyInfo info;
	ReadInto(info, ModifyInfoBytes({ { (BYTE)MAX_MODIFY, 1 }, { MODIFY_CURRENT_HP, 7 }, { 0xFF, 2 } },
		{ { 200, 3 }, { MODIFY_GOLD, 8 }, { 0xFF, 4 } }));

	BoundedStatus status;
	AffectModifyInfo(&status, &info);

	CHECK_EQ((size_t)6, status.rawTypes.size());
	if (status.rawTypes.size() == 6)
	{
		CHECK_EQ((DWORD)MAX_MODIFY, status.rawTypes[0]);
		CHECK_EQ((DWORD)MODIFY_CURRENT_HP, status.rawTypes[1]);
		CHECK_EQ(255u, status.rawTypes[2]);
		CHECK_EQ(200u, status.rawTypes[3]);
		CHECK_EQ((DWORD)MODIFY_GOLD, status.rawTypes[4]);
		CHECK_EQ(255u, status.rawTypes[5]);
	}

	// The in-range entries around the refused ones still land, and
	// nothing else changed.
	for (DWORD n = 0; n < MAX_MODIFY; ++n)
	{
		DWORD expected = kFill;
		if (n == MODIFY_CURRENT_HP)
			expected = 7;
		else if (n == MODIFY_GOLD)
			expected = 8;
		CHECK_EQ(expected, status.GetStatus(n));
	}
}

TEST(AffectModifyInfo, EveryOutOfRangeWireTypeIsPassedOn)
{
	std::vector<Entry> shorts;
	for (DWORD n = MAX_MODIFY; n <= 0xFF; ++n)
		shorts.push_back(Entry{(BYTE)n, n});

	TestModifyInfo info;
	ReadInto(info, ModifyInfoBytes(shorts, {}));

	BoundedStatus status;
	AffectModifyInfo(&status, &info);

	CHECK_EQ(shorts.size(), status.rawTypes.size());
	for (size_t i = 0; i < status.rawTypes.size() && i < shorts.size(); ++i)
		CHECK_EQ((DWORD)shorts[i].type, status.rawTypes[i]);
	for (DWORD n = 0; n < MAX_MODIFY; ++n)
		CHECK_EQ(kFill, status.GetStatus(n));
}

//----------------------------------------------------------------------
// Real packets
//----------------------------------------------------------------------

TEST(AffectModifyInfo, AppliesAGCModifyInformationReadThroughItsFactory)
{
	GCModifyInformationFactory factory;
	std::unique_ptr<Packet> packet(factory.createPacket());
	GCModifyInformation* pInfo = dynamic_cast<GCModifyInformation*>(packet.get());
	CHECK(pInfo != NULL);
	if (pInfo == NULL)
		return;

	ReadInto(*pInfo, ModifyInfoBytes({ { MODIFY_CURRENT_HP, 99 }, { MODIFY_BONUS_POINT, 3 } },
		{ { MODIFY_STR_EXP_REMAIN, 123456 } }));

	FilledStatus status;
	AffectModifyInfo(&status, pInfo);

	CHECK_EQ(99u, status.GetHP());
	CHECK_EQ(3u, status.GetBonusPoint());
	CHECK_EQ(123456u, status.GetSTR_EXP());
}

TEST(AffectModifyInfo, AppliesAGCOtherModifyInfoAfterItsObjectID)
{
	// GCOtherModifyInfoHandler applies this one to another creature.
	std::vector<unsigned char> bytes = { 0x78, 0x56, 0x34, 0x12 };	// object id 0x12345678
	const std::vector<unsigned char> modify = ModifyInfoBytes({ { MODIFY_ALIGNMENT, 4 } }, { { MODIFY_GUILDID, 77 } });
	bytes.insert(bytes.end(), modify.begin(), modify.end());

	GCOtherModifyInfoFactory factory;
	std::unique_ptr<Packet> packet(factory.createPacket());
	GCOtherModifyInfo* pInfo = dynamic_cast<GCOtherModifyInfo*>(packet.get());
	CHECK(pInfo != NULL);
	if (pInfo == NULL)
		return;

	StreamFixture f;
	f.Preload(bytes);
	pInfo->read(f.m_Stream);
	CHECK(f.m_Stream.isEmpty());
	CHECK_EQ((ObjectID_t)0x12345678, pInfo->getObjectID());

	FilledStatus status;
	AffectModifyInfo(&status, pInfo);

	CHECK_EQ(4u, status.GetAlignment());
	CHECK_EQ(77u, status.GetStatus(MODIFY_GUILDID));
}

TEST(AffectModifyInfo, AppliesEveryPacketClassDerivingFromModifyInfo)
{
	// The 29 packet classes that derive from ModifyInfo, each created by
	// its own factory: every one is drained the same way. This covers the
	// classes, not the handlers: 28 handlers pass their packet in, and
	// GCMakeItemFail's handler ignores the ModifyInfo it carries.
	std::vector<std::pair<std::string, std::unique_ptr<PacketFactory>>> factories;
	factories.emplace_back("GCAttackArmsOK1", std::make_unique<GCAttackArmsOK1Factory>());
	factories.emplace_back("GCAttackArmsOK2", std::make_unique<GCAttackArmsOK2Factory>());
	factories.emplace_back("GCAttackMeleeOK1", std::make_unique<GCAttackMeleeOK1Factory>());
	factories.emplace_back("GCAttackMeleeOK2", std::make_unique<GCAttackMeleeOK2Factory>());
	factories.emplace_back("GCBloodDrainOK1", std::make_unique<GCBloodDrainOK1Factory>());
	factories.emplace_back("GCBloodDrainOK2", std::make_unique<GCBloodDrainOK2Factory>());
	factories.emplace_back("GCCrossCounterOK1", std::make_unique<GCCrossCounterOK1Factory>());
	factories.emplace_back("GCCrossCounterOK2", std::make_unique<GCCrossCounterOK2Factory>());
	factories.emplace_back("GCKnocksTargetBackOK1", std::make_unique<GCKnocksTargetBackOK1Factory>());
	factories.emplace_back("GCKnocksTargetBackOK2", std::make_unique<GCKnocksTargetBackOK2Factory>());
	factories.emplace_back("GCMakeItemFail", std::make_unique<GCMakeItemFailFactory>());
	factories.emplace_back("GCMakeItemOK", std::make_unique<GCMakeItemOKFactory>());
	factories.emplace_back("GCMineExplosionOK1", std::make_unique<GCMineExplosionOK1Factory>());
	factories.emplace_back("GCModifyInformation", std::make_unique<GCModifyInformationFactory>());
	factories.emplace_back("GCOtherModifyInfo", std::make_unique<GCOtherModifyInfoFactory>());
	factories.emplace_back("GCSkillFailed1", std::make_unique<GCSkillFailed1Factory>());
	factories.emplace_back("GCSkillToInventoryOK1", std::make_unique<GCSkillToInventoryOK1Factory>());
	factories.emplace_back("GCSkillToObjectOK1", std::make_unique<GCSkillToObjectOK1Factory>());
	factories.emplace_back("GCSkillToObjectOK2", std::make_unique<GCSkillToObjectOK2Factory>());
	factories.emplace_back("GCSkillToObjectOK6", std::make_unique<GCSkillToObjectOK6Factory>());
	factories.emplace_back("GCSkillToSelfOK1", std::make_unique<GCSkillToSelfOK1Factory>());
	factories.emplace_back("GCSkillToTileOK1", std::make_unique<GCSkillToTileOK1Factory>());
	factories.emplace_back("GCSkillToTileOK2", std::make_unique<GCSkillToTileOK2Factory>());
	factories.emplace_back("GCSkillToTileOK6", std::make_unique<GCSkillToTileOK6Factory>());
	factories.emplace_back("GCThrowBombOK1", std::make_unique<GCThrowBombOK1Factory>());
	factories.emplace_back("GCThrowBombOK2", std::make_unique<GCThrowBombOK2Factory>());
	factories.emplace_back("GCThrowItemOK2", std::make_unique<GCThrowItemOK2Factory>());
	factories.emplace_back("GCUseBonusPointOK", std::make_unique<GCUseBonusPointOKFactory>());
	factories.emplace_back("GCUseOK", std::make_unique<GCUseOKFactory>());
	CHECK_EQ((size_t)29, factories.size());

	for (auto& named : factories)
	{
		std::unique_ptr<Packet> packet(named.second->createPacket());
		ModifyInfo* pInfo = dynamic_cast<ModifyInfo*>(packet.get());
		CHECK(pInfo != NULL);
		if (pInfo == NULL)
			continue;

		pInfo->addShortData(MODIFY_CURRENT_HP, 41);
		pInfo->addLongData(MODIFY_GOLD, 4200000000ul);

		BoundedStatus status;
		AffectModifyInfo(&status, pInfo);

		CHECK_EQ((size_t)2, status.calls.size());
		CHECK_EQ(41u, status.GetHP());
		CHECK_EQ(4200000000u, status.GetGold());
		CHECK_EQ(0, (int)pInfo->getShortCount());
		CHECK_EQ(0, (int)pInfo->getLongCount());
	}
}
