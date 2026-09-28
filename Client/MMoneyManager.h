//-----------------------------------------------------------------------------
// MMoneyManager.h
//-----------------------------------------------------------------------------
// Money는 MoneyLimit를 넘으면 안된다.
//-----------------------------------------------------------------------------

#ifndef __MMONEYMANAGER_H__
#define __MMONEYMANAGER_H__

#include <cstdint>

class MMoneyManager {

	public :
		MMoneyManager();
		MMoneyManager(const MMoneyManager& mm);
		MMoneyManager& operator=(const MMoneyManager& mm);
		~MMoneyManager();

		//-------------------------------------------------------
		// Money
		//-------------------------------------------------------		
		bool		SetMoney(int money);
		int			GetMoney() const		{ return m_Money; }

		bool		AddMoney(int money);
		bool		UseMoney(int money);

		// money만큼 돈을 add/use할 수 있나?
		bool		CanAddMoney(int money);
		bool		CanUseMoney(int money);

		//-------------------------------------------------------		
		// Money Limit
		//-------------------------------------------------------		
		void		SetMoneyLimit(int limit)	{ m_MoneyLimit = limit; }
		int			GetMoneyLimit() const		{ return m_MoneyLimit;}
		int			GetMaxAddMoney() const		{ return m_MoneyLimit - m_Money; }

		//-------------------------------------------------------
		// Donation
		//
		// The donation dialog takes the amount in units of 10,000.
		// Gives the gold to send for that many units, or false
		// when nothing is to be sent.
		//-------------------------------------------------------
		static bool	DonationGold(std::int64_t units, int balance, std::uint32_t& outGold);

		//-------------------------------------------------------
		// Storage hint (docs/RESTRUCTURING.md task 4.2)
		//
		// The help system wants to know the first time a wallet
		// passes 100,000, to suggest buying a storage box. The
		// manager only reports it, through a hook the executable
		// installs on the player's own wallet at start-up; nothing
		// UI-side is reachable from here, and the temporary wallets
		// a trade or the storage box builds carry no hook and give
		// no hint.
		//-------------------------------------------------------
		typedef void (*StorageHintHook)();
		void		SetStorageHintHook(StorageHintHook hook)	{ m_StorageHintHook = hook; }

	protected :
		int			m_MoneyLimit;	// the most this wallet may hold
		int			m_Money;		// the balance

		bool			m_bStorageHintGiven;	// the hint fires once per wallet
		StorageHintHook	m_StorageHintHook;
};

extern MMoneyManager*		g_pMoneyManager;

#endif

