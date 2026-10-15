//----------------------------------------------------------------------
// CreatureEffectDraw.cpp
//----------------------------------------------------------------------
// A creature drawn with an effect: fading out, moving fast, invisible,
// its weapon fading out, and the turrets (task 4.250: MTopView's members
// in DrawCreatureEffect.cpp until then, gameview's since). The view's
// packs, surface and selection come in a CreatureDrawPacks.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "CreatureEffectDraw.h"
#include "MCreatureWear.h"
#include "MFakeCreature.h"
#include "MItem.h"
#include "MItemOptionTable.h"
#include "AdvancementAction.h"
#include "CSpriteSurface.h"
#include "CIndexSprite.h"
#include "CSpriteOutlineManager.h"

#define	SHIFT_SNIPPING			2

namespace {
	POINT pointTemp;
	RECT rect;
}

void	DrawFadeOut( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	WORD clothes;
	BYTE clothesType;
	MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;

	//-----------------------------------------------------------
	// 좌우 잔상
	//-----------------------------------------------------------
	for (int i=0; i<ADDON_MAX; i++)
	{
		// Creature의 현재 방향에 따라서...
		// 옷을 출력해주는 순서가 다를 수 있다.
		clothesType = MCreatureWear::s_AddonOrder[pCreature->GetDirection()][i];
		
		// i번째 종류의 옷을 입고 있다면 출력해 준다.
		const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(clothesType);
		
		if (addonInfo.bAddon)
		{
			clothes = addonInfo.FrameID;
			
			FRAME_ARRAY &FA = (*packs.pAddonFPK)[clothes][action][direction];
			
			// 있는 동작인 경우
			if (FA.GetSize() > frame)
			{
				CFrame &Frame = FA[frame];					
				int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
				int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
				int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
				
				
				CIndexSprite* pSprite = &(*packs.pAddonSPK)[ sprite ];
				
				// 복장Sprite가 초기화 되지 않은 경우
				//							if (pSprite->IsNotInit())
				//							{								
				//								LoadFromFileAddonSPK( clothes, action );
				//								//continue;
				//							}
				
				pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
				pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
				
				//---------------------------------------- 
				// 캐릭터 선택 사각형 영역 설정
				//---------------------------------------- 	
				rect.left	= pointTemp.x;
				rect.top	= pointTemp.y;
				rect.right	= rect.left + pSprite->GetWidth();
				rect.bottom = rect.top + pSprite->GetHeight();
				pCreature->AddScreenRect( &rect );
				
				int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
				if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
				{
					// 유니크 아이템이거나 퀘스트 아이템이면
					//								
					//								if(colorSet2 == QUEST_ITEM_COLOR)
					//									colorSet2 = MItem::GetQuestItemColorset();
					//								else
					//									colorSet2 = MItem::GetUniqueItemColorset();									
					
					colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
					if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
						colorSet1 = colorSet2;
				}
				
				CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
				
				//-----------------------------------------------------------
				// 좌우 잔상
				//-----------------------------------------------------------
				POINT pointTemp2;
				
				//CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_NET );
				//CIndexSprite::SetEffect( CIndexSprite::EFFECT_NET );
				
				// 왼쪽
				pointTemp2.x = pointTemp.x - 25;
				pointTemp2.y = pointTemp.y;
				packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, 2);
				
				// 오른쪽
				pointTemp2.x = pointTemp.x + 25;
				pointTemp2.y = pointTemp.y;
				packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, 2);
				
				// 가운데							
				//m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);	
				//m_pSurface->BltIndexSprite(&pointTemp, pSprite);	
			}
		}
	}
}
void	DrawFadeOutForACSlayer( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	//by viva
	int action_viva = action;

	action = ConvAdvancementSlayerActionFromSlayerAction( action, dynamic_cast< MCreatureWear* >(pCreature) );
	if( action == -1 )
		return;
	else
		action -= ADVANCEMENT_ACTION_START;
	
	action_viva = ConvNewSlayerActionFromSlayerAction(action_viva, dynamic_cast< MCreatureWear* >(pCreature));

	

	int clothes;
	BYTE clothesType;
	MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;

	CCreatureFramePack& slayerFPK = pCreature->IsMale() ? (*packs.pAdvancementSlayerManFPK) : (*packs.pAdvancementSlayerWomanFPK);
	CIndexSpritePack& addonISPK = pCreature->IsMale() ? (*packs.pAdvancementSlayerManSPK) : (*packs.pAdvancementSlayerWomanSPK);



	//-----------------------------------------------------------
	// 좌우 잔상
	//-----------------------------------------------------------
	for (int i=0; i<AC_ADDON_MAX; i++)
	{
		// Creature의 현재 방향에 따라서...
		// 옷을 출력해주는 순서가 다를 수 있다.
		clothesType = MCreatureWear::s_ACAddonOrder[ direction ][ i ];
		
		// i번째 종류의 옷을 입고 있다면 출력해 준다.
		const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetACAddonInfo(clothesType);

		if (addonInfo.bAddon )
		{
			if((addonInfo.ItemClass==ITEM_CLASS_COAT && addonInfo.ItemType>=28)			//11
						||	(addonInfo.ItemClass==ITEM_CLASS_SWORD && addonInfo.ItemType>=16)		//14
						||	(addonInfo.ItemClass==ITEM_CLASS_BLADE && addonInfo.ItemType>=16)		//15
						||	(addonInfo.ItemClass==ITEM_CLASS_SHIELD && addonInfo.ItemType>=15)	//16
						||  (addonInfo.ItemClass==ITEM_CLASS_CROSS && addonInfo.ItemType>=14)		//17
						||	(addonInfo.ItemClass==ITEM_CLASS_HELM && addonInfo.ItemType>=15)		//19
						||  (addonInfo.ItemClass==ITEM_CLASS_AR && addonInfo.ItemType>=16)		//22
						||  (addonInfo.ItemClass==ITEM_CLASS_SR && addonInfo.ItemType>=16)		//23
						||  (addonInfo.ItemClass==ITEM_CLASS_MACE && addonInfo.ItemType>=14)		//35
						)
			{

				int k = 1;
				if(clothesType == AC_ADDON_BODY)	k = 2;
				for(int i=0;i<k;i++)
				{							
					clothes=addonInfo.FrameID + i;
					if( clothes == -1 ) continue;
					if( clothes > slayerFPK.GetSize() -1 ) continue;
					if(action_viva > slayerFPK[clothes].GetSize() -1) continue;
					try{
					FRAME_ARRAY& FA = slayerFPK[ clothes ][ action_viva ][ direction ];
					// 있는 동작인 경우
					if (FA.GetSize() > frame)
					{
						CFrame &Frame = FA[frame];					
						int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
						int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
						int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
						
						
						CIndexSprite* pSprite = &addonISPK[ sprite ];					
						
						// 복장Sprite가 초기화 되지 않은 경우
						//							if (pSprite->IsNotInit())
						//							{								
						//								LoadFromFileAddonSPK( clothes, action );
						//								//continue;
						//							}
						
						pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
						pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
						
						//---------------------------------------- 
						// 캐릭터 선택 사각형 영역 설정
						//---------------------------------------- 	
						rect.left	= pointTemp.x;
						rect.top	= pointTemp.y;
						rect.right	= rect.left + pSprite->GetWidth();
						rect.bottom = rect.top + pSprite->GetHeight();
						pCreature->AddScreenRect( &rect );
						
						int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
						if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
						{
							// 유니크 아이템이거나 퀘스트 아이템이면
							//								
							//								if(colorSet2 == QUEST_ITEM_COLOR)
							//									colorSet2 = MItem::GetQuestItemColorset();
							//								else
							//									colorSet2 = MItem::GetUniqueItemColorset();									
							
							colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
							if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
								colorSet1 = colorSet2;
						}
						
						CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
						
						//-----------------------------------------------------------
						// 좌우 잔상
						//-----------------------------------------------------------
						POINT pointTemp2;
						
						//CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_NET );
						//CIndexSprite::SetEffect( CIndexSprite::EFFECT_NET );
						
						// 왼쪽
						pointTemp2.x = pointTemp.x - 25;
						pointTemp2.y = pointTemp.y;
						packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, 2);
						
						// 오른쪽
						pointTemp2.x = pointTemp.x + 25;
						pointTemp2.y = pointTemp.y;
						packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, 2);
						
						// 가운데							
						//m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);	
						//m_pSurface->BltIndexSprite(&pointTemp, pSprite);	
					}
					}catch(...){}
				}
			}
			else
			{
				clothes = GetAdvancementPartFromItemClass( addonInfo.ItemClass, addonInfo.FrameID );
				
				if( clothes == -1 )
					continue;
				try{
				FRAME_ARRAY &FA = slayerFPK[clothes][action][direction];
				
				// 있는 동작인 경우
				if (FA.GetSize() > frame)
				{
					CFrame &Frame = FA[frame];					
					int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
					int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
					int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
					
					
					CIndexSprite* pSprite = &addonISPK[ sprite ];					
					
					// 복장Sprite가 초기화 되지 않은 경우
					//							if (pSprite->IsNotInit())
					//							{								
					//								LoadFromFileAddonSPK( clothes, action );
					//								//continue;
					//							}
					
					pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
					pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
					
					//---------------------------------------- 
					// 캐릭터 선택 사각형 영역 설정
					//---------------------------------------- 	
					rect.left	= pointTemp.x;
					rect.top	= pointTemp.y;
					rect.right	= rect.left + pSprite->GetWidth();
					rect.bottom = rect.top + pSprite->GetHeight();
					pCreature->AddScreenRect( &rect );
					
					int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
					if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
					{
						// 유니크 아이템이거나 퀘스트 아이템이면
						//								
						//								if(colorSet2 == QUEST_ITEM_COLOR)
						//									colorSet2 = MItem::GetQuestItemColorset();
						//								else
						//									colorSet2 = MItem::GetUniqueItemColorset();									
						
						colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
						if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
							colorSet1 = colorSet2;
					}
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					
					//-----------------------------------------------------------
					// 좌우 잔상
					//-----------------------------------------------------------
					POINT pointTemp2;
					
					//CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_NET );
					//CIndexSprite::SetEffect( CIndexSprite::EFFECT_NET );
					
					// 왼쪽
					pointTemp2.x = pointTemp.x - 25;
					pointTemp2.y = pointTemp.y;
					packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, 2);
					
					// 오른쪽
					pointTemp2.x = pointTemp.x + 25;
					pointTemp2.y = pointTemp.y;
					packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, 2);
					
					// 가운데							
					//m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);	
					//m_pSurface->BltIndexSprite(&pointTemp, pSprite);	
				}
				}catch(...){}
			}
		}
	}
}

