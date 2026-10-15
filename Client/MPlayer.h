//----------------------------------------------------------------------
// MPlayer.h
//----------------------------------------------------------------------
// A class that handles only the Player specifically
// Implements the pathfinding algorithm
//----------------------------------------------------------------------

#ifndef	__MPLAYER_H__
#define	__MPLAYER_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include <list>
#include <queue>

//----------------------------------------------------------------------
// define
//----------------------------------------------------------------------
// flags for tracing
#define	FLAG_TRACE_NULL					0
#define	FLAG_TRACE_CREATURE_BASIC		0x0001
#define	FLAG_TRACE_CREATURE_SPECIAL		0x0002
#define	FLAG_TRACE_SECTOR_BASIC			0x0004
#define	FLAG_TRACE_SECTOR_SPECIAL		0x0008
#define	FLAG_TRACE_ITEM					0x0010
#define	FLAG_TRACE_INTERACTIONOBJECT	0x0020
#define	FLAG_TRACE_SELF					0x0040
#define	FLAG_TRACE_INVENTORY			0x0080
#define	FLAG_TRACE_EFFECT				0x0100


//----------------------------------------------------------------------
// class declaration
//----------------------------------------------------------------------
class MCreature;
class MItem;
class MInteractionObject;
class Effect;
class MSlayerGear;
class MVampireGear;
class MOustersGear;

extern MSlayerGear*		g_pSlayerGear;
extern MVampireGear*	g_pVampireGear;
extern MOustersGear*	g_pOustersGear;

#include "MCreatureWear.h"
#include "MonotonicClock.h"
#include "MEffectTarget.h"
#include "MRequestMode.h"
#include "PlayerStateHost.h"

extern MonotonicClock::TimePoint	g_FrameNow;

#ifndef __DNODE_DEFINE__
#define __DNODE_DEFINE__

//----------------------------------------------------------------------
//
// Direction Node used for pathfinding
//
//----------------------------------------------------------------------
class DNode {
	public :
		DNode(int _x=0, int _y=0, int _d=0, int _ds=0, int _s=0)
		{ 
			x=_x;
			y=_y;
			direction=_d;
			distance=_ds;
			step = _s;
			pParent=NULL;
		}

		void	SetParent(DNode* pDNode)	{ pParent = pDNode; }

		int		x;
		int		y;
		int		direction;
		int		distance;
		int		step;
		DNode*	pParent;
};


//----------------------------------------------------------------------
//
// Comparison operation to pick the better Node when pathfinding
//
//----------------------------------------------------------------------
// [1] the closer distance
// [2] fewer moves made
// [3] the same as the previous direction
class Comparison {
	public :
		// must pick the one with the smaller distance.
		// true : choose right.
		// false : choose left.
		bool operator () (DNode * left, DNode * right) const
		{ 
			int diff = left->distance - right->distance;			
			int s = left->step - right->step;

			// when the distances (diff) are the same..
			if (diff==0)
			{			
				// when the number of moves is the same
				if (s==0)
				{
					// Prefer the node that carries on in its parent's direction.
					//
					// This used to look only at right, so two nodes that both
					// continued straight each compared less than the other. That is
					// not a strict weak ordering - it is not even irreflexive, since
					// a straight node compared less than itself - and push_heap trips
					// _Debug_lt_pred's "invalid comparator" check on it, so walking
					// far enough for two such nodes to meet in the open set crashed
					// a debug build inside MPlayer::SetDestination.
					//
					// Compare the property on both sides instead. A node with no
					// parent is the start node and cannot be carrying on in any
					// direction, so it counts as not straight.
					const bool bLeftStraight  = (left->pParent  != NULL && left->pParent->direction  == left->direction);
					const bool bRightStraight = (right->pParent != NULL && right->pParent->direction == right->direction);

					return !bLeftStraight && bRightStraight;	// choose right only when it alone is straight
				}
				// fewer moves made
				else if (s>0) 
				{
					return true;
				}
				
				return false;				
			}			
			// smaller distance
			else if (diff>0) return true; 

			return false;
		}
};
#endif

