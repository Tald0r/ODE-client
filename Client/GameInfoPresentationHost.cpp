#include "Client_PCH.h"
#include "GameInfoPresentationHost.h"

namespace GameInfoPresentation {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

void ClearNPC()
{
	if (s_Host && s_Host->ClearNPC) s_Host->ClearNPC();
}

void SetNPCInfo(const char* pName, int npcID, int x, int y)
{
	if (s_Host && s_Host->SetNPCInfo) s_Host->SetNPCInfo(pName, npcID, x, y);
}

void RunSkillTree(int domain, int maxLevel)
{
	if (s_Host && s_Host->RunSkillTree) s_Host->RunSkillTree(domain, maxLevel);
}

void MiniGameScores(GCMiniGameScores* pPacket)
{
	if (s_Host && s_Host->MiniGameScores) s_Host->MiniGameScores(pPacket);
}

void RunImageNotice(const char* name, std::uint32_t id)
{
	if (s_Host && s_Host->RunImageNotice) s_Host->RunImageNotice(name, id);
}

int GetCharInfoLevel()
{
	return s_Host && s_Host->GetCharInfoLevel ? s_Host->GetCharInfoLevel() : 0;
}

void SetDelayEternity()
{
	if (s_Host && s_Host->SetDelayEternity) s_Host->SetDelayEternity();
}

}