void	DrawFastMove( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	WORD clothes;
	BYTE clothesType;
	MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
	
	for (int i=0; i<ADDON_MAX; i++)
	{
		// Creature의 현재 방향에 따라서...
		// 옷을 출력해주는 순서가 다를 수 있다.
		clothesType = MCreatureWear::s_AddonOrder[pCreature->GetDirection()][i];
		
		// i번째 종류의 옷을 입고 있다면 출력해 준다.
		const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(clothesType);
		
		if (addonInfo.bAddon)
		{
			clothes = addonInfo.FrameID;
			
			FRAME_ARRAY &FA = (*packs.pAddonFPK)[clothes][action][direction];
			
			// 있는 동작인 경우
			if (FA.GetSize() > frame)
			{
				CFrame &Frame = FA[frame];					
				int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
				int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
				int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
				
				
				CIndexSprite* pSprite = &(*packs.pAddonSPK)[ sprite ];
				
				
				pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
				pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
				
				//---------------------------------------- 
				// 캐릭터 선택 사각형 영역 설정
				//---------------------------------------- 	
				rect.left	= pointTemp.x;
				rect.top	= pointTemp.y;
				rect.right	= rect.left + pSprite->GetWidth();
				rect.bottom = rect.top + pSprite->GetHeight();
				pCreature->AddScreenRect( &rect );
				
				int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
				if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
				{
					//								// 유니크 아이템이면
					//								if(colorSet2 == QUEST_ITEM_COLOR)
					//									colorSet2 = MItem::GetQuestItemColorset();
					//								else
					//									colorSet2 = MItem::GetUniqueItemColorset();									
					colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
					if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
						colorSet1 = colorSet2;
				}
				
				CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
				
				//-----------------------------------------------------------
				// 빠르게 움직이는 중
				//-----------------------------------------------------------
				POINT pointTemp2;
				
				//CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_NET );
				//CIndexSprite::SetEffect( CIndexSprite::EFFECT_NET );
				
				// 2 pixel skip
				pointTemp2.x = pointTemp.x - pCreature->GetCX();
				pointTemp2.y = pointTemp.y - pCreature->GetCY();
				
				//CSpriteSurface::s_Value1 = 3;
				//m_pSurface->BltIndexSpriteEffect(&pointTemp2, pSprite);
				packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, 2);
				
				// 1 pixel skip
				pointTemp2.x = pointTemp.x - (pCreature->GetCX() >> 1);
				pointTemp2.y = pointTemp.y - (pCreature->GetCY() >> 1);
				
				//CSpriteSurface::s_Value1 = 1;
				//m_pSurface->BltIndexSpriteEffect(&pointTemp2, pSprite);
				packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, 1);
				
				//---------------------------------------- 
				// 캐릭터 부분 색깔이 바뀌는 경우
				//---------------------------------------- 
				//if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
				//{
				//	m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, 
				//									addonInfo.EffectColorSet);
				//}
				//---------------------------------------- 
				// 정상적인 캐릭터 출력
				//---------------------------------------- 
				//else
				{								
					// [새기술] - fake는 fast move일때 무조건 어둡게..
					if (pCreature->IsFakeCreature())
					{
						MFakeCreature *pFakeCreature = dynamic_cast<MFakeCreature*>(pCreature);
						if(pFakeCreature->GetFakeCreatureType() == MFakeCreature::FAKE_CREATURE_BRIGHTNESS )
							packs.pSurface->BltIndexSpriteBrightness(&pointTemp, pSprite, 32);
						else
							packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
					}
					else
					{
						packs.pSurface->BltIndexSprite(&pointTemp, pSprite);
					}						
					
					// 정상적인 출력
					//CIndexSprite::SetUsingColorSet( addonInfo.ColorSet1, addonInfo.ColorSet2 );
					//m_pSurface->BltIndexSprite(&pointTemp, pSprite);
				}
				
			}
		}
	}
}

