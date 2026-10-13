////////////////////////////////////////////////////////////////////////////////
// Filename    : GCPetStashListHandler.cpp
// Written By  : 김성민
// Description :
////////////////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCPetStashList.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "ExperienceTable.h"
#include "MItem.h"
#include "MStorage.h"
void GCPetStashListHandler::execute ( GCPetStashList * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
//	__BEGIN_DEBUG_EX
	__BEGIN_DEBUG
	
		//------------------------------------------------------------
		// The old stash goes, and a new one is made.
		//------------------------------------------------------------
		if (g_pStorage!=NULL) 
		{
			delete g_pStorage;
		}

		g_pStorage = new MStorage;

		
		g_pStorage->Init( 1 );

		//------------------------------------------------------------
		// The rack the stash is shown on.
		//------------------------------------------------------------
		g_pStorage->SetCurrent( 0 );

		std::vector<PetStashItemInfo*> vPetInfo = pPacket->getPetStashItemInfos();

		std::vector<PetStashItemInfo*>::const_iterator		itr;

		itr = vPetInfo.begin();
		PetInfo* pPetInfo;
		PetStashItemInfo* petStashItemInfo;
		int index = 0;
		while(itr != vPetInfo.end() && index < 20)
		{
			petStashItemInfo = (PetStashItemInfo*)*itr;
			if(NULL == petStashItemInfo)
			{
				itr ++;
				index ++;
				continue;
			}
			pPetInfo = petStashItemInfo->pPetInfo;
			if(NULL == pPetInfo)
			{
				itr ++;
				index ++;
				continue;
			}
			// The factory makes an MPetItem for this class; anything else
			// (none, in a test binary without a factory) skips the slot.
			MItem* pNewItem = MItem::NewItem( ITEM_CLASS_PET_ITEM );
			MPetItem *pItem = dynamic_cast<MPetItem *>(pNewItem);
			if (pItem == NULL)
			{
				delete pNewItem;
				DEBUG_ADD_FORMAT_ERR("[Error] GCPetStashList: no pet item for slot %d", index);
				itr ++;
				index ++;
				continue;
			}
			pItem->SetItemType( pPetInfo->getPetType() );
			pItem->SetCurrentDurability( pPetInfo->getPetHP() );
			pItem->SetEnchantLevel( pPetInfo->getPetAttr() );
			pItem->SetSilver( pPetInfo->getPetAttrLevel() );
			pItem->SetNumber( pPetInfo->getPetLevel() );
			pItem->SetPetFoodType( pPetInfo->getFoodType() );
			int accum = 0;
			accum = g_pExperienceTable->GetPetExp(pPetInfo->getPetLevel()).AccumExp;
			pItem->SetPetExpRemain(accum-pPetInfo->getPetExp());
			pItem->SetPetCutHead( pPetInfo->canCutHead()?true:false );
			pItem->SetPetGamble( pPetInfo->canGamble()?true:false );
			pItem->SetPetAttack( pPetInfo->canAttack()?true:false );
			
			pItem->ClearItemOption();
			if(pPetInfo->getPetOption() != 0)
			{
				pItem->AddItemOption(pPetInfo->getPetOption());
			}

			pItem->SetID( pPetInfo->getPetItemObjectID());
			pItem->SetItemType( pPetInfo->getPetType() );
			
			pItem->SetPetKeepedDay(petStashItemInfo->KeepDays);
			//------------------------------------------------------------
			// Put the pet in its slot
			//------------------------------------------------------------
			if (!g_pStorage->SetItem( index, pItem ))
			{
				// The slot refused it.
				delete pItem;

				DEBUG_ADD_FORMAT_ERR("[Error] Can't Add Item to Storage. slot=%d", index);
			}

			itr ++;
			index ++;
		}

		//------------------------------------------------------------
		// Open the pet storage window.
		//------------------------------------------------------------
		DialogPresentation::RunPetStorage();
		DialogPresentation::SetPetStorage(g_pStorage);

	__END_DEBUG
//	__END_DEBUG_EX 
	__END_CATCH
}
