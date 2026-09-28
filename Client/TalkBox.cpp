//--------------------------------------------------------------------------
// TalkBox.cpp
//--------------------------------------------------------------------------
#include "Client_PCH.h"
#include "TalkBox.h"
#include "MStringList.h"

//--------------------------------------------------------------------------
// global
//--------------------------------------------------------------------------
MStringList	*	g_pNPCTalkBox = NULL;

PCTalkBox	*	g_pPCTalkBox = NULL;

//--------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------
PCTalkBox::PCTalkBox()
{ 
	m_Type		= NORMAL;
	m_NPCID		= 0;
	m_CreatureType = 0;
	m_ScriptID	= 0; 
	m_AnswerID	= 0;
}

//--------------------------------------------------------------------------
// Map Menu Answer
//--------------------------------------------------------------------------
// menuID is the 1-based exec id of the chosen menu line. m_AnswerIDMap
// holds the 0-based answer index of each line, so line menuID answers
// m_AnswerIDMap[menuID-1] + 1. An id past the end of the map is returned
// unchanged.
//--------------------------------------------------------------------------
int
PCTalkBox::MapMenuAnswer(int menuID) const
{
	if( m_AnswerIDMap.size() >= static_cast<size_t>(menuID) )
		return m_AnswerIDMap[menuID-1] + 1;

	return menuID;
}