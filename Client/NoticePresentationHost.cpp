#include "Client_PCH.h"
#include "NoticePresentationHost.h"

namespace NoticePresentation {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

void RunNotice(std::uint32_t kind, std::uint32_t parameter)
{
	if (s_Host && s_Host->RunNotice) s_Host->RunNotice(kind, parameter);
}

void DeleteNotice(std::uint32_t kind)
{
	if (s_Host && s_Host->DeleteNotice) s_Host->DeleteNotice(kind);
}

void RunWelcomeMessage()
{
	if (s_Host && s_Host->RunWelcomeMessage) s_Host->RunWelcomeMessage();
}

void LotteryResult(bool bSuccess)
{
	if (s_Host && s_Host->LotteryResult) s_Host->LotteryResult(bSuccess);
}

void RunHorn(int zoneID)
{
	if (s_Host && s_Host->RunHorn) s_Host->RunHorn(zoneID);
}

void MasterLairMessage(std::uint8_t type, short value, std::uint16_t zoneID)
{
	if (s_Host && s_Host->MasterLairMessage) s_Host->MasterLairMessage(type, value, zoneID);
}

void PopupMessage(int gameStringID)
{
	if (s_Host && s_Host->PopupMessage) s_Host->PopupMessage(gameStringID);
}

void PopupText(const char* text, bool bLarge)
{
	if (s_Host && s_Host->PopupText) s_Host->PopupText(text, bLarge);
}

void RunMinigame(int game)
{
	if (s_Host && s_Host->RunMinigame) s_Host->RunMinigame(game);
}

void CloseCTFStatusWindow()
{
	if (s_Host && s_Host->CloseCTFStatusWindow) s_Host->CloseCTFStatusWindow();
}

void SendCrashReport()
{
	if (s_Host && s_Host->SendCrashReport) s_Host->SendCrashReport();
}

void SetFlagTo(bool bOn)
{
	if (s_Host && s_Host->SetFlagTo) s_Host->SetFlagTo(bOn);
}

void StartQuestEnding()
{
	if (s_Host && s_Host->StartQuestEnding) s_Host->StartQuestEnding();
}

}
