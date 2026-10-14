//----------------------------------------------------------------------
// MActionResult.cpp
//----------------------------------------------------------------------
// The results the server confirmed, applied to the zone's creatures, the
// player and the inventory through the hosts the executable installs.
#include "Client_PCH.h"
#include "MActionResult.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "MCreatureTable.h"
#include "MInventory.h"
#include "MItemOptionTable.h"
#include "MorphState.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "SoundDef.h"
#include "VsUiPresentationHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"

//----------------------------------------------------------------------
// 
// ActionResultNode :: constructor/destructor
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
//
// ActionResultNode - ActionInfo
//
//----------------------------------------------------------------------
MActionResultNodeActionInfo::MActionResultNodeActionInfo(TYPE_ACTIONINFO nActionInfo, TYPE_OBJECTID uid, TYPE_OBJECTID tid, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, DWORD delayFrame, BYTE temp)
{ 
	m_UserID		= uid;
	m_TargetID		= tid;
	m_nActionInfo	= nActionInfo;
	m_DelayFrame	= delayFrame;

	m_sX			= sX;
	m_sY			= sY;

	m_Temp			= temp;
}

//----------------------------------------------------------------------
// ActionResultNode - ActionInfo::Execute
//----------------------------------------------------------------------
void
MActionResultNodeActionInfo::Execute()
{
	DEBUG_ADD_FORMAT("ActionResultNodeAIExecute. u=%d, t=%d", m_UserID, m_TargetID);
	//POINT point;
	if (!ZoneState::Exists())
	{
		return;
	}


	//--------------------------------------------------------
	// The user and the target.
	//--------------------------------------------------------
	MCreature*	pTargetCreature = ZoneState::GetCreature( m_TargetID );
	MCreature*	pUserCreature	= ZoneState::GetCreature( m_UserID );

	// No creature, nothing to do.
	if (pTargetCreature==NULL)// || pUserCreature==NULL)
	{
		
		return;		
	}

	TYPE_ACTIONINFO nUsedActionInfo = m_nActionInfo;

	//--------------------------------------------------------
	// The target acts out the result.
	//--------------------------------------------------------

	int resultActionInfo;
	resultActionInfo = nUsedActionInfo + (*g_pActionInfoTable).GetMinResultActionInfo();	// the result

	switch( nUsedActionInfo )
	{
	case SKILL_SHADOW_DANCING :
		resultActionInfo = SKILL_SINGLE_BLOW + (*g_pActionInfoTable).GetMinResultActionInfo();
		break;
	// add by Coffee 2007-4-15
//	case SKILL_ILLUSION_INVERSION:
		//resultActionInfo = RESULT_TEMP_SKILL_ILLUSION_INVERSION;
		//break;
	// end by Coffee
	case SKILL_ILLENDUE :

		resultActionInfo = SKILL_LIGHT_BALL + (*g_pActionInfoTable).GetMinResultActionInfo();
		break;

	case SKILL_BLAZE_WALK :
		resultActionInfo = SKILL_BLAZE_WALK_ATTACK + (*g_pActionInfoTable).GetMinResultActionInfo();
		break;
	}
	
	if( resultActionInfo >= g_pActionInfoTable->GetSize() )
	{
		DEBUG_ADD_FORMAT("[Execute-ResultActionInfo] exceed resultAction %d/%d",nUsedActionInfo,
			resultActionInfo);
		return;
	}
	
	#ifdef OUTPUT_DEBUG
		DEBUG_ADD_FORMAT("[Execute-ResultActionInfo] ID=%d, ai=%d", m_TargetID, resultActionInfo);
	#endif


	
	CreatureState::SetEffectDelayFrame(pTargetCreature, resultActionInfo, m_DelayFrame );

 	if( pUserCreature != NULL)
 	{
		if(nUsedActionInfo == MAGIC_BLOODY_ZENITH)
			CreatureState::SetBloodyZenith(pUserCreature, 8);
 	}
	
	if(nUsedActionInfo == SKILL_BLITZ_SLIDING_ATTACK )
	{
		if( pUserCreature != NULL )
		{
			CreatureState::PacketSpecialActionResult(pUserCreature, resultActionInfo, m_TargetID, m_sX, m_sY );
		}
	}
	else
	{
		if( (*g_pActionInfoTable)[nUsedActionInfo].IsUseActionGrade() && m_Temp != 0 )
			CreatureState::PacketSpecialActionResult(pTargetCreature, resultActionInfo, m_TargetID, m_sX, m_sY, m_Temp );
		else
			CreatureState::PacketSpecialActionResult(pTargetCreature, resultActionInfo, m_TargetID, m_sX, m_sY);
	}
	

}


