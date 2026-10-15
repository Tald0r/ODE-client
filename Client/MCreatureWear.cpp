//----------------------------------------------------------------------
// MCreatureWear.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MCreatureWear.h"
#include "MItem.h"
#include "AddonDef.h"
#include "SkillDef.h"
#include "DebugLog.h"
#include "ClientConfig.h"
#include "MEffectGeneratorTable.h"
#include "MEffectSpriteTypeTable.h"
#include "EffectSpriteTypeDef.h"
#include "MItemOptionTable.h"
#include "../basic/ColorSetDef.h"
#include "FrameClock.h"
#include "WorldEventsHost.h"

int defaultSkinColor		= 410;					
int defaultCoatColor		= 377;//368;	
int defaultTrouserColor	= 377;//489;	


#define	ATTACHEFFECTCOLOR_NULL		0xFFFF

// The allocator pool of the creatures that wear gear (task 4.243:
// MemoryPool.cpp's until then).
MemoryPool g_CreatureWearMemoryPool( sizeof( MCreatureWear ), 30 );


//----------------------------------------------------------------------
// static member
//----------------------------------------------------------------------
// Depending on the Creature's current direction...
// the order in which clothes are drawn may differ.
// This part sets the clothes drawing order for each direction.
int			MCreatureWear::s_AddonOrder[DIRECTION_MAX][ADDON_MAX] = 
{
	{ ADDON_MOTOR, ADDON_TROUSER, ADDON_COAT, ADDON_HAIR, ADDON_HELM, ADDON_RIGHTHAND, ADDON_LEFTHAND },	// LEFT
	{ ADDON_MOTOR, ADDON_TROUSER, ADDON_COAT, ADDON_HAIR, ADDON_HELM, ADDON_RIGHTHAND, ADDON_LEFTHAND },	// LEFTDOWN
	{ ADDON_MOTOR, ADDON_TROUSER, ADDON_COAT, ADDON_HAIR, ADDON_HELM, ADDON_LEFTHAND, ADDON_RIGHTHAND },	// DOWN	
	{ ADDON_MOTOR, ADDON_TROUSER, ADDON_COAT, ADDON_HAIR, ADDON_HELM, ADDON_LEFTHAND, ADDON_RIGHTHAND },	// RIGHTDOWN	
	{ ADDON_MOTOR, ADDON_TROUSER, ADDON_COAT, ADDON_HAIR, ADDON_HELM, ADDON_LEFTHAND, ADDON_RIGHTHAND },	// RIGHT
	{ ADDON_MOTOR, ADDON_TROUSER, ADDON_COAT, ADDON_HAIR, ADDON_HELM, ADDON_LEFTHAND, ADDON_RIGHTHAND },	// RIGHTUP
	{ ADDON_MOTOR, ADDON_TROUSER, ADDON_COAT, ADDON_HAIR, ADDON_HELM, ADDON_RIGHTHAND, ADDON_LEFTHAND },	// UP
	{ ADDON_MOTOR, ADDON_TROUSER, ADDON_COAT, ADDON_HAIR, ADDON_HELM, ADDON_RIGHTHAND, ADDON_LEFTHAND }	// LEFTUP
};

int	MCreatureWear::s_ACAddonOrder[ DIRECTION_MAX ][ AC_ADDON_MAX ] =
{
	{ AC_ADDON_MOTOR, AC_ADDON_BODY, AC_ADDON_HELM, AC_ADDON_SHOULDER, AC_ADDON_RIGHTHAND, AC_ADDON_LEFTHAND },
	{ AC_ADDON_MOTOR, AC_ADDON_BODY, AC_ADDON_HELM, AC_ADDON_SHOULDER, AC_ADDON_RIGHTHAND, AC_ADDON_LEFTHAND },
	{ AC_ADDON_MOTOR, AC_ADDON_BODY, AC_ADDON_HELM, AC_ADDON_SHOULDER, AC_ADDON_LEFTHAND, AC_ADDON_RIGHTHAND },
	{ AC_ADDON_MOTOR, AC_ADDON_BODY, AC_ADDON_HELM, AC_ADDON_SHOULDER, AC_ADDON_LEFTHAND, AC_ADDON_RIGHTHAND },
	{ AC_ADDON_MOTOR, AC_ADDON_BODY, AC_ADDON_HELM, AC_ADDON_SHOULDER, AC_ADDON_LEFTHAND, AC_ADDON_RIGHTHAND },
	{ AC_ADDON_MOTOR, AC_ADDON_BODY, AC_ADDON_HELM, AC_ADDON_SHOULDER, AC_ADDON_LEFTHAND, AC_ADDON_RIGHTHAND },
	{ AC_ADDON_MOTOR, AC_ADDON_BODY, AC_ADDON_HELM, AC_ADDON_SHOULDER, AC_ADDON_RIGHTHAND, AC_ADDON_LEFTHAND },
	{ AC_ADDON_MOTOR, AC_ADDON_BODY, AC_ADDON_HELM, AC_ADDON_SHOULDER, AC_ADDON_RIGHTHAND, AC_ADDON_LEFTHAND },
};


//----------------------------------------------------------------------
//
//  constructor/destructor
//
//----------------------------------------------------------------------
MCreatureWear::MCreatureWear()
{
	for (int i=0; i<ADDON_MAX; i++)
	{
		ClearAddonInfo( i );
	}

	m_HairFrameID = ADDONID_HAIR1_MALE;
	m_HairColorSet = 0;

	m_ShoulderAddon.bAddon = FALSE;
	m_ShoulderAddon.FrameID	= FRAMEID_NULL;		// outfit

	m_ShoulderAddon.ItemClass	= ITEM_CLASS_NULL;	// item class
	m_ShoulderAddon.ItemType	= ITEMTYPE_NULL;			// item type
	m_ShoulderAddon.ColorSet1	= 0;						// colorset1
	m_ShoulderAddon.ColorSet2	= 0;						// colorset2
	m_ShoulderAddon.bEffectColor = FALSE;					// Is this a part shown with the effect color?
	m_ShoulderAddon.EffectColorSet	= 0;				// effectcolorset
}

MCreatureWear::~MCreatureWear()
{
}

//----------------------------------------------------------------------
//
//  member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Clear AddonInfo
//----------------------------------------------------------------------
void			
MCreatureWear::ClearAddonInfo(int Addon)
{
	ADDON_INFO& addon = m_Addon[Addon];

	// Initialize
	addon.bAddon	= FALSE;			// Is it worn?
	addon.FrameID	= FRAMEID_NULL;		// outfit

	addon.ItemClass	= ITEM_CLASS_NULL;	// item class
	addon.ItemType	= ITEMTYPE_NULL;			// item type
	addon.ColorSet1	= 0;						// colorset1
	addon.ColorSet2	= 0;						// colorset2
	addon.bEffectColor = FALSE;					// Is this a part shown with the effect color?
	addon.EffectColorSet	= 0;				// effectcolorset
}

//----------------------------------------------------------------------
// Set Same Wear
//----------------------------------------------------------------------
void				
MCreatureWear::SetSameWear(const MCreatureWear* pCreature)
{
	// Outfit info
	for (int i=0; i<ADDON_MAX; i++)
	{
		m_Addon[i] = pCreature->m_Addon[i];
	}

	m_HairFrameID = pCreature->m_HairFrameID;
	m_HairColorSet = pCreature->m_HairColorSet;
}

