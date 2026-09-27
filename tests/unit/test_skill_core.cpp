//----------------------------------------------------------------------
// test_skill_core.cpp
//----------------------------------------------------------------------
//
// The skill core (docs/RESTRUCTURING.md task 4.4, fourth slice): the
// info table one entry per skill, the set of skills a character may use
// now, and the domains that hold a skill tree and hand out what the
// character learns. The half that decides what the player can use at
// this moment - the weapon in hand, the inventory, the zone - is the
// executable's MSkillAvailable.cpp and is not reachable from here; the
// use delays run on the item host's clock, which these tests drive.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "MSkillManager.h"
#include "SkillDef.h"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace {

MonotonicClock::TimePoint	s_Now;

int			DropFrameCount(TYPE_FRAMEID)	{ return 0; }
void		RefreshAffect(MItem*)			{}
void		PlayItemSound(TYPE_SOUNDID)		{}
void		RecalculateStatus()				{}
void		ResetQuickItemSlot()			{}
void		RepairHint()					{}
MMagazine*	EmptyMagazineFor(MItem*)		{ return NULL; }

DWORD	s_Frame = 0;

// The clock is all the skill core wants from a host.
const MItemHost	s_Host = { &s_Frame, DropFrameCount, RefreshAffect, PlayItemSound, &s_Now,
							RecalculateStatus, ResetQuickItemSlot, RepairHint, EmptyMagazineFor,
							NULL, NULL };

const char* const	kTempFile = "skill_core_test.bin";

// Three skills wired into a chain: the root, what it leads to, and what
// that leads to.
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

		// A three-step chain, one level apart.
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

} // namespace

//----------------------------------------------------------------------
// One skill's entry
//----------------------------------------------------------------------
TEST(SkillInfoNode, UseDelaysRunOnTheHostClock)
{
	SkillWorld world;
	SKILLINFO_NODE node;

	// Nothing used yet: available, with nothing left to wait for.
	CHECK(node.IsAvailableTime());
	CHECK_EQ(0, (int)node.GetAvailableTimeLeft());

	// Using it starts its delay from now.
	node.SetDelayTime(3000);
	CHECK_EQ(3000, (int)node.GetDelayTime());
	s_Now = MonotonicClock::FromMillis(10000);
	node.SetNextAvailableTime();
	CHECK_EQ(false, node.IsAvailableTime());
	CHECK_EQ(3000, (int)node.GetAvailableTimeLeft());
	s_Now = MonotonicClock::FromMillis(12999);
	CHECK_EQ(false, node.IsAvailableTime());
	CHECK_EQ(1, (int)node.GetAvailableTimeLeft());
	s_Now = MonotonicClock::FromMillis(13000);
	CHECK(node.IsAvailableTime());
	CHECK_EQ(0, (int)node.GetAvailableTimeLeft());

	// Setting it available takes a delay of its own, or none at all.
	node.SetAvailableTime(500);
	CHECK_EQ(false, node.IsAvailableTime());
	node.SetAvailableTime(0);
	CHECK(node.IsAvailableTime());

	// Without a clock there is no delay.
	node.SetNextAvailableTime();
	CHECK_EQ(false, node.IsAvailableTime());
	MItem::SetHost(NULL);
	CHECK(node.IsAvailableTime());
	CHECK_EQ(0, (int)node.GetAvailableTimeLeft());
	MItem::SetHost(&s_Host);
	CHECK_EQ(false, node.IsAvailableTime());
}

TEST(SkillInfoNode, NextSkillsSortByIdAndRefuseADuplicate)
{
	SkillWorld world;
	SKILLINFO_NODE node;

	CHECK(node.AddNextSkill((ACTIONINFO)40));
	CHECK(node.AddNextSkill((ACTIONINFO)10));
	CHECK(node.AddNextSkill((ACTIONINFO)30));
	CHECK_EQ(false, node.AddNextSkill((ACTIONINFO)30));

	const SKILLINFO_NODE::SKILLID_LIST& list = node.GetNextSkillList();
	CHECK_EQ(3, (int)list.size());
	SKILLINFO_NODE::SKILLID_LIST::const_iterator it = list.begin();
	CHECK_EQ(10, (int)*it);		++it;
	CHECK_EQ(30, (int)*it);		++it;
	CHECK_EQ(40, (int)*it);
}

TEST(SkillInfoNode, SaveAndLoadRoundTripTheServerFields)
{
	SkillWorld world;

	// A skill of any other domain carries the common fields only.
	SKILLINFO_NODE src;
	src.Set(3, "Blade Dance", 4, 5, 77, "Blade Dance");
	src.DomainType = SKILLDOMAIN_SWORD;		// not 0, so the field is really carried
	src.SetMP(42);
	src.SetLearnLevel(9);
	src.SkillPoint = 6;

	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		src.SaveFromFileServerSkillInfo(out);
	}

	SKILLINFO_NODE dst;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		dst.LoadFromFileServerSkillInfo(in);
	}
	std::remove(kTempFile);

	CHECK_EQ(42, dst.GetMP());
	CHECK_EQ(9, dst.GetLearnLevel());
	CHECK_EQ((int)SKILLDOMAIN_SWORD, dst.DomainType);
	CHECK_EQ(0, dst.SkillPoint);		// an Ousters field, not in this row

	// An Ousters skill carries the elemental block too.
	SKILLINFO_NODE ousters;
	ousters.Set(2, "Flourish", 0, 0, 0, "Flourish");
	ousters.DomainType = SKILLDOMAIN_OUSTERS;
	ousters.SetMP(11);
	ousters.SkillPoint = 6;
	ousters.LevelUpPoint = 7;
	ousters.Fire = 1;
	ousters.Water = 2;
	ousters.Earth = 3;
	ousters.Wind = 4;
	ousters.CanDelete = 1;
	ousters.SkillTypeList.push_back(5);

	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		ousters.SaveFromFileServerSkillInfo(out);
	}

	SKILLINFO_NODE back;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		back.LoadFromFileServerSkillInfo(in);
	}
	std::remove(kTempFile);

	CHECK_EQ(11, back.GetMP());
	CHECK_EQ(6, back.SkillPoint);
	CHECK_EQ(7, back.LevelUpPoint);
	CHECK_EQ(1, back.Fire);
	CHECK_EQ(4, back.Wind);
	CHECK_EQ(1, (int)back.CanDelete);
	CHECK_EQ(1, (int)back.SkillTypeList.size());
}

