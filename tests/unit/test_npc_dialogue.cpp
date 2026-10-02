#include "test_framework.h"
#include "MNPCScriptTable.h"
#include "MNPCScriptTableEnglish.h"
#include "TextEncoding.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Bytes
{
	std::vector<unsigned char> data;
	Bytes& U32(std::uint32_t value)
	{
		for (unsigned int shift = 0; shift < 32; shift += 8)
			data.push_back(static_cast<unsigned char>(value >> shift));
		return *this;
	}
	Bytes& Text(const std::string& value)
	{
		U32(static_cast<std::uint32_t>(value.size()));
		data.insert(data.end(), value.begin(), value.end());
		return *this;
	}
	Bytes& Strings(std::initializer_list<std::string> values)
	{
		U32(static_cast<std::uint32_t>(values.size()));
		for (const auto& value : values) Text(value);
		return *this;
	}
	Bytes& Row(const std::string& owner, std::initializer_list<std::string> subjects,
		std::initializer_list<std::string> replies)
	{
		return Text(owner).Strings(subjects).Strings(replies);
	}
};

struct DialogueFile
{
	std::filesystem::path path;
	explicit DialogueFile(const Bytes& bytes)
	{
		static unsigned int sequence = 0;
		path = std::filesystem::temp_directory_path() / ("darkeden_npc_dialogue_"
			+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
			+ "_" + std::to_string(sequence++) + ".bin");
		std::ofstream output(path, std::ios::binary);
		if (!bytes.data.empty())
			output.write(reinterpret_cast<const char*>(bytes.data.data()),
				static_cast<std::streamsize>(bytes.data.size()));
		output.close();
		if (!output.good()) throw std::runtime_error("Cannot write dialogue fixture");
	}
	~DialogueFile() { std::error_code error; std::filesystem::remove(path, error); }
};

template<class Table>
bool Load(Table& table, const Bytes& bytes)
{
	DialogueFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	table.LoadFromFile(input);
	if (input.good()) CHECK_EQ(bytes.data.size(), input.tellg());
	return input.good();
}

template<class Table>
std::vector<unsigned char> Save(Table& table)
{
	DialogueFile saved({});
	{
		std::ofstream output(saved.path, std::ios::binary);
		table.SaveToFile(output);
		CHECK(output.good());
	}
	std::ifstream input(saved.path, std::ios::binary);
	return {std::istreambuf_iterator<char>(input), {}};
}

bool Is(const char* actual, const char* expected)
{
	return actual != nullptr && std::strcmp(actual, expected) == 0;
}

struct DialogueWorld
{
	MNPCScriptTable table;
	MNPCScriptTable* previous = g_pNPCScriptTable;
	TextEncoding::Encoding encoding = TextEncoding::GetResourceEncoding();
	DialogueWorld()
	{
		g_pNPCScriptTable = &table;
		TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Utf8);
	}
	~DialogueWorld()
	{
		g_pNPCScriptTable = previous;
		TextEncoding::SetResourceEncoding(encoding);
	}
	NPC_SCRIPT& Add(unsigned int id, const std::string& owner,
		std::initializer_list<std::string> subjects, std::initializer_list<std::string> replies)
	{
		auto row = std::make_unique<NPC_SCRIPT>();
		row->OwnerID = owner.c_str();
		row->SubjectTable.Init(static_cast<int>(subjects.size()));
		row->ContentTable.Init(static_cast<int>(replies.size()));
		int index = 0;
		for (const auto& value : subjects) row->SubjectTable.Set(index++, value.c_str());
		index = 0;
		for (const auto& value : replies) row->ContentTable.Set(index++, value.c_str());
		if (!table.AddData(id, row.get())) throw std::runtime_error("Duplicate dialogue fixture id");
		return *row.release();
	}
};

} // namespace

TEST(NPCDialogue, MissingScriptsHaveZeroCountsAndNullLookups)
{
	DialogueWorld world;
	for (int id : {0, 1, -1, 2000000000})
	{
		CHECK_EQ(0, world.table.GetSubjectSize(id));
		CHECK_EQ(0, world.table.GetContentSize(id));
		CHECK(world.table.GetSubject(id, 0) == nullptr);
		CHECK(world.table.GetContent(id, 0) == nullptr);
	}
}

