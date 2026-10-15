#pragma once

// An advanced (second class) creature's actions (task 4.239): the plain
// action converted to the advanced one, by race, and each advanced
// action's frame count. Advanced actions start at ADVANCEMENT_ACTION_START
// and pass through unchanged; -1 is an action with no advanced form.

#include "../basic/Platform.h"
#include "MTypeDef.h"
#include "ItemClassDef.h"
#include "DrawTypeDef.h"

class MCreature;
class MCreatureWear;

// The slayer's, by the item class in the right hand.
int ConvAdvancementSlayerAction( int CurAction, ITEM_CLASS rightHand );
// The ousters', by whether a chakram is in the right hand.
int ConvAdvancementOustersActionFromOusterAction( int CurAction, bool bChakram );
// The vampire's (the creature is not read).
int GetAdvancementVampireActionFromVampireAction( int CurAction, const MCreature* pCreature );
// The new vampire action of a plain one (13 and 12 have one: 16 and 7).
int GetNewVampireActionFromVampireAction( int CurAction );
// The advanced slayer's sprite part for an item class (the motorcycle by
// its frame), -1 for none.
int GetAdvancementPartFromItemClass( ITEM_CLASS itemClass, TYPE_FRAMEID frameID );
// The advanced slayer's gun attack for an item class, by weapon speed; -1
// for no gun.
int GetAdvancementSlayerAttackActionFromItemClass( ITEM_CLASS itemClass, MCreature* pCreature );
// The new slayer action of a plain one (a gun in the right hand changes
// standing and walking).
int ConvNewSlayerActionFromSlayerAction( int CurAction, MCreatureWear* pCreatureWear );
// ConvAdvancementSlayerAction by the wearer's right hand.
int ConvAdvancementSlayerActionFromSlayerAction( int CurAction, MCreatureWear* pCreatureWear );

// Each advanced action's frame count, indexed from ADVANCEMENT_ACTION_START;
// the executable fills them from the view's frame packs at start-up.
extern BYTE g_AdvanceVampireActionMaxCount[ ACTION_ADVANCEMENT_MAX - ACTION_ADVANCEMENT_STOP ];
extern BYTE g_AdvanceSlayerActionMaxCount[ ACTION_ADVANCEMENT_SLAYER_MAX - ACTION_ADVANCEMENT_SLAYER_STOP_SWORD ];
extern BYTE g_AdvanceOustersActionMaxCount[ ACTION_ADVANCEMENT_OUSTERS_MAX - ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_STOP ];
