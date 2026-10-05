#include "test_framework.h"
#include "MEffectTarget.h"
#include <memory>
#include <vector>

namespace {
std::vector<int> removals;
int freedResults;
const MEffectTargetHost host{.RemoveFromPlayer = [](BYTE id) { removals.push_back(id); }};
struct World {
	const MEffectTargetHost* previous = MEffectTarget::SetHost(&host);
	BYTE previousID = MEffectTarget::s_EffectID;
	World() { removals.clear(); freedResults = 0; MEffectTarget::s_EffectID = 73; }
	~World() { MEffectTarget::SetHost(previous); MEffectTarget::s_EffectID = previousID; }
};
struct Marker : MActionResultNode {
	~Marker() override { ++freedResults; }
	void Execute() override {}
};
void Mark(MEffectTarget& target) {
	auto result = std::make_unique<MActionResult>(); result->Add(new Marker); target.SetResult(result.release());
}
}

TEST(EffectTargetRegistration, DestroyingCopiesLeavesOriginalRegistrationAndDestroysTheirResults)
{
	World world;
	auto original = std::make_unique<MEffectTarget>(3); original->NewEffectID(); Mark(*original);
	{
		MEffectTarget copy(*original); Mark(copy);
		MEffectTarget nested(copy); Mark(nested);
		CHECK_EQ(73, copy.GetEffectID()); CHECK_EQ(73, nested.GetEffectID());
	}
	CHECK_EQ(2, freedResults); CHECK(removals.empty()); CHECK(original->IsExistResult());
	original.reset(); CHECK_EQ(3, freedResults); CHECK(removals == std::vector<int>{73});
}

TEST(EffectTargetRegistration, NewIdentityPromotesACopyAndAssignmentPreservesRegistrationIdentity)
{
	World world;
	auto original = std::make_unique<MEffectTarget>(3); original->NewEffectID();
	auto copy = std::make_unique<MEffectTarget>(*original); copy->NewEffectID();
	auto visual = std::make_unique<MEffectTarget>(*copy);
	*visual = *original; *copy = *original; *original = *visual;
	CHECK_EQ(74, copy->GetEffectID()); CHECK_EQ(73, original->GetEffectID()); CHECK_EQ(74, visual->GetEffectID());
	visual.reset(); CHECK(removals.empty()); copy.reset(); CHECK(removals == std::vector<int>{74});
	original.reset(); CHECK(removals == std::vector<int>({74,73}));
}

TEST(EffectTargetRegistration, PortalCopiesAlsoLeaveOriginalRegistrationIntact)
{
	World world;
	auto original = std::make_unique<MPortalEffectTarget>(3); original->NewEffectID();
	{ MPortalEffectTarget copy(*original); Mark(copy); }
	CHECK_EQ(1, freedResults); CHECK(removals.empty()); original.reset(); CHECK(removals == std::vector<int>{73});
}

TEST(EffectTargetRegistration, CompletingCopyPhasesDoesNotRemoveTheOriginalPlayerEntry)
{
	World world;
	auto original = std::make_unique<MEffectTarget>(3); original->NewEffectID();
	{
		MEffectTarget copy(*original); copy.RemovePlayerRegistration(); CHECK(removals.empty());
		original->RemovePlayerRegistration(); CHECK(removals == std::vector<int>{73});
	}
	CHECK(removals == std::vector<int>{73});
}