//----------------------------------------------------------------------
//
// MPlayer class
//
//----------------------------------------------------------------------
class MPlayer : public MCreatureWear, public MRequestMode {
	public :
		typedef	std::list<BYTE>		DIRECTION_LIST;

		enum REQUEST_MODE
		{
			REQUEST_NULL,
			REQUEST_TRADE,
			REQUEST_PARTY
		};

		enum ATTACK_MODE 
		{
			ATTACK_MODE_PEACE,		// attacks no one
			ATTACK_MODE_NORMAL,		// attacks only enemies(!)
			ATTACK_MODE_AGGRESS,	// attacks anyone
			ATTACK_MODE_GUILD,		// only our guild is on our side
		};

		// What an item waiting on the server is for (PlayerStateHost.h).
		using ITEM_CHECK_BUFFER = PlayerState::ITEM_CHECK_BUFFER;
		using enum PlayerState::ITEM_CHECK_BUFFER;

		// What the player waits for the server to verify (PlayerStateHost.h).
		using WAIT_VERIFY = PlayerState::WAIT_VERIFY;
		using enum PlayerState::WAIT_VERIFY;

	public :
		MPlayer();
		~MPlayer();

		void* operator new( size_t size )
		{
			return ::operator new( size );
		}

		void operator delete( void* pmem )
		{
			::operator delete ((MPlayer*)pmem);
		}
		
		virtual CLASS_TYPE	GetClassType() const	{ return CLASS_PLAYER; }

		class MPlayerGear*	GetGear() const 
		{
			switch(GetRace())
			{
			case RACE_SLAYER:
				return (MPlayerGear *)g_pSlayerGear;
				
			case RACE_VAMPIRE:
				return (MPlayerGear *)g_pVampireGear;
				
			case RACE_OUSTERS:
				return (MPlayerGear *)g_pOustersGear;

			default:
				break;
			}

			return NULL;
		}

		// set the Zone.
		void	SetZone(MZone* pZone);

		virtual void	SetCreatureType(TYPE_CREATURETYPE type);		

		// 
		virtual void		SetServerPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);

		// sight related
		void	CalculateLightSight();
		int			GetLightSight() const			{ return m_LightSight; }
		char	GetTimeLightSight()	const		{ return m_TimeLightSight; }
		void	SetTimeLightSight(char s);
		void	SetItemLightSight(int s);
		int		GetItemLightSight() const			{ return m_ItemLightSight; }
		void	SetSight(char s)				{ m_Sight = s; }
		char	GetSight()	const			{ return m_Sight; }
		
		// action taken~
		void	Action();

		// set the next action..
		void	SetNextAction(BYTE action);	
		void	SetNextActionToMove();

		// 
		void	SetAction(BYTE action);

		// temporary, to remove the vampire drain delay
		void	StopBloodDrain();
		void	StopAbsorbSoul();	// absorb soul

		//----------------------------------------------------------
		// EffectStatus
		//----------------------------------------------------------
		bool	AddEffectStatus(EFFECTSTATUS status, DWORD delayFrame);
		bool	RemoveEffectStatus(EFFECTSTATUS status);		

		//----------------------------------------------------------
		// change status values
		//----------------------------------------------------------
		void	SetStatus(DWORD n, DWORD value);
		void	CalculateStatus();
		
		void	CalculateSight();
		
		//----------------------------------------------------------
		// transform
		//----------------------------------------------------------
		bool	ChangeToSlayer();
		bool	ChangeToVampire();

