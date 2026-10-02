#include "Client_PCH.h"
#include "MActionResult.h"

//----------------------------------------------------------------------
// 
// MActionResult :: constructor/destructor
//
//----------------------------------------------------------------------
MActionResult::MActionResult()
{
}

MActionResult::~MActionResult()
{
	ACTIONRESULTNODE_LIST::iterator	iNode = m_List.begin();

	// 모든 node를 delete해준다.
	while (iNode != m_List.end())
	{
		MActionResultNode* pResultNode = *iNode;
		
		// 결과 실행..
		//pResultNode->Execute();

		delete pResultNode;
		
		iNode++;
	}

	m_List.clear();
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
MActionResult::Release()
{
	ACTIONRESULTNODE_LIST::iterator	iNode = m_List.begin();

	// 모든 node를 delete해준다.
	while (iNode != m_List.end())
	{
		MActionResultNode* pResultNode = *iNode;
		
		delete pResultNode;
		
		iNode++;
	}

	m_List.clear();
}

//----------------------------------------------------------------------
// Add : 결과 하나를 추가한다.
//----------------------------------------------------------------------
void		
MActionResult::Add(MActionResultNode* pNode)
{
	if (pNode==NULL)
		return;

	// list에 추가
	m_List.push_back( pNode );
}

//----------------------------------------------------------------------
// ExecuteResult
//----------------------------------------------------------------------
// ActionInfo(Effect)에 따른 결과를 실행한다.
//----------------------------------------------------------------------
void
MActionResult::Execute()
{
	//------------------------------------------------
	// 모두~~ 처리한다.
	//------------------------------------------------
	while (!m_List.empty())
	{	
		MActionResultNode* pResultNode = m_List.front();
		
		m_List.pop_front();

		if (pResultNode!=NULL)
		{
			pResultNode->Execute();

			delete pResultNode;
		}
	}
}