void	DrawFastMoveForACSlayer( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	//by viva
	int action_viva = action;
	action = ConvAdvancementSlayerActionFromSlayerAction( action, dynamic_cast< MCreatureWear* >(pCreature) );
	if( action == -1 )
		return;
	else
		action -= ADVANCEMENT_ACTION_START;
	
	action_viva = ConvNewSlayerActionFromSlayerAction(action_viva, dynamic_cast< MCreatureWear* >(pCreature));
	
	int clothes;
	BYTE clothesType;
	MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
	
	
	CCreatureFramePack& slayerFPK = pCreature->IsMale() ? (*packs.pAdvancementSlayerManFPK) : (*packs.pAdvancementSlayerWomanFPK);
	CIndexSpritePack& addonISPK = pCreature->IsMale() ? (*packs.pAdvancementSlayerManSPK) : (*packs.pAdvancementSlayerWomanSPK);
	
	
	//-----------------------------------------------------------
	// 좌우 잔상
	//-----------------------------------------------------------
	for (int i=0; i<AC_ADDON_MAX; i++)
	{
		// Creature의 현재 방향에 따라서...
		// 옷을 출력해주는 순서가 다를 수 있다.
		clothesType = MCreatureWear::s_ACAddonOrder[ direction ][ i ];
		
		// i번째 종류의 옷을 입고 있다면 출력해 준다.
		const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetACAddonInfo(clothesType);
				
		if (addonInfo.bAddon)
		{
			if((addonInfo.ItemClass==ITEM_CLASS_COAT && addonInfo.ItemType>=28)			//11
				||	(addonInfo.ItemClass==ITEM_CLASS_SWORD && addonInfo.ItemType>=16)		//14
				||	(addonInfo.ItemClass==ITEM_CLASS_BLADE && addonInfo.ItemType>=16)		//15
				||	(addonInfo.ItemClass==ITEM_CLASS_SHIELD && addonInfo.ItemType>=15)	//16
				||  (addonInfo.ItemClass==ITEM_CLASS_CROSS && addonInfo.ItemType>=14)		//17
				||	(addonInfo.ItemClass==ITEM_CLASS_HELM && addonInfo.ItemType>=15)		//19
				||  (addonInfo.ItemClass==ITEM_CLASS_AR && addonInfo.ItemType>=16)		//22
				||  (addonInfo.ItemClass==ITEM_CLASS_SR && addonInfo.ItemType>=16)		//23
				||  (addonInfo.ItemClass==ITEM_CLASS_MACE && addonInfo.ItemType>=14)		//35
				)
			{
				int k = 1;
				if(clothesType == AC_ADDON_BODY)	k = 2;
				for(int i=0;i<k;i++)
				{							
					clothes=addonInfo.FrameID + i;
					if( clothes == -1 ) continue;
					if( clothes > slayerFPK.GetSize() -1 ) continue;
					if(action_viva > slayerFPK[clothes].GetSize() -1) continue;
					try{
					FRAME_ARRAY& FA = slayerFPK[ clothes ][ action_viva ][ direction ];
					
					// 있는 동작인 경우
					if (FA.GetSize() > frame)
					{
						CFrame &Frame = FA[frame];					
						int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
						int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
						int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
						
						
						CIndexSprite* pSprite = &addonISPK[ sprite ];					
						
						
						pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
						pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
						
						//---------------------------------------- 
						// 캐릭터 선택 사각형 영역 설정
						//---------------------------------------- 	
						rect.left	= pointTemp.x;
						rect.top	= pointTemp.y;
						rect.right	= rect.left + pSprite->GetWidth();
						rect.bottom = rect.top + pSprite->GetHeight();
						pCreature->AddScreenRect( &rect );
						
						int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
						if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
						{
							//								// 유니크 아이템이면
							//								if(colorSet2 == QUEST_ITEM_COLOR)
							//									colorSet2 = MItem::GetQuestItemColorset();
							//								else
							//									colorSet2 = MItem::GetUniqueItemColorset();									
							colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
							if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
								colorSet1 = colorSet2;
						}
						
						CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
						
						//-----------------------------------------------------------
						// 빠르게 움직이는 중
						//-----------------------------------------------------------
						POINT pointTemp2;
						
						//CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_NET );
						//CIndexSprite::SetEffect( CIndexSprite::EFFECT_NET );
						
						// 2 pixel skip
						pointTemp2.x = pointTemp.x - pCreature->GetCX();
						pointTemp2.y = pointTemp.y - pCreature->GetCY();
						
						//CSpriteSurface::s_Value1 = 3;
						//m_pSurface->BltIndexSpriteEffect(&pointTemp2, pSprite);
						packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, 2);
						
						// 1 pixel skip
						pointTemp2.x = pointTemp.x - (pCreature->GetCX() >> 1);
						pointTemp2.y = pointTemp.y - (pCreature->GetCY() >> 1);
						
						//CSpriteSurface::s_Value1 = 1;
						//m_pSurface->BltIndexSpriteEffect(&pointTemp2, pSprite);
						packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, 1);
						
						//---------------------------------------- 
						// 캐릭터 부분 색깔이 바뀌는 경우
						//---------------------------------------- 
						//if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
						//{
						//	m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, 
						//									addonInfo.EffectColorSet);
						//}
						//---------------------------------------- 
						// 정상적인 캐릭터 출력
						//---------------------------------------- 
						//else
						{								
							// [새기술] - fake는 fast move일때 무조건 어둡게..
							if (pCreature->IsFakeCreature())
							{
								MFakeCreature *pFakeCreature = dynamic_cast<MFakeCreature*>(pCreature);
								if(pFakeCreature->GetFakeCreatureType() == MFakeCreature::FAKE_CREATURE_BRIGHTNESS )
									packs.pSurface->BltIndexSpriteBrightness(&pointTemp, pSprite, 32);
								else
									packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
							}
							else
							{
								packs.pSurface->BltIndexSprite(&pointTemp, pSprite);
							}						
							
							// 정상적인 출력
							//CIndexSprite::SetUsingColorSet( addonInfo.ColorSet1, addonInfo.ColorSet2 );
							//m_pSurface->BltIndexSprite(&pointTemp, pSprite);
						}
					}
					}catch(...) {}
				}
			}
			else
			{
				clothes = GetAdvancementPartFromItemClass( addonInfo.ItemClass, addonInfo.FrameID );
				
				if( clothes == -1 )
					continue;
				try{
				FRAME_ARRAY &FA = slayerFPK[clothes][action][direction];
				
				// 있는 동작인 경우
				if (FA.GetSize() > frame)
				{
					CFrame &Frame = FA[frame];					
					int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
					int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
					int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
					
					
					CIndexSprite* pSprite = &addonISPK[ sprite ];					
					
					
					pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
					pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
					
					//---------------------------------------- 
					// 캐릭터 선택 사각형 영역 설정
					//---------------------------------------- 	
					rect.left	= pointTemp.x;
					rect.top	= pointTemp.y;
					rect.right	= rect.left + pSprite->GetWidth();
					rect.bottom = rect.top + pSprite->GetHeight();
					pCreature->AddScreenRect( &rect );
					
					int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
					if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
					{
						//								// 유니크 아이템이면
						//								if(colorSet2 == QUEST_ITEM_COLOR)
						//									colorSet2 = MItem::GetQuestItemColorset();
						//								else
						//									colorSet2 = MItem::GetUniqueItemColorset();									
						colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
						if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
							colorSet1 = colorSet2;
					}
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					
					//-----------------------------------------------------------
					// 빠르게 움직이는 중
					//-----------------------------------------------------------
					POINT pointTemp2;
					
					//CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_NET );
					//CIndexSprite::SetEffect( CIndexSprite::EFFECT_NET );
					
					// 2 pixel skip
					pointTemp2.x = pointTemp.x - pCreature->GetCX();
					pointTemp2.y = pointTemp.y - pCreature->GetCY();
					
					//CSpriteSurface::s_Value1 = 3;
					//m_pSurface->BltIndexSpriteEffect(&pointTemp2, pSprite);
					packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, 2);
					
					// 1 pixel skip
					pointTemp2.x = pointTemp.x - (pCreature->GetCX() >> 1);
					pointTemp2.y = pointTemp.y - (pCreature->GetCY() >> 1);
					
					//CSpriteSurface::s_Value1 = 1;
					//m_pSurface->BltIndexSpriteEffect(&pointTemp2, pSprite);
					packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, 1);
					
					//---------------------------------------- 
					// 캐릭터 부분 색깔이 바뀌는 경우
					//---------------------------------------- 
					//if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
					//{
					//	m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, 
					//									addonInfo.EffectColorSet);
					//}
					//---------------------------------------- 
					// 정상적인 캐릭터 출력
					//---------------------------------------- 
					//else
					{								
						// [새기술] - fake는 fast move일때 무조건 어둡게..
						if (pCreature->IsFakeCreature())
						{
							MFakeCreature *pFakeCreature = dynamic_cast<MFakeCreature*>(pCreature);
							if(pFakeCreature->GetFakeCreatureType() == MFakeCreature::FAKE_CREATURE_BRIGHTNESS )
								packs.pSurface->BltIndexSpriteBrightness(&pointTemp, pSprite, 32);
							else
								packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
						}
						else
						{
							packs.pSurface->BltIndexSprite(&pointTemp, pSprite);
						}						
						
						// 정상적인 출력
						//CIndexSprite::SetUsingColorSet( addonInfo.ColorSet1, addonInfo.ColorSet2 );
						//m_pSurface->BltIndexSprite(&pointTemp, pSprite);
					}
				}
				}catch(...) {} //by viva
			}
		}
	}
}

