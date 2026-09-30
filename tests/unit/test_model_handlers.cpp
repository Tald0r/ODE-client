//----------------------------------------------------------------------
// test_model_handlers.cpp
//----------------------------------------------------------------------
//
// The eight packet handlers that reach only model state compile in
// gamemodel (docs/RESTRUCTURING.md task 4.15), so each test here reads
// a packet from wire bytes into the packet its real factory creates,
// runs the real handler's execute() on it and checks the model state it
// changes: the phone slots in g_pUserInformation, the trade box and the
// wallet, the server's system switches and the monster-kill quest
// goals. The wire bytes are built field by field from the packet types'
// widths, little-endian, as the server writes them, and every read must
// consume them exactly.
//
// The four phone handlers hold seven of the subscripts
// tests/tools/check_packet_indices.pl counts as raw: a wire slot byte
// into PCSUserName and OtherPCSNumber, each MAX_PCS_SLOT long. Every
// slot byte past the arrays is run here, so a guard that went missing
// would show as a changed neighbour, and under ASan as the access.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "MTradeManager.h"
#include "MInventory.h"
#include "MMoneyManager.h"
#include "SystemAvailabilities.h"
#include "MMonsterKillQuestInfo.h"
#include "MCreatureTable.h"

#include "SocketInputStream.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "Gpackets/GCPhoneConnected.h"
#include "Gpackets/GCPhoneDisconnected.h"
#include "Gpackets/GCPhoneSay.h"
#include "Gpackets/GCRing.h"
#include "Gpackets/GCTradeMoney.h"
#include "Gpackets/GCTradeRemoveItem.h"
#include "Gpackets/GCSystemAvailabilities.h"
#include "Gpackets/GCMonsterKillQuestInfo.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

//----------------------------------------------------------------------
// Wire bytes
//----------------------------------------------------------------------
class Wire
{
public:
	// One field, little-endian, as wide as its type.
	template <class T>
	Wire& Put(T value)
	{
		const std::uint64_t v = (std::uint64_t)value;
		for (size_t i = 0; i < sizeof(T); ++i)
			m_Bytes.push_back((unsigned char)(v >> (8 * i)));
		return *this;
	}

	// A string with its one-byte length in front.
	Wire& Text(const std::string& text)
	{
		Put<BYTE>((BYTE)text.size());
		m_Bytes.insert(m_Bytes.end(), text.begin(), text.end());
		return *this;
	}

	const std::vector<unsigned char>&	Bytes() const	{ return m_Bytes; }

private:
	std::vector<unsigned char>	m_Bytes;
};

struct StreamFixture
{
	Socket				m_Socket;
	SocketInputStream	m_Stream;

	StreamFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 1024)
	{
	}
};

// The packet the factory creates, read from the bytes, which it must
// consume exactly and which must be the size the packet reports.
template <class Factory, class P>
std::unique_ptr<P> ReadPacket(const Wire& wire)
{
	Factory factory;
	std::unique_ptr<Packet> packet(factory.createPacket());
	P* p = dynamic_cast<P*>(packet.get());
	CHECK(p != NULL);
	if (p == NULL)
		return std::unique_ptr<P>();
	packet.release();
	std::unique_ptr<P> owned(p);

	StreamFixture f;
	SocketInputStreamTestAccess::Preload(f.m_Stream, wire.Bytes().data(), (unsigned int)wire.Bytes().size());
	owned->read(f.m_Stream);
	CHECK(f.m_Stream.isEmpty());
	CHECK_EQ(wire.Bytes().size(), (size_t)owned->getPacketSize());
	return owned;
}

std::string	NameOf(const MString& name)
{
	return name.GetString() == NULL ? std::string() : std::string(name.GetString());
}

//----------------------------------------------------------------------
// The model the handlers reach, created as GameInit creates it and
// torn down in reverse; the globals a test found are put back.
//----------------------------------------------------------------------
int		s_Alive = 0;
DWORD	s_Frame = 0;
MonotonicClock::TimePoint	s_Now;

int		DropFrameCount(TYPE_FRAMEID)	{ return 0; }
void	RefreshAffect(MItem*)			{}
void	PlayItemSound(TYPE_SOUNDID)		{}

// A host that carries the trade's accept-delay clock and nothing else of note.
const MItemHost	s_Host = { &s_Frame, DropFrameCount, RefreshAffect, PlayItemSound, &s_Now,
							NULL, NULL, NULL, NULL, NULL, NULL };

const int	kCreatureTypes = 4;

struct HandlerWorld : GameModelWorld
{
	MInventory*						m_pPrevInventory;
	MMoneyManager*					m_pPrevMoney;
	MTradeManager*					m_pPrevTrade;
	SystemAvailabilitiesManager*	m_pPrevSystem;
	MQuestInfoManager*				m_pPrevQuest;
	CREATURE_TABLE*					m_pPrevCreatures;

