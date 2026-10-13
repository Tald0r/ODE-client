#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "DialogPresentationHost.h"
#include "ExperienceTable.h"
#include "MItem.h"
#include "MStorage.h"
#include "PetInfo.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCPetStashList.h"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// The pet stash replaces the storage with a new one holding a pet item per
// stashed pet, in the pet's slot, built from its details: kind, HP, level,
// attribute, food, the experience left to its next level, what it can do,
// its option, its item ID and how many days it has been kept. Then the pet
// storage window opens on it.
namespace {

std::vector<std::string> calls;
int alive = 0;
bool plainItem = false;

struct Pet : MPetItem
{
	Pet() { alive++; }
	~Pet() { alive--; }
};
struct Plain : MItem
{
	Plain() { alive++; }
	~Plain() { alive--; }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_PET_ITEM; }
};

MItemHost FactoryHost()
{
	MItemHost host{};
	host.NewItem = [](ITEM_CLASS itemClass) -> MItem* {
		if (itemClass != ITEM_CLASS_PET_ITEM) return nullptr;
		if (plainItem) return new Plain;
		return new Pet;
	};
	return host;
}
const MItemHost items = FactoryHost();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.RunPetStorage = []() { calls.push_back("run"); };
	host.SetPetStorage = [](MStorage* pStorage) {
		calls.push_back(pStorage == g_pStorage ? "show|stash" : "show|other");
	};
	return host;
}();

struct World : GameModelWorld
{
	ExperienceTable experience;
	ExperienceTable* oldExperience = g_pExperienceTable;
	MStorage* oldStorage = g_pStorage;
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	std::filesystem::path petExp = std::filesystem::temp_directory_path() / ("de_pet_exp_"
		+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
	World()
	{
		// Levels 1 to 3: goal 10, 20, 30; accumulated 100, 300, 600.
		{
			std::ofstream out(petExp, std::ios::binary);
			const std::int32_t rows[] = {3, 1, 10, 100, 2, 20, 300, 3, 30, 600};
			out.write(reinterpret_cast<const char*>(rows), sizeof(rows));
		}
		std::ifstream in(petExp, std::ios::binary);
		experience.LoadFromFilePetExp(in);
		g_pExperienceTable = &experience;
		g_pStorage = new MStorage;
		g_pStorage->Init(1);
		MItem::SetHost(&items);
		calls.clear();
		alive = 0;
		plainItem = false;
	}
	~World()
	{
		delete g_pStorage;
		g_pStorage = oldStorage;
		MItem::SetHost(nullptr);
		DialogPresentation::SetHost(oldDialog);
		g_pExperienceTable = oldExperience;
		std::error_code ignored;
		std::filesystem::remove(petExp, ignored);
	}
};

template<class Value>
std::vector<unsigned char> Wire(const Value& value)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream stream(&out);
	stream.write(value);
	return SocketOutputStreamTestAccess::Bytes(stream);
}

struct Stashed
{
	int slot;
	PetInfo info;
	ObjectID_t itemID;
	DWORD keepDays;
};

// The stash's 20 slots as the server sends them. PetInfo::write writes a
// zero item ID (the client never sends one), so each pet's ID is put in
// its place.
GCPetStashList Decoded(std::vector<Stashed>& pets)
{
	std::vector<unsigned char> bytes;
	for (int slot = 0; slot < MAX_PET_STASH; slot++)
	{
		Stashed* pet = nullptr;
		for (Stashed& candidate : pets)
			if (candidate.slot == slot) pet = &candidate;
		bytes.push_back(pet ? 1 : 0);
		if (pet == nullptr) continue;
		Socket out((EnsureSocketsInitialised(), new SocketImpl()));
		SocketOutputStream stream(&out);
		pet->info.write(stream);
		std::vector<unsigned char> info = SocketOutputStreamTestAccess::Bytes(stream);
		const std::vector<unsigned char> id = Wire(pet->itemID);
		// A PET_NONE writes only its type, and no ID.
		CHECK(info.size() >= id.size());
		if (info.size() >= id.size())
			std::copy(id.begin(), id.end(), info.end() - id.size());
		bytes.insert(bytes.end(), info.begin(), info.end());
		const std::vector<unsigned char> days = Wire(pet->keepDays);
		bytes.insert(bytes.end(), days.begin(), days.end());
	}
	GCPetStashList packet;
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 4096);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
	return packet;
}

// The packet never frees what its read allocates (a follow-up in the doc).
void Free(GCPetStashList& packet)
{
	for (PetStashItemInfo*& stashed : packet.getPetStashItemInfos())
	{
		if (stashed != nullptr) delete stashed->pPetInfo;
		delete stashed;
		stashed = nullptr;
	}
}

