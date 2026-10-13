#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "VsUiPresentationHost.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCExecuteElement.h"
#include "Gpackets/GCGQuestInventory.h"
#include <cstdint>
#include <string>
#include <vector>

// In a zone, a quest element is handed to the quest window to run, and the
// quest inventory becomes quest items (of the listed types, durability 1)
// that the quest window keeps. Without a zone both do nothing; without a
// quest window to take them the items are freed.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
int alive = 0;
std::vector<MItem*> kept;

std::string N(long long value) { return std::to_string(value); }

struct QuestItem : MItem
{
	QuestItem() { alive++; }
	~QuestItem() { alive--; }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_GQUEST_ITEM; }
};

MItemHost FactoryHost()
{
	MItemHost host{};
	host.NewItem = [](ITEM_CLASS itemClass) -> MItem* { return itemClass == ITEM_CLASS_GQUEST_ITEM ? new QuestItem : nullptr; };
	return host;
}
const MItemHost items = FactoryHost();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	return host;
}();

const VsUiPresentation::Host ui = []() {
	VsUiPresentation::Host host{};
	host.PushGQuestExecuteElement = [](std::uint32_t questID, std::uint8_t condition, std::uint16_t index) {
		calls.push_back("element|" + N(questID) + "|" + N(condition) + "|" + N(index));
	};
	host.UpdateQuestItemInfo = [](std::vector<MItem*>& items) {
		std::string call = "items";
		for (MItem* item : items)
		{
			call += "|" + N(item->GetItemType()) + "/" + N(item->GetCurrentDurability());
			kept.push_back(item);
		}
		calls.push_back(call);
	};
	return host;
}();

struct World : GameModelWorld
{
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const VsUiPresentation::Host* oldUi = VsUiPresentation::SetHost(&ui);
	World()
	{
		MItem::SetHost(&items);
		calls.clear();
		zoneExists = true;
		alive = 0;
	}
	~World()
	{
		// The quest window owns what it was handed.
		for (MItem* item : kept) delete item;
		kept.clear();
		MItem::SetHost(nullptr);
		VsUiPresentation::SetHost(oldUi);
		ZoneState::SetHost(oldZone);
	}
};

template<class PacketType>
void Read(PacketType& packet, const std::vector<unsigned char>& bytes)
{
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

// GCExecuteElement has no quest ID setter, so its bytes are written here:
// the quest ID, the condition and the index.
void ReceiveElement(DWORD questID, BYTE condition, WORD index)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(questID);
	encoded.write(condition);
	encoded.write(index);
	GCExecuteElement packet;
	Read(packet, SocketOutputStreamTestAccess::Bytes(encoded));
	GCExecuteElementHandler::execute(&packet, nullptr);
}

void ReceiveInventory(const std::vector<ItemType_t>& types)
{
	GCGQuestInventory original;
	original.getItemList() = types;
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	GCGQuestInventory packet;
	Read(packet, SocketOutputStreamTestAccess::Bytes(encoded));
	GCGQuestInventoryHandler::execute(&packet, nullptr);
}

} // namespace

TEST(QuestElementInventoryHandlers, AQuestElementIsHandedToTheQuestWindowInAZone)
{
	World world;
	ReceiveElement(70001, 2, 513);
	zoneExists = false;
	ReceiveElement(70002, 1, 1);
	CHECK(calls == std::vector<std::string>{"element|70001|2|513"});
}

TEST(QuestElementInventoryHandlers, TheQuestInventoryBecomesQuestItemsTheWindowKeeps)
{
	World world;
	ReceiveInventory({4, 9, 4});
	CHECK(calls == std::vector<std::string>{"items|4/1|9/1|4/1"});
	CHECK_EQ(3, alive);
	// An empty inventory still empties the window.
	ReceiveInventory({});
	CHECK(calls.back() == "items");
}

TEST(QuestElementInventoryHandlers, WithoutAZoneOrAWindowNothingIsKept)
{
	World world;
	zoneExists = false;
	ReceiveInventory({4});
	CHECK(calls.empty());
	CHECK_EQ(0, alive);
	zoneExists = true;
	VsUiPresentation::SetHost(nullptr);
	ReceiveInventory({4, 9});
	const VsUiPresentation::Host empty{};
	VsUiPresentation::SetHost(&empty);
	ReceiveInventory({4});
	ReceiveElement(70001, 2, 513);
	CHECK(calls.empty());
	// Nobody took the items, so the reply freed them.
	CHECK_EQ(0, alive);
	// Without a factory nothing is made.
	VsUiPresentation::SetHost(&ui);
	MItem::SetHost(nullptr);
	ReceiveInventory({4});
	CHECK(calls == std::vector<std::string>{"items"});
}
