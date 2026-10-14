#pragma once

#include <cstdint>

namespace NPCPresentation {

// The windows an NPC's answer opens (GCNPCResponse): the shop's repair and
// silvering, the tutorial's, the guilds' and castles', the couples', the
// quests', the lottery and the events'. A missing service skips it.
struct Host
{
	// The inventory opened for repairs, the shop's way to pick what to mend.
	void (*OpenInventoryToRepair)() = nullptr;
	// The helicopter's waypoint choice.
	void (*RunSelectWayPoint)() = nullptr;
	// The inventory opened for silver coating.
	void (*OpenInventoryToSilvering)() = nullptr;
	// The tutorial's exit window (the newbie items).
	void (*RunTutorialExit)() = nullptr;
	// The tutorial's computer, briefing and bookcase windows.
	void (*RunComputer)() = nullptr;
	void (*RunBriefing)() = nullptr;
	void (*RunBookcase)() = nullptr;
	// The character window's silver damage.
	void (*SetCharInfoSilverDamage)(int minDamage, int maxDamage) = nullptr;
	// The guild registration window, at its fee.
	void (*ShowGuildRegist)(std::uint32_t fee) = nullptr;
	// The castle's tax window with its balance; SetTotalFee updates it.
	void (*OpenBringFee)(std::uint32_t totalFee) = nullptr;
	void (*SetTotalFee)(std::uint32_t fee) = nullptr;
	// The couple's name input (0 meet, 1 part, 2 part by force); CloseInputNameWindow closes it.
	void (*RunInputNameWindow)(int id) = nullptr;
	void (*CloseInputNameWindow)() = nullptr;
	// Clears the quest status window.
	void (*InitQuestStatus)() = nullptr;
	// The lottery window at its step (1-5).
	void (*RunLottery)(int step) = nullptr;
	// The skill downgrade window.
	void (*DownSkill)() = nullptr;
	// The castle's tax rate window.
	void (*RunModifyTax)() = nullptr;
	// The advancement item swap window.
	void (*ShowSwapAdvancementItem)() = nullptr;
	// The donation campaign's window.
	void (*RunCampaignHelp)(int value) = nullptr;
	// The event item's confirmation.
	void (*RunConfirmGetEventItem)(int value) = nullptr;
	// The December 2004 comeback event's welcome popup (0 the notice, 1 the paid one, 2 the recommendation).
	void (*PopupComebackNotice)(int which) = nullptr;
	// A popup of the game string.
	void (*PopupMessage)(int gameStringID) = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
void OpenInventoryToRepair();
void RunSelectWayPoint();
void OpenInventoryToSilvering();
void RunTutorialExit();
void RunComputer();
void RunBriefing();
void RunBookcase();
void SetCharInfoSilverDamage(int minDamage, int maxDamage);
void ShowGuildRegist(std::uint32_t fee);
void OpenBringFee(std::uint32_t totalFee);
void SetTotalFee(std::uint32_t fee);
void RunInputNameWindow(int id);
void CloseInputNameWindow();
void InitQuestStatus();
void RunLottery(int step);
void DownSkill();
void RunModifyTax();
void ShowSwapAdvancementItem();
void RunCampaignHelp(int value);
void RunConfirmGetEventItem(int value);
void PopupComebackNotice(int which);
void PopupMessage(int gameStringID);

}
