#pragma once

#include <cstdint>
#include <string>

class GCActiveGuildList;
class GCGuildMemberList;
class GCShowGuildInfo;
class GCShowGuildMemberInfo;
class GCShowUnionInfo;
class GCShowWaitGuildInfo;
class GCWaitGuildList;

namespace GuildPresentation {

// Guild and union replies only present what they receive. A missing service
// skips that presentation; a packet is borrowed for the duration of the call.
struct Host
{
	void (*AddChatToHistory)(const char* message, const char* sender, int chatType, std::uint32_t color) = nullptr;
	void (*PopupMessage)(int gameStringID) = nullptr;
	void (*SetGuildName)(int guildID, const std::string& guildName) = nullptr;
	void (*ShowGuildJoin)(std::uint32_t joinFee, int rank, const char* date, const char* guildName, int guildID) = nullptr;
	void (*ShowGuildInfo)(GCShowGuildInfo* pPacket) = nullptr;
	void (*ShowGuildMemberInfo)(GCShowGuildMemberInfo* pPacket) = nullptr;
	void (*ShowGuildMemberList)(GCGuildMemberList* pPacket) = nullptr;
	void (*ShowUnionGuildMemberInfo)(GCShowUnionInfo* pPacket) = nullptr;
	void (*ShowWaitGuildInfo)(GCShowWaitGuildInfo* pPacket) = nullptr;
	void (*ShowActiveGuildList)(GCActiveGuildList* pPacket) = nullptr;
	void (*ShowWaitGuildList)(GCWaitGuildList* pPacket) = nullptr;
	void (*SetGuild)(std::uint16_t guildID, std::uint8_t rank, const std::string& guildName) = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
void AddChatToHistory(const char* message, const char* sender, int chatType, std::uint32_t color);
void PopupMessage(int gameStringID);
void SetGuildName(int guildID, const std::string& guildName);
void ShowGuildJoin(std::uint32_t joinFee, int rank, const char* date, const char* guildName, int guildID);
void ShowGuildInfo(GCShowGuildInfo* pPacket);
void ShowGuildMemberInfo(GCShowGuildMemberInfo* pPacket);
void ShowGuildMemberList(GCGuildMemberList* pPacket);
void ShowUnionGuildMemberInfo(GCShowUnionInfo* pPacket);
void ShowWaitGuildInfo(GCShowWaitGuildInfo* pPacket);
void ShowActiveGuildList(GCActiveGuildList* pPacket);
void ShowWaitGuildList(GCWaitGuildList* pPacket);
void SetGuild(std::uint16_t guildID, std::uint8_t rank, const std::string& guildName);

}
