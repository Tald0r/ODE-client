//----------------------------------------------------------------------
// MTopView.h
//----------------------------------------------------------------------
//
// Class that draws a particular location of a Zone.
//
//----------------------------------------------------------------------
//
// DrawTile()     : draws only the Tiles of the current Surface.
//                                   Draws them into m_pTileSurface.
//
// DrawZone()	  : draws the Tiles on the current Surface using the previous Tiles
//                                   and then renders everything in the Zone.
//
// * When the current Tile changes suddenly (on Zone change, on Teleport),
//     set m_bFirstTileDraw = true with SetFirstDraw(), and
//     Draw() picks DrawTile() once,
//                                         then DrawZone() from then on.
//
//---------------------------------------------------------------------- 
//
// - The Surface that stores the previous Tiles is larger than the actual screen.
//     Since (800,600) is the actual screen,
//     the Surface is created at (1152,768), 3 Tiles larger on every side.
//
// - With this, when the area currently needed is read from the previous Tile Surface,
//     if the range being read lies within the previous Tile Surface,
//     there may be no part that has to be redrawn.
//     Also, in that case the Tile Surface needs no update either.
// 
// - The downside is that the part to redraw is a little~ bigger when redrawing,
//     but the average speed improves, so it should not be much of a problem.
//
// * Adjust the size after testing...
//
//---------------------------------------------------------------------- 
//
// * The SpriteSet / FrameSet information has to be read in.
//
// [ MapView : Filename.MV ]   - loaded by the View.
//   Tile SpriteSet
//   Obstacle SpriteSet 
//   Obstacle FrameSet
//   ImageObject SpriteSet
//
//---------------------------------------------------------------------- 
//
// ImageObject Map...
//
// At first... scan all Tiles
// then.. scan only the Tiles that changed from the previous ones
//
//---------------------------------------------------------------------- 

#ifndef	__MTOPVIEW_H__
#define	__MTOPVIEW_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include "MViewDef.h"
#include "CreatureDrawPacks.h"
#include "WorldTileGeometry.h"
#include "MScreenFade.h"
#include <fstream>
#include <map>
#include <queue>


//----------------------------------------------------------------------
//
// using class..
//
//----------------------------------------------------------------------
#include "MZone.h"
class MObject;
#include "MImageObject.h"
#include "MCreature.h"
class MMissile;
class MItem;
class MEffect;
#include "SpriteLib/SP.h"
#include "FR.h"
//#include "2D.h"
#include "DrawTextNode.h"
#include "DrawItemNameNode.h"
#include "COrderedList.h"
#include "MRequestMode.h"
#include "SpriteLib/CIndexSpritePack.h"
#include "SpriteLib/CFilter.h"
#include "SpriteLib/CFilterPack.h"
#include "SpriteLib/CSpriteOutlineManager.h"

// TileRenderer for unified tile rendering
#include "SpriteLib/TileRenderer.h"
#include "SpriteLib/ZoneTileProvider.h"
#include "framelib/EffectResourceContainer.h"


//class MZoneInfo;
class TextComparison {
	public :
		// The older one must be chosen.
		// true : choose right.
		// false : choose left.
		bool operator () (DRAWTEXT_NODE * left, DRAWTEXT_NODE * right) const;
};

// How some creature types are drawn differently (gamemodel's since 4.229).
#include "CreatureDrawExceptions.h"

//----------------------------------------------------------------------
// Class that draws the picture of a particular Zone on a Surface
//----------------------------------------------------------------------
class MTopView : public MRequestMode {
	// The view's tests read its members through these, one per test file
	// (task 4.260).
	friend struct MTopViewTestAccess1;
	friend struct MTopViewTestAccess2;
	friend struct MTopViewTestAccess3;
	friend struct MTopViewTestAccess4;

	public :	
		// ImageObject map used for output
		typedef std::map<QWORD, MImageObject*>			IMAGEOBJECT_OUTPUT_MAP;

		// Creature map used for output
		typedef std::map<QWORD, MCreature*>				CREATURE_OUTPUT_MAP;

		// Item map used for output
		typedef std::map<QWORD, MItem*>					ITEM_OUTPUT_MAP;

		// Place to remember the strings to output..
		//typedef	std::list<DRAWTEXT_NODE*>				DRAWTEXT_LIST;
		typedef std::priority_queue<DRAWTEXT_NODE*, std::vector<DRAWTEXT_NODE*>, TextComparison>	DRAWTEXT_PQ;

		// item name output
		typedef	std::list<DRAWITEMNAME_NODE*>			DRAWITEMNAME_LIST;

		// int ordered list
		typedef	COrderedList<int>						INT_ORDERED_LIST;

