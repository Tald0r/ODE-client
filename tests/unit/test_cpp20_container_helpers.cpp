//----------------------------------------------------------------------
// test_cpp20_container_helpers.cpp
//----------------------------------------------------------------------
//
// The library sites converted to the C++20 container and string helpers
// (docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md, the
// post-migration backlog's priority 2): map/set membership written as
// contains() instead of find() != end(), and the two line-trimming
// tests written as starts_with()/ends_with() instead of an index into
// the string.
//
// The conversion is meant to be invisible at run time, so these tests
// pin the OBSERVABLE contract of each converted function rather than
// its spelling: present versus absent, the empty container, the empty
// line, and the equal-length line (a line that is nothing but the
// character being tested for). They were written against the
// pre-conversion code and pass unchanged after it.
//
// Every check here goes through a public entry point of the library the
// converted line lives in; nothing reaches into a container directly.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"
#include "packet_stream_access.h"

#include "PacketIDSet.h"
#include "PlayerStatus.h"
#include "Properties.h"
#include "ScriptParameter.h"
#include "Gpackets/GCNPCAskVariable.h"
#include "Gpackets/GCTimeLimitItemInfo.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "Exception.h"

#include "gamemodel_world.h"
#include "MItemManager.h"
#include "MSortedItemManager.h"
#include "MSkillManager.h"
#include "MTimeItemManager.h"
#include "SystemAvailabilities.h"
#include "SkillDef.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

//----------------------------------------------------------------------
// The wire fixture: a small input ring over a never-used socket, the
// same seam test_packetwire_parsers.cpp uses.
//----------------------------------------------------------------------
struct StreamFixture
{
	Socket				m_Socket;
	SocketInputStream	m_Stream;

	explicit StreamFixture(uint bufferLen = 64)
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, bufferLen)
	{
	}

	void	Preload(const std::vector<unsigned char>& wire)
	{
		SocketInputStreamTestAccess::Preload(m_Stream, &wire[0], (uint)wire.size());
	}
};

//----------------------------------------------------------------------
// Little-endian four-byte field, matching what the stream reads back.
//----------------------------------------------------------------------
void	AppendDword(std::vector<unsigned char>& wire, DWORD value)
{
	for (int i = 0; i < 4; i++)
		wire.push_back((unsigned char)((value >> (8 * i)) & 0xFF));
}

//----------------------------------------------------------------------
// The gamemodel fixtures. One item class of one 1 x 1 row is all the
// two id-keyed managers need from the item table.
//----------------------------------------------------------------------
int		s_Alive = 0;

struct HelperWorld : GameModelWorld
{
	HelperWorld()
	{
		s_Alive = 0;
		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).SetGrid(1, 1);
	}
};

struct Sword : public MItem
{
	Sword(TYPE_OBJECTID id)			{ SetID(id); SetItemType(0); s_Alive++; }
	~Sword()						{ s_Alive--; }
	ITEM_CLASS	GetItemClass() const	{ return ITEM_CLASS_SWORD; }
};

//----------------------------------------------------------------------
// The skill fixture: a three-step chain, one level apart, so
// MSkillDomain::AddSkill has children to walk down into.
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

const char* const	kPropertiesFile = "cpp20_container_helpers_test.conf";

} // namespace

//----------------------------------------------------------------------
// PacketIDSet::hasPacketID - std::set membership
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers, PacketIDSetAnswersMembershipPerSetType)
{
	// An empty ordinary set holds nothing.
	PacketIDSet empty(CPS_NONE, PacketIDSet::PIST_NORMAL);
	CHECK_EQ(false, empty.hasPacketID((PacketID_t)11));

	PacketIDSet normal(CPS_NONE, PacketIDSet::PIST_NORMAL);
	normal.addPacketID((PacketID_t)11);
	normal.addPacketID((PacketID_t)22);

	CHECK(normal.hasPacketID((PacketID_t)11));
	CHECK(normal.hasPacketID((PacketID_t)22));
	CHECK_EQ(false, normal.hasPacketID((PacketID_t)33));
	CHECK_EQ(false, normal.hasPacketID((PacketID_t)0));

	// The same id twice is refused, and the set is unchanged by it.
	bool bThrew = false;
	try {
		normal.addPacketID((PacketID_t)11);
	} catch (DuplicatedException&) {
		bThrew = true;
	}
	CHECK(bThrew);
	CHECK(normal.hasPacketID((PacketID_t)11));

	// PIST_ANY allows everything without consulting the set, and
	// PIST_NONE allows nothing.
	PacketIDSet any(CPS_NONE, PacketIDSet::PIST_ANY);
	CHECK(any.hasPacketID((PacketID_t)11));
	PacketIDSet none(CPS_NONE, PacketIDSet::PIST_NONE);
	CHECK_EQ(false, none.hasPacketID((PacketID_t)11));
}

