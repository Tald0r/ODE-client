//----------------------------------------------------------------------
// test_skill_info.cpp
//----------------------------------------------------------------------
//
// ApplySkillInfo (docs/RESTRUCTURING.md task 4.16) is the skill-model
// rebuild GCSkillInfoHandler runs: it resets the user's skill flags,
// rebuilds every domain's tree with nothing learned, and learns and
// sets up each skill the packet lists, per race. Each test here reads
// a GCSkillInfo from wire bytes into the packet the real factory
// creates, runs ApplySkillInfo on it and checks the model it leaves:
// which skills each domain holds as learned, what the info table
// carries for them, which domains say a new skill can be learned, the
// domain levels, the skills usable now (g_pSkillAvailable, the set the
// learns add to) and the five flags. test_bonus_skill_handlers.cpp covers
// the complete handler, including sweeper reset and the host callback for
// live availability refresh.
//
// The wire bytes are built field by field from the packet types'
// widths, little-endian, as the server writes them (its Slayer.cpp,
// Vampire.cpp and Ousters.cpp send the three races' lists), and every
// read must consume them exactly. Three tests read the server's own
// five goldens for this packet (tests/golden/GCSkillInfo.*.hex in the
// server repository, byte-identical here as hex strings). The last
// four tests pin the two fixes task 4.16 made: a skill type past the
// info table is skipped, and a duration's milliseconds are a DWORD.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "MSkillManager.h"
#include "SkillDef.h"
#include "ApplySkillInfo.h"
#include "ConvertDuration.h"

#include "SocketInputStream.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "Gpackets/GCSkillInfo.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

//----------------------------------------------------------------------
// Wire bytes
//----------------------------------------------------------------------
class Wire
{
public:
	// One field, little-endian, as wide as its type.
	template <class T>
	Wire& Put(T value)
	{
		const std::uint64_t v = (std::uint64_t)value;
		for (size_t i = 0; i < sizeof(T); ++i)
			m_Bytes.push_back((unsigned char)(v >> (8 * i)));
		return *this;
	}

	// Bytes given as hex text, as the golden files hold them.
	Wire& Hex(const char* hex)
	{
		for (const char* p = hex; p[0] != '\0' && p[1] != '\0'; p += 2)
		{
			const std::string byte(p, 2);
			m_Bytes.push_back((unsigned char)std::stoul(byte, NULL, 16));
		}
		return *this;
	}

	const std::vector<unsigned char>&	Bytes() const	{ return m_Bytes; }

private:
	std::vector<unsigned char>	m_Bytes;
};

struct StreamFixture
{
	Socket				m_Socket;
	SocketInputStream	m_Stream;

	StreamFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 1024)
	{
	}
};

// The packet the factory creates, read from the bytes, which it must
// consume exactly and which must be the size the packet reports.
std::unique_ptr<GCSkillInfo> ReadSkillInfo(const Wire& wire)
{
	GCSkillInfoFactory factory;
	std::unique_ptr<Packet> packet(factory.createPacket());
	GCSkillInfo* p = dynamic_cast<GCSkillInfo*>(packet.get());
	CHECK(p != NULL);
	if (p == NULL)
		return std::unique_ptr<GCSkillInfo>();
	packet.release();
	std::unique_ptr<GCSkillInfo> owned(p);

	StreamFixture f;
	SocketInputStreamTestAccess::Preload(f.m_Stream, wire.Bytes().data(), (unsigned int)wire.Bytes().size());
	owned->read(f.m_Stream);
	CHECK(f.m_Stream.isEmpty());
	CHECK_EQ(wire.Bytes().size(), (size_t)owned->getPacketSize());
	return owned;
}

// Reads the packet and applies it; the packet must come back drained.
void	Apply(const Wire& wire)
{
	std::unique_ptr<GCSkillInfo> p = ReadSkillInfo(wire);
	CHECK(p != NULL);
	if (p == NULL)
		return;
	ApplySkillInfo(p.get());
	CHECK_EQ(0, p->getListNum());
}

//----------------------------------------------------------------------
// The three races' lists, as the server writes them
//----------------------------------------------------------------------
struct SlayerSkill
{
	int		type;
	DWORD	exp;
	int		expLevel;
	DWORD	turn;
	DWORD	castingTime;
	bool	enable;
};

struct SlayerDomain
{
	bool						learnNew;
	int							domain;
	std::vector<SlayerSkill>	skills;
};

Wire	SlayerBytes(const std::vector<SlayerDomain>& domains)
{
	Wire w;
	w.Put<BYTE>(PC_SLAYER).Put<BYTE>((BYTE)domains.size());
	for (size_t d = 0; d < domains.size(); ++d)
	{
		w.Put<BYTE>(domains[d].learnNew ? 1 : 0)
		 .Put<SkillDomainType_t>((SkillDomainType_t)domains[d].domain)
		 .Put<BYTE>((BYTE)domains[d].skills.size());
		for (size_t i = 0; i < domains[d].skills.size(); ++i)
		{
			const SlayerSkill& s = domains[d].skills[i];
			w.Put<SkillType_t>((SkillType_t)s.type).Put<Exp_t>(s.exp)
			 .Put<ExpLevel_t>((ExpLevel_t)s.expLevel).Put<Turn_t>(s.turn)
			 .Put<Turn_t>(s.castingTime).Put<BYTE>(s.enable ? 1 : 0);
		}
	}
	return w;
}

struct VampireSkill
{
	int		type;
	DWORD	turn;
	DWORD	castingTime;
};

