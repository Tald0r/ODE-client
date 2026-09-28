#include "test_framework.h"
#include "ZoneFileHeader.h"
#include "MImageObject.h"
#include "MShadowObject.h"
#include "MAnimationObject.h"
#include "MShadowAnimationObject.h"
#include "MInteractionObject.h"
#include "ZoneMapData.h"
#include <cstdio>
#include <cstring>
#include <memory>

TEST(ZoneMapRecords, RealRecordClassesLinkWithoutGameGlobals)
{
	MImageObject image;
	MShadowObject shadow;
	MAnimationObject animation;
	MShadowAnimationObject shadowAnimation;
	MInteractionObject interaction;
	CHECK_EQ(MObject::TYPE_IMAGEOBJECT, image.GetObjectType());
	CHECK_EQ(MObject::TYPE_SHADOWOBJECT, shadow.GetObjectType());
	CHECK_EQ(MObject::TYPE_ANIMATIONOBJECT, animation.GetObjectType());
	CHECK_EQ(MObject::TYPE_SHADOWANIMATIONOBJECT, shadowAnimation.GetObjectType());
	CHECK_EQ(MObject::TYPE_INTERACTIONOBJECT, interaction.GetObjectType());

	const char* path = "zone_map_records_test.bin";
	FILEINFO_ZONE_HEADER header;
	header.ZoneID = 42;
	header.ZoneGroupID = 7;
	header.ZoneName = "zone";
	header.ZoneType = 1;
	header.ZoneLevel = 2;
	header.Description = "description";
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		header.SaveToFile(out);
		CHECK(bool(out));
	}
	{
		std::ifstream in(path, std::ios::binary);
		FILEINFO_ZONE_HEADER loaded;
		loaded.LoadFromFile(in);
		CHECK(bool(in));
		CHECK_EQ(42, loaded.ZoneID);
		CHECK_EQ(7, loaded.ZoneGroupID);
		CHECK(loaded.ZoneName == "zone");
		CHECK(loaded.Description == "description");
	}
	std::remove(path);
}

TEST(ZoneMapRecords, TheImageBaseDestroysTheOwnedDerivedRecord)
{
	bool destroyed = false;
	struct Derived : MImageObject {
		bool& destroyed;
		explicit Derived(bool& flag) : destroyed(flag) {}
		~Derived() { destroyed = true; }
	};
	{
		std::unique_ptr<MImageObject> record = std::make_unique<Derived>(destroyed);
	}
	CHECK(destroyed);
}

TEST(ZoneMapRecords, ClipSectorRectKeepsAnInRangeRect)
{
	int left = 2, top = 3, right = 7, bottom = 9;
	CHECK(ClipSectorRect(left, top, right, bottom, 20, 20));
	CHECK_EQ(2, left);
	CHECK_EQ(3, top);
	CHECK_EQ(7, right);
	CHECK_EQ(9, bottom);

	left = -3; top = -1; right = 40; bottom = 25;
	CHECK(ClipSectorRect(left, top, right, bottom, 20, 20));
	CHECK_EQ(0, left);
	CHECK_EQ(0, top);
	CHECK_EQ(19, right);
	CHECK_EQ(19, bottom);
}

TEST(ZoneMapRecords, ClipSectorRectOrdersInvertedEdges)
{
	int left = 10, top = 8, right = 5, bottom = 4;
	CHECK(ClipSectorRect(left, top, right, bottom, 20, 20));
	CHECK_EQ(5, left);
	CHECK_EQ(10, right);
	CHECK_EQ(4, top);
	CHECK_EQ(8, bottom);
}

TEST(ZoneMapRecords, ClipSectorRectRejectsARectRightOfTheGrid)
{
	// Clamping before ordering would give 19..30 once the swap is repaired,
	// and index past the row.
	int left = 30, top = 0, right = 40, bottom = 0;
	CHECK(!ClipSectorRect(left, top, right, bottom, 20, 20));
}

TEST(ZoneMapRecords, ClipSectorRectRejectsARectBelowTheGrid)
{
	// Clamping before ordering gives rows 19..30, past the last row.
	int left = 0, top = 30, right = 0, bottom = 35;
	CHECK(!ClipSectorRect(left, top, right, bottom, 20, 20));
}

TEST(ZoneMapRecords, ClipSectorRectRejectsAnEmptyGrid)
{
	int left = 0, top = 0, right = 0, bottom = 0;
	CHECK(!ClipSectorRect(left, top, right, bottom, 0, 0));
}