TEST(Cpp20ContainerHelpers, PacketIDSetIgnoreExceptThrowsForAnAbsentID)
{
	PacketIDSet ignore(CPS_NONE, PacketIDSet::PIST_IGNORE_EXCEPT);
	ignore.addPacketID((PacketID_t)11);

	CHECK(ignore.hasPacketID((PacketID_t)11));

	bool bThrew = false;
	try {
		ignore.hasPacketID((PacketID_t)12);
	} catch (IgnorePacketException&) {
		bThrew = true;
	}
	CHECK(bThrew);
}

//----------------------------------------------------------------------
// GCTimeLimitItemInfo - std::map membership on the wire and behind the
// packet's own accessor
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers, TimeLimitItemInfoRefusesADuplicateObjectID)
{
	GCTimeLimitItemInfo packet;

	CHECK_EQ(0xffff, (int)packet.getTimeLimit(1));		// empty map

	packet.addTimeLimit(1, 100);
	CHECK_EQ(100, (int)packet.getTimeLimit(1));
	CHECK_EQ(0xffff, (int)packet.getTimeLimit(2));

	bool bThrew = false;
	try {
		packet.addTimeLimit(1, 200);
	} catch (Error&) {
		bThrew = true;
	}
	CHECK(bThrew);

	// The refused call left the first value in place.
	CHECK_EQ(100, (int)packet.getTimeLimit(1));
	CHECK_EQ(1, (int)packet.m_TimeLimitItemInfos.size());
}

TEST(Cpp20ContainerHelpers, TimeLimitItemInfoReadKeepsTheFirstOfADuplicatePair)
{
	StreamFixture f;

	std::vector<unsigned char> wire;
	wire.push_back(3);				// three entries, two of them the same id
	AppendDword(wire, 7);	AppendDword(wire, 100);
	AppendDword(wire, 7);	AppendDword(wire, 200);
	AppendDword(wire, 8);	AppendDword(wire, 300);
	f.Preload(wire);

	GCTimeLimitItemInfo packet;
	packet.read(f.m_Stream);

	CHECK_EQ(2, (int)packet.m_TimeLimitItemInfos.size());
	CHECK_EQ(100, (int)packet.getTimeLimit(7));		// the first one wins
	CHECK_EQ(300, (int)packet.getTimeLimit(8));
	CHECK_EQ(0xffff, (int)packet.getTimeLimit(9));
}

//----------------------------------------------------------------------
// GCNPCAskVariable::addScriptParameter - std::map<std::string,...>
// membership
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers, NPCAskVariableRefusesADuplicateParameterName)
{
	GCNPCAskVariable packet;

	ScriptParameter* first = new ScriptParameter;
	first->setName("gold");
	first->setValue("100");
	packet.addScriptParameter(first);		// the packet owns it now

	ScriptParameter* twin = new ScriptParameter;
	twin->setName("gold");
	twin->setValue("200");

	bool bThrew = false;
	try {
		packet.addScriptParameter(twin);
	} catch (DuplicatedException&) {
		bThrew = true;
	}
	CHECK(bThrew);
	delete twin;							// refused, so still the test's

	ScriptParameter* other = new ScriptParameter;
	other->setName("silver");
	other->setValue("5");
	packet.addScriptParameter(other);

	CHECK_EQ(2, (int)packet.getScriptParameters().size());
	CHECK(packet.getValue("gold") == "100");
	CHECK(packet.getValue("silver") == "5");
	// A name the packet does not carry comes back unchanged.
	CHECK(packet.getValue("copper") == "copper");
}

//----------------------------------------------------------------------
// Properties::load - starts_with for the comment line, the empty line
// through the same test
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers, PropertiesSkipsCommentAndEmptyLines)
{
	{
		std::ofstream out(kPropertiesFile, std::ios::out | std::ios::trunc);
		out << "# a comment line, with a separator in it : ignored\n";
		out << "#\n";			// a line that is nothing but the comment character
		out << "\n";			// an empty line
		out << "MODE : MODE_IV\n";
		out << "NumOfPlayerThreads : 4\n";
	}

	Properties prop;
	prop.load(kPropertiesFile);

	CHECK(prop.getProperty("MODE") == "MODE_IV");
	CHECK_EQ(4, prop.getPropertyInt("NumOfPlayerThreads"));

	// The comment lines never became properties.
	bool bThrew = false;
	try {
		prop.getProperty("# a comment line, with a separator in it");
	} catch (NoSuchElementException&) {
		bThrew = true;
	}
	CHECK(bThrew);

	std::remove(kPropertiesFile);
}

