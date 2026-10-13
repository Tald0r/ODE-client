#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "TradePresentationHost.h"
#include "MInventory.h"
#include "MMoneyManager.h"
#include "MTradeManager.h"
#include "TempInformation.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCGoodsList.h"
#include "Gpackets/GCMakeItemFail.h"
#include "Gpackets/GCTradeFinish.h"
#include <cstdint>
#include <string>
#include <vector>

// Trade, crafting and item-mall replies present what they receive through
// TradePresentation::Host. The trade reply also drives the trade manager,
// closes the exchange windows on a rejection, and the crafting failure
// unlocks the item only for a pending crafting request.
namespace {

std::vector<std::string> calls;
bool exchangeOpen = false;
int modeAtUnlock = -1;

std::string Address(const void* pointer) { return std::to_string(reinterpret_cast<std::uintptr_t>(pointer)); }

const TradePresentation::Host host{
	.IsRunningExchange = []() { calls.push_back("running?"); return exchangeOpen; },
	.CloseExchange = []() { calls.push_back("close"); },
	.CloseExchangeAsk = []() { calls.push_back("closeask"); },
	.CloseExchangeCancel = []() { calls.push_back("closecancel"); },
	.UnlockItem = []() {
		modeAtUnlock = g_pTempInformation ? static_cast<int>(g_pTempInformation->GetMode()) : -1;
		calls.push_back("unlock");
	},
	.RunItemShop = [](GCGoodsList* p) { calls.push_back("shop|" + Address(p)); },
};

struct World : GameModelWorld
{
	MInventory* oldInventory = g_pInventory;
	MMoneyManager* oldMoney = g_pMoneyManager;
	MTradeManager* oldTrade = g_pTradeManager;
	TempInformation temp;
	TempInformation* oldTemp = g_pTempInformation;
	const TradePresentation::Host* oldHost = TradePresentation::SetHost(&host);
	World()
	{
		g_pInventory = new MInventory;
		g_pInventory->Init(3, 2);
		g_pMoneyManager = new MMoneyManager;
		g_pMoneyManager->SetMoney(1000);
		g_pTradeManager = new MTradeManager;
		g_pTradeManager->Init();
		g_pTempInformation = &temp;
		calls.clear();
		exchangeOpen = false;
		modeAtUnlock = -1;
	}
	~World()
	{
		TradePresentation::SetHost(oldHost);
		g_pTempInformation = oldTemp;
		delete g_pTradeManager;	g_pTradeManager = oldTrade;
		delete g_pMoneyManager;	g_pMoneyManager = oldMoney;
		delete g_pInventory;	g_pInventory = oldInventory;
	}
};

void ReceiveTradeFinish(BYTE code)
{
	GCTradeFinish original;
	original.setTargetObjectID(4242);
	original.setCode(code);
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	GCTradeFinish packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	GCTradeFinishHandler::execute(&packet, nullptr);
}

void ReceiveMakeItemFail()
{
	GCMakeItemFail packet;
	GCMakeItemFailHandler::execute(&packet, nullptr);
}

// One reply for each of the six host entries.
void RunEveryHandler(World& world)
{
	exchangeOpen = true;
	world.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	ReceiveTradeFinish(GC_TRADE_FINISH_REJECT);
	world.temp.SetMode(TempInformation::MODE_SKILL_MAKE_ITEM);
	ReceiveMakeItemFail();
	GCGoodsList goods;
	GCGoodsListHandler::execute(&goods, nullptr);
}

} // namespace

TEST(TradePresentationHandlers, AcceptReconsiderAndExecuteDriveOnlyTheTradeManager)
{
	World world;
	ReceiveTradeFinish(GC_TRADE_FINISH_ACCEPT);
	CHECK(g_pTradeManager->IsAcceptOtherTrade());
	ReceiveTradeFinish(GC_TRADE_FINISH_RECONSIDER);
	CHECK(!g_pTradeManager->IsAcceptOtherTrade());

	g_pTradeManager->AcceptMyTrade();
	ReceiveTradeFinish(GC_TRADE_FINISH_ACCEPT);
	ReceiveTradeFinish(GC_TRADE_FINISH_EXECUTE);
	CHECK(!g_pTradeManager->IsAcceptMyTrade());
	CHECK(!g_pTradeManager->IsAcceptOtherTrade());
	CHECK(calls.empty());
}

