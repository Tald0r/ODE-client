#include "Client_PCH.h"
#include "VsUiPresentationHost.h"

namespace VsUiPresentation {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

int InventoryMode()
{
	return s_Host && s_Host->InventoryMode ? s_Host->InventoryMode() : 0;
}

void AddNickNameList(NicknameInfo* pInfo)
{
	if (s_Host && s_Host->AddNickNameList) s_Host->AddNickNameList(pInfo);
}

void SetNickNameList(std::vector<NicknameInfo*>* pList)
{
	if (s_Host && s_Host->SetNickNameList) s_Host->SetNickNameList(pList);
}

void AddMail(int tabID, std::uint32_t id, int width, int height, const char* sender, const char* title, std::uint32_t date, bool open, int guildID, int guildType)
{
	if (s_Host && s_Host->AddMail) s_Host->AddMail(tabID, id, width, height, sender, title, date, open, guildID, guildType);
}

void AddMailContents(int tabID, std::uint32_t id, const char* contents)
{
	if (s_Host && s_Host->AddMailContents) s_Host->AddMailContents(tabID, id, contents);
}

void PushGQuestExecuteElement(std::uint32_t questID, std::uint8_t condition, std::uint16_t index)
{
	if (s_Host && s_Host->PushGQuestExecuteElement) s_Host->PushGQuestExecuteElement(questID, condition, index);
}

bool UpdateQuestItemInfo(std::vector<MItem*>& items)
{
	if (!s_Host || !s_Host->UpdateQuestItemInfo) return false;
	s_Host->UpdateQuestItemInfo(items);
	return true;
}

bool ModifyQuestManagerInfo(UI_GQuestInfo* pInfo, int type)
{
	return s_Host && s_Host->ModifyQuestManagerInfo ? s_Host->ModifyQuestManagerInfo(pInfo, type) : false;
}

void ChangeCustomNaming(int id, const char* name)
{
	if (s_Host && s_Host->ChangeCustomNaming) s_Host->ChangeCustomNaming(id, name);
}

void CloseNamingChange()
{
	if (s_Host && s_Host->CloseNamingChange) s_Host->CloseNamingChange();
}

}