//		//----------------------------------------------------------
//		// sight
//		//----------------------------------------------------------
//		void	SetLightSight(char LightSight);
//		void	UnSetLightSight();	
		
		//----------------------------------------------------------
		// is it in a safe zone?
		//----------------------------------------------------------
		BOOL	IsInSafeSector() const;
		
		//----------------------------------------------------------
		// alive/dead
		//----------------------------------------------------------
		void	SetAlive();		// comes back to life? (sigh)
		void	SetDead();		// dies.

		//----------------------------------------------------------
		// change outfit
		//----------------------------------------------------------
		bool	SetAddonItem( MItem* pItem );
		bool	RemoveAddonItem( MItem* pItem );

		//----------------------------------------------------------
		// SoulChain failed
		//----------------------------------------------------------
		void	SetCannotTrace();
		

		//----------------------------------------------------------
		//
		//			 Item handling
		//
		//----------------------------------------------------------
		void	PickupItem(MItem* pItem);	// handle item pickup
		void	PickupItemToInventory(MItem* pItem);
		void	PickupItemToMouse(MItem* pItem);
		bool	PickupItemToQuickslot(MItem* pItem);
		void	PickupMoney(MMoney* pItem);
		void	RideMotorcycle(MMotorcycle* pMotorcycle);		

		//----------------------------------------------------------
		//
		//            related to Server Packet
		//
		//----------------------------------------------------------
		// there is no Move Packet being waited for.
		BYTE	GetSendMove() const		{ return m_SendMove; }
		void	ResetSendMove();
		
		//----------------------------------------------------------
		// state of waiting for verification from the server
		//----------------------------------------------------------
		BOOL	IsWaitVerifyNULL() const	{ return m_WaitVerify==WAIT_VERIFY_NULL; }
		BOOL	IsWaitVerify() const		{ return m_WaitVerify!=WAIT_VERIFY_NULL; }
		WAIT_VERIFY	GetWaitVerify() const	{ return m_WaitVerify; }
		TYPE_ACTIONINFO GetWaitVerifyActionInfo() const { return m_WaitVerifyActionInfo; }
		void	SetWaitVerifyNULL()			{ m_WaitVerify=WAIT_VERIFY_NULL; m_WaitVerifyActionInfo=ACTIONINFO_NULL; }
		void	SetWaitVerify(WAIT_VERIFY wv, TYPE_ACTIONINFO ai=ACTIONINFO_NULL)	{ m_WaitVerify=wv; m_WaitVerifyActionInfo=ai; }

		//----------------------------------------------------------
		// packet handling
		//----------------------------------------------------------
		//void	PacketBack();
		void	PacketMoveOK(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, BYTE direction);
		void	PacketMoveNO();
		void	PacketSpecialActionResult(TYPE_ACTIONINFO nResultActionInfo, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, BYTE temp=0);
		bool	PacketAddActionResult(WORD effectID, MActionResult* pResult);	// set the result

		bool	FastMovePosition(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, bool server = false);	// moves quickly (sigh) even on a Zone Sector.

		bool	KnockBackPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, BYTE Action = 0);

		//----------------------------------------------------------
		//
		//           tracing
		//
		//----------------------------------------------------------
		bool	SelfSpecialAction();
		// traces another Creature and takes an Action.
		bool	TraceCreatureToBasicAction(TYPE_OBJECTID id, bool bForceAttack=false, bool	bClick = false);
		bool	TraceCreatureToSpecialAction(TYPE_OBJECTID id, bool bForceAttack=false);
		bool	TraceSectorToBasicAction(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
		bool	TraceSectorToSpecialAction(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
		bool	TraceItem(TYPE_OBJECTID id);
		bool	TraceInteractionObject(TYPE_OBJECTID id, BYTE currentAction);
		bool	TraceEffect(TYPE_OBJECTID id);
		bool	TraceInventoryItem(TYPE_OBJECTID id);

		// keep tracing the creature
		void	KeepTraceCreature();

		// stop all tracing
		void	TraceNULL()				{ m_fTrace=FLAG_TRACE_NULL; m_fNextTrace=FLAG_TRACE_NULL; }
		void	TraceNextNULL()			{ m_fNextTrace=FLAG_TRACE_NULL; } 

		// what is the usable distance of nActionInfo?
		int		GetActionInfoRange(TYPE_ACTIONINFO nActionInfo);
		
		//----------------------------------------------------------
		//
		//         pathfinding
		//
		//----------------------------------------------------------
		// set the target position in the Zone
		bool	SetMovePosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
		bool	SetNextDestination(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
		bool	SetDestination(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);		
		bool	IsNextDirectionNULL()		{ return m_listDirection.empty(); }
		void	SetStop();		
		
		// get
		void	GetDestination(POINT &dest);
		void	GetNextDestination(POINT &dest);
		bool	GetNextPosition(POINT &next);
		BYTE	GetNextDirection()	const			{ return m_listDirection.front(); }

		//----------------------------------------------------------
		// tracing
		//----------------------------------------------------------
		BYTE			GetBasicAttackDistance() const	{ return m_BasicAttackDistance; }
		void			SetBasicAttackDistance(BYTE bad) { m_BasicAttackDistance = (bad==0)?1:bad; }
		TYPE_OBJECTID	GetTraceID() const				{ return m_TraceID; }
		BOOL			IsTraceNULL()	const			{ return m_fTrace==0; }
		BOOL			IsTraceCreature()	const		{ return (m_fTrace & FLAG_TRACE_CREATURE_BASIC) || (m_fTrace & FLAG_TRACE_CREATURE_SPECIAL); }
		BOOL			IsTraceSector()	const			{ return (m_fTrace & FLAG_TRACE_SECTOR_BASIC) || (m_fTrace & FLAG_TRACE_SECTOR_SPECIAL); }
		BOOL			IsTraceItem()	const			{ return m_fTrace & FLAG_TRACE_ITEM; }
		BOOL			IsTraceInteractionObject() const { return m_fTrace & FLAG_TRACE_INTERACTIONOBJECT; }
		BOOL			IsTraceEffect() const			{ return m_fTrace & FLAG_TRACE_EFFECT; }

		//----------------------------------------------------------
		// is it continuously tracing a Creature?
		//----------------------------------------------------------
		bool			IsKeepTraceCreature() const		{ return m_bKeepTraceCreature; }
		void			SetKeepTraceCreature()			{ m_bKeepTraceCreature = true; }
		void			UnSetKeepTraceCreature()		{ m_bKeepTraceCreature = false; }

		//----------------------------------------------------------
		// repeat the action
		//----------------------------------------------------------
		BOOL			IsRepeatAction() const			{ return m_bRepeatAction; }
		void			SetRepeatAction()				{ m_bRepeatAction = TRUE; m_RepeatCount = 0; m_RepeatTimer = MonotonicClock::Now(); }		
		void			UnSetRepeatAction();

		// change skill
		void			SetSpecialActionInfo( TYPE_ACTIONINFO n );

		// is it attacking?
		bool			OnAttacking() const;
				
		//----------------------------------------------------------
		// has the delay ended after the delay time passed?
		//----------------------------------------------------------
		bool			IsNotDelay() const		{ return m_DelayTime <= g_FrameNow; }
		bool			IsNotDeadDelay() const	{ return m_DeadDelayTime <= g_FrameNow; }
		void			SetDelay(DWORD delay)	{ m_DelayTime = g_FrameNow + MonotonicClock::Millis(delay); }
		DWORD			GetDeadDelayLast() const;

		//----------------------------------------------------------
		// remaining time to transform into a vampire
		//----------------------------------------------------------
		void			SetConversionDelay(DWORD last)		{ m_ConversionDelayTime = g_FrameNow + MonotonicClock::Millis(last); }
		MonotonicClock::TimePoint	GetConversionDelayTime() const	{ return m_ConversionDelayTime; }
		void			UnSetConversionDelay()				{ m_ConversionDelayTime = MonotonicClock::TimePoint(); }
		void			UpdateConversionTime();

		//----------------------------------------------------------
		// skill in progress..
		//----------------------------------------------------------
		void			RemoveEffectTarget(BYTE id);
		void			AddEffectTarget(MEffectTarget* pEffectTarget);
		
		//----------------------------------------------------------
		// Player's Attack Mode
		//----------------------------------------------------------
		ATTACK_MODE	GetAttackMode() const	{ return m_AttackMode; }
		bool	IsAttackModePeace() const	{ return m_AttackMode == ATTACK_MODE_PEACE; }
		bool	IsAttackModeNormal() const	{ return m_AttackMode == ATTACK_MODE_NORMAL; }
		bool	IsAttackModeAggress() const	{ return m_AttackMode == ATTACK_MODE_AGGRESS; }
		bool	IsAttackModeGuild() const	{ return m_AttackMode == ATTACK_MODE_GUILD; }
		void	SetAttackModePeace()		{ m_AttackMode = ATTACK_MODE_PEACE; }
		void	SetAttackModeNormal()		{ m_AttackMode = ATTACK_MODE_NORMAL; }
		void	SetAttackModeAggress()		{ m_AttackMode = ATTACK_MODE_AGGRESS; }
		void	SetAttackModeGuild()		{ m_AttackMode = ATTACK_MODE_GUILD; }

		// is it an AttackMode that can attack the to race?
		bool	CanAttackTribe( enum CREATURETRIBE to ) const;
		bool	CanAttackTribe( const MCreature* pCreature ) const	{ return CanAttackTribe( pCreature->GetCreatureTribe() ); }
		bool	CanAttackGuild( const MCreature* pCreature ) const;

		//----------------------------------------------------------
		// Item Check Buffer
		//----------------------------------------------------------
		bool	IsItemCheckBufferNULL()				{ return m_pItemCheckBuffer==NULL; }

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag (sub-inventory)
		void	SetItemCheckBuffer(MItem* pItem, ITEM_CHECK_BUFFER status, TYPE_OBJECTID SubItem = OBJECTID_NULL);
	#else
		void	SetItemCheckBuffer(MItem* pItem, ITEM_CHECK_BUFFER status);
	#endif
		
		void	ClearItemCheckBuffer();		// remove
		MItem*	GetItemCheckBuffer() const				{ return m_pItemCheckBuffer; }
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag (sub-inventory)
		TYPE_OBJECTID	GetItemIDCheckBufferSubInventory() const	{ return m_dwSubItemIDCheckBuffer; }
	#endif
		ITEM_CHECK_BUFFER	GetItemCheckBufferStatus() const	{ return m_ItemCheckBufferStatus; }

		// functions made on a whim (sigh)
		bool	IsItemCheckBufferPickupToInventory() const		{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY; }
		bool	IsItemCheckBufferPickupToMouse() const			{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE; }
		bool	IsItemCheckBufferPickupToQuickslot() const		{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT; }
		bool	IsItemCheckBufferPickupMoney() const			{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_PICKUP_MONEY; }
		bool	IsItemCheckBufferUseFromInventory() const		{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_USE_FROM_INVENTORY; }
		bool	IsItemCheckBufferUseFromQuickSlot() const		{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT; }
		bool	IsItemCheckBufferInsertFromInventory() const	{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_INSERT_FROM_INVENTORY; }
		bool	IsItemCheckBufferInsertFromQuickSlot() const	{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_INSERT_FROM_QUICKSLOT; }
		bool	IsItemCheckBufferDropToInventory() const		{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_DROP_TO_INVENTORY; }
		bool	IsItemCheckBufferPickupSomeFromInventory() const	{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY; }		
		bool	IsItemCheckBufferPickupFromInventory() const	{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_PICKUP_FROM_INVENTORY; }
		bool	IsItemCheckBufferTradeAdd() const				{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_TRADE_ADD; }
		bool	IsItemCheckBufferTradeRemove() const			{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_TRADE_REMOVE; }
		bool	IsItemCheckBufferSkillToInventory() const		{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY; }
		bool	IsItemCheckBufferSkillFromItem() const			{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_SKILL_FROM_ITEM; }
		bool	IsItemCheckBufferDropToZone() const				{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_DROP_TO_ZONE; }
		bool	IsItemCheckBufferDropToRelicTable() const		{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE; }
		bool	IsItemCheckBufferItemToItem() const				{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_ITEM_TO_ITEM; }
		bool	IsItemCheckBufferDropToCreature() const			{ return m_ItemCheckBufferStatus==ITEM_CHECK_BUFFER_DROP_TO_CREATURE; }
		
		//----------------------------------------------------------
		// Lock Mode
		//----------------------------------------------------------
		bool	IsLockMode() const		{ return m_bLockMode; }
		void	SetLockMode()			{ m_bLockMode = true; m_LockTimer = MonotonicClock::Now(); }
		void	UnSetLockMode()			{ m_bLockMode = false; }
		int		FindEnemy();

		//----------------------------------------------------------
		// is the player inside Darkness?
		// DarknessCount is how many tiles are visible.
		//----------------------------------------------------------
		void	CheckInDarkness();

		void	SetUndergroundCreature();
		void	SetGroundCreature();
		void	SetFlyingCreature();

		// 2004, 5, 7, sobeit add start
		void	SetPetDelay(MonotonicClock::TimePoint tp_delay) {m_PetDelayTime = tp_delay;}
		// 2004, 5, 7, sobeit add end

		// 2004, 8, 21, sobeit add start
		void	SetSweepViewValue(BYTE n ) {m_SweepVice_Value = n;}
		BYTE	GetSweepViewValue() { return m_SweepVice_Value;}
		bool	CheckRbuttonUpSkill();
		void	SetSweepPos(int x, int y) { m_SweepViceX = x; m_SweepViceY = y;	}
		void	SetTempSelectPos(int x, int y) { m_TempSelectPosX = x; m_TempSelectPosY = y;	}

		bool	IsShowAdamCloud() { return m_bShowAdamCloud;}
		void	SetShowAdamCloud(bool bShow) { m_bShowAdamCloud = bShow;}
		// 2004, 8, 21, sobeit add end
		
		// 2005, 1, 4, sobeit add start
		void	UseWildWolf_Corpse(MItem *pItem);
		// 2005, 1, 4, sobeit add end

		void	SetResurrectZoneID(int ZoneID) { m_ResurrectZoneID = ZoneID; }
		int		GetResurrectZoneID() { return m_ResurrectZoneID; }

	protected :				
		void	ActionMove();			// move
		void	ActionEffect();			// effect
		void	ActionToSendPacket();		// sends the packet..
		bool	ActionInTraceDistance();	// when within action range

		// true if the item lies on a dark tile that hides it from this player
		bool	IsItemHiddenInDarkness(const MItem* pItem) const;

		void	AffectUsedActionInfo(TYPE_ACTIONINFO nUsedActionInfo);		// applies m_nUsedActionInfo.
		void	AttachCastingEffect(TYPE_ACTIONINFO nUsedActionInfo, BOOL bForceAttach=FALSE);

		// apply the basic action to the traced creature
		void	BasicActionToCreature();

		// decide the next action
		bool	CheckBufferAction();

		// decide the repeat action
		bool	CheckRepeatAction();

		// delete the list node.
		void	RemoveNodes();

		// calculate the distance to the target position
		int		CalculateDistance(int x, int y);
		
		// sweep vice update
		void	UpdateSweepVice();
		bool	UpdateBurningSol();
		bool	UpDateInstallTurret();

	protected :		
		MonotonicClock::TimePoint	m_RepeatTimer;				// Repeat mode time limit
		MonotonicClock::TimePoint	m_LockTimer;				// Lock mode time limit

		TYPE_SECTORPOSITION			m_DestX;
		TYPE_SECTORPOSITION			m_DestY;					// target position (Sector)

		TYPE_SECTORPOSITION			m_NextDestX; 
		TYPE_SECTORPOSITION			m_NextDestY;				// next target position (Sector)

		TYPE_SECTORPOSITION			m_BlockDestX;				// a place that could not be reached..
		TYPE_SECTORPOSITION			m_BlockDestY;				// 

		// number of move packets being waited for
		BYTE					m_SendMove;

		// attackable distance of the Basic attack : must not be 0. at least 1.
		BYTE					m_BasicAttackDistance;	

		// tracing
		WORD					m_fTrace;				// kind of trace
		WORD					m_fTraceBuffer;			// kind of trace (temporarily remembered)
		TYPE_OBJECTID			m_TraceIDBuffer;		//
		BYTE					m_TraceDistance;		// range
		BYTE					m_TraceObjectAction;	// state of the traced Object
		bool					m_bTraceCreatureToForceAttack;
		bool					m_bNextForceAttack;

		// next trace - Buffering
		WORD					m_fNextTrace;
		TYPE_OBJECTID			m_NextTraceID;
		TYPE_SECTORPOSITION		m_NextTraceX;
		TYPE_SECTORPOSITION		m_NextTraceY;
		short					m_NextTraceZ;
		BYTE					m_NextTraceObjectAction;

		// is the action repeated?
		BOOL					m_bRepeatAction;
		
		// which Creature is being continuously traced?
		bool					m_bKeepTraceCreature;

		// things needed for moving..
		typedef std::priority_queue<DNode*, std::vector<DNode*>, Comparison>	DNODE_PQ;
		typedef	std::list<DNode*>												DNODE_LIST;

		DNODE_PQ			m_pqDNodes;
		DNODE_LIST			m_listDNodes;

		DIRECTION_LIST	m_listDirection;
		DIRECTION_LIST	m_listSendDirection;

		// the delay after using a skill
		MonotonicClock::TimePoint	m_DelayTime;

		// delay after using pet looting
		MonotonicClock::TimePoint	m_PetDelayTime;

		// skill currently in use
		EFFECTTARGET_LIST			m_listEffectTarget;

		// current combat Mode
		ATTACK_MODE					m_AttackMode;

		// the delay after death - until when
		MonotonicClock::TimePoint	m_DeadDelayTime;

		// remember the Item..
		ITEM_CHECK_BUFFER			m_ItemCheckBufferStatus;
		MItem*						m_pItemCheckBuffer;

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 bag inside a bag (sub-inventory)
		DWORD						m_dwSubItemIDCheckBuffer; // add 2005, 3, 2, sobeit added  - which subinventory it was used from
	#endif

		// skill use target : effect target
		MEffectTarget*				m_pEffectTarget;

		// state of waiting for verification from the server
		WAIT_VERIFY					m_WaitVerify;
		TYPE_ACTIONINFO				m_WaitVerifyActionInfo;

		// the change to vampire - until when
		MonotonicClock::TimePoint	m_ConversionDelayTime;

		// actioninfo that does not send a packet
		// added because of flash sliding. (sigh)
		TYPE_ACTIONINFO					m_nNoPacketUsedActionInfo;

		//----------------------------------------------------
		// Lock Mode
		//----------------------------------------------------
		// stop in place and attack nearby enemies.
		bool						m_bLockMode;

		// size of the light
		char					m_TimeLightSight;
//		TYPE_SECTORPOSITION		m_TimeLightSightX;	// most recent position where the sight was Set
//		TYPE_SECTORPOSITION		m_TimeLightSightY;		
		int						m_ItemLightSight;
		int						m_LightSight;

		// size of the sight
		char					m_Sight;		

		// 2004, 8, 21, sobeit add start
		// related to enchant 130 skill
		BYTE					m_SweepVice_Value;
		TYPE_SECTORPOSITION		m_TempSelectPosX; // to use temporarily.. (sigh)
		TYPE_SECTORPOSITION		m_TempSelectPosY;
		TYPE_SECTORPOSITION		m_SweepViceX;
		TYPE_SECTORPOSITION		m_SweepViceY;
		
		bool					m_bShowAdamCloud;
		// 2004, 8, 21, sobeit add end

		int						m_ResurrectZoneID;

};


extern MPlayer*		g_pPlayer;

#endif


