#include "test_framework.h"
#include "MScreenEffectManager.h"
#include "MScreenEffect.h"

#include <functional>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>

namespace {

struct Event
{
	char kind;
	int tag;
	bool operator==(const Event&) const = default;
};
std::vector<Event> events;
DWORD frameNow;
int managerClockReads;
std::function<void(MEffect*)> onGenerate;
const MEffectHost effectHost{
	.CurrentFrame = []() { return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE) { return 7; },
};
const MScreenEffectManagerHost managerHost{
	.CurrentFrame = []() { ++managerClockReads; return frameNow; },
	.GenerateNext = [](MEffect* effect) {
		events.push_back({'G', effect->GetPower()});
		if (onGenerate) onGenerate(effect);
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(nullptr);
	const MScreenEffectManagerHost* previousManager = MScreenEffectManager::SetHost(&managerHost);
	World() { frameNow = 100; managerClockReads = 0; events.clear(); onGenerate = {}; }
	~World()
	{
		onGenerate = {};
		MScreenEffectManager::SetHost(previousManager);
		MEffectTarget::SetHost(previousTarget);
		MEffect::SetHost(previousEffect);
	}
};

struct Effect : MEffect
{
	bool active;
	std::function<void()> onUpdate;
	Effect(int tag, bool alive = true) : MEffect(BLT_EFFECT), active(alive) { SetPower(tag); }
	~Effect() override { events.push_back({'D', GetPower()}); }
	bool Update() override
	{
		events.push_back({'U', GetPower()});
		if (onUpdate) onUpdate();
		return active;
	}
};

struct Target : MEffectTarget
{
	using MEffectTarget::operator=;
	int tag;
	Target(int id, BYTE phases = 3) : MEffectTarget(phases), tag(id) { NextPhase(); }
	~Target() override { events.push_back({'T', tag}); }
};

void Link(MEffect& effect, DWORD duration = 20, DWORD link = 5)
{
	effect.SetCount(duration, link);
	effect.SetLink(42, new Target(effect.GetPower()));
}

} // namespace

TEST(ScreenEffectManager, EmptyAndNullInsertionsDoNotCallServices)
{
	World world;
	MScreenEffectManager manager;
	manager.AddEffect(nullptr);
	manager.Update();
	CHECK_EQ(0, manager.GetSize());
	CHECK_EQ(0, managerClockReads);
	CHECK(events.empty());
}

TEST(ScreenEffectManager, InsertionsOwnEffectsInNewestFirstOrder)
{
	World world;
	MScreenEffectManager manager;
	auto* first = new Effect(1);
	auto* second = new Effect(2);
	manager.AddEffect(first);
	manager.AddEffect(second);
	CHECK_EQ(2, manager.GetSize());
	auto it = manager.GetEffects();
	CHECK(*it++ == second);
	CHECK(*it == first);
	CHECK(events.empty());
}

TEST(ScreenEffectManager, ReleaseDestroysEveryOwnedEffectAndCanBeRepeated)
{
	World world;
	MScreenEffectManager manager;
	manager.AddEffect(new Effect(1));
	manager.AddEffect(new Effect(2));
	manager.Release();
	CHECK_EQ(0, manager.GetSize());
	CHECK(events == std::vector<Event>({{'D', 2}, {'D', 1}}));
	events.clear();
	manager.Release();
	manager.Update();
	CHECK(events.empty());
}

TEST(ScreenEffectManager, VirtualBaseDestructionReleasesEffectsAndTheirTargets)
{
	World world;
	{
		std::unique_ptr<MEffectManager> manager = std::make_unique<MScreenEffectManager>();
		auto* effect = new Effect(1);
		Link(*effect);
		manager->AddEffect(effect);
	}
	CHECK(events == std::vector<Event>({{'D', 1}, {'T', 1}}));
}

TEST(ScreenEffectManager, ActiveEffectsUpdateNewestFirst)
{
	World world;
	MScreenEffectManager manager;
	for (int tag : {1, 2, 3}) manager.AddEffect(new Effect(tag));
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 3}, {'U', 2}, {'U', 1}}));
	CHECK_EQ(3, manager.GetSize());
	CHECK_EQ(3, managerClockReads);
}

