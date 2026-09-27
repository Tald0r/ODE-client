//----------------------------------------------------------------------
// test_cpp20_container_helpers_2.cpp
//----------------------------------------------------------------------
//
// The observable contract of the library sites written with contains(),
// starts_with() and std::ranges::find / any_of / none_of: present versus
// absent, the empty container, the empty line, the line one character
// short of the keyword, and the line that is only the character tested
// for. Every check goes through a public entry point.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "MGameStringTable.h"
#include "MSkillManager.h"
#include "SkillDef.h"
#include "SystemAvailabilities.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace {

//----------------------------------------------------------------------
// The skill fixture: a three-step chain, one level apart, so
// MSkillDomain's tree walk has children to go down into.
//----------------------------------------------------------------------
DWORD	s_Frame = 0;
MonotonicClock::TimePoint	s_Now;

int			DropFrameCount(TYPE_FRAMEID)	{ return 0; }
void		RefreshAffect(MItem*)			{}
void		PlayItemSound(TYPE_SOUNDID)		{}
void		RecalculateStatus()				{}
void		ResetQuickItemSlot()			{}
void		RepairHint()					{}
MMagazine*	EmptyMagazineFor(MItem*)		{ return NULL; }

const MItemHost	s_Host = { &s_Frame, DropFrameCount, RefreshAffect, PlayItemSound, &s_Now,
							RecalculateStatus, ResetQuickItemSlot, RepairHint, EmptyMagazineFor,
							NULL, NULL };

const ACTIONINFO	kRoot	= SKILL_SINGLE_BLOW;
const ACTIONINFO	kChild	= SKILL_DOUBLE_IMPACT;
const ACTIONINFO	kLeaf	= SKILL_FAST_RELOAD;

struct SkillWorld : GameModelWorld
{
	SkillWorld()
	{
		s_Now = MonotonicClock::TimePoint();

		g_pSkillInfoTable = new MSkillInfoTable;
		g_pSkillManager = new MSkillManager;
		g_pSkillAvailable = new MSkillSet;

		testfw::MutableRow(*g_pSkillInfoTable, kRoot).Set(0, "Single Blow", 0, 0, 0, "Single Blow");
		testfw::MutableRow(*g_pSkillInfoTable, kChild).Set(1, "Double Impact", 1, 0, 0, "Double Impact");
		testfw::MutableRow(*g_pSkillInfoTable, kLeaf).Set(2, "Fast Reload", 2, 0, 0, "Fast Reload");
		testfw::MutableRow(*g_pSkillInfoTable, kRoot).AddNextSkill(kChild);
		testfw::MutableRow(*g_pSkillInfoTable, kChild).AddNextSkill(kLeaf);

		MItem::SetHost(&s_Host);
	}

	~SkillWorld()
	{
		delete g_pSkillAvailable;	g_pSkillAvailable = NULL;
		delete g_pSkillManager;		g_pSkillManager = NULL;
		delete g_pSkillInfoTable;	g_pSkillInfoTable = NULL;
	}
};

//----------------------------------------------------------------------
// The language file the game string table reads its language out of.
//----------------------------------------------------------------------
const char* const	kLanguageFile	= "cpp20_container_helpers_2_language.inf";
const char* const	kMissingFile	= "cpp20_container_helpers_2_no_such_file.inf";

void	WriteLanguageFile(const char* text)
{
	std::ofstream out(kLanguageFile, std::ios::out | std::ios::binary | std::ios::trunc);
	out.write(text, (std::streamsize)strlen(text));
}

//----------------------------------------------------------------------
// The availability script, loaded from a stream the way the game data
// hands it over.
//----------------------------------------------------------------------
bool	LoadScript(SystemAvailabilitiesManager& m, const char* text)
{
	std::istringstream in(text);
	return m.LoadFromStream(in);
}

} // namespace

//----------------------------------------------------------------------
// MSkillDomain::IsExistSkillStep - std::map membership
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers2, SkillDomainAnswersStepMembershipFromTheMap)
{
	SkillWorld world;

	// Two steps across the three skills of the chain.
	testfw::MutableRow(*g_pSkillInfoTable, kRoot).SetSkillStep(SKILL_STEP_APPRENTICE);
	testfw::MutableRow(*g_pSkillInfoTable, kChild).SetSkillStep(SKILL_STEP_APPRENTICE);
	testfw::MutableRow(*g_pSkillInfoTable, kLeaf).SetSkillStep(SKILL_STEP_ADEPT);

	MSkillDomain domain;

	// An empty domain holds no step at all.
	CHECK(domain.IsExistSkillStep(SKILL_STEP_APPRENTICE) == FALSE);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_ADEPT) == FALSE);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_NULL) == FALSE);

	domain.SetRootSkill(kRoot);

	// Present, present, and two that the walk never put in.
	CHECK(domain.IsExistSkillStep(SKILL_STEP_APPRENTICE) != FALSE);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_ADEPT) != FALSE);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_MASTER) == FALSE);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_NULL) == FALSE);

	// A clear takes both steps away again.
	domain.Clear();
	CHECK(domain.IsExistSkillStep(SKILL_STEP_APPRENTICE) == FALSE);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_ADEPT) == FALSE);
	CHECK(domain.GetSkillStepList(SKILL_STEP_APPRENTICE) == NULL);
}