	HandlerWorld()
	: m_pPrevInventory(g_pInventory), m_pPrevMoney(g_pMoneyManager),
	  m_pPrevTrade(g_pTradeManager), m_pPrevSystem(g_pSystemAvailableManager),
	  m_pPrevQuest(g_pQuestInfoManager), m_pPrevCreatures(g_pCreatureTable)
	{
		s_Alive = 0;
		s_Now = MonotonicClock::TimePoint();

		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).SetGrid(1, 1);
		g_pClientConfig->TRADE_ACCEPT_DELAY_TIME = 5000;

		g_pInventory = new MInventory;
		g_pInventory->Init(3, 2);
		g_pMoneyManager = new MMoneyManager;
		g_pMoneyManager->SetMoney(1000);
		MItem::SetHost(&s_Host);

		g_pTradeManager = new MTradeManager;
		g_pTradeManager->Init();

		g_pSystemAvailableManager = new SystemAvailabilitiesManager;
		g_pQuestInfoManager = new MQuestInfoManager;

		g_pCreatureTable = new CREATURE_TABLE;
		g_pCreatureTable->Init(kCreatureTypes);
		testfw::MutableRow(*g_pCreatureTable, 0).Name = "Soldier";
		testfw::MutableRow(*g_pCreatureTable, 1).Name = "Wolf";
		testfw::MutableRow(*g_pCreatureTable, 3).Name = "Ghoul";
		// Row 2 keeps no name.
	}

	~HandlerWorld()
	{
		delete g_pCreatureTable;			g_pCreatureTable = m_pPrevCreatures;
		delete g_pQuestInfoManager;			g_pQuestInfoManager = m_pPrevQuest;
		delete g_pSystemAvailableManager;	g_pSystemAvailableManager = m_pPrevSystem;
		delete g_pTradeManager;				g_pTradeManager = m_pPrevTrade;
		delete g_pMoneyManager;				g_pMoneyManager = m_pPrevMoney;
		delete g_pInventory;				g_pInventory = m_pPrevInventory;
	}
};

struct Sword : public MItem
{
	Sword(TYPE_OBJECTID id)			{ SetID(id); SetItemType(0); s_Alive++; }
	~Sword()						{ s_Alive--; }
	ITEM_CLASS	GetItemClass() const	{ return ITEM_CLASS_SWORD; }
};

//----------------------------------------------------------------------
// Phone slots
//----------------------------------------------------------------------

// Every slot holds a number and a name the test can recognise.
void	FillSlots()
{
	for (int i = 0; i < MAX_PCS_SLOT; ++i)
	{
		g_pUserInformation->OtherPCSNumber[i] = 7000 + i;
		g_pUserInformation->PCSUserName[i] = (std::string("caller") + (char)('0' + i)).c_str();
	}
}

// The slots as FillSlots left them, but for `changed`.
void	CheckSlotsFilledBut(int changed)
{
	for (int i = 0; i < MAX_PCS_SLOT; ++i)
	{
		if (i == changed)
			continue;
		CHECK_EQ(7000 + i, g_pUserInformation->OtherPCSNumber[i]);
		CHECK(NameOf(g_pUserInformation->PCSUserName[i]) == std::string("caller") + (char)('0' + i));
	}
}

// The slot bytes past the arrays: the first one out and the largest.
const int	kOutOfRangeSlots[] = { MAX_PCS_SLOT, MAX_PCS_SLOT + 1, 0x7F, 0x80, 0xFF };

Wire	PhoneConnectedBytes(PhoneNumber_t number, int slot, const std::string& name)
{
	Wire w;
	w.Put<PhoneNumber_t>(number).Put<SlotID_t>((SlotID_t)slot).Text(name);
	return w;
}

//----------------------------------------------------------------------
// Trade money
//----------------------------------------------------------------------
Wire	TradeMoneyBytes(Gold_t amount, BYTE code)
{
	Wire w;
	w.Put<ObjectID_t>(0x01020304).Put<Gold_t>(amount).Put<BYTE>(code);
	return w;
}

void	RunTradeMoney(Gold_t amount, BYTE code)
{
	std::unique_ptr<GCTradeMoney> p = ReadPacket<GCTradeMoneyFactory, GCTradeMoney>(TradeMoneyBytes(amount, code));
	CHECK(p != NULL);
	if (p == NULL)
		return;
	CHECK_EQ(amount, p->getAmount());
	CHECK_EQ(code, p->getCode());
	GCTradeMoneyHandler::execute(p.get(), NULL);
}

MMoneyManager*	MyBox()		{ return g_pTradeManager->GetMyMoneyManager(); }
MMoneyManager*	OtherBox()	{ return g_pTradeManager->GetOtherMoneyManager(); }

// Both sides pressed OK at time 0, so a refusal starts the delay then.
void	AcceptBoth()
{
	g_pTradeManager->AcceptMyTrade();
	g_pTradeManager->AcceptOtherTrade();
}

