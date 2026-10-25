//----------------------------------------------------------------------
// MCreature.h
//----------------------------------------------------------------------
// class for moving creatures
//----------------------------------------------------------------------
// UndergroundCreature
// GroundCreature
// is distinguished by FlyingCreature.
//
// All have a Z coordinate, but
// only FlyingCreature exists at a fixed height.
//----------------------------------------------------------------------

#ifndef	__MCREATURE_H__
#define	__MCREATURE_H__

#include "MTypeDef.h"
#include "CreatureNameSelection.h"
#include "MonotonicClock.h"
#include "MObject.h"
#include "MAttachEffect.h"
#include "MActionInfoTable.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MEffectStatusTable.h"
#include "MStatus.h"
#include "RaceType.h"
#include "../basic/MemoryPool.h"

// The creatures' allocator pool (MCreature.cpp's); the executable's
// creature classes have their own (MemoryPool.h).
extern MemoryPool g_CreatureMemoryPool;
#include "ZoneStateHost.h"
#include "NicknameInfo.h"
#include <string_view>
class MItem;

//----------------------------------------------------------------------
// define
//----------------------------------------------------------------------
#define	MAX_FRAME_MOVE	16

// height of flying characters (fixed)
//#define	FLYINGCREATURE_HEIGHT	0	//64

class MZone;
class MActionFrameArray;
class MActionResult;

class MCreature : public MObject, public MStatus {
	//--------------------------------------------------------------
	// enum
	//--------------------------------------------------------------
	public :
		enum CLASS_TYPE
		{
			CLASS_CREATURE,
			CLASS_CREATUREWEAR,
			CLASS_NPC,
			CLASS_FAKE,
			CLASS_PLAYER
		};

		// Creature type: underground, ground, air
		enum MOVE_TYPE
		{
			CREATURE_UNDERGROUND = 0,
			CREATURE_GROUND,
			CREATURE_FLYING,
			CREATURE_FAKE_NO_BLOCK,		// ones that do not exist in the sector..
			CREATURE_FAKE_UNDERGROUND,
			CREATURE_FAKE_GROUND,
			CREATURE_FAKE_FLYING,
		};

		// How the creature moves (ZoneStateHost.h).
		using MOVE_DEVICE = CreatureState::MOVE_DEVICE;
		using enum CreatureState::MOVE_DEVICE;

		// way of moving
		enum WEAPON_SPEED
		{
			WEAPON_SPEED_SLOW,			// slow weapon
			WEAPON_SPEED_NORMAL,			// normal weapon
			WEAPON_SPEED_FAST				// fast weapon
		};

	public :
		//----------------------------------------------------------------------
		// MoveNode
		//----------------------------------------------------------------------
		// means moving from (x,y) in the direction
		class MoveNode {
			public :
				int		x;
				int		y;
				int		direction;
		};

		typedef	std::list<MoveNode*>	MOVENODE_LIST;

		//----------------------------------------------------------------------
		// HPModifyList
		//----------------------------------------------------------------------
		class HPModify {
			public :
				HPModify(int value_modify, MonotonicClock::TimePoint value_TickCount) { modify = value_modify; TickCount = value_TickCount; }
				int		modify;
				MonotonicClock::TimePoint	TickCount;
		};

		typedef	std::list<HPModify>	HPMODIFYLIST;

	public :
		MCreature();//MActionFrameArray* pFrames);
		virtual ~MCreature();
		
		void* operator new( size_t size )
		{
			return g_CreatureMemoryPool.Alloc(size);
		}

		void* operator new(size_t size, std::align_val_t alignment)
		{
			return g_CreatureMemoryPool.Alloc(size, static_cast<size_t>(alignment));
		}

		void operator delete(void* pmem, std::align_val_t) noexcept
		{
			g_CreatureMemoryPool.Free(pmem);
		}

		void operator delete( void *pmem ) noexcept
		{
			g_CreatureMemoryPool.Free( pmem );
		}

		virtual CLASS_TYPE	GetClassType() const	{ return CLASS_CREATURE; }

		//------------------------------------------------------
		// Init
		//------------------------------------------------------
		static void	InitMoveTable();
		static void	ReleaseMoveTable();

		//------------------------------------------------------
		// returns the coordinate moved from (x,y) in direction.
		//------------------------------------------------------
		static void	GetPositionToDirection(TYPE_SECTORPOSITION &x, TYPE_SECTORPOSITION &y, BYTE direction);

		int				GetHeadSkin() const;
		//-------------------------------------------------------
		// Creature Type
		//-------------------------------------------------------
		virtual void	SetCreatureType(TYPE_CREATURETYPE type);		
		TYPE_CREATURETYPE	GetCreatureType() const				{ return m_CreatureType; }

		CREATURETRIBE	GetCreatureTribe() const				{ return (*g_pCreatureTable)[m_CreatureType].GetCreatureTribe(); }
		bool			IsPlayer() const		{ return (*g_pCreatureSpriteTable)[(*g_pCreatureTable)[m_CreatureType].SpriteTypes[m_current_sprite_index]].IsPlayerSprite() != false; }
		bool			IsPlayerOnly() const	{ return (*g_pCreatureSpriteTable)[(*g_pCreatureTable)[m_CreatureType].SpriteTypes[m_current_sprite_index]].IsPlayerOnlySprite() != false; }
		bool			IsSlayer() const		{ return (*g_pCreatureTable)[m_CreatureType].IsSlayer(); }
		bool			IsVampire() const		{ return (*g_pCreatureTable)[m_CreatureType].IsVampire(); }
		bool			IsOusters() const		{ return (*g_pCreatureTable)[m_CreatureType].IsOusters(); }
		bool			IsNPC() const			{ return (*g_pCreatureTable)[m_CreatureType].IsNPC(); }
		int				GetActionMax() const;	
		bool			IsSlayerCharacter() const;
		Race			GetRace() const			{ if(IsSlayer())return RACE_SLAYER; else if(IsVampire())return RACE_VAMPIRE; return RACE_OUSTERS; }

		// 2004, 12, 7, sobeit add start - whether it is an administrator or not
		bool			IsSlayerOperator() const { return m_CreatureType == CREATURETYPE_SLAYER_OPERATOR ;}
		bool			IsVampireOperator() const { return m_CreatureType == CREATURETYPE_VAMPIRE_OPERATOR ;}
		bool			IsOustersOperator() const { return m_CreatureType == CREATURETYPE_OUSTERS_OPERATOR ;}
		bool			IsOperator() const { return (	m_CreatureType == CREATURETYPE_SLAYER_OPERATOR
													||	m_CreatureType == CREATURETYPE_VAMPIRE_OPERATOR
													||	m_CreatureType == CREATURETYPE_OUSTERS_OPERATOR);}
		// 2004, 12, 7, sobeit add end
		//------------------------------------------------------
		// Set the IndexSet.
		//------------------------------------------------------
		int			GetCreatureColorSet() const	{ return (*g_pCreatureTable)[m_CreatureType].ColorSet; }
		