TEST(SkillInfoNode, AFreshEntrySitsNowhereInItsTree)
{
	SkillWorld world;
	SKILLINFO_NODE node;

	// A domain branches on the step, so it cannot be left to whatever
	// the memory held.
	CHECK_EQ((int)SKILL_STEP_NULL, (int)node.GetSkillStep());
	CHECK_EQ(0, node.GetX());
	CHECK_EQ(0, node.GetY());
	CHECK_EQ(0, node.GetMP());
	CHECK_EQ(0, (int)node.GetDelayTime());
	CHECK(node.IsAvailableTime());

	// The table's own entries are just as defined, and it sets the one
	// cast delay it knows by hand.
	CHECK_EQ((int)SKILL_STEP_NULL, (int)(*g_pSkillInfoTable)[kLeaf].GetSkillStep());
	CHECK_EQ(3000, (int)(*g_pSkillInfoTable)[SUMMON_HELICOPTER].GetDelayTime());
}

//----------------------------------------------------------------------
// The set of skills a character may use
//----------------------------------------------------------------------
TEST(SkillSet, HoldsSkillsByIdAndEnablesThemOneAtATime)
{
	SkillWorld world;
	MSkillSet set;

	CHECK_EQ(false, set.IsEnableSkill(kRoot));
	CHECK(set.AddSkill(kRoot));
	CHECK(set.AddSkill(kChild));
	CHECK_EQ(false, set.AddSkill(kRoot));		// already held
	CHECK_EQ(2, (int)set.size());
	CHECK(set.IsEnableSkill(kRoot));

	// Disabling keeps the skill but takes it out of use.
	CHECK(set.DisableSkill(kRoot));
	CHECK_EQ(false, set.IsEnableSkill(kRoot));
	CHECK(set.IsEnableSkill(kChild));
	CHECK_EQ(2, (int)set.size());
	CHECK(set.EnableSkill(kRoot));
	CHECK(set.IsEnableSkill(kRoot));

	// A skill it does not hold cannot be enabled or disabled.
	CHECK_EQ(false, set.EnableSkill(kLeaf));
	CHECK_EQ(false, set.DisableSkill(kLeaf));

	// Removing takes it out for good.
	CHECK(set.RemoveSkill(kRoot));
	CHECK_EQ(false, set.RemoveSkill(kRoot));
	CHECK_EQ(false, set.IsEnableSkill(kRoot));
	CHECK_EQ(1, (int)set.size());
}

//----------------------------------------------------------------------
// A domain and its tree
//----------------------------------------------------------------------
TEST(SkillDomain, TheRootSkillPullsInTheChainBelowIt)
{
	SkillWorld world;
	MSkillDomain domain;

	CHECK_EQ(0, domain.GetSize());
	domain.SetRootSkill(kRoot);

	// All three arrive: the root can be learned next, the rest cannot
	// be learned yet.
	CHECK_EQ(3, domain.GetSize());
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NEXT, (int)domain.GetSkillStatus(kRoot));
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_OTHER, (int)domain.GetSkillStatus(kChild));
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_OTHER, (int)domain.GetSkillStatus(kLeaf));

	// A skill outside the tree is not in the domain at all.
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NULL, (int)domain.GetSkillStatus(SKILL_FLOURISH));

	// The domain level is the server's, and a root reset zeroes it.
	CHECK_EQ(0, domain.GetDomainLevel());
	domain.SetDomainLevel(4);
	domain.SetRootSkill(kRoot);
	CHECK_EQ(0, domain.GetDomainLevel());

	CHECK_EQ(false, (bool)domain.HasNewSkill());
	domain.SetNewSkill();
	CHECK(domain.HasNewSkill());

	domain.Clear();
	CHECK_EQ(0, domain.GetSize());
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NULL, (int)domain.GetSkillStatus(kRoot));
}

TEST(SkillDomain, LearningWalksDownTheChainAndUnlearningBackUp)
{
	SkillWorld world;
	MSkillDomain domain;
	domain.SetRootSkill(kRoot);

	// Nothing is learned without a skill point to spend.
	CHECK_EQ(false, domain.LearnSkill(kRoot));
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NEXT, (int)domain.GetSkillStatus(kRoot));

	// With one, the root is learned and becomes usable; what follows it
	// becomes learnable.
	CHECK_EQ(false, g_pSkillAvailable->IsEnableSkill(kRoot));
	domain.SetNewSkill();
	CHECK(domain.LearnSkill(kRoot));
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_LEARNED, (int)domain.GetSkillStatus(kRoot));
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NEXT, (int)domain.GetSkillStatus(kChild));
	CHECK(g_pSkillAvailable->IsEnableSkill(kRoot));

	// Learning spends the point, so each refusal below is given one of
	// its own: otherwise they would all stop at that first gate.
	domain.SetNewSkill();
	CHECK_EQ(false, domain.LearnSkill(kRoot));		// already learned
	domain.SetNewSkill();
	CHECK_EQ(false, domain.LearnSkill(SKILL_FLOURISH));	// not in this domain

	// Unlearning takes the deepest one back off, and out of use. The
	// child is refused for not being learned, at its own gate.
	domain.SetNewSkill();
	CHECK(domain.LearnSkill(kChild));
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NEXT, (int)domain.GetSkillStatus(kLeaf));
	CHECK_EQ(false, domain.UnLearnSkill(kRoot));	// not the deepest any more
	CHECK(domain.UnLearnSkill(kChild));
	// Its parent is still learned, so it is learnable again rather than
	// out of reach; what followed it is out of reach.
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NEXT, (int)domain.GetSkillStatus(kChild));
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_OTHER, (int)domain.GetSkillStatus(kLeaf));
	CHECK_EQ(false, g_pSkillAvailable->IsEnableSkill(kChild));
	CHECK(domain.UnLearnSkill(kRoot));
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_OTHER, (int)domain.GetSkillStatus(kRoot));
	CHECK_EQ(false, g_pSkillAvailable->IsEnableSkill(kRoot));
	CHECK_EQ(false, domain.UnLearnSkill(kRoot));
}

