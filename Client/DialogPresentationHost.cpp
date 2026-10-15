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

void RunStorage()
{
	if (s_Host && s_Host->RunStorage) s_Host->RunStorage();
}

void SetStorage(MStorage* pStorage)
{
	if (s_Host && s_Host->SetStorage) s_Host->SetStorage(pStorage);
}

void PickUpItem(MItem* pItem)
{
	if (s_Host && s_Host->PickUpItem) s_Host->PickUpItem(pItem);
}

MItem* GetMouseItem()
{
	return s_Host && s_Host->GetMouseItem ? s_Host->GetMouseItem() : nullptr;
}

void DropItem()
{
	if (s_Host && s_Host->DropItem) s_Host->DropItem();
}

void UnlockGear()
{
	if (s_Host && s_Host->UnlockGear) s_Host->UnlockGear();
}

void CloseMixingForge()
{
	if (s_Host && s_Host->CloseMixingForge) s_Host->CloseMixingForge();
}

void PopupPCTalkDlg()
{
	if (s_Host && s_Host->PopupPCTalkDlg) s_Host->PopupPCTalkDlg();
}

void ShowPCTalkDlg()
{
	if (s_Host && s_Host->ShowPCTalkDlg) s_Host->ShowPCTalkDlg();
}

void RunStorageBuy(int price)
{
	if (s_Host && s_Host->RunStorageBuy) s_Host->RunStorageBuy(price);
}

void OpenInventoryToSell()
{
	if (s_Host && s_Host->OpenInventoryToSell) s_Host->OpenInventoryToSell();
}

void CloseRequestResurrectWindow()
{
	if (s_Host && s_Host->CloseRequestResurrectWindow) s_Host->CloseRequestResurrectWindow();
}

void SetShop(MShop* pShop)
{
	if (s_Host && s_Host->SetShop) s_Host->SetShop(pShop);
}

void RunShop()
{
	if (s_Host && s_Host->RunShop) s_Host->RunShop();
}

bool IsLockInput()
{
	return s_Host && s_Host->IsLockInput ? s_Host->IsLockInput() : false;
}

void OkMixingForge(std::uint32_t parameter, MItem* pItem, MItem* pItem2)
{
	if (s_Host && s_Host->OkMixingForge) s_Host->OkMixingForge(parameter, pItem, pItem2);
}

void OkRemoveOption(std::uint32_t parameter, MItem* pItem)
{
	if (s_Host && s_Host->OkRemoveOption) s_Host->OkRemoveOption(parameter, pItem);
}

void ClosePCTalkDlg()
{
	if (s_Host && s_Host->ClosePCTalkDlg) s_Host->ClosePCTalkDlg();
}

void UnSetLockInputPCTalk()
{
	if (s_Host && s_Host->UnSetLockInputPCTalk) s_Host->UnSetLockInputPCTalk();
}

bool IsRunningStorage()
{
	return s_Host && s_Host->IsRunningStorage && s_Host->IsRunningStorage();
}

bool IsRunningShop()
{
	return s_Host && s_Host->IsRunningShop && s_Host->IsRunningShop();
}

bool IsRunningBookcase()
{
	return s_Host && s_Host->IsRunningBookcase && s_Host->IsRunningBookcase();
}

bool IsRunningRepair()
{
	return s_Host && s_Host->IsRunningRepair && s_Host->IsRunningRepair();
}

bool IsRunningSell()
{
	return s_Host && s_Host->IsRunningSell && s_Host->IsRunningSell();
}

bool IsRunningParty()
{
	return s_Host && s_Host->IsRunningParty && s_Host->IsRunningParty();
}

void CloseAllDialog()
{
	if (s_Host && s_Host->CloseAllDialog) s_Host->CloseAllDialog();
}

}
