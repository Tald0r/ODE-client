#include "Client_PCH.h"
#include "DialogPresentationHost.h"

namespace DialogPresentation {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

void PopupFreeMessageDlg(const char* message)
{
	if (s_Host && s_Host->PopupFreeMessageDlg) s_Host->PopupFreeMessageDlg(message);
}

void UnlockItem()
{
	if (s_Host && s_Host->UnlockItem) s_Host->UnlockItem();
}

void UnlockItemTrade()
{
	if (s_Host && s_Host->UnlockItemTrade) s_Host->UnlockItemTrade();
}

void RemoveItemFromItemShop(std::uint32_t id)
{
	if (s_Host && s_Host->RemoveItemFromItemShop) s_Host->RemoveItemFromItemShop(id);
}

bool IsRunningSwapAdvancementItem()
{
	return s_Host && s_Host->IsRunningSwapAdvancementItem && s_Host->IsRunningSwapAdvancementItem();
}

void RunPetStorage()
{
	if (s_Host && s_Host->RunPetStorage) s_Host->RunPetStorage();
}

void SetPetStorage(MStorage* pStorage)
{
	if (s_Host && s_Host->SetPetStorage) s_Host->SetPetStorage(pStorage);
}

void EmptyMove()
{
	if (s_Host && s_Host->EmptyMove) s_Host->EmptyMove();
}

}