	public :
		MTopView();
		~MTopView();

		//------------------------------------------------------
		//
		// init
		//
		//------------------------------------------------------
		bool		Init();
		bool		InitChanges();	// when switching 2D <--> 3D.. hmm - -;
		bool		IsInit() const		{ return m_bInit; }
		void		Release();
		void		SetSurface(CSpriteSurface*& pSurface);
		void		SetZone(MZone* pZone);
		void		RestoreSurface();
		void		ClearShadowManager();
		const FRAME_ARRAY&	GetCreatureFrameArray(int body, int action, int direction) const	{ return m_CreatureFPK[body][action][direction]; }
		const CIndexSprite*	GetCreatureSprite(int spriteID)		{ return &m_CreatureSPK[ spriteID ]; }

		//------------------------------------------------------
		//
		// Things that must be loaded per Zone
		//
		//------------------------------------------------------		
		void		LoadMinimap(const char* filename);//, MZoneInfo* pZoneInfo=NULL);
		bool		LoadFromFileTileSPKLargeZone(std::ifstream & file);	// zone-related spk load
		bool		LoadFromFileImageObjectSPKLargeZone(std::ifstream & file);	// zone-related spk load
		bool		LoadFromFileTileSPKSmallZone(std::ifstream & file);	// zone-related spk load
		bool		LoadFromFileImageObjectSPKSmallZone(std::ifstream & file);	// zone-related spk load
		void		LoadFromFileCreatureSPK(int n);			// creature load		
// 		void		LoadFromFileAddonSPK(int frame, int action);		// addon load		
//		void		LoadFromFileCreatureActionSPK(int frame, int action);		// addon load		
		bool		LoadFromFileTileAndImageObjectSet(const CSpriteSetManager &TileSSM, const CSpriteSetManager &ImageObjectSSM);
		void		ReleaseCreatureSPK(int n);			// creature release
		void		ReleaseTileSPKLargeZone();
		void		ReleaseImageObjectSPKLargeZone();
		void		ReleaseTileSPKSmallZone();
		void		ReleaseImageObjectSPKSmallZone();
		//void		ReleaseAddonSPK(int frame, int action);
//		void		ReleaseCreatureActionSPK(int frame, int action);

		void		ReleaseUselessCreatureSPKExcept(const INT_ORDERED_LIST& listUse);
		void		AddMonsterSpriteTypes(const INT_ORDERED_LIST& liseUse);

//		void		StopLoadImageObjectSPK()	{ m_ImageObjectSPK.SetLoadingStop(); }
//		void		StopLoadTileSPK()			{ m_TileSPK.SetLoadingStop(); }

		// When all the Tiles must be redrawn
		void		SetFirstDraw()			{ m_bFirstTileDraw = true; }
		
		//------------------------------------------------------
		//
		// Draw Functions
		//
		//------------------------------------------------------
		void		Draw(int firstPointX, int firstPointY);

		// Help text for testing
		void		DrawDebugInfo(void* pSurface);


		void		DrawBloodBibleEffect_InGear(POINT* pPoint);
		
		//------------------------------------------------------
		//
		// Fade In/out
		//
		//------------------------------------------------------
		bool			IsFade() const			{ return m_Fade.IsActive(); }
		void			SetFadeStart(signed char start, signed char end, signed char step, BYTE r=0, BYTE g=0, BYTE b=0, WORD delay = 0);
		void			SetFadeEnd() 			{ m_Fade.Stop(); }
		
		//------------------------------------------------------
		//
		// Coordinate conversion
		//
		//------------------------------------------------------
		static POINT	PixelToMap(const int& Xp, const int& Yp)	{ POINT p; p.x=WorldTileGeometry::PixelToTileX(Xp); p.y=WorldTileGeometry::PixelToTileY(Yp); return p; }//{ POINT p; p.x=Xp>>6; p.y=Yp>>5; return p; }
		static POINT	MapToPixel(const int& Xm, const int& Ym)	{ POINT p; p.x=WorldTileGeometry::TileToPixelX(Xm); p.y=WorldTileGeometry::TileToPixelY(Ym); return p; }//{ POINT p; p.x=Xm<<6; p.y=Ym<<5; return p; }

		static int		PixelToMapX(const int& Xp)		{ return WorldTileGeometry::PixelToTileX(Xp); }
		static int		PixelToMapY(const int& Yp)		{ return WorldTileGeometry::PixelToTileY(Yp); }
		static int		MapToPixelX(const int& Xm)		{ return WorldTileGeometry::TileToPixelX(Xm); }
		static int		MapToPixelY(const int& Ym)		{ return WorldTileGeometry::TileToPixelY(Ym); }