void	DrawInvisible( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	WORD clothes;
	BYTE clothesType;
	MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
		
	for (int i=0; i<ADDON_MAX; i++)
	{
		// Creature의 현재 방향에 따라서...
		// 옷을 출력해주는 순서가 다를 수 있다.
		clothesType = MCreatureWear::s_AddonOrder[pCreature->GetDirection()][i];
		
		// i번째 종류의 옷을 입고 있다면 출력해 준다.
		const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(clothesType);
		
		if (addonInfo.bAddon)
		{
			clothes = addonInfo.FrameID;
	
			FRAME_ARRAY &FA = (*packs.pAddonFPK)[clothes][action][direction];
			
			// 있는 동작인 경우
			if (FA.GetSize() > frame)
			{
				CFrame &Frame = FA[frame];					
				int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
				int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
				int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
				
				
				CIndexSprite* pSprite = &(*packs.pAddonSPK)[ sprite ];
				
				
				pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
				pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
				
				//---------------------------------------- 
				// 캐릭터 선택 사각형 영역 설정
				//---------------------------------------- 	
				rect.left	= pointTemp.x;
				rect.top	= pointTemp.y;
				rect.right	= rect.left + pSprite->GetWidth();
				rect.bottom = rect.top + pSprite->GetHeight();
				pCreature->AddScreenRect( &rect );
				
				//---------------------------------------- 
				// 캐릭터 부분 색깔이 바뀌는 경우
				//---------------------------------------- 
				//if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
				//{
				//	m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, 
				//									addonInfo.EffectColorSet);
				//}
				//---------------------------------------- 
				// 정상적인 캐릭터 출력
				//---------------------------------------- 
				//else
				{
					int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
					if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
					{
						// 유니크 아이템이면
						//									if(colorSet2 == QUEST_ITEM_COLOR)
						//										colorSet2 = MItem::GetQuestItemColorset();
						//									else
						//										colorSet2 = MItem::GetUniqueItemColorset();										
						
						colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
						
						if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
							colorSet1 = colorSet2;
					}
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					
					int wipeValue = pCreature->GetInvisibleCount();
					
					if (wipeValue==0)
					{
						packs.pSurface->BltIndexSprite(&pointTemp, pSprite);
					}
					else if (wipeValue==64)
					{
						//CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_SIMPLE_OUTLINE );
						//CIndexSprite::SetEffect( CIndexSprite::EFFECT_SIMPLE_OUTLINE );
						
						//m_pSurface->BltIndexSpriteEffect(&pointTemp, pSprite);
						
						// slayer의 invisible인 snipping
						packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, SHIFT_SNIPPING);
					}
					else
					{
						POINT pointTemp2 = pointTemp;
						// slayer의 invisible인 snipping
						packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, SHIFT_SNIPPING);
						
						CSpriteSurface::s_Value1 = wipeValue;
						
						CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_WIPE_OUT );
						CIndexSprite::SetEffect( CIndexSprite::EFFECT_WIPE_OUT );
						
						packs.pSurface->BltIndexSpriteEffect(&pointTemp, pSprite);
						
						//CSpriteSurface::s_Value1 = wipeValue/10+1;
						
						//CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_NET );
						//CIndexSprite::SetEffect( CIndexSprite::EFFECT_NET );
						
						//m_pSurface->BltIndexSpriteEffect(&pointTemp, pSprite);
					}
				}						
				
				// 정상적인 출력
				//CIndexSprite::SetUsingColorSet( addonInfo.ColorSet1, addonInfo.ColorSet2 );
				//m_pSurface->BltIndexSprite(&pointTemp, pSprite);
				
			}
		}
	}
}