//----------------------------------------------------------------------
// The manager over the domains
//----------------------------------------------------------------------
TEST(SkillManager, InitGivesEveryDomainItsRootSkill)
{
	SkillWorld world;

	g_pSkillManager->Init();

	CHECK_EQ(MAX_SKILLDOMAIN, g_pSkillManager->GetSize());
	CHECK((*g_pSkillManager)[SKILLDOMAIN_BLADE].GetSkillStatus(SKILL_SINGLE_BLOW)
			!= MSkillDomain::SKILLSTATUS_NULL);
	CHECK((*g_pSkillManager)[SKILLDOMAIN_SWORD].GetSkillStatus(SKILL_DOUBLE_IMPACT)
			!= MSkillDomain::SKILLSTATUS_NULL);
	CHECK((*g_pSkillManager)[SKILLDOMAIN_OUSTERS].GetSkillStatus(SKILL_FLOURISH)
			!= MSkillDomain::SKILLSTATUS_NULL);

	// The blade domain took the chain the table wires under its root.
	CHECK_EQ(3, (*g_pSkillManager)[SKILLDOMAIN_BLADE].GetSize());

	// A domain holds only its own root's tree.
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NULL,
			(int)(*g_pSkillManager)[SKILLDOMAIN_OUSTERS].GetSkillStatus(SKILL_SINGLE_BLOW));

	// The other five roots are there too.
	CHECK((*g_pSkillManager)[SKILLDOMAIN_GUN].GetSkillStatus(SKILL_FAST_RELOAD)
			!= MSkillDomain::SKILLSTATUS_NULL);
	CHECK((*g_pSkillManager)[SKILLDOMAIN_ENCHANT].GetSkillStatus(MAGIC_CREATE_HOLY_WATER)
			!= MSkillDomain::SKILLSTATUS_NULL);
	CHECK((*g_pSkillManager)[SKILLDOMAIN_HEAL].GetSkillStatus(MAGIC_CURE_LIGHT_WOUNDS)
			!= MSkillDomain::SKILLSTATUS_NULL);
	CHECK((*g_pSkillManager)[SKILLDOMAIN_VAMPIRE].GetSkillStatus(MAGIC_HIDE)
			!= MSkillDomain::SKILLSTATUS_NULL);
	CHECK((*g_pSkillManager)[SKILLDOMAIN_ETC].GetSkillStatus(SKILL_SOUL_CHAIN)
			!= MSkillDomain::SKILLSTATUS_NULL);
}

TEST(SkillManager, ADomainTheFileNamesPastTheTableIsRefused)
{
	SkillWorld world;
	g_pSkillManager->Init();

	// The domain-experience file names a domain per row, and the table
	// is indexed raw. A row naming a domain outside it stops the read
	// rather than writing through a wild pointer.
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int rows = 2;
		const int past = MAX_SKILLDOMAIN + 4;
		const int payload = 0;
		out.write((const char*)&rows, 4);
		out.write((const char*)&past, 4);
		for (int i = 0; i < 8; i++)
			out.write((const char*)&payload, 4);
	}
	{
		std::ifstream in(kTempFile, std::ios::binary);
		g_pSkillManager->LoadFromFileServerDomainInfo(in);
	}

	// A negative one, and a row that ends before its domain does.
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int rows = 1;
		const int negative = -3;
		out.write((const char*)&rows, 4);
		out.write((const char*)&negative, 4);
	}
	{
		std::ifstream in(kTempFile, std::ios::binary);
		g_pSkillManager->LoadFromFileServerDomainInfo(in);
	}
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int rows = 40;
		out.write((const char*)&rows, 4);
	}
	{
		std::ifstream in(kTempFile, std::ios::binary);
		g_pSkillManager->LoadFromFileServerDomainInfo(in);
	}
	std::remove(kTempFile);

	// The tree the manager built is still the tree it built.
	CHECK_EQ(MAX_SKILLDOMAIN, g_pSkillManager->GetSize());
	CHECK_EQ(3, (*g_pSkillManager)[SKILLDOMAIN_BLADE].GetSize());
}

//----------------------------------------------------------------------
// The step lists a domain groups its tree into
//----------------------------------------------------------------------
TEST(SkillDomain, AStepListIsOrderedByLearnLevelAndHoldsEachSkillOnce)
{
	SkillWorld world;

	// All three sit in one step, and the level the skill file says
	// each is learned at runs against the order the tree walk meets
	// them.
	testfw::MutableRow(*g_pSkillInfoTable, kRoot).SetSkillStep(SKILL_STEP_APPRENTICE);
	testfw::MutableRow(*g_pSkillInfoTable, kChild).SetSkillStep(SKILL_STEP_APPRENTICE);
	testfw::MutableRow(*g_pSkillInfoTable, kLeaf).SetSkillStep(SKILL_STEP_APPRENTICE);
	testfw::MutableRow(*g_pSkillInfoTable, kRoot).SetLearnLevel(30);
	testfw::MutableRow(*g_pSkillInfoTable, kChild).SetLearnLevel(20);
	testfw::MutableRow(*g_pSkillInfoTable, kLeaf).SetLearnLevel(10);

	MSkillDomain domain;

	CHECK(domain.IsExistSkillStep(SKILL_STEP_APPRENTICE) == FALSE);
	CHECK(domain.GetSkillStepList(SKILL_STEP_APPRENTICE) == NULL);

	domain.SetRootSkill(kRoot);

	CHECK(domain.IsExistSkillStep(SKILL_STEP_APPRENTICE) != FALSE);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_MASTER) == FALSE);
	CHECK(domain.GetSkillStepList(SKILL_STEP_MASTER) == NULL);

	const MSkillDomain::SKILL_STEP_LIST* pList = domain.GetSkillStepList(SKILL_STEP_APPRENTICE);

	CHECK(pList != NULL);

	if (pList != NULL)
	{
		// Lowest learn level first, whichever order they arrived in.
		CHECK_EQ(3, (int)pList->size());
		CHECK_EQ((int)kLeaf, (int)(*pList)[0]);
		CHECK_EQ((int)kChild, (int)(*pList)[1]);
		CHECK_EQ((int)kRoot, (int)(*pList)[2]);
	}

	// A second walk over a tree that is already there does not list
	// anything twice.
	domain.SetRootSkill(kRoot);
	pList = domain.GetSkillStepList(SKILL_STEP_APPRENTICE);
	CHECK(pList != NULL);

	if (pList != NULL)
	{
		CHECK_EQ(3, (int)pList->size());
	}
}