		static BYTE		GetDirectionToPosition(int originX, int originY, int destX, int destY);
		static POINT	GetChangeValueToDirection(int direction);

		//------------------------------------------------------
		// Optimized coordinate calculation (using Shift) when TileSize is 64*32..
		//------------------------------------------------------
		//static POINT	PixelToMap(const int& Xp, const int& Yp)	{ POINT p; p.x=Xp>>6; p.y=Yp>>5; return p; }
		//static POINT	MapToPixel(const int& Xm, const int& Ym)	{ POINT p; p.x=Xm<<6; p.y=Ym<<5; return p; }

		//static int		PixelToMapX(const int& Xp)		{ return (Xp >> 6); }
		//static int		PixelToMapY(const int& Yp)		{ return (Yp >> 5); }
		//static int		MapToPixelX(const int& Xm)		{ return (Xm << 6); }	
		//static int		MapToPixelY(const int& Ym)		{ return (Ym << 5); }


		//------------------------------------------------------
		//
		// Relation between the View and the screen
		//
		//------------------------------------------------------
		// Which point, in Zone pixel coordinates, is the on-screen coordinate (x,y)?
		POINT			ScreenToPixel(int x, int y);

		// Which point on the screen is the Zone pixel coordinate?
		POINT			MapToScreen(int sX, int sY);
		POINT			PixelToScreen(int x, int y);
		POINT			GetFirstZonePixel()	const		{ return m_FirstZonePixel; }		
		POINT			GetFirstSector() const			{ return m_FirstSector; }

		//------------------------------------------------------
		//
		// Object selected with the Mouse
		//
		//------------------------------------------------------
		// Selected Sector
		inline void		SetSelectedSector(int sX, int sY)	{ m_SelectSector.x = sX; m_SelectSector.y = sY; }	
		inline void		SetSelectedSector(const POINT& s)	{ m_SelectSector.x = s.x; m_SelectSector.y = s.y; }
		void			SetSelectedSectorNULL()		{ m_SelectSector.x = m_SelectSector.y = SECTORPOSITION_NULL; }
		POINT			GetSelectedSector(int x, int y);// const;

		// Selection mode when attacking
		//void			SetSelectModeAttack()				{ m_bSelectModeAttack = true; }
		//void			SetSelectModeNormal()				{ m_bSelectModeAttack = false; }


		// Returns the Object selected when (x,y) on the screen is chosen.
		MObject*		GetSelectedObject(int x, int y);
		MObject*		GetSelectedObjectSprite(int x, int y);

		// Select a Creature
//		void			SetSelectedCreature(TYPE_OBJECTID id)	{ m_SelectCreatureID = id; m_SelectItemID = m_SelectInteractionObjectID = m_SelectEffectID = OBJECTID_NULL; }		
		void			SetSelectedCreature(TYPE_OBJECTID id)	{ m_SelectCreatureID = id; m_SelectItemID = m_SelectEffectID = OBJECTID_NULL; }		
		TYPE_OBJECTID	GetSelectedCreature() const				{ return m_SelectCreatureID; }

		// Select an Item
//		void			SetSelectedItem(TYPE_OBJECTID id)	{ m_SelectItemID = id;  m_SelectCreatureID = m_SelectInteractionObjectID = m_SelectEffectID = OBJECTID_NULL; }
		void			SetSelectedItem(TYPE_OBJECTID id)	{ m_SelectItemID = id;  m_SelectCreatureID = m_SelectEffectID = OBJECTID_NULL; }
		TYPE_OBJECTID	GetSelectedItemID()					{ return m_SelectItemID; }
		
		// Select an InteractionObject
//		void			SetSelectedInteractionObject(TYPE_OBJECTID id)	{ m_SelectInteractionObjectID = id; m_SelectItemID = m_SelectCreatureID = m_SelectEffectID = OBJECTID_NULL; }

		// Select an Effect
//		void			SetSelectedEffect(TYPE_OBJECTID id)		{ m_SelectEffectID = id; m_SelectItemID = m_SelectCreatureID = m_SelectInteractionObjectID = OBJECTID_NULL; }
		void			SetSelectedEffect(TYPE_OBJECTID id)		{ m_SelectEffectID = id; m_SelectItemID = m_SelectCreatureID =  OBJECTID_NULL; }
		
		// Make nothing selected.
//		void			SetSelectedNULL()				{ m_SelectCreatureID = m_SelectItemID = m_SelectInteractionObjectID = m_SelectEffectID = OBJECTID_NULL;}
		void			SetSelectedNULL()				{ m_SelectCreatureID = m_SelectItemID = m_SelectEffectID = OBJECTID_NULL;}


