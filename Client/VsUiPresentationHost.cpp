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

}