//----------------------------------------------------------------------
// A domain's own save file
//----------------------------------------------------------------------
TEST(SkillDomain, ASavedDomainComesBackReadyToLearnAndUnlearn)
{
	SkillWorld world;
	MSkillDomain domain;

	testfw::MutableRow(*g_pSkillInfoTable, kRoot).SetSkillStep(SKILL_STEP_APPRENTICE);
	testfw::MutableRow(*g_pSkillInfoTable, kChild).SetSkillStep(SKILL_STEP_APPRENTICE);
	testfw::MutableRow(*g_pSkillInfoTable, kLeaf).SetSkillStep(SKILL_STEP_APPRENTICE);

	domain.SetRootSkill(kRoot);
	domain.SetNewSkill();
	CHECK(domain.LearnSkill(kRoot));
	domain.SetNewSkill();
	CHECK(domain.LearnSkill(kChild));

	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		domain.SaveToFile(out);
	}

	//------------------------------------------------------------------
	// A domain of its own gets the statuses back, and with them the
	// levels the learn and unlearn paths index - which the file does
	// not carry and the info table does.
	//------------------------------------------------------------------
	{
		MSkillDomain loaded;
		std::ifstream in(kTempFile, std::ios::binary);
		loaded.LoadFromFile(in);

		CHECK_EQ(3, loaded.GetSize());
		CHECK_EQ((int)MSkillDomain::SKILLSTATUS_LEARNED, (int)loaded.GetSkillStatus(kRoot));
		CHECK_EQ((int)MSkillDomain::SKILLSTATUS_LEARNED, (int)loaded.GetSkillStatus(kChild));
		CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NEXT, (int)loaded.GetSkillStatus(kLeaf));

		// The step lists are the tree walk's, so they come back too.
		const MSkillDomain::SKILL_STEP_LIST* pList = loaded.GetSkillStepList(SKILL_STEP_APPRENTICE);
		CHECK(pList != NULL);

		if (pList != NULL)
		{
			CHECK_EQ(3, (int)pList->size());
		}

		// The deepest learned skill is the one that can be given back.
		CHECK_EQ(false, loaded.UnLearnSkill(kRoot));
		CHECK(loaded.UnLearnSkill(kChild));
		CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NEXT, (int)loaded.GetSkillStatus(kChild));
		CHECK_EQ((int)MSkillDomain::SKILLSTATUS_OTHER, (int)loaded.GetSkillStatus(kLeaf));
		CHECK(loaded.UnLearnSkill(kRoot));
	}

	//------------------------------------------------------------------
	// And so does the domain the load runs over, whose array the
	// clear at the head of the load takes away.
	//------------------------------------------------------------------
	{
		std::ifstream in(kTempFile, std::ios::binary);
		domain.LoadFromFile(in);

		CHECK_EQ(3, domain.GetSize());
		CHECK(domain.UnLearnSkill(kChild));

		// What is still learned is usable again, and what was just
		// given back is not.
		CHECK(g_pSkillAvailable->IsEnableSkill(kRoot));
		CHECK_EQ(false, g_pSkillAvailable->IsEnableSkill(kChild));
	}

	//------------------------------------------------------------------
	// A file that says a skill deep in the tree is learned and its
	// parent is not: the level below it holds no skill, so the
	// unlearn that walks down to that level asks the tree what the
	// empty slot leads to - which AddNextSkill now refuses outright
	// instead of leaning on the info table's out-of-range entry.
	//------------------------------------------------------------------
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int rows = 1;
		const WORD id = (WORD)kChild;
		const BYTE status = (BYTE)MSkillDomain::SKILLSTATUS_LEARNED;
		out.write((const char*)&rows, 4);
		out.write((const char*)&id, 2);
		out.write((const char*)&status, 1);
	}
	{
		MSkillDomain loaded;
		std::ifstream in(kTempFile, std::ios::binary);
		loaded.LoadFromFile(in);

		CHECK_EQ(1, loaded.GetSize());
		CHECK(loaded.UnLearnSkill(kChild));
		CHECK_EQ((int)MSkillDomain::SKILLSTATUS_OTHER, (int)loaded.GetSkillStatus(kChild));
		CHECK_EQ(false, loaded.UnLearnSkill(kChild));
	}

	std::remove(kTempFile);
}