//----------------------------------------------------------------------
// System availabilities
//----------------------------------------------------------------------
Wire	SystemBytes(DWORD flag, BYTE degree, BYTE skillLimit)
{
	Wire w;
	w.Put<DWORD>(flag).Put<BYTE>(degree).Put<BYTE>(skillLimit);
	return w;
}

void	RunSystem(DWORD flag, BYTE degree, BYTE skillLimit)
{
	std::unique_ptr<GCSystemAvailabilities> p =
		ReadPacket<GCSystemAvailabilitiesFactory, GCSystemAvailabilities>(SystemBytes(flag, degree, skillLimit));
	CHECK(p != NULL);
	if (p == NULL)
		return;
	GCSystemAvailabilitiesHandler::execute(p.get(), NULL);
}

// Zone 100 opens at degree 0, 200 at 1 and 800 at 8 (the wire's 1, 2 and 9).
const char* const	kZoneFilter =
	"Z0\n" "100\n" "99999\n"
	"Z1\n" "200\n" "99999\n"
	"Z8\n" "800\n" "99999\n";

//----------------------------------------------------------------------
// Monster-kill quest info
//----------------------------------------------------------------------
struct QuestRow
{
	QuestID_t		questID;
	SpriteType_t	sType;
	WORD			goal;
	DWORD			timeLimit;
};

Wire	QuestBytes(const std::vector<QuestRow>& rows)
{
	Wire w;
	w.Put<BYTE>((BYTE)rows.size());
	for (const QuestRow& r : rows)
		w.Put<QuestID_t>(r.questID).Put<SpriteType_t>(r.sType).Put<WORD>(r.goal).Put<DWORD>(r.timeLimit);
	return w;
}

void	RunQuest(const std::vector<QuestRow>& rows)
{
	std::unique_ptr<GCMonsterKillQuestInfo> p =
		ReadPacket<GCMonsterKillQuestInfoFactory, GCMonsterKillQuestInfo>(QuestBytes(rows));
	CHECK(p != NULL);
	if (p == NULL)
		return;
	GCMonsterKillQuestInfoHandler::execute(p.get(), NULL);
	CHECK(p->empty());			// the handler drains the packet
}

std::string	QuestName(QUEST_INFO* pInfo)
{
	return pInfo->GetName() == NULL ? std::string() : std::string(pInfo->GetName());
}

} // namespace

//======================================================================
// GCPhoneConnected: the number and the name land in the packet's slot
//======================================================================
TEST(ModelHandlers, PhoneConnectedStoresTheCallerInItsSlot)
{
	HandlerWorld world;

	for (int slot = 0; slot < MAX_PCS_SLOT; ++slot)
	{
		FillSlots();
		std::unique_ptr<GCPhoneConnected> p = ReadPacket<GCPhoneConnectedFactory, GCPhoneConnected>(
			PhoneConnectedBytes(0x00ABCDEF, slot, "Alucard"));
		CHECK(p != NULL);
		if (p == NULL)
			return;
		GCPhoneConnectedHandler::execute(p.get(), NULL);

		CHECK_EQ(0x00ABCDEF, g_pUserInformation->OtherPCSNumber[slot]);
		CHECK(NameOf(g_pUserInformation->PCSUserName[slot]) == std::string("Alucard"));
		CheckSlotsFilledBut(slot);
	}
}

TEST(ModelHandlers, PhoneConnectedIgnoresASlotPastTheArrays)
{
	HandlerWorld world;

	for (int slot : kOutOfRangeSlots)
	{
		FillSlots();
		std::unique_ptr<GCPhoneConnected> p = ReadPacket<GCPhoneConnectedFactory, GCPhoneConnected>(
			PhoneConnectedBytes(1234, slot, "Intruder"));
		CHECK(p != NULL);
		if (p == NULL)
			return;
		CHECK_EQ(slot, (int)p->getSlotID());
		GCPhoneConnectedHandler::execute(p.get(), NULL);
		CheckSlotsFilledBut(-1);
	}
}

//======================================================================
// GCRing: the same two stores as GCPhoneConnected
//======================================================================
TEST(ModelHandlers, RingStoresTheCallerInItsSlot)
{
	HandlerWorld world;

	for (int slot = 0; slot < MAX_PCS_SLOT; ++slot)
	{
		FillSlots();
		std::unique_ptr<GCRing> p = ReadPacket<GCRingFactory, GCRing>(PhoneConnectedBytes(0x01020304, slot, "Seraph"));
		CHECK(p != NULL);
		if (p == NULL)
			return;
		GCRingHandler::execute(p.get(), NULL);

		CHECK_EQ(0x01020304, g_pUserInformation->OtherPCSNumber[slot]);
		CHECK(NameOf(g_pUserInformation->PCSUserName[slot]) == std::string("Seraph"));
		CheckSlotsFilledBut(slot);
	}
}