//----------------------------------------------------------------------
// MSkillDomain::AddSkillStep - the scan that asks whether a skill is in
// the step list already. Its "already there" arm has no reachable input
// through the public API, so what is pinned is the arm that does run and
// the invariant it holds: no step list names a skill twice.
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers2, AStepListTakesEachSkillOnceHoweverOftenTheWalkRuns)
{
	SkillWorld world;

	// All three in one step, in the order the tree walk meets them.
	testfw::MutableRow(*g_pSkillInfoTable, kRoot).SetSkillStep(SKILL_STEP_APPRENTICE);
	testfw::MutableRow(*g_pSkillInfoTable, kChild).SetSkillStep(SKILL_STEP_APPRENTICE);
	testfw::MutableRow(*g_pSkillInfoTable, kLeaf).SetSkillStep(SKILL_STEP_APPRENTICE);
	testfw::MutableRow(*g_pSkillInfoTable, kRoot).SetLearnLevel(30);
	testfw::MutableRow(*g_pSkillInfoTable, kChild).SetLearnLevel(20);
	testfw::MutableRow(*g_pSkillInfoTable, kLeaf).SetLearnLevel(10);

	MSkillDomain domain;

	// The first skill of a step goes into a list that is still empty.
	domain.SetRootSkill(kRoot);

	const MSkillDomain::SKILL_STEP_LIST* pList = domain.GetSkillStepList(SKILL_STEP_APPRENTICE);
	CHECK(pList != NULL);

	if (pList != NULL)
	{
		CHECK_EQ(3, (int)pList->size());
		CHECK_EQ((int)kLeaf, (int)(*pList)[0]);
		CHECK_EQ((int)kChild, (int)(*pList)[1]);
		CHECK_EQ((int)kRoot, (int)(*pList)[2]);
	}

	// A second walk over the tree that is already there leaves the list
	// exactly as it was.
	domain.SetRootSkill(kRoot);

	pList = domain.GetSkillStepList(SKILL_STEP_APPRENTICE);
	CHECK(pList != NULL);

	if (pList != NULL)
	{
		CHECK_EQ(3, (int)pList->size());
		CHECK_EQ((int)kLeaf, (int)(*pList)[0]);
		CHECK_EQ((int)kChild, (int)(*pList)[1]);
		CHECK_EQ((int)kRoot, (int)(*pList)[2]);
	}

	// A cleared domain starts its lists empty again, so the same walk
	// fills exactly the same list.
	domain.Clear();
	domain.SetRootSkill(kRoot);

	pList = domain.GetSkillStepList(SKILL_STEP_APPRENTICE);
	CHECK(pList != NULL);

	if (pList != NULL)
	{
		CHECK_EQ(3, (int)pList->size());
		CHECK_EQ((int)kLeaf, (int)(*pList)[0]);
	}
}

//----------------------------------------------------------------------
// UseEnglishTextFrom - the language file's comment lines and its
// LANGUAGE keyword, which is a prefix test and not a whole-word one
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers2, LanguageFileKeywordIsMatchedAsAPrefix)
{
	// Nothing to read at all: the built-in English text stands.
	CHECK(UseEnglishTextFrom(NULL));
	CHECK(UseEnglishTextFrom(kMissingFile));

	// A file with nothing in it, a line that is nothing but the comment
	// character, and an empty line.
	WriteLanguageFile("");
	CHECK(UseEnglishTextFrom(kLanguageFile));
	WriteLanguageFile(";\n");
	CHECK(UseEnglishTextFrom(kLanguageFile));
	WriteLanguageFile("\n");
	CHECK(UseEnglishTextFrom(kLanguageFile));

	// The keyword selects a language.
	WriteLanguageFile("LANGUAGE 0\n");
	CHECK(!UseEnglishTextFrom(kLanguageFile));
	WriteLanguageFile("LANGUAGE 3\n");
	CHECK(UseEnglishTextFrom(kLanguageFile));

	// A commented-out keyword is not one: the comment test runs first.
	WriteLanguageFile(";LANGUAGE 0\n");
	CHECK(UseEnglishTextFrom(kLanguageFile));

	// One character short of the keyword, with nothing after it.
	WriteLanguageFile("LANGUAG");
	CHECK(UseEnglishTextFrom(kLanguageFile));

	// The short line must not swallow the real one that follows it.
	WriteLanguageFile("LANGUAG\nLANGUAGE 0\n");
	CHECK(!UseEnglishTextFrom(kLanguageFile));

	// Exactly the keyword and nothing else: it matches, and the number
	// that is not there leaves English in place.
	WriteLanguageFile("LANGUAGE");
	CHECK(UseEnglishTextFrom(kLanguageFile));

	// A prefix match with no separator is still a match, and the digit
	// right behind the keyword is still read.
	WriteLanguageFile("LANGUAGE0\n");
	CHECK(!UseEnglishTextFrom(kLanguageFile));

	std::remove(kLanguageFile);
}