TEST(SkillDomain, ASaveFileThatEndsEarlyLoadsWhatIsThereAndStops)
{
	SkillWorld world;

	//------------------------------------------------------------------
	// A count with no rows behind it invents no skills: the id and the
	// status of a row that is not there are never read.
	//------------------------------------------------------------------
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int rows = 4;
		out.write((const char*)&rows, 4);
	}
	{
		MSkillDomain domain;
		std::ifstream in(kTempFile, std::ios::binary);
		domain.LoadFromFile(in);

		CHECK_EQ(0, domain.GetSize());

		// A domain with no tree learns and unlearns nothing. Both
		// refusals stop at the gates that were already there - the
		// skill is not in the list, and no level is learned - so they
		// do not reach the bounds guards on the learned-skill array,
		// which after this fix nothing can: the array is absent only
		// while the list is empty too.
		domain.SetNewSkill();
		CHECK_EQ(false, domain.LearnSkill(kRoot));
		CHECK_EQ(false, domain.UnLearnSkill(kRoot));
	}

	//------------------------------------------------------------------
	// Two rows behind a count of a thousand are two skills.
	//------------------------------------------------------------------
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int rows = 1000;
		WORD id;
		BYTE status;
		out.write((const char*)&rows, 4);
		id = (WORD)kRoot;	status = (BYTE)MSkillDomain::SKILLSTATUS_LEARNED;
		out.write((const char*)&id, 2);
		out.write((const char*)&status, 1);
		id = (WORD)kChild;	status = (BYTE)MSkillDomain::SKILLSTATUS_NEXT;
		out.write((const char*)&id, 2);
		out.write((const char*)&status, 1);
	}
	{
		MSkillDomain domain;
		std::ifstream in(kTempFile, std::ios::binary);
		domain.LoadFromFile(in);

		CHECK_EQ(2, domain.GetSize());
		CHECK_EQ((int)MSkillDomain::SKILLSTATUS_LEARNED, (int)domain.GetSkillStatus(kRoot));
		CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NEXT, (int)domain.GetSkillStatus(kChild));
		CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NULL, (int)domain.GetSkillStatus(kLeaf));
	}

	//------------------------------------------------------------------
	// A status the enum does not have is not a status, and neither is
	// the one that means the skill is not in the domain at all.
	//------------------------------------------------------------------
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int rows = 3;
		WORD id;
		BYTE status;
		out.write((const char*)&rows, 4);
		id = (WORD)kRoot;	status = 200;
		out.write((const char*)&id, 2);
		out.write((const char*)&status, 1);
		id = (WORD)kLeaf;	status = (BYTE)MSkillDomain::SKILLSTATUS_NULL;
		out.write((const char*)&id, 2);
		out.write((const char*)&status, 1);
		id = (WORD)kChild;	status = (BYTE)MSkillDomain::SKILLSTATUS_LEARNED;
		out.write((const char*)&id, 2);
		out.write((const char*)&status, 1);
	}
	{
		MSkillDomain domain;
		std::ifstream in(kTempFile, std::ios::binary);
		domain.LoadFromFile(in);

		CHECK_EQ(1, domain.GetSize());
		CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NULL, (int)domain.GetSkillStatus(kRoot));
		CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NULL, (int)domain.GetSkillStatus(kLeaf));
		CHECK_EQ((int)MSkillDomain::SKILLSTATUS_LEARNED, (int)domain.GetSkillStatus(kChild));
	}

	//------------------------------------------------------------------
	// A negative count is not a count.
	//------------------------------------------------------------------
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int rows = -5;
		const WORD id = (WORD)kRoot;
		const BYTE status = (BYTE)MSkillDomain::SKILLSTATUS_LEARNED;
		out.write((const char*)&rows, 4);
		out.write((const char*)&id, 2);
		out.write((const char*)&status, 1);
	}
	{
		MSkillDomain domain;
		std::ifstream in(kTempFile, std::ios::binary);
		domain.LoadFromFile(in);

		CHECK_EQ(0, domain.GetSize());
	}

	// An empty file is a domain with nothing in it.
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
	}
	{
		MSkillDomain domain;
		std::ifstream in(kTempFile, std::ios::binary);
		domain.LoadFromFile(in);

		CHECK_EQ(0, domain.GetSize());
	}

	std::remove(kTempFile);
}

//----------------------------------------------------------------------
// A domain's experience rows
//----------------------------------------------------------------------
TEST(SkillDomain, AnExperienceLevelPastTheTableIsRefused)
{
	SkillWorld world;
	MSkillDomain domain;

	const int			kGoal	= 0x01234567;
	const unsigned int	kAccum	= 0x07654321;

	// A row for a level the table holds reads back through GetExpInfo.
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int level = 3;
		out.write((const char*)&level, 4);
		out.write((const char*)&kGoal, 4);
		out.write((const char*)&kAccum, 4);
	}
	{
		std::ifstream in(kTempFile, std::ios::binary);
		domain.LoadFromFileServerDomainInfo(in);
	}
	CHECK_EQ(kGoal, domain.GetExpInfo(3).GoalExp);
	CHECK_EQ((int)kAccum, (int)domain.GetExpInfo(3).AccumExp);

	//------------------------------------------------------------------
	// A level outside it is not loaded at all. A regression guard
	// rather than a reproduction: the table's own range test - in
	// force in every build since an earlier slice - already sent the
	// store to the one out-of-range row it shares across every
	// experience table, and that row is not the one a read gets back,
	// so the checks below pass against the unfixed loader too. What
	// they pin is the contract: the file's numbers never become what
	// a level past the end of the table reads.
	//------------------------------------------------------------------
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int level = 4000;
		out.write((const char*)&level, 4);
		out.write((const char*)&kGoal, 4);
		out.write((const char*)&kAccum, 4);
	}
	{
		std::ifstream in(kTempFile, std::ios::binary);
		domain.LoadFromFileServerDomainInfo(in);
	}
	CHECK(domain.GetExpInfo(4000).GoalExp != kGoal);

	// The same for a negative one.
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int level = -2;
		out.write((const char*)&level, 4);
		out.write((const char*)&kGoal, 4);
		out.write((const char*)&kAccum, 4);
	}
	{
		std::ifstream in(kTempFile, std::ios::binary);
		domain.LoadFromFileServerDomainInfo(in);
	}
	CHECK(domain.GetExpInfo(-2).GoalExp != kGoal);

	// A file that ends before the level does names no level.
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
	}
	{
		std::ifstream in(kTempFile, std::ios::binary);
		domain.LoadFromFileServerDomainInfo(in);
	}

	// The row that did load is untouched by any of it.
	CHECK_EQ(kGoal, domain.GetExpInfo(3).GoalExp);

	std::remove(kTempFile);
}

