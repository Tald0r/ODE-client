#include "test_framework.h"

#include <cstddef>
#include "MFunctionManager.h"
#include "ModifyStatusManager.h"
#include "VS_UI/src/header/AcceleratorManager.h"

#include <cstring>
#include <limits>
#include <new>
#include <type_traits>
#include <vector>

// Both live callers inherit this exact implementation. These type checks
// instantiate neither game manager; the tests construct the library class.
static_assert(std::is_same_v<decltype(&ModifyStatusManager::Execute),
	bool (MFunctionManager::*)(int, void*) const>);
static_assert(std::is_same_v<decltype(&AcceleratorManager::Execute),
	bool (MFunctionManager::*)(int, void*) const>);

namespace {

class Functions : public MFunctionManager
{
public:
	using MFunctionManager::SetFunction;
};

struct Payload
{
	Functions* manager = nullptr;
	std::vector<int> calls;
	bool nestedResult = false;
};

void First(void* value)
{
	static_cast<Payload*>(value)->calls.push_back(1);
}

void Second(void* value)
{
	static_cast<Payload*>(value)->calls.push_back(2);
}

int nullCalls = 0;
void CheckNull(void* value)
{
	CHECK(value == nullptr);
	++nullCalls;
}

void ReleaseDuringCall(void* value)
{
	auto& payload = *static_cast<Payload*>(value);
	payload.calls.push_back(1);
	payload.manager->Release();
	payload.calls.push_back(2);
}

void ReinitializeDuringCall(void* value)
{
	auto& payload = *static_cast<Payload*>(value);
	payload.calls.push_back(1);
	payload.manager->Init(3);
	payload.manager->SetFunction(2, Second);
}

void DispatchDuringCall(void* value)
{
	auto& payload = *static_cast<Payload*>(value);
	payload.calls.push_back(10);
	payload.nestedResult = payload.manager->Execute(1, value);
	payload.calls.push_back(20);
}

} // namespace

TEST(FunctionManager, FreshStorageHasNoCallableSlots)
{
	alignas(Functions) unsigned char storage[sizeof(Functions)];
	std::memset(storage, 0xcc, sizeof(storage));
	auto* manager = new (storage) Functions;
	Payload payload;
	manager->SetFunction(0, First);
	CHECK(!manager->Execute(0, &payload));
	CHECK(!manager->Execute(-1, &payload));
	CHECK(payload.calls.empty());
	manager->Release();
	manager->Release();
	manager->~Functions();
}

TEST(FunctionManager, InitializedSlotsStayEmptyUntilRegistered)
{
	Functions manager;
	manager.Init(4);
	Payload payload;
	for (int id = 0; id < 4; ++id)
		CHECK(!manager.Execute(id, &payload));
	manager.SetFunction(1, First);
	manager.SetFunction(3, Second);
	const MFunctionManager& dispatcher = manager;
	CHECK(dispatcher.Execute(3, &payload));
	CHECK(dispatcher.Execute(1, &payload));
	CHECK(dispatcher.Execute(3, &payload));
	CHECK(!dispatcher.Execute(0, &payload));
	CHECK(!dispatcher.Execute(2, &payload));
	CHECK((payload.calls == std::vector<int>{2, 1, 2}));
}

TEST(FunctionManager, DispatchForwardsExplicitAndDefaultNullPayloads)
{
	Functions manager;
	manager.Init(1);
	manager.SetFunction(0, CheckNull);
	nullCalls = 0;
	CHECK(manager.Execute(0));
	CHECK_EQ(1, nullCalls);
	CHECK(manager.Execute(0, nullptr));
	CHECK_EQ(2, nullCalls);
}

TEST(FunctionManager, RegistrationReplacesAndRemovesOnlyItsOwnSlot)
{
	Functions manager;
	manager.Init(2);
	Payload payload;
	manager.SetFunction(0, First);
	manager.SetFunction(1, First);
	CHECK(manager.Execute(0, &payload));
	manager.SetFunction(0, Second);
	CHECK(manager.Execute(0, &payload));
	manager.SetFunction(0, nullptr);
	CHECK(!manager.Execute(0, &payload));
	CHECK(manager.Execute(1, &payload));
	CHECK((payload.calls == std::vector<int>{1, 2, 1}));
}

