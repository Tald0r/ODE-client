//----------------------------------------------------------------------
// test_decore_vectors.cpp
//----------------------------------------------------------------------
//
// The shared parity vectors of the vendored de-core copy
// (third_party/decore/domain/vectors/*.tsv), asserted row by row.
//
// The server records these rows on its reference build and asserts them
// in its formula_tests; this suite asserts the same rows against the
// same source built by every client toolchain (MSVC, clang-cl, GCC,
// Clang, Apple Clang, and Emscripten under node in web.yml). A row that
// fails here is a toolchain computing a different number from the same
// source: investigate it, never re-record it on this side.
//
// The reader mirrors the server's (tests/formula_test.cpp there): LF
// only, '#' comments and blank lines skipped, each row is function, row
// name, the inputs in the order the file's header documents, and the
// expected value last, compared as text: a number as std::to_string
// writes it, a grade policy by its enumerator name, and gradeOffsets'
// struct as its six fields comma-separated, in declaration order. Row names are unique within a file, every file has at
// least one row for each function it belongs to, and a vector file this
// suite does not know fails rather than being skipped.
//
// decore_tests links only decore and the test framework, which proves the
// copy is self-contained.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "domain/ItemDurability.h"
#include "domain/ItemGrade.h"
#include "domain/ItemPrice.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

//----------------------------------------------------------------------
// Every vector file, and the functions each one holds rows for. A sync
// that brings a new file fails VectorDirectoryHoldsOnlyKnownFiles until
// it is listed here.
//----------------------------------------------------------------------
const std::map<std::string, std::set<std::string>>&	KnownFiles()
{
	static const std::map<std::string, std::set<std::string>> files = {
		{ "price.tsv", { "itemPrice", "skullSellTotal" } },
		{ "repair_price.tsv", { "repairPrice" } },
		{ "durability.tsv", { "maxDurabilityBase", "maxDurabilityWithOptions", "maxDurability" } },
		{ "item_grade.tsv", { "gradeOffsets", "gradePolicyOf", "hasDurability" } },
	};
	return files;
}

void	Fail(const std::string& message)
{
	::testfw::RecordFailure(__FILE__, __LINE__, message.c_str());
}

//----------------------------------------------------------------------
// The grade policies, and the names the vector files spell them by: the
// enumerator names, as the server's harness spells them.
//----------------------------------------------------------------------
const decore::GradePolicy	kGradePolicies[] = {
	decore::GradePolicy::None, decore::GradePolicy::Plain, decore::GradePolicy::Weapon,
	decore::GradePolicy::Cloth, decore::GradePolicy::Grocery, decore::GradePolicy::Accessory,
};

std::string	GradePolicyName(decore::GradePolicy policy)
{
	switch (policy)
	{
		case decore::GradePolicy::None :		return "None";
		case decore::GradePolicy::Plain :		return "Plain";
		case decore::GradePolicy::Weapon :		return "Weapon";
		case decore::GradePolicy::Cloth :		return "Cloth";
		case decore::GradePolicy::Grocery :		return "Grocery";
		case decore::GradePolicy::Accessory :	return "Accessory";
	}
	return "?";
}

std::vector<std::string>	SplitTabs(const std::string& line)
{
	std::vector<std::string> fields;
	std::string::size_type start = 0;
	for (;;)
	{
		const std::string::size_type tab = line.find('\t', start);
		fields.push_back(line.substr(start, tab == std::string::npos ? std::string::npos : tab - start));
		if (tab == std::string::npos)
			return fields;
		start = tab + 1;
	}
}

//----------------------------------------------------------------------
// One row's inputs, read left to right. A malformed field records an
// error and reads as 0, so the row fails with a message.
//----------------------------------------------------------------------
class RowReader
{
public:
	explicit RowReader(const std::vector<std::string>& fields)
		: m_Fields(fields), m_Next(2) {}

	long long	Integer()
	{
		const std::string text = Next();
		if (text.empty())
			return 0;
		char* end = nullptr;
		const long long value = std::strtoll(text.c_str(), &end, 10);
		if (end == nullptr || *end != '\0')
			SetError("not an integer: \"" + text + "\"");
		return value;
	}

	bool	Flag()
	{
		const long long value = Integer();
		if (value != 0 && value != 1)
			SetError("not 0 or 1");
		return value == 1;
	}

	std::vector<int>	List()
	{
		const std::string text = Next();
		std::vector<int> values;
		if (text == "-")
			return values;
		std::stringstream items(text);
		std::string item;
		while (std::getline(items, item, ','))
		{
			char* end = nullptr;
			const long value = std::strtol(item.c_str(), &end, 10);
			if (item.empty() || end == nullptr || *end != '\0')
				SetError("not an integer list: \"" + text + "\"");
			values.push_back((int)value);
		}
		return values;
	}

