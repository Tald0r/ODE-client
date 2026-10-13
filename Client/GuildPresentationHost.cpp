#include "Client_PCH.h"
#include "GuildPresentationHost.h"

namespace GuildPresentation {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

void AddChatToHistory(const char* message, const char* sender, int chatType, std::uint32_t color)
{
	if (s_Host && s_Host->AddChatToHistory) s_Host->AddChatToHistory(message, sender, chatType, color);
}

void PopupMessage(int gameStringID)
{
	if (s_Host && s_Host->PopupMessage) s_Host->PopupMessage(gameStringID);
}

void SetGuildName(int guildID, const std::string& guildName)
{
	if (s_Host && s_Host->SetGuildName) s_Host->SetGuildName(guildID, guildName);
}

void ShowGuildJoin(std::uint32_t joinFee, int rank, const char* date, const char* guildName, int guildID)
{
	if (s_Host && s_Host->ShowGuildJoin) s_Host->ShowGuildJoin(joinFee, rank, date, guildName, guildID);
}

void ShowGuildInfo(GCShowGuildInfo* pPacket)
{
	if (s_Host && s_Host->ShowGuildInfo) s_Host->ShowGuildInfo(pPacket);
}

void ShowGuildMemberInfo(GCShowGuildMemberInfo* pPacket)
{
	if (s_Host && s_Host->ShowGuildMemberInfo) s_Host->ShowGuildMemberInfo(pPacket);
}

void ShowGuildMemberList(GCGuildMemberList* pPacket)
{
	if (s_Host && s_Host->ShowGuildMemberList) s_Host->ShowGuildMemberList(pPacket);
}

void ShowUnionGuildMemberInfo(GCShowUnionInfo* pPacket)
{
	if (s_Host && s_Host->ShowUnionGuildMemberInfo) s_Host->ShowUnionGuildMemberInfo(pPacket);
}

void ShowWaitGuildInfo(GCShowWaitGuildInfo* pPacket)
{
	if (s_Host && s_Host->ShowWaitGuildInfo) s_Host->ShowWaitGuildInfo(pPacket);
}

}