//----------------------------------------------------------------------
//
// ActionResultNode - ChangePosition
//
//----------------------------------------------------------------------
MActionResultNodeChangePosition::MActionResultNodeChangePosition(TYPE_OBJECTID uid, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{ 
	m_UserID	= uid;
	m_X			= sX;
	m_Y			= sY;
}

//----------------------------------------------------------------------
// ActionResultNode - ChangePosition::Execute
//----------------------------------------------------------------------
void
MActionResultNodeChangePosition::Execute()
{
	if (PlayerState::Creature()==NULL
		|| !ZoneState::Exists())
	{
		return;
	}

	//-----------------------------------------------------------
	// The player
	//-----------------------------------------------------------
	if (m_UserID==PlayerState::GetID())
	{
		CreatureState::MovePosition( PlayerState::Creature(), m_X, m_Y );
		PlayerState::SetServerPosition( m_X, m_Y );
	}
	//-----------------------------------------------------------
	// Any other creature
	//-----------------------------------------------------------
	else
	{
		MCreature* pCreature = ZoneState::GetCreature( m_UserID );


		if (pCreature!=NULL)
		{
			//------------------------------------------------
			// Drop its pending moves.
			//------------------------------------------------
			CreatureState::ReleaseMoveBuffer(pCreature);

			// Move it.
			CreatureState::MovePosition(pCreature, m_X, m_Y );
			CreatureState::SetServerPosition(pCreature, m_X, m_Y );
		}
	}
}

//----------------------------------------------------------------------
//
// ActionResultNode - ChangeStatus
//
//----------------------------------------------------------------------
MActionResultNodeChangeStatus::MActionResultNodeChangeStatus(TYPE_OBJECTID tid, MStatus* pStatus)
{
	m_TargetID = tid;
	m_pStatus = pStatus;
}

MActionResultNodeChangeStatus::~MActionResultNodeChangeStatus()
{
	if (m_pStatus != NULL)
	{
		delete m_pStatus;
	}
		
}

//----------------------------------------------------------------------
// ActionResultNode - ChangeStatus::Execute
//----------------------------------------------------------------------
void
MActionResultNodeChangeStatus::Execute()
{
	if (PlayerState::Creature()==NULL)
	{
		return;
	}

	//---------------------------------------------------------
	// Only the player has a status here, so the target can only be the
	// player. 
	//---------------------------------------------------------
	if (m_TargetID==PlayerState::GetID()
		&& m_pStatus!=NULL)
	{
		if (MStatus* pPlayerStatus = PlayerState::Status())
			pPlayerStatus->ApplyStatus( *m_pStatus );		
	}
}

//----------------------------------------------------------------------
// ActionResultNodeCreatureDie - CreatureDie::Execute
//----------------------------------------------------------------------
void
MActionResultNodeCreatureDie::Execute()
{
	if (!ZoneState::Exists()
		|| PlayerState::Creature()==NULL)
	{
		return;
	}

	//---------------------------------------------------------
	// The player
	//---------------------------------------------------------
	if (m_TargetID == PlayerState::GetID())
	{
		CreatureState::SetDead( PlayerState::Creature() );
	}
	//---------------------------------------------------------
	// Any other creature
	//---------------------------------------------------------
	else
	{
		MCreature*	pCreature = ZoneState::GetCreature( m_TargetID );

		//---------------------------------------------------------
		// No creature, nothing to do.
		//---------------------------------------------------------
		if (pCreature==NULL)
		{
			return;
		}
		
		//---------------------------------------------------------
		// Kill it.
		//---------------------------------------------------------		
		CreatureState::SetDead(pCreature);	
	}
}

//----------------------------------------------------------------------
// ActionResultNodeCreatureBurrow - CreatureBurrow::Execute
//----------------------------------------------------------------------
void
MActionResultNodeCreatureBurrow::Execute()
{
	if (PlayerState::Creature()==NULL
		|| !ZoneState::Exists())
	{
		return;
	}

	//---------------------------------------------------------
	// The player
	//---------------------------------------------------------
	if (m_TargetID == PlayerState::GetID())
	{
		// Make it an underground creature.
		PlayerState::SetUndergroundCreature();
	}
	//---------------------------------------------------------
	// Any other creature
	//---------------------------------------------------------
	else
	{
		MCreature*	pCreature = ZoneState::GetCreature( m_TargetID );

		//---------------------------------------------------------
		// No creature, nothing to do.
		//---------------------------------------------------------
		if (pCreature==NULL)
		{
			return;
		}
		
		//---------------------------------------------------------
		// Bury it.
		//---------------------------------------------------------
		CreatureState::SetUndergroundCreature(pCreature);
	}
}

//----------------------------------------------------------------------
// ActionResultNodeCreatureInvisible - CreatureInvisible::Execute
//----------------------------------------------------------------------
void
MActionResultNodeCreatureInvisible::Execute()
{
	if (!ZoneState::Exists()
		|| PlayerState::Creature()==NULL)
	{
		return;
	}

	//---------------------------------------------------------
	// The player
	//---------------------------------------------------------
	if (m_TargetID == PlayerState::GetID())
	{
		// Hide it.
		CreatureState::SetInvisible( PlayerState::Creature() );
	}
	//---------------------------------------------------------
	// Any other creature
	//---------------------------------------------------------
	else
	{
		MCreature*	pCreature = ZoneState::GetCreature( m_TargetID );

		//---------------------------------------------------------
		// No creature, nothing to do.
		//---------------------------------------------------------
		if (pCreature==NULL)
		{
			return;
		}
		
		//---------------------------------------------------------
		// invisible
		//---------------------------------------------------------		
		CreatureState::SetInvisible(pCreature);
	}
}

//----------------------------------------------------------------------
// ActionResultNodeCreatureVisible - CreatureVisible::Execute
//----------------------------------------------------------------------
void
MActionResultNodeCreatureVisible::Execute()
{
	if (!ZoneState::Exists()
		|| PlayerState::Creature()==NULL)
	{
		return;
	}

	//---------------------------------------------------------
	// The player
	//---------------------------------------------------------
	if (m_TargetID == PlayerState::GetID())
	{
		// Show it.
		CreatureState::SetVisible( PlayerState::Creature() );
	}
	//---------------------------------------------------------
	// Any other creature
	//---------------------------------------------------------
	else
	{
		MCreature*	pCreature = ZoneState::GetCreature( m_TargetID );

		//---------------------------------------------------------
		// No creature, nothing to do.
		//---------------------------------------------------------
		if (pCreature==NULL)
		{
			return;
		}
		
		//---------------------------------------------------------
		// invisible
		//---------------------------------------------------------		
		CreatureState::SetVisible(pCreature);
	}
}

//----------------------------------------------------------------------
// ActionResultNodeCreatureSnipping - CreatureSnipping::Execute
//----------------------------------------------------------------------
void
MActionResultNodeCreatureSnipping::Execute()
{
	#ifdef OUTPUT_DEBUG
		DEBUG_ADD_FORMAT("[Execute-CreatureSnipping] ID=%d", m_TargetID);
	#endif

	if (!ZoneState::Exists()
		|| PlayerState::Creature()==NULL)
	{
		return;
	}

	//---------------------------------------------------------
	// The player
	//---------------------------------------------------------
	if (m_TargetID == PlayerState::GetID())
	{		
		CreatureState::AddEffectStatus( PlayerState::Creature(), EFFECTSTATUS_SNIPPING_MODE, 0xFFFF );
		CreatureState::SetInvisible( PlayerState::Creature() );
	}
	//---------------------------------------------------------
	// Any other creature
	//---------------------------------------------------------
	else
	{
		MCreature*	pCreature = ZoneState::GetCreature( m_TargetID );

		//---------------------------------------------------------
		// No creature, nothing to do.
		//---------------------------------------------------------
		if (pCreature==NULL)
		{
			return;
		}
		
		//---------------------------------------------------------
		// invisible
		//---------------------------------------------------------		
		CreatureState::AddEffectStatus(pCreature, EFFECTSTATUS_SNIPPING_MODE, 0xFFFF );
		CreatureState::SetInvisible(pCreature);
	}
}

//----------------------------------------------------------------------
// ActionResultNodeCreatureTurning - CreatureTurning::Execute
//----------------------------------------------------------------------
void
MActionResultNodeCreatureTurning::Execute()
{
	if (!ZoneState::Exists()
		|| PlayerState::Creature()==NULL)
	{
		return;
	}

	//---------------------------------------------------------
	// The player
	//---------------------------------------------------------
	if (m_TargetID == PlayerState::GetID())
	{
		// Turn it.
		CreatureState::SetTurning( PlayerState::Creature(), m_TurnFrame );
	}
	//---------------------------------------------------------
	// Any other creature
	//---------------------------------------------------------
	else
	{
		MCreature*	pCreature = ZoneState::GetCreature( m_TargetID );

		//---------------------------------------------------------
		// No creature, nothing to do.
		//---------------------------------------------------------
		if (pCreature==NULL)
		{
			return;
		}
		
		//---------------------------------------------------------
		// invisible
		//---------------------------------------------------------		
		CreatureState::SetTurning(pCreature, m_TurnFrame );
	}
}

//----------------------------------------------------------------------
// ActionResultNodeAddEffectStatus
//----------------------------------------------------------------------
void
MActionResultNodeAddEffectStatus::Execute()
{
	if (!ZoneState::Exists())
	{
		return;
	}

	MCreature*	pTargetCreature = ZoneState::GetCreature( m_TargetID );

	// No creature, nothing to do.
	if (pTargetCreature==NULL)// || pUserCreature==NULL)
		return;

	//--------------------------------------------------------
	// Add the effect status.
	//--------------------------------------------------------
	CreatureState::AddEffectStatus(pTargetCreature, m_EffectStatus, m_DelayFrame);	
}

//----------------------------------------------------------------------
//
// ActionResultNode - ChangeCreatureType
//
//----------------------------------------------------------------------
MActionResultNodeChangeCreatureType::MActionResultNodeChangeCreatureType(TYPE_OBJECTID tid, int ctype)
{
	m_TargetID = tid;
	m_CreatureType = ctype;
}

MActionResultNodeChangeCreatureType::~MActionResultNodeChangeCreatureType()
{
}

//----------------------------------------------------------------------
// ActionResultNode - ChangeStatus::Execute
//----------------------------------------------------------------------
void
MActionResultNodeChangeCreatureType::Execute()
{
	if (!ZoneState::Exists()
		|| PlayerState::Creature()==NULL)
	{
		return;
	}

	MCreature*	pCreature = ZoneState::GetCreature( m_TargetID );
	
	// No creature, nothing to do.
	if (pCreature==NULL)
		return;


	if (CreatureState::IsUndergroundCreature(pCreature))
	{
		// A buried creature stays as it is.
	}
	else
	{
		// The gender and body colors survive the change.
		bool bMale = CreatureState::IsMale(pCreature);
		WORD cs1 = CreatureState::GetBodyColor1(pCreature);
		WORD cs2 = CreatureState::GetBodyColor2(pCreature);

		CreatureState::SetCreatureType(pCreature, m_CreatureType );

		CreatureState::SetMale(pCreature, bMale );
		if( m_CreatureType == CREATURETYPE_WER_WOLF )
			CreatureState::SetBodyColor1(pCreature, cs2 );
		else
			CreatureState::SetBodyColor1(pCreature, cs1 );

		CreatureState::SetBodyColor2(pCreature, cs2 );

		// It flies or walks as its new type does.
		if ((*g_pCreatureTable)[m_CreatureType].IsFlyingCreature())
		{
			CreatureState::SetFlyingCreature(pCreature);
		}
		else
		{
			CreatureState::SetGroundCreature(pCreature);
		}
	}

	// The player's skill in hand follows the new form.
	if (m_TargetID == PlayerState::GetID())
	{
		int selectSkill = ACTIONINFO_NULL;

		switch (m_CreatureType)
		{
			case CREATURETYPE_BAT :
				selectSkill = MAGIC_UN_TRANSFORM;
			break;
		
			case CREATURETYPE_WOLF :
				selectSkill = MAGIC_EAT_CORPSE;
			break;

			case CREATURETYPE_WER_WOLF :
				selectSkill = MAGIC_UN_TRANSFORM;
			break;
			
			// 2004-09-13: the gunner's level-130 skill.
			case CREATURETYPE_INSTALL_TURRET:
				selectSkill = MAGIC_UN_TRANSFORM;
			break;
			// 2004, 9, 13, sobeit add end
		}
		
		if (selectSkill!=ACTIONINFO_NULL)
		{
			VsUiPresentation::SelectSkill( selectSkill );
		}
		else
		{
			VsUiPresentation::UnselectSkill();
		}

		// The earlier morph is over.
//		if (g_MorphCreatureType==m_CreatureType)
		{
			g_MorphCreatureType = 0;
		}
	}
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
MActionResultNodeSummonCasket::MActionResultNodeSummonCasket(TYPE_OBJECTID tid, int casketType)
{
	m_TargetID = tid;
	m_CasketType = casketType;
}

MActionResultNodeSummonCasket::~MActionResultNodeSummonCasket()
{
}

//----------------------------------------------------------------------
// ActionResultNode - SummonCasket::Execute
//----------------------------------------------------------------------
void
MActionResultNodeSummonCasket::Execute()
{
	if (!ZoneState::Exists())
	{
		return;
	}

	MCreature*	pCreature = ZoneState::GetCreature( m_TargetID );
	
	// No creature, nothing to do.
	if (pCreature==NULL)
		return;

	//
	if (!CreatureState::IsInCasket(pCreature))
	{
		CreatureState::SetStop(pCreature);
		CreatureState::MovePosition(pCreature, CreatureState::GetServerX(pCreature), CreatureState::GetServerY(pCreature) );
		CreatureState::AddCasket(pCreature, m_CasketType );
	}
}

//----------------------------------------------------------------------
// ActionResultNode - OpenCasket::Execute
//----------------------------------------------------------------------
void
MActionResultNodeOpenCasket::Execute()
{
	if (!ZoneState::Exists())
	{
		return;
	}

	MCreature*	pCreature = ZoneState::GetCreature( m_TargetID );
	
	// No creature, nothing to do.
	if (pCreature==NULL)
		return;

	if (CreatureState::IsInCasket(pCreature))
	{
		CreatureState::RemoveCasket(pCreature);
	}
}

//----------------------------------------------------------------------
// ActionResultNode - OpenCasket::Execute
//----------------------------------------------------------------------
void
MActionResultNodeRemoveItemInInventory::Execute()
{
	if (g_pInventory==NULL)
	{
		return;
	}

	MItem* pRemovedItem = g_pInventory->RemoveItem( m_TargetID );
	delete pRemovedItem;

	MItem*	pMouseItem = NULL;

	if(PlayerState::IsItemCheckBufferItemToItem())
	{

		pMouseItem = PlayerState::GetItemCheckBuffer();

		PlayerState::ClearItemCheckBuffer();

		if(pMouseItem != NULL)
			delete pMouseItem;
	}
	
}

//----------------------------------------------------------------------
// ActionResultNode - OpenCasket::Execute
//----------------------------------------------------------------------
void
MActionResultNodeChangeItemOptionInInventory::Execute()
{
	if (g_pInventory==NULL)
	{
		return;
	}

	MItem*	pInvenItem = g_pInventory->GetItem(m_TargetID);
	MItem*	pMouseItem = NULL;

	bool bSecondPetEnchant = false;
	bool bRevivalPet = false;
	bool bRemovePetOption = false;
	bool bGradePlus = false;
	if(PlayerState::IsItemCheckBufferItemToItem())
	{

		pMouseItem = PlayerState::GetItemCheckBuffer();

		PlayerState::ClearItemCheckBuffer();

		if(pMouseItem != NULL)
		{
			if(pMouseItem->GetItemClass() == ITEM_CLASS_PET_ENCHANT_ITEM)
			{
				if(pMouseItem->GetItemType() == 13 ||
					pMouseItem->GetItemType() == 14)	// pet revival
				{
					bRevivalPet = true;
				}

				if(pMouseItem->GetItemType() == 15)
				bSecondPetEnchant = true;
			}
			if(pMouseItem->GetItemClass() == ITEM_CLASS_MIXING_ITEM &&
				pMouseItem->GetItemType() >= 9 && pMouseItem->GetItemType() <= 18)
			{
				bRemovePetOption = true;
			}

			if(pMouseItem->GetItemClass() == ITEM_CLASS_EVENT_STAR &&
				pMouseItem->GetItemType() == 23) // yellow drop
			{
				bGradePlus = true;
			}
			delete pMouseItem;
		}
	}
	
	// No item, nothing to do.
	if (pInvenItem==NULL)
		return;

	bool bSucces = false;

	if(bRemovePetOption)
	{
		pInvenItem->ClearItemOption();
		bSucces = true;
	}
	else if(bGradePlus)
	{
		pInvenItem->SetGrade(m_Type);
		bSucces = true;
	}
	else if(pInvenItem->GetItemClass() == ITEM_CLASS_PET_ITEM)
	{
		if(bRevivalPet == true)
		{
			pInvenItem->SetCurrentDurability(m_Type);
			bSucces = true;
		}
		else
		if(bSecondPetEnchant == false)
		{
			// A pet's first enchant: its attribute.
			WORD AttrType = HIWORD(m_Type);
			WORD AttrLevel = LOWORD(m_Type);
			pInvenItem->SetEnchantLevel(AttrType);	// Attr Type
			pInvenItem->SetSilver(AttrLevel);	// AttrLevel
			pInvenItem->SetItemColorSet(0xFFFF);
			bSucces = true;
		}
		else
		{
			// A pet's second enchant: one option.
			pInvenItem->ClearItemOption();	// clear whatever was there first
			pInvenItem->AddItemOption(static_cast<TYPE_ITEM_OPTION>(m_Type));
			bSucces = true;
		}
	}
	else
	{

		DWORD Option = m_Type;
	//	int Option2 = LOWORD(m_Type);

	//	if(oriOption < newOption)
		for(; Option != 0; Option >>= 16)	// the low word first, then the high word
		{
			int oriOption = (Option & 0xff00) >> 8;
			int newOption = (Option & 0xff);

			if(newOption == 0 && oriOption == 0)
				continue;

	//			continue;
			
			if(oriOption)
				pInvenItem->ChangeItemOption(oriOption, newOption);
			else
				pInvenItem->AddItemOption(newOption);

			if((*g_pItemOptionTable)[oriOption].UpgradeOptionType == newOption)
				bSucces |= true;
		}
	}

	if(bSucces)
		WorldEvents::PlaySound(SOUND_EVENT_FANFARE);

}

//----------------------------------------------------------------------
// ActionResultNodeFakeDie - FakeDie::Execute
//----------------------------------------------------------------------
void
MActionResultNodeFakeDie::Execute()
{
	if (!ZoneState::Exists()
		|| PlayerState::Creature()==NULL)
	{
		return;
	}

}

//----------------------------------------------------------------------
// ActionResultNodeEvent - Event::Execute
//----------------------------------------------------------------------
void
MActionResultNodeEvent::Execute()
{
	if (!ZoneState::Exists()
		|| PlayerState::Creature()==NULL)
	{
		return;
	}

}

//----------------------------------------------------------------------
// ActionResultNode - OpenCasket::Execute
//----------------------------------------------------------------------
void
MActionResultNodeChangeItemGenderInInventory::Execute()
{
	if (g_pInventory==NULL)
	{
		return;
	}

	MItem*	pInvenItem = g_pInventory->GetItem(m_TargetID);
	MItem*	pMouseItem = NULL;

	if(PlayerState::IsItemCheckBufferItemToItem())
	{
		pMouseItem = PlayerState::GetItemCheckBuffer();

		PlayerState::ClearItemCheckBuffer();
		if(pMouseItem != NULL)
			delete pMouseItem;
	}
	
	// No item, nothing to do.
	if (pInvenItem==NULL)
		return;

	pInvenItem->SetItemType( pInvenItem->GetItemType() ^ 0x1 );

	g_pInventory->CheckAffectStatus( pInvenItem );
	WorldEvents::PlaySound(SOUND_EVENT_FANFARE);
}

//----------------------------------------------------------------------
// CreateActionResultNode
//----------------------------------------------------------------------
MActionResultNode*
CreateActionResultNode(MCreature* pCreature, int skillID, BYTE grade)
{
	//------------------------------------------------------
	//
	// The result node the skill leaves, if any.
	//
	//------------------------------------------------------
	MActionResultNode* pActionResultNode = NULL;

	switch ((*g_pActionInfoTable)[skillID].GetActionResultID())
	{
		//------------------------------------------------------
		// Burrow
		//------------------------------------------------------
		case ACTIONRESULTNODE_CREATURE_BURROW :
			DEBUG_ADD("[ACTIONRESULTNODE_CREATURE_BURROW]");
			
			pActionResultNode = new MActionResultNodeCreatureBurrow(CreatureState::GetID(pCreature));
		break;

		//------------------------------------------------------
		// Invisible
		//------------------------------------------------------
		case ACTIONRESULTNODE_CREATURE_INVISIBLE :
			DEBUG_ADD("[ACTIONRESULTNODE_CREATURE_INVISIBLE]");
			
			pActionResultNode = new MActionResultNodeCreatureInvisible(CreatureState::GetID(pCreature));
		break;		

		//------------------------------------------------------
		// Visible
		//------------------------------------------------------
		case ACTIONRESULTNODE_CREATURE_VISIBLE :
			DEBUG_ADD("[ACTIONRESULTNODE_CREATURE_VISIBLE]");
			
			pActionResultNode = new MActionResultNodeCreatureVisible(CreatureState::GetID(pCreature));

			CreatureState::RemoveEffectStatus(pCreature, EFFECTSTATUS_INVISIBILITY );
			CreatureState::RemoveEffectStatus(pCreature, EFFECTSTATUS_SNIPPING_MODE );
		break;

		//------------------------------------------------------
		// Snipping
		//------------------------------------------------------
		case ACTIONRESULTNODE_CREATURE_SNIPPING :			
			DEBUG_ADD("[ACTIONRESULTNODE_CREATURE_SNIPPING]");
			
			pActionResultNode = new MActionResultNodeCreatureSnipping(CreatureState::GetID(pCreature));						
		break;

		//------------------------------------------------------
		// Turning
		//------------------------------------------------------
		case ACTIONRESULTNODE_CREATURE_TURNING :			
			DEBUG_ADD("[ACTIONRESULTNODE_CREATURE_TURNING]");

			if( skillID == SKILL_TYPHOON )
			{
				if( grade == 1 )
					pActionResultNode = new MActionResultNodeCreatureTurning(CreatureState::GetID(pCreature), 16);
			}
			else
				pActionResultNode = new MActionResultNodeCreatureTurning(CreatureState::GetID(pCreature), 16);
		break;

		//------------------------------------------------------
		// Casket
		//------------------------------------------------------
		case ACTIONRESULTNODE_SUMMON_CASKET :			
			DEBUG_ADD("[ACTIONRESULTNODE_SUMMON_CASKET]");
			
			pActionResultNode = new MActionResultNodeSummonCasket(CreatureState::GetID(pCreature), 0);
		break;

		//------------------------------------------------------
		// FakeDie
		//------------------------------------------------------
		case ACTIONRESULTNODE_FAKE_DIE:
			DEBUG_ADD("[ACTIONRESULTNODE_FAKE_DIE]");
			
			pActionResultNode = new MActionResultNodeFakeDie(CreatureState::GetID(pCreature));
			CreatureState::SetFakeDie(pCreature);
		break;

		//------------------------------------------------------
		// EVENT
		//------------------------------------------------------
		case ACTIONRESULTNODE_EVENT:
			DEBUG_ADD("[ACTIONRESULTNODE_EVENT]");
			
			pActionResultNode = new MActionResultNodeEvent(skillID);
		break;

	}


	return pActionResultNode;
}
