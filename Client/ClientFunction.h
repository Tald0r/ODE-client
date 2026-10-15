//-----------------------------------------------------------------------------
// ClientFunction.h
//-----------------------------------------------------------------------------

#ifndef __CLIENTFUNCTION_H__
#define __CLIENTFUNCTION_H__

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include "MTypeDef.h"
#include "ItemClassDef.h"

class MItem;

extern const char*	GetWhisperID();

extern void			PlaySound(TYPE_SOUNDID soundID);

extern void			AddNewInventoryEffect(TYPE_OBJECTID id, TYPE_ACTIONINFO ai, DWORD delayFrame, DWORD value = 0);

//---------------------------------------------------------------------------
// Draw AlphaBox
//---------------------------------------------------------------------------
// The UI's translucent box on its back surface, within g_GameRect: r and b
// 0~31, g 0~63, alpha 0~31 (VS_UI_Base.cpp, over SpriteLib's AlphaBox.h).
// Declared here for gamemodel's callers; VS_UI defines it.
//---------------------------------------------------------------------------
extern void		DrawAlphaBox(RECT* pRect, BYTE r, BYTE g, BYTE b, BYTE alpha);

// 0 not, 1 safe for the player's race, 2 safe for everyone.
extern BOOL		IsPlayerInSafePosition();

// Where the item of the class and type made from pItem goes in the
// inventory; false for nowhere.
extern bool		GetMakeItemFitPosition(MItem* pItem, ITEM_CLASS itemClass, int itemType, POINT& fitPoint);

#endif
