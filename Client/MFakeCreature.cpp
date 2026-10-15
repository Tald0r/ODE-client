//----------------------------------------------------------------------
// MFakeCreature.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MFakeCreature.h"
#include "DebugLog.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "WorldTileGeometry.h"
#include "DirectionSelection.h"
#include "ViewSectors.h"
#include "MViewDef.h"
#include "SkillDef.h"
#include "UserInformation.h"
#include "OrbitEffectPolicy.h"
extern MonotonicClock::TimePoint g_FrameNow;
extern bool			HasEffectStatusSummonSylph( MCreature* pCreature );
extern void			RemoveEffectStatusSummonSylph( MCreature* pCreature );
BYTE GetCreatureActionCountMax( const MCreature* pCreature, int action );

// The fake creatures' allocator pool (task 4.245: MemoryPool.cpp's until
// then, which it retires).
MemoryPool g_FakeCreatureMemoryPool( sizeof( MFakeCreature ), 30 );

#define MoveNextPosition()					\
											\
		MovePosition( m_NextX, m_NextY );	\
		if (m_NextDirection != DIRECTION_NULL)	\
		{									\
			int dir = m_Direction;			\
											\
			SetAction( m_MoveAction );		\
			m_Direction = m_NextDirection;	\
			m_DirectionMove = m_NextDirection;	\
			ActionMove();					\
			m_Direction = dir;				\
		}									\
											\
		m_NextX = SECTORPOSITION_NULL;		\
		m_NextY = SECTORPOSITION_NULL;		\
		m_NextDirection = DIRECTION_NULL;

#define	ActionMoveNextPosition()					\
		if (!m_bFastMove)							\
		{											\
			if (m_NextX != SECTORPOSITION_NULL)		\
			{										\
				MoveNextPosition();					\
			}										\
			else if (m_NextAction==m_MoveAction)	\
			{										\
				SetAction( m_NextAction );			\
				ActionMove();						\
			}										\
		}
//----------------------------------------------------------------------
// static
//----------------------------------------------------------------------
DWORD MFakeCreature::m_FakeID = 0;

//----------------------------------------------------------------------
// Get FakeID
//----------------------------------------------------------------------
DWORD
MFakeCreature::GetFakeID()
{
	// Assumes the server uses objectIDs starting from 10000... probably.
	// Yum yum.... hard-coded, heh..
	MCreature *pFakeCreature = NULL;

	do
	{
		m_FakeID++;
		if (m_FakeID==10000)
		{
			m_FakeID = 0;
		}
		pFakeCreature = ZoneState::GetFakeCreature(m_FakeID);

	} while(pFakeCreature != NULL);


	return m_FakeID;
}

//----------------------------------------------------------------------
// 
// constructor / destructor
//
//----------------------------------------------------------------------
MFakeCreature::MFakeCreature()
{
	m_FakeCreatureType = FAKE_CREATURE_STAND;

	m_MoveType = CREATURE_FAKE_NO_BLOCK;

	m_FakeX = 0;
	m_FakeY = 0;
	m_Delay = 0;
	m_OwnerID = OBJECTID_NULL;
	m_bTraceFlag = false;

	m_ID = GetFakeID();

	m_pPetItem = NULL;

	// destination
	m_DestX = SECTORPOSITION_NULL;
	m_DestY = SECTORPOSITION_NULL;

	// next destination
	m_NextDestX = SECTORPOSITION_NULL;
	m_NextDestY = SECTORPOSITION_NULL;

	// a place it could not reach..
	m_BlockDestX = SECTORPOSITION_NULL;
	m_BlockDestY = SECTORPOSITION_NULL;

	m_nextMoveTime = MonotonicClock::Now() + MonotonicClock::Millis((rand()%8+3)*1000);
	m_PatrolCount = rand()%5+1;

	m_TurretDelay = 0;
	m_WildWolf_Mode = 0;
	m_IsEatCorps = false;
}

MFakeCreature::~MFakeCreature()
{
	if(m_pPetItem != NULL)
	{
		TYPE_OBJECTID id = GetOwnerID();
		if(PlayerState::Creature() != NULL && id != PlayerState::Creature()->GetID())
		{
			delete m_pPetItem;
			m_pPetItem = NULL;
		}
	}

	// remove priority queue 
	RemoveNodes();

	// remove list
	m_listDirection.clear();

}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Set Fake Creature
//----------------------------------------------------------------------
void				
MFakeCreature::SetFakeCreatureType(FAKE_CREATURE_TYPE fct)
{ 
	m_FakeCreatureType = fct; 

	switch (fct)
	{
		//----------------------------------------------
		// disappears after one faded action
		//----------------------------------------------
		case FAKE_CREATURE_FAST_MOVE_ACTION :		// [new skill]
		case FAKE_CREATURE_FADE_ACTION :
		case FAKE_CREATURE_BRIGHTNESS :
			SetFade();
		break;

		//----------------------------------------------
		// when entering a portal
		//----------------------------------------------
		case FAKE_CREATURE_TO_PORTAL :
			SetCutHeightFrom( 0, 80, 5 );
		break;

		//----------------------------------------------
		// spinning around
		//----------------------------------------------
		case FAKE_CREATURE_TURNING_MOVE :
			SetFade();
			break;
		case FAKE_CREATURE_ROCKET:
			break;
		default:
			break;
	}
}

//----------------------------------------------------------------------
// Set ActionResult  [new skill]
//----------------------------------------------------------------------
// It should go into Creature.. but to cut compile time -_-;;
//----------------------------------------------------------------------
/*
MActionResult*		
MFakeCreature::SetActionResult(MActionResult* pResult)
{
	if (m_pActionResult != NULL)
	{		
		// process the result that already exists.
		// [ TEST CODE ]
		// process the result..
		//
		// (!) m_pActionResult may change inside Execute,
		//		so it must be saved and deleted afterwards.
		MActionResult* pOldResult = m_pActionResult;

		m_pActionResult = NULL;

		pOldResult->Execute();

		delete pOldResult;			
	}

	// set the result
	m_pActionResult = pActionResult;
}
*/