void	DrawInvisibleForACSlayer( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	
	//by viva
	int action_viva = action;
	action = ConvAdvancementSlayerActionFromSlayerAction( action, dynamic_cast< MCreatureWear* >(pCreature) );
	if( action == -1 )
		return;
	else
		action -= ADVANCEMENT_ACTION_START;
	
	action_viva = ConvNewSlayerActionFromSlayerAction(action_viva, dynamic_cast< MCreatureWear* >(pCreature));
	
	int clothes;
	BYTE clothesType;
	MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
	
	//by viva
	CIndexSpritePack& addonISPK = pCreature->IsMale() ? (*packs.pAdvancementSlayerManSPK) : (*packs.pAdvancementSlayerWomanSPK);
	CCreatureFramePack& slayerFPK = pCreature->IsMale() ? (*packs.pAdvancementSlayerManFPK) : (*packs.pAdvancementSlayerWomanFPK);
	
	
	//-----------------------------------------------------------
	// 좌우 잔상
	//-----------------------------------------------------------
	for (int i=0; i<AC_ADDON_MAX; i++)
	{
		// Creature의 현재 방향에 따라서...
		// 옷을 출력해주는 순서가 다를 수 있다.
		clothesType = MCreatureWear::s_ACAddonOrder[ direction ][ i ];
		
		// i번째 종류의 옷을 입고 있다면 출력해 준다.
		const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetACAddonInfo(clothesType);
		
		if (addonInfo.bAddon)
		{
			if((addonInfo.ItemClass==ITEM_CLASS_COAT && addonInfo.ItemType>=28)			//11
				||	(addonInfo.ItemClass==ITEM_CLASS_SWORD && addonInfo.ItemType>=16)		//14
				||	(addonInfo.ItemClass==ITEM_CLASS_BLADE && addonInfo.ItemType>=16)		//15
				||	(addonInfo.ItemClass==ITEM_CLASS_SHIELD && addonInfo.ItemType>=15)	//16
				||  (addonInfo.ItemClass==ITEM_CLASS_CROSS && addonInfo.ItemType>=14)		//17
				||	(addonInfo.ItemClass==ITEM_CLASS_HELM && addonInfo.ItemType>=15)		//19
				||  (addonInfo.ItemClass==ITEM_CLASS_AR && addonInfo.ItemType>=16)		//22
				||  (addonInfo.ItemClass==ITEM_CLASS_SR && addonInfo.ItemType>=16)		//23
				||  (addonInfo.ItemClass==ITEM_CLASS_MACE && addonInfo.ItemType>=14)		//35
				)
			{
				int k = 1;
				if(clothesType == AC_ADDON_BODY)	k = 2;
				for(int i=0;i<k;i++)
				{							
					clothes=addonInfo.FrameID + i;
					if( clothes == -1 ) continue;
					if( clothes > slayerFPK.GetSize() -1 ) continue;
					if(action_viva > slayerFPK[clothes].GetSize() -1) continue;
					try{
					FRAME_ARRAY& FA = slayerFPK[ clothes ][ action_viva ][ direction ];
					
					// 있는 동작인 경우
					if (FA.GetSize() > frame)
					{
						CFrame &Frame = FA[frame];					
						int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
						int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
						int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
						
						
						CIndexSprite* pSprite = &addonISPK[ sprite ];					
						
						
						pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
						pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
						
						//---------------------------------------- 
						// 캐릭터 선택 사각형 영역 설정
						//---------------------------------------- 	
						rect.left	= pointTemp.x;
						rect.top	= pointTemp.y;
						rect.right	= rect.left + pSprite->GetWidth();
						rect.bottom = rect.top + pSprite->GetHeight();
						pCreature->AddScreenRect( &rect );
						
						//---------------------------------------- 
						// 캐릭터 부분 색깔이 바뀌는 경우
						//---------------------------------------- 
						//if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
						//{
						//	m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, 
						//									addonInfo.EffectColorSet);
						//}
						//---------------------------------------- 
						// 정상적인 캐릭터 출력
						//---------------------------------------- 
						//else
						{
							int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
							if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
							{
								// 유니크 아이템이면
								//									if(colorSet2 == QUEST_ITEM_COLOR)
								//										colorSet2 = MItem::GetQuestItemColorset();
								//									else
								//										colorSet2 = MItem::GetUniqueItemColorset();										
								
								colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
								
								if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
									colorSet1 = colorSet2;
							}
							
							CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
							
							int wipeValue = pCreature->GetInvisibleCount();
							
							if (wipeValue==0)
							{
								packs.pSurface->BltIndexSprite(&pointTemp, pSprite);
							}
							else if (wipeValue==64)
							{
								//CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_SIMPLE_OUTLINE );
								//CIndexSprite::SetEffect( CIndexSprite::EFFECT_SIMPLE_OUTLINE );
								
								//m_pSurface->BltIndexSpriteEffect(&pointTemp, pSprite);
								
								// slayer의 invisible인 snipping
								packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, SHIFT_SNIPPING);
							}
							else
							{
								POINT pointTemp2 = pointTemp;
								// slayer의 invisible인 snipping
								packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, SHIFT_SNIPPING);
								
								CSpriteSurface::s_Value1 = wipeValue;
								
								CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_WIPE_OUT );
								CIndexSprite::SetEffect( CIndexSprite::EFFECT_WIPE_OUT );
								
								packs.pSurface->BltIndexSpriteEffect(&pointTemp, pSprite);
								
								//CSpriteSurface::s_Value1 = wipeValue/10+1;
								
								//CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_NET );
								//CIndexSprite::SetEffect( CIndexSprite::EFFECT_NET );
								
								//m_pSurface->BltIndexSpriteEffect(&pointTemp, pSprite);
							}
						}						
						
						// 정상적인 출력
						//CIndexSprite::SetUsingColorSet( addonInfo.ColorSet1, addonInfo.ColorSet2 );
						//m_pSurface->BltIndexSprite(&pointTemp, pSprite);
						
					}
					}catch(...){}
				}
			}
			else
			{
				clothes = GetAdvancementPartFromItemClass( addonInfo.ItemClass , addonInfo.FrameID);
				
				if( clothes == -1 )
					continue;
				try{
				FRAME_ARRAY &FA = slayerFPK[clothes][action][direction];
				
				// 있는 동작인 경우
				if (FA.GetSize() > frame)
				{
					CFrame &Frame = FA[frame];					
					int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
					int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
					int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
					
					
					CIndexSprite* pSprite = &addonISPK[ sprite ];					
					
					
					pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
					pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
					
					//---------------------------------------- 
					// 캐릭터 선택 사각형 영역 설정
					//---------------------------------------- 	
					rect.left	= pointTemp.x;
					rect.top	= pointTemp.y;
					rect.right	= rect.left + pSprite->GetWidth();
					rect.bottom = rect.top + pSprite->GetHeight();
					pCreature->AddScreenRect( &rect );
					
					//---------------------------------------- 
					// 캐릭터 부분 색깔이 바뀌는 경우
					//---------------------------------------- 
					//if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
					//{
					//	m_pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite, 
					//									addonInfo.EffectColorSet);
					//}
					//---------------------------------------- 
					// 정상적인 캐릭터 출력
					//---------------------------------------- 
					//else
					{
						int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
						if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
						{
							// 유니크 아이템이면
							//									if(colorSet2 == QUEST_ITEM_COLOR)
							//										colorSet2 = MItem::GetQuestItemColorset();
							//									else
							//										colorSet2 = MItem::GetUniqueItemColorset();										
							
							colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
							
							if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
								colorSet1 = colorSet2;
						}
						
						CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
						
						int wipeValue = pCreature->GetInvisibleCount();
						
						if (wipeValue==0)
						{
							packs.pSurface->BltIndexSprite(&pointTemp, pSprite);
						}
						else if (wipeValue==64)
						{
							//CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_SIMPLE_OUTLINE );
							//CIndexSprite::SetEffect( CIndexSprite::EFFECT_SIMPLE_OUTLINE );
							
							//m_pSurface->BltIndexSpriteEffect(&pointTemp, pSprite);
							
							// slayer의 invisible인 snipping
							packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, SHIFT_SNIPPING);
						}
						else
						{
							POINT pointTemp2 = pointTemp;
							// slayer의 invisible인 snipping
							packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite, SHIFT_SNIPPING);
							
							CSpriteSurface::s_Value1 = wipeValue;
							
							CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_WIPE_OUT );
							CIndexSprite::SetEffect( CIndexSprite::EFFECT_WIPE_OUT );
							
							packs.pSurface->BltIndexSpriteEffect(&pointTemp, pSprite);
							
							//CSpriteSurface::s_Value1 = wipeValue/10+1;
							
							//CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_NET );
							//CIndexSprite::SetEffect( CIndexSprite::EFFECT_NET );
							
							//m_pSurface->BltIndexSpriteEffect(&pointTemp, pSprite);
						}
					}						
					
					// 정상적인 출력
					//CIndexSprite::SetUsingColorSet( addonInfo.ColorSet1, addonInfo.ColorSet2 );
					//m_pSurface->BltIndexSprite(&pointTemp, pSprite);
					
				}
				}catch(...){}
			}
		}
	}
}

