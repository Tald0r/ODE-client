//----------------------------------------------------------------------
// test_virtual_destructors.cpp
//----------------------------------------------------------------------
//
// Library classes that have virtual functions and are deleted through a
// pointer to themselves carry a virtual destructor, so a delete through
// that pointer runs the destructor of the object's real class.
//
// Beside them, the value nodes whose hand-written copy assignment was
// replaced by the implicit one: both copies still copy every field.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MItem.h"				// first: it carries the platform types the other headers use
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MInventory.h"
#include "MSkillManager.h"
#include "PetInfo.h"
#include "basic/CPositionList.h"

#include <type_traits>

TEST(VirtualDestructors, GearAndInventory)
{
	CHECK(std::has_virtual_destructor_v<MSlayerGear>);
	CHECK(std::has_virtual_destructor_v<MVampireGear>);
	CHECK(std::has_virtual_destructor_v<MOustersGear>);
	CHECK(std::has_virtual_destructor_v<MInventory>);
}

TEST(VirtualDestructors, PetInfo)
{
	CHECK(std::has_virtual_destructor_v<PetInfo>);
}

TEST(ImplicitCopy, PositionNodeCopiesBothCoordinates)
{
	POSITION_NODE<unsigned short> source{};
	source.X = 12;
	source.Y = 34;

	POSITION_NODE<unsigned short> constructed(source);
	CHECK_EQ(12, constructed.X);
	CHECK_EQ(34, constructed.Y);

	POSITION_NODE<unsigned short> assigned{};
	assigned = source;
	CHECK_EQ(12, assigned.X);
	CHECK_EQ(34, assigned.Y);
}

TEST(ImplicitCopy, SkillIdNodeCopiesSkillAndFlag)
{
	const SKILLID_NODE source(MAX_ACTIONINFO, 0);

	SKILLID_NODE constructed(source);
	CHECK_EQ(MAX_ACTIONINFO, constructed.SkillID);
	CHECK_EQ(0, constructed.Flag);

	SKILLID_NODE assigned(SKILL_ATTACK_MELEE);
	CHECK_EQ(FLAG_SKILL_ENABLE, assigned.Flag);
	assigned = source;
	CHECK_EQ(MAX_ACTIONINFO, assigned.SkillID);
	CHECK_EQ(0, assigned.Flag);
}
