//////////////////////////////////////////////////////////////////////
//
// Filename    : GCGQuestStatusInfoHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCGQuestStatusInfo.h"
#include "DebugLog.h"
#include "VsUiPresentationHost.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void GCGQuestStatusInfoHandler::execute ( GCGQuestStatusInfo * pGCGQuestStatusInfo , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;
//	__BEGIN_DEBUG_EX
		
	if(VsUiPresentation::IsRunningQuestManager()) 
		VsUiPresentation::CloseQuestManager();
	VsUiPresentation::RunQuestManager(); // ui setting - load xml file

	std::list<QuestStatusInfo*>	Infos = pGCGQuestStatusInfo->getInfos();
	std::list<QuestStatusInfo*>::iterator itr = Infos.begin(); 

	int MaxSize = static_cast<int>(Infos.size());
//#ifdef __DEBUG_OUTPUT__
	DEBUG_ADD_FORMAT("[Receive GQuestStatusInfo] MaxSize = %d", MaxSize); 
//#endif

	//while(itr != Infos.end())
	for(int TempCount = 0; TempCount<MaxSize; TempCount++)
	{
		QuestStatusInfo* QuestInfo = (*itr);

		UI_GQuestInfo *UI_Info = new UI_GQuestInfo; 

		UI_Info->dwQuestID	= QuestInfo->GetQuestID();
		UI_Info->bStatus	= QuestInfo->GetStatus();

		std::list<MissionInfo> TempMissionInfo = QuestInfo->GetMission();
		std::list<MissionInfo>::iterator subitr = TempMissionInfo.begin();

		while(subitr != TempMissionInfo.end())
		{
			MissionInfo TempMission = (*subitr);

			UI_GMissionInfo* UI_Mission = new UI_GMissionInfo;
			UI_Mission->bCondition = TempMission.m_Condition;
			UI_Mission->bStatus = TempMission.m_Status;
			UI_Mission->bIndex = TempMission.m_Index;
			UI_Mission->m_StrArg = TempMission.m_StrArg;
			UI_Mission->m_NumArg = TempMission.m_NumArg;
			UI_Mission->bTimeLimited = false;

			UI_Info->vMissionList.push_back(UI_Mission);

			subitr++;
		}

		if(false == VsUiPresentation::SetQuestManagerInfo(UI_Info))
		{
			// Nobody kept the record: free its missions, then the record. (The
			// loop here erased each mission and went on with the erased
			// iterator, which a checked build aborts on.)
			for (UI_GMissionInfo* pMission : UI_Info->vMissionList)
			{
				delete pMission;
			}
			delete UI_Info;
		}
		itr++;
	}

	VsUiPresentation::ReleaseQuestXML(); // Release xml file

	//__END_DEBUG_EX 
	__END_CATCH
}
