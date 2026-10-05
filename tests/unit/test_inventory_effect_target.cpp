#include "test_framework.h"
#include "InventoryEffectTarget.h"
#include "MEffect.h"

#include <memory>
#include <stdexcept>
#include <utility>

namespace {
struct Counts
{
	int executed = 0;
	int destroyed = 0;
};

struct Action : MActionResultNode
{
	Counts& counts;
	explicit Action(Counts& values) : counts(values) {}
	~Action() override { ++counts.destroyed; }
	void Execute() override { ++counts.executed; }
};

std::unique_ptr<MEffectTarget> Prepare(Counts& counts)
{
	return PrepareInventoryEffectTarget(3, 4, 5, 1234, 60, std::make_unique<Action>(counts));
}
}

// Regression guards for the production preparation helper used by
// AddNewInventoryEffect. The executable's full inventory/UI path is not linked.
TEST(InventoryEffectTarget, PreparationOwnsTheResultBeforeSubmission)
{
	Counts counts;
	auto target = Prepare(counts);
	CHECK_EQ(3, target->GetMaxPhase());
	CHECK_EQ(0, target->GetCurrentPhase());
	CHECK_EQ(4, target->GetX());
	CHECK_EQ(5, target->GetY());
	CHECK_EQ(0, target->GetZ());
	CHECK_EQ(1234, target->GetID());
	CHECK_EQ(60, target->GetDelayFrame());
	CHECK(target->IsExistResult());
	CHECK_EQ(1, target->GetResult()->GetSize());
	CHECK_EQ(0, counts.executed);
	CHECK_EQ(0, counts.destroyed);
	target.reset();
	CHECK_EQ(0, counts.executed);
	CHECK_EQ(1, counts.destroyed);
}

TEST(InventoryEffectTarget, MissingActionDoesNotCreateAnEmptyResult)
{
	auto target = PrepareInventoryEffectTarget(1, 0, 0, 7, 0, nullptr);
	CHECK(target->GetResult() == nullptr);
	CHECK(target->IsResultEmpty());
	CHECK_EQ(7, target->GetID());
}

TEST(InventoryEffectTarget, SynchronousCompletionSeesAndExecutesThePreparedResult)
{
	Counts counts;
	auto target = Prepare(counts);
	auto complete = [](std::unique_ptr<MEffectTarget> submitted) {
		CHECK(submitted->IsExistResult());
		submitted->SetResultTime();
		submitted->GetResult()->Execute();
	};
	complete(std::move(target));
	CHECK(target == nullptr);
	CHECK_EQ(1, counts.executed);
	CHECK_EQ(1, counts.destroyed);
}

TEST(InventoryEffectTarget, DeferredEffectOwnsThePreparedResultUntilCompletion)
{
	Counts counts;
	{
		MEffect effect(BLT_EFFECT);
		auto target = Prepare(counts);
		effect.SetLink(42, target.release());
		CHECK_EQ(0, counts.executed);
		CHECK_EQ(0, counts.destroyed);
		effect.GetEffectTarget()->GetResult()->Execute();
		CHECK_EQ(1, counts.executed);
		CHECK_EQ(1, counts.destroyed);
	}
	CHECK_EQ(1, counts.executed);
	CHECK_EQ(1, counts.destroyed);
}

TEST(InventoryEffectTarget, DiscardAndExceptionDestroyThePreparedActionOnce)
{
	for (bool throws : {false, true})
	{
		Counts counts;
		auto target = Prepare(counts);
		auto consume = [throws](std::unique_ptr<MEffectTarget> submitted) {
			CHECK(submitted->IsExistResult());
			if (throws) throw std::runtime_error("generation failed");
		};
		bool threw = false;
		try { consume(std::move(target)); }
		catch (const std::runtime_error&) { threw = true; }
		CHECK_EQ(throws, threw);
		CHECK(target == nullptr);
		CHECK_EQ(0, counts.executed);
		CHECK_EQ(1, counts.destroyed);
	}
}