//----------------------------------------------------------------------
// Set Addon ColorSet1
//----------------------------------------------------------------------
void			
MCreatureWear::SetAddonColorSet1(int Addon, WORD colorSet)	
{ 
	if (Addon >= ADDON_MAX)
	{
		return;
	}
	
	// 2004, 5, 4 sobeit add start - simply added because there was no code to set Ouster boots for other characters
	if(Addon == ADDON_TROUSER)
		m_Addon[Addon].bAddon = true;
	// 2004, 5, 4 sobeit add end

	if (colorSet < MAX_COLORSET || colorSet == UNIQUE_ITEM_COLOR || colorSet == QUEST_ITEM_COLOR)
	{	
		m_Addon[Addon].ColorSet1 = colorSet; 		
	}
	else
	{
		DEBUG_ADD_FORMAT_ERR("[Error] Exceed Maximum ColorSet. addon=%d, set=%d", Addon, colorSet);
	}
}

//----------------------------------------------------------------------
// Set Addon ColorSet2
//----------------------------------------------------------------------
void			
MCreatureWear::SetAddonColorSet2(int Addon, WORD colorSet)	
{ 
	if (Addon >= ADDON_MAX)
	{
		return;
	}

	// 2004, 5, 4 sobeit add start - simply added because there was no code to set Ouster boots for other characters
	if(Addon == ADDON_TROUSER)
		m_Addon[Addon].bAddon = true;
	// 2004, 5, 4 sobeit add end
	
	if (colorSet < MAX_COLORSET || colorSet == UNIQUE_ITEM_COLOR || colorSet == QUEST_ITEM_COLOR)
	{	
		// An operator's coat colours its whole body. T_T;;
		if (m_CreatureType==CREATURETYPE_SLAYER_OPERATOR
			&& Addon==ADDON_COAT)
		{
			m_ColorBody1 = m_ColorBody2 = colorSet;
		}
		
		m_Addon[Addon].ColorSet2 = colorSet; 		
	}
	else
	{
		DEBUG_ADD_FORMAT_ERR("[Error] Exceed Maximum ColorSet. addon=%d, set=%d", Addon, colorSet);
	}
}

//----------------------------------------------------------------------
// New Item From AddonInfo
//----------------------------------------------------------------------
// Creates an Item from the Addon's info.
// The caller must delete it.
//----------------------------------------------------------------------
MItem*
MCreatureWear::NewItemFromAddonInfo(int Addon)
{
	if (Addon >= ADDON_MAX)
	{
		return NULL;
	}

	ADDON_INFO& addon = m_Addon[Addon];

	//--------------------------------------------------
	// If an item is worn
	//--------------------------------------------------
	if (!addon.bAddon || addon.ItemClass==ITEM_CLASS_NULL)
	{
		return NULL;
	}
	
	//--------------------------------------------------
	// create the item just to read its info; the caller frees it.
	//--------------------------------------------------
	MItem* pItem = MItem::NewItem( addon.ItemClass );

	if (pItem == NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] NewItemFromAddonInfo: invalid item class %d", addon.ItemClass);
		return NULL;
	}

	pItem->SetItemType( addon.ItemType );

	return pItem;
}

//----------------------------------------------------------------------
// Remove Addon
//----------------------------------------------------------------------
// Removes the outfit at the Addon's position.
//----------------------------------------------------------------------
bool		
MCreatureWear::RemoveAddon(int Addon)
{ 
	DEBUG_ADD("MCreatureWear::RemoveAddon");
	
	if (Addon >= ADDON_MAX)
	{
		return false;
	}

	ADDON_INFO& addon = m_Addon[Addon];

	//--------------------------------------------------
	// Only take it off if something is equipped. - -;
	//--------------------------------------------------
	if (addon.bAddon)
	{
		MItem* pItem = NewItemFromAddonInfo( Addon );
		
		if (pItem!=NULL)
		{
			//--------------------------------------------------
			// Remove the addon Item from the outfit.
			//--------------------------------------------------
			bool bRemove = RemoveAddonItem( pItem );

			delete pItem;

			return bRemove;
		}

		//--------------------------------------------------
		// Remove the info..
		//--------------------------------------------------
		ClearAddonInfo( Addon );

		return true;
	}

	return false;
}

//---------------------------------------------------------------------------
// Set AddonItem
//---------------------------------------------------------------------------
// Sets the hair.
//---------------------------------------------------------------------------
void			
MCreatureWear::SetAddonHair(TYPE_FRAMEID id, WORD cs1)
{
	ADDON_INFO& addon = m_Addon[ADDON_HAIR];

	// Initialize
	addon.bAddon	= TRUE;			// Is it worn?
	addon.FrameID	= id;		// outfit

	addon.ItemClass	= ITEM_CLASS_NULL;	// item class

	addon.ItemType	= ITEMTYPE_NULL;			// item type
	addon.ColorSet1	= cs1;						// colorset1
	addon.ColorSet2	= 0;						// colorset2
	addon.bEffectColor = FALSE;
	addon.EffectColorSet = 0;

	// Save it.
	m_HairFrameID = id;
	m_HairColorSet = cs1;
}

