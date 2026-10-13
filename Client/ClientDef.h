//-----------------------------------------------------------------------------
// ClientDef.h
//-----------------------------------------------------------------------------
// Client에 관련된 header file   - -;
//-----------------------------------------------------------------------------


#ifndef	__CLIENTDEF_H__
#define	__CLIENTDEF_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

//-----------------------------------------------------------------------------
// class
//-----------------------------------------------------------------------------

class CSpriteSurface;
class MTopView;
class MItem;
//#include "MMusic.h"
#include "MPlayer.h"
class ClientPlayer;
#include "MZone.h"
#include "CMessageArray.h"
//#include "UIFunction.h"
#include "PacketFunction.h"
#include "MHelpManager.h"
#include "DebugInfo.h"
#include "COrderedList.h"

//-----------------------------------------------------------------------------
// Global data
//-----------------------------------------------------------------------------
extern HWND							g_hWnd;

// DirectDraw
extern CSpriteSurface*			g_pBack;
extern CSpriteSurface*			g_pLast;

// Chat string
#define	MAX_RECEIVE	20
#ifdef	OUTPUT_DEBUG
	extern CMessageArray*			g_pDebugMessage;
#endif

extern CMessageArray*				g_pSystemMessage;

extern CMessageArray*				g_pPlayerMessage;  //鯤소斤口잚

extern CMessageArray*				g_pGameMessage;

extern CMessageArray*				g_pHelpMessage;

#include "ClientMode.h"	

//-----------------------------------------------------------------------------
// update type
//-----------------------------------------------------------------------------
enum	CLIENT_UPDATE
{
	UPDATE_OPENING,
	UPDATE_WAITUI,
	UPDATE_WAITPACKET,
	UPDATE_GAME
};


//-----------------------------------------------------------------------------
// 현재의 Client Mode에 대한 설정..
//-----------------------------------------------------------------------------
extern enum CLIENT_MODE		g_Mode;
extern enum CLIENT_MODE		g_ModeNext;


//-----------------------------------------------------------------------------
//
//					Global Function Prototypes
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// init / mode
//-----------------------------------------------------------------------------
extern BOOL		InitPlayer(int x, int y, int dir);
extern void		InitSkillTree();
extern void		SetMode(enum CLIENT_MODE mode);

//-----------------------------------------------------------------------------
// sound
//-----------------------------------------------------------------------------
extern void		PlaySound(TYPE_SOUNDID soundID, bool repeat, int x, int y);
extern void		PlaySound(TYPE_SOUNDID soundID);
extern void		StopSound(TYPE_SOUNDID soundID);

//-----------------------------------------------------------------------------
// weather
//-----------------------------------------------------------------------------
extern void		SetWeather(int weather, int level);
extern void		SetLightning(DWORD delay);

//-----------------------------------------------------------------------------
// load, zone
//-----------------------------------------------------------------------------
extern BOOL		LoadZone(int n);
extern BOOL		LoadZoneInfo(int n);
extern void		MoveZone(int n);
extern BOOL		LoadCreature(int spriteType);
extern BOOL		LoadCreatureType(int creatureType);
extern void		ReleaseUselessCreatureSPKExcept(const COrderedList<int>& listUse);

//-----------------------------------------------------------------------------
// Convert 
//-----------------------------------------------------------------------------
// ConvertDurationToFrame and ConvertDurationToMillisecond are gamemodel's.
#include "ConvertDuration.h"
extern DWORD	ConvertMillisecondToFrame(DWORD ms);
extern POINT	ConvertPositionMapToScreen(int sX, int sY);


#endif