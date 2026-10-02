//---------------------------------------------------------------------------
// MNPCScriptTable.cpp
//---------------------------------------------------------------------------

#include "Client_PCH.h"
#include "MNPCScriptTable.h"
#include "TextEncoding.h"

#include <limits>
#include <memory>

namespace {

static_assert(sizeof(int) == 4 && sizeof(unsigned int) == 4);

bool ReadCount(std::ifstream& file, int& count, std::streamoff minimumBytes)
{
	count = 0;
	if (!file.read(reinterpret_cast<char*>(&count), 4) || count < 0)
	{
		file.setstate(std::ios::failbit);
		return false;
	}
	const std::streamoff start = file.tellg();
	if (start < 0)
	{
		file.setstate(std::ios::failbit);
		return false;
	}
	file.seekg(0, std::ios::end);
	const std::streamoff end = file.tellg();
	if (!file.good() || end < start)
	{
		file.setstate(std::ios::failbit);
		return false;
	}
	file.seekg(start, std::ios::beg);
	if (!file.good() || count > (end - start) / minimumBytes)
	{
		file.setstate(std::ios::failbit);
		return false;
	}
	return true;
}

bool ReadStrings(std::ifstream& file, NPC_SCRIPT::STRING_TABLE& strings)
{
	int count = 0;
	if (!ReadCount(file, count, 4)) return false;
	if (strings.GetSize() != count) strings.Init(count);
	for (int index = 0; index < count; ++index)
	{
		strings.GetMutable(index)->LoadFromFile(file);
		if (!file) return false;
	}
	return true;
}

bool CanSaveText(const MString& text)
{
	std::string encoded;
	return TextEncoding::Convert(text.GetString(), text.GetLength(),
		TextEncoding::Encoding::Utf8, TextEncoding::GetResourceEncoding(), encoded)
		&& encoded.size() <= 65536u;
}

bool CanSaveStrings(const NPC_SCRIPT::STRING_TABLE& strings)
{
	for (int index = 0; index < strings.GetSize(); ++index)
		if (!CanSaveText(strings[index])) return false;
	return true;
}

bool CanSave(const NPC_SCRIPT& script)
{
	return CanSaveText(script.OwnerID) && CanSaveStrings(script.SubjectTable)
		&& CanSaveStrings(script.ContentTable);
}

void ApplyParameters(const char* source, const HashMapScriptParameter& parameters,
	std::string& output)
{
	output = source ? source : "";
	for (const auto& entry : parameters)
	{
		if (!entry.second) continue;
		const std::string key = "%(" + entry.first + ")";
		const std::string value = entry.second->getValue();
		std::string::size_type position = 0;
		while ((position = output.find(key, position)) != std::string::npos)
		{
			output.replace(position, key.size(), value);
			// Do not revisit placeholders inserted by this key. Later keys
			// still run in map order, preserving the existing cascading rule.
			position += value.size();
		}
	}
}

} // namespace

//---------------------------------------------------------------------------
// global
//---------------------------------------------------------------------------
MNPCScriptTable*		g_pNPCScriptTable = NULL;

//---------------------------------------------------------------------------
// 
// constructor / destructor
//
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//
// member functions
//
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Save To File
//---------------------------------------------------------------------------
void				
NPC_SCRIPT::SaveToFile(std::ofstream& file)
{
	if (!CanSave(*this))
	{
		file.setstate(std::ios::failbit);
		return;
	}
	OwnerID.SaveToFile( file );

	SubjectTable.SaveToFile( file );
	ContentTable.SaveToFile( file );
}

//---------------------------------------------------------------------------
// Load From File
//---------------------------------------------------------------------------
void				
NPC_SCRIPT::LoadFromFile(std::ifstream& file)
{
	// The table loader owns this staging row until every group is complete.
	OwnerID.LoadFromFile( file );
	if (!file || !ReadStrings(file, SubjectTable)) return;
	ReadStrings(file, ContentTable);
}


//---------------------------------------------------------------------------
//
// MNPCScriptTable
//
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Get Subject Size
//---------------------------------------------------------------------------
int			
MNPCScriptTable::GetSubjectSize(int scriptID) const
{
	const NPC_SCRIPT* pScript = GetData( scriptID );
	
	if (pScript==NULL)
	{
		return 0;
	}

	return pScript->SubjectTable.GetSize();
}

//---------------------------------------------------------------------------
// Get Content Size
//---------------------------------------------------------------------------
int			
MNPCScriptTable::GetContentSize(int scriptID) const
{
	const NPC_SCRIPT* pScript = GetData( scriptID );

	if (pScript==NULL)
	{
		return 0;
	}

	return pScript->ContentTable.GetSize();
}

//---------------------------------------------------------------------------
// Get Subject
//---------------------------------------------------------------------------
const char*	
MNPCScriptTable::GetSubject(int scriptID, int subjectID) const
{
	const NPC_SCRIPT* pScript = GetData( scriptID );

	if (pScript==NULL)
	{
		return NULL;
	}

	if (subjectID >= pScript->GetSubjectSize())
	{
		return NULL;
	}

	return pScript->SubjectTable[subjectID].GetString();
}

//---------------------------------------------------------------------------
// Get Content
//---------------------------------------------------------------------------
const char*	
MNPCScriptTable::GetContent(int scriptID, int contentID) const
{
	const NPC_SCRIPT* pScript = GetData( scriptID );

	if (pScript==NULL)
	{
		return NULL;
	}

	if (contentID >= pScript->GetContentSize())
	{
		return NULL;
	}

	return pScript->ContentTable[contentID].GetString();
}


//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
void		
MNPCScriptTable::SaveToFile(std::ofstream& file)
{
	if (size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
	{
		file.setstate(std::ios::failbit);
		return;
	}
	for (const auto& entry : *this)
	{
		if (!entry.second || !CanSave(*entry.second))
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
		
//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
void		
MNPCScriptTable::LoadFromFile(std::ifstream& file)
{
	Release();
	int count = 0;
	// Key, owner length and both group counts cost at least sixteen bytes.
	if (!ReadCount(file, count, 16)) return;
	MNPCScriptTable loaded;
	for (int index = 0; index < count; ++index)
	{
		unsigned int id = 0;
		if (!file.read(reinterpret_cast<char*>(&id), 4)) return;
		auto row = std::make_unique<NPC_SCRIPT>();
		row->LoadFromFile(file);
		if (!file) return;
		if (loaded.AddData(id, row.get())) row.release();
	}
	swap(loaded);
}

void
MNPCScriptTable::GetContentParameter(int scriptID, int contentID, HashMapScriptParameter para, std::string& str)
{
	ApplyParameters(GetContent(scriptID, contentID), para, str);
}

void
MNPCScriptTable::GetSubjectParameter(int scriptID, int subjectID, HashMapScriptParameter para, std::string& str)
{
	ApplyParameters(GetSubject(scriptID, subjectID), para, str);
}