TEST(NPCDialogue, SubjectsAndRepliesKeepTheirIndependentIndexes)
{
	DialogueWorld world;
	world.Add(7, "NPC", {"first", "second"}, {"yes", "", "no"});
	CHECK(g_pNPCScriptTable == &world.table);
	CHECK_EQ(2, world.table.GetSubjectSize(7));
	CHECK_EQ(3, world.table.GetContentSize(7));
	CHECK(Is(world.table.GetSubject(7, 0), "first"));
	CHECK(Is(world.table.GetSubject(7, 1), "second"));
	CHECK(Is(world.table.GetContent(7, 0), "yes"));
	CHECK(world.table.GetContent(7, 1) == nullptr); // An assigned empty MString has no buffer.
	CHECK(Is(world.table.GetContent(7, 2), "no"));
	CHECK(world.table.GetSubject(7, 2) == nullptr);
	CHECK(world.table.GetContent(7, 3) == nullptr);
}

TEST(NPCDialogue, BinaryRowsStoreOwnerThenSubjectAndReplyGroups)
{
	DialogueWorld world;
	const auto bytes = Bytes().U32(2)
		.U32(7).Row("NPC", {"first", "second"}, {"yes", "", "no"})
		.U32(90).Row("other", {}, {"back"});
	CHECK(Load(world.table, bytes));
	CHECK_EQ(2, world.table.size());
	CHECK(Is(world.table.GetData(7)->OwnerID.GetString(), "NPC"));
	CHECK(Is(world.table.GetSubject(7, 1), "second"));
	CHECK(Is(world.table.GetContent(7, 2), "no"));
	CHECK_EQ(0, world.table.GetSubjectSize(90));
	CHECK(Is(world.table.GetContent(90, 0), "back"));
	CHECK(Save(world.table) == bytes.data);
}

TEST(NPCDialogue, ReloadReplacesScriptsAndAcceptsAnEmptyTable)
{
	DialogueWorld world;
	world.Add(7, "NPC", {"old"}, {"old reply"});
	CHECK(Load(world.table, Bytes().U32(1).U32(9).Row("new", {"hello"}, {})));
	CHECK_EQ(1, world.table.size());
	CHECK(world.table.GetData(7) == nullptr);
	CHECK(Is(world.table.GetSubject(9, 0), "hello"));
	CHECK(Load(world.table, Bytes().U32(0)));
	CHECK(world.table.empty());
	CHECK(Save(world.table) == Bytes().U32(0).data);
}

TEST(NPCDialogue, DuplicateKeysKeepTheFirstDialogueAndSaveInKeyOrder)
{
	DialogueWorld world;
	CHECK(Load(world.table, Bytes().U32(3)
		.U32(9).Row("first", {"hello"}, {"yes"})
		.U32(9).Row("discarded", {"different"}, {"no"})
		.U32(2).Row("second", {}, {"back"})));
	CHECK_EQ(2, world.table.size());
	CHECK(Save(world.table) == Bytes().U32(2)
		.U32(2).Row("second", {}, {"back"})
		.U32(9).Row("first", {"hello"}, {"yes"}).data);
}

TEST(NPCDialogue, ParametersUseMapKeysAndReplaceAllOriginalOccurrences)
{
	DialogueWorld world;
	world.Add(1, "NPC", {"%(Name), hello %(Name)!"}, {"Ask %(Name) about %(Place)."});
	ScriptParameter name, place;
	name.setName("ignored packet-name field");
	name.setValue("Alice");
	place.setValue("Eslania");
	HashMapScriptParameter parameters{{"Name", &name}, {"Place", &place}};
	std::string text;
	world.table.GetSubjectParameter(1, 0, parameters, text);
	CHECK(text == "Alice, hello Alice!");
	world.table.GetContentParameter(1, 0, parameters, text);
	CHECK(text == "Ask Alice about Eslania.");
	CHECK(Is(world.table.GetSubject(1, 0), "%(Name), hello %(Name)!"));
	CHECK(name.getValue() == "Alice");
}

TEST(NPCDialogue, SubstitutionsSupportEmptyAndUtf8Values)
{
	DialogueWorld world;
	world.Add(1, "NPC", {"%(Empty)%(Name)%(Empty)"}, {"%(Name)/%(Name)"});
	ScriptParameter name, empty;
	name.setValue("가");
	empty.setValue("");
	HashMapScriptParameter parameters{{"Name", &name}, {"Empty", &empty}};
	std::string text;
	world.table.GetSubjectParameter(1, 0, parameters, text);
	CHECK(text == "가");
	world.table.GetContentParameter(1, 0, parameters, text);
	CHECK(text == "가/가");
}

