#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "WorldEventsHost.h"
#include "PlayerPresentationHost.h"
#include "BloodBibleSignInfo.h"
#include "ClientPlayer.h"
#include "ConvertDuration.h"
#include "PetInfo.h"
#include "RequestServerPlayer.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCAddInstalledMineToZone.h"
#include "Gpackets/GCAddNewItemToZone.h"
#include "Gpackets/GCAddVampirePortal.h"
#include "Gpackets/GCBloodBibleSignInfo.h"
#include "Gpackets/GCChangeWeather.h"
#include "Gpackets/GCLightning.h"
#include "Gpackets/GCPetInfo.h"
#include "Rpackets/CRDisconnect.h"
#include <cstdint>
#include <string>
#include <vector>

// Replies the client hands on whole to its world and connections reach them
// through WorldEvents::Host: what they pass, converted as each one converts.
namespace {

std::vector<std::string> calls;
const void* lastPointer = nullptr;
// What the zone answers it added.
MItem* added = nullptr;

std::string N(long long value) { return std::to_string(value); }

const WorldEvents::Host host{
	.AddItemToZone = [](GCAddItemToZone* pPacket, bool bDropping) -> MItem* {
		lastPointer = pPacket;
		calls.push_back("additem|" + N(pPacket->getObjectID()) + "|" + N(bDropping));
		return added;
	},
	.AddVampirePortal = [](int serverID, const char* ownerName, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY,
		std::uint32_t delayFrame, int zoneID, TYPE_SECTORPOSITION zoneX, TYPE_SECTORPOSITION zoneY, bool bStartFromMainNode) {
		calls.push_back("portal|" + N(serverID) + "|" + (ownerName ? ownerName : "(null)") + "|" + N(sX) + "|" + N(sY) + "|"
			+ N(delayFrame) + "|" + N(zoneID) + "|" + N(zoneX) + "|" + N(zoneY) + "|" + N(bStartFromMainNode));
	},
	.SetBloodBibleSlot = [](BloodBibleSignInfo* pInfo) { lastPointer = pInfo; calls.push_back("bloodbible"); },
	.SetWeather = [](int weather, int level) { calls.push_back("weather|" + N(weather) + "|" + N(level)); },
	.SetLightning = [](std::uint32_t delay) { calls.push_back("lightning|" + N(delay)); },
	.SetPetInfo = [](PetInfo* pPetInfo, TYPE_OBJECTID objectID) { lastPointer = pPetInfo; calls.push_back("pet|" + N(objectID)); },
	.RequestDisconnect = [](const char* pName) { calls.push_back("disconnect|" + std::string(pName ? pName : "(null)")); },
};

struct Ware : MItem
{
	ITEM_CLASS m_Class;
	Ware(ITEM_CLASS itemClass, TYPE_OBJECTID id) : m_Class(itemClass) { SetID(id); }
	ITEM_CLASS GetItemClass() const override { return m_Class; }
};

MItemHost ItemHost()
{
	MItemHost items{};
	items.SetMineInstalled = [](MItem* pMine) { calls.push_back("installed|" + N(pMine->GetID())); };
	return items;
}
const MItemHost items = ItemHost();

// The frame conversion reads the client configuration GameModelWorld makes.
struct World : GameModelWorld
{
	const WorldEvents::Host* oldHost = WorldEvents::SetHost(&host);
	World() { calls.clear(); lastPointer = nullptr; added = nullptr; MItem::SetHost(&items); }
	~World() { MItem::SetHost(nullptr); WorldEvents::SetHost(oldHost); }
};

template<class PacketType>
void Decode(PacketType& packet, const PacketType& original)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

// A peer connection, closed when the test is done with it.
struct Peer : RequestServerPlayer
{
	Peer() : RequestServerPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Peer() { setPlayerStatus(CPS_END_SESSION); }
};

struct LoginConnection : ClientPlayer
{
	LoginConnection() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~LoginConnection() { setPlayerStatus(CPS_END_SESSION); }
};

// An owner name longer than std::string's inline buffer, so a pointer into
// a dead temporary would read freed heap that ASan reports.
const char* const kOwner = "Nyx of the Long Night Hall";

void ReceivePortal(BYTE createFlag)
{
	GCAddVampirePortal original;
	original.setObjectID(9001);
	original.setOwnerID(kOwner);
	original.setX(31);
	original.setY(32);
	original.setDuration(600);
	original.setTargetZoneID(1201);
	original.setTargetX(140);
	original.setTargetY(150);
	original.setCreateFlag(createFlag);
	GCAddVampirePortal packet;
	Decode(packet, original);
	GCAddVampirePortalHandler::execute(&packet, nullptr);
}

void ReceiveWeather(Weather weather, WeatherLevel_t level)
{
	GCChangeWeather original;
	original.setWeather(weather);
	original.setWeatherLevel(level);
	GCChangeWeather packet;
	Decode(packet, original);
	GCChangeWeatherHandler::execute(&packet, nullptr);
}

void ReceiveLightning(BYTE delay)
{
	GCLightning original;
	original.setDelay(delay);
	GCLightning packet;
	Decode(packet, original);
	GCLightningHandler::execute(&packet, nullptr);
}

// Every handler once: eight calls with a host.
void RunEveryHandler()
{
	Peer peer;
	peer.setName("Lantern");
	CRDisconnect disconnect;
	CRDisconnectHandler::execute(&disconnect, &peer);
	GCAddNewItemToZone item;
	item.setObjectID(77);
	GCAddNewItemToZoneHandler::execute(&item, nullptr);
	ReceivePortal(0);
	BloodBibleSignInfo sign;
	GCBloodBibleSignInfo bible;
	bible.setSignInfo(&sign);
	GCBloodBibleSignInfoHandler::execute(&bible, nullptr);
	ReceiveWeather(WEATHER_RAINY, 3);
	ReceiveLightning(2);
	PetInfo pet;
	GCPetInfo petInfo;
	petInfo.setObjectID(55);
	petInfo.setPetInfo(&pet);
	GCPetInfoHandler::execute(&petInfo, nullptr);
	GCAddInstalledMineToZone mine;
	mine.setObjectID(78);
	GCAddInstalledMineToZoneHandler::execute(&mine, nullptr);
}

} // namespace

