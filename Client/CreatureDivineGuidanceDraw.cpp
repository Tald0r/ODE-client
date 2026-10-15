//----------------------------------------------------------------------
// CreatureDivineGuidanceDraw.cpp
//----------------------------------------------------------------------
// A creature under divine guidance is drawn again as a darkened, turning
// silhouette of its frame 30 of being drained (task 4.248: MTopView's
// members in DrawCreatureDivineGuidance.cpp until then, gameview's since).
// The view's packs and surface come in a CreatureDrawPacks.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "CreatureDivineGuidanceDraw.h"
#include "MCreatureWear.h"
#include "MCreatureTable.h"
#include "MItem.h"
#include "MItemOptionTable.h"
#include "AdvancementAction.h"
#include "FrameClock.h"
#include "CSpriteSurface.h"
#include "CIndexSprite.h"

// MCreatureWear.cpp's: the coat colour a vampire wears without one.
extern int defaultCoatColor;

namespace {
	POINT pointTemp;
}

void	DrawDivineGuidanceSlayerCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction )
{
	MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
	
	// Draw the add-ons that match the creature's action.
	//action = pCreature->GetAction();
	const int tempFrame = 30;
	
	WORD clothes;
	BYTE clothesType;
	
	for(int k = 0; k < 2; k++)
	{
		for (int i=0; i<ADDON_MAX; i++)
		{
			// Depending on the creature's current direction...
			// the order the clothes are drawn in can differ.
			clothesType = MCreatureWear::s_AddonOrder[pCreature->GetDirection()][i];
			
			// Draw the i-th kind of clothing if it is worn.
			const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(clothesType);
			
			if (addonInfo.bAddon)
			{
				clothes = addonInfo.FrameID;
				
				FRAME_ARRAY &FA = (*packs.pAddonFPK)[clothes][ACTION_DRAINED][(direction+g_CurrentFrame/2+k)%8];
				
				// If the action exists
				if (FA.GetSize() > tempFrame)
				{
					CFrame &Frame = FA[tempFrame];					
					int sprite	= Frame.GetSpriteID();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetSpriteID();
					int cx		= Frame.GetCX();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetCX();
					int cy		= Frame.GetCY();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetCY();
					
					
					CIndexSprite* pSprite = &(*packs.pAddonSPK)[ sprite ];
					
					pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
					pointTemp.y = pPoint->y + cy-(*g_pCreatureTable)[pCreature->GetCreatureType()].Height;// + pCreature->GetSY();
					
					//---------------------------------------- 
					// Draw a normal character
					//---------------------------------------- 
					int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
					if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
					{
						colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
						if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
							colorSet1 = colorSet2;
					}
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					
					packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 2-k);
				}
			}
		}
	}
}

void	DrawDivineGuidanceAdvancementClassSlayerCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction )
{
	MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
	
	// Draw the add-ons that match the creature's action.
	//action = pCreature->GetAction();
	const int tempFrame = 30;
	
	WORD clothes;
	BYTE clothesType;
	
	CCreatureFramePack& slayerFPK = pCreature->IsMale() ? (*packs.pAdvancementSlayerManFPK) : (*packs.pAdvancementSlayerWomanFPK);
	CIndexSpritePack& addonISPK = pCreature->IsMale() ? (*packs.pAdvancementSlayerManSPK) : (*packs.pAdvancementSlayerWomanSPK);
			
	for(int k = 0; k < 2; k++)
	{
		for (int i=0; i<AC_ADDON_MAX; i++)	// the order's row (ADDON_MAX read past it)
		{
			// Depending on the creature's current direction...
			// the order the clothes are drawn in can differ.
			clothesType = MCreatureWear::s_ACAddonOrder[pCreature->GetDirection()][i];
			
			// Draw the i-th kind of clothing if it is worn.
			const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetACAddonInfo(clothesType);
			
			if (addonInfo.bAddon)
			{
				clothes = GetAdvancementPartFromItemClass( addonInfo.ItemClass, addonInfo.FrameID );
				
				FRAME_ARRAY &FA = slayerFPK[clothes][ACTION_ADVANCEMENT_SLAYER_DRAINED-ADVANCEMENT_ACTION_START][(direction+g_CurrentFrame/2+k)%8];
				
				
				// If the action exists
				if (FA.GetSize() > tempFrame)
				{
					CFrame &Frame = FA[tempFrame];					
					int sprite	= Frame.GetSpriteID();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetSpriteID();
					int cx		= Frame.GetCX();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetCX();
					int cy		= Frame.GetCY();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetCY();
					
					
					CIndexSprite* pSprite = &addonISPK[ sprite ];					
					
					pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
					pointTemp.y = pPoint->y + cy-(*g_pCreatureTable)[pCreature->GetCreatureType()].Height;// + pCreature->GetSY();
					
					//---------------------------------------- 
					// Draw a normal character
					//---------------------------------------- 
					int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
					if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
					{
						colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
						if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
							colorSet1 = colorSet2;
					}
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					
					packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 2-k);
				}
			}
		}
	}
}

void	DrawDivineGuidanceVampireCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction, int body )
{
	static int tempGuidance = 0;
	tempGuidance = (g_CurrentFrame)%20;
	const int tempFrame = 30;
	

	for(int k = 0; k < 2; k++)
	{
		FRAME_ARRAY &FA = (*packs.pCreatureFPK)[body][ACTION_DRAINED][(direction+g_CurrentFrame/2+k)%8];
		
		if (FA.GetSize() > tempFrame)
		{
			CFrame& Frame =	FA[tempFrame];
			int sprite = Frame.GetSpriteID(),	//(*packs.pCreatureFPK)[body][action][direction][frame].GetSpriteID(),
				cx		= Frame.GetCX(),	//(*packs.pCreatureFPK)[body][action][direction][frame].GetCX(),
				cy		= Frame.GetCY();	//(*packs.pCreatureFPK)[body][action][direction][frame].GetCY();
			
			// Adjust the coordinates
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy-(*g_pCreatureTable)[pCreature->GetCreatureType()].Height;// + pCreature->GetSY();
			
			CIndexSprite* pSprite = &(*packs.pCreatureSPK)[ sprite ];
			
			int colorSet1 = pCreature->GetBodyColor1();
			int colorSet2 = pCreature->GetBodyColor2();
			
			if( colorSet1 == QUEST_ITEM_COLOR || colorSet1 == UNIQUE_ITEM_COLOR )
				colorSet1 = MItem::GetSpecialColorItemColorset( colorSet1 );
			
			if( colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )
				colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
			
			CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
			
			pointTemp.y += abs(10-tempGuidance);
			packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 2-k);
		}
	}
}

void	DrawDivineGuidanceAdvancementClassVampireCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction, int body )
{
	(void)body;
	static int tempGuidance = 0;
	tempGuidance = (g_CurrentFrame)%20;
	const int tempFrame = 30;

	CCreatureFramePack& advanceVampireFPK = pCreature->IsMale() ? (*packs.pAdvancementVampireManFPK) : (*packs.pAdvancementVampireWomanFPK);
	CIndexSpritePack& advanceVampireSPK = pCreature->IsMale() ? (*packs.pAdvancementVampireManSPK) : (*packs.pAdvancementVampireWomanSPK);

	for(int k = 0; k < 2; k++)
	{
		FRAME_ARRAY &FA = advanceVampireFPK[0][ACTION_ADVANCEMENT_DRAINED-ADVANCEMENT_ACTION_START][(direction+g_CurrentFrame/2+k)%8];
		
		if (FA.GetSize() > tempFrame)
		{
			CFrame& Frame =	FA[tempFrame];
			int sprite = Frame.GetSpriteID(),	//(*packs.pCreatureFPK)[body][action][direction][frame].GetSpriteID(),
				cx		= Frame.GetCX(),	//(*packs.pCreatureFPK)[body][action][direction][frame].GetCX(),
				cy		= Frame.GetCY();	//(*packs.pCreatureFPK)[body][action][direction][frame].GetCY();
			
			// Adjust the coordinates
			pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
			pointTemp.y = pPoint->y + cy-(*g_pCreatureTable)[pCreature->GetCreatureType()].Height;// + pCreature->GetSY();
			
			CIndexSprite* pSprite = &advanceVampireSPK[ sprite ];
			
			int colorSet1 = pCreature->GetBodyColor1();
			int colorSet2 = pCreature->GetBodyColor2();
			
			if( colorSet1 == QUEST_ITEM_COLOR || colorSet1 == UNIQUE_ITEM_COLOR )
				colorSet1 = MItem::GetSpecialColorItemColorset( colorSet1 );
			
			if( colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )
				colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
			
			CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
			
			pointTemp.y += abs(10-tempGuidance);
			packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 2-k);
		}
	}
}

void	DrawDivineGuidanceOustersCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction )
{
	// It has a chakram
	MCreatureWear *pCreatureWear = (MCreatureWear *)pCreature;
	const int tempFrame = 30;
	
	const MCreatureWear::ADDON_INFO& addonInfoChakram = pCreatureWear->GetAddonInfo(ADDON_RIGHTHAND);
	bool bChakram = addonInfoChakram.bAddon && addonInfoChakram.ItemClass == ITEM_CLASS_OUSTERS_CHAKRAM;
	
	int tempAction = ACTION_DRAINED;
	
	// Draw the body, then the chakram
	const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(ADDON_COAT);
	const MCreatureWear::ADDON_INFO& bootsAddonInfo = pCreatureWear->GetAddonInfo(ADDON_TROUSER);
	
	for(int k = 0; k < 2; k++)
	{
		if (addonInfo.bAddon && !pCreatureWear->IsGhost(1))
		{
			int clothes = addonInfo.FrameID;			
			
			FRAME_ARRAY &FA = (*packs.pOustersFPK)[clothes][tempAction][(direction+g_CurrentFrame/2+k)%8];
			
			// If the action exists
			if (FA.GetSize() > tempFrame)
			{
				CFrame &Frame = FA[tempFrame];					
				int sprite	= Frame.GetSpriteID();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetSpriteID();
				int cx		= Frame.GetCX();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetCX();
				int cy		= Frame.GetCY();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetCY();
				
				CIndexSprite* pSprite = &(*packs.pOustersSPK)[ sprite ];
				
				pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
				pointTemp.y = pPoint->y + cy-(*g_pCreatureTable)[pCreature->GetCreatureType()].Height;// + pCreature->GetSY();
				
				CIndexSprite::SetUsingColorSetOnly( 0, pCreature->GetBodyColor1() );	// hair colour
				
				int Colorset;
				
				Colorset = addonInfo.ColorSet2;
				
				if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
					Colorset = MItem::GetSpecialColorItemColorset( Colorset );
				CIndexSprite::SetUsingColorSetOnly( 1, Colorset );	// coat colour
				
				if(bChakram == false)	// if it wears a wristlet
				{
					if(addonInfoChakram.bAddon)
					{
						Colorset = addonInfoChakram.ColorSet2;
						if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
							Colorset = MItem::GetSpecialColorItemColorset( Colorset );
						
						CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arms
					}
					else
					{
						Colorset = defaultCoatColor;
						if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
							Colorset = MItem::GetSpecialColorItemColorset( Colorset );
						
						CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arms
					}
				}
				else
				{
					Colorset = defaultCoatColor;
					if(addonInfoChakram.bAddon)
						Colorset = addonInfoChakram.ColorSet2;
					if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					
					CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arms
					
				}
				
				if(bootsAddonInfo.bAddon)
				{
					Colorset = bootsAddonInfo.ColorSet2;
					if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					
					CIndexSprite::SetUsingColorSetOnly( 3, Colorset );	// boots colour
				}
				else
				{
					Colorset = defaultCoatColor ;
					if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					
					CIndexSprite::SetUsingColorSetOnly( 3, Colorset );	// bare feet (upstream's comment said arms)
				}
				
				packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 2-k);
				
			}
		}
		
		if (bChakram && !pCreatureWear->IsGhost(2))
		{
			int clothes = addonInfoChakram.FrameID;
			
			FRAME_ARRAY &FA = (*packs.pOustersFPK)[clothes][tempAction][(direction+g_CurrentFrame/2+k)%8];
			
			// If the action exists
			if (FA.GetSize() > tempFrame)
			{
				CFrame &Frame = FA[tempFrame];					
				int sprite	= Frame.GetSpriteID();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetSpriteID();
				int cx		= Frame.GetCX();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetCX();
				int cy		= Frame.GetCY();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetCY();
				
				CIndexSprite* pSprite = &(*packs.pOustersSPK)[ sprite ];
				
				pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
				pointTemp.y = pPoint->y + cy-(*g_pCreatureTable)[pCreature->GetCreatureType()].Height;// + pCreature->GetSY();
				
				int colorset1, colorset2;
				colorset1 = addonInfoChakram.ColorSet1;
				colorset2 = addonInfoChakram.ColorSet2;
				
				if( colorset1 == QUEST_ITEM_COLOR || colorset1 == UNIQUE_ITEM_COLOR )
					colorset1 = MItem::GetSpecialColorItemColorset( colorset1 );
				if( colorset2 == QUEST_ITEM_COLOR || colorset2 == UNIQUE_ITEM_COLOR )
					colorset2 = MItem::GetSpecialColorItemColorset( colorset2 );
				CIndexSprite::SetUsingColorSet( colorset1, colorset2 );
				
				packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 2-k);
			}
		}
	}
}

