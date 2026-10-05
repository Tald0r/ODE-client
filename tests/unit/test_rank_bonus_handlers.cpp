#include "test_framework.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "RankBonusHandlerHost.h"
#include "RankBonusTable.h"
#include "RankBonusDef.h"
#include "TempInformation.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "Gpackets/GCRankBonusInfo.h"
#include "Gpackets/GCSelectRankBonusOK.h"

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct ScratchFile
{
	std::filesystem::path directory;
	std::filesystem::path path;

	ScratchFile()
	{
		// Reserve the directory atomically so independent test processes never
		// truncate one another's fixture, even if they start simultaneously.
		for (int i = 0; i < 1000; ++i)
		{
			auto candidate = std::filesystem::temp_directory_path()
				/ ("darkeden-rank-bonus-handlers-" + std::to_string(i));
			if (std::filesystem::create_directory(candidate))
			{
				directory = candidate;
				path = directory / "table.bin";
				return;
			}
		}
		throw std::runtime_error("cannot reserve rank-bonus fixture directory");
	}

	~ScratchFile()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
		std::filesystem::remove(directory, error);
	}
};

int s_RefreshCount = 0;
int s_AlternateRefreshCount = 0;
std::vector<RankBonusInfo::RANK_BONUS_STATUS> s_RefreshStatuses;
TempInformation::TEMP_MODE s_RefreshMode = TempInformation::MODE_NULL;

void Refresh()
{
	++s_RefreshCount;
	s_RefreshStatuses.clear();
	for (int i = 0; i < g_pRankBonusTable->GetSize(); ++i)
		s_RefreshStatuses.push_back((*g_pRankBonusTable)[i].GetStatus());
	s_RefreshMode = g_pTempInformation->GetMode();
}

struct Fixture
{
	RankBonusTable table;
	TempInformation temporary;
	RankBonusTable* previousTable = g_pRankBonusTable;
	TempInformation* previousTemporary = g_pTempInformation;
	RankBonusHandlers::Host host{.CheckRegen = Refresh};
	const RankBonusHandlers::Host* previousHost;

	Fixture() : previousHost(RankBonusHandlers::SetHost(&host))
	{
		g_pRankBonusTable = &table;
		g_pTempInformation = &temporary;
		s_RefreshCount = 0;
		s_AlternateRefreshCount = 0;
		s_RefreshStatuses.clear();
	}

	~Fixture()
	{
		RankBonusHandlers::SetHost(previousHost);
		g_pTempInformation = previousTemporary;
		g_pRankBonusTable = previousTable;
	}

	void LoadLevels(std::initializer_list<unsigned char> levels)
	{
		// Exercise the production loader; level has no public setter.
		const ScratchFile scratch;
		{
			std::ofstream out(scratch.path, std::ios::binary | std::ios::trunc);
			const int count = static_cast<int>(levels.size());
			out.write(reinterpret_cast<const char*>(&count), 4);
			unsigned short type = 0;
			for (unsigned char level : levels)
			{
				const int zero = 0;
				const unsigned char race = RACE_SLAYER;
				const unsigned short icon = 0;
				out.write(reinterpret_cast<const char*>(&type), 2);
				out.write(reinterpret_cast<const char*>(&zero), 4); // Empty name.
				out.write(reinterpret_cast<const char*>(&level), 1);
				out.write(reinterpret_cast<const char*>(&race), 1);
				out.write(reinterpret_cast<const char*>(&zero), 4); // Points.
				out.write(reinterpret_cast<const char*>(&icon), 2);
				++type;
			}
			CHECK(out.good());
		}
		{
			std::ifstream in(scratch.path, std::ios::binary);
			table.LoadFromFile(in);
			CHECK(in.good());
		}
		CHECK_EQ(levels.size(), static_cast<size_t>(table.GetSize()));
	}

	void Fill(RankBonusInfo::RANK_BONUS_STATUS status)
	{
		for (int i = 0; i < table.GetSize(); ++i)
			testfw::MutableRow(table, i).SetStatus(status);
	}
};