//---------------------------------------------------------------------------
// Set AddonItem
//---------------------------------------------------------------------------
// For an item (AddonItem) whose appearance changes when equipped
// equips it on the Creature and changes its state.
//---------------------------------------------------------------------------
bool
MCreatureWear::SetAddonItem(MItem* pItem)
{
	DEBUG_ADD("MCreatureWear::SetAddonItem");
	
	if (pItem==NULL)
	{
		DEBUG_ADD_WAR("The Item is NULL");
		
		return false;
	}

	//-------------------------------------------------
	//
	// The outfit can only be changed while stopped.
	//
	//-------------------------------------------------
	// If it is not the player, or..
	// if riding a motorcycle..
	// just make it stop. - -;
	if (GetClassType()!=MCreature::CLASS_PLAYER
		|| pItem->GetItemClass()==ITEM_CLASS_MOTORCYCLE)
	{
		SetAction( (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND );
	}

	if (m_Action==ACTION_STAND 
		|| (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND)
		|| (IsOusters() && m_Action==ACTION_OUSTERS_FAST_MOVE_STAND) )
	{
		//-------------------------------------------------
		// Only handle Items whose appearance changes when equipped.
		//-------------------------------------------------
		if (pItem->IsAddonItem() && pItem->IsAffectStatus())
		{
			TYPE_FRAMEID	fid;
			ADDON			Addon = pItem->GetAddonSlot();

			//-------------------------------------------------
			// If there is a position where the outfit changes
			//-------------------------------------------------
			if (pItem->GetAddonSlot()==ADDON_NULL)
			{			
				DEBUG_ADD_FORMAT_ERR("[Error] Item doesn't have Addon Slot. id=%d, class=%d, type=%d",
														pItem->GetID(), (int)pItem->GetItemClass(), (int)pItem->GetItemType());
				
			}
			else
			{	
				//-------------------------------------------------
				//
				//			Handle according to the equip position..
				//
				//-------------------------------------------------
				switch (Addon)
				{
					//-------------------------------------------------
					// If a motorcycle is equipped
					//-------------------------------------------------
					case ADDON_MOTOR :
						//------------------------------------------
						// Change the way of moving
						//------------------------------------------					
						SetMoveDevice( MCreature::MOVE_DEVICE_RIDE );
					break;

					//-------------------------------------------------
					// If the item to equip is a left-hand weapon?
					//-------------------------------------------------
					case ADDON_LEFTHAND :
					{
						// Only shields exist. - -;
						// Ultimate hard-coding.. - -;;;;

						//-------------------------------------------------
						// If the existing right-hand weapon is two-handed
						// remove the right-hand Addon.
						//-------------------------------------------------
						MItem* pOldItem = NewItemFromAddonInfo( ADDON_RIGHTHAND );

						// If there is a right-hand item, remove it.
						if (pOldItem!=NULL)
						{	
							if (pOldItem->IsGearSlotTwoHand())
							{
								RemoveAddon( ADDON_RIGHTHAND );
							}

							delete pOldItem;
						}
					}
					break;

					//-------------------------------------------------
					// If the item to equip is a right-hand weapon?
					//-------------------------------------------------
					case ADDON_RIGHTHAND :
					{
						// If it is two-handed, remove the left hand.
						if (pItem->IsGearSlotTwoHand())
						{
							RemoveAddon( ADDON_LEFTHAND );
						}				
					}	
					break;

					//-------------------------------------------------
					// Hat
					//-------------------------------------------------
					// For a hat,
					// remove short hair.
					// Short hair: female 1, male 123
					//-------------------------------------------------
					case ADDON_HELM :
						if (m_HairFrameID==ADDONID_HAIR1_FEMALE
							|| m_HairFrameID==ADDONID_HAIR1_MALE
							|| m_HairFrameID==ADDONID_HAIR2_MALE
							|| m_HairFrameID==ADDONID_HAIR3_MALE)
						{
							RemoveAddon( ADDON_HAIR );
						}
					break;

					//-------------------------------------------------
					// For an operator.. changing clothes changes the body colour.
					//-------------------------------------------------
					case ADDON_COAT :
						if (m_CreatureType==CREATURETYPE_SLAYER_OPERATOR
							|| m_CreatureType==CREATURETYPE_VAMPIRE_OPERATOR
							|| m_CreatureType==CREATURETYPE_OUSTERS_OPERATOR)
						{
//							if(pItem->IsUniqueItem())
//								m_ColorBody1 = m_ColorBody2 = UNIQUE_ITEM_COLOR;
//							else
								m_ColorBody1 = m_ColorBody2 = pItem->GetItemOptionColorSet();
						}
					break;

					default:
						break;
				}
					
				//-------------------------------------------------
				//
				//			FrameID by gender
				//
				//-------------------------------------------------	
				if (IsMale())
				{
					fid = pItem->GetAddonMaleFrameID();
				}			
				else //if (pCreature->IsFemale())
				{
					fid = pItem->GetAddonFemaleFrameID();			
				}
			
				//-------------------------------------------------	
				// If it is a Slayer with no fid..
				//-------------------------------------------------					
				if (fid == FRAMEID_NULL && IsSlayer())
				{					
					DEBUG_ADD_FORMAT_ERR("[Error] Item doesn't have FrameID. id=%d, class=%d, type=%d",
										pItem->GetID(), (int)pItem->GetItemClass(), (int)pItem->GetItemType());					
				}
				//-------------------------------------------------					
				// Normal case.. or a Vampire
				//-------------------------------------------------					
				else
				{	
					//-------------------------------------------------
					// Equip it.
					//-------------------------------------------------
					ADDON_INFO& addon = m_Addon[Addon];

					// Initialize
					addon.bAddon	= TRUE;
					addon.FrameID	= fid;

					addon.ItemClass	= pItem->GetItemClass();
					addon.ItemType	= pItem->GetItemType();

					// Added Vampire clothes
					if(pItem->GetItemClass() == ITEM_CLASS_VAMPIRE_COAT)
					{
						unsigned short nSpecialActionInfo = GetSpecialActionInfo();

						if(m_CreatureType != CREATURETYPE_VAMPIRE_OPERATOR)
						{
							if (m_CreatureType != CREATURETYPE_WOLF && m_CreatureType != CREATURETYPE_BAT )
							{
								if( m_CreatureType == CREATURETYPE_WER_WOLF )
								{
									SetCreatureType( CREATURETYPE_WER_WOLF );
								}								
								else
								{
									if (IsMale())
									{
										SetCreatureType(pItem->GetAddonMaleFrameID());
									}			
									else //if (pCreature->IsFemale())
									{
										SetCreatureType(pItem->GetAddonFemaleFrameID());
										//SetCreatureType(812);
									}
								}
							}
							addon.ColorSet1	= m_ColorBody1;//defaultSkinColor;	// skin
//							if(pItem->IsUniqueItem())
//								m_ColorBody2 = UNIQUE_ITEM_COLOR;
//							else
							m_ColorBody2	= pItem->GetItemOptionColorSet();			// option
							if( m_CreatureType == CREATURETYPE_WER_WOLF )
								m_ColorBody1 = m_ColorBody2;
								
						}
						else
						{
							m_ColorBody1 = m_ColorBody2 = pItem->GetItemOptionColorSet();
						}

						SetSpecialActionInfo(nSpecialActionInfo);
					}

					else 
					if (Addon==ADDON_COAT || Addon==ADDON_TROUSER)
					{
						{
							addon.ColorSet1	= m_ColorBody1;//defaultSkinColor;	// skin
							//						if(pItem->IsUniqueItem())
							//							addon.ColorSet2	= UNIQUE_ITEM_COLOR;			// option
							//						else
							addon.ColorSet2	= pItem->GetItemOptionColorSet();			// option
							addon.bEffectColor = FALSE;
							addon.EffectColorSet = 0;
							
							// The outfit only changes for a coat.
							//if (Addon==ADDON_COAT)
							{
								//-------------------------------------------------
								// Applies to Vampire only.
								//-------------------------------------------------
								if (m_CreatureType==CREATURETYPE_VAMPIRE_OPERATOR)
								{
									//								if(pItem->IsUniqueItem())
									//									m_ColorBody1 = m_ColorBody2 = UNIQUE_ITEM_COLOR;
									//								else
									m_ColorBody1 = m_ColorBody2 = pItem->GetItemOptionColorSet();
								}
								else
								{
									//								if(pItem->IsUniqueItem())
									//									m_ColorBody2 = UNIQUE_ITEM_COLOR;
									//								else
									m_ColorBody2 = pItem->GetItemOptionColorSet();
								}
							}
						}
					}
					else
					{
//						if(pItem->IsUniqueItem())
//						{
//							addon.ColorSet1	= UNIQUE_ITEM_COLOR;	// option
//							addon.ColorSet2	= UNIQUE_ITEM_COLOR;//pItem->GetItemColorSet();			// null
//						}
//						else
						{
							addon.ColorSet1	= pItem->GetItemOptionColorSet();	// option
							addon.ColorSet2	= pItem->GetItemOptionColorSet();//pItem->GetItemColorSet();			// null
						}
						addon.bEffectColor = FALSE;
						addon.EffectColorSet = 0;
					}					
				}				
			}
		}			
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Item is Not Addon Item. id=%d, class=%d, type=%d",
									pItem->GetID(), (int)pItem->GetItemClass(), (int)pItem->GetItemType());			
		}

		//-------------------------------------------------
		// The equipped Item
		// if it is a basic-attack item...
		// the basic-attack ActionInfo must change.
		//-------------------------------------------------
		if (pItem->IsBasicWeapon() && pItem->IsAffectStatus())
		{
			TYPE_ACTIONINFO ai = pItem->GetUseActionInfo();

			//-------------------------------------------------
			// If no actionInfo is set
			//-------------------------------------------------
			if (ai==ACTIONINFO_NULL)
			{
				// Switch to the basic bare-hand attack action.
				SetBasicActionInfo( SKILL_ATTACK_MELEE );
				
				DEBUG_ADD("[Empty ActionInfo]This Creature's basic attack is Melee");				
			}
			//-------------------------------------------------
			// If an actionInfo is set..
			//-------------------------------------------------
			else
			{
				// Set it as the basic attack action.
				SetBasicActionInfo( ai );
				
				DEBUG_ADD_FORMAT("[Set BasicActionInfo] %d", ai);
			}
		}

		//-------------------------------------------------
		// If the right hand is empty..
		// the attack can be treated as bare-handed..
		// Hard-coding.. - -;;
		//-------------------------------------------------
		if (!m_Addon[ADDON_RIGHTHAND].bAddon)
		{
			DEBUG_ADD("[Empty RightHand]This Creature's basic attack is Melee");
			
			// Switch to the basic bare-hand attack action.
			SetBasicActionInfo( SKILL_ATTACK_MELEE );			
		}

		return true;	
		
	}

	//-------------------------------------------------
	//
	// If not in the stopped state
	//
	//-------------------------------------------------
	DEBUG_ADD_FORMAT_ERR("[Error] Not STAND action. id=%d, action=%d", m_ID, m_Action);
	
	// Change the outfit the next time it stops..
	// That is what should be done.. but it is a bother. - -;;

	return false;	
}