//----------------------------------------------------------------------
// Action
//----------------------------------------------------------------------
void
MFakeCreature::Action()
{
	if( m_Delay > 0 )
	{
		m_Delay --;
		return;
	}

	//--------------------------------------------------------
	// when the chat string darkens
	//--------------------------------------------------------
	if (m_NextChatFadeTime < g_FrameNow)
	{
		FadeChatString();		
	}
	//--------------------------------------------------------
	// Status change - HP, MP change
	//--------------------------------------------------------
	UpdateStatus();	
	
	//--------------------------------------------------------
	// special effects
	//--------------------------------------------------------
	UpdateInvisible();
	UpdateCasket();
	UpdateCutHeight();
	UpdateTurning();

	//--------------------------------------------------------
	// Fake
	//--------------------------------------------------------
	UpdateFake();

	UpdateGhost();
	
	switch(GetFakeCreatureType())
	{
		case FAKE_CREATURE_TRACE:
			KeepTraceCreature();
			break;
		case FAKE_CREATURE_GHOST:
			KeepWanderCreature();
			break;
		case FAKE_CREATURE_ROCKET:
			KeepRocketLuncher();
			break;
		case FAKE_CREATURE_WILD_WOLF:
			KeepWildWolfAttack();
			break;
		default:
			break;

	}
	//--------------------------------------------------------
	// make the direction change look natural
	//--------------------------------------------------------
	ChangeNearDirection();
	//m_CurrentDirection = m_Direction;

	
	BOOL bSlayer = IsSlayer();

	if (m_nNextUsedActionInfo!=ACTIONINFO_NULL)
	{		
		// after affectUsedActionInfo is executed,
		// NextActionInfo may have changed.
		TYPE_ACTIONINFO nextUsedActionInfo = m_nNextUsedActionInfo;
		TYPE_ACTIONINFO nextNextUsedActionInfo = ACTIONINFO_NULL;

		//--------------------------------------------------------
		// if a skill was in use before... apply it right away.
		//--------------------------------------------------------
		if (m_nUsedActionInfo!=ACTIONINFO_NULL)
		{	
			m_nNextUsedActionInfo = ACTIONINFO_NULL;

			// if the castingEffect has not been shown yet, show it.
			if (GetActionInfoCastingStartFrame(m_nUsedActionInfo) >= m_ActionCount)
			{
				AttachCastingEffect( m_nUsedActionInfo, TRUE );
			}

			AffectUsedActionInfo( m_nUsedActionInfo );
			m_nUsedActionInfo	= ACTIONINFO_NULL;

			nextNextUsedActionInfo = m_nNextUsedActionInfo;
		}

		m_nUsedActionInfo = nextUsedActionInfo;
		m_nNextUsedActionInfo = nextNextUsedActionInfo;

		int actionInfoAction = GetActionInfoAction(m_nUsedActionInfo);

		// if it is a hit action.. blood splatter..
		if (actionInfoAction==ACTION_DAMAGED)
		{
			if //(m_Action!=ACTION_STAND 
				//&& (!IsSlayer() || IsSlayer && m_Action!=ACTION_SLAYER_MOTOR_STAND)
				//&& 
				(GetActionInfoCastingStartFrame(m_nUsedActionInfo) < m_ActionCount)
			{
				AttachCastingEffect( m_nUsedActionInfo, TRUE );

				AffectUsedActionInfo( m_nUsedActionInfo );
		
				m_nUsedActionInfo = ACTIONINFO_NULL;					
			}
		}

		// added 2001.05.21
		// ACTION_STAND is not shown in a skill action.
		if (actionInfoAction!=ACTION_STAND)
		{
			SetAction( actionInfoAction );
		}
	}
	//--------------------------------------------------------
	// remembered next action...
	//--------------------------------------------------------
	else if (m_bNextAction 
			|| (m_NextAction!=ACTION_STAND 
			&& (!bSlayer || (bSlayer && m_NextAction != ACTION_SLAYER_MOTOR_STAND))
			&& m_NextAction!=m_MoveAction))
	{
		SetAction( m_NextAction );
	}	 

	//--------------------------------------------------------
	// when paralyzed..
	//--------------------------------------------------------
	if (HasEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS)
		|| HasEffectStatus(EFFECTSTATUS_EXPLOSION_WATER)
		|| (HasEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS) && !IsSlayer())
		|| HasEffectStatus(EFFECTSTATUS_GLACIER))
	{			
		//--------------------------------------------------------
		// when trying to use a skill
		//--------------------------------------------------------
		if (m_nUsedActionInfo!=ACTIONINFO_NULL)
		{
			AffectUsedActionInfo(m_nUsedActionInfo);
			m_nUsedActionInfo	= ACTIONINFO_NULL;
		}
	}
	//--------------------------------------------------------
	// normal case
	//--------------------------------------------------------
	else
	{
		//--------------------------------------------------------
		// process Action
		//--------------------------------------------------------
		if (m_ActionCount < m_ActionCountMax)
		{		
			//--------------------------------------------------------
			// when trying to use a skill
			//--------------------------------------------------------
			if (m_nUsedActionInfo!=ACTIONINFO_NULL)
			{
				ActionEffect();
			}
			//--------------------------------------------------------
			// action..
			//--------------------------------------------------------
			else
			{
				// perform the appropriate Action
				//switch (m_Action)
				if (m_Action==ACTION_STAND 
					|| (IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_STAND))
				{
					m_ActionCount++;
				}

				/*
				else if (m_Action==ACTION_MOVE
					|| IsSlayer() && m_Action==ACTION_SLAYER_MOTOR_MOVE)
				{
						// walking action is handled separately.
						//ActionMove();
				}
				*/
				
				//	default :
				else 
				{
					ActionEffect();
				}
			}
		}
		
		//--------------------------------------------------------
		// when the Action has finished...
		//--------------------------------------------------------
		if (m_ActionCount>=m_ActionCountMax)
		{
			//--------------------------------------------------------
			// only if alive
			//--------------------------------------------------------
			if (m_bAlive)
			{
				//--------------------------------------------------------
				// having arrived at the target tile..
				// if it keeps standing.. or has finished walking..
				// show it swaying in place.
				//--------------------------------------------------------
				if (m_MoveCount==m_MoveCountMax)
				{		
					//#ifdef CONNECT_SERVER
					//--------------------------------------------------------
					// if there is no next move.. stop it.
					//--------------------------------------------------------
					AffectMoveBuffer();

					BOOL bSlayer = IsSlayer();

					if (m_bNextAction 
						|| (m_NextAction!=ACTION_STAND 
						&& (!bSlayer || (bSlayer && m_NextAction!=ACTION_SLAYER_MOTOR_STAND))))
					{
						m_Action = ((m_MoveDevice==MOVE_DEVICE_RIDE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND);
						SetAction( m_NextAction );
					}	 
					else
					{
						SetAction( (m_MoveDevice==MOVE_DEVICE_RIDE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND );				
					}
					//#else
					//	SetAction( m_MoveAction );				
					//#endif
				}
				//--------------------------------------------------------
				// it showed another action while walking, or was still walking.
				// walk again.
				//--------------------------------------------------------
				else 
				{			
					m_Action = m_MoveAction;				
					
					ActionMove();

					if (m_MoveCount>=m_MoveCountMax)
					{
						// if there is a next place to move to..
//						if (m_NextX != SECTORPOSITION_NULL)
						if (m_NextX != SECTORPOSITION_NULL
							&& m_NextDirection != DIRECTION_NULL)
						{		
//							DEBUG_ADD_FORMAT("[ Apply Buffering Move ] [ID=%d] Current(%d, %d) Dir(%d) --> Next(%d, %d) Dir(%d)", 
//															m_ID,
//															m_X, m_Y, m_Direction, 
//															m_NextX,m_NextY, m_NextDirection);
//							
//							MovePosition( m_NextX, m_NextY );
//							m_Direction = m_NextDirection;					
//							
//							// make it move.
//							SetAction( m_MoveAction );
//
//							m_NextX = SECTORPOSITION_NULL;
//							m_NextY = SECTORPOSITION_NULL;
//							m_NextDirection = DIRECTION_NULL;
							MovePosition( m_NextX, m_NextY );
							int dir = m_Direction;

							SetAction( m_MoveAction );
							m_Direction = m_NextDirection;
							m_DirectionMove = m_NextDirection;
							ActionMove();
							m_Direction = dir;

							m_NextX = SECTORPOSITION_NULL;
							m_NextY = SECTORPOSITION_NULL;
							m_NextDirection = DIRECTION_NULL;
						}
						else
						{
							//#ifdef CONNECT_SERVER

							//--------------------------------------------------------
							// if there is no next move.. stop it.
							//--------------------------------------------------------
							AffectMoveBuffer();

							BOOL bSlayer = IsSlayer();

							if (m_bNextAction 
								|| (m_NextAction!=ACTION_STAND 
								&& (!bSlayer || (bSlayer && m_NextAction!=ACTION_SLAYER_MOTOR_STAND))))
							{
								m_Action = ((m_MoveDevice==MOVE_DEVICE_RIDE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND);
								SetAction( m_NextAction );
							}	 
							else
							{
								SetAction( (m_MoveDevice==MOVE_DEVICE_RIDE)? ACTION_SLAYER_MOTOR_STAND : ACTION_STAND );				
							}
							//#else
							//	SetAction( m_MoveAction );				
							//#endif
						}
					}
				}
			}
		}
	}
}

//----------------------------------------------------------------------
// Set FakeCreature FastMoveAction [new skill]
//----------------------------------------------------------------------
void
MFakeCreature::SetFakeCreatureFastMoveAction(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY,
											 TYPE_ACTIONINFO skillID, 
											 TYPE_OBJECTID targetID)
{
	FastMovePosition(sX, sY);

	m_nBasicActionInfo = m_nSpecialActionInfo = skillID;
	m_TraceID = targetID;
}


//----------------------------------------------------------------------
// Set FakePosition
//----------------------------------------------------------------------
void				
MFakeCreature::SetFakePosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)	
{ 
	m_FakeX = sX; 
	m_FakeY = sY; 

	switch (m_FakeCreatureType)
	{
		//----------------------------------------------
		// spinning around
		//----------------------------------------------
		case FAKE_CREATURE_TURNING_MOVE :
		{
			//int actionCountMax = 2 * (*g_pCreatureTable)[m_CreatureType].GetActionCount( m_Action );
			int actionCountMax = 2 * GetCreatureActionCountMax( this, m_Action );

			if (actionCountMax==0)
			{
				actionCountMax = 1;
			}

			// target 200, current 100
			// gapX = 200 - 100 = 100
			// m_cX = 100 / c
			int gapX = m_FakeX - m_X;
			int gapY = m_FakeY - m_Y;
			float step = static_cast<float>(actionCountMax);
			m_FakeCX = gapX*TILE_X / step;
			m_FakeCY = gapY*TILE_Y / step;

			m_sX = 0;
			m_sY = 0;
			m_FakeSX = 0.0f;
			m_FakeSY = 0.0f;
		}
		break;

		default:
			break;
	}
}

//----------------------------------------------------------------------
// Update Fake
//----------------------------------------------------------------------
void
MFakeCreature::UpdateFake()
{
	switch (m_FakeCreatureType)
	{
		//----------------------------------------------
		// spinning around
		//----------------------------------------------
		case FAKE_CREATURE_TURNING_MOVE :
		{
			m_Direction = ((m_Direction+1) & 0x07);

			if (m_X!=m_FakeX || m_Y!=m_FakeY)
			{			
				m_FakeSX += m_FakeCX;
				m_FakeSY += m_FakeCY;

				int sX = static_cast<int>(m_FakeSX);
				int sY = static_cast<int>(m_FakeSY);

				// if it moved more than one tile coordinate..
				if (abs(sX) >= TILE_X || abs(sY) >= TILE_Y)
				{
					// calculate the new map coordinate
					int mapX = m_X;
					int mapY = m_Y;
					int	pixelX = WorldTileGeometry::TileToPixelX(mapX);
					int	pixelY = WorldTileGeometry::TileToPixelY(mapY);

					pixelX += sX;
					pixelY += sY;

					m_X = WorldTileGeometry::PixelToTileX(pixelX);
					m_Y = WorldTileGeometry::PixelToTileY(pixelY);

					sX %= TILE_X;
					sY %= TILE_Y;

					m_FakeSX = static_cast<float>(sX);
					m_FakeSY = static_cast<float>(sY);
				}
			}			
			else
			{
				m_FakeSX = 0;
				m_FakeSY = 0;
				m_FakeCX = 0;
				m_FakeCY = 0;
			}
		}
		break;

		default:
			break;
	}
}

//----------------------------------------------------------------------
// IsFakeEnd
//----------------------------------------------------------------------
bool
MFakeCreature::IsFakeEnd()
{
	switch (m_FakeCreatureType)
	{
		case FAKE_CREATURE_FADE_ACTION :
			return (m_Action==ACTION_STAND
					|| m_ActionCount>=m_ActionCountMax);
		break;

		case FAKE_CREATURE_TO_PORTAL :
			return m_CutHeightCount >= m_CutHeightCountLast;
		break;

		case FAKE_CREATURE_TURNING_MOVE :
			return m_X==m_FakeX 
					&& m_Y==m_FakeY 
					&& m_ActionCount>=m_ActionCountMax;
		break;

		// [new skill]
		case FAKE_CREATURE_FAST_MOVE_ACTION :
			return !m_bFastMove
					&& m_nUsedActionInfo==ACTIONINFO_NULL
					&& m_nNextUsedActionInfo==ACTIONINFO_NULL
					&& (m_Action==ACTION_STAND
						|| m_ActionCount>=m_ActionCountMax);
		break;
		case FAKE_CREATURE_ROCKET:
			{
				MCreature *pCreature = ZoneState::GetCreature(m_TraceID);
//			if(pCreature != NULL)
//			{
//				if(GetX() == pCreature->GetX() && GetY() == pCreature->GetY())
//				{
//					WorldEvents::ExecuteActionInfoFromMainNode(RESULT_TEMP_SKILL_PLASMA_ROCKET_LAUNCHER,pCreature->GetX(), pCreature->GetY(), 0,pCreature->GetDirection(),	0,
//							pCreature->GetX(), pCreature->GetY(), 0, 10, NULL, false);			
//					return true;
//				}
//			}
				if( MonotonicClock::Now() > m_nextMoveTime || NULL == pCreature)
				{
					if(pCreature != NULL)
						WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_PLASMA_ROCKET_LAUNCHER_BLOW,pCreature->GetX(), pCreature->GetY(), 0,pCreature->GetDirection(),	0,
								pCreature->GetX(), pCreature->GetY(), 0, 10, NULL, false);			
					return true;
				}
			}
			break;
		case FAKE_CREATURE_WILD_WOLF:
			{
				if(WILD_WOLF_MODE_REMOVE == m_WildWolf_Mode && m_PatrolCount > 30)
					return true;
			}
			break;
		case FAKE_CREATURE_DRAGON_TORNADO:
			{
				POINT	firstSector, endSector;
				firstSector.x = PlayerState::Creature()->GetX() + g_SECTOR_SKIP_PLAYER_LEFT-1;
				firstSector.y = PlayerState::Creature()->GetY() + g_SECTOR_SKIP_PLAYER_UP-1;
				endSector.x	  = firstSector.x+g_SECTOR_WIDTH+2;
				endSector.y   = firstSector.y+g_SECTOR_HEIGHT+2;
				// if it goes off screen, remove it.. for now..
				if(!IsInSector(firstSector.x, firstSector.y, endSector.x, endSector.y))
					return true;
			}
			break;
		default:
			break;
	}

	return false;
}

