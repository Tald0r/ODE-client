#include "Client_PCH.h"
#include "NPCPresentationHost.h"

namespace NPCPresentation {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

void OpenInventoryToRepair()
{
	if (s_Host && s_Host->OpenInventoryToRepair) s_Host->OpenInventoryToRepair();
}

void RunSelectWayPoint()
{
	if (s_Host && s_Host->RunSelectWayPoint) s_Host->RunSelectWayPoint();
}

void OpenInventoryToSilvering()
{
	if (s_Host && s_Host->OpenInventoryToSilvering) s_Host->OpenInventoryToSilvering();
}

void RunTutorialExit()
{
	if (s_Host && s_Host->RunTutorialExit) s_Host->RunTutorialExit();
}

void RunComputer()
{
	if (s_Host && s_Host->RunComputer) s_Host->RunComputer();
}

void RunBriefing()
{
	if (s_Host && s_Host->RunBriefing) s_Host->RunBriefing();
}

void RunBookcase()
{
	if (s_Host && s_Host->RunBookcase) s_Host->RunBookcase();
}

void SetCharInfoSilverDamage(int minDamage, int maxDamage)
{
	if (s_Host && s_Host->SetCharInfoSilverDamage) s_Host->SetCharInfoSilverDamage(minDamage, maxDamage);
}

void ShowGuildRegist(std::uint32_t fee)
{
	if (s_Host && s_Host->ShowGuildRegist) s_Host->ShowGuildRegist(fee);
}

void OpenBringFee(std::uint32_t totalFee)
{
	if (s_Host && s_Host->OpenBringFee) s_Host->OpenBringFee(totalFee);
}

void SetTotalFee(std::uint32_t fee)
{
	if (s_Host && s_Host->SetTotalFee) s_Host->SetTotalFee(fee);
}

void RunInputNameWindow(int id)
{
	if (s_Host && s_Host->RunInputNameWindow) s_Host->RunInputNameWindow(id);
}

void CloseInputNameWindow()
{
	if (s_Host && s_Host->CloseInputNameWindow) s_Host->CloseInputNameWindow();
}

void InitQuestStatus()
{
	if (s_Host && s_Host->InitQuestStatus) s_Host->InitQuestStatus();
}

void RunLottery(int step)
{
	if (s_Host && s_Host->RunLottery) s_Host->RunLottery(step);
}

void DownSkill()
{
	if (s_Host && s_Host->DownSkill) s_Host->DownSkill();
}

void RunModifyTax()
{
	if (s_Host && s_Host->RunModifyTax) s_Host->RunModifyTax();
}

void ShowSwapAdvancementItem()
{
	if (s_Host && s_Host->ShowSwapAdvancementItem) s_Host->ShowSwapAdvancementItem();
}

void RunCampaignHelp(int value)
{
	if (s_Host && s_Host->RunCampaignHelp) s_Host->RunCampaignHelp(value);
}

void RunConfirmGetEventItem(int value)
{
	if (s_Host && s_Host->RunConfirmGetEventItem) s_Host->RunConfirmGetEventItem(value);
}

void PopupComebackNotice(int which)
{
	if (s_Host && s_Host->PopupComebackNotice) s_Host->PopupComebackNotice(which);
}

void PopupMessage(int gameStringID)
{
	if (s_Host && s_Host->PopupMessage) s_Host->PopupMessage(gameStringID);
}

}
