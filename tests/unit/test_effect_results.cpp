#include "test_framework.h"
#include "MActionResult.h"
#include "MEffectTarget.h"
#include "MZoneTable.h"

#include <cstring>
#include <functional>
#include <memory>
#include <set>
#include <vector>

namespace {

std::vector<int> events;
std::set<MActionResultNode*> allocated;

struct Node : MActionResultNode
{
	int id;
	std::function<void()> onExecute, onDestroy;
	explicit Node(int value) : id(value) { allocated.insert(this); }
	~Node() override
	{
		events.push_back(-id);
		allocated.erase(this);
		if (onDestroy) onDestroy();
	}
	void Execute() override
	{
		events.push_back(id);
		if (onExecute) onExecute();
	}
};

const MEffectTargetHost host{
	.RemoveFromPlayer = [](BYTE id) { events.push_back(1000 + id); },
};
const MEffectTargetHost otherHost{
	.RemoveFromPlayer = [](BYTE id) { events.push_back(2000 + id); },
};

struct World
{
	CZoneTable zones;
	CZoneTable* previousZones = g_pZoneTable;
	const MEffectTargetHost* previousHost = MEffectTarget::SetHost(nullptr);
	BYTE previousID = MEffectTarget::s_EffectID;
	World()
	{
		g_pZoneTable = &zones;
		MEffectTarget::s_EffectID = 0;
		events.clear();
	}
	~World()
	{
		CHECK(allocated.empty());
		// Preserve later cases when a failing ownership probe leaves a node.
		while (!allocated.empty()) delete *allocated.begin();
		g_pZoneTable = previousZones;
		MEffectTarget::SetHost(previousHost);
		MEffectTarget::s_EffectID = previousID;
	}
};

MActionResult* Result(int id)
{
	auto result = std::make_unique<MActionResult>();
	result->Add(new Node(id));
	return result.release();
}

void Initialize(MEffectTarget& target)
{
	target.Set(-12, 34, 56, 789);
	target.SetServerID(912);
	target.SetDelayFrame(45);
	target.NextPhase();
	target.NewEffectID();
}

} // namespace

TEST(EffectResults, EmptyQueuesIgnoreNullAndCanBeClearedOrExecutedRepeatedly)
{
	World world;
	MActionResult result;
	result.Add(nullptr);
	CHECK(result.IsEmpty());
	CHECK_EQ(0, result.GetSize());
	result.Execute();
	result.Release();
	result.Execute();
	CHECK(events.empty());
}

TEST(EffectResults, ExecuteUsesInsertionOrderAndDeletesEachNodeAfterItsAction)
{
	World world;
	MActionResult result;
	for (int id : {1, 2, 3}) result.Add(new Node(id));
	CHECK_EQ(3, result.GetSize());
	CHECK_EQ(1, static_cast<const Node*>(*result.GetIterator())->id);
	CHECK(events.empty());
	result.Execute();
	CHECK(events == std::vector<int>({1, -1, 2, -2, 3, -3}));
	CHECK(result.IsEmpty());
	CHECK(allocated.empty());
}

TEST(EffectResults, ReleaseAndDestructionDiscardPendingActions)
{
	World world;
	{
		MActionResult result;
		result.Add(new Node(1));
		result.Add(new Node(2));
		result.Release();
		CHECK(result.IsEmpty());
		CHECK(events == std::vector<int>({-1, -2}));
		result.Add(new Node(3));
	}
	CHECK(events == std::vector<int>({-1, -2, -3}));
}

TEST(EffectResults, TheExecutingNodeIsRemovedBeforeItsCallback)
{
	World world;
	MActionResult result;
	auto* first = new Node(1);
	auto* second = new Node(2);
	first->onExecute = [&]() {
		CHECK_EQ(1, result.GetSize());
		CHECK(*result.GetIterator() == second);
	};
	result.Add(first);
	result.Add(second);
	result.Execute();
}

TEST(EffectResults, ActionsAppendedDuringExecutionRunInTheSamePass)
{
	World world;
	MActionResult result;
	auto* first = new Node(1);
	first->onExecute = [&]() { result.Add(new Node(3)); };
	result.Add(first);
	result.Add(new Node(2));
	result.Execute();
	CHECK(events == std::vector<int>({1, -1, 2, -2, 3, -3}));
}

TEST(EffectResults, ClearingFromAnActionDiscardsTheRemainingNodes)
{
	World world;
	MActionResult result;
	auto* first = new Node(1);
	first->onExecute = [&]() { result.Release(); };
	result.Add(first);
	result.Add(new Node(2));
	result.Execute();
	CHECK(events == std::vector<int>({1, -2, -1}));
	CHECK(result.IsEmpty());
}

