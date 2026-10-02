//---------------------------------------------------------------------------
// MNPCScriptTable.cpp
//---------------------------------------------------------------------------

#include "Client_PCH.h"
#include "MNPCScriptTable.h"

namespace {

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
	//file.write((const char*)&ScriptID, 4);
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
	//file.read((char*)&ScriptID, 4);
	OwnerID.LoadFromFile( file );

	SubjectTable.LoadFromFile( file );
	ContentTable.LoadFromFile( file );
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
	TYPE_MAP::iterator iData = begin();

	//-----------------------------------------------------
	// 개수 저장
	//-----------------------------------------------------
	int infoSize = static_cast<int>(size());
	file.write((const char*)&infoSize, 4);

	//-----------------------------------------------------
	// 각 info 저장
	//-----------------------------------------------------
	while (iData != end())
	{
		unsigned int	id		= (*iData).first;
		NPC_SCRIPT*		pData	= (*iData).second;		

		file.write((const char*)&id, 4);	// id 저장
		pData->SaveToFile( file );			// NPC info 저장

		iData ++;
	}
}
		
//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
void		
MNPCScriptTable::LoadFromFile(std::ifstream& file)
{
	//-----------------------------------------------------
	// 기존에 있던것 제거
	//-----------------------------------------------------
	Release();

	//-----------------------------------------------------
	// size 
	//-----------------------------------------------------
	int infoSize;
	file.read((char*)&infoSize, 4);

	//-----------------------------------------------------
	// 각 info
	//-----------------------------------------------------
	unsigned int id;
	for (int i=0; i<infoSize; i++)
	{		
		file.read((char*)&id, 4);
		NPC_SCRIPT*	pData = new NPC_SCRIPT;

		pData->LoadFromFile( file );

		//-----------------------------------------------------
		// map에 추가한다.
		//-----------------------------------------------------
		if (!AddData( id, pData ))
		{
			delete pData;
		}
	}
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