TEST(ModelHandlers, RingIgnoresASlotPastTheArrays)
{
	HandlerWorld world;

	for (int slot : kOutOfRangeSlots)
	{
		FillSlots();
		std::unique_ptr<GCRing> p = ReadPacket<GCRingFactory, GCRing>(PhoneConnectedBytes(99, slot, "Intruder"));
		CHECK(p != NULL);
		if (p == NULL)
			return;
		GCRingHandler::execute(p.get(), NULL);
		CheckSlotsFilledBut(-1);
	}
}

//======================================================================
// GCPhoneDisconnected: the slot's number goes to 0 and its name is freed
//======================================================================
TEST(ModelHandlers, PhoneDisconnectedClearsItsSlot)
{
	HandlerWorld world;

	for (int slot = 0; slot < MAX_PCS_SLOT; ++slot)
	{
		FillSlots();
		Wire w;
		w.Put<PhoneNumber_t>(7000 + slot).Put<SlotID_t>((SlotID_t)slot);
		std::unique_ptr<GCPhoneDisconnected> p = ReadPacket<GCPhoneDisconnectedFactory, GCPhoneDisconnected>(w);
		CHECK(p != NULL);
		if (p == NULL)
			return;
		GCPhoneDisconnectedHandler::execute(p.get(), NULL);

		CHECK_EQ(0, g_pUserInformation->OtherPCSNumber[slot]);
		// An empty MString keeps no storage.
		CHECK(g_pUserInformation->PCSUserName[slot].GetString() == NULL);
		CheckSlotsFilledBut(slot);
	}
}

TEST(ModelHandlers, PhoneDisconnectedIgnoresASlotPastTheArrays)
{
	HandlerWorld world;

	for (int slot : kOutOfRangeSlots)
	{
		FillSlots();
		Wire w;
		w.Put<PhoneNumber_t>(7000).Put<SlotID_t>((SlotID_t)slot);
		std::unique_ptr<GCPhoneDisconnected> p = ReadPacket<GCPhoneDisconnectedFactory, GCPhoneDisconnected>(w);
		CHECK(p != NULL);
		if (p == NULL)
			return;
		GCPhoneDisconnectedHandler::execute(p.get(), NULL);
		CheckSlotsFilledBut(-1);
	}
}

//======================================================================
// GCPhoneSay: reads the slot's name to build a line that goes nowhere
// (its chat call is commented out), so it changes no model state; the
// test is that no slot byte reaches past the name array or changes it.
//======================================================================
TEST(ModelHandlers, PhoneSayChangesNoSlotForAnySlotByte)
{
	HandlerWorld world;

	for (int slot = 0; slot <= 0xFF; ++slot)
	{
		FillSlots();
		Wire w;
		w.Put<SlotID_t>((SlotID_t)slot).Text("hello");
		std::unique_ptr<GCPhoneSay> p = ReadPacket<GCPhoneSayFactory, GCPhoneSay>(w);
		CHECK(p != NULL);
		if (p == NULL)
			return;
		CHECK(p->getMessage() == std::string("hello"));
		GCPhoneSayHandler::execute(p.get(), NULL);
		CheckSlotsFilledBut(-1);
	}
}

//======================================================================
// GCTradeMoney: the other side's offer, and the results of the
// player's own, moving money between the wallet and the trade box.
// Any change clears both OKs; a decrease on the other side also
// restarts the accept delay.
//======================================================================
TEST(ModelHandlers, TradeMoneyIncreaseAddsToTheOtherSidesOffer)
{
	HandlerWorld world;
	CHECK(OtherBox()->SetMoney(300));
	AcceptBoth();

	RunTradeMoney(200, GC_TRADE_MONEY_INCREASE);

	CHECK_EQ(500, OtherBox()->GetMoney());
	CHECK_EQ(0, MyBox()->GetMoney());
	CHECK_EQ(1000, g_pMoneyManager->GetMoney());
	CHECK_EQ(false, g_pTradeManager->IsAcceptMyTrade());
	CHECK_EQ(false, g_pTradeManager->IsAcceptOtherTrade());
	CHECK_EQ(false, g_pTradeManager->IsAcceptTime());		// the refusals started the delay
}

TEST(ModelHandlers, TradeMoneyDecreaseTakesFromTheOtherSidesOfferAndDelaysOK)
{
	HandlerWorld world;
	CHECK(OtherBox()->SetMoney(300));

	RunTradeMoney(200, GC_TRADE_MONEY_DECREASE);

	CHECK_EQ(100, OtherBox()->GetMoney());
	CHECK_EQ(1000, g_pMoneyManager->GetMoney());
	// Nobody had pressed OK, and the delay still starts.
	CHECK_EQ(false, g_pTradeManager->IsAcceptTime());
	s_Now = MonotonicClock::FromMillis(4999);
	CHECK_EQ(false, g_pTradeManager->IsAcceptTime());
	s_Now = MonotonicClock::FromMillis(5000);
	CHECK(g_pTradeManager->IsAcceptTime());
}

