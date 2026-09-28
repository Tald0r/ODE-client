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
bool
MMoneyManager::AddMoney(int money)
{
	return SetMoney( m_Money + money );
}

//-----------------------------------------------------------------------------
// Remove Money
//-----------------------------------------------------------------------------
bool
MMoneyManager::UseMoney(int money)
{
	return SetMoney( m_Money - money );
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
	int left = m_Money - money;

	if (left<0)
	{
		return false; 
	}

	return true;
}

//-----------------------------------------------------------------------------
// Donation Gold
//-----------------------------------------------------------------------------
bool
MMoneyManager::DonationGold(std::int64_t units, int balance, std::uint32_t& outGold)
{
	(void)balance;

	if (units <= 0)
	{
		return false;
	}

	outGold = static_cast<std::uint32_t>(units * 10000);
	return true;
}
