#include "test_framework.h"
#include "MRequestMode.h"

#include <cstring>
#include <initializer_list>
#include <new>

namespace {

void CheckIdle(const MRequestMode& mode)
{
	CHECK_EQ(MRequestMode::REQUEST_NULL, mode.GetRequestMode());
	CHECK(!mode.IsRequestMode());
	CHECK(!mode.IsRequestTrade());
	CHECK(!mode.IsRequestParty());
	CHECK(!mode.IsRequestInfo());
}

} // namespace

TEST(RequestMode, FreshStorageStartsWithoutARequest)
{
	alignas(MRequestMode) unsigned char storage[sizeof(MRequestMode)];
	std::memset(storage, 0xcc, sizeof(storage));
	auto* mode = new (storage) MRequestMode;
	CheckIdle(*mode);
	mode->~MRequestMode();
}

TEST(RequestMode, EachNewRequestReplacesThePreviousKind)
{
	MRequestMode mode;
	mode.SetRequestMode(MRequestMode::REQUEST_TRADE);
	CHECK_EQ(MRequestMode::REQUEST_TRADE, mode.GetRequestMode());
	CHECK(mode.IsRequestMode());
	CHECK(mode.IsRequestTrade());
	CHECK(!mode.IsRequestParty());
	CHECK(!mode.IsRequestInfo());
	mode.SetRequestMode(MRequestMode::REQUEST_PARTY);
	CHECK_EQ(MRequestMode::REQUEST_PARTY, mode.GetRequestMode());
	CHECK(mode.IsRequestMode());
	CHECK(!mode.IsRequestTrade());
	CHECK(mode.IsRequestParty());
	CHECK(!mode.IsRequestInfo());
	mode.SetRequestMode(MRequestMode::REQUEST_INFO);
	CHECK_EQ(MRequestMode::REQUEST_INFO, mode.GetRequestMode());
	CHECK(mode.IsRequestMode());
	CHECK(!mode.IsRequestTrade());
	CHECK(!mode.IsRequestParty());
	CHECK(mode.IsRequestInfo());
}

TEST(RequestMode, EveryKindCanBeCancelledRepeatedlyOrResetExplicitly)
{
	MRequestMode mode;
	for (auto request : {MRequestMode::REQUEST_TRADE, MRequestMode::REQUEST_PARTY, MRequestMode::REQUEST_INFO})
	{
		mode.SetRequestMode(request);
		mode.UnSetRequestMode();
		CheckIdle(mode);
		mode.UnSetRequestMode();
		CheckIdle(mode);
		mode.SetRequestMode(request);
		mode.SetRequestMode(MRequestMode::REQUEST_NULL);
		CheckIdle(mode);
	}
}

TEST(RequestMode, TransferringARequestKeepsIndependentState)
{
	MRequestMode view;
	MRequestMode player;
	view.SetRequestMode(MRequestMode::REQUEST_TRADE);
	player.SetRequestMode(view.GetRequestMode());
	view.UnSetRequestMode();
	CheckIdle(view);
	CHECK(player.IsRequestTrade());
	view.SetRequestMode(MRequestMode::REQUEST_INFO);
	CHECK(player.IsRequestTrade());
	player.SetRequestMode(view.GetRequestMode());
	CHECK(player.IsRequestInfo());
	player.UnSetRequestMode();
	CheckIdle(player);
	CHECK(view.IsRequestInfo());
}