TEST(ScreenEffectManager, ConsecutiveExpiredEffectsAreDeletedWithoutSkippingTheirNeighbor)
{
	World world;
	MScreenEffectManager manager;
	manager.AddEffect(new Effect(1, false));
	manager.AddEffect(new Effect(2, false));
	auto* survivor = new Effect(3);
	manager.AddEffect(survivor);
	manager.Update();
	CHECK_EQ(1, manager.GetSize());
	CHECK(*manager.GetEffects() == survivor);
	CHECK(events == std::vector<Event>({{'U', 3}, {'U', 2}, {'D', 2}, {'U', 1}, {'D', 1}}));
	CHECK_EQ(1, managerClockReads);
}

TEST(ScreenEffectManager, ActiveLinksUseAnInclusiveDeadlineAfterTheEffectUpdate)
{
	World world;
	MScreenEffectManager manager;
	auto* effect = new Effect(1);
	Link(*effect);
	manager.AddEffect(effect);
	frameNow = 103;
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 1}}));
	events.clear();
	frameNow = 104;
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 1}, {'G', 1}}));
	CHECK_EQ(1, manager.GetSize());
	CHECK_EQ(1, effect->GetLinkSize());
}

TEST(ScreenEffectManager, RetainedActiveLinksAreRequestedAgainUntilTheGeneratorTransfersThem)
{
	World world;
	MScreenEffectManager manager;
	auto* effect = new Effect(1);
	Link(*effect, 20, 1);
	manager.AddEffect(effect);
	manager.Update();
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 1}, {'G', 1}, {'U', 1}, {'G', 1}}));
	CHECK(effect->GetEffectTarget() != nullptr);
}

TEST(ScreenEffectManager, ActiveEffectsWithoutAPendingPhaseDoNotGenerate)
{
	World world;
	MScreenEffectManager manager;
	auto* first = new Effect(1);
	first->SetCount(20, 1);
	first->SetLink(42, new MEffectTarget(3));
	auto* second = new Effect(2);
	second->SetCount(20, 1);
	second->SetLink(42, new Target(2, 1));
	manager.AddEffect(first);
	manager.AddEffect(second);
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 2}, {'U', 1}}));
	CHECK_EQ(2, manager.GetSize());
}

TEST(ScreenEffectManager, ExpiryGeneratesBeforeDeletionEvenBeforeTheLinkDeadline)
{
	World world;
	MScreenEffectManager manager;
	auto* effect = new Effect(1, false);
	Link(*effect, 20, 100);
	manager.AddEffect(effect);
	onGenerate = [&](MEffect* borrowed) {
		CHECK(borrowed == effect);
		CHECK_EQ(1, manager.GetSize());
		CHECK_EQ(42, borrowed->GetActionInfo());
		CHECK(borrowed->GetEffectTarget() != nullptr);
	};
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 1}, {'G', 1}, {'D', 1}, {'T', 1}}));
	CHECK_EQ(0, manager.GetSize());
	CHECK_EQ(0, managerClockReads);
}

TEST(ScreenEffectManager, EndedTargetsAreDestroyedWithoutGeneration)
{
	World world;
	MScreenEffectManager manager;
	auto* effect = new Effect(1, false);
	effect->SetLink(42, new Target(1, 1));
	manager.AddEffect(effect);
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 1}, {'D', 1}, {'T', 1}}));
	CHECK_EQ(0, manager.GetSize());
}

TEST(ScreenEffectManager, MissingClockSkipsActiveLinksButStillAllowsExpiryGeneration)
{
	World world;
	const MScreenEffectManagerHost generateOnly{.GenerateNext = managerHost.GenerateNext};
	MScreenEffectManager::SetHost(&generateOnly);
	MScreenEffectManager manager;
	auto* effect = new Effect(1);
	Link(*effect, 20, 1);
	manager.AddEffect(effect);
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 1}}));
	events.clear();
	effect->active = false;
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 1}, {'G', 1}, {'D', 1}, {'T', 1}}));
	CHECK_EQ(0, managerClockReads);
}