TEST(EffectResults, RecursiveExecutionDrainsPendingActionsOnce)
{
	World world;
	MActionResult result;
	auto* first = new Node(1);
	first->onExecute = [&]() { result.Execute(); };
	result.Add(first);
	result.Add(new Node(2));
	result.Execute();
	CHECK(events == std::vector<int>({1, 2, -2, -1}));
	CHECK(result.IsEmpty());
}

TEST(EffectResults, ANodeDestructorCanAppendWorkDuringExecution)
{
	World world;
	MActionResult result;
	auto* first = new Node(1);
	first->onDestroy = [&]() { result.Add(new Node(3)); };
	result.Add(first);
	result.Add(new Node(2));
	result.Execute();
	CHECK(events == std::vector<int>({1, -1, 2, -2, 3, -3}));
}

TEST(EffectResults, TargetDefaultsExposeEmptyResultsAndInitialProgress)
{
	World world;
	MEffectTarget target(3);
	CHECK_EQ(MEffectTarget::EFFECT_TARGET_NORMAL, target.GetEffectTargetType());
	CHECK_EQ(3, target.GetMaxPhase());
	CHECK_EQ(0, target.GetCurrentPhase());
	CHECK_EQ(0, target.GetDelayFrame());
	CHECK_EQ(0, target.GetEffectID());
	CHECK(target.IsResultEmpty());
	CHECK(!target.IsExistResult());
	CHECK(!target.IsResultTime());
	CHECK(!target.IsEnd());
}

TEST(EffectResults, TargetSettersAndPhaseProgressAreIndependent)
{
	World world;
	MEffectTarget target(2), empty(0);
	Initialize(target);
	CHECK_EQ(-12, target.GetX());
	CHECK_EQ(34, target.GetY());
	CHECK_EQ(56, target.GetZ());
	CHECK_EQ(789, target.GetID());
	CHECK_EQ(912, target.GetServerID());
	CHECK_EQ(45, target.GetDelayFrame());
	CHECK_EQ(1, target.GetCurrentPhase());
	CHECK(!target.IsEnd());
	target.NextPhase();
	CHECK(target.IsEnd());
	CHECK(empty.IsEnd());
	target.SetResultTime();
	CHECK(target.IsResultTime());
}

TEST(EffectResults, EffectIdsKeepTheLegacyByteSequenceAndWrap)
{
	World world;
	MEffectTarget::s_EffectID = 254;
	MEffectTarget first(1), second(1), third(1);
	first.NewEffectID();
	second.NewEffectID();
	third.NewEffectID();
	CHECK_EQ(254, first.GetEffectID());
	CHECK_EQ(255, second.GetEffectID());
	CHECK_EQ(0, third.GetEffectID());
	CHECK_EQ(1, MEffectTarget::s_EffectID);
}

TEST(EffectResults, TargetCopyKeepsProgressButDoesNotShareOwnedResults)
{
	World world;
	MEffectTarget::s_EffectID = 17;
	MEffectTarget source(3);
	Initialize(source);
	source.SetResultTime();
	source.SetResult(Result(1));
	MEffectTarget copy(source);
	CHECK_EQ(3, copy.GetMaxPhase());
	CHECK_EQ(1, copy.GetCurrentPhase());
	CHECK_EQ(-12, copy.GetX());
	CHECK_EQ(34, copy.GetY());
	CHECK_EQ(56, copy.GetZ());
	CHECK_EQ(789, copy.GetID());
	CHECK_EQ(OBJECTID_NULL, copy.GetServerID());
	CHECK_EQ(45, copy.GetDelayFrame());
	CHECK_EQ(17, copy.GetEffectID());
	CHECK(!copy.IsResultTime());
	CHECK(copy.IsResultEmpty());
	CHECK_EQ(1, allocated.size());
}

TEST(EffectResults, BaseAssignmentKeepsDestinationIdentityAndOwnedResult)
{
	World world;
	MEffectTarget source(3), destination(5);
	Initialize(source);
	destination.NewEffectID();
	destination.SetResultTime();
	auto* result = Result(1);
	destination.SetResult(result);
	destination = source;
	CHECK_EQ(3, destination.GetMaxPhase());
	CHECK_EQ(1, destination.GetCurrentPhase());
	CHECK_EQ(-12, destination.GetX());
	CHECK_EQ(912, destination.GetServerID());
	CHECK_EQ(1, destination.GetEffectID());
	CHECK(destination.IsResultTime());
	CHECK(destination.GetResult() == result);
	CHECK(events.empty());
}