// The vampire's one entry.
Wire	VampireBytes(bool learnNew, const std::vector<VampireSkill>& skills)
{
	Wire w;
	w.Put<BYTE>(PC_VAMPIRE).Put<BYTE>(1)
	 .Put<BYTE>(learnNew ? 1 : 0).Put<BYTE>((BYTE)skills.size());
	for (size_t i = 0; i < skills.size(); ++i)
		w.Put<SkillType_t>((SkillType_t)skills[i].type).Put<Turn_t>(skills[i].turn).Put<Turn_t>(skills[i].castingTime);
	return w;
}

struct OustersSkill
{
	int		type;
	int		expLevel;
	DWORD	turn;
	DWORD	castingTime;
};

// The ousters' one entry.
Wire	OustersBytes(bool learnNew, const std::vector<OustersSkill>& skills)
{
	Wire w;
	w.Put<BYTE>(PC_OUSTERS).Put<BYTE>(1)
	 .Put<BYTE>(learnNew ? 1 : 0).Put<BYTE>((BYTE)skills.size());
	for (size_t i = 0; i < skills.size(); ++i)
		w.Put<SkillType_t>((SkillType_t)skills[i].type).Put<ExpLevel_t>((ExpLevel_t)skills[i].expLevel)
		 .Put<Turn_t>(skills[i].turn).Put<Turn_t>(skills[i].castingTime);
	return w;
}

//----------------------------------------------------------------------
// The skill model the rebuild reaches, created as GameInit creates it
//----------------------------------------------------------------------
MonotonicClock::TimePoint	s_Now;
DWORD	s_Frame = 0;

int		DropFrameCount(TYPE_FRAMEID)	{ return 0; }
void	RefreshAffect(MItem*)			{}
void	PlayItemSound(TYPE_SOUNDID)		{}

// The clock the use delays run on is all the skill model wants from a host.
const MItemHost	s_Host = { &s_Frame, DropFrameCount, RefreshAffect, PlayItemSound, &s_Now,
							NULL, NULL, NULL, NULL, NULL, NULL };

// The skills each domain's tree holds here, one level below its root.
const ACTIONINFO	kVampireSkills[] = { MAGIC_GROUND_ATTACK, MAGIC_BLOODY_SNAKE, MAGIC_BLOODY_WARP, MAGIC_HALLUCINATION };
const ACTIONINFO	kBombs[] = { BOMB_SPLINTER, BOMB_ACER, BOMB_BULLS, BOMB_STUN, BOMB_CROSSBOW };
const ACTIONINFO	kMines[] = { MINE_ANKLE_KILLER, MINE_POMZ, MINE_AP_C1, MINE_DIAMONDBACK, MINE_SWIFT_EX };

// The domain levels the packet does not carry, set before each test so
// that a rebuild that touched them would show.
int	PresetDomainLevel(int domain)	{ return 10 + domain; }

struct SkillInfoWorld : GameModelWorld
{
	MSkillInfoTable*	m_pPrevInfoTable;
	MSkillManager*		m_pPrevManager;
	MSkillSet*			m_pPrevAvailable;

	SkillInfoWorld()
	: m_pPrevInfoTable(g_pSkillInfoTable), m_pPrevManager(g_pSkillManager),
	  m_pPrevAvailable(g_pSkillAvailable)
	{
		s_Now = MonotonicClock::FromMillis(100000);
		MItem::SetHost(&s_Host);

		g_pSkillInfoTable = new MSkillInfoTable;
		g_pSkillAvailable = new MSkillSet;

		// The roots InitSkillList starts each domain from, at level 0.
		Row(SKILL_SINGLE_BLOW, 0, SKILL_STEP_APPRENTICE);
		Row(SKILL_DOUBLE_IMPACT, 0, SKILL_STEP_APPRENTICE);
		Row(SKILL_FAST_RELOAD, 0, SKILL_STEP_APPRENTICE);
		Row(MAGIC_CREATE_HOLY_WATER, 0, SKILL_STEP_APPRENTICE);
		Row(MAGIC_CURE_LIGHT_WOUNDS, 0, SKILL_STEP_APPRENTICE);
		Row(MAGIC_HIDE, 0, SKILL_STEP_VAMPIRE_INNATE);
		Row(SKILL_FLOURISH, 0, SKILL_STEP_OUSTERS_COMBAT);
		// The ETC domain's root is the one skill of the ETC step here.
		Row(SKILL_SOUL_CHAIN, 0, SKILL_STEP_ETC);

		// One level down: Restore under healing, the bomb and the mine
		// under guns, the vampire's four flag skills under Hide.
		Row(MAGIC_RESTORE, 1, SKILL_STEP_ADEPT);
		testfw::MutableRow(*g_pSkillInfoTable, MAGIC_CURE_LIGHT_WOUNDS).AddNextSkill(MAGIC_RESTORE);
		Row(SKILL_THROW_BOMB, 1, SKILL_STEP_ADEPT);
		Row(SKILL_INSTALL_MINE, 1, SKILL_STEP_ADEPT);
		testfw::MutableRow(*g_pSkillInfoTable, SKILL_FAST_RELOAD).AddNextSkill(SKILL_THROW_BOMB);
		testfw::MutableRow(*g_pSkillInfoTable, SKILL_FAST_RELOAD).AddNextSkill(SKILL_INSTALL_MINE);
		for (size_t i = 0; i < sizeof(kVampireSkills) / sizeof(kVampireSkills[0]); ++i)
		{
			Row(kVampireSkills[i], 1, SKILL_STEP_VAMPIRE_POISON);
			testfw::MutableRow(*g_pSkillInfoTable, MAGIC_HIDE).AddNextSkill(kVampireSkills[i]);
		}

		// The bombs and mines are in no tree; only their levels are set.
		for (int i = 0; i < 5; ++i)
		{
			Row(kBombs[i], 0, SKILL_STEP_NULL);
			Row(kMines[i], 0, SKILL_STEP_NULL);
		}

		g_pSkillManager = new MSkillManager;
		g_pSkillManager->Init();

		for (int d = 0; d < MAX_SKILLDOMAIN; ++d)
			testfw::MutableRow(*g_pSkillManager, d).SetDomainLevel(PresetDomainLevel(d));
	}