		//------------------------------------------------------
		//
		// Sight (field of view) related..
		//
		//------------------------------------------------------		
		BYTE			GetDarkBits() const				{ return m_DarkBits; }
		void			SetDarkBits(BYTE DarkBits)		{ if (DarkBits<16) m_DarkBits = DarkBits; }

		// 3D sight related
		void			ClearLightBufferFilter3D();
		void			AddLightFilter3D(int x, int y, BYTE range, bool bMapPixel=true, bool bForceLight=false);
		void			DrawLightBuffer3D();

		// 2D sight related
		void			ClearLightBufferFilter2D();
		void			AddLightFilter2D(int x, int y, BYTE range, bool bMapPixel=true, bool bForceLight=false);
		void			DrawLightBuffer2D();		

		//------------------------------------------------------
		//
		// Things related to Creature Output
		//
		//------------------------------------------------------
		QWORD			GetOutputImageObjectID(const MImageObject* pImageObject) const
											{ return ((QWORD)pImageObject->GetViewpoint() << 32) | pImageObject->GetID(); }

		//------------------------------------------------------
		//
		// Things related to Creature Output
		//
		//------------------------------------------------------		
		// Made up of [pixel y coordinate][object id] ,  4 bytes  +  4 bytes  = 8 bytes.
		QWORD			GetOutputCreatureID(const MCreature* pCreature) const	
											{ return ((QWORD)pCreature->GetPixelY() << 32) | pCreature->GetID(); }

		QWORD			GetOutputCreatureID(TYPE_OBJECTID id, int y) const	
											{ return ((QWORD)y << 32) | id; }

		// Add the whole Creature map of m_pZone in one go~
		void			AddOutputCreatureAll();

		// Add pCreature.
		bool			AddOutputCreature(MCreature* pCreature);

		// Clear the Creature OutputMap.
		void			ClearOutputCreature()	{ m_mapCreature.clear(); }

		// Remove the creature associated with the id.
		bool			RemoveOutputCreature(MCreature* pCreature);

		// Update the information (id related to the output coordinates) of the creature associated with the id.
		bool			UpdateOutputCreature(TYPE_OBJECTID id, int y0, int y1);


		//------------------------------------------------------		
		//
		// Output chat strings etc..
		//
		//------------------------------------------------------		
		void			ClearTextList();
		void			AddText(DRAWTEXT_NODE* pNode);
		void			DrawTextList();

		//------------------------------------------------------		
		//
		// ItemName output..
		//
		//------------------------------------------------------		
		bool			IsDrawItemNameList() const		{ return m_bDrawItemNameList; }
		void			SetDrawItemNameList()			{ m_bDrawItemNameList = true; }
		void			UnSetDrawItemNameList()			{ m_bDrawItemNameList = false; }
		void			ClearItemNameList();
		void			AddItemName(DRAWITEMNAME_NODE* pNode);
		void			DrawItemNameList();
		MItem*			SelectItemName(int x, int y);

		//------------------------------------------------------		
		//
		// Request
		//
		//------------------------------------------------------		
		bool			IsDrawRequest() const			{ return m_bDrawRequest; }
		
		//------------------------------------------------------		
		// Get MaxEffectFrame
		//------------------------------------------------------		
		int				GetMaxEffectFrame(BLT_TYPE bltType, TYPE_FRAMEID frameID) const;
		int				GetEffectLight(BLT_TYPE bltType, TYPE_FRAMEID frameID, int dir, int frame) const;
		int				GetEffectSpriteType(BLT_TYPE bltType, TYPE_FRAMEID frameID) const;

		//------------------------------------------------------		
		// Etc..
		//------------------------------------------------------		
		int				GetRandomMonsterTypeInZone() const;

//		void			InitMinimapTexture(CSpriteSurface *minimap_surface);			// initialize minimap texture

	protected :
		//------------------------------------------------------
		//
		// Functions that initialize internally..
		//
		//------------------------------------------------------
		bool			InitCreatureFrames();		// initialize character information
		bool			InitImageFrames();			// initialize ImageFrames information
		bool			InitAnimationFrames();		// initialize AnimationFrames
		bool			InitEffectFrames();			// initialize EffectFrames
		bool			InitSprites();				// initialize Sprites
		bool			InitSurfaces();				// initialize Surfaces
		bool			InitFilters();				// initialize Filters
		//bool	InitSpriteSurfaces();		// initialize SpriteSurfaces
		bool			InitColors();				// initialize colors
// 		bool			Init3DBoxSurface();
		bool			InitFonts();				// initialize font information

		// Set the color of the box surface
// 		void			Set3DBoxColor(WORD pixel);