TEST(EffectResults, ResultReplacementDeletesThePreviouslyOwnedQueue)
{
	World world;
	MEffectTarget target(1);
	target.SetResult(Result(1));
	target.SetResult(Result(2));
	CHECK(events == std::vector<int>({-1}));
	CHECK_EQ(1, allocated.size());
	target.SetResult(nullptr);
	CHECK(events == std::vector<int>({-1, -2}));
	CHECK(target.IsResultEmpty());
}

TEST(EffectResults, DetachedResultsRemainOwnedByTheCaller)
{
	World world;
	std::unique_ptr<MActionResult> detached;
	{
		MEffectTarget target(1);
		target.SetResult(Result(1));
		detached.reset(target.GetResult());
		target.SetResultNULL();
		CHECK(target.IsResultEmpty());
	}
	CHECK(events.empty());
	detached->Execute();
	CHECK(events == std::vector<int>({1, -1}));
}

TEST(EffectResults, DestructionReleasesResultsBeforeRemovingThePlayerId)
{
	World world;
	CHECK(MEffectTarget::SetHost(&host) == nullptr);
	{
		MEffectTarget target(1);
		MEffectTarget::s_EffectID = 19;
		target.NewEffectID();
		target.SetResult(Result(1));
	}
	CHECK(events == std::vector<int>({-1, 1019}));
}

TEST(EffectResults, HostReplacementIsReadAtDestructionAndMissingActionsAreSkipped)
{
	World world;
	CHECK(MEffectTarget::SetHost(&host) == nullptr);
	{
		MEffectTarget target(1);
		CHECK(MEffectTarget::SetHost(&otherHost) == &host);
	}
	CHECK(events == std::vector<int>({2000}));
	const MEffectTargetHost empty;
	CHECK(MEffectTarget::SetHost(&empty) == &otherHost);
	{ MEffectTarget target(1); }
	CHECK(MEffectTarget::SetHost(nullptr) == &empty);
	{ MEffectTarget target(1); }
	CHECK(events == std::vector<int>({2000}));
}

TEST(EffectResults, PortalDefaultsAndNameLookupUseTheRealZoneTable)
{
	World world;
	MPortalEffectTarget target(2);
	CHECK_EQ(MEffectTarget::EFFECT_TARGET_PORTAL, target.GetEffectTargetType());
	CHECK_EQ(static_cast<int>(OBJECTID_NULL), target.GetZoneID());
	CHECK_EQ(SECTORPOSITION_NULL, target.GetZoneX());
	CHECK_EQ(SECTORPOSITION_NULL, target.GetZoneY());
	CHECK(target.GetOwnerName() == nullptr);
	CHECK(target.GetZoneName() == nullptr);
	auto zone = std::make_unique<ZONETABLE_INFO>();
	zone->ID = 1003;
	zone->Name = "Destination";
	CHECK(world.zones.Add(zone.get()));
	zone.release();
	target.SetOwnerName("Owner");
	target.SetPortal(1003, 50, 70);
	CHECK_EQ(50, target.GetZoneX());
	CHECK_EQ(70, target.GetZoneY());
	CHECK(std::strcmp(target.GetOwnerName(), "Owner") == 0);
	CHECK(std::strcmp(target.GetZoneName(), "Destination") == 0);
	g_pZoneTable = nullptr;
	CHECK(target.GetZoneName() == nullptr);
}

TEST(EffectResults, PortalConstructionFromABaseTargetKeepsProgressAndResetsDestination)
{
	World world;
	MEffectTarget source(3);
	Initialize(source);
	MPortalEffectTarget portal(source);
	CHECK_EQ(3, portal.GetMaxPhase());
	CHECK_EQ(1, portal.GetCurrentPhase());
	CHECK_EQ(-12, portal.GetX());
	CHECK_EQ(789, portal.GetID());
	CHECK_EQ(OBJECTID_NULL, portal.GetServerID());
	CHECK_EQ(static_cast<int>(OBJECTID_NULL), portal.GetZoneID());
	CHECK_EQ(SECTORPOSITION_NULL, portal.GetZoneX());
	CHECK_EQ(SECTORPOSITION_NULL, portal.GetZoneY());
}

TEST(EffectResults, PortalCopyConstructionKeepsDestinationAndCopiesOwnerText)
{
	World world;
	MPortalEffectTarget source(3);
	Initialize(source);
	source.SetOwnerName("First");
	source.SetPortal(61, 102, 220);
	source.SetResult(Result(1));
	MPortalEffectTarget copy(source);
	source.SetOwnerName("Changed");
	CHECK_EQ(61, copy.GetZoneID());
	CHECK_EQ(102, copy.GetZoneX());
	CHECK_EQ(220, copy.GetZoneY());
	CHECK(std::strcmp(copy.GetOwnerName(), "First") == 0);
	CHECK(copy.IsResultEmpty());
}
