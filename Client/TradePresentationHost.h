#pragma once

#include <cstdint>

class GCGoodsList;

namespace TradePresentation {

// Trade, crafting and item-mall replies only present what they receive. A
// missing service skips that presentation, and a missing exchange query
// answers that no exchange window is open; a packet is borrowed for the call.
struct Host
{
	bool (*IsRunningExchange)() = nullptr;
	void (*CloseExchange)() = nullptr;
	void (*CloseExchangeAsk)() = nullptr;
	void (*CloseExchangeCancel)() = nullptr;
	void (*UnlockItem)() = nullptr;
	void (*RunItemShop)(GCGoodsList* pPacket) = nullptr;
	// Opens the exchange window with the creature of the ID, or asks the
	// player whether to trade with it.
	void (*RunExchange)(std::uint32_t otherID) = nullptr;
	void (*RunExchangeAsk)(std::uint32_t otherID) = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
bool IsRunningExchange();
void CloseExchange();
void CloseExchangeAsk();
void CloseExchangeCancel();
void UnlockItem();
void RunItemShop(GCGoodsList* pPacket);
void RunExchange(std::uint32_t otherID);
void RunExchangeAsk(std::uint32_t otherID);

}