TEST(SkillDomain, EveryTruncatedExperienceRowPreservesThePreviousValue)
{
	SkillWorld world;
	const unsigned char seed[] = {3, 0, 0, 0, 77, 0, 0, 0, 88, 0, 0, 0};
	const unsigned char replacement[] = {
		3, 0, 0, 0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x08
	};
	for (std::streamsize size = 0; size < static_cast<std::streamsize>(sizeof(replacement)); ++size) {
		MSkillDomain domain;
		{
			std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
			out.write(reinterpret_cast<const char*>(seed), sizeof(seed));
		}
		{
			std::ifstream in(kTempFile, std::ios::binary);
			domain.LoadFromFileServerDomainInfo(in);
			CHECK(in.good());
		}
		{
			std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
			out.write(reinterpret_cast<const char*>(replacement), size);
		}
		{
			std::ifstream in(kTempFile, std::ios::binary);
			domain.LoadFromFileServerDomainInfo(in);
			CHECK(in.fail());
		}
		CHECK_EQ(77, domain.GetExpInfo(3).GoalExp);
		CHECK_EQ(88, domain.GetExpInfo(3).AccumExp);
	}
	std::remove(kTempFile);
}

TEST(SkillManager, InvalidExperienceCountsCannotPublishRows)
{
	SkillWorld world;
	g_pSkillManager->Init();
	const int seed[] = {1, SKILLDOMAIN_BLADE, 3, 77, 88};
	for (int count : {-1, 60000}) {
		{
			std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
			out.write(reinterpret_cast<const char*>(seed), sizeof(seed));
		}
		{
			std::ifstream in(kTempFile, std::ios::binary);
			g_pSkillManager->LoadFromFileServerDomainInfo(in);
			CHECK(in.good());
		}
		{
			const int replacement[] = {count, SKILLDOMAIN_BLADE, 3, 100, 200};
			std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
			out.write(reinterpret_cast<const char*>(replacement), sizeof(replacement));
		}
		{
			std::ifstream in(kTempFile, std::ios::binary);
			g_pSkillManager->LoadFromFileServerDomainInfo(in);
			CHECK(in.fail());
		}
		const auto& info = (*g_pSkillManager)[SKILLDOMAIN_BLADE].GetExpInfo(3);
		CHECK_EQ(77, info.GoalExp);
		CHECK_EQ(88, info.AccumExp);
	}
	std::remove(kTempFile);
}

TEST(SkillManager, InvalidExperienceDomainLeavesTheStreamFailed)
{
	SkillWorld world;
	g_pSkillManager->Init();
	for (int domain : {-1, static_cast<int>(MAX_SKILLDOMAIN)}) {
		{
			const int record[] = {1, domain, 3, 100, 200};
			std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
			out.write(reinterpret_cast<const char*>(record), sizeof(record));
		}
		{
			std::ifstream in(kTempFile, std::ios::binary);
			g_pSkillManager->LoadFromFileServerDomainInfo(in);
			CHECK(in.fail());
		}
	}
	std::remove(kTempFile);
}

TEST(SkillManager, TruncatedExperienceFilesPreserveRowsAndEmptyFilesPreserveTheNextRecord)
{
	SkillWorld world;
	g_pSkillManager->Init();
	const int seed[] = {1, SKILLDOMAIN_BLADE, 3, 77, 88};
	const int replacement[] = {1, SKILLDOMAIN_BLADE, 3, 100, 200};
	for (std::streamsize size = 0; size < static_cast<std::streamsize>(sizeof(replacement)); ++size) {
		{
			std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
			out.write(reinterpret_cast<const char*>(seed), sizeof(seed));
		}
		{
			std::ifstream in(kTempFile, std::ios::binary);
			g_pSkillManager->LoadFromFileServerDomainInfo(in);
			CHECK(in.good());
		}
		{
			std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
			out.write(reinterpret_cast<const char*>(replacement), size);
		}
		{
			std::ifstream in(kTempFile, std::ios::binary);
			g_pSkillManager->LoadFromFileServerDomainInfo(in);
			CHECK(in.fail());
		}
		const auto& info = (*g_pSkillManager)[SKILLDOMAIN_BLADE].GetExpInfo(3);
		CHECK_EQ(77, info.GoalExp);
		CHECK_EQ(88, info.AccumExp);
	}
	{
		const int empty = 0;
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		out.write(reinterpret_cast<const char*>(&empty), sizeof(empty));
		out.put('\x7e');
	}
	{
		std::ifstream in(kTempFile, std::ios::binary);
		g_pSkillManager->LoadFromFileServerDomainInfo(in);
		CHECK(in.good());
		CHECK_EQ(0x7e, in.get());
		CHECK_EQ(77, (*g_pSkillManager)[SKILLDOMAIN_BLADE].GetExpInfo(3).GoalExp);
	}
	std::remove(kTempFile);
}

TEST(SkillManager, InitSkillListRebuildsTheTreesAndKeepsTheDomainLevels)
{
	SkillWorld world;

	g_pSkillManager->Init();

	MSkillDomain& blade = testfw::MutableRow(*g_pSkillManager, SKILLDOMAIN_BLADE);

	blade.SetDomainLevel(9);
	blade.SetDomainExpRemain(1200);
	blade.SetNewSkill();
	CHECK(blade.LearnSkill(kRoot));
	CHECK(g_pSkillAvailable->IsEnableSkill(kRoot));

	//------------------------------------------------------------------
	// The server sends the domain levels once and the list of learned
	// skills on every login, so the rebuild the skill-info packet asks
	// for keeps the levels and drops what was learned.
	//------------------------------------------------------------------
	g_pSkillManager->InitSkillList();

	CHECK_EQ(9, blade.GetDomainLevel());
	CHECK_EQ(1200, (int)blade.GetDomainExpRemain());
	CHECK_EQ(3, blade.GetSize());
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NEXT, (int)blade.GetSkillStatus(kRoot));
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_OTHER, (int)blade.GetSkillStatus(kChild));
	CHECK_EQ(false, g_pSkillAvailable->IsEnableSkill(kRoot));

	// The tree is ready to be learned into again straight away.
	blade.SetNewSkill();
	CHECK(blade.LearnSkill(kRoot));

	// A second rebuild is the same rebuild.
	g_pSkillManager->InitSkillList();

	CHECK_EQ(MAX_SKILLDOMAIN, g_pSkillManager->GetSize());
	CHECK_EQ(9, blade.GetDomainLevel());
	CHECK_EQ(3, blade.GetSize());
	CHECK_EQ(2, (*g_pSkillManager)[SKILLDOMAIN_SWORD].GetSize());
	CHECK_EQ(1, (*g_pSkillManager)[SKILLDOMAIN_GUN].GetSize());
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NEXT, (int)blade.GetSkillStatus(kRoot));
}

