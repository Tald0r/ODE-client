//--------------------------------------------------------------------------
// MItemOptionTable.cpp
//--------------------------------------------------------------------------
#include "Client_PCH.h"
#include "MItemOptionTable.h"
#include <utility>

//--------------------------------------------------------------------------
// Global
//--------------------------------------------------------------------------
ITEMOPTION_TABLE*		g_pItemOptionTable = NULL;

//char ITEMOPTION_INFO::ITEMOPTION_PARTENAME[MAX_PART][MAX_PARTNAME_LENGTH] =
//{
//	"STR",
//	"DEX",
//	"INT",
//	"HP",
//	"MP",
//	"HP Steal",
//	"MP Steal",
//	"HP Regeneration",
//	"MP Regeneration",
//	"ToHit",
//	"Defense",
//	"Damage",
//	"Protection",
//	"Durability",
//	"Poison Resistance",
//	"Acid Resistance",
//	"Curse Resistance",
//	"Blood Resistance",
//	"Vision",
//	"Attack Speed",
//	"Critical Hit",
//	"Luck",         // increase looting item type
//	"All Registance",      // increase all registance
//	"All Attributes",     // increase all attributes(str, dex, int)
//};
//
//char ITEMOPTION_INFO::ITEMOPTION_PARTNAME[MAX_PART][MAX_PARTNAME_LENGTH] =
//{
//	"STR",
//	"DEX",
//	"INT",
//	"HP",
//	"MP",
//	"HP흡수",
//	"MP흡수",
//	"HP재생력",
//	"MP재생력",
//	"명중률",
//	"디펜스",
//	"데미지",
//	"프로텍션",
//	"내구성",
//	"독 마법저항력",
//	"산 마법저항력",
//	"저주 마법저항력",
//	"블러드 마법저항력",
//	"시야",
//	"공격속도",
//	"크리티컬 히트",
//	"행운",
//	"모든 저항력",
//	"모든 능력치",
//};

//--------------------------------------------------------------------------
//
// constructor/destructor
//
//--------------------------------------------------------------------------
ITEMOPTION_INFO::ITEMOPTION_INFO()
: Part(0)
, PlusPoint(0)
, PriceMultiplier(0)
, RequireSTR(0)
, RequireDEX(0)
, RequireINT(0)
, RequireSUM(0)
, RequireLevel(0)
, ColorSet(0)
, UpgradeOptionType(0)
, PreviousOptionType(0)
{
	// The loader fills every row it sizes; a table sized but never
	// loaded (a test's), or a load that returned early, must still hand
	// out defined rows - the none row an unoptioned item reads its
	// colour from above all.
}

ITEMOPTION_INFO::~ITEMOPTION_INFO()
{
}

//#include <stdio.h>

//--------------------------------------------------------------------------
//
// member functions
//
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
// Save To File
//--------------------------------------------------------------------------
//void			
//ITEMOPTION_INFO::SaveToFile(std::ofstream& file)
//{
//	//char str[80];
//	//sprintf(str, "E-%s", Name.GetString());
//	//EName = str;
//
//	EName.SaveToFile(file);							// ItemOption 이름	
//	Name.SaveToFile(file);							// ItemOption 이름
//	file.write((const char*)&Part, 4);				// ItemOption Part	
//	file.write((const char*)&PlusPoint, 4);			// 증가치
//	file.write((const char*)&PriceMultiplier, 4);				// 가치
//	//file.write((const char*)&PlusRequireAbility, 4);	// 필요 능력	
//	
//
//	file.write((const char*)&RequireSTR, 4);
//	file.write((const char*)&RequireDEX, 4);
//	file.write((const char*)&RequireINT, 4);		
//	file.write((const char*)&RequireSUM, 4);
//	file.write((const char*)&RequireLevel, 4);	
//
//	file.write((const char*)&ColorSet, 4);			// ColorSet번호
//	file.write((const char*)&UpgradeOptionType, 4);
//	file.write((const char*)&PreviousOptionType, 4);
//}

