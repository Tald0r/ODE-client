//-----------------------------------------------------------------------------
// MMoneyManager.cpp
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "MMoneyManager.h"
//-----------------------------------------------------------------------------
// Global
//-----------------------------------------------------------------------------
MMoneyManager*		g_pMoneyManager = NULL;

//-----------------------------------------------------------------------------
//
// constructor / destructor
//
//-----------------------------------------------------------------------------
MMoneyManager::MMoneyManager()
{
	m_MoneyLimit	= 2000000000;		// two billion
	m_Money			= 0;
	m_bStorageHintGiven	= false;
	m_StorageHintHook	= NULL;
}

// A copy keeps the balance, the limit and the hint state, not the hook:
// a temporary made from the player's wallet must not give hints.
MMoneyManager::MMoneyManager(const MMoneyManager& mm)
{
	m_MoneyLimit = mm.m_MoneyLimit;
	m_Money = mm.m_Money;
	m_bStorageHintGiven = mm.m_bStorageHintGiven;
	m_StorageHintHook = NULL;
}

// Assignment: the balance, the limit and the hint state come across;
// the wallet assigned to keeps its own hook (the compiler's default
// would have copied the source's, unlike the copy constructor).
MMoneyManager&
MMoneyManager::operator=(const MMoneyManager& mm)
{
	m_MoneyLimit = mm.m_MoneyLimit;
	m_Money = mm.m_Money;
	m_bStorageHintGiven = mm.m_bStorageHintGiven;
	return *this;
}

MMoneyManager::~MMoneyManager()
{
}

//-----------------------------------------------------------------------------
//
// member functions
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Set Money
//-----------------------------------------------------------------------------
bool
MMoneyManager::SetMoney(int money)
{
	// 0이하거나 한계를 넘어가면 안된다.
	if (money<0 || money > m_MoneyLimit)
	{
		return false; 
	}

	m_Money = money;

	// Past 100,000 for the first time: suggest a storage box (once).
	if (!m_bStorageHintGiven && m_Money > 100000)
	{
		m_bStorageHintGiven = true;
		if (m_StorageHintHook != NULL)
			m_StorageHintHook();
	}
	return true;
}

//-----------------------------------------------------------------------------
// Add Money
//-----------------------------------------------------------------------------
// The new balance is taken in 64 bits: two amounts within the limit can
// pass INT_MAX, and an int that overflows is undefined, not a refusal. A
// balance outside 0..limit is refused as SetMoney refuses it.
//-----------------------------------------------------------------------------
bool
MMoneyManager::AddMoney(int money)
{
	const std::int64_t balance = (std::int64_t)m_Money + money;
	if (balance < 0 || balance > m_MoneyLimit)
	{
		return false;
	}
	return SetMoney( (int)balance );
}

//-----------------------------------------------------------------------------
// Remove Money
//-----------------------------------------------------------------------------
bool
MMoneyManager::UseMoney(int money)
{
	const std::int64_t balance = (std::int64_t)m_Money - money;
	if (balance < 0 || balance > m_MoneyLimit)
	{
		return false;
	}
	return SetMoney( (int)balance );
}

//-----------------------------------------------------------------------------
// Can Add Money
//-----------------------------------------------------------------------------
bool
MMoneyManager::CanAddMoney(int money)
{
	// The question is whether the BALANCE stays within the limit, not
	// whether the amount alone does; this used to compare the amount,
	// so a wallet near the limit said yes and the AddMoney that
	// followed said no (docs/RESTRUCTURING.md task 4.2). Written as a
	// subtraction so a large amount cannot overflow the sum.
	if (money < 0)
	{
		return false;
	}
	return money <= m_MoneyLimit - m_Money;
}

//-----------------------------------------------------------------------------
// Can Use Money
//-----------------------------------------------------------------------------
bool		
MMoneyManager::CanUseMoney(int money)
{
	// As CanAddMoney: a negative amount is an add, not a use, and is
	// refused; the rest is compared, not subtracted, so no amount can
	// overflow the difference.
	if (money < 0)
	{
		return false;
	}
	return money <= m_Money;
}

//-----------------------------------------------------------------------------
// Donation Gold
//-----------------------------------------------------------------------------
bool
MMoneyManager::DonationGold(std::int64_t units, int balance, std::uint32_t& outGold)
{
	// Only a positive amount the balance covers. The bound is checked
	// by division, so an amount above it is never multiplied and
	// cannot wrap the 32-bit gold of the packet.
	if (units <= 0 || balance <= 0 || units > balance / 10000)
	{
		return false;
	}

	outGold = static_cast<std::uint32_t>(units * 10000);
	return true;
}