TEST(SkillDomain, ASkillDeeperThanTheDomainWasBuiltForIsRefused)
{
	SkillWorld world;
	MSkillDomain domain;

	domain.SetRootSkill(kRoot);

	// The array that says what is learned at each level is as long as
	// the deepest skill the tree walk counted - three levels here.
	domain.SetNewSkill();
	CHECK(domain.LearnSkill(kRoot));

	//------------------------------------------------------------------
	// The levels themselves come out of the skill file, which is read
	// once before the trees are built, so the two agree in the client
	// today. They are one table apart all the same: a level the walk
	// never counted used to be written straight into that array,
	// which is the shape of a heap write past the end - here nine
	// entries into an array of three.
	//------------------------------------------------------------------
	testfw::MutableRow(*g_pSkillInfoTable, kChild).Set(9, "Double Impact", 1, 0, 0, "Double Impact");

	domain.SetNewSkill();
	CHECK_EQ(false, domain.LearnSkill(kChild));

	// Refused before it was made usable, so nothing is left behind.
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NEXT, (int)domain.GetSkillStatus(kChild));
	CHECK_EQ(false, g_pSkillAvailable->IsEnableSkill(kChild));

	// A negative level is no level either.
	testfw::MutableRow(*g_pSkillInfoTable, kChild).Set(-1, "Double Impact", 1, 0, 0, "Double Impact");
	domain.SetNewSkill();
	CHECK_EQ(false, domain.LearnSkill(kChild));
	CHECK_EQ(false, g_pSkillAvailable->IsEnableSkill(kChild));

	// And the skill at a level the domain does hold still learns.
	testfw::MutableRow(*g_pSkillInfoTable, kChild).Set(1, "Double Impact", 1, 0, 0, "Double Impact");
	domain.SetNewSkill();
	CHECK(domain.LearnSkill(kChild));
	CHECK(g_pSkillAvailable->IsEnableSkill(kChild));
}

TEST(SkillDomain, TheSkillTheRacesShareTakesTheStepOfTheDomainItJoins)
{
	SkillWorld world;

	// The one skill every race has sits in a step of its own for each
	// of them, and the domain it is added to is what says which.
	testfw::MutableRow(*g_pSkillInfoTable, SKILL_SOUL_CHAIN).SetSkillStep(SKILL_STEP_ETC);

	g_pSkillManager->Init();

	testfw::MutableRow(*g_pSkillManager, SKILLDOMAIN_VAMPIRE).SetRootSkill(SKILL_SOUL_CHAIN, false);
	testfw::MutableRow(*g_pSkillManager, SKILLDOMAIN_OUSTERS).SetRootSkill(SKILL_SOUL_CHAIN, false);

	const MSkillDomain::SKILL_STEP_LIST* pList;

	// The vampire's own innate step, the ousters' own etc step, and
	// for anyone else the apprentices - never the step the file gave.
	pList = (*g_pSkillManager)[SKILLDOMAIN_VAMPIRE].GetSkillStepList(SKILL_STEP_VAMPIRE_INNATE);
	CHECK(pList != NULL);
	CHECK((*g_pSkillManager)[SKILLDOMAIN_VAMPIRE].GetSkillStepList(SKILL_STEP_ETC) == NULL);

	pList = (*g_pSkillManager)[SKILLDOMAIN_OUSTERS].GetSkillStepList(SKILL_STEP_OUSTERS_ETC);
	CHECK(pList != NULL);
	CHECK((*g_pSkillManager)[SKILLDOMAIN_OUSTERS].GetSkillStepList(SKILL_STEP_ETC) == NULL);

	pList = (*g_pSkillManager)[SKILLDOMAIN_ETC].GetSkillStepList(SKILL_STEP_APPRENTICE);
	CHECK(pList != NULL);
	CHECK((*g_pSkillManager)[SKILLDOMAIN_ETC].GetSkillStepList(SKILL_STEP_ETC) == NULL);

	if (pList != NULL)
	{
		CHECK_EQ(1, (int)pList->size());
		CHECK_EQ((int)SKILL_SOUL_CHAIN, (int)(*pList)[0]);
	}

	// A skill whose step the file did name keeps it.
	testfw::MutableRow(*g_pSkillInfoTable, kRoot).SetSkillStep(SKILL_STEP_MASTER);
	MSkillDomain domain;
	domain.SetRootSkill(kRoot);
	CHECK(domain.GetSkillStepList(SKILL_STEP_MASTER) != NULL);
}