TEST(TradePresentationHandlers, ARejectionReturnsTheOfferAndClosesWhatIsOpen)
{
	World world;
	g_pTradeManager->GetMyMoneyManager()->SetMoney(300);
	exchangeOpen = true;
	world.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	ReceiveTradeFinish(GC_TRADE_FINISH_REJECT);
	CHECK_EQ(1300, static_cast<long long>(g_pMoneyManager->GetMoney()));
	CHECK_EQ(0, static_cast<long long>(g_pTradeManager->GetMyMoneyManager()->GetMoney()));
	CHECK_EQ(4u, calls.size());
	CHECK(calls.size() == 4 && calls[0] == "running?" && calls[1] == "close"
		&& calls[2] == "closecancel" && calls[3] == "closeask");
	// The rejection leaves the trade request for its dialog to clear.
	CHECK(world.temp.GetMode() == TempInformation::MODE_TRADE_REQUEST);

	calls.clear();
	exchangeOpen = false;
	world.temp.SetMode(TempInformation::MODE_NULL);
	ReceiveTradeFinish(GC_TRADE_FINISH_REJECT);
	CHECK_EQ(1u, calls.size());
	CHECK(!calls.empty() && calls[0] == "running?");
}

TEST(TradePresentationHandlers, WithoutATradeManagerOnlyTheRejectionIsPresented)
{
	World world;
	MTradeManager* trade = g_pTradeManager;
	g_pTradeManager = nullptr;
	for (BYTE code : {BYTE(GC_TRADE_FINISH_ACCEPT), BYTE(GC_TRADE_FINISH_RECONSIDER), BYTE(GC_TRADE_FINISH_EXECUTE),
		BYTE(GC_TRADE_FINISH_MAX), BYTE(0xFF)})
		ReceiveTradeFinish(code);
	CHECK(calls.empty());
	exchangeOpen = true;
	ReceiveTradeFinish(GC_TRADE_FINISH_REJECT);
	CHECK_EQ(2u, calls.size());
	g_pTradeManager = trade;
}

TEST(TradePresentationHandlers, ACraftingFailureUnlocksOnlyForAPendingCraftingRequest)
{
	World world;
	world.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	ReceiveMakeItemFail();
	CHECK(calls.empty());
	CHECK(world.temp.GetMode() == TempInformation::MODE_TRADE_REQUEST);

	world.temp.SetMode(TempInformation::MODE_SKILL_MAKE_ITEM);
	ReceiveMakeItemFail();
	CHECK_EQ(1u, calls.size());
	CHECK(!calls.empty() && calls[0] == "unlock");
	CHECK_EQ(static_cast<int>(TempInformation::MODE_NULL), modeAtUnlock);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
}

TEST(TradePresentationHandlers, TheGoodsListPassesThrough)
{
	World world;
	GCGoodsList goods;
	GCGoodsListHandler::execute(&goods, nullptr);
	CHECK_EQ(1u, calls.size());
	CHECK(!calls.empty() && calls[0] == "shop|" + Address(&goods));
}

TEST(TradePresentationHandlers, MissingHostsAndEntriesSkipPresentationOnly)
{
	World world;
	TradePresentation::SetHost(nullptr);
	g_pTradeManager->GetMyMoneyManager()->SetMoney(50);
	RunEveryHandler(world);
	CHECK(calls.empty());
	CHECK_EQ(1050, static_cast<long long>(g_pMoneyManager->GetMoney()));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	const TradePresentation::Host empty{};
	CHECK(TradePresentation::SetHost(&empty) == nullptr);
	RunEveryHandler(world);
	CHECK(calls.empty());
	CHECK(TradePresentation::SetHost(&host) == &empty);
	RunEveryHandler(world);
	CHECK_EQ(6u, calls.size());
}