	decore::PriceRace	Race()
	{
		const std::string text = Next();
		if (text == "None")
			return decore::PriceRace::None;
		if (text == "Slayer")
			return decore::PriceRace::Slayer;
		if (text == "Vampire")
			return decore::PriceRace::Vampire;
		if (text == "Ousters")
			return decore::PriceRace::Ousters;
		SetError("not a race: \"" + text + "\"");
		return decore::PriceRace::None;
	}

	decore::GradePolicy	GradePolicy()
	{
		const std::string text = Next();
		for (decore::GradePolicy policy : kGradePolicies)
		{
			if (text == GradePolicyName(policy))
				return policy;
		}
		SetError("not a grade policy: \"" + text + "\"");
		return decore::GradePolicy::None;
	}

	// Every input consumed, and exactly the expected column left.
	void	Finish()
	{
		if (m_Next + 1 != m_Fields.size())
			SetError("expected " + std::to_string(m_Next + 1) + " columns, found "
				+ std::to_string(m_Fields.size()));
	}

	const std::string&	Error() const { return m_Error; }

private:
	std::string	Next()
	{
		if (m_Next + 1 >= m_Fields.size())
		{
			SetError("too few columns");
			m_Next++;
			return std::string();
		}
		return m_Fields[m_Next++];
	}

	void	SetError(const std::string& message)
	{
		if (m_Error.empty())
			m_Error = message;
	}

	const std::vector<std::string>&		m_Fields;
	std::vector<std::string>::size_type	m_Next;
	std::string				m_Error;
};

decore::ItemPriceInput	ReadPriceItem(RowReader& in, std::vector<int>& multipliers)
{
	decore::ItemPriceInput input = {};
	input.itemClass = (int)in.Integer();
	input.itemType = (int)in.Integer();
	input.basePrice = (unsigned)in.Integer();
	input.grade = (int)in.Integer();
	input.charge = (int)in.Integer();
	input.maxCharge = (int)in.Integer();
	multipliers = in.List();
	input.optionPriceMultipliers = multipliers.data();
	input.optionCount = (int)multipliers.size();
	input.curDurability = (unsigned)in.Integer();
	input.maxDurability = (unsigned)in.Integer();
	return input;
}

//----------------------------------------------------------------------
// Evaluates one row and returns the result as the expected column
// spells it; sets `error` for a row this suite cannot read.
//----------------------------------------------------------------------
std::string	EvaluateRow(const std::vector<std::string>& fields, std::string& error)
{
	RowReader in(fields);
	const std::string& function = fields[0];
	long long result = 0;

	if (function == "itemPrice")
	{
		std::vector<int> multipliers;
		decore::ItemPriceInput input = ReadPriceItem(in, multipliers);
		input.marketCond = (int)in.Integer();
		input.mysteriousRack = in.Flag();
		input.createTypeGame = in.Flag();
		input.timeLimited = in.Flag();
		input.crownPrice = (int)in.Integer();
		input.race = in.Race();
		input.currentStatSum = (int)in.Integer();
		input.premiumHalf = in.Flag();
		input.potionPriceRatio = (int)in.Integer();
		in.Finish();
		if (in.Error().empty())
			result = decore::itemPrice(input);
	}
	else if (function == "repairPrice")
	{
		std::vector<int> multipliers;
		decore::ItemPriceInput input = ReadPriceItem(in, multipliers);
		in.Finish();
		if (in.Error().empty())
			result = decore::repairPrice(input);
	}
	else if (function == "skullSellTotal")
	{
		const unsigned priceTimesNum = (unsigned)in.Integer();
		const unsigned bonus = (unsigned)in.Integer();
		in.Finish();
		result = decore::skullSellTotal(priceTimesNum, bonus);
	}
	else if (function == "maxDurabilityBase")
	{
		const unsigned info = (unsigned)in.Integer();
		const bool hasDurability = in.Flag();
		const int offset = (int)in.Integer();
		in.Finish();
		result = decore::maxDurabilityBase(info, hasDurability, offset);
	}
	else if (function == "maxDurabilityWithOptions")
	{
		const unsigned base = (unsigned)in.Integer();
		const std::vector<int> plusPoints = in.List();
		in.Finish();
		result = decore::maxDurabilityWithOptions(base, plusPoints.data(), (int)plusPoints.size());
	}
	else if (function == "maxDurability")
	{
		const unsigned info = (unsigned)in.Integer();
		const bool hasDurability = in.Flag();
		const int offset = (int)in.Integer();
		const std::vector<int> plusPoints = in.List();
		in.Finish();
		result = decore::maxDurability(info, hasDurability, offset, plusPoints.data(), (int)plusPoints.size());
	}
	else if (function == "gradeOffsets")
	{
		const decore::GradePolicy policy = in.GradePolicy();
		const int grade = (int)in.Integer();
		in.Finish();
		error = in.Error();
		const decore::GradeOffsets offsets = decore::gradeOffsets(policy, grade);
		return std::to_string(offsets.durability) + "," + std::to_string(offsets.damage)
			+ "," + std::to_string(offsets.critical) + "," + std::to_string(offsets.defense)
			+ "," + std::to_string(offsets.protection) + "," + std::to_string(offsets.luck);
	}
	else if (function == "gradePolicyOf")
	{
		const int itemClass = (int)in.Integer();
		in.Finish();
		error = in.Error();
		return GradePolicyName(decore::gradePolicyOf(itemClass));
	}
	else if (function == "hasDurability")
	{
		const int itemClass = (int)in.Integer();
		in.Finish();
		result = decore::hasDurability(itemClass) ? 1 : 0;
	}
	else
	{
		error = "unknown function \"" + function + "\"";
		return std::string();
	}
	error = in.Error();
	return std::to_string(result);
}