//----------------------------------------------------------------------
// Packet Special Action To Other [new skill]
//----------------------------------------------------------------------
void		
MFakeCreature::PacketSpecialActionToOther(TYPE_ACTIONINFO nActionInfo, TYPE_OBJECTID id, MActionResult* pActionResult)
{
	MCreature::PacketSpecialActionToOther(nActionInfo, id, pActionResult);

	switch (m_FakeCreatureType)
	{
		case FAKE_CREATURE_FAST_MOVE_ACTION :
		{		
			int dir = GetDirectionToPosition( m_TraceX, m_TraceY );
			SetDirection( dir );
			SetCurrentDirection( dir );
		}
		break;

		default:
			break;
	}
}

void
MFakeCreature::KeepTraceCreature()
{
	MCreature *pCreature = ZoneState::GetCreature(m_TraceID);

	if(pCreature != NULL)
	{
		UpdateTurret();

		int distance = max(abs(pCreature->GetX()-m_X), abs(pCreature->GetY()-m_Y));
		if(distance > 6)
		{
			m_FinalTurretDirect = 0xFF;
			RemoveNodes();
			m_bTraceFlag = true;
			SetFakeCreatureFastMoveAction(pCreature->GetX(), pCreature->GetY(), ACTIONINFO_NULL, pCreature->GetID());
		}
		else
		if(distance > 3)
		{
			m_FinalTurretDirect = 0xFF;
			m_bTraceFlag = true;
			RemoveNodes();
			SetNextDestination(pCreature->GetX(), pCreature->GetY());
			// hard-coded, hard-coded, heh
			if(m_Action == ACTION_VAMPIRE_DRAIN)
			{
				if(m_ActionCount < m_ActionCountMax-13)
					m_ActionCount = m_ActionCountMax-13;
				
				SetNextActionToMove();
			}
			else
			{
				if( m_Action != ACTION_MOVE )
					SetAction(m_MoveAction);
			}
		}

		static BYTE lastAction = ACTION_STAND;
		if(lastAction != ACTION_STAND && m_Action == ACTION_STAND)
		{
			// it stopped
			m_bTraceFlag = false;
		}
		lastAction = m_Action;

		if(m_bFastMove == false && MonotonicClock::Now() >= m_nextMoveTime)
		{
			if(m_PatrolCount <= 0)
			{
				MPetItem *pPetItem = GetPetItem();
				if(pPetItem != NULL)
				{
					switch(pPetItem->GetItemType())
					{
					case 1:
					case 2:
						m_nextMoveTime = MonotonicClock::Now() + MonotonicClock::Millis((rand()%5+3)*1000);
						m_PatrolCount = rand()%5+3;
						SetAction(ACTION_VAMPIRE_DRAIN);
						break;

					case 3:
						m_PatrolCount = rand()%3+2;
						m_nextMoveTime = MonotonicClock::Now() + MonotonicClock::Millis((rand()%3+2)*1000);
						WorldEvents::ExecuteActionInfoFromMainNode(
							SKILL_CLIENT_TANK_SMOKE,										// skill number used
							
							0, 0, 0,
							DIRECTION_DOWN, // direction used
							
							GetID(),												// information about the target
							0, 0, 0, 
							
							0,													// (remaining) duration of the skill
							
							NULL,
							
							false);	
						break;
					}
				}
			}
			else if(m_Action == ACTION_STAND && m_PatrolCount > 0 && GetPetItem() != NULL)
			{
				MPetItem *pPetItem = GetPetItem();

				if(GetCreatureType() == 702 || GetCreatureType() == 703 || GetCreatureType() == 704)
				{
					SetTurretFinalDirection( (BYTE)RandomIndex(rand(), 32) );
					m_nextMoveTime = MonotonicClock::Now() + MonotonicClock::Millis((rand()%3+2)*1000);
					m_PatrolCount--;
				}
				else
				{
					m_nextMoveTime = MonotonicClock::Now() + MonotonicClock::Millis((rand()%5+3)*1000);
					m_PatrolCount--;
					int minDistance = 1;

					if(pPetItem != NULL)
					{
					int petLevel = GetPetItem()->GetNumber();

					if(petLevel >= 30)
						minDistance = 2;
					else if(petLevel >= 20)
						minDistance = 1;
					}

					POINT p = CaculatePetPosition(pCreature->GetX(), pCreature->GetY(), 2, minDistance, GetMoveType(), GetX(), GetY());
					SetNextDestination(static_cast<TYPE_SECTORPOSITION>(p.x), static_cast<TYPE_SECTORPOSITION>(p.y));
					SetAction( m_MoveAction );
				}
			}
		}


//		else if(distance <=1)
//			m_bTraceFlag = false;
//
//		if(m_bTraceFlag)
//		{		
//			BYTE dir = GetDirectionToPosition(pCreature->GetX(), pCreature->GetY());
//			TYPE_SECTORPOSITION x = m_X, y = m_Y;
//			GetPositionToDirection(x, y, dir);
//			PacketMove(x, y, dir);
//			SetNextDestination(pCreature->GetX(), pCreature->GetY());
//			SetAction( m_MoveAction );
//
//		}
	}
	else
	{
		SetStop();
	}
}