void	DrawWeaponFadeOut( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	WORD clothes;
	BYTE clothesType;
	MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
	
	for (int i=0; i<ADDON_MAX; i++)
	{
		// Creature의 현재 방향에 따라서...
		// 옷을 출력해주는 순서가 다를 수 있다.
		clothesType = MCreatureWear::s_AddonOrder[pCreature->GetDirection()][i];
		
		// i번째 종류의 옷을 입고 있다면 출력해 준다.
		const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(clothesType);
		
		if (addonInfo.bAddon)
		{
			clothes = addonInfo.FrameID;
			
			FRAME_ARRAY &FA = (*packs.pAddonFPK)[clothes][action][direction];
			
			// 있는 동작인 경우
			if (FA.GetSize() > frame)
			{
				CFrame &Frame = FA[frame];					
				int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
				int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
				int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
				
				
				CIndexSprite* pSprite = &(*packs.pAddonSPK)[ sprite ];
				
				
				pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
				pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
				
				//---------------------------------------- 
				// 캐릭터 선택 사각형 영역 설정
				//---------------------------------------- 	
				rect.left	= pointTemp.x;
				rect.top	= pointTemp.y;
				rect.right	= rect.left + pSprite->GetWidth();
				rect.bottom = rect.top + pSprite->GetHeight();
				pCreature->AddScreenRect( &rect );
				
				
				//------------------------------------------------------
				// 잔상 추가
				//------------------------------------------------------
				if (clothesType==ADDON_RIGHTHAND)
				{			
					// 0, 1, 2, 3, 4, 5
					const int darkbits[6] =
					{
						0, 1, 1, 2, 2, 3
					};
					
					POINT pointTemp2;
					
					int maxShadow = min( frame, 5 );
					
					for (int f=1; f<=maxShadow; f++)
					{
						int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
						
						if(colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )										
						{
							colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
							if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
								colorSet1 = colorSet2;
							
						}
						
						CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
						
						//---------------------------------------- 
						// f frame 전
						//---------------------------------------- 
						CFrame &Frame_f = FA[frame-f];					
						int sprite_f	= Frame_f.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
						
						CIndexSprite* pSprite_f = &(*packs.pAddonSPK)[ sprite_f ];
						
						int cx_f		= Frame_f.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
						int cy_f		= Frame_f.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
						
						pointTemp2.x = pPoint->x + cx_f;// + pCreature->GetSX();
						pointTemp2.y = pPoint->y + cy_f;// + pCreature->GetSY();
						
						if( pCreature->HasEffectStatus( EFFECTSTATUS_REDIANCE ) )
							packs.pSurface->BltIndexSpriteColorSet(&pointTemp2, pSprite_f, addonInfo.EffectColorSet);
						else
							packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite_f, darkbits[f]);
						
					}								
				}
				
				//---------------------------------------- 
				// 캐릭터 부분 색깔이 바뀌는 경우
				//---------------------------------------- 
				if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
				{
					packs.pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite,
						addonInfo.EffectColorSet);
				}
				//---------------------------------------- 
				// 정상적인 캐릭터 출력
				//---------------------------------------- 
				else
				{
					int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
					if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
					{
						
						colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
						if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
							colorSet1 = colorSet2;
					}
					
					CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
					
					// 어둡게 찍기
					if (pCreature->IsFade())
					{
						packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
					}
					else
					{
						packs.pSurface->BltIndexSprite(&pointTemp, pSprite);
					}
				}						
				
				// 정상적인 출력
				//CIndexSprite::SetUsingColorSet( addonInfo.ColorSet1, addonInfo.ColorSet2 );
				//m_pSurface->BltIndexSprite(&pointTemp, pSprite);
				
			}
		}
	}
}

