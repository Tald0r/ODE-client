#include "test_framework.h"
#include "MBloodyWaveEffectGenerator.h"
#include "MEffect.h"

#include <memory>
#include <stdexcept>
#include <vector>

namespace {
int destroyedTargets;
std::vector<std::unique_ptr<MEffect>> retained;
MEffectTarget* dyingTarget;
std::unique_ptr<MEffectTargetOwner> callbackOwner;

struct Target : MEffectTarget
{
	using MEffectTarget::operator=;
	Target() : MEffectTarget(4) { NextPhase(); }
	~Target() override { ++destroyedTargets; }
};

const MBloodyWaveEffectHost throwingHost{
	.Sprite = [](TYPE_EFFECTSPRITETYPE, MBloodyWaveEffectSprite& sprite) {
		sprite = {BLT_EFFECT, 1, false};
		return true;
	},
	.MaxFrames = [](BYTE, TYPE_FRAMEID, int& count) { count = 3; return true; },
	.Queue = [](std::unique_ptr<MEffect>) -> bool { throw std::runtime_error("queue"); },
};

struct World
{
	const MBloodyWaveEffectHost* previous = MBloodyWaveEffectGenerator::SetHost(&throwingHost);
	World() { destroyedTargets = 0; }
	~World() { retained.clear(); MBloodyWaveEffectGenerator::SetHost(previous); }
};

// The table itself remains executable-side. Reproduce its ownership boundary
// using the actual library generator, target and effect, without stubbing them.
void Generate(MEffectTarget* target)
{
	MEffectTargetOwner owner(target);
	EFFECTGENERATOR_INFO info{};
	info.pEffectTarget = target;
	info.effectSpriteType = 17;
	MBloodyWaveEffectGenerator generator;
	generator.Generate(info);
}
}

TEST(EffectTargetOwner, InitialGenerationExceptionDestroysTheUntransferredTarget)
{
	World world;
	auto* target = new Target;
	bool threw = false;
	try { Generate(target); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);
	CHECK_EQ(1, destroyedTargets);
}

TEST(EffectTargetOwner, NextGenerationExceptionDestroysTheDetachedTarget)
{
	World world;
	MEffect previous(BLT_EFFECT);
	previous.SetLink(42, new Target);
	auto* target = previous.GetEffectTarget();
	previous.SetEffectTargetNULL(); // GenerateNext detaches before invoking a generator.
	bool threw = false;
	try { Generate(target); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);
	CHECK(previous.GetEffectTarget() == nullptr);
	CHECK_EQ(1, destroyedTargets);
}

TEST(EffectTargetOwner, ExceptionAfterFirstAcceptanceLeavesTheTargetWithThatEffect)
{
	World world;
	const MBloodyWaveEffectHost partial{
		.Sprite = throwingHost.Sprite,
		.MaxFrames = throwingHost.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			if (!retained.empty()) throw std::runtime_error("second queue");
			retained.push_back(std::move(effect));
			return true;
		},
	};
	MBloodyWaveEffectGenerator::SetHost(&partial);
	auto* target = new Target;
	bool threw = false;
	try { Generate(target); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);
	CHECK_EQ(1, retained.size());
	CHECK_EQ(0, destroyedTargets);
	if (!retained.empty()) CHECK(retained.front()->GetEffectTarget() == target);
	retained.clear();
	CHECK_EQ(1, destroyedTargets);
}

TEST(EffectTargetOwner, SuccessfulGenerationTransfersToOnlyTheFirstEffect)
{
	World world;
	const MBloodyWaveEffectHost accepting{
		.Sprite = throwingHost.Sprite,
		.MaxFrames = throwingHost.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			retained.push_back(std::move(effect));
			return true;
		},
	};
	MBloodyWaveEffectGenerator::SetHost(&accepting);
	auto* target = new Target;
	Generate(target);
	CHECK_EQ(4, retained.size());
	CHECK_EQ(0, destroyedTargets);
	for (size_t i = 0; i < retained.size(); ++i)
		CHECK(retained[i]->GetEffectTarget() == (i == 0 ? target : nullptr));
	retained.clear();
	CHECK_EQ(1, destroyedTargets);
}

TEST(EffectTargetOwner, NormalReturnWithoutTransferDestroysTheTarget)
{
	World world;
	MBloodyWaveEffectGenerator::SetHost(nullptr);
	Generate(new Target);
	CHECK_EQ(1, destroyedTargets);
}