//----------------------------------------------------------------------
// Set Next Destination
//----------------------------------------------------------------------
// Decide the next target position to move to.
// What to do if the target position is unreachable?
//----------------------------------------------------------------------
bool	
MFakeCreature::SetNextDestination(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{ 
	// while not tracing,
	// if already at the target position, no path finding is needed.
	if (m_bTraceFlag==false && sX==m_X && sY==m_Y)
	{
		return false;
	}	

	// invalid value
	// a place it could not reach before..
	if (sX==SECTORPOSITION_NULL || sY==SECTORPOSITION_NULL
		|| (sX==m_BlockDestX && sY==m_BlockDestY))
	{
		return false;
	}

	if(m_bTraceFlag == false)
	{
		BOOL bCanStand = ZoneState::CanMoveIn(m_pZone, m_MoveType, sX,sY);
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

			int limit = 20;	// give up if not found within 20 valid tries

			while( bCanStand == FALSE && !( GetX() == sX && GetY() == sY )
				&& sX < ZoneState::GetWidthIn(m_pZone) && sY < ZoneState::GetHeightIn(m_pZone)
				&& limit-- > 0
				)
			{
				if( GetX() != sX )sX = static_cast<TYPE_SECTORPOSITION>(sX + MovePoint.x);
				if( GetY() != sY )sY = static_cast<TYPE_SECTORPOSITION>(sY + MovePoint.y);
				bCanStand = ZoneState::CanMoveIn(m_pZone,  m_MoveType, sX,sY );
			};

			if( bCanStand == FALSE || (GetX() == sX && GetY() == sY))
				return false;
		}
	}

	
	m_NextDestX = sX; 
	m_NextDestY = sY; 

//	SetDestination(m_NextDestX, m_NextDestY);

	return true;
}

//----------------------------------------------------------------------
// Get NextDestination()
//----------------------------------------------------------------------
void
MFakeCreature::GetNextDestination(POINT &dest)
{
	dest.x = m_NextDestX;
	dest.y = m_NextDestY;
}

