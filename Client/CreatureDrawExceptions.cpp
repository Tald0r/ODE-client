//----------------------------------------------------------------------
// CreatureDrawExceptions.cpp
//----------------------------------------------------------------------
// How some creature types are drawn differently from their action: which
// action, frame and direction to draw (task 4.229, gamemodel's since then;
// MTopviewExceptionProc.cpp before). MTopView looks a creature's type up in
// the table MapCreatureDrawExceptions fills, and calls the procedure on the
// action, frame and direction it is about to draw.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif
#include <algorithm>
#include <cassert>
#include "CreatureDrawExceptions.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "MEventManager.h"
#include "MStatus.h"
#include "SoundDef.h"

void SweeperKeeperProc( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )	// 672
{
	if( action == ACTION_MOVE )
	{
		frame = (currentFrame + CreatureState::GetID(pCreature)) % 11;		
		
		if( (currentFrame % 14 ) == 0 )
			WorldEvents::PlaySound( SOUND_SWEEPER_REPEAT );		
	}
	direction = 2;
}

void GildreProc( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	(void)direction;
	if( CreatureState::HasEffectStatus(pCreature, EFFECTSTATUS_GDR_FLOATING)/* && action == ACTION_MOVE */)
	{
		action = ACTION_VAMPIRE_DRAIN;
		frame = 6 + (frame & 0x07);		// the drain action's repeating frames start at 6		
		
		//			m_MoveAction = ACTION_DRAINED;
		//SetAction(ACTION_DRAINED);
	}
	if(CreatureState::IsDead(pCreature))
	{
		if(CreatureState::GetActionCount(pCreature)>=CreatureState::GetActionCountMax(pCreature))
		{
			if(CreatureState::GetCreatureType(pCreature)  == 723)
				frame = 46; // left kneeling			
		}
	}
}

void FrozenGildreProc( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)action;
	(void)frame;
	if(currentFrame&0x01)
	{
		CreatureState::SetDirection(pCreature, (direction+1)%8);
		direction = CreatureState::GetDirection(pCreature);
	}
}

void FishShopLandLord( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	(void)pCreature;
	(void)direction;
	// The fish shop's landlord, for a quest: lying down for now (to be
	// revisited once analysed).
	action = 6;
	frame = 14;
}

void HornProc( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	(void)frame;
	if(!CreatureState::HasEffectStatus(pCreature, EFFECTSTATUS_COMA) && !CreatureState::IsDead(pCreature))
	{
		action = ACTION_STAND;
	}
	else
		action = ACTION_DIE;
	
	direction = 2;
}

void TepezProc( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	(void)frame;
	if(!CreatureState::HasEffectStatus(pCreature, EFFECTSTATUS_COMA) && !CreatureState::IsDead(pCreature))
		action = ACTION_STAND;
	else
		action = ACTION_DIE;
	direction = 1;
}

void OustersNPC_675( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	(void)pCreature;
	action = ACTION_MAGIC;
	frame = 0;
	direction = 3;
}

void WayPointKindOfThree( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	if(CreatureState::IsDead(pCreature)) return;
	action = ACTION_STAND;
	frame = 0;
	direction = 2;

}

void ObstacleProc( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	if(CreatureState::IsDead(pCreature))
	{
		action = ACTION_DIE;
		direction = 2;
		if(CreatureState::GetActionCount(pCreature)>=CreatureState::GetActionCountMax(pCreature)) return;
	}
	else
	{
		action = ACTION_STAND;
		frame = 0;
		direction = 2;
	}
}

void MakeStandAndSouthDirectionProc( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	(void)pCreature;
	// The stand action facing south, at frame 0.
	action = ACTION_STAND;
	frame = 0;
	direction = 2;
}

void CraymoreProc( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	(void)pCreature;
	action = ACTION_STAND;
	frame = 0;
	direction = 1;
}

void GuildTowerProc( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	direction = 1;
	if(CreatureState::IsAlive(pCreature))
	{
		action = ACTION_STAND;
		frame = 0;
	}
}

void SummonGoreGrand( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	(void)frame;
	direction = 2;
	if(CreatureState::IsDead(pCreature))
	{
		action = ACTION_DIE;
		if(CreatureState::GetActionCount(pCreature)>=CreatureState::GetActionCountMax(pCreature)) return;		
	}
}

