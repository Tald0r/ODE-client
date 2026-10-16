//-----------------------------------------------------------------------------
// GameUIBuilders.h
//-----------------------------------------------------------------------------
// The records GameUI's calls build and hand to windows that need the data's
// sprite packs: the title's character slots (UI_SetCharacter) and the other
// player's info (UI_RunOtherInfo). Built apart so a test can read them.
//-----------------------------------------------------------------------------

#ifndef	__GAMEUIBUILDERS_H__
#define	__GAMEUIBUILDERS_H__

#include "VS_UI_GameCommon.h"	// S_SLOT, C_VS_UI_OTHER_INFO

class PCSlayerInfo;
class PCVampireInfo;
class PCOustersInfo;
class MCreature;

// The slot of the character called name. Each also gives g_StatusManager
// the character's combat stats (SetCharacterSelectSlot), which the slot
// reads back.
S_SLOT		MakeCharacterSlot(const char* name, const PCSlayerInfo* pInfo);
S_SLOT		MakeCharacterSlot(const char* name, const PCVampireInfo* pInfo);
S_SLOT		MakeCharacterSlot(const char* name, const PCOustersInfo* pInfo);

// The record's scalars have no initialiser, so it is filled, not made: what
// the creature does not give (a nameless creature's name, a non-slayer's
// face) stays as the caller left it.
void		FillOtherInfo(C_VS_UI_OTHER_INFO::PLAYER_INFO& info, MCreature* pCreature);

#endif