	~SkillInfoWorld()
	{
		delete g_pSkillManager;		g_pSkillManager = m_pPrevManager;
		delete g_pSkillAvailable;	g_pSkillAvailable = m_pPrevAvailable;
		delete g_pSkillInfoTable;	g_pSkillInfoTable = m_pPrevInfoTable;
	}

	static void	Row(ACTIONINFO id, int level, SKILL_STEP step)
	{
		SKILLINFO_NODE& row = testfw::MutableRow(*g_pSkillInfoTable, id);
		row.Set(level, "skill", 0, 0, 0, "skill");
		row.SetSkillStep(step);
	}
};

const MSkillDomain&		Domain(int domain)	{ return (*g_pSkillManager)[domain]; }
const SKILLINFO_NODE&	Info(int id)		{ return (*g_pSkillInfoTable)[id]; }

bool	IsLearned(int domain, ACTIONINFO id)
{
	return Domain(domain).GetSkillStatus(id) == MSkillDomain::SKILLSTATUS_LEARNED;
}

bool	IsUsable(ACTIONINFO id)
{
	return g_pSkillAvailable->find(id) != g_pSkillAvailable->end();
}

// How many skills the domain holds as learned.
int		LearnedCount(int domain)
{
	MSkillDomain& d = testfw::MutableRow(*g_pSkillManager, domain);
	int n = 0;
	for (d.SetBegin(); d.IsNotEnd(); d.Next())
	{
		if (d.GetSkillStatus() == MSkillDomain::SKILLSTATUS_LEARNED)
			++n;
	}
	return n;
}

int		LearnedTotal()
{
	int n = 0;
	for (int d = 0; d < MAX_SKILLDOMAIN; ++d)
		n += LearnedCount(d);
	return n;
}

void	CheckDomainLevelsUntouched()
{
	for (int d = 0; d < MAX_SKILLDOMAIN; ++d)
		CHECK_EQ(PresetDomainLevel(d), Domain(d).GetDomainLevel());
}

void	SetAllFlags()
{
	g_pUserInformation->HasSkillRestore = true;
	g_pUserInformation->HasMagicGroundAttack = true;
	g_pUserInformation->HasMagicHallu = true;
	g_pUserInformation->HasMagicBloodyWarp = true;
	g_pUserInformation->HasMagicBloodySnake = true;
}

void	CheckNoFlags()
{
	CHECK_EQ(false, g_pUserInformation->HasSkillRestore);
	CHECK_EQ(false, g_pUserInformation->HasMagicGroundAttack);
	CHECK_EQ(false, g_pUserInformation->HasMagicHallu);
	CHECK_EQ(false, g_pUserInformation->HasMagicBloodyWarp);
	CHECK_EQ(false, g_pUserInformation->HasMagicBloodySnake);
}

// Nothing learned and no domain offering a new skill.
void	CheckNothingLearned()
{
	CHECK_EQ(0, LearnedTotal());
	CHECK_EQ(0, (int)g_pSkillAvailable->size());
	for (int d = 0; d < MAX_SKILLDOMAIN; ++d)
		CHECK_EQ(false, Domain(d).HasNewSkill());
}

} // namespace

//----------------------------------------------------------------------
// The conversions the rebuild reads its durations through
//----------------------------------------------------------------------
TEST(ConvertDuration, TenthsOfASecondBecomeMillisecondsAndFrames)
{
	CHECK_EQ(0u, ConvertDurationToMillisecond(0));
	CHECK_EQ(100u, ConvertDurationToMillisecond(1));
	CHECK_EQ(3000u, ConvertDurationToMillisecond(30));
	CHECK_EQ(6553500u, ConvertDurationToMillisecond(65535));

	GameModelWorld world;
	g_pClientConfig->FPS = 16;
	CHECK_EQ(0u, ConvertDurationToFrame(0));
	CHECK_EQ(16u, ConvertDurationToFrame(10));	// a second
	CHECK_EQ(1u, ConvertDurationToFrame(1));	// 1.6 frames, truncated
	CHECK_EQ(48u, ConvertDurationToFrame(30));
	g_pClientConfig->FPS = 30;
	CHECK_EQ(30u, ConvertDurationToFrame(10));
}

