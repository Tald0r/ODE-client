#include "test_framework.h"
#include "gamemodel_world.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"
#include "PlayerStateHost.h"
#include "TalkBox.h"
#include "TempInformation.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCBloodBibleList.h"
#include <string>
#include <vector>

// The list of blood bibles the player may rent ends the player's wait and
// asks as an NPC would: the PC talk box offers each bible by its type,
// name and bonus, and a way out (999), under the rental subject; it is
// shown, and the next answer is a skill-learning one.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
bool zoneExists = true;
int me;

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? reinterpret_cast<MCreature*>(&me) : nullptr; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.PopupPCTalkDlg = []() { calls.push_back("talk box shown"); };
	return host;
}();

std::string Text(int id) { return (*g_pGameStringTable)[id].GetString(); }

struct World : GameModelWorld
{
	PCTalkBox talkBox;
	TempInformation temp;
	PCTalkBox* oldTalkBox = g_pPCTalkBox;
	TempInformation* oldTemp = g_pTempInformation;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	World()
	{
		InitGameStringTable();
		calls.clear();
		hasPlayer = true;
		zoneExists = true;
		g_pPCTalkBox = &talkBox;
		g_pTempInformation = &temp;
		talkBox.AddString("stale");
	}
	~World()
	{
		DialogPresentation::SetHost(oldDialog);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		g_pTempInformation = oldTemp;
		g_pPCTalkBox = oldTalkBox;
	}
	std::vector<std::string> Lines() const
	{
		std::vector<std::string> lines;
		for (const MString* pString : talkBox) lines.push_back(pString->GetString());
		return lines;
	}
};

void Receive(std::initializer_list<ItemType_t> bibles)
{
	GCBloodBibleList packet;
	packet.getList() = bibles;
	GCBloodBibleListHandler::execute(&packet, nullptr);
}

std::string Offer(const char* number, int type)
{
	return std::string(number) + " Borrow the " + Text(UI_STRING_MESSAGE_BLOOD_BIBLE_ARMEGA + type) + " seal. ("
		+ Text(STRING_MESSAGE_BLOOD_BIBLE_BONUS_ARMEGA + type) + ")";
}

} // namespace

TEST(BloodBibleReplies, TheListOffersEachBibleAndAWayOut)
{
	World world;
	Receive({0, 3});
	CHECK(calls == std::vector<std::string>({"verified", "talk box shown"}));
	CHECK(world.talkBox.GetType() == PCTalkBox::BLOOD_BIBLE_SIGN);
	CHECK(std::string(world.talkBox.GetContent()) == Text(UI_STRING_MESSAGE_RENT_BLOOD_BIBLE));
	CHECK(Offer("  0", 0) == "  0 Borrow the Armega seal. (MP(HP) cost of skills -50%)");
	CHECK(world.Lines() == std::vector<std::string>({Offer("  0", 0), Offer("  3", 3),
		"999" + Text(UI_STRING_MESSAGE_RENT_LATER_BLOOD_BIBLE)}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_SKILL_LEARN);
}

TEST(BloodBibleReplies, ABibleWithoutAStringIsOfferedWithEmptyText)
{
	World world;
	Receive({5000});
	CHECK(world.Lines() == std::vector<std::string>({"5000 Borrow the  seal. ()", "999" + Text(UI_STRING_MESSAGE_RENT_LATER_BLOOD_BIBLE)}));
	// None at all: only the way out.
	world.talkBox.Release();
	world.talkBox.AddString("stale");
	Receive({});
	CHECK(world.Lines() == std::vector<std::string>{"999" + Text(UI_STRING_MESSAGE_RENT_LATER_BLOOD_BIBLE)});
}

TEST(BloodBibleReplies, WithoutAPlayerZoneOrTalkBoxNothingHappens)
{
	World world;
	hasPlayer = false;
	Receive({0});
	hasPlayer = true;
	zoneExists = false;
	Receive({0});
	zoneExists = true;
	g_pPCTalkBox = nullptr;
	Receive({0});
	g_pPCTalkBox = &world.talkBox;
	CHECK(calls.empty());
	CHECK(world.Lines() == std::vector<std::string>{"stale"});
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	// Without a dialog host the box is filled but not shown.
	DialogPresentation::SetHost(nullptr);
	Receive({0});
	CHECK(calls == std::vector<std::string>{"verified"});
	CHECK_EQ(2, static_cast<int>(world.Lines().size()));
}
