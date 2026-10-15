//----------------------------------------------------------------------
// MFakeCreature.h
//----------------------------------------------------------------------
// for creatures that do not exist in the sector..
//
//
//----------------------------------------------------------------------

#ifndef __MFAKECREATURE_H__
#define	__MFAKECREATURE_H__

#include "MCreatureWear.h"
#include "MonotonicClock.h"
#include <list>
#include <queue>
#include "../basic/MemoryPool.h"

// The fake creatures' pool (MFakeCreature.cpp's).
extern MemoryPool g_FakeCreatureMemoryPool;

#ifndef __DNODE_DEFINE__
#define __DNODE_DEFINE__
//----------------------------------------------------------------------
//
// Direction Node used for path finding
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
// comparison to choose the better Node in path finding
//
//----------------------------------------------------------------------
// [1] the closer distance
// [2] the fewer moves
// [3] the same as the previous direction
class Comparison {
	public :
		// the smaller distance must be chosen.
		// true : choose right.
		// false : choose left.
		bool operator () (DNode * left, DNode * right) const
		{ 
			int diff = left->distance - right->distance;			
			int s = left->step - right->step;

			// when the distance (diff) is the same..
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
				// the fewer moves
				else if (s>0) 
				{
					return true;
				}
				
				return false;				
			}			
			// the smaller distance
			else if (diff>0) return true; 

			return false;
		}
};
#endif


class MFakeCreature : public MCreatureWear {
	typedef	std::list<BYTE>		DIRECTION_LIST;
	
	public :
		enum FAKE_CREATURE_TYPE
		{
			FAKE_CREATURE_STAND,			// standing still
			FAKE_CREATURE_WANDER,			// wandering
			FAKE_CREATURE_FADE_ACTION,		// disappears after one faded action.
			FAKE_CREATURE_TO_PORTAL,		// character entering a portal (vampire)
			FAKE_CREATURE_TURNING_MOVE,		// spinning movement (test)
			FAKE_CREATURE_FAST_MOVE_ACTION,	// move fast and attack [new skill]
			FAKE_CREATURE_BRIGHTNESS,		// draw bright
			FAKE_CREATURE_TRACE,			// trace
			FAKE_CREATURE_GHOST,			// Gilles de Rais's lair ghost
			FAKE_CREATURE_ROCKET,			// rocket launcher
			FAKE_CREATURE_WILD_WOLF,		// Wild Wolf attack
			FAKE_CREATURE_DRAGON_TORNADO,	// Dragon Tornado
		};

		enum WILD_WOLF_ATTACK_MODE // in order
		{
			 WILD_WOLF_MODE_ATTACK_1st = 0, // first attack
			 WILD_WOLF_MODE_MOVE_1st,		// move
			 WILD_WOLF_MODE_ATTACK_2nd,		// second attack
			 WILD_WOLF_MODE_MOVE_2st,		// move
			 WILD_WOLF_MODE_ATTACK_3rd,		// third attack
			 WILD_WOLF_MODE_REMOVE,			// remove
		};
	public :
		MFakeCreature();
		virtual ~MFakeCreature();

		void* operator new( size_t size )
		{
			return g_FakeCreatureMemoryPool.Alloc(size);
		}

		void* operator new(size_t size, std::align_val_t alignment)
		{
			return g_FakeCreatureMemoryPool.Alloc(size, static_cast<size_t>(alignment));
		}

		void operator delete(void* pmem, std::align_val_t) noexcept
		{
			g_FakeCreatureMemoryPool.Free(pmem);
		}

		void operator delete( void* pmem ) noexcept
		{
			g_FakeCreatureMemoryPool.Free( pmem );
		}