		//------------------------------------------------------
		// Set Same Body
		//------------------------------------------------------
		void		SetSameBody(const MCreature* pCreature);

		//------------------------------------------------------
		// color related...
		//------------------------------------------------------
		void		SetBodyColor1(WORD set);
		void		SetBodyColor2(WORD set);
		void		SetChangeColorSet(WORD set)	{ m_ChangeColorSet = set; }
		WORD		GetBodyColor1() const		{ return m_ColorBody1; }
		WORD		GetBodyColor2() const		{ return m_ColorBody2; }		
		WORD		GetChangeColorSet() const	{ return m_ChangeColorSet; }
		void		SetBatColor(WORD set)		{ m_BatColorSet = set; }
		WORD		GetBatColor() const			{ return m_BatColorSet; }
		// 2005, 2, 21, sobeit add start
		void		SetMasterEffectType(BYTE bType)		{ m_MasterEffectType = bType; }
		BYTE		GetMasterEffectType() const			{ return m_MasterEffectType; }
		// 2005, 2, 21, sobeit add end
		//------------------------------------------------------
		// Move Type
		//------------------------------------------------------
		MOVE_TYPE	GetMoveType() const				{ return m_MoveType; }
		void	SetMoveType(MOVE_TYPE moveType)		{ m_MoveType = moveType; }
		void	SetUndergroundCreature();
		void	SetGroundCreature();
		void	SetFlyingCreature();
		void	SetFakeCreature()				{ m_MoveType = CREATURE_FAKE_NO_BLOCK; m_Z=(*g_pCreatureTable)[m_CreatureType].FlyingHeight; }
		bool	IsUndergroundCreature()	const	{ return m_MoveType == CREATURE_UNDERGROUND || m_MoveType == CREATURE_FAKE_UNDERGROUND; }
		bool	IsGroundCreature()	const		{ return m_MoveType == CREATURE_GROUND || m_MoveType == CREATURE_FAKE_GROUND; }
		bool	IsFlyingCreature()	const		{ return m_MoveType == CREATURE_FLYING || m_MoveType == CREATURE_FAKE_FLYING; }
		bool	IsFakeCreature()	const		{ return m_MoveType == CREATURE_FAKE_NO_BLOCK || m_MoveType == CREATURE_FAKE_UNDERGROUND || m_MoveType == CREATURE_FAKE_GROUND || m_MoveType == CREATURE_FAKE_FLYING; }

		//------------------------------------------------------
		// gender
		//------------------------------------------------------
		//bool	IsMale() const					{ return (*g_pCreatureTable)[m_CreatureType].bMale; }
		//bool	IsFemale() const				{ return !(*g_pCreatureTable)[m_CreatureType].bMale; }
		void	SetMale(bool bMale=true)		{ m_bMale = bMale; }
		void	SetFemale()						{ m_bMale = false; }
		bool	IsMale() const					{ return m_bMale; }
		bool	IsFemale() const				{ return !m_bMale; }

		//------------------------------------------------------
		// height
		//------------------------------------------------------
		int		GetHeight() const				{ if (m_bAlive) return (*g_pCreatureTable)[m_CreatureType].Height; return (*g_pCreatureTable)[m_CreatureType].DeadHeight; }
		
		//------------------------------------------------------
		// way of moving
		//------------------------------------------------------	
		void		SetMoveDevice(MOVE_DEVICE md);
		MOVE_DEVICE	GetMoveDevice()	const			{ return m_MoveDevice; }

		//------------------------------------------------------
		// moving
		//------------------------------------------------------		
		virtual void	Action();			// Make it take the current action.
		virtual BOOL	IsStop();
		virtual void	SetStop();

		//------------------------------------------------------		
		// Get the next buffered movement info.
		//------------------------------------------------------		
		virtual bool	AffectMoveBuffer();		
		virtual void	AffectMoveBufferAll();		
		void			ReleaseMoveBuffer();

		//----------------------------------------------------------
		// change state value
		//----------------------------------------------------------
		virtual void	SetStatus(DWORD n, DWORD value);

		//----------------------------------------------------------
		// transformation
		//----------------------------------------------------------
		virtual bool	ChangeToSlayer();
		virtual bool	ChangeToVampire();

		//----------------------------------------------------------
		// check whether item use is possible
		//----------------------------------------------------------
		virtual void	CheckAffectStatus(MItem* pItem);	// specific item

		//------------------------------------------------------
		//	set
		//-------------------------------------x-----------------
		void	SetName(const char* pName);
		bool	MovePosition(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);	// also moves in the Zone's Sector.
		void	SetZone(MZone* pZone)		{ m_pZone = pZone; }
		void	SetPosition(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
		// the server's direction byte indexes the eight-way tables: one past them keeps the facing
		void	SetDirection(BYTE d)		{ if (d < DIRECTION_MAX) m_Direction = d; }
		void	SetCurrentDirection(BYTE d)		{ if (d < DIRECTION_MAX) m_CurrentDirection = d; }
		void	SetDirectionToPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);		
		virtual void	SetAction(BYTE action);
		//void	SetFrame(BYTE f)			{ m_ActionCount = f; }
		//void	SetZ(short z)				{ m_Z = z; }	
		
		//----------------------------------------------------------
		// set direction
		//----------------------------------------------------------
		bool	ChangeNearDirection();	// Smoothly change CurrentDirection to Direction.