//----------------------------------------------------------------------
// Asserts every row of one vector file. Returns the number of rows.
//----------------------------------------------------------------------
int	CheckVectorFile(const std::string& file)
{
	const std::string path = std::string(DECORE_VECTOR_DIR) + "/" + file;
	const auto known = KnownFiles().find(file);
	if (known == KnownFiles().end())
	{
		Fail(path + ": not a vector file decore_tests knows; list it in KnownFiles()");
		return 0;
	}
	const std::set<std::string>& functions = known->second;

	std::ifstream in(path.c_str(), std::ios::binary);
	CHECK(in.good());
	if (!in.good())
	{
		Fail("missing " + path);
		return 0;
	}

	std::map<std::string, int> rowsPerFunction;
	std::set<std::string> names;
	int lineNumber = 0;
	int rows = 0;
	std::string line;
	while (std::getline(in, line))
	{
		lineNumber++;
		if (!line.empty() && line[line.size() - 1] == '\r')
			Fail(path + ":" + std::to_string(lineNumber) + ": CRLF line ending; vector files are LF");
		if (line.empty() || line[0] == '#')
			continue;

		const std::vector<std::string> fields = SplitTabs(line);
		const std::string where = path + ":" + std::to_string(lineNumber)
			+ " (" + (fields.size() > 1 ? fields[1] : std::string("?")) + ")";
		::testfw::RecordCheck();
		if (fields.size() < 3)
		{
			Fail(where + ": fewer than three columns");
			continue;
		}
		if (functions.count(fields[0]) == 0)
			Fail(where + ": " + fields[0] + " does not belong in " + file);
		if (!names.insert(fields[1]).second)
			Fail(where + ": row name used twice");
		rowsPerFunction[fields[0]]++;
		rows++;

		std::string error;
		const std::string actual = EvaluateRow(fields, error);
		if (!error.empty())
		{
			Fail(where + ": " + error);
			continue;
		}
		if (fields[fields.size() - 1] != actual)
			Fail(where + ": expected " + fields[fields.size() - 1] + ", actual " + actual);
	}

	for (const std::string& function : functions)
	{
		::testfw::RecordCheck();
		if (rowsPerFunction[function] == 0)
			Fail(path + " has no " + function + " row");
	}

	std::printf("      %s: %d rows\n", file.c_str(), rows);
	return rows;
}

} // namespace

TEST(DecoreVectors, Price)
{
	CHECK(CheckVectorFile("price.tsv") > 0);
}

TEST(DecoreVectors, RepairPrice)
{
	CHECK(CheckVectorFile("repair_price.tsv") > 0);
}

TEST(DecoreVectors, Durability)
{
	CHECK(CheckVectorFile("durability.tsv") > 0);
}

TEST(DecoreVectors, ItemGrade)
{
	CHECK(CheckVectorFile("item_grade.tsv") > 0);
}

//----------------------------------------------------------------------
// Every file in the vector directory is one the tests above check, so a
// sync that brings a new family cannot pass by being ignored, and every
// known file is there.
//----------------------------------------------------------------------
TEST(DecoreVectors, VectorDirectoryHoldsOnlyKnownFiles)
{
	std::set<std::string> found;
	std::error_code error;
	for (std::filesystem::directory_iterator it(DECORE_VECTOR_DIR, error), end; !error && it != end; it.increment(error))
	{
		if (it->is_regular_file())
			found.insert(it->path().filename().string());
	}
	CHECK(!error);
	if (error)
		Fail(std::string("cannot list ") + DECORE_VECTOR_DIR + ": " + error.message());

	CHECK(!found.empty());
	for (const std::string& file : found)
	{
		::testfw::RecordCheck();
		if (KnownFiles().count(file) == 0)
			Fail(std::string(DECORE_VECTOR_DIR) + "/" + file + ": not a vector file decore_tests knows; list it in KnownFiles()");
	}
	for (const auto& known : KnownFiles())
	{
		::testfw::RecordCheck();
		if (found.count(known.first) == 0)
			Fail(std::string(DECORE_VECTOR_DIR) + "/" + known.first + " is missing");
	}
}
