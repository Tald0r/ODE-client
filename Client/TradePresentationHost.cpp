#include "Client_PCH.h"
#include "TradePresentationHost.h"

namespace TradePresentation {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

bool IsRunningExchange()
{
	return s_Host && s_Host->IsRunningExchange && s_Host->IsRunningExchange();
}

void CloseExchange()
{
	if (s_Host && s_Host->CloseExchange) s_Host->CloseExchange();
}

void CloseExchangeAsk()
{
	if (s_Host && s_Host->CloseExchangeAsk) s_Host->CloseExchangeAsk();
}

void CloseExchangeCancel()
{
	if (s_Host && s_Host->CloseExchangeCancel) s_Host->CloseExchangeCancel();
}

void UnlockItem()
{
	if (s_Host && s_Host->UnlockItem) s_Host->UnlockItem();
}

void RunItemShop(GCGoodsList* pPacket)
{
	if (s_Host && s_Host->RunItemShop) s_Host->RunItemShop(pPacket);
}

void RunExchange(std::uint32_t otherID)
{
	if (s_Host && s_Host->RunExchange) s_Host->RunExchange(otherID);
}

void RunExchangeAsk(std::uint32_t otherID)
{
	if (s_Host && s_Host->RunExchangeAsk) s_Host->RunExchangeAsk(otherID);
}

}