TEST(WorldEventHandlers, APeerAsksToDisconnectByItsNameOnlyOnAPeerConnection)
{
	World world;
	Peer peer;
	peer.setName("Lantern");
	CRDisconnect packet;
	CRDisconnectHandler::execute(&packet, &peer);
	LoginConnection login;
	CRDisconnectHandler::execute(&packet, &login);
	CRDisconnectHandler::execute(&packet, nullptr);
	CHECK(calls == std::vector<std::string>{"disconnect|Lantern"});
}

TEST(WorldEventHandlers, ANewItemIsAddedToTheZoneWithoutADrop)
{
	World world;
	GCAddNewItemToZone packet;
	packet.setObjectID(77);
	GCAddNewItemToZoneHandler::execute(&packet, nullptr);
	CHECK(calls == std::vector<std::string>{"additem|77|0"});
	CHECK(lastPointer == static_cast<GCAddItemToZone*>(&packet));
}

TEST(WorldEventHandlers, AnInstalledMineIsAddedWithoutADropAndMarkedInstalled)
{
	World world;
	// Outlives the items, whose destructors read the installed item host.
	const MItemHost unmarking{};
	Ware mine(ITEM_CLASS_MINE, 78);
	Ware sword(ITEM_CLASS_SWORD, 79);
	GCAddInstalledMineToZone packet;
	packet.setObjectID(78);
	added = &mine;
	GCAddInstalledMineToZoneHandler::execute(&packet, nullptr);
	CHECK(calls == std::vector<std::string>({"additem|78|0", "installed|78"}));
	CHECK(lastPointer == static_cast<GCAddItemToZone*>(&packet));

	// Anything but a mine, or nothing added, is left as it is.
	calls.clear();
	packet.setObjectID(79);
	added = &sword;
	GCAddInstalledMineToZoneHandler::execute(&packet, nullptr);
	added = nullptr;
	GCAddInstalledMineToZoneHandler::execute(&packet, nullptr);
	CHECK(calls == std::vector<std::string>({"additem|79|0", "additem|79|0"}));

	// Without an item service, or with one that cannot mark a mine, the
	// mine is added and left as it is.
	calls.clear();
	MItem::SetHost(nullptr);
	added = &mine;
	GCAddInstalledMineToZoneHandler::execute(&packet, nullptr);
	MItem::SetHost(&unmarking);
	GCAddInstalledMineToZoneHandler::execute(&packet, nullptr);
	CHECK(calls == std::vector<std::string>({"additem|79|0", "additem|79|0"}));
}

TEST(WorldEventHandlers, AVampirePortalCarriesItsOwnerPlaceTargetAndFrameDuration)
{
	World world;
	ReceivePortal(0);
	ReceivePortal(1);
	ReceivePortal(2);
	const std::string common = "portal|9001|" + std::string(kOwner) + "|31|32|" + N(ConvertDurationToFrame(600)) + "|1201|140|150|";
	// Only create flag 0 starts the portal from its main node.
	CHECK(calls == std::vector<std::string>({common + "1", common + "0", common + "0"}));
	CHECK(ConvertDurationToFrame(600) != 600);
}