void AppendDWORD(std::vector<unsigned char>& bytes, DWORD value)
{
	for (unsigned int i = 0; i < 4; ++i)
		bytes.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

template<class PacketType>
void Read(PacketType& packet, const std::vector<unsigned char>& bytes)
{
	Socket socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&socket, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
	CHECK_EQ(bytes.size(), static_cast<size_t>(packet.getPacketSize()));
}

void ApplyList(std::initializer_list<DWORD> types)
{
	std::vector<unsigned char> bytes{static_cast<unsigned char>(types.size())};
	for (DWORD type : types) AppendDWORD(bytes, type);
	GCRankBonusInfo packet;
	Read(packet, bytes);
	GCRankBonusInfoHandler::execute(&packet, nullptr);
	CHECK_EQ(0, packet.getListNum());
}

void Select(DWORD type)
{
	std::vector<unsigned char> bytes;
	AppendDWORD(bytes, type);
	GCSelectRankBonusOK packet;
	Read(packet, bytes);
	GCSelectRankBonusOKHandler::execute(&packet, nullptr);
}
}

TEST(RankBonusHandlers, ListResetsStatusesAndExcludesOnlyContiguousPeers)
{
	Fixture f;
	f.LoadLevels({1, 2, 2, 2, 3, 2});
	f.Fill(RankBonusInfo::STATUS_LEARNED);
	ApplyList({2});
	CHECK_EQ(RankBonusInfo::STATUS_NULL, f.table[0].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_CANNOT_LEARN, f.table[1].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[2].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_CANNOT_LEARN, f.table[3].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_NULL, f.table[4].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_NULL, f.table[5].GetStatus());
	CHECK_EQ(1, s_RefreshCount);
	CHECK_EQ(6, s_RefreshStatuses.size());
	if (s_RefreshStatuses.size() != 6) return;
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, s_RefreshStatuses[2]);
	CHECK_EQ(RankBonusInfo::STATUS_CANNOT_LEARN, s_RefreshStatuses[3]);
}

TEST(RankBonusHandlers, ListPreservesWireOrderAndIgnoresOutOfRangeTypes)
{
	Fixture f;
	f.LoadLevels({1, 1, 1, 2});
	ApplyList({0, 4, (std::numeric_limits<DWORD>::max)(), 2, 3});
	CHECK_EQ(RankBonusInfo::STATUS_CANNOT_LEARN, f.table[0].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_CANNOT_LEARN, f.table[1].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[2].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[3].GetStatus());
	CHECK_EQ(1, s_RefreshCount);
}

TEST(RankBonusHandlers, EmptyListClearsStatusesAndRefreshesEvenAnEmptyTable)
{
	Fixture f;
	f.table.Init(2);
	f.Fill(RankBonusInfo::STATUS_LEARNED);
	ApplyList({});
	CHECK_EQ(RankBonusInfo::STATUS_NULL, f.table[0].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_NULL, f.table[1].GetStatus());
	f.table.Release();
	ApplyList({});
	CHECK_EQ(2, s_RefreshCount);
}

TEST(RankBonusHandlers, SelectionClearsRequestAndPreservesOtherLevels)
{
	Fixture f;
	f.LoadLevels({1, 2, 2, 2, 3, 2});
	f.Fill(RankBonusInfo::STATUS_LEARNED);
	f.temporary.SetMode(TempInformation::MODE_SKILL_LEARN);
	Select(2);
	CHECK_EQ(TempInformation::MODE_NULL, f.temporary.GetMode());
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[0].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_CANNOT_LEARN, f.table[1].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[2].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_CANNOT_LEARN, f.table[3].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[4].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[5].GetStatus());
	CHECK_EQ(0, s_RefreshCount);
}

