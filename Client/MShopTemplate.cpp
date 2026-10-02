//-----------------------------------------------------------------------------
// MShopTemplateTable.cpp
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "MShopTemplateTable.h"

#include <limits>
#include <memory>

static_assert(sizeof(int) == 4 && sizeof(unsigned int) == 4
	&& sizeof(WORD) == 2 && sizeof(BYTE) == 1);

//-----------------------------------------------------------------------------
// global
//-----------------------------------------------------------------------------
MShopTemplateTable*		g_pShopTemplateTable = NULL;

//-----------------------------------------------------------------------------
//
// constructor / destructor
//
//-----------------------------------------------------------------------------
MShopTemplate::MShopTemplate()
	: Type(0), Class(0), MinType(0), MaxType(0), MinOption(0), MaxOption(0)
{
}

MShopTemplate::~MShopTemplate()
{
}


//-----------------------------------------------------------------------------
// Save To File
//-----------------------------------------------------------------------------
void		
MShopTemplate::SaveToFile(std::ofstream& file)
{
	//file.write((const char*)&m_ID,        szShopTemplateID);
	file.write((const char*)&Type,      1);
	file.write((const char*)&Class,     4);
	file.write((const char*)&MinType,   2);
	file.write((const char*)&MaxType,   2);
	file.write((const char*)&MinOption, 1);
	file.write((const char*)&MaxOption, 1);
}

//-----------------------------------------------------------------------------
// Load From File
//-----------------------------------------------------------------------------
void		
MShopTemplate::LoadFromFile(std::ifstream& file)
{
	MShopTemplate loaded;
	file.read(reinterpret_cast<char*>(&loaded.Type),      1);
	file.read(reinterpret_cast<char*>(&loaded.Class),     4);
	file.read(reinterpret_cast<char*>(&loaded.MinType),   2);
	file.read(reinterpret_cast<char*>(&loaded.MaxType),   2);
	file.read(reinterpret_cast<char*>(&loaded.MinOption), 1);
	file.read(reinterpret_cast<char*>(&loaded.MaxOption), 1);
	if (file) *this = loaded;
}


//-----------------------------------------------------------------------------
//
// MShopTemplateTable
//
//-----------------------------------------------------------------------------
MShopTemplateTable::MShopTemplateTable()
{
}

MShopTemplateTable::~MShopTemplateTable()
{
}

void MShopTemplateTable::LoadFromFile(std::ifstream& file)
{
	Release();
	int count = 0;
	if (!file.read(reinterpret_cast<char*>(&count), 4) || count < 0)
	{
		file.setstate(std::ios::failbit);
		return;
	}

	// Each key and row consumes exactly fifteen bytes. Reject impossible
	// counts before allocating, including incomplete keys and row bodies.
	const std::streamoff start = file.tellg();
	if (start < 0)
	{
		file.setstate(std::ios::failbit);
		return;
	}
	file.seekg(0, std::ios::end);
	const std::streamoff end = file.tellg();
	if (!file.good() || end < start)
	{
		file.setstate(std::ios::failbit);
		return;
	}
	file.seekg(start, std::ios::beg);
	if (!file.good() || count > (end - start) / 15)
	{
		file.setstate(std::ios::failbit);
		return;
	}

	MShopTemplateTable loaded;
	for (int index = 0; index < count; ++index)
	{
		unsigned int id = 0;
		if (!file.read(reinterpret_cast<char*>(&id), 4)) return;
		auto row = std::make_unique<MShopTemplate>();
		row->LoadFromFile(file);
		if (!file) return;
		if (loaded.AddData(id, row.get())) row.release();
	}
	swap(loaded);
}

void MShopTemplateTable::SaveToFile(std::ofstream& file)
{
	if (size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
	{
		file.setstate(std::ios::failbit);
		return;
	}
	for (const auto& entry : *this)
	{
		if (!entry.second)
		{
			file.setstate(std::ios::failbit);
			return;
		}
	}
	const int count = static_cast<int>(size());
	file.write(reinterpret_cast<const char*>(&count), 4);
	for (const auto& entry : *this)
	{
		file.write(reinterpret_cast<const char*>(&entry.first), 4);
		entry.second->SaveToFile(file);
	}
}