TEST(ModelHandlers, TradeMoneyIncreaseResultMovesTheWalletIntoTheBox)
{
	HandlerWorld world;
	AcceptBoth();

	RunTradeMoney(400, GC_TRADE_MONEY_INCREASE_RESULT);

	CHECK_EQ(600, g_pMoneyManager->GetMoney());
	CHECK_EQ(400, MyBox()->GetMoney());
	CHECK_EQ(0, OtherBox()->GetMoney());
	CHECK_EQ(false, g_pTradeManager->IsAcceptMyTrade());
	CHECK_EQ(false, g_pTradeManager->IsAcceptOtherTrade());

	// The whole wallet.
	RunTradeMoney(600, GC_TRADE_MONEY_INCREASE_RESULT);
	CHECK_EQ(0, g_pMoneyManager->GetMoney());
	CHECK_EQ(1000, MyBox()->GetMoney());
}

TEST(ModelHandlers, TradeMoneyDecreaseResultMovesTheBoxBackIntoTheWallet)
{
	HandlerWorld world;
	RunTradeMoney(400, GC_TRADE_MONEY_INCREASE_RESULT);
	AcceptBoth();

	RunTradeMoney(150, GC_TRADE_MONEY_DECREASE_RESULT);

	CHECK_EQ(750, g_pMoneyManager->GetMoney());
	CHECK_EQ(250, MyBox()->GetMoney());
	CHECK_EQ(false, g_pTradeManager->IsAcceptMyTrade());
	CHECK_EQ(false, g_pTradeManager->IsAcceptOtherTrade());

	RunTradeMoney(250, GC_TRADE_MONEY_DECREASE_RESULT);
	CHECK_EQ(1000, g_pMoneyManager->GetMoney());
	CHECK_EQ(0, MyBox()->GetMoney());
}

TEST(ModelHandlers, TradeMoneyWithAnUnknownCodeChangesNothing)
{
	HandlerWorld world;
	CHECK(OtherBox()->SetMoney(300));
	CHECK(MyBox()->SetMoney(200));
	AcceptBoth();

	for (int code = GC_TRADE_MONEY_DECREASE_RESULT + 1; code <= 0xFF; ++code)
		RunTradeMoney(100, (BYTE)code);

	CHECK_EQ(300, OtherBox()->GetMoney());
	CHECK_EQ(200, MyBox()->GetMoney());
	CHECK_EQ(1000, g_pMoneyManager->GetMoney());
	CHECK(g_pTradeManager->IsAcceptMyTrade());
	CHECK(g_pTradeManager->IsAcceptOtherTrade());
	CHECK(g_pTradeManager->IsAcceptTime());
}

TEST(ModelHandlers, TradeMoneyWithoutATradeChangesNothing)
{
	HandlerWorld world;
	MTradeManager* pTrade = g_pTradeManager;
	g_pTradeManager = NULL;

	for (BYTE code = GC_TRADE_MONEY_INCREASE; code <= GC_TRADE_MONEY_DECREASE_RESULT; ++code)
		RunTradeMoney(100, code);

	CHECK_EQ(1000, g_pMoneyManager->GetMoney());
	g_pTradeManager = pTrade;
}

//----------------------------------------------------------------------
// A result reports a move the server has already made. The server
// rejects a move before it changes anything (decideMoneyIncrease and
// decideMoneyDecrease in its trade/TradeTableDecision.cpp), and
// otherwise sets its wallet and its stake, then sends the result. So
// each side here follows the server on its own: when this client's
// wallet cannot follow (it disagrees with the server's), the trade box
// still does, and the other way round. Both OKs still clear.
//----------------------------------------------------------------------
TEST(ModelHandlers, TradeMoneyIncreaseResultFillsTheBoxWhenTheWalletCannotCover)
{
	HandlerWorld world;
	CHECK(g_pMoneyManager->SetMoney(100));
	AcceptBoth();

	RunTradeMoney(500, GC_TRADE_MONEY_INCREASE_RESULT);

	CHECK_EQ(500, MyBox()->GetMoney());				// the server's stake
	CHECK_EQ(100, g_pMoneyManager->GetMoney());		// cannot go below 0
	CHECK_EQ(false, g_pTradeManager->IsAcceptMyTrade());
	CHECK_EQ(false, g_pTradeManager->IsAcceptOtherTrade());
}

TEST(ModelHandlers, TradeMoneyIncreaseResultEmptiesTheWalletWhenTheBoxCannotHold)
{
	HandlerWorld world;
	CHECK(MyBox()->SetMoney(MyBox()->GetMoneyLimit() - 500));

	RunTradeMoney(1000, GC_TRADE_MONEY_INCREASE_RESULT);

	CHECK_EQ(0, g_pMoneyManager->GetMoney());		// the server's wallet lost 1000
	CHECK_EQ(MyBox()->GetMoneyLimit() - 500, MyBox()->GetMoney());
}