TEST(RankBonusHandlers, SelectionRefreshesUranusAfterUpdatingState)
{
	Fixture f;
	f.table.Init(RANK_BONUS_MAX);
	f.temporary.SetMode(TempInformation::MODE_SKILL_LEARN);
	Select(RANK_BONUS_URANUS_BLESS);
	CHECK_EQ(1, s_RefreshCount);
	CHECK_EQ(TempInformation::MODE_NULL, s_RefreshMode);
	CHECK_EQ(RANK_BONUS_MAX, s_RefreshStatuses.size());
	if (s_RefreshStatuses.size() != RANK_BONUS_MAX) return;
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, s_RefreshStatuses[RANK_BONUS_URANUS_BLESS]);
	CHECK_EQ(RankBonusInfo::STATUS_CANNOT_LEARN, s_RefreshStatuses[0]);
	f.table.Release();
	Select(RANK_BONUS_URANUS_BLESS);
	CHECK_EQ(2, s_RefreshCount); // The callback is independent of table membership.
}

TEST(RankBonusHandlers, OutOfRangeSelectionStillClearsTheRequest)
{
	Fixture f;
	f.table.Init(2);
	f.Fill(RankBonusInfo::STATUS_LEARNED);
	for (DWORD type : {DWORD(2), DWORD((std::numeric_limits<int>::max)())})
	{
		f.temporary.SetMode(TempInformation::MODE_SKILL_LEARN);
		Select(type);
		CHECK_EQ(TempInformation::MODE_NULL, f.temporary.GetMode());
		CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[0].GetStatus());
		CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[1].GetStatus());
	}
	CHECK_EQ(0, s_RefreshCount);
}

TEST(RankBonusHandlers, MissingHostAndMissingCallbackStillApplyModelChanges)
{
	Fixture f;
	f.table.Init(RANK_BONUS_MAX);
	CHECK(RankBonusHandlers::SetHost(nullptr) == &f.host);
	ApplyList({0});
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[0].GetStatus());
	Select(RANK_BONUS_URANUS_BLESS);
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[RANK_BONUS_URANUS_BLESS].GetStatus());
	const RankBonusHandlers::Host empty{};
	CHECK(RankBonusHandlers::SetHost(&empty) == nullptr);
	ApplyList({1});
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[1].GetStatus());
	Select(RANK_BONUS_URANUS_BLESS);
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[RANK_BONUS_URANUS_BLESS].GetStatus());
	CHECK_EQ(0, s_RefreshCount);
	CHECK(RankBonusHandlers::SetHost(nullptr) == &empty);
}

TEST(RankBonusHandlers, ReplacementAndRestorationAreObservedOnEveryPacket)
{
	Fixture f;
	f.table.Init(1);
	const RankBonusHandlers::Host alternate{
		.CheckRegen = [] { ++s_AlternateRefreshCount; },
	};
	const auto* previous = RankBonusHandlers::SetHost(&alternate);
	CHECK(previous == &f.host);
	ApplyList({0});
	CHECK_EQ(1, s_AlternateRefreshCount);
	CHECK_EQ(0, s_RefreshCount);
	CHECK(RankBonusHandlers::SetHost(previous) == &alternate);
	ApplyList({0});
	CHECK_EQ(1, s_AlternateRefreshCount);
	CHECK_EQ(1, s_RefreshCount);
}

// UINT32_MAX used to narrow to -1 before the range check. The fallback
// row's level is zero, so the forward peer walk changed real level-zero
// rows even though the selected wire ID is outside the table.
TEST(RankBonusHandlers, InvalidUnsignedSelectionPreservesEveryStatus)
{
	Fixture f;
	f.table.Init(2);
	const DWORD signedMaximum = static_cast<DWORD>((std::numeric_limits<int>::max)());
	const DWORD unsignedMaximum = (std::numeric_limits<DWORD>::max)();
	for (DWORD type : {DWORD(2), signedMaximum, signedMaximum + DWORD(1),
		signedMaximum + DWORD(2), unsignedMaximum - DWORD(1), unsignedMaximum})
	{
		f.Fill(RankBonusInfo::STATUS_LEARNED);
		f.temporary.SetMode(TempInformation::MODE_SKILL_LEARN);
		Select(type);
		CHECK_EQ(TempInformation::MODE_NULL, f.temporary.GetMode());
		CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[0].GetStatus());
		CHECK_EQ(RankBonusInfo::STATUS_LEARNED, f.table[1].GetStatus());
		CHECK_EQ(0, s_RefreshCount);
	}
}