		//------------------------------------------------------
		// Change the ImageObjects shown on the screen
		//------------------------------------------------------
		void			DetermineImageObject();
		void			UpdateImageObject(const POINT &newFirstSector);

		//------------------------------------------------------
		// SurfaceLock
		//------------------------------------------------------
		//BOOL			SurfaceLock();
		//BOOL			SurfaceLockForEffect();
		//void			SurfaceLockRestore( BOOL bOldLock );

		//------------------------------------------------------
		// Draw functions..
		//------------------------------------------------------
		void		DrawTileSurface();
		void		DrawZone(int X,int Y);
		void		DrawInformation();		
		void		DrawEventString(int &strX, int &strY);			// Function that takes the part printed as a string by the event manager and draws it on the screen
		int			DrawChatString(POINT* pPoint, MCreature* pCreature, COLORREF color, BYTE flag=0);
		void		DrawItem(POINT* pPoint, MItem* pItem);
		void		DrawItemShadow(POINT* pPoint, MItem* pItem);
		void		DrawImageObject(POINT* pPoint, MImageObject* pImageObject);
		void		DrawAttachEffect(POINT* pPoint, ATTACHEFFECT_LIST::const_iterator iEffect, BYTE size, MCreature* pCreature, int type=0);
		void		DrawGroundEffect();
		void		DrawEffect(POINT* pPoint, MEffect* pEffect, bool bSeletable);
		void		DrawEffect(POINT* pPoint, EFFECT_LIST::const_iterator iEffect, BYTE size);
		void		DrawAlphaBox(RECT* pRect, BYTE r, BYTE g, BYTE b, BYTE alpha);
		void		DrawTestHelp();
		void		DrawMinimap(int x, int y, BYTE scale);
		void		DrawItemBroken(int x, int y);
		void		DrawPartyHP(POINT* pPoint, MCreature* pCreature);
// 		void		DrawBox3D(RECT* pRect, WORD pixel);			
		void		DrawFade();

		//------------------------------------------------------
		// Character Draw functions... by sonee   2003.6
		//------------------------------------------------------
		void		DrawCreature(POINT* pPoint, MCreature* pCreature);		
		void		DrawCreatureShadow(POINT* pPoint, MCreature* pCreature);
		void		DrawCreatureName(MCreature* pCreature);
		void		DrawCreatureMyName();
		void		DrawCreatureHPModify(POINT *point, MCreature* pCreature);
		void		DrawUndergroundCreature(POINT *pPoint, MCreature *pCreature);

		void		DrawGuildMarkInSiegeWar(MCreature* pCreature, int YPos); // Let's draw the guild mark in the big size..lol

	protected :		
		bool				m_bInit;

		//------------------------------------------------------
		// Surface that is the output target
		//------------------------------------------------------
		CSpriteSurface*		m_pSurface;
		//S_SURFACEINFO		m_SurfaceInfo;
	

		// SpriteSurfacePack
		//CSpriteSurfacePack	m_SpriteSurfacePack;	// pictures
		//CSpriteSurface**			m_ppSurface;			// SpriteSurfaces

		//------------------------------------------------------
		//
		//                   Frame
		//
		//------------------------------------------------------
	public :
		//------------------------------------------------------
		// FramePacks
		//------------------------------------------------------		
		CCreatureFramePack		m_CreatureFPK;			// Creature frames
		CCreatureFramePack		m_AddonFPK;				// Player addon frames
		CCreatureFramePack		m_OustersFPK;			// Ousters frames

		CCreatureFramePack		m_AdvancementSlayerManFPK;			// Creature frames
		CCreatureFramePack		m_AdvancementSlayerWomanFPK;			// Creature frames
		CCreatureFramePack		m_AdvancementVampireManFPK;				// Player addon frames
		CCreatureFramePack		m_AdvancementVampireWomanFPK;				// Player addon frames
		CCreatureFramePack		m_AdvancementOustersFPK;			// Ousters frames

		CImageFramePack			m_ItemTileFPK;			// for the Item on a Tile..
		CAnimationFramePack		m_ItemDropFPK;			// for the Item falling to the ground..
		CAnimationFramePack		m_ItemBrokenFPK;		// for the Item breaking..
		CAnimationFramePack		m_ImageObjectFPK;		// frames for ImageObject
		CAnimationFramePack		m_ImageObjectShadowFPK;		// frames for ImageObject
//		CAnimationFramePack		m_InteractionObjectFPK;		// frames for ImageObject
		CEffectFramePack		m_EffectAlphaFPK;		// frames for Effect
		CEffectFramePack		m_EffectNormalFPK;		// frames for Effect
		CEffectFramePack		m_EffectShadowFPK;		// frames for Effect
		CEffectFramePack		m_EffectScreenFPK;
		
