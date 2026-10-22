//-----------------------------------------------------------------------------
// MTradeManager.h
//-----------------------------------------------------------------------------
// The exchange. Init pairs an inventory and a wallet for each side -
// mine (the player's own inventory) and the other's - and the UI works
// on them through the getters. Adding or removing an item, or changing
// the money, must clear both acceptances; a trade goes through only
// when both sides have accepted: check CanTrade(), then Trade().
//-----------------------------------------------------------------------------

#ifndef __MTRADEMANAGER_H__
#define __MTRADEMANAGER_H__

#include "MInventory.h"
#include "MMoneyManager.h"
#include "MonotonicClock.h"

class MTradeManager {
	public :
		MTradeManager();
		~MTradeManager();

		//-------------------------------------------------------
		// Init / Release
		//-------------------------------------------------------
		void				Init();
		void				Release();

		//-------------------------------------------------------
		// inventory 얻기
		//-------------------------------------------------------
		MInventory*			GetMyInventory() const			{ return m_pMyInventory; }
		MInventory*			GetOtherInventory() const		{ return m_pOtherInventory; }

		//-------------------------------------------------------
		// money manager 얻기
		//-------------------------------------------------------
		MMoneyManager*		GetMyMoneyManager() const		{ return m_pMyMoney; }
		MMoneyManager*		GetOtherMoneyManager() const	{ return m_pOtherMoney; }

		//-------------------------------------------------------
		// Trade OK ?
		//-------------------------------------------------------
		// The accept delay runs on the item host's frame stamp
		// (docs/reviews/RESTRUCTURING.md task 4.2); without one there is no delay.
		bool				IsAcceptTime() const;
		void				SetNextAcceptTime();		// when accepting is allowed again

		bool				IsAcceptMyTrade() const				{ return m_bAcceptMyTrade; }
		bool				IsAcceptOtherTrade() const			{ return m_bAcceptOtherTrade; }

		void				AcceptMyTrade()						{ m_bAcceptMyTrade = true; }
		void				AcceptOtherTrade()					{ m_bAcceptOtherTrade = true; }

		void				RefuseMyTrade();
		void				RefuseOtherTrade();

		//-------------------------------------------------------
		// Trade
		//-------------------------------------------------------
		bool				CanTrade() const;		// trade 가능한가?
		bool				Trade();				// 교환!
		bool				CancelTrade();			// 교환 거절

		//-------------------------------------------------------
		// 교환할려는 사람의 정보
		//-------------------------------------------------------
		void				SetOtherID(TYPE_OBJECTID otherID)	{ m_OtherID = otherID; }
		TYPE_OBJECTID		GetOtherID() const					{ return m_OtherID; }
		void				SetOtherName(const char* pName)		{ m_OtherName = pName; }
		const char*			GetOtherName() const				{ return m_OtherName.GetString(); }

	protected :
		TYPE_OBJECTID		m_OtherID;				// 다른 사람 ID
		MString				m_OtherName;			// 다른 사람 이름

		MInventory*			m_pMyInventory;			// 내꺼
		MInventory*			m_pOtherInventory;		// 남꺼

		MMoneyManager*		m_pMyMoney;				// 내 돈
		MMoneyManager*		m_pOtherMoney;			// 남 돈

		bool				m_bAcceptMyTrade;			// 나의 교환확인
		bool				m_bAcceptOtherTrade;		// 남의 교한확인

		MonotonicClock::TimePoint	m_NextAcceptTime;	// when OK may be pressed again
};

extern MTradeManager*		g_pTradeManager;

#endif