//----------------------------------------------------------------------
// Slayer
//----------------------------------------------------------------------
TEST(ApplySkillInfo, SlayerLearnsEachDomainsSkillsAndCarriesTheirState)
{
	SkillInfoWorld world;

	std::vector<SlayerDomain> domains(3);
	domains[0].learnNew = false;
	domains[0].domain = SKILL_DOMAIN_BLADE;
	domains[0].skills.push_back(SlayerSkill{ SKILL_SINGLE_BLOW, 1234, 17, 30, 5, true });
	domains[1].learnNew = true;
	domains[1].domain = SKILL_DOMAIN_HEAL;
	domains[1].skills.push_back(SlayerSkill{ MAGIC_CURE_LIGHT_WOUNDS, 10, 2, 0, 0, true });
	domains[1].skills.push_back(SlayerSkill{ MAGIC_RESTORE, 99, 3, 600, 0, false });
	domains[2].learnNew = false;
	domains[2].domain = SKILL_DOMAIN_GUN;
	domains[2].skills.push_back(SlayerSkill{ SKILL_FAST_RELOAD, 5, 1, 17, 0, true });

	Apply(SlayerBytes(domains));

	// Learned, in its own domain only, and usable.
	CHECK(IsLearned(SKILLDOMAIN_BLADE, SKILL_SINGLE_BLOW));
	CHECK(IsLearned(SKILLDOMAIN_HEAL, MAGIC_CURE_LIGHT_WOUNDS));
	CHECK(IsLearned(SKILLDOMAIN_HEAL, MAGIC_RESTORE));
	CHECK(IsLearned(SKILLDOMAIN_GUN, SKILL_FAST_RELOAD));
	CHECK_EQ(4, LearnedTotal());
	CHECK(IsUsable(SKILL_SINGLE_BLOW));
	CHECK(IsUsable(MAGIC_RESTORE));
	CHECK_EQ(4, (int)g_pSkillAvailable->size());

	// What learning the root unlocked is learnable next; the untouched
	// roots stay learnable too.
	CHECK_EQ(MSkillDomain::SKILLSTATUS_NEXT, Domain(SKILLDOMAIN_GUN).GetSkillStatus(SKILL_THROW_BOMB));
	CHECK_EQ(MSkillDomain::SKILLSTATUS_NEXT, Domain(SKILLDOMAIN_SWORD).GetSkillStatus(SKILL_DOUBLE_IMPACT));

	// The wire's state for each skill: exp level, exp, the reuse delay
	// (under 1.8 s reads as none), enable and the delay still to run.
	CHECK_EQ(17, Info(SKILL_SINGLE_BLOW).GetExpLevel());
	CHECK_EQ(1234, Info(SKILL_SINGLE_BLOW).GetSkillExp());
	CHECK_EQ(3000u, Info(SKILL_SINGLE_BLOW).GetDelayTime());
	CHECK_EQ(true, Info(SKILL_SINGLE_BLOW).IsEnable());
	CHECK_EQ(500u, Info(SKILL_SINGLE_BLOW).GetAvailableTimeLeft());
	CHECK_EQ(3, Info(MAGIC_RESTORE).GetExpLevel());
	CHECK_EQ(99, Info(MAGIC_RESTORE).GetSkillExp());
	CHECK_EQ(60000u, Info(MAGIC_RESTORE).GetDelayTime());
	CHECK_EQ(false, Info(MAGIC_RESTORE).IsEnable());
	CHECK_EQ(0u, Info(MAGIC_RESTORE).GetAvailableTimeLeft());
	CHECK_EQ(0u, Info(SKILL_FAST_RELOAD).GetDelayTime());		// 1.7 s

	// Only the entry that said so offers a new skill: each learn clears
	// the flag it set to be allowed to learn.
	CHECK_EQ(false, Domain(SKILLDOMAIN_BLADE).HasNewSkill());
	CHECK_EQ(true, Domain(SKILLDOMAIN_HEAL).HasNewSkill());
	CHECK_EQ(false, Domain(SKILLDOMAIN_GUN).HasNewSkill());
	CHECK_EQ(false, Domain(SKILLDOMAIN_SWORD).HasNewSkill());

	// Restore sets its flag; the others are the vampire's.
	CHECK_EQ(true, g_pUserInformation->HasSkillRestore);
	CHECK_EQ(false, g_pUserInformation->HasMagicGroundAttack);
	CHECK_EQ(false, g_pUserInformation->HasMagicBloodySnake);

	// GCSkillInfo carries no domain level.
	CheckDomainLevelsUntouched();
}

TEST(ApplySkillInfo, SlayerBombAndMinePassTheirLevelToEveryBombAndMine)
{
	SkillInfoWorld world;

	std::vector<SlayerDomain> domains(1);
	domains[0].learnNew = false;
	domains[0].domain = SKILL_DOMAIN_GUN;
	domains[0].skills.push_back(SlayerSkill{ SKILL_FAST_RELOAD, 0, 0, 0, 0, true });
	domains[0].skills.push_back(SlayerSkill{ SKILL_THROW_BOMB, 0, 40, 0, 0, true });
	domains[0].skills.push_back(SlayerSkill{ SKILL_INSTALL_MINE, 0, 55, 0, 0, true });

	Apply(SlayerBytes(domains));

	CHECK(IsLearned(SKILLDOMAIN_GUN, SKILL_THROW_BOMB));
	CHECK(IsLearned(SKILLDOMAIN_GUN, SKILL_INSTALL_MINE));
	for (int i = 0; i < 5; ++i)
	{
		CHECK_EQ(40, Info(kBombs[i]).GetExpLevel());
		CHECK_EQ(55, Info(kMines[i]).GetExpLevel());
	}
	// Only their levels: they are not learned, and nothing else of theirs moves.
	CHECK_EQ(false, IsUsable(BOMB_SPLINTER));
	CHECK_EQ(0, Info(BOMB_SPLINTER).GetSkillExp());
}

TEST(ApplySkillInfo, EtcSkillIsLearnedInEveryDomainThatHoldsIt)
{
	SkillInfoWorld world;

	// Soul Chain is of the ETC step: the rebuild tries every domain but
	// the ousters' for a slayer. Only the ETC domain's tree holds it here.
	std::vector<SlayerDomain> domains(1);
	domains[0].learnNew = false;
	domains[0].domain = SKILL_DOMAIN_ETC;
	domains[0].skills.push_back(SlayerSkill{ SKILL_SOUL_CHAIN, 0, 9, 0, 0, true });

	Apply(SlayerBytes(domains));

	CHECK(IsLearned(SKILLDOMAIN_ETC, SKILL_SOUL_CHAIN));
	CHECK_EQ(1, LearnedTotal());
	CHECK_EQ(9, Info(SKILL_SOUL_CHAIN).GetExpLevel());
	CHECK_EQ(false, Domain(SKILLDOMAIN_ETC).HasNewSkill());

	// Known, not fixed (task 4.16): every other domain tried refuses the
	// learn, which leaves the new-skill flag the rebuild set to allow it
	// - the skill window then offers the domain's next skill though the
	// server did not say one can be learned. The ousters' is not tried.
	CHECK_EQ(true, Domain(SKILLDOMAIN_BLADE).HasNewSkill());
	CHECK_EQ(true, Domain(SKILLDOMAIN_SWORD).HasNewSkill());
	CHECK_EQ(true, Domain(SKILLDOMAIN_GUN).HasNewSkill());
	CHECK_EQ(true, Domain(SKILLDOMAIN_HEAL).HasNewSkill());
	CHECK_EQ(true, Domain(SKILLDOMAIN_ENCHANT).HasNewSkill());
	CHECK_EQ(true, Domain(SKILLDOMAIN_VAMPIRE).HasNewSkill());
	CHECK_EQ(false, Domain(SKILLDOMAIN_OUSTERS).HasNewSkill());
}