TEST(ModelHandlers, TradeMoneyDecreaseResultFillsTheWalletWhenTheBoxCannotCover)
{
	HandlerWorld world;
	CHECK(MyBox()->SetMoney(100));
	AcceptBoth();

	RunTradeMoney(500, GC_TRADE_MONEY_DECREASE_RESULT);

	CHECK_EQ(1500, g_pMoneyManager->GetMoney());	// the server's wallet gained 500
	CHECK_EQ(100, MyBox()->GetMoney());				// cannot go below 0
	CHECK_EQ(false, g_pTradeManager->IsAcceptMyTrade());
	CHECK_EQ(false, g_pTradeManager->IsAcceptOtherTrade());
}

TEST(ModelHandlers, TradeMoneyDecreaseResultEmptiesTheBoxWhenTheWalletCannotHold)
{
	HandlerWorld world;
	g_pMoneyManager->SetMoneyLimit(1000);		// full at 1000
	CHECK(MyBox()->SetMoney(100));

	RunTradeMoney(100, GC_TRADE_MONEY_DECREASE_RESULT);

	CHECK_EQ(0, MyBox()->GetMoney());				// the server's stake
	CHECK_EQ(1000, g_pMoneyManager->GetMoney());
}

//----------------------------------------------------------------------
// Gold_t is unsigned and no wallet holds more than two billion (the
// server's MAX_MONEY), so an amount past INT_MAX is not one the server
// moved; narrowed to the handler's int it turned negative and ran each
// move backwards. Such a packet changes nothing, OKs included.
//----------------------------------------------------------------------
TEST(ModelHandlers, TradeMoneyPastTheIntRangeChangesNothing)
{
	HandlerWorld world;
	const Gold_t	amounts[] = { 0x80000000u, 0x80000001u, 0xFFFFFFFEu, 0xFFFFFFFFu };

	for (Gold_t amount : amounts)
	{
		for (BYTE code = GC_TRADE_MONEY_INCREASE; code <= GC_TRADE_MONEY_DECREASE_RESULT; ++code)
		{
			CHECK(OtherBox()->SetMoney(300));
			CHECK(MyBox()->SetMoney(200));
			CHECK(g_pMoneyManager->SetMoney(1000));
			AcceptBoth();

			RunTradeMoney(amount, code);

			CHECK_EQ(300, OtherBox()->GetMoney());
			CHECK_EQ(200, MyBox()->GetMoney());
			CHECK_EQ(1000, g_pMoneyManager->GetMoney());
			CHECK(g_pTradeManager->IsAcceptMyTrade());
			CHECK(g_pTradeManager->IsAcceptOtherTrade());
		}
	}

	// INT_MAX itself is an int; the wallets refuse it as too much.
	CHECK(OtherBox()->SetMoney(300));
	RunTradeMoney(0x7FFFFFFFu, GC_TRADE_MONEY_INCREASE_RESULT);
	RunTradeMoney(0x7FFFFFFFu, GC_TRADE_MONEY_DECREASE_RESULT);
	RunTradeMoney(0x7FFFFFFFu, GC_TRADE_MONEY_INCREASE);
	RunTradeMoney(0x7FFFFFFFu, GC_TRADE_MONEY_DECREASE);
	CHECK_EQ(300, OtherBox()->GetMoney());
	CHECK_EQ(200, MyBox()->GetMoney());
	CHECK_EQ(1000, g_pMoneyManager->GetMoney());
}

//======================================================================
// GCTradeRemoveItem: the other side takes an item off its offer
//======================================================================
TEST(ModelHandlers, TradeRemoveItemDeletesTheOtherSidesItem)
{
	HandlerWorld world;
	MInventory* pOther = g_pTradeManager->GetOtherInventory();
	CHECK(pOther->AddItem(new Sword(11)));
	CHECK(pOther->AddItem(new Sword(12)));
	CHECK_EQ(2, s_Alive);
	AcceptBoth();

	Wire w;
	w.Put<ObjectID_t>(0x01020304).Put<ObjectID_t>(11);
	std::unique_ptr<GCTradeRemoveItem> p = ReadPacket<GCTradeRemoveItemFactory, GCTradeRemoveItem>(w);
	CHECK(p != NULL);
	if (p == NULL)
		return;
	GCTradeRemoveItemHandler::execute(p.get(), NULL);

	CHECK_EQ(1, s_Alive);
	CHECK_EQ(1, pOther->GetItemNum());
	CHECK(pOther->GetItem(11) == NULL);
	CHECK(pOther->GetItem(12) != NULL);
	CHECK_EQ(false, g_pTradeManager->IsAcceptMyTrade());
	CHECK_EQ(false, g_pTradeManager->IsAcceptOtherTrade());
	CHECK_EQ(false, g_pTradeManager->IsAcceptTime());
}