		CCreatureFramePack		m_CreatureShadowFPK;			// Creature frames
		CCreatureFramePack		m_AddonShadowFPK;			// Player addon frames
		CCreatureFramePack		m_OustersShadowFPK;			// Ousters addon frames

		CCreatureFramePack		m_AdvancementSlayerManShadowFPK;			// Creature frames
		CCreatureFramePack		m_AdvancementSlayerWomanShadowFPK;			// Creature frames
		CCreatureFramePack		m_AdvancementVampireManShadowFPK;			// Player addon frames
		CCreatureFramePack		m_AdvancementVampireWomanShadowFPK;			// Player addon frames
		CCreatureFramePack		m_AdvancementOustersShadowFPK;			// Ousters addon frames

		//add by viva
		//                   Sprite
		//
		//------------------------------------------------------
		//------------------------------------------------------
		// SpritePack
		//------------------------------------------------------		
		CSpritePack				m_TileSPK;				// Tile
		CSpritePack				m_ImageObjectSPK;		// ImageObject Sprites
//		CSpritePack				m_InteractionObjectSPK;		// ImageObject Sprites
		CIndexSpritePack		m_CreatureSPK;			// Creature Sprites
		CIndexSpritePack		m_AddonSPK;		// Sprites
		CIndexSpritePack		m_OustersSPK;		// Sprites

		CIndexSpritePack		m_AdvancementSlayerManSPK;			// Creature Sprites
		CIndexSpritePack		m_AdvancementSlayerWomanSPK;			// Creature Sprites
		CIndexSpritePack		m_AdvancementVampireManSPK;		// Sprites
		CIndexSpritePack		m_AdvancementVampireWomanSPK;		// Sprites
		CIndexSpritePack		m_AdvancementOustersSPK;		// Sprites
		
		//end
		
		//CSpritePack				m_ItemTileSPK;			// Sprites
		CIndexSpritePack		m_ItemTileISPK;			// Sprites
		CIndexSpritePack		m_ItemDropISPK;			//
		CSpritePack				m_ItemBrokenSPK;		// breaking items
		CSpritePack				m_ItemRealSPK;			// Sprites
		CAlphaSpritePalPack		m_EffectAlphaSPK;		// AlphaSprites
		CSpritePalPack			m_EffectScreenSPK;		// ScreenEffect
		MPalettePack			m_EffectAlphaPPK;		// AlphaEffect
		MPalettePack			m_EffectScreenPPK;		// ScreenEffect
		CSpritePack				m_EffectNormalSPK;		// NormalSprites
		CShadowSpritePack		m_EffectShadowSPK;		// ShadowSprites

		// Effect resource container for safe access (refactored)
		EffectResourceContainer	m_EffectResources;

		CSpritePack				m_WeatherSPK;			// WeatherSprites
		CSpritePack				m_GuildSPK;				// GuildMark

		

		CSpritePack				m_EtcSPK;				// Etc...
		CSpritePack				m_OustersFinSPK;				// Etc...
		CSpritePack				m_AdvacementQuestEnding;				// Etc...


		//------------------------------------------------------		
		// Shadow SpritePack
		//------------------------------------------------------
		CShadowSpritePack		m_AddonSSPK;			// Sprites
		CShadowSpritePack		m_CreatureSSPK;
		CShadowSpritePack		m_OustersSSPK;
		CShadowSpritePack		m_ImageObjectSSPK;

		CShadowSpritePack		m_AdvancementSlayerManSSPK;			// Sprites
		CShadowSpritePack		m_AdvancementSlayerWomanSSPK;			// Sprites
		CShadowSpritePack		m_AdvancementVampireManSSPK;
		CShadowSpritePack		m_AdvancementVampireWomanSSPK;
		CShadowSpritePack		m_AdvancementOustersSSPK;
		//add by viva
//		CShadowSpritePack		m_InteractionObjectSSPK;

		//CShadowSpritePack		m_CreatureSSPK;

//		float					m_SectorToMinimapHeight;
//		CSpriteSurface*			m_pMinimapTexture;
//		int						m_MinimapTextureWidth;
//		int						m_MinimapTextureHeight;