//----------------------------------------------------------------------
// Vampire
//----------------------------------------------------------------------
TEST(ApplySkillInfo, VampireLearnsIntoItsDomainAndSetsTheThreeFlags)
{
	SkillInfoWorld world;

	// The vampire list carries no exp level: a level set before stays.
	testfw::MutableRow(*g_pSkillInfoTable, MAGIC_GROUND_ATTACK).SetExpLevel(77);

	std::vector<VampireSkill> skills;
	skills.push_back(VampireSkill{ MAGIC_HIDE, 20, 0 });
	skills.push_back(VampireSkill{ MAGIC_GROUND_ATTACK, 45, 12 });
	skills.push_back(VampireSkill{ MAGIC_BLOODY_SNAKE, 0, 0 });
	skills.push_back(VampireSkill{ MAGIC_BLOODY_WARP, 0, 0 });
	skills.push_back(VampireSkill{ MAGIC_HALLUCINATION, 0, 0 });

	Apply(VampireBytes(true, skills));

	CHECK(IsLearned(SKILLDOMAIN_VAMPIRE, MAGIC_HIDE));
	for (size_t i = 0; i < sizeof(kVampireSkills) / sizeof(kVampireSkills[0]); ++i)
	{
		CHECK(IsLearned(SKILLDOMAIN_VAMPIRE, kVampireSkills[i]));
		CHECK(IsUsable(kVampireSkills[i]));
	}
	CHECK_EQ(5, LearnedTotal());

	CHECK_EQ(4500u, Info(MAGIC_GROUND_ATTACK).GetDelayTime());
	CHECK_EQ(1200u, Info(MAGIC_GROUND_ATTACK).GetAvailableTimeLeft());
	CHECK_EQ(2000u, Info(MAGIC_HIDE).GetDelayTime());
	CHECK_EQ(77, Info(MAGIC_GROUND_ATTACK).GetExpLevel());

	CHECK_EQ(true, Domain(SKILLDOMAIN_VAMPIRE).HasNewSkill());

	// Ground Attack, Bloody Snake and Bloody Warp set theirs;
	// Hallucination's case is commented out, and Restore is the slayer's.
	CHECK_EQ(true, g_pUserInformation->HasMagicGroundAttack);
	CHECK_EQ(true, g_pUserInformation->HasMagicBloodySnake);
	CHECK_EQ(true, g_pUserInformation->HasMagicBloodyWarp);
	CHECK_EQ(false, g_pUserInformation->HasMagicHallu);
	CHECK_EQ(false, g_pUserInformation->HasSkillRestore);

	CheckDomainLevelsUntouched();
}

//----------------------------------------------------------------------
// Ousters
//----------------------------------------------------------------------
TEST(ApplySkillInfo, OustersLearnsIntoItsDomainWithItsLevel)
{
	SkillInfoWorld world;

	std::vector<OustersSkill> skills;
	skills.push_back(OustersSkill{ SKILL_FLOURISH, 25, 18, 3 });

	Apply(OustersBytes(false, skills));

	CHECK(IsLearned(SKILLDOMAIN_OUSTERS, SKILL_FLOURISH));
	CHECK(IsUsable(SKILL_FLOURISH));
	CHECK_EQ(1, LearnedTotal());
	CHECK_EQ(25, Info(SKILL_FLOURISH).GetExpLevel());
	CHECK_EQ(1800u, Info(SKILL_FLOURISH).GetDelayTime());
	CHECK_EQ(300u, Info(SKILL_FLOURISH).GetAvailableTimeLeft());
	CHECK_EQ(false, Domain(SKILLDOMAIN_OUSTERS).HasNewSkill());
	CheckNoFlags();
	CheckDomainLevelsUntouched();
}

TEST(ApplySkillInfo, OustersEtcSkillTriesTheOustersDomainToo)
{
	SkillInfoWorld world;

	// The ousters' ETC loop adds its own domain; still only the ETC
	// domain holds Soul Chain here, so the ousters' flag is left set.
	std::vector<OustersSkill> skills;
	skills.push_back(OustersSkill{ SKILL_SOUL_CHAIN, 4, 0, 0 });

	Apply(OustersBytes(false, skills));

	CHECK(IsLearned(SKILLDOMAIN_ETC, SKILL_SOUL_CHAIN));
	CHECK_EQ(4, Info(SKILL_SOUL_CHAIN).GetExpLevel());
	CHECK_EQ(true, Domain(SKILLDOMAIN_OUSTERS).HasNewSkill());
	CHECK_EQ(false, Domain(SKILLDOMAIN_ETC).HasNewSkill());
}

