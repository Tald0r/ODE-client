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
// struct as its six fields comma-separated, in declaration order. A
// weapon family is read by its enumerator name, and a StatAttr as its
// six fields in declaration order. An equip race is read by its
// EquipRace enumerator name, an EquipRequirement as its six fields and
// an EquipStats as its five, in declaration order, and requiredStats'
// result is written as its six fields comma-separated. A skill output
// row reads the ten SkillInput fields in declaration order, each as a
// 32-bit int except the gun, read by its GunClass enumerator name, and
// is written as the six SkillOutput fields comma-separated, in
// declaration order, from a zeroed SkillOutput. An empty field where a
// number is read is an error, not an input of 0, and so is a value past
// its column's 32-bit type; an option list is "-" or integers joined by
// single commas. Row names are unique within a file,
// every file has at least one row for each function it belongs to, and a
// vector file this suite does not know fails rather than being skipped.
//
// decore_tests links only decore and the test framework, which proves the
// copy is self-contained.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "domain/EquipRequirement.h"
#include "domain/Formulas.h"
#include "domain/ItemDurability.h"
#include "domain/ItemGrade.h"
#include "domain/ItemPrice.h"
#include "domain/SkillOutputFormulas.h"
#include "domain/SkillRange.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
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
		{ "price.tsv", { "itemPrice", "skullSellTotal", "applyCastleTax" } },
		{ "repair_price.tsv", { "repairPrice" } },
		{ "durability.tsv", { "maxDurabilityBase", "maxDurabilityWithOptions", "maxDurability" } },
		{ "item_grade.tsv", { "gradeOffsets", "gradePolicyOf", "hasDurability" } },
		{ "stats.tsv", {
			"slayerToHit", "vampireToHit", "oustersToHit",
			"slayerDefense", "vampireDefense", "oustersDefense",
			"slayerProtection", "vampireProtection", "oustersProtection",
			"slayerMinDamage", "vampireMinDamage", "oustersMinDamage",
			"slayerMaxDamage", "vampireMaxDamage", "oustersMaxDamage",
			"slayerStealRatio", "vampireStealRatio", "oustersStealRatio",
			"vampireSkillConsumeMP", "vampireDexHPRegenBonus" } },
		{ "equip.tsv", { "requiredStats", "meetsRequirement", "genderAllows" } },
		{ "skill_output.tsv", { "WillOfLife", "Bless", "partyEffectBoost", "partyDurationBoost" } },
		{ "skill_range.tsv", { "skillRange" } },
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

//----------------------------------------------------------------------
// The weapon families, and the names the vector files spell them by: the
// enumerator names, as the server's harness spells them.
//----------------------------------------------------------------------
const decore::WeaponFamily	kWeaponFamilies[] = {
	decore::WeaponFamily::None, decore::WeaponFamily::Sword, decore::WeaponFamily::Blade,
	decore::WeaponFamily::Cross, decore::WeaponFamily::Mace, decore::WeaponFamily::Arms,
	decore::WeaponFamily::Other,
};

std::string	WeaponFamilyName(decore::WeaponFamily weapon)
{
	switch (weapon)
	{
		case decore::WeaponFamily::None :	return "None";
		case decore::WeaponFamily::Sword :	return "Sword";
		case decore::WeaponFamily::Blade :	return "Blade";
		case decore::WeaponFamily::Cross :	return "Cross";
		case decore::WeaponFamily::Mace :	return "Mace";
		case decore::WeaponFamily::Arms :	return "Arms";
		case decore::WeaponFamily::Other :	return "Other";
	}
	return "?";
}

//----------------------------------------------------------------------
// The gun classes, and the names the vector files spell them by: the
// enumerator names, as the server's harness spells them.
//----------------------------------------------------------------------
const decore::skillformula::GunClass	kGunClasses[] = {
	decore::skillformula::GunClass::SG, decore::skillformula::GunClass::AR,
	decore::skillformula::GunClass::SMG, decore::skillformula::GunClass::SR,
	decore::skillformula::GunClass::Other,
};