TEST(ScreenEffectManager, MissingGeneratorLeavesTargetOwnershipWithTheEffect)
{
	World world;
	const MScreenEffectManagerHost clockOnly{.CurrentFrame = managerHost.CurrentFrame};
	MScreenEffectManager::SetHost(&clockOnly);
	MScreenEffectManager manager;
	auto* effect = new Effect(1);
	Link(*effect, 20, 1);
	manager.AddEffect(effect);
	manager.Update();
	CHECK(effect->GetEffectTarget() != nullptr);
	CHECK_EQ(1, manager.GetSize());
	effect->active = false;
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 1}, {'U', 1}, {'D', 1}, {'T', 1}}));
}

TEST(ScreenEffectManager, EmptyOrAbsentHostsStillUpdateAndDestroyExpiredEffects)
{
	World world;
	const MScreenEffectManagerHost empty{};
	for (const auto* host : {static_cast<const MScreenEffectManagerHost*>(nullptr), &empty})
	{
		MScreenEffectManager::SetHost(host);
		MScreenEffectManager manager;
		auto* effect = new Effect(1, false);
		Link(*effect);
		manager.AddEffect(effect);
		events.clear();
		manager.Update();
		CHECK_EQ(0, manager.GetSize());
		CHECK(events == std::vector<Event>({{'U', 1}, {'D', 1}, {'T', 1}}));
	}
}

TEST(ScreenEffectManager, ClockCallbackCanReplaceTheGenerationHost)
{
	World world;
	const MScreenEffectManagerHost changing{
		.CurrentFrame = []() {
			MScreenEffectManager::SetHost(&managerHost);
			return frameNow;
		},
	};
	CHECK(MScreenEffectManager::SetHost(&changing) == &managerHost);
	MScreenEffectManager manager;
	auto* effect = new Effect(1);
	Link(*effect, 20, 1);
	manager.AddEffect(effect);
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 1}, {'G', 1}}));
	CHECK(MScreenEffectManager::SetHost(&changing) == &managerHost);
}

TEST(ScreenEffectManager, GenerationHostIsReadAgainForTheNextEffect)
{
	World world;
	MScreenEffectManager manager;
	for (int tag : {1, 2})
	{
		auto* effect = new Effect(tag, false);
		Link(*effect);
		manager.AddEffect(effect);
	}
	onGenerate = [](MEffect*) { MScreenEffectManager::SetHost(nullptr); };
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 2}, {'G', 2}, {'D', 2}, {'T', 2},
		{'U', 1}, {'D', 1}, {'T', 1}}));
}

TEST(ScreenEffectManager, GeneratedFrontInsertionsWaitUntilTheNextUpdate)
{
	World world;
	MScreenEffectManager manager;
	for (int tag : {1, 2})
	{
		auto* effect = new Effect(tag, false);
		Link(*effect);
		manager.AddEffect(effect);
	}
	int nextTag = 3;
	onGenerate = [&](MEffect*) { manager.AddEffect(new Effect(nextTag++)); };
	manager.Update();
	CHECK_EQ(2, manager.GetSize());
	CHECK(events == std::vector<Event>({{'U', 2}, {'G', 2}, {'D', 2}, {'T', 2},
		{'U', 1}, {'G', 1}, {'D', 1}, {'T', 1}}));
	events.clear();
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 4}, {'U', 3}}));
}

TEST(ScreenEffectManager, EffectsAddedDuringTheirPredecessorUpdateAreDeferred)
{
	World world;
	MScreenEffectManager manager;
	manager.AddEffect(new Effect(1));
	auto* effect = new Effect(2);
	effect->onUpdate = [&] { manager.AddEffect(new Effect(3)); };
	manager.AddEffect(effect);
	manager.Update();
	CHECK_EQ(3, manager.GetSize());
	CHECK(events == std::vector<Event>({{'U', 2}, {'U', 1}}));
	effect->onUpdate = {};
	events.clear();
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 3}, {'U', 2}, {'U', 1}}));
}

TEST(ScreenEffectManager, GenerationCanTransferTheOwnedTargetBeforeDeletingItsParent)
{
	World world;
	MScreenEffectManager manager;
	auto* parent = new Effect(1, false);
	Link(*parent);
	auto* target = parent->GetEffectTarget();
	manager.AddEffect(parent);
	Effect* child = nullptr;
	onGenerate = [&](MEffect* effect) {
		child = new Effect(2);
		child->SetLink(effect->GetActionInfo(), effect->GetEffectTarget());
		effect->SetEffectTargetNULL();
		manager.AddEffect(child);
	};
	manager.Update();
	CHECK_EQ(1, manager.GetSize());
	CHECK(child != nullptr);
	CHECK(child->GetEffectTarget() == target);
	CHECK(events == std::vector<Event>({{'U', 1}, {'G', 1}, {'D', 1}}));
	events.clear();
	manager.Release();
	CHECK(events == std::vector<Event>({{'D', 2}, {'T', 1}}));
}