TEST(EffectTargetOwner, NullAndExplicitlyDeletedTargetsAreSafe)
{
	World world;
	{
		MEffectTargetOwner empty(nullptr);
		auto* target = new Target;
		MEffectTargetOwner owner(target);
		delete target;
		CHECK_EQ(1, destroyedTargets);
	}
	CHECK_EQ(1, destroyedTargets);
}

TEST(EffectTargetOwner, RelinkingTheSamePointerDisarmsPendingOwnership)
{
	World world;
	{
		MEffect effect(BLT_EFFECT);
		auto* target = new Target;
		effect.SetLink(42, target);
		{
			MEffectTargetOwner owner(target);
			effect.SetLink(43, target);
		}
		CHECK_EQ(0, destroyedTargets);
		CHECK(effect.GetEffectTarget() == target);
		CHECK_EQ(43, effect.GetActionInfo());
	}
	CHECK_EQ(1, destroyedTargets);
}

TEST(EffectTargetOwner, ReplacingALinkDisarmsBothTheDeletedAndAdoptedTargets)
{
	World world;
	{
		MEffect effect(BLT_EFFECT);
		auto* oldTarget = new Target;
		effect.SetLink(42, oldTarget);
		{
			MEffectTargetOwner oldOwner(oldTarget);
			auto* next = new Target;
			MEffectTargetOwner nextOwner(next);
			effect.SetLink(43, next);
			CHECK(effect.GetEffectTarget() == next);
			CHECK_EQ(1, destroyedTargets);
		}
		CHECK_EQ(1, destroyedTargets);
	}
	CHECK_EQ(2, destroyedTargets);
}

TEST(EffectTargetOwner, CopiesDoNotInheritTheSourcesPendingOwner)
{
	World world;
	{
		MEffect effect(BLT_EFFECT);
		{
			auto* original = new Target;
			MEffectTargetOwner owner(original);
			auto* copy = new Target(*original);
			effect.SetLink(42, copy);
		}
		CHECK_EQ(1, destroyedTargets);
		CHECK(effect.GetEffectTarget() != nullptr);
	}
	CHECK_EQ(2, destroyedTargets);
}

TEST(EffectTargetOwner, AssignmentPreservesBothObjectsPendingOwnership)
{
	World world;
	{
		auto* source = new Target;
		auto* destination = new Target;
		MEffectTargetOwner sourceOwner(source);
		MEffectTargetOwner destinationOwner(destination);
		static_cast<MEffectTarget&>(*destination) = *source;
		CHECK_EQ(0, destroyedTargets);
	}
	CHECK_EQ(2, destroyedTargets);
}

TEST(EffectTargetOwner, NestedOwnershipTransfersResponsibilityToTheInnerScope)
{
	World world;
	{
		auto* target = new Target;
		MEffectTargetOwner outer(target);
		{
			MEffectTargetOwner inner(target);
			CHECK_EQ(0, destroyedTargets);
		}
		CHECK_EQ(1, destroyedTargets);
	}
	CHECK_EQ(1, destroyedTargets);
}

TEST(EffectTargetOwner, DetachmentCanBeGuardedAgainAfterAnEarlierTransfer)
{
	World world;
	MEffect previous(BLT_EFFECT);
	{
		auto* target = new Target;
		MEffectTargetOwner first(target);
		previous.SetLink(42, target);
	}
	CHECK_EQ(0, destroyedTargets);
	{
		auto* target = previous.GetEffectTarget();
		previous.SetEffectTargetNULL();
		MEffectTargetOwner next(target);
	}
	CHECK_EQ(1, destroyedTargets);
	CHECK(previous.GetEffectTarget() == nullptr);
}

TEST(EffectTargetOwner, DestructionCallbacksCannotRearmADyingTarget)
{
	World world;
	const MEffectTargetHost host{
		.RemoveFromPlayer = [](BYTE) {
			MEffectTargetOwner local(dyingTarget);
			callbackOwner = std::make_unique<MEffectTargetOwner>(dyingTarget);
		},
	};
	struct RestoreHost
	{
		const MEffectTargetHost* previous;
		~RestoreHost() { callbackOwner.reset(); MEffectTarget::SetHost(previous); }
	} restore{MEffectTarget::SetHost(&host)};
	{
		dyingTarget = new Target;
		MEffectTargetOwner owner(dyingTarget);
	}
	CHECK_EQ(1, destroyedTargets);
	callbackOwner.reset();
	CHECK_EQ(1, destroyedTargets);
	dyingTarget = nullptr;
}