		//------------------------------------------------------
		//
		//                  Sprite Index
		//
		//------------------------------------------------------
		CFileIndexTable			m_TileSPKI;
		CFileIndexTable			m_ImageObjectSPKI;
//		CFileIndexTable			m_EffectAlphaSPKI;
//		CFileIndexTable			m_EffectScreenSPKI;

		
		//------------------------------------------------------
		// 
		//                  Sprite File
		//
		//------------------------------------------------------
//		std::ifstream			m_TileSPKFile;
//		std::ifstream			m_ImageObjectSPKFile;
//		std::ifstream			m_EffectAlphaSPKFile;
//		std::ifstream			m_EffectScreenSPKFile;
//		std::ifstream			m_AddonSPKFile;
//		std::ifstream			m_AddonSSPKFile;
//		std::ifstream			m_CreatureSPKFile;
//		std::ifstream			m_CreatureSSPKFile;

		//------------------------------------------------------
		//
		//                   Filter
		//
		//------------------------------------------------------
		//CFilterPack				m_Filter;
		CFilter					m_ImageObjectFilter;

		//------------------------------------------------------
		// Information about things loaded independently per Zone
		//------------------------------------------------------
		CSpriteFilePositionArray*	m_pTileSFPArrayLargeZone;
		CSpriteFilePositionArray*	m_pImageObjectSFPArrayLargeZone;
		CSpriteFilePositionArray*	m_pTileSFPArraySmallZone;
		CSpriteFilePositionArray*	m_pImageObjectSFPArraySmallZone;


		//------------------------------------------------------
		//
		//                Other Data members
		//
		//------------------------------------------------------

		//------------------------------------------------------
		// Zone
		//------------------------------------------------------
		MZone*				m_pZone;				// Zone currently being drawn

		static int			m_MiddleX;				// middle of the zone (pixel X coordinate)
		POINT				m_FirstSector;			// coordinate (sector) in the Zone of the first point on the screen
		POINT				m_PlusPoint;			// correction value, in pixels, of FirstSector
		POINT				m_FirstZonePixel;		// Zone pixel coordinate that screen (0,0) represents

		//------------------------------------------------------
		// Things on the screen selected with the Mouse..
		//------------------------------------------------------
		POINT				m_SelectSector;		
		TYPE_OBJECTID		m_SelectCreatureID;
		TYPE_OBJECTID		m_SelectItemID;
//		TYPE_OBJECTID		m_SelectInteractionObjectID;
		TYPE_OBJECTID		m_SelectEffectID;
		//bool				m_bSelectModeAttack;

		WORD					m_SOMOutlineColor;
		CSpriteOutlineManager	m_SOM;	// draw the outline of the selected one

		//------------------------------------------------------
		// Sight handling
		//------------------------------------------------------		
		POINT					m_FilterPosition;	// Sight handling that covers the Player
		BYTE					m_DarkBits;
		

		// Sight handling related - the lit part of the sight
		CFilterPack				m_LightFTP;
		CFilter					m_LightBufferFilter;
		CSpriteSurface*			m_pLightBufferTexture;
		int						m_nLight;


		//------------------------------------------------------
		// Fade In/Out
		//------------------------------------------------------		
		MScreenFade			m_Fade;
		WORD				m_FadeColor;	// fade colour
		
		//------------------------------------------------------
		// Store the Tiles of the current screen and use them the next time they are drawn.
		//------------------------------------------------------
		CSpriteSurface*				m_pTileSurface;
		bool						m_bFirstTileDraw;
		int							m_TileSurfaceFirstZonePixelX,
									m_TileSurfaceFirstZonePixelY;
		int							m_TileSurfaceFirstSectorX,
									m_TileSurfaceFirstSectorY;

		//------------------------------------------------------
		// TileRenderer for unified tile rendering (Phase 4 integration)
		//------------------------------------------------------
		TileRenderer*				m_pTileRenderer;
		ZoneTileProvider<MZone>		m_zoneTileProvider;

		//------------------------------------------------------
		// Information about the ImageObjects present on the current screen
		//------------------------------------------------------
		IMAGEOBJECT_OUTPUT_MAP				m_mapImageObject;

		//------------------------------------------------------
		// Creature OutputMap - related to output order.
		//------------------------------------------------------
		CREATURE_OUTPUT_MAP					m_mapCreature;
		bool								m_bTileSearchForCreature;

		//------------------------------------------------------
		// Information for drawing the chat of the Creature selected with the Mouse
		//------------------------------------------------------
		POINT						m_pointChatString;
		MCreature*					m_pSelectedCreature;

		//------------------------------------------------------
		// Texture Surface
		//------------------------------------------------------
		// Effect
		//CTexturePack				m_EffectTPK;
		//CTexturePack				m_CreatureTPK;
		//CTexturePack				m_ShadowTPK;	// for test

		
		//------------------------------------------------------
		// Remember the String to output..
		//------------------------------------------------------
		DRAWTEXT_PQ					m_pqDrawText;