//----------------------------------------------------------------------
// Every packet starts from scratch
//----------------------------------------------------------------------
TEST(ApplySkillInfo, EachPacketClearsTheFlagsAndWhatWasLearnedBefore)
{
	SkillInfoWorld world;

	std::vector<VampireSkill> skills;
	skills.push_back(VampireSkill{ MAGIC_HIDE, 0, 0 });
	skills.push_back(VampireSkill{ MAGIC_BLOODY_WARP, 0, 0 });
	Apply(VampireBytes(false, skills));
	CHECK(IsLearned(SKILLDOMAIN_VAMPIRE, MAGIC_BLOODY_WARP));
	CHECK_EQ(true, g_pUserInformation->HasMagicBloodyWarp);

	SetAllFlags();

	// The server's golden for a slayer with no domain at all.
	Apply(Wire().Hex("0000"));

	CheckNoFlags();
	CheckNothingLearned();
	CHECK_EQ(MSkillDomain::SKILLSTATUS_NEXT, Domain(SKILLDOMAIN_VAMPIRE).GetSkillStatus(MAGIC_HIDE));
	CHECK_EQ(MSkillDomain::SKILLSTATUS_OTHER, Domain(SKILLDOMAIN_VAMPIRE).GetSkillStatus(MAGIC_BLOODY_WARP));
	CheckDomainLevelsUntouched();
}

TEST(ApplySkillInfo, AnEmptyListLearnsNothing)
{
	SkillInfoWorld world;

	Apply(VampireBytes(false, std::vector<VampireSkill>()));
	CheckNothingLearned();
	Apply(OustersBytes(false, std::vector<OustersSkill>()));
	CheckNothingLearned();

	// The server's golden for a slayer whose one domain entry lists no
	// skill (learn-new set, domain 0xC3, past the domain table).
	SetAllFlags();
	Apply(Wire().Hex("000101c300"));
	CheckNothingLearned();
	CheckNoFlags();
}

TEST(ApplySkillInfo, NewSkillMarkOutlivesTheNextPacket)
{
	SkillInfoWorld world;

	Apply(OustersBytes(true, std::vector<OustersSkill>()));
	CHECK_EQ(true, Domain(SKILLDOMAIN_OUSTERS).HasNewSkill());

	// Known, not fixed (task 4.16): the rebuild resets the trees but not
	// a domain's new-skill flag, which only a learn clears, so a packet
	// that no longer offers a new skill leaves the earlier offer up.
	Apply(OustersBytes(false, std::vector<OustersSkill>()));
	CHECK_EQ(true, Domain(SKILLDOMAIN_OUSTERS).HasNewSkill());
}

//----------------------------------------------------------------------
// The wire's hostile values
//----------------------------------------------------------------------
TEST(ApplySkillInfo, SlayerDomainPastTheDomainTableLearnsNothing)
{
	SkillInfoWorld world;

	// The server sends a slayer's domains 0 to SKILL_DOMAIN_ETC; the
	// domain table has MAX_SKILLDOMAIN rows, and past them the lookup
	// answers nothing.
	const int kDomains[] = { MAX_SKILLDOMAIN, 0x91, 0xB2, 0xFF };
	for (size_t k = 0; k < sizeof(kDomains) / sizeof(kDomains[0]); ++k)
	{
		std::vector<SlayerDomain> domains(1);
		domains[0].learnNew = true;
		domains[0].domain = kDomains[k];
		domains[0].skills.push_back(SlayerSkill{ SKILL_SINGLE_BLOW, 1, 2, 30, 0, true });
		domains[0].skills.push_back(SlayerSkill{ MAGIC_RESTORE, 1, 6, 0, 0, true });

		Apply(SlayerBytes(domains));

		CheckNothingLearned();
		// The info table entries are still set, and Restore's flag too.
		CHECK_EQ(2, Info(SKILL_SINGLE_BLOW).GetExpLevel());
		CHECK_EQ(6, Info(MAGIC_RESTORE).GetExpLevel());
		CHECK_EQ(true, g_pUserInformation->HasSkillRestore);
		CheckDomainLevelsUntouched();
	}
}

TEST(ApplySkillInfo, SlayerDomainOfAnotherRaceLearnsThere)
{
	SkillInfoWorld world;

	// Domains the server never sends a slayer are still in the table.
	std::vector<SlayerDomain> domains(1);
	domains[0].learnNew = false;
	domains[0].domain = SKILL_DOMAIN_VAMPIRE;
	domains[0].skills.push_back(SlayerSkill{ MAGIC_HIDE, 0, 0, 0, 0, true });

	Apply(SlayerBytes(domains));

	CHECK(IsLearned(SKILLDOMAIN_VAMPIRE, MAGIC_HIDE));
}

TEST(ApplySkillInfo, ExpLevelPastAnyTableIsStoredAsSent)
{
	SkillInfoWorld world;

	// No table is indexed by the level here; every value is stored.
	std::vector<SlayerDomain> domains(1);
	domains[0].learnNew = false;
	domains[0].domain = SKILL_DOMAIN_GUN;
	domains[0].skills.push_back(SlayerSkill{ SKILL_FAST_RELOAD, 0xFFFFFFFFu, 0xFFFF, 0, 0, true });
	domains[0].skills.push_back(SlayerSkill{ SKILL_THROW_BOMB, 0, 0x8000, 0, 0, true });
	Apply(SlayerBytes(domains));

	CHECK_EQ(0xFFFF, Info(SKILL_FAST_RELOAD).GetExpLevel());
	CHECK_EQ(-1, Info(SKILL_FAST_RELOAD).GetSkillExp());	// Exp_t through an int
	CHECK_EQ(0x8000, Info(BOMB_CROSSBOW).GetExpLevel());

	std::vector<OustersSkill> skills;
	skills.push_back(OustersSkill{ SKILL_FLOURISH, 0xFFFF, 0, 0 });
	Apply(OustersBytes(false, skills));
	CHECK_EQ(0xFFFF, Info(SKILL_FLOURISH).GetExpLevel());
	CHECK(IsLearned(SKILLDOMAIN_OUSTERS, SKILL_FLOURISH));
}

