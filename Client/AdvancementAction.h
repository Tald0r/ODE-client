#pragma once

// An advanced (second class) creature's actions (task 4.239): the plain
// action converted to the advanced one, by race, and each advanced
// action's frame count. Advanced actions start at ADVANCEMENT_ACTION_START
// and pass through unchanged; -1 is an action with no advanced form.

#include "../basic/Platform.h"
#include "MTypeDef.h"
#include "ItemClassDef.h"

class MCreature;

// The slayer's, by the item class in the right hand.
int ConvAdvancementSlayerAction( int CurAction, ITEM_CLASS rightHand );
// The ousters', by whether a chakram is in the right hand.
int ConvAdvancementOustersActionFromOusterAction( int CurAction, bool bChakram );
// The vampire's (the creature is not read).
int GetAdvancementVampireActionFromVampireAction( int CurAction, const MCreature* pCreature );

// Each advanced action's frame count, indexed from ADVANCEMENT_ACTION_START;
// the executable fills them from the view's frame packs at start-up.
extern BYTE g_AdvanceVampireActionMaxCount[ ACTION_ADVANCEMENT_MAX - ACTION_ADVANCEMENT_STOP ];
extern BYTE g_AdvanceSlayerActionMaxCount[ ACTION_ADVANCEMENT_SLAYER_MAX - ACTION_ADVANCEMENT_SLAYER_STOP_SWORD ];
extern BYTE g_AdvanceOustersActionMaxCount[ ACTION_ADVANCEMENT_OUSTERS_MAX - ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_STOP ];
