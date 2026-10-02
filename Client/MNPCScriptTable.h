//---------------------------------------------------------------------------
// MNPCScriptTable.h
//---------------------------------------------------------------------------
/*
	int TotalNumberofScript; // 총 스크립트의 수
	{
		int    ScriptID; 
		int    OwnerIDSize;
		string OwnerID;

		int TotalNumberOfSubject;
		{
			int    SubjectSize;
			string Subject;
		}

		int TotalNumberOfContent;
		{
			int    ContentSize;
			string Content;
		}
	}

	{} 사이는 반복
*/
//---------------------------------------------------------------------------

#ifndef __MNPCSCRIPTTABLE_H__
#define	__MNPCSCRIPTTABLE_H__

#include "CTypeTable.h"
#include "CTypeMap2.h"
#include "MString.h"
#include "ScriptParameter.h"

typedef std::map<std::string,ScriptParameter*>			HashMapScriptParameter;


//---------------------------------------------------------------------------
// NPC_SCRIPT
//---------------------------------------------------------------------------
class NPC_SCRIPT {
	public :
		typedef CTypeTable<MString>		STRING_TABLE;
		NPC_SCRIPT() = default;
		NPC_SCRIPT(const NPC_SCRIPT&) = delete;
		NPC_SCRIPT& operator=(const NPC_SCRIPT&) = delete;

	public :		// public public ㅋㅋ~~
		MString				OwnerID;

		STRING_TABLE		SubjectTable;	
		STRING_TABLE		ContentTable;

	public :
		int					GetSubjectSize() const	{ return SubjectTable.GetSize(); }
		int					GetContentSize() const	{ return ContentTable.GetSize(); }

		void				SaveToFile(std::ofstream& file);
		void				LoadFromFile(std::ifstream& file);
};

//---------------------------------------------------------------------------
// MNPCScriptTable
//---------------------------------------------------------------------------
class MNPCScriptTable : public CTypeMap2<NPC_SCRIPT> {
	public :
		MNPCScriptTable() = default;
		MNPCScriptTable(const MNPCScriptTable&) = delete;
		MNPCScriptTable& operator=(const MNPCScriptTable&) = delete;

		// Saves validate all text before writing to the supplied stream.
		// Reloads clear old scripts and publish only complete tables.
		void		SaveToFile(std::ofstream& file);
		void		LoadFromFile(std::ifstream& file);

		int			GetSubjectSize(int scriptID) const;
		int			GetContentSize(int scriptID) const;
	
		const char*	GetSubject(int scriptID, int subjectID) const;
		const char*	GetContent(int scriptID, int contentID) const;

		// 해당 스크립트 아이디에 파라미터값을 replace 한다.
		// Missing text clears str; null parameters leave their markers intact.
		// Keys run in map order, without rescanning a key's own replacement.
		void GetContentParameter(int scriptID, int contentID,HashMapScriptParameter para,std::string &str);
		void GetSubjectParameter(int scriptID, int subjectID,HashMapScriptParameter para,std::string &str);
};


//---------------------------------------------------------------------------
// global
//---------------------------------------------------------------------------
extern MNPCScriptTable*		g_pNPCScriptTable;

#endif