TEST(ApplySkillInfo, DuplicateSkillIsLearnedOnceAndTheLastStateWins)
{
	SkillInfoWorld world;

	std::vector<SlayerDomain> domains(1);
	domains[0].learnNew = false;
	domains[0].domain = SKILL_DOMAIN_BLADE;
	domains[0].skills.push_back(SlayerSkill{ SKILL_SINGLE_BLOW, 1, 3, 30, 0, true });
	domains[0].skills.push_back(SlayerSkill{ SKILL_SINGLE_BLOW, 2, 4, 40, 0, false });

	Apply(SlayerBytes(domains));

	CHECK(IsLearned(SKILLDOMAIN_BLADE, SKILL_SINGLE_BLOW));
	CHECK_EQ(1, LearnedTotal());
	CHECK_EQ(1, (int)g_pSkillAvailable->size());
	CHECK_EQ(4, Info(SKILL_SINGLE_BLOW).GetExpLevel());
	CHECK_EQ(2, Info(SKILL_SINGLE_BLOW).GetSkillExp());
	CHECK_EQ(4000u, Info(SKILL_SINGLE_BLOW).GetDelayTime());
	CHECK_EQ(false, Info(SKILL_SINGLE_BLOW).IsEnable());

	// Known, not fixed (task 4.16): the second learn is refused and
	// leaves the new-skill flag set. The server sends a skill once.
	CHECK_EQ(true, Domain(SKILLDOMAIN_BLADE).HasNewSkill());
}

TEST(ApplySkillInfo, RaceByteOutOfRangeIsRefusedByTheRead)
{
	// The factory's packet refuses a race past the three before it
	// reads a list, so no such packet reaches the rebuild.
	const BYTE kRaces[] = { 3, 4, 0x7F, 0x80, 0xFF };
	for (size_t k = 0; k < sizeof(kRaces) / sizeof(kRaces[0]); ++k)
	{
		GCSkillInfoFactory factory;
		std::unique_ptr<Packet> packet(factory.createPacket());
		Wire w;
		w.Put<BYTE>(kRaces[k]).Put<BYTE>(0);
		StreamFixture f;
		SocketInputStreamTestAccess::Preload(f.m_Stream, w.Bytes().data(), (unsigned int)w.Bytes().size());
		bool refused = false;
		try {
			packet->read(f.m_Stream);
		} catch (InvalidProtocolException&) {
			refused = true;
		}
		CHECK_EQ(true, refused);
	}
}

TEST(ApplySkillInfo, RaceOutOfRangeOnABuiltPacketDropsItsEntries)
{
	SkillInfoWorld world;
	SetAllFlags();

	// Built rather than read: the entries are popped and deleted
	// unapplied, after the flags and the trees were reset.
	GCSkillInfo packet;
	packet.setPCType(3);
	SlayerSkillInfo* pEntry = new SlayerSkillInfo;
	pEntry->setDomainType(SKILL_DOMAIN_BLADE);
	pEntry->setLearnNewSkill(true);
	SubSlayerSkillInfo* pSkill = new SubSlayerSkillInfo;
	pSkill->setSkillType(SKILL_SINGLE_BLOW);
	pSkill->setSkillExp(0);
	pSkill->setSkillExpLevel(0);
	pSkill->setSkillTurn(0);
	pSkill->setCastingTime(0);
	pSkill->setEnable(true);
	pEntry->addListElement(pSkill);
	pEntry->setListNum(1);
	packet.addListElement(pEntry);

	ApplySkillInfo(&packet);

	CHECK_EQ(0, packet.getListNum());
	CheckNoFlags();
	CheckNothingLearned();
}

TEST(ApplySkillInfo, SkillTypePastTheInfoTableIsSkipped)
{
	SkillInfoWorld world;

	// The constructor sizes the table to MIN_RESULT_ACTIONINFO (512), and
	// this fixture loads no data file, so that is its size here; in the
	// game LoadFromFile resizes it to the count its data file declares.
	// The server's skill types end at its SKILL_MAX, 397. 2048 and up are
	// past ACTIONINFO's range of values too.
	CHECK_EQ((int)MIN_RESULT_ACTIONINFO, g_pSkillInfoTable->GetSize());
	const int kTypes[] = { MIN_RESULT_ACTIONINFO, MAX_ACTIONINFO, 2047, 2048, 0x92A3, 0xFFFF };
	const int kCount = (int)(sizeof(kTypes) / sizeof(kTypes[0]));

	// A slayer entry of nothing but such skills leaves its domain as it
	// was; the entry beside it still applies.
	std::vector<SlayerDomain> domains(2);
	domains[0].learnNew = false;
	domains[0].domain = SKILL_DOMAIN_BLADE;
	domains[0].skills.push_back(SlayerSkill{ SKILL_SINGLE_BLOW, 0, 5, 0, 0, true });
	domains[1].learnNew = false;
	domains[1].domain = SKILL_DOMAIN_SWORD;
	for (int k = 0; k < kCount; ++k)
		domains[1].skills.push_back(SlayerSkill{ kTypes[k], 1, 2, 30, 5, true });
	Apply(SlayerBytes(domains));

	CHECK(IsLearned(SKILLDOMAIN_BLADE, SKILL_SINGLE_BLOW));
	CHECK_EQ(5, Info(SKILL_SINGLE_BLOW).GetExpLevel());
	CHECK_EQ(1, LearnedTotal());
	CHECK_EQ(1, (int)g_pSkillAvailable->size());
	CHECK_EQ(false, Domain(SKILLDOMAIN_SWORD).HasNewSkill());
	CHECK_EQ(false, Domain(SKILLDOMAIN_BLADE).HasNewSkill());

	// The same for the vampire's and the ousters' one entry.
	std::vector<VampireSkill> vampire;
	for (int k = 0; k < kCount; ++k)
		vampire.push_back(VampireSkill{ kTypes[k], 30, 5 });
	Apply(VampireBytes(false, vampire));
	CheckNothingLearned();

	std::vector<OustersSkill> ousters;
	for (int k = 0; k < kCount; ++k)
		ousters.push_back(OustersSkill{ kTypes[k], 3, 30, 5 });
	Apply(OustersBytes(false, ousters));
	CheckNothingLearned();
}