std::string	GunClassName(decore::skillformula::GunClass gun)
{
	switch (gun)
	{
		case decore::skillformula::GunClass::SG :		return "SG";
		case decore::skillformula::GunClass::AR :		return "AR";
		case decore::skillformula::GunClass::SMG :		return "SMG";
		case decore::skillformula::GunClass::SR :		return "SR";
		case decore::skillformula::GunClass::Other :	return "Other";
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

	// An empty field is malformed: a doubled tab or a missing value must
	// not pass as an input of 0.
	long long	Integer()
	{
		const std::string text = Next();
		if (text.empty())
		{
			SetError("empty field");
			return 0;
		}
		char* end = nullptr;
		const long long value = std::strtoll(text.c_str(), &end, 10);
		if (end == nullptr || *end != '\0')
			SetError("not an integer: \"" + text + "\"");
		return value;
	}

	// A column of a 32-bit type: a value past the type is an error, so a
	// mistyped limit row cannot wrap to another input and still pass.
	unsigned	UnsignedInteger()
	{
		const long long value = Integer();
		if (value < 0 || value > (long long)std::numeric_limits<unsigned>::max())
			SetError("out of unsigned range");
		return (unsigned)value;
	}

	int	IntInteger()
	{
		const long long value = Integer();
		if (value < (long long)std::numeric_limits<int>::min()
			|| value > (long long)std::numeric_limits<int>::max())
			SetError("out of int range");
		return (int)value;
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
		// getline yields no token for an empty cell or after a trailing
		// comma, so either would silently drop an entry.
		if (text.empty() || text[text.size() - 1] == ',')
		{
			SetError("not an integer list: \"" + text + "\"");
			return values;
		}
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

	decore::EquipRace	EquipRace()
	{
		const std::string text = Next();
		if (text == "Slayer")
			return decore::EquipRace::Slayer;
		if (text == "Vampire")
			return decore::EquipRace::Vampire;
		if (text == "Ousters")
			return decore::EquipRace::Ousters;
		SetError("not an equip race: \"" + text + "\"");
		return decore::EquipRace::Slayer;
	}

	// The six EquipRequirement fields in declaration order.
	decore::EquipRequirement	EquipRequirement()
	{
		decore::EquipRequirement r = {};
		r.str = (int)Integer();
		r.dex = (int)Integer();
		r.inte = (int)Integer();
		r.sum = (int)Integer();
		r.level = (int)Integer();
		r.gender = (int)Integer();
		return r;
	}

	// The five EquipStats fields in declaration order.
	decore::EquipStats	EquipStats()
	{
		decore::EquipStats c = {};
		c.str = (int)Integer();
		c.dex = (int)Integer();
		c.inte = (int)Integer();
		c.level = (int)Integer();
		c.sex = (int)Integer();
		return c;
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

	decore::WeaponFamily	WeaponFamily()
	{
		const std::string text = Next();
		for (decore::WeaponFamily weapon : kWeaponFamilies)
		{
			if (text == WeaponFamilyName(weapon))
				return weapon;
		}
		SetError("not a weapon family: \"" + text + "\"");
		return decore::WeaponFamily::None;
	}

	// The six StatAttr fields in declaration order.
	decore::StatAttr	StatAttr()
	{
		decore::StatAttr a = {};
		a.str = (int)Integer();
		a.dex = (int)Integer();
		a.inte = (int)Integer();
		a.level = (int)Integer();
		a.weapon = WeaponFamily();
		a.weaponDomainLevel = (int)Integer();
		return a;
	}

	decore::skillformula::GunClass	GunClass()
	{
		const std::string text = Next();
		for (decore::skillformula::GunClass gun : kGunClasses)
		{
			if (text == GunClassName(gun))
				return gun;
		}
		SetError("not a gun class: \"" + text + "\"");
		return decore::skillformula::GunClass::Other;
	}

	// The ten SkillInput fields in declaration order.
	decore::skillformula::SkillInput	SkillInput()
	{
		decore::skillformula::SkillInput in = {};
		in.SkillLevel = IntInteger();
		in.DomainLevel = IntInteger();
		in.DomainGrade = IntInteger();
		in.STR = IntInteger();
		in.DEX = IntInteger();
		in.INTE = IntInteger();
		in.TargetType = IntInteger();
		in.Range = IntInteger();
		in.Gun = GunClass();
		in.PartySize = IntInteger();
		return in;
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
// The stat functions a row names, by the arguments they take after the
// StatAttr: none, or one int (the combat damage bonus, or the steal
// amount).
//----------------------------------------------------------------------
typedef int (*StatFunction)(const decore::StatAttr&);
typedef int (*StatFunctionWithInt)(const decore::StatAttr&, int);

const std::map<std::string, StatFunction>&	StatFunctions()
{
	static const std::map<std::string, StatFunction> functions = {
		{ "slayerToHit", decore::slayerToHit },
		{ "vampireToHit", decore::vampireToHit },
		{ "oustersToHit", decore::oustersToHit },
		{ "slayerDefense", decore::slayerDefense },
		{ "vampireDefense", decore::vampireDefense },
		{ "oustersDefense", decore::oustersDefense },
		{ "slayerProtection", decore::slayerProtection },
		{ "vampireProtection", decore::vampireProtection },
		{ "oustersProtection", decore::oustersProtection },
		{ "oustersMinDamage", decore::oustersMinDamage },
		{ "oustersMaxDamage", decore::oustersMaxDamage },
	};
	return functions;
}

const std::map<std::string, StatFunctionWithInt>&	StatFunctionsWithInt()
{
	static const std::map<std::string, StatFunctionWithInt> functions = {
		{ "slayerMinDamage", decore::slayerMinDamage },
		{ "slayerMaxDamage", decore::slayerMaxDamage },
		{ "vampireMinDamage", decore::vampireMinDamage },
		{ "vampireMaxDamage", decore::vampireMaxDamage },
		{ "slayerStealRatio", decore::slayerStealRatio },
	};
	return functions;
}

//----------------------------------------------------------------------
// The skill output formulas a row names. Each takes the ten SkillInput
// fields and yields the six SkillOutput fields.
//----------------------------------------------------------------------
typedef void (*SkillOutputFunction)(const decore::skillformula::SkillInput&, decore::skillformula::SkillOutput&);

const std::map<std::string, SkillOutputFunction>&	SkillOutputFunctions()
{
	static const std::map<std::string, SkillOutputFunction> functions = {
		{ "WillOfLife", decore::skillformula::WillOfLife },
		{ "Bless", decore::skillformula::Bless },
	};
	return functions;
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
		const unsigned priceTimesNum = in.UnsignedInteger();
		const unsigned bonus = in.UnsignedInteger();
		in.Finish();
		result = decore::skullSellTotal(priceTimesNum, bonus);
	}
	else if (function == "applyCastleTax")
	{
		const unsigned total = in.UnsignedInteger();
		const int ratio = in.IntInteger();
		in.Finish();
		result = decore::applyCastleTax(total, ratio);
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
	else if (function == "requiredStats")
	{
		const decore::EquipRace race = in.EquipRace();
		const decore::EquipRequirement base = in.EquipRequirement();
		const std::vector<int> reqSums = in.List();
		const std::vector<int> reqLevels = in.List();
		in.Finish();
		error = in.Error();
		if (error.empty() && reqSums.size() != reqLevels.size())
			error = "the option sum and level lists differ in length";
		if (!error.empty())
			return std::string();
		const decore::EquipRequirement r =
			decore::requiredStats(race, base, reqSums.data(), reqLevels.data(), (int)reqSums.size());
		return std::to_string(r.str) + "," + std::to_string(r.dex) + "," + std::to_string(r.inte)
			+ "," + std::to_string(r.sum) + "," + std::to_string(r.level) + "," + std::to_string(r.gender);
	}
	else if (function == "meetsRequirement")
	{
		const decore::EquipRace race = in.EquipRace();
		const decore::EquipRequirement required = in.EquipRequirement();
		const decore::EquipStats current = in.EquipStats();
		in.Finish();
		result = decore::meetsRequirement(race, required, current) ? 1 : 0;
	}
	else if (function == "genderAllows")
	{
		const int sex = (int)in.Integer();
		const int reqGender = (int)in.Integer();
		in.Finish();
		result = decore::genderAllows(sex, reqGender) ? 1 : 0;
	}
	else if (SkillOutputFunctions().count(function) != 0)
	{
		const decore::skillformula::SkillInput input = in.SkillInput();
		in.Finish();
		error = in.Error();
		if (!error.empty())
			return std::string();
		decore::skillformula::SkillOutput out;
		SkillOutputFunctions().at(function)(input, out);
		return std::to_string(out.Damage) + "," + std::to_string(out.Duration) + "," + std::to_string(out.Tick)
			+ "," + std::to_string(out.ToHit) + "," + std::to_string(out.Range) + "," + std::to_string(out.Delay);
	}
	else if (function == "partyEffectBoost")
	{
		const int partySize = in.IntInteger();
		in.Finish();
		result = decore::skillformula::partyEffectBoost(partySize);
	}
	else if (function == "partyDurationBoost")
	{
		const int partySize = in.IntInteger();
		in.Finish();
		result = decore::skillformula::partyDurationBoost(partySize);
	}
	else if (function == "skillRange")
	{
		const int minRange = in.IntInteger();
		const int maxRange = in.IntInteger();
		const int expLevel = in.IntInteger();
		in.Finish();
		result = decore::skillRange(minRange, maxRange, expLevel);
	}
	else if (StatFunctions().count(function) != 0)
	{
		const decore::StatAttr a = in.StatAttr();
		in.Finish();
		result = StatFunctions().at(function)(a);
	}
	else if (StatFunctionsWithInt().count(function) != 0)
	{
		const decore::StatAttr a = in.StatAttr();
		const int value = (int)in.Integer();
		in.Finish();
		result = StatFunctionsWithInt().at(function)(a, value);
	}
	else if (function == "vampireStealRatio")
	{
		const int amount = (int)in.Integer();
		in.Finish();
		result = decore::vampireStealRatio(amount);
	}
	else if (function == "oustersStealRatio")
	{
		const int amount = (int)in.Integer();
		in.Finish();
		result = decore::oustersStealRatio(amount);
	}
	else if (function == "vampireSkillConsumeMP")
	{
		const int originalMP = (int)in.Integer();
		const int magicLevel = (int)in.Integer();
		const int intStat = (int)in.Integer();
		in.Finish();
		result = decore::vampireSkillConsumeMP(originalMP, magicLevel, intStat);
	}
	else if (function == "vampireDexHPRegenBonus")
	{
		const int dexBasic = (int)in.Integer();
		in.Finish();
		result = decore::vampireDexHPRegenBonus(dexBasic);
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

TEST(DecoreVectors, Stats)
{
	CHECK(CheckVectorFile("stats.tsv") > 0);
}

TEST(DecoreVectors, Equip)
{
	CHECK(CheckVectorFile("equip.tsv") > 0);
}

TEST(DecoreVectors, SkillOutput)
{
	CHECK(CheckVectorFile("skill_output.tsv") > 0);
}

TEST(DecoreVectors, SkillRange)
{
	CHECK(CheckVectorFile("skill_range.tsv") > 0);
}

//----------------------------------------------------------------------
// A doubled tab or a missing value leaves an empty field; read as a
// number it is an error, not an input of 0, and so is a flag read the
// same way (the server's SharedVectors.AnEmptyNumberIsAnError).
//----------------------------------------------------------------------
TEST(DecoreVectors, AnEmptyNumberIsAnError)
{
	const std::vector<std::string> fields = { "hasDurability", "empty-item-class", "", "0" };
	RowReader in(fields);
	CHECK_EQ(0LL, in.Integer());
	in.Finish();
	CHECK(in.Error() == "empty field");

	const std::vector<std::string> flagFields = { "itemPrice", "empty-flag", "", "0" };
	RowReader flags(flagFields);
	CHECK_EQ(false, flags.Flag());
	CHECK(flags.Error() == "empty field");

	const std::vector<std::string> filled = { "hasDurability", "filled", "7", "0" };
	RowReader ok(filled);
	CHECK_EQ(7LL, ok.Integer());
	ok.Finish();
	CHECK(ok.Error().empty());
}

//----------------------------------------------------------------------
// A 32-bit column reads its limits and rejects a value one past them
// (the server's SharedVectors.AnOutOfRangeNumberIsAnError).
//----------------------------------------------------------------------
TEST(DecoreVectors, AnOutOfRangeNumberIsAnError)
{
	const std::vector<std::string> limits = { "applyCastleTax", "limits", "4294967295",
		"-2147483648", "2147483647", "0" };
	RowReader ok(limits);
	CHECK_EQ(4294967295u, ok.UnsignedInteger());
	CHECK_EQ(std::numeric_limits<int>::min(), ok.IntInteger());
	CHECK_EQ(std::numeric_limits<int>::max(), ok.IntInteger());
	ok.Finish();
	CHECK(ok.Error().empty());

	for (const char* text : { "4294967296", "-1" })
	{
		const std::vector<std::string> fields = { "applyCastleTax", "past-unsigned", text, "0" };
		RowReader in(fields);
		in.UnsignedInteger();
		CHECK(in.Error() == "out of unsigned range");
	}
	for (const char* text : { "2147483648", "-2147483649" })
	{
		const std::vector<std::string> fields = { "applyCastleTax", "past-int", text, "0" };
		RowReader in(fields);
		in.IntInteger();
		CHECK(in.Error() == "out of int range");
	}
}

//----------------------------------------------------------------------
// A list is "-" or integers joined by single commas: an empty cell or an
// empty entry anywhere would otherwise read as one option fewer (the
// server's SharedVectors.AMalformedListIsAnError).
//----------------------------------------------------------------------
TEST(DecoreVectors, AMalformedListIsAnError)
{
	for (const char* text : { "", "5,", ",5", "5,,6", "5,x" })
	{
		const std::vector<std::string> fields = { "requiredStats", "malformed-list", text, "0" };
		RowReader in(fields);
		in.List();
		CHECK(in.Error() == "not an integer list: \"" + std::string(text) + "\"");
	}

	const std::vector<std::string> none = { "requiredStats", "no-options", "-", "0" };
	RowReader noOptions(none);
	CHECK(noOptions.List().empty());
	CHECK(noOptions.Error().empty());

	const std::vector<std::string> two = { "requiredStats", "two-options", "30,40", "0" };
	RowReader twoOptions(two);
	CHECK(twoOptions.List() == std::vector<int>({ 30, 40 }));
	CHECK(twoOptions.Error().empty());
}

//----------------------------------------------------------------------
// A gun class is one of the five enumerator names, spelled exactly (the
// server's SharedVectors.AnUnknownGunClassIsAnError).
//----------------------------------------------------------------------
TEST(DecoreVectors, AnUnknownGunClassIsAnError)
{
	for (const char* text : { "", "sg", "Rifle", "SR " })
	{
		const std::vector<std::string> fields = { "WillOfLife", "bad-gun", text, "0" };
		RowReader in(fields);
		CHECK(in.GunClass() == decore::skillformula::GunClass::Other);
		CHECK(in.Error() == "not a gun class: \"" + std::string(text) + "\"");
	}
	for (decore::skillformula::GunClass gun : kGunClasses)
	{
		const std::vector<std::string> fields = { "WillOfLife", "good-gun", GunClassName(gun), "0" };
		RowReader in(fields);
		CHECK(in.GunClass() == gun);
		CHECK(in.Error().empty());
	}
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