		//----------------------------------------------------------
		//
		//         path finding
		//
		//----------------------------------------------------------
		// set the target position within the Zone
		bool	SetNextDestination(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
		bool	SetDestination(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);		
		bool	IsNextDirectionNULL()		{ return m_listDirection.empty(); }
		void	SetStop();		
		
		// get
		void	GetDestination(POINT &dest);
		void	GetNextDestination(POINT &dest);
		bool	GetNextPosition(POINT &next);
		BYTE	GetNextDirection()	const			{ return m_listDirection.front(); }

		// delete the list node
		void	RemoveNodes();

		// calculate the distance to the target position
		int		CalculateDistance(int x, int y);

		//------------------------------------------------------
		// class type
		//------------------------------------------------------
		virtual CLASS_TYPE	GetClassType() const	{ return CLASS_FAKE; }

		//------------------------------------------------------
		// Fake CreatureType
		//------------------------------------------------------
		FAKE_CREATURE_TYPE	GetFakeCreatureType() const		{ return m_FakeCreatureType; }
		void				SetFakeCreatureType(FAKE_CREATURE_TYPE fct);

		//------------------------------------------------------
		// Set FakePosition
		//------------------------------------------------------
		void				SetFakePosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
		TYPE_SECTORPOSITION	GetFakeX() const	{ return m_FakeX; }
		TYPE_SECTORPOSITION	GetFakeY() const	{ return m_FakeY; }

		//------------------------------------------------------
		// Action
		//------------------------------------------------------
		virtual void		Action();
		virtual void		ActionMove();
		void				ActionMovePet();
		void				KeepTraceCreature();

		void				UpdateFake();
		void				SetDelay(DWORD			Time = 0) { m_Delay = Time; }
		
		bool				IsFakeEnd();

		void				SetOwnerID(TYPE_OBJECTID id);
		TYPE_OBJECTID		GetOwnerID() const					{ return m_OwnerID; }

		static DWORD		GetFakeID();

		MPetItem*			GetPetItem()					{ return m_pPetItem; }
		void				SetPetItem(MPetItem* pItem)		{ m_pPetItem = pItem; }

		// [new skill]
		//void				SetActionResult(MActionResult* pResult);

		// [new skill]
		void				PacketSpecialActionToOther(TYPE_ACTIONINFO nActionInfo, TYPE_OBJECTID id, MActionResult* pActionResult);
		void				SetFakeCreatureFastMoveAction(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY,
											 TYPE_ACTIONINFO skillID, 
											 TYPE_OBJECTID targetID); 
											 //MActionResult* pResult);
		void				SetZone(MZone* pZone);
		static POINT		CaculatePetPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int distance, int distanceMin, MOVE_TYPE moveType, TYPE_SECTORPOSITION petX, TYPE_SECTORPOSITION petY);
		void				SyncTurretDirection();
		BYTE				GetTurretDirection() const;
		void				SetTurretFinalDirection(BYTE d);
		void				UpdateTurret();
		void				SetTurretDelay( BYTE delay ) { m_TurretDelay = delay; }
		const BYTE&			GetTurretDelay() const		{ return m_TurretDelay; }
		
		//2004, 03, 22 sobeit add start
		void				KeepWanderCreature();
		bool				IsInSector(LONG sX, LONG sY, LONG eX, LONG eY);
		//2004, 03, 22 sobeit add end

		// 2004, 11, 8, sobeit add start
		void KeepRocketLuncher();
		void SetTurreDirection(BYTE Dir) { m_TurretDirect = Dir;}
		void SetTime(MonotonicClock::TimePoint tp_time) { m_nextMoveTime = tp_time; }
		// 2004, 11, 8, sobeit add end
		
		// 2004, 12, 31, sobeit add start
		void KeepWildWolfAttack();
		void SetIsEatCorpse(bool IsEat) { m_IsEatCorps = IsEat;	}
		void SetTargetTile(WORD x, WORD y) { m_DestX = x; m_DestY = y;}
		void SetFakeMoveOnce(BYTE Dir);
		// 2004, 12, 31, sobeit add end
		
	protected :
		FAKE_CREATURE_TYPE		m_FakeCreatureType;

		// coordinates.. used for something - -;
		TYPE_SECTORPOSITION		m_FakeX;
		TYPE_SECTORPOSITION		m_FakeY;
		float					m_FakeSX;
		float					m_FakeSY;
		float					m_FakeCX;
		float					m_FakeCY;
		DWORD					m_Delay;
		
		TYPE_OBJECTID			m_OwnerID;		// used for Pet
		bool					m_bTraceFlag;
		
		MPetItem*				m_pPetItem;

		static DWORD			m_FakeID;		

		DIRECTION_LIST	m_listDirection;

		TYPE_SECTORPOSITION			m_DestX;
		TYPE_SECTORPOSITION			m_DestY;					// target position (Sector)

		TYPE_SECTORPOSITION			m_NextDestX; 
		TYPE_SECTORPOSITION			m_NextDestY;				// next target position (Sector)

		TYPE_SECTORPOSITION			m_BlockDestX;				// a place it could not reach..
		TYPE_SECTORPOSITION			m_BlockDestY;				// 

		// things needed for moving..
		typedef std::priority_queue<DNode*, std::vector<DNode*>, Comparison>	DNODE_PQ;
		typedef	std::list<DNode*>												DNODE_LIST;

		DNODE_PQ			m_pqDNodes;
		DNODE_LIST			m_listDNodes;

		// check whether it is a place already computed
		MonotonicClock::TimePoint	m_nextMoveTime;
		int							m_PatrolCount;
		BYTE						m_TurretDirect;
		BYTE						m_FinalTurretDirect;
		BYTE						m_TurretDelay;

		// Wild Wolf
		BYTE						m_WildWolf_Mode;
		bool						m_IsEatCorps; // false : attack, true : eat corpse

};


#endif