TEST(ApplySkillInfo, ServerGoldensOfEachRaceLearnNothingTheClientDoesNotKnow)
{
	SkillInfoWorld world;

	// The server's goldens (origin/master 7f833cef) hold fixture values
	// in every field: skill types from 0x92A3 up, turns past INT_MAX.
	// The slayer's two domains, 0x91 and 0xB2, are past the domain table.
	Apply(Wire().Hex("0002019102a392c7b6a594a998cdbcab9ac1b0af9e01a493c8b6a594aa98cebcab9ac2b0af9e0000b201a392c7b6a594a998cdbcab9ac1b0af9e01"));
	CheckNothingLearned();
	CheckNoFlags();

	// The ousters' entry offers no new skill; its skills leave the
	// domain's flag down.
	Apply(Wire().Hex("02010002c3b2c5b4e9d8c7b6eddccbbac4b3c6b4ead8c7b6eedccbba"));
	CheckNothingLearned();

	// The vampire's entry offers a new skill, which is all it applies.
	Apply(Wire().Hex("01010102b3a2d7c6b5a4dbcab9a8b4a3d8c6b5a4dccab9a8"));
	CHECK_EQ(0, LearnedTotal());
	CHECK_EQ(0, (int)g_pSkillAvailable->size());
	CHECK_EQ(true, Domain(SKILLDOMAIN_VAMPIRE).HasNewSkill());
	CHECK_EQ(false, Domain(SKILLDOMAIN_OUSTERS).HasNewSkill());
	CheckNoFlags();
	CheckDomainLevelsUntouched();
}

//----------------------------------------------------------------------
// Durations past the int range
//----------------------------------------------------------------------
// The wire's turns are DWORDs; ConvertDurationToMillisecond takes an
// int, so one past INT_MAX arrives negative, and the milliseconds are a
// DWORD. The product is taken modulo 2^32, as a DWORD.
namespace {
DWORD	Millis(DWORD turn)	{ return (DWORD)(turn * 100u); }
} // namespace

TEST(ConvertDuration, MillisecondsPastTheIntRangeWrapAsADword)
{
	// The last in range, then the first past it, both ways.
	CHECK_EQ(2147483600u, ConvertDurationToMillisecond(21474836));
	CHECK_EQ(2147483700u, ConvertDurationToMillisecond(21474837));
	CHECK_EQ(Millis(0xFFFFFF9Cu), ConvertDurationToMillisecond(-100));
	CHECK_EQ(Millis(0xFEB851ECu), ConvertDurationToMillisecond(-21474836));
	CHECK_EQ(Millis(0xFEB851EBu), ConvertDurationToMillisecond(-21474837));
	CHECK_EQ(Millis(0x7FFFFFFFu), ConvertDurationToMillisecond(0x7FFFFFFF));
	CHECK_EQ(Millis(0x80000000u), ConvertDurationToMillisecond((int)0x80000000u));
	CHECK_EQ(Millis(0xFFFFFFFFu), ConvertDurationToMillisecond(-1));
}

TEST(ApplySkillInfo, TurnsPastTheIntRangeKeepTheirWrappedDelay)
{
	SkillInfoWorld world;

	// The server golden's slayer turns, on a skill the client knows.
	std::vector<SlayerDomain> domains(1);
	domains[0].learnNew = false;
	domains[0].domain = SKILL_DOMAIN_BLADE;
	domains[0].skills.push_back(SlayerSkill{ SKILL_SINGLE_BLOW, 0, 0, 0x9AABBCCDu, 0x9EAFB0C1u, true });
	Apply(SlayerBytes(domains));

	CHECK(IsLearned(SKILLDOMAIN_BLADE, SKILL_SINGLE_BLOW));
	CHECK_EQ(Millis(0x9AABBCCDu), Info(SKILL_SINGLE_BLOW).GetDelayTime());
	// The remaining delay goes on as an int: past INT_MAX milliseconds
	// it is negative, and the skill reads as usable now.
	CHECK_EQ(true, (int)Millis(0x9EAFB0C1u) < 0);
	CHECK_EQ(0u, Info(SKILL_SINGLE_BLOW).GetAvailableTimeLeft());

	// The vampire's and the ousters' lists read the same turns.
	std::vector<VampireSkill> vampire;
	vampire.push_back(VampireSkill{ MAGIC_HIDE, 0xFFFFFFFFu, 0x80000000u });
	Apply(VampireBytes(false, vampire));
	CHECK(IsLearned(SKILLDOMAIN_VAMPIRE, MAGIC_HIDE));
	CHECK_EQ(Millis(0xFFFFFFFFu), Info(MAGIC_HIDE).GetDelayTime());
	CHECK_EQ(0u, Info(MAGIC_HIDE).GetAvailableTimeLeft());	// 0 ms

	std::vector<OustersSkill> ousters;
	ousters.push_back(OustersSkill{ SKILL_FLOURISH, 1, 0x7FFFFFFFu, 21474837u });
	Apply(OustersBytes(false, ousters));
	CHECK(IsLearned(SKILLDOMAIN_OUSTERS, SKILL_FLOURISH));
	CHECK_EQ(Millis(0x7FFFFFFFu), Info(SKILL_FLOURISH).GetDelayTime());
	CHECK_EQ(0u, Info(SKILL_FLOURISH).GetAvailableTimeLeft());
}
