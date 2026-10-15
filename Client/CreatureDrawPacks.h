#pragma once

// The packs a creature is drawn from and the surface it is drawn on
// (task 4.248): MTopView's, handed to the creature drawing that moved out
// of it into gameview. MTopView fills one from its members at each draw;
// a test fills one from packs of its own. A drawing reads only the packs
// it draws from.

#include "CFramePack.h"
#include "CIndexSpritePack.h"

class CSpriteSurface;

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
};