TEST(ModelHandlers, TradeRemoveItemOfAnUnknownIdStillClearsTheOKs)
{
	HandlerWorld world;
	MInventory* pOther = g_pTradeManager->GetOtherInventory();
	CHECK(pOther->AddItem(new Sword(11)));
	AcceptBoth();

	Wire w;
	w.Put<ObjectID_t>(0x01020304).Put<ObjectID_t>(99);
	std::unique_ptr<GCTradeRemoveItem> p = ReadPacket<GCTradeRemoveItemFactory, GCTradeRemoveItem>(w);
	CHECK(p != NULL);
	if (p == NULL)
		return;
	GCTradeRemoveItemHandler::execute(p.get(), NULL);

	CHECK_EQ(1, s_Alive);
	CHECK_EQ(1, pOther->GetItemNum());
	CHECK_EQ(false, g_pTradeManager->IsAcceptMyTrade());
	CHECK_EQ(false, g_pTradeManager->IsAcceptOtherTrade());
	// The handler restarts the delay whether or not the item was there.
	CHECK_EQ(false, g_pTradeManager->IsAcceptTime());
}

TEST(ModelHandlers, TradeRemoveItemWithoutATradeChangesNothing)
{
	HandlerWorld world;
	MTradeManager* pTrade = g_pTradeManager;
	g_pTradeManager = NULL;

	Wire w;
	w.Put<ObjectID_t>(0x01020304).Put<ObjectID_t>(11);
	std::unique_ptr<GCTradeRemoveItem> p = ReadPacket<GCTradeRemoveItemFactory, GCTradeRemoveItem>(w);
	CHECK(p != NULL);
	if (p != NULL)
		GCTradeRemoveItemHandler::execute(p.get(), NULL);

	g_pTradeManager = pTrade;
	CHECK_EQ(0, s_Alive);
}

//======================================================================
// GCSystemAvailabilities: the switches, the skill level limit, and the
// open degree, which the wire counts from 1
//======================================================================
TEST(ModelHandlers, SystemAvailabilitiesSetsTheSwitchesAndTheSkillLimit)
{
	HandlerWorld world;

	// Guild (bit 4) and market (bit 7) only.
	RunSystem((1u << 4) | (1u << 7), 1, 30);
	CHECK(g_pSystemAvailableManager->IsAvailableGuildSystem());
	CHECK(g_pSystemAvailableManager->IsAvailableMarketSystem());
	CHECK_EQ(false, g_pSystemAvailableManager->IsAvailablePartySystem());
	CHECK_EQ(false, g_pSystemAvailableManager->IsAvailableGambleSystem());
	CHECK_EQ(false, g_pSystemAvailableManager->IsAvailableCTFSystem());
	CHECK_EQ(30, (int)g_pSystemAvailableManager->GetLimitLearnSkillLevel());

	// Every bit: every switch on; bits past the last switch are dropped.
	RunSystem(0xFFFFFFFFu, 1, 0xFF);
	CHECK(g_pSystemAvailableManager->IsAvailablePartySystem());
	CHECK(g_pSystemAvailableManager->IsAvailableGambleSystem());
	CHECK(g_pSystemAvailableManager->IsAvailableRankBonusSystem());
	CHECK(g_pSystemAvailableManager->IsAvailableEnchantSystem());
	CHECK(g_pSystemAvailableManager->IsAvailableGuildSystem());
	CHECK(g_pSystemAvailableManager->IsAvailableMasterLairSystem());
	CHECK(g_pSystemAvailableManager->IsAvailablePKZoneSystem());
	CHECK(g_pSystemAvailableManager->IsAvailableMarketSystem());
	CHECK(g_pSystemAvailableManager->IsAvailableGrandMasterEffectSystem());
	CHECK(g_pSystemAvailableManager->IsAvailableCoupleSystem());
	CHECK(g_pSystemAvailableManager->IsAvailableGuildWarSystem());
	CHECK(g_pSystemAvailableManager->IsAvailableRaceWarSystem());
	CHECK(g_pSystemAvailableManager->IsAvailableCTFSystem());
	CHECK_EQ(0xFF, (int)g_pSystemAvailableManager->GetLimitLearnSkillLevel());

	RunSystem(0, 1, 0);
	CHECK_EQ(false, g_pSystemAvailableManager->IsAvailableGuildSystem());
	CHECK_EQ(false, g_pSystemAvailableManager->IsAvailableCTFSystem());
	CHECK_EQ(0, (int)g_pSystemAvailableManager->GetLimitLearnSkillLevel());
}