		//----------------------------------------------------------
		// Server
		//----------------------------------------------------------
		virtual void		SetServerPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);

		// set next action..
		virtual void	SetNextAction(BYTE action);				
		virtual void	SetNextActionToMove();


		//------------------------------------------------------
		// get
		//------------------------------------------------------		
		const char*	GetName() const				{ return m_pName; }
		int	GetCX()	const				{ return m_cX; }	// pixel x moved at once
		int	GetCY()	const				{ return m_cY; }	// pixel y moved at once
		int	GetSX()	const				{ return m_sX; }	// X value away from the sector
		int	GetSY()	const				{ return m_sY; }	// Y value away from the sector
		TYPE_SECTORPOSITION	GetServerX() const	{ return m_ServerX; }
		TYPE_SECTORPOSITION	GetServerY() const	{ return m_ServerY; }
		BYTE		GetAction()	const			{ return m_Action; }
		BYTE		GetNextAction()	const		{ return m_NextAction; }
		BYTE		GetDirection()	const		{ return m_CurrentDirection; }
		BYTE		GetServerDirection()	const		{ return m_Direction; }
		short		GetZ() const				{ return m_Z; }
		BYTE		GetActionCount() const		{ return m_ActionCount; }
		BYTE		GetActionCountMax() const;//{ return m_ActionCountMax; }
		BYTE		GetMoveCount() const		{ return m_MoveCount; }
		BYTE		GetMoveCountMax() const		{ return m_MoveCountMax; }
		BYTE		GetFrame() const			
		{ 
			// when moving without any special action
			if (m_ActionCount>=GetActionCountMax())
			{
				// when it has finished moving and is stopped...the action must be shown.
				return (m_MoveCount<m_MoveCountMax)?m_MoveCount:
						(m_ActionCount==0)? 0 : GetActionCountMax()-1; 
			}
			// when expressing an action
			else
			{
				return (m_ActionCount<GetActionCountMax())?m_ActionCount:GetActionCountMax()-1; 				
			}
		}

		// Find the action appropriate for nActionInfo.
		int			GetActionInfoAction(TYPE_ACTIONINFO nActionInfo, bool IsSelfAction = false);
		int			GetActionInfoCastingStartFrame(TYPE_ACTIONINFO nActionInfo);
		int			GetActionInfoCastingFrames(TYPE_ACTIONINFO nActionInfo);
		int			GetActionInfoDelay(TYPE_ACTIONINFO nActionInfo);
		int			GetActionInfoActionEffectSpriteType(TYPE_ACTIONINFO nActionInfo);
		int			GetActionInfoRepeatStartFrame(TYPE_ACTIONINFO nActionInfo);
		int			GetActionInfoRepeatEndFrame(TYPE_ACTIONINFO nActionInfo);
		

		// Find the casting sound appropriate for nActionInfo.
		TYPE_SOUNDID	GetCastingSoundID(TYPE_ACTIONINFO nActionInfo);

		void		GetNextPosition(BYTE direction, POINT &next);
		//void		GetNextPosition(POINT &next);
		void		GetNextPosition(TYPE_SECTORPOSITION &sX, TYPE_SECTORPOSITION &sY);
		int			GetCounterDirection( int d );
		
		// Return the larger value of LightSight or MaxEffectLight. (the current largest sight)
		//char		GetMaxLightSight() const			{ return (m_MaxEffectLight > m_LightSight)? m_MaxEffectLight : m_LightSight; }
		BYTE		GetDirectionToPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);

		// Pixel coordinate
		// During the draw phase (g_bInterpolateDraw) these return a position
		// interpolated between the previous and current logic tick, so the
		// renderer can present smooth frames between 62 ms ticks. Logic code
		// runs with the flag off and always sees the exact tick position.
		int			GetPixelX() const;
		int			GetPixelY() const;

		// Remembers the pre-tick pixel position; called once per logic tick
		// from MZone::UpdateAllCreature before Action() moves the creature.
		void		SnapshotDrawState();

		// interpolated-minus-exact pixel gap, used to bias the camera so the
		// view scrolls smoothly with the player between ticks.
		int			GetDrawGapX() const;
		int			GetDrawGapY() const;

		// item count
		int			GetItemCount() const	{ return m_ItemCount; }
		void		SetItemCount(int c)		{ m_ItemCount = c; }

		// ( skill ID, Effect DelayFrame )
		TYPE_ACTIONINFO	GetDelayActionInfo() const		{ return m_DelayActionInfo; }
		DWORD			GetEffectDelayFrame() const		{ return m_EffectDelayFrame; }
		void			SetEffectDelayFrame(TYPE_ACTIONINFO ai, DWORD f)	{ m_DelayActionInfo=ai; m_EffectDelayFrame = f; }


		//------------------------------------------------------
		// character info
		//------------------------------------------------------
		TYPE_FRAMEID	GetCreatureFrameID(int index) const	{ return m_CreatureFrameIDs[index]; }
		int				GetCreatureFrameCount()	  const { return m_CreatureFrameIDs.GetSize();}

		BOOL		IsExistBody() const			{ return m_bExistBody; }
		
		//------------------------------------------------------
		// skill
		//------------------------------------------------------
		void				SetBasicActionInfo( TYPE_ACTIONINFO n )		{ m_nBasicActionInfo = n; }
		TYPE_ACTIONINFO		GetBasicActionInfo() const;
		virtual void		SetSpecialActionInfo( TYPE_ACTIONINFO n )	{ m_nSpecialActionInfo = n; }
		void				SetSpecialActionInfoNULL()					{ m_nSpecialActionInfo = ACTIONINFO_NULL; }
		TYPE_ACTIONINFO		GetSpecialActionInfo() const				{ return m_nSpecialActionInfo; }		
		TYPE_ACTIONINFO		GetUsedActionInfo() const					{ return m_nUsedActionInfo; }

		//----------------------------------------------------------
		// what is the skill target?
		//----------------------------------------------------------
		BOOL	IsSpecialActionInfoTargetSelf()	const	{ return (m_nSpecialActionInfo==ACTIONINFO_NULL)?FALSE : (*g_pActionInfoTable)[m_nSpecialActionInfo].IsTargetSelf(); }	
		BOOL	IsSpecialActionInfoTargetOther() const	{ return (m_nSpecialActionInfo==ACTIONINFO_NULL)?FALSE : (*g_pActionInfoTable)[m_nSpecialActionInfo].IsTargetOther(); }
		BOOL	IsSpecialActionInfoTargetZone() const	{ return (m_nSpecialActionInfo==ACTIONINFO_NULL)?FALSE : (*g_pActionInfoTable)[m_nSpecialActionInfo].IsTargetZone(); }
		BOOL	IsSpecialActionInfoTargetItem() const	{ return (m_nSpecialActionInfo==ACTIONINFO_NULL)?FALSE : (*g_pActionInfoTable)[m_nSpecialActionInfo].IsTargetItem(); }		
		

		// outfit
		virtual BOOL		IsWear() const					{ return FALSE; }

		//------------------------------------------------------
		// Chatting String
		//------------------------------------------------------
		void		SetChatString(const char *str, COLORREF color = RGB(255, 255, 255));
		void		SetPersnalString(const char *str, COLORREF color = RGB(255, 255, 255));
		const char*	GetChatString(BYTE i);
		void		ClearChatString();
		void		FadeChatString();
		COLORREF	GetChatColor()		{ return m_ChatColor; }
		COLORREF	GetOriChatColor()	{ return m_OriChatColor; }
		DWORD		GetChatTime() const	{ return m_ChatTime; }

		//------------------------------------------------
		//
		// Effect related...
		//
		// AttachEffect means an effect attached to the character's body.
		// GroundAttachEffect means an effect attached under the character's feet.
		// But AttachEffect also includes GroundAttachEffect.
		// (one of the many pieces of code that turned into a mess when added, since it was not in the original design. 1818!)
		//
		//------------------------------------------------
		bool		IsExistAttachEffect() const		{ return !m_listEffect.empty(); }
		bool		IsExistGroundAttachEffect() const		{ return !m_listGroundEffect.empty(); }
		int			GetAttachEffectSize() const		{ return static_cast<int>(m_listEffect.size()); }
		int			GetGroundAttachEffectSize() const		{ return static_cast<int>(m_listGroundEffect.size()); }
		void		ClearAttachEffect();
		

		// Among the colors expressing the various attached Effects --> the color about to be output now
		WORD		GetAttachEffectColor() const	{ return m_AttachEffectColor; }

		// add/delete sight of AttachEffect
		//void		SetLightSightAttachEffect();
		//void		UnSetLightSightAttachEffect();

		// so that work can be done with an iterator from outside..
		ATTACHEFFECT_LIST::const_iterator GetAttachEffectIterator() const { return m_listEffect.begin(); }
		ATTACHEFFECT_LIST::const_iterator GetGroundAttachEffectIterator() const { return m_listGroundEffect.begin(); }

		// Add/Update
		MAttachEffect*		CreateAttachEffect(TYPE_EFFECTSPRITETYPE type, DWORD delayFrame, DWORD linkDelayFrame, BOOL bGroundEffect=FALSE, MEffect::EFFECT_TYPE eType=MEffect::EFFECT_ATTACH);
		virtual bool		AddEffectStatus(EFFECTSTATUS status, DWORD delayFrame);
		virtual bool		RemoveEffectStatus(EFFECTSTATUS status);		
		bool				HasEffectStatus(EFFECTSTATUS status) const		{ return m_bEffectStatus[status]; }
		bool				IsAvailableEffectStatus() const		{ return m_bEffectStatus != NULL; }

		virtual void		UpdateAttachEffect();

		void				RemoveGlacierEffect();
		void				Remove_Curse_Paralsis_Effect();

		//char		GetMaxEffectLight() const		{ return m_MaxEffectLight; }

		//------------------------------------------------------
		// death..
		//------------------------------------------------------
		virtual void		SetDead();		// Shows the dying appearance.
		virtual void		SetAlive();		// Comes back to life.
		virtual void		SetCorpse();	// dead corpse state
		virtual bool		IsDead() const		{ return !m_bAlive; }
		virtual bool		IsAlive() const		{ return m_bAlive; }		
		
		//------------------------------------------------------
		// Invisible
		//------------------------------------------------------
		int					IsInvisible() const			{ return m_InvisibleCount; }
		void				SetInvisible();
		void				SetInvisibleSoon();	// Make it invisible immediately.
		void				SetVisible();
		void				SetVisibleSoon();
		int					GetInvisibleCount() const	{ return m_InvisibleCount; }
		void				UpdateInvisible();

		//------------------------------------------------------
		// Level Name
		//------------------------------------------------------
		void				SetLevelName(int ln);
		BOOL				HasLevelName() const		{ return m_NameSelection.HasLevelName(); }
		const char*			GetLevelName() const;

		//------------------------------------------------------
		// Guild
		//------------------------------------------------------
		void				SetGuildNumber(int gn)		{ m_GuildNumber = gn; }
		int					GetGuildNumber() const		{ return m_GuildNumber; }		
		
		void				SetUnionGuildID(int gn)		{ m_nUnionGuildID = gn; }
		int					GetUnionGuildID() const		{ return m_nUnionGuildID; }		

		//------------------------------------------------------
		// Weapon Speed
		//------------------------------------------------------
		WEAPON_SPEED		GetWeaponSpeed() const			{ return m_WeaponSpeed; }
		void				SetWeaponSpeed(int speed);
		void				SetWeaponSpeed(WEAPON_SPEED ws)	{ m_WeaponSpeed = ws; }

		//------------------------------------------------------
		// Head
		//------------------------------------------------------
		bool				CanRemoveHead() const			{ return (*g_pCreatureTable)[m_CreatureType].bHeadCut; }
		void				RemoveHead()					{ if (CanRemoveHead()) m_bHasHead = false; }
		bool				HasHead() const					{ return m_bHasHead; }

		//------------------------------------------------------
		// HP recovery
		//------------------------------------------------------
		// Recovers rate, times times.
		inline BOOL			IsRecoveryHP();
		inline BOOL			IsRecoveryMP();
		void				SetRecoveryHP(int amount, int times, DWORD delay);
		void				SetRecoveryMP(int amount, int times, DWORD delay);
		void				StopRecoveryHP()	{ m_RecoveryHPTimes = 0; }
		void				StopRecoveryMP()	{ m_RecoveryMPTimes = 0; }
		DWORD				GetRecoveryHPDelayTime() const		{ return m_RecoveryHPDelayTime; }
		int					GetRecoveryHPTimes() const			{ return m_RecoveryHPTimes; }
		int					GetRecoveryHPAmmount() const		{ return m_RecoveryHPAmount; }
		DWORD				GetRecoveryMPDelayTime() const		{ return m_RecoveryMPDelayTime; }
		int					GetRecoveryMPTimes() const			{ return m_RecoveryMPTimes; }
		int					GetRecoveryMPAmmount() const		{ return m_RecoveryMPAmount; }

		void				SetRegen(int amount, DWORD delay);
		void				SetRegenBonus(int amount, DWORD delay);			
		void				CheckRegen();

		void				UpdateStatus();

		//------------------------------------------------------
		// bleeding
		//------------------------------------------------------
		virtual void		CheckDropBlood();

		//------------------------------------------------------
		// length of the HP bar shown on screen
		//------------------------------------------------------
		int					GetHPBarWidth()	const				{ return (*g_pCreatureTable)[m_CreatureType].HPBarWidth; }

		//------------------------------------------------------
		// Fade
		//------------------------------------------------------
		// when the whole body is shown dark
		bool				IsFade() const						{ return m_bFade; }
		void				SetFade() 							{ m_bFade = true; }
		void				UnSetFade()							{ m_bFade = false; }

		// number of shadow images following the character's action.. - -;
		int					GetShadowCount() const				{ return m_ShadowCount; }
		void				SetShadowCount(int n);

		//------------------------------------------------------
		// yum..
		//------------------------------------------------------
		virtual int			FindEnemy();

		//------------------------------------------------------
		//
		//              Server Packet handling
		//
		//------------------------------------------------------
		virtual void		PacketSpecialActionResult(TYPE_ACTIONINFO nResultActionInfo, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, BYTE temp = 0);
		virtual void		PacketAttackNormal(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, BYTE direction);
		virtual void		PacketMove(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, BYTE direction);		
		virtual void		PacketSpecialActionToSector(TYPE_ACTIONINFO nActionInfo, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, MActionResult* pActionResult);
		virtual void		PacketSpecialActionToOther(TYPE_ACTIONINFO nActionInfo, TYPE_OBJECTID id, MActionResult* pActionResult);
		virtual void		PacketSpecialActionToNobody(TYPE_ACTIONINFO nActionInfo, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);	// when the target is not in the zone
		virtual void		PacketSpecialActionToSelf(TYPE_ACTIONINFO nActionInfo, MActionResult* pActionResult);
		virtual void		PacketSpecialActionToInventoryItem(TYPE_ACTIONINFO nActionInfo);
		
	
		//------------------------------------------------------
		// Fast Move
		//------------------------------------------------------
		bool				IsFastMove() const	{ return m_bFastMove; }
		virtual bool		FastMovePosition(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y,bool server = false);	// also in the Zone's Sector quickly(-_-;) moves.
		void				StopFastMove();

		//------------------------------------------------------
		// Knock Back
		//------------------------------------------------------
		int					IsKnockBack() const			{ return m_bKnockBack>0; }
		virtual bool		KnockBackPosition(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, BYTE Action = 0);
		void				StopKnockBack()			{ m_bKnockBack = 0; }
		
		//------------------------------------------------------
		// Casket
		//------------------------------------------------------
		bool				IsInCasket() const			{ return m_bInCasket; }
		int					GetCasketCount() const		{ return m_CasketCount; }
		int					GetCasketType() const		{ return m_CasketType; }
		void				AddCasket(int casketType);
		void				RemoveCasket();
		void				AddCasketSoon(int casketType);
		void				RemoveCasketSoon();
		void				UpdateCasket();		

		//--------------------------------------------------
		// CutHeight
		//--------------------------------------------------
		bool				IsCutHeight() const			{ return m_bCutHeight; }
		int					GetCutHeightCount() const	{ return m_CutHeightCount; }
		void				SetCutHeightFrom(int startCount, int lastCount, int inc);
		void				UpdateCutHeight();

		//--------------------------------------------------
		// Turning
		//--------------------------------------------------
		bool				IsTurning() const			{ return m_bTurning; }
		int					GetTurningCount() const		{ return m_TurningCount; }
		void				SetTurning(WORD count)		{ m_bTurning = true; m_TurningCount = count; }
		void				StopTurning()				{ m_bTurning = false; }
		void				UpdateTurning();
		
		//--------------------------------------------------
		// CauseCriticalWounds
		//--------------------------------------------------
		bool				IsCauseCriticalWounds() const			{ return m_bCauseCriticalWounds; }
		void				SetCauseCriticalWounds(int delay)		{ m_bCauseCriticalWounds = true; m_CauseCriticalWoundsCount = delay; }
		void				StopCauseCriticalWounds()				{ m_bCauseCriticalWounds = false; }
		void				UpdateCauseCriticalWounds();

		//--------------------------------------------------
		// BloodyZenith
		//--------------------------------------------------
		bool				IsBloodyZenith();
		void				SetBloodyZenith(int delay)		{ m_bBloodyZenith = true; m_BloodyZenithCount = delay; }
		void				StopBloodyZenith()				{ m_bBloodyZenith = false; }
		int					GetBloodyZenithCount();
		void				UpdateBloodyZenith();
		
		//--------------------------------------------------
		// GunShotGuidance
		//--------------------------------------------------
		bool				IsGunShotGuidance() const			{ return m_bGunShotGuidance; }
		void				SetGunShotGuidance(int delay)		{ m_bGunShotGuidance = true; m_GunShotGuidanceCount = delay; }
		void				StopGunShotGuidance()				{ m_bGunShotGuidance = false; }
		void				UpdateGunShotGuidance();

		//--------------------------------------------------
		// BurningSolCount
		//--------------------------------------------------
		bool				IsBurningSol() const			{ return m_bBurningSol; }
		void				SetBurningSol(int delay)		{ m_bBurningSol = true; m_bBurningSolCount = delay; }
		void				StopBurningSol()				{ m_bBurningSol = false; }
		bool				UpdateBurningSol();

		//--------------------------------------------------
		// InstallTurret
		//--------------------------------------------------
		BYTE				GetInstallTurretCount() const			{ return m_bInstallTurretCount; }
		void				SetInstallTurretCount(int Count)		{ m_bInstallTurretCount = Count; }
		BYTE				GetInstallTurretDirect() const			{ return m_bInstallTurretDirect; }
		void				SetInstallTurretDirect(int Count)		{ m_bInstallTurretDirect = Count; }
		bool				UpDateInstallTurret();
		void				InstallTurretStopAttack();


		//--------------------------------------------------
		// Armageddon
		//--------------------------------------------------
		bool				IsArmageddon() const			{ return m_bArmageddon; }
		void				SetArmageddon(int delay)		{ m_bArmageddon = true; m_ArmageddonCount = delay; }
		int					GetArmageddon() const			{ if(IsArmageddon())return m_ArmageddonCount; return -1; }
		void				StopArmageddon()				{ m_bArmageddon = false; }
		void				UpdateArmageddon();

		//--------------------------------------------------
		// Teleport
		//--------------------------------------------------
		bool				IsGhost(BYTE flag) const			{ return (m_fGhost&flag)?true:false; }
		void				SetGhost(BYTE flag, int delay)		{ m_fGhost = flag; m_GhostCount = delay; }
		int					GetGhost() const			{ if(IsGhost(0xFF))return m_GhostCount; return -1; }
		void				StopGhost()				{ m_fGhost = 0; }
		void				UpdateGhost();
		
		//--------------------------------------------------
		// FakeDie
		//--------------------------------------------------
		bool				IsFakeDie() const			{ return m_bFakeDie; }
		void				SetFakeDie();
		int					GetFakeDieCount()			{ return m_FakeDieCount; }
		void				StopFakeDie()				{ m_bFakeDie = false; }
		void				UpdateFakeDie();

		//----------------------------------------------------------
		// Is the player inside Darkness?
		// DarknessCount is how many tiles are visible?.
		//----------------------------------------------------------
		BOOL				IsInDarkness() const		{ return m_DarknessCount>=0; }
		BOOL				ShowInDarkness(int sX, int sY) const;
		int					GetDarknessCount() const	{ return m_DarknessCount; }
		void				UpdateDarkness();
		virtual void		CheckInDarkness();
		void				PlaceInDarkness(bool IsBlindness = false); // 2004, 6, 24 sobeit modify - because of blindness handling
		void				PlaceNotInDarkness();

		BOOL				IsInGroundElemental() const;
		//--------------------------------------------------
		// temporarily, to remove the blood-sucking delay..
		//--------------------------------------------------
		void				SetStopBloodDrain()			{ m_bStopBloodDrain++; }
		void				UnSetStopBloodDrain()		{ if (m_bStopBloodDrain>0) m_bStopBloodDrain--; }
		void				ClearStopBloodDrain()		{ m_bStopBloodDrain = 0; }
		virtual void		StopBloodDrain();

		//--------------------------------------------------
		// temporarily, to remove the soul-absorbing delay..
		//--------------------------------------------------
		void				SetStopAbsorbSoul()			{ m_bStopAbsorbSoul++; }
		void				UnSetStopAbsorbSoul()		{ if (m_bStopAbsorbSoul>0) m_bStopAbsorbSoul--; }
		void				ClearStopAbsorbSoul()		{ m_bStopAbsorbSoul = 0; }
		virtual void		StopAbsorbSoul();
		
		//--------------------------------------------------
		// party check
		//--------------------------------------------------
		bool				IsPlayerParty() const		{ return m_bPlayerParty; }
		void				SetPlayerParty()			{ m_bPlayerParty = true; }
		void				UnSetPlayerParty()			{ m_bPlayerParty = false; }

		//--------------------------------------------------
		// show as a mob when Hallucination is applied - -;
		//--------------------------------------------------		
		bool				IsHallu() const				{ return m_bHallu; }
		void				DetermineHalluActionFrame();
		void				SetHalluCreature(TYPE_CREATURETYPE type);
		void				UnSetHalluCreature()		{ m_bHallu = false; }
		TYPE_FRAMEID		GetHalluCreatureFrameID() const	{ return m_HalluCreatureFrameID; }
		TYPE_CREATURETYPE	GetHalluCreatureType() const { return m_HalluCreatureType; }
		WORD				GetHalluColor() const		{ return m_HalluColorBody; }
		BYTE				GetHalluAction() const		{ return m_HalluAction; }
		BYTE				GetHalluFrame() const		{ return m_HalluFrame; }
		const char*			GetHalluName() const;
		

		// administrator
		void				SetCompetence(BYTE c)		{ m_Competence = c; }
		BYTE				GetCompetence() const		{ return m_Competence; }

		// blood sucking
		void				SetDrainCreatureID(TYPE_OBJECTID id)	{ m_DrainCreatureID = id; }
		TYPE_OBJECTID		GetDrainCreatureID() const				{ return m_DrainCreatureID; }
		void				StopDrain();	// Stop the blood-sucking action of the Creature that was sucking my blood

		// soul absorption
		void				SetAbsorbCreatureID(TYPE_OBJECTID id)	{ m_DrainCreatureID = id; }
		TYPE_OBJECTID		GetAbsorbCreatureID() const				{ return m_DrainCreatureID; }
		void				StopAbsorb();	// Stop the soul-absorbing action of the Creature that was sucking my blood
		
		// PET
		void				SetPetID(TYPE_OBJECTID id)				{ m_PetID = id; }
		TYPE_OBJECTID		GetPetID() const						{ return m_PetID; }
		
		// Elemental
		void				SetElementalID(TYPE_OBJECTID id)				{ m_ElementalID = id; }
		TYPE_OBJECTID		GetElementalID() const						{ return m_ElementalID; }
		
		//--------------------------------------------------
		// HP Modify related
		//--------------------------------------------------		
		const HPMODIFYLIST *GetHPModifyList() const			{ return &m_HPModifyList; }
		void				AddHPModify(const int modify);
		bool				IsEmptyHPModifyList() const		{ return m_HPModifyList.empty(); }
		int					GetHPModifyListSize() const		{ return static_cast<int>(m_HPModifyList.size()); }

		void			SetActionGrade( BYTE a ) { if( a != 0 ) m_GradeActionInfo = a; }
		void			ClearActionGrade() {m_GradeActionInfo = 0;}
		BYTE			GetActionGrade() { return m_GradeActionInfo; }
		
		virtual	void	SetTraceID( TYPE_OBJECTID id );

		void			SetActionDelay(short time)			{ m_DelayLastAction = time; }
		short			GetActionDelay()		const		{ return m_DelayLastAction; }

		void			SetNickName(BYTE bType, char* szNickName) { m_NickNameType = bType; m_NickName = szNickName;}
		const string&		GetNickName() const { return m_NickName;}
		BYTE			GetNickNameType() { return m_NickNameType;}

		bool			GetIsInSiegeWar() { return (m_bSiegeWar>0)?true:false;}
		void			SetSiegeWar(BYTE bSiege) { m_bSiegeWar = bSiege; }
		bool			GetIsAttackerInSiegeWar() {return (m_bSiegeWar>2&&m_bSiegeWar<8)?true:false;}
		bool			GetIsDefenderInSiegeWar() {return (m_bSiegeWar>0&&m_bSiegeWar<3)?true:false;}
		
		BOOL			IsInFuryOfGnome() const;


		string			GetPersnalShopMessage() {return m_PersnalMessage; }
		void 			SetPersnalShopMessage(const char * str) {m_PersnalMessage = str; }

		void			SetPersnalShop(int curshop) { m_Persnal_flag = curshop; }
		int				CurPernalShop() {return m_Persnal_flag;}

		void			SetInputChatting(bool b_input) {  m_Input_Chat_Flag = b_input ;}
		bool			GetInputChatting() { return  m_Input_Chat_Flag; }

		BYTE			GetPersonalShopMsgTime() const { return m_PersonalShopOpenTime; }
		void			SetPersonalShopMsgTime( BYTE time ) { m_PersonalShopOpenTime = time;	}
		
		bool			IsAdvancementClass() const { return GetAdvancementClassLevel() > 0 && GetAdvancementClassLevel() != MODIFY_NULL; }


		// 2005, 1, 5, sobeit add start
		void		UpdateBikeCrash();
		// 2005, 1, 5, sobeit add end
		void			RemoveCauseCriticalWoundsEffect();

		// 2005, 2, 22, sobeit add start
		int 			GetMasterEffectType(DWORD Status);
		void			ChangeMasterEffectType(int MarketEffect);
		// 2005, 2, 22, sobeit add end
	protected :
		virtual void	ActionMove();
		virtual void	ActionEffect();
	
		virtual void	AffectUsedActionInfo(TYPE_ACTIONINFO nUsedActionInfo);
		virtual void	AttachCastingEffect(TYPE_ACTIONINFO nUsedActionInfo, BOOL bForceAttach=FALSE);

		virtual void	AffectBufferedActionInfo();
	

	protected :
		// Creature type
		TYPE_CREATURETYPE		m_CreatureType;
		//TYPE_FRAMEID			m_CreatureFrameID;
		CTypeTable<TYPE_FRAMEID>			m_CreatureFrameIDs;
		BOOL					m_bExistBody;
		MonotonicClock::TimePoint	m_TraceTimer;				// follow time limit

		//add by viva
		BYTE					m_MoveCount;		// number of moves made
		//end

		//--------------------------------------------------
		// color
		//--------------------------------------------------
		// PC Vampire : 1=skin, 2=clothes
		// Monster : 1=a color that changes somewhere on the body.. - -; 2=Nothing
		WORD					m_ColorBody1;		
		WORD					m_ColorBody2;	
		WORD					m_ChangeColorSet;	// when the whole mob color changes
		WORD					m_BatColorSet;
		// 2005, 2, 21, sobeit add start
		BYTE					m_MasterEffectType;
		// 2005, 2, 21, sobeit add end
		

		// Move type: underground, ground, air
		MOVE_TYPE				m_MoveType;
		short					m_Z;			// height Z

		// way of moving
		MOVE_DEVICE				m_MoveDevice;
		BYTE					m_MoveAction;

		MZone*					m_pZone;		// Zone this Creature belongs to


		// next direction to move
		TYPE_SECTORPOSITION		m_NextX;
		TYPE_SECTORPOSITION		m_NextY;
		BYTE					m_NextDirection;

		//--------------------------------------------------
		// Move Buffering
		//--------------------------------------------------
		MOVENODE_LIST			m_listMoveBuffer;
	

		// for smooth scroll between sectors
		int				m_sX, m_sY;		// remaining pixels to move
		int				m_cX, m_cY;		// horizontal pixels moved at once

		// pre-tick pixel position for draw-phase interpolation. Valid only
		// when m_DrawSnapFrame equals the current logic frame; otherwise the
		// creature was not ticked (corpse conversion, fresh spawn) and draw
		// falls back to the exact position.
		int				m_PrevPixelX, m_PrevPixelY;
		DWORD			m_DrawSnapFrame;
		
		// info needed for moving
		static int		m_sXTable[MAX_DIRECTION];
		static int		m_sYTable[MAX_DIRECTION];
		static int		*m_cXTable[MAX_FRAME_MOVE][MAX_DIRECTION];
		static int		*m_cYTable[MAX_FRAME_MOVE][MAX_DIRECTION];		

		// info about the current frame
		BYTE					m_Action;			// ID of the Action
		//add by viva for no increase speed
		//BYTE					m_CurrentDirection;	// current Direction
		//BYTE					m_Direction;		// Direction to face
		//BYTE					m_DirectionMove;	// direction to move
		//BYTE					m_DirectionMoved;	// direction moved
		//BYTE					m_ActionCount;		// ActionCount == Frame
		//BYTE					m_ActionCountMax;	// ActionCountMax
		//BYTE					m_MoveCount;		// number of moves made
		//BYTE					m_MoveCountMax;		// target number of moves
		//BYTE					m_MoveTableCount;	// count of cX,YTable
		//BYTE					m_NextMoveCount;	// count to move next


		//BYTE					m_NextAction;
		//end
		// skill type
		TYPE_ACTIONINFO			m_nBasicActionInfo;		// selected basic skill type
		TYPE_ACTIONINFO			m_nSpecialActionInfo;	// selected special skill type
		TYPE_ACTIONINFO			m_nUsedActionInfo;		// used one
		
		// skill to be used next..
		TYPE_ACTIONINFO			m_nNextUsedActionInfo;

		// info about the attack target
		TYPE_OBJECTID			m_TraceID;				// ID of the Object being followed
		TYPE_SECTORPOSITION		m_TraceX;				// tracked target position
		TYPE_SECTORPOSITION		m_TraceY;
		short					m_TraceZ;

		
		// position most recently verified by the Server
		TYPE_SECTORPOSITION			m_ServerX;	
		TYPE_SECTORPOSITION			m_ServerY;		

		// expression of Effects attached to the character
		bool*					m_bAttachEffect;
		bool*					m_bEffectStatus;
		ATTACHEFFECT_LIST		m_listEffect;
		ATTACHEFFECT_LIST		m_listGroundEffect;
		WORD					m_AttachEffectColor;	// color for changing the character color about to be output now -_-;(the wording is odd..umm)
		//BYTE					m_nAlphaEffect;		
		//char					m_MaxEffectLight;		// maximum brightness (sight) of Effect

		// chat
		COLORREF				m_ChatColor;				// current chat color
		COLORREF				m_OriChatColor;				// chat color before fading
		MonotonicClock::TimePoint	m_NextChatFadeTime;	// when the chat string starts to fade
		BYTE					m_ChatStringCurrent;
		char**					m_ChatString;
		DWORD					m_ChatTime;
		BYTE					m_PersonalShopOpenTime;
		
		// result for ActionInfo (verified by the Server)
		MActionResult*			m_pActionResult;

		// character name
		char*					m_pName;

		// alive?
		bool					m_bAlive;		

		// is there a next action?
		bool					m_bNextAction;		

		// number of items held [ TEST CODE ] - -;;
		int						m_ItemCount;
		//add by viva
		BYTE					m_DirectionMove;	// direction to move
		//end
		// set skill duration
		TYPE_ACTIONINFO			m_DelayActionInfo;
		DWORD					m_EffectDelayFrame;

		// Level title and hallucination-name selection.
		CreatureNameSelection	m_NameSelection;

		// guild
		int						m_GuildNumber;
		int						m_nUnionGuildID;
		//add by viva
		BYTE					m_Direction;		// Direction to face
		//end
		// fast move
		bool					m_bFastMove;

		// repeat count
		int						m_RepeatCount;

		// weapon attack speed
		WEAPON_SPEED			m_WeaponSpeed;

		static int				s_SlayerActionSpeed[ACTION_MAX_SLAYER][3];
		static int				s_VampireActionSpeed[ACTION_MAX_SLAYER][3];	//ACTION_MAX_VAMPIRE][3];
		static int				s_OustersActionSpeed[ACTION_MAX_SLAYER][3];	//ACTION_MAX_VAMPIRE][3];

		//--------------------------------------------------
		// HP recovery over time
		//--------------------------------------------------
		MonotonicClock::TimePoint	m_RecoveryHPNextTime;
		DWORD					m_RecoveryHPDelayTime;
		int						m_RecoveryHPTimes;
		int						m_RecoveryHPAmount;
		MonotonicClock::TimePoint	m_RecoveryMPNextTime;
		DWORD					m_RecoveryMPDelayTime;
		int						m_RecoveryMPTimes;
		int						m_RecoveryMPAmount;
		//DWORD					m_RecoveryPart;		// hp, mp?
		//add by viva
		BYTE					m_NextAction;
		//end
		// automatic regen - -;
		DWORD					m_RegenDelayTime;
		MonotonicClock::TimePoint	m_RegenNextTime;
		int						m_RegenAmount;
		DWORD					m_RegenBonusDelayTime;
		MonotonicClock::TimePoint	m_RegenBonusNextTime;
		int						m_RegenBonusAmount;
		//add by viva
		BYTE					m_ActionCountMax;	// ActionCountMax
		//end
		//--------------------------------------------------
		// has a head?
		//--------------------------------------------------
		bool					m_bHasHead;

		//--------------------------------------------------
		// dark drawing
		//--------------------------------------------------
		bool					m_bFade;
		int						m_ShadowCount;
		//add by viva
		BYTE					m_NextMoveCount;	// count to move next
		//end
		//--------------------------------------------------
		// gender
		//--------------------------------------------------
		bool					m_bMale;

		//--------------------------------------------------
		// bleeding
		//--------------------------------------------------
		MonotonicClock::TimePoint	m_NextBloodingTime;

		//--------------------------------------------------
		// KnockBack handling
		//--------------------------------------------------
		int						m_bKnockBack;	// if >0, KnockBack in progress.

		//--------------------------------------------------
		// These should all be bundled together...  someday~ --;;
		//--------------------------------------------------
		// becoming transparent
		//--------------------------------------------------
		int						m_InvisibleCount;	// if 0, fully visible. If 64, fully transparent
		int						m_InvisibleStep;
		
		//--------------------------------------------------
		// casket
		//--------------------------------------------------
		bool					m_bInCasket;
		int						m_CasketCount;
		int						m_CasketCountInc;
		int						m_CasketType;
		//add by viva
		BYTE					m_MoveTableCount;	// count of cX,YTable
		//end
		//--------------------------------------------------
		// draw all or not..
		//--------------------------------------------------
		bool					m_bCutHeight;
		int						m_CutHeightCount;
		int						m_CutHeightCountLast;
		int						m_CutHeightCountInc;

		//--------------------------------------------------
		// spinning around
		//--------------------------------------------------
		bool					m_bTurning;
		int						m_TurningCount;

		//--------------------------------------------------
		// temporary measure to remove the blood-sucking delay
		//--------------------------------------------------
		BYTE					m_bStopBloodDrain;
		BYTE					m_bStopAbsorbSoul;

		//----------------------------------------------------
		// CauseCriticalWoundsaction of being drained of blood while bound by
		//----------------------------------------------------
		bool					m_bCauseCriticalWounds; 
		int						m_CauseCriticalWoundsCount; 
		//add by viva
		BYTE					m_ActionCount;		// ActionCount == Frame
		//end
		//----------------------------------------------------
		// Bloody Zenithaction of being drained of blood while bound by
		//----------------------------------------------------
		bool					m_bBloodyZenith; 
		int						m_BloodyZenithCount; 
		
		//----------------------------------------------------
		// GunShotGuidanceshooting action while bound by
		//----------------------------------------------------
		bool					m_bGunShotGuidance; 
		int						m_GunShotGuidanceCount; 
		
		//----------------------------------------------------
		// Armageddonwhile bound by
		//----------------------------------------------------
		bool					m_bArmageddon; 
		int						m_ArmageddonCount; 
		//add by viva
		BYTE					m_CurrentDirection;	// current Direction
		//end
		//----------------------------------------------------
		// Teleportwhile bound by
		//----------------------------------------------------
		BYTE					m_fGhost; 
		int						m_GhostCount; 
		
		//----------------------------------------------------
		// action of the character disappearing
		//----------------------------------------------------
		bool					m_bFakeDie; 
		int						m_FakeDieCount; 
		
		//----------------------------------------------------
		// darkness
		//----------------------------------------------------
		int						m_DarknessCount;
		int						m_DarknessCountInc;

		//----------------------------------------------------
		// to speed up the party - player check..
		//----------------------------------------------------
		// Set before the constructor's body, whose SetStatus calls ask it.
		bool					m_bPlayerParty = false;

		//--------------------------------------------------
		// show as a mob when Hallucination is applied
		//--------------------------------------------------		
		bool					m_bHallu;
		TYPE_CREATURETYPE		m_HalluCreatureType;
		TYPE_FRAMEID			m_HalluCreatureFrameID;
		WORD					m_HalluColorBody;
		BYTE					m_HalluAction;
		BYTE					m_HalluFrame;
		short					m_DelayLastAction;					// delay when returning to the normal action after that action
		//add by viva
		BYTE					m_DirectionMoved;	// direction moved
		//end
		//--------------------------------------------------		
		// administrator
		//--------------------------------------------------		
		BYTE					m_Competence;

		//--------------------------------------------------		
		// to stop the blood-sucking action if the target dies during blood sucking.
		// the person being drained remembers the ID of the person draining them
		//--------------------------------------------------		
		TYPE_OBJECTID			m_DrainCreatureID;

		// PET ID
		TYPE_OBJECTID			m_PetID;
		TYPE_OBJECTID			m_ElementalID;
		
		//--------------------------------------------------
		// remember the amount of HP change
		//--------------------------------------------------
		HPMODIFYLIST			m_HPModifyList;		
		BYTE					m_GradeActionInfo;				// stage of the Action about to be performed.
		
		int						m_HeadSkin;
		
		BYTE					m_NickNameType;
		string					m_NickName;
		
		//--------------------------------------------------
		// whether participating in a siege..
		// 1,2 : defenders
		// 3~7 : attackers
		//--------------------------------------------------
		BYTE					m_bSiegeWar;	

		// Burning Sol
		BYTE					m_bBurningSolCount;
		bool					m_bBurningSol;

		BYTE					m_bInstallTurretCount;
		BYTE					m_bInstallTurretDirect;
		int						m_current_sprite_index;

		//add by viva for no increase speed
		BYTE					m_MoveCountMax;		// target number of moves
		//end

		string					m_PersnalMessage;
		int						m_Persnal_flag;
		bool					m_Input_Chat_Flag;

		bool					m_IsSkipMotorStand;

	private :
		void AppendChatRow(std::string_view text);

};

#endif

		