// Each ability on its own, so a swapped one shows.
PetInfo Info(PetType_t type, PetLevel_t level, PetExp_t exp, OptionType_t option, bool gamble, bool cutHead, bool attack)
{
	PetInfo info;
	info.setPetType(type);
	info.setPetCreatureType(3);
	info.setPetLevel(level);
	info.setPetExp(exp);
	info.setPetHP(77);
	info.setPetAttr(4);
	info.setPetAttrLevel(6);
	info.setPetOption(option);
	info.setFoodType(9);
	info.setGamble(gamble ? 1 : 0);
	info.setCutHead(cutHead ? 1 : 0);
	info.setAttack(attack ? 1 : 0);
	return info;
}

void Receive(std::vector<Stashed> pets)
{
	GCPetStashList packet = Decoded(pets);
	GCPetStashListHandler::execute(&packet, nullptr);
	Free(packet);
}

} // namespace

TEST(PetStashHandler, EachStashedPetBecomesAPetItemInItsSlotThenTheWindowOpens)
{
	World world;
	// The old storage, and the pet it holds, go (a new storage can reuse
	// the old one's address, so the pet is what tells them apart).
	CHECK(g_pStorage->SetItem(5, new Pet));
	Receive({{0, Info(1, 2, 50, 11, true, false, true), 900, 3}, {3, Info(2, 3, 100, 0, false, true, false), 901, 8}});
	CHECK(g_pStorage->GetItem(5) == nullptr);
	CHECK(calls == std::vector<std::string>({"run", "show|stash"}));
	const MPetItem* first = dynamic_cast<const MPetItem*>(g_pStorage->GetItem(0));
	CHECK(first != nullptr);
	if (first != nullptr)
	{
		CHECK_EQ(900, static_cast<long long>(first->GetID()));
		CHECK_EQ(1, static_cast<long long>(first->GetItemType()));
		CHECK_EQ(77, static_cast<long long>(first->GetCurrentDurability()));
		CHECK_EQ(4, static_cast<long long>(first->GetEnchantLevel()));
		CHECK_EQ(6, first->GetSilver());
		CHECK_EQ(2, static_cast<long long>(first->GetNumber()));
		CHECK_EQ(9, static_cast<long long>(first->GetPetFoodType()));
		// Level 2 accumulates 300; 50 is earned.
		CHECK_EQ(250, static_cast<long long>(first->GetPetExpRemain()));
		CHECK(first->IsCanGamble() && !first->IsCanCutHead() && first->IsCanAttack());
		CHECK(first->GetItemOptionList() == std::list<TYPE_ITEM_OPTION>({11}));
		CHECK_EQ(3, static_cast<long long>(first->GetPetKeepedDay()));
	}
	const MPetItem* fourth = dynamic_cast<const MPetItem*>(g_pStorage->GetItem(3));
	CHECK(fourth != nullptr);
	if (fourth != nullptr)
	{
		CHECK_EQ(901, static_cast<long long>(fourth->GetID()));
		CHECK_EQ(500, static_cast<long long>(fourth->GetPetExpRemain()));
		CHECK(!fourth->IsCanGamble() && fourth->IsCanCutHead() && !fourth->IsCanAttack());
		// No option is no option.
		CHECK(fourth->GetItemOptionList().empty());
		CHECK_EQ(8, static_cast<long long>(fourth->GetPetKeepedDay()));
	}
	CHECK(g_pStorage->GetItem(1) == nullptr);
	CHECK_EQ(2, alive);
}

TEST(PetStashHandler, AnEmptyStashStillReplacesTheStorageAndOpensTheWindow)
{
	World world;
	CHECK(g_pStorage->SetItem(5, new Pet));
	Receive({});
	CHECK(g_pStorage->GetItem(5) == nullptr);
	CHECK_EQ(0, alive);
	CHECK(calls == std::vector<std::string>({"run", "show|stash"}));
}

TEST(PetStashHandler, AFactoryThatMakesNoPetItemSkipsTheSlot)
{
	World world;
	plainItem = true;
	Receive({{0, Info(1, 2, 50, 0, true, true, true), 900, 3}});
	CHECK(g_pStorage->GetItem(0) == nullptr);
	CHECK_EQ(0, alive);
	MItem::SetHost(nullptr);
	Receive({{0, Info(1, 2, 50, 0, true, true, true), 900, 3}});
	CHECK(g_pStorage->GetItem(0) == nullptr);
}

TEST(PetStashHandler, MissingServicesStillBuildTheStash)
{
	World world;
	DialogPresentation::SetHost(nullptr);
	Receive({{0, Info(1, 2, 50, 0, true, true, true), 900, 3}});
	const DialogPresentation::Host empty{};
	DialogPresentation::SetHost(&empty);
	Receive({{1, Info(1, 2, 50, 0, true, true, true), 902, 3}});
	CHECK(calls.empty());
	CHECK(g_pStorage->GetItem(1) != nullptr);
}