TEST(FunctionManager, InvalidIDsNeitherCallNorOverwriteHandlers)
{
	Functions manager;
	manager.Init(2);
	manager.SetFunction(0, First);
	manager.SetFunction(1, Second);
	Payload payload;
	for (int id : {(std::numeric_limits<int>::min)(), -1, 2, (std::numeric_limits<int>::max)()})
	{
		manager.SetFunction(id, CheckNull);
		CHECK(!manager.Execute(id, &payload));
	}
	CHECK(payload.calls.empty());
	CHECK(manager.Execute(0, &payload));
	CHECK(manager.Execute(1, &payload));
	CHECK((payload.calls == std::vector<int>{1, 2}));
}

TEST(FunctionManager, ReinitializationDropsHandlersWhenGrowingOrShrinking)
{
	Functions manager;
	Payload payload;
	manager.Init(2);
	manager.SetFunction(1, First);
	manager.Init(5);
	for (int id = 0; id < 5; ++id)
		CHECK(!manager.Execute(id, &payload));
	manager.SetFunction(1, First);
	manager.SetFunction(4, Second);
	manager.Init(2);
	CHECK(!manager.Execute(1, &payload));
	CHECK(!manager.Execute(4, &payload));
	manager.SetFunction(1, Second);
	CHECK(manager.Execute(1, &payload));
	CHECK((payload.calls == std::vector<int>{2}));
}

TEST(FunctionManager, NonpositiveSizesReleaseExistingHandlers)
{
	Functions manager;
	Payload payload;
	for (int size : {0, -1, (std::numeric_limits<int>::min)()})
	{
		manager.Init(1);
		manager.SetFunction(0, First);
		manager.Init(size);
		manager.SetFunction(0, Second);
		CHECK(!manager.Execute(0, &payload));
	}
	CHECK(payload.calls.empty());
}

TEST(FunctionManager, ReleasedManagerCanBeReusedWithoutAffectingAnotherInstance)
{
	Functions first;
	Functions second;
	first.Init(1);
	second.Init(1);
	first.SetFunction(0, First);
	second.SetFunction(0, Second);
	Payload payload;
	first.Release();
	first.Release();
	CHECK(!first.Execute(0, &payload));
	CHECK(second.Execute(0, &payload));
	first.Init(1);
	CHECK(!first.Execute(0, &payload));
	first.SetFunction(0, First);
	CHECK(first.Execute(0, &payload));
	CHECK(second.Execute(0, &payload));
	CHECK((payload.calls == std::vector<int>{2, 1, 2}));
}

TEST(FunctionManager, CallbackCanReleaseItsTableBeforeReturning)
{
	Functions manager;
	manager.Init(1);
	manager.SetFunction(0, ReleaseDuringCall);
	Payload payload;
	payload.manager = &manager;
	CHECK(manager.Execute(0, &payload));
	CHECK((payload.calls == std::vector<int>{1, 2}));
	CHECK(!manager.Execute(0, &payload));
}

TEST(FunctionManager, CallbackCanReplaceTheTableForSubsequentDispatch)
{
	Functions manager;
	manager.Init(1);
	manager.SetFunction(0, ReinitializeDuringCall);
	Payload payload;
	payload.manager = &manager;
	CHECK(manager.Execute(0, &payload));
	CHECK(!manager.Execute(0, &payload));
	CHECK(manager.Execute(2, &payload));
	CHECK((payload.calls == std::vector<int>{1, 2}));
}

TEST(FunctionManager, NestedDispatchRunsSynchronouslyWithTheSamePayload)
{
	Functions manager;
	manager.Init(2);
	manager.SetFunction(0, DispatchDuringCall);
	manager.SetFunction(1, Second);
	Payload payload;
	payload.manager = &manager;
	CHECK(manager.Execute(0, &payload));
	CHECK(payload.nestedResult);
	CHECK((payload.calls == std::vector<int>{10, 2, 20}));
}