TEST(NPCDialogue, UnknownIncompleteAndCaseMismatchedMarkersStayLiteral)
{
	DialogueWorld world;
	const char* original = "%(name) %(Missing) %(Name %(NameX) %% plain";
	world.Add(1, "NPC", {original}, {original});
	ScriptParameter name;
	name.setValue("Alice");
	std::string text;
	world.table.GetSubjectParameter(1, 0, {{"Name", &name}}, text);
	CHECK(text == original);
	world.table.GetContentParameter(1, 0, {}, text);
	CHECK(text == original);
}

TEST(NPCDialogue, ParameterPassesPreserveTheExistingMapOrder)
{
	DialogueWorld world;
	world.Add(1, "NPC", {"%(A) %(B)"}, {"%(B)"});
	ScriptParameter a, b;
	a.setValue("%(B)");
	b.setValue("done");
	std::string text;
	world.table.GetSubjectParameter(1, 0, {{"B", &b}, {"A", &a}}, text);
	CHECK(text == "done done");
	a.setValue("earlier");
	b.setValue("%(A)");
	world.table.GetContentParameter(1, 0, {{"A", &a}, {"B", &b}}, text);
	CHECK(text == "%(A)");
}

TEST(NPCDialogue, EmptySubjectAndReplyTemplatesClearPreviousOutput)
{
	DialogueWorld world;
	CHECK(Load(world.table, Bytes().U32(1).U32(1).Row("NPC", {""}, {""})));
	std::string text = "previous";
	world.table.GetSubjectParameter(1, 0, {}, text);
	CHECK(text.empty());
	text = "previous";
	world.table.GetContentParameter(1, 0, {}, text);
	CHECK(text.empty());
}

TEST(NPCDialogue, EnglishOverlayChangesExistingSlotsWithoutChangingShape)
{
	DialogueWorld world;
	auto& row = world.Add(100, "owner", {"original", "extra subject"},
		{"first", "second", "third", "fourth", "extra reply"});
	ApplyEnglishNPCScriptTable();
	CHECK_EQ(1, world.table.size());
	CHECK_EQ(2, world.table.GetSubjectSize(100));
	CHECK_EQ(5, world.table.GetContentSize(100));
	CHECK(Is(row.OwnerID.GetString(), "owner"));
	CHECK(Is(world.table.GetContent(100, 0), "Yes. May I see your wares?"));
	CHECK(Is(world.table.GetSubject(100, 1), "extra subject"));
	CHECK(Is(world.table.GetContent(100, 4), "extra reply"));
	const std::string first = world.table.GetSubject(100, 0);
	ApplyEnglishNPCScriptTable();
	CHECK(Is(world.table.GetSubject(100, 0), first.c_str()));
}

TEST(NPCDialogue, EnglishOverlaySkipsMissingTablesScriptsAndSlots)
{
	DialogueWorld world;
	g_pNPCScriptTable = nullptr;
	ApplyEnglishNPCScriptTable();
	g_pNPCScriptTable = &world.table;
	world.Add(100, "empty", {}, {});
	world.Add(2000000000, "unknown", {"keep"}, {"keep reply"});
	ApplyEnglishNPCScriptTable();
	CHECK_EQ(2, world.table.size());
	CHECK_EQ(0, world.table.GetSubjectSize(100));
	CHECK_EQ(0, world.table.GetContentSize(100));
	CHECK(Is(world.table.GetSubject(2000000000, 0), "keep"));
	CHECK(Is(world.table.GetContent(2000000000, 0), "keep reply"));
}

TEST(NPCDialogue, EnglishOverlayKeepsRuntimeParameterMarkers)
{
	DialogueWorld world;
	world.Add(111, "quest", {"original"}, {});
	ApplyEnglishNPCScriptTable();
	CHECK(std::string(world.table.GetSubject(111, 0)).find("%(MonsterName)") != std::string::npos);
	ScriptParameter monster;
	monster.setValue("Wraith");
	std::string text;
	world.table.GetSubjectParameter(111, 0, {{"MonsterName", &monster}}, text);
	CHECK(text.find("Wraith suits you best") != std::string::npos);
	CHECK(text.find("%(MonsterName)") == std::string::npos);
}

TEST(NPCDialogue, AllThreeStringGroupsUseTheDeclaredResourceCodec)
{
	DialogueWorld world;
	TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Cp949);
	const auto bytes = Bytes().U32(1).U32(7).Row("\xb0\xa1", {"\xb3\xaa"}, {"\xb4\xd9"});
	CHECK(Load(world.table, bytes));
	CHECK(Is(world.table.GetData(7)->OwnerID.GetString(), "가"));
	CHECK(Is(world.table.GetSubject(7, 0), "나"));
	CHECK(Is(world.table.GetContent(7, 0), "다"));
	CHECK(Save(world.table) == bytes.data);
}