TEST(ModelHandlers, SystemAvailabilitiesOpensZonesByTheWiresDegreeLessOne)
{
	HandlerWorld world;
	std::istringstream filter(kZoneFilter);
	CHECK(g_pSystemAvailableManager->LoadFromStream(filter));

	// Degree 1 is the client's 0: only the first list.
	RunSystem(0, 1, 0);
	CHECK(g_pSystemAvailableManager->ZoneFiltering(100));
	CHECK_EQ(false, g_pSystemAvailableManager->ZoneFiltering(200));
	CHECK_EQ(false, g_pSystemAvailableManager->ZoneFiltering(800));

	RunSystem(0, 2, 0);
	CHECK(g_pSystemAvailableManager->ZoneFiltering(100));
	CHECK(g_pSystemAvailableManager->ZoneFiltering(200));
	CHECK_EQ(false, g_pSystemAvailableManager->ZoneFiltering(800));

	// Degree 9 is the last list the client holds (MAX_OPEN_DEGREE is 9).
	RunSystem(0, 9, 0);
	CHECK(g_pSystemAvailableManager->ZoneFiltering(800));
	CHECK_EQ(false, g_pSystemAvailableManager->ZoneFiltering(12345));

	// Degree 10 and above is past every list: every zone opens.
	RunSystem(0, 10, 0);
	CHECK(g_pSystemAvailableManager->ZoneFiltering(12345));
	RunSystem(0, 0xFF, 0);
	CHECK(g_pSystemAvailableManager->ZoneFiltering(12345));

	// Degree 0 wraps to 0xFF in the BYTE the manager keeps: every zone
	// opens, as before the server's first packet.
	RunSystem(0, 0, 0);
	CHECK(g_pSystemAvailableManager->ZoneFiltering(12345));
}

//======================================================================
// GCMonsterKillQuestInfo: each entry sets a quest's goal, time limit
// and the name of the creature to kill
//======================================================================
TEST(ModelHandlers, MonsterKillQuestInfoAddsEachQuestWithItsCreaturesName)
{
	HandlerWorld world;

	RunQuest({ { 501, 1, 20, 3600 }, { 502, 3, 5, 0 } });

	CHECK_EQ((size_t)2, g_pQuestInfoManager->size());
	QUEST_INFO* pWolf = g_pQuestInfoManager->GetInfo(501);
	CHECK(pWolf != NULL);
	if (pWolf != NULL)
	{
		CHECK_EQ(501u, pWolf->GetID());
		CHECK_EQ(20u, pWolf->GetGoal());
		CHECK_EQ(3600u, pWolf->GetTimeLimit());
		CHECK(QuestName(pWolf) == std::string("Wolf"));
		CHECK(pWolf->GetType() == QUEST_INFO_MONSTER_KILL);
	}
	QUEST_INFO* pGhoul = g_pQuestInfoManager->GetInfo(502);
	CHECK(pGhoul != NULL);
	if (pGhoul != NULL)
	{
		CHECK_EQ(5u, pGhoul->GetGoal());
		CHECK_EQ(0u, pGhoul->GetTimeLimit());
		CHECK(QuestName(pGhoul) == std::string("Ghoul"));
	}
}

TEST(ModelHandlers, MonsterKillQuestInfoUpdatesAQuestItAlreadyHolds)
{
	HandlerWorld world;

	RunQuest({ { 501, 1, 20, 3600 } });
	QUEST_INFO* pBefore = g_pQuestInfoManager->GetInfo(501);

	RunQuest({ { 501, 0, 0xFFFF, 0xFFFFFFFFu } });

	CHECK_EQ((size_t)1, g_pQuestInfoManager->size());
	QUEST_INFO* pAfter = g_pQuestInfoManager->GetInfo(501);
	CHECK(pAfter == pBefore);
	if (pAfter != NULL)
	{
		CHECK_EQ(0xFFFFu, pAfter->GetGoal());
		CHECK_EQ(0xFFFFFFFFu, pAfter->GetTimeLimit());
		CHECK(QuestName(pAfter) == std::string("Soldier"));
	}
}

TEST(ModelHandlers, MonsterKillQuestInfoWithNoEntriesChangesNothing)
{
	HandlerWorld world;

	RunQuest({});

	CHECK_EQ((size_t)0, g_pQuestInfoManager->size());
}

//----------------------------------------------------------------------
// The sprite type is the server's, and the creature table answers a
// type it does not hold with an empty row, as it does for a row with
// no name: the quest keeps an empty name.
//----------------------------------------------------------------------
TEST(ModelHandlers, MonsterKillQuestInfoKeepsAnEmptyNameForANamelessCreature)
{
	HandlerWorld world;

	RunQuest({ { 601, 2, 7, 60 },						// in the table, no name
			   { 602, (SpriteType_t)kCreatureTypes, 8, 70 },	// the first type past it
			   { 603, 0xFFFF, 9, 80 } });					// the last the wire holds

	CHECK_EQ((size_t)3, g_pQuestInfoManager->size());
	for (QuestID_t id = 601; id <= 603; ++id)
	{
		QUEST_INFO* pInfo = g_pQuestInfoManager->GetInfo(id);
		CHECK(pInfo != NULL);
		if (pInfo == NULL)
			continue;
		CHECK_EQ(id - 594, pInfo->GetGoal());
		CHECK_EQ((id - 595) * 10, pInfo->GetTimeLimit());
		CHECK(QuestName(pInfo).empty());
	}
}