void	DrawWeaponFadeOutForACSlayer( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame )
{
	//by viva
	int action_viva = action;
	action = ConvAdvancementSlayerActionFromSlayerAction( action, dynamic_cast< MCreatureWear* >(pCreature) );
	if( action == -1 )
		return;
	else
		action -= ADVANCEMENT_ACTION_START;
	
	action_viva = ConvNewSlayerActionFromSlayerAction(action_viva, dynamic_cast< MCreatureWear* >(pCreature));
	
	int clothes;
	BYTE clothesType;
	MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;
	
	//by viva
	/*
	action = ConvAdvancementSlayerActionFromSlayerAction( action, dynamic_cast< MCreatureWear* >(pCreature) );
	
	  if( action == -1 )
	  return;
	  else
	  action -= ADVANCEMENT_ACTION_START;
	*/
	
	CCreatureFramePack& slayerFPK = pCreature->IsMale() ? (*packs.pAdvancementSlayerManFPK) : (*packs.pAdvancementSlayerWomanFPK);
	CIndexSpritePack& addonISPK = pCreature->IsMale() ? (*packs.pAdvancementSlayerManSPK) : (*packs.pAdvancementSlayerWomanSPK);
	
	
	//-----------------------------------------------------------
	// 좌우 잔상
	//-----------------------------------------------------------
	for (int i=0; i<AC_ADDON_MAX; i++)
	{
		// Creature의 현재 방향에 따라서...
		// 옷을 출력해주는 순서가 다를 수 있다.
		clothesType = MCreatureWear::s_ACAddonOrder[ direction ][ i ];
		
		// i번째 종류의 옷을 입고 있다면 출력해 준다.
		const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetACAddonInfo(clothesType);
		
		if (addonInfo.bAddon)
		{
			if((addonInfo.ItemClass==ITEM_CLASS_COAT && addonInfo.ItemType>=28)			//11
				||	(addonInfo.ItemClass==ITEM_CLASS_SWORD && addonInfo.ItemType>=16)		//14
				||	(addonInfo.ItemClass==ITEM_CLASS_BLADE && addonInfo.ItemType>=16)		//15
				||	(addonInfo.ItemClass==ITEM_CLASS_SHIELD && addonInfo.ItemType>=15)	//16
				||  (addonInfo.ItemClass==ITEM_CLASS_CROSS && addonInfo.ItemType>=14)		//17
				||	(addonInfo.ItemClass==ITEM_CLASS_HELM && addonInfo.ItemType>=15)		//19
				||  (addonInfo.ItemClass==ITEM_CLASS_AR && addonInfo.ItemType>=16)		//22
				||  (addonInfo.ItemClass==ITEM_CLASS_SR && addonInfo.ItemType>=16)		//23
				||  (addonInfo.ItemClass==ITEM_CLASS_MACE && addonInfo.ItemType>=14)		//35
				)
			{
				int k = 1;
				if(clothesType == AC_ADDON_BODY)	k = 2;
				for(int i=0;i<k;i++)
				{							
					clothes=addonInfo.FrameID + i;
					if( clothes == -1 ) continue;
					if( clothes > slayerFPK.GetSize() -1 ) continue;
					if(action_viva > slayerFPK[clothes].GetSize() -1) continue;
					try{
					FRAME_ARRAY& FA = slayerFPK[ clothes ][ action_viva ][ direction ];
					
					// 있는 동작인 경우
					if (FA.GetSize() > frame)
					{
						CFrame &Frame = FA[frame];					
						int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
						int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
						int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
						
						
						CIndexSprite* pSprite = &addonISPK[ sprite ];					
						
						
						pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
						pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
						
						//---------------------------------------- 
						// 캐릭터 선택 사각형 영역 설정
						//---------------------------------------- 	
						rect.left	= pointTemp.x;
						rect.top	= pointTemp.y;
						rect.right	= rect.left + pSprite->GetWidth();
						rect.bottom = rect.top + pSprite->GetHeight();
						pCreature->AddScreenRect( &rect );
						
						
						//------------------------------------------------------
						// 잔상 추가
						//------------------------------------------------------
						if (clothesType==AC_ADDON_RIGHTHAND)	// the AC order's slot (ADDON_RIGHTHAND is AC_ADDON_SHOULDER)
						{			
							// 0, 1, 2, 3, 4, 5
							const int darkbits[6] =
							{
								0, 1, 1, 2, 2, 3
							};
							
							POINT pointTemp2;
							
							int maxShadow = min( frame, 5 );
							
							for (int f=1; f<=maxShadow; f++)
							{
								int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
								
								if(colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )										
								{
									colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
									if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
										colorSet1 = colorSet2;
									
								}
								
								CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
								
								//---------------------------------------- 
								// f frame 전
								//---------------------------------------- 
								CFrame &Frame_f = FA[frame-f];					
								int sprite_f	= Frame_f.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
								
								CIndexSprite* pSprite_f = &addonISPK[ sprite_f ];					
								
								int cx_f		= Frame_f.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
								int cy_f		= Frame_f.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
								
								pointTemp2.x = pPoint->x + cx_f;// + pCreature->GetSX();
								pointTemp2.y = pPoint->y + cy_f;// + pCreature->GetSY();
								
								if( pCreature->HasEffectStatus( EFFECTSTATUS_REDIANCE ) )
									packs.pSurface->BltIndexSpriteColorSet(&pointTemp2, pSprite_f, addonInfo.EffectColorSet);
								else
									packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite_f, darkbits[f]);
								
							}								
						}
						
						//---------------------------------------- 
						// 캐릭터 부분 색깔이 바뀌는 경우
						//---------------------------------------- 
						if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
						{
							packs.pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite,
								addonInfo.EffectColorSet);
						}
						//---------------------------------------- 
						// 정상적인 캐릭터 출력
						//---------------------------------------- 
						else
						{
							int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
							if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
							{
								
								colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
								if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
									colorSet1 = colorSet2;
							}
							
							CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
							
							// 어둡게 찍기
							if (pCreature->IsFade())
							{
								packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
							}
							else
							{
								packs.pSurface->BltIndexSprite(&pointTemp, pSprite);
							}
						}						
						
						// 정상적인 출력
						//CIndexSprite::SetUsingColorSet( addonInfo.ColorSet1, addonInfo.ColorSet2 );
						//m_pSurface->BltIndexSprite(&pointTemp, pSprite);
					}
					}catch(...) {}
				}
			}
			
			else
			{
				clothes = GetAdvancementPartFromItemClass( addonInfo.ItemClass , addonInfo.FrameID);
				
				if( clothes == -1 )
					continue;
				try{
				FRAME_ARRAY &FA = slayerFPK[clothes][action][direction];
				
				// 있는 동작인 경우
				if (FA.GetSize() > frame)
				{
					CFrame &Frame = FA[frame];					
					int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
					int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
					int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
					
					
					CIndexSprite* pSprite = &addonISPK[ sprite ];					
					
					
					pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
					pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
					
					//---------------------------------------- 
					// 캐릭터 선택 사각형 영역 설정
					//---------------------------------------- 	
					rect.left	= pointTemp.x;
					rect.top	= pointTemp.y;
					rect.right	= rect.left + pSprite->GetWidth();
					rect.bottom = rect.top + pSprite->GetHeight();
					pCreature->AddScreenRect( &rect );
					
					
					//------------------------------------------------------
					// 잔상 추가
					//------------------------------------------------------
					if (clothesType==AC_ADDON_RIGHTHAND)	// the AC order's slot (ADDON_RIGHTHAND is AC_ADDON_SHOULDER)
					{			
						// 0, 1, 2, 3, 4, 5
						const int darkbits[6] =
						{
							0, 1, 1, 2, 2, 3
						};
						
						POINT pointTemp2;
						
						int maxShadow = min( frame, 5 );
						
						for (int f=1; f<=maxShadow; f++)
						{
							int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
							
							if(colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )										
							{
								colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
								if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
									colorSet1 = colorSet2;
								
							}
							
							CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
							
							//---------------------------------------- 
							// f frame 전
							//---------------------------------------- 
							CFrame &Frame_f = FA[frame-f];					
							int sprite_f	= Frame_f.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
							
							CIndexSprite* pSprite_f = &addonISPK[ sprite_f ];					
							
							int cx_f		= Frame_f.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
							int cy_f		= Frame_f.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();
							
							pointTemp2.x = pPoint->x + cx_f;// + pCreature->GetSX();
							pointTemp2.y = pPoint->y + cy_f;// + pCreature->GetSY();
							
							if( pCreature->HasEffectStatus( EFFECTSTATUS_REDIANCE ) )
								packs.pSurface->BltIndexSpriteColorSet(&pointTemp2, pSprite_f, addonInfo.EffectColorSet);
							else
								packs.pSurface->BltIndexSpriteDarkness(&pointTemp2, pSprite_f, darkbits[f]);
							
						}								
					}
					
					//---------------------------------------- 
					// 캐릭터 부분 색깔이 바뀌는 경우
					//---------------------------------------- 
					if (addonInfo.bEffectColor)	//colorSet < MAX_COLORSET)
					{
						packs.pSurface->BltIndexSpriteColorSet(&pointTemp, pSprite,
							addonInfo.EffectColorSet);
					}
					//---------------------------------------- 
					// 정상적인 캐릭터 출력
					//---------------------------------------- 
					else
					{
						int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
						if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
						{
							
							colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
							if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
								colorSet1 = colorSet2;
						}
						
						CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
						
						// 어둡게 찍기
						if (pCreature->IsFade())
						{
							packs.pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
						}
						else
						{
							packs.pSurface->BltIndexSprite(&pointTemp, pSprite);
						}
					}						
					
					// 정상적인 출력
					//CIndexSprite::SetUsingColorSet( addonInfo.ColorSet1, addonInfo.ColorSet2 );
					//m_pSurface->BltIndexSprite(&pointTemp, pSprite);
					
				}
				}catch(...){}
			}
		}
	}
}

