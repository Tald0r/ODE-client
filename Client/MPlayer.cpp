//----------------------------------------------------------------------
// MPlayer.cpp
//----------------------------------------------------------------------
//
// m_nUsedActionInfo is the action performed right on the next turn, and
//
// m_nNextUsedActionInfo is
//          the action performed only after the trace is finished (acts as a buffer).
//
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include <math.h>
#include "MZone.h"
#include "WorldEventsHost.h"
#include "GameMessages.h"
#include "../basic/CMessageArray.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "ClientMode.h"
#include "MHelpDef.h"
#include "SkillDef.h"
#include "MSkillManager.h"
#include "MViewDef.h"
#include "MInventory.h"
#include "MStorage.h"
#include "FrameClock.h"
#include "WorldTileGeometry.h"
#include "MItem.h"
#include "MCreature.h"
#include "MInteractionObject.h"
#include "MLinearEffect.h"
#include "TempInformation.h"
#include "MParabolaEffect.h"
#include "MPlayer.h"
#include "MActionInfoTable.h"
#include "MEffectTarget.h"
#include "MEffectGeneratorTable.h"
#include "PacketDef.h" 
#include "PlayerPresentationHost.h"
#include "MCreatureTable.h"
#include "MSoundTable.h"
#include "MItemTable.h"
#include "ClientConfig.h"
#include "MGameStringTable.h"
#include "MStatusManager.h"
#include "MItemOptionTable.h"
#include "TalkBox.h"
#include "UIDialog.h"
#include "MPortal.h"
#include "InputState.h"
#include "MGameTime.h"
#include "MObjectSelector.h"
#include "SoundDef.h"
#include "MItemFinder.h"
#include "Packet/Cpackets/CGSelectTileEffect.h"
#include "Packet/Cpackets/CGPartyInvite.h"
#include "EffectSpriteTypeDef.h"
#include "ModifyStatusManager.h"
#include "ClientCommunicationManager.h"
#include "MEventManager.h"
#include "MItemFinder.h"
#include "MZoneTable.h"

#include "Rpackets/RCPositionInfo.h"
#include "Rpackets/RCStatusHP.h"
#include "Rpackets/RCCharacterInfo.h"
#include "Cpackets/CGRequestInfo.h"
#include "Cpackets/CGUseItemFromGear.h"

#include "UserInformation.h"
#include "MParty.h"
#include "MJusticeAttackManager.h"
#include "RequestUserManager.h"
#include "MWarManager.h"

#include "MGuildType.h"
#include "GameSocket.h"
#include "DebugLog.h"
#include "RankBonusTable.h"
#include "RankBonusDef.h"
#include "MOustersGear.h"
#include "MQuickSlot.h"

#include "Cpackets/CGAbsorbSoul.h"

#include "DirectionSelection.h"
#include "PetWindowInfo.h"
#include "SystemAvailabilities.h"

#include "VsUiPresentationHost.h"
#include "DialogPresentationHost.h"

#include "Cpackets/CGPartyPosition.h"
#include "WebSocketTransport.h"

#include "NoticePresentationHost.h"
extern bool UpdateSocketOutput();
//GameNew Mode  add by sonc 2006.9.27
extern	LONG g_SECTOR_WIDTH;
extern	LONG g_SECTOR_HEIGHT;
extern	LONG g_SECTOR_WIDTH_HALF;
extern	LONG g_SECTOR_HEIGHT_HALF;
extern	LONG g_SECTOR_SKIP_PLAYER_LEFT;
extern	LONG g_SECTOR_SKIP_PLAYER_UP;
extern	LONG g_TILESURFACE_SECTOR_WIDTH;
extern	LONG g_TILESURFACE_SECTOR_HEIGHT;
extern	LONG g_TILESURFACE_SECTOR_OUTLINE_RIGHT;
extern	LONG g_TILESURFACE_SECTOR_OUTLINE_DOWN;
extern	LONG g_TILESURFACE_WIDTH;
extern	LONG g_TILESURFACE_HEIGHT;
extern	LONG g_TILESURFACE_OUTLINE_RIGHT;
extern	LONG g_TILESURFACE_OUTLINE_DOWN;
//end 
#include "StatusPresentationHost.h"
bool		GetMakeItemFitPosition(MItem* pItem, ITEM_CLASS itemClass, int itemType, POINT& fitPoint);

// [new skill]
void		SkillShadowDancing(MCreature* pUserCreature, MCreature* pTargetCreature, int skillID);
bool			IsCreatureMove( MCreature *pCreature );
bool			IsCreatureActionAttack( MCreature *pCreature );
bool			HasEffectStatusSummonSylph( MCreature* pCreature );
void			RemoveEffectStatusSummonSylph( MCreature* pCreature );
BYTE GetCreatureActionCountMax( const MCreature* pCreature, int action );

// #ifndef CONNECT_SERVER
	#include "MFakeCreature.h"
// #endif



#ifdef __METROTECH_TEST__
//	#define OUTPUT_DEBUG_PLAYER_ACTION
#endif



#include "ZoneLoadingHost.h"
#include "GameInfoPresentationHost.h"

extern	MonotonicClock::TimePoint	g_FrameNow;
extern BOOL					g_bCButtonDown;
extern bool g_bPreviousMove;

extern int					g_nZoneLarge;
extern int					g_nZoneSmall;
extern bool					g_bZonePlayerInLarge;
extern bool					g_bZoneSafe;
extern bool					g_bHolyLand;

//#define	new			DEBUG_NEW

#define				TRADE_DISTANCE_NPC		4
#define				TRADE_DISTANCE_PC		2


		

//----------------------------------------------------------------------
//
//							Global Player
//
//----------------------------------------------------------------------
MPlayer*			g_pPlayer = NULL;

// The view's tile-to-pixel conversions (MTopView's statics forward to
// these): a sector's pixel, and the sector of a pixel.
static POINT MapToPixel(int x, int y)
{
	POINT p = { WorldTileGeometry::TileToPixelX(x), WorldTileGeometry::TileToPixelY(y) };
	return p;
}

static POINT PixelToMap(int x, int y)
{
	POINT p = { WorldTileGeometry::PixelToTileX(x), WorldTileGeometry::PixelToTileY(y) };
	return p;
}

//----------------------------------------------------------------------
// define Functions
//----------------------------------------------------------------------
// The moment it becomes a corpse...
/*
#define GET_DYING_CREATURE( pCreature, id )								\
		if (pCreature==NULL)											\
		{																\
			MItem* pItem = g_pZone->GetItem( id );						\
																		\
			if (pItem!=NULL												\
				&& pItem->GetItemClass()==ITEM_CLASS_CORPSE)		\
			{															\
				pCreature = ((MCorpse*)pItem)->GetCreature();			\
																		\
				if (pCreature->GetActionCount() >= pCreature->GetActionCountMax())	\
				{														\
					pCreature = NULL;									\
				}														\
			}															\
		}
*/

//----------------------------------------------------------------------
// CHECK_REPEAT_LIMIT_TO_RETURN_FALSE
//----------------------------------------------------------------------
#define CHECK_REPEAT_LIMIT_TO_RETURN_FALSE(nActionInfo, repeatCount)		\
		if (nActionInfo < g_pActionInfoTable->GetSize())	\
		{													\
			if (repeatCount >= (*g_pActionInfoTable)[nActionInfo].GetRepeatLimit())	\
			{												\
				UnSetRepeatAction();						\
				return false;								\
			}												\
		}

//----------------------------------------------------------------------
// Auto-drink potions
//----------------------------------------------------------------------
#ifdef __METROTECH_TEST__

	int  g_iAutoHealPotion = 1;
	bool g_bAutoManaPotion = true;
	int	 g_iAutoReload = 1;
	bool g_bLight = false;
	int	 g_iSpeed = 0;

	void
	AutoUseSkill(int skill)
	{
		if (g_pSkillInfoTable != NULL
			&& g_pInventory!=NULL 
			&& g_pPlayer!=NULL
			&& g_pPlayer->IsSlayer()
			&& g_pPlayer->IsItemCheckBufferNULL()
			&& !g_pPlayer->IsInSafeSector())
		{
			bool bUse = false;
			int skill_id = ACTIONINFO_NULL;

			if(skill == 0)	//healing
			{
				const int hp_gap = g_pPlayer->GetMAX_HP()-g_pPlayer->GetHP();
				
				if(hp_gap > 0)
				{
					if(g_iAutoHealPotion == 2)	// when energy runs low, refill by one step
					{
						if(g_pSkillAvailable->IsEnableSkill( MAGIC_CURE_CRITICAL_WOUNDS ) &&
							(*g_pSkillInfoTable)[MAGIC_CURE_CRITICAL_WOUNDS].IsEnable() && 
							(*g_pSkillInfoTable)[MAGIC_CURE_CRITICAL_WOUNDS].GetExpLevel()/2+30 <= hp_gap)
						{
							skill_id = MAGIC_CURE_CRITICAL_WOUNDS;
							bUse = true;
						}
						else
						if(g_pSkillAvailable->IsEnableSkill( MAGIC_CURE_SERIOUS_WOUNDS ) &&
							(*g_pSkillInfoTable)[MAGIC_CURE_SERIOUS_WOUNDS].IsEnable() && 
							(*g_pSkillInfoTable)[MAGIC_CURE_SERIOUS_WOUNDS].GetExpLevel()/4+30 <= hp_gap)
						{
							skill_id = MAGIC_CURE_SERIOUS_WOUNDS;
							bUse = true;
						}
						else
						if(g_pSkillAvailable->IsEnableSkill( MAGIC_CURE_LIGHT_WOUNDS ) &&
							(*g_pSkillInfoTable)[MAGIC_CURE_LIGHT_WOUNDS].IsEnable() && 
							(*g_pSkillInfoTable)[MAGIC_CURE_LIGHT_WOUNDS].GetExpLevel()/4+10 <= hp_gap)
						{
							skill_id = MAGIC_CURE_LIGHT_WOUNDS;
							bUse = true;
						}
					}
					else if(g_iAutoHealPotion == 3)	// when energy runs low, refill
					{
						if(g_pSkillAvailable->IsEnableSkill( MAGIC_CURE_CRITICAL_WOUNDS ) &&
							(*g_pSkillInfoTable)[MAGIC_CURE_CRITICAL_WOUNDS].IsEnable())
						{
							skill_id = MAGIC_CURE_CRITICAL_WOUNDS;
							bUse = true;
						}
						else
						if(g_pSkillAvailable->IsEnableSkill( MAGIC_CURE_SERIOUS_WOUNDS ) &&
							(*g_pSkillInfoTable)[MAGIC_CURE_SERIOUS_WOUNDS].IsEnable())
						{
							skill_id = MAGIC_CURE_SERIOUS_WOUNDS;
							bUse = true;
						}
						else
						if(g_pSkillAvailable->IsEnableSkill( MAGIC_CURE_LIGHT_WOUNDS ) &&
							(*g_pSkillInfoTable)[MAGIC_CURE_LIGHT_WOUNDS].IsEnable())
						{
							skill_id = MAGIC_CURE_LIGHT_WOUNDS;
							bUse = true;
						}
					}
				}
			}

			if(skill == 1)	// create holy water
			{
				if(g_pSkillAvailable->IsEnableSkill( MAGIC_CREATE_HOLY_WATER ) &&
					(*g_pSkillInfoTable)[MAGIC_CREATE_HOLY_WATER].IsEnable() &&
					(*g_pSkillInfoTable)[MAGIC_CREATE_HOLY_WATER].IsAvailableTime())
				{
					skill_id = MAGIC_CREATE_HOLY_WATER;
					bUse = true;
				}
			}

			if(bUse == true && skill_id != ACTIONINFO_NULL)
			{
				// Use skill
				g_pPlayer->UnSetRequestMode();
				
				TYPE_ACTIONINFO old_special = g_pPlayer->GetSpecialActionInfo();
				g_pPlayer->SetSpecialActionInfo(skill_id);
				g_pPlayer->SelfSpecialAction();
				g_pPlayer->SetSpecialActionInfo(old_special);
			
//				g_bPreviousMove = false;
			}
		}
	}

	//------------------------------------------------------------
	// Drink healing
	//------------------------------------------------------------
	void
	AutoUsePotion(int bHeal)	// 0:heal 1:mana 2:magazine
	{
		if (g_pInventory!=NULL 
			&& g_pPlayer!=NULL
			&& !g_pPlayer->IsVampire()
			&& g_pPlayer->IsItemCheckBufferNULL()
			&& !gbl_item_lock)
		{
			// Healing
			int badClass = -1;
			int badType = -1;

			int currentValue;
			int maxValue; 
			float modifier = 1.0f;

			switch(bHeal)
			{
			case 0:	// heal
				currentValue = g_pPlayer->GetHP();
				maxValue = g_pPlayer->GetMAX_HP();
				break;

			case 1: // mana
				currentValue = g_pPlayer->GetMP();
				maxValue = g_pPlayer->GetMAX_MP();

				modifier = 1.0f + g_pPlayer->GetINT() / 300.0f;
				break;

			case 2: // magazine
			case 3: // magazine
			case 4: // magazien
			case 5: // magazine
				if(g_pCurrentMagazine == NULL)
					return;

				currentValue = g_pCurrentMagazine->GetNumber();
				maxValue = 1;
				break;
			}

			if (maxValue<=0) return;

			int lost = maxValue - currentValue;
			int percent = currentValue * 100 / maxValue;

			bool forceUse = false;
			
			ITEM_CLASS itemClass = ITEM_CLASS_POTION;
			if(g_pPlayer->IsOusters())
				itemClass = ITEM_CLASS_PUPA;

			int itemType;
			int maxItemType = 4;
			
			if(bHeal >= 2)
			{
				if(g_pSlayerGear != NULL && !g_pSlayerGear->GetItem(MSlayerGear::GEAR_SLAYER_LEFTHAND)->IsGunItem())return;
//				badType = (g_pSlayerGear->GetItem(MSlayerGear::GEAR_SLAYER_LEFTHAND)->GetItemClass()-ITEM_CLASS_SG)*2-1;
				badType = (g_pCurrentMagazine->GetItemType() & ~1) -1;
				itemClass = ITEM_CLASS_MAGAZINE;
				
				if(currentValue > bHeal-2)
					return;

				itemType = badType+2;
			}
			else
			{
				if (lost >= 200.0f * modifier)		itemType = 4;
				else if (lost >= 100.0f * modifier)	itemType = 3;
				else if (lost >= 51.0f * modifier)	itemType = 2;
				else if (lost >= 24.0f * modifier)	itemType = 1;
				else if (lost >= 10.0f * modifier)	itemType = 0;
				else								itemType = badType;
			}

			switch(bHeal)
			{
			case 0:	// Heal
				// Drink when HP is 50% or less.
				forceUse = percent <= 50;
				break;

			case 1:	// Mana
				badType += 5;
				itemType += 5;
				maxItemType += 5;

				// Drink mana by comparing with the skill currently in use.
				if (g_pSkillInfoTable!=NULL)
				{
					int skill = g_pPlayer->GetSpecialActionInfo();

					if (skill < g_pSkillInfoTable->GetSize())
					{
						forceUse = currentValue < 5
									|| (*g_pSkillInfoTable)[skill].GetMP()*2 >= currentValue;
					}
				}
				break;

			}
			
			if (itemType!=badType && itemClass!=badClass)
			{
				MItem* pItem = NULL;
				int type = itemType;

				do
				{
					pItem = g_pInventory->FindItem(itemClass , type );

					// When reloading, look for silver bullets
					if(bHeal >= 2 && pItem == NULL) pItem = g_pInventory->FindItem(itemClass , type +8);

					if (pItem!=NULL) break;					
				}
				while (--type > badType);

				// Below a certain percent just drink it. Except bullets
				if (bHeal != 2 && pItem==NULL && forceUse)
				{
					type = badType + 1;

					do
					{
						pItem = g_pInventory->FindItem( ITEM_CLASS_POTION, type );

						if (pItem!=NULL) break;					
					}
					while (++type <= maxItemType);
				}
			
				if (pItem!=NULL && g_pPlayer->IsItemCheckBufferNULL())
				{
					if(bHeal >= 2)
					{
						CGReloadFromInventory _CGReloadFromInventory;
						_CGReloadFromInventory.setObjectID( pItem->GetID() );
						_CGReloadFromInventory.setX( pItem->GetGridX() );
						_CGReloadFromInventory.setY( pItem->GetGridY() );

						g_pSocket->sendPacket( &_CGReloadFromInventory );
					}
					else
					{
						CGUsePotionFromInventory _CGUsePotionFromInventory;
						_CGUsePotionFromInventory.setObjectID( pItem->GetID() );
						_CGUsePotionFromInventory.setX( pItem->GetGridX() );
						_CGUsePotionFromInventory.setY( pItem->GetGridY() );

						g_pSocket->sendPacket( &_CGUsePotionFromInventory );
					}

					g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
				}
				else if (g_pQuickSlot!=NULL)
				{
					// Look in the belt
					type = itemType;

					do
					{
						MItemClassTypeFinder itemFinder( itemClass, type );

						pItem = g_pQuickSlot->FindItem( itemFinder );

						// When reloading, look for silver bullets
						if(bHeal >= 2 && pItem == NULL)
						{
							MItemClassTypeFinder itemFinder( itemClass, type +8 );

							pItem = g_pQuickSlot->FindItem( itemFinder );
						}

						if (pItem!=NULL) break;					
					}
					while (--type > badType);

					// Below a certain percent just drink it. Except for bullets
					if (bHeal != 2 && pItem==NULL && forceUse)
					{
						type = badType + 1;

						do
						{
							MItemClassTypeFinder itemFinder( ITEM_CLASS_POTION, type );

							pItem = g_pQuickSlot->FindItem( itemFinder );

							if (pItem!=NULL) break;					
						}
						while (++type <= maxItemType);
					}

					if (pItem!=NULL && g_pPlayer->IsItemCheckBufferNULL() )
					{
						if(bHeal >= 2)
						{
							g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);

							CGReloadFromQuickSlot _CGReloadFromQuickSlot;
							_CGReloadFromQuickSlot.setObjectID( pItem->GetID() );
							_CGReloadFromQuickSlot.setSlotID( pItem->GetItemSlot() );							

							g_pSocket->sendPacket( &_CGReloadFromQuickSlot );
						}
						else
						{
							g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);

							CGUsePotionFromQuickSlot _CGUsePotionFromQuickSlot;
							_CGUsePotionFromQuickSlot.setObjectID( pItem->GetID() );
							_CGUsePotionFromQuickSlot.setSlotID( pItem->GetItemSlot() );							

							g_pSocket->sendPacket( &_CGUsePotionFromQuickSlot );
						}

						g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);

						VsUiPresentation::LockGear();
					}					
				}			
				
			}
		}
	}

#endif

bool	CanActionByZoneInfo()
{	
	if(g_pZone != NULL)
	{
		ZONETABLE_INFO *pZoneInfo = g_pZoneTable->Get( g_pZone->GetID() );
		
		if(pZoneInfo->CannotAttackInSafe == true)					// if attacking inside the safe zone is not allowed
		{			
			if ( g_pZone->GetSector(g_pPlayer->GetX(),g_pPlayer->GetY()).IsSafeComplete() )
				return true;
			else
			{
				if(g_pPlayer->IsSlayer() &&	g_pZone->GetSector(g_pPlayer->GetX(),g_pPlayer->GetY()).IsSafeSlayer() )
					return true;				
				if(g_pPlayer->IsVampire() && g_pZone->GetSector(g_pPlayer->GetX(),g_pPlayer->GetY()).IsSafeVampire() ) 
					return true;
				if(g_pPlayer->IsOusters() && g_pZone->GetSector(g_pPlayer->GetX(),g_pPlayer->GetY()).IsSafeOusters() ) 
					return true;
			}
		}
	}
	return false;
}

//----------------------------------------------------------------------
// Send my coordinates to party members.
//----------------------------------------------------------------------
void
SendPositionInfoToParty()
{
	//------------------------------------------------------
	// When sending coordinates continuously
	//------------------------------------------------------		
	if (g_pParty->GetSize()!=0
		&& g_pClientCommunicationManager!=NULL)
	{
		static int oldX = -1;
		static int oldY = -1;
		static int oldZoneID = -1;

		int x		= g_pPlayer->GetX();
		int y		= g_pPlayer->GetY();
		int zoneID	= (g_bZonePlayerInLarge?g_nZoneLarge : g_nZoneSmall);

		// Send if the coordinates changed.
		if (oldX!=x || oldY!=y || oldZoneID!=zoneID)			
		{
			if(g_pUserInformation->bKorean == true && !NetworkTransport::UsesWebSocket())
			{
				RCPositionInfo _RCPositionInfo;

				// No need to send the name since it is checked by IP.
				//_RCPositionInfo.setName( g_pUserInformation->CharacterID.GetString() );
				_RCPositionInfo.setZoneX( x );
				_RCPositionInfo.setZoneY( y );
				_RCPositionInfo.setZoneID( zoneID );

				// Remember the sent information
				oldX = x;
				oldY = y;
				oldZoneID = zoneID;

				for (int i=0; i<g_pParty->GetSize(); i++)
				{
					PARTY_INFO* pInfo = g_pParty->GetMemberInfo( i );
					
					if (pInfo!=NULL
						// Send position info only when out of sight.
						// Might be meaningless.. (sigh)
						//&& !pInfo->bInSight
						)
					{
						int port = g_pClientConfig->CLIENT_COMMUNICATION_UDP_PORT;

						RequestUserInfo* pUserInfo = g_pRequestUserManager->GetUserInfo( pInfo->Name.GetString() );

						if (pUserInfo!=NULL)
						{
							if (pUserInfo->UDPPort!=0)
								port = pUserInfo->UDPPort;
						}

						g_pClientCommunicationManager->sendPacket( pInfo->IP, 
																port, 
																&_RCPositionInfo );
					}
				}			
			}
			else	// not Korean
			{
				CGPartyPosition _CGPartyPosition;

				_CGPartyPosition.setZoneID ( zoneID );
				_CGPartyPosition.setXY( x, y );
				_CGPartyPosition.setHP( static_cast<HP_t>(g_pPlayer->GetMAX_HP()), static_cast<HP_t>(g_pPlayer->GetHP()));

				g_pSocket->sendPacket( &_CGPartyPosition );
			}
		}
	}
}

//----------------------------------------------------------------------
// Send StatusInfo To Party
//----------------------------------------------------------------------
// Send my status info to party members. When? (sigh)
//----------------------------------------------------------------------
void
SendStatusInfoToParty()
{
	static MonotonicClock::TimePoint nextTime = g_FrameNow;

	if (g_FrameNow >= nextTime)
	{
		//------------------------------------------------------
		// When sending coordinates continuously
		//------------------------------------------------------		
		if (g_pParty->GetSize()!=0
			&& g_pClientCommunicationManager!=NULL)
		{
			static int oldHP = -1;
			static int oldMaxHP = -1;
			
			int HP		= g_pPlayer->GetHP();
			int MaxHP	= g_pPlayer->GetMAX_HP();
			
			// Send if the coordinates changed.
			if (oldHP!=HP || oldMaxHP!=MaxHP)
			{
				if(g_pUserInformation->bKorean == true && !NetworkTransport::UsesWebSocket())
				{
					RCStatusHP _RCStatusHP;

					// No need to send the name since it is checked by IP.
					//_RCStatusHP.setName( g_pUserInformation->CharacterID.GetString() );
					_RCStatusHP.setCurrentHP( HP );
					_RCStatusHP.setMaxHP( MaxHP );
					
					// Remember the sent information
					oldHP = HP;
					oldMaxHP = MaxHP;
					
					for (int i=0; i<g_pParty->GetSize(); i++)
					{
						PARTY_INFO* pInfo = g_pParty->GetMemberInfo( i );
						
						if (pInfo!=NULL
							// Send status info only when out of sight.
							// Might be meaningless.. (sigh)
							//&& !pInfo->bInSight
							)
						{
							int port = g_pClientConfig->CLIENT_COMMUNICATION_UDP_PORT;

							RequestUserInfo* pUserInfo = g_pRequestUserManager->GetUserInfo( pInfo->Name.GetString() );

							if (pUserInfo!=NULL)
							{
								if (pUserInfo->UDPPort!=0)
									port = pUserInfo->UDPPort;
							}

							g_pClientCommunicationManager->sendPacket( pInfo->IP, 
																	port, 
																	&_RCStatusHP );
						}
					}	
				}
				else // not Korean
				{
					int x		= g_pPlayer->GetX();
					int y		= g_pPlayer->GetY();
					int zoneID	= (g_bZonePlayerInLarge?g_nZoneLarge : g_nZoneSmall);

					CGPartyPosition _CGPartyPosition;

					_CGPartyPosition.setZoneID ( zoneID );
					_CGPartyPosition.setXY( x, y );
					_CGPartyPosition.setHP( static_cast<HP_t>(g_pPlayer->GetMAX_HP()), static_cast<HP_t>(g_pPlayer->GetHP()));

					g_pSocket->sendPacket( &_CGPartyPosition );
				}
			}
		}

		// HP of members out of sight, so
		// refresh it every five seconds
		nextTime = g_FrameNow + MonotonicClock::Millis(g_pClientConfig->CLIENT_COMMUNICATION_STATUS_DELAY);
	}
}

//----------------------------------------------------------------------
// Send StatusInfo To Party
//----------------------------------------------------------------------
// Send my status info to party members. When? (sigh)
//----------------------------------------------------------------------
void
SendCharacterInfoToParty()
{
	//------------------------------------------------------
	// When sending coordinates continuously
	//------------------------------------------------------		
	if (g_pParty->GetSize()!=0
		&& g_pClientCommunicationManager!=NULL)
	{
		int guildID	= g_pPlayer->GetGuildNumber();
		if (guildID>0)
		{
			RCCharacterInfo _RCCharacterInfo;

			// No need to send the name since it is checked by IP.
			//_RCStatusHP.setName( g_pUserInformation->CharacterID.GetString() );
			_RCCharacterInfo.setGuildID( guildID );
			
			for (int i=0; i<g_pParty->GetSize(); i++)
			{
				PARTY_INFO* pInfo = g_pParty->GetMemberInfo( i );
				
				if (pInfo!=NULL
					// Send status info only when out of sight.
					// Might be meaningless.. (sigh)
					//&& !pInfo->bInSight
					)
				{
					int port = g_pClientConfig->CLIENT_COMMUNICATION_UDP_PORT;

					RequestUserInfo* pUserInfo = g_pRequestUserManager->GetUserInfo( pInfo->Name.GetString() );

					if (pUserInfo!=NULL)
					{
						if (pUserInfo->UDPPort!=0)
							port = pUserInfo->UDPPort;
					}

					g_pClientCommunicationManager->sendPacket( pInfo->IP, 
															port, 
															&_RCCharacterInfo );
				}
			}
		}
	}
}

//----------------------------------------------------------------------
// Is Block All Direction
//----------------------------------------------------------------------
extern POINT g_DirectionValue[MAX_DIRECTION];
bool
IsBlockAllDirection()
{
	if (g_pPlayer!=NULL && g_pZone!=NULL)
	{
		int x = g_pPlayer->GetX();
		int y = g_pPlayer->GetY();

		for (int i=0; i<8; i++)
		{
			const POINT& pt = g_DirectionValue[i];

			int sx = x + pt.x;
			int sy = y + pt.y;

			if (g_pZone->CanMove( g_pPlayer->GetMoveType(), sx, sy ))
			{
				return false;
			}
		}
	}

	return true;
}

//----------------------------------------------------------------------
// 
// contructor/destructor
//
//----------------------------------------------------------------------
MPlayer::MPlayer()
{	
	// Trace info
	m_fTrace				= FLAG_TRACE_NULL;
	m_TraceDistance			= 0;
	m_TraceObjectAction		= 0;
	m_fTraceBuffer			= FLAG_TRACE_NULL;

	m_BasicAttackDistance	= 1;

	// Next trace info
	m_fNextTrace			= FLAG_TRACE_NULL;
	m_NextTraceID			= OBJECTID_NULL;
	m_NextTraceX			= SECTORPOSITION_NULL;
	m_NextTraceY			= SECTORPOSITION_NULL;
	m_NextTraceZ			= 0;
	m_NextTraceObjectAction	= 0;

	
	// Destination
	m_DestX = SECTORPOSITION_NULL;
	m_DestY = SECTORPOSITION_NULL;

	// Next destination
	m_NextDestX = SECTORPOSITION_NULL;
	m_NextDestY = SECTORPOSITION_NULL;

	// Places that could not be reached..
	m_BlockDestX = SECTORPOSITION_NULL;
	m_BlockDestY = SECTORPOSITION_NULL;

	// Number of messages sent to the server
	m_SendMove = 0;

	// the action delay
	m_DelayTime	= MonotonicClock::TimePoint();
	
	// attack mode
	m_AttackMode = ATTACK_MODE_NORMAL;

	// Item just picked up
	m_ItemCheckBufferStatus = ITEM_CHECK_BUFFER_NULL;
	m_pItemCheckBuffer = NULL;
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
		m_dwSubItemIDCheckBuffer = OBJECTID_NULL;
	#endif
	// Target of skill use
	m_pEffectTarget = NULL;

	// Waiting on the server
	m_WaitVerify = WAIT_VERIFY_NULL;
	m_WaitVerifyActionInfo = ACTIONINFO_NULL;

	// Keep tracing..
	m_bKeepTraceCreature = true;
	m_bTraceCreatureToForceAttack = false;
	m_bNextForceAttack = false;
	m_TraceIDBuffer = OBJECTID_NULL;

	// Repeat action
	m_bRepeatAction		= FALSE;

	// when the change to vampire is due
	m_ConversionDelayTime = MonotonicClock::TimePoint();

	m_nNoPacketUsedActionInfo = ACTIONINFO_NULL;

	m_bLockMode = false;

	//-------------------------------------------------------
	// Light radius sight
	//-------------------------------------------------------
	m_TimeLightSight = 0;
//	m_TimeLightSightX = SECTORPOSITION_NULL;
//	m_TimeLightSightY = SECTORPOSITION_NULL;	
	m_ItemLightSight = 0;
	m_LightSight = 0;

	// Character sight
	m_Sight = 0;
	m_PetDelayTime = MonotonicClock::TimePoint();

	m_SweepVice_Value = 0;
	m_TempSelectPosX = SECTORPOSITION_NULL;
	m_TempSelectPosY = SECTORPOSITION_NULL;
	m_SweepViceX = SECTORPOSITION_NULL;
	m_SweepViceY = SECTORPOSITION_NULL;
	m_bShowAdamCloud = false;

	m_ResurrectZoneID = 0;
}

MPlayer::~MPlayer()
{
	// Ugh~~ a killer hack (sigh)
	// When an Effect is deleted,
	// it accesses g_pPlayer to remove the player's EffectTarget, so..
	// The structure should be changed.. (sob) no time to fix.. (sigh)
	g_pUserInformation->bCompetence = false;
	g_pUserInformation->bCompetenceShape = false;
	g_pPlayer = NULL;

	// remove priority queue 
	RemoveNodes();

	// remove list
	m_listDirection.clear();
	m_listSendDirection.clear();

	// Skill use target
	if (m_pEffectTarget!=NULL)
	{
		MEffectTarget* pTempEffectTarget = m_pEffectTarget;
		m_pEffectTarget = NULL;

		delete pTempEffectTarget;
	}
}


//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Set the Zone in which the MPlayer exists.
//----------------------------------------------------------------------
void	
MPlayer::SetZone(MZone* pZone)
{
	// base class functions
	MCreature::SetZone(pZone);

}

//----------------------------------------------------------------------
// Stop the Player.
//----------------------------------------------------------------------
void
MPlayer::SetStop()
{ 
	// Remove pathfinding
	// 2004, 9, 1, sobeit add start - prevent dying while standing? (heh)
	m_bKnockBack = 0;

	// 2004, 9, 1, sobeit add end

	m_listDirection.clear(); 

	// Stop tracing
	m_fTrace		= FLAG_TRACE_NULL;
	//m_fNextTrace	= FLAG_TRACE_NULL;

	// Stop Action
	m_sX=0; 
	m_sY=0;

	int tempAction = m_MoveAction==ACTION_SLAYER_MOTOR_MOVE? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND;
	if(IsOusters() && HasEffectStatusSummonSylph( dynamic_cast<MCreature*>(this) ))
		tempAction = ACTION_OUSTERS_FAST_MOVE_STAND;

	m_Action		= tempAction;
	// 2004, 11, 3, sobeit modify start - set m_ActionCount to the max count of the changed action - because of the Ousters ghost bug..
	m_ActionCount	= m_ActionCountMax; 
	//m_ActionCount	= (*g_pCreatureTable)[m_CreatureType].GetActionCount( m_Action );
	// 2004, 11, 3, sobeit modify end
	m_MoveCount		= m_MoveCountMax; 

	// Also remove the next action
	m_bNextAction = false;
	m_NextX = SECTORPOSITION_NULL;
	m_NextY = SECTORPOSITION_NULL;
	
	//m_nNextUsedActionInfo = ACTIONINFO_NULL;
	//m_nUsedActionInfo = ACTIONINFO_NULL;

	
	// Remove target
	m_DestX			= SECTORPOSITION_NULL; 
	m_DestY			= SECTORPOSITION_NULL; 
	m_NextDestX		= SECTORPOSITION_NULL; 
	m_NextDestY		= SECTORPOSITION_NULL; 
}


//----------------------------------------------------------------------
// Delete m_pqDNodes, the priority queue.
//----------------------------------------------------------------------
void
MPlayer::RemoveNodes()
{
	while (!m_pqDNodes.empty())
	{
		//DNode* pDNode = m_pqDNodes.top();
		m_pqDNodes.pop();
		//delete pDNode;		
	}


	// Delete temp.
	DNODE_LIST::iterator iNode = m_listDNodes.begin();

	while (iNode != m_listDNodes.end())
	{
		DNode* pNode = *iNode;
		delete pNode;
		iNode++;		
	}

	m_listDNodes.clear();
}

//----------------------------------------------------------------------
// Get the distance from (x,y) to m_Dest(X,Y).
//----------------------------------------------------------------------
// Horizontal, vertical and diagonal all cost 1, so the largest value is the distance,
// and being on the same coordinate as the target is better, so add +1 otherwise.
//----------------------------------------------------------------------
int
MPlayer::CalculateDistance(int x, int y)
{
	//int xx = x-m_DestX,
	//	yy = y-m_DestY;

	//return abs(xx)+abs(yy);

	//return sqrt(xx*xx + yy*yy);
//	return xx*xx + yy*yy;

//	/*
	int xd = abs(x-m_DestX),
		yd = abs(y-m_DestY);	 

	int plus = !(m_DestX==x) + !(m_DestY==y);
		
	if (xd>yd)
	{
		return xd*xd + plus;
	}
	else return yd*yd + plus;

	return 0;
//	*/
}

//----------------------------------------------------------------------
// Get Destination()
//----------------------------------------------------------------------
void
MPlayer::GetDestination(POINT &dest)
{
	dest.x = m_DestX;
	dest.y = m_DestY;
}

//----------------------------------------------------------------------
// Set Move Position
//----------------------------------------------------------------------
bool	
MPlayer::SetMovePosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	// When not alive..
	if	(
		(
		!m_bAlive
		// When inside a casket
		|| m_bInCasket
		// When underground
		|| IsUndergroundCreature()
		|| HasEffectStatus(EFFECTSTATUS_SLEEP)
		)
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		)
	{
		return false;
	}

	// When there is a skill to use next...
	//if (m_nUsedActionInfo!=ACTIONINFO_NULL)
	if (m_ActionCount>=m_ActionCountMax)
	{
		m_fTraceBuffer = FLAG_TRACE_NULL;
		m_TraceIDBuffer = OBJECTID_NULL;
		m_nNextUsedActionInfo = ACTIONINFO_NULL;

		//if (m_ActionCount==m_ActionCountMax)
		//{			
			m_nUsedActionInfo = ACTIONINFO_NULL;

			/*
			DEBUG_ADD("Clear m_nUsedActionInfo");
		}
		else
		{
			DEBUG_ADD_FORMAT("Not Clear : action=%d, %d/%d", (int)m_Action, (int)m_ActionCount, (int)m_ActionCountMax);
		}
		*/
		TraceNULL();
	}
	
	return SetNextDestination(sX, sY);
}

//----------------------------------------------------------------------
// Set Next Destination
//----------------------------------------------------------------------
// Decide the next target position to move to.
// What to do when the target position is unreachable?
//----------------------------------------------------------------------
bool	
MPlayer::SetNextDestination(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{ 
	//DEBUG_ADD_FORMAT("[Set] NextDestination (%d, %d) --> (%d, %d)", m_X, m_Y, sX, sY);

	// When not alive..
	if	(
		(!m_bAlive || HasEffectStatus(EFFECTSTATUS_SLEEP))
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		)
	{
		return false;
	}

	// When there is a skill to use next...
	if (m_nUsedActionInfo!=ACTIONINFO_NULL)
	{
		m_fTraceBuffer = FLAG_TRACE_NULL;
		m_TraceIDBuffer = OBJECTID_NULL;
		m_nNextUsedActionInfo = ACTIONINFO_NULL;

		if (m_ActionCount>=m_ActionCountMax)
		{			
			m_nUsedActionInfo = ACTIONINFO_NULL;

			DEBUG_ADD("Clear m_nUsedActionInfo");			
		}
		else
		{
			DEBUG_ADD_FORMAT("Not Clear : action=%d, %d/%d", (int)m_Action, (int)m_ActionCount, (int)m_ActionCountMax);			
		}
		
		TraceNULL();
	}
	
	// Not tracing, and
	// no pathfinding needed if already at the target position.
	if (m_fTrace==FLAG_TRACE_NULL && sX==m_X && sY==m_Y)
	{
		return false;
	}	

	// Invalid value
	// Place that could not be reached before..
	if (sX==SECTORPOSITION_NULL || sY==SECTORPOSITION_NULL
		|| (sX==m_BlockDestX && sY==m_BlockDestY))
	{
		return false;
	}

	//---------------------------------------------------
	// [Method 1]
	// If the target point is unreachable,
	// move to the closest point to the target.
	//---------------------------------------------------
	/*
	TYPE_SECTORPOSITION destX = sX;
	TYPE_SECTORPOSITION	destY = sY;	

	//---------------------------------------------------
	// Specify a reachable target point.
	//---------------------------------------------------
	while (!m_pZone->CanMove(m_MoveType, destX, destY))
	{
		// Direction from the target (sX,sY) to the current position
		BYTE direction = SelectFacingDirection(sX, sY, m_X, m_Y);

		GetPositionToDirection(destX, destY, direction);

		// Completely (!) unreachable.
		if (destX==m_X && destY==m_Y)
			return false;
	}
	m_NextDestX = destX; 
	m_NextDestY = destY; 
	*/

	if(m_fTrace==FLAG_TRACE_NULL)
	{
		BOOL bCanStand = m_pZone->CanMove(m_MoveType, sX,sY);
		if(!bCanStand)
		{
			POINT MovePoint;

			if( sY > GetY() )
				MovePoint.y = -1;
			else if( sY < GetY() )
				MovePoint.y = 1;
			else
				MovePoint.y = 0;

			if( sX > GetX() )
				MovePoint.x = -1;
			else if( sX < GetX() )
				MovePoint.x = 1;
			else
				MovePoint.x = 0;

			int limit = 20;	// give up if no valid value is found within 20 tries

			while( bCanStand == FALSE && !( GetX() == sX && GetY() == sY )
				&& sX < m_pZone->GetWidth() && sY < m_pZone->GetHeight()
				&& limit-- > 0
				)
			{
				if( GetX() != sX )sX = static_cast<TYPE_SECTORPOSITION>(sX + MovePoint.x);
				if( GetY() != sY )sY = static_cast<TYPE_SECTORPOSITION>(sY + MovePoint.y);
				bCanStand = m_pZone->CanMove( m_MoveType, sX,sY );
			};

			if( bCanStand == FALSE || (GetX() == sX && GetY() == sY))
				return false;
		}
	}
//	else
	//---------------------------------------------------
	// [Method 2]
	// Move one tile from the current position toward the target..
	//
	// However, when tracing... it doesn't matter..
	// Only while moving...
	//---------------------------------------------------
//	if (m_fTrace==FLAG_TRACE_NULL && !m_pZone->CanMove(m_MoveType, sX, sY))
//	{
//		BYTE direction = SelectFacingDirection(m_X, m_Y, sX, sY);
//
//		sX = m_X;
//		sY = m_Y;
//
//		GetPositionToDirection(sX, sY, direction);
//
//		// When unable to reach the new target point..
//		if (!m_pZone->CanMove(m_MoveType, sX, sY))
//		{
//			return false;
//		}
//	}


	
	m_NextDestX = sX; 
	m_NextDestY = sY; 

	return true;
}

//----------------------------------------------------------------------
// Reset Send Move
//----------------------------------------------------------------------
// Mark that there is no message awaited from the server.
//----------------------------------------------------------------------
void	
MPlayer::ResetSendMove()	
{ 
	g_bNetStatusGood=true; 
	m_SendMove = 0; 
	m_listSendDirection.clear(); 
}

//----------------------------------------------------------------------
// Get DeadDelay Last
//----------------------------------------------------------------------
// Waiting time after the Player dies..
//----------------------------------------------------------------------
DWORD			
MPlayer::GetDeadDelayLast() const
{
	const MonotonicClock::Duration left = m_DeadDelayTime - g_FrameNow;

	if (left <= MonotonicClock::Duration(0))
	{
		return 0;
	}

	return (DWORD)(left.count() / 1000);
}

//----------------------------------------------------------------------
// Get NextDestination()
//----------------------------------------------------------------------
void
MPlayer::GetNextDestination(POINT &dest)
{
	dest.x = m_NextDestX;
	dest.y = m_NextDestY;
}

//----------------------------------------------------------------------
// Get NextPosition()
//----------------------------------------------------------------------
// Among the paths determined by pathfinding,
// return the coordinates of the very next Sector to go to.
//
// Add the appropriate change for the 'next direction' to the 'current position'.
//----------------------------------------------------------------------
bool
MPlayer::GetNextPosition(POINT &next)
{
	// When no path has been decided
	if (m_listDirection.empty())
	{
		//next.x = SECTORPOSITION_NULL;
		//next.y = SECTORPOSITION_NULL;
		return false;
	}

	
	// When a path has been decided, return the next coordinates.

	// Set the target for the next direction from the current position
	next.x = m_X;
	next.y = m_Y;
	
	switch (m_listDirection.front())
	{
		case DIRECTION_LEFTDOWN		: next.x--;	next.y++;	break;
		case DIRECTION_RIGHTUP		: next.x++;	next.y--;	break;
		case DIRECTION_LEFTUP		: next.x--;	next.y--;	break;
		case DIRECTION_RIGHTDOWN	: next.x++;	next.y++;	break;
		case DIRECTION_LEFT			: next.x--;				break;
		case DIRECTION_DOWN			:			next.y++;	break;
		case DIRECTION_UP			:			next.y--;	break;
		case DIRECTION_RIGHT		: next.x++;				break;
	}

	return true;
}


//----------------------------------------------------------------------
// Set Destination(sX, sY)
//----------------------------------------------------------------------
// At the moment the target position is specified, decide the
// Best Path to it and store it in m_listDirection.
//----------------------------------------------------------------------
bool
MPlayer::SetDestination(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	if(HasEffectStatus(EFFECTSTATUS_SLEEP)
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		)
		return false;

	// message : find path
	//DEBUG_ADD_FORMAT("........Find Path........");				

	#ifdef OUTPUT_DEBUG_PLAYER_ACTION
		DEBUG_ADD_FORMAT("SetDest(%d, %d)", sX, sY);
	#endif

	//-------------------------------------------------------
	// When not alive..
	//-------------------------------------------------------
	if (!m_bAlive)
	{
		#ifdef OUTPUT_DEBUG_PLAYER_ACTION
			DEBUG_ADD("dead");
		#endif
		return false;
	}

	//-------------------------------------------------------
	// if not empty, delete all elements
	//-------------------------------------------------------
	if (!m_listDirection.empty())
	{
		m_listDirection.clear();
	}

		// While carrying a relic you cannot enter your own side's safe zone!
	// Whose safe zone the cell is matters on the map only: past it, the
	// check below refuses the move anyway.
	const bool bOnMap = sX < g_pZone->GetWidth() && sY < g_pZone->GetHeight();
	bool bHasRelic = bOnMap &&
		( m_bEffectStatus[EFFECTSTATUS_HAS_VAMPIRE_RELIC] || m_bEffectStatus[EFFECTSTATUS_HAS_SLAYER_RELIC] ) && 
		((g_pZone->GetSector(sX, sY).IsSafeSlayer() && IsSlayer()) ||
		(g_pZone->GetSector(sX, sY).IsSafeVampire() && IsVampire()) ||
		(g_pZone->GetSector(sX, sY).IsSafeOusters() && IsOusters())
		);

	bool bHasBloodBible = bOnMap &&
	(
	m_bEffectStatus[EFFECTSTATUS_HAS_BLOOD_BIBLE_ARMEGA] || m_bEffectStatus[EFFECTSTATUS_HAS_BLOOD_BIBLE_MIHOLE] ||
	m_bEffectStatus[EFFECTSTATUS_HAS_BLOOD_BIBLE_KIRO] ||m_bEffectStatus[EFFECTSTATUS_HAS_BLOOD_BIBLE_INI] ||
	m_bEffectStatus[EFFECTSTATUS_HAS_BLOOD_BIBLE_GREGORI] ||m_bEffectStatus[EFFECTSTATUS_HAS_BLOOD_BIBLE_CONCILIA] ||
	m_bEffectStatus[EFFECTSTATUS_HAS_BLOOD_BIBLE_LEGIOS] ||m_bEffectStatus[EFFECTSTATUS_HAS_BLOOD_BIBLE_HILLEL] ||
	m_bEffectStatus[EFFECTSTATUS_HAS_BLOOD_BIBLE_JAVE] ||m_bEffectStatus[EFFECTSTATUS_HAS_BLOOD_BIBLE_NEMA] ||
	m_bEffectStatus[EFFECTSTATUS_HAS_BLOOD_BIBLE_AROSA] ||m_bEffectStatus[EFFECTSTATUS_HAS_BLOOD_BIBLE_CHASPA] ) && 
		((g_pZone->GetSector(sX, sY).IsSafeSlayer() && IsSlayer()) ||
		(g_pZone->GetSector(sX, sY).IsSafeOusters() && IsOusters()) ||
		(g_pZone->GetSector(sX, sY).IsSafeVampire() && IsVampire())
		);

	if(bHasBloodBible)
	{
		g_pSystemMessage->Add((*g_pGameStringTable)[STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_BLOOD_BIBLE].GetString());
	}

	if(bHasRelic)
	{
		g_pSystemMessage->Add((*g_pGameStringTable)[STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_RELIC].GetString());
	}

	if( bOnMap && HasEffectStatus( EFFECTSTATUS_HAS_FLAG ) &&
		((g_pZone->GetSector(sX, sY).IsSafeSlayer() && IsSlayer()) ||
		(g_pZone->GetSector(sX, sY).IsSafeVampire() && IsVampire()) ||
		(g_pZone->GetSector(sX, sY).IsSafeOusters() && IsOusters())
		) )
	{
		bHasRelic = true;
		g_pSystemMessage->Add((*g_pGameStringTable)[STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_FLAG].GetString() );
	}

	//-------------------------------------------------------
	// Do not check if outside the Zone area.
	//-------------------------------------------------------
	if (sX>=m_pZone->GetWidth() || sY>=m_pZone->GetHeight()
		// [new skill] Cannot move into Sanctuary.
		|| g_pZone->GetSector(m_X, m_Y).HasSanctuary()
		|| bHasRelic
		|| bHasBloodBible
		)
	{
		// When not checking if outside the Zone area
		//m_ActionCount = m_ActionCountMax;
		m_MoveCount = m_MoveCountMax;
		m_DestX = SECTORPOSITION_NULL;
		m_DestY = SECTORPOSITION_NULL;

		#ifdef OUTPUT_DEBUG_PLAYER_ACTION
			DEBUG_ADD("outOfZone or Sanctuary");
		#endif

		return false;

		// When making it move in the current direction
		/*
		POINT position = WorldEvents::GetSelectedSector(g_x, g_y);
		BYTE direction = GetDirectionToPosition(position.x, position.y);		

		// Decide the position according to the direction.
		MCreature::GetNextPosition(direction, position);
		
		sX = position.x;
		sY = position.y;
		
		// When the current direction leaves the Zone
		if (sX<0 || sY<0 
			|| sX>=m_pZone->GetWidth() || sY>=m_pZone->GetHeight()) 
		{
			return false;
		}
		*/
	}

	//-------------------------------------------------------
	// When tracing and already within range...
	//-------------------------------------------------------
	if (m_fTrace!=FLAG_TRACE_NULL
		&& max(abs(m_TraceX-m_X), abs(m_TraceY-m_Y))<=m_TraceDistance)
	{
		//m_ActionCount = m_ActionCountMax;
		m_MoveCount = m_MoveCountMax;

		m_DestX = SECTORPOSITION_NULL;
		m_DestY = SECTORPOSITION_NULL;
	
		#ifdef OUTPUT_DEBUG_PLAYER_ACTION
			DEBUG_ADD("already in dist");
		#endif

		return false;
	}

	/*
	// priority queue test code
	ofstream file("test.txt", ios::app);
	CString str;

	m_pqDNodes.push( new DNode(0,0,0,1) );
	m_pqDNodes.push( new DNode(0,0,0,4) );
	m_pqDNodes.push( new DNode(0,0,0,3) );
	m_pqDNodes.push( new DNode(0,0,0,8) );
	m_pqDNodes.push( new DNode(0,0,0,2) );
	m_pqDNodes.push( new DNode(0,0,0,4) );
	m_pqDNodes.push( new DNode(0,0,0,7) );

	while (!m_pqDNodes.empty())
	{
		DNode*	pDNode = m_pqDNodes.top();
		m_pqDNodes.pop();

		str.Format("Pop : (%d,%d) , [%d], dist=%d", pDNode->x, pDNode->y, pDNode->direction, pDNode->distance);
		
		file << str << endl;
	}

	return;
	*/

	//--------------------------------------------------------------
	// If it is a reachable place
	//--------------------------------------------------------------
	// Best First Search
	//--------------------------------------------------------------
	BOOL	bCanStand = m_pZone->CanMove(m_MoveType, sX,sY);

	//--------------------------------------------------------------
	// If it is a reachable place, or
	// if tracing, find the path with Best Search~.
	//--------------------------------------------------------------
	if (bCanStand || m_fTrace!=FLAG_TRACE_NULL) 
	{		
		//-------------------------------------------------------
		// Store the target position
		//-------------------------------------------------------
		m_DestX = sX;
		m_DestY = sY;

		//-------------------------------------------------------
		// Initialize the priority queue.
		//-------------------------------------------------------
		RemoveNodes();	

		//-------------------------------------------------------
		// Set all visited to false,
		// and start visiting unvisited places from the current position.
		//-------------------------------------------------------
		POINT firstSector = WorldEvents::GetFirstSector();
		//int x0 = m_DestX - g_pClientConfig->MAX_FINDPATH_WIDTH, 
		//	x1 = m_DestX + g_pClientConfig->MAX_FINDPATH_WIDTH+1, 
		//	y0 = m_DestY - g_pClientConfig->MAX_FINDPATH_HEIGHT,
		//	y1 = m_DestY + g_pClientConfig->MAX_FINDPATH_HEIGHT+1;
		int x0 = firstSector.x - 1, 
			x1 = firstSector.x + g_SECTOR_WIDTH + 5, 
			y0 = firstSector.y - 1,
			y1 = firstSector.y + g_SECTOR_HEIGHT + 5;

		if (x0 < 0)						
			x0 = 0;
		if (x1 > g_pZone->GetWidth())	
			x1 = g_pZone->GetWidth();
		
		if (y0 < 0)				
			y0 = 0;
		if (y1 > g_pZone->GetHeight())	
			y1 = g_pZone->GetHeight();
		
		for (int i=y0; i<y1; i++)
		{			
			for (int j=x0; j<x1; j++)
			{
				//m_ppVisited[i][j] = false;
				g_pZone->UnSetVisitedFlag(j, i);
			}
			//memset( m_ppVisited[i]+x0, false, size );
		}
			

		//-------------------------------------------------------
		//
		// Current position
		//
		//-------------------------------------------------------

		//-------------------------------------------------------
		// Pathfind starting from the direction toward the destination.
		//-------------------------------------------------------
		static BYTE FirstDirection = GetDirectionToPosition(m_DestX, m_DestY);
		static TYPE_SECTORPOSITION previousDestX = SECTORPOSITION_NULL;
		static TYPE_SECTORPOSITION previousDestY = SECTORPOSITION_NULL;

		//-------------------------------------------------------
		// Only when the target position differs from the previous pathfinding,
		// set the direction toward the destination.
		//-------------------------------------------------------
		if (previousDestX!=m_DestX || previousDestY!=m_DestY)
		{
			FirstDirection = GetDirectionToPosition(m_DestX, m_DestY);
			previousDestX = m_DestX;
			previousDestY = m_DestY;
		}
		//-------------------------------------------------------
		// When heading toward the same target,
		// look starting from the current Player direction.
		//-------------------------------------------------------
		else
		{
			FirstDirection = m_Direction;
		}
		
		DNode*	currentDNode = new DNode(
									m_X,
									m_Y,
									FirstDirection,
									CalculateDistance(m_X, m_Y),
									0
									);
	
		//-------------------------------------------------------
		// Put in listDNodes to delete later.
		//-------------------------------------------------------
		m_listDNodes.push_back( currentDNode );

		currentDNode->SetParent(NULL);
		m_pqDNodes.push(currentDNode);	
		
		POINT	next;
		int		dist;
		bool	bFound = false;		// means not reached yet

		while (!bFound && !m_pqDNodes.empty())
		{
			//-------------------------------------------------------
			// Pick the best (!) Node (it is a priority queue)
			// and calculate where to go.
			//-------------------------------------------------------
			DNode* pDNode = m_pqDNodes.top();
			m_pqDNodes.pop();

			//-------------------------------------------------------
			// When fully arrived
			//-------------------------------------------------------
			if (pDNode->distance==0) 
			{
				m_pqDNodes.push(pDNode);
				bFound = true;
				break;
			}
			
			//-------------------------------------------------------
			// Approach within range
			//-------------------------------------------------------
			if (m_fTrace!=FLAG_TRACE_NULL && 
				max(abs(pDNode->x-m_DestX), abs(pDNode->y-m_DestY))<=m_TraceDistance)
			{
				m_pqDNodes.push(pDNode);
				bFound = true;
				break;
			}			

			int Direction = pDNode->direction;

			//-------------------------------------------------------
			// Check all 8 directions
			//-------------------------------------------------------
			for (int i=0; i<8; Direction++, i++)
			{		
				if (Direction==8) Direction=0;

				next.x = pDNode->x;
				next.y = pDNode->y;

				switch (Direction)
				{
					case DIRECTION_LEFTDOWN		: next.x--;	next.y++;	break;
					case DIRECTION_RIGHTUP		: next.x++;	next.y--;	break;
					case DIRECTION_LEFTUP		: next.x--;	next.y--;	break;
					case DIRECTION_RIGHTDOWN	: next.x++;	next.y++;	break;
					case DIRECTION_LEFT			: next.x--;				break;
					case DIRECTION_DOWN			: next.y++;				break;
					case DIRECTION_UP			: next.y--;				break;
					case DIRECTION_RIGHT		: next.x++;				break;
				}

				//-------------------------------------------------------
				// Do not check if outside the Zone area.
				//-------------------------------------------------------
				if (next.x<0 || next.y<0 
					|| next.x>=m_pZone->GetWidth() || next.y>=m_pZone->GetHeight()) continue;

				//-------------------------------------------------------
				// Do not go to places already visited.
				//-------------------------------------------------------
				if (g_pZone->IsVisitedFlag(static_cast<TYPE_SECTORPOSITION>(next.x), static_cast<TYPE_SECTORPOSITION>(next.y))) continue;

				// Do not check if it goes beyond one screen.
				if (next.x<x0 || next.y<y0 
					|| next.x>=x1 || next.y>=y1) continue;

				//-------------------------------------------------------
				// If reachable, put it in the pqueue.
				//-------------------------------------------------------
				if (m_pZone->CanMove(m_MoveType, static_cast<TYPE_SECTORPOSITION>(next.x), static_cast<TYPE_SECTORPOSITION>(next.y))
					&& !bHasRelic && !bHasBloodBible
					
					)
				{		
					g_pZone->SetVisitedFlag(static_cast<TYPE_SECTORPOSITION>(next.x), static_cast<TYPE_SECTORPOSITION>(next.y));
//					m_ppVisited[next.y][next.x] = true;

					//-------------------------------------------------------
					// Create a Node and add it to the priority queue.
					//-------------------------------------------------------
					// Distance calculation
					dist = CalculateDistance(next.x, next.y);

					// Create a Node and add it
					DNode*	nextDNode = new DNode(
											next.x, 
											next.y, 
											Direction,
											dist,
											pDNode->step + 1);
					
				
					//-------------------------------------------------------
					// Put in listDNodes to delete later.
					//-------------------------------------------------------
					m_listDNodes.push_back( nextDNode );
					
					//CString str;
					//str.Format("NewNode : (%d,%d)-->(%d,%d) , [%d], dist=%d", next.x, next.y, m_Dest.x, m_Dest.y, i, dist);

					nextDNode->SetParent(pDNode);
					m_pqDNodes.push(nextDNode);					
				}

			
			}
		}

		//-------------------------------------------------------
		// If found, the path must be decided.
		//-------------------------------------------------------
		if (bFound)
		{				
			//-------------------------------------------------------
			// Fill the list from m_pqDNodes. (must follow the parent)
			//-------------------------------------------------------

			// Target position
			DNode* currentNode = m_pqDNodes.top();
			m_pqDNodes.pop();
			
			while (currentNode->pParent!=NULL)
			{
				m_listDirection.insert( m_listDirection.begin(), currentNode->direction);
				currentNode = currentNode->pParent;			
			}

			RemoveNodes();

			m_BlockDestX = SECTORPOSITION_NULL;
			m_BlockDestY = SECTORPOSITION_NULL;
		}
		//-------------------------------------------------------
		// Stop if there is no path.
		//-------------------------------------------------------
		else
		{
			// Stop tracing
			if (m_fTrace!=FLAG_TRACE_NULL)				
			{
				m_MoveCount = m_MoveCountMax;

				TraceNULL();

				// added 2001.7.31
				m_nUsedActionInfo = ACTIONINFO_NULL;
				m_nNextUsedActionInfo = ACTIONINFO_NULL;
			}

			m_BlockDestX = m_DestX;
			m_BlockDestY = m_DestY;

			m_DestX = SECTORPOSITION_NULL;
			m_DestY = SECTORPOSITION_NULL;			
		}
	}
	// Unreachable place
	else 
	{
		//m_ActionCount = m_ActionCountMax;
		m_MoveCount = m_MoveCountMax;

		// Set the place that could not be reached this time
		//m_BlockDestX = m_DestX;
		//m_BlockDestY = m_DestY;

		m_DestX = SECTORPOSITION_NULL;
		m_DestY = SECTORPOSITION_NULL;		

		#ifdef OUTPUT_DEBUG_PLAYER_ACTION
			DEBUG_ADD("no way!");
		#endif

		return false;
	}

	/*
	else
	// There seems to be a slight error in this part!! (sigh)
	//--------------------------------------------------------------
	// If it is completely unreachable..
	//--------------------------------------------------------------
	// Walk in a straight line(?) from the Player position to the target point,
	// and stop when an obstacle appears.
	// (m_X, m_Y)  --->  (sX, sY)
	//--------------------------------------------------------------		
	{
		int	stepX = sX - m_X,
			stepY = sY - m_Y,
			x = m_X,
			y = m_Y,
			signX, signY;

		//-------------------------------------------------------
		// if not empty, delete all elements
		//-------------------------------------------------------
		if (!m_listDirection.empty())
		{
			m_listDirection.clear();
		}
			
		
		while (1)
		{
			// According to the sign..
			signX = (stepX==0)? 0 : (stepX<0)? -1 : 1;
			signY = (stepY==0)? 0 : (stepY<0)? -1 : 1;

			x += signX;
			y += signY;
				
			// If reachable, put it in the list.
			if (m_pZone->CanMove(x,y))
			{				
				m_listDirection.insert( m_listDirection.end(), DetermineDirection(stepX, stepY));
			}
			else 
			{
				// Set the position one tile before x,y as the target point.
				m_DestX = x - signX;
				m_DestY = y - signY;
				break;
			}	
			
			// Mark that it moved one tile
			stepX -= signX;
			stepY -= signY;
		}				
		
		return;
	}
	*/

	#ifdef OUTPUT_DEBUG_PLAYER_ACTION
		DEBUG_ADD("find way");
	#endif

	return true;
}

//----------------------------------------------------------------------
// Self Special Action
//----------------------------------------------------------------------
// Use a special skill on oneself
//----------------------------------------------------------------------
bool	
MPlayer::SelfSpecialAction()
{
	if(HasEffectStatus(EFFECTSTATUS_SLEEP)
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		)
		return false;

	//-----------------------------------------------------
	// Must not have a delay.
	//-----------------------------------------------------
	if (!IsNotDelay() || HasEffectStatus( EFFECTSTATUS_ETERNITY_PAUSE ) )
	{
		return false;
	}

	// [new skill 3] When inside a casket, only Open casket can be used.
	//if (IsInCasket())
//	{
//		m_nSpecialActionInfo = MAGIC_OPEN_CASKET;
//	}

	
	if (m_nSpecialActionInfo == ACTIONINFO_NULL
		// Special attacks are not allowed while riding a motorcycle.
		|| m_MoveDevice == MOVE_DEVICE_RIDE
		// Cannot be used if some skill use is awaiting verification.
		//|| m_WaitVerify != WAIT_VERIFY_NULL)	// 2001.8.20 commented out
		|| IsInSafeSector() == 2
		// Skills cannot be used in a complete safe zone.
		)
		return false;

	// Use only those that can be used on oneself.
	if ((*g_pActionInfoTable)[m_nSpecialActionInfo].IsTargetSelf())
	{	
		
		//-------------------------------------------------------
		// Make a mine
		//-------------------------------------------------------
		// skillID            itemType
		// MINE_ANKLE_KILLER,	// 5
		// MINE_POMZ,			// 6
		// MINE_AP_C1,			// 7
		// MINE_DIAMONDBACK,	// 8
		// MINE_SWIFT_EX		// 9
		//
		// Hard hard...
		//			
		if (m_nSpecialActionInfo==SKILL_INSTALL_MINE
			|| (m_nSpecialActionInfo>=MINE_ANKLE_KILLER 
				&& m_nSpecialActionInfo<=MINE_SWIFT_EX))
		{
			int					useSkill = SKILL_INSTALL_MINE;					

			//-------------------------------------------------------
			// Check whether the skill is currently usable..
			// Disallow it if it is a Passive skill..
			//-------------------------------------------------------
			if (g_pSkillAvailable->IsEnableSkill( (ACTIONINFO)useSkill )
				&& (*g_pSkillInfoTable)[useSkill].IsEnable()
				&& (*g_pSkillInfoTable)[useSkill].IsAvailableTime()
				&& !(*g_pSkillInfoTable)[useSkill].IsPassive()
				&& IsSlayer()
				&& !IsWaitVerify()
				&& IsItemCheckBufferNULL())
			{					

				ITEM_CLASS	itemClass = ITEM_CLASS_MINE;

				MItem* pItem = NULL;

				if (m_nSpecialActionInfo!=SKILL_INSTALL_MINE)
				{
					// Find a mine of a specific type.
					int itemType = m_nSpecialActionInfo-MINE_ANKLE_KILLER;

					MItemClassTypeFinder itemFinder(itemClass, itemType);
					pItem = ((MItemManager*)g_pInventory)->FindItem( itemFinder );
				}
				else
				{
					// Find any mine.
					MItemClassFinder itemFinder(itemClass);
					pItem = ((MItemManager*)g_pInventory)->FindItem( itemFinder );
				}
				
				// When there is a bomb to use
				if (pItem!=NULL)
				{
					// Prepare to install the mine..
					PlayerPresentation::StartInstallMineProgress( pItem->GetGridX(), pItem->GetGridY() );
				}									
			}

			// Do not set a repeat action.
			m_fNextTrace = FLAG_TRACE_NULL;
			UnSetRepeatAction();
			return false;	
		}

		//-------------------------------------------------------
		// Check whether the skill is currently usable..
		// Disallow it if it is a Passive skill..
		//-------------------------------------------------------
		if (!g_pSkillAvailable->IsEnableSkill( (ACTIONINFO)m_nSpecialActionInfo )
			|| !(*g_pSkillInfoTable)[m_nSpecialActionInfo].IsEnable()
			|| !(*g_pSkillInfoTable)[m_nSpecialActionInfo].IsAvailableTime()
			|| (*g_pSkillInfoTable)[m_nSpecialActionInfo].IsPassive())
		{
			m_fNextTrace = FLAG_TRACE_NULL;

			UnSetRepeatAction();

			return false;
		}

		if (IsStop()) // || m_Action!=ACTION_ATTACK)
		{
			//-------------------------------------------------------
			// Check for specific skills
			//-------------------------------------------------------
			switch (m_nSpecialActionInfo)
			{
				//-------------------------------------------------------
				// Portal
				//-------------------------------------------------------
				case MAGIC_BLOODY_MARK :
					if (IsVampire() && g_bHolyLand == false && g_pZoneTable->Get( m_pZone->GetID() )->CannotUseSpecialItem == false)
					{
						//----------------------------------------------------
						// When connected to the Server
						//----------------------------------------------------
							//----------------------------------------------------
							// When there is nothing to be verified..
							//----------------------------------------------------
							if (!IsWaitVerify()
								&& IsItemCheckBufferNULL())
							{									
								// Find an unused (Marked=false) VampirePortalItem.

							#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
								MItem* pSubInventory = NULL;
								MVampirePortalItemFinder finder1( false );
								MItem* pItem = ((MItemManager*)g_pInventory)->FindItemAll( finder1 , pSubInventory );
							#else
								MVampirePortalItemFinder finder1( false );
								MItem* pItem = ((MItemManager*)g_pInventory)->FindItem( finder1 );
							#endif
								
								
								
								if (pItem!=NULL && pItem->GetNumber()>0 && pItem->IsAffectStatus())
								{	
									#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
										DWORD SubInventoryItemID = 0;
									#endif
									//----------------------------------------------------
									// When connected to the Server
									//----------------------------------------------------
										CGSkillToInventory _CGSkillToInventory;
										_CGSkillToInventory.setObjectID( pItem->GetID() );
										_CGSkillToInventory.setX( pItem->GetGridX() );
										_CGSkillToInventory.setY( pItem->GetGridY() );
										_CGSkillToInventory.setSkillType( MAGIC_BLOODY_MARK );
										//_CGSkillToInventory.setCEffectID( 0 );	// -_-;;							
									#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
										if(NULL != pSubInventory)
										{
											SubInventoryItemID = pSubInventory->GetID();
										}
										_CGSkillToInventory.setInventoryItemObjectID( SubInventoryItemID );
									#endif
										g_pSocket->sendPacket( &_CGSkillToInventory );

										//----------------------------------------------------
										// Wait for verification of using the item from the Inventory.
										//----------------------------------------------------
										SetWaitVerify( WAIT_VERIFY_SKILL_SUCCESS );

									#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
										SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY, SubInventoryItemID);
									#else
										SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY);
									#endif


										if (auto* entry = g_pSkillInfoTable->GetMutable(m_nSpecialActionInfo)) {
											entry->SetNextAvailableTime();
										}
								}
							}
					}						
					
					return false;
				break;

				//-------------------------------------------------------
				// Portal
				//-------------------------------------------------------
				case MAGIC_BLOODY_TUNNEL :					
					if (IsVampire() && g_bHolyLand == false && g_pZoneTable->Get( m_pZone->GetID() )->CannotUseSpecialItem == false)
					{
							//----------------------------------------------------
							// When there is nothing to be verified..
							//----------------------------------------------------
							if (!IsWaitVerify()
								&& IsItemCheckBufferNULL())
							{		
								// Find a used (Marked=true) VampirePortalItem.
							#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
								MItem* pSubInventory = NULL;
								MVampirePortalItemFinder finder2( true );
								MItem* pItem = ((MItemManager*)g_pInventory)->FindItemAll( finder2 , pSubInventory );
							#else
								MVampirePortalItemFinder finder2( true );
								MItem* pItem = ((MItemManager*)g_pInventory)->FindItem( finder2 );
							#endif
								
								
								if (pItem!=NULL && pItem->GetNumber()>0 && pItem->IsAffectStatus())
								{				
									#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
										DWORD SubInventoryItemID = 0;
									#endif
										CGSkillToInventory _CGSkillToInventory;
										_CGSkillToInventory.setObjectID( pItem->GetID() );
										_CGSkillToInventory.setX( pItem->GetGridX() );
										_CGSkillToInventory.setY( pItem->GetGridY() );
										_CGSkillToInventory.setSkillType( MAGIC_BLOODY_TUNNEL );
										//_CGSkillToInventory.setCEffectID( 0 );	// -_-;;							
									#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
										if(NULL != pSubInventory)
										{
											SubInventoryItemID = pSubInventory->GetID();
										}
										_CGSkillToInventory.setInventoryItemObjectID( SubInventoryItemID );
									#endif
										g_pSocket->sendPacket( &_CGSkillToInventory );

										//----------------------------------------------------
										// Wait for verification of using the item from the Inventory.
										//----------------------------------------------------
										SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );

									#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
										SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY, SubInventoryItemID);
									#else
										SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY);
									#endif
										

										if (auto* entry = g_pSkillInfoTable->GetMutable(m_nSpecialActionInfo)) {
											entry->SetNextAvailableTime();
										}

								}
							}
					}						
					
					return false;
				break;

				//-------------------------------------------------------
				// Portal
				//-------------------------------------------------------
				case SUMMON_HELICOPTER :
					if (IsSlayer() && g_bHolyLand == false && g_pZoneTable->Get( m_pZone->GetID() )->CannotUseSpecialItem == false)
					{
							//----------------------------------------------------
							// When there is nothing to be verified..
							//----------------------------------------------------
							if (IsWaitVerifyNULL()
								&& IsItemCheckBufferNULL()
								&& g_pZone->GetHelicopter( GetID() )==NULL)				
							{		

							#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
								MItem* pSubInventory = NULL;
								MSlayerPortalItemFinder finder3;
								MItem* pItem = ((MItemManager*)g_pInventory)->FindItemAll( finder3 , pSubInventory);
							#else
								MSlayerPortalItemFinder finder3;
								MItem* pItem = ((MItemManager*)g_pInventory)->FindItem( finder3 );
							#endif
								
								
								if (pItem!=NULL && pItem->GetNumber()>0 && pItem->IsAffectStatus())
								{				
									#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
										DWORD SubInventoryItemID = 0;
									#endif
										CGUseItemFromInventory _CGUseItemFromInventory;
										_CGUseItemFromInventory.setObjectID( pItem->GetID() );
										_CGUseItemFromInventory.setX( pItem->GetGridX() );
										_CGUseItemFromInventory.setY( pItem->GetGridY() );

									#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
										if(NULL != pSubInventory)
										{
											SubInventoryItemID = pSubInventory->GetID();
										}
										_CGUseItemFromInventory.setInventoryItemObjectID( SubInventoryItemID );
									#endif

										g_pSocket->sendPacket( &_CGUseItemFromInventory );

										//----------------------------------------------------
										// Wait for verification of using the item from the Inventory.
										//----------------------------------------------------
									#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
										SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY, SubInventoryItemID);
									#else
										SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
									#endif
										
								}
							}
					}						
					
					return false;
				break;

				//-------------------------------------------------------
				// Make holy water
				//-------------------------------------------------------
				case MAGIC_CREATE_HOLY_WATER :				
					if (
						IsSlayer()
						&& !IsWaitVerify()
						&& IsItemCheckBufferNULL())
					{					
						MItem* pItem = NULL;

						for( int i = 0; i <= 2; i++ )
						{
							pItem = g_pInventory->FindItem( ITEM_CLASS_WATER,  i);
							if( pItem != NULL )
								break;
						}

						if (pItem!=NULL)
						{
//							POINT fitPoint;			// spot where the holy water goes

//							if (GetMakeItemFitPosition(pItem, 
//														ITEM_CLASS_HOLYWATER, 
//														pItem->GetItemType(), 
//														fitPoint))
							{
								CGSkillToInventory _CGSkillToInventory;
								_CGSkillToInventory.setObjectID( pItem->GetID() );
								_CGSkillToInventory.setX( pItem->GetGridX() );
								_CGSkillToInventory.setY( pItem->GetGridY() );
								_CGSkillToInventory.setTargetX( pItem->GetGridX() );
								_CGSkillToInventory.setTargetY( pItem->GetGridY() );
								_CGSkillToInventory.setSkillType( MAGIC_CREATE_HOLY_WATER );
								//_CGSkillToInventory.setCEffectID( 0 );	// -_-;;							

								g_pSocket->sendPacket( &_CGSkillToInventory );

								
								// For now (!) just remove it and see.
								//(*g_pInventory).RemoveItem( pItem->GetID() );

								//----------------------------------------------------
								// Wait for verification of using the item from the Inventory.
								//----------------------------------------------------
								SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );
								SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );

								if (auto* entry = g_pSkillInfoTable->GetMutable(MAGIC_CREATE_HOLY_WATER)) {
									entry->SetNextAvailableTime();
								}

								//----------------------------------------------------
								// Skill use attempt action
								//----------------------------------------------------
								WorldEvents::AddNewInventoryEffect( pItem->GetID(),
													MAGIC_CREATE_HOLY_WATER, //+ (*g_pActionInfoTable).GetMinResultActionInfo(),
													g_pClientConfig->FPS*3	// 3 seconds
												);
								// Do not set a repeat action.
								m_fNextTrace = FLAG_TRACE_NULL;
								UnSetRepeatAction();
							}
//							else
//							{
//								// g_pGameMessage( "No room~" );
//							}
						}						
					}
					
					return false;					
				break;
			case SKILL_CREATE_HOLY_POTION :				
					if (
						IsSlayer()
						&& !IsWaitVerify()
						&& IsItemCheckBufferNULL())
					{					
						
						MItem* pItem = NULL;

						for( int i = 3; i <= 6; i++ )
						{
							pItem = g_pInventory->FindItem( ITEM_CLASS_WATER,  i);
							if( pItem != NULL )
								break;
						}

						if (pItem!=NULL)
						{
							POINT fitPoint;			// spot where the holy water goes

							if (GetMakeItemFitPosition(pItem, 
														ITEM_CLASS_POTION, 
														(*g_pItemTable)[pItem->GetItemClass()][pItem->GetItemType()].Value3, 
														fitPoint))
							{
								CGSkillToInventory _CGSkillToInventory;
								_CGSkillToInventory.setObjectID( pItem->GetID() );
								_CGSkillToInventory.setX( pItem->GetGridX() );
								_CGSkillToInventory.setY( pItem->GetGridY() );
								_CGSkillToInventory.setTargetX( static_cast<Coord_t>(fitPoint.x) );
								_CGSkillToInventory.setTargetY( static_cast<Coord_t>(fitPoint.y) );
								_CGSkillToInventory.setSkillType( SKILL_CREATE_HOLY_POTION );
								//_CGSkillToInventory.setCEffectID( 0 );	// -_-;;							

								g_pSocket->sendPacket( &_CGSkillToInventory );

								
								// For now (!) just remove it and see.
								//(*g_pInventory).RemoveItem( pItem->GetID() );

								//----------------------------------------------------
								// Wait for verification of using the item from the Inventory.
								//----------------------------------------------------
								SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );
								SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );

								if (auto* entry = g_pSkillInfoTable->GetMutable(SKILL_CREATE_HOLY_POTION)) {
									entry->SetNextAvailableTime();
								}

								//----------------------------------------------------
								// Skill use attempt action
								//----------------------------------------------------
								WorldEvents::AddNewInventoryEffect( pItem->GetID(),
													SKILL_CREATE_HOLY_POTION, //+ (*g_pActionInfoTable).GetMinResultActionInfo(),
													g_pClientConfig->FPS*3	// 3 seconds
												);
								// Do not set a repeat action.
								m_fNextTrace = FLAG_TRACE_NULL;
								UnSetRepeatAction();
							}
							else
							{
								// g_pGameMessage( "No room~" );
							}
						}						
					}
					
					return false;					
				break;

				//-------------------------------------------------------
				// Make a bomb
				// Make a mine
				//-------------------------------------------------------
				case SKILL_MAKE_BOMB :
				case SKILL_MAKE_MINE :
					if (IsSlayer()
						&& !IsWaitVerify()
						&& IsItemCheckBufferNULL()
						&& !PlayerPresentation::IsInstallMineProgress())	// if a mine is being made..
						//|| gC_vs_ui.IsCreateMineProgress()	// if a mine is being made..
						//|| gC_vs_ui.IsCreateBombProgress()	// if a mine is being made..
					{					

						int					skillID;
						ITEM_CLASS	itemClass;
						int					minItemType;
						int					maxItemType;
						int					itemTypeModifier;
							
						// If itemType is 0~4 it is a bomb.
						if (m_nSpecialActionInfo==SKILL_MAKE_BOMB)
						{
							skillID = SKILL_MAKE_BOMB;
							itemClass = ITEM_CLASS_BOMB;
							minItemType = 0;
							maxItemType = 4;
							itemTypeModifier = 0;
						}
						else
						{
							skillID = SKILL_MAKE_MINE;
							itemClass = ITEM_CLASS_MINE;
							minItemType = 5;
							maxItemType = 9;
							itemTypeModifier = -5;
						}

						MItemClassRangeTypeFinder itemFinder(ITEM_CLASS_BOMB_MATERIAL,
																minItemType, maxItemType);

						MItem* pItem = ((MItemManager*)g_pInventory)->FindItem( itemFinder );

						if (pItem!=NULL)
						{
							POINT fitPoint;			// spot where it goes

							if (GetMakeItemFitPosition(pItem, 
														itemClass, 
														pItem->GetItemType() + itemTypeModifier, 
														fitPoint))
							{
								// The moment making starts...
								if (skillID==SKILL_MAKE_BOMB)
								{
									PlayerPresentation::StartCreateBombProgress( pItem->GetGridX(), pItem->GetGridY() );
								}
								else
								{
									PlayerPresentation::StartCreateMineProgress( pItem->GetGridX(), pItem->GetGridY() );
								}

								// After the bar is shown in the UI,
								// handle it where UI_ITEM_USE arrives.
								
								// To use continuously
//								m_fNextTrace	= FLAG_TRACE_SELF;
//				
//								m_bRepeatAction = FALSE;
								
								// Do not set a repeat action.
								m_fNextTrace = FLAG_TRACE_NULL;
								UnSetRepeatAction();
								
								/*
								CGSkillToInventory _CGSkillToInventory;
								_CGSkillToInventory.setObjectID( pItem->GetID() );
								_CGSkillToInventory.setX( pItem->GetGridX() );
								_CGSkillToInventory.setY( pItem->GetGridY() );
								_CGSkillToInventory.setTargetX( fitPoint.x );
								_CGSkillToInventory.setTargetY( fitPoint.y );
								_CGSkillToInventory.setSkillType( skillID );
								//_CGSkillToInventory.setCEffectID( 0 );	// -_-;;							

								g_pSocket->sendPacket( &_CGSkillToInventory );

								
								// For now (!) just remove it and see.
								//(*g_pInventory).RemoveItem( pItem->GetID() );

								//----------------------------------------------------
								// Wait for verification of using the item from the Inventory.
								//----------------------------------------------------
								SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );
								SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );

								(*g_pSkillInfoTable)[skillID].SetNextAvailableTime();

								//----------------------------------------------------
								// Skill use attempt action
								//----------------------------------------------------
								WorldEvents::AddNewInventoryEffect( pItem->GetID(),
													skillID, //+ (*g_pActionInfoTable).GetMinResultActionInfo(),
													g_pClientConfig->FPS*3	// 3 seconds
												);
								*/
							}
							else
							{
								// g_pGameMessage( "No room~" );
							}
						}						
					}
					
					return false;					
				break;

				//-------------------------------------------------------
				// Bat or wolf transformation
				//-------------------------------------------------------
				case SKILL_TRANSFORM_TO_WERWOLF :
				case MAGIC_TRANSFORM_TO_WOLF :
				case MAGIC_TRANSFORM_TO_BAT :					
					//----------------------------------------------------
					//
					//					Transformation item - for a vampire
					//
					//----------------------------------------------------
					if (IsVampire() && g_pZoneTable->Get( m_pZone->GetID() )->CannotUseSpecialItem == false)
					{
							//----------------------------------------------------
							// When there is nothing to be verified..
							//----------------------------------------------------
							if (!IsWaitVerify()
								&& IsItemCheckBufferNULL())
							{		
								MItem* pItem = NULL;

							#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
								MItem* pSubInventory = NULL;
							#endif

								if (m_nSpecialActionInfo==MAGIC_TRANSFORM_TO_WOLF)
								{
									// Find the wolf claw
									pItem = g_pInventory->FindItem(ITEM_CLASS_VAMPIRE_ETC, 0);
								}
								else if ( m_nSpecialActionInfo == SKILL_TRANSFORM_TO_WERWOLF )
								{
									pItem = g_pInventory->FindItem(ITEM_CLASS_SKULL, 39);
								}
								else
								{
								#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
									pItem = g_pInventory->FindItemAll( MItemClassTypeFinder(ITEM_CLASS_VAMPIRE_ETC , 1), pSubInventory );
								#else
									pItem = g_pInventory->FindItem(ITEM_CLASS_VAMPIRE_ETC, 1);
								#endif
									
								}

								if (pItem!=NULL)
								{			
								#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
									DWORD dwSubInventoryID = 0;
								#endif
									CGSkillToInventory _CGSkillToInventory;
									_CGSkillToInventory.setObjectID( pItem->GetID() );
									_CGSkillToInventory.setX( pItem->GetGridX() );
									_CGSkillToInventory.setY( pItem->GetGridY() );
									_CGSkillToInventory.setSkillType( m_nSpecialActionInfo );
									//_CGSkillToInventory.setCEffectID( 0 );	// -_-;;	
									
								#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
									if(NULL != pSubInventory)
										dwSubInventoryID = pSubInventory->GetID();

									_CGSkillToInventory.setInventoryItemObjectID(dwSubInventoryID);
								#endif

									g_pSocket->sendPacket( &_CGSkillToInventory );


									// For now (!) just remove it and see.
									//(*g_pInventory).RemoveItem( pItem->GetID() );

									//----------------------------------------------------
									// Wait for verification of using the item from the Inventory.
									//----------------------------------------------------
									SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );
									
								#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
									SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY , dwSubInventoryID);
								#else
									SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY );
								#endif
									

									if (auto* entry = g_pSkillInfoTable->GetMutable(m_nSpecialActionInfo)) {
										entry->SetNextAvailableTime();
									}

									//----------------------------------------------------
									// Skill use attempt action
									//----------------------------------------------------
									WorldEvents::AddNewInventoryEffect( pItem->GetID(),
														m_nSpecialActionInfo, //+ (*g_pActionInfoTable).GetMinResultActionInfo(),
														g_pClientConfig->FPS*3	// 3 seconds
													);
								}
							}
					}						
					
					return false;
				break;

				//----------------------------------------------------
				//
				// Release transformation
				// 
				//----------------------------------------------------
				case MAGIC_UN_TRANSFORM :
					if (IsVampire())
					{
						const MSector& sector = m_pZone->GetSector(m_X, m_Y);
						
						if (sector.IsBlockGround())
						{
							m_fNextTrace = FLAG_TRACE_NULL;

							UnSetRepeatAction();

							return false;
						}						
					}
				break;
				
				case SKILL_SUMMON_SYLPH :
					if( IsOusters() && !HasEffectStatus( EFFECTSTATUS_HAS_FLAG )
						//----------------------------------------------------
						// When there is nothing to be verified..
						//----------------------------------------------------
						&& IsWaitVerifyNULL()
						&& IsItemCheckBufferNULL()
						)
					{
						//MItem* pItem = g_pInventory->FindItem( ITEM_CLASS_OUSTERS_SUMMON_ITEM );

					#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
						MItem* pSubInventory = NULL;
						MOustersSummonGemItemFinder finder4;
						MItem* pItem = ((MItemManager*)g_pInventory)->FindItemAll( finder4, pSubInventory );
					#else
						MOustersSummonGemItemFinder finder4;
						MItem* pItem = ((MItemManager*)g_pInventory)->FindItem( finder4 );
					#endif
						
						if( pItem != NULL )
						{
							if( pItem->IsAffectStatus() && pItem->IsChargeItem() && pItem->GetNumber() > 0 )
							{
								
							#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
								DWORD SubInventoryItemID = 0;
							#endif

								CGUseItemFromInventory _CGUseItemFromInventory;
								_CGUseItemFromInventory.setObjectID( pItem->GetID() );
								_CGUseItemFromInventory.setX( pItem->GetGridX() );
								_CGUseItemFromInventory.setY( pItem->GetGridY() );

							#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
								if(NULL != pSubInventory)
								{
									SubInventoryItemID = pSubInventory->GetID();
								}

								_CGUseItemFromInventory.setInventoryItemObjectID( SubInventoryItemID );
							#endif

								g_pSocket->sendPacket( &_CGUseItemFromInventory );

							#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
								SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY, SubInventoryItemID);
							#else
								SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
							#endif

								if (auto* entry = g_pSkillInfoTable->GetMutable(SKILL_SUMMON_SYLPH)) {
									entry->SetAvailableTime( 4000 );
								}
							}
						} else
						{
							
						}
					}
					return false;
				break;
				//-----------------------------------------------------
				//
				// Trace
				//
				//-----------------------------------------------------
				case SKILL_SOUL_CHAIN :
					// No distinction between vampire and slayer.

					if (!IsWaitVerify())
					{
						// Show the UI window.
						// After showing the UI window, when the message arrives, do the sendpacket there.
						PlayerPresentation::RunTraceWindow();
						
//						int skillID = SKILL_SOUL_CHAIN;
//						
//						if (skillID < MIN_RESULT_ACTIONINFO)
//						{
//							(*g_pSkillInfoTable)[skillID].SetAvailableTime();
//						}
						
					}
					return false;
					
					break;

				case SKILL_LOVE_CHAIN :
					//----------------------------------------------------
					// When there is nothing to be verified..
					//----------------------------------------------------
					if (!IsWaitVerify()
						&& IsItemCheckBufferNULL())
					{	
						ITEM_CLASS ItemClass;
						switch( GetRace() )
						{
						case RACE_SLAYER :
							ItemClass = ITEM_CLASS_COUPLE_RING;
							break;
						case RACE_VAMPIRE :
							ItemClass = ITEM_CLASS_VAMPIRE_COUPLE_RING;
							break;
						default :
							ItemClass = ITEM_CLASS_COUPLE_RING;
							break;
						}
						MPlayerGear* pGear = GetGear();
						MItemClassFinder itemFinder( ItemClass );
						
						MItem *pItem = pGear->FindItem( itemFinder );
						if(pItem == NULL || !pItem->IsAffectStatus())
							return false;

						CGUseItemFromGear _CGUseItemFromGear;

						_CGUseItemFromGear.setObjectID ( pItem->GetID() );
						_CGUseItemFromGear.setPart ( pItem->GetItemSlot() );
						
						g_pSocket->sendPacket( &_CGUseItemFromGear );

						SetItemCheckBuffer( pItem, ITEM_CHECK_BUFFER_USE_FROM_GEAR );
						return false;
					}
					break;
				// add by Coffee 2007-6-9 new skill item usage restriction (garbled)
// 				case SKILL_BLLODY_SCARIFY:
// 				case SKILL_BLOOD_CURSE:
// 					if (IsVampire() && !IsWaitVerify() && IsItemCheckBufferNULL())
// 					{
// 						MItem* pItem = NULL;
// 
// 					}
// 					return false;
// 					break;
// 				case SKILL_SHINE_SWORD:
// 				case SKILL_BOMB_CRASH_WALK:
// 				case SKILL_SATELLITE_BOMB:
// 				case SKILL_ILLUSION_INVERSION:
// 				case SKILL_HEAVEN_GROUND:
// 				case SKILL_DUMMY_DRAKE:
// 				case SKILL_HYDRO_CONVERGENCE:
// 				case SKILL_SUMMON_CLAY:
// 				case SKILL_HETER_CHAKRAM:
// 					break;
				// end add By Coffee 2007-6-9
			}			

			if( (*g_pActionInfoTable)[m_nSpecialActionInfo].GetParentActionInfo() != ACTIONINFO_NULL )
				m_nSpecialActionInfo = (*g_pActionInfoTable)[m_nSpecialActionInfo].GetParentActionInfo();

			// Set as the skill used
			m_nUsedActionInfo	= m_nSpecialActionInfo;

			// Initialize trace info
			SetTraceID( m_ID );
			m_fTrace	= FLAG_TRACE_SELF;
			m_TraceX	= m_X;//SECTORPOSITION_NULL;
			m_TraceY	= m_Y;//SECTORPOSITION_NULL;
			m_TraceZ	= 0;

			// Remember the target currently being traced.
			m_fTraceBuffer	= m_fTrace;	

			//------------------------------------------------------------
			// Depending on what the BasicAction is..
			// TraceDistance will vary with the current Player's abilities.
			//------------------------------------------------------------
			
			m_TraceDistance		= 0;
			
			//------------------------------------------------------------
			// Skill use Action display
			SetNextAction( GetActionInfoAction(m_nUsedActionInfo, true) );

			// Remove buffering.
			m_fNextTrace			= FLAG_TRACE_NULL;	

			//------------------------------------------------------------
			// Print message
			//------------------------------------------------------------
			DEBUG_ADD_FORMAT("Self SpecialAction");					
		}
		else
		{
			//------------------------------------------------------------
			// Buffer the next action when not in a repeat action
			//------------------------------------------------------------
			if (m_bRepeatAction)
			{
				m_fNextTrace	= FLAG_TRACE_NULL;

				return false;
			}
			else
			{
				m_fNextTrace	= FLAG_TRACE_SELF;
				
				m_bRepeatAction = FALSE;

				return false;
			}
		}

		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Keep Trace Creature
//----------------------------------------------------------------------
// Keep tracing the Creature that was being traced.
//----------------------------------------------------------------------
void
MPlayer::KeepTraceCreature()
{
	if (IsInDarkness())
	{
		TraceNULL();			
		SetStop();			
		UnSetRepeatAction();
			
		return;			
	}

	// Read the info of the Creature being traced.
	MCreature*	pCreature = m_pZone->GetCreature( m_TraceID );

	//GET_DYING_CREATURE( pCreature, m_TraceID );

	bool	bTraceTimer = false;

	if( m_TraceTimer != MonotonicClock::TimePoint() )
	{
		if( (DWORD)(MonotonicClock::Now() - m_TraceTimer).count() / 1000 > g_pClientConfig->TRACE_CHARACTER_LIMIT_TIME )
		{
			if( pCreature != NULL &&					// only for the same race
				((pCreature->IsSlayer() && IsSlayer()) ||
				(pCreature->IsVampire() && IsVampire()) ||
				(pCreature->IsOusters() && IsOusters()))
				)
			{
				bTraceTimer = true;
				m_TraceTimer = MonotonicClock::TimePoint();
			}			
		}
	}
	
	//-------------------------------------------------------
	// Stop tracing when the traced creature is gone. A player who is not
	// a vampire cannot follow a character into darkness without
	// LIGHTNESS, and in zone 3001 neither can a vampire; ghost mode
	// ignores those limits, but never a creature that no longer exists.
	//-------------------------------------------------------
	if (pCreature==NULL ||
		(((pCreature->IsInDarkness() && !pCreature->IsNPC() && 
		((!IsVampire() && !HasEffectStatus( EFFECTSTATUS_LIGHTNESS)) ||
		(IsVampire() && g_pZone->GetID() == 3001)))
		||bTraceTimer
		|| (pCreature->IsInGroundElemental() && pCreature->IsOusters() && !g_pPlayer->IsOusters()))
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		))
	{
		// Stop tracing
		TraceNULL();
			
		// Stop.
		SetStop();	
		
		UnSetRepeatAction();
		m_TraceTimer = MonotonicClock::TimePoint();
			
		return;			
	}			
	
	if (max(abs(pCreature->GetX()-m_X), 
			abs(pCreature->GetY()-m_Y)) > 1)
	{		
		TraceCreatureToBasicAction( m_TraceID, false );	
		
		WorldEvents::SetSelectedSectorNULL();
	}	
}


//----------------------------------------------------------------------
// Get ActionInfo Range
//----------------------------------------------------------------------
// How far away the player can use nActionInfo from, in tiles.
//----------------------------------------------------------------------
int		
MPlayer::GetActionInfoRange(TYPE_ACTIONINFO nActionInfo)
{
	if( nActionInfo == SKILL_HEAD_SHOT )
		return 3;
	
	if( (*g_pActionInfoTable)[nActionInfo].GetParentActionInfo() != ACTIONINFO_NULL )
		nActionInfo = (*g_pActionInfoTable)[nActionInfo].GetParentActionInfo();

	//-------------------------------------------------------------
	// Does it take the current weapon's range?
	//-------------------------------------------------------------
	if ((*g_pActionInfoTable)[nActionInfo].IsAffectCurrentWeaponRange())
	{
		return m_BasicAttackDistance;
		//nActionInfo = m_nBasicActionInfo;
	}
	
	const SKILLINFO_NODE& skillInfo = (*g_pSkillInfoTable)[nActionInfo];

	int minRange = skillInfo.minRange;
	int maxRange = skillInfo.maxRange;
	int skillLevel = skillInfo.GetExpLevel();
	
#ifdef __METROTECH_TEST__
	if(g_bLight)
		return maxRange;
#endif

	// The skills whose range the client computes its own way
	
	switch( nActionInfo )
	{
	case MAGIC_RAPID_GLIDING :
		return ( min(6, int(2+ (GetDEX() / 50)) ) );
		break;
	
	case MAGIC_BLOODY_ZENITH :
		return min(6, int(2 + GetSTR()/80 + GetDEX()/120 + GetINT()/240) );
		break;	
	
	case SKILL_SOUL_REBIRTH:
		return GetSoulRebirthRange(skillLevel, (*g_pSkillInfoTable)[SKILL_SOUL_REBIRTH_MASTERY].GetExpLevel());
		break;
	}
	
	const Race race = IsSlayer() ? RACE_SLAYER : (IsOusters() ? RACE_OUSTERS : RACE_VAMPIRE);

	return GetSkillRangeAtLevel(race, minRange, maxRange, skillLevel);

}

//----------------------------------------------------------------------
// Trace Creature To BasicAction
//----------------------------------------------------------------------
// Trace and attack one Creature with the basic attack (L-Click).
// If not standing still.. set it in Next.
//----------------------------------------------------------------------
bool
MPlayer::TraceCreatureToBasicAction(TYPE_OBJECTID id, bool bForceAttack, bool bClick)
{
//	if(bForceAttack && !g_pSDLInput->KeyDown(DIK_LSHIFT))
//	{
//		bForceAttack = false;
//	}
	
	if((HasEffectStatus(EFFECTSTATUS_SLEEP) || HasEffectStatus( EFFECTSTATUS_BLOCK_HEAD ) )
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		)
		return false;

	//-----------------------------------------------------
	// Must not have a delay.
	//-----------------------------------------------------
	if (!IsNotDelay() 
		|| IsInCasket()	// [new skill 3]
		|| HasEffectStatus( EFFECTSTATUS_ETERNITY_PAUSE )
		|| (IsInDarkness() && 
		!HasEffectStatus( EFFECTSTATUS_LIGHTNESS )
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		 ))
	{
		return false;
	}

	if (m_nBasicActionInfo == ACTIONINFO_NULL)
		return false;

	if (m_nUsedActionInfo!=ACTIONINFO_NULL)
		return false;

	if (IsStop()) // || m_Action!=ACTION_ATTACK)
	{		
		// Check whether the Creature exists in the Zone.
		MCreature*	pCreature = m_pZone->GetCreature(id);
		if(pCreature == NULL)
		{
			MFakeCreature *pFakeCreature = (MFakeCreature *)m_pZone->GetFakeCreature(id);
			if(pFakeCreature != NULL && pFakeCreature->GetOwnerID() != OBJECTID_NULL)	// it is a pet
			{
				MPetItem *pPetItem = pFakeCreature->GetPetItem();
				if(pPetItem != NULL)
				{
					PETINFO petInfo;
					// The countdown, as the pet window and the description panel show it.
					petInfo.HP = pPetItem->GetRemainingDurability();
					petInfo.ATTR = pPetItem->GetEnchantLevel();
					petInfo.ATTR_VALUE = pPetItem->GetSilver();
					petInfo.LEVEL = pPetItem->GetNumber();
					petInfo.FOODTYPE = pPetItem->GetPetFoodType();
					petInfo.EXP_REMAIN = pPetItem->GetPetExpRemain();
					petInfo.CUT_HEAD = pPetItem->IsCanCutHead();
					petInfo.GAMBLE = pPetItem->IsCanGamble();
					if(pPetItem->GetItemOptionListCount() == 0)
						petInfo.OPTION = -1;
					else
						petInfo.OPTION = pPetItem->GetItemOptionList().front();
					petInfo.ITEM_TYPE = pPetItem->GetItemType();
					petInfo.NAME = pPetItem->GetPetName();
					petInfo.ITEM_ID = pPetItem->GetID();
					petInfo.ATTACK = pPetItem->IsCanAttack();
					petInfo.NICK_NAME = pFakeCreature->GetNickName();
					if(pFakeCreature->GetOwnerID() == g_pPlayer->GetID())
						petInfo.ChangeAbleNickName = true;
					else
						petInfo.ChangeAbleNickName = false;
					PlayerPresentation::RunPetInfo(&petInfo);
				}
				return false;
			}
		}
		// 2004,5, 28 sobeit add start - other character info
		if(static_cast<MRequestMode::REQUEST_MODE>(WorldEvents::GetViewRequestMode()) == MRequestMode::REQUEST_INFO)
		{
			SetTraceID( id );
			SetRequestMode( static_cast<MRequestMode::REQUEST_MODE>(WorldEvents::GetViewRequestMode()) );
			BasicActionToCreature();				
			SetTraceID( 0 );
			return false;	
		}	
		// 2004,5, 28 sobeit add end
		//GET_DYING_CREATURE( pCreature, id );

		// When there is no creature
		// When the creature is already dead
		// As a Slayer I cannot chase a character that is inside Darkness.
		
		if( pCreature != NULL )
		{
			if( pCreature->GetObjectType() == MObject::TYPE_CREATURE )
			{
				if( (IsSlayer() && g_pZone->GetSector( GetX(), GetY() ).IsSafeSlayer() && 
					pCreature->IsSlayer() && !g_pZone->GetSector( pCreature->GetX(), pCreature->GetY() ).IsSafeSlayer() && pCreature->IsPlayerOnly()) ||
					(IsVampire() && g_pZone->GetSector( GetX(), GetY() ).IsSafeVampire() && 
					pCreature->IsVampire() && !g_pZone->GetSector( pCreature->GetX(), pCreature->GetY() ).IsSafeVampire() && pCreature->IsPlayerOnly()) ||
					(IsOusters() && g_pZone->GetSector( GetX(), GetY() ).IsSafeOusters() && 
					pCreature->IsOusters() && !g_pZone->GetSector( pCreature->GetX(), pCreature->GetY() ).IsSafeOusters() && pCreature->IsPlayerOnly())
					)
				{
					//						UnSetLockMode();
					UnSetRepeatAction();
					//						g_pTopView->SetSelectedNULL();
					return false;
				}
			}
		}
		
//		bool bOusters = IsOusters();
//		MItem *pOustersItem = g_pOustersGear->GetItem(MOustersGear::GEAR_OUSTERS_RIGHTHAND);
		if (pCreature==NULL
			|| pCreature->IsDead()
			|| (((pCreature->IsInDarkness() && !pCreature->IsNPC() &&
			( (!IsVampire() && !HasEffectStatus( EFFECTSTATUS_LIGHTNESS )) ||
			(IsVampire() &&g_pZone->GetID() == 3001)))
			|| (pCreature->IsInGroundElemental() && pCreature->IsOusters() && !g_pPlayer->IsOusters()))
			&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
			&& !g_bLight
#endif
			))
		{
			UnSetRepeatAction();
			
			m_fTraceBuffer	= FLAG_TRACE_NULL;	
			m_fNextTrace	= FLAG_TRACE_NULL;
			return false;
		}


		bool	bTraceCreatureToForceAttack;
		bool	bKeepTraceCreature;

		bool bOusters = IsOusters();
		MItem *pOustersItem = g_pOustersGear->GetItem(MOustersGear::GEAR_OUSTERS_RIGHTHAND);

		//------------------------------------------------
		// When chasing in order to Request
		//------------------------------------------------
		if (WorldEvents::IsViewRequestMode()
			// Only Players are allowed.
			//&& pCreature->GetCreatureType()<=CREATURETYPE_VAMPIRE_FEMALE
			&& (*g_pCreatureSpriteTable)[(*g_pCreatureTable)[m_CreatureType].SpriteTypes[0]].IsPlayerOnlySprite()
			// Not allowed when inside a casket.
			&& !pCreature->IsInCasket()
			// Not allowed when hiding.
			&& !pCreature->IsUndergroundCreature()
			&& pCreature->GetCreatureType() != CREATURETYPE_SLAYER_OPERATOR
			&& pCreature->GetCreatureType() != CREATURETYPE_VAMPIRE_OPERATOR
			&& pCreature->GetCreatureType() != CREATURETYPE_OUSTERS_OPERATOR

			// When I am neither a bat nor a wolf
			//&& m_CreatureType!=CREATURETYPE_BAT
			//&& m_CreatureType!=CREATURETYPE_WOLF
			)
		{
			bTraceCreatureToForceAttack = false;
			bKeepTraceCreature			= false;
			m_bNextForceAttack			= false;
			
			SetRequestMode( static_cast<MRequestMode::REQUEST_MODE>(WorldEvents::GetViewRequestMode()) );
		}
		//------------------------------------------------
		// Cannot attack while riding a motorcycle.
		// Cannot attack when a bat.
		//------------------------------------------------
		else if (m_MoveDevice == MOVE_DEVICE_RIDE 
				|| m_MoveDevice == MOVE_DEVICE_SUMMON_SYLPH || HasEffectStatusSummonSylph( dynamic_cast<MCreature*>(this) )
				|| m_CreatureType==CREATURETYPE_BAT
				|| !((bOusters && pOustersItem!=NULL && pOustersItem->GetItemClass() == ITEM_CLASS_OUSTERS_CHAKRAM) || !bOusters)
				|| CanActionByZoneInfo()
				)
		{
			bTraceCreatureToForceAttack = false;
			bKeepTraceCreature			= true;	
			
			UnSetRequestMode();
		}
		//------------------------------------------------
		// Is it a forced attack?
		//------------------------------------------------		
		else if (bForceAttack)
		{
//			if(! (IsSlayer() && g_pZone->GetSector( GetX(), GetY() ).IsSafeSlayer() && pCreature->IsSlayer() && pCreature->IsPlayerOnly()|| 
//				IsVampire() && g_pZone->GetSector( GetX(), GetY() ).IsSafeVampire() && pCreature->IsVampire() && pCreature->IsPlayerOnly()) )
				bTraceCreatureToForceAttack = true;

			bKeepTraceCreature			= false;			

			UnSetRequestMode();
		}
		//------------------------------------------------
		// Otherwise.. depending on the race..
		//------------------------------------------------
		else
		{
			bTraceCreatureToForceAttack = CanAttackTribe( pCreature )
										// A vampire can attack depending on the Guild, or..
										|| (IsVampire() && CanAttackGuild( pCreature ))
										|| g_pObjectSelector->IsWarEnemy( pCreature );
									//	|| m_pZone->IsFreePKZone();			// -_- Vanatul


			// Is it self-defense?
			bTraceCreatureToForceAttack |= g_pJusticeAttackManager->HasCreature( pCreature->GetName() );

			// Keep chasing? - if it is not an attack..
			bKeepTraceCreature			= !bTraceCreatureToForceAttack;	
		}

		// 2005, 1, 8, sobeit add start - among promoted Slayers, the healer-line Enchanter cannot make a normal attack. There is no motion..
		if(IsAdvancementClass())
		{
			const MCreatureWear::ADDON_INFO& TempaddonInfo = GetAddonInfo(ADDON_RIGHTHAND);
			MCreature*	pCreature = m_pZone->GetCreature(id);
			if(pCreature != NULL)
			{
				if(!pCreature->IsNPC() && IsSlayer() && (!TempaddonInfo.bAddon || TempaddonInfo.ItemClass == ITEM_CLASS_CROSS || TempaddonInfo.ItemClass == ITEM_CLASS_MACE)
					&& bTraceCreatureToForceAttack)
					return false;
			}
		}
		// 2005, 1, 8, sobeit add end
		//------------------------------------------------
		// For a gun attack
		// --> just check whether a magazine is set.
		//------------------------------------------------
		if (bKeepTraceCreature)
		{
			// Things to check when tracing...
		}
		// If it is not an NPC it is an attack... must check bullets.
		else if (!pCreature->IsNPC())
		{
			if(!IsRequestMode() && IsInSafeSector() == 2)
				return false;
			
			// Just make sure g_pCurrentMagazine is set to NULL.
			//if (IsSlayer())
			{
			//	const MItem* pWeapon = g_pSlayerGear->GetItem( MSlayerGear::GEAR_SLAYER_RIGHTHAND );
				
				//------------------------------------------------
				// Check bullets
				//------------------------------------------------
			//	if (pWeapon!=NULL					// has a weapon and
			//		&& pWeapon->IsGunItem()			// is a gun and
				//	&&	

				if (
					((IsOusters() && 
					g_pOustersGear->GetItem( MOustersGear::GEAR_OUSTERS_RIGHTHAND ) != NULL &&
					g_pOustersGear->GetItem( MOustersGear::GEAR_OUSTERS_RIGHTHAND )->GetItemClass() == ITEM_CLASS_OUSTERS_CHAKRAM) || 
					!IsOusters() ) 
					&& 	(*g_pActionInfoTable)[m_nBasicActionInfo].IsWeaponTypeGunAny() 
					)
				{
					if (!IsRequestMode() 
						
						&& g_pCurrentMagazine!=NULL)		// when no bullets are needed
					{
						if (g_pCurrentMagazine->GetNumber()==0)	// when there are no bullets
						{
							// When using a gun without a magazine
							WorldEvents::PlaySound( SOUND_ITEM_NO_MAGAZINE );

							m_fNextTrace	= FLAG_TRACE_NULL;

							// the delay for an empty magazine
							m_DelayTime	= g_FrameNow + MonotonicClock::Duration(GetActionInfoDelay(m_nBasicActionInfo));

							// [Help] When bullets run out
//							__BEGIN_HELP_EVENT
//								GameInfoPresentation::ExecuteHelpEvent( HE_ITEM_EMPTY_BULLET );
//							__END_HELP_EVENT

							m_fNextTrace = FLAG_TRACE_NULL;

							UnSetRepeatAction();

						
							return false;
						}
					}
				}
			}
			
			// Set as the skill used
			m_nNextUsedActionInfo	= GetBasicActionInfo(); //m_nBasicActionInfo;
		}
		
		// Set trace info
		SetTraceID( id );

		if( !bForceAttack && bClick )
		{
			m_TraceTimer = MonotonicClock::Now();
		}
		
		m_fTrace	= FLAG_TRACE_CREATURE_BASIC;
		m_TraceX	= pCreature->GetX();
		m_TraceY	= pCreature->GetY();
		m_TraceZ	= pCreature->GetZ();
		
		//------------------------------------------------------------
		// Is it a forced attack?
		// Keep tracing?
		//------------------------------------------------------------
		m_bTraceCreatureToForceAttack	= bTraceCreatureToForceAttack;
		m_bKeepTraceCreature			= bKeepTraceCreature;

		//------------------------------------------------------------
		// Trace target distance..
		//------------------------------------------------------------
		// Depending on what the BasicAction is..
		// TraceDistance will vary with the current Player's abilities.
		//
		// 1 if it is following,
		// otherwise depending on the basic attack..
		//
		// For an NPC the distance may differ.
		//------------------------------------------------------------
		if (m_bKeepTraceCreature)
		{
			m_TraceDistance = 1;
		}
		else
		{
			m_TraceDistance		= m_BasicAttackDistance;	
		}

		// NPC 
		if (pCreature->IsNPC())
		{
			if(pCreature->GetCreatureType() == 659)	// the Earth Elemental's horn is one tile
				m_TraceDistance		= 1;	
			else
				m_TraceDistance		= TRADE_DISTANCE_NPC;	
		}
		// When chasing in order to Trade
		else if (IsRequestMode())
		{
			m_TraceDistance		= TRADE_DISTANCE_PC;	
		}

		// Remember the target currently being traced.
		m_fTraceBuffer	= m_fTrace;	
		m_TraceIDBuffer = id;

		//------------------------------------------------------------

		if( HasEffectStatus( EFFECTSTATUS_BLOCK_HEAD ) )
			m_bTraceCreatureToForceAttack = false;


		// Set the tracing path
		SetAction( m_MoveAction );
		SetNextDestination(m_TraceX, m_TraceY);

		// Remove buffering.
		m_fNextTrace			= FLAG_TRACE_NULL;	

		//------------------------------------------------------------
		// Print message
		//------------------------------------------------------------
		DEBUG_ADD_FORMAT("Trace Creature To BasicAction : ID=%d, (%d, %d)", id, m_TraceX, m_TraceY);				

		//------------------------------------------------------------
		// Returning false means no repeat action.
		//------------------------------------------------------------
		if (m_bKeepTraceCreature)// || m_bTraceCreatureToForceAttack)
		{
			return false;
		}

		// Clear the selected sector
		WorldEvents::SetSelectedSectorNULL();
			

		return true;
	}
	else
	{
		//------------------------------------------------------------
		// Buffer the next action when not in a repeat action
		//------------------------------------------------------------
		if (m_bRepeatAction)
		{
			m_fNextTrace	= FLAG_TRACE_NULL;
			
			return false;
		}
		else
		{
			m_fNextTrace	= FLAG_TRACE_CREATURE_BASIC;
			m_NextTraceID	= id;
			m_bNextForceAttack = bForceAttack;

			m_TraceIDBuffer = id;

			m_bRepeatAction = FALSE;

			m_fTraceBuffer = FLAG_TRACE_NULL;

			return true;
		}
	}

	return true;
}


//----------------------------------------------------------------------
// Trace Creature To SpecialAction
//----------------------------------------------------------------------
// Trace and attack one Creature with the special attack (R-Click).
// If not standing still.. set it in Next.
//----------------------------------------------------------------------
// [return value]
//
//  Returns false only when it cannot be used on another Creature.
//
//----------------------------------------------------------------------
bool	
MPlayer::TraceCreatureToSpecialAction(TYPE_OBJECTID id, bool bForceAttack)
{
	SetActionDelay( 0 );

	if((HasEffectStatus(EFFECTSTATUS_SLEEP) || HasEffectStatus( EFFECTSTATUS_BLOCK_HEAD ) )
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		)
		return false;

//	if(bForceAttack && !g_pSDLInput->KeyDown(DIK_LSHIFT))
//	{
//		bForceAttack = false;
//	}
		
	//-----------------------------------------------------
	// Must not have a delay.
	//-----------------------------------------------------
	if (!IsNotDelay() 
		|| HasEffectStatus( EFFECTSTATUS_ETERNITY_PAUSE )
		|| IsInCasket()	// [new skill 3]
		|| (IsInDarkness() && (!HasEffectStatus( EFFECTSTATUS_LIGHTNESS ) || g_pZone->GetID() == 3001)
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		))
	{
		return false;
	}

	if (m_nSpecialActionInfo == ACTIONINFO_NULL
		// Special attacks are not allowed while riding a motorcycle.
		|| m_MoveDevice == MOVE_DEVICE_RIDE
		// Cannot be used if some skill use is awaiting verification.
		//|| m_WaitVerify != WAIT_VERIFY_NULL)	// 2001.8.20 commented out
		|| IsInSafeSector() == 2
		// Skills cannot be used in a complete safe zone.
		|| CanActionByZoneInfo()
		|| HasEffectStatusSummonSylph( dynamic_cast<MCreature*>(this) )				// cannot do it while running around~
		)
	{
		m_fNextTrace = FLAG_TRACE_NULL;

		UnSetRepeatAction();

		return false;
	}

		
	// When using another skill
	if (m_nUsedActionInfo!=ACTIONINFO_NULL)
		return false;

	//-------------------------------------------------------
	// Use only those that can be used on others.
	//-------------------------------------------------------
	
	if ((*g_pActionInfoTable)[m_nSpecialActionInfo].IsTargetOther())
	{
		int useSkill = m_nSpecialActionInfo;
		int originalSkill = m_nSpecialActionInfo;

//		// 2004, 6, 9 sobeit add start - test, make it work after the successful bloodsucking message arrives..
//		if(m_nSpecialActionInfo == SKILL_BLOOD_DRAIN)
//		{
//			m_fNextTrace = FLAG_TRACE_NULL;
//			//UnSetRepeatAction();
//			if(g_pSkillAvailable->IsEnableSkill( (ACTIONINFO)m_nSpecialActionInfo ) && m_TraceID != -1)
//			{
//				CGBloodDrain _CGBloodDrain;
//				_CGBloodDrain.setObjectID( m_TraceID );
//				g_pSocket->sendPacket( &_CGBloodDrain );
//				DEBUG_ADD("Sent bloodsucking message");
//			}
//			return false;
//		}
//		// 2004, 6, 9 sobeit add end - test, make it work after the successful bloodsucking message arrives..
	
		if( m_nSpecialActionInfo == SKILL_BLITZ_SLIDING_ATTACK ||
			m_nSpecialActionInfo == SKILL_BLAZE_WALK_ATTACK 
			)
		{
			m_fNextTrace = FLAG_TRACE_NULL;
			UnSetRepeatAction();

			return false;
		}

		int grade = 0;
		
		if( IsOusters() )
		{
			grade = (*g_pSkillInfoTable)[originalSkill].GetExpLevel() / 15;
			DEBUG_ADD_FORMAT("OUSTERS LEVEL : %d %d",originalSkill,(*g_pSkillInfoTable)[originalSkill].GetExpLevel());
		}

		if( grade>0 && (*g_pActionInfoTable)[originalSkill].IsUseActionStep() )
			useSkill = (*g_pActionInfoTable)[originalSkill].GetActionStep( grade - 1 );
		// 2004, 9, 20, sobeit add start
		if((*g_pActionInfoTable)[useSkill].IsMasterySkillStep())
		{
			int TempSkill = (*g_pActionInfoTable)[useSkill].GetActionStep( (*g_pActionInfoTable)[useSkill].GetMasterySkillStep() );
			if(g_pSkillAvailable->IsEnableSkill( (ACTIONINFO)TempSkill ))
				useSkill = TempSkill;
		}
		// 2004, 9, 20, sobeit add end
		if( (*g_pActionInfoTable)[useSkill].GetParentActionInfo() != ACTIONINFO_NULL )
			originalSkill = (*g_pActionInfoTable)[useSkill].GetParentActionInfo();

		DEBUG_ADD_FORMAT("SKILL : %d %d", originalSkill, useSkill );

		//-------------------------------------------------------
		// [SKILLREPEAT DIAG] kept as a permanent regression signal (the
		// 2026-08-31 stutter was root-caused with it: a learn-time server
		// seed reaching the client as a 2 s cooldown - see
		// docs/reports/skill-repeat-diagnostics-2026-08-31.md). INFO level, so
		// Release builds filter it in log_write. Records the state every
		// re-issue of a held-button skill sees, so the abort reason is
		// unambiguous.
		//-------------------------------------------------------
		DEBUG_ADD_FORMAT("[SKILLREPEAT] enter ai=%d rep=%d stop=%d act=%d cnt=%d/%d mv=%d/%d mvAct=%d used=%d trace=%02x buf=%02x",
			originalSkill, (int)m_bRepeatAction, (int)IsStop(),
			(int)m_Action, (int)m_ActionCount, (int)m_ActionCountMax,
			(int)m_MoveCount, (int)m_MoveCountMax, (int)m_MoveAction,
			(int)m_nUsedActionInfo, (int)m_fTrace, (int)m_fTraceBuffer);

		//-------------------------------------------------------
		// Check whether the skill is currently usable..
		// Disallow it if it is a Passive skill..
		//-------------------------------------------------------
			if (!g_pSkillAvailable->IsEnableSkill( (ACTIONINFO)originalSkill )
				|| !(*g_pSkillInfoTable)[originalSkill].IsEnable()
				|| !(*g_pSkillInfoTable)[originalSkill].IsAvailableTime()
				|| (*g_pSkillInfoTable)[originalSkill].IsPassive())
			{
				// [SKILLREPEAT DIAG] kept
				DEBUG_ADD_FORMAT("[SKILLREPEAT] abort=SKILLSET avail=%d enable=%d time=%d passive=%d left=%u",
					(int)g_pSkillAvailable->IsEnableSkill( (ACTIONINFO)originalSkill ),
					(int)(*g_pSkillInfoTable)[originalSkill].IsEnable(),
					(int)(*g_pSkillInfoTable)[originalSkill].IsAvailableTime(),
					(int)(*g_pSkillInfoTable)[originalSkill].IsPassive(),
					(unsigned)(*g_pSkillInfoTable)[originalSkill].GetAvailableTimeLeft());

				m_fNextTrace = FLAG_TRACE_NULL;

				UnSetRepeatAction();

				return false;
			}

		if (IsStop()) // || m_Action!=ACTION_ATTACK)
		{
			// Check whether the Creature exists in the Zone.
			MCreature*	pCreature = m_pZone->GetCreature(id);

			//GET_DYING_CREATURE( pCreature, id );
			
			// When there is no creature
			// As a Slayer I cannot chase a character that is inside Darkness.
			if (pCreature==NULL 
				// Need to add info on skills usable on corpses.
				|| (pCreature->IsDead() && originalSkill!=MAGIC_RESURRECT && originalSkill!=SKILL_TRANSFUSION  && originalSkill!=SKILL_SOUL_REBIRTH) 
				|| (pCreature->IsInGroundElemental() && pCreature->IsOusters() && !g_pPlayer->IsOusters())
				// 2004, 10, 14, sobiet modify start - fixed because reportedly a target inside Darkness could not be attacked while under Lightness..
//				|| pCreature->IsInDarkness() && !pCreature->IsNPC() && 
//				!(IsVampire() && !HasEffectStatus( EFFECTSTATUS_LIGHTNESS )  ||
//				g_pZone->GetID() != 3001 && IsVampire() )
//				&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST ) 
				|| (pCreature->IsInDarkness() && !pCreature->IsNPC() && !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST ) &&
				((!IsVampire() && !HasEffectStatus( EFFECTSTATUS_LIGHTNESS ))  ||	(g_pZone->GetID() == 3001 && IsVampire()) )
				// 2004, 10, 14, sobiet modify end- fixed because reportedly a target inside Darkness could not be attacked while under Lightness..
#ifdef __METROTECH_TEST__
				&& !g_bLight
#endif
				)
				|| ((*g_pActionInfoTable)[originalSkill].GetUser() == FLAG_ACTIONINFO_USER_SLAYER && (!pCreature->IsSlayer() || !pCreature->IsPlayerOnly()))
				|| ((*g_pActionInfoTable)[originalSkill].GetUser() == FLAG_ACTIONINFO_USER_VAMPIRE && (!pCreature->IsVampire() || !pCreature->IsPlayerOnly()))
				|| ((*g_pActionInfoTable)[originalSkill].GetUser() == FLAG_ACTIONINFO_USER_OUSTERS && (!pCreature->IsOusters() || !pCreature->IsPlayerOnly()))
				|| ((*g_pActionInfoTable)[originalSkill].GetUser() == FLAG_ACTIONINFO_USER_MONSTER && (!pCreature->IsVampire() || pCreature->IsPlayerOnly()))

				|| (IsSlayer() && !(*g_pActionInfoTable)[originalSkill].IsUserSlayer() && pCreature->IsSlayer() && pCreature->IsPlayerOnly() && !bForceAttack)
				|| (IsVampire() && !(*g_pActionInfoTable)[originalSkill].IsUserVampire() && pCreature->IsVampire() && pCreature->IsPlayerOnly() && !bForceAttack)				
				|| (IsOusters() && !(*g_pActionInfoTable)[originalSkill].IsUserOusters() && pCreature->IsOusters() && pCreature->IsPlayerOnly() && !bForceAttack)				
				) 
			{
				// [SKILLREPEAT DIAG] kept
				DEBUG_ADD_FORMAT("[SKILLREPEAT] abort=TARGET null=%d dead=%d dark=%d",
					(int)(pCreature==NULL),
					(int)(pCreature!=NULL && pCreature->IsDead()),
					(int)(pCreature!=NULL && pCreature->IsInDarkness()));

				// Reaching here means ACTIONINFO_NULL..
				// so that UnSetRepeatAction() handles it properly.. (sigh)
				// Anyway it's a mess.. (sigh)
				if (IsRepeatAction())
				{
					m_nUsedActionInfo = originalSkill;

					UnSetRepeatAction();

					m_nUsedActionInfo = ACTIONINFO_NULL;
				}
				else
				{
					UnSetRepeatAction();
				}

				m_fNextTrace = FLAG_TRACE_NULL; 
				m_fTraceBuffer	= FLAG_TRACE_NULL;	
				return false;
			}

			if( pCreature != NULL )
			{
				if( pCreature->GetObjectType() == MObject::TYPE_CREATURE )
				{
					if( (IsSlayer() && g_pZone->GetSector( GetX(), GetY() ).IsSafeSlayer() && 
						pCreature->IsSlayer() && !g_pZone->GetSector( pCreature->GetX(), pCreature->GetY() ).IsSafeSlayer() && pCreature->IsPlayerOnly()) ||
						(IsVampire() && g_pZone->GetSector( GetX(), GetY() ).IsSafeVampire() && 
						pCreature->IsVampire() && !g_pZone->GetSector( pCreature->GetX(), pCreature->GetY() ).IsSafeVampire() && pCreature->IsPlayerOnly()) ||
						(IsOusters() && g_pZone->GetSector( GetX(), GetY() ).IsSafeOusters() && 
						pCreature->IsOusters() && !g_pZone->GetSector( pCreature->GetX(), pCreature->GetY() ).IsSafeOusters() && pCreature->IsPlayerOnly()))
					{
						// [SKILLREPEAT DIAG] kept
						DEBUG_ADD_FORMAT("[SKILLREPEAT] abort=SAFESECTOR");
//						UnSetLockMode();
						UnSetRepeatAction();
//						g_pTopView->SetSelectedNULL();
						return false;
					}
				}
			}

			bool bTraceCreatureToForceAttack = true;
			bool bKeepTraceCreature			= false;			

			//-------------------------------------------------------
			// For an NPC.. just let it trace.
			//-------------------------------------------------------
			if (pCreature->IsNPC())
			{
				// [SKILLREPEAT DIAG] kept
				DEBUG_ADD_FORMAT("[SKILLREPEAT] abort=NPC");

				TraceCreatureToBasicAction(id, false);

				return false;
			}
			else
				if (bForceAttack)
				{
					bTraceCreatureToForceAttack = true;
					bKeepTraceCreature			= false;			
					
					UnSetRequestMode();
				}


			//-------------------------------------------------------
			// Checks for specific skills - should be split out, as always.. (sigh)
			//-------------------------------------------------------
			switch (originalSkill)
			{
				//-------------------------------------------------------
				// SKILL_TRANSFUSION
				//-------------------------------------------------------
				case SKILL_TRANSFUSION :

					//--------------------------------------------------
					// When it cannot be used
					//--------------------------------------------------
					// My HP is less than 30
					// Different guild
					// Opponent's HP is full
					if (GetHP() < 30
						|| pCreature->IsSlayer()
							//|| GetGuildNumber()!=pCreature->GetGuildNumber()	
							// Default guild of vampire
							//&& GetGuildNumber()!=0
						//	&& pCreature->GetGuildNumber()!=0
						|| pCreature->GetHP()==pCreature->GetMAX_HP()
						|| pCreature->IsUndergroundCreature()
						|| pCreature->IsFlyingCreature() )
					{
						m_fNextTrace = FLAG_TRACE_NULL;

						UnSetRepeatAction();

						return false;
					}
				break;

				//-------------------------------------------------------
				// SKILL_THROW_BOMB
				//-------------------------------------------------------
				case SKILL_THROW_BOMB :
				{
					MItem* pItem = g_pInventory->FindItem( ITEM_CLASS_BOMB );
								
					if (pItem==NULL)
					{
						m_fNextTrace = FLAG_TRACE_NULL;
						UnSetRepeatAction();
						return false;
					}

					useSkill = pItem->GetUseActionInfo();
				}
				break;
			}

			//-------------------------------------------------------
			// Check for specific situations
			//-------------------------------------------------------
			if ((*g_pActionInfoTable)[originalSkill].HasOption())
			{
				//-------------------------------------------------------
				// Skill that requires bless to be active
				//-------------------------------------------------------
				if ((*g_pActionInfoTable)[originalSkill].IsOptionUseWithBless())
				{
					if (!m_bEffectStatus[EFFECTSTATUS_BLESS])
					{
							m_fNextTrace = FLAG_TRACE_NULL;

							UnSetRepeatAction();

							return false;
					}
				}
			}

			//-------------------------------------------------------
			// If the skill uses an item, the item must be checked.
			//-------------------------------------------------------
			WORD fWeaponType = (*g_pActionInfoTable)[originalSkill].GetWeaponType();
			if (fWeaponType	& FLAG_ACTIONINFO_WEAPON_HOLY_WATER)
			{
				MItem* pItem = g_pInventory->FindItem( ITEM_CLASS_HOLYWATER );

				if (pItem==NULL)
				{
					m_fNextTrace = FLAG_TRACE_NULL;

					UnSetRepeatAction();

					return false;
				}

				// If there is holy water..
				// Fix the item... so nothing else can be done..
				// Must be released after the skill ends.
				//SetItemCheckBuffer( pItem, ITEM_CHECK_BUFFER_SKILL_FROM_ITEM );
			}

			//-------------------------------------------------------
			// If the skill uses an item, the item must be checked.
			//-------------------------------------------------------
			if (fWeaponType & FLAG_ACTIONINFO_WEAPON_BOMB)
			{
				MItem* pItem = g_pInventory->FindItem( ITEM_CLASS_BOMB );

				if (pItem==NULL)
				{
					m_fNextTrace = FLAG_TRACE_NULL;

					UnSetRepeatAction();

					return false;
				}

				// If there is holy water..
				// Fix the item... so nothing else can be done..
				// Must be released after the skill ends.
				//SetItemCheckBuffer( pItem, ITEM_CHECK_BUFFER_SKILL_FROM_ITEM );
			}

			//-------------------------------------------------------
			// If the skill uses a gun,
			// bullets must be checked.
			//-------------------------------------------------------
			if ((*g_pActionInfoTable)[originalSkill].IsWeaponTypeGunAny())
			{
					if (g_pCurrentMagazine==NULL)
					{
						// When using a gun without a magazine
						WorldEvents::PlaySound( SOUND_ITEM_NO_MAGAZINE );

						// the delay for an empty magazine
							m_DelayTime	= g_FrameNow + MonotonicClock::Duration(GetActionInfoDelay(m_nBasicActionInfo));

						m_fNextTrace	= FLAG_TRACE_NULL;
						return false;					
					}
					else
					{
						if (g_pCurrentMagazine->GetNumber()==0)
						{
							// When using a gun without a magazine
							WorldEvents::PlaySound( SOUND_ITEM_NO_MAGAZINE );

							// the delay for an empty magazine
							m_DelayTime	= g_FrameNow + MonotonicClock::Duration(GetActionInfoDelay(m_nBasicActionInfo));

							if (IsRepeatAction())
							{
								m_nUsedActionInfo = originalSkill;

								UnSetRepeatAction();

								m_nUsedActionInfo = ACTIONINFO_NULL;
							}
							else
							{
								UnSetRepeatAction();
							}

							m_fNextTrace	= FLAG_TRACE_NULL;

							// [Help] When bullets run out
//							__BEGIN_HELP_EVENT
//								GameInfoPresentation::ExecuteHelpEvent( HE_ITEM_EMPTY_BULLET );
//							__END_HELP_EVENT
							

							return false;
						}
					}
			}

			// When the creature is already dead
			//if (pCreature->IsDead()) return;

			// Set as the skill used
			m_nNextUsedActionInfo	= useSkill;

			// Set trace info
			SetTraceID( id );
			m_fTrace	= FLAG_TRACE_CREATURE_SPECIAL;		
			m_TraceX	= pCreature->GetX();
			m_TraceY	= pCreature->GetY();
			m_TraceZ	= pCreature->GetZ();

			// Race of the character being traced
			//m_TraceCreatureTribe = pCreature->GetCreatureTribe();

			// Remember the target currently being traced.
			m_fTraceBuffer	= m_fTrace;	
			m_TraceIDBuffer = id;

			//------------------------------------------------------------
			// Depending on what the SpecialAction is..
			// TraceDistance will vary with the current Player's abilities.
			//------------------------------------------------------------
			//------------------------------------------------------------
			// Is it a forced attack?
			// Keep tracing?
			//------------------------------------------------------------
			m_bTraceCreatureToForceAttack	= bTraceCreatureToForceAttack;
			m_bKeepTraceCreature			= bKeepTraceCreature;
			
			m_TraceDistance		= GetActionInfoRange(m_nNextUsedActionInfo);
			
			//------------------------------------------------------------

			// Set the tracing path
			SetAction( m_MoveAction );
			SetNextDestination(m_TraceX, m_TraceY);

			// Remove buffering.
			m_fNextTrace			= FLAG_TRACE_NULL;

			// Stop continuing the chase
			m_bKeepTraceCreature	= false;
		
			//------------------------------------------------------------
			// Print message
			//------------------------------------------------------------
			DEBUG_ADD_FORMAT("Trace Creature To SpecialAction: ID=%d, (%d, %d)", id, m_TraceX, m_TraceY);					

			// Clear the selected sector
			WorldEvents::SetSelectedSectorNULL();
		}
		else
		{
			// [SKILLREPEAT DIAG] kept
			DEBUG_ADD_FORMAT("[SKILLREPEAT] abort=NOTSTOP rep=%d act=%d cnt=%d/%d mv=%d/%d",
				(int)m_bRepeatAction, (int)m_Action,
				(int)m_ActionCount, (int)m_ActionCountMax,
				(int)m_MoveCount, (int)m_MoveCountMax);

			//------------------------------------------------------------
			// Buffer the next action when not in a repeat action
			//------------------------------------------------------------
			if (m_bRepeatAction)
			{
				m_fNextTrace	= FLAG_TRACE_NULL;

				return false;
			}
			else
			{
				m_fNextTrace	= FLAG_TRACE_CREATURE_SPECIAL;
				m_bNextForceAttack = bForceAttack;
				m_NextTraceID	= id;

				m_TraceIDBuffer = id;

				m_bRepeatAction = FALSE;

				m_fTraceBuffer = FLAG_TRACE_NULL;

				return true;//false;
			}
		}

		return true;
	}
	
	// Means the target is not a creature.
	return false;
}


//----------------------------------------------------------------------
// Trace Sector To BasicAction
//----------------------------------------------------------------------
// Attack Sector(sX,sY) with the basic attack (L-Click).
// If not standing still.. set it in Next.
//----------------------------------------------------------------------
bool	
MPlayer::TraceSectorToBasicAction(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	//-----------------------------------------------------
	// Must not have a delay.
	//-----------------------------------------------------
	if (!IsNotDelay() 
		|| HasEffectStatus ( EFFECTSTATUS_ETERNITY_PAUSE )
		|| IsInCasket()	// [new skill 3]
		|| g_bZoneSafe)
	{
		return false;
	}

	// When the coordinates were entered wrongly
	if (sX==SECTORPOSITION_NULL || sY==SECTORPOSITION_NULL)
		return false;
	
	if (m_nBasicActionInfo == ACTIONINFO_NULL
		// No attacking while riding a motorcycle
		|| m_MoveDevice == MOVE_DEVICE_RIDE
		|| CanActionByZoneInfo())
	{
		return false;
	}
		
	if (IsStop()) // || m_Action!=ACTION_ATTACK)
	{		
		// Set as the skill used
		m_nNextUsedActionInfo	= GetBasicActionInfo(); //m_nBasicActionInfo;
		
		// Set trace info
		m_fTrace	= FLAG_TRACE_SECTOR_BASIC;
		m_TraceX	= sX;
		m_TraceY	= sY;
		m_TraceZ	= 0;
		
		// Remember the target currently being traced.
		m_fTraceBuffer	= m_fTrace;	

		//------------------------------------------------------------
		// Depending on what the BasicAction is..
		// TraceDistance will vary with the current Player's abilities.
		//------------------------------------------------------------
		
		m_TraceDistance		= GetActionInfoRange(m_nNextUsedActionInfo);
		
		//------------------------------------------------------------		

		// Set the tracing path
		SetAction( m_MoveAction );
		SetNextDestination(m_TraceX, m_TraceY);

		// Remove buffering.
		m_fNextTrace			= FLAG_TRACE_NULL;
		
		// Stop continuing the chase
		m_bKeepTraceCreature	= false;

		//------------------------------------------------------------
		// Print message
		//------------------------------------------------------------
		#ifdef	OUTPUT_DEBUG
			DEBUG_ADD_FORMAT("Trace Sector To BasicAction: (%d, %d)", sX, sY);				
		#endif
	}
	else
	{
		//------------------------------------------------------------
		// Buffer the next action when not in a repeat action
		//------------------------------------------------------------
		if (m_bRepeatAction)
		{
			m_fNextTrace	= FLAG_TRACE_NULL;

			return false;
		}
		else
		{
			m_fNextTrace	= FLAG_TRACE_SECTOR_BASIC;
			m_NextTraceX	= sX;
			m_NextTraceY	= sY;

			m_bRepeatAction = FALSE;

			return false;
		}
	}

	return true;
}

//----------------------------------------------------------------------
// Trace Sector To SpecialAction
//----------------------------------------------------------------------
// Attack Sector(sX,sY) with the special attack (R-Click).
// If not standing still.. set it in Next.
//----------------------------------------------------------------------
// [return value]
//
//  Returns false only when it cannot be used on the Zone.
//
//----------------------------------------------------------------------
bool	
MPlayer::TraceSectorToSpecialAction(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	if((HasEffectStatus(EFFECTSTATUS_SLEEP) || HasEffectStatus( EFFECTSTATUS_BLOCK_HEAD ) )
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		)
		return false;

	//-----------------------------------------------------
	// Must not have a delay.
	//-----------------------------------------------------
	if (!IsNotDelay() 
		|| HasEffectStatus( EFFECTSTATUS_ETERNITY_PAUSE )
		|| IsInCasket()	// [new skill 3]
		|| g_bZoneSafe
		|| CanActionByZoneInfo()
		)
	{
		return false;
	}

	// When the coordinates were entered wrongly
	if (sX==SECTORPOSITION_NULL || sY==SECTORPOSITION_NULL)
		return false;
	
	if (m_nSpecialActionInfo == ACTIONINFO_NULL		
		// Skills cannot be used while riding a motorcycle.
		|| m_MoveDevice == MOVE_DEVICE_RIDE
		// Cannot be used if some skill use is awaiting verification.
		//|| m_WaitVerify != WAIT_VERIFY_NULL)	// 2001.8.20 commented out
		|| IsInSafeSector() == 2
		// Skills cannot be used in a complete safe zone.
		)
	{
		m_fNextTrace = FLAG_TRACE_NULL;

		UnSetRepeatAction();

		return false;
	}
	// 2004, 9, 18, sobeit add start - turret fire is ignored when out of range (sob)
	if (m_nSpecialActionInfo==SKILL_TURRET_FIRE)
	{
		m_TraceDistance		= GetActionInfoRange(m_nSpecialActionInfo);
		if (max(abs(m_X-sX), abs(m_Y-sY)) > m_TraceDistance || !g_pSkillAvailable->IsEnableSkill( SKILL_TURRET_FIRE ))
		{
			m_fNextTrace = FLAG_TRACE_NULL;
			UnSetRepeatAction();
			InstallTurretStopAttack();
			return false;
		}
	}
	// 2004, 9, 18, sobeit add end

	// When using another skill
	if (m_nUsedActionInfo!=ACTIONINFO_NULL)
		return false;

	// 2004, 9, 21, sobeit add start - allow target-creature skills to be used as target-zone skills
	bool bAbleTargetSector = false;
//	if(m_nSpecialActionInfo == SKILL_DESTRUCTION_SPEAR || m_nSpecialActionInfo == SKILL_DESTRUCTION_SPEAR_MASTERY)
//	{
//		if(g_pSkillAvailable->IsEnableSkill( SKILL_DESTRUCTION_SPEAR_MASTERY ))
//			bAbleTargetSector = true;
//	}
//	else 
	if(m_nSpecialActionInfo == SKILL_ICE_LANCE || m_nSpecialActionInfo == SKILL_ICE_LANCE_MASTERY)
	{
		if(g_pSkillAvailable->IsEnableSkill( SKILL_ICE_LANCE_MASTERY ))
			bAbleTargetSector = true;
	}
	// 2004, 9, 21, sobeit add end
	// Only use the ones that can be used on a Zone.
	if ((*g_pActionInfoTable)[m_nSpecialActionInfo].IsTargetZone() || bAbleTargetSector)
	{
		int grade = 0;
		int		useSkill = m_nSpecialActionInfo;

		if( IsOusters() )
		{
			grade = (*g_pSkillInfoTable)[useSkill].GetExpLevel() / 15;
		}
		
		if( (*g_pActionInfoTable)[useSkill].IsUseActionStep() && grade > 0 )
			useSkill = (*g_pActionInfoTable)[useSkill].GetActionStep( grade - 1 );			
			
		// 2004, 9, 20, sobeit add start
		if((*g_pActionInfoTable)[useSkill].IsMasterySkillStep())
		{
			int TempSkill = (*g_pActionInfoTable)[useSkill].GetActionStep( (*g_pActionInfoTable)[useSkill].GetMasterySkillStep() );
			if(g_pSkillAvailable->IsEnableSkill( (ACTIONINFO)TempSkill ))
				useSkill = TempSkill;
		}
		// 2004, 9, 20, sobeit add end
		
//		if (useSkill==SKILL_TURRET_FIRE)
//		{
//			if (max(abs(m_X-sX), abs(m_Y-sY)) 
//				> m_TraceDistance)
//			{
//				m_fNextTrace = FLAG_TRACE_NULL;
//				UnSetRepeatAction();
//				return false;
//			}
//		}
		//-------------------------------------------------------
		// Pick a bomb and throw it
		//-------------------------------------------------------
		if (useSkill==SKILL_THROW_BOMB)
		{
			//-------------------------------------------------------
			// Check whether the skill is currently usable..
			// Disallow it if it is a Passive skill..
			//-------------------------------------------------------
			if (IsSlayer()
				&& !IsWaitVerify()
				&& IsItemCheckBufferNULL())
			{											
				ITEM_CLASS	itemClass = ITEM_CLASS_BOMB;

				MItemClassFinder itemFinder(itemClass);
				MItem* pItem = ((MItemManager*)g_pInventory)->FindItem( itemFinder );
							
				if (pItem==NULL)
				{
					m_fNextTrace = FLAG_TRACE_NULL;
					UnSetRepeatAction();
					return false;
				}

				useSkill = pItem->GetUseActionInfo();				
			}
		}

		
		//-------------------------------------------------------
		// Check whether the skill is currently usable..
		// Disallow it if it is a Passive skill..
		//-------------------------------------------------------
		if (
			(!g_pSkillAvailable->IsEnableSkill( (ACTIONINFO)m_nSpecialActionInfo ) && m_nSpecialActionInfo != SKILL_TURRET_FIRE)
			|| !(*g_pSkillInfoTable)[m_nSpecialActionInfo].IsEnable()
			|| !(*g_pSkillInfoTable)[m_nSpecialActionInfo].IsAvailableTime()
			|| (*g_pSkillInfoTable)[m_nSpecialActionInfo].IsPassive())
		{
			m_fNextTrace = FLAG_TRACE_NULL;

			UnSetRepeatAction();

			return false;
		}

		if (IsStop()) // || m_Action!=ACTION_ATTACK)
		{	
#ifdef __METROTECH_TEST__
			if (IsOusters())
			{
				bool bAbsorbSoul = true;
				
				MItem* pItem = g_pZone->GetItem( g_pZone->GetItemID( sX, sY ) );
				
				if (pItem==NULL 
					|| pItem->GetItemClass()!=ITEM_CLASS_CORPSE)
				{
					bAbsorbSoul = false;
					
					if(pItem == NULL)
					{
						MCreature *pCreature = g_pZone->GetCreature(g_pZone->GetCreatureID(sX, sY));
						
						if(pCreature != NULL && pCreature->HasEffectStatus(EFFECTSTATUS_COMA))
						{
							bAbsorbSoul = true;
						}
					}
				}
				
				if(pItem != NULL)
				{
					// Do not eat relics
					MCreature* pCreature = ((MCorpse*)pItem)->GetCreature();
					if(pCreature != NULL)
					{
						if(pCreature->GetCreatureType() >= 371 && pCreature->GetCreatureType() <= 376 || pCreature->GetCreatureType() >= 560 && pCreature->GetCreatureType() <= 563 || pCreature->GetCreatureType() >= 526 && pCreature->GetCreatureType() <= 549 || pCreature->GetCreatureType() == 670 || pCreature->GetCreatureType() == 672 || pCreature->GetCreatureType() == 673)
							bAbsorbSoul = false;
					}
				}
				if(bAbsorbSoul)
					useSkill = SKILL_ABSORB_SOUL;
			}
#endif
			
			int targetX	= sX;
			int targetY = sY;

			//-------------------------------------------------------
			// Check for specific skills
			//-------------------------------------------------------
			switch (useSkill)
			{
				// Transfusion is flagged as a tile skill but cannot actually be used
				case SKILL_TRANSFUSION:
					return false;
					break;
					
				case SKILL_WILD_TYPHOON :
					return false;
					break;

				//-------------------------------------------------------
				// Soul drain
				//-------------------------------------------------------
				// Do not use the skill if there is no corpse on the ground.
				//-------------------------------------------------------
				case SKILL_ABSORB_SOUL :
					if (IsOusters())
					{
						MItem* pItem = g_pZone->GetItem( g_pZone->GetItemID( sX, sY ) );
						
						if (pItem==NULL 
							|| pItem->GetItemClass()!=ITEM_CLASS_CORPSE)
						{
							if(pItem == NULL)
							{
								MCreature *pCreature = g_pZone->GetCreature(g_pZone->GetCreatureID(sX, sY));

								if(pCreature != NULL && pCreature->HasEffectStatus(EFFECTSTATUS_COMA))
									break;
							}

							m_fNextTrace = FLAG_TRACE_NULL;
							
							UnSetRepeatAction();
							
							return false;
						}
						
						// Do not eat relics
						MCreature* pCreature = ((MCorpse*)pItem)->GetCreature();
						if(pCreature != NULL)
						{
							if((pCreature->GetCreatureType() >= 371 && pCreature->GetCreatureType() <= 376) || (pCreature->GetCreatureType() >= 560 && pCreature->GetCreatureType() <= 563) || (pCreature->GetCreatureType() >= 526 && pCreature->GetCreatureType() <= 549) || pCreature->GetCreatureType() == 670 || pCreature->GetCreatureType() == 672 || pCreature->GetCreatureType() == 673)
								return false;
						}
						
					}
					else
					{
						m_fNextTrace = FLAG_TRACE_NULL;
						
						UnSetRepeatAction();
						
						return false;
					}
					break;

				//-------------------------------------------------------
				// Eat the corpse if a wolf
				//-------------------------------------------------------
				// Do not use the skill if there is no corpse on the ground.
				//-------------------------------------------------------
				case SKILL_BITE_OF_DEATH :
					if (m_CreatureType != CREATURETYPE_WER_WOLF )
					{
						return false;
					}
					break;
				case MAGIC_EAT_CORPSE :
					if (m_CreatureType==CREATURETYPE_WOLF)
					{
						MItem* pItem = g_pZone->GetItem( g_pZone->GetItemID( sX, sY ) );

						if (pItem==NULL 
							|| pItem->GetItemClass()!=ITEM_CLASS_CORPSE)
						{
							m_fNextTrace = FLAG_TRACE_NULL;

							UnSetRepeatAction();

							return false;
						}

						// Do not eat relics
						MCreature* pCreature = ((MCorpse*)pItem)->GetCreature();
						if(pCreature != NULL)
						{
							if((pCreature->GetCreatureType() >= 371 && pCreature->GetCreatureType() <= 376) || (pCreature->GetCreatureType() >= 560 && pCreature->GetCreatureType() <= 563) || (pCreature->GetCreatureType() >= 526 && pCreature->GetCreatureType() <= 549) || pCreature->GetCreatureType() == 670 || pCreature->GetCreatureType() == 672 || pCreature->GetCreatureType() == 673)
								return false;
						}
					
					}
					else
					{
						m_fNextTrace = FLAG_TRACE_NULL;

						UnSetRepeatAction();

						return false;
					}
				break;
//				// 2004, 8, 30, sobeit add start - sweep vice test
//				case SKILL_SWEEP_VICE_1:
//					if(!GetSweepViewValue()) //when Sweep Vice is not yet in use
//					{
//						SetSweepViewValue(1);
//						g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);
//						m_TraceX	= 	g_SelectSector.x;
//						m_TraceY	=	g_SelectSector.y;
//						ExecuteActionInfoFromMainNode(SKILL_CLIENT_SWEEP_VICE,g_SelectSector.x, g_SelectSector.y, 0,g_pPlayer->GetDirection(),0	,	
//							g_SelectSector.x, g_SelectSector.y, 0, 0xffff, NULL, false);
//						UnSetRepeatAction();
//					//	return false ;
//					}
//					else
//					{
//						if(!g_bRButtonDown)
//						{
//							if(2 == GetSweepViewValue())
//								useSkill = SKILL_SWEEP_VICE_3;
//							else if(3 == GetSweepViewValue())
//								useSkill = SKILL_SWEEP_VICE_5;
//						}
//						else 
//							return false;
//					}
//					break;
				case SKILL_TURRET_FIRE :
					if (!HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
						return false;
					break;
			}

	

// 2004, 8, 30, sob
	

			WORD fWeaponType = (*g_pActionInfoTable)[useSkill].GetWeaponType();
			
			//------------------------------------------------
			// For bombs: offset the coordinates depending on level
			//------------------------------------------------
			if (fWeaponType	& FLAG_ACTIONINFO_WEAPON_BOMB)
			{
				int skillLevel = (*g_pSkillInfoTable)[SKILL_THROW_BOMB].GetExpLevel();

				int hitRoll = 50 + skillLevel/2;

				if (rand()%100 < hitRoll)
				{
					// Direct hit
				}
				else
				{
					if (rand()%100 < 25
						&& skillLevel < 30)		// within 2 tiles
					{
						// It has to be a near miss.
						targetX += rand()%5 - 2;
						targetY += rand()%5 - 2;
					}
					else						// within 1 tile
					{
						// It has to be a near miss.
						targetX += rand()%3 - 1;
						targetY += rand()%3 - 1;
					}
				}
			}

			//------------------------------------------------
			// Check ammo
			//------------------------------------------------
			if ((*g_pActionInfoTable)[useSkill].IsWeaponTypeGunAny())
			{
				if (!IsRequestMode() && g_pCurrentMagazine!=NULL)		// no ammo needed
				{
					if (g_pCurrentMagazine->GetNumber()==0)	// out of ammo
					{
						// When using a gun without a magazine
						WorldEvents::PlaySound( SOUND_ITEM_NO_MAGAZINE );

						m_fNextTrace	= FLAG_TRACE_NULL;

						

						// [Help] when ammo runs out
//						__BEGIN_HELP_EVENT
//							GameInfoPresentation::ExecuteHelpEvent( HE_ITEM_EMPTY_BULLET );
//						__END_HELP_EVENT

						m_fNextTrace = FLAG_TRACE_NULL;

						UnSetRepeatAction();
						// 2004, 9, 23, sobeit add start - turret attack end motion
						if(HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
						{
							InstallTurretStopAttack();
						}
						else
							// the delay for an empty magazine
						m_DelayTime	= g_FrameNow + MonotonicClock::Duration(GetActionInfoDelay(m_nBasicActionInfo));
						// 2004, 9, 23, sobeit add end
						return false;
					}
				}
			}

			// Set as the skill used
			m_nNextUsedActionInfo	= useSkill;

			// Set trace info
			m_fTrace	= FLAG_TRACE_SECTOR_SPECIAL;
			m_TraceX	= targetX;
			m_TraceY	= targetY;
			m_TraceZ	= 0;

			// Remember the target currently being traced.
			m_fTraceBuffer	= m_fTrace;	
			
			//------------------------------------------------------------
			// Depending on what the BasicAction is..
			// TraceDistance will vary with the current Player's abilities.
			//------------------------------------------------------------
			
			m_TraceDistance		= GetActionInfoRange(m_nNextUsedActionInfo);
			
			//------------------------------------------------------------		

			// Set the tracing path
			SetAction( m_MoveAction );
			SetNextDestination(m_TraceX, m_TraceY);

			// Remove buffering.
			m_fNextTrace			= FLAG_TRACE_NULL;

			// Stop continuing the chase
			m_bKeepTraceCreature	= false;
			
			//------------------------------------------------------------
			// Print message
			//------------------------------------------------------------
			DEBUG_ADD_FORMAT("Trace Sector To SpecialAction: (%d, %d)", sX, sY);					
			if(useSkill == SKILL_TELEPORT)
			{
				return false;
			}
		}
		else
		{

			//------------------------------------------------------------
			// Buffer the next action when not in a repeat action
			//------------------------------------------------------------
			if (m_bRepeatAction)
			{
				m_fNextTrace	= FLAG_TRACE_NULL;

				return false;
			}
			else
			{
				DEBUG_ADD_FORMAT("Trace Sector To SpecialAction: Buffering");					
				
				m_fNextTrace	= FLAG_TRACE_SECTOR_SPECIAL;
				m_NextTraceX	= sX;
				m_NextTraceY	= sY;

				m_bRepeatAction = FALSE;

				// 200411, 12, 21, sobeit modify start - fix skill not repeating when used while moving
				//return false;
				return true;
				// 200411, 12, 21, sobeit modify end
			}
		}

		return true;
	}

	// Means it is not used on a Zone
	return false;
}

//----------------------------------------------------------------------
// Is Item Hidden In Darkness
//----------------------------------------------------------------------
// An item on a dark tile cannot be traced or picked up unless the player
// is a ghost, or is a vampire or has LIGHTNESS outside zone 3001.
// This is the negation of the item hover gate in CGameUpdate.cpp.
//----------------------------------------------------------------------
bool
MPlayer::IsItemHiddenInDarkness(const MItem* pItem) const
{
	return g_pZone->GetSector(pItem->GetX(), pItem->GetY()).HasDarkness()
		&& !HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		&& (g_pZone->GetID() == 3001
			|| (!IsVampire() && !HasEffectStatus( EFFECTSTATUS_LIGHTNESS )));
}

//----------------------------------------------------------------------
// Trace Item
//----------------------------------------------------------------------
// Must approach the Item and pick it up.
// If not standing still.. set it in Next.
//----------------------------------------------------------------------
bool	
MPlayer::TraceItem(TYPE_OBJECTID id)
{
	//-----------------------------------------------------
	// Must not have a delay.
	//-----------------------------------------------------
	if (!IsNotDelay() 
		|| HasEffectStatus( EFFECTSTATUS_ETERNITY_PAUSE )
		|| IsInCasket()	// [new skill 3]
		|| (IsInDarkness() && (!HasEffectStatus( EFFECTSTATUS_LIGHTNESS )||g_pZone->GetID() == 3001)
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		))
	{
		return false;
	}

	if (IsStop()) // || m_Action!=ACTION_ATTACK)
	{
		// Check whether the Item exists in the Zone.
		MItem*	pItem = m_pZone->GetItem(id);

		// the item is not in the zone, or darkness hides it
		if (pItem==NULL
			|| IsItemHiddenInDarkness( pItem ))
		{
			return false;
		}
	
		//-------------------------------------------------------------
		// If already within pickup range of the Item..
		//-------------------------------------------------------------
		if (max(abs(pItem->GetX()-m_X), abs(pItem->GetY()-m_Y)) <= 1)
		{
			DEBUG_ADD("Pickup Item in Trace Dist.");

			TraceNULL();
			
			// Tracing is complete, so pick up the Item.
			PickupItem( pItem );	
			
			return true;
		}


		// Set trace info
		SetTraceID( id );
		m_fTrace	= FLAG_TRACE_ITEM;
		m_TraceX	= pItem->GetX();
		m_TraceY	= pItem->GetY();
		m_TraceZ	= 0;

		// Remember the target currently being traced.
		m_fTraceBuffer	= m_fTrace;	
		

		//------------------------------------------------------------		
		// Should TraceDistance vary with the current Player's abilities?
		//------------------------------------------------------------
		
		m_TraceDistance		= 1;
		
		//------------------------------------------------------------

		// Set the tracing path
		SetAction( m_MoveAction );
		SetNextDestination(m_TraceX, m_TraceY);

		// Remove buffering.
		m_fNextTrace			= FLAG_TRACE_NULL;
	
		// Stop continuing the chase
		m_bKeepTraceCreature	= false;

		//------------------------------------------------------------
		// Print message
		//------------------------------------------------------------
		DEBUG_ADD_FORMAT("Trace IteM: %d", id);				
	}
	else
	{
		//------------------------------------------------------------
		// Buffer the next action when not in a repeat action
		//------------------------------------------------------------
		if (m_bRepeatAction)
		{
			m_fNextTrace	= FLAG_TRACE_NULL;

			return false;
		}
		else
		{
			m_fNextTrace	= FLAG_TRACE_ITEM;
			m_NextTraceID	= id;

			m_bRepeatAction = FALSE;

			return false;
		}
	}

	return true;
}

//----------------------------------------------------------------------
// Trace InteractionObject
//----------------------------------------------------------------------
// Approach the InteractionObject and change currentAction.
// --> When we get there, if currentAction
//     has "already" been changed, it must not be changed.
//
// If not standing still.. set it in Next.
//----------------------------------------------------------------------
bool	
MPlayer::TraceInteractionObject(TYPE_OBJECTID id, BYTE currentAction)
{
	//-----------------------------------------------------
	// Must not have a delay.
	//-----------------------------------------------------
	if (!IsNotDelay()
		|| HasEffectStatus( EFFECTSTATUS_ETERNITY_PAUSE )
		|| IsInCasket())	// [new skill 3]
	{
		return false;
	}

	//------------------------------------------------
	// Cannot open a door while riding a motorcycle.
	//------------------------------------------------
	if (m_MoveDevice == MOVE_DEVICE_RIDE)
	{
		return false;
	}

	if (IsStop()) // || m_Action!=ACTION_ATTACK)
	{
		// Check whether the Item exists in the Zone.
		MImageObject*	pImageObject = m_pZone->GetImageObject(id);

		// Return if there is no ImageObject
		if (pImageObject==NULL) return false;

		// Return if not an InteractionObject
		if (pImageObject->GetObjectType()!=MObject::TYPE_INTERACTIONOBJECT)
			return false;	 	

		// Set trace info
		SetTraceID( id );
		m_fTrace			= FLAG_TRACE_INTERACTIONOBJECT;
		m_TraceObjectAction	= currentAction;
		m_TraceX			= pImageObject->GetX();
		m_TraceY			= pImageObject->GetY();
		m_TraceZ			= 0;

		// Remember the target currently being traced.
		m_fTraceBuffer	= m_fTrace;	

		//------------------------------------------------------------		
		// Should TraceDistance vary with the current Player's abilities?
		//------------------------------------------------------------
		
		m_TraceDistance		= 1;
		
		//------------------------------------------------------------

		// Set the tracing path
		SetAction( m_MoveAction );
		SetNextDestination(m_TraceX, m_TraceY);

		// Remove buffering.
		m_fNextTrace			= FLAG_TRACE_NULL;
	
		// Stop continuing the chase
		m_bKeepTraceCreature	= false;

		//------------------------------------------------------------
		// Print message
		//------------------------------------------------------------
		DEBUG_ADD_FORMAT("Trace InteractionObject: %d", id);				
	}
	else
	{
		//------------------------------------------------------------
		// Buffer the next action when not in a repeat action
		//------------------------------------------------------------
		if (m_bRepeatAction)
		{
			m_fNextTrace	= FLAG_TRACE_NULL;

			return false;
		}
		else
		{
			m_fNextTrace			= FLAG_TRACE_INTERACTIONOBJECT;
			m_NextTraceID			= id;
			m_NextTraceObjectAction	= currentAction;

			m_bRepeatAction = FALSE;

			return false;
		}
	}

	return true;
}

//----------------------------------------------------------------------
// Trace Effect
//----------------------------------------------------------------------
// Approach the SelectableEffect.
//----------------------------------------------------------------------
bool	
MPlayer::TraceEffect(TYPE_OBJECTID id)
{
	//-----------------------------------------------------
	// Must not have a delay.
	//-----------------------------------------------------
	if (!IsNotDelay()
		|| HasEffectStatus( EFFECTSTATUS_ETERNITY_PAUSE )
		|| IsInCasket())	// [new skill 3]
	{
		return false;
	}

	//------------------------------------------------
	// Cannot open a door while riding a motorcycle.
	//------------------------------------------------
	if (m_MoveDevice == MOVE_DEVICE_RIDE)
	{
		return false;
	}

	if (IsStop()) // || m_Action!=ACTION_ATTACK)
	{
		MEffect*	pEffect = m_pZone->GetEffect( id );

		if (pEffect==NULL)
		{
			pEffect = m_pZone->GetGroundEffect( id );

			if (pEffect==NULL)
			{
				return false;
			}
		}
		
		if (!pEffect->IsSelectable())
			return false;	 	

		// Set trace info
		SetTraceID( id );
		m_fTrace			= FLAG_TRACE_EFFECT;
		//m_TraceObjectAction	= currentAction;
		m_TraceX			= pEffect->GetX();
		m_TraceY			= pEffect->GetY();
		m_TraceZ			= 0;

		// Remember the target currently being traced.
		m_fTraceBuffer	= m_fTrace;	

		//------------------------------------------------------------		
		// Should TraceDistance vary with the current Player's abilities?
		//------------------------------------------------------------
		
		m_TraceDistance		= 1;
		
		//------------------------------------------------------------

		// Set the tracing path
		SetAction( m_MoveAction );
		SetNextDestination(m_TraceX, m_TraceY);

		// Remove buffering.
		m_fNextTrace			= FLAG_TRACE_NULL;
	
		// Stop continuing the chase
		m_bKeepTraceCreature	= false;

		//------------------------------------------------------------
		// Print message
		//------------------------------------------------------------
		DEBUG_ADD_FORMAT("Trace Effect: %d", id);				
	}
	else
	{
		//------------------------------------------------------------
		// Buffer the next action when not in a repeat action
		//------------------------------------------------------------
		if (m_bRepeatAction)
		{
			m_fNextTrace	= FLAG_TRACE_NULL;

			return false;
		}
		else
		{
			m_fNextTrace			= FLAG_TRACE_EFFECT;
			m_NextTraceID			= id;
			//m_NextTraceObjectAction	= currentAction;

			m_bRepeatAction = FALSE;

			return false;
		}
	}

	return true;
}



//----------------------------------------------------------------------
// Set Trace Position
//----------------------------------------------------------------------
// A sector with a Creature cannot be entered because the Creature is already there.
// So we have to pick the Sector that is closest to the Creature and
// also near the current Player.
//----------------------------------------------------------------------
/*
void
MPlayer::SetTraceDestination()
{
	// Set the order of directions to check.
	int		stepX = m_X - m_TraceX,
			stepY = m_Y - m_TraceY;
	int		signX = (stepX==0)? 0 : (stepX<0)? -1 : 1,
			signY = (stepY==0)? 0 : (stepY<0)? -1 : 1;

	int		check[8];

	// by signX, signY
	switch (DetermineDirection(signX, signY))
	{
		case RIGHT :
			
		break;
	}
	

	//-------------------------------------------------------
	// Check all 8 directions
	//-------------------------------------------------------
	for (int i=0; i<8; i++)
	{		
		switch (i)
		{
			case DIRECTION_LEFT			: next.x--;	next.y++;	break;
			case DIRECTION_RIGHT		: next.x++;	next.y--;	break;
			case DIRECTION_UP			: next.x--;	next.y--;	break;
			case DIRECTION_DOWN			: next.x++;	next.y++;	break;
			case DIRECTION_LEFTUP		: next.x--;				break;
			case DIRECTION_LEFTDOWN		: next.y++;				break;
			case DIRECTION_RIGHTUP		: next.y--;				break;
			case DIRECTION_RIGHTDOWN	: next.x++;				break;
		}

		//-------------------------------------------------------
		// Skip the check if outside the Zone's area.
		//-------------------------------------------------------
		if (next.x<0 || next.y<0 
					|| next.x>=m_pZone->GetWidth() || next.y>=m_pZone->GetHeight()) continue;

		//-------------------------------------------------------
		// Select it if it can be entered.
		//-------------------------------------------------------
		if (m_pZone->CanMove(next.x, next.y))
		{				
		}
	}
}
*/			

//----------------------------------------------------------------------
// Can Attack Tribe
//----------------------------------------------------------------------
// Find out which tribes the AttackMode allows attacking.
//----------------------------------------------------------------------
bool
MPlayer::CanAttackTribe( enum CREATURETRIBE to ) const
{
	switch ( m_AttackMode )
	{
		//--------------------------------------------
		// Peace: attack no one.
		//--------------------------------------------
		case ATTACK_MODE_PEACE :
			return false;
		break;

		//--------------------------------------------
		// Attack: attack anyone.
		//--------------------------------------------
		case ATTACK_MODE_AGGRESS :
			return true;
		break;

		//--------------------------------------------
		// Normal: attack enemies only.
		//--------------------------------------------
		// Enemy = different Tribe..
		// NPCs cannot be attacked.
		case ATTACK_MODE_NORMAL :
			return GetCreatureTribe()!=to;// && to!=CREATURETRIBE_NPC;
		break;		

		default:
		break;
	}

	return false;
}

//----------------------------------------------------------------------
// Can Attack Guild
//----------------------------------------------------------------------
// Find out which guilds the AttackMode allows attacking.
//----------------------------------------------------------------------
bool	
MPlayer::CanAttackGuild( const MCreature* pCreature ) const
{
	switch ( m_AttackMode )
	{
		//--------------------------------------------
		// Peace: attack no one.
		//--------------------------------------------
		case ATTACK_MODE_PEACE :
			return false;
		break;

		//--------------------------------------------
		// Attack: attack anyone.
		//--------------------------------------------
		case ATTACK_MODE_AGGRESS :
			return true;
		break;

		//--------------------------------------------
		// Normal: attack enemies only.
		//--------------------------------------------
		// Enemy = different Guild..
		case ATTACK_MODE_NORMAL :
		{
			int gn = pCreature->GetGuildNumber();

			return (m_GuildNumber!=gn
					// Currently users must not attack each other, so..
					// only non-user guilds can be attacked.
					// Shouldn't this be removed later?

					// For vampires. Only vampires use AttackByGuild for now
					&& gn!=GUILDID_VAMPIRE_DEFAULT && gn<MIN_USER_GUILDID)
					|| GetCreatureTribe()!=pCreature->GetCreatureTribe();
					// && to!=CREATURETRIBE_NPC;
		}
		break;		

		default:
		break;
	}

	return false;
}

//----------------------------------------------------------------------
// Action In Trace Distance
//----------------------------------------------------------------------
// Action handling when within range for the action
//
// return true : when in range
//		  false : out of range.
//----------------------------------------------------------------------
bool
MPlayer::ActionInTraceDistance()
{
	int nextAction;
	if (m_nUsedActionInfo==ACTIONINFO_NULL)
	{
		if (m_MoveDevice==MOVE_DEVICE_RIDE)
		{
			if (IsSlayer())
			{
				nextAction = ACTION_SLAYER_MOTOR_STAND;				
			}
			else
			{
				// (a mess)..
				//nextAction = ACTION_STAND;
			}
		}
		//-----------------------------------------------------
		// When walking
		//-----------------------------------------------------
		else if( m_MoveDevice == MOVE_DEVICE_SUMMON_SYLPH )
		{
			if( IsOusters() )
			{
				nextAction = ACTION_OUSTERS_FAST_MOVE_STAND;
			}
		}
		else
		{
			nextAction = ACTION_STAND;
		}
	}
	else
	{
		nextAction = GetActionInfoAction(m_nUsedActionInfo);
	}

	//-------------------------------------------------------
	//
	// [ Trace complete ]
	//
	// : within range --> stop tracing --> trace Action
	//
	//-------------------------------------------------------			
	if (max(abs(m_X-m_TraceX), abs(m_Y-m_TraceY)) 
		<= m_TraceDistance)
	{	
		//----------------------------------------------------
		//
		// Switch Action Mode since we are within range
		//
		//----------------------------------------------------
				
		//----------------------------------------------------
		// Tracing is complete, so take the appropriate action.
		// Set the appropriate action in m_NextAction..
		//----------------------------------------------------
		//---------------------
		// Action on self
		//---------------------
		if (m_fTrace & FLAG_TRACE_SELF)
		{
			//---------------------------------------------------------------
			// Depending on the currently selected special skill..
			//---------------------------------------------------------------					
			//m_ActionCount = m_ActionCountMax;
			m_MoveCount = m_MoveCountMax;					
			SetNextAction(nextAction);

			//--------------------
			// message
			//--------------------
			DEBUG_ADD("Do Self SpecialAction");							
			
			return true;
		}
		//----------------------------------------------------
		//
		//					Basic action
		//
		//----------------------------------------------------
		else if (m_fTrace & FLAG_TRACE_CREATURE_BASIC)
		{
			//---------------------------------------------------------------
			// Depending on the weapon currently in use...
			//---------------------------------------------------------------
			// pCreature
			// m_TraceX, m_TraceY is the destination
			//---------------------------------------------------------------
			//m_ActionCount = m_ActionCountMax;
			m_MoveCount = m_MoveCountMax;

			BasicActionToCreature();			// by viva	

			return true;
		}
		//----------------------------------------------------
		//
		//						Special action
		//
		//----------------------------------------------------
		else if (m_fTrace & FLAG_TRACE_CREATURE_SPECIAL)
		{
			//---------------------------------------------------------------
			// Depending on the special attack set in the Slot (it may not be an attack)..
			//---------------------------------------------------------------
			// pCreature
			// m_TraceX, m_TraceY is the destination
			//---------------------------------------------------------------
			//m_ActionCount = m_ActionCountMax;
			m_MoveCount = m_MoveCountMax;
			SetNextAction(nextAction);

			/*
			#ifdef	CONNECT_SERVER
				CGAttackNormal _CGAttackNormal;
				_CGAttackNormal.setX( m_X );
				_CGAttackNormal.setY( m_Y );
				_CGAttackNormal.setDir( m_Direction );
				_CGAttackNormal.setObjectID( m_TraceID );
				g_pSocket->sendPacket( &_CGAttackNormal );

				#ifdef	OUTPUT_DEBUG
					DEBUG_ADD( _CGAttackNormal.toString().c_str() );							
				#endif
			#endif
			*/

			//--------------------
			// message
			//--------------------
			DEBUG_ADD_FORMAT("Special Attack : %d", m_TraceID);							
			
			return true;
		}
		//----------------------------------------------------
		//
		//				Basic action -> Sector
		//
		//----------------------------------------------------
		else if (m_fTrace & FLAG_TRACE_SECTOR_BASIC)
		{
			//---------------------------------------------------------------
			// Depending on the weapon currently in use...
			//---------------------------------------------------------------
			// pCreature
			// m_TraceX, m_TraceY is the destination
			//---------------------------------------------------------------
			//m_ActionCount = m_ActionCountMax;
			m_MoveCount = m_MoveCountMax;
			SetNextAction(nextAction);

			/*
			#ifdef	CONNECT_SERVER
				CGAttackNormal _CGAttackNormal;
				_CGAttackNormal.setX( m_X );
				_CGAttackNormal.setY( m_Y );
				_CGAttackNormal.setDir( m_Direction );
				_CGAttackNormal.setObjectID( m_TraceID );
				g_pSocket->sendPacket( &_CGAttackNormal );

				#ifdef	OUTPUT_DEBUG
					DEBUG_ADD( _CGAttackNormal.toString().c_str() );							
				#endif
			#endif
			*/

			//--------------------
			// message
			//--------------------
			DEBUG_ADD_FORMAT("Basic Attack Sector : (%d, %d)", m_TraceX, m_TraceY);
			
			return true;
		}
		//----------------------------------------------------
		//
		//				Special action -> Sector
		//
		//----------------------------------------------------
		else if (m_fTrace & FLAG_TRACE_SECTOR_SPECIAL)
		{
			//---------------------------------------------------------------
			// Depending on the special attack set in the Slot (it may not be an attack)..
			//---------------------------------------------------------------
			// pCreature
			// m_TraceX, m_TraceY is the destination
			//---------------------------------------------------------------
			//m_NextAction = ACTION_SHOOT;
			//m_ActionCount = m_ActionCountMax; 
			m_MoveCount = m_MoveCountMax;
			SetNextAction(nextAction);

			/*
			#ifdef	CONNECT_SERVER
				CGAttackNormal _CGAttackNormal;
				_CGAttackNormal.setX( m_X );
				_CGAttackNormal.setY( m_Y );
				_CGAttackNormal.setDir( m_Direction );
				_CGAttackNormal.setObjectID( m_TraceID );
				g_pSocket->sendPacket( &_CGAttackNormal );

				#ifdef	OUTPUT_DEBUG
					DEBUG_ADD( _CGAttackNormal.toString().c_str() );							
				#endif
			#endif
			*/

			//--------------------
			// message
			//--------------------
			DEBUG_ADD_FORMAT("Special Attack Sector : (%d, %d)", m_TraceX, m_TraceY);							
			//*/

			return true;
		}
		//----------------------------------------------------
		//
		//					Item trace
		//
		//----------------------------------------------------
		else if (m_fTrace & FLAG_TRACE_ITEM)
		{
			MItem*	pItem = m_pZone->GetItem( m_TraceID );

			// Tracing is complete, so pick up the Item.
			if (pItem!=NULL
				&& !IsItemHiddenInDarkness( pItem ))
			{
				PickupItem( pItem );
			}

			return true;
		}
		//----------------------------------------------------
		//
		//			Use skill on an Inventory Item
		//
		//----------------------------------------------------
		else if (m_fTrace & FLAG_TRACE_INVENTORY)
		{
			MItem* pItem = g_pInventory->GetItemToModify( m_TraceID );

			// Tracing is complete, so pick up the Item.
			if (pItem!=NULL)
			{
				// To call ActionEffect()...
				m_MoveCount = m_MoveCountMax;
				SetNextAction(nextAction);
			}
			else
			{
				DEBUG_ADD_FORMAT("No such Item in Inventory: id=%d", m_TraceID);
			}
			
			return true;
		}
		//----------------------------------------------------
		//
		//			Effect trace
		//
		//----------------------------------------------------
		else if (m_fTrace & FLAG_TRACE_EFFECT)
		{
			MEffect* pEffect = m_pZone->GetEffect( m_TraceID );

			if (pEffect==NULL)
			{
				pEffect = m_pZone->GetGroundEffect( m_TraceID );

				if (pEffect==NULL)
				{
					return true;
				}
			}

			if (pEffect->IsSelectable())
			{		
					MEffectTarget* pEffectTarget = pEffect->GetEffectTarget();

					if (pEffectTarget!=NULL)
					{
						bool bSendPacket = true;
						if (pEffectTarget->GetEffectTargetType()==MEffectTarget::EFFECT_TARGET_PORTAL)
						{					
							if (IsSlayer())
							{
								bSendPacket = false;
							}
						}						

						if (bSendPacket)
						{
							int serverID = pEffectTarget->GetServerID();

							CGSelectTileEffect _CGSelectTileEffect;
							_CGSelectTileEffect.setEffectObjectID( serverID );
							
							g_pSocket->sendPacket( &_CGSelectTileEffect );
						}
					}
			}

			return true;
		}	
		//----------------------------------------------------
		//
		//			InteractionObject trace
		//
		//----------------------------------------------------
		else if (m_fTrace & FLAG_TRACE_INTERACTIONOBJECT)
		{
			MImageObject* pImageObject = m_pZone->GetImageObject( m_TraceID );

			if (pImageObject->GetObjectType()==MObject::TYPE_INTERACTIONOBJECT)
			{
				MInteractionObject*	pInteractionObject = (MInteractionObject*)pImageObject;
				pInteractionObject->SetNextAction( m_TraceObjectAction );
				
			}

			return true;
		}				

		return false;
	}
	//-------------------------------------------------------
	// When tracing must continue
	//-------------------------------------------------------
	else
	{
		//------------------------------------------
		// When there is no path to trace --> pathfinding
		//------------------------------------------
		if (m_listDirection.empty())
		{
			//---------------------
			// Basic action
			//---------------------
			if (m_fTrace & FLAG_TRACE_CREATURE_BASIC)
			{
				TraceCreatureToBasicAction( m_TraceID, m_bTraceCreatureToForceAttack );
			}
			//---------------------
			// Special action
			//---------------------
			else  if (m_fTrace & FLAG_TRACE_CREATURE_SPECIAL)
			{
				TraceCreatureToSpecialAction( m_TraceID, m_bTraceCreatureToForceAttack );
			}
			//---------------------
			// Basic action -> Sector
			//---------------------
			else if (m_fTrace & FLAG_TRACE_SECTOR_BASIC)
			{
				TraceSectorToBasicAction( m_TraceX, m_TraceY );
			}
			//---------------------
			// Special action -> Sector
			//---------------------
			else if (m_fTrace & FLAG_TRACE_SECTOR_SPECIAL)
			{
				// 2004, 9, 18, sobeit add start - turret fire is ignored when out of range
				if (m_nUsedActionInfo==SKILL_TURRET_FIRE)
				{
					m_fNextTrace = FLAG_TRACE_NULL;
					UnSetRepeatAction();
					InstallTurretStopAttack();
					//SetAction(ACTION_STAND);
					return false;
				}
				// 2004, 9, 18, sobeit add end
				TraceSectorToSpecialAction( m_TraceX, m_TraceY );
			}
			//---------------------
			// Item trace
			//---------------------
			else if (m_fTrace & FLAG_TRACE_ITEM)
			{
				TraceItem( m_TraceID );
			}
			//---------------------
			// Use skill on Inventory
			//---------------------
			else if (m_fTrace & FLAG_TRACE_INVENTORY)
			{
				TraceInventoryItem( m_TraceID );
			}
			//---------------------
			// Effect trace
			//---------------------
			else if (m_fTrace & FLAG_TRACE_EFFECT)
			{
				TraceEffect( m_TraceID );
			}
			//---------------------
			// InteractionObject trace
			//---------------------
			else if (m_fTrace & FLAG_TRACE_INTERACTIONOBJECT)
			{
				TraceInteractionObject( m_TraceID, m_TraceObjectAction );
			}

			// Trace while moving..
			//SetAction( m_MoveAction );
		}


		//m_NextAction = m_MoveAction;
		SetNextAction( m_MoveAction );
	}

	return false;
}

//----------------------------------------------------------------------
// Move
//----------------------------------------------------------------------
// Similar to MCreature::Move, but MPlayer::Move
//    reads directions one by one from m_listDirection and moves.
// 
// However, if blocked along the way, the path must be chosen again (SetDest...)
//----------------------------------------------------------------------
void	
MPlayer::ActionMove()
{
	#ifdef OUTPUT_DEBUG_PLAYER_ACTION
		DEBUG_ADD("ActionMove");
	#endif

	//----------------------------------------------------
	//
	// When the next Sector is reached
	//
	//----------------------------------------------------
	// Read one direction from m_listDirection,
	// check it, and move.
	//----------------------------------------------------
	if( HasEffectStatus( EFFECTSTATUS_TRYING ) )
	{
		MZone::EFFECT_MAP::const_iterator itr = g_pZone->GetGroundEffects();
		bool bFind = false;

		for(int i = 0; i < g_pZone->GetGroundEffectCount(); itr++,i++ )
		{
			const MEffect* pEffect = itr->second;
			
			if( GetX() == pEffect->GetX() &&
				GetY() == pEffect->GetY() &&
				pEffect->GetFrameID() == EFFECTSPRITETYPE_REGEN_TOWER_GROUND )
			{
				bFind = true;
				break;
			}
		}
		if( bFind == false )
			RemoveEffectStatus( EFFECTSTATUS_TRYING );
	}

	if (m_MoveCount==0)//m_sX==0 && m_sY==0)
	{
		TYPE_SECTORPOSITION	x, y;

		//-------------------------------------------------------------
		// For fast move, the coordinates are in nextX, nextY.
		//-------------------------------------------------------------
		if (m_bFastMove)
		{
			x = m_NextX;
			y = m_NextY;

			m_NextX = SECTORPOSITION_NULL;					

			//m_CurrentDirection = 
			m_Direction = GetDirectionToPosition( x, y );

		}
		//-------------------------------------------------------------
		// Normal movement
		//-------------------------------------------------------------
		else
		{
			//----------------------------------------------------
			// Do not move if there is no target position.
			//----------------------------------------------------
			if (m_DestX==SECTORPOSITION_NULL && m_NextDestX==SECTORPOSITION_NULL) 
			{
				DEBUG_ADD("### Don't need to move any more - Move Stop! ###");
				
				m_MoveCount = m_MoveCountMax;
				return;
			}

				//sprintf(g_pDebugMessage->GetCurrent(), "ListSize = %d", m_listDirection.size());
			
			//----------------------------------------------------
			// 
			// 1. Do pathfinding and move..
			// 2. Send to have the Server verify..
			//
			//----------------------------------------------------
			// Print the number of messages awaited from the Server
			if (m_SendMove != 0  && g_bNetStatusGood)
			{
				static int lastSendCount = 0;

				if (lastSendCount != m_SendMove)
				{
					DEBUG_ADD_FORMAT("Waiting MoveOK = %d", m_SendMove);						
					
					lastSendCount = m_SendMove;
				}
			}

			//----------------------------------------------------
			//
			//
			//                 If tracing..
			//
			//
			//----------------------------------------------------		
			if (m_fTrace != FLAG_TRACE_NULL)
			{				
				//----------------------------------------------------	
				// If tracing a Creature..
				// The Creature may vanish or
				// the trace target coordinates may change, so check.
				//----------------------------------------------------	
				if ((m_fTrace & FLAG_TRACE_CREATURE_BASIC) 
					|| (m_fTrace & FLAG_TRACE_CREATURE_SPECIAL))
				{
					// Read the info of the Creature being traced.
					MCreature*	pCreature = m_pZone->GetCreature( m_TraceID );				

					//GET_DYING_CREATURE( pCreature, m_TraceID );

					//-------------------------------------------------------
					// If the traced Creature vanished --> stop tracing
					// As a Slayer, I cannot chase a character that went into Darkness.
					//-------------------------------------------------------								
					if (pCreature==NULL 
						|| (pCreature->IsInDarkness() && !pCreature->IsNPC() && 
						((!IsVampire() && !HasEffectStatus( EFFECTSTATUS_LIGHTNESS ))  || 
						(g_pZone->GetID() == 3001 && IsVampire()) ||
						(pCreature->IsOusters() && pCreature->IsInGroundElemental() && !g_pPlayer->IsOusters()))
						&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )						
#ifdef __METROTECH_TEST__
						&& !g_bLight
#endif
						)) 
					{
						if (IsRepeatAction())
						{
							m_fNextTrace = FLAG_TRACE_NULL;

							// The button was released, so cancel action repeat.
							UnSetRepeatAction();
							//TraceNextNULL();
						}
						else
						{
							// Stop tracing
							TraceNULL();
								
							// Stop.
							SetStop();					
						}

						#ifdef OUTPUT_DEBUG_PLAYER_ACTION
							DEBUG_ADD("creature lost");
						#endif
							
						return;			
					}
				
					//-------------------------------------------------------			
					// If the coordinates of the Creature being traced changed
					//-------------------------------------------------------			
					if (pCreature->GetX()!=m_TraceX ||
						pCreature->GetY()!=m_TraceY)
					{										
						m_NextDestX = pCreature->GetX();
						m_NextDestY = pCreature->GetY();
						// Z does not matter.
					}

					m_TraceX	= pCreature->GetX();
					m_TraceY	= pCreature->GetY();
					m_TraceZ	= pCreature->GetZ();
				}
				//----------------------------------------------------	
				// If tracing an Item..
				// The Item may vanish, so check.
				//----------------------------------------------------
				else if (m_fTrace & FLAG_TRACE_ITEM)
				{
					// Read the info of the Creature being traced.
					MItem*	pItem = m_pZone->GetItem( m_TraceID );

					//-------------------------------------------------------
					// If the traced Item vanished --> stop tracing
					//-------------------------------------------------------
					if (pItem==NULL
						|| IsItemHiddenInDarkness( pItem ))
					{
						// Stop tracing
						TraceNULL();
							
						// Stop.
						SetStop();					
							
						#ifdef OUTPUT_DEBUG_PLAYER_ACTION
							DEBUG_ADD("item lost");
						#endif

						return;			
					}
				}

				//-------------------------------------------------------
				// Determine whether within range for the action.
				//-------------------------------------------------------
				if (ActionInTraceDistance())
				{
					// Use skill~~
					if (m_nNextUsedActionInfo!=ACTIONINFO_NULL)
					{						
						m_nUsedActionInfo = m_nNextUsedActionInfo;
						m_nNextUsedActionInfo = ACTIONINFO_NULL;

						// Set next action
						m_bNextAction = true;
						// 2004, 11, 23, sobeit add start - ugh.. I caught a cold.. can't see anything..
						if(m_nUsedActionInfo == SKILL_SET_AFIRE)
						{
							if(max(abs(m_X-m_TraceX), abs(m_Y-m_TraceY))<2)
								SetNextAction(ACTION_ATTACK);
							else
								SetNextAction(ACTION_STAND);
						}
						else
						// 2004, 11, 23, sobeit add end
							SetNextAction(GetActionInfoAction( m_nUsedActionInfo ));
					}

					// No need to go any further.
					m_listDirection.clear();

					// Stop Action
					//m_ActionCount = m_ActionCountMax;
					m_MoveCount = m_MoveCountMax;

					// Make the Player face the direction of the current enemy.
					SetDirectionToPosition( m_TraceX, m_TraceY );
					
							
					//----------------------------------------------------			
					// Stop tracing unless continuing to trace
					//----------------------------------------------------
					if (m_bKeepTraceCreature)
					{
						int fTrace = m_fTrace;
						
						// Stop
						SetStop();

						m_fTrace = fTrace;
					}	
					else
					{
						TraceNULL();

						// Stop
						SetStop();
					}
				}
			
			}

			//----------------------------------------------------
			// Set the buffered pathfinding result.
			//----------------------------------------------------
			if (m_NextDestX!=SECTORPOSITION_NULL)
			{
				// Pathfinding
				//SetDestination(m_NextDestX, m_NextDestY);
				m_DestX		= m_NextDestX;	// TEST
				m_DestY		= m_NextDestY;	// TEST
				m_NextDestX = SECTORPOSITION_NULL;
				m_NextDestY = SECTORPOSITION_NULL;			
			}

			//----------------------------------------------------
			// Pathfinding: do pathfinding on every Tile moved.
			//----------------------------------------------------
			if (m_DestX!=SECTORPOSITION_NULL)
				SetDestination(m_DestX, m_DestY);

			//----------------------------------------------------
			// Send to have the Server verify
			//----------------------------------------------------
			//SetAction( m_MoveAction );

			
			//----------------------------------------------------
			// Only allow movement if the number of
			// unverified moves is at or below the set number.
			//----------------------------------------------------
			if (m_SendMove > g_pClientConfig->MAX_CLIENT_MOVE)	
			{			
				g_bNetStatusGood = false;

				#ifdef OUTPUT_DEBUG_PLAYER_ACTION
					DEBUG_ADD("max client move");
				#endif
				
				return;
			}
			else
			{
				g_bNetStatusGood = true;
			}

			//----------------------------------------------------
			// When there is no path
			//----------------------------------------------------
			if (m_listDirection.empty())
			{
				//--------------------------------------------
				// When the destination is reached..
				//--------------------------------------------
				if (m_DestX==m_X && m_DestY==m_Y)
				{
					m_DestX = SECTORPOSITION_NULL;
					m_DestY = SECTORPOSITION_NULL;

					#ifdef OUTPUT_DEBUG_PLAYER_ACTION
						DEBUG_ADD("dest reached");
					#endif

					return;
				}

				#ifdef OUTPUT_DEBUG_PLAYER_ACTION
					DEBUG_ADD("no way");
				#endif

				return;
			}		

			
			//----------------------------------------------------	
			//
			// Move along the direction.
			//
			//----------------------------------------------------			
			//----------------------------------------------------
			// Read one direction from m_listDirection.
			//----------------------------------------------------
			m_Direction = m_listDirection.front();
			m_listDirection.pop_front();	
				
			//----------------------------------------------------
			// When pathfinding must be done next...
			//----------------------------------------------------
			if (m_NextDestX!=SECTORPOSITION_NULL)
			{
				//m_NextAction = m_MoveAction;
				SetNextAction( m_MoveAction );
			}

			//----------------------------------------------------
			// Advance one Sector in the direction read,
			// and reset cX, cY, sX, sY.
			//----------------------------------------------------

			//----------------------------------------------------
			// Check whether movement is possible
			//----------------------------------------------------
			x = m_X;
			y = m_Y;

			switch (m_Direction)
			{
				case DIRECTION_LEFTDOWN		: x--;	y++;	break;
				case DIRECTION_RIGHTUP		: x++;	y--;	break;
				case DIRECTION_LEFTUP		: x--;	y--;	break;
				case DIRECTION_RIGHTDOWN	: x++;	y++;	break;
				case DIRECTION_LEFT			: x--;			break;
				case DIRECTION_DOWN			:		y++;	break;
				case DIRECTION_UP			:		y--;	break;
				case DIRECTION_RIGHT		: x++;			break;
			}
		}

		//--------------------------------------------------------------------
		// When movement is possible
		//--------------------------------------------------------------------
		if (m_pZone->CanMove(m_MoveType, x,y)
			|| m_bFastMove		// Always movable for Fast Move..
			)
		{		
			//---------------------------------------------------------------
			// For Fast Move..
			//---------------------------------------------------------------
			if (m_bFastMove)
			{
				// nothing
			}
			//---------------------------------------------------------------
			// Normal movement --> needs verification
			//---------------------------------------------------------------
			else
			{
				//----------------------------------------------------
				// Cancel if installing a mine
				//----------------------------------------------------
				if (PlayerPresentation::IsInstallMineProgress())
				{
					PlayerPresentation::EndInstallMineProgress();
				}

				CGMove _CGMove;
				_CGMove.setX( static_cast<Coord_t>(m_X) );
				_CGMove.setY( static_cast<Coord_t>(m_Y) );
				_CGMove.setDir( m_Direction );
				g_pSocket->sendPacket( &_CGMove );

				//yckou
//				UpdateSocketOutput();
				// Remember the direction sent to the Server.
				m_listSendDirection.push_back( m_Direction );
				m_SendMove++;
				

				//----------------------------------------------------
				// When moving while talking to an NPC..
				//----------------------------------------------------
				if (m_WaitVerifyActionInfo==WAIT_VERIFY_NPC_ASK)
				{
					MCreature* pCreature = g_pZone->GetCreature( g_pPCTalkBox->GetNPCID() );

					if (pCreature!=NULL)
					{
						//----------------------------------------------------
						// Dialog stops when the distance grows.
						//----------------------------------------------------
						if ((max(abs(m_X-pCreature->GetX()), abs(m_Y-pCreature->GetY()))) > TRADE_DISTANCE_NPC)
						{
							// Remove verification
							SetWaitVerifyNULL();

							// Close the Dialog.
							g_pUIDialog->ClosePCTalkDlg();
						}
					}
				}
			}

			//-----------------------
			// Change the view
			//-----------------------
			//m_pZone->MoveLightSight(m_X, m_Y, m_LightSight, m_Direction);
			//m_pZone->UnSetLightSight(m_X, m_Y, m_LightSight);
			//m_pZone->SetLightSight(x, y, m_LightSight);

			// Remember previous coordinates
			TYPE_SECTORPOSITION ox = m_X;
			TYPE_SECTORPOSITION oy = m_Y;

			// Update the zone's sector info.
			MovePosition( x, y );
			
			//CheckInDarkness();

			//--------------------------------------------
			//
			// When the destination is a 'portal'
			//
			//--------------------------------------------
			if (IsSlayer())
			{
				const MSector& sector = m_pZone->GetSector(x, y);
				if (sector.IsPortal())
				{
					int numPortal = sector.GetPortalSize();		
					
					if (numPortal!=0)
					{
						PORTAL_LIST::const_iterator	iPortal = sector.GetPortalBegin();

						PORTAL_INFO portalInfo = *iPortal;			

						//--------------------------------------------
						// For a multi portal, show the elevator.
						//--------------------------------------------
						if (portalInfo.Type==MPortal::TYPE_MULTI_PORTAL)
						{
							DEBUG_ADD("[MPlayer] Run Elevator");
							
							PlayerPresentation::RunElevator();

							// Remove the destination marker.
							WorldEvents::SetSelectedSectorNULL();

							// Destination removed..
							m_DestX		= SECTORPOSITION_NULL;
							m_DestY		= SECTORPOSITION_NULL;	
							m_NextDestX = SECTORPOSITION_NULL;
							m_NextDestY = SECTORPOSITION_NULL;	
						}

						iPortal++;
					}
				}
			}

			//--------------------------------------------
			// Assuming there are 5 Frames
			//--------------------------------------------
			//
			// [1] for UP, DOWN, LEFT, RIGHT,
			//
			//     X change : 0 16 32 48 64   (+-16)
			//     Y change : 0  8 16 24 32   (+-8)
			// 
			//
			// [2] when moving diagonally (LEFTUP, LEFTDOWN, RIGHTUP, RIGHTDOWN),
			//
			//     X change : 0  8 16 24 32   (+-8)
			//     Y change : 0  4  8 12 16   (+-4)
			//
			//--------------------------------------------
			// sX,sY : total pixels to move (one TILE)
			// cX,cY : pixels per move step
			//--------------------------------------------		
			//--------------------------------------------
			// For Fast Move
			//--------------------------------------------
			if (m_bFastMove)
			{
				// Pixel distance when moving in a straight line
				// ex) left 2 --> 1 : ( 2 - 1 ) * TILE_X
				//     up 2 --> 1 : ( 2 - 1 ) * TILE_Y
				m_sX = (ox - m_X) * TILE_X;
				m_sY = (oy - m_Y) * TILE_Y;
					
				//------------------------------------------------
				// Move to the destination at half the speed of a normal one-tile move.
				//------------------------------------------------
				// Number of Frames for one tile move
				int moveTimes_div_2 = (*g_pCreatureTable)[m_CreatureType].MoveTimes >> 1;
				// 2005, 1, 5, sobeit add start
				if(HasEffectStatus(EFFECTSTATUS_BIKE_CRASH))
				{
//					moveTimes_div_2 = moveTimes_div_2/2;
					m_MoveCountMax = m_MoveCountMax*2;
				}
				// 2005, 1, 5, sobeit add end
				m_cX = -m_sX / moveTimes_div_2;
				m_cY = -m_sY / moveTimes_div_2;

				m_MoveCount += 2;

				if (m_MoveCount > m_MoveCountMax)
				{
					m_MoveCount = m_MoveCountMax;
				}
			}
			//--------------------------------------------	
			// Normal move
			//--------------------------------------------
			else
			{	
				m_DirectionMoved = m_Direction;

				m_sX = m_sXTable[m_DirectionMoved];
				m_sY = m_sYTable[m_DirectionMoved];

				//????????????????????????????????????????????????????????
				//???                                                  ???
				//???   Move one Frame... or not??   ???
				//???                                                  ???
				//????????????????????????????????????????????????????????
				int moveTimes_1;
				
				#ifdef OUTPUT_DEBUG
					if (m_CreatureType >= g_pCreatureTable->GetSize())
					{
						DEBUG_ADD_FORMAT_ERR("[Error] CreatureType is exceed MAX. id=%d, type=%d", m_ID, m_CreatureType);
						
						SetStop();

						return;
					}
				#endif

				if (m_MoveDevice==MOVE_DEVICE_WALK)
				{
					moveTimes_1 = (*g_pCreatureTable)[m_CreatureType].MoveTimes-1;
				}
				else if (m_MoveDevice==MOVE_DEVICE_RIDE)
				{
					moveTimes_1 = (*g_pCreatureTable)[m_CreatureType].MoveTimesMotor-1;
				}
				else if (m_MoveDevice==MOVE_DEVICE_SUMMON_SYLPH || HasEffectStatusSummonSylph( dynamic_cast<MCreature*>(this) ))
				{
					moveTimes_1 = (*g_pCreatureTable)[m_CreatureType].MoveTimesMotor-1;
				}
				else
				{					
					DEBUG_ADD_FORMAT_ERR("[Error] What is MoveType of this Player??? id=%d", m_ID);
					
					SetStop();

					return;
				}

				m_MoveTableCount = 0;
				m_cX = m_cXTable[moveTimes_1][m_DirectionMoved][m_MoveTableCount];
				m_cY = m_cYTable[moveTimes_1][m_DirectionMoved][m_MoveTableCount];
			}

			m_sX += m_cX;
			m_sY += m_cY;

			m_MoveCount++;

			// Set the count for the next move.
			m_NextMoveCount = (*g_pCreatureTable)[m_CreatureType].MoveRatio;

			//------------------------------------------------
			// Play the Sound matching the character's MoveAction.
			//------------------------------------------------
			TYPE_SOUNDID soundID = (*g_pCreatureTable)[m_CreatureType].GetActionSound( m_MoveAction );

			if (soundID!=SOUNDID_NULL)
			{
				WorldEvents::PlaySoundAt( soundID,
							false,
							m_X,
							m_Y);
			}
		}	
		//--------------------------------------------
		// When movement is impossible
		//--------------------------------------------
		// Redo the pathfinding.
		else
		{
			m_NextDestX = m_DestX;
			m_NextDestY = m_DestY;
			m_listDirection.clear();


			DEBUG_ADD("다른 캐릭터에 의해서 Block됐음.");					
		}

		// Move to the next Sector.
		if (!m_listDirection.empty())
		{
			//m_NextAction = m_MoveAction;
			SetNextAction( m_MoveAction );		
		}
	}
	//----------------------------------------------------
	//
	// When the next Sector is not yet reached
	//
	//----------------------------------------------------
	else
	{
		// Move one Frame.
		/*
		switch (m_MoveDevice)
		{
			case MOVE_DEVICE_NULL :			
				m_sX += m_cXTableDeviceNull[m_Direction][m_ActionCount];
				m_sY += m_cYTableDeviceNull[m_Direction][m_ActionCount];
			break;

			case MOVE_DEVICE_MOTOR1 :						
				m_sX += m_cXTableDeviceMotor1[m_Direction][m_ActionCount];
				m_sY += m_cYTableDeviceMotor1[m_Direction][m_ActionCount];
			break;
		}
		*/

		#ifdef OUTPUT_DEBUG_PLAYER_ACTION
			DEBUG_ADD("going");
		#endif

		// Move only when the move count is reached.
		if (m_MoveCount>=m_NextMoveCount)
		{			
			//--------------------------------------------
			// For Fast Move
			//--------------------------------------------
			if (m_bFastMove)
			{
				// 2001.8.22 added
				if (m_sX!=0)	m_sX += m_cX;
				if (m_sY!=0)	m_sY += m_cY;		

				m_MoveCount += 2;

				if (m_MoveCount > m_MoveCountMax)
				{
					m_MoveCount = m_MoveCountMax;
				}

				// Set the count for the next move.
				m_NextMoveCount += (*g_pCreatureTable)[m_CreatureType].MoveRatio;

				if (m_MoveCount+1 >= m_MoveCountMax
					&& m_nNoPacketUsedActionInfo==ACTIONINFO_NULL)
				{
					//----------------------------------------------------------
					// Hmm.. test code.
					//----------------------------------------------------------
					// Hit once after fast move..
					// Flash Sliding
					//----------------------------------------------------------
					//m_TraceID	= id;		
					MCreature* pCreature = g_pZone->GetCreature( m_TraceID );

					//GET_DYING_CREATURE( pCreature, m_TraceID );

					if (pCreature!=NULL 
						&& m_nSpecialActionInfo != MAGIC_RAPID_GLIDING 
						&& m_nSpecialActionInfo != SKILL_TELEPORT
						&& !HasEffectStatus(EFFECTSTATUS_BIKE_CRASH)
						)		// currently selected is... so..
					{
//						_MinTrace(" SpecialActionInfo : %d UsedAction : %d NextUsedActionInfo :%d\n",
//							m_nSpecialActionInfo, m_nUsedActionInfo, m_nNextUsedActionInfo);
						#ifdef OUTPUT_DEBUG
							DEBUG_ADD("[FastMovePosition] set next action to attack");
						#endif

						UnSetRequestMode();

						// Set as the used skill
						
						//---------------------------------------------------
						// (heh)..
						//---------------------------------------------------
						MActionResult* pResult = new MActionResult;

						// Show result
						int ActionInfo = GetBasicActionInfo() ; //m_nBasicActionInfo;
						if( m_nSpecialActionInfo == SKILL_BLITZ_SLIDING || m_nSpecialActionInfo == SKILL_BLAZE_WALK )
						{
							m_fTrace	= FLAG_TRACE_CREATURE_SPECIAL;
							if(m_nSpecialActionInfo == SKILL_BLITZ_SLIDING )
								m_nNextUsedActionInfo	= SKILL_BLITZ_SLIDING_ATTACK;
							else if( m_nSpecialActionInfo == SKILL_BLAZE_WALK )
								m_nNextUsedActionInfo	= SKILL_BLAZE_WALK_ATTACK;
							else
								return;
							m_TraceX	= pCreature->GetX();
							m_TraceY	= pCreature->GetY();
							m_TraceZ	= pCreature->GetZ();
							
							m_bTraceCreatureToForceAttack	= false;
							m_bKeepTraceCreature			= false;
							m_TraceDistance		= m_BasicAttackDistance;
							
							m_bNextAction = true;
							SetNextAction(GetActionInfoAction( m_nNextUsedActionInfo ));
							SetDirection( SelectFacingDirection( GetX(), GetY(), pCreature->GetX(), pCreature->GetY() ) );
							return;
						}
						if(m_nSpecialActionInfo == SKILL_SET_AFIRE)
						{
							m_nNextUsedActionInfo = SKILL_SET_AFIRE;
							m_nNoPacketUsedActionInfo = m_nNextUsedActionInfo;
							m_NextAction = ACTION_ATTACK;
							
							return;
						}
//						if(m_nSpecialActionInfo == MAGIC_BLOODY_ZENITH)
//						{
//							SetBloodyZenith(16);
//						}

						m_nNextUsedActionInfo	= GetBasicActionInfo() ; //m_nBasicActionInfo;

						pResult->Add( new MActionResultNodeActionInfo(
														ActionInfo,
														m_ID,
														pCreature->GetID(), 
														pCreature->GetX(),
														pCreature->GetY(),
														0 ) 
										);	

						// Add as a result of the Player's skill currently in progress.
						if( m_nUsedActionInfo == SKILL_BLITZ_SLIDING_ATTACK ||
							m_nUsedActionInfo == SKILL_BLAZE_WALK_ATTACK 
							)
							m_nUsedActionInfo = GetBasicActionInfo(); //m_nBasicActionInfo;
						
						int tempActionInfo = m_nUsedActionInfo;
						m_nUsedActionInfo = GetBasicActionInfo(); //m_nBasicActionInfo;	// reckless code
						
						// reckless ~~
						if (m_listEffectTarget.size()==0)
						{
							m_pEffectTarget = new MEffectTarget( 0 );
							m_pEffectTarget->NewEffectID();

							m_listEffectTarget.push_back( m_pEffectTarget );							
						}

						PacketAddActionResult( 0, pResult );

						m_nUsedActionInfo = tempActionInfo;
						//---------------------------------------------------
						// (heh)... added on 2001.12.10..
						//---------------------------------------------------
						
						// Set trace info
						m_fTrace	= FLAG_TRACE_CREATURE_BASIC;
						m_TraceX	= pCreature->GetX();
						m_TraceY	= pCreature->GetY();
						m_TraceZ	= pCreature->GetZ();

						m_bTraceCreatureToForceAttack	= true;
						m_bKeepTraceCreature			= false;
						m_TraceDistance		= m_BasicAttackDistance;
						
						m_bNextAction = true;
						SetNextAction(GetActionInfoAction( m_nNextUsedActionInfo ));
						SetDirection( SelectFacingDirection( GetX(), GetY(), pCreature->GetX(), pCreature->GetY() ) );

						// Do not send a packet once next time.
						m_nNoPacketUsedActionInfo = m_nNextUsedActionInfo;
					}
					else
					{
						#ifdef OUTPUT_DEBUG
							DEBUG_ADD_FORMAT("[FastMovePosition] there is no such creature.id=%d", m_TraceID);
						#endif
					}
				}
	
			}
			//--------------------------------------------
			// For normal movement
			//--------------------------------------------
			else
			{
				int moveTimes_1;
							
				if (m_MoveDevice==MOVE_DEVICE_WALK)
				{
					moveTimes_1 = (*g_pCreatureTable)[m_CreatureType].MoveTimes-1;
				}
				else if (m_MoveDevice==MOVE_DEVICE_RIDE)
				{	
					moveTimes_1 = (*g_pCreatureTable)[m_CreatureType].MoveTimesMotor-1;
				}
				else if (m_MoveDevice==MOVE_DEVICE_SUMMON_SYLPH || HasEffectStatusSummonSylph( dynamic_cast<MCreature*>(this)))
				{
					moveTimes_1 = (*g_pCreatureTable)[m_CreatureType].MoveTimesMotor-1;
				}
				else
				{
					DEBUG_ADD_FORMAT_ERR("[Error] What is MoveType of this Creature??? id=%d", m_ID);
					SetStop();					
					return;
				}

				m_MoveTableCount++;
				// 2004, 12, 22, sobeit add start - if the max value is exceeded, a wrong coordinate gets in.
				if(m_MoveTableCount > moveTimes_1)
					m_MoveTableCount = moveTimes_1;
				// 2004, 12, 22, sobeit add end
				m_cX = m_cXTable[moveTimes_1][m_DirectionMoved][m_MoveTableCount];
				m_cY = m_cYTable[moveTimes_1][m_DirectionMoved][m_MoveTableCount];

				// 2001.8.22 added
				if (m_sX!=0)	m_sX += m_cX;
				if (m_sY!=0)	m_sY += m_cY;		
			
				// Set the count for the next move.
				m_NextMoveCount += (*g_pCreatureTable)[m_CreatureType].MoveRatio;

				m_MoveCount++;
			}
		}	
		else
		{
			m_MoveCount++;
		}
		
		//m_ActionCount++;
		// Change the frame. If the last Frame was reached, go back to Frame 0.
		//if (++m_ActionCount == (*m_pFrames)[m_Action][m_Direction].GetCount())
		//	m_ActionCount = 0;

		//if (m_ActionCount == (m_ActionCountMax>>1))
		//if (m_MoveCount == (m_MoveCountMax>>1))
		//{				
		//	SetLightSight( m_LightSight );
		//}
	}

	#ifdef OUTPUT_DEBUG_PLAYER_ACTION
		DEBUG_ADD("moveok");
	#endif
}

//----------------------------------------------------------------------
// Attach CastingEffect
//----------------------------------------------------------------------
void	
MPlayer::AttachCastingEffect(TYPE_ACTIONINFO nUsedActionInfo, BOOL bForceAttach)
{
	MCreature::AttachCastingEffect(nUsedActionInfo, bForceAttach);

	// (gasp)..
}

//----------------------------------------------------------------------
// Affect UsedActionInfo
//----------------------------------------------------------------------
// Execute m_nUsedActionInfo.
//----------------------------------------------------------------------
void
MPlayer::AffectUsedActionInfo(TYPE_ACTIONINFO nUsedActionInfo)
{
	if (nUsedActionInfo==ACTIONINFO_NULL)
	{
		return;
	}

	switch(nUsedActionInfo)
	{
	case SKILL_ACID_STORM:
		{
			if((*g_pRankBonusTable)[RANK_BONUS_WIDE_STORM].GetStatus() == RankBonusInfo::STATUS_LEARNED)
				nUsedActionInfo = SKILL_ACID_STORM_WIDE;
		}
		break;
		
	case SKILL_POISON_STORM:
		{
			if((*g_pRankBonusTable)[RANK_BONUS_WIDE_STORM].GetStatus() == RankBonusInfo::STATUS_LEARNED)
				nUsedActionInfo = SKILL_POISON_STORM_WIDE;
		}
		break;

	case SKILL_KASAS_ARROW:
		break;

	case SKILL_LARSLASH :
		
		break;					

	case SKILL_SWEEP_VICE_1:
	case SKILL_SWEEP_VICE_3:
	case SKILL_SWEEP_VICE_5:
		if(g_pSkillAvailable->IsEnableSkill( SKILL_SWEEP_VICE_1 ))
		{
		}
		else
			return;
		break;
		//by viva
//	case SKILL_SLAYER_HEAL_SACRED_STAMP:
//		return;
	}
	POINT point;	
#ifdef __METROTECH_TEST__
//	if(g_bLight)
//	{
//		int plusX = GetX()-m_TraceX;
//		int plusY = GetY()-m_TraceY;
//		int moveX = 0, moveY = 0;
//		if(plusX != 0)
//			moveX = plusX/abs(plusX);
//		if(plusY != 0)
//			moveY = plusY/abs(plusY);
//		
//		g_pPlayer->UnSetRequestMode();
//		
//		g_SelectSector.x = GetX()+moveX;
//		g_SelectSector.y = GetY()+moveY;
//		if(abs(g_SelectSector.x - m_TraceX) > GetActionInfoRange(nUsedActionInfo))
//			g_SelectSector.x = GetX();
//		if(abs(g_SelectSector.y - m_TraceY) > GetActionInfoRange(nUsedActionInfo))
//			g_SelectSector.y = GetY();
//		
//		// Set as the next target position
//		g_pPlayer->TraceNULL();
//		
//		if (g_pPlayer->SetMovePosition(g_SelectSector.x, g_SelectSector.y))
//		{
//			g_pPlayer->SetNextActionToMove();
//		}
//		
//		// If there is no next place to go
//		POINT point;
//		g_pPlayer->GetNextDestination( point );		
//		if (point.x==SECTORPOSITION_NULL || point.y==SECTORPOSITION_NULL)
//		{
//			// If there is no place currently being headed to
//			g_pPlayer->GetDestination( point );
//			if (point.x==SECTORPOSITION_NULL || point.y==SECTORPOSITION_NULL)
//			{
//				//-_-;;
//			}
//			else
//			{
//				g_pTopView->SetSelectedSector( point );
//			}
//		}
//		else
//		{
//			g_pTopView->SetSelectedSector( point );
//		}		
//	}
#endif

	//-----------------------------------------------------
	// EffectTarget to use in this skill
	//-----------------------------------------------------
	MEffectTarget* pEffectTarget = NULL;

	DEBUG_ADD_FORMAT("[MPlayer-StartEffect] ActionInfo=%d, ActionCount=%d/%d", nUsedActionInfo, m_ActionCount, m_ActionCountMax);
	
	//--------------------------------------------------------
	// Is it a Casting ActionInfo?
	//--------------------------------------------------------
	BOOL bCastingAction = (*g_pActionInfoTable)[nUsedActionInfo].IsCastingAction();

	//--------------------------------------------------------
	// Target position in Pixel coordinates
	//--------------------------------------------------------
	point = MapToPixel(m_TraceX, m_TraceY);
	//point.x += m_sX;
	//point.y += m_sY;
	//point.x += TILE_X_HALF;
	
	//--------------------------------------------------------
	//
	//					Set Effect target
	//Z
	//--------------------------------------------------------
	//MEffectTarget* m_pEffectTarget = new MEffectTarget( (*g_pActionInfoTable)[nUsedActionInfo].GetSize() );
	// 
	//--------------------------------------------------------
	// For a casting action
	//--------------------------------------------------------
	if (bCastingAction)
	{
		// Create an EffectTarget for casting.
		pEffectTarget = new MEffectTarget( (*g_pActionInfoTable)[nUsedActionInfo].GetSize() );
		pEffectTarget->NewEffectID();

		if( nUsedActionInfo == CASTING_FIRE || nUsedActionInfo == CASTING_WATER ||
			nUsedActionInfo == CASTING_GROUND || nUsedActionInfo == CASTING_FIRE2 ||
			nUsedActionInfo == CASTING_GROUND2 || nUsedActionInfo == CASTING_WATER2 )
			pEffectTarget->Set( point.x, point.y, m_TraceZ, GetID() );
		else
			pEffectTarget->Set( point.x, point.y, m_TraceZ, m_TraceID );

		//--------------------------------------------------------
		// Delay also applies in castingAction, but
		// for castingAction the delay must not be removed.
		//--------------------------------------------------------
		// There is no way to verify this.
		//if (m_DelayActionInfo==nUsedActionInfo) - -;
		{
			pEffectTarget->SetDelayFrame( m_EffectDelayFrame );
			m_DelayActionInfo	= ACTIONINFO_NULL;
			m_EffectDelayFrame	= 0;
		}

	}
	//--------------------------------------------------------
	// For a real skill, not a casting action..
	//--------------------------------------------------------
	else
	{
		//--------------------------------------------------------
		// For a start skill.. new was already done, so
		// only create the EffectTarget for a result skill.
		//--------------------------------------------------------
		if (nUsedActionInfo < (*g_pActionInfoTable).GetMinResultActionInfo())
		{
		}
		else
		{
			if (m_pEffectTarget != NULL)
			{
				DEBUG_ADD_FORMAT("RemoveEffectTarget: action=(%d/%d) ActionInfo=%d, pEffectTarget=%p", m_ActionCount, m_ActionCountMax, nUsedActionInfo, static_cast<const void*>(m_pEffectTarget));
				
				RemoveEffectTarget( m_pEffectTarget->GetEffectID() );
				MEffectTarget* pTempEffectTarget = m_pEffectTarget;
				m_pEffectTarget = NULL;
				
				// m_pEffectTarget gets accessed while being deleted
				delete pTempEffectTarget;				
			}

			//----------------------------------------------------------
			// Create one result in advance.
			//----------------------------------------------------------
			m_pEffectTarget = new MEffectTarget( (*g_pActionInfoTable)[nUsedActionInfo].GetSize() );
			m_pEffectTarget->NewEffectID();

			// So it can be deleted later..
			AddEffectTarget( m_pEffectTarget );
			//m_listEffectTarget.push_back( pEffectTarget );

			DEBUG_ADD_FORMAT("NewEffectTarget: action=(%d/%d) ActionInfo=%d, pEffectTarget=%p", m_ActionCount, m_ActionCountMax, nUsedActionInfo, static_cast<const void*>(m_pEffectTarget));
		}
		

		if (m_pEffectTarget==NULL)
		{	
			DEBUG_ADD_FORMAT("EffectTarget is NULL, newEffectTarget : %p", static_cast<const void*>(m_pEffectTarget));
			
			m_pEffectTarget = new MEffectTarget( (*g_pActionInfoTable)[nUsedActionInfo].GetSize() );
			m_pEffectTarget->NewEffectID();

			DEBUG_ADD("new EffectTarget OK");
		}

//		if( nUsedActionInfo == MAGIC_ACID_TOUCH || nUsedActionInfo == MAGIC_BLOODY_NAIL ||
//			nUsedActionInfo == SKILL_FLOURISH || nUsedActionInfo == SKILL_ABSORB_SOUL)
		if(nUsedActionInfo != ACTIONINFO_NULL && (*g_pActionInfoTable)[nUsedActionInfo].IsAttachSelf() )
		{
			//m_pEffectTarget->Set( point.x, point.y, m_TraceZ, m_TraceID );
			m_pEffectTarget->Set( point.x, point.y, m_TraceZ, GetID() );
		} else
		{
			m_pEffectTarget->Set( point.x, point.y, m_TraceZ, m_TraceID );
		}

		//--------------------------------------------------------
		// Play the Sound matching the Action
		//--------------------------------------------------------
		// Only play the sound here for results.
		if (nUsedActionInfo >= g_pActionInfoTable->GetMinResultActionInfo())
		{
			int soundID = (*g_pCreatureTable)[m_CreatureType].GetActionSound( 
									GetActionInfoAction(nUsedActionInfo) );

			WorldEvents::PlaySoundAt( soundID,	false, m_X, m_Y	);

			//-------------------------------------------------
			// Force Feel
			//-------------------------------------------------
			if (PlayerPresentation::ForceFeelReady()
				&& soundID < g_pSoundTable->GetSize())
			{
				DEBUG_ADD_FORMAT("ForceAction-AffectUsedActionInfo(%d, %s)", soundID, strrchr((*g_pSoundTable)[soundID].Filename.GetString(), '\\'));

				PlayerPresentation::ForceAction( soundID );
			}
		}

		//--------------------------------------------------------
		// Set duration
		//--------------------------------------------------------
		if (m_DelayActionInfo==nUsedActionInfo)
		{
			m_pEffectTarget->SetDelayFrame( m_EffectDelayFrame );
			m_DelayActionInfo	= ACTIONINFO_NULL;
			m_EffectDelayFrame	= 0;
		}

		// Save it,
		pEffectTarget = m_pEffectTarget;
		
		// and delete..
		m_pEffectTarget = NULL;
	}

	// Remember it as an Effect in progress.
	//AddEffectTarget( m_pEffectTarget );
	
	// Set info for the result - delete happens when the result is applied..
	/*
	MActionResult* pResult = new MActionResult;
	pResult->Add( new MActionResultNodeActionInfo( m_ID, m_TraceID, nUsedActionInfo ) );
	
	m_pEffectTarget->SetResult( pResult );
	*/
	

	//--------------------------------------------------------
	//
	//					Decide the start position.
	//
	// Default to the target position. An entry whose start flag is only
	// "sky" (MAGIC_METEOR, RESULT_MAGIC_METEOR and SKILL_ERUPTION in the
	// shipped Action.inf) matches neither branch below, and used to hand
	// uninitialised x/y to the effect generator - /RTC1 stops on it.
	//
	//--------------------------------------------------------
	int x = point.x, y = point.y, z = m_Z, direction;

	//--------------------------------------------------------
	// Start at the user's position
	//--------------------------------------------------------
	if ((*g_pActionInfoTable)[nUsedActionInfo].IsStartUser())
	{
		point = MapToPixel(m_X, m_Y);
		point.x += m_sX;
		point.y += m_sY;
		//point.x += TILE_X_HALF;

		x			= point.x;
		y			= point.y;
		z			= m_Z+1;//+60;
	}
	//--------------------------------------------------------
	// Start at the target's position
	//--------------------------------------------------------
	else if ((*g_pActionInfoTable)[nUsedActionInfo].IsStartTarget())
	{
		x			= point.x;
		y			= point.y;
		z			= m_Z;//+60;			
	}

	//--------------------------------------------------------
	// Start in the sky
	//--------------------------------------------------------
	if ((*g_pActionInfoTable)[nUsedActionInfo].IsStartSky())
	{
		z	= m_Z + (*g_pActionInfoTable)[nUsedActionInfo].GetValue();

		//direction	= DIRECTION_DOWN;
	}
	//--------------------------------------------------------
	// Start on the ground
	//--------------------------------------------------------
	//else
	//{
	if( nUsedActionInfo == SKILL_ABSORB_SOUL )
	{
		// m_TraceID is a corpse in the zone, so it is an item.
		direction = SelectFacingDirection(
			GetX(), GetY(),m_TraceX, m_TraceY);
	}
	else
		direction	= m_Direction;
	//}

	DEBUG_ADD_FORMAT("[MPlayer-StartEffect] Before Generate");
	
	//--------------------------------------------------------
	//
	//                   Create Effect
	//
	//--------------------------------------------------------
	g_pEffectGeneratorTable->Generate(
			x,y,z,				// start position
			direction, 		// direction
			1,					// power
			nUsedActionInfo,		//	ActionInfoTable type,
			pEffectTarget		// target info
			, GetActionGrade()
	);
	ClearActionGrade();

	DEBUG_ADD_FORMAT("[MPlayer-StartEffect] AffectUsedActionInfo OK");
}

//----------------------------------------------------------------------
// Action to Send Packet
//----------------------------------------------------------------------
// Send the packet...
//----------------------------------------------------------------------
void
MPlayer::ActionToSendPacket()
{
	//--------------------------------------------------------
	// Only send the packet if it is not a result action.
	//--------------------------------------------------------
	if (m_nUsedActionInfo >= g_pActionInfoTable->GetMinResultActionInfo() || m_nUsedActionInfo == ACTIONINFO_NULL)
	{
		return;
	}

	switch(m_nUsedActionInfo)
	{
	case SKILL_ACID_STORM_WIDE:
		m_nUsedActionInfo = SKILL_ACID_STORM;
		break;

	case SKILL_POISON_STORM_WIDE:
		m_nUsedActionInfo = SKILL_POISON_STORM;
		break;
	case SKILL_BURNING_SOUL_CHARGING:
		if(IsBurningSol()) 
			return;
		break;
	case SKILL_SWEEP_VICE_1:
	case SKILL_SWEEP_VICE_3:
	case SKILL_SWEEP_VICE_5:
//		if(!GetSweepViewValue()) //when Sweep Vice is not yet in use
//		{
//			SetSweepViewValue(1);
//			g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);
//			ExecuteActionInfoFromMainNode(SKILL_CLIENT_SWEEP_VICE,g_SelectSector.x, g_SelectSector.y, 0,g_pPlayer->GetDirection(),0	,	
//				g_SelectSector.x, g_SelectSector.y, 0, 0xffff, NULL, false);
//			return;
//		}
		if(g_bRButtonDown/*||!GetSweepViewValue()*/) 
			return;
			
		if(2 == GetSweepViewValue() || 3 == GetSweepViewValue())
			m_fTraceBuffer = FLAG_TRACE_SECTOR_SPECIAL;
		break;
	case SKILL_PLASMA_ROCKET_LAUNCHER:
		if(g_pCurrentMagazine != NULL)
		 {
			 int MagazineCount = g_pCurrentMagazine->GetNumber();
			 if(MagazineCount>0)
			 {
				 g_pCurrentMagazine->SetNumber(MagazineCount-1);
			 }
			 else
				 return;
		 }
		else return;
		break;
	// add by Coffee	2007-6-9
		
	case SKILL_BLLODY_SCARIFY :
	case SKILL_BLOOD_CURSE :
	case SKILL_VAMPIRE_INNATE_DEADLY_CLAW://by viva
	case SKILL_VAMPIRE_POISON_VOODOO_RING://by viva
/*		if (IsVampire() && g_pZoneTable->Get( m_pZone->GetID() )->CannotUseSpecialItem == false)
		{
			if (!IsWaitVerify()
				&& IsItemCheckBufferNULL())
			{
				MItem* pItem = NULL;
				pItem = g_pInventory->FindItem(ITEM_CLASS_MOON_CARD, 6);
				if (pItem!=NULL)
				{
					CGUseItemFromInventory _CGUseItemFromInventory;
					_CGUseItemFromInventory.setObjectID( pItem->GetID() );
					_CGUseItemFromInventory.setX( pItem->GetGridX() );
					_CGUseItemFromInventory.setY( pItem->GetGridY() );
					g_pSocket->sendPacket( &_CGUseItemFromInventory );
					SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
				}
				else
				{
					g_pGameMessage->AddSafeFormat("[瓊刻] %s",(*g_pGameStringTable)[STRING_STATUS_NOT_FIND_SKILL_CRAD].GetString());
					return;
				}
			}
		}else return;
*/		break;
	case SKILL_SHINE_SWORD :
	case SKILL_BOMB_CRASH_WALK :
	case SKILL_SATELLITE_BOMB :
	case SKILL_ILLUSION_INVERSION :
	case SKILL_HEAVEN_GROUND :
	case SKILL_SLAYER_SWORD_SKY_FIRE://by viva
	case SKILL_SLAYER_BLADE_CUT_STORM://by viva
	case SKILL_SLAYER_GUN_BOMB_XRL_MISSILE://by viva
	case SKILL_SLAYER_HEAL_SACRED_STAMP://by viva
	case SKILL_SLAYER_ENCHANT_BRAMBLE_HALO://by viva

/*		if (IsSlayer() && g_pZoneTable->Get( m_pZone->GetID() )->CannotUseSpecialItem == false)
		{
			if (!IsWaitVerify()
				&& IsItemCheckBufferNULL())
			{
				MItem* pItem = NULL;
				pItem = g_pInventory->FindItem(ITEM_CLASS_MOON_CARD, 5);
				if (pItem!=NULL)
				{
					CGUseItemFromInventory _CGUseItemFromInventory;
					_CGUseItemFromInventory.setObjectID( pItem->GetID() );
					_CGUseItemFromInventory.setX( pItem->GetGridX() );
					_CGUseItemFromInventory.setY( pItem->GetGridY() );
					g_pSocket->sendPacket( &_CGUseItemFromInventory );
					SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
				}
				else
				{
					g_pGameMessage->AddSafeFormat("[瓊刻] %s",(*g_pGameStringTable)[STRING_STATUS_NOT_FIND_SKILL_CRAD].GetString());
					return;
				}
			}
		}else return;
*/		break;
	case SKILL_DUMMY_DRAKE :
	case SKILL_HYDRO_CONVERGENCE :
	case SKILL_SUMMON_CLAY :
	case SKILL_HETER_CHAKRAM :
/*		if (IsOusters() && g_pZoneTable->Get( m_pZone->GetID() )->CannotUseSpecialItem == false)
		{
			if (!IsWaitVerify()
				&& IsItemCheckBufferNULL())
			{
				MItem* pItem = NULL;
				pItem = g_pInventory->FindItem(ITEM_CLASS_MOON_CARD, 7);
				if (pItem!=NULL)
				{
					CGUseItemFromInventory _CGUseItemFromInventory;
					_CGUseItemFromInventory.setObjectID( pItem->GetID() );
					_CGUseItemFromInventory.setX( pItem->GetGridX() );
					_CGUseItemFromInventory.setY( pItem->GetGridY() );
					g_pSocket->sendPacket( &_CGUseItemFromInventory );
					SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
				}
				else
				{
					g_pGameMessage->AddSafeFormat("[瓊刻] %s",(*g_pGameStringTable)[STRING_STATUS_NOT_FIND_SKILL_CRAD].GetString());
					return;
				}
			}
		}else return;
*/		break;
	// add end by Coffee	
	}

	/*
	// [proper timing]..
	BOOL	bActionStand	= (m_Action==ACTION_STAND || IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND);
	int		actionCountMax_1 = m_ActionCountMax-1;
	BOOL	bEndAction		= (m_ActionCount==actionCountMax_1);// || bActionStand;
	int		StartFrame		= (*g_pActionInfoTable)[m_nUsedActionInfo].GetStartFrame( m_WeaponSpeed );

	int		packetSendTiming = min(actionCountMax_1, StartFrame);
	BOOL	bStartAction	= (m_ActionCount==packetSendTiming);	// packetSendTiming was originally 0.
	
	if (bActionStand)
	{
		bEndAction = bStartAction;
	}
	*/

	BOOL	bActionStand	= (m_Action==ACTION_STAND || (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND) || (IsOusters() && m_Action == ACTION_OUSTERS_FAST_MOVE_STAND));
	int		actionCountMax_1 = m_ActionCountMax-1;
	BOOL	bEndAction		= (m_ActionCount==actionCountMax_1) || bActionStand;
	int		StartFrame		= (*g_pActionInfoTable)[m_nUsedActionInfo].GetStartFrame( m_WeaponSpeed );

	BOOL	bStartAction	= (m_ActionCount==0);	
	

	// An Effect starts when..
	// (1) it is the StartFrame
	// (2) it is the last ActionFrame
	[[maybe_unused]] BOOL	bStartEffect = m_ActionCount==StartFrame || 
							(StartFrame >= m_ActionCountMax && bEndAction);
	
	//BOOL	bCastingEffect	= GetActionInfoCastingStartFrame(m_nUsedActionInfo)==m_ActionCount;
		// Start frame of the repeated action
	bStartAction |= m_bRepeatAction 
					&& (*g_pActionInfoTable)[m_nUsedActionInfo].IsUseRepeatFrame()
					&& m_RepeatCount!=0
					&& m_ActionCount == GetActionInfoRepeatStartFrame( m_nUsedActionInfo );

	//----------------------------------------------------------------------
	// [ TEST CODE ]
	//----------------------------------------------------------------------
	// This is quite risky, but..
	// it should not be a big problem...
	//----------------------------------------------------------------------
	// The EffectTarget is created after the casting motion ends,
	// but its ID must be sent to the server when casting starts,
	// so as a stopgap the EffectTarget was created first and used.
	//----------------------------------------------------------------------
	// Hmm. Changed it to a data member.
	//static MEffectTarget* 

	
	#ifdef OUTPUT_DEBUG
			//DEBUG_ADD_FORMAT("action=(%d/%d) ActionInfo=%d, pEffectTarget=%x", m_ActionCount, m_ActionCountMax, m_nUsedActionInfo, m_pEffectTarget);
	#endif

	//----------------------------------------------------------
	//
	//					Motion start point..
	//
	//----------------------------------------------------------
	if (//bStartEffect)
		bStartAction || m_nUsedActionInfo == SKILL_BURNING_SOUL_LAUNCH)
	{			
		//----------------------------------------------------------
		// This is the case where no Packet is sent.
		//----------------------------------------------------------
		if (m_nNoPacketUsedActionInfo==m_nUsedActionInfo)
		{
			m_nNoPacketUsedActionInfo = ACTIONINFO_NULL;

			int soundID = (*g_pCreatureTable)[m_CreatureType].GetActionSound( 
											GetActionInfoAction(m_nUsedActionInfo) );

			//------------------------------------------------
			// Play the Sound matching the character's Action.
			//------------------------------------------------
			WorldEvents::PlaySoundAt( soundID,	false, m_X, m_Y	);

			//-------------------------------------------------
			// Force Feel
			//-------------------------------------------------
			if (PlayerPresentation::ForceFeelReady())
			{				
				if ((*g_pActionInfoTable)[m_nUsedActionInfo].IsWeaponTypeGunAny())
				{
					soundID = SOUND_SLAYER_ATTACK_AR;
				}

				if (soundID < g_pSoundTable->GetSize())
				{
					DEBUG_ADD_FORMAT("ForceAction-ActionToSendPacket(%d, %s)", soundID, strrchr((*g_pSoundTable)[soundID].Filename.GetString(), '\\'));
					PlayerPresentation::ForceAction( soundID );
				}
			}

			return;
		}

		//----------------------------------------------------------------------
		// The Packet must be sent at this stage.
		//----------------------------------------------------------------------
		// However, even if ActionEffect() was called, not in every case
		// does a Packet get sent.
		//
		// This is because the Player may take some Action after receiving from the Server.
		//
		// Before ActionEffect() is called..
		// a variable deciding whether to send a Packet will be needed.
		//
		// (!) The Packet must carry the Instance ID of the ActionInfo used.
		//     This is for checking when the result is received.
		//     Also, remember only the most recent Instance ID.
		//----------------------------------------------------------------------

		// Only send the Packet if it is not an Action showing a result
		// but an Action the Player actually used.
		if (m_nUsedActionInfo < (*g_pActionInfoTable).GetMinResultActionInfo())
		{
			//--------------------------------------------------------
			// Set the skill use time Delay.
			// Only for start skills..
			//--------------------------------------------------------
			if (m_Action==ACTION_STAND)
			{
				// a skill used standing still has a 0.3 s delay
				m_DelayTime	= g_FrameNow + MonotonicClock::Millis(300);
			}
			else if (m_bRepeatAction)
			{				
			}
			else
			{
				m_DelayTime	= g_FrameNow
								+ MonotonicClock::Duration(GetActionInfoDelay(m_nUsedActionInfo));
								// when sent at the right moment, subtract
								// - (m_ActionCount<<6);	// the delay of the action counts already passed
			}

		//----------------------------------------------------------
		//
		//			When connected to the server..
		//
		//----------------------------------------------------------

			// 
			if (m_pEffectTarget != NULL)
			{
				DEBUG_ADD_FORMAT("RemoveEffectTarget: action=(%d/%d) ActionInfo=%d, pEffectTarget=%p", m_ActionCount, m_ActionCountMax, m_nUsedActionInfo, static_cast<const void*>(m_pEffectTarget));
				
				RemoveEffectTarget( m_pEffectTarget->GetEffectID() );

				MEffectTarget* pTempEffectTarget = m_pEffectTarget;
				m_pEffectTarget = NULL;
				
				delete pTempEffectTarget;			
				pTempEffectTarget = NULL;
			}

			//----------------------------------------------------------
			// Create one result in advance.
			//----------------------------------------------------------
			m_pEffectTarget = new MEffectTarget( (*g_pActionInfoTable)[m_nUsedActionInfo].GetSize() );
			
			DEBUG_ADD_FORMAT("NewEffectTarget: action=(%d/%d) ActionInfo=%d, pEffectTarget=%p", m_ActionCount, m_ActionCountMax, m_nUsedActionInfo, static_cast<const void*>(m_pEffectTarget));
			
			//---------------------------------------------------------
			// Add as a result in progress.
			//----------------------------------------------------------
			AddEffectTarget( m_pEffectTarget );

			//----------------------------------------------------------
			// Get a new EffectID issued?
			//----------------------------------------------------------
			m_pEffectTarget->NewEffectID();

			
			if (m_fTraceBuffer!=FLAG_TRACE_NULL)
			{
				//----------------------------------------------------------
				// Set the time this skill can next be used
				//----------------------------------------------------------
				int originalActionInfo = m_nUsedActionInfo;
				if( (*g_pActionInfoTable)[m_nUsedActionInfo].GetParentActionInfo() != ACTIONINFO_NULL )
					originalActionInfo = (*g_pActionInfoTable)[m_nUsedActionInfo].GetParentActionInfo();
				if (auto* entry = g_pSkillInfoTable->GetMutable(originalActionInfo)) {
					entry->SetNextAvailableTime();
				}

				//----------------------------------------------------------
				// Set the type of packet to send
				//----------------------------------------------------------
				ACTIONINFO_PACKET	packetType		= (*g_pActionInfoTable)[m_nUsedActionInfo].GetPacketType();
				

				//----------------------------------------------------------
				//
				//			If used on an opponent..
				//
				//----------------------------------------------------------
				// Basic attack
				//----------------------------------------------------------
				if (m_fTraceBuffer & FLAG_TRACE_CREATURE_BASIC) 
				{
					CGAttack _CGAttack;
					_CGAttack.setX( static_cast<Coord_t>(m_X) );
					_CGAttack.setY( static_cast<Coord_t>(m_Y) );
					_CGAttack.setDir( m_Direction );
					_CGAttack.setObjectID( m_TraceID );
					g_pSocket->sendPacket( &_CGAttack );

					// [ TEST CODE ] - -;;
					// [ TEST CODE ] - -;;
					// [ TEST CODE ] - -;;
					/*
					#if defined(OUTPUT_DEBUG) && defined(__DEBUG_OUTPUT__) && defined(_DEBUG)
						if (g_DXInput.KeyDown(DIK_LMENU))
						{
							for (int i=0; i<200; i++)
							{
								g_pSocket->sendPacket( &_CGAttack );
							}
						}
					#endif
					*/

				}
				//----------------------------------------------------------
				//
				// For Blood drain
				//
				//----------------------------------------------------------
				else if (packetType==ACTIONINFO_PACKET_BLOOD_DRAIN)
				{
					CGBloodDrain _CGBloodDrain;
					_CGBloodDrain.setObjectID( m_TraceID );
					g_pSocket->sendPacket( &_CGBloodDrain );
			//		DEBUG_ADD("blood drain message sent");
					// 2001.8.20 commented out
//					SetWaitVerify( WAIT_VERIFY_SKILL_SUCCESS, m_nUsedActionInfo );
				}
				//----------------------------------------------------------
				//
				// For Soul drain
				//
				//----------------------------------------------------------
				else if (packetType==ACTIONINFO_PACKET_ABSORB_SOUL)
				{
					CGAbsorbSoul _CGAbsorbSoul;
					_CGAbsorbSoul.setObjectID( g_pZone->GetItemID( m_TraceX, m_TraceY ) );
					_CGAbsorbSoul.setTargetZoneX( m_TraceX );
					_CGAbsorbSoul.setTargetZoneY( m_TraceY );

					int larvaSize = (*g_pItemTable)[ITEM_CLASS_LARVA].GetSize();
					MItem* pLarvaItem = NULL;
					for(int i = 0; i < larvaSize; i++)
					{
						 pLarvaItem= g_pInventory->FindItem( ITEM_CLASS_LARVA, i );
						if(pLarvaItem != NULL)
							break;
					}

					if(pLarvaItem == NULL)
					{
						_CGAbsorbSoul.setInvenObjectID(OBJECTID_NULL);
						_CGAbsorbSoul.setInvenX(255);
						_CGAbsorbSoul.setInvenY(255);
						_CGAbsorbSoul.setTargetInvenX(255);
						_CGAbsorbSoul.setTargetInvenY(255);
					}
					else
					{
						_CGAbsorbSoul.setInvenObjectID(pLarvaItem->GetID());
						_CGAbsorbSoul.setInvenX(pLarvaItem->GetGridX());
						_CGAbsorbSoul.setInvenY(pLarvaItem->GetGridY());

						MOustersPupa pupa;

						// Same type as Lava
						pupa.SetItemType(pLarvaItem->GetItemType());

						POINT p;
						g_pInventory->GetFitPosition(&pupa, p);
						_CGAbsorbSoul.setTargetInvenX(static_cast<CoordInven_t>(p.x));
						_CGAbsorbSoul.setTargetInvenY(static_cast<CoordInven_t>(p.y));

						// (!!!) Items must not be movable until the verification packet is received.
						// Stop Item-related actions
						SetItemCheckBuffer(pLarvaItem, ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY);
						
						// Stop skill use
						// 2001.8.20 commented out
						// 2001.10.26 uncommented - Items must be verified.
						SetWaitVerify( WAIT_VERIFY_SKILL_SUCCESS, m_nUsedActionInfo );
					}

					g_pSocket->sendPacket( &_CGAbsorbSoul );

					
					// 2001.8.20 commented out
					SetWaitVerify( WAIT_VERIFY_SKILL_SUCCESS, m_nUsedActionInfo );
				}
				//----------------------------------------------------------
				//
				// When sending a packet used on another person
				//
				//----------------------------------------------------------
				else if	(packetType==ACTIONINFO_PACKET_OTHER
						||	(m_fTraceBuffer & FLAG_TRACE_CREATURE_SPECIAL))
				{
						//-----------------------------------------------
						// When using a skill by using an item
						//-----------------------------------------------					
						if ((*g_pActionInfoTable)[m_nUsedActionInfo].GetWeaponType()
							& FLAG_ACTIONINFO_WEAPON_HOLY_WATER)
						{						
							MItem* pUsingItem = g_pInventory->FindItem( ITEM_CLASS_HOLYWATER );
							
							//-----------------------------------------------
							// When there is no item to use
							//-----------------------------------------------
							if (pUsingItem==NULL)
							{
								//-------------------------------------------------
								// Skill Icon check - maybe needed..
								//-------------------------------------------------
								g_pSkillAvailable->SetAvailableSkills();

								// Stop tracing
								TraceNULL();

								//------------------------------------------
								// Wobbling in place
								//------------------------------------------
								SetAction( ACTION_STAND );

								m_ActionCount++;

								m_nUsedActionInfo = ACTIONINFO_NULL;
	
								return;
							}

							CGThrowItem	_CGThrowItem;
							_CGThrowItem.setObjectID( pUsingItem->GetID() );	// item id
							_CGThrowItem.setTargetObjectID( m_TraceID );	//
							_CGThrowItem.setX( pUsingItem->GetGridX() );
							_CGThrowItem.setY( pUsingItem->GetGridY() );

							g_pSocket->sendPacket( &_CGThrowItem );						
							
							//--------------------------------------------------
							// If there is more than one, only reduce the count.
							//--------------------------------------------------
							if (pUsingItem->GetNumber()>1)
							{
								pUsingItem->SetNumber( pUsingItem->GetNumber() - 1 );
							}
							//--------------------------------------------------
							// If only one is left, remove the Item completely
							//--------------------------------------------------
							else
							{
								MItem* pRemovedItem = g_pInventory->RemoveItem( pUsingItem->GetGridX(), pUsingItem->GetGridY() );

								if (pRemovedItem!=NULL)
								{
									// Remove item info
									PlayerPresentation::RemoveDescriptor( (void*)pRemovedItem );

									delete pRemovedItem;								
								}
								else
								{
									DEBUG_ADD_FORMAT_ERR("[Error] Cannot Remove Item to use Skill. id=%d", pUsingItem->GetID());
								}

								//-------------------------------------------------
								// Skill Icon check
								//-------------------------------------------------
								g_pSkillAvailable->SetAvailableSkills();
							}
						}
						//-----------------------------------------------
						// When using a skill by using a bomb
						//-----------------------------------------------					
						if ((*g_pActionInfoTable)[m_nUsedActionInfo].GetWeaponType()
							& FLAG_ACTIONINFO_WEAPON_BOMB)
						{		
							int itemType;

							MItem* pUsingItem = NULL;

							if (m_nUsedActionInfo==SKILL_THROW_BOMB)
							{
								pUsingItem = g_pInventory->FindItem( ITEM_CLASS_BOMB );
							}
							else
							{
								itemType = m_nUsedActionInfo - BOMB_SPLINTER;

								pUsingItem = g_pInventory->FindItem( ITEM_CLASS_BOMB, itemType );
							}
							
							//-----------------------------------------------
							// When there is no item to use
							//-----------------------------------------------
							if (pUsingItem==NULL)
							{
								//-------------------------------------------------
								// Skill Icon check - maybe needed..
								//-------------------------------------------------
								g_pSkillAvailable->SetAvailableSkills();

								// Stop tracing
								TraceNULL();

								//------------------------------------------
								// Wobbling in place
								//------------------------------------------
								SetAction( ACTION_STAND );

								m_ActionCount++;

								m_nUsedActionInfo = ACTIONINFO_NULL;
	
								return;
							}

							CGThrowBomb	cgThrowBomb;

							cgThrowBomb.setZoneX( static_cast<Coord_t>(m_TraceX) );
							cgThrowBomb.setZoneY( static_cast<Coord_t>(m_TraceY) );
							cgThrowBomb.setBombX( pUsingItem->GetGridX() );
							cgThrowBomb.setBombY( pUsingItem->GetGridY() );
							cgThrowBomb.setAttackSlayerFlag( m_TraceID!=OBJECTID_NULL );

							g_pSocket->sendPacket( &cgThrowBomb );						
							
							//--------------------------------------------------
							// If there is more than one, only reduce the count.
							//--------------------------------------------------
							if (pUsingItem->GetNumber()>1)
							{
								pUsingItem->SetNumber( pUsingItem->GetNumber() - 1 );
							}
							//--------------------------------------------------
							// If only one is left, remove the Item completely
							//--------------------------------------------------
							else
							{
								MItem* pRemovedItem = g_pInventory->RemoveItem( pUsingItem->GetGridX(), pUsingItem->GetGridY() );

								if (pRemovedItem!=NULL)
								{
									// Remove item info
									PlayerPresentation::RemoveDescriptor( (void*)pRemovedItem );

									delete pRemovedItem;								
								}
								else
								{
									DEBUG_ADD_FORMAT_ERR("[Error] Cannot Remove Item to use Skill. id=%d", pUsingItem->GetID());
								}

								//-------------------------------------------------
								// Skill Icon check
								//-------------------------------------------------
								g_pSkillAvailable->SetAvailableSkills();
							}
						}
						else
						{
							int SkillType = m_nUsedActionInfo;
							if( (*g_pActionInfoTable)[SkillType].GetParentActionInfo() != ACTIONINFO_NULL )
								SkillType = (*g_pActionInfoTable)[SkillType].GetParentActionInfo();

							if(SkillType == SKILL_TELEPORT || SkillType == SKILL_CHARGING_ATTACK)
							{
								g_pPlayer->SetDelay(1000);
							}
							CGSkillToObject _CGSkillToObject;
							_CGSkillToObject.setSkillType( SkillType );
							_CGSkillToObject.setCEffectID( m_pEffectTarget->GetEffectID() );			
							_CGSkillToObject.setTargetObjectID( m_TraceID );
							g_pSocket->sendPacket( &_CGSkillToObject );						
						}

						
					// 2001.8.20 commented out
					SetWaitVerify( WAIT_VERIFY_SKILL_SUCCESS, m_nUsedActionInfo );
				}
				//----------------------------------------------------------
				//
				//			If used on self..
				//
				//----------------------------------------------------------
				else if (packetType==ACTIONINFO_PACKET_SELF
						|| (m_fTraceBuffer & FLAG_TRACE_SELF))
				{
					//----------------------------------------------------------
					// OLD_PACKET
					//----------------------------------------------------------
					///*
					int originalActionInfo = m_nUsedActionInfo;
					if( (*g_pActionInfoTable)[originalActionInfo].GetParentActionInfo() != ACTIONINFO_NULL )
						originalActionInfo = (*g_pActionInfoTable)[originalActionInfo].GetParentActionInfo();
					CGSkillToSelf _CGSkillToSelf;
					_CGSkillToSelf.setSkillType( originalActionInfo );
					_CGSkillToSelf.setCEffectID( m_pEffectTarget->GetEffectID() );
					g_pSocket->sendPacket( &_CGSkillToSelf );

					// 2001.8.20 commented out
					SetWaitVerify( WAIT_VERIFY_SKILL_SUCCESS, m_nUsedActionInfo );
					//*/
				}
				//----------------------------------------------------------
				//
				//				If used on a Tile..
				//
				//----------------------------------------------------------
				else if (packetType==ACTIONINFO_PACKET_ZONE
						|| (m_fTraceBuffer & FLAG_TRACE_SECTOR_BASIC) 
						|| (m_fTraceBuffer & FLAG_TRACE_SECTOR_SPECIAL))
				{
					//----------------------------------------------------------
					// OLD_PACKET
					//----------------------------------------------------------
					///*
					int SkillType = m_nUsedActionInfo;
					if( (*g_pActionInfoTable)[SkillType].GetParentActionInfo() != ACTIONINFO_NULL )
						SkillType = (*g_pActionInfoTable)[SkillType].GetParentActionInfo();

					if(SkillType == SKILL_TELEPORT || SkillType == SKILL_CHARGING_ATTACK)
					{
						g_pPlayer->SetDelay(1000);
					}
					else if(SkillType == SKILL_SWEEP_VICE_1)
					{
						if(1 == GetSweepViewValue())
							SkillType = SKILL_SWEEP_VICE_1;
						else if(2 == GetSweepViewValue())
							SkillType = SKILL_SWEEP_VICE_3;
						else if(3 == GetSweepViewValue())
							SkillType = SKILL_SWEEP_VICE_5;
						m_TraceX = m_SweepViceX;
						m_TraceY = m_SweepViceY;
					}
					else 	if(SkillType == SKILL_BURNING_SOUL_LAUNCH)
					{
						m_TraceX = m_TempSelectPosX;
						m_TraceY = m_TempSelectPosY;
					}
					
					CGSkillToTile _CGSkillToTile;
					_CGSkillToTile.setSkillType( SkillType );
					_CGSkillToTile.setCEffectID( m_pEffectTarget->GetEffectID() );
					_CGSkillToTile.setX( static_cast<Coord_t>(m_TraceX) );
					_CGSkillToTile.setY( static_cast<Coord_t>(m_TraceY) );
					g_pSocket->sendPacket( &_CGSkillToTile );

					// 2001.8.20 commented out
					SetWaitVerify( WAIT_VERIFY_SKILL_SUCCESS, m_nUsedActionInfo );
					//*/
				}
				//----------------------------------------------------------
				//
				//				If used on the Inventory..
				//
				//----------------------------------------------------------
				else if (packetType==ACTIONINFO_PACKET_ITEM
						|| (m_fTraceBuffer & FLAG_TRACE_INVENTORY))
				{
					MItem* pItem = g_pInventory->GetItemToModify( m_TraceID );

					if (pItem!=NULL && IsItemCheckBufferNULL())
					{
						// Just handle the packet as using a skill on an inventory Item.
						CGSkillToInventory _CGSkillToInventory;
						
						_CGSkillToInventory.setSkillType( m_nUsedActionInfo );
						//_CGSkillToInventory.setCEffectID( m_pEffectTarget->GetEffectID() );
						_CGSkillToInventory.setObjectID( m_TraceID );
						_CGSkillToInventory.setX( pItem->GetGridX() );
						_CGSkillToInventory.setY( pItem->GetGridY() );

							
						// (!!!) Items must not be movable until the verification packet is received.
						// Stop Item-related actions
						SetItemCheckBuffer(pItem, ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY);
						
						// Stop skill use
						// 2001.8.20 commented out
						// 2001.10.26 uncommented - Items must be verified.
						SetWaitVerify( WAIT_VERIFY_SKILL_SUCCESS, m_nUsedActionInfo );
					}
					
				}

				
			}		
		}


		//----------------------------------------------------------------------
		// When a Packet result is received..
		//----------------------------------------------------------------------
		// Receive the Instance ID sent from the Client too..
		// If that ID equals the currently stored InstanceID.. that ActionInfo is still
		// in progress. When the Player's ActionInfo ends,
		// shouldn't the Instance ID be set to 0?..
		//
		// If equal.. remember the ActionInfo result that came from the Server
		//          and show it later~~~ (after the ActionInfo ends)
		// If different.. just show it immediately.
		//
		// A forced time gap like this has to create the flow that follows Packet handling.
		//----------------------------------------------------------------------

		// The casting motion was originally shown here.

		//------------------------------------------------
		// Play the sound matching the skill's motion.
		//------------------------------------------------
		//g_Sound.Play( g_SoundTable[(*g_pActionInfoTable)[m_nUsedActionInfo].GetSoundID()].pDSBuffer );
		//PlaySound( (*g_pActionInfoTable)[m_nUsedActionInfo].GetSoundID() ,
		//			false,
		//			m_X, m_Y);	

		//------------------------------------------------
		// Play the Sound matching the character's Action.
		//------------------------------------------------
		int soundID = (*g_pCreatureTable)[m_CreatureType].GetActionSound( 
										GetActionInfoAction(m_nUsedActionInfo) );
		if(!HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET) && !m_bBurningSol)
			WorldEvents::PlaySoundAt( soundID,	false, m_X, m_Y	);

		//-------------------------------------------------
		// Force Feel
		//-------------------------------------------------
		if (PlayerPresentation::ForceFeelReady())
		{
			if ((*g_pActionInfoTable)[m_nUsedActionInfo].IsWeaponTypeGunAny())
			{
				soundID = SOUND_SLAYER_ATTACK_AR;
			}

			if (soundID < g_pSoundTable->GetSize())
			{
				DEBUG_ADD_FORMAT("ForceAction-ActionToSendPacket(%d, %s)", soundID, strrchr((*g_pSoundTable)[soundID].Filename.GetString(), '\\'));
				PlayerPresentation::ForceAction( soundID );
			}
		}
	}
}

//----------------------------------------------------------------------
// Set ServerPosition
//----------------------------------------------------------------------
void		
MPlayer::SetServerPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	//--------------------------------------------------------
	// Remove all..
	//--------------------------------------------------------
	m_listSendDirection.clear();

	m_SendMove = 0;
	
	g_bNetStatusGood = true;

	//--------------------------------------------------------
	// Remember the position verified by the Server.
	//--------------------------------------------------------
	m_ServerX	= sX;
	m_ServerY	= sY;

	//--------------------------------------------------------
	// (m_ServerX, m_ServerY) onward,
	// remove Creatures not in the Player's view from the Zone.
	//--------------------------------------------------------
	if (m_pZone!=NULL)
	{
		m_pZone->KeepObjectInSight(m_ServerX, m_ServerY, m_Sight);
	}

	SendPositionInfoToParty();
}

//----------------------------------------------------------------------
// Action Effect
//----------------------------------------------------------------------
// Perform the Effect when the motion fully ends.
//----------------------------------------------------------------------
void	
MPlayer::ActionEffect()
{
	/*
	// [proper timing] code that sends the packet
	// 
	BOOL	bActionStand	= (m_Action==ACTION_STAND || IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND);
	int		actionCountMax_1 = m_ActionCountMax-1;
	BOOL	bEndAction		= (m_ActionCount==actionCountMax_1);// || bActionStand;
	int		StartFrame		= (*g_pActionInfoTable)[m_nUsedActionInfo].GetStartFrame( m_WeaponSpeed );
	
	// Send the packet at the end of the motion. 2001.8.1
	int		packetSendTiming = min(actionCountMax_1, StartFrame);
	BOOL	bStartAction	= (m_ActionCount==packetSendTiming);	// packetSendTiming was originally 0.
	
	if (bActionStand)
	{
		bEndAction = bStartAction;
	}
	*/

	#ifdef OUTPUT_DEBUG_PLAYER_ACTION
		DEBUG_ADD("ActionEffect");
	#endif

	// 2002.3.12
	// If it is a start-motion skill that must send a packet..
	if (m_nUsedActionInfo < g_pActionInfoTable->GetMinResultActionInfo())
	{
		// Only perform ActionEffect when in range.
		if (max(abs(m_X-m_TraceX), abs(m_Y-m_TraceY)) 
			<= m_TraceDistance)
		{
			#ifdef OUTPUT_DEBUG_PLAYER_ACTION
				DEBUG_ADD("inDist");
			#endif
		}
		else
		{
			#ifdef OUTPUT_DEBUG_PLAYER_ACTION
				DEBUG_ADD("not inDist");
			#endif

			// If not in range?
			//if (m_listDirection.empty())
			{
				if (m_DestX != SECTORPOSITION_NULL
					&& m_DestY != SECTORPOSITION_NULL)
				{
					// When there is a target currently being headed to
				}
				else if (abs((int)m_X-m_TraceX) < g_SECTOR_WIDTH
						&& abs((int)m_Y-m_TraceY) < g_SECTOR_HEIGHT)
				{
					// When tracing
					m_DestX = m_TraceX;
					m_DestY = m_TraceY;
				}
				
				if (SetDestination(m_TraceX, m_TraceY))
				{
					#ifdef OUTPUT_DEBUG_PLAYER_ACTION
						DEBUG_ADD("NewWayOK");
					#endif

					// When there is a path
					m_nNextUsedActionInfo = m_nUsedActionInfo;
					m_nUsedActionInfo = ACTIONINFO_NULL;
					
					SetAction( ACTION_STAND );
				}
				else
				{				
					// When there is no path
					// Yet another piece of code like this. (sigh).. this should be completely overhauled
					SetAction(((m_MoveDevice==MOVE_DEVICE_WALK)? ACTION_STAND : ACTION_SLAYER_MOTOR_STAND));
					SetNextAction(ACTION_STAND);
					m_fTraceBuffer = 0;
					m_fNextTrace = 0;
					m_bTraceCreatureToForceAttack = false;
					SetStop();
					UnSetRepeatAction();
					TraceNULL();
					m_nUsedActionInfo = ACTIONINFO_NULL;
					m_nNextUsedActionInfo = ACTIONINFO_NULL;

					#ifdef OUTPUT_DEBUG_PLAYER_ACTION
						DEBUG_ADD("Stop!");
					#endif

					return;
				}

				return;
			}
		}
	}

	BOOL	bActionStand	= (m_Action==ACTION_STAND || (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND));
	int		actionCountMax_1 = m_ActionCountMax-1;
	BOOL	bEndAction		= (m_ActionCount==actionCountMax_1) || bActionStand;
	int		StartFrame		= (*g_pActionInfoTable)[m_nUsedActionInfo].GetStartFrame( m_WeaponSpeed );
	
	BOOL	bStartAction	= (m_ActionCount==0 && !IsGunShotGuidance());
	
	// An Effect starts when..
	// (1) it is the StartFrame
	// (2) it is the last ActionFrame
	BOOL	bStartEffect = m_ActionCount==StartFrame || 
							(StartFrame >= m_ActionCountMax && bEndAction);

	// Start frame of the repeated action
	bStartAction |= m_bRepeatAction 
					&& (*g_pActionInfoTable)[m_nUsedActionInfo].IsUseRepeatFrame()
					&& m_RepeatCount!=0
					&& m_ActionCount == GetActionInfoRepeatStartFrame( m_nUsedActionInfo );

	//BOOL	bCastingEffect	= GetActionInfoCastingStartFrame(m_nUsedActionInfo)==m_ActionCount;
	

	//----------------------------------------------------------------------
	// [ TEST CODE ]
	//----------------------------------------------------------------------
	// This is quite risky, but..
	// it should not be a big problem...
	//----------------------------------------------------------------------
	// The EffectTarget is created after the casting motion ends,
	// but its ID must be sent to the server when casting starts,
	// so as a stopgap the EffectTarget was created first and used.
	//----------------------------------------------------------------------
	// Hmm. Changed it to a data member.
	//static MEffectTarget* 

	
	#ifdef OUTPUT_DEBUG
			//DEBUG_ADD_FORMAT("action=(%d/%d) ActionInfo=%d, pEffectTarget=%x", m_ActionCount, m_ActionCountMax, m_nUsedActionInfo, m_pEffectTarget);
	#endif

	int SkillInfo = m_nUsedActionInfo;
	if(SkillInfo == SKILL_SWEEP_VICE_1)
	{
		if(!GetSweepViewValue() && g_bRButtonDown) //when Sweep Vice is not yet in use
		{
			if(g_pSkillAvailable->IsEnableSkill( SKILL_SWEEP_VICE_1)
				&&!IsDead()
				&&!(*g_pSkillInfoTable)[SKILL_SWEEP_VICE_1].GetAvailableTimeLeft()
				)
			{
				SetSweepViewValue(1);
				g_SelectSector = WorldEvents::GetSelectedSector(g_x, g_y);

				m_TempSelectPosX = static_cast<TYPE_SECTORPOSITION>(g_SelectSector.x);
				m_TempSelectPosY = static_cast<TYPE_SECTORPOSITION>(g_SelectSector.y);
				m_SweepViceX = m_TraceX;
				m_SweepViceY = m_TraceY;
				WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_SWEEP_VICE,m_SweepViceX, m_SweepViceY, 0,g_pPlayer->GetDirection(),0	,
						m_SweepViceX, m_SweepViceY, 0, 0xffff, NULL, false);
			}
			return;
		}
		else if(g_bRButtonDown)
			return;

	//	bStartEffect = true;

		if(1 == GetSweepViewValue())
			SkillInfo = SKILL_SWEEP_VICE_1;
		else if(2 == GetSweepViewValue())
			SkillInfo = SKILL_SWEEP_VICE_3;
		else if(3 == GetSweepViewValue())
			SkillInfo = SKILL_SWEEP_VICE_5;
		else
		{
			SkillInfo = SKILL_SWEEP_VICE_1;
			SetSweepViewValue(0);
		}
	}
	
	//----------------------------------------------------------
	//
	//					Motion start point..
	//
	//----------------------------------------------------------
	if (//bStartEffect)		// Send the packet when the Effect is attached.
		bStartAction)		// Send the packet when the motion starts.
	{	
		if(HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
		{
			 if(bStartEffect && g_pCurrentMagazine != NULL)
			 {
				 int MagazineCount = g_pCurrentMagazine->GetNumber();
				 if(MagazineCount>0)
				 {
					 g_pCurrentMagazine->SetNumber(MagazineCount-1);
					 ActionToSendPacket();
				 }
			 }
		}
		else
			ActionToSendPacket();
	}

	//-------------------------------------------------------------
	//
	// Casting effect
	//
	//-------------------------------------------------------------
	AttachCastingEffect( SkillInfo );

	//-------------------------------------------------------------
	// If it is a stop motion..
	// Show the effect right away.
	//-------------------------------------------------------------

	// To send at the [proper timing], this must be commented out.
	if (bActionStand)
	{
		m_ActionCount = actionCountMax_1;
	}

	//----------------------------------------------------------
	// Increase action count
	//----------------------------------------------------------
	// Why increase it here?
	// Because actionCount may change below..
	//----------------------------------------------------------
	m_ActionCount++; 


	//--------------------------------------------------------	
	// Temporarily stopping the blood drain motion..
	//--------------------------------------------------------	
	if (m_bStopBloodDrain)
	{
		StopBloodDrain();
	}
	if (m_bStopAbsorbSoul)
	{
		StopAbsorbSoul();
	}

		
	//----------------------------------------------------------
	//
	//					Effect start
	//
	//----------------------------------------------------------
	// this may change after affect (sigh);
	if (bStartEffect)
	{
		AffectUsedActionInfo(SkillInfo);
	}
	




	//----------------------------------------------------------
	//
	//					When the Action motion ends
	//
	//----------------------------------------------------------
	if (bEndAction)
		//&& (*g_pActionInfoTable)[ m_nUsedActionInfo ].GetSize()!=0)		
	{
		//--------------------------------------------------------
		// Clear the trace info.
		//--------------------------------------------------------
		//m_TraceID	= OBJECTID_NULL;
		//m_Action	= ACTION_STAND;

		//--------------------------------------------------------
		// Set the skill-use delay time.
		// Only for the starting skill..
		//--------------------------------------------------------
		// [ TEST CODE ]
		//if (//currentUsedActionInfo!=ACTIONINFO_NULL && 
		//	currentUsedActionInfo < (*g_pActionInfoTable).GetMinResultActionInfo())
		//{
		//	m_DelayTime	= g_CurrentTime + GetActionInfoDelay(currentUsedActionInfo);
		//}

		//m_bAffectUsedActionInfo = true;


		//------------------------------------------------------------
		// Check repeated action..
		//------------------------------------------------------------
			///*
		if (CheckRepeatAction())
		{
			CheckBufferAction();
		}

		// 2002.3.24 moved down here from above checkRepeatAction().
		m_nUsedActionInfo = ACTIONINFO_NULL;		

		//*/
	}	

	#ifdef OUTPUT_DEBUG_PLAYER_ACTION
		DEBUG_ADD("AEEnd");
	#endif
}


//----------------------------------------------------------------------
// Modify sight
//----------------------------------------------------------------------
//void	
//MPlayer::SetLightSight(char LightSight)
//{
//	// When a sight is set, remove it..
////	if (m_TimeLightSightX != SECTORPOSITION_NULL)
////	{
//////		m_pZone->UnSetLight(m_LightSightX, m_LightSightY, m_LightSight);	
////	}
//
//	// Set
//	m_TimeLightSight = LightSight;
//
//	// Set the coordinates for the configured sight..
////	m_TimeLightSightX = m_X;
////	m_TimeLightSightY = m_Y;
//
//	// Specify a new sight range.
////	m_pZone->SetLight(m_X, m_Y, m_LightSight);			
//}

//----------------------------------------------------------------------
// Remove sight
//----------------------------------------------------------------------
//void	
//MPlayer::UnSetLightSight()
//{
//	// When a sight is set, remove it..
////	if (m_LightSightX != SECTORPOSITION_NULL)
////	{
////		m_pZone->UnSetLight(m_LightSightX, m_LightSightY, m_LightSight);	
////	}
//	
//	//m_LightSight = 0;
//
////	m_LightSightX = SECTORPOSITION_NULL;
////	m_LightSightY = SECTORPOSITION_NULL;
//}
//
//----------------------------------------------------------------------
// Set ItemCheckBuffer
//----------------------------------------------------------------------
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
void MPlayer::SetItemCheckBuffer(MItem* pItem, ITEM_CHECK_BUFFER status, TYPE_OBJECTID SubItem)
	#else
void MPlayer::SetItemCheckBuffer(MItem* pItem, ITEM_CHECK_BUFFER status)
	#endif

{
	m_pItemCheckBuffer = pItem;

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
		m_dwSubItemIDCheckBuffer = SubItem;
	#endif

	m_ItemCheckBufferStatus = status;	

	#ifdef OUTPUT_DEBUG
	DEBUG_ADD_FORMAT("[SetItemCheckBuffer] status %d", status);
//		switch (status)
//		{
//			case ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY :
//				DEBUG_ADD("ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY");
//			break;
//
//			case ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE :
//				DEBUG_ADD("ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE");
//			break;
//
//			case ITEM_CHECK_BUFFER_DROP_TO_ZONE :
//				DEBUG_ADD("ITEM_CHECK_BUFFER_DROP_TO_ZONE");
//			break;
//		}
	#endif

	PlayerPresentation::LockItem();
	VsUiPresentation::LockGear();
	PlayerPresentation::LockItemTrade();
}

//----------------------------------------------------------------------
// Clear ItemCheckBuffer
//----------------------------------------------------------------------
void
MPlayer::ClearItemCheckBuffer()
{
	DEBUG_ADD("Set Item Check Buffer to NULL");
	
	m_pItemCheckBuffer = NULL;
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag
		m_dwSubItemIDCheckBuffer = OBJECTID_NULL;
	#endif
	m_ItemCheckBufferStatus = ITEM_CHECK_BUFFER_NULL;

	// 2004, 8, 27, sobeit add start - while in coffin-summon state, taking off and putting on equipped items after using an inventory item
	// releases the coffin state on the client side. So UI_UnlockGear() is not called while a coffin is summoned.
	if(!IsInCasket())
		DialogPresentation::UnlockGear();
	// 2004, 8, 27, sobeit add end - 
	DialogPresentation::UnlockItem();
	DialogPresentation::UnlockItemTrade();
}

//----------------------------------------------------------------------
// Decide action
//----------------------------------------------------------------------
void	
MPlayer::SetAction(BYTE action)
{
//	if ( m_nSpecialActionInfo == MAGIC_BLOODY_ZENITH && m_Action == ACTION_ATTACK &&
//		m_ActionCount >= m_ActionCountMax && GetActionDelay() > 0)
//	{
//		SetActionDelay( GetActionDelay() - 1 );
//		m_ActionCount = m_ActionCountMax - 2;
//		return;
//	}	
	// 2004, 10, 6, sobeit add start
	if(HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET) && (action == ACTION_DAMAGED || action == ACTION_DRAINED))
		return;
	// 2004, 10, 6, sobeit add end
	if(HasEffectStatus(EFFECTSTATUS_SLEEP)
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		)
	{
//		return;
		action = ACTION_STAND;
	}

	// Always turn it off.
	m_bFastMove = false;
	//m_bFastMove = false;
	
	//-----------------------------------------------------------------
	// When it matches the action of the current form. 2001.10.5
	//-----------------------------------------------------------------
	if (action < GetActionMax())
	{
		//-----------------------------------------------------
		// While riding a motorcycle
		//-----------------------------------------------------
		if (m_MoveDevice==MOVE_DEVICE_RIDE)
		{
			if (IsSlayer())
			{
				if (action==ACTION_STAND)
				{
					action = ACTION_SLAYER_MOTOR_STAND;
				}
				else if (action==ACTION_MOVE)
				{
					action = ACTION_SLAYER_MOTOR_MOVE;
				}
			}
		}
		//-----------------------------------------------------
		// When walking
		//-----------------------------------------------------
		else
		{
			if (IsSlayer())
			{
				if (action==ACTION_SLAYER_MOTOR_STAND)
				{
					action = ACTION_STAND;
				}
				else if (action==ACTION_SLAYER_MOTOR_MOVE)
				{
					action = ACTION_MOVE;
				}
			}
		}

		if( IsOusters() && (HasEffectStatusSummonSylph( dynamic_cast<MCreature*>(this) ) || m_MoveDevice==MOVE_DEVICE_SUMMON_SYLPH))
		{
			if (action==ACTION_STAND)
			{
				action = ACTION_OUSTERS_FAST_MOVE_STAND;
			} 
			else if( action == ACTION_MOVE )
			{
				action = ACTION_OUSTERS_FAST_MOVE_MOVE;
			}
		}

		#ifdef OUTPUT_DEBUG
			if (IsSlayer())
			{
				// Slayer case
				if (action >= GetActionMax())
				{
					DEBUG_ADD_FORMAT_ERR("[Error] Slayer's Action exceed MAX : id=%d, action=%d", m_ID, action);
					
					return;
				}
			}
			else if (IsVampire())
			{
				if (action >= GetActionMax())
				{
					DEBUG_ADD_FORMAT_ERR("[Error] Vampire's Action exceed MAX : id=%d, action=%d", m_ID, action);
					
					return;
				}
			}
			else if (IsOusters())
			{
				if (action >= GetActionMax())
				{
					DEBUG_ADD_FORMAT_ERR("[Error] Ousters's Action exceed MAX : id=%d, action=%d", m_ID, action);
					
					return;
				}
			}
			else if (IsNPC())
			{
				if (action >= GetActionMax())
				{
					DEBUG_ADD_FORMAT_ERR("[Error] NPC's Action exceed MAX : id=%d, action=%d", m_ID, action);
					
					return;
				}
			}
			else
			{
				// - -;;
				DEBUG_ADD_ERR("[Error] What is this Creature????");
				
				return;
			}
		#endif

		m_bNextAction = false;

		//------------------------------------------------
		// Move case
		//------------------------------------------------
		if (action==m_MoveAction)
		{
			// If just standing... turn it into the ending motion.
			// To mark that the Action has finished..
			if (m_Action==ACTION_STAND 
				|| (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND)
				|| (IsOusters() && m_Action==ACTION_OUSTERS_FAST_MOVE_STAND))
			{			
				m_ActionCount = 0;
				m_ActionCountMax = 0;			
			}

			//------------------------------------------------
			// Only move when stopped.
			//------------------------------------------------
			if (IsStop())
			{	
				m_Action = action;
				//SetAction( action );

				SetNextAction(ACTION_STAND);

				// First Move motion
				m_MoveCount = 0;
				m_NextMoveCount = 0;
				
				// Stopped  :  m_sX = 0; m_sY = 0;
				// Depending on the kind of movement..
				//m_MoveCountMax = (*g_pCreatureTable)[m_CreatureType].GetActionCount( action );
				m_MoveCountMax = GetCreatureActionCountMax( this, action );
					//g_pTopView->m_CreatureFPK[(*g_pCreatureTable)[m_CreatureType].FrameID]
					//									[action][m_Direction].GetSize();		
			
				//---------------------------------------------
				// Prevent moving gear items in the UI.
				//---------------------------------------------
				//gC_vs_ui.LockItem();
				VsUiPresentation::LockGear();
			}
			//------------------------------------------------
			// If already moving, buffer it.
			//------------------------------------------------
			else
			{
				//---------------------------------------------
				// Allow moving gear items in the UI.
				//---------------------------------------------
				//gC_vs_ui.UnlockItem();
				DialogPresentation::UnlockGear();

				// buffering
				SetNextAction(action);
			}
		}
		//------------------------------------------------
		// Action case
		//------------------------------------------------
		else
		{
			//------------------------------------------------
			// extreme is released on a particular motion.
			//------------------------------------------------
//			if (HasEffectStatus(EFFECTSTATUS_EXTREME))
//			{
//				if (action==ACTION_MAGIC
//					|| action==ACTION_DIE
//					|| action==ACTION_VAMPIRE_DRAIN
//					|| action==ACTION_DRAINED)
//				{
//					RemoveEffectStatus(EFFECTSTATUS_EXTREME);
//				}
//			}

			if (// do not show the action while riding a motorcycle.
				(m_MoveDevice==MOVE_DEVICE_RIDE && action!=ACTION_SLAYER_MOTOR_STAND)
				// damaged case..
				|| (action==ACTION_DAMAGED 						
						&& (// if not being attacked or not a stop motion, do not show the being-hit look
							m_Action!=ACTION_DAMAGED && m_Action!=ACTION_STAND && m_Action!=ACTION_MOVE

							// during the blood-sucked motion, or
	//						m_Action==ACTION_DRAINED
							// when a vampire, do not show damaged during the blood-sucking motion.
	//						|| IsVampire() && m_Action==ACTION_VAMPIRE_DRAIN
							))
				)
			{
				SetNextAction(ACTION_STAND);
				return;
			}

			//---------------------------------------------
			// Allow moving gear items in the UI.
			//---------------------------------------------
			//gC_vs_ui.UnlockItem();
			if(!IsInCasket())
				DialogPresentation::UnlockGear();

			// Set motion
			SetNextAction(ACTION_STAND);

			//m_ActionCountMax = (*g_pCreatureTable)[m_CreatureType].GetActionCount( action );
			m_ActionCountMax = GetCreatureActionCountMax( this, action );

			if (m_ActionCountMax==0)
			{
				// When there is no motion
				m_Action = ((m_MoveDevice==MOVE_DEVICE_WALK)? ACTION_STAND : ACTION_SLAYER_MOTOR_STAND);
				//SetAction( ((m_MoveDevice==MOVE_DEVICE_WALK)? ACTION_STAND : ACTION_SLAYER_MOTOR_STAND) );
				m_ActionCount = 0;		
				m_RepeatCount = 0;
				//m_ActionCountMax = (*g_pCreatureTable)[m_CreatureType].GetActionCount( m_Action );
				m_ActionCountMax = GetCreatureActionCountMax( this, m_Action );
			}
			else
			{
				//---------------------------------------------
				// When the action repeats
				//---------------------------------------------
				if (m_nUsedActionInfo!=ACTIONINFO_NULL 
					&& m_bRepeatAction 
					&& (*g_pActionInfoTable)[m_nUsedActionInfo].IsUseRepeatFrame())// m_WeaponSpeed ))
				{
					m_Action = action; 
					//SetAction( action );

					// If a repeat count limit applies..
					if (m_RepeatCount >= (*g_pActionInfoTable)[m_nUsedActionInfo].GetRepeatLimit())
					{		
						m_RepeatCount = 0;

						// Always from the first motion..
						m_ActionCount = 0;
					}
					else
					{
						//---------------------------------------------
						// When repeating continuously
						//---------------------------------------------
						BOOL bSlayer = IsSlayer();

						if (m_RepeatCount!=0 
							
							&& action!=ACTION_STAND 
							&& (!bSlayer || (bSlayer && action!=ACTION_SLAYER_MOTOR_STAND)))
						{
							// Set count..
							//m_ActionCount = (*g_pActionInfoTable)[m_nUsedActionInfo].GetRepeatStartFrame( m_WeaponSpeed );
							m_ActionCount = GetActionInfoRepeatStartFrame( m_nUsedActionInfo );
						}
						//---------------------------------------------
						// When starting the first repeat
						//---------------------------------------------
						else
						{
							m_ActionCount = 0;				
						}
					}

					//m_ActionCountMax = (*g_pActionInfoTable)[m_nUsedActionInfo].GetRepeatEndFrame( m_WeaponSpeed );			
					m_ActionCountMax = GetActionInfoRepeatEndFrame( m_nUsedActionInfo );
				}
				//---------------------------------------------
				// Normal case..
				//---------------------------------------------
				else
				{			
					if (m_ActionCount!=m_ActionCountMax
						&& ((action==ACTION_STAND && m_Action==ACTION_STAND)
							|| (IsSlayer() && action==ACTION_SLAYER_MOTOR_STAND && m_Action==ACTION_SLAYER_MOTOR_STAND)
							|| (IsOusters() && action==ACTION_OUSTERS_FAST_MOVE_STAND && m_Action==ACTION_OUSTERS_FAST_MOVE_STAND))
							)
					{
						// Do not set if it is the same stop motion.
						//m_ActionCount = 0;
						//m_ActionCountMax = (*g_pCreatureTable)[m_CreatureType].GetActionCount( m_Action );
					}
					else
					{
						m_Action = action;
						//SetAction( action );

						// Set count..
						m_ActionCount = 0;
						
							//g_pTopView->m_CreatureFPK[(*g_pCreatureTable)[m_CreatureType].FrameID]
							//												[m_Action][m_Direction].GetSize();
					}			
				}			
			}
		}		
	}


	// Additional Effects attached when a motion starts.
	// Should be moved somewhere else. (sigh)
	if( HasEffectStatus( EFFECTSTATUS_REDIANCE ) )
	{
		if( 
			( m_Action == ACTION_SLAYER_SWORD_2 || m_Action == ACTION_SLAYER_SWORD_2_SLOW || m_Action == ACTION_SLAYER_SWORD_2_FAST ) 
			)
		{
			MAttachEffect *pEffect = CreateAttachEffect( EFFECTSPRITETYPE_REDIANCE_MALE_NORMAL, 0, 0 );
			if( pEffect != NULL )
			{
				pEffect->SetDirection( GetDirection() );
				pEffect->SetLink( ACTIONINFO_NULL, NULL );							
				pEffect->SetAttachCreatureID( GetID() );
			}
		} 
		else if(
			( m_Action == ACTION_SLAYER_SWORD || m_Action == ACTION_SLAYER_SWORD_FAST || m_Action == ACTION_SLAYER_SWORD_SLOW ) )
		{
			MAttachEffect *pEffect = CreateAttachEffect( EFFECTSPRITETYPE_REDIANCE_MALE_NORMAL_ATTACK_NORMAL, 0, 0 );
			if( pEffect != NULL )
			{
				pEffect->SetDirection( GetDirection() );
				pEffect->SetLink( ACTIONINFO_NULL, NULL );							
				pEffect->SetAttachCreatureID( GetID() );
			}
		}				
	}
}

//----------------------------------------------------------------------
// Set AddonItem
//----------------------------------------------------------------------
// Equip an AddonItem.
//----------------------------------------------------------------------
bool
MPlayer::SetAddonItem( MItem* pItem )
{
	DEBUG_ADD("MPlayer::SetAddonItem");
	
	//-------------------------------------------------
	// If equipped properly
	//-------------------------------------------------
	if (MCreatureWear::SetAddonItem( pItem ))
	{
		//-------------------------------------------------
		// If the equipped Item
		// is a basic-attack item...
		// the basic-attack ActionInfo must change.
		//-------------------------------------------------
		if (pItem->IsBasicWeapon())
		{
			//-------------------------------------------------
			// Set attack range
			// Only guns differ,
			// everything else is melee, so 1.
			//-------------------------------------------------
			//if (pItem->IsGunItem())
			//{
				SetBasicAttackDistance( pItem->GetReach() );
			//}
			//else
			//{								
			//	SetBasicAttackDistance( 1 );
			//}

			//-------------------------------------------------
			// Skill Icon check
			//-------------------------------------------------
			//(*g_pSkillAvailable).SetAvailableSkills();
		}

		return true;
	}

	return false;	
}

//----------------------------------------------------------------------
// Remove AddonItem
//----------------------------------------------------------------------
// Unequip an AddonItem.
//----------------------------------------------------------------------
bool
MPlayer::RemoveAddonItem( MItem* pItem )
{
	DEBUG_ADD("MCreatureWear::RemoveAddonItem");
	
	//-------------------------------------------------
	// If unequipped properly
	//-------------------------------------------------
	if (MCreatureWear::RemoveAddonItem( pItem ))
	{
		//-------------------------------------------------
		// If the removed Item
		// is a basic-attack item...
		// the basic-attack ActionInfo must change to bare-handed attack.
		//-------------------------------------------------
		if (pItem->IsBasicWeapon())
		{
			// Switch to the basic bare-handed attack motion.
			SetBasicActionInfo( SKILL_ATTACK_MELEE );


			// Set attack range
			SetBasicAttackDistance( 1 );

			//-------------------------------------------------
			// Skill Icon check
			//-------------------------------------------------
			//(*g_pSkillAvailable).SetAvailableSkills();
		}

		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Set Next Action
//----------------------------------------------------------------------
// Set the next action.. if currently just standing (STAND), set the action immediately
// otherwise, buffer it
//----------------------------------------------------------------------
void	
MPlayer::SetNextAction(BYTE action)
{ 	
	if(HasEffectStatus(EFFECTSTATUS_SLEEP)
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		)
	{
//		return;
		action = ACTION_STAND;
	}

	if(action == ACTION_STAND)
	{
		action = ((m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND);
		if(IsOusters() && HasEffectStatusSummonSylph( dynamic_cast<MCreature*>(this) ))
			action = ACTION_OUSTERS_FAST_MOVE_STAND;
	}
	else if(action == ACTION_MOVE)
		if(IsOusters() && HasEffectStatusSummonSylph( dynamic_cast<MCreature*>(this) ))
			action = ACTION_OUSTERS_FAST_MOVE_MOVE;

	if (m_Action==ACTION_STAND || (IsOusters() && m_Action==ACTION_OUSTERS_FAST_MOVE_STAND)
		|| (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND)) 
	{
		//SetAction( action );
		m_ActionCount = m_ActionCountMax;
	}
	//else
	//{
		m_NextAction = action;
	//}
}

//----------------------------------------------------------------------
// Set NextAction To Move
//----------------------------------------------------------------------
// Move on the next action.
//----------------------------------------------------------------------
void	
MPlayer::SetNextActionToMove()
{ 	
	if (m_Action==ACTION_STAND || (IsOusters() && m_Action == ACTION_OUSTERS_FAST_MOVE_STAND)
		|| (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND)) 
	{
		//SetAction( action );
		m_ActionCount = m_ActionCountMax;
	}
	//else
	//{
		SetNextAction(m_MoveAction);
	//}
}

//----------------------------------------------------------------------
// Set Alive
//----------------------------------------------------------------------
// Alive again~
//----------------------------------------------------------------------
void	
MPlayer::SetAlive()
{
	m_bAlive = true;

	// Release item movement
	DialogPresentation::UnlockItem();
	DialogPresentation::UnlockGear();
	DialogPresentation::UnlockItemTrade();

	SetAction( ACTION_STAND );

	//-------------------------------------------------------
	// Remove Effect states
	//-------------------------------------------------------
	/*
	if (g_pEffectStatusTable!=NULL)
	{
		for (int e=0; e<g_pEffectStatusTable->GetSize(); e++)
		{
			RemoveEffectStatus( (EFFECTSTATUS)e );
		}
	}
	*/

	if (WorldEvents::ViewExists())
	{
		WorldEvents::SetFadeEnd();
	}

	if (m_MoveType == CREATURE_FLYING)
	{
		m_Z=(*g_pCreatureTable)[m_CreatureType].FlyingHeight;
	}
}

//----------------------------------------------------------------------
// Set Dead
//----------------------------------------------------------------------
// Dead~
// Go to ServerPosition and die..
//----------------------------------------------------------------------
void	
MPlayer::SetDead()
{
	// Only kill if alive. (sigh)
	if (m_bAlive)
	{
		if(HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
		{
			RemoveEffectStatus(EFFECTSTATUS_INSTALL_TURRET);
			SetAction( ACTION_STAND );
		}
		if(HasEffectStatusSummonSylph( dynamic_cast<MCreature*>(this) ) )
		{
			RemoveEffectStatusSummonSylph( dynamic_cast<MCreature*>(this) );
			SetAction( ACTION_STAND );
		}
		if(HasEffectStatus(EFFECTSTATUS_GLACIER))
		{ 
			RemoveEffectStatus( EFFECTSTATUS_GLACIER );
			SetAction(ACTION_STAND);
			ClearAttachEffect();
		}
		PlayerPresentation::FinishLevelUpButton();
		
		StopDrain();
		StopAbsorb();

		// Prevent item movement..
		PlayerPresentation::LockItem();
		VsUiPresentation::LockGear();
		PlayerPresentation::LockItemTrade();

		SetStop();	
//		StopCauseCriticalWounds();

		MovePosition( m_ServerX, m_ServerY );

		DEBUG_ADD_FORMAT("PlayerDeadPosition(%d, %d)", m_ServerX, m_ServerY);

		// Stop
		TraceNULL();
		
		// ActionInfo on death
		m_nUsedActionInfo	= (*g_pCreatureTable)[m_CreatureType].DeadActionInfo;
		
		if (m_nUsedActionInfo!=ACTIONINFO_NULL)
		{
			// Motion on death
			SetAction( GetActionInfoAction(m_nUsedActionInfo) );

			// Motion directed at myself
			m_NextTraceID			= m_ID;
			m_NextTraceX			= m_X;
			m_NextTraceY			= m_Y;
			m_NextTraceZ			= 0;	

			SetTraceID( m_ID );
			m_TraceX			= m_X;
			m_TraceY			= m_Y;
			m_TraceZ			= 0;	
		}

		// the delay
		m_DeadDelayTime	= g_FrameNow + MonotonicClock::Duration(g_pClientConfig->DELAY_PLAYER_DEAD);


		//-------------------------------------------------------
		// Make visible
		//-------------------------------------------------------
		SetVisibleSoon();

		// To remove the sudden standing up on death.. making sure.. (sigh) 2001.7.23
		m_bNextAction = false;
		m_bKeepTraceCreature = false;
		SetNextAction(ACTION_STAND);
		UnSetRepeatAction();

		// HP
		StopRecoveryHP();
		StopRecoveryMP();
		SetStatus( MODIFY_CURRENT_HP, 0 );

		// Turn off LockMode
		UnSetLockMode();

		// Slow down the music...
		//g_SDLMusic.SetCurrentTempo( g_SDLMusic.GetOriginalTempo() * 0.8f );

		m_Z = 0;
		// Confirm..
		PlayerPresentation::CloseSelectWayPoint();

		// cancel the logout
		g_pUserInformation->CancelLogout();

		SetWaitVerifyNULL();

		g_pEventManager->RemoveEvent(EVENTID_LOGOUT);
		g_pEventManager->RemoveEvent(EVENTID_LOVECHAIN);

		// Dead.
		m_bAlive = false;
		m_bInCasket = false;
	
		//-------------------------------------------------------
		// Remove Effect states
		//-------------------------------------------------------
		if (g_pEffectStatusTable!=NULL)
		{
			for (int e=0; e<g_pEffectStatusTable->GetSize(); e++)
			{
				// Do not remove blood-sucking.
				if (e!=EFFECTSTATUS_BLOOD_DRAIN && 
					// 2004, 11, 16, sobeit add start - the siege attackers' states are needed, so not deleted..
					e!=EFFECTSTATUS_SIEGE_ATTACKER_1 &&	
					e!=EFFECTSTATUS_SIEGE_ATTACKER_2 &&
					e!=EFFECTSTATUS_SIEGE_ATTACKER_3 &&
					e!=EFFECTSTATUS_SIEGE_ATTACKER_4 &&
					e!=EFFECTSTATUS_SIEGE_ATTACKER_5 &&
					// 2004, 11, 16, sobeit add end
					HasEffectStatus((EFFECTSTATUS)e))
				{
					RemoveEffectStatus( (EFFECTSTATUS)e );
				}
			}
		}
		ClearAttachEffect();

//		g_pTempInformation->SetMode( TempInformation::MODE_NULL );

		//---------------------------------------------------------------
		// Send my info to the party.
		//---------------------------------------------------------------
		SendStatusInfoToParty();	

		// 2004, 5, 7 sobeit add start - help message on death
		GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_DIE);
		// 2004, 5, 7 sobeit add end
	}
}

//----------------------------------------------------------------------
// Check Repeat Action
//----------------------------------------------------------------------
bool
MPlayer::CheckRepeatAction()
{
	//----------------------------------------------------------------------
	// When repeat action is not possible
	//----------------------------------------------------------------------
	if (m_bKeepTraceCreature || IsRequestMode() || IsInDarkness())// || m_bTraceCreatureToForceAttack)
	{
		// [SKILLREPEAT DIAG] kept
		if (m_bRepeatAction)
			DEBUG_ADD_FORMAT("[SKILLREPEAT] why=KEEPTRACE keep=%d req=%d dark=%d",
				(int)m_bKeepTraceCreature, (int)IsRequestMode(), (int)IsInDarkness());

		UnSetRepeatAction();
		return false;
	}

	//----------------------------------------------------------------------
	// Apply repeat action if it is the last motion of the action or a stop motion.
	//----------------------------------------------------------------------
	if (m_ActionCount>=m_ActionCountMax-1 
			|| m_Action==ACTION_STAND 
			|| (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND))
	{
		static DWORD repeatFrameIncTurn = 0;

		//------------------------------------------------------------
		// When a repeat action is set
		//------------------------------------------------------------
		if (m_bRepeatAction)				
		{
			//------------------------------------------------------------
			// Special skill on self
			//------------------------------------------------------------
			if (m_fTraceBuffer & FLAG_TRACE_SELF)
			{
				//SelfSpecialAction();
				m_fNextTrace = m_fTraceBuffer;

				if (repeatFrameIncTurn!=g_CurrentFrame)
				{
					repeatFrameIncTurn = g_CurrentFrame;
					m_RepeatCount ++;
				}

				//CHECK_REPEAT_LIMIT_TO_RETURN_FALSE( m_nUsedActionInfo, m_RepeatCount )

				return true;
			}				
			//------------------------------------------------------------
			// Basic action
			//------------------------------------------------------------
			else if (m_fTraceBuffer & FLAG_TRACE_CREATURE_BASIC)
			{	
				//TraceCreatureToBasicAction( m_TraceID, m_bTraceCreatureToForceAttack );
				m_fNextTrace = m_fTraceBuffer;

				m_NextTraceID  = m_TraceIDBuffer;				
				
				m_bNextForceAttack = m_bTraceCreatureToForceAttack;

				if (repeatFrameIncTurn!=g_CurrentFrame)
				{
					repeatFrameIncTurn = g_CurrentFrame;
					m_RepeatCount ++;
				}

				//CHECK_REPEAT_LIMIT_TO_RETURN_FALSE( m_nUsedActionInfo, m_RepeatCount )

				return true;
			}				
			//------------------------------------------------------------
			// Special action
			//------------------------------------------------------------
			else  if (m_fTraceBuffer & FLAG_TRACE_CREATURE_SPECIAL)
			{
				//TraceCreatureToSpecialAction( m_TraceID );
				m_fNextTrace = m_fTraceBuffer;

				m_NextTraceID = m_TraceIDBuffer;

				m_bNextForceAttack = m_bTraceCreatureToForceAttack;

				if (repeatFrameIncTurn!=g_CurrentFrame)
				{
					repeatFrameIncTurn = g_CurrentFrame;
					m_RepeatCount ++;
				}
				
				//CHECK_REPEAT_LIMIT_TO_RETURN_FALSE( m_nUsedActionInfo, m_RepeatCount )				
				if (m_nUsedActionInfo < g_pActionInfoTable->GetSize())
				{
					// If the repeat count limit is hit..
					if (m_RepeatCount >= (*g_pActionInfoTable)[m_nUsedActionInfo].GetRepeatLimit())
					{		
						// Send to the end of the motion.
						//m_RepeatCount = 0;

						BOOL bSlayer = IsSlayer();

						if ((*g_pActionInfoTable)[m_nUsedActionInfo].IsUseRepeatFrame() //m_WeaponSpeed )
							&& m_Action!=ACTION_STAND 
							&& (!bSlayer || (bSlayer && m_Action!=ACTION_SLAYER_MOTOR_STAND))
							&& m_ActionCountMax!=0 )	// Will this work.. (sigh)
						{	
							//m_ActionCountMax = (*g_pCreatureTable)[m_CreatureType].GetActionCount( m_Action );
							m_ActionCountMax = GetCreatureActionCountMax( this, m_Action );

							DEBUG_ADD_FORMAT("unsetrepeat: actionCount=%d / %d", m_ActionCount, m_ActionCountMax);		
						}						
					}												
				}
				
				return true;
			}
			//------------------------------------------------------------
			// Special action -> Sector
			//------------------------------------------------------------
			else if (m_fTraceBuffer & FLAG_TRACE_SECTOR_SPECIAL)
			{
				//TraceSectorToSpecialAction( m_TraceX, m_TraceY );
				m_fNextTrace = m_fTraceBuffer;

				//m_NextTraceX = m_TraceX;
				//m_NextTraceY = m_TraceY;

				//------------------------------------------------------------
				// Read the current mouse coordinates and convert them to map coordinates.
				//------------------------------------------------------------
				POINT temp = WorldEvents::ScreenToPixel(g_x, g_y);
				temp = PixelToMap( temp.x, temp.y );
				
				m_NextTraceX = static_cast<TYPE_SECTORPOSITION>(temp.x);
				m_NextTraceY = static_cast<TYPE_SECTORPOSITION>(temp.y);

				if (repeatFrameIncTurn!=g_CurrentFrame)
				{
					repeatFrameIncTurn = g_CurrentFrame;
					m_RepeatCount ++;
				}
				
				//CHECK_REPEAT_LIMIT_TO_RETURN_FALSE( m_nUsedActionInfo, m_RepeatCount )

				return true;
			}		
		}
		//------------------------------------------------------------
		// When no repeat action is set
		//------------------------------------------------------------
		else
		{
			//------------------------------------------------------------
			// If there is a buffered next action..
			//------------------------------------------------------------
			if (m_fNextTrace!=FLAG_TRACE_NULL)
			{
				//m_fTraceBuffer = m_fNextTrace;
				return true;
			}			
		}
	}
	else
	{
	}

	return false;
}

//----------------------------------------------------------------------
// Check BufferAction
//----------------------------------------------------------------------
// Check whether there is a buffered action to process next.
// Return true if there is
// Return false if there is not
//----------------------------------------------------------------------
bool
MPlayer::CheckBufferAction()
{
	//DEBUG_ADD("Before CheckBufferAction");
	
	//------------------------------------------		
	//         When tracing is required..
	//------------------------------------------
	if (m_fNextTrace != FLAG_TRACE_NULL)
	{			
		//SetAction( (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND );

		//---------------------
		// Special skill on self
		//---------------------
		if (m_fNextTrace & FLAG_TRACE_SELF)
		{
			SelfSpecialAction();
		}				
		//---------------------
		// Basic action
		//---------------------
		if (m_fNextTrace & FLAG_TRACE_CREATURE_BASIC)
		{
			TraceCreatureToBasicAction( m_NextTraceID, m_bNextForceAttack );
		}				
		//---------------------
		// Special action
		//---------------------
		else  if (m_fNextTrace & FLAG_TRACE_CREATURE_SPECIAL)
		{
			TraceCreatureToSpecialAction( m_NextTraceID, m_bNextForceAttack );
		}
		//---------------------
		// Basic action -> Sector
		//---------------------
		else if (m_fNextTrace & FLAG_TRACE_SECTOR_BASIC)
		{
			TraceSectorToBasicAction( m_NextTraceX, m_NextTraceY );
		}
		//---------------------
		// Special action -> Sector
		//---------------------
		else if (m_fNextTrace & FLAG_TRACE_SECTOR_SPECIAL)
		{
			TraceSectorToSpecialAction( m_NextTraceX, m_NextTraceY );
		}
		//---------------------
		// Item trace
		//---------------------
		else if (m_fNextTrace & FLAG_TRACE_ITEM)
		{
			TraceItem( m_NextTraceID );
		}
		//---------------------
		// Use skill on an Item in the Inventory
		//---------------------
		else if (m_fNextTrace & FLAG_TRACE_INVENTORY)
		{
			TraceInventoryItem( m_NextTraceID );
		}
		//---------------------
		// Effect trace
		//---------------------
		else if (m_fNextTrace & FLAG_TRACE_EFFECT)
		{
			TraceEffect( m_NextTraceID );
		}
		//---------------------
		// InteractionObject trace
		//---------------------
		else if (m_fNextTrace & FLAG_TRACE_INTERACTIONOBJECT)
		{
			TraceInteractionObject( m_NextTraceID, m_NextTraceObjectAction );
		}

		//DEBUG_ADD("After CheckBufferAction : true");
		
		return true;
	}

	//------------------------------------------		
	//         Buffered next action
	//------------------------------------------
	BOOL bSlayer = IsSlayer();

	if (m_bNextAction || 
		(m_NextAction!=ACTION_STAND 
		&& (!IsOusters() || (IsOusters() && m_NextAction!=ACTION_OUSTERS_FAST_MOVE_STAND))// && m_fNextTrace==FLAG_TRACE_NULL)
		&& (!bSlayer || (bSlayer && m_NextAction!=ACTION_SLAYER_MOTOR_STAND))))// && m_fNextTrace==FLAG_TRACE_NULL)
	{
		SetAction( m_NextAction );	

		DEBUG_ADD("After CheckBufferAction : true - SetAction");
		
		return true;
	}	

	//DEBUG_ADD("After CheckBufferAction - false");
	
	return false;
}

//----------------------------------------------------------------------
// UnSet Repeat Action
//----------------------------------------------------------------------
void			
MPlayer::UnSetRepeatAction()
{
	// [SKILLREPEAT DIAG] kept - only the transition matters, so stay
	// quiet when the repeat was already off (this is called unconditionally
	// from dozens of sites).
	if (m_bRepeatAction)
	{
		DEBUG_ADD_FORMAT("[SKILLREPEAT] unset used=%d special=%d act=%d cnt=%d/%d rb=%d lb=%d lock=%d",
			(int)m_nUsedActionInfo, (int)m_nSpecialActionInfo,
			(int)m_Action, (int)m_ActionCount, (int)m_ActionCountMax,
			(int)g_bRButtonDown, (int)g_bLButtonDown, (int)m_bLockMode);
	}

	// If repeat stops, should the next buffered action be removed too?

	m_bRepeatAction = FALSE;

	m_RepeatCount = 0;

	m_nNextUsedActionInfo = ACTIONINFO_NULL;

	BOOL bSlayer = IsSlayer();

	//m_fTraceBuffer = FLAG_TRACE_NULL;

	if (m_nUsedActionInfo!=ACTIONINFO_NULL 
		&& (*g_pActionInfoTable)[m_nUsedActionInfo].IsUseRepeatFrame() //m_WeaponSpeed )
		&& m_Action!=ACTION_STAND 
		&& (!bSlayer || (bSlayer && m_Action!=ACTION_SLAYER_MOTOR_STAND))
		&& m_ActionCountMax!=0 )	// Will this work.. (sigh)
	{	
		//m_ActionCountMax = (*g_pCreatureTable)[m_CreatureType].GetActionCount( m_Action );		
		m_ActionCountMax = GetCreatureActionCountMax( this, m_Action );

		DEBUG_ADD_FORMAT("unsetrepeat: actionCount=%d / %d", m_ActionCount, m_ActionCountMax);		
	}

	//m_fNextTrace = FLAG_TRACE_NULL; 
}

//----------------------------------------------------------------------
// Add EffectStatus
//----------------------------------------------------------------------
bool		
MPlayer::AddEffectStatus(EFFECTSTATUS status, DWORD delayFrame)
{	
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
			SetMoveDevice(MOVE_DEVICE_SUMMON_SYLPH);
			break;

			//--------------------------------------------------
			// Bless
			//--------------------------------------------------
			case EFFECTSTATUS_BLESS :
				// [Help] striking
//				GameInfoPresentation::ExecuteHelpEvent( HE_EFFECT_BLESS );
			break;

			//--------------------------------------------------
			// Striking
			//--------------------------------------------------
			case EFFECTSTATUS_STRIKING :
				// [Help] striking
//				GameInfoPresentation::ExecuteHelpEvent( HE_EFFECT_STRIKING );
			break;

			//--------------------------------------------------
			// Green Poison
			//--------------------------------------------------
			case EFFECTSTATUS_GREEN_POISON :
			case EFFECTSTATUS_POISON :
				// [Help] green poison
//				GameInfoPresentation::ExecuteHelpEvent( HE_EFFECT_GREEN_POISON );
			break;

			//--------------------------------------------------
			// Purple Wall
			//--------------------------------------------------
			//case EFFECTSTATUS_PURPLE_WALL :
			case EFFECTSTATUS_CURSE_PARALYSIS :
				// [Help] purple wall
//				GameInfoPresentation::ExecuteHelpEvent( HE_EFFECT_PURPLE_WALL );
			break;

			//--------------------------------------------------
			// Yellow Poison
			//--------------------------------------------------
			case EFFECTSTATUS_YELLOW_POISON :
			case EFFECTSTATUS_YELLOW_POISON_TO_CREATURE :
				// [Help] yellow poison
//				GameInfoPresentation::ExecuteHelpEvent( HE_EFFECT_YELLOW_POISON );
			break;
		
			//--------------------------------------------------
			// BLOOD DRAIN 
			//--------------------------------------------------
			case EFFECTSTATUS_BLOOD_DRAIN :
			{
				// 2004, 5, 6 sobeit add start [Help] when blood-sucked
				GameInfoPresentation::ExecuteHelpEvent( HELP_EVENT_DRAIN_BLOOD );
				// 2004, 5, 6 sobeit add end
			}
			break;		

			//--------------------------------------------------
			// OBSERVING_EYE [new skill 6]
			//--------------------------------------------------
			case EFFECTSTATUS_OBSERVING_EYE :
				// If not yet applied
				if (!m_bEffectStatus[EFFECTSTATUS_OBSERVING_EYE])
				{
					CalculateLightSight();
				}
			break;

			case EFFECTSTATUS_GHOST:
				SetFlyingCreature();
//				g_pUserInformation->bCompetence = true;
			break;

			default:
			break;
		}

	//--------------------------------------------------
	// If it is an effect that turns into a vampire.. set the time.
	//--------------------------------------------------
	switch (status)
	{
		//--------------------------------------------------
		// Blood-sucking
		//--------------------------------------------------
		case EFFECTSTATUS_BLOOD_DRAIN :
		{
			DWORD conversionTime = delayFrame * 1000 / 16;// / g_pClientConfig->FPS;

			SetConversionDelay( conversionTime );

			DEBUG_ADD_FORMAT( "[BloodDrained] conversion in %lu ms", (unsigned long)conversionTime );

//			delayFrame *= 16;
//			UI_SetHP( GetHP(), GetMAX_HP() );
		}
		break;

		//--------------------------------------------------
		// hallu
		//--------------------------------------------------
		case EFFECTSTATUS_HALLUCINATION :
			g_pZone->ChangeToHalluCreature();			
		break;

		case EFFECTSTATUS_LIGHTNESS :
		case EFFECTSTATUS_YELLOW_POISON :
		case EFFECTSTATUS_FLARE :
		case EFFECTSTATUS_YELLOW_POISON_TO_CREATURE :
//		case EFFECTSTATUS_BLINDNESS:
			CalculateSight();
			break;
		case EFFECTSTATUS_CAN_ENTER_GDR_LAIR:
			delayFrame = delayFrame*10;
			break;
		case EFFECTSTATUS_INSTALL_TURRET:
//			g_pSkillAvailable->SetAvailableSkills();
			SetDelay(1000);
			VsUiPresentation::UnselectSkill();
			//gC_vs_ui.SelectSkill( SKILL_TURRET_FIRE );
			break;

		default:
			break;
	}

	bool re = MCreature::AddEffectStatus( status, delayFrame );
	//--------------------------------------------------
	// Notify the UI. Why does judging this by re go wrong (sigh) 2002.5.5 Ssukgat
	//--------------------------------------------------
	if(!(IsSlayer() && status == EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS))
		PlayerPresentation::AddEffectStatus( status, delayFrame );
	
	if(status == EFFECTSTATUS_INSTALL_TURRET)
	{
		g_pSkillAvailable->SetAvailableSkills();
		VsUiPresentation::SelectSkill( SKILL_TURRET_FIRE );
	}
	CheckRegen();

	return re; 
}

//----------------------------------------------------------------------
// Remove EffectStatus
//----------------------------------------------------------------------
bool		
MPlayer::RemoveEffectStatus(EFFECTSTATUS status)
{
	bool bOK = MCreatureWear::RemoveEffectStatus( status );

	//--------------------------------------------------
	// Notify the UI.
	//--------------------------------------------------
	PlayerPresentation::RemoveEffectStatus( status );

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
		if(GetWaitVerify() == MPlayer::WAIT_VERIFY_SYLPH_SUMMON_GETOFF)
			SetWaitVerifyNULL();
		
		/* add by sonic (garbled) fix for a bug, 2006.10.4 */
		if(IsAdvancementClass() && status==EFFECTSTATUS_SUMMON_SYLPH)
		{
				m_bEffectStatus[status]=true;
				MCreature::RemoveEffectStatus( status );
		}
		break;
		/*   end    */
		
		//--------------------------------------------------
		// hallu
		//--------------------------------------------------
		case EFFECTSTATUS_HALLUCINATION :
			g_pZone->RemoveHalluCreature();			
		break;

		//--------------------------------------------------
		// If it is an effect that turns into a vampire.. clear the time
		//--------------------------------------------------
		case EFFECTSTATUS_BLOOD_DRAIN :
			UnSetConversionDelay();

			StatusPresentation::SetHP( GetHP(), GetMAX_HP() );
		break;

		//--------------------------------------------------
		// When revived after dying
		//--------------------------------------------------		
		case EFFECTSTATUS_COMA :
			// Remove verification
			if (g_pTempInformation->GetMode()==TempInformation::MODE_WAIT_RESURRECT)
			{
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);
			}

			if (GetWaitVerify()==WAIT_VERIFY_RESURRECT)
			{
				SetWaitVerifyNULL();
			}

			PlayerPresentation::FinishRequestDie();
			PlayerPresentation::FinishRequestResurrect();
			PlayerPresentation::CloseRequestShrineMinimap();
		break;

		//--------------------------------------------------
		// OBSERVING_EYE [new skill 6]
		//--------------------------------------------------
		case EFFECTSTATUS_OBSERVING_EYE :
			CalculateStatus();
		break;

		case EFFECTSTATUS_GHOST:
			if (!(*g_pCreatureTable)[m_CreatureType].bFlyingCreature)	// Bat case
				SetGroundCreature();
			break;
		case EFFECTSTATUS_WILL_OF_LIFE :
			if(IsVampire() )
				MCreature::CheckRegen();
			break;
		case EFFECTSTATUS_FLARE :
		case EFFECTSTATUS_YELLOW_POISON :
		case EFFECTSTATUS_YELLOW_POISON_TO_CREATURE :
		case EFFECTSTATUS_LIGHTNESS :
			// 2004, 6, 21 sobeit add start - about blindness
//		case EFFECTSTATUS_BLINDNESS:
			// 2004, 6, 21 sobeit add end - about blindness
			if( bOK )
				CalculateSight();
			break;
//		case EFFECTSTATUS_GLACIER:
//			if (m_bAlive)
//			{
//				SetStop();
//				StopCauseCriticalWounds();
//			}			
//		break;
		case EFFECTSTATUS_INSTALL_TURRET:
			g_pSkillAvailable->SetAvailableSkills();
			WorldEvents::ExecuteActionInfoFromMainNode(RESULT_SKILL_INSTALL_TURRET,GetX(), GetY(), 0,0,GetID()	,
				GetX(), GetY(), 0, 0, NULL, false);
			SetDelay(1000);
			break;

		default:
			break;
	}
	
	return bOK;
}

//----------------------------------------------------------------------
// Set CreatureType
//----------------------------------------------------------------------
void	
MPlayer::SetCreatureType(TYPE_CREATURETYPE type)
{
	//add by viva 
//	type = 808; 
	MCreatureWear::SetCreatureType(type);

	//---------------------------------------------------
	// Check the Skill Icon again.
	//---------------------------------------------------
	if (g_pSkillAvailable!=NULL)
	{
		g_pSkillAvailable->SetAvailableSkills();		
	}	
	if(type==CREATURETYPE_SLAYER_OPERATOR
		|| type==CREATURETYPE_VAMPIRE_OPERATOR
		|| type==CREATURETYPE_OUSTERS_OPERATOR)
		g_pUserInformation->bCompetenceShape = true;
	else
		g_pUserInformation->bCompetenceShape = false;
}

//----------------------------------------------------------------------
// UpdateConversionTime
//----------------------------------------------------------------------
void
MPlayer::UpdateConversionTime()
{
	//--------------------------------------------------------
	// Check the Effect that turns into a vampire
	// Piling on code left and right.. heh (sigh)
	//--------------------------------------------------------

	// Check remaining war time
	
	if (IsSlayer())
	{
		if (m_ConversionDelayTime > g_FrameNow)
		{
			//--------------------------------------------------------
			// give the UI the time left before the change
			//--------------------------------------------------------
			// msec to minutes: * GetTimeRatio / 1000 / 60
			int changeTime = (int)(m_ConversionDelayTime - g_FrameNow).count()/* * g_pGameTime->GetTimeRatio()*/ / 60000 ;
			PlayerPresentation::SetChangeVampireTime( changeTime );

			static MonotonicClock::TimePoint enableBlinkTime;	// when the next blink may start
			
			//const DWORD sixHour = 6*60*60*1000;

			// { remaining time, blink interval }
			const int maxBlink = 12;
			const DWORD blinkValue[maxBlink][2] = 
			{
				{ 4*60*60*1000, 6*60*1000 },	// 4 hours or more --> every 5 minutes
				{ 3*60*60*1000, 4*60*1000 },	// 3 hours or more --> every 4 minutes
				{ 2*60*60*1000, 3*60*1000 },	// 2 hours or more --> every 3 minutes
				{ 1*60*60*1000, 2*60*1000 },	// 1 hour or more --> every 2 minutes
				{ 1*60*60*1000, 1*60*1000 },	// 10 minutes or more --> 1 minute
				{ 3*60*1000, 30*1000 },			// 3 minutes or more --> 30 seconds
				{ 2*60*1000, 10*1000 },			// 2 minutes or more --> 10 seconds
				{ 1*60*1000, 5*1000 },			// 1 minute or more --> 5 seconds
				{ 30*60*1000, 3*1000 },			// 30 seconds or more --> 3 seconds
				{ 10*1000, 2*1000 },			// 10 seconds or more --> 2 seconds
				{ 10*1000, 2*1000 },			// 5 seconds or more --> 1 second
				{ 0*1000, 300 }					// 0 seconds or more --> 0.3 seconds
			};

			//--------------------------------------------------------
			// after the last blink,
			// is it time to blink again?
			//--------------------------------------------------------
			if (g_FrameNow > enableBlinkTime)
			{
				// time left to the change; /1000 gives seconds
				DWORD timeGap = (DWORD)(m_ConversionDelayTime - g_FrameNow).count();
				
				// Always output. Output the effect only when less than 6 hours remain.
				if (1)//timeGap < sixHour)
				{
					BOOL bBlink = FALSE;

					for (int i=0; i<maxBlink; i++)
					{
						if (timeGap > blinkValue[i][0])
						{
							WorldEvents::SetFadeStart(25, 31, 2, 31,0,0, 0);

							// when the next blink may start
							enableBlinkTime = g_FrameNow + MonotonicClock::Millis(blinkValue[i][1]);

							bBlink = TRUE;

							break;
						}
					}

					if (!bBlink)
					{
						UnSetConversionDelay();
					}
				}
			}
		}
		else
		{
			m_ConversionDelayTime = MonotonicClock::TimePoint();

			//--------------------------------------------------------
			// give the UI the time left before the change
			//--------------------------------------------------------
			PlayerPresentation::SetChangeVampireTime( 0 );
		}
	}
}

//----------------------------------------------------------------------
// Take the next action.
//----------------------------------------------------------------------
void
MPlayer::Action()
{	
	if( !HasEffectStatus( EFFECTSTATUS_FIRE_ELEMENTAL ) && !HasEffectStatus( EFFECTSTATUS_WATER_ELEMENTAL ) && GetElementalID() != OBJECTID_NULL && g_pZone != NULL)
	{
		g_pZone->RemoveFakeCreature(GetElementalID());
	}

	// When there is a pet and over 1 second has passed since the pet's action
	if( GetPetID() != OBJECTID_NULL)
	{
		MFakeCreature *pFakeCreature = (MFakeCreature *)g_pZone->GetFakeCreature(GetPetID());
		if(pFakeCreature != NULL)
		{
			MPetItem *pPetItem = pFakeCreature->GetPetItem();
			if(pPetItem != NULL)
			{
				const int petLevelMax = 50;
				int petLevel = pPetItem->GetNumber();
				int petDelayTime = 0;	
				TYPE_ITEMTYPE tmpItemType = pPetItem->GetItemType();
				// add by svi 2009-07-01 pet speed (garbled)
				if(tmpItemType == 6 || tmpItemType == 7 || tmpItemType == 8)
				{
					petDelayTime = 1200 + ((petLevelMax-petLevel)*120)/petLevelMax;
				}
				// end
				else if(pPetItem->GetItemType()>2)// If it is a 2nd-tier pet
					petDelayTime = 3200+((petLevelMax-petLevel)*2800)/petLevelMax;
				else
					petDelayTime = 1200+((petLevelMax-petLevel)*2800)/petLevelMax;

				if(MonotonicClock::Now() - m_PetDelayTime > MonotonicClock::Millis((DWORD)max(0, petDelayTime)) )
				{
					int itemLimit = 1;	// Excluding the head

					if(pPetItem->IsCanCutHead())	// If it has the head-cutting function, including the head
						itemLimit = 0;

					MItem *pItem = g_pZone->GetCorpseKilledByMe(itemLimit);

					if(pItem != NULL)
					{
						CGDissectionCorpse _CGDissectionCorpse;
						_CGDissectionCorpse.setX( static_cast<Coord_t>(pItem->GetX()) );
						_CGDissectionCorpse.setY( static_cast<Coord_t>(pItem->GetY()) );
						_CGDissectionCorpse.setPet( true );
						_CGDissectionCorpse.setObjectID( pItem->GetID() );

						g_pSocket->sendPacket( &_CGDissectionCorpse );

						// Count everything together, let's set it as the limit
						DEBUG_ADD_FORMAT("[PETPET] itemLimit %d", itemLimit);
						pItem->SetNumber( itemLimit );
					}
					
					m_PetDelayTime = MonotonicClock::Now();
				}
			}
		}
	}

	// So it does not repeat for too long
	if( m_nUsedActionInfo == SKILL_BLITZ_SLIDING_ATTACK || m_nUsedActionInfo == SKILL_BLAZE_WALK_ATTACK )
	{
		// [SKILLREPEAT DIAG] kept
		if (m_bRepeatAction)
			DEBUG_ADD_FORMAT("[SKILLREPEAT] why=BLITZBLAZE");

		UnSetRepeatAction();
	}

	if(IsRepeatAction())
	{
		if(MonotonicClock::Now() - m_RepeatTimer > MonotonicClock::Millis(g_pClientConfig->REPEAT_TIME))
		{
			// [SKILLREPEAT DIAG] kept
			DEBUG_ADD_FORMAT("[SKILLREPEAT] why=REPEATTIMER elapsed=%u limit=%u",
				(unsigned)(MonotonicClock::Now() - m_RepeatTimer).count(), (unsigned)g_pClientConfig->REPEAT_TIME);

			UnSetRepeatAction();
		}
	}
	// So it does not repeat for too long
	if(IsLockMode())
	{
		if(MonotonicClock::Now() - m_LockTimer > MonotonicClock::Millis(g_pClientConfig->LOCK_TIME))
			UnSetLockMode();
	}
	//if (m_ActionCount==m_ActionCountMax)
	//{
	//	int a =0;
	//}
	
	//----------------------------------------------------------------------
	// Send my status info to the party members.
	//----------------------------------------------------------------------
	SendStatusInfoToParty();
	
#ifdef OUTPUT_DEBUG_PLAYER_ACTION
	DEBUG_ADD_FORMAT("[MPLayer::Action] Action[%d]=%d/%d. Move=%d/%d"
					, m_Action, m_ActionCount, m_ActionCountMax, m_MoveCount, m_MoveCountMax);	
#endif
	
#ifdef __METROTECH_TEST__
	if(!IsVampire())
	{
		if (g_iAutoHealPotion == 1 && m_RecoveryHPTimes==0)	AutoUsePotion(0);		// HP
		if (g_bAutoManaPotion && m_RecoveryMPTimes==0)	AutoUsePotion(1);		// MP
		if (g_iAutoReload)	AutoUsePotion(2+g_iAutoReload-1);					// Magazine
//		if (g_iAutoHealPotion == 2 || g_iAutoHealPotion == 3)  AutoUseSkill(0);									// Healing
//		if (g_bHolyWater)  AutoUseSkill(1);									// CreateHolyWater
	}
#endif
	/*
	// [ TEST CODE ]
	// EffectTarget add-result test
	if (m_listEffectTarget.size()!=0 && m_ActionCount==m_ActionCountMax)
	{
		MActionResult* pResult = new MActionResult;
		pResult->Add( new MActionResultNodeActionInfo( 1, 1001, GUIDANCE_FIRE ) );
		
		// When there is no Effect related to the result..
		EFFECTTARGET_LIST::iterator iEffectTarget = m_listEffectTarget.begin();
		MEffectTarget* pEffectTarget = *iEffectTarget;
		
		BYTE id = pEffectTarget->GetEffectID();

		if (!AddActionResult(id, pResult))
		{
			// Execute the result right away..
			// and delete it from memory.
			delete pResult;
		}
		
	}
	*/

	#ifdef OUTPUT_DEBUG
		if (m_ActionCount > m_ActionCountMax)
		{
			DEBUG_ADD("So Terrible!");
		}
	#endif

	//--------------------------------------------------------
	// Help --;
	//--------------------------------------------------------
//	__BEGIN_HELP_EVENT
//		static DWORD nextHelpTime = g_CurrentTime+20*1000;	// first message output after 20 seconds
//		if (g_CurrentTime > nextHelpTime)
//		{
//			GameInfoPresentation::ExecuteHelpEvent( HE_TIME_PASSED );
//
//			nextHelpTime = g_CurrentTime + 20*1000;	// 20 seconds
//		}
//	__END_HELP_EVENT

	//--------------------------------------------------------
	// Status change - HP, MP change
	//--------------------------------------------------------
	UpdateStatus();	
	
	//--------------------------------------------------------
	// Check the Effect that turns into a vampire
	//--------------------------------------------------------
	UpdateConversionTime();
	
	//--------------------------------------------------------
	// when the chat string scrolls
	//--------------------------------------------------------
	if (m_NextChatFadeTime < g_FrameNow)
	{
		FadeChatString();
	}

	//--------------------------------------------------------
	// Special effects
	//--------------------------------------------------------
	if(UpDateInstallTurret())
		return;
	UpdateInvisible();
	UpdateCasket();
	UpdateCutHeight();
	UpdateTurning();
	UpdateCauseCriticalWounds();
//	UpdateBloodyZenith();
	UpdateGunShotGuidance();
	UpdateFakeDie();
	UpdateGhost();

	UpdateDarkness();

	UpdateSweepVice();
	UpdateBurningSol();		

	UpdateBikeCrash();
	//--------------------------------------------------------
	// Make direction changes natural
	//--------------------------------------------------------
	ChangeNearDirection();
	//m_CurrentDirection = m_Direction;

	//--------------------------------------------------------
	// Check result display depending on the skill
	//--------------------------------------------------------
	#ifdef OUTPUT_DEBUG_PLAYER_ACTION
		DEBUG_ADD("beforeCheckListEffect");
	#endif
	
	if (m_nUsedActionInfo < g_pActionInfoTable->GetMinResultActionInfo())
	{		
		int numTarget = static_cast<int>(m_listEffectTarget.size());

		if (numTarget!=0)
		{
			//------------------------------------------------------------
			// If some skill is being used
			// so that the result is shown at that skill's result point...
			//------------------------------------------------------------
			BOOL	bActionStand	= (m_Action==ACTION_STAND || (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND));
			//int		actionCountMax_1 = m_ActionCountMax-1;
			//BOOL	bEndAction		= (m_ActionCount==actionCountMax_1) || bActionStand;
			int		StartFrame		= (*g_pActionInfoTable)[m_nUsedActionInfo].GetStartFrame( m_WeaponSpeed );

			BOOL	bStartResult	= bActionStand 
										|| (m_ActionCount >= StartFrame)
										|| m_ActionCount >= m_ActionCountMax;

			bStartResult |= m_bRepeatAction 
							&& (*g_pActionInfoTable)[m_nUsedActionInfo].IsUseRepeatFrame()
							&& m_RepeatCount!=0
							&& m_ActionCount >= GetActionInfoRepeatStartFrame( m_nUsedActionInfo );

			// Execute all the results.
			if (bStartResult)
			{
				EFFECTTARGET_LIST::iterator iEffectTarget = m_listEffectTarget.begin();
		
				while (iEffectTarget != m_listEffectTarget.end())
				{
					MEffectTarget* pEffectTarget = *iEffectTarget;

					if (pEffectTarget->IsResultTime())// || pEffectTarget->IsEnd())
					{
						MActionResult* pResult = pEffectTarget->GetResult();

						if (pResult!=NULL)
						{
							// Remove the executed result.
							int numTargetBeforeExecute = static_cast<int>(m_listEffectTarget.size());

							pEffectTarget->SetResultNULL();

							pResult->Execute();

							delete pResult;

							int numTargetAfterExecute = static_cast<int>(m_listEffectTarget.size());
							
							// m_listEffectTarget may be removed in pResult->Execute() (sigh)
							// Messy structure... needs fixing (sigh)
							if (numTargetBeforeExecute!=numTargetAfterExecute)
							{
								// Check again from the beginning
								iEffectTarget = m_listEffectTarget.begin();
								continue;
							}

							// The delete is done externally...
							// Probably (sigh) attached to the Effect and deleted when the Effect is removed..
							// But.. I don't remember well..
							// It crashes if deleted, and doesn't crash if not deleted, so that seems right.. (sigh)
							//delete pEffectTarget;
						}
					}

					iEffectTarget++;
				}		

				m_listEffectTarget.clear();
			}
		}
	}

	#ifdef OUTPUT_DEBUG_PLAYER_ACTION
		DEBUG_ADD("bcleok");
	#endif

	//--------------------------------------------------------
	// If all the delay after skill use has finished...
	//--------------------------------------------------------	
	if ((!m_bTurning	// [new skill]
		&& !HasEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS)
		&& !IsCauseCriticalWounds()
//		&& !IsBloodyZenith()
		&& !IsGunShotGuidance()
		&& !HasEffectStatus(EFFECTSTATUS_SOUL_CHAIN)
		&& !HasEffectStatus(EFFECTSTATUS_TRAP_TRIGGERED)
		&& !HasEffectStatus(EFFECTSTATUS_TRAPPED))
//		&& g_pUserInformation->LogoutTime == 0
		|| g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		|| g_bLight
#endif
		)// && !HasEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS))	//IsNotDelay())
	{
		//--------------------------------------------------------
		// Next buffered actioninfo
		//--------------------------------------------------------
		/*
		if (m_nNextUsedActionInfo!=ACTIONINFO_NULL
			&& m_nUsedActionInfo==ACTIONINFO_NULL)
		{
			m_nUsedActionInfo = m_nNextUsedActionInfo;
			m_nNextUsedActionInfo = ACTIONINFO_NULL;
		}
		*/
		//--------------------------------------------------------
		// If any effect is applied, move twice as slowly.
		//--------------------------------------------------------
		if ((HasEffectStatus( EFFECTSTATUS_HAS_SLAYER_RELIC )       || HasEffectStatus( EFFECTSTATUS_HAS_VAMPIRE_RELIC ) ||
			HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_GREGORI )|| HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_NEMA ) ||
			HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_LEGIOS ) || HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_MIHOLE ) ||
			HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_AROSA )  || HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_ARMEGA ) ||
			HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_INI )    || HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_JAVE ) ||
			HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_CHASPA ) || HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_CONCILIA ) ||
			HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_KIRO )   || HasEffectStatus( EFFECTSTATUS_HAS_BLOOD_BIBLE_HILLEL ) ||
			HasEffectStatus( EFFECTSTATUS_AMBER_OF_GUARD ) || HasEffectStatus( EFFECTSTATUS_AMBER_OF_AVENGER ) ||
			HasEffectStatus( EFFECTSTATUS_AMBER_OF_IMMORTAL ) || HasEffectStatus( EFFECTSTATUS_AMBER_OF_CURSE ) ||
			HasEffectStatus( EFFECTSTATUS_HAS_CASTLE_SYMBOL_5 ) || HasEffectStatus( EFFECTSTATUS_HAS_CASTLE_SYMBOL_6 ) ||
			(IsCreatureMove( dynamic_cast<MCreature*>(this) ) && HasEffectStatus( EFFECTSTATUS_ICE_FIELD_TO_CREATURE )) ||
			(IsCreatureMove( dynamic_cast<MCreature*>(this) ) && HasEffectStatus( EFFECTSTATUS_FROZEN_ARMOR_TO_ENEMY )) ||
			(IsCreatureActionAttack( this ) && (	HasEffectStatus( EFFECTSTATUS_ICE_OF_SOUL_STONE ) || (IsInFuryOfGnome() && !IsOusters()/*(IsSlayer() || IsVampire())*/)	)) || 
			(IsCreatureMove( dynamic_cast<MCreature*>(this) ) && HasEffectStatus( EFFECTSTATUS_JABBING_VEIN )) ||
			HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER ) || HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_2 ) ||
			HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_3 ) || HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_4 ) ||
			HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_5 ) || HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_6 ) ||
			HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_7 ) || HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_8 ) ||
			HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_9 ) || HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_10 ) ||
			HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_11 ) || HasEffectStatus( EFFECTSTATUS_HAS_SWEEPER_12 ))
			&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )			
#ifdef __METROTECH_TEST__
			&& !g_bLight
#endif
			)			
		{
			if (g_CurrentFrame & 0x01)
			{
				return;
			}
		}

		//--------------------------------------------------------
		// KnockBack handling 2001.10.9
		//--------------------------------------------------------
		if (m_bKnockBack > 0)
		{
			m_sX += m_cX;
			m_sY += m_cY;

			// When knockBack is all done, stop motion.
			if (--m_bKnockBack==0)
			{
				m_sX = 0;
				m_sY = 0;

				SetAction( ACTION_STAND );

				return;
			}			
		}
		//--------------------------------------------------------
		// Do not take actions during FastMove. 2001.8.10
		//--------------------------------------------------------
		else if (!m_bFastMove)
		{
			//--------------------------------------------------------
			// Use Lock Mode
			//--------------------------------------------------------
			if (m_bLockMode && !IsInDarkness())
			{
				int creatureID = FindEnemy();

				//--------------------------------------------------------
				// When there is no enemy within range
				//--------------------------------------------------------
				if (static_cast<TYPE_OBJECTID>(creatureID) == OBJECTID_NULL)
				{
					// [SKILLREPEAT DIAG] kept
					if (m_bRepeatAction)
						DEBUG_ADD_FORMAT("[SKILLREPEAT] why=NOENEMY rb=%d lb=%d", (int)g_bRButtonDown, (int)g_bLButtonDown);

					m_listDirection.clear();

					UnSetRepeatAction();
					TraceNULL();
					m_fTraceBuffer	= FLAG_TRACE_NULL;	
					m_fNextTrace	= FLAG_TRACE_NULL;

					// Clear the path that was found.
					m_DestX		= SECTORPOSITION_NULL;
					m_DestY		= SECTORPOSITION_NULL;
					m_NextDestX	= SECTORPOSITION_NULL;
					m_NextDestY	= SECTORPOSITION_NULL;

					WorldEvents::SetSelectedNULL();
				}
				//--------------------------------------------------------
				// When there is an enemy
				//--------------------------------------------------------
				else
				{
					int bBasicAction = g_bLButtonDown;
					int bSpecialAction = g_bRButtonDown;

					//---------------------------------------------------------------
					// Basic action
					//-------------------------------------------------------------
					if (bBasicAction)
					{
						if (TraceCreatureToBasicAction( 
									creatureID, 
									true ))		// forced attack
						{
							//----------------------------------
							// Set repeat only for repeat actions
							//----------------------------------
							//if (m_nBasicActionInfo!=ACTIONINFO_NULL 
							//	&& (*g_pActionInfoTable)[m_nBasicActionInfo].IsUseRepeatFrame()
							//	&& !IsRepeatAction())
							//{
							//	SetRepeatAction();
							//}

							WorldEvents::SetSelectedCreature( creatureID );
							PlayerPresentation::SetCursorAttack();

							//g_bPreviousMove = false;
						}
					}
					//-------------------------------------------------------------
					// Special action
					//-------------------------------------------------------------
					else if (bSpecialAction)
					{
						if (TraceCreatureToSpecialAction( creatureID, true ))
						{
							//----------------------------------
							// Set repeat only for repeat actions
							//----------------------------------
							if (m_nSpecialActionInfo!=ACTIONINFO_NULL 
								&& (*g_pActionInfoTable)[m_nSpecialActionInfo].IsUseRepeatFrame()
								&& !IsRepeatAction()
								)
							{
								SetRepeatAction();
							}							
							
							WorldEvents::SetSelectedCreature( creatureID );
							PlayerPresentation::SetCursorAttack();

							//g_bPreviousMove = false;
						}
					}					

					// Remove the destination marker.
					WorldEvents::SetSelectedSectorNULL();
				}
				
			}

			//--------------------------------------------------------
			// Check bleeding
			//--------------------------------------------------------
			if (m_bAlive)
			{
				CheckDropBlood();
			}

			// Set delayTime to none.
			//m_DelayTime	= 0;
			//----------------------------------------------------
			// When continuing to trace
			// Hmm.. the messy code keeps growing... (sigh)
			//----------------------------------------------------
			if (m_bKeepTraceCreature && (m_fTrace & FLAG_TRACE_CREATURE_BASIC))
			{							
				KeepTraceCreature();		
			}
			
			//------------------------------------------		
			//         Buffered next action
			//------------------------------------------
			BOOL bSlayer = IsSlayer();
			if (m_bNextAction ||
				((!IsOusters() || (IsOusters() && m_NextAction!=ACTION_OUSTERS_FAST_MOVE_STAND ))
				&& m_NextAction!=ACTION_STAND 
				&& (!bSlayer || (bSlayer && m_NextAction!=ACTION_SLAYER_MOTOR_STAND))
				&& m_NextAction!=m_MoveAction))
			{
				SetAction( m_NextAction );
				
				#ifdef OUTPUT_DEBUG
					if (m_ActionCount > m_ActionCountMax)
					{
						DEBUG_ADD("So Terrible! return1");
					}
				#endif

				return;
			}

			//------------------------------------------
			// [ TEST CODE ]
			//------------------------------------------
			// While moving..
			// When tracing and the trace target distance is reached..
			// the action must be performed right this time.
			// So, return false
			if (m_NextAction==m_MoveAction &&
				m_fTrace != FLAG_TRACE_NULL &&
				(max(abs(m_X-m_TraceX), abs(m_Y-m_TraceY))) <= m_TraceDistance)
			{
				SetAction( m_NextAction );
			}

			//------------------------------------------
			// Perform Action.. to the next Frame...
			//------------------------------------------
			if (m_ActionCount < m_ActionCountMax)
			{		
				if (m_nUsedActionInfo!=ACTIONINFO_NULL)
				{
					ActionEffect();
				}
				else
				{
					//m_ActionCount--;

					// Perform the appropriate Action
					//switch (m_Action)
					//

					BOOL bSlayer = IsSlayer();

						// Stop motion
					if (m_Action==ACTION_STAND || (IsOusters() && m_Action==ACTION_OUSTERS_FAST_MOVE_MOVE)
						|| (bSlayer && m_Action==ACTION_SLAYER_MOTOR_STAND))
					{
												
						m_ActionCount++;						

						//------------------------------------------------------------
						// Check repeated action..
						//------------------------------------------------------------
						if (CheckRepeatAction())
						{
							if (CheckBufferAction())
							{
								if (m_nNextUsedActionInfo != ACTIONINFO_NULL)
								{
									// Ugh.. confusing.. (sigh)
									m_nUsedActionInfo = m_nNextUsedActionInfo;
									m_nNextUsedActionInfo = ACTIONINFO_NULL;							

									if (ActionInTraceDistance())
									{
										if (m_nNextUsedActionInfo != ACTIONINFO_NULL)
										{
											// Ugh.. confusing.. (sigh)
											m_nUsedActionInfo = m_nNextUsedActionInfo;
											m_nNextUsedActionInfo = ACTIONINFO_NULL;
										}
										else
										{
											// No need to go any further.
											m_listDirection.clear();

											// Stop moving
											m_MoveCount = m_MoveCountMax;

											// Make the direction the Player faces the direction where the enemy currently is.
											SetDirectionToPosition( m_TraceX, m_TraceY );

											SetAction( m_NextAction );

											//ActionToSendPacket();
										}
									}
								}
								#ifdef OUTPUT_DEBUG
									if (m_ActionCount > m_ActionCountMax)
									{
										DEBUG_ADD("So Terrible! return2");
									}
								#endif

								return;
							}
						}					

						/*
						else if (m_Action==ACTION_MOVE 
								|| bSlayer && ACTION_SLAYER_MOTOR_MOVE)
						{
							// Move is handled separately.
							//ActionMove();					
						}
						*/					
					}
					else
					{
					//default :
						if (m_nUsedActionInfo==ACTIONINFO_NULL)
						{	
							m_ActionCount++;

							if (m_bStopBloodDrain)
							{
								StopBloodDrain();
							}							
							if (m_bStopAbsorbSoul)
							{
								StopAbsorbSoul();
							}							
						}
						else
						{
							ActionEffect();
						}
					}
				}
			}
		}
		


		//------------------------------------------
		// When the Action has finished
		//------------------------------------------
		if (m_bAlive)
		{
			// Added inequality sign 2001.10.7
			if (m_ActionCount>=m_ActionCountMax) //if (IsStop())
			{
				//--------------------------------------------------------
				// Having arrived at the target tile..
				// if standing still.. or if done walking..
				// show a swaying-in-place look.
				//--------------------------------------------------------
				// Added inequality sign 2001.10.7
				if (m_MoveCount>=m_MoveCountMax)
				{					
					//if (m_bRepeatAction && m_RepeatCount!=0 && (*g_pActionInfoTable)[m_nUsedActionInfo].IsRepeatFrame())
					//{						
					//	int a =0;
					//}
					//------------------------------------------------------------
					// Check repeated action..
					//------------------------------------------------------------
					//if (
						bool bCheckRepeat = CheckRepeatAction();
						//)
					//{
					//}

					if ((!m_bRepeatAction || !bCheckRepeat)
						&& m_fNextTrace != FLAG_TRACE_NULL)
					{			
						if (m_ActionCount>=m_ActionCountMax)
						{
							SetAction( ACTION_STAND );
						}
					}

					if (CheckBufferAction()) 
					{
						/*
						if (m_NextAction==m_MoveAction &&
							//m_fTrace != FLAG_TRACE_NULL &&
							(max(abs(m_X-m_TraceX), abs(m_Y-m_TraceY))) <= m_TraceDistance)
						{
							SetAction( m_NextAction );
						}
						*/
						if (m_nNextUsedActionInfo != ACTIONINFO_NULL)
						{
							// Ugh.. confusing.. (sigh)
							m_nUsedActionInfo = m_nNextUsedActionInfo;
							m_nNextUsedActionInfo = ACTIONINFO_NULL;							

							if (ActionInTraceDistance())
							{
								if (m_nNextUsedActionInfo != ACTIONINFO_NULL)
								{
									// Ugh.. confusing.. (sigh)
									m_nUsedActionInfo = m_nNextUsedActionInfo;
									m_nNextUsedActionInfo = ACTIONINFO_NULL;
								}
								else
								{
									// No need to go any further.
									m_listDirection.clear();

									// Stop moving
									m_MoveCount = m_MoveCountMax;

									// Make the direction the Player faces the direction where the enemy currently is.
									SetDirectionToPosition( m_TraceX, m_TraceY );

									SetAction( m_NextAction );

									//ActionToSendPacket();
								}
							}							
						}

						#ifdef OUTPUT_DEBUG
							if (m_ActionCount > m_ActionCountMax)
							{
								DEBUG_ADD("So Terrible! return3");
							}
						#endif

						return;
					}

					//------------------------------------------
					// Swaying-in-place look
					//------------------------------------------
					if (m_bRepeatAction)// && m_RepeatCount!=0)// && (*g_pActionInfoTable)[m_nUsedActionInfo].IsRepeatFrame())
					{						
					}
					else
					{
						SetAction( ACTION_STAND );
					}
					
				}
				//--------------------------------------------------------
				// Showed another action while walking, or was walking continuously.
				// Walk again.
				//--------------------------------------------------------					
				else
				{			
					m_Action = m_MoveAction;
					ActionMove();

					//----------------------------------------------------
					// When movement is all done..
					// Swaying-in-place look
					//----------------------------------------------------
					if (m_MoveCount==m_MoveCountMax)
					{		
						if (m_fNextTrace != FLAG_TRACE_NULL)
						{		
							if (m_ActionCount>=m_ActionCountMax)
							{
								SetAction( ACTION_STAND );
							}
						}

						if (CheckBufferAction()) 
						{
							if (m_nNextUsedActionInfo != ACTIONINFO_NULL)
							{
								// Ugh.. confusing.. (sigh)
								m_nUsedActionInfo = m_nNextUsedActionInfo;
								m_nNextUsedActionInfo = ACTIONINFO_NULL;							

								if (ActionInTraceDistance())
								{
									if (m_nNextUsedActionInfo != ACTIONINFO_NULL)
									{
										// Ugh.. confusing.. (sigh)
										m_nUsedActionInfo = m_nNextUsedActionInfo;
										m_nNextUsedActionInfo = ACTIONINFO_NULL;
									}
									else
									{
										// No need to go any further.
										m_listDirection.clear();

										// Stop moving
										m_MoveCount = m_MoveCountMax;

										// Make the direction the Player faces the direction where the enemy currently is.
										SetDirectionToPosition( m_TraceX, m_TraceY );

										SetAction( m_NextAction );

										//ActionToSendPacket();
									}
								}
							}

							#ifdef OUTPUT_DEBUG
								if (m_ActionCount > m_ActionCountMax)
								{
									DEBUG_ADD("So Terrible! return4");
								}
							#endif

							return;
						}

						//------------------------------------------
						// Swaying-in-place look
						//------------------------------------------				
						if (m_ActionCount>=m_ActionCountMax)
						{
							SetAction( ACTION_STAND );
						}
					}
				}			
			}	
		}
	}

	
	//--------------------------------------------------------
	// When there is a Delay...
	// Only if alive --> show the stop motion.
	//--------------------------------------------------------
	/*
	else if (m_bAlive)
	{
		if (m_ActionCount < m_ActionCountMax)
		{	
			if (m_Action==ACTION_STAND 
				|| IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND)
			{
				m_ActionCount++;
			}		

			// When the action has finished..
			if (m_ActionCount==m_ActionCountMax) 
			{
				// Stop motion again
				SetAction( (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND );
			}		hflh
		}
		else
		{
			SetAction( (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND );
		}
	}
	*/
	
}

//----------------------------------------------------------------------
// Add EffectTarget
//----------------------------------------------------------------------
// Add an in-progress EffectTarget.
//----------------------------------------------------------------------
void
MPlayer::AddEffectTarget(MEffectTarget* pEffectTarget)
{
	// Delete all the existing ones. heh
	m_listEffectTarget.clear();

	// Add one new one
	m_listEffectTarget.push_back( pEffectTarget );
}

//----------------------------------------------------------------------
// Remove EffectTarget
//----------------------------------------------------------------------
// When an in-progress Effect has finished...
// Remove the Target related to it.
//----------------------------------------------------------------------
void
MPlayer::RemoveEffectTarget(BYTE id)
{
	EFFECTTARGET_LIST::iterator iEffectTarget = m_listEffectTarget.begin();

	// Find and remove the one with the same id.
	// Since it is a list, everything must be compared, but...
	// the Player is only using a few skills in practice...
	while (iEffectTarget != m_listEffectTarget.end())
	{
		// When the id is the same
		if ((*iEffectTarget)->GetEffectID() == id)
		{
			// Removal from memory is done externally.
			//delete (*iEffectTarget);

			// Remove from the list.
			m_listEffectTarget.erase( iEffectTarget );

			break;
		}

		iEffectTarget++;
	}
}

//----------------------------------------------------------------------
// Packet Back
//----------------------------------------------------------------------
// If moving..
// Go Back to the previous tile.
// This is the handling for when attacked.
// There may be a hacking risk.. hmm.. (sigh)
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Packet MoveOK
//----------------------------------------------------------------------
// When MOVE_OK is received from the Server
//
// It means moving to the (x,y) sector in direction from the current position is possible.
//----------------------------------------------------------------------
void
MPlayer::PacketMoveOK(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, BYTE direction)
{
	(void)direction;
	// 2004, 9, 16, sobeit add start - change turret direction, maybe remove it.. (sigh)
//	if(HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
//	{
//		SetCurrentDirection(direction);
//		return;
//	}
	// 2004, 9, 16, sobeit add end
	// 2004, 9, 21, sobeit add start - Adam north cloud
	m_bShowAdamCloud = false;
	if(NULL != g_pZone && g_pZone->GetID() == 74)
	{
		if(sY<60)
			m_bShowAdamCloud = true;
	}
	// 2004, 9, 21, sobeit add end
	//--------------------------------------------------------
	// Among the remembered ones..
	// Remove the verified direction.
	//--------------------------------------------------------
	if (!m_listSendDirection.empty())
	{
		m_listSendDirection.pop_front();
	}

	//--------------------------------------------------------
	// When MoveOK arrives right after MoveError
	//--------------------------------------------------------
	if (m_SendMove==0)
	{
		//--------------------------------------------------------
		// There were cases where MoveOK arrived right after MoveError.
		// In this case.. (probably (sigh)) the coordinates get out of sync
		// and from then on it seems coordinate verification is never received.
		// So.. as a temporary(?) measure... move straight to the coordinates verified by the Server.
		//--------------------------------------------------------
		MovePosition( sX, sY );

		// Stop moving.
		m_sX = 0;	
		m_sY = 0;

		m_MoveCount		= m_MoveCountMax;
		m_ActionCount	= m_ActionCountMax; 

		// Remove path finding
		m_listDirection.clear();

		//----------------------------------------------------
		// If it was continuously tracing a Creature...
		//----------------------------------------------------
		if (m_bKeepTraceCreature						// continuously tracing
			&& (m_fTrace & FLAG_TRACE_CREATURE_BASIC)	// tracing a creature
			&& !m_bTraceCreatureToForceAttack)			// just following
		{							
			KeepTraceCreature();		
		}
		//----------------------------------------------------
		// When stopped during normal movement
		//----------------------------------------------------
		else
		{		
			TraceNULL();	

			// Clear the path that was found.
			m_DestX		= SECTORPOSITION_NULL;
			m_DestY		= SECTORPOSITION_NULL;
			m_NextDestX	= SECTORPOSITION_NULL;
			m_NextDestY	= SECTORPOSITION_NULL;
		}
	}
	//--------------------------------------------------------
	// Normal MoveOK case
	//--------------------------------------------------------
	else if (m_SendMove>0)
	{
		m_SendMove--;		
	}

	g_bNetStatusGood = true;

	//--------------------------------------------------------
	// Remember the position verified by the Server.
	//--------------------------------------------------------
	//POINT nextPoint;
	//MCreature::GetNextPosition(direction, nextPoint);
//	m_ServerX	= nextPoint.x;
//	m_ServerY	= nextPoint.y;

	m_ServerX	= sX;
	m_ServerY	= sY;


	//--------------------------------------------------------
	// (m_ServerX, m_ServerY) onward,
	// remove Creatures not in the Player's view from the Zone.
	//--------------------------------------------------------
	if (m_pZone!=NULL)
	{
		m_pZone->KeepObjectInSight(m_ServerX, m_ServerY, m_Sight);
	}

	SendPositionInfoToParty();

	return;
}

//----------------------------------------------------------------------
// Packet MoveNO
//----------------------------------------------------------------------
// When MOVE_NO is received from the Server
//----------------------------------------------------------------------
void	
MPlayer::PacketMoveNO()	
{
	//m_listSendDirection.front();
	if (!m_listSendDirection.empty())
	{
		m_listSendDirection.pop_front();
	}

	m_SendMove = 0;

	//------------------------------------------------------------
	// See whether there is a path to the current position.
	//
	// If there is no path, or the path is too long?
	//		move to the most recently verified position (ServerX,Y).
	//
	// If there is a path, send the movement packet up to the found position.
	//------------------------------------------------------------
	//if (!SetDestination( m_X, m_Y ) || m_listDirection.empty()	// no path
		//|| m_SendMove > g_pClientConfig->MAX_CLIENT_MOVE)		// path too long
	{
		// Remove from the Sector in the Zone where it currently is
		// and move to the most recently verified Sector from the Server.
		MovePosition( m_ServerX, m_ServerY );
		

		// Stop moving.
		m_sX = 0;	
		m_sY = 0;
		
		m_MoveCount		= m_MoveCountMax;
		//m_ActionCount	= m_ActionCountMax; 

		//----------------------------------------------------
		// If it was continuously tracing a Creature...
		//----------------------------------------------------
		if (m_bKeepTraceCreature						// continuously tracing
			&& (m_fTrace & FLAG_TRACE_CREATURE_BASIC)	// tracing a creature
			&& !m_bTraceCreatureToForceAttack			// just following
			&& m_ActionCount>=m_ActionCountMax)			// was moving with no special motion
		{							
			KeepTraceCreature();		
		}
		//----------------------------------------------------
		// When stopped during normal movement
		//----------------------------------------------------
		else
		{		
			// Clear the path that was found.
			m_listDirection.clear();

			/*
			// Somehow it feels like it will work if everything is commented out... (sigh)  2001.8.1
			m_DestX		= SECTORPOSITION_NULL;
			m_DestY		= SECTORPOSITION_NULL;
			m_NextDestX	= SECTORPOSITION_NULL;
			m_NextDestY	= SECTORPOSITION_NULL;

			TraceNULL();	

			// Added 2001.7.27
			UnSetRepeatAction();	
			*/
		}

		// 2001.8.22
		if (m_Action==m_MoveAction)
		{
			// If moving, just stop
			/*
			SetStop();

			m_DestX		= SECTORPOSITION_NULL;
			m_DestY		= SECTORPOSITION_NULL;
			m_NextDestX	= SECTORPOSITION_NULL;
			m_NextDestY	= SECTORPOSITION_NULL;

			TraceNULL();	

			UnSetRepeatAction();			
			*/
			// Remove path finding
			m_listDirection.clear(); 

			// Stop tracing
			//m_fTrace		= FLAG_TRACE_NULL;
			//m_fNextTrace	= FLAG_TRACE_NULL;

			// Stop Action
			//m_Action		= (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND;
			//m_ActionCount	= m_ActionCountMax; 	
			m_MoveCount		= m_MoveCountMax-1; 
			m_NextMoveCount = m_MoveCount;

			// Also remove the next motion
			m_bNextAction = false;
			m_NextX = SECTORPOSITION_NULL;
			m_NextY = SECTORPOSITION_NULL;
			
			//m_nNextUsedActionInfo = ACTIONINFO_NULL;
			//m_nUsedActionInfo = ACTIONINFO_NULL;

			
			// Remove target
			m_DestX			= SECTORPOSITION_NULL; 
			m_DestY			= SECTORPOSITION_NULL; 
			m_NextDestX		= SECTORPOSITION_NULL; 
			m_NextDestY		= SECTORPOSITION_NULL; 
		}
		// 2001.8.6
		// Temporary measure.. - when bumping into something and bouncing back while moving..
		else if (m_ActionCount>=m_ActionCountMax)
		{
			if (m_Action==m_MoveAction)
			{
				/*
				SetStop();

				m_DestX		= SECTORPOSITION_NULL;
				m_DestY		= SECTORPOSITION_NULL;
				m_NextDestX	= SECTORPOSITION_NULL;
				m_NextDestY	= SECTORPOSITION_NULL;

				TraceNULL();	

				UnSetRepeatAction();
				*/
				m_sX = 0;//-m_cX;
				m_sY = 0;//-m_cY;

				m_MoveCount = m_MoveCountMax - 1;
				m_NextMoveCount = m_MoveCount;
			}
		}

		//m_SendMove = 0;
		// Here the SX, SY values somehow become weird~~!!!!!!!!!!!!!!!!
	}
	/*
	// Because it is mostly blocked by a one-tile difference
	// it is not very meaningful, so it was commented out.
	// Also.. this is code that has never been tested even once. (sigh)
	else
	{
		DEBUG_ADD_FORMAT("[MoveNo] Find New Way. num=%d", m_listDirection.size());

		//----------------------------------------------------	
		//
		// From (m_ServerX, m_ServerY)
		//	move in the direction up to (m_X, m_Y).
		//
		//----------------------------------------------------
		int x = m_ServerX;
		int y = m_ServerY;

		while (!m_listDirection.empty())
		{
			//----------------------------------------------------
			// Read one direction from m_listDirection.
			//----------------------------------------------------
			int direction = m_listDirection.front();
			m_listDirection.pop_front();	
		
			switch (direction)
			{
				case DIRECTION_LEFTDOWN		: x--;	y++;	break;
				case DIRECTION_RIGHTUP		: x++;	y--;	break;
				case DIRECTION_LEFTUP		: x--;	y--;	break;
				case DIRECTION_RIGHTDOWN	: x++;	y++;	break;
				case DIRECTION_LEFT			: x--;			break;
				case DIRECTION_DOWN			:		y++;	break;
				case DIRECTION_UP			:		y--;	break;
				case DIRECTION_RIGHT		: x++;			break;
			}

			//----------------------------------------------------
			// Must be verified by the Server.
			//----------------------------------------------------
			#ifdef	CONNECT_SERVER
				CGMove _CGMove;
				_CGMove.setX( x );
				_CGMove.setY( y );
				_CGMove.setDir( direction );
				g_pSocket->sendPacket( &_CGMove );
				
				// Remember the direction sent to the Server.
				m_listSendDirection.push_back( direction );
				m_SendMove++;				
			#endif
		}
	}
	*/
}


//----------------------------------------------------------------------
// Packet SpecialAction Result
//----------------------------------------------------------------------
// This Creature took Damage, so
// if moving, it must be put Back to the tile just before.
//
// 0. Stop all motions..
// 1. Action --> Damage-taking motion
// 2. Show Damage
//----------------------------------------------------------------------
void		
MPlayer::PacketSpecialActionResult(TYPE_ACTIONINFO nResultActionInfo, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, BYTE temp)
{
	//--------------------------------------------------
	// Return if dead
	//--------------------------------------------------
	if (!m_bAlive)
		return;

	//----------------------------------------------------------------------
	// If it is not a result action..  - does it matter?? hmm..
	//----------------------------------------------------------------------
	if (nResultActionInfo < (*g_pActionInfoTable).GetMinResultActionInfo()
		// Do not show the action during fast move... // 2001.8.10
		|| m_bFastMove || m_bKnockBack)
	{
		return;
	}

	//----------------------------------------------------------------------
	// Shield effect.. (sigh)
	//----------------------------------------------------------------------
	int actionInfoAction = GetActionInfoAction(nResultActionInfo);
	
	if (actionInfoAction==ACTION_DAMAGED)
	{
		//if (HasEffectStatus(EFFECTSTATUS_AURA_PRISM))
		{
		//	nResultActionInfo = REFLECT_AURA_PRISM;
		//	actionInfoAction = GetActionInfoAction(nResultActionInfo);
		}
		//else 
			if (HasEffectStatus(EFFECTSTATUS_AURA_SHIELD))
		{
			nResultActionInfo = REFLECT_AURA_PRISM;//REFLECT_AURA_SHIELD;
			actionInfoAction = GetActionInfoAction(nResultActionInfo);
		}

		//-------------------------------------------------
		// Force Feel
		//-------------------------------------------------
//		if (gpC_Imm!=NULL && gpC_Imm->IsDevice())
//		{
//			gpC_Imm->ForceAction( CImm::FORCE_ACTION_DAMAGED );
//		}
	}

	//-------------------------------------------------
	// Remember it because of the repeat action.
	//-------------------------------------------------
	int oldTraceID = m_TraceID;

	//----------------------------------------------------------------------
	// If there was something it was about to use...
	//----------------------------------------------------------------------
	///*
	m_nNextUsedActionInfo = ACTIONINFO_NULL;
	TraceNextNULL();

	if (m_nUsedActionInfo!=ACTIONINFO_NULL)
	{
		BOOL	bStartActionInfo = m_nUsedActionInfo < g_pActionInfoTable->GetMinResultActionInfo();
		BOOL	bSentPacket		= (m_ActionCount>0);
		int		StartFrame		= (*g_pActionInfoTable)[m_nUsedActionInfo].GetStartFrame( m_WeaponSpeed );

		// When the Effect starts:
		// (1) when it is the StartFrame
		// (2) when it is the last ActionFrame
		BOOL	bStartedEffect = m_ActionCount>=StartFrame;

		//--------------------------------------------------
		// Attach it if an effect is not already attached.
		//--------------------------------------------------		
		if (!bStartActionInfo						// result packet, or..
			|| (bSentPacket && !bStartedEffect))	// packet was sent but the Effect was not attached..
		{
			TYPE_ACTIONINFO oldUsedActionInfo = m_nUsedActionInfo;

			m_nUsedActionInfo = ACTIONINFO_NULL;

			AffectUsedActionInfo(oldUsedActionInfo);
			
			// Result display produced by AffectUsedActionInfo..
			if (m_nUsedActionInfo != ACTIONINFO_NULL)
			{
				AttachCastingEffect( m_nUsedActionInfo, TRUE );
				AffectUsedActionInfo(m_nUsedActionInfo);
			}
		}			
		//--------------------------------------------------		
		// When the immediately preceding Effect is not output.
		//--------------------------------------------------		
		else
		{
			m_nUsedActionInfo = ACTIONINFO_NULL;			
		}
		
		//-------------------------------------------------------------
		// If castingEffect has not been output yet, output it.
		//-------------------------------------------------------------
		if (m_nUsedActionInfo!=ACTIONINFO_NULL
			&& GetActionInfoCastingStartFrame(m_nUsedActionInfo) >= m_ActionCount)
		{
			AttachCastingEffect( m_nUsedActionInfo, TRUE );		
		}
	
	}
	//*/

	// Stop moving.
	//m_sX = 0;	
	//m_sY = 0;
	// Put Back.
	////SetStop();
	//PacketBack();		
	
	//m_ActionCount = m_ActionCountMax; 

	// Stop tracing
	//-------------------------------------------------
	// On 2000.09.22 only m_fTrace was changed to NULL..
	//-------------------------------------------------
	//TraceNULL();
	m_fTrace = FLAG_TRACE_NULL;


	SetTraceID( id );
	m_TraceX	= sX;
	m_TraceY	= sY;

	// Clear the path that was found.
	m_listDirection.clear();
	m_DestX		= m_X;//SECTORPOSITION_NULL;
	m_DestY		= m_Y;//SECTORPOSITION_NULL;
	m_NextDestX	= SECTORPOSITION_NULL;
	m_NextDestY	= SECTORPOSITION_NULL;

	// Need to find the ActionInfo matching nResultActionInfo.
	// Just add MIN_RESULT_ACTIONINFO to the original ActionInfo.
//	m_nUsedActionInfo	= nResultActionInfo;	// + (*g_pActionInfoTable).GetMinResultActionInfo()
	//SetAction( (*g_pActionInfoTable)[m_nUsedActionInfo].GetAction() );

	// If done like this..
	// it goes to the tile currently in progress.. and gets attacked there,
	// so it can happen that it is attacked one tile away.
	// What should be done??
	//m_NextAction = (*g_pActionInfoTable)[m_nUsedActionInfo].GetAction();
	
	//SetNextAction( GetActionInfoAction(m_nUsedActionInfo) );
	//m_bNextAction = true;

	// Added 2001.05.21
	// ACTION_STAND is not shown in the skill motion.
	if (actionInfoAction!=ACTION_STAND)
	{
		SetAction( actionInfoAction );
	}

	if( temp != 0)
		SetActionGrade( temp );

	// Apply immediately
	//-------------------------------------------------------------
	//
	// Casting effect
	//
	//-------------------------------------------------------------
	//-------------------------------------------------------------
	// Casting effect
	//-------------------------------------------------------------
	AttachCastingEffect( nResultActionInfo, TRUE );		
	AffectUsedActionInfo( nResultActionInfo );


	//-------------------------------------------------
	// because of the repeat action...
	//-------------------------------------------------
	SetTraceID( oldTraceID );

	// use on self?..
	//-------------------------------------------------
	// commented out on 2000.09.22.. (sigh)
	//-------------------------------------------------
	//m_TraceID = m_ID;

	// Test Code
	/*
	if (rand()%2)
		SetChatString( "Shit!" );
	else
		SetChatString( "Oops!" );
	*/
}

//----------------------------------------------------------------------
// Packet Add ActionResult
//----------------------------------------------------------------------
// Receives from the Server the result related to an Action the Player used,
// and adds the ActionResult created externally
// to the Player's ActionResultList.
//
// That is, sets the Result of the MEffectTarget whose InstanceID is id.
//----------------------------------------------------------------------
// Return value:
//     true means it was added
//     false means there is no related EffectTarget,
//               so the result must be executed immediately.
//----------------------------------------------------------------------
bool		
MPlayer::PacketAddActionResult(WORD effectID, MActionResult* pActionResult)
{
	(void)effectID;
	/*
	MActionResult* pResult = new MActionResult;
	pResult->Add( new MActionResultNodeActionInfo( m_ID, m_TraceID, m_nUsedActionInfo ) );
		
	pEffectTarget->SetResult( pResult );
	*/
	
	// when there is no result to process
	if (pActionResult==NULL)
	{
		return false;
	}


	if (m_nUsedActionInfo < g_pActionInfoTable->GetMinResultActionInfo())
	{
		//------------------------------------------------------------
		// when the Effect in progress has an EffectTarget
		//------------------------------------------------------------
		if (m_listEffectTarget.size()!=0)
		{
			// check the result of the last EffectTarget..
			MEffectTarget* pEffectTarget = m_listEffectTarget.back();

			// if there is already a result..
			MActionResult* pResult = pEffectTarget->GetResult();

			if (pResult!=NULL)
			{
				pEffectTarget->SetResultNULL();
			}

			// assign the new result.
			pEffectTarget->SetResult( pActionResult );

			if (pResult!=NULL)
			{
				// execute the existing result right away.
				pResult->Execute();

				delete pResult;
			}			

			return true;

			/*
			EFFECTTARGET_LIST::iterator iEffectTarget = m_listEffectTarget.begin();

			while (iEffectTarget != m_listEffectTarget.end())
			{
				//------------------------------------------------------------
				// when the ids are the same
				//------------------------------------------------------------
				// EffectID should be checked,
				// but at present(!) one skill is used and verified one at a time,
				// so there is no need to check EffectID.
				//------------------------------------------------------------
				// the result will be shown in line with this action unconditionally,
				// so store the result.
				//if ((*iEffectTarget)->GetEffectID() == effectID)
				{
					// add it as the result of the EffectTarget.
					// when there is already a result..(?)
					// execute the previous result right away.
					if ((*iEffectTarget)->IsExistResult())
					{
						(*iEffectTarget)->GetResult()->Execute();
						//break; // originally.. (sigh)
					}

					// setting a new one deletes the old one.
					(*iEffectTarget)->SetResult( pActionResult );

					// when it was set properly
					return true;
				}

				iEffectTarget++;
			}
			*/
		}
	}

	//------------------------------------------------------------
	// when the Effect in progress has no EffectTarget
	//------------------------------------------------------------			
	// execute the result right away..
	// and delete it from memory.
	// (!) a Manager class for executing results is needed.
	pActionResult->Execute();

	delete pActionResult;
	
	return false;	
}

//----------------------------------------------------------------------
// On Attacking
//----------------------------------------------------------------------
// when attacking during a repeat action..
//----------------------------------------------------------------------
bool			
MPlayer::OnAttacking() const				
{ 
	return m_bRepeatAction && m_bTraceCreatureToForceAttack; 
}

//----------------------------------------------------------------------
// Basic Action To Creature
//----------------------------------------------------------------------
// when tracing is complete.. take the basic action on the Creature.
//----------------------------------------------------------------------
void
MPlayer::BasicActionToCreature()
{
	if (IsInDarkness())
	{
		return;
	}

	MCreature* pCreature = g_pZone->GetCreature( m_TraceID );

	if (pCreature==NULL)// || pCreature->IsDead())
	{
		return;
	}
	if(pCreature->CurPernalShop() == 1) // by csm 12/4
		return;

	//---------------------------------------------------------------
	// when the target is an NPC..
	//---------------------------------------------------------------
	if (pCreature->IsNPC())
	{
		// if it is not a bat or wolf.. trading is possible.
		if (//m_CreatureType!=CREATURETYPE_BAT
			//&& m_CreatureType!=CREATURETYPE_WOLF)
			(*g_pCreatureSpriteTable)[(*g_pCreatureTable)[m_CreatureType].SpriteTypes[0]].IsPlayerOnlySprite())
		{		
			if(pCreature->GetCreatureType() == 659)	// if it is the Earth Spirit's Horn, show the UI
				NoticePresentation::RunHorn(g_pZone->GetID());
			else
				
				if (IsWaitVerifyNULL())
				{
					// send the NPC processing packet.
					CGNPCTalk _CGNPCTalk;
					_CGNPCTalk.setObjectID( m_TraceID );

					g_pSocket->sendPacket( &_CGNPCTalk );

					SetWaitVerify( WAIT_VERIFY_NPC_ASK );
				}
		}

		// might be needed.. (sigh)
		m_bTraceCreatureToForceAttack = false;
		m_bKeepTraceCreature = false;

		// stop.
		SetNextAction(ACTION_STAND);
		m_nUsedActionInfo = ACTIONINFO_NULL;
		m_nNextUsedActionInfo = ACTIONINFO_NULL;
		m_fNextTrace = FLAG_TRACE_NULL;
		UnSetRepeatAction();
	}
	//---------------------------------------------------------------
	// when trying to Request
	//---------------------------------------------------------------
	else if (IsRequestMode())
	{
		switch (m_RequestMode)
		{
			//---------------------------------------------------------------
			// when trying to Trade
			//---------------------------------------------------------------
			case MRequestMode::REQUEST_TRADE :
			{
				if (IsWaitVerifyNULL() 
					&& g_pTempInformation->GetMode() == TempInformation::MODE_NULL
					&& ((IsSlayer() && pCreature->IsSlayer()) || 
					(IsVampire() && pCreature->IsVampire()) || 
					(IsOusters() && pCreature->IsOusters()))
					)
				{
						// trade request
						CGTradePrepare _CGTradePrepare;
						_CGTradePrepare.setTargetObjectID( m_TraceID );
						_CGTradePrepare.setCode( CG_TRADE_PREPARE_CODE_REQUEST );

						g_pSocket->sendPacket( &_CGTradePrepare );

					SetWaitVerify( WAIT_VERIFY_TRADE );
					
					// also set temp information.. (hm.. a bit sloppy)
					g_pTempInformation->SetMode(TempInformation::MODE_TRADE_REQUEST);
					g_pTempInformation->Value1 = m_TraceID;

					//-----------------------------------------------------
					// cancel the trade?
					//-----------------------------------------------------
					PlayerPresentation::RunExchangeCancel( pCreature->GetName() );
				}				
			}
			break;

			//---------------------------------------------------------------
			// when trying to Party
			//---------------------------------------------------------------
			case MRequestMode::REQUEST_PARTY :
			{
				if (IsWaitVerifyNULL() 
					&& g_pTempInformation->GetMode() == TempInformation::MODE_NULL
					&& g_pParty!=NULL
					// can only request if they are not in my party.
					&& !g_pParty->HasMember( pCreature->GetName() )
					&& ((IsSlayer() && pCreature->IsSlayer()) || (IsVampire() && pCreature->IsVampire()) || (IsOusters() && pCreature->IsOusters()))
					&& g_pSystemAvailableManager->IsAvailablePartySystem()
					)
				{
						// party request
						CGPartyInvite _CGPartyInvite;
						_CGPartyInvite.setTargetObjectID( m_TraceID );
						_CGPartyInvite.setCode( CG_PARTY_INVITE_REQUEST );

						g_pSocket->sendPacket( &_CGPartyInvite );

					SetWaitVerify( WAIT_VERIFY_PARTY );

					// also set temp information.. (hm.. a bit sloppy)
					g_pTempInformation->SetMode(TempInformation::MODE_PARTY_REQUEST);
					g_pTempInformation->Value1 = m_TraceID;

					//-----------------------------------------------------
					// cancel the trade?
					//-----------------------------------------------------
					PlayerPresentation::RunPartyCancel( pCreature->GetName() );
				}				
			}
			break;

			//---------------------------------------------------------------
			// when trying to view OtherInfo
			//---------------------------------------------------------------
			case REQUEST_INFO:
			{
//				MFakeCreature *pFakeCreature = (MFakeCreature *)g_pZone->GetFakeCreature(m_TraceID);
//				if(pFakeCreature != NULL && pFakeCreature->GetOwnerID() != OBJECTID_NULL)	// it's a pet
//				{
//					UI_RunPetInfo(pFakeCreature->GetPetItem());
//				}
//				else
				if (IsWaitVerifyNULL() 
					&& g_pTempInformation->GetMode() == TempInformation::MODE_NULL
					&& ((IsSlayer() && pCreature->IsSlayer()) || 
					(IsVampire() && pCreature->IsVampire()) || 
					(IsOusters() && pCreature->IsOusters()) ||
					GetCreatureType() == CREATURETYPE_SLAYER_OPERATOR ||
					GetCreatureType() == CREATURETYPE_VAMPIRE_OPERATOR ||
					GetCreatureType() == CREATURETYPE_OUSTERS_OPERATOR ||
					GetCompetence() == 0 )
					)
				{
						const char* str = PlayerPresentation::GetInputString();
						if(strlen(str)>0)
						{
							if(str[0] == '*')
							{
								PlayerPresentation::AddInputString(pCreature->GetName());
								break;

							}
						}
						// info request
						CGRequestInfo _CGRequestInfo;
						_CGRequestInfo.setValue( m_TraceID );
						_CGRequestInfo.setCode( CGRequestInfo::REQUEST_CHARACTER_INFO );

						g_pSocket->sendPacket( &_CGRequestInfo );

						MCreature *pCreature = g_pZone->GetCreature(m_TraceID);
						
						if(pCreature != NULL)
						{
							GameInfoPresentation::RunOtherInfo(pCreature);
						}


				}
			}
			break;
			default:
				break;
	
		}

		// might be needed.. (sigh)
		m_bTraceCreatureToForceAttack = false;
		m_bKeepTraceCreature = false;

		// stop.
		SetNextAction(ACTION_STAND);
		m_nUsedActionInfo = ACTIONINFO_NULL;
		m_nNextUsedActionInfo = ACTIONINFO_NULL;
		m_fNextTrace = FLAG_TRACE_NULL;
		UnSetRepeatAction();

		UnSetRequestMode();
	}	
	//---------------------------------------------------------------
	// otherwise.. it's an attack or a stop
	//---------------------------------------------------------------
	else
	{
		//---------------------------------------------------------------
		// either a forced attack..
		// decide whether to attack by looking at the race of the character being traced.
		//---------------------------------------------------------------
		bool bOusters = IsOusters();
		MItem *pOustersItem = NULL;
		if( bOusters) 
			pOustersItem = g_pOustersGear->GetItem(MOustersGear::GEAR_OUSTERS_RIGHTHAND);

		if (m_bTraceCreatureToForceAttack 
			&& ((bOusters && pOustersItem != NULL && pOustersItem->GetItemClass() == ITEM_CLASS_OUSTERS_CHAKRAM)
				|| !bOusters)
			)
		{
			SetNextAction(GetActionInfoAction(m_nBasicActionInfo));

			//--------------------
			// message
			//--------------------
			DEBUG_ADD_FORMAT("Basic Action : %d", m_TraceID);					
			
			// 
			//m_bTraceCreatureToForceAttack = false;
		}
		else
		{
			DEBUG_ADD_FORMAT("Basic Action : %d - But don't attack", m_TraceID);					
			
			// do not attack.
			m_nUsedActionInfo = ACTIONINFO_NULL;
			
			//---------------------------------
			// if tracing must continue..
			//---------------------------------
			if (m_bKeepTraceCreature)
			{
				return;
			}
		}
	}
	/*
	#ifdef	CONNECT_SERVER
		CGAttackNormal _CGAttackNormal;
		_CGAttackNormal.setX( m_X );
		_CGAttackNormal.setY( m_Y );
		_CGAttackNormal.setDir( m_Direction );
		_CGAttackNormal.setObjectID( m_TraceID );
		g_pSocket->sendPacket( &_CGAttackNormal );

		#ifdef	OUTPUT_DEBUG
			DEBUG_ADD( _CGAttackNormal.toString().c_str() );							
		#endif
	#endif
	*/	
}
//void
//MPlayer::BasicActionToCreature()
//{
//	if (IsInDarkness())
//	{
//		return;
//	}
//
//	MCreature* pCreature = g_pZone->GetCreature( m_TraceID );
//
//	if (pCreature==NULL)// || pCreature->IsDead())
//	{
//		return;
//	}
//
//	//---------------------------------------------------------------
//	// when the target is an NPC..
//	//---------------------------------------------------------------
//	if (pCreature->IsNPC())
//	{
//		// if it is not a bat or wolf.. trading is possible.
//		if (//m_CreatureType!=CREATURETYPE_BAT
//			//&& m_CreatureType!=CREATURETYPE_WOLF)
//			(*g_pCreatureSpriteTable)[(*g_pCreatureTable)[m_CreatureType].SpriteType].IsPlayerOnlySprite())
//		{		
//			if(pCreature->GetCreatureType() == 659)	// if it is the Earth Spirit's Horn, show the UI
//				UI_RunHorn(g_pZone->GetID());
//			else
//				
//				if (IsWaitVerifyNULL())
//				{
//					// send the NPC processing packet.
//					CGNPCTalk _CGNPCTalk;
//					_CGNPCTalk.setObjectID( m_TraceID );
//
//					g_pSocket->sendPacket( &_CGNPCTalk );
//
//					SetWaitVerify( WAIT_VERIFY_NPC_ASK );
//				}
//		}
//
//		// might be needed.. (sigh)
//		m_bTraceCreatureToForceAttack = false;
//		m_bKeepTraceCreature = false;
//
//		// stop.
//		SetNextAction(ACTION_STAND);
//		m_nUsedActionInfo = ACTIONINFO_NULL;
//		m_nNextUsedActionInfo = ACTIONINFO_NULL;
//		m_fNextTrace = FLAG_TRACE_NULL;
//		UnSetRepeatAction();
//	}
//	//---------------------------------------------------------------
//	// when trying to Request
//	//---------------------------------------------------------------
//	else if (IsRequestMode())
//	{
//		switch (m_RequestMode)
//		{
//			//---------------------------------------------------------------
//			// when trying to Trade
//			//---------------------------------------------------------------
//			case REQUEST_TRADE :
//			{
//				if (IsWaitVerifyNULL() 
//					&& g_pTempInformation->GetMode() == TempInformation::MODE_NULL
//					&& (IsSlayer() && pCreature->IsSlayer() || 
//					IsVampire() && pCreature->IsVampire() || 
//					IsOusters() && pCreature->IsOusters())
//					)
//				{
//						// trade request
//						CGTradePrepare _CGTradePrepare;
//						_CGTradePrepare.setTargetObjectID( m_TraceID );
//						_CGTradePrepare.setCode( CG_TRADE_PREPARE_CODE_REQUEST );
//
//						g_pSocket->sendPacket( &_CGTradePrepare );
//
//					SetWaitVerify( WAIT_VERIFY_TRADE );
//					
//					// also set temp information.. (hm.. a bit sloppy)
//					g_pTempInformation->SetMode(TempInformation::MODE_TRADE_REQUEST);
//					g_pTempInformation->Value1 = m_TraceID;
//
//					//-----------------------------------------------------
//					// cancel the trade?
//					//-----------------------------------------------------
//					UI_RunExchangeCancel( pCreature->GetName() );
//				}				
//			}
//			break;
//
//			//---------------------------------------------------------------
//			// when trying to Party
//			//---------------------------------------------------------------
//			case REQUEST_PARTY :
//			{
//				if (IsWaitVerifyNULL() 
//					&& g_pTempInformation->GetMode() == TempInformation::MODE_NULL
//					&& g_pParty!=NULL
//					// can only request if they are not in my party.
//					&& !g_pParty->HasMember( pCreature->GetName() )
//					&& (IsSlayer() && pCreature->IsSlayer() || IsVampire() && pCreature->IsVampire() || IsOusters() && pCreature->IsOusters())
//					&& g_pSystemAvailableManager->IsAvailablePartySystem()
//					)
//				{
//						// party request
//						CGPartyInvite _CGPartyInvite;
//						_CGPartyInvite.setTargetObjectID( m_TraceID );
//						_CGPartyInvite.setCode( CG_PARTY_INVITE_REQUEST );
//
//						g_pSocket->sendPacket( &_CGPartyInvite );
//
//					SetWaitVerify( WAIT_VERIFY_PARTY );
//
//					// also set temp information.. (hm.. a bit sloppy)
//					g_pTempInformation->SetMode(TempInformation::MODE_PARTY_REQUEST);
//					g_pTempInformation->Value1 = m_TraceID;
//
//					//-----------------------------------------------------
//					// cancel the trade?
//					//-----------------------------------------------------
//					UI_RunPartyCancel( pCreature->GetName() );
//				}				
//			}
//			break;
//
//			//---------------------------------------------------------------
//			// when trying to view OtherInfo
//			//---------------------------------------------------------------
//			case REQUEST_INFO:
//			{
////				MFakeCreature *pFakeCreature = (MFakeCreature *)g_pZone->GetFakeCreature(m_TraceID);
////				if(pFakeCreature != NULL && pFakeCreature->GetOwnerID() != OBJECTID_NULL)	// it's a pet
////				{
////					UI_RunPetInfo(pFakeCreature->GetPetItem());
////				}
////				else
//				if (IsWaitVerifyNULL() 
//					&& g_pTempInformation->GetMode() == TempInformation::MODE_NULL
//					&& (IsSlayer() && pCreature->IsSlayer() || 
//					IsVampire() && pCreature->IsVampire() || 
//					IsOusters() && pCreature->IsOusters() ||
//					GetCreatureType() == CREATURETYPE_SLAYER_OPERATOR ||
//					GetCreatureType() == CREATURETYPE_VAMPIRE_OPERATOR ||
//					GetCreatureType() == CREATURETYPE_OUSTERS_OPERATOR ||
//					GetCompetence() == 0 )
//					)
//				{
//						const char* str = gC_vs_ui.GetInputString();
//						if(strlen(str)>0)
//						{
//							if(str[0] == '*')
//							{
//								gC_vs_ui.AddInputString(pCreature->GetName());
//								break;
//
//							}
//						}
//						// info request
//						CGRequestInfo _CGRequestInfo;
//						_CGRequestInfo.setValue( m_TraceID );
//						_CGRequestInfo.setCode( CGRequestInfo::REQUEST_CHARACTER_INFO );
//
//						g_pSocket->sendPacket( &_CGRequestInfo );
//
//						MCreature *pCreature = g_pZone->GetCreature(m_TraceID);
//						
//						if(pCreature != NULL)
//						{
//							UI_RunOtherInfo(pCreature);
//
//						}
//
//
//				}
//			}
//			break;
//	
//		}
//
//		// might be needed.. (sigh)
//		m_bTraceCreatureToForceAttack = false;
//		m_bKeepTraceCreature = false;
//
//		// stop.
//		SetNextAction(ACTION_STAND);
//		m_nUsedActionInfo = ACTIONINFO_NULL;
//		m_nNextUsedActionInfo = ACTIONINFO_NULL;
//		m_fNextTrace = FLAG_TRACE_NULL;
//		UnSetRepeatAction();
//
//		UnSetRequestMode();
//	}	
//	//---------------------------------------------------------------
//	// otherwise.. it's an attack or a stop
//	//---------------------------------------------------------------
//	else
//	{
//		//---------------------------------------------------------------
//		// either a forced attack..
//		// decide whether to attack by looking at the race of the character being traced.
//		//---------------------------------------------------------------
//		bool bOusters = IsOusters();
//		MItem *pOustersItem = NULL;
//		if( bOusters) 
//			pOustersItem = g_pOustersGear->GetItem(MOustersGear::GEAR_OUSTERS_RIGHTHAND);
//
//		if (m_bTraceCreatureToForceAttack 
//			&& (bOusters && pOustersItem != NULL && pOustersItem->GetItemClass() == ITEM_CLASS_OUSTERS_CHAKRAM
//				|| !bOusters)
//			)
//		{
//			SetNextAction(GetActionInfoAction(m_nBasicActionInfo));
//
//			//--------------------
//			// message
//			//--------------------
//			DEBUG_ADD_FORMAT("Basic Action : %d", m_TraceID);					
//			
//			// 
//			//m_bTraceCreatureToForceAttack = false;
//		}
//		else
//		{
//			DEBUG_ADD_FORMAT("Basic Action : %d - But don't attack", m_TraceID);					
//			
//			// do not attack.
//			m_nUsedActionInfo = ACTIONINFO_NULL;
//			
//			//---------------------------------
//			// if tracing must continue..
//			//---------------------------------
//			if (m_bKeepTraceCreature)
//			{
//				return;
//			}
//		}
//	}
//	/*
//	#ifdef	CONNECT_SERVER
//		CGAttackNormal _CGAttackNormal;
//		_CGAttackNormal.setX( m_X );
//		_CGAttackNormal.setY( m_Y );
//		_CGAttackNormal.setDir( m_Direction );
//		_CGAttackNormal.setObjectID( m_TraceID );
//		g_pSocket->sendPacket( &_CGAttackNormal );
//
//		#ifdef	OUTPUT_DEBUG
//			DEBUG_ADD( _CGAttackNormal.toString().c_str() );							
//		#endif
//	#endif
//	*/	
//}

//----------------------------------------------------------------------
// Pickup Item
//----------------------------------------------------------------------
// Performs item pickup.
//----------------------------------------------------------------------
void
MPlayer::PickupItem(MItem* pItem)
{
	//----------------------------------------------------
	// when there is no item awaiting verification..
	// and no Item is currently held
	//----------------------------------------------------

	if( HasEffectStatusSummonSylph( dynamic_cast<MCreature*>(this) ) )
		return;

	if (IsItemCheckBufferNULL() 
		&& PlayerPresentation::GetPickUpItem()==NULL)
	{
		//----------------------------------------------------------------
		// position has been verified
		//----------------------------------------------------------------
		//if (m_SendMove==0)
		{
			// [ TEST CODE ]
			// read the Item info and play the pickup Sound.
			// this part later..
			// it might be better to do it
			// when the Server Packet clearly says "picked up".
			//MItem*	pItem = m_pZone->GetItem( m_TraceID );

			//g_Sound.Play( g_SoundTable[ g_ItemTable[pItem->GetItemType()].PickupSoundID ].pDSBuffer );
			//PlaySound( g_ItemTable[pItem->GetItemClass()][pItem->GetItemType()].TileSoundID ,
			//			false,
			//			m_X, m_Y);

			// don't compare item classes..
			// it seems better to put a virtual pickup() in the item class,
			// too lazy.. (sigh)

			//----------------------------------------------------------------
			//
			//	 if Motorcycle --> ride it
			//
			//----------------------------------------------------------------
			if (pItem->GetItemClass()==ITEM_CLASS_MOTORCYCLE)
			{	
				if (IsSlayer()
					&& m_CreatureType!=CREATURETYPE_SLAYER_OPERATOR)
				{
					RideMotorcycle( (MMotorcycle*)pItem );
				}
			}	
			
			//----------------------------------------------------------------
			//
			// if corpse --> Looting
			//
			//----------------------------------------------------------------
			else if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
			{				
				//----------------------------------------------------
				// while connected to the Server,
				//----------------------------------------------------
					MCreature* pCreature = ((MCorpse*)pItem)->GetCreature();

					if ( pCreature != NULL &&
						(
						pCreature->GetActionCount() >= pCreature->GetActionCountMax() || 
						pCreature->GetCreatureType() == 672 
						)
						)
					{	
						if (pItem->GetNumber()>0 || 
							(pCreature->GetCreatureType() >= 371 && pCreature->GetCreatureType() <= 376) || 
							(pCreature->GetCreatureType() >= 560 && pCreature->GetCreatureType() <= 563) || 
							(pCreature->GetCreatureType() >= 526 && pCreature->GetCreatureType() <= 549) ||
							pCreature->GetCreatureType() == 670 ||			// if it is a flagpole
							pCreature->GetCreatureType() == 672	||			// if it is a sweeper storage
							pCreature->GetCreatureType() == 673
							)
						{
							// send only if it is not a bat.
							// only when walking normally, not riding a motorcycle
							// when not petrified
							if (m_CreatureType!=CREATURETYPE_BAT
								&& m_MoveDevice==MOVE_DEVICE_WALK
								&& !m_bEffectStatus[EFFECTSTATUS_CURSE_PARALYSIS])
							{
								CGDissectionCorpse _CGDissectionCorpse;
								_CGDissectionCorpse.setX( static_cast<Coord_t>(pItem->GetX()) );
								_CGDissectionCorpse.setY( static_cast<Coord_t>(pItem->GetY()) );
								_CGDissectionCorpse.setObjectID( pItem->GetID() );

								g_pSocket->sendPacket( &_CGDissectionCorpse );

								if(pItem->GetNumber() > 0)
									pItem->SetNumber( pItem->GetNumber()-1 );
							}
						}
						// when there is no item..
						else
						{
							// if it is a wolf, eat the corpse
							// only if it is not a relic and not a guardian altar.
							if ( m_CreatureType==CREATURETYPE_WOLF && 
								!(
								(pCreature->GetCreatureType() >= 371 && pCreature->GetCreatureType() <= 376) || 
								pCreature->GetCreatureType() == 670	||			// if it is a flagpole
								pCreature->GetCreatureType() == 672 ||			// if it is a sweeper storage
								pCreature->GetCreatureType() == 673 ||
								(pCreature->GetCreatureType() >= 526 && pCreature->GetCreatureType() <= 549) || 
								(pCreature->GetCreatureType() >= 560 && pCreature->GetCreatureType() <= 563)
								)								
							   )
							{
								// set as the skill used
								m_nNextUsedActionInfo	= MAGIC_EAT_CORPSE;

								// set the trace info
								m_fTraceBuffer = m_fTrace = FLAG_TRACE_SECTOR_SPECIAL;
								m_TraceX	= pItem->GetX();
								m_TraceY	= pItem->GetY();
								m_TraceZ	= 0;

								m_TraceDistance		= GetActionInfoRange(m_nNextUsedActionInfo);

								SetAction( m_MoveAction );
								SetNextDestination(m_TraceX, m_TraceY);

								m_fNextTrace			= FLAG_TRACE_NULL;
								
								m_bKeepTraceCreature	= false;
							}							
						}						
					}
			}

			//----------------------------------------------------------------
			//
			// if money --> goes straight to the inventory..
			//
			//----------------------------------------------------------------
			else if (pItem->GetItemClass()==ITEM_CLASS_MONEY)
			{
				PickupMoney( (MMoney*)pItem );	
			}
			
			else //if (m_CreatureType!=CREATURETYPE_BAT || m_MoveDevice!=MOVE_DEVICE_WALK)
			{
				//----------------------------------------------------------------
				// an installed mine cannot be picked up.
				//----------------------------------------------------------------
				if (pItem->GetItemClass()==ITEM_CLASS_MINE)
				{
					if (((MMine*)pItem)->IsInstalled())
					{
						return;
					}
				}				
				//----------------------------------------------------------------
				//
				//	 if Event item
				//
				//----------------------------------------------------------------
//				else if (pItem->GetItemClass()==ITEM_CLASS_EVENT_GIFT_BOX)
//				{
//					// check whether an item of the same type exists.
//					if (NULL != g_pInventory->FindItem( ITEM_CLASS_EVENT_GIFT_BOX, pItem->GetItemType() ))
//					{
//						// if so, it cannot be picked up.
//						return;
//					}
//				}

				//----------------------------------------------------------------
				//
				//	if the Item can go into Gear..
				//
				//----------------------------------------------------------------
				if (pItem->IsGearItem())
				{					
					//------------------------------------------
					// if gear or Inventory is open --> to the mouse
					//------------------------------------------
					if (PlayerPresentation::GetGearOpenState() ||
						PlayerPresentation::GetInventoryOpenState())
					{							
						PickupItemToMouse( pItem );
					}
					//------------------------------------------
					// otherwise
					//------------------------------------------
					else
					{
						// try putting it in gear, and if it does not fit,
						// try the inventory.

						// if it fits in neither, it cannot be picked up.

						// for now.. (sigh)
						PickupItemToInventory( pItem );									
					}
				}
				//----------------------------------------------------------------
				//
				//	if the Item can go into the Inventory..
				//
				//----------------------------------------------------------------
				else if (pItem->IsInventoryItem())
				{												
					//----------------------------------------------------------------
					// if the inventory is open --> to the mouse
					//----------------------------------------------------------------
					if (PlayerPresentation::GetInventoryOpenState())
					{							
						PickupItemToMouse( pItem );
					}
					//----------------------------------------------------------------
					// otherwise straight into the inventory
					//----------------------------------------------------------------
					else
					{
						if (pItem->IsQuickItem()
							&& PickupItemToQuickslot( pItem ))
						{
							// this is the case where it can go into the quickslot, so that is fine.
							// meow..
						}
						else
						{
							PickupItemToInventory( pItem );
						}
					}	
				}
			}
		}
		//----------------------------------------------------------------
		// position has not been verified
		//----------------------------------------------------------------
		//DEBUG_ADD_FORMAT("Cannot Pickup item (Position not verified) sendMove=%d", m_SendMove);		
	}
	#ifdef OUTPUT_DEBUG
	else
	{
		if (PlayerPresentation::GetPickUpItem()!=NULL)
		{
			DEBUG_ADD_FORMAT("[UI] Already Handle a Item id=%d", PlayerPresentation::GetPickUpItem()->GetID());
		}
	}
	#endif
	
}

//----------------------------------------------------------------------
// Pickup Item to Inventory
//----------------------------------------------------------------------
// Picks up an Item from the Zone and puts it into the inventory.
//----------------------------------------------------------------------
void
MPlayer::PickupItemToInventory(MItem* pItem)
{
	//------------------------------------------
	// when there is room in the Inventory
	// : send a packet to be verified by the Server.
	//------------------------------------------
		POINT fitPoint;
		//------------------------------------------
		// when there is no item previously being processed..
		//------------------------------------------
		if (IsItemCheckBufferNULL())
		{
			if (g_pInventory->GetFitPosition(pItem, fitPoint) && 
				((pItem->IsSlayerItem() && IsSlayer()) || 
				(pItem->IsVampireItem() && IsVampire()) || 
				(pItem->IsOustersItem() && IsOusters())))
			{
				CGAddZoneToInventory _CGAddZoneToInventory;
				
				_CGAddZoneToInventory.setObjectID( pItem->GetID() );
				_CGAddZoneToInventory.setZoneX( static_cast<Coord_t>(pItem->GetX()) );
				_CGAddZoneToInventory.setZoneY( static_cast<Coord_t>(pItem->GetY()) );
				_CGAddZoneToInventory.setInvenX( static_cast<CoordInven_t>(fitPoint.x) );
				_CGAddZoneToInventory.setInvenY( static_cast<CoordInven_t>(fitPoint.y) );

				g_pSocket->sendPacket( &_CGAddZoneToInventory );

				// remember the item to pick up.
				pItem->SetGridXY( static_cast<BYTE>(fitPoint.x), static_cast<BYTE>(fitPoint.y) );
				SetItemCheckBuffer(pItem, ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY);											
			}
			else
			{
				//------------------------------------------------------------
				// this is the case where there is no suitable space in the inventory, so it cannot be picked up.
				//------------------------------------------------------------
				pItem->SetDropping();

				// 2004, 5, 7 , sobeit add start - if the inventory has no room, show a help message suggesting buying a storage box
				GameInfoPresentation::ExecuteHelpEvent( HELP_EVENT_STORAGE_BUY );
				// 2004, 5, 6, sobeit add end
			}
		}
		//------------------------------------------
		// because another item is still being processed,
		// this is the case where the item cannot be picked up.
		//------------------------------------------
		else
		{
			// Print message
			// "You cannot pick up the item yet."
		}
}

//----------------------------------------------------------------------
// Pickup Money
//----------------------------------------------------------------------
// Picks up an Item from the Zone.
//----------------------------------------------------------------------
void
MPlayer::PickupMoney(MMoney* pItem)
{
		//------------------------------------------
		// when there is no item previously being processed..
		//------------------------------------------
		if (IsItemCheckBufferNULL())
		{		
			CGPickupMoney _CGPickupMoney;
			
			_CGPickupMoney.setObjectID( pItem->GetID() );
			_CGPickupMoney.setZoneX( static_cast<Coord_t>(pItem->GetX()) );
			_CGPickupMoney.setZoneY( static_cast<Coord_t>(pItem->GetY()) );
		
			g_pSocket->sendPacket( &_CGPickupMoney );

			// remember the item to pick up.
			SetItemCheckBuffer(pItem, ITEM_CHECK_BUFFER_PICKUP_MONEY);											
		
		}
		//------------------------------------------
		// because another item is still being processed,
		// this is the case where the item cannot be picked up.
		//------------------------------------------
		else
		{
			// Print message
			// "You cannot pick up the item yet."
		}
}

//----------------------------------------------------------------------
// Pickup Item to Mouse
//----------------------------------------------------------------------
// Picks up an Item from the Zone and attaches it to the Mouse. (sigh)
//----------------------------------------------------------------------
void
MPlayer::PickupItemToMouse(MItem* pItem)
{
	//------------------------------------------
	// when connected to the Server..
	//------------------------------------------
	// the server must verify
	// whether the item can be picked up.
	//------------------------------------------
		//------------------------------------------
		// when there is no item previously being processed..
		//------------------------------------------
		if (IsItemCheckBufferNULL())
		{
			if((pItem->IsSlayerItem() && IsSlayer()) || 
				(pItem->IsVampireItem() && IsVampire()) ||
				(pItem->IsOustersItem() && IsOusters()))
			{
				CGAddZoneToMouse _CGAddZoneToMouse;
				
				_CGAddZoneToMouse.setObjectID( pItem->GetID() );
				_CGAddZoneToMouse.setZoneX( static_cast<Coord_t>(pItem->GetX()) );
				_CGAddZoneToMouse.setZoneY( static_cast<Coord_t>(pItem->GetY()) );
				
				g_pSocket->sendPacket( &_CGAddZoneToMouse );
				
				// remember the item to pick up.
				SetItemCheckBuffer(pItem, ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE);
			}
		}								
		//------------------------------------------
		// this is the case where the item cannot be picked up.
		//------------------------------------------
		else
		{
			// Print message
			// "You cannot pick up the item yet."
		}
}

//----------------------------------------------------------------------
// Pickup Item to Quickslot
//----------------------------------------------------------------------
// Picks up an Item from the Zone and attaches it to the Mouse. (sigh)
//----------------------------------------------------------------------
bool
MPlayer::PickupItemToQuickslot(MItem* pItem)
{
	//------------------------------------------------------------------
	// first, check whether it can go into the QuickSlot.
	//------------------------------------------------------------------
	if (CanPickupItemToQuickslot( GetRace(),
		g_pQuickSlot!=NULL,
		g_pArmsBand1!=NULL||g_pArmsBand2!=NULL,
		IsItemCheckBufferNULL(),
		g_pTempInformation->GetMode()==TempInformation::MODE_NULL,
		pItem ))
	{
		BOOL FirstArmsband = TRUE;
		int slot;

		//---------------------------------------------------------
		// where in the QuickSlot can it go?
		//---------------------------------------------------------
		if (IsSlayer() && !g_pQuickSlot->FindSlotToAddItem(pItem, slot))
		{
			return false;
		} 
		else if( IsOusters() )
		{
			// search the first one first, and if none, search the second.
			
			if( g_pArmsBand1 == NULL ||  !g_pArmsBand1->FindSlotToAddItem( pItem, slot ) )
			{
				if( g_pArmsBand2 == NULL || !g_pArmsBand2->FindSlotToAddItem( pItem,slot ) )
					return false;
				else FirstArmsband = FALSE;
			} else
			{
				FirstArmsband = TRUE;
			}
		}

			//---------------------------------------------------------
			// first it must be verified that it can go to the mouse...
			//---------------------------------------------------------
			CGAddZoneToMouse _CGAddZoneToMouse;

			_CGAddZoneToMouse.setObjectID( pItem->GetID() );
			_CGAddZoneToMouse.setZoneX( static_cast<Coord_t>(pItem->GetX()) );
			_CGAddZoneToMouse.setZoneY( static_cast<Coord_t>(pItem->GetY()) );

			g_pSocket->sendPacket( &_CGAddZoneToMouse );

			//---------------------------------------------------------
			// it can go into the slot.
			//---------------------------------------------------------
			g_pTempInformation->SetMode(TempInformation::MODE_TRADE_VERIFY_PICKUP_TO_QUICKSLOT);
			g_pTempInformation->Value1 = FirstArmsband ? slot : slot + 3;
			g_pTempInformation->pValue = (void*)pItem;

			SetItemCheckBuffer(pItem, ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT);


		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Pickup Item to Mouse
//----------------------------------------------------------------------
// Picks up an Item from the Zone and attaches it to the Mouse. (sigh)
//----------------------------------------------------------------------
void
MPlayer::RideMotorcycle(MMotorcycle* pMotorcycle)
{
	if (pMotorcycle==NULL)
	{
		DEBUG_ADD_ERR("[Error] Player::Motorcycle to ride is NULL");
		
		return;
	}

	//------------------------------------------
	// can ride a motorcycle while walking.
	//------------------------------------------
	if (m_MoveDevice==MOVE_DEVICE_WALK)
	{
		CGRideMotorCycle _CGRideMotorCycle;

		_CGRideMotorCycle.setObjectID( pMotorcycle->GetID() );
		_CGRideMotorCycle.setX( static_cast<Coord_t>(pMotorcycle->GetX()) );
		_CGRideMotorCycle.setY( static_cast<Coord_t>(pMotorcycle->GetY()) );

		g_pSocket->sendPacket( &_CGRideMotorCycle );

		// wait for verification to ride the Motorcycle.
		SetWaitVerify( WAIT_VERIFY_MOTORCYCLE_GETON );
	}
	//------------------------------------------
	// this is the case where a motorcycle is already being ridden.
	//------------------------------------------
	else
	{
		// cannot ride a motorcycle.
	}
}

//----------------------------------------------------------------------
// Skill To Inventory Item
//----------------------------------------------------------------------
// Uses a skill on an item in the Inventory.
// here it is enough to just start the casting motion.
//
// the result is handled by receiving a packet..
// the result is shown by (global) WorldEvents::AddNewInventoryEffect(...).
//----------------------------------------------------------------------
bool
MPlayer::TraceInventoryItem(TYPE_OBJECTID id)
{
	//-----------------------------------------------------
	// Must not have a delay.
	//-----------------------------------------------------
	if (!IsNotDelay()
		|| HasEffectStatus( EFFECTSTATUS_ETERNITY_PAUSE ))
	{
		return false;
	}

	//--------------------------------------------------------
	// when it cannot be used
	//--------------------------------------------------------
	if (m_nSpecialActionInfo == ACTIONINFO_NULL
		// Special attacks are not allowed while riding a motorcycle.
		|| m_MoveDevice == MOVE_DEVICE_RIDE
		// Cannot be used if some skill use is awaiting verification.
		//|| m_WaitVerify != WAIT_VERIFY_NULL)	// 2001.8.20 commented out
		)
	{
		return false;
	}
	
	//--------------------------------------------------------
	// When using another skill
	//--------------------------------------------------------
	if (m_nUsedActionInfo!=ACTIONINFO_NULL)
		return false;

	

	//--------------------------------------------------------
	// is the current skill really one used on items?
	//--------------------------------------------------------
	if ((*g_pActionInfoTable)[m_nSpecialActionInfo].IsTargetItem())
	{
		//-------------------------------------------------------
		// Check whether the skill is currently usable..
		// Disallow it if it is a Passive skill..
		//-------------------------------------------------------
		if (!g_pSkillAvailable->IsEnableSkill( (ACTIONINFO)m_nSpecialActionInfo )
			|| !(*g_pSkillInfoTable)[m_nSpecialActionInfo].IsEnable()
			|| !(*g_pSkillInfoTable)[m_nSpecialActionInfo].IsAvailableTime()
			|| (*g_pSkillInfoTable)[m_nSpecialActionInfo].IsPassive())
		{
			m_fNextTrace = FLAG_TRACE_NULL;

			UnSetRepeatAction();

			return false;
		}

		// can be used only while stationary.
		if (IsStop() && IsItemCheckBufferNULL()) // || m_Action!=ACTION_ATTACK)
		{
			//--------------------------------------------------------
			// check whether pItem is an item in the inventory.
			//--------------------------------------------------------
			const MItem* pItem = ((MItemManager)(*g_pInventory)).GetItem( id );
			
			if (pItem==NULL)
			{
				return false;
			}

			// Set as the skill used
			m_nNextUsedActionInfo	= m_nSpecialActionInfo;

			// Set trace info
			SetTraceID( id );
			m_fTrace	= FLAG_TRACE_INVENTORY;
			m_TraceX	= pItem->GetGridX();
			m_TraceY	= pItem->GetGridY();
			m_TraceZ	= 0;

			// Remember the target currently being traced.
			m_fTraceBuffer	= m_fTrace;	
			

			//------------------------------------------------------------		
			// meaningless for Inventory Items. (sigh)
			//------------------------------------------------------------	
			m_TraceDistance		= 1;		//(garbled) probably: if changed to 40, all are the same range
			SetAction( m_MoveAction );
			SetNextDestination(m_X, m_Y);

			// Remove buffering.
			m_fNextTrace			= FLAG_TRACE_NULL;
		
			// Stop continuing the chase
			m_bKeepTraceCreature	= false;

			//------------------------------------------------------------
			// Print message
			//------------------------------------------------------------
			DEBUG_ADD_FORMAT("Trace IteMM: %d", id);				
		}
		else
		{
			//------------------------------------------------------------
			// Buffer the next action when not in a repeat action
			//------------------------------------------------------------
			if (m_bRepeatAction)
			{
				m_fNextTrace	= FLAG_TRACE_NULL;

				return false;
			}
			else
			{
				m_fNextTrace	= FLAG_TRACE_INVENTORY;
				m_NextTraceID	= id;

				m_bRepeatAction = FALSE;

				return false;
			}
		}

		return true;
	}

	// means it is not a skill used on items
	return false;
}

//----------------------------------------------------------------------
// Change To Slayer
//----------------------------------------------------------------------
// Transforms into a slayer..
// if already a slayer.. return false
//----------------------------------------------------------------------
bool	
MPlayer::ChangeToSlayer()
{
	if (MCreature::ChangeToSlayer())
	{
		m_ConversionDelayTime = MonotonicClock::TimePoint();

		g_pCurrentMagazine = NULL;
		
		//-----------------------------------------------------
		// also remove the storage box.
		//-----------------------------------------------------
		if (g_pStorage!=NULL)
		{
			delete g_pStorage;
			g_pStorage = NULL;
		}

		//-----------------------------------------------------
		// flash
		//-----------------------------------------------------
		WorldEvents::SetFadeStart(1, 31, 6,  5,5,31, 0);

		ZoneLoading::ChangeInterfaceRace(RACE_SLAYER);

		SetWaitVerifyNULL();

		ResetSendMove();
		
		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Change To Vampire
//----------------------------------------------------------------------
// Transforms into a vampire..
// if already a vampire.. return false
//----------------------------------------------------------------------
bool
MPlayer::ChangeToVampire()
{
	if (MCreature::ChangeToVampire())
	{
		m_ConversionDelayTime = MonotonicClock::TimePoint();

		g_pCurrentMagazine = NULL;
		

		//-----------------------------------------------------
		// also remove the storage box.
		//-----------------------------------------------------
		if (g_pStorage!=NULL)
		{
			delete g_pStorage;
			g_pStorage = NULL;
		}

		WorldEvents::SetFadeStart(1, 31, 6,  31,0,0, 0);

		ZoneLoading::ChangeInterfaceRace(RACE_VAMPIRE);

		SetWaitVerifyNULL();

		ResetSendMove();

		DEBUG_ADD("MPlayer::Change to Vampire OK");
		
		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Set Status
//----------------------------------------------------------------------
// Changes the character's status values.
//----------------------------------------------------------------------
void	
MPlayer::SetStatus(DWORD n, DWORD value)
{ 
	if (n >= MODIFY_MAX)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] Modify Part is Wrong : part=%d, value=%d", n, value);
		
		return;
	}

	MODIFY_VALUE modifyValue;
	modifyValue.oldValue = m_Status[n];
	modifyValue.newValue = value;

	// first change the value
	m_Status[n] = value; 
	
	// handling related to the changed value
	g_pModifyStatusManager->Execute( n, (void*)&modifyValue );

	// again.. (sigh)
	m_Status[n] = modifyValue.newValue;
}

//----------------------------------------------------------------------
// Check Status
//----------------------------------------------------------------------
// Checks for changing values
//----------------------------------------------------------------------
void	
MPlayer::CalculateStatus()
{
	//-----------------------------------------------------------------
	// depending on the equipped items...
	//-----------------------------------------------------------------
	MItemManager* pGear = NULL;

	int weaponSpeed = 0;
	int weaponTohit = 0;

	switch(GetRace())
	{
	case RACE_SLAYER:
		{
			pGear = g_pSlayerGear;
			
			const MItem* pWeapon = g_pSlayerGear->GetItem( MSlayerGear::GEAR_SLAYER_RIGHTHAND );
			
			SKILLDOMAIN domain = MAX_SKILLDOMAIN;
			int domainLevel = 0;
			
			//------------------------------------------------------
			// check whether a gun is held
			//------------------------------------------------------
			if (pWeapon!=NULL && pWeapon->IsAffectStatus())
			{
				if (pWeapon->IsGunItem())
				{
					domain = SKILLDOMAIN_GUN;
					
					weaponTohit = pWeapon->GetToHit();
				}
				else if (pWeapon->GetItemClass()==ITEM_CLASS_SWORD)
				{
					domain = SKILLDOMAIN_SWORD;
				}
				else if (pWeapon->GetItemClass()==ITEM_CLASS_BLADE)
				{
					domain = SKILLDOMAIN_BLADE;
				}
				else if (pWeapon->GetItemClass()==ITEM_CLASS_CROSS)
				{
					domain = SKILLDOMAIN_HEAL;
				}
				else if (pWeapon->GetItemClass()==ITEM_CLASS_MACE)
				{
					domain = SKILLDOMAIN_ENCHANT;
				}
				
				if (domain < MAX_SKILLDOMAIN)
				{
					domainLevel = (*g_pSkillManager)[domain].GetDomainLevel();
				}
				
				
				//----------------------------------------------------
				// base speed of the weapon
				//----------------------------------------------------			
				if(pWeapon->IsAffectStatus())
					weaponSpeed = pWeapon->GetOriginalSpeed();
			}
			
			g_StatusManager.SetCurrentWeaponDomain( domain, domainLevel );
		}
		break;

	case RACE_VAMPIRE:
		{
			pGear = g_pVampireGear;
			
			const MItem* pWeapon = g_pVampireGear->GetItem( MVampireGear::GEAR_VAMPIRE_RIGHTHAND );
			
			if (pWeapon!=NULL)
			{
				//----------------------------------------------------
				// base speed of the weapon
				//----------------------------------------------------			
				if(pWeapon->IsAffectStatus())
					weaponSpeed = pWeapon->GetOriginalSpeed();
			}
			g_StatusManager.SetCurrentWeaponDomain( SKILLDOMAIN_VAMPIRE, GetLEVEL() );
		}
		break;

	case RACE_OUSTERS:
		{
			pGear = g_pOustersGear;
			
			const MItem* pWeapon = g_pOustersGear->GetItem( MOustersGear::GEAR_OUSTERS_RIGHTHAND );
			
			if (pWeapon!=NULL)
			{
				//----------------------------------------------------
				// base speed of the weapon
				//----------------------------------------------------			
				if(pWeapon->IsAffectStatus())
					weaponSpeed = pWeapon->GetOriginalSpeed();
			}
			g_StatusManager.SetCurrentWeaponDomain( SKILLDOMAIN_OUSTERS, GetLEVEL() );
		}
		break;
	default:
		break;
	}

	
	//-----------------------------------------------------------------
	// for all items in Gear..
	//
	// apply only the DAM, AC, TOHIT, CC options.
	//-----------------------------------------------------------------
	g_StatusManager.Set(GetSTR(), GetDEX(), GetINT());

	int DV		= g_StatusManager.GetDefense();
	int PV		= g_StatusManager.GetProtection();
	int TOHIT	= g_StatusManager.GetTOHIT() + weaponTohit;
	//int CC		= g_StatusManager.GetCC();
	int MinDAM	= m_Status[MODIFY_MIN_DAMAGE];	//g_StatusManager.GetMinDAM();
	int MaxDAM	= m_Status[MODIFY_MAX_DAMAGE];	//g_StatusManager.GetMaxDAM();
	int AttackSpeed = g_StatusManager.GetAttackSpeed() + weaponSpeed;

	if((*g_pRankBonusTable)[RANK_BONUS_HAWK_WING].GetStatus() == RankBonusInfo::STATUS_LEARNED)
		AttackSpeed += (*g_pRankBonusTable)[RANK_BONUS_HAWK_WING].GetPoint();

	if((*g_pRankBonusTable)[RANK_BONUS_CROW_WING].GetStatus() == RankBonusInfo::STATUS_LEARNED)
		AttackSpeed += (*g_pRankBonusTable)[RANK_BONUS_CROW_WING].GetPoint();
	
	if((*g_pRankBonusTable)[RANK_BONUS_SWIFT_ARM].GetStatus() == RankBonusInfo::STATUS_LEARNED)
		AttackSpeed += (*g_pRankBonusTable)[RANK_BONUS_SWIFT_ARM].GetPoint();

	// Pet Bonus
	if( GetPetID() != OBJECTID_NULL)
	{
		MFakeCreature *pFakeCreature = (MFakeCreature *)g_pZone->GetFakeCreature(GetPetID());
		if(pFakeCreature != NULL)
		{
			MPetItem *pPetItem = pFakeCreature->GetPetItem();
			if(pPetItem != NULL)
			{
				// The attribute as the number the server sent: it need not name a part.
				const int attr = pPetItem->GetEnchantLevel();
				if(attr == ITEMOPTION_TABLE::PART_ATTACK_SPEED)
				{
					AttackSpeed += pPetItem->GetSilver();
				}
			}
		}
	}

	int	ElementalFire = 0;
	int	ElementalWater = 0;
	int	ElementalEarth = 0;
	int	ElementalWind = 0;

	//-----------------------------------------------------------------
	// check whether items brighten the sight.
	//-----------------------------------------------------------------
	SetItemLightSight(0);

	pGear->SetBegin();

	while (pGear->IsNotEnd())
	{
		const MItem* pItem = pGear->Get();

		//----------------------------------------------------------
		// only for items to which values apply..
		//----------------------------------------------------------
		if (pItem != NULL && pItem->IsAffectStatus())
		{
			if(pItem->GetItemClass() == ITEM_CLASS_OUSTERS_WRISTLET || pItem->GetItemClass() == ITEM_CLASS_OUSTERS_STONE)
			{
				ITEMTABLE_INFO iInfo = (*g_pItemTable)[pItem->GetItemClass()][pItem->GetItemType()];
				if(iInfo.ElementalType == ITEMTABLE_INFO::ELEMENTAL_TYPE_FIRE)
					ElementalFire += iInfo.Elemental;
				else if(iInfo.ElementalType == ITEMTABLE_INFO::ELEMENTAL_TYPE_WATER)
					ElementalWater += iInfo.Elemental;
				else if(iInfo.ElementalType == ITEMTABLE_INFO::ELEMENTAL_TYPE_EARTH)
					ElementalEarth += iInfo.Elemental;
				else if(iInfo.ElementalType == ITEMTABLE_INFO::ELEMENTAL_TYPE_WIND)
					ElementalWind += iInfo.Elemental;

			}
			//---------------------------------------------------
			// basic defense increase
			//---------------------------------------------------
			int def = pItem->GetDefenseValue();
			
			if (def != -1)
			{
				DV += def;
			}

			int pro = pItem->GetProtectionValue();
			
			if (pro != -1)
			{
				PV += pro;
			}

			const std::list<TYPE_ITEM_OPTION>& optionList = pItem->GetItemOptionList();

			std::list<TYPE_ITEM_OPTION>::const_iterator optionListItr = optionList.begin();

			while(optionListItr != optionList.end())
			{
				
				const ITEMOPTION_INFO& optionInfo = (*g_pItemOptionTable)[*optionListItr];
				//---------------------------------------------------
				// additional Options
				//---------------------------------------------------
				switch ((ITEMOPTION_TABLE::ITEMOPTION_PART)optionInfo.Part)
				{
				case ITEMOPTION_TABLE::PART_DEFENSE		: DV += optionInfo.PlusPoint; break;
				case ITEMOPTION_TABLE::PART_TOHIT		: TOHIT += optionInfo.PlusPoint; break;
				case ITEMOPTION_TABLE::PART_PROTECTION	: PV += optionInfo.PlusPoint; break;
					//case ITEMOPTION_INFO::PART_DAMAGE		: MinDAM += optionInfo.PlusPoint; 
					//											MaxDAM += optionInfo.PlusPoint; 
					//break;
					
				case ITEMOPTION_TABLE::PART_ATTACK_SPEED : AttackSpeed += optionInfo.PlusPoint; break;
					
				case ITEMOPTION_TABLE::PART_VISION :	SetItemLightSight(GetItemLightSight() + optionInfo.PlusPoint); break;
				default: break;
				}
				optionListItr++;
			}

			std::list<TYPE_ITEM_OPTION>::const_iterator itr = pItem->GetItemDefaultOptionList().begin();

			while(itr != pItem->GetItemDefaultOptionList().end())
			{
				TYPE_ITEM_OPTION option = *itr;

				const ITEMOPTION_INFO& optionInfo = (*g_pItemOptionTable)[option];
				//---------------------------------------------------
				// additional Options
				//---------------------------------------------------
				switch ((ITEMOPTION_TABLE::ITEMOPTION_PART)optionInfo.Part)
				{
				case ITEMOPTION_TABLE::PART_DEFENSE		: DV += optionInfo.PlusPoint; break;
				case ITEMOPTION_TABLE::PART_TOHIT		: TOHIT += optionInfo.PlusPoint; break;
				case ITEMOPTION_TABLE::PART_PROTECTION	: PV += optionInfo.PlusPoint; break;
					//case ITEMOPTION_INFO::PART_DAMAGE		: MinDAM += optionInfo.PlusPoint; 
					//											MaxDAM += optionInfo.PlusPoint; 
					//break;
					
				case ITEMOPTION_TABLE::PART_ATTACK_SPEED : AttackSpeed += optionInfo.PlusPoint; break;
					
				case ITEMOPTION_TABLE::PART_VISION :	SetItemLightSight(GetItemLightSight() + optionInfo.PlusPoint); break;
				default: break;
				}

				itr++;
			}
		}

		pGear->Next();
	}	

	// 2004, 9, 20, sobeit add start - there is a rank skill that raises the spirit level
	if((*g_pRankBonusTable)[RANK_BONUS_SALAMANDERS_KNOWLEDGE].GetStatus() == RankBonusInfo::STATUS_LEARNED)
		ElementalFire ++;
	if((*g_pRankBonusTable)[RANK_BONUS_UNDINES_KNOWLEDGE].GetStatus() == RankBonusInfo::STATUS_LEARNED)
		ElementalWater ++;
	if((*g_pRankBonusTable)[RANK_BONUS_GNOMES_KNOWLEDGE].GetStatus() == RankBonusInfo::STATUS_LEARNED)
		ElementalEarth ++;
	// 2004, 9, 20, sobeit add end
	// set spirit value
	SetStatus(MODIFY_ELEMENTAL_FIRE, ElementalFire);
	SetStatus(MODIFY_ELEMENTAL_WATER, ElementalWater);
	SetStatus(MODIFY_ELEMENTAL_EARTH, ElementalEarth);
	SetStatus(MODIFY_ELEMENTAL_WIND, ElementalWind);
	if(IsOusters())
		g_pSkillAvailable->SetAvailableSkills();

	//-----------------------------------------------------------------
	// set weapon speed
	//-----------------------------------------------------------------
	SetStatus(MODIFY_ATTACK_SPEED, AttackSpeed);

	//-----------------------------------------------------------------
	// apply only the DAM, AC, TOHI, CC options.
	//-----------------------------------------------------------------
	// removed 2001.9.28
	//SetStatus(MODIFY_DEFENSE, DV);
	//SetStatus(MODIFY_PROTECTION, PV);
	//SetStatus(MODIFY_TOHIT, TOHIT);
//	SetStatus(MODIFY_CARRYWEIGHT, CC);			

	//-----------------------------------------------------------------
	// check DAM separately.. yum yum..
	//-----------------------------------------------------------------	
//	m_Status[MODIFY_MIN_DAMAGE] = MinDAM; 
//	m_Status[MODIFY_MAX_DAMAGE] = MaxDAM; 
	int maxDAM = 0, minDAM = 0;
	int maxSilverDAM = 0, minSilverDAM = 0;

	//-----------------------------------------------------------------
	// if Slayer --> apply weapon damage
	//-----------------------------------------------------------------
	switch(GetRace())
	{
		case RACE_SLAYER:
		{
			const MItem* pWeapon = g_pSlayerGear->GetItem( MSlayerGear::GEAR_SLAYER_RIGHTHAND );

			if (pWeapon==NULL || !pWeapon->IsAffectStatus())
			{
				minDAM = MinDAM;
				maxDAM = MaxDAM;
			}
			else //if (pWeapon->IsAffectStatus())
			{
				minDAM = MinDAM + pWeapon->GetMinDamage();
				maxDAM = MaxDAM + pWeapon->GetMaxDamage();		

				//-----------------------------------------------------------------
				// for a cross or mace : silver damage +10%
				//-----------------------------------------------------------------
				if (pWeapon->GetItemClass()==ITEM_CLASS_MACE
					|| pWeapon->GetItemClass()==ITEM_CLASS_CROSS)
				{
					minSilverDAM = pWeapon->GetMinDamage() / 10;
					maxSilverDAM = pWeapon->GetMaxDamage() / 10;
				}

				//-----------------------------------------------------------------
				// if silver-plated : silver damage +10%
				// if a gun, check for silver bullets
				//-----------------------------------------------------------------
				if ((!pWeapon->IsGunItem() && pWeapon->GetSilver() > 0)
					|| (pWeapon->IsGunItem() 
						&& g_pCurrentMagazine!=NULL 
						&& g_pCurrentMagazine->GetItemType()>=8
						&& g_pCurrentMagazine->GetItemType()<=15))
				{
					minSilverDAM += pWeapon->GetMinDamage() / 10;
					maxSilverDAM += pWeapon->GetMaxDamage() / 10;
				}
			}
		}
	break;

	case RACE_VAMPIRE:
		//-----------------------------------------------------------------
		// vampire..
		//-----------------------------------------------------------------
		{
			const MItem* pWeapon = g_pVampireGear->GetItem( MVampireGear::GEAR_VAMPIRE_RIGHTHAND );

			if (pWeapon==NULL || !pWeapon->IsAffectStatus())
			{
				minDAM = MinDAM;
				maxDAM = MaxDAM;
			}
			else //if (pWeapon->IsAffectStatus())
			{
				minDAM = MinDAM + pWeapon->GetMinDamage();
				maxDAM = MaxDAM + pWeapon->GetMaxDamage();		

			}
		}
		break;

	case RACE_OUSTERS:
		//-----------------------------------------------------------------
		// ousters..
		//-----------------------------------------------------------------
		{
			const MItem* pWeapon = g_pOustersGear->GetItem( MOustersGear::GEAR_OUSTERS_RIGHTHAND );
			
			if (pWeapon==NULL || !pWeapon->IsAffectStatus() || pWeapon->GetItemClass() == ITEM_CLASS_OUSTERS_WRISTLET)
			{
				minDAM = MinDAM;
				maxDAM = MaxDAM;
			}
			else //if (pWeapon->IsAffectStatus())
			{
				minDAM = MinDAM + pWeapon->GetMinDamage();
				maxDAM = MaxDAM + pWeapon->GetMaxDamage();		
				
			}
		}
		break;
	default:
		break;
	}

	PlayerPresentation::SetCharInfoDAM( maxDAM, minDAM );
	
	PlayerPresentation::SetSilverDamage( minSilverDAM, maxSilverDAM );

	// during the game the server sends it, so..
	if (g_Mode == MODE_WAIT_PCLIST)
	{
		StatusPresentation::SetCharInfoDefense( DV );
		StatusPresentation::SetCharInfoProtection( PV );
		StatusPresentation::SetCharInfoTOHIT( TOHIT );
	}
	else
	{
		StatusPresentation::SetCharInfoDefense( GetDefense() );
		StatusPresentation::SetCharInfoProtection( GetProtection() );
		StatusPresentation::SetCharInfoTOHIT( GetTOHIT() );
	}
}

//----------------------------------------------------------------------
// Fast Move Position
//----------------------------------------------------------------------
// (m_X, m_Y) --> (x, y)
//----------------------------------------------------------------------
bool
MPlayer::FastMovePosition(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, bool server)
{	
	if (WorldEvents::ViewExists())
	{
		WorldEvents::SetSelectedSectorNULL();
	}

	if (MCreature::FastMovePosition( x, y ))
	{
		if(server == true)
			SetServerPosition( x, y);
		// 2001.8.8  commented out to try continuing the trace
		// but.. there was some problem, so again... (sigh)
		//m_nUsedActionInfo = ACTIONINFO_NULL;
		//m_Action		= (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND;

		///*
		m_fNextTrace = FLAG_TRACE_NULL;

		m_bKeepTraceCreature = false;
		m_Action		= (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND;

		UnSetRepeatAction();
		//*/

		// consider everything verified.
		ResetSendMove();

		// added on 2001.8.10
		m_nUsedActionInfo = ACTIONINFO_NULL;

		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// KnockBackPosition
//----------------------------------------------------------------------
bool	
MPlayer::KnockBackPosition(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, BYTE Action)
{
	if (MCreature::KnockBackPosition( x, y, Action ))
	{
		// 2001.8.8  commented out to try continuing the trace
		// but.. there was some problem, so again... (sigh)
		//m_nUsedActionInfo = ACTIONINFO_NULL;
		//m_Action		= (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND;

		///*
		//m_fNextTrace = FLAG_TRACE_NULL;

		//m_bKeepTraceCreature = false;
		//m_Action		= (m_MoveAction==ACTION_SLAYER_MOTOR_MOVE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND;

		//UnSetRepeatAction();
		//*/

		// consider everything verified.
		ResetSendMove();

		// added on 2001.8.10
		m_nUsedActionInfo = ACTIONINFO_NULL;

		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Find Enemy
//----------------------------------------------------------------------
// Finds a nearby enemy.
// If there is no enemy, returns OBJECTID_NULL.
//----------------------------------------------------------------------
int		
MPlayer::FindEnemy()
{
	//---------------------------------------------------------------
	// decides the LockMode action based on whether the L and R Buttons are currently pressed.
	// not very clean.. heh..
	//---------------------------------------------------------------
	int bBasicAction = g_bLButtonDown;
	int bSpecialAction = g_bRButtonDown;
	int actionDistance = 0;

	//---------------------------------------------------------------
	// basic action
	//-------------------------------------------------------------
	if (bBasicAction)
	{
		actionDistance = m_BasicAttackDistance;		
	}
	//-------------------------------------------------------------
	// special action
	//-------------------------------------------------------------
	else if (bSpecialAction)
	{
		// when no special skill is set
		if (m_nSpecialActionInfo==ACTIONINFO_NULL)
		{
			return OBJECTID_NULL;
		}

		actionDistance = GetActionInfoRange( m_nSpecialActionInfo );
	}

	//-------------------------------------------------------------
	// search all Creatures in the Zone.
	//-------------------------------------------------------------
	int	numCreature = g_pZone->GetCreatureNumber();

	MZone::CREATURE_MAP::const_iterator iCreature = g_pZone->GetCreatureBegin();
		
	int targetCreatureID = OBJECTID_NULL;

	for (int i=0; i<numCreature; i++)
	{
		MCreature* pCreature = iCreature->second;

		//--------------------------------------------------
		// if it is neither an NPC nor myself..
		//--------------------------------------------------
		if (!pCreature->IsNPC() 
			&& pCreature->GetID()!=m_ID)
		{
			int tx = pCreature->GetServerX();
			int ty = pCreature->GetServerY();

			//--------------------------------------------------
			// if it is an enemy..
			// and within action range...
			//--------------------------------------------------
			if (g_pObjectSelector->CanAttack(pCreature)
				&& max(abs(tx-m_X), abs(ty-m_Y)) <= actionDistance)
				//&& g_pObjectSelector->IsAblePKbyZone(pCreature))					
			{
				//--------------------------------------------------
				// if it is the first selected character
				//--------------------------------------------------
				if (static_cast<TYPE_OBJECTID>(targetCreatureID)==OBJECTID_NULL)
				{
					targetCreatureID = pCreature->GetID();
				}
				//--------------------------------------------------
				// if it is not the first selected character
				//--------------------------------------------------
				else
				{
					//--------------------------------------------------
					// re-pick the target with 50% probability.
					//--------------------------------------------------
					if (rand()%2)
					{
						targetCreatureID = pCreature->GetID();	
					}
					//--------------------------------------------------
					// if it was not re-picked
					// select the previously set character.
					//--------------------------------------------------
					else
					{
						return targetCreatureID;
					}
				}
			}
		}

		iCreature++;
	}	

	return targetCreatureID;
}

//----------------------------------------------------------------------
// Stop Blood Drain
//----------------------------------------------------------------------
// Call this when m_bStopBloodDrain is set.
//----------------------------------------------------------------------
void	
MPlayer::StopBloodDrain()
{
	if (m_Action==ACTION_VAMPIRE_DRAIN
		&& IsVampire()
		&& m_ActionCountMax>60)
	{
		if (m_ActionCount>9 && m_ActionCount<60)
		{
			m_ActionCount = 60;

			// reset delay
			//m_DelayTime	= g_CurrentTime + (m_ActionCountMax-m_ActionCount)*60;		
			
			UnSetStopBloodDrain();
//			char TempBuffer[128];
//			sprintf(TempBuffer, "StopBD: D:%d, m:%d, C:%d", m_DelayTime,m_bStopBloodDrain, m_ActionCount);
//			g_pSystemMessage->Add(TempBuffer);

			//DEBUG_ADD_FORMAT("[Player] StopBloodDrain1(%d)", m_bStopBloodDrain);

		}	
	}
	else
	{
		//UnSetStopBloodDrain();
		//DEBUG_ADD_FORMAT("[Player] StopBloodDrain2(%d), action=%d(%d)", m_bStopBloodDrain, m_Action, m_ActionCountMax);
	}
}

//----------------------------------------------------------------------
// Stop Absorb Soul
//----------------------------------------------------------------------
// Call this when m_bStopAbsorbSoul is set.
//----------------------------------------------------------------------
void	
MPlayer::StopAbsorbSoul()
{
	if (m_Action==ACTION_OUSTERS_DRAIN
		&& IsOusters()
		&& m_ActionCountMax>30)
	{
		if (m_ActionCount>9 && m_ActionCount<30)
		{
			m_ActionCount = 30;
			
			ATTACHEFFECT_LIST::iterator itr = m_listEffect.begin();
			ATTACHEFFECT_LIST::iterator endItr = m_listEffect.end();
			
			while(itr != endItr)
			{
				MAttachEffect*	pEffect = *itr;
				
				if(pEffect->GetEffectSpriteType() == EFFECTSPRITETYPE_ABSORB_SOUL)
				{
					bool bUseEffectSprite = pEffect->IsEffectSprite();
					
					// remove flag
					if (bUseEffectSprite)
					{
						m_bAttachEffect[pEffect->GetEffectSpriteType()] = false;	// remove flag
					}
					
					delete pEffect;
					m_listEffect.erase(itr);
					break;
				}
				itr++;
			}
			
			UnSetStopAbsorbSoul();
		}	
	}
	else
	{
		//UnSetStopBloodDrain();
	}
}

//----------------------------------------------------------------------
// Check In Darkness
//----------------------------------------------------------------------
// the moment darkness appears/disappears
//            can be checked at the moment the player moves.
//----------------------------------------------------------------------
void
MPlayer::CheckInDarkness()
{
	if (g_pZone!=NULL && 
		( (!IsVampire() && (!HasEffectStatus( EFFECTSTATUS_LIGHTNESS)|| g_pZone->GetID() == 3001 )) || 
		(g_pZone->GetID() == 3001 && IsVampire()))
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		&& !HasEffectStatus( EFFECTSTATUS_INSTALL_TURRET)
		)
	{
		//-------------------------------------------------------
		// check whether it entered Darkness
		//-------------------------------------------------------
		// the current coordinates look better than the server coordinates.. heh..
		const MSector& sector = g_pZone->GetSector( m_X, m_Y );

		if (sector.HasDarkness())
		{
			// when in darkness
			PlaceInDarkness();
		}
		else
		{		
			// the darkness has disappeared.
			PlaceNotInDarkness();
		}
	}
	else
	{
		PlaceNotInDarkness();
	}
}

//----------------------------------------------------------------------
// Set SpecialActionInfo
//----------------------------------------------------------------------
void			
MPlayer::SetSpecialActionInfo( TYPE_ACTIONINFO n )	
{ 
	if (g_pActionInfoTable!=NULL 
		&& n<g_pActionInfoTable->GetMinResultActionInfo())
	{
		m_nSpecialActionInfo = n; 

		// during repetition..
		// the skill's Target may change.
		if (IsRepeatAction())
		{
			if (m_fTraceBuffer != FLAG_TRACE_NULL
				// does not matter during the basic action.
				&& !(m_fTraceBuffer & FLAG_TRACE_CREATURE_BASIC)
				&& !(m_fTraceBuffer & FLAG_TRACE_SECTOR_BASIC)
				&& !(m_fTraceBuffer & FLAG_TRACE_ITEM)
				&& !(m_fTraceBuffer & FLAG_TRACE_INTERACTIONOBJECT)
				&& !(m_fTraceBuffer & FLAG_TRACE_EFFECT)
				)
			{			
				// when a skill is being used..
				BOOL bOther = ((*g_pActionInfoTable)[n].IsTargetOther());
				BOOL bSelf = ((*g_pActionInfoTable)[n].IsTargetSelf());
				BOOL bZone = ((*g_pActionInfoTable)[n].IsTargetZone());
				BOOL bItem = ((*g_pActionInfoTable)[n].IsTargetItem());

				// normal case...
				if (((m_fTraceBuffer & FLAG_TRACE_SELF) && bSelf)
					|| ((m_fTraceBuffer & FLAG_TRACE_CREATURE_SPECIAL) && bOther)
					|| ((m_fTraceBuffer & FLAG_TRACE_SECTOR_SPECIAL) && bZone)
					|| ((m_fTraceBuffer & FLAG_TRACE_INVENTORY) && bItem)
					)
				{				
				}
				// abnormal case.. the target setting must be changed.
				else
				{
					if (bOther)
					{
						m_fTraceBuffer = FLAG_TRACE_CREATURE_SPECIAL;
					}
					else if (bSelf)
					{
						m_fTraceBuffer = FLAG_TRACE_SELF;
					}
					else if (bZone)
					{
						m_fTraceBuffer = FLAG_TRACE_SECTOR_SPECIAL;
					}
					else if (bItem)
					{
						m_fTraceBuffer = FLAG_TRACE_INVENTORY;
					}
				}
			}		
		}
	}
	
}

//----------------------------------------------------------------------
// Is In SafeSector
//----------------------------------------------------------------------
BOOL
MPlayer::IsInSafeSector() const
{
	if(g_pWarManager->IsExist(m_pZone->GetID()))
		return FALSE;

	if (m_pZone!=NULL)
	{
		if(m_pZone->GetSector(m_X, m_Y).IsSafeComplete() || g_bZoneSafe)
			return 2;

		if (IsSlayer())
		{
			return (m_pZone->GetSector(m_X, m_Y).IsSafeSlayer())?1:0;
		}
		else
		if (IsVampire())
		{
			return (m_pZone->GetSector(m_X, m_Y).IsSafeVampire())?1:0;
		} else
		if (IsOusters() )
		{
			return (m_pZone->GetSector(m_X, m_Y).IsSafeOusters())?1:0;
		}
	}

	return FALSE;
}

//-------------------------------------------------------------------------
// Soul Chain File
//-------------------------------------------------------------------------
void	
MPlayer::SetCannotTrace()
{
	PlayerPresentation::SetCannotTrace();
}

void	MPlayer::SetTimeLightSight(char s)
{
	m_TimeLightSight = s; 
	CalculateLightSight();
}

void	MPlayer::SetItemLightSight(int s)
{
	m_ItemLightSight = s; 
	CalculateLightSight();
}

void	MPlayer::CalculateLightSight()
{
//	CalculateSight();
	m_LightSight = GetTimeLightSight() + GetItemLightSight();

	// [new skill 6] observing eye widens the sight when active.
	if (m_bEffectStatus[EFFECTSTATUS_OBSERVING_EYE])
	{
		m_LightSight +=  1 + (*g_pSkillInfoTable)[SKILL_OBSERVING_EYE].GetLevel() / 50;
	}
	// Pet Bonus
	if( GetPetID() != OBJECTID_NULL)
	{
		MFakeCreature *pFakeCreature = (MFakeCreature *)g_pZone->GetFakeCreature(GetPetID());
		if(pFakeCreature != NULL)
		{
			MPetItem *pPetItem = pFakeCreature->GetPetItem();
			if(pPetItem != NULL)
			{
				// The attribute as the number the server sent: it need not name a part.
				const int attr = pPetItem->GetEnchantLevel();
				if(attr == ITEMOPTION_TABLE::PART_VISION)
				{
					m_LightSight += pPetItem->GetSilver();
				}
			}
		}
	}
	
	if(g_pSkillAvailable->IsEnableSkill(SKILL_HOLYLAND_BLOOD_BIBLE_GREGORI))
	{
		m_LightSight += 5;
	}
	
	if(g_pSkillAvailable->IsEnableSkill(SKILL_SWEEPER_BONUS_12))
	{
		m_LightSight += 5;
	}

	if(IsFlyingCreature())
	{
		m_LightSight += 3;
	}
	// 2004, 6, 24, sobeit add start
	if(HasEffectStatus(EFFECTSTATUS_BLINDNESS))
	{
		//m_LightSight = 1;
		PlaceInDarkness(true);
	}
	// 2004, 6, 24, sobeit add end
}

void	MPlayer::SetUndergroundCreature()
{
	MCreature::SetUndergroundCreature();
	CalculateLightSight();
}

void	MPlayer::SetGroundCreature()
{
	MCreature::SetGroundCreature();
	CalculateLightSight();
}

void	MPlayer::SetFlyingCreature()
{
	MCreature::SetFlyingCreature();
	CalculateLightSight();
}

void	MPlayer::CalculateSight()
{
	// (sigh) setting the sight, heh, too many bugs so just write it from scratch

	//-----------------------------------------------------------------
	// depending on the equipped items...
	//-----------------------------------------------------------------
	MItemManager* pGear = GetGear();

	//-----------------------------------------------------------------
	// if Slayer
	//-----------------------------------------------------------------	
	if(pGear == NULL )
		return;

	SetItemLightSight(0);

	pGear->SetBegin();

	while (pGear->IsNotEnd())
	{
		const MItem* pItem = pGear->Get();

		//----------------------------------------------------------
		// only for items to which values apply..
		//----------------------------------------------------------
		if (pItem->IsAffectStatus())
		{	
			const std::list<TYPE_ITEM_OPTION>& optionList = pItem->GetItemOptionList();
			std::list<TYPE_ITEM_OPTION>::const_iterator optionListItr = optionList.begin();
			while(optionListItr != optionList.end())
			{				
				const ITEMOPTION_INFO& optionInfo = (*g_pItemOptionTable)[*optionListItr];
				//---------------------------------------------------
				// additional Options
				//---------------------------------------------------
				switch ((ITEMOPTION_TABLE::ITEMOPTION_PART)optionInfo.Part)
				{
				case ITEMOPTION_TABLE::PART_VISION :	
					SetItemLightSight(GetItemLightSight() + optionInfo.PlusPoint); 
					break;
				default:
					break;
				}
				optionListItr++;
			}

			std::list<TYPE_ITEM_OPTION>::const_iterator itr = pItem->GetItemDefaultOptionList().begin();

			while(itr != pItem->GetItemDefaultOptionList().end())
			{
				TYPE_ITEM_OPTION option = *itr;
				const ITEMOPTION_INFO& optionInfo = (*g_pItemOptionTable)[option];
				//---------------------------------------------------
				// additional Options
				//---------------------------------------------------
				switch ((ITEMOPTION_TABLE::ITEMOPTION_PART)optionInfo.Part)
				{
				case ITEMOPTION_TABLE::PART_VISION :	
					SetItemLightSight(GetItemLightSight() + optionInfo.PlusPoint); 
					break;
				default:
					break;
				}
				itr++;
			}
		}
		pGear->Next();
	}
	if( HasEffectStatus(EFFECTSTATUS_LIGHTNESS) 
		|| g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		|| g_bLight
#endif
		)
	{
		// the strongest~~
		SetItemLightSight( GetItemLightSight() + 15 );
	} else
	{
		if( (HasEffectStatus( EFFECTSTATUS_FLARE ) && IsVampire()) || 
			(HasEffectStatus( EFFECTSTATUS_YELLOW_POISON ) && IsSlayer()) ||
			HasEffectStatus( EFFECTSTATUS_YELLOW_POISON_TO_CREATURE ) )//||
//			HasEffectStatus( EFFECTSTATUS_BLINDNESS ))
			SetItemLightSight( 1 );
	}
}
// 2004, 8, 30, sobeit add start - sweep vice test
void
MPlayer::UpdateSweepVice()
{
	if(GetSweepViewValue())	// when in Sweep Vice
	{
		//if(IsDead() || m_nUsedActionInfo != SKILL_SWEEP_VICE_1 || m_SweepViceX == SECTORPOSITION_NULL ||	m_SweepViceY == SECTORPOSITION_NULL	)
		if(IsDead() ||!IsSlayer() || m_SweepViceX == SECTORPOSITION_NULL ||	m_SweepViceY == SECTORPOSITION_NULL	)
		{
			g_pZone->RemoveSwapViceType();
			SetSweepViewValue(0);
			return;
		}
		POINT CurrentPos = WorldEvents::GetSelectedSector(g_x, g_y);
		int TempVal = 0;
		int Distance = max(abs(m_TempSelectPosX - CurrentPos.x),abs(m_TempSelectPosY - CurrentPos.y));

		if(Distance<2)
		{
			TempVal = 1;
			if(GetSweepViewValue()!=TempVal)
			{
				g_pZone->ChangeSwapViceType(m_SweepViceX, m_SweepViceY, EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_1);
				WorldEvents::PlaySound( SOUND_SLAYER_SWEEPVICE_RESIZE );
			}
				
		}
		else if(Distance<3)
		{
			TempVal = 2;
			if(GetSweepViewValue()!=TempVal)
			{
				g_pZone->ChangeSwapViceType(m_SweepViceX, m_SweepViceY, EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_2);
				WorldEvents::PlaySound( SOUND_SLAYER_SWEEPVICE_RESIZE );
			}
		}
		else
		{
			TempVal = 3;
			if(GetSweepViewValue()!=TempVal)
			{
				g_pZone->ChangeSwapViceType(m_SweepViceX, m_SweepViceY, EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_3);
				WorldEvents::PlaySound( SOUND_SLAYER_SWEEPVICE_RESIZE );
			}
		}
		SetSweepViewValue(TempVal);
	}
}

bool 
MPlayer::CheckRbuttonUpSkill()
{
	if(HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
	{
		m_fNextTrace = FLAG_TRACE_NULL;
		UnSetRepeatAction();
		InstallTurretStopAttack();
	}
	if(GetSweepViewValue())
	{
		g_pZone->RemoveSwapViceType();
		m_nUsedActionInfo = SKILL_SWEEP_VICE_1;
		ActionEffect();
		SetSweepViewValue(0);
		m_nUsedActionInfo = SKILL_SWEEP_VICE_1;
		m_TempSelectPosX = SECTORPOSITION_NULL;
		m_TempSelectPosY = SECTORPOSITION_NULL;
		m_SweepViceX = SECTORPOSITION_NULL;
		m_SweepViceY = SECTORPOSITION_NULL;

		return true;
	}
//	else if(IsBurningSol())
//	{
//		return true;
//	}
	return false;
}
// 2004, 8, 30, sobeit add end - sweep vice test



bool MPlayer::UpdateBurningSol()
{
	if(m_bBurningSol)
	{
		if(IsDead()||!IsSlayer())
		{
			SetBurningSol(0);
			StopBurningSol();
			SetStop();
			m_nUsedActionInfo = ACTIONINFO_NULL;
			m_fNextTrace = FLAG_TRACE_NULL;
			UnSetRepeatAction();
		//	m_ActionCount = m_ActionCountMax;
			SetAction(ACTION_DIE);
			return false ;
		}
		else if(HasEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS)
			|| m_MoveDevice == MOVE_DEVICE_RIDE)
		{
			SetBurningSol(0);
			StopBurningSol();
			//SetStop();
			m_nUsedActionInfo = ACTIONINFO_NULL;
			m_fNextTrace = FLAG_TRACE_NULL;
			UnSetRepeatAction();
			RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1);
			RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2);
			RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_3);
			RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_4);
			return false;
		}
		if(m_TempSelectPosX == SECTORPOSITION_NULL || m_TempSelectPosY == SECTORPOSITION_NULL)
		{
			//SetBurningSol(0);
			StopBurningSol();
			m_nUsedActionInfo = ACTIONINFO_NULL;
			m_fNextTrace = FLAG_TRACE_NULL;
//			UnSetRepeatAction();
			//m_ActionCount = m_ActionCountMax;
			SetAction(ACTION_STAND);
			return false;
		}
		int bCheckFrame = 0;
		if(!IsAdvancementClass())
			bCheckFrame = IsMale()?7:11;
		else
			bCheckFrame = 7;

		m_bBurningSolCount++;

		//if(g_bRButtonDown)
		if(HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1)||
			HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2)||
			HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_3)||
			HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_4))
		{
			if(m_bBurningSolCount>=bCheckFrame)
				m_bBurningSolCount = bCheckFrame;
		}
		POINT  TempPos = WorldEvents::GetSelectedSector(g_x, g_y);
		m_TempSelectPosX = static_cast<TYPE_SECTORPOSITION>(TempPos.x);
		m_TempSelectPosY = static_cast<TYPE_SECTORPOSITION>(TempPos.y);
		m_Direction = SelectFacingDirection(g_pPlayer->GetX(), g_pPlayer->GetY(),m_TempSelectPosX, m_TempSelectPosY);
		ChangeNearDirection();
		//else
		if(!g_bRButtonDown)
		{
			if(m_bBurningSolCount>=bCheckFrame  )
			{
//				POINT  TempPos = g_pTopView->GetSelectedSector(g_x, g_y);
//				m_TempSelectPosX = TempPos.x;
//				m_TempSelectPosY = TempPos.y;
//				m_CurrentDirection = SelectFacingDirection(g_pPlayer->GetX(), g_pPlayer->GetY(),m_TempSelectPosX, m_TempSelectPosY);
//				

				bool ShowLunchEffect = true;
				int TempDistance = GetActionInfoRange(SKILL_BURNING_SOUL_LAUNCH);
				if (max(abs(m_X-m_TempSelectPosX), abs(m_Y-m_TempSelectPosY)) > TempDistance)
				{
					ShowLunchEffect = false;
				}
				
				if(HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1))
				{
					if(ShowLunchEffect)
						WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_BURNING_SOL_1,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),
								m_TempSelectPosX,m_TempSelectPosY, 0, 10, NULL, false);	
					RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1);
				}
				else if(HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2))
				{
					if(ShowLunchEffect)
						WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_BURNING_SOL_2,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),
								m_TempSelectPosX, m_TempSelectPosY, 0, 10, NULL, false);	
					RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2);
				}
				else if(HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_3))
				{
					if(ShowLunchEffect)
						WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_BURNING_SOL_3,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),
								m_TempSelectPosX, m_TempSelectPosY, 0, 10, NULL, false);	
					RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_3);
				}
				else if(HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_4))
				{
					if(ShowLunchEffect)
						WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_BURNING_SOL_4,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),
								m_TempSelectPosX, m_TempSelectPosY, 0, 10, NULL, false);	
					RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_4);
				}

				m_nUsedActionInfo = SKILL_BURNING_SOUL_LAUNCH;
				m_fTraceBuffer = FLAG_TRACE_SECTOR_SPECIAL;
				ActionToSendPacket();
				m_nNextUsedActionInfo = ACTIONINFO_NULL;
				//SetBurningSol(0);
				StopBurningSol();
				
				UnSetRepeatAction();
				m_TempSelectPosX = SECTORPOSITION_NULL;
				m_TempSelectPosY = SECTORPOSITION_NULL;
			}
		}
		m_ActionCount = m_bBurningSolCount;
			
		return true;
	#ifdef OUTPUT_DEBUG					
		DEBUG_ADD_FORMAT("[UpdateBurningSol] ActionCount  : %d", m_bBurningSolCount);
	#endif
	}
	return false;
}


bool MPlayer::UpDateInstallTurret()
{
	if(HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
	{
		if(!IsNotDelay())
		{
			m_Action = ACTION_STAND;
			//m_CurrentDirection = 2; 
			return true;
		}
		if(IsAlive() && (m_Action == ACTION_STAND))
		{
			if (m_bInstallTurretDirect != m_CurrentDirection)
			{
				if( ((m_bInstallTurretDirect - m_CurrentDirection + 8)%8) < 4 )
				{
					m_bInstallTurretCount = ((m_bInstallTurretCount-1)+16)%16;
				}
				else
				{
					m_bInstallTurretCount = (m_bInstallTurretCount+1)%16;
				}

				if(!(m_bInstallTurretCount&0x01))
					m_bInstallTurretDirect = ((m_bInstallTurretCount /2) +2 )%8;
			}
			return true;
		}

		
		SetInstallTurretDirect(m_CurrentDirection);
		SetInstallTurretCount(((m_CurrentDirection+6)%8)*2);
	}
	return false;
}
// 2005, 1, 4, sobeit add start
void	
MPlayer::UseWildWolf_Corpse(MItem *pItem)
{
	if(pItem == NULL) return;
	m_TraceDistance = GetActionInfoRange(SKILL_WILD_WOLF);
	if (max(abs(m_X-pItem->GetX()), abs(m_Y-pItem->GetY())) <= m_TraceDistance && g_pSkillAvailable->IsEnableSkill( SKILL_WILD_WOLF ))
	{
		SetAction( ACTION_MAGIC );
		CGSkillToObject _CGSkillToObject;
		_CGSkillToObject.setSkillType( SKILL_WILD_WOLF );
		_CGSkillToObject.setCEffectID( 0 );			
		_CGSkillToObject.setTargetObjectID( pItem->GetID() );
		g_pSocket->sendPacket( &_CGSkillToObject );				
	}
}
// 2005, 1, 4, sobeit add end