//---------------------------------------------------------------------------
// Remove AddonItem
//---------------------------------------------------------------------------
// Unequips an AddonItem.
//---------------------------------------------------------------------------
bool
MCreatureWear::RemoveAddonItem( MItem* pItem )
{
	DEBUG_ADD("MCreatureWear::RemoveAddonItem");

	if (pItem==NULL)
	{
		DEBUG_ADD_WAR("The Item is NULL");
		
		return false;
	}

	//-------------------------------------------------
	//
	// The outfit can only be changed while stopped.
	//
	//-------------------------------------------------
	// If it is not the player, or..
	// if riding a motorcycle..
	// just make it stop. - -;
	if (GetClassType()!=MCreature::CLASS_PLAYER
		|| pItem->GetItemClass()==ITEM_CLASS_MOTORCYCLE)
	{
		SetAction( (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND );
	}


	if (m_Action==ACTION_STAND
		|| (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND)
		|| (IsOusters() && m_Action==ACTION_OUSTERS_FAST_MOVE_STAND) )
	{
		// Added Vampire clothes
		if(pItem->GetItemClass() == ITEM_CLASS_VAMPIRE_COAT)
		{
			if(m_CreatureType != CREATURETYPE_VAMPIRE_OPERATOR)
			{
				if (m_CreatureType != CREATURETYPE_WOLF && m_CreatureType != CREATURETYPE_BAT )
				{
					if( m_CreatureType == CREATURETYPE_WER_WOLF )
					{
						SetCreatureType( CREATURETYPE_WER_WOLF );
						m_ColorBody1 = defaultCoatColor;
					} else
					{
						if (IsMale())	// Ugh, I should not hard-code this, but -_-
						{
							SetCreatureType(2);
						}			
						else //if (pCreature->IsFemale())
						{
							SetCreatureType(3); //by viva
						}
					}
				}
				//						addon.ColorSet1	= m_ColorBody1;//defaultSkinColor;	// skin

				m_ColorBody2	= defaultCoatColor;
			}
			else
			{
				m_ColorBody1	= m_ColorBody2	= defaultCoatColor;
			}
		}

		//-------------------------------------------------
		// Is it an Item whose appearance changes when worn?
		//-------------------------------------------------
		// --> find the equip slot
		//    and remove the Addon of that slot.
		//-------------------------------------------------
//		else
		if (pItem->IsAddonItem())
		{						
			enum ADDON add = pItem->GetAddonSlot();

			//-------------------------------------------------
			// Remove the Addon of the equip slot.
			//-------------------------------------------------
			if (add == ADDON_NULL)
			{				
				DEBUG_ADD_FORMAT_ERR("[Error] Item doesn't have Addon Slot. id=%d, class=%d, type=%d",
													pItem->GetID(), (int)pItem->GetItemClass(), (int)pItem->GetItemType());				
			}
			else
			{
				//--------------------------------------------------
				// Remove the info.
				//--------------------------------------------------
				ClearAddonInfo( add );

				//-------------------------------------------------
				//
				//			According to the equip slot
				//
				//-------------------------------------------------	
				switch (add)
				{
					//-------------------------------------------------
					// Coat: if bare, use the default body..
					//-------------------------------------------------
					case ADDON_COAT :
					{
						//MItem* pItem = MItem::NewItem( ITEM_CLASS_TROUSER );
						//pItem->SetItemType( 0 );

						//SetAddonItem( pItem );	

						//delete pItem;
						//-------------------------------------------------
						// Equip it.
						//-------------------------------------------------						
						ADDON_INFO& addon = m_Addon[ADDON_COAT];

						// Initialize
						addon.bAddon	= TRUE;
						if(GetRace() == RACE_OUSTERS)
						{
							addon.FrameID = 1;
							addon.ColorSet1 = defaultCoatColor;
						}
						else
						{
							addon.FrameID	= IsMale()? ADDONID_COAT0_MALE : ADDONID_COAT0_FEMALE; 
							addon.ColorSet1	= m_ColorBody1;//defaultSkinColor;	// default skin colour
						}

						addon.ItemClass	= ITEM_CLASS_COAT;
						addon.ItemType	= 0;	//by viva
						addon.ColorSet2	= defaultCoatColor;
						addon.bEffectColor = FALSE;
						addon.EffectColorSet = 0;
						
						//-------------------------------------------------	
						// Applies to Vampire only.
						// The operator probably applies too..
						//-------------------------------------------------													
						if (m_CreatureType==CREATURETYPE_VAMPIRE_OPERATOR
							|| m_CreatureType==CREATURETYPE_SLAYER_OPERATOR
							|| m_CreatureType==CREATURETYPE_OUSTERS_OPERATOR
							)
						{
							m_ColorBody1 = m_ColorBody2 = defaultSkinColor;	
						}
						else
						{
							m_ColorBody2 = defaultCoatColor;
						}
						
					}
					break;

					//-------------------------------------------------
					// Trousers: if bare, use the default body..
					//-------------------------------------------------
					case ADDON_TROUSER :
					{
						//MItem* pItem = MItem::NewItem( ITEM_CLASS_TROUSER );
						//pItem->SetItemType( 0 );

						//SetAddonItem( pItem );

						//delete pItem;
						//-------------------------------------------------
						// Equip it.
						//-------------------------------------------------
						ADDON_INFO& addon = m_Addon[ADDON_TROUSER];

						// Initialize
						addon.bAddon	= TRUE;
						addon.FrameID	= IsMale()? ADDONID_TROUSER0_MALE : ADDONID_TROUSER0_FEMALE;

						addon.ItemClass	= ITEM_CLASS_TROUSER;
						addon.ItemType	= 0;
						addon.ColorSet1	= m_ColorBody1;//defaultSkinColor;	// default skin colour
						addon.ColorSet2	= defaultTrouserColor;
						addon.bEffectColor = FALSE;
						addon.EffectColorSet = 0;
					}
					break;

					//-------------------------------------------------
					// The right hand is a weapon.., so..
					//-------------------------------------------------				
					case ADDON_RIGHTHAND :
						// Switch to the basic bare-hand attack action.
						SetBasicActionInfo( SKILL_ATTACK_MELEE );

						// Effect that applies only to certain weapons
						switch (pItem->GetItemClass())
						{
							//-------------------------------------------------	
							// Sword
							//-------------------------------------------------	
							case ITEM_CLASS_BLADE :
								if (HasEffectStatus(EFFECTSTATUS_BERSERKER))
								{
									RemoveEffectStatus(EFFECTSTATUS_BERSERKER);
								}
							break;

							//-------------------------------------------------	
							// Cross
							//-------------------------------------------------	
							case ITEM_CLASS_CROSS :
							case ITEM_CLASS_MACE :
								if (HasEffectStatus(EFFECTSTATUS_HYPNOSIS))
								{
									RemoveEffectStatus(EFFECTSTATUS_HYPNOSIS);
								}
							break;

							default:
								break;
						}
					break;

					//-------------------------------------------------
					// Motorcycle unequipped
					//-------------------------------------------------
					case ADDON_MOTOR :
						//------------------------------------------
						// Change the way of moving
						//------------------------------------------					
						SetMoveDevice( MCreature::MOVE_DEVICE_WALK );
					break;

					//-------------------------------------------------
					// Hat
					//-------------------------------------------------
					// When the hat is removed,
					// put the short hair back.
					//-------------------------------------------------
					case ADDON_HELM :
						if (m_HairFrameID==ADDONID_HAIR1_FEMALE
							|| m_HairFrameID==ADDONID_HAIR1_MALE
							|| m_HairFrameID==ADDONID_HAIR2_MALE
							|| m_HairFrameID==ADDONID_HAIR3_MALE)
						{
							SetAddonHair( m_HairFrameID, m_HairColorSet );
						}
					break;

					default:
						break;
				}
				
			}

			return true;
		}

		// If it is not an addon item
		return true;
	}

	//-------------------------------------------------
	//
	// If not in the stopped state
	//
	//-------------------------------------------------
	DEBUG_ADD_FORMAT_ERR("[Error] Not STAND action. id=%d, action=%d", m_ID, m_Action);
	
	// Change the outfit the next time it stops..
	// That is what should be done.. but it is a bother. - -;;


	return false;
}

//---------------------------------------------------------------------------
// Set Creature Type
//---------------------------------------------------------------------------
// Puts on the default outfit.
//---------------------------------------------------------------------------
void
MCreatureWear::SetCreatureType(TYPE_CREATURETYPE type)
{
	//-------------------------------------------------
	// Set the type
	//-------------------------------------------------
	MCreature::SetCreatureType( type );

	//-------------------------------------------------
	// Take everything off.
	//-------------------------------------------------
	for (int i=0; i<ADDON_MAX; i++)
	{
		ClearAddonInfo( i );
	}

	if(IsVampire())
	{
		//-------------------------------------------------
		// Coat: if bare, use the default body..
		//-------------------------------------------------
		ADDON_INFO& addon = m_Addon[ADDON_COAT];

		// Initialize
		addon.bAddon	= TRUE;
//		addon.FrameID	= IsMale()? ADDONID_COAT0_MALE : ADDONID_COAT0_FEMALE;
		
		addon.ItemClass	= ITEM_CLASS_VAMPIRE_COAT;
		addon.ItemType	= 0;
		addon.ColorSet1	= m_ColorBody1;//defaultSkinColor;	// default skin colour
		addon.ColorSet2	= defaultCoatColor;	
		addon.bEffectColor = FALSE;
		addon.EffectColorSet = 0;
	}
	else
	if(IsOusters())
	{
		//-------------------------------------------------
		// Coat: if bare, use the default body..
		//-------------------------------------------------
		ADDON_INFO& addon = m_Addon[ADDON_COAT];
		
		// Initialize
		addon.bAddon	= TRUE;
		addon.FrameID	= 1;
		//		addon.FrameID	= IsMale()? ADDONID_COAT0_MALE : ADDONID_COAT0_FEMALE;
		
		addon.ItemClass	= ITEM_CLASS_OUSTERS_COAT;
		addon.ItemType	= 0;
		addon.ColorSet1	= m_ColorBody1;//defaultSkinColor;	// default skin colour
		addon.ColorSet2	= defaultCoatColor;	
		addon.bEffectColor = FALSE;
		addon.EffectColorSet = 0;
	}
	else
	{
		//-------------------------------------------------
		// Coat: if bare, use the default body..
		//-------------------------------------------------
		ADDON_INFO& addon = m_Addon[ADDON_COAT];

		// Initialize
		addon.bAddon	= TRUE;
		addon.FrameID	= IsMale()? ADDONID_COAT0_MALE : ADDONID_COAT0_FEMALE;
		
		addon.ItemClass	= ITEM_CLASS_COAT;
		addon.ItemType	= 0;
		addon.ColorSet1	= m_ColorBody1;//defaultSkinColor;	// default skin colour
		addon.ColorSet2	= defaultCoatColor;	
		addon.bEffectColor = FALSE;
		addon.EffectColorSet = 0;
		
		//-------------------------------------------------
		// Trousers: if bare, use the default body..
		//-------------------------------------------------
		ADDON_INFO& addon2 = m_Addon[ADDON_TROUSER];
		
		// Initialize
		addon2.bAddon	= TRUE;
		addon2.FrameID	= IsMale()? ADDONID_TROUSER0_MALE : ADDONID_TROUSER0_FEMALE;
		
		addon2.ItemClass	= ITEM_CLASS_TROUSER;
		addon2.ItemType	= 0;
		addon2.ColorSet1	= m_ColorBody1;//defaultSkinColor;	// default skin colour
		addon2.ColorSet2	= defaultTrouserColor;
		addon2.bEffectColor = FALSE;
		addon2.EffectColorSet = 0;
	}
}

//----------------------------------------------------------------------
// Remove EffectStatus
//----------------------------------------------------------------------
// Removes a specific EffectSpriteType.
//----------------------------------------------------------------------
bool
MCreatureWear::RemoveEffectStatus(EFFECTSTATUS status)
{
	DEBUG_ADD_FORMAT("[RemoveEffectStatus] id=%d, status=%d", m_ID, (int)status);
	
	if (status>=g_pEffectStatusTable->GetSize())
	{
		DEBUG_ADD_FORMAT_ERR("[Error]EffectStatus exceed Max : %d", (int)status);
		
		return false;
	}

	if (!m_bEffectStatus[status])
	{
		return false;
	}

	m_bEffectStatus[status] = false;

	bool bUseEffectSprite = (*g_pEffectStatusTable)[status].bUseEffectSprite;
	TYPE_EFFECTSPRITETYPE type = (*g_pEffectStatusTable)[status].EffectSpriteType;
	//------------------------------------------------------------
	// Depending on effectStatus.
	//------------------------------------------------------------
	switch (status)
	{
		case EFFECTSTATUS_SUMMON_SYLPH_GREEN :
		case EFFECTSTATUS_SUMMON_SYLPH_ORANGE :
		case EFFECTSTATUS_SUMMON_SYLPH_RED :
		case EFFECTSTATUS_SUMMON_SYLPH_YELLOW :
		case EFFECTSTATUS_SUMMON_SYLPH_WHITE :
		case EFFECTSTATUS_SUMMON_SYLPH_BROWN :
		case EFFECTSTATUS_SUMMON_SYLPH_LILAC :
		case EFFECTSTATUS_SUMMON_SYLPH_BLACK :
		case EFFECTSTATUS_SUMMON_SYLPH:
			SetMoveDevice(MOVE_DEVICE_WALK);
			if(IsAdvancementClass() && status==EFFECTSTATUS_SUMMON_SYLPH)
			{
				type = EFFECTSPRITETYPE_OUSTERS_FASTMOVE_LOOP;
				WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_ADVANCEMENT_SUMMON_SYLPH_END,GetX(), GetY(), 0,GetDirection(),	GetID(),
					GetX(), GetY(), 0, 8, NULL, false);
			}
		break;
		case EFFECTSTATUS_BIKE_CRASH:
			SetMoveDevice(MOVE_DEVICE_WALK);
			break;
		//------------------------------------------------------------
		// When paralysis ends
		//------------------------------------------------------------
		// add by Coffee 2007-3-21
		case EFFECTSTATUS_SATELLITE_BOMB_AIM :
		// end	
		case EFFECTSTATUS_GUN_SHOT_GUIDANCE_AIM :
			if (m_bAlive)
			{
				SetStop();
				StopGunShotGuidance();
	#ifdef OUTPUT_DEBUG					
			DEBUG_ADD_FORMAT("[GSG] Stop");
	#endif
			}
		break;

		//------------------------------------------------------------
		// When paralysis ends
		//------------------------------------------------------------
		case EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS :
		case EFFECTSTATUS_EXPLOSION_WATER :
			if (m_bAlive)
			{
				SetStop();
				StopCauseCriticalWounds();
	#ifdef OUTPUT_DEBUG					
			DEBUG_ADD_FORMAT("[CCW] Stop");
	#endif
			}
		break;

		case EFFECTSTATUS_CURSE_PARALYSIS :
			if (m_bAlive)
			{
				SetStop();
				Remove_Curse_Paralsis_Effect();
			}
		break;

		//------------------------------------------------------------
		// EFFECTSTATUS_SUMMON_CASKET [new skill]
		//------------------------------------------------------------
		case EFFECTSTATUS_CASKET :
			// Make it invisible right away.
			RemoveCasket();
		break;

		//------------------------------------------------------------
		// Remove invisible
		//------------------------------------------------------------
		case EFFECTSTATUS_INVISIBILITY :
		case EFFECTSTATUS_SNIPPING_MODE :
			// Remove the invisible.
			SetVisible();
		break;

		//------------------------------------------------------------
		// EFFECTSTATUS_COMA
		//------------------------------------------------------------
		case EFFECTSTATUS_COMA :
			SetAlive();
			if(IsVampire())
			{
				WorldEvents::ExecuteActionInfoFromMainNode(
							BLOOD_RESURRECT,										// skill number used
						
							m_X, m_Y, 0,
							(int)m_Direction,														// direction used
							
							m_ID,												// target info
							m_X, m_Y, 0,
							
							2*16, //5*16, 
							
							NULL,
							
							false,
							
							(int)m_sX, (int)m_sY);	
			}
			break;

		case EFFECTSTATUS_GHOST:
			if (!(*g_pCreatureTable)[m_CreatureType].bFlyingCreature)	// unless it flies (a bat)
				SetGroundCreature();
			break;
		case EFFECTSTATUS_GLACIER:	
			MCreature::RemoveGlacierEffect();
			break;

		case EFFECTSTATUS_INSTALL_TURRET:
			SetAction( ACTION_STAND );
			SetInstallTurretCount(0);
			SetInstallTurretDirect(2);
			SetDirection( 2 );
			SetCurrentDirection( 2 );
			break;

		default:
			break;
	}

	//------------------------------------------------------------
	//
	//			Case expressed with EffectSprite
	//
	//------------------------------------------------------------
	if (bUseEffectSprite)
	{
		//TYPE_EFFECTSPRITETYPE type = (*g_pEffectStatusTable)[status].EffectSpriteType;
		TYPE_EFFECTSPRITETYPE type2 = EFFECTSPRITETYPE_NULL;

		if(type == EFFECTSPRITETYPE_FAST_MOVE_STOP)
			type2 = EFFECTSPRITETYPE_FAST_MOVE_FLY;

		DEBUG_ADD("remove effectsprite");

		//-------------------------------------------------------
		// Among the Effects attached to the Creature,
		// remove those whose EffectSpriteType is type.
		//-------------------------------------------------------
		if (type < g_pEffectSpriteTypeTable->GetSize())
		{
			if (IsFemale()
				&& (*g_pEffectSpriteTypeTable)[type].FemaleEffectSpriteType!=EFFECTSPRITETYPE_NULL)
			{
				type = (*g_pEffectSpriteTypeTable)[type].FemaleEffectSpriteType;
			}
			

			if (m_bAttachEffect[type])
			{
				//-------------------------------------------------------
				// If attached to the body
				//-------------------------------------------------------			
				ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();

				bool re = false;

				while (iEffect != m_listEffect.end())
				{
					MAttachEffect* pEffect = *iEffect;
					
					//-------------------------------------------------------
					// Find the same type.
					//-------------------------------------------------------
					if (pEffect->IsEffectSprite() 
						&& (pEffect->GetEffectSpriteType() == type ||
						(type2 != EFFECTSPRITETYPE_NULL && pEffect->GetEffectSpriteType() == type2) ) )
					{						
						// free the memory
						delete pEffect;
						pEffect = NULL;
						
						// remove from the list; erase() hands back the following
						// element, so the loop must not step forward again below.
						iEffect = m_listEffect.erase( iEffect );

						m_bAttachEffect[type] = false;

						re = true;
						continue;
					}					
					
					iEffect++;
				}

				if(re)
					return true;

				//-------------------------------------------------------
				// If attached to the ground
				//-------------------------------------------------------
				iEffect = m_listGroundEffect.begin();

				while (iEffect != m_listGroundEffect.end())
				{
					MAttachEffect* pEffect = *iEffect;
					
					//-------------------------------------------------------
					// Find the same type.
					//-------------------------------------------------------
					if (pEffect->IsEffectSprite() && 
						( pEffect->GetEffectSpriteType() == type || (type2 != EFFECTSPRITETYPE_NULL && 
						type2 == pEffect->GetEffectSpriteType())) )
					{
						// Free memory
						delete pEffect;

						// Remove from the list
						m_listGroundEffect.erase( iEffect );

						m_bAttachEffect[type] = false;
				
						return true;
					} 
					iEffect++;
				}
			}
		}
	}
	//------------------------------------------------------------
	//
	//			Case expressed with EffectColor
	//
	//------------------------------------------------------------
	else
	{
		WORD colorSet = (*g_pEffectStatusTable)[status].EffectColor;

		//-------------------------------------------------------
		// Among the Effects attached to the Creature,
		// remove those whose EffectColor is color.
		//-------------------------------------------------------
		ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();

		while (iEffect != m_listEffect.end())
		{
			MAttachEffect* pEffect = *iEffect;
			
			//-------------------------------------------------------
			// Find the same type.
			//-------------------------------------------------------
			if (pEffect->IsEffectColor() && pEffect->GetEffectColor() == colorSet)
			{
				ADDON	part = pEffect->GetEffectColorPart();

				//------------------------------------------------------------
				// In some cases only a specific part changes colour.
				//------------------------------------------------------------
				if (part < ADDON_MAX)
				{
					m_Addon[part].bEffectColor = FALSE;
				}
				//------------------------------------------------------------
				// Restore all parts to the original...
				//------------------------------------------------------------
				else
				{
					for (int p=0; p<ADDON_MAX; p++)
					{
						m_Addon[p].bEffectColor = FALSE;
					}
				}

				// Free memory
				delete pEffect;

				// Remove from the list
				m_listEffect.erase( iEffect );

				return true;
			}
			
			iEffect++;
		}		
	}
	
	return false;
}