void Ctype_764_765_Proc( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	if(action == ACTION_DIE && CreatureState::GetCreatureType(pCreature) == 765)
		direction = 2;
	if(action != ACTION_MAGIC)
		CreatureState::SetShadowCount(pCreature, 0);
	if(action == ACTION_MOVE)
	{
		if(frame == 1 || frame == 8) // the feet land on frames 3 and 10; two frames early roughly keeps it in sync
		{
			MEvent event;
			event.eventID = EVENTID_METEOR_SHAKE;
			event.eventType = EVENTTYPE_ZONE;
			event.eventDelay = 300;
			event.eventFlag = EVENTFLAG_SHAKE_SCREEN;
			event.parameter3 = 1;
			g_pEventManager->AddEvent(event);
		}
	}
	else if(action == ACTION_DRAINED)
	{
		action = ACTION_ATTACK;
		if(frame>4)
		{
			frame = 4;
			
			if(currentFrame&0x01)
			{
				direction = ((direction - 1 ) + 8)%8;
				CreatureState::SetCurrentDirection(pCreature, direction);
			}
		}
	}
}
// 2004, 12, 11, sobeit add start
void GuildWarGateProc( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	action = ACTION_STAND;
	direction = 1;
	if(CreatureState::IsAlive(pCreature))
	{
		int currentHP		= max(0,int(CreatureState::Status(pCreature)->GetHP()));
		frame = 4 -(currentHP*5/CreatureState::Status(pCreature)->GetMAX_HP());
		if(frame<0) frame = 0;
	}
	else
		frame = 4;
}
// 2004, 12, 11, sobeit add end
// 2005, 1, 17, sobeit add start - the Seal of Reason
void Ctype_792( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	(void)pCreature;
	(void)frame;
	if(action != ACTION_DIE)
		action = ACTION_STAND;
	direction = 3;
}
// 2005, 1, 17, sobeit add end
void MakeDirectionToSouth( MCreature* pCreature, int& action, int& frame, int& direction, DWORD currentFrame )
{
	(void)currentFrame;
	(void)pCreature;
	(void)action;
	(void)frame;
	direction = 2;
}

//----------------------------------------------------------------------
// The table: each creature type with its procedure (one per type).
//----------------------------------------------------------------------
void	MapCreatureDrawExceptions( std::map< DWORD, DrawCreatureExceptionProc >& procs )
{
	auto RegistExceptionProcFunction = [&procs]( DWORD objectID, DrawCreatureExceptionProc proc )
	{
		assert( procs.find( objectID ) == procs.end() );
		procs[ objectID ] = proc;
	};

	int i;
	for( i = 371; i <= 376; ++i )
		RegistExceptionProcFunction( i , MakeDirectionToSouth );

	RegistExceptionProcFunction( 433 , TepezProc );
	RegistExceptionProcFunction( 482 , MakeDirectionToSouth );

	for( i = 526; i <= 549; ++i )
		RegistExceptionProcFunction( i , MakeDirectionToSouth );
	for( i = 560; i <= 563; ++i )
		RegistExceptionProcFunction( i , MakeDirectionToSouth );
	
	RegistExceptionProcFunction( 636 , FishShopLandLord );
	RegistExceptionProcFunction( 650 , MakeDirectionToSouth );
	for( i = 660; i <= 670; ++i )
		RegistExceptionProcFunction( i , MakeDirectionToSouth );
	
	RegistExceptionProcFunction( 672 , SweeperKeeperProc );
	RegistExceptionProcFunction( 673 , MakeDirectionToSouth );
	RegistExceptionProcFunction( 675 , OustersNPC_675 );
	RegistExceptionProcFunction( 698 , HornProc );
	
	RegistExceptionProcFunction( 717 , GildreProc );
	RegistExceptionProcFunction( 721 , GildreProc );
	RegistExceptionProcFunction( 722 , FrozenGildreProc );
	RegistExceptionProcFunction( 723 , GildreProc );
	RegistExceptionProcFunction( 730 , WayPointKindOfThree );
	RegistExceptionProcFunction( 731 , WayPointKindOfThree );
	RegistExceptionProcFunction( 732 , WayPointKindOfThree );
	RegistExceptionProcFunction( 734 , GuildTowerProc );
	RegistExceptionProcFunction( 738 , MakeStandAndSouthDirectionProc );
	RegistExceptionProcFunction( 739 , MakeStandAndSouthDirectionProc );
	RegistExceptionProcFunction( 740 , CraymoreProc );
	RegistExceptionProcFunction( 741 , MakeStandAndSouthDirectionProc );
	
	for( i = 743; i <= 748; ++i )
		RegistExceptionProcFunction( i, ObstacleProc );
	
	RegistExceptionProcFunction( 753 , SummonGoreGrand );
	RegistExceptionProcFunction( 764 , Ctype_764_765_Proc );
	RegistExceptionProcFunction( 765 , Ctype_764_765_Proc );

	// 2004, 12, 11, sobeit add start
	RegistExceptionProcFunction( 726 , GuildWarGateProc );
	RegistExceptionProcFunction( 727 , GuildWarGateProc );
	RegistExceptionProcFunction( 728 , GuildWarGateProc );
	RegistExceptionProcFunction( 729 , GuildWarGateProc );
	// 2004, 12, 11, sobeit add end
	
	// 2005, 1, 17, sobeit add start - the Seal of Reason
	RegistExceptionProcFunction( 792 , Ctype_792 );
	// 2005, 1, 17, sobeit add end

}