//--------------------------------------------------------------------------
// Load From File
//--------------------------------------------------------------------------
void			
ITEMOPTION_INFO::LoadFromFile(std::ifstream& file)
{
	EName.LoadFromFile(file);							// ItemOption 이름	
	Name.LoadFromFile(file);							// ItemOption 이름
	file.read((char*)&Part, 4);				// ItemOption Part	
	file.read((char*)&PlusPoint, 4);			// 증가치
	file.read((char*)&PriceMultiplier, 4);				// 가치
	//file.read((char*)&PlusRequireAbility, 4);	// 필요 능력	
	
	file.read((char*)&RequireSTR, 4);
	file.read((char*)&RequireDEX, 4);
	file.read((char*)&RequireINT, 4);		
	file.read((char*)&RequireSUM, 4);
	file.read((char*)&RequireLevel, 4);

	
	file.read((char*)&ColorSet, 4);			// ColorSet번호
	file.read((char*)&UpgradeOptionType, 4);
	file.read((char*)&PreviousOptionType, 4);
}

//--------------------------------------------------------------------------
// Load From File
//--------------------------------------------------------------------------
void			
ITEMOPTION_INFO::SaveToFile(std::ofstream& file)
{
	(void)file;
}

//--------------------------------------------------------------------------
// Load From File
//--------------------------------------------------------------------------
void
ITEMOPTION_TABLE::LoadFromFile(std::ifstream& file)
{
	int size = 0;
	file.read(reinterpret_cast<char*>(&size), 4);
	if (!file.good())
		return;
	if (size < 0 || size > MAX_PART)
	{
		file.setstate(std::ios::failbit);
		return;
	}

	ITEMOPTION_TABLE pending;
	for(int i = 0; i < size; i++)
	{
		MString englishName, localName;
		englishName.LoadFromFile(file);
		localName.LoadFromFile(file);
		if (!file.good())
			return;
		pending.ITEMOPTION_PARTENAME[i] = englishName.GetString() == nullptr ? "" : englishName.GetString();
		pending.ITEMOPTION_PARTNAME[i] = localName.GetString() == nullptr ? "" : localName.GetString();
	}

	int count = 0;
	file.read(reinterpret_cast<char*>(&count), 4);
	// Two string prefixes and eleven integers are required even for empty names.
	if (!IsEntryCountSane(file, count, 52))
	{
		file.setstate(std::ios::failbit);
		return;
	}
	pending.Init(count);
	for (int i = 0; i < count; ++i)
	{
		ITEMOPTION_INFO& row = *pending.GetMutable(i);
		row.LoadFromFile(file);
		if (!file.good())
			return;
		if (row.Part < 0 || row.Part >= MAX_PART)
		{
			file.setstate(std::ios::failbit);
			return;
		}
	}

	// Publish only after every name and row has been read and validated.
	// Swaps cannot allocate, so even allocation failures leave the old table intact.
	// Empty slots in a smaller valid reload replace the old part names too.
	ITEMOPTION_PARTNAME.swap(pending.ITEMOPTION_PARTNAME);
	ITEMOPTION_PARTENAME.swap(pending.ITEMOPTION_PARTENAME);
	std::swap(m_Size, pending.m_Size);
	std::swap(m_pTypeInfo, pending.m_pTypeInfo);
}

std::string ITEMOPTION_TABLE::GetPartName(int part, char manaPrefix) const
{
	if (part < 0 || part >= MAX_PART)
		return {};
	std::string result = ITEMOPTION_PARTNAME[part];
	if (manaPrefix == 'H' || manaPrefix == 'E')
	{
		const size_t pos = result.find("MP");
		if (pos != std::string::npos)
			result[pos] = manaPrefix;
	}
	return result;
}

std::string ITEMOPTION_TABLE::GetPartEName(int part) const
{
	if (part < 0 || part >= MAX_PART)
		return {};
	return ITEMOPTION_PARTENAME[part];
}
