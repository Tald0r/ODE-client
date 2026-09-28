//----------------------------------------------------------------------
// test_money_manager.cpp
//----------------------------------------------------------------------
//
// MMoneyManager (gamemodel, docs/RESTRUCTURING.md task 4.2): a wallet
// with a limit. The contract under test is the limit - no balance
// below zero, none above the limit, and CanAddMoney/CanUseMoney
// answering for the BALANCE the operation would leave, which is what
// the trade manager asks before it moves money - plus the storage
// hint, which the executable receives through a hook.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MMoneyManager.h"

namespace {

int	g_HintCount = 0;
void	CountHint()	{ g_HintCount++; }

} // namespace

TEST(MoneyManager, StartsEmptyWithTheTwoBillionLimit)
{
	MMoneyManager wallet;
	CHECK_EQ(0, wallet.GetMoney());
	CHECK_EQ(2000000000, wallet.GetMoneyLimit());
	CHECK_EQ(2000000000, wallet.GetMaxAddMoney());
}

TEST(MoneyManager, SetMoneyRefusesNegativeAndOverTheLimit)
{
	MMoneyManager wallet;
	wallet.SetMoneyLimit(1000);

	CHECK(wallet.SetMoney(1000));
	CHECK_EQ(1000, wallet.GetMoney());
	CHECK(!wallet.SetMoney(1001));
	CHECK_EQ(1000, wallet.GetMoney());	// a refused set leaves the balance
	CHECK(!wallet.SetMoney(-1));
	CHECK_EQ(1000, wallet.GetMoney());
	CHECK(wallet.SetMoney(0));
	CHECK_EQ(0, wallet.GetMoney());
}

TEST(MoneyManager, AddAndUseMoveTheBalanceWithinTheLimit)
{
	MMoneyManager wallet;
	wallet.SetMoneyLimit(1000);

	CHECK(wallet.AddMoney(600));
	CHECK(wallet.AddMoney(400));
	CHECK_EQ(1000, wallet.GetMoney());
	CHECK(!wallet.AddMoney(1));
	CHECK_EQ(1000, wallet.GetMoney());

	CHECK(wallet.UseMoney(999));
	CHECK_EQ(1, wallet.GetMoney());
	CHECK(!wallet.UseMoney(2));
	CHECK_EQ(1, wallet.GetMoney());
	CHECK(wallet.UseMoney(1));
	CHECK_EQ(0, wallet.GetMoney());
}

//----------------------------------------------------------------------
// CanAddMoney used to compare the AMOUNT against the limit and ignore
// the balance, so a wallet near its limit said yes and the AddMoney
// that followed said no - the trade manager asks CanAddMoney before it
// accepts a trade and AddMoney after, and between those two answers
// the other side's money had nowhere to go.
//----------------------------------------------------------------------
TEST(MoneyManager, CanAddMoneyAnswersForTheBalanceItWouldLeave)
{
	MMoneyManager wallet;
	wallet.SetMoneyLimit(1000);
	CHECK(wallet.SetMoney(900));

	CHECK(wallet.CanAddMoney(100));
	CHECK(!wallet.CanAddMoney(101));
	CHECK(wallet.CanAddMoney(0));
	CHECK(!wallet.CanAddMoney(-1));

	// CanAddMoney and AddMoney agree on every amount around the edge.
	for (int amount = 98; amount <= 102; amount++)
	{
		MMoneyManager probe(wallet);
		CHECK_EQ(wallet.CanAddMoney(amount), probe.AddMoney(amount));
	}
}

TEST(MoneyManager, CanAddMoneyDoesNotOverflowOnALargeAmount)
{
	MMoneyManager wallet;			// limit two billion
	CHECK(wallet.SetMoney(1500000000));
	CHECK(!wallet.CanAddMoney(2000000000));	// the sum would wrap past INT_MAX
	CHECK(wallet.CanAddMoney(500000000));
}

TEST(MoneyManager, CanUseMoneyAnswersForTheBalanceItWouldLeave)
{
	MMoneyManager wallet;
	CHECK(wallet.SetMoney(50));
	CHECK(wallet.CanUseMoney(50));
	CHECK(!wallet.CanUseMoney(51));
	CHECK(wallet.CanUseMoney(0));
}

//----------------------------------------------------------------------
// The storage hint: once per wallet, on the first balance past
// 100,000, only where a hook is installed. A copy keeps the "already
// hinted" state and drops the hook.
//----------------------------------------------------------------------
TEST(MoneyManager, StorageHintFiresOncePastTheThresholdThroughTheHook)
{
	g_HintCount = 0;
	MMoneyManager wallet;
	wallet.SetStorageHintHook(CountHint);

	CHECK(wallet.SetMoney(100000));
	CHECK_EQ(0, g_HintCount);		// not past the threshold yet
	CHECK(wallet.SetMoney(100001));
	CHECK_EQ(1, g_HintCount);
	CHECK(wallet.SetMoney(0));
	CHECK(wallet.SetMoney(500000));
	CHECK_EQ(1, g_HintCount);		// once per wallet
}

