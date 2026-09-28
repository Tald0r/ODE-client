//----------------------------------------------------------------------
// MFakeCreature.h
//----------------------------------------------------------------------
// sector에는 존재하지 않는 애덜..
//
//
//----------------------------------------------------------------------

#ifndef __MFAKECREATURE_H__
#define	__MFAKECREATURE_H__

#include "MCreatureWear.h"
#include "MonotonicClock.h"
#include <list>
#include <queue>
#include "MemoryPool.h"

#ifndef __DNODE_DEFINE__
#define __DNODE_DEFINE__
//----------------------------------------------------------------------
//
// 길찾기에 이용할 Direction Node
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
// 길찾기할 때 더 나은 Node를 선택하는 비교 연산
//
//----------------------------------------------------------------------
// [1] 거리가 가까운 것
// [2] 움직인 회수가 적은 것
// [3] 이전 방향과 같은 것
class Comparison {
	public :
		// distance가 적을 걸 선택해야 한다.		
		// true : right를 선택한다.
		// false : left를 선택한다.
		bool operator () (DNode * left, DNode * right) const
		{ 
			int diff = left->distance - right->distance;			
			int s = left->step - right->step;

			// 거리가(diff) 같은 경우..
			if (diff==0)
			{			
				// 움직인 회수가 같은 경우
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
				// 움직인 회수가 적은 것
				else if (s>0) 
				{
					return true;
				}
				
				return false;				
			}			
			// 거리가 적은 것
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
			FAKE_CREATURE_STAND,			// 가만히 서 있는 것
			FAKE_CREATURE_WANDER,			// 방황
			FAKE_CREATURE_FADE_ACTION,		// 흐릿모양의 한가지 동작 후에 사라짐.
			FAKE_CREATURE_TO_PORTAL,		// 포탈로 들어가는 캐릭터(뱀파)			
			FAKE_CREATURE_TURNING_MOVE,		// 빙글빙글 이동(테스트)
			FAKE_CREATURE_FAST_MOVE_ACTION,	// 빨리 이동해서 공격함하기 [새기술]
			FAKE_CREATURE_BRIGHTNESS,		// 밝게 찍기
			FAKE_CREATURE_TRACE,			// 추적
			FAKE_CREATURE_GHOST,			// 질드레 레어 유령
			FAKE_CREATURE_ROCKET,			// 로켓 런쳐
			FAKE_CREATURE_WILD_WOLF,		// 와일드 울프 공격
			FAKE_CREATURE_DRAGON_TORNADO,	// 드레곤 토네이도
		};

		enum WILD_WOLF_ATTACK_MODE // 차례대로
		{
			 WILD_WOLF_MODE_ATTACK_1st = 0, // 첫번쨰 공격 
			 WILD_WOLF_MODE_MOVE_1st,		// 이동
			 WILD_WOLF_MODE_ATTACK_2nd,		// 두번째 공격
			 WILD_WOLF_MODE_MOVE_2st,		// 이동
			 WILD_WOLF_MODE_ATTACK_3rd,		// 세번째 공격
			 WILD_WOLF_MODE_REMOVE,			// 제거
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
		//         길찾기
		//
		//----------------------------------------------------------
		// Zone상에서의 목표위치를 설정
		bool	SetNextDestination(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
		bool	SetDestination(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);		
		bool	IsNextDirectionNULL()		{ return m_listDirection.empty(); }
		void	SetStop();		
		
		// get
		void	GetDestination(POINT &dest);
		void	GetNextDestination(POINT &dest);
		bool	GetNextPosition(POINT &next);
		BYTE	GetNextDirection()	const			{ return m_listDirection.front(); }

		// list node를 지운다.
		void	RemoveNodes();

		// 목표위치까지의 거리 계산
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

		// [새기술]
		//void				SetActionResult(MActionResult* pResult);

		// [새기술]
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

		// 좌표.. 뭔가에 사용할 - -;
		TYPE_SECTORPOSITION		m_FakeX;
		TYPE_SECTORPOSITION		m_FakeY;
		float					m_FakeSX;
		float					m_FakeSY;
		float					m_FakeCX;
		float					m_FakeCY;
		DWORD					m_Delay;
		
		TYPE_OBJECTID			m_OwnerID;		// Pet에 사용한다
		bool					m_bTraceFlag;
		
		MPetItem*				m_pPetItem;

		static DWORD			m_FakeID;		

		DIRECTION_LIST	m_listDirection;

		TYPE_SECTORPOSITION			m_DestX;
		TYPE_SECTORPOSITION			m_DestY;					// 목표위치(Sector)

		TYPE_SECTORPOSITION			m_NextDestX; 
		TYPE_SECTORPOSITION			m_NextDestY;				// 다음 목표위치(Sector)

		TYPE_SECTORPOSITION			m_BlockDestX;				// 못 갔던 곳.. 
		TYPE_SECTORPOSITION			m_BlockDestY;				// 

		// 움직이는데 필요한 것들..
		typedef std::priority_queue<DNode*, std::vector<DNode*>, Comparison>	DNODE_PQ;
		typedef	std::list<DNode*>												DNODE_LIST;

		DNODE_PQ			m_pqDNodes;
		DNODE_LIST			m_listDNodes;

		// 계산 했던 곳인지 check
		MonotonicClock::TimePoint	m_nextMoveTime;
		int							m_PatrolCount;
		BYTE						m_TurretDirect;
		BYTE						m_FinalTurretDirect;
		BYTE						m_TurretDelay;

		// 와일드 울프
		BYTE						m_WildWolf_Mode;
		bool						m_IsEatCorps; // false : 공격, true : 시체 먹기

};


#endif