void	DrawDivineGuidanceAdvancementClassOustersCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction )
{
	// It has a chakram
	MCreatureWear *pCreatureWear = (MCreatureWear *)pCreature;
	const int tempFrame = 30;
	
	const MCreatureWear::ADDON_INFO& addonInfoChakram = pCreatureWear->GetAddonInfo(ADDON_RIGHTHAND);
	bool bChakram = addonInfoChakram.bAddon && addonInfoChakram.ItemClass == ITEM_CLASS_OUSTERS_CHAKRAM;
	
	int tempAction = ACTION_ADVANCEMENT_OUSTERS_DRAINED-ADVANCEMENT_ACTION_START;
	const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(ADDON_COAT);
	const MCreatureWear::ADDON_INFO& bootsAddonInfo = pCreatureWear->GetAddonInfo(ADDON_TROUSER);
	
	for(int k = 0; k < 2; k++)
	{
		if (addonInfo.bAddon && !pCreatureWear->IsGhost(1))
		{
			int clothes = 1;
			
			FRAME_ARRAY &FA = (*packs.pAdvancementOustersFPK)[clothes][tempAction][(direction+g_CurrentFrame/2+k)%8];
			
			// If the action exists
			if (FA.GetSize() > tempFrame)
			{
				CFrame &Frame = FA[tempFrame];					
				int sprite	= Frame.GetSpriteID();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetSpriteID();
				int cx		= Frame.GetCX();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetCX();
				int cy		= Frame.GetCY();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetCY();
				
				CIndexSprite* pSprite = &(*packs.pAdvancementOustersSPK)[ sprite ];
				
				pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
				pointTemp.y = pPoint->y + cy-(*g_pCreatureTable)[pCreature->GetCreatureType()].Height;// + pCreature->GetSY();
				
				CIndexSprite::SetUsingColorSetOnly( 0, pCreature->GetBodyColor1() );	// hair colour
				
				int Colorset;
				
				Colorset = addonInfo.ColorSet2;
				
				if( Colorset == QUEST_ITEM_COLOR || Colorset == UNIQUE_ITEM_COLOR )
					Colorset = MItem::GetSpecialColorItemColorset( Colorset );
				CIndexSprite::SetUsingColorSetOnly( 1, Colorset );	// coat colour
				
				if(bChakram == false)	// if it wears a wristlet
				{
					if(addonInfoChakram.bAddon)
					{
						Colorset = addonInfoChakram.ColorSet2;
						if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
							Colorset = MItem::GetSpecialColorItemColorset( Colorset );
						
						CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arms
					}
					else
					{
						Colorset = defaultCoatColor;
						if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
							Colorset = MItem::GetSpecialColorItemColorset( Colorset );
						
						CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arms
					}
				}
				else
				{
					Colorset = defaultCoatColor;
					if(addonInfoChakram.bAddon)
						Colorset = addonInfoChakram.ColorSet2;
					if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					
					CIndexSprite::SetUsingColorSetOnly( 2, Colorset );	// arms
					
				}
				
				if(bootsAddonInfo.bAddon)
				{
					Colorset = bootsAddonInfo.ColorSet2;
					if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					
					CIndexSprite::SetUsingColorSetOnly( 3, Colorset );	// boots colour
				}
				else
				{
					Colorset = defaultCoatColor ;
					if (Colorset == QUEST_ITEM_COLOR||Colorset == UNIQUE_ITEM_COLOR )
						Colorset = MItem::GetSpecialColorItemColorset( Colorset );
					
					CIndexSprite::SetUsingColorSetOnly( 3, Colorset );	// bare feet (upstream's comment said arms)
				}
				
				packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 2-k);
				
			}
		}
		
		if (bChakram && !pCreatureWear->IsGhost(2))
		{
			int clothes = 0;
			
			FRAME_ARRAY &FA = (*packs.pAdvancementOustersFPK)[clothes][tempAction][(direction+g_CurrentFrame/2+k)%8];
			
			// If the action exists
			if (FA.GetSize() > tempFrame)
			{
				CFrame &Frame = FA[tempFrame];					
				int sprite	= Frame.GetSpriteID();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetSpriteID();
				int cx		= Frame.GetCX();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetCX();
				int cy		= Frame.GetCY();	//(*packs.pAddonFPK)[clothes][action][direction][frame].GetCY();
				
				CIndexSprite* pSprite = &(*packs.pAdvancementOustersSPK)[ sprite ];
				
				pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
				pointTemp.y = pPoint->y + cy-(*g_pCreatureTable)[pCreature->GetCreatureType()].Height;// + pCreature->GetSY();
				
				int colorset1, colorset2;
				colorset1 = addonInfoChakram.ColorSet1;
				colorset2 = addonInfoChakram.ColorSet2;
				
				if( colorset1 == QUEST_ITEM_COLOR || colorset1 == UNIQUE_ITEM_COLOR )
					colorset1 = MItem::GetSpecialColorItemColorset( colorset1 );
				if( colorset2 == QUEST_ITEM_COLOR || colorset2 == UNIQUE_ITEM_COLOR )
					colorset2 = MItem::GetSpecialColorItemColorset( colorset2 );
				CIndexSprite::SetUsingColorSet( colorset1, colorset2 );
				
				packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 2-k);
			}
		}
	}
}

//----------------------------------------------------------------------
// The silhouette for the creature's race and class (MTopView::DrawCreature
// chose it until task 4.248).
//----------------------------------------------------------------------
void	DrawDivineGuidanceCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction, int body, bool bSlayer, bool bOusters )
{
	if (bSlayer)
	{
		if (pCreature->IsAdvancementClass())
			DrawDivineGuidanceAdvancementClassSlayerCharacter( packs, pPoint, pCreature, direction );
		else
			DrawDivineGuidanceSlayerCharacter( packs, pPoint, pCreature, direction );
	}
	else if (bOusters)
	{
		if (pCreature->IsAdvancementClass())
			DrawDivineGuidanceAdvancementClassOustersCharacter( packs, pPoint, pCreature, direction );
		else
			DrawDivineGuidanceOustersCharacter( packs, pPoint, pCreature, direction );
	}
	else
	{
		if (pCreature->IsAdvancementClass())
			DrawDivineGuidanceAdvancementClassVampireCharacter( packs, pPoint, pCreature, direction, body );
		else
			DrawDivineGuidanceVampireCharacter( packs, pPoint, pCreature, direction, body );
	}
}