TEST(ScreenEffectManager, FrameIsReadAfterEachEffectUpdate)
{
	World world;
	MScreenEffectManager manager;
	auto* effect = new Effect(1);
	Link(*effect);
	effect->onUpdate = [] { frameNow = 104; };
	manager.AddEffect(effect);
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 1}, {'G', 1}}));
	CHECK_EQ(1, managerClockReads);
}

TEST(ScreenEffectManager, WrappedLinkDeadlineRetainsAbsoluteUnsignedComparison)
{
	World world;
	MScreenEffectManager manager;
	frameNow = (std::numeric_limits<DWORD>::max)() - 1;
	auto* effect = new Effect(1);
	Link(*effect, 20, 4);
	CHECK_EQ(1, effect->GetEndLinkFrame());
	manager.AddEffect(effect);
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 1}, {'G', 1}}));
	events.clear();
	frameNow = 0;
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 1}}));
}

TEST(ScreenEffectManager, RealScreenEffectsAnimateAndExpireThroughTheManager)
{
	World world;
	MScreenEffectManager manager;
	auto* effect = new MScreenEffect(BLT_EFFECT);
	effect->SetPower(1);
	effect->SetFrameID(12, 3);
	Link(*effect, 3, MAX_LINKCOUNT);
	manager.AddEffect(effect);
	manager.Update();
	CHECK_EQ(1, effect->GetFrame());
	CHECK_EQ(7, effect->GetLight());
	CHECK(events.empty());
	frameNow = 101;
	manager.Update();
	CHECK_EQ(2, effect->GetFrame());
	frameNow = 102;
	manager.Update();
	CHECK_EQ(0, manager.GetSize());
	CHECK(events == std::vector<Event>({{'G', 1}, {'T', 1}}));
}

TEST(ScreenEffectManager, AddingAnOwnedPointerAgainDoesNotDuplicateOwnershipOrUpdates)
{
	World world;
	struct Manager : MScreenEffectManager
	{
		void RemoveExtraAliasesForCleanup()
		{
			while (m_listEffect.size() > 1) m_listEffect.pop_front();
		}
	} manager;
	auto* effect = new Effect(1);
	manager.AddEffect(effect);
	manager.AddEffect(effect);
	CHECK_EQ(1, manager.GetSize());
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 1}}));
	// Keep the failing version safe to destroy after observing duplicate entries.
	manager.RemoveExtraAliasesForCleanup();
	events.clear();
	manager.Release();
	CHECK(events == std::vector<Event>({{'D', 1}}));
}

TEST(ScreenEffectManager, OwningManagersCannotBeShallowCopiedOrMovedThroughCopying)
{
	CHECK(!std::is_copy_constructible_v<MScreenEffectManager>);
	CHECK(!std::is_copy_assignable_v<MScreenEffectManager>);
	CHECK(!std::is_move_constructible_v<MScreenEffectManager>);
	CHECK(!std::is_move_assignable_v<MScreenEffectManager>);
	CHECK(!std::is_copy_assignable_v<MEffectManager>);
}

TEST(ScreenEffectManager, ReaddingAMiddleEntryPreservesOrderAndItsOwnedTarget)
{
	World world;
	MScreenEffectManager manager;
	manager.AddEffect(new Effect(1));
	auto* middle = new Effect(2);
	Link(*middle);
	auto* target = middle->GetEffectTarget();
	manager.AddEffect(middle);
	manager.AddEffect(new Effect(3));
	manager.AddEffect(middle);
	CHECK_EQ(3, manager.GetSize());
	CHECK(middle->GetEffectTarget() == target);
	manager.Update();
	CHECK(events == std::vector<Event>({{'U', 3}, {'U', 2}, {'U', 1}}));
	events.clear();
	manager.Release();
	CHECK(events == std::vector<Event>({{'D', 3}, {'D', 2}, {'T', 2}, {'D', 1}}));
}