//----------------------------------------------------------------------
// Update Effect
//----------------------------------------------------------------------
// Advances the Frame of every Effect attached to the Creature..
// and deletes finished ones from the list and memory.
//----------------------------------------------------------------------
void		
MCreatureWear::UpdateAttachEffect()
{
	ATTACHEFFECT_LIST::iterator iEffect = m_listEffect.begin();
	ATTACHEFFECT_LIST::iterator iEffectTemp;

	// Brightness is 0
	//m_MaxEffectLight = 0;

	//---------------------------------------------------------------------
	// Colour that changes the character's colour..
	//---------------------------------------------------------------------
	// It is a NULL value.. the caller must check..
	m_AttachEffectColor = m_ChangeColorSet;	//ATTACHEFFECTCOLOR_NULL;
	int bShowColor = (HasEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS) 
						|| g_CurrentFrame % g_pClientConfig->FRAME_DRAW_ORIGINAL_SPRITE);	// Every few frames, show the original colour once.

	int bShowColorPart = (g_CurrentFrame & 0x00000004);	// Alternate the colour output every 4 frames
	int numColors = 0;
	BOOL bChangeColor = FALSE;

	BOOL	bAddonColorPart[ADDON_MAX] = { FALSE, };
	WORD	addonColor[ADDON_MAX] = { m_ChangeColorSet, };

	//---------------------------------------------------------------------
	// Update all Effects.
	//---------------------------------------------------------------------
	while (iEffect != m_listEffect.end())
	{
		MAttachEffect*	pEffect = *iEffect;
		bool			bErase = false;
		
		int x		= pEffect->GetX();
		int y		= pEffect->GetY();
		int light = pEffect->GetLight();		

		//---------------------------------------------------------------------
		// Update
		//---------------------------------------------------------------------
		// message : find path
		//#ifdef	OUTPUT_DEBUG
			//sprintf(g_pDebugMessage->GetCurrent(), "Before UpdateEffect : Type=%d", pEffect->GetFrameID());	
			//g_pDebugMessage->Next();
		//#endif

		if( pEffect->GetEffectSpriteType() == EFFECTSPRITETYPE_FAST_MOVE_FLY ||
			pEffect->GetEffectSpriteType() == EFFECTSPRITETYPE_FAST_MOVE_STOP ||
			(
			pEffect->GetEffectSpriteType() == EFFECTSPRITETYPE_SUMMON_SYLPH_GREEN_FLY &&
			pEffect->GetEffectSpriteType() == EFFECTSPRITETYPE_SUMMON_SYLPH_BLACK_STOP
			)
			)
		{		
			BLT_TYPE		bltType = (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_FAST_MOVE_STOP].BltType;
			TYPE_EFFECTSPRITETYPE		EffectSpriteType = pEffect->GetEffectSpriteType();
			bool			bNewSummonSylphEffect = false;
			bool			bNewSummonSylphEffectStop = false;
			
			if( EffectSpriteType >= EFFECTSPRITETYPE_SUMMON_SYLPH_GREEN_FLY &&
				EffectSpriteType <= EFFECTSPRITETYPE_SUMMON_SYLPH_BLACK_STOP )
			{
				bNewSummonSylphEffect = true;

				if( (EffectSpriteType - EFFECTSPRITETYPE_SUMMON_SYLPH_GREEN_FLY) & 0x1 )
					bNewSummonSylphEffectStop = true;
			}
			
			if( GetAction() == ACTION_OUSTERS_FAST_MOVE_STAND && (
				EffectSpriteType != EFFECTSPRITETYPE_FAST_MOVE_STOP ||
				( bNewSummonSylphEffect && !bNewSummonSylphEffectStop )
				)
			  )
			{
				TYPE_FRAMEID	frameID	= (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_FAST_MOVE_STOP].FrameID;
				
				if( bNewSummonSylphEffect )
					frameID = (*g_pEffectSpriteTypeTable)[ EffectSpriteType + 1 ].FrameID;

				int maxFrame = WorldEvents::GetMaxEffectFrame(bltType, frameID);
			
				pEffect->SetEffectSprite( EFFECTSPRITETYPE_FAST_MOVE_STOP );
				if( bNewSummonSylphEffect )
					pEffect->SetEffectSprite( EffectSpriteType + 1 );
				pEffect->SetFrameID( frameID, maxFrame );
			}
			else
			if( GetAction() == ACTION_OUSTERS_FAST_MOVE_MOVE && 
				( pEffect->GetEffectSpriteType() != EFFECTSPRITETYPE_FAST_MOVE_FLY ||
				( bNewSummonSylphEffect && bNewSummonSylphEffectStop )
			  ) )
			{
				TYPE_FRAMEID	frameID	= (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_FAST_MOVE_FLY].FrameID;
				if( bNewSummonSylphEffect )
					frameID = (*g_pEffectSpriteTypeTable)[ EffectSpriteType - 1 ].FrameID;

				int maxFrame = WorldEvents::GetMaxEffectFrame(bltType, frameID);

				pEffect->SetEffectSprite( EFFECTSPRITETYPE_FAST_MOVE_FLY );

				if( bNewSummonSylphEffect )
					pEffect->SetEffectSprite( EffectSpriteType - 1 );
				pEffect->SetFrameID( frameID, maxFrame );
			}
		} 
		else if( pEffect->GetEffectSpriteType() >= EFFECTSPRITETYPE_LAR_SLASH_MALE_FAST && pEffect->GetEffectSpriteType() <= EFFECTSPRITETYPE_LAR_SLASH_FEMALE_SLOW )
		{
			if(!(  GetAction() == ACTION_SLAYER_SWORD_2 || GetAction() == ACTION_SLAYER_SWORD_2_SLOW || GetAction() == ACTION_SLAYER_SWORD_2_FAST ) 
				&& m_RepeatCount <= 0 )
			{
				pEffect->SetCount(0);
			}
		}
		else if(		
					(pEffect->GetEffectSpriteType() >= EFFECTSPRITETYPE_REGEN_TOWER_THROW_LIGHT_START &&
					pEffect->GetEffectSpriteType() <= EFFECTSPRITETYPE_REGEN_TOWER_THROW_LIGHT_END &&
					!HasEffectStatus( EFFECTSTATUS_TRYING ) 
					)
			)
		{
			bErase = true;			
		}

		if (bErase == false && pEffect->Update())
		{
			// -_- Absorb-soul does not change direction..
			if( pEffect->GetEffectSpriteType() != EFFECTSPRITETYPE_ABSORB_SOUL )
				pEffect->SetDirection( m_CurrentDirection );
			// Save the brightness of the Effect with the highest brightness.
//			if (m_MaxEffectLight < pEffect->GetLight())
//			{
//				m_MaxEffectLight = pEffect->GetLight();
//			}

			// Set the position
			pEffect->SetPosition(m_X, m_Y);

			//---------------------------------------------------------------------
			// If the position changed or
			// the light size (sight) changed
			//---------------------------------------------------------------------
			if (x!=pEffect->GetX() || y!=pEffect->GetY()
				|| light != pEffect->GetLight())
			{				
				// Change the sight
//				g_pZone->UnSetLight(x, y, light);
//				g_pZone->SetLight(pEffect->GetX(), pEffect->GetY(), pEffect->GetLight());
			}

			//---------------------------------------------------------------------
			// If it is an Effect that changes the character's colour
			// one must be chosen.
			//---------------------------------------------------------------------
			if (pEffect->IsEffectColor())
			{
				ADDON	part = pEffect->GetEffectColorPart();

				//--------------------------------------------------------
				// Effect that changes colour partially
				//--------------------------------------------------------
				if (part < ADDON_MAX)
				{
					if (bShowColorPart)
					{
						bAddonColorPart[part] = TRUE;
						addonColor[part] = pEffect->GetEffectColor();

						bChangeColor = TRUE;
					}
				}
				//--------------------------------------------------------
				// Effect that changes the whole colour
				//--------------------------------------------------------
				else if (bShowColor)
				{
					numColors++;
					//if (m_AttachEffectColor == ATTACHEFFECTCOLOR_NULL || rand()%numColors==0)
					if (m_AttachEffectColor == m_ChangeColorSet || rand()%numColors==0)
					{
						m_AttachEffectColor = pEffect->GetEffectColor();

						bChangeColor = TRUE;
					}
				}				
			}

			//-----------------------------------------------
			//
			// Before this Effect ends, by LinkCount
			// if there is a next linked Effect, it must be created.
			//
			// If the current Frame has passed EndLinkFrame
			//
			//-----------------------------------------------
			if (g_CurrentFrame >= pEffect->GetEndLinkFrame()
				&& pEffect->GetLinkSize() != 0)
			{
				// In GenerateNext,
				// pEffect's EffectTarget is set to NULL, so
				// there is no need to delete it here.
				g_pEffectGeneratorTable->GenerateNext( pEffect );

				// pEffect must still exist, so do not delete it.
			}

			// If it went fine
			iEffect++;
		}
		//---------------------------------------------------------------------
		// If it ends because time ran out
		//---------------------------------------------------------------------
		else
		{
			bool bUseEffectSprite = pEffect->IsEffectSprite();

			// Remove the flag
			if (bUseEffectSprite)
			{
				m_bAttachEffect[pEffect->GetEffectSpriteType()] = false;	// Remove the flag
			}


			ADDON	part = pEffect->GetEffectColorPart();

			//------------------------------------------------------------
			// In some cases only a specific part changes colour.
			//------------------------------------------------------------
			if (part < ADDON_MAX)
			{
				m_Addon[part].bEffectColor = FALSE;
			}
			//------------------------------------------------------------
			// Restore everything to the original.
			//------------------------------------------------------------
			else
			{
				for (int p=0; p<ADDON_MAX; p++)
				{
					m_Addon[p].bEffectColor = FALSE;
				}
			}
		
			//---------------------------------------------------------------------
			//
			// if there is a next linked Effect, it must be created.
			//
			//---------------------------------------------------------------------
			if (pEffect->GetLinkSize() != 0)
			{				
				g_pEffectGeneratorTable->GenerateNext( pEffect );
			}

			// If it is a glowing Effect, the sight must be removed.
			//if (pEffect->GetBltType()==BLT_EFFECT)
			{
				//m_nAlphaEffect --;

				// If all glowing Effects are gone, remove the sight.
				//if (m_nAlphaEffect==0)
				{
//					g_pZone->UnSetLight(x, y, light);

				}
			}

			DEBUG_ADD_FORMAT("[DeleteAttachEffect] id=%d, esType=%d", m_ID, pEffect->GetEffectSpriteType());
			
			// Delete memory, delete from list
			delete pEffect;						// Free memory
			
			DEBUG_ADD("[DeleteAttachEffect] OK");
			
			// Save temporarily.. to delete from the list
			iEffectTemp = iEffect;

			iEffect++;
			m_listEffect.erase( iEffectTemp );	// Remove from the list
		}		
	}

	//---------------------------------------------------------------------
	// Effect attached to the body
	//---------------------------------------------------------------------
	// Update all Effects.
	//---------------------------------------------------------------------
	iEffect = m_listGroundEffect.begin();
	
	while (iEffect != m_listGroundEffect.end())
	{
		MAttachEffect*	pEffect = *iEffect;
		
		int x		= pEffect->GetX();
		int y		= pEffect->GetY();
		int light = pEffect->GetLight();		

		//---------------------------------------------------------------------
		// Update
		//---------------------------------------------------------------------
		// message : find path
		//#ifdef	OUTPUT_DEBUG
			//sprintf(g_pDebugMessage->GetCurrent(), "Before UpdateEffect : Type=%d", pEffect->GetFrameID());	
			//g_pDebugMessage->Next();
		//#endif

		if (pEffect->Update())
		{
			// Save the brightness of the Effect with the highest brightness.
//			if (m_MaxEffectLight < pEffect->GetLight())
//			{
//				m_MaxEffectLight = pEffect->GetLight();
//			}

			// Set the position
			pEffect->SetPosition(m_X, m_Y);

			//---------------------------------------------------------------------
			// If the position changed or
			// the light size (sight) changed
			//---------------------------------------------------------------------
			if (x!=pEffect->GetX() || y!=pEffect->GetY()
				|| light != pEffect->GetLight())
			{				
				// Change the sight
//				g_pZone->UnSetLight(x, y, light);
//				g_pZone->SetLight(pEffect->GetX(), pEffect->GetY(), pEffect->GetLight());
			}

			//-----------------------------------------------
			//
			// Before this Effect ends, by LinkCount
			// if there is a next linked Effect, it must be created.
			//
			// If the current Frame has passed EndLinkFrame
			//
			//-----------------------------------------------
			if (g_CurrentFrame >= pEffect->GetEndLinkFrame()
				&& pEffect->GetLinkSize() != 0)
			{
				// In GenerateNext,
				// pEffect's EffectTarget is set to NULL, so
				// there is no need to delete it here.
				g_pEffectGeneratorTable->GenerateNext( pEffect );

				// pEffect must still exist, so do not delete it.
			}

			// If it went fine
			iEffect++;
		}
		//---------------------------------------------------------------------
		// If it ends because time ran out
		//---------------------------------------------------------------------
		else
		{
			bool bUseEffectSprite = pEffect->IsEffectSprite();

			// Remove the flag
			if (bUseEffectSprite)
			{
				m_bAttachEffect[pEffect->GetEffectSpriteType()] = false;	// Remove the flag
			}

			//---------------------------------------------------------------------
			//
			// if there is a next linked Effect, it must be created.
			//
			//---------------------------------------------------------------------
			if (pEffect->GetLinkSize() != 0)
			{				
				g_pEffectGeneratorTable->GenerateNext( pEffect );
			}

			// If it is a glowing Effect, the sight must be removed.
			//if (pEffect->GetBltType()==BLT_EFFECT)
			{
				//m_nAlphaEffect --;

				// If all glowing Effects are gone, remove the sight.
				//if (m_nAlphaEffect==0)
				{
//					g_pZone->UnSetLight(x, y, light);

				}
			}

			DEBUG_ADD_FORMAT("[DeleteAttachEffect] id=%d, esType=%d", m_ID, pEffect->GetEffectSpriteType());
			

			// Delete memory, delete from list
			delete pEffect;						// Free memory
			
			
			DEBUG_ADD("[DeleteAttachEffect] OK");
			

			// Save temporarily.. to delete from the list
			iEffectTemp = iEffect;

			iEffect++;
			m_listGroundEffect.erase( iEffectTemp );	// Remove from the list
		}		
	}

	//---------------------------------------------------------------------
	// If it is the time to show the colour change..
	//---------------------------------------------------------------------
	if (bChangeColor)
	{
		//---------------------------------------------------------------------
		// If there is no Effect changing the whole colour
		//---------------------------------------------------------------------	
		if (m_AttachEffectColor == m_ChangeColorSet)//ATTACHEFFECTCOLOR_NULL)
		{
			for (int p=0; p<ADDON_MAX; p++)
			{
				if (bAddonColorPart[p])
				{
					// Use the changing colour for that part only..
					m_Addon[p].bEffectColor		= TRUE;
					m_Addon[p].EffectColorSet	= addonColor[p];
				}		
				else
				{
					// Use the original colour..
					m_Addon[p].bEffectColor		= FALSE;
				}
			}
		}
		//---------------------------------------------------------------------
		// If there is an Effect changing the whole colour
		//---------------------------------------------------------------------
		else
		{
			for (int p=0; p<ADDON_MAX; p++)
			{
				m_Addon[p].bEffectColor		= TRUE;

				if (bAddonColorPart[p])
				{
					// Use the changing colour for that part only..
					m_Addon[p].EffectColorSet	= addonColor[p];
				}		
				else
				{
					// Use the changing colour for the whole..
					m_Addon[p].EffectColorSet	= m_AttachEffectColor;
				}
			}
		}
	}
	//---------------------------------------------------------------------
	// Time when the colour change is not shown...
	//---------------------------------------------------------------------
	else
	{
		for (int p=0; p<ADDON_MAX; p++)
		{
			m_Addon[p].bEffectColor		= FALSE;
		}
	}
}

const MCreatureWear::ADDON_INFO&	MCreatureWear::GetACAddonInfo(int Addon) const
{ 
	switch( Addon )
	{
	case AC_ADDON_BODY :
		return m_Addon[ ADDON_COAT ];
		
	case AC_ADDON_HELM :
		return m_Addon[ ADDON_HELM ];

	case AC_ADDON_LEFTHAND :
		return m_Addon[ ADDON_LEFTHAND ];

	case AC_ADDON_RIGHTHAND :
		return m_Addon[ ADDON_RIGHTHAND ];

	case AC_ADDON_MOTOR :
		return m_Addon[ ADDON_MOTOR ];
	}

	return m_ShoulderAddon;
}