TEST(WorldEventHandlers, WeatherLightningBloodBibleAndPetInfoArePassedOn)
{
	World world;
	ReceiveWeather(WEATHER_SNOWY, 7);
	ReceiveLightning(3);
	BloodBibleSignInfo sign;
	GCBloodBibleSignInfo bible;
	bible.setSignInfo(&sign);
	GCBloodBibleSignInfoHandler::execute(&bible, nullptr);
	CHECK(lastPointer == &sign);
	PetInfo pet;
	GCPetInfo petInfo;
	petInfo.setObjectID(55);
	petInfo.setPetInfo(&pet);
	GCPetInfoHandler::execute(&petInfo, nullptr);
	CHECK(lastPointer == &pet);
	// The lightning's delay arrives in tenths of a second and is passed on in milliseconds.
	CHECK(calls == std::vector<std::string>({"weather|" + N(WEATHER_SNOWY) + "|7", "lightning|300", "bloodbible", "pet|55"}));
}

TEST(WorldEventHandlers, MissingHostsAndEntriesSkipEverything)
{
	World world;
	WorldEvents::SetHost(nullptr);
	RunEveryHandler();
	CHECK(calls.empty());
	CHECK(WorldEvents::AddItemToZone(nullptr, true) == nullptr);
	const WorldEvents::Host empty{};
	CHECK(WorldEvents::SetHost(&empty) == nullptr);
	RunEveryHandler();
	CHECK(calls.empty());
	CHECK(WorldEvents::SetHost(&host) == &empty);
	RunEveryHandler();
	CHECK_EQ(8u, calls.size());
}

TEST(WorldEventHandlers, TheClientsPlaySoundIsTheHostsEntry)
{
	// The UI's sounds go through it (task 4.252).
	World world;
	static std::vector<int> played;
	played.clear();
	const WorldEvents::Host sound{
		.PlaySound = [](TYPE_SOUNDID soundID) { played.push_back(soundID); },
	};
	const WorldEvents::Host* old = WorldEvents::SetHost(&sound);
	::PlaySound(7);
	CHECK(played == std::vector<int>({7}));
	WorldEvents::SetHost(nullptr);
	::PlaySound(8);
	CHECK(played == std::vector<int>({7}));
	WorldEvents::SetHost(old);
}

TEST(WorldEventHandlers, TheViewQueriesAndTheHotKeyPassTheirArguments)
{
	// The view's entries PacketFunction reaches through, and the quick
	// slot's hot key (task 4.253).
	World world;
	static std::vector<std::string> seen;
	seen.clear();
	const WorldEvents::Host* old = WorldEvents::SetHost(nullptr);
	// Without a host: the origin, no sprite, nothing set.
	const POINT none = WorldEvents::MapToScreen(3, 4);
	CHECK(none.x == 0 && none.y == 0);
	CHECK_EQ(-1, WorldEvents::CreatureSpriteHeight(1, 2, 3));
	WorldEvents::SetFlagSprites(true);
	PlayerPresentation::SetHotKey(1, 2);
	CHECK(seen.empty());
	const WorldEvents::Host view{
		.MapToScreen = [](int sectorX, int sectorY) { POINT point = { sectorX * 10, sectorY * 100 }; return point; },
		.CreatureSpriteHeight = [](int body, int action, int direction) { return body * 100 + action * 10 + direction; },
		.SetFlagSprites = [](bool bTae) { seen.push_back(bTae ? "flag tae" : "flag original"); },
	};
	WorldEvents::SetHost(&view);
	const POINT point = WorldEvents::MapToScreen(3, 4);
	CHECK(point.x == 30 && point.y == 400);
	CHECK_EQ(123, WorldEvents::CreatureSpriteHeight(1, 2, 3));
	WorldEvents::SetFlagSprites(true);
	WorldEvents::SetFlagSprites(false);
	PlayerPresentation::Host keys{};
	keys.SetHotKey = [](int hotkey, int id) { seen.push_back("hot key " + std::to_string(hotkey) + "|" + std::to_string(id)); };
	const PlayerPresentation::Host* oldKeys = PlayerPresentation::SetHost(&keys);
	PlayerPresentation::SetHotKey(1, 2);
	PlayerPresentation::SetHost(oldKeys);
	WorldEvents::SetHost(old);
	CHECK(seen == std::vector<std::string>({"flag tae", "flag original", "hot key 1|2"}));
}