		//------------------------------------------------------
		// ItemName output
		//------------------------------------------------------
		bool						m_bDrawItemNameList;
		DRAWITEMNAME_LIST			m_listDrawItemName;

		//------------------------------------------------------
		// Request mode
		//------------------------------------------------------
		bool						m_bDrawRequest;

		//------------------------------------------------------
		// 3D related..
		//------------------------------------------------------
//		CSpriteSurface*				m_p3DBoxSurface;
//		WORD						m_3DBoxCurrentPixel;

//		CSpriteSurface*				m_p3DBoxBlackSurface;
//		CSpriteSurface*				m_p3DBoxHPSurface;
		

		//------------------------------------------------------
		// Colors..
		//------------------------------------------------------
		// interactionObject
//		WORD		m_ColorOutlineInteractionObject;

		// item
		COLORREF	m_ColorNameItem;
		COLORREF	m_ColorNameItemOption;
		WORD		m_ColorOutlineItem;

		// by race
		COLORREF	m_ColorNameVampire;
		COLORREF	m_ColorNameSlayer;
		COLORREF	m_ColorNameNPC;

		COLORREF	m_ColorNameAlignment[5];	// hmm..

		// Attackable?
		WORD		m_ColorOutlineNPC;
		WORD		m_ColorOutlineAttackPossible;
		WORD		m_ColorOutlineAttackImpossible;

		// color of the hp bar
		WORD		m_ColorHPBar;
		WORD		m_ColorHPBarBg;  // HP bar background color (max HP)

		// UI box background color
		WORD		m_ColorUIBoxBg;  // UI box background (item descriptions etc.)

		// semi-transparent black
		WORD		m_ColorBlackHalf;


		//------------------------------------------------------
		// Screen pixel size taken up by one point in the 2D light
		//------------------------------------------------------
		int*		m_p2DLightPixelWidth;
		int*		m_p2DLightPixelHeight;

		//------------------------------------------------------
		// Information about what was loaded
		//------------------------------------------------------
		INT_ORDERED_LIST	m_listLoadedCreatureSprite;
		INT_ORDERED_LIST	m_listLoadedMonsterSprite;
		
	public :
		// Size on the screen of one Light coordinate
		static float				s_LightWidth;
		static float				s_LightHeight;
		
		// 2004, 8, 18 sobeit add start - added somehow because of siege-war gate targeting..
		enum GENERATE_OPTION{
			GENERATE_ALL = 0,
			GENERATE_EXCEPT_LEFT,	// exclude the left outline
			GENERATE_EXCEPT_RIGHT,  // exclude the right outline
			GENERATE_EXCEPT_SIDE,	// exclude the outlines on both sides
			// -_- do I need to make top and bottom too..-_-;
		};
		// 2004, 8, 18 sobeit add start

	private :
		// -_- exception handling pulled out into a function pointer.
		//DrawCreatureExecptionProc*	m_pExceptionProc;

		// DrawSelectedCharacter
		void	DrawSelectedVampireCreature( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame, int body, int frameindex );
		void	DrawSelectedSlayerCreature( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame );
		void	DrawSelectedOustersCreature( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame );

		void	DrawSelectedAdvancementVampireCreature( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame, int body, int frameindex );
		void	DrawSelectedAdvancementSlayerCreature( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame );
		void	DrawSelectedAdvancementOustersCreature( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame );

		// DrawEffect
		
		
		// The packs and surface the creature drawing moved to gameview reads
		// (task 4.248: the divine-guidance silhouettes).
		CreatureDrawPacks	GetCreatureDrawPacks();
		
		// DrawNormalCharacter
		void	DrawSlayerCharacter( POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, int FrameIndex );		
		void	DrawVampireCharacter( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame, int body, int frameindex );
		void	DrawOustersCharacter( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame );


		// DrawAdvancementClassCharacter
		void	DrawAdvancementClassSlayerCharacter( POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, int FrameIndex );
		void	DrawAdvancementClassOustersCharacter( POINT *pPoint, MCreature* pCreature, int action, int direction, int frame );
		void	DrawAdvancementClassVampireCharacter( POINT* pPoint, MCreature* pCreature, int action, int direction, int frame, int body, int frameindex );

		// 2004, 12, 11, sobeit add start
		// 2004, 12, 11, sobeit add end
		
		// 2005, 1, 20, sobeit add start
		bool	DrawEvent();
		bool	ExcuteOustersFinEvent();
		bool	ExcuteAdvancementQuestEnding(void *pVoid);
		// 2005, 1, 20, sobeit add end
		
		std::map< DWORD, DrawCreatureExceptionProc >	m_ExceptionProcMap;
};

extern MTopView*	g_pTopView;

#endif