//----------------------------------------------------------------------
// SystemAvailabilitiesManager::LoadFromStream - ends_with for the
// carriage return a Windows-written script file leaves on every line.
//
// The blank line inside the script block is the point: the loader
// counts one script row per line it does not skip, so a blank line that
// still carried its '\r' would be parsed as a row, close the block one
// line early and lose the row that follows it.
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers, SystemAvailabilitiesIgnoresABlankCarriageReturnLine)
{
	const char* const kScript =
		"; system 4 (guild) refuses two scripts\r\n"
		"*4 2\r\n"
		"10 2\r\n"
		"\r\n"				// blank line in the middle of the block
		"11 2\r\n";

	SystemAvailabilitiesManager m;
	std::istringstream in(kScript);
	CHECK(m.LoadFromStream(in));

	// Every system open: nothing is filtered at all.
	CHECK(m.ScriptFiltering(10, 1));
	CHECK(m.ScriptFiltering(11, 1));

	// Close system 4: both rows of the block are in force, which they
	// only are if the blank line was skipped rather than counted.
	m.SetFlag(~(1u << 4));
	CHECK_EQ(false, m.ScriptFiltering(10, 1));
	CHECK_EQ(false, m.ScriptFiltering(11, 1));
	CHECK(m.ScriptFiltering(12, 1));
	CHECK(m.ScriptFiltering(10, 0));
}

//----------------------------------------------------------------------
// MTimeItemManager::IsExist - std::map membership, including the empty
// container the old code short-circuited on
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers, TimeItemManagerIsExistFollowsTheRegister)
{
	MTimeItemManager reg;

	CHECK_EQ(false, reg.IsExist(1));		// empty register

	CHECK(reg.AddTimeItem(1, 60));
	CHECK(reg.IsExist(1));
	CHECK_EQ(false, reg.IsExist(2));

	CHECK(reg.AddTimeItem(2, 60));
	CHECK(reg.IsExist(1));
	CHECK(reg.IsExist(2));

	CHECK(reg.RemoveTimeItem(1));
	CHECK_EQ(false, reg.IsExist(1));
	CHECK(reg.IsExist(2));

	// Removing what is not there reports false and changes nothing.
	CHECK_EQ(false, reg.RemoveTimeItem(1));
	CHECK(reg.IsExist(2));

	CHECK(reg.RemoveTimeItem(2));
	CHECK_EQ(false, reg.IsExist(2));		// empty again
}

//----------------------------------------------------------------------
// MItemManager::AddItem - one item per object id
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers, ItemManagerHoldsOneItemPerObjectID)
{
	HelperWorld world;
	MItemManager manager;

	Sword* first = new Sword(1);
	Sword* twin = new Sword(1);			// the same id again
	Sword* other = new Sword(2);

	CHECK(manager.AddItem(first));
	CHECK_EQ(false, manager.AddItem(twin));
	CHECK(manager.AddItem(other));
	CHECK_EQ(2, manager.GetItemNum());

	// The first item is still the one the id answers with.
	CHECK(manager.GetItem(1) == first);
	CHECK(manager.GetItem(2) == other);
	CHECK(manager.GetItem(3) == NULL);

	// A removed id can be taken again.
	CHECK(manager.RemoveItem(1) == first);
	CHECK(manager.AddItem(twin));
	CHECK(manager.GetItem(1) == twin);

	delete first;
	manager.Release();
	CHECK_EQ(0, s_Alive);
}

//----------------------------------------------------------------------
// MSortedItemManager::AddItem - one item per sort key (footprint plus
// object id)
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers, SortedItemManagerHoldsOneItemPerKey)
{
	HelperWorld world;
	MSortedItemManager sorted;

	Sword* first = new Sword(1);
	Sword* twin = new Sword(1);			// same footprint, same id: same key
	Sword* other = new Sword(2);

	CHECK(sorted.AddItem(first));
	CHECK_EQ(false, sorted.AddItem(twin));
	CHECK(sorted.AddItem(other));
	CHECK_EQ(2, (int)sorted.size());

	delete twin;						// refused, so still the test's
	sorted.Release();
	CHECK_EQ(0, s_Alive);
}

//----------------------------------------------------------------------
// MSkillDomain::AddSkill - the tree walk adds each skill below the root
// exactly once
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers, SkillDomainAddsEachSkillBelowTheRootOnce)
{
	SkillWorld world;
	MSkillDomain domain;

	CHECK_EQ(0, domain.GetSize());
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NULL, (int)domain.GetSkillStatus(kRoot));

	// The root pulls the whole chain in.
	domain.SetRootSkill(kRoot);
	CHECK_EQ(3, domain.GetSize());
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NEXT, (int)domain.GetSkillStatus(kRoot));
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_OTHER, (int)domain.GetSkillStatus(kChild));
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_OTHER, (int)domain.GetSkillStatus(kLeaf));

	// Adding a skill the domain already holds changes nothing.
	CHECK_EQ(false, domain.AddSkill(kRoot));
	CHECK_EQ(false, domain.AddSkill(kChild));
	CHECK_EQ(false, domain.AddSkill(kLeaf));
	CHECK_EQ(3, domain.GetSize());
	CHECK_EQ((int)MSkillDomain::SKILLSTATUS_NEXT, (int)domain.GetSkillStatus(kRoot));
}