TEST(SkillDomain, ALoadOverALiveDomainLeavesNoneOfTheOldTreeBehind)
{
	SkillWorld world;

	// Two skills in one step, one in another.
	testfw::MutableRow(*g_pSkillInfoTable, kRoot).SetSkillStep(SKILL_STEP_APPRENTICE);
	testfw::MutableRow(*g_pSkillInfoTable, kChild).SetSkillStep(SKILL_STEP_APPRENTICE);
	testfw::MutableRow(*g_pSkillInfoTable, kLeaf).SetSkillStep(SKILL_STEP_ADEPT);

	MSkillDomain domain;
	domain.SetRootSkill(kRoot);

	const MSkillDomain::SKILL_STEP_LIST* pList = domain.GetSkillStepList(SKILL_STEP_APPRENTICE);
	CHECK(pList != NULL);
	if (pList != NULL)
		CHECK_EQ(2, (int)pList->size());
	CHECK(domain.GetSkillStepList(SKILL_STEP_ADEPT) != NULL);

	//------------------------------------------------------------------
	// A file naming one skill of the tree and nothing else. The step
	// lists follow the skill list, so the step the loaded skill is not
	// in must be gone and the one it is in must hold only it - the
	// lists used to be freed by the destructor alone, so every rebuild
	// added to lists still naming the tree before it.
	//------------------------------------------------------------------
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int rows = 1;
		const WORD id = (WORD)kChild;
		const BYTE status = (BYTE)MSkillDomain::SKILLSTATUS_NEXT;
		out.write((const char*)&rows, 4);
		out.write((const char*)&id, 2);
		out.write((const char*)&status, 1);
	}
	{
		std::ifstream in(kTempFile, std::ios::binary);
		domain.LoadFromFile(in);
	}

	CHECK_EQ(1, domain.GetSize());
	CHECK(domain.GetSkillStepList(SKILL_STEP_ADEPT) == NULL);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_ADEPT) == FALSE);

	pList = domain.GetSkillStepList(SKILL_STEP_APPRENTICE);
	CHECK(pList != NULL);
	if (pList != NULL)
	{
		CHECK_EQ(1, (int)pList->size());
		CHECK_EQ((int)kChild, (int)(*pList)[0]);
	}

	// The skill-info packet asks for the same rebuild every login, so
	// it must not grow the lists either.
	g_pSkillManager->Init();
	g_pSkillManager->InitSkillList();
	g_pSkillManager->InitSkillList();

	pList = (*g_pSkillManager)[SKILLDOMAIN_BLADE].GetSkillStepList(SKILL_STEP_APPRENTICE);
	CHECK(pList != NULL);
	if (pList != NULL)
		CHECK_EQ(2, (int)pList->size());

	std::remove(kTempFile);
}

TEST(SkillDomain, AnExperienceRowThatCannotBeStoredCostsOnlyItself)
{
	SkillWorld world;
	MSkillDomain domain;

	const int	kBadGoal	= 0x0BADBAD0;
	const int	kGoodGoal	= 0x00ABCDEF;

	//------------------------------------------------------------------
	// Two rows, the first for a level the table cannot hold. Refusing
	// it must still take its eight payload bytes off the stream, or
	// the next read starts in the middle of a row - and the manager's
	// loop reads the next domain from wherever this leaves it.
	//------------------------------------------------------------------
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int	bad		= 900;
		const int	good	= 7;
		const unsigned int	accum = 0;
		out.write((const char*)&bad, 4);
		out.write((const char*)&kBadGoal, 4);
		out.write((const char*)&accum, 4);
		out.write((const char*)&good, 4);
		out.write((const char*)&kGoodGoal, 4);
		out.write((const char*)&accum, 4);
	}
	{
		std::ifstream in(kTempFile, std::ios::binary);
		domain.LoadFromFileServerDomainInfo(in);
		domain.LoadFromFileServerDomainInfo(in);
	}

	CHECK_EQ(kGoodGoal, domain.GetExpInfo(7).GoalExp);
	CHECK(domain.GetExpInfo(900).GoalExp != kBadGoal);

	std::remove(kTempFile);
}

TEST(SkillDomain, TheGlobalsTheExecutableOwnsMayBeGone)
{
	SkillWorld world;

	// The usable-skill set is created at start-up and deleted at
	// shutdown, and the manager is a global of its own; library code
	// runs either side of both.
	MSkillSet*		pSet = g_pSkillAvailable;
	MSkillManager*	pManager = g_pSkillManager;

	testfw::MutableRow(*g_pSkillInfoTable, SKILL_SOUL_CHAIN).SetSkillStep(SKILL_STEP_ETC);

	g_pSkillAvailable = NULL;
	g_pSkillManager = NULL;

	MSkillDomain domain;

	// Without the manager there is no domain to compare against, so
	// the shared skill keeps the step the file gave it.
	domain.SetRootSkill(SKILL_SOUL_CHAIN);
	CHECK(domain.GetSkillStepList(SKILL_STEP_ETC) != NULL);

	domain.SetRootSkill(kRoot, false);
	domain.SetNewSkill();
	CHECK(domain.LearnSkill(kRoot));
	CHECK(domain.UnLearnSkill(kRoot));
	domain.ClearSkillList();
	CHECK_EQ(0, domain.GetSize());

	g_pSkillAvailable = pSet;
	g_pSkillManager = pManager;
}

//----------------------------------------------------------------------
// The legacy wrap
//----------------------------------------------------------------------
TEST(SkillInfoNode, UseDelayAcrossTheLegacyWrapStillWaits)
{
	// The DWORD sum wrapped: (2^32 - 1000) + 3000 is 2000 mod 2^32, and
	// "clock >= 2000" made the skill available at once.
	const DWORD dw_now = (DWORD)(0x100000000ull - 1000);
	CHECK(dw_now >= (DWORD)(dw_now + 3000));

	SkillWorld world;
	SKILLINFO_NODE node;
	node.SetDelayTime(3000);

	s_Now = MonotonicClock::FromMillis(0x100000000ull - 1000);
	node.SetNextAvailableTime();
	CHECK_EQ(false, node.IsAvailableTime());
	CHECK_EQ(3000, (int)node.GetAvailableTimeLeft());
	s_Now = MonotonicClock::FromMillis(0x100000000ull + 1999);
	CHECK_EQ(false, node.IsAvailableTime());
	CHECK_EQ(1, (int)node.GetAvailableTimeLeft());
	s_Now = MonotonicClock::FromMillis(0x100000000ull + 2000);
	CHECK(node.IsAvailableTime());
	CHECK_EQ(0, (int)node.GetAvailableTimeLeft());
}