//----------------------------------------------------------------------
// Get NextPosition()
//----------------------------------------------------------------------
// Of the paths decided by path finding,
// return the coordinates of the Sector to go to right next.
//
// add the appropriate change for the 'next direction' to the 'current position'.
//----------------------------------------------------------------------
bool
MFakeCreature::GetNextPosition(POINT &next)
{
	// when no path has been decided
	if (m_listDirection.empty())
	{
		//next.x = SECTORPOSITION_NULL;
		//next.y = SECTORPOSITION_NULL;
		return false;
	}

	
	// when a path has been decided, return the next coordinates.

	// set the target from the current position for the next direction
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
// At the moment the target position is specified,
// decide the Best Path to it and store it in m_listDirection.
//----------------------------------------------------------------------
bool
MFakeCreature::SetDestination(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	if(HasEffectStatus(EFFECTSTATUS_SLEEP)
		&& !PlayerState::HasEffectStatus( EFFECTSTATUS_GHOST )
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
	// if not alive..
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

	//-------------------------------------------------------
	// do not check if outside the Zone area.
	//-------------------------------------------------------
	if (sX>=ZoneState::GetWidthIn(m_pZone) || sY>=ZoneState::GetHeightIn(m_pZone)
		)
	{
		// case: do not check if outside the Zone area
		//m_ActionCount = m_ActionCountMax;
		m_MoveCount = m_MoveCountMax;
		m_DestX = SECTORPOSITION_NULL;
		m_DestY = SECTORPOSITION_NULL;

		#ifdef OUTPUT_DEBUG_PLAYER_ACTION
			DEBUG_ADD("outOfZone or Sanctuary");
		#endif

		return false;
	}

	//-------------------------------------------------------
	// when tracing and already within range...
	//-------------------------------------------------------
	if (m_bTraceFlag == true
		&& max(abs(m_TraceX-m_X), abs(m_TraceY-m_Y))<=1)
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
	// if it is a place it can go
	//--------------------------------------------------------------
	// Best First Search
	//--------------------------------------------------------------
	BOOL	bCanStand = ZoneState::CanMoveIn(m_pZone, m_MoveType, sX,sY);

	//--------------------------------------------------------------
	// if it is a place it can go, or
	// if tracing, find the path with Best Search~.
	//--------------------------------------------------------------
	if (bCanStand || m_bTraceFlag == true) 
	{		
		//-------------------------------------------------------
		// store the target position
		//-------------------------------------------------------
		m_DestX = sX;
		m_DestY = sY;

		//-------------------------------------------------------
		// initialize the priority queue.
		//-------------------------------------------------------
		RemoveNodes();	

		//-------------------------------------------------------
		// set visited all to false, and
		// start visiting the unvisited places from the current position.
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
		if (x1 > ZoneState::GetWidth())
			x1 = ZoneState::GetWidth();
		
		if (y0 < 0)				
			y0 = 0;
		if (y1 > ZoneState::GetHeight())
			y1 = ZoneState::GetHeight();
		
//		int size = x1-x0;	// number of BYTEs
		for (int i=y0; i<y1; i++)
		{			
			for (int j=x0; j<x1; j++)
			{
				//m_ppVisited[i][j] = false;
				ZoneState::UnSetVisitedFlag(j, i);
			}
//			memset( m_ppVisited[i]+x0, false, size );
		}
			

		//-------------------------------------------------------
		//
		// current position
		//
		//-------------------------------------------------------

		//-------------------------------------------------------
		// start path finding from the direction toward the destination.
		//-------------------------------------------------------
		static BYTE FirstDirection = GetDirectionToPosition(m_DestX, m_DestY);
		static TYPE_SECTORPOSITION previousDestX = SECTORPOSITION_NULL;
		static TYPE_SECTORPOSITION previousDestY = SECTORPOSITION_NULL;

		//-------------------------------------------------------
		// only when the target position differs from the previous path finding,
		// set the direction toward the destination.
		//-------------------------------------------------------
		if (previousDestX!=m_DestX || previousDestY!=m_DestY)
		{
			FirstDirection = GetDirectionToPosition(m_DestX, m_DestY);
			previousDestX = m_DestX;
			previousDestY = m_DestY;
		}
		//-------------------------------------------------------
		// when heading to the same target,
		// look from the current Player direction first.
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
		// put it into listDNodes to delete later.
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
			// pick the best (!) Node (it is a priority queue)
			// and calculate where to go.
			//-------------------------------------------------------
			DNode* pDNode = m_pqDNodes.top();
			m_pqDNodes.pop();

			//-------------------------------------------------------
			// fully arrived
			//-------------------------------------------------------
			if (pDNode->distance==0) 
			{
				m_pqDNodes.push(pDNode);
				bFound = true;
				break;
			}
			
			//-------------------------------------------------------
			// approach to within range
			//-------------------------------------------------------
			if (m_bTraceFlag == true && 
				max(abs(pDNode->x-m_DestX), abs(pDNode->y-m_DestY))<=1)
			{
				m_pqDNodes.push(pDNode);
				bFound = true;
				break;
			}			

			int Direction = pDNode->direction;

			//-------------------------------------------------------
			// check all 8 directions
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
				// do not check if outside the Zone area.
				//-------------------------------------------------------
				if (next.x<0 || next.y<0 
					|| next.x>=ZoneState::GetWidthIn(m_pZone) || next.y>=ZoneState::GetHeightIn(m_pZone)) continue;

				//-------------------------------------------------------
				// do not go to a place already visited.
				//-------------------------------------------------------
				if (ZoneState::IsVisitedFlag(static_cast<TYPE_SECTORPOSITION>(next.x), static_cast<TYPE_SECTORPOSITION>(next.y))) continue;

				// do not check if it goes beyond one screen.
				if (next.x<x0 || next.y<y0 
					|| next.x>=x1 || next.y>=y1) continue;

				//-------------------------------------------------------
				// if it can go, put it into the pqueue.
				//-------------------------------------------------------
				if (ZoneState::CanMoveIn(m_pZone, m_MoveType, static_cast<TYPE_SECTORPOSITION>(next.x), static_cast<TYPE_SECTORPOSITION>(next.y)))
				{				
					ZoneState::SetVisitedFlag(static_cast<TYPE_SECTORPOSITION>(next.x), static_cast<TYPE_SECTORPOSITION>(next.y));

					//-------------------------------------------------------
					// create a Node and add it to the priority queue.
					//-------------------------------------------------------
					// calculate distance
					dist = CalculateDistance(next.x, next.y);

					// create a Node and add it
					DNode*	nextDNode = new DNode(
											next.x, 
											next.y, 
											Direction,
											dist,
											pDNode->step + 1);
					
				
					//-------------------------------------------------------
					// put it into listDNodes to delete later.
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
		// when found, the path must be decided.
		//-------------------------------------------------------
		if (bFound)
		{				
			//-------------------------------------------------------
			// fill the list from m_pqDNodes. (must follow the parent)
			//-------------------------------------------------------

			// target position
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
		// stop if there is no path.
		//-------------------------------------------------------
		else
		{
			// stop tracing
			if (m_bTraceFlag == true)				
			{
				m_MoveCount = m_MoveCountMax;

				m_bTraceFlag = false;

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
	// a place it cannot go
	else 
	{
		//m_ActionCount = m_ActionCountMax;
		m_MoveCount = m_MoveCountMax;

		// set the place it could not go this time
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
	// There seems to be a slight error in this part!! -_-;;;;
	//--------------------------------------------------------------
	// if it is a place it cannot go at all..
	//--------------------------------------------------------------
	// walk in a straight line(?) from the Player position to the target,
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
			// according to the sign..
			signX = (stepX==0)? 0 : (stepX<0)? -1 : 1;
			signY = (stepY==0)? 0 : (stepY<0)? -1 : 1;

			x += signX;
			y += signY;
				
			// if it is a place it can go, put it into the list.
			if (m_pZone->CanMove(x,y))
			{				
				m_listDirection.insert( m_listDirection.end(), DetermineDirection(stepX, stepY));
			}
			else 
			{
				// set the position one step before x,y as the target.
				m_DestX = x - signX;
				m_DestY = y - signY;
				break;
			}	
			
			// mark that it moved one step
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
// Get Destination()
//----------------------------------------------------------------------
void
MFakeCreature::GetDestination(POINT &dest)
{
	dest.x = m_DestX;
	dest.y = m_DestY;
}

//----------------------------------------------------------------------
// delete m_pqDNodes, the priority queue.
//----------------------------------------------------------------------
void
MFakeCreature::RemoveNodes()
{
	while (!m_pqDNodes.empty())
	{
		m_pqDNodes.pop();
	}


	// delete temp.
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
// get the distance from (x,y) to m_Dest(X,Y).
//----------------------------------------------------------------------
// Horizontal, vertical and diagonal are all 1, so the largest value is the distance,
// and having a coordinate equal to the target is better, so add +1 if not.
//----------------------------------------------------------------------
int
MFakeCreature::CalculateDistance(int x, int y)
{

	int xd = abs(x-m_DestX),
		yd = abs(y-m_DestY);	 

	int plus = !(m_DestX==x) + !(m_DestY==y);
		
	if (xd>yd)
	{
		return xd*xd + plus;
	}
	else return yd*yd + plus;

	return 0;
}

void
MFakeCreature::SetStop()
{
	// remove path finding
	m_listDirection.clear(); 

	// stop tracing
	m_bTraceFlag = false;
	//m_fNextTrace	= FLAG_TRACE_NULL;

	// stop Action
	m_sX=0; 
	m_sY=0;

	// also remove the next action
	m_NextX = SECTORPOSITION_NULL;
	m_NextY = SECTORPOSITION_NULL;

	// remove target
	m_DestX			= SECTORPOSITION_NULL; 
	m_DestY			= SECTORPOSITION_NULL; 
	m_NextDestX		= SECTORPOSITION_NULL; 
	m_NextDestY		= SECTORPOSITION_NULL; 

	m_bFastMove = false;
	m_bKnockBack = 0;	

	//------------------------------------------------
	// delete the whole existing MoveBuffer
	//------------------------------------------------
	AffectMoveBufferAll();
	//ReleaseMoveBuffer();

	// 2001.11.8 - make sure the coordinates are corrected when stopping..
	ActionMoveNextPosition();

	// stop Action
	m_sX=0; 
	m_sY=0;

	int action = ACTION_STAND;
	m_Action		= action;
	m_ActionCount	= m_ActionCountMax; 
	m_MoveCount		= m_MoveCountMax;

	// also remove the next action
	m_bNextAction = false;
	m_NextX = SECTORPOSITION_NULL;
	m_NextY = SECTORPOSITION_NULL;

	
	//m_nNextUsedActionInfo = ACTIONINFO_NULL;

}

//----------------------------------------------------------------------
// Move
//----------------------------------------------------------------------
//
// Move one Frame in the current direction (m_CurrentDirection).
//
// This is not moving one Sector; it moves one Frame
// in the middle of moving between Sector and Sector.
//
// The question is how many Frames to move from Sector --> Sector??
//    = assign the per-step pixel unit to cX,cY
//    = and enter the per-step pixel unit in sX,sY.
//
// In Move(),
//    if cX,cY is 0, regenerate the cX,cY, sX,sY values,
//    if it is not 0, sX
//----------------------------------------------------------------------
void	
MFakeCreature::ActionMovePet()
{
	#ifdef OUTPUT_DEBUG_PLAYER_ACTION
		DEBUG_ADD("ActionMove");
	#endif

	int moveTimesTemp = 0;
	if (m_MoveDevice==MOVE_DEVICE_WALK)
	{
		moveTimesTemp = (*g_pCreatureTable)[m_CreatureType].MoveTimes;
	}
	else if (m_MoveDevice==MOVE_DEVICE_RIDE)
	{
		moveTimesTemp = (*g_pCreatureTable)[m_CreatureType].MoveTimesMotor;
	}
	else if (m_MoveDevice==MOVE_DEVICE_SUMMON_SYLPH || HasEffectStatusSummonSylph( dynamic_cast<MCreature*>(this) ))
	{
		moveTimesTemp = (*g_pCreatureTable)[m_CreatureType].MoveTimesMotor;
	}

	//----------------------------------------------------
	//
	// when the next Sector has been reached
	//
	//----------------------------------------------------
	// read one direction from m_listDirection,
	// check it and move.
	//----------------------------------------------------
	if (m_MoveCount%moveTimesTemp==0)//m_sX==0 && m_sY==0)
	{
		TYPE_SECTORPOSITION	x, y;

		//-------------------------------------------------------------
		// for fast move, the coordinates are in nextX, nextY.
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
		// normal movement
		//-------------------------------------------------------------
		else
		{
			//----------------------------------------------------
			// if there is no target position, do not move.
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
			//
			//                 if tracing..
			//
			//
			//----------------------------------------------------		
			//----------------------------------------------------	
			// if tracing a Creature..
			// the Creature may disappear, or
			// the trace target coordinates may change, so check.
			//----------------------------------------------------	
			if (m_bTraceFlag == true) 
			{
				// read the information of the Creature being traced.
				MCreature*	pCreature = ZoneState::GetCreatureIn(m_pZone,  m_TraceID );

				//GET_DYING_CREATURE( pCreature, m_TraceID );

				//-------------------------------------------------------
				// if the traced Creature disappeared --> stop tracing
				// if I am a Slayer, I cannot chase a character that entered Darkness.
				//-------------------------------------------------------								
				if (pCreature==NULL )
				{
					// stop tracing
					m_bTraceFlag = false;
						
					// stop.
					SetStop();					
					return;			
				}
			
				//-------------------------------------------------------			
				// if the coordinates of the Creature to trace changed
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
			// set the buffered path finding.
			//----------------------------------------------------
			if (m_NextDestX!=SECTORPOSITION_NULL)
			{
				// path finding
				//SetDestination(m_NextDestX, m_NextDestY);
				m_DestX		= m_NextDestX;	// TEST
				m_DestY		= m_NextDestY;	// TEST
				m_NextDestX = SECTORPOSITION_NULL;
				m_NextDestY = SECTORPOSITION_NULL;			
			}

			//----------------------------------------------------
			// path finding: do path finding every time it moves one Tile.
			//----------------------------------------------------
			if (m_DestX!=SECTORPOSITION_NULL)
				SetDestination(m_DestX, m_DestY);

			//----------------------------------------------------
			// Send it to get verified by the Server
			//----------------------------------------------------
			//SetAction( m_MoveAction );

			
			//----------------------------------------------------
			// when there is no path to go
			//----------------------------------------------------
			if (m_listDirection.empty())
			{
				//--------------------------------------------
				// when the destination has been reached..
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
			// move in the direction.
			//
			//----------------------------------------------------			
			//----------------------------------------------------
			// read one direction from m_listDirection.
			//----------------------------------------------------
			m_Direction = m_listDirection.front();
			m_listDirection.pop_front();	
				
			//----------------------------------------------------
			// when path finding needs to be done next...
			//----------------------------------------------------
			if (m_NextDestX!=SECTORPOSITION_NULL)
			{
				//m_NextAction = m_MoveAction;
				SetNextAction( m_MoveAction );
			}

			//----------------------------------------------------
			// advance one Sector in the direction read,
			// and cX,cY, sX,sY must be set again.
			//----------------------------------------------------

			//----------------------------------------------------
			// check whether movement is possible
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
		// when it can move
		//--------------------------------------------------------------------
		if (ZoneState::CanMoveIn(m_pZone, m_MoveType, x,y)
			|| m_bFastMove		// Fast Move can always move..
			
			)
		{		
			//---------------------------------------------------------------
			// for Fast Move..
			//---------------------------------------------------------------
			if (m_bFastMove)
			{
				// nothing
			}
			//---------------------------------------------------------------
			// normal movement --> needs verification
			//---------------------------------------------------------------
			else
			{
			}

			//-----------------------
			// change the view
			//-----------------------
			//m_pZone->MoveLightSight(m_X, m_Y, m_LightSight, m_Direction);
			//m_pZone->UnSetLightSight(m_X, m_Y, m_LightSight);
			//m_pZone->SetLightSight(x, y, m_LightSight);

			// remember the previous coordinates
			TYPE_SECTORPOSITION ox = m_X;
			TYPE_SECTORPOSITION oy = m_Y;

			// change the zone's sector information.
			MovePosition( x, y );
			
			//CheckInDarkness();

			//--------------------------------------------
			// suppose there are 5 Frames
			//--------------------------------------------
			//
			// [1] for UP,DOWN,LEFT,RIGHT,
			//
			//     X change : 0 16 32 48 64   (+-16)
			//     Y change : 0  8 16 24 32   (+-8)
			// 
			//
			// [2] when moving diagonally (LEFTUP,LEFTDOWN,RIGHTUP,RIGHTDOWN),
			//
			//     X change : 0  8 16 24 32   (+-8)
			//     Y change : 0  4  8 12 16   (+-4)
			//
			//--------------------------------------------
			// sX,sY : total pixels to move (one TILE)
			// cX,cY : pixels per step
			//--------------------------------------------		
			//--------------------------------------------
			// for Fast Move
			//--------------------------------------------

			if (m_bFastMove)
			{
				// pixel distance when moving in a straight line
				// ex) left 2 --> 1 : ( 2 - 1 ) * TILE_X
				//     up 2 --> 1 : ( 2 - 1 ) * TILE_Y
				m_sX = (ox - m_X) * TILE_X;
				m_sY = (oy - m_Y) * TILE_Y;
				
				//------------------------------------------------
				// reaches the destination at half the speed of actually moving one tile.
				//------------------------------------------------
				// number of Frames when moving one tile
				int moveTimes_div_2 = (*g_pCreatureTable)[m_CreatureType].MoveTimes >> 1;
				
				m_cX = -m_sX / moveTimes_div_2;
				m_cY = -m_sY / moveTimes_div_2;

				m_MoveCount += 2;

				if (m_MoveCount > m_MoveCountMax)
				{
					m_MoveCount = m_MoveCountMax;
				}
			}
			//--------------------------------------------	
			// normal movement
			//--------------------------------------------
			else
			{	
				m_DirectionMoved = m_Direction;

				m_sX = m_sXTable[m_DirectionMoved];
				m_sY = m_sYTable[m_DirectionMoved];

				//????????????????????????????????????????????????????????
				//???                                                  ???
				//???   Should it move one Frame... or not??   ???
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
				m_cX = m_cXTable[moveTimes_1][m_DirectionMoved][m_MoveTableCount%moveTimes_1];
				m_cY = m_cYTable[moveTimes_1][m_DirectionMoved][m_MoveTableCount%moveTimes_1];
			}

			m_sX += m_cX;
			m_sY += m_cY;

			if( m_MoveCount == 0 )
				m_NextMoveCount = 1;//(*g_pCreatureTable)[m_CreatureType].MoveRatio;
			else
				m_NextMoveCount += 1;//(*g_pCreatureTable)[m_CreatureType].MoveRatio;

			m_MoveCount++;

			// set the count for the next move.

			//------------------------------------------------
			// play the Sound matching the character's MoveAction.
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
		// when it cannot move
		//--------------------------------------------
		// do path finding again.
		else
		{
			m_NextDestX = m_DestX;
			m_NextDestY = m_DestY;
			m_listDirection.clear();


			DEBUG_ADD("Blocked by another character.");
		}

		// move to the next Sector.
		if (!m_listDirection.empty())
		{
			//m_NextAction = m_MoveAction;
			SetNextAction( m_MoveAction );		
		}
	}
	//----------------------------------------------------
	//
	// when it has not yet reached the next Sector
	//
	//----------------------------------------------------
	else
	{
		// move one Frame.
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

		// move only when it is time for the move count.
		if (m_MoveCount>=m_NextMoveCount)
		{			
			//--------------------------------------------
			// for Fast Move
			//--------------------------------------------
			if (m_bFastMove)
			{
				// added 2001.8.22 - yum yum
				if (m_sX!=0)	m_sX += m_cX;
				if (m_sY!=0)	m_sY += m_cY;		

				m_MoveCount += 2;

				if (m_MoveCount > m_MoveCountMax)
				{
					m_MoveCount = m_MoveCountMax;
				}

				// set the count for the next move.
				m_NextMoveCount += 1;//(*g_pCreatureTable)[m_CreatureType].MoveRatio;

			}
			//--------------------------------------------
			// in the case of normal movement
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
				m_cX = m_cXTable[moveTimes_1][m_DirectionMoved][m_MoveTableCount%moveTimes_1];
				m_cY = m_cYTable[moveTimes_1][m_DirectionMoved][m_MoveTableCount%moveTimes_1];

				// added 2001.8.22 - yum yum
				if (m_sX!=0)	m_sX += m_cX;
				if (m_sY!=0)	m_sY += m_cY;		

				// set the count for the next move.
				m_NextMoveCount += 1;//(*g_pCreatureTable)[m_CreatureType].MoveRatio;

				m_MoveCount++;
			}
		}	
		else
		{
			m_MoveCount++;
		}
		
		//m_ActionCount++;
		// change the frame. If it reached the last Frame, go back to Frame 0.
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


void MFakeCreature::ActionMove()
{
	if(GetOwnerID() != OBJECTID_NULL)
		ActionMovePet();
	else
		MCreature::ActionMove();
}

void	
MFakeCreature::SetZone(MZone* pZone)
{
	// base class functions
	MCreature::SetZone(pZone);
}

void
MFakeCreature::SetOwnerID(TYPE_OBJECTID id)
{
	m_OwnerID = id;
}

POINT
MFakeCreature::CaculatePetPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int distance, int distanceMin, MOVE_TYPE moveType, TYPE_SECTORPOSITION petX, TYPE_SECTORPOSITION petY)
{
	POINT p = {sX, sY};

	if(!ZoneState::Exists())
	{
		return p;
	}

	const int x0 = sX-distance;
	const int x1 = sX+distance;
	const int y0 = sY-distance;
	const int y1 = sY+distance;

	std::vector<POINT> vMovePoint;

	for (int i=y0; i<y1; i++)
	{			
		for (int j=x0; j<x1; j++)
		{
			if(ZoneState::CanMove(moveType, j, i) && j != sX && i != sY && max(abs(i-petY), abs(j-petX)) >= distanceMin)
			{
				POINT tempPoint = {j, i};
				vMovePoint.push_back(tempPoint);
			}
		}
	}

	if(!vMovePoint.empty())
	{
		p = vMovePoint[rand()%vMovePoint.size()];
	}

	return p;
}

void
MFakeCreature::SyncTurretDirection()
{
	m_TurretDirect = ((GetDirection()+6)%8)*4;
	m_FinalTurretDirect = 0xFF;
}

BYTE
MFakeCreature::GetTurretDirection() const
{
	return m_TurretDirect;
}

void
MFakeCreature::SetTurretFinalDirection(BYTE d)
{
	m_FinalTurretDirect = d;
}

void
MFakeCreature::UpdateTurret()
{
	if( (GetCreatureType() == 702 || GetCreatureType() == 703 || GetCreatureType() == 704) )
	{
		if( GetTurretDelay() > 0 )
			m_TurretDelay--;
		
		bool bSetFinalDirect = false;

		TYPE_OBJECTID ownerID = GetOwnerID();

		BYTE finalDirect = 0;
		
		MCreature *pOwnerCreature = ZoneState::GetCreature( ownerID );

		if( pOwnerCreature != NULL )
		{
			TYPE_OBJECTID targetID = pOwnerCreature->GetStatus( MODIFY_LAST_TARGET );

			MCreature *pTargetCreature = ZoneState::GetCreature( targetID );

			if( pTargetCreature != NULL )
			{
				BYTE direct = GetDirectionToPosition( pTargetCreature->GetX(), pTargetCreature->GetY() );

				bSetFinalDirect = true;
				finalDirect = ((direct+6)%8)*4;
			}
		}
		
		if( bSetFinalDirect == false )
		{
			finalDirect = ((GetDirection()+6)%8)*4;
			if( m_FinalTurretDirect != 0xFF )
				finalDirect = m_FinalTurretDirect;
		}
			
		if( finalDirect != m_TurretDirect )
		{
			if( ((finalDirect - m_TurretDirect +32)%32) < 16 )
			{
				m_TurretDirect = (m_TurretDirect + 1)%32;
			}
			else
			{
				m_TurretDirect = (m_TurretDirect - 1 +32)%32;
			}
		}
	}
}
//2004, 03, 22 sobeit add start
void 
MFakeCreature::KeepWanderCreature()
{
	// return if not a ghost
	if(GetCreatureType() != CREATURETYPE_GHOST)
		return;
	POINT	firstSector, endSector,NextMove;
	firstSector.x = PlayerState::Creature()->GetX() + g_SECTOR_SKIP_PLAYER_LEFT-1;// +4 test
	firstSector.y = PlayerState::Creature()->GetY() + g_SECTOR_SKIP_PLAYER_UP-1;
	endSector.x	  = firstSector.x+g_SECTOR_WIDTH+2;
	endSector.y   = firstSector.y+g_SECTOR_HEIGHT+2;
	// if it is a ghost inside the screen..
	if(IsInSector(firstSector.x, firstSector.y, endSector.x, endSector.y))
	{
		NextMove.x = GetX(); NextMove.y = GetY();
		switch (m_Direction)
		{
			case DIRECTION_LEFTDOWN		: NextMove.x--;	NextMove.y++;	break;
			case DIRECTION_RIGHTUP		: NextMove.x++;	NextMove.y--;	break;
			case DIRECTION_LEFTUP		: NextMove.x--;	NextMove.y--;	break;
			case DIRECTION_RIGHTDOWN	: NextMove.x++;	NextMove.y++;	break;
			case DIRECTION_LEFT			: NextMove.x--;				break;
			case DIRECTION_DOWN			:			NextMove.y++;	break;
			case DIRECTION_UP			:			NextMove.y--;	break;
			case DIRECTION_RIGHT		: NextMove.x++;				break;
		}

		if(	!ZoneState::CanMoveIn(m_pZone, m_MoveType, static_cast<TYPE_SECTORPOSITION>(NextMove.x),static_cast<TYPE_SECTORPOSITION>(NextMove.y)))
			m_Direction = (m_Direction + 3 + rand()%3)%8;
		
		if( m_Action != ACTION_MOVE )
		{
			if(0xff == m_DirectionMove)
				m_Direction = rand()%8;
			SetAction(ACTION_MOVE);
		}
		m_DirectionMove = m_Direction;
	}
	else
	{
		if(ACTION_MOVE == PlayerState::Creature()->GetAction())
		{
			//SetStop();
			SetAction(ACTION_STAND);
			m_DirectionMove = 0xff;
		}
		else
		{
			if(abs(GetX()-PlayerState::Creature()->GetX())>10 ||abs(GetY()-PlayerState::Creature()->GetY())>15 )
			{
				//SetStop();
				SetAction(ACTION_STAND);
				return;
			}
			if(IsInSector(firstSector.x-1, firstSector.y, firstSector.x-1, endSector.y))
			{// left wall
				m_Direction = 3 + rand()%3;
				m_DirectionMove = m_Direction;
				SetAction(ACTION_MOVE);
			}
			else if(IsInSector(endSector.x+1, firstSector.y, endSector.x+1, endSector.y))
			{// right wall
				m_Direction = (7 + rand()%3)%8;
				m_DirectionMove = m_Direction;
				SetAction(ACTION_MOVE);
			}
			else if(IsInSector(firstSector.x, firstSector.y-1, endSector.x, firstSector.y-1))
			{// top wall
				m_Direction = 1 + rand()%3;
				m_DirectionMove = m_Direction;
				SetAction(ACTION_MOVE);
			}
			else if(IsInSector(firstSector.x, endSector.y+1, endSector.x, endSector.y+1))
			{// bottom wall
				m_Direction = 5 + rand()%3;
				m_DirectionMove = m_Direction;
				SetAction(ACTION_MOVE);
			}
			else 
				//SetStop();
				SetAction(ACTION_STAND);
			
		}
	}


}
bool
MFakeCreature::IsInSector(LONG sX, LONG sY, LONG eX, LONG eY)
{
	if (GetX() >= sX && GetX() <= eX
		&& GetY() >= sY && GetY() <= eY)
	{
		return true;
	}
	return false;
}
//2004, 03, 22 sobeit add end

// 2004, 11, 8, sobeit add start
void 
MFakeCreature::KeepRocketLuncher()
{
	if(GetCreatureType() != CREATURETYPE_ROCKET_LUNCHER)
		return;

	if(IsGhost(0xff))
		return;
	MCreature *pCreature = ZoneState::GetCreature(m_TraceID);
	if(pCreature != NULL)
	{
		int distance = max(abs(pCreature->GetX()-m_X), abs(pCreature->GetY()-m_Y));
//		// trace only until the first approach (use the existing variable m_PatrolCount.. heh)
		if(distance > 1)
		{
			m_DirectionMove = m_Direction = m_CurrentDirection = GetDirectionToPosition(pCreature->GetX(), pCreature->GetY());
			m_DestX = pCreature->GetX();
			m_DestY = pCreature->GetY();
			m_PatrolCount = m_CurrentDirection;
		}
//		// circle around the target..? -_-;
		else
		{
			if(m_DestX == GetX() && m_DestY == GetY())
			{
				m_PatrolCount ++;
				m_DestX = pCreature->GetX(); m_DestY = pCreature->GetY();
				switch (m_PatrolCount % 8)
				{
					case DIRECTION_LEFTDOWN		: m_DestX--;	m_DestY++;	break;
					case DIRECTION_RIGHTUP		: m_DestX++;	m_DestY--;	break;
					case DIRECTION_LEFTUP		: m_DestX--;	m_DestY--;	break;
					case DIRECTION_RIGHTDOWN	: m_DestX++;	m_DestY++;	break;
					case DIRECTION_LEFT			: m_DestX--;				break;
					case DIRECTION_DOWN			:			m_DestY++;	break;
					case DIRECTION_UP			:			m_DestY--;	break;
					case DIRECTION_RIGHT		: m_DestX++;				break;
				}
			}
			m_DirectionMove = m_Direction = m_CurrentDirection = GetDirectionToPosition(m_DestX, m_DestY);
		}
	}

	if( m_Action != ACTION_MOVE )
		SetAction(m_MoveAction);

			

}
// 2004, 11, 8, sobeit add end

void MFakeCreature::SetFakeMoveOnce(BYTE Dir)
{
	m_DirectionMove = m_Direction = m_CurrentDirection = Dir%8;
	SetAction(m_MoveAction);
}

void
MFakeCreature::KeepWildWolfAttack()
{
	if(!ZoneState::Exists())
		return;
	
	if(m_PatrolCount++ < 4 ) return;
	if(m_IsEatCorps)
	{
		if((GetX() == m_DestX && GetY() ==m_DestY) || m_WildWolf_Mode == 0 ) 
		{
			switch(m_WildWolf_Mode)
			{
				case WILD_WOLF_MODE_ATTACK_1st:
				case WILD_WOLF_MODE_ATTACK_2nd:
				case WILD_WOLF_MODE_ATTACK_3rd:
				{
					int TempDir = SelectFacingDirection(GetX(), GetY(), m_DestX, m_DestY);
					POINT TargetPoint = GetNextTileByDirection(m_DestX, m_DestY, TempDir);
					m_bTraceFlag = true;
					SetCurrentDirection(TempDir);
					SetFakeCreatureFastMoveAction(static_cast<TYPE_SECTORPOSITION>(TargetPoint.x), static_cast<TYPE_SECTORPOSITION>(TargetPoint.y), ACTIONINFO_NULL, 0);	
					//SetNextDestination(TargetPoint.x, TargetPoint.y);
					m_DestX = static_cast<TYPE_SECTORPOSITION>(TargetPoint.x);
					m_DestY = static_cast<TYPE_SECTORPOSITION>(TargetPoint.y);
					m_WildWolf_Mode ++;
					m_DirectionMove = TempDir;
					if( m_Action != ACTION_MOVE )
						SetAction(ACTION_MOVE);
					m_PatrolCount = 0;
				}
				break;
				case WILD_WOLF_MODE_MOVE_1st:
				case WILD_WOLF_MODE_MOVE_2st:
				{
					int TempDir = (GetDirection()+4)%8;
					SetDirection(TempDir);
					SetAction(ACTION_VAMPIRE_DRAIN);
					m_PatrolCount = 0;
					m_WildWolf_Mode = WILD_WOLF_MODE_REMOVE;
				}
				break;
			}
			
		}
	}
	else
	{
		MCreature *pCreature = ZoneState::GetCreature(m_TraceID);
		if(NULL != pCreature)
		{
			if((GetX() == m_DestX && GetY() ==m_DestY) || m_WildWolf_Mode == 0 )
			{
				switch(m_WildWolf_Mode)
				{
				case WILD_WOLF_MODE_ATTACK_1st:
				case WILD_WOLF_MODE_ATTACK_2nd:
				case WILD_WOLF_MODE_ATTACK_3rd:
					{
						int TempDir = SelectFacingDirection(GetX(), GetY(), pCreature->GetX(), pCreature->GetY());
						POINT TargetPoint = GetNextTileByDirection(pCreature->GetX(), pCreature->GetY(), TempDir);
						m_bTraceFlag = true;
						SetCurrentDirection(TempDir);
						SetFakeCreatureFastMoveAction(static_cast<TYPE_SECTORPOSITION>(TargetPoint.x), static_cast<TYPE_SECTORPOSITION>(TargetPoint.y), ACTIONINFO_NULL, pCreature->GetID());	
						//SetNextDestination(TargetPoint.x, TargetPoint.y);
						m_WildWolf_Mode ++;
						m_DestX = static_cast<TYPE_SECTORPOSITION>(TargetPoint.x);
						m_DestY = static_cast<TYPE_SECTORPOSITION>(TargetPoint.y);
						m_DirectionMove = TempDir;
						if( m_Action != ACTION_MOVE )
							SetAction(ACTION_MOVE);
						m_PatrolCount = 0;
					}
					break;
				case WILD_WOLF_MODE_MOVE_1st:
				case WILD_WOLF_MODE_MOVE_2st:
					{
						int TempDir = SelectFacingDirection(GetX(), GetY(), pCreature->GetX(), pCreature->GetY());
						TempDir = (TempDir +7)%8;
						POINT TargetPoint = GetNextTileByDirection(GetX(), GetY(), TempDir );
						m_bTraceFlag = true;
						SetCurrentDirection(TempDir);
						SetFakeCreatureFastMoveAction(static_cast<TYPE_SECTORPOSITION>(TargetPoint.x), static_cast<TYPE_SECTORPOSITION>(TargetPoint.y), ACTIONINFO_NULL, pCreature->GetID());	
						//SetNextDestination(TargetPoint.x, TargetPoint.y);
						m_WildWolf_Mode ++;
						m_DestX = static_cast<TYPE_SECTORPOSITION>(TargetPoint.x);
						m_DestY = static_cast<TYPE_SECTORPOSITION>(TargetPoint.y);
						m_DirectionMove = TempDir;
						if( m_Action != ACTION_MOVE )
							SetAction(ACTION_MOVE);
						m_PatrolCount = 0;
					}
					break;
				}
				
			}
		}
		else
		{
			m_WildWolf_Mode = WILD_WOLF_MODE_REMOVE;
		}
		
	}
}

