#pragma once

// The packs a creature is drawn from, the surface it is drawn on and the
// selection (task 4.248; the shadows' since 4.249, the selection since
// 4.250): MTopView's, handed to the creature drawing that moved out of
// it into gameview. MTopView fills one from its members at each draw;
// a test fills one from packs of its own. A drawing reads only the packs
// it draws from.

#include "CFramePack.h"
#include "CIndexSpritePack.h"
#include "CShadowSprite.h"
#include "CSpritePack.h"
#include "MTypeDef.h"

class CSpriteSurface;
class CSpriteOutlineManager;

struct CreatureDrawPacks
{
	CSpriteSurface*		pSurface = nullptr;

	CCreatureFramePack*	pCreatureFPK = nullptr;
	CIndexSpritePack*	pCreatureSPK = nullptr;
	CCreatureFramePack*	pAddonFPK = nullptr;
	CIndexSpritePack*	pAddonSPK = nullptr;
	CCreatureFramePack*	pOustersFPK = nullptr;
	CIndexSpritePack*	pOustersSPK = nullptr;

	// The advanced (second) classes', by race and gender.
	CCreatureFramePack*	pAdvancementSlayerManFPK = nullptr;
	CIndexSpritePack*	pAdvancementSlayerManSPK = nullptr;
	CCreatureFramePack*	pAdvancementSlayerWomanFPK = nullptr;
	CIndexSpritePack*	pAdvancementSlayerWomanSPK = nullptr;
	CCreatureFramePack*	pAdvancementVampireManFPK = nullptr;
	CIndexSpritePack*	pAdvancementVampireManSPK = nullptr;
	CCreatureFramePack*	pAdvancementVampireWomanFPK = nullptr;
	CIndexSpritePack*	pAdvancementVampireWomanSPK = nullptr;
	CCreatureFramePack*	pAdvancementOustersFPK = nullptr;
	CIndexSpritePack*	pAdvancementOustersSPK = nullptr;

	// The shadows', frames and sprites, as above.
	CCreatureFramePack*	pCreatureShadowFPK = nullptr;
	CShadowSpritePack*	pCreatureSSPK = nullptr;
	CCreatureFramePack*	pAddonShadowFPK = nullptr;
	CShadowSpritePack*	pAddonSSPK = nullptr;
	CCreatureFramePack*	pOustersShadowFPK = nullptr;
	CShadowSpritePack*	pOustersSSPK = nullptr;
	CCreatureFramePack*	pAdvancementSlayerManShadowFPK = nullptr;
	CShadowSpritePack*	pAdvancementSlayerManSSPK = nullptr;
	CCreatureFramePack*	pAdvancementSlayerWomanShadowFPK = nullptr;
	CShadowSpritePack*	pAdvancementSlayerWomanSSPK = nullptr;
	CCreatureFramePack*	pAdvancementVampireManShadowFPK = nullptr;
	CShadowSpritePack*	pAdvancementVampireManSSPK = nullptr;
	CCreatureFramePack*	pAdvancementVampireWomanShadowFPK = nullptr;
	CShadowSpritePack*	pAdvancementVampireWomanSSPK = nullptr;
	CCreatureFramePack*	pAdvancementOustersShadowFPK = nullptr;
	CShadowSpritePack*	pAdvancementOustersSSPK = nullptr;

	// The other sprites: a slayer's cross, the caskets.
	CSpritePack*		pEtcSPK = nullptr;

	// The selection: the creature under the cursor, whose sprites go to
	// the outline manager to be drawn outlined, not to the surface.
	TYPE_OBJECTID			selectedCreatureID = OBJECTID_NULL;
	CSpriteOutlineManager*	pOutlines = nullptr;
};