// The two halves of the copy rule, each on a wallet where only that
// half can suppress the hint.
TEST(MoneyManager, ACopyDropsTheHookButKeepsTheHintState)
{
	g_HintCount = 0;

	// An un-hinted, hooked wallet: its copy has the state to fire but
	// no hook to fire through.
	MMoneyManager fresh;
	fresh.SetStorageHintHook(CountHint);
	MMoneyManager copyOfFresh(fresh);
	CHECK(copyOfFresh.SetMoney(900000));
	CHECK_EQ(0, g_HintCount);

	// A hinted wallet: its copy gets a hook of its own and still stays
	// quiet, because the "already hinted" state came with it.
	CHECK(fresh.SetMoney(200000));
	CHECK_EQ(1, g_HintCount);
	MMoneyManager copyOfHinted(fresh);
	copyOfHinted.SetStorageHintHook(CountHint);
	CHECK(copyOfHinted.SetMoney(0));
	CHECK(copyOfHinted.SetMoney(900000));
	CHECK_EQ(1, g_HintCount);
}

// Assignment follows the same rule as copying, except that the wallet
// being assigned to keeps the hook it already had.
TEST(MoneyManager, AssignmentCopiesTheBalanceAndStateButKeepsTheTargetsHook)
{
	g_HintCount = 0;
	MMoneyManager source;
	source.SetMoneyLimit(5000);
	CHECK(source.SetMoney(4000));

	MMoneyManager target;
	target.SetStorageHintHook(CountHint);
	target = source;
	CHECK_EQ(4000, target.GetMoney());
	CHECK_EQ(5000, target.GetMoneyLimit());
	CHECK(!target.SetMoney(5001));

	// The source never hinted, so the target's own hook fires once.
	target.SetMoneyLimit(2000000000);
	CHECK(target.SetMoney(300000));
	CHECK_EQ(1, g_HintCount);
}

TEST(MoneyManager, NoHookMeansNoHintAndNoCrash)
{
	g_HintCount = 0;
	MMoneyManager wallet;			// a trade's temporary wallet
	CHECK(wallet.SetMoney(1000000));
	CHECK_EQ(0, g_HintCount);
}

//----------------------------------------------------------------------
// The donation dialog takes its amount in units of 10,000, and the
// dialog's own cap (balance / 10,000) does not always run before the
// amount is sent. DonationGold gives the gold for a CGDonationMoney:
// only a positive amount the balance covers, checked before any
// multiplication. The old inline left*10000 wrapped in the 32-bit
// Gold_t above 429,496 units and sent a smaller amount the server
// accepted.
//----------------------------------------------------------------------
TEST(MoneyManager, DonationGoldSendsWhatTheBalanceCovers)
{
	std::uint32_t gold = 0;
	CHECK(MMoneyManager::DonationGold(200000, 2000000000, gold));
	CHECK_EQ(2000000000, gold);

	gold = 0;
	CHECK(MMoneyManager::DonationGold(1, 10000, gold));
	CHECK_EQ(10000, gold);
}

TEST(MoneyManager, DonationGoldRefusesMoreThanTheBalance)
{
	std::uint32_t gold = 7;
	CHECK(!MMoneyManager::DonationGold(200001, 2000000000, gold));
	CHECK(!MMoneyManager::DonationGold(1, 9999, gold));
	CHECK(!MMoneyManager::DonationGold(1, 0, gold));
	CHECK_EQ(7, gold);			// a refusal leaves the output alone
}

TEST(MoneyManager, DonationGoldRefusesAnAmountThatWouldWrap)
{
	std::uint32_t gold = 7;
	// 500,000 units is 5,000,000,000 gold, which wrapped to 705,032,704.
	CHECK(!MMoneyManager::DonationGold(500000, 2000000000, gold));
	// The dialog's largest value.
	CHECK(!MMoneyManager::DonationGold(2147483647, 2000000000, gold));
	CHECK_EQ(7, gold);
}

TEST(MoneyManager, DonationGoldRefusesNothingOrLess)
{
	std::uint32_t gold = 7;
	CHECK(!MMoneyManager::DonationGold(0, 2000000000, gold));
	CHECK(!MMoneyManager::DonationGold(-1, 2000000000, gold));
	CHECK_EQ(7, gold);
}