void	DrawCentauroTurret( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame , int body)
{
	(void)action;
	(void)direction;
	(void)frame;
	MFakeCreature *pFakeCreature = (MFakeCreature *)pCreature;
	BYTE direct = pFakeCreature->GetTurretDirection();
	
	FRAME_ARRAY& FA = (*packs.pCreatureFPK)[body][ACTION_VAMPIRE_DRAIN][2];
	
	CFrame& Frame =	FA[direct];
	int sprite = Frame.GetSpriteID(),	//m_CreatureFPK[body][action][direction][frame].GetSpriteID(),
		cx		= Frame.GetCX(),	//m_CreatureFPK[body][action][direction][frame].GetCX(),
		cy		= Frame.GetCY();	//m_CreatureFPK[body][action][direction][frame].GetCY();

	if( pCreature->GetCreatureType() == 702 )
	{
		POINT pointGap[8] = 
		{
			{ 11, 6 },	// 좌
			{ 8, 2 },	// 좌하
			{ 0, 0 },	// 하
			{ -7, 2 },	// 우하
			{ -10, 6 },	// 우
			{ -7, 10 },	// 우상
			{ 0, 12 },	// 상
			{ 8, 10 },	// 좌상
		};
		cx += pointGap[pFakeCreature->GetDirection()].x;
		cy += pointGap[pFakeCreature->GetDirection()].y;
	}
	
	if( pFakeCreature->GetTurretDelay() > 0 )
	{
		extern POINT g_DirectionValue[MAX_DIRECTION];
		BYTE direct = (pFakeCreature->GetTurretDirection()/4+2)%8;
		cx -= g_DirectionValue[direct].x*pFakeCreature->GetTurretDelay()*2;
		cy -= g_DirectionValue[direct].y*pFakeCreature->GetTurretDelay()*2;
	}
	
	// 좌표 보정
	pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
	pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();
	
	int colorSet1 = pCreature->GetBodyColor1();
	int colorSet2 = pCreature->GetBodyColor2();
	
	if( colorSet1 == QUEST_ITEM_COLOR || colorSet1 == UNIQUE_ITEM_COLOR )
		colorSet1 = MItem::GetSpecialColorItemColorset( colorSet1 );
	
	if( colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )
		colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
	
	CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );

	CIndexSprite* pSprite = &(*packs.pCreatureSPK)[ sprite ];

	//m_pSurface->BltIndexSprite(&pointTemp, pSprite);
	if(packs.selectedCreatureID == pCreature->GetID())
		packs.pOutlines->Add( pointTemp.x, pointTemp.y, pSprite );
	else
		packs.pSurface->BltIndexSprite(&pointTemp, pSprite);
}

void	DrawInstallTurret( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, int frameindex )
{
	//	frame = pCreature->GetInstallTurretCount();
	if(ACTION_STAND == action)
	{
		direction = 2;
		frame = pCreature->GetInstallTurretCount();
	}
	FRAME_ARRAY& FA = (*packs.pCreatureFPK)[222][action][direction];
	
	if (FA.GetSize() > frame)
	{			
		CFrame&	Frame = FA[frame];
		
		int sprite = Frame.GetSpriteID(),	//m_CreatureFPK[body][action][direction][frame].GetSpriteID(),
			cx		= Frame.GetCX(),	//m_CreatureFPK[body][action][direction][frame].GetCX(),
			cy		= Frame.GetCY();	//m_CreatureFPK[body][action][direction][frame].GetCY();
		
		CIndexSprite* pSprite = &(*packs.pCreatureSPK)[ sprite ];
		
		pointTemp.x = pPoint->x + cx;// + pCreature->GetSX();
		pointTemp.y = pPoint->y + cy;// + pCreature->GetSY();			
		
		//---------------------------------------- 		
		// 캐릭터 선택 사각형 영역 설정
		//---------------------------------------- 	
		rect.left	= pointTemp.x;
		rect.top	= pointTemp.y;
		rect.right	= rect.left + pSprite->GetWidth();
		rect.bottom = rect.top + pSprite->GetHeight();
		if(frameindex ==0 )
			pCreature->SetScreenRect( &rect );
		
		if(packs.selectedCreatureID == pCreature->GetID())
		{
			WORD colorSet = pCreature->GetAttachEffectColor();
			packs.pOutlines->Add( pointTemp.x, pointTemp.y, pSprite, colorSet );
		}
		else
			packs.pSurface->BltIndexSprite(&pointTemp, pSprite);
		
	}
}


