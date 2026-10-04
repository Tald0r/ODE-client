//--------------------------------------------------------------------------------
//
// Filename    : GCRemoveCorpseHeadHandler.cpp
// Written By  : Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRemoveCorpseHead.h"
#include "ClientDef.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCRemoveCorpseHeadHandler::execute ( GCRemoveCorpseHead * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;


	int objectID = pPacket->getObjectID();

	// Creature를 생성해서 MCorpse에 추가해서 Zone에 넣는다.
	//------------------------------------------------------
	// Zone이 아직 생성되지 않은 경우
	//------------------------------------------------------
	if (g_pZone==NULL)
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	}
	//------------------------------------------------------
	// 정상.. 
	//------------------------------------------------------
	else
	{	
		//----------------------------------------	
		// 이미 있는 Creature인가?
		//----------------------------------------	
		MCreature* pCreature = g_pZone->GetCreature( objectID );
		
		//---------------------------------------------------------
		//
		//					Zone에 없는 경우
		//
		//---------------------------------------------------------
		if (pCreature==NULL)
		{
			// 이미 시체가 있나?
			MItem* pItem = g_pZone->GetItem( objectID );

			//---------------------------------------------------------
			// 시체도 없으면 에러당
			//---------------------------------------------------------
			if (pItem==NULL)
			{
				// 그런 아이템은 없다.
				DEBUG_ADD_ERR("[Error] There is no such Item ID");
			}
			//---------------------------------------------------------
			// 시체가 있으면 머리를 없앤다.
			//---------------------------------------------------------
			else if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
			{
				MCreature* pCreature = ((MCorpse*)pItem)->GetCreature();

				pCreature->RemoveHead();
			}
			else
			{
				// 아이템은 있는데 시체가 아닌 경우
				DEBUG_ADD_FORMAT_ERR("[Error] It's not Corpse. id=%d", objectID);
			}
		}
		//---------------------------------------------------------
		// 아직 몹인 상태인 경우.. 머리 없애기
		//---------------------------------------------------------
		else
		{
			pCreature->RemoveHead();			
		}
	}


	__END_CATCH
}