//----------------------------------------------------------------------
// SystemAvailabilitiesManager::LoadFromStream - the four line kinds the
// loader branches on, with the comment and empty lines that must not be
// counted as rows of the block they sit in
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers2, AvailabilityScriptRecognisesEveryLineKind)
{
	const char* const kScript =
		"; a comment before anything\r\n"
		";\r\n"					// nothing but the comment character
		"\r\n"					// an empty line
		"Z0\r\n"
		"100\r\n"
		"200\r\n"
		"99999\r\n"
		"*4 2\r\n"
		"10 2\r\n"
		"\r\n"					// an empty line inside the script block
		"; and a comment inside it\r\n"
		"11 2\r\n"
		"S0 1\r\n"
		"20 3 999\r\n";

	SystemAvailabilitiesManager m;
	CHECK(LoadScript(m, kScript));

	// The '*' block: both rows are in force, which they only are if the
	// empty line and the comment line were skipped rather than counted.
	m.SetFlag(~(1u << 4));
	CHECK(!m.ScriptFiltering(10, 1));
	CHECK(!m.ScriptFiltering(11, 1));
	CHECK(m.ScriptFiltering(12, 1));
	CHECK(m.ScriptFiltering(10, 0));

	// The 'Z' block: the two zones it named, and one it did not.
	m.SetOpenDegree(0);
	CHECK(m.ZoneFiltering(100));
	CHECK(m.ZoneFiltering(200));
	CHECK(!m.ZoneFiltering(300));

	// The 'S' block: its row names zone 999, which degree 0 does not open.
	CHECK(!m.ScriptFiltering(20, 2));
	CHECK(m.ScriptFiltering(20, 1));
	CHECK(m.ScriptFiltering(21, 2));
}

//----------------------------------------------------------------------
// SystemAvailabilitiesManager::ZoneFiltering - the scan over a degree's
// zone list, an empty list and the wildcard row
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers2, ZoneFilterAnswersFromTheDegreeList)
{
	const char* const kScript =
		"Z0\n"
		"99999\n"				// degree 0 names no zone at all
		"Z1\n"
		"90909\n"				// the row that opens every zone
		"99999\n"
		"Z2\n"
		"100\n"
		"99999\n";

	SystemAvailabilitiesManager m;
	CHECK(LoadScript(m, kScript));

	// The default degree is past every list, so nothing is consulted.
	CHECK(m.ZoneFiltering(100));

	// Degree 0's list is empty.
	m.SetOpenDegree(0);
	CHECK(!m.ZoneFiltering(100));
	CHECK(!m.ZoneFiltering(90909));

	// Degree 1 adds the wildcard row, which opens a zone no list names.
	m.SetOpenDegree(1);
	CHECK(m.ZoneFiltering(100));
	CHECK(m.ZoneFiltering(12345));

	// Degree 2 adds a list with one ordinary zone; the walk goes down to
	// degree 0, so the wildcard is still reachable.
	m.SetOpenDegree(2);
	CHECK(m.ZoneFiltering(100));
	CHECK(m.ZoneFiltering(12345));
}

//----------------------------------------------------------------------
// SystemAvailabilitiesManager::CheckScript, through ScriptFiltering -
// the scan over one system's filter rows, matching on both ids
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers2, ScriptFilterMatchesOnBothIDsOrNotAtAll)
{
	const char* const kScript =
		"*4 2\n"
		"10 2\n"					// script 10, answer 2 (stored as 1)
		"20 4\n";					// script 20, answer 4 (stored as 3)

	SystemAvailabilitiesManager m;
	CHECK(LoadScript(m, kScript));

	// While system 4 is open the rows are never consulted.
	CHECK(m.ScriptFiltering(10, 1));

	m.SetFlag(~(1u << 4));

	// Both ids match: refused.
	CHECK(!m.ScriptFiltering(10, 1));
	CHECK(!m.ScriptFiltering(20, 3));

	// The right script with the wrong answer, and the reverse, are both
	// allowed.
	CHECK(m.ScriptFiltering(10, 3));
	CHECK(m.ScriptFiltering(20, 1));

	// Neither id appears at all.
	CHECK(m.ScriptFiltering(30, 1));

	// A system whose filter list is empty refuses nothing.
	m.SetFlag(0);
	CHECK(!m.ScriptFiltering(10, 1));
	CHECK(m.ScriptFiltering(30, 1));
}

