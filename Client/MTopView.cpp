

//----------------------------------------------------------------------
// MTopView.cpp
//----------------------------------------------------------------------
// [ Effect Layer ]  - in drawing order:
//
//		-- the tiles
//	* effects attached to a tile
//
//
//	* a sector's effects
//		-- each character
//	* the effects attached to a character
// 
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "DirectionSelection.h"
#include "CrtCompat.h"
#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

// DX3D.h removed (SDL2) - Direct3D has been replaced with SDL2
// Unified: All platforms use TextSystem
#include "TextSystem/TextService.h"
#include "DataPath.h"
#include <math.h>
#include <list>
#include <stdio.h>
#include <fstream>
#include "MZone.h"
#include "MCreature.h"
#include "MFakeCreature.h"
#include "MPlayer.h"
#include "MItem.h"
#include "MImageObject.h"
#include "MAnimationObject.h"
#include "MShadowAnimationObject.h"
//#include "MInteractionObject.h"
#include "MMovingEffect.h"
#include "MScreenEffect.h"
#include "MTopView.h"
#include "MItemTable.h"
#include "MCreatureTable.h"
#include "MWeather.h"
#include "../basic/CMessageArray.h"
#include "DXLib/DXLib.h"
#include "DebugLog.h"
#include "SP.h"
#include "Fl2.h"
//#include "2D.h"
#include "ClientConfig.h"
#include "VS_UI_Base.h"
#include "VS_UI_mouse_pointer.h"
#include "UIFunction.h"
#include "AddonDef.h"
#include "MZoneTable.h"
#include "MGameTime.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "UserOption.h"
#include "UserInformation.h"
#include "SpriteIDDef.h"
#include "SpriteLib/MLoadingSPKWorkNode.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"
#include "HPModifyLabel.h"
//#include "MZoneInfo.h"
#include "MObjectSelector.h"
#include "MPortal.h"
#include "EffectSpriteTypeDef.h"
#include "MScreenEffectManager.h"
#include "MEffectSpriteTypeTable.h"
#include "RequestServerPlayerManager.h"
#include "MJusticeAttackManager.h"
#include "Profiler.h"
//#include "MFileDef.h"
#include "Properties.h"
#include "MGuildMarkManager.h"
#include "MEventManager.h"
#include "MAttachOrbitEffect.h"
#include "MItemOptionTable.h"
#include "MMonsterKillQuestInfo.h"
#include "CSprite555.h"
#include "CSprite565.h"
#include "DXLib/COGGSTREAM.h"
#include "FrameClock.h"
#include "SystemAvailabilities.h"
#include "SkillDef.h"
#include "VS_UI_item.h"
#include "NicknameInfo.h"
#include "WorldEventsHost.h"
#include "AdvancementAction.h"

//----------------------------------------------------------------------
// Light rendering path selector.
//
// The original client chose between a D3D texture light path ("3D") and a
// software light path ("2D") with CDirect3D::IsHAL(). The SDL port has no
// D3D device: InitFilters() only prepares the 2D path (Light2D.ftp plus the
// per-pixel width/height tables) and m_pLightBufferTexture is never created,
// so the 3D path dereferences NULL in DrawLightBuffer3D(). Keep every former
// IsHAL() selector on the 2D path.
//----------------------------------------------------------------------
static const bool s_bUse3DLight = false;

#include "MTopViewDraw.inl"
#include "VS_UI_GameCommon2.h"
//#undef OUTPUT_DEBUG

//#define	new			DEBUG_NEW
//#define	delete		DEBUG_DELETE

#if defined(OUTPUT_DEBUG)//defined(_DEBUG) || 
//	#define OUTPUT_DEBUG_DRAW_3DBOX
	#ifdef __METROTECH_TEST__
//		#define OUTPUT_DEBUG_DRAW_PROCESS
//		#define OUTPUT_DEBUG_DRAW_PROCESS_INPUT
	#endif
//	#define OUTPUT_DEBUG_DRAW_SHADOW
#endif

#ifdef __METROTECH_TEST__
	extern bool g_bLight;
#endif

extern HWND							g_hWnd;
extern POINT						g_SelectSector;
extern BOOL							g_bLButtonDown;
extern BOOL							g_bRButtonDown;
extern BOOL							g_bCButtonDown;
extern BOOL					g_bUIInput;
//extern MInput						g_Input;
//extern BOOL							g_WindowsNT;
extern BOOL			g_MyFull;
extern RECT			g_GameRect;
extern int			g_nZoneLarge;
extern int			g_nZoneSmall;
extern bool			g_bZonePlayerInLarge;
//extern HANDLE		g_hFileThread;

extern CMessageArray*	g_pSystemMessage;
extern CMessageArray*	g_pPlayerMessage;
extern CMessageArray*	g_pGameMessage;
extern CMessageArray*	g_pNoticeMessage;

extern MScreenEffectManager*	g_pInventoryEffectManager;

extern bool FileOpenBinary(const char* filename, std::ifstream& file);
//GameNew Mode  add by sonc 2006.9.27
extern	LONG g_SECTOR_WIDTH;
extern	LONG g_SECTOR_HEIGHT;
extern	LONG g_SECTOR_WIDTH_HALF;
extern	LONG g_SECTOR_HEIGHT_HALF;
extern	LONG g_SECTOR_SKIP_PLAYER_LEFT;
extern	LONG g_SECTOR_SKIP_PLAYER_UP;
extern	LONG g_TILESURFACE_SECTOR_WIDTH;
extern	LONG g_TILESURFACE_SECTOR_HEIGHT;
extern	LONG g_TILESURFACE_SECTOR_OUTLINE_RIGHT;
extern	LONG g_TILESURFACE_SECTOR_OUTLINE_DOWN;
extern	LONG g_TILESURFACE_WIDTH;
extern	LONG g_TILESURFACE_HEIGHT;
extern	LONG g_TILESURFACE_OUTLINE_RIGHT;
extern	LONG g_TILESURFACE_OUTLINE_DOWN;
//end 
//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
MTopView*	g_pTopView = NULL;
extern bool g_bFrameChanged;

// Camera-interpolation bias of the last draw (CGameUpdate). The camera the
// draw published (m_PlusPoint / m_FirstZonePixel) carries it; adding it back
// puts the picking functions (ScreenToPixel, GetSelectedSector,
// GetSelectedObject*) in exact tick space, so per-tick hit tests agree with
// per-tick object positions. Zero while the player stands still.
extern int	g_DrawCamGapX;
extern int	g_DrawCamGapY;

bool g_bMouseInPortal  = false;

// Temporary hack.. hehe
POINT g_MouseSector = { 0, 0 };		// Sector coordinate the mouse points at

extern int	g_x;
extern int	g_y;

extern int	g_UpdateDelay;

int g_ShowImageObjectID = 0;

// For Netmarble
//std::map<int, bool> g_mapPremiumZone;

#ifdef OUTPUT_DEBUG
	extern bool g_bSlideScreenShot;
	extern RECT g_rectScreenShot;

	extern int gtx_op;
	extern int gtx_src;
	extern int gtx_dest;

	MString g_ProfileInfoName("DrawZone");
#endif

//----------------------------------------------------------------------
// Determines the screen ratio of one filter coordinate..
//----------------------------------------------------------------------
float MTopView::s_LightWidth	= (float)g_GameRect.right / SCREENLIGHT_WIDTH;
float MTopView::s_LightHeight	= (float)g_GameRect.bottom / SCREENLIGHT_HEIGHT;

//----------------------------------------------------------------------
//
// defines
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Number of picture types for all Creatures
//----------------------------------------------------------------------
#define	MAX_CREATURE_BODY		3
#define	MAX_CREATURE_ADDON		4
#define	MAX_ANIMATION			2
#define	MAX_ALPHAEFFECT			10
#define	MAX_NORMALEFFECT		1

//----------------------------------------------------------------------
// Setting value for Light 3D
//-----------------------------------------------------------------------
#define	MAX_LIGHT_SETSIZE		12

//-----------------------------------------------------------------------
// Shift value for dark output while sniping
//-----------------------------------------------------------------------

//----------------------------------------------------------------------
//
// FONT related
//
//----------------------------------------------------------------------
enum FONTID
{
	FONTID_INFO,	
	FONTID_SLAYER_NAME,
	FONTID_VAMPIRE_NAME,
	FONTID_NPC_NAME,
	FONTID_SLAYER_CHAT,
	FONTID_VAMPIRE_CHAT,
	FONTID_NPC_CHAT,
	FONTID_ITEM,
	FONTID_LARGE_CHAT,
	FONTID_USER_ID,
	MAX_FONTID
};

PrintInfo*	g_ClientPrintInfo[MAX_FONTID];

//----------------------------------------------------------------------
//
//		Text Comparison
//
//----------------------------------------------------------------------
// The older time must be selected.
// Return value for left < right
//
// return false to keep the current order
// return true to swap the order.
//
// The front one is output first.. so the earlier (smaller) time must come first.
// 
// The PQ sends the smaller one to the back..
// Therefore... the later (larger) time must go to the back.
bool 
TextComparison::operator () (DRAWTEXT_NODE * left, DRAWTEXT_NODE * right) const
{ 
	// If it is a character that was set earlier.. OK
	// If the times are equal, it goes behind for now.
	return left->GetTextTime() >= right->GetTextTime();
}

inline		bool			IsAffectFromDarkness()
{
	if( g_pPlayer->IsVampire() ||
		g_pPlayer->HasEffectStatus( EFFECTSTATUS_LIGHTNESS ) ||
		g_pZone->GetID() == 3001 )
		return false;

	return true;
}

//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------
MTopView::MTopView()
{
	m_bInit = false;

	m_pZone		= NULL;

	m_FirstSector.x = 0;
	m_FirstSector.y = 0;
	m_FirstZonePixel.x = 0;	
	m_FirstZonePixel.y = 0;

	m_SelectSector.x = SECTORPOSITION_NULL;
	m_SelectSector.y = SECTORPOSITION_NULL;

	// The selected ones
	m_SelectCreatureID			= OBJECTID_NULL;
	m_SelectItemID				= OBJECTID_NULL;
//	m_SelectInteractionObjectID	= OBJECTID_NULL;
	m_SelectEffectID = OBJECTID_NULL;
	//m_bSelectModeAttack			= false;

	//m_ppSurface			= NULL;

	// Tile Surface
	m_pTileSurface			= NULL;
	m_bFirstTileDraw		= true;
	m_TileSurfaceFirstZonePixelX	= 0;
	m_TileSurfaceFirstZonePixelY	= 0;

	// TileRenderer (Phase 4 integration)
	m_pTileRenderer			= NULL;


	//m_SelectSector.x = 0;
	//m_SelectSector.y = 0;

	// Sight
	m_DarkBits = 0;





	// LightBuffer Texture
	m_pLightBufferTexture = NULL;

//

	m_p2DLightPixelWidth = NULL;
	m_p2DLightPixelHeight = NULL;


	m_bDrawItemNameList = false;

	//
	m_pTileSFPArrayLargeZone = NULL;
	m_pImageObjectSFPArrayLargeZone = NULL;
	m_pTileSFPArraySmallZone = NULL;
	m_pImageObjectSFPArraySmallZone = NULL;

	m_bDrawRequest = false;

	//-----------------------------------------------
	// So that AlphBox can be used before g_pTopView->Init().
	//-----------------------------------------------
//
//		Set3DBoxColor( m_3DBoxCurrentPixel );
//
//

//	m_bTileSearchForCreature = true;
	m_bTileSearchForCreature = false;		

	MapCreatureDrawExceptions( m_ExceptionProcMap );
}


MTopView::~MTopView()
{
	Release();
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Precalculate Advancement Class Creature Frames
//----------------------------------------------------------------------
// Each advanced action's frame count (AdvancementAction.h) from the view's
// frame packs, once they are loaded. Client.cpp's until task 4.260.
//----------------------------------------------------------------------
static void
PrecalculateAdvancementClassCreatureFrames(MTopView& view)
{
	memset( g_AdvanceVampireActionMaxCount, 0, sizeof( g_AdvanceVampireActionMaxCount ) );
	memset( g_AdvanceSlayerActionMaxCount, 0, sizeof( g_AdvanceSlayerActionMaxCount ) );
	memset( g_AdvanceOustersActionMaxCount, 0, sizeof( g_AdvanceOustersActionMaxCount ) );

	int i;


	for( i = ACTION_ADVANCEMENT_SLAYER_STOP_SWORD; i < ACTION_ADVANCEMENT_SLAYER_MAX; ++i )
	{
		int actionindex = i - ADVANCEMENT_ACTION_START;

		char part = 0;

		if( i == ACTION_ADVANCEMENT_SLAYER_BIKE_MOVE )
		{
			g_AdvanceSlayerActionMaxCount[ actionindex ] = 4;
			continue;
		}
			// ||
			//i == ACTION_ADVANCEMENT_SLAYER_BIKE_STOP )
			//part = AC_BIKE_1;

//		try{
		g_AdvanceSlayerActionMaxCount[ actionindex ] =
			static_cast<BYTE>(view.m_AdvancementSlayerManFPK[ part ][ actionindex ][2].GetSize());
//		}catch(...){} //add by viva
	}


	for( i = ACTION_ADVANCEMENT_STOP; i < ACTION_ADVANCEMENT_MAX; ++i )
	{
		int actionindex = i - ADVANCEMENT_ACTION_START;
//		try{
		g_AdvanceVampireActionMaxCount[ actionindex ] =
			static_cast<BYTE>(view.m_AdvancementVampireManFPK[0][ actionindex ][2].GetSize());
//		}catch(...){} //add by viva
	}

	for( i = ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_STOP; i < ACTION_ADVANCEMENT_OUSTERS_MAX; ++i )
	{
		int actionindex = i - ADVANCEMENT_ACTION_START;
//		try{
		g_AdvanceOustersActionMaxCount[ actionindex ] =
			static_cast<BYTE>(view.m_AdvancementOustersFPK[1][ actionindex ][2].GetSize());
//		}catch(...){} //add by viva
	}
}

//----------------------------------------------------------------------
// Init
//----------------------------------------------------------------------
// Initializes each part the View needs.
//----------------------------------------------------------------------
bool
MTopView::Init()
{
	//------------------------------------------
	// If already init'ed, do not init again.
	//------------------------------------------
	if (m_bInit)
	{
		return false;
	}

	//------------------------------------------
	// Texture Memory calculation
	//------------------------------------------
	// SDL2: Unified path - use default values for all platforms
	// (DirectDraw video memory query no longer available)
	int num = 30;  // Default number of effect textures
	DEBUG_ADD("[TextureMemory] Using default value for SDL2");

	//num = 20;

	// Reset.. - -;
	//g_pClientConfig->MAX_TEXTUREPART_EFFECT				= num*3/2;
	//g_pClientConfig->MAX_TEXTUREPART_CREATURESHADOW		= num*3;
	//g_pClientConfig->MAX_TEXTUREPART_IMAGEOBJECTSHADOW	= num;
	//g_pClientConfig->MAX_TEXTUREPART_ADDON_SHADOW			= 240 + num*7;

	//------------------------------------------------------------------------
	// Calculation for when character shadows are always drawn black
	//------------------------------------------------------------------------
	g_pClientConfig->MAX_TEXTUREPART_EFFECT				= num*2;	//num*10;
	g_pClientConfig->MAX_TEXTUREPART_SCREENEFFECT		= num*5;		//num*10;
	g_pClientConfig->MAX_TEXTUREPART_CREATURESHADOW		= 1;//num*3;
	g_pClientConfig->MAX_TEXTUREPART_IMAGEOBJECTSHADOW	= num*2;
	g_pClientConfig->MAX_TEXTUREPART_ADDON_SHADOW		= 1;//240 + num*7;

	LOG_INFO("[MTopView::TexturePart] AlphaEffect = %d", g_pClientConfig->MAX_TEXTUREPART_EFFECT);
	LOG_INFO("[MTopView::TexturePart] ScreenEffect = %d", g_pClientConfig->MAX_TEXTUREPART_SCREENEFFECT);
	LOG_INFO("[MTopView::TexturePart] CreatureShadow = %d", g_pClientConfig->MAX_TEXTUREPART_CREATURESHADOW);
	LOG_INFO("[MTopView::TexturePart] ImageObjectShadow = %d", g_pClientConfig->MAX_TEXTUREPART_IMAGEOBJECTSHADOW);
	LOG_INFO("[MTopView::TexturePart] AddonShadow = %d", g_pClientConfig->MAX_TEXTUREPART_ADDON_SHADOW);	

	if (InitSurfaces() &&
		InitColors() &&
//		Init3DBoxSurface() &&
		InitSprites() &&
		InitFilters() &&
		InitCreatureFrames() &&
		InitImageFrames() &&
		InitAnimationFrames() &&
		InitEffectFrames() &&
		InitFonts())
	{
		//------------------------------------------
		// Calculates the appropriate Texture Memory
		//------------------------------------------
		#ifdef OUTPUT_DEBUG
			DDSCAPS2 ddsCaps2;
			DWORD dwTotal;
			DWORD dwFree;
			ZeroMemory(&ddsCaps2, sizeof(ddsCaps2)); 
			ddsCaps2.dwCaps = DDSCAPS_TEXTURE; 
			HRESULT hr = CSDLGraphics::GetDD()->GetAvailableVidMem(&ddsCaps2, &dwTotal, &dwFree);

			DEBUG_ADD_FORMAT("[TextureMemory] After Init View = %d/%d", dwFree, dwTotal);			
		#endif

		m_bInit = true;

		PrecalculateAdvancementClassCreatureFrames(*this);

		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Init Changes
//----------------------------------------------------------------------
// Used when switching 2D <--> 3D..
//----------------------------------------------------------------------
bool
MTopView::InitChanges()
{
	//------------------------------------------
	// If not init'ed yet, return
	//------------------------------------------
	if (!m_bInit)
	{
		return false;
	}

	if (InitSurfaces() &&
		InitColors() &&
//		Init3DBoxSurface() &&
		InitSprites() &&
		InitFilters() &&
		InitFonts())
	{
		//---------------------------------------------------------
		// Marks that no character has been loaded.
		// It is because of the shadows.. yum yum.. - -; 2001.10.6
		//---------------------------------------------------------
		if (g_pCreatureSpriteTable!=NULL)
		{
			int numSize = g_pCreatureSpriteTable->GetSize();

			for (int i=0; i<numSize; i++)
			{
				if (auto* entry = g_pCreatureSpriteTable->GetMutable(i)) {
					entry->bLoad = false;
				}
			}
		}

//
//			for (int i=0; i<numSize; i++)
//			{
//				(*g_pAddonSpriteTable)[i].bLoad = false;
//			}
//		}

//
//			for (int i=0; i<numSize; i++)
//			{
//				(*g_pCreatureActionSpriteTable)[i].bLoad = false;
//			}
//		}

		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Init Fonts
//----------------------------------------------------------------------
bool
MTopView::InitFonts()
{	
	//------------------------------------------
	// Register the UI Font
	//------------------------------------------	
	g_ClientPrintInfo[FONTID_INFO]			= &gpC_base->m_info_pi;			// info
	g_ClientPrintInfo[FONTID_SLAYER_NAME]	= &gpC_base->m_char_name_pi;	// slayer
	g_ClientPrintInfo[FONTID_VAMPIRE_NAME]	= &gpC_base->m_char_name_pi;	// vampire
	g_ClientPrintInfo[FONTID_NPC_NAME]		= &gpC_base->m_char_name_pi;	// npc
	g_ClientPrintInfo[FONTID_SLAYER_CHAT]	= &gpC_base->m_char_chat_pi;	// slayer
	g_ClientPrintInfo[FONTID_VAMPIRE_CHAT]	= &gpC_base->m_char_chat_pi;	// vampire
	g_ClientPrintInfo[FONTID_NPC_CHAT]		= &gpC_base->m_char_chat_pi;	// npc
	g_ClientPrintInfo[FONTID_ITEM]			= &gpC_base->m_item_pi;			// item
	g_ClientPrintInfo[FONTID_LARGE_CHAT]	= &gpC_base->m_char_chat_large_pi;
	g_ClientPrintInfo[FONTID_USER_ID]		= &gpC_base->m_user_id_pi;
	return true;
}

//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
MTopView::Release()
{
	//------------------------------------------------
	// Close the File
	//------------------------------------------------
//
//
//
//
//
//
//
//
	DEBUG_ADD("MTV-Rel-File closed");

	//----------------------------------------------------------------------
	// Remove the Tile Surface
	//----------------------------------------------------------------------
	if (m_pTileSurface != NULL)
	{
		delete m_pTileSurface;
		m_pTileSurface = NULL;

		DEBUG_ADD("MTV-Rel-TileSur");
	}

	//----------------------------------------------------------------------
	// Remove the TileRenderer (Phase 4 integration)
	//----------------------------------------------------------------------
	if (m_pTileRenderer != NULL)
	{
		delete m_pTileRenderer;
		m_pTileRenderer = NULL;

		DEBUG_ADD("MTV-Rel-TileRenderer");
	}


	DEBUG_ADD("MTV-Rel-StartRelTex");

	//----------------------------------------------------------------------
	// Texture Manager
	//----------------------------------------------------------------------
//
//
//	//----------------------------------------------------------------------
//	// Shadow Manager
//	//----------------------------------------------------------------------

//
//
//	

	//----------------------------------------------------------------------
	// LightBuffer Texture
	//----------------------------------------------------------------------
	if (m_pLightBufferTexture!=NULL)
	{
		delete m_pLightBufferTexture;
		m_pLightBufferTexture = NULL;
		DEBUG_ADD("MTV-Rel-LightBuffer");
	}	

	//----------------------------------------------------------------------
	// 3D Box Texture Surface
	//----------------------------------------------------------------------
//
//
//	
//	//----------------------------------------------------------------------
//	// Minimap
//	//----------------------------------------------------------------------
//


	//----------------------------------------------------------------------
	// 2D Light Pixel
	//----------------------------------------------------------------------
	if (m_p2DLightPixelWidth!=NULL)
	{
		delete [] m_p2DLightPixelWidth;
		m_p2DLightPixelWidth = NULL;
		DEBUG_ADD("MTV-Rel-2DLightPixelW");
	}

	if (m_p2DLightPixelHeight!=NULL)
	{
		delete [] m_p2DLightPixelHeight;
		m_p2DLightPixelHeight = NULL;
		DEBUG_ADD("MTV-Rel-2DLightPixelH");
	}

	DEBUG_ADD("MTV-Rel-ClearList");

	//----------------------------------------------------------------------
	// Remove list , map
	//----------------------------------------------------------------------
	ClearItemNameList();
	ClearOutputCreature();	
	ClearTextList();

	DEBUG_ADD("MTV-Rel-Loaded");

	m_listLoadedCreatureSprite.Release();
	m_listLoadedMonsterSprite.Release();
	m_mapImageObject.clear();

	//----------------------------------------------------------------------
	// Delete the FPKs
	//----------------------------------------------------------------------
	DEBUG_ADD("MTV-Rel-FPK");

	m_CreatureFPK.Release();			// Creature frames
	m_AddonFPK.Release();				// Player addon frames
	m_OustersFPK.Release();				// Player addon frames
	m_ItemTileFPK.Release();			// For Items on the Tile..
	m_ItemDropFPK.Release();			// For Items falling to the ground..
	m_ImageObjectFPK.Release();		// Frames for ImageObject
	m_ImageObjectShadowFPK.Release();

	// Delete the promotion-related fpk
	m_AdvancementSlayerManFPK.Release();
	m_AdvancementSlayerWomanFPK.Release();
	m_AdvancementVampireManFPK.Release();
	m_AdvancementVampireWomanFPK.Release();
	m_AdvancementOustersFPK.Release();
	m_AdvancementSlayerManShadowFPK.Release();
	m_AdvancementSlayerWomanShadowFPK.Release();
	m_AdvancementVampireManShadowFPK.Release();
	m_AdvancementVampireWomanShadowFPK.Release();
	m_AdvancementOustersShadowFPK.Release();
//	m_InteractionObjectFPK.Release();		// Frames for ImageObject

	DEBUG_ADD("MTV-Rel-EffectFPK");

	m_EffectAlphaFPK.Release();		// Frames for Effect
	m_EffectScreenFPK.Release();
	m_EffectShadowFPK.Release();		// Frames for Effect
	m_EffectNormalFPK.Release();		// Frames for Effect

	DEBUG_ADD("MTV-Rel-CFPK");

	m_CreatureShadowFPK.Release();			// Creature frames
	m_AddonShadowFPK.Release();			// Player addon frames	
	m_OustersShadowFPK.Release();			// Player addon frames	

	//------------------------------------------------------
	// Remove the SpritePacks
	//------------------------------------------------------		
	DEBUG_ADD("MTV-Rel-SPK");

	m_TileSPK.Release();				// Tile
	m_ImageObjectSPK.Release();		// ImageObject Sprites
//	m_InteractionObjectSPK.Release();		// ImageObject Sprites
	m_CreatureSPK.Release();			// Creature Sprites
	m_AddonSPK.Release();		// Sprites
	m_OustersSPK.Release();

	DEBUG_ADD("MTV-Rel-ItemSPK");

	m_ItemTileISPK.Release();			// Sprites
	m_ItemDropISPK.Release();			//
	m_ItemBrokenSPK.Release();			//
	m_ItemRealSPK.Release();			// Sprites

	DEBUG_ADD("MTV-Rel-EffectSPk");
	m_EffectAlphaSPK.Release();		// AlphaSprites
	m_EffectScreenSPK.Release();		// AlphaSprites
	m_EffectShadowSPK.Release();		// ShadowSprites
	m_EffectNormalSPK.Release();		// NormalSprites
	m_EffectScreenPPK.Release();
	m_EffectAlphaPPK.Release();

	DEBUG_ADD("MTV-Rel-Other");

	m_WeatherSPK.Release();			// WeatherSprites
	m_GuildSPK.Release();

	m_EtcSPK.Release();				// Etc...
	m_OustersFinSPK.Release();
	m_AdvacementQuestEnding.Release();

	// Remove the promotion-related SPK
	m_AdvancementSlayerManSPK.Release();
	m_AdvancementSlayerWomanSPK.Release();
	m_AdvancementVampireManSPK.Release();
	m_AdvancementVampireWomanSPK.Release();
	m_AdvancementOustersSPK.Release();

	m_AdvancementSlayerManSSPK.Release();
	m_AdvancementSlayerWomanSSPK.Release();
	m_AdvancementVampireManSSPK.Release();
	m_AdvancementVampireWomanSSPK.Release();
	m_AdvancementOustersSSPK.Release();

	//------------------------------------------------------		
	// Shadow SpritePack
	//------------------------------------------------------
	DEBUG_ADD("MTV-Rel-SSPK");

	m_AddonSSPK.Release();			// Sprites
	m_OustersSSPK.Release();			// Sprites
	m_ImageObjectSSPK.Release();
//	m_InteractionObjectSSPK.Release();
	m_CreatureSSPK.Release();

	//----------------------------------------------------------------------
	// Delete the File Index Table
	//----------------------------------------------------------------------
	DEBUG_ADD("MTV-Rel-SSPKI");

	m_TileSPKI.Release();
	m_ImageObjectSPKI.Release();
//	m_EffectAlphaSPKI.Release();
//	m_EffectScreenSPKI.Release();

	//----------------------------------------------------------------------
	// Filter
	//----------------------------------------------------------------------
	DEBUG_ADD("MTV-Rel-Filter");

	m_ImageObjectFilter.Release();
	m_LightFTP.Release();
	m_LightBufferFilter.Release();;

	//----------------------------------------------------------------------
	// Remove the SFP Array
	//----------------------------------------------------------------------
	if (m_pTileSFPArrayLargeZone != NULL)
	{
		delete m_pTileSFPArrayLargeZone;
		m_pTileSFPArrayLargeZone = NULL;
		DEBUG_ADD("MTV-Rel-TileSFPAL");
	}

	if (m_pImageObjectSFPArrayLargeZone != NULL)
	{
		delete m_pImageObjectSFPArrayLargeZone;
		m_pImageObjectSFPArrayLargeZone = NULL;
		DEBUG_ADD("MTV-Rel-IMSFPAL");
	}

	if (m_pTileSFPArraySmallZone != NULL)
	{
		delete m_pTileSFPArraySmallZone;
		m_pTileSFPArraySmallZone = NULL;
		DEBUG_ADD("MTV-Rel-TileSFPS");
	}

	if (m_pImageObjectSFPArraySmallZone != NULL)
	{
		delete m_pImageObjectSFPArraySmallZone;
		m_pImageObjectSFPArraySmallZone = NULL;
		DEBUG_ADD("MTV-Rel-IMSFPS");
	}

	m_bInit = false;

	DEBUG_ADD("MTV-Rel-All OK");
}

//----------------------------------------------------------------------
// Decides the Surface that is the output target.
//----------------------------------------------------------------------
void	
MTopView::SetSurface(CSpriteSurface*& pSurface)
{
	m_pSurface = pSurface;
}

//----------------------------------------------------------------------
// Clear ShadowManager
//----------------------------------------------------------------------
void
MTopView::ClearShadowManager()
{
	// SDL2: No DirectDraw cleanup needed
}

//----------------------------------------------------------------------
// Set 3D Box Color
//----------------------------------------------------------------------
//
//	if (m_p3DBoxSurface->Lock())
//	{	
//		WORD *lpSurface = (WORD*)m_p3DBoxSurface->GetSurfacePointer();
//		WORD *lpSurfaceTemp = lpSurface;
//		*lpSurfaceTemp = pixel;
//		m_p3DBoxSurface->Unlock();
//		
//		m_3DBoxCurrentPixel = pixel;
//
//
////----------------------------------------------------------------------
//// Init 3D BoxSurface
////----------------------------------------------------------------------
//	//-----------------------------------------------
//	// 3D Box Texture Surface
//	//-----------------------------------------------
//	if (true)
//	{	
//		//-----------------------------------------------
//		// Black
//		//-----------------------------------------------
//				WORD *lpSurface = (WORD*)m_p3DBoxBlackSurface->GetSurfacePointer();
//				*lpSurface = m_ColorBlackHalf;
//
//		//-----------------------------------------------
//		// HP color
//		//-----------------------------------------------
//				WORD *lpSurface = (WORD*)m_p3DBoxHPSurface->GetSurfacePointer();
//				*lpSurface = m_ColorHPBar;
//
//	return true;
//}

//----------------------------------------------------------------------
// Restore Surface
//----------------------------------------------------------------------
void
MTopView::RestoreSurface()
{
	m_bDrawRequest = false;

	//for (int i=0; i<4; i++)
	{
		CSDLGraphics::RestoreAllSurfaces();

	// SDL2: Direct3D restore no longer needed

	//	CSDLGraphics::Flip();
	}

	if (m_pSurface!=NULL)
	{
		m_pSurface->Restore();
	}

	if (m_pTileSurface!=NULL)
	{
		m_pTileSurface->Restore();
	}

	// All textures redrawn

	ClearOutputCreature();

	ClearItemNameList();

	//------------------------------------------------------------
	// When using acceleration, textures need to be restored
	// because they reside in video memory.
	//------------------------------------------------------------
	// SDL2: InitFilters is called for all platforms
	DEBUG_ADD("Restore Surfaces - m_pLightBufferTexture");
	InitFilters();

	//------------------------------------------------------------
	// minimap texture initialization
	//------------------------------------------------------------
	/*
	if (true)
	{
		//------------------------------------------------------------
		//
		// Load  EffectPack
		//
		//------------------------------------------------------------
		std::ifstream	effectFile2(FILE_ASPRITE_ALPHAEFFECT, ios::binary);
		m_EffectAlphaSPK.LoadFromFile(effectFile2);
		effectFile2.close();	

		//------------------------------------------------------------
		//
		// Init Effect TextureSurface
		//
		//------------------------------------------------------------		
		//--------------------------------------------
		// Creates the TexturePack from the AlphaSPK.
		//--------------------------------------------
		if (!m_EffectTPK.Init( m_EffectAlphaSPK ))
		{
			MessageBox(g_hWnd, "Can't Init EffectTexture!", NULL, MB_OK);
			return;
		}

		//------------------------------------------------------------	
		// Deletes the EffectAlphaSprite from memory.
		// --> Since the TextureSurface will be used.. it is not needed.
		//------------------------------------------------------------		
		m_EffectAlphaSPK.Release();	
	}
	*/
}


//----------------------------------------------------------------------
// Initializes the Surfaces to be used.
//----------------------------------------------------------------------
bool
MTopView::InitSurfaces()
{
	// The renderer borrows this surface. A rejected pack must not leave it
	// pointing at the surface being replaced, and retries must not leak it.
	delete m_pTileRenderer;
	m_pTileRenderer = nullptr;

	//-----------------------------------------------
	// Tile Surface
	//-----------------------------------------------
	if (m_pTileSurface!=NULL)
	{
		delete m_pTileSurface;
	}

	m_pTileSurface = new CSpriteSurface;



	//
	// If 3D acceleration is available, in VideoMemory.. (if possible. - -;)
	//
	// If not.. in system memory...
	// (Why? Hmm.. I suspect there is related code somewhere but I can't be bothered to look. Hmm)
	//
	//----------------------------------------------------------------
	// 3D
	//----------------------------------------------------------------
	//-----------------------------------------------
	//
	//-----------------------------------------------
//
//	//-----------------------------------------------
//	//
//	//-----------------------------------------------
//
//
//	//-----------------------------------------------
//	//
//	//-----------------------------------------------

	// SDL2: Unified path - always use system memory for tile surface
	DEBUG_ADD("[ InitGame ]  MTopView::InitSurface() - TileSurface sysmem");

	m_pTileSurface->InitOffsurface(g_TILESURFACE_WIDTH,
									g_TILESURFACE_HEIGHT);
	// A render reset redraws the whole cache (SetFirstDraw). Reading it back
	// for device recovery stalled the GPU on every scroll while walking.
	m_pTileSurface->SetTransient(true);

	m_pTileSurface->SetTransparency( 0 );

	//----------------------------------------------------------------------
	// Note: TileRenderer initialization moved to InitSprites()
	// to fix initialization order issue (InitSprites is called after InitSurfaces)
	//----------------------------------------------------------------------



	UI_DrawProgress(1);
	WorldEvents::DrawTitleLoading();

	return true;
}

//----------------------------------------------------------------------
// Init Colors
//----------------------------------------------------------------------
// Initializes the colors.
//----------------------------------------------------------------------
bool
MTopView::InitColors()
{
	//---------------------------------------------------
	// Always the same color
	//---------------------------------------------------
	// item
	m_ColorNameItem					= g_pClientConfig->COLOR_NAME_ITEM;
	m_ColorNameItemOption			= g_pClientConfig->COLOR_NAME_ITEM_OPTION;

	// Per race
	m_ColorNameVampire				= g_pClientConfig->COLOR_NAME_VAMPIRE;
	m_ColorNameSlayer				= g_pClientConfig->COLOR_NAME_SLAYER;
	m_ColorNameNPC					= g_pClientConfig->COLOR_NAME_NPC;

	m_ColorNameAlignment[0]			= g_pClientConfig->COLOR_NAME_EVIL_MORE;
	m_ColorNameAlignment[1]			= g_pClientConfig->COLOR_NAME_EVIL;
	m_ColorNameAlignment[2]			= g_pClientConfig->COLOR_NAME_NEUTRAL;
	m_ColorNameAlignment[3]			= g_pClientConfig->COLOR_NAME_GOOD;
	m_ColorNameAlignment[4]			= g_pClientConfig->COLOR_NAME_GOOD_MORE;	


	//---------------------------------------------------
	// In the 5:6:5 case the value can be read directly.
	//---------------------------------------------------
	if (CSDLGraphics::Is565())
	{
		// interactionObject
//		m_ColorOutlineInteractionObject	= g_pClientConfig->COLOR_OUTLINE_INTERACTIONOBJECT;

		m_ColorOutlineItem				= g_pClientConfig->COLOR_OUTLINE_ITEM;

		// Can attack?
		m_ColorOutlineNPC				= g_pClientConfig->COLOR_OUTLINE_NPC;
		m_ColorOutlineAttackPossible	= g_pClientConfig->COLOR_OUTLINE_ATTACK_POSSIBLE;
		m_ColorOutlineAttackImpossible	= g_pClientConfig->COLOR_OUTLINE_ATTACK_IMPOSSIBLE;	
	}
	//---------------------------------------------------
	// In the 5:5:5 case the value is converted
	//---------------------------------------------------
	else
	{
		// interactionObject
//		m_ColorOutlineInteractionObject	= CSDLGraphics::Convert565to555(g_pClientConfig->COLOR_OUTLINE_INTERACTIONOBJECT);

		// item
		m_ColorOutlineItem				= CSDLGraphics::Convert565to555(g_pClientConfig->COLOR_OUTLINE_ITEM);

		// Can attack?
		m_ColorOutlineNPC				= CSDLGraphics::Convert565to555(g_pClientConfig->COLOR_OUTLINE_NPC);
		m_ColorOutlineAttackPossible	= CSDLGraphics::Convert565to555(g_pClientConfig->COLOR_OUTLINE_ATTACK_POSSIBLE);
		m_ColorOutlineAttackImpossible	= CSDLGraphics::Convert565to555(g_pClientConfig->COLOR_OUTLINE_ATTACK_IMPOSSIBLE);
	}


	//---------------------------------------------------
	// Colors related to 3D acceleration...
	//---------------------------------------------------
//
	{
		m_ColorHPBar = CSDLGraphics::Color(g_pClientConfig->COLOR_HP_BAR_R,
											g_pClientConfig->COLOR_HP_BAR_G,
											g_pClientConfig->COLOR_HP_BAR_B);

		m_ColorHPBarBg = CSDLGraphics::Color(g_pClientConfig->COLOR_HP_BAR_BG_R,
											 g_pClientConfig->COLOR_HP_BAR_BG_G,
											 g_pClientConfig->COLOR_HP_BAR_BG_B);

		// UI box background color: G of RGB565 has 6 bits, so it needs doubling
		// Dark gray: R=12, G=24, B=12
		m_ColorUIBoxBg = CSDLGraphics::Color(12, 24, 12);

		m_ColorBlackHalf = 0;
	}

	return true;
}


//----------------------------------------------------------------------
// Reads the SpritePack and Loads it into memory.
//----------------------------------------------------------------------
bool
MTopView::InitSprites()
{
	// Sprite
	/*
	WORD		spriteID = 0;

	WORD	*lpSurface, 
			*lpSurfaceTemp,
			*lpSurfaceTemp2;

	WORD	lPitch;

	int i,j;

	// Temp Surface	
	CSpriteSurface TempSurface;
	*/



	/*
	m_ImageObjectSSPK.Init( 42 );

	CShadowSprite::SetColorkey( 0x001F );
	//------------------------------------------------------------
	// Shadow
	//------------------------------------------------------------
	TempSurface.InitFromBMP("BuildingShadow0.bmp", 1024,768, DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	m_ImageObjectSSPK[ 0 ].SetPixel(lpSurface, lPitch, 874, 360);
	TempSurface.Unlock();

	TempSurface.InitFromBMP("BuildingShadow1.bmp", 1024,768, DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	m_ImageObjectSSPK[ 1 ].SetPixel(lpSurface, lPitch, 906, 494);
	TempSurface.Unlock();

	TempSurface.InitFromBMP("BuildingShadow2.bmp", 1024,768, DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	m_ImageObjectSSPK[ 2 ].SetPixel(lpSurface, lPitch, 928, 436);
	TempSurface.Unlock();

	TempSurface.InitFromBMP("BuildingShadow3.bmp", 1024,768, DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	m_ImageObjectSSPK[ 3 ].SetPixel(lpSurface, lPitch, 804, 502);	

	for (i=4; i<=31; i++)
	{
		m_ImageObjectSSPK[ i ].SetPixel(lpSurface, lPitch, 804, 502);	
	}

	TempSurface.Unlock();

	TempSurface.InitFromBMP("ImageObjectSomeShadow.bmp", 800,600, DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	lpSurfaceTemp = lpSurface;
	m_ImageObjectSSPK[ 32 ].SetPixel(lpSurfaceTemp, lPitch, 87, 82);
	lpSurfaceTemp = lpSurface + 89;
	m_ImageObjectSSPK[ 33 ].SetPixel(lpSurfaceTemp, lPitch, 47, 50);
	lpSurfaceTemp = lpSurface + 157;
	m_ImageObjectSSPK[ 34 ].SetPixel(lpSurfaceTemp, lPitch, 58, 50);
	m_ImageObjectSSPK[ 35 ].SetPixel(lpSurfaceTemp, lPitch, 58, 50);

	lpSurfaceTemp = lpSurface + 223;
	m_ImageObjectSSPK[ 36 ].SetPixel(lpSurfaceTemp, lPitch, 49, 50);	
	m_ImageObjectSSPK[ 38 ].SetPixel(lpSurfaceTemp, lPitch, 49, 50);
	m_ImageObjectSSPK[ 40 ].SetPixel(lpSurfaceTemp, lPitch, 49, 50);

	lpSurfaceTemp = lpSurface + 286;
	m_ImageObjectSSPK[ 37 ].SetPixel(lpSurfaceTemp, lPitch, 51, 41);
	m_ImageObjectSSPK[ 39 ].SetPixel(lpSurfaceTemp, lPitch, 51, 41);	
	m_ImageObjectSSPK[ 41 ].SetPixel(lpSurfaceTemp, lPitch, 51, 41);
	TempSurface.Unlock();


	//------------------------------------------------------------	
	// Save  ImageObject Shadow SpritePack	
	//------------------------------------------------------------
	std::ofstream	ImageObjectSFile(FILE_SSPRITE_IMAGEOBJECT, ios::binary);	
	std::ofstream	ImageObjectSIndexFile(FILE_SSPRITEINDEX_IMAGEOBJECT, ios::binary);	

	m_ImageObjectSSPK.SaveToFile(ImageObjectSFile, ImageObjectSIndexFile);

	ImageObjectSFile.close();
	ImageObjectSIndexFile.close();
	*/

	//------------------------------------------------------------	
	// Load  ImageObject ShadowSpritePack	
	//------------------------------------------------------------
	//------------------------------------------------------------
	//
	// Load  EffectPack
	//
	//------------------------------------------------------------
	// If 3D acceleration is available, use m_pImageObjectShadowManager,
	// otherwise use m_ImageObjectSSPK.
	/*
	std::ifstream	ImageObjectShadowFile2;//(FILE_SSPRITE_IMAGEOBJECT, ios::binary);
	if (!FileOpenBinary(FILE_SSPRITE_IMAGEOBJECT, ImageObjectShadowFile2))
		return false;
	m_ImageObjectSSPK.LoadFromFile(ImageObjectShadowFile2);
	ImageObjectShadowFile2.close();
	*/
//
//	//m_ImageObjectSSPK.Release();
//
	{
		if (!m_ImageObjectSSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_SSPRITE_IMAGEOBJECT").c_str())) return false;
// 		ImageObjectShadowFile2.close();
	}

//	UI_DrawProgress(2);
//	DrawTitleLoading();

	//------------------------------------------------------------
	//
	//
	//                  Creature SpritePack
	//
	//
	//------------------------------------------------------------	
	//------------------------------------------------------------	
	// Init  Creature SpritePack	
	//------------------------------------------------------------
	// For now, reserve memory for the whole size.
	//------------------------------------------------------------
	//  Init Creature SpriteSet
	//------------------------------------------------------------	
	if (m_CreatureSPK.GetSize()==0)
	{
		/*
		WORD size;
		std::ifstream CreaturePackIndexFile;//(FILE_ISPRITEINDEX_CREATURE, ios::binary);
		if (!FileOpenBinary(FILE_ISPRITEINDEX_CREATURE, CreaturePackIndexFile))
			return false;
		CreaturePackIndexFile.read((char*)&size, 2);	// Number of Sprites
		CreaturePackIndexFile.close();	

		m_CreatureSPK.Init( size, CSDLGraphics::Is565() );
		*/
//		
//		// Only open the file.
//		//------------------------------------------------------------
//		// sprite load
//		//------------------------------------------------------------
//		if (!FileOpenBinary(FILE_ISPRITE_CREATURE, m_CreatureSPKFile))
//			return false;	
//
		if (!m_CreatureSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_ISPRITE_CREATURE").c_str() )) return false;

	}

//	UI_DrawProgress(3);
//	DrawTitleLoading();

	//m_CreatureSSPK.Init( size );
	//m_CreatureTPK.Init( size );

	//------------------------------------------------------------	
	//
	//			Creature Shadow SpritePack	
	//
	//------------------------------------------------------------	
	/*
	m_CreatureSSPK.Init( 1 );

	CShadowSprite::SetColorkey( 0x001F );
	//------------------------------------------------------------
	// Shadow
	//------------------------------------------------------------
	TempSurface.InitFromBMP("CreatureShadow.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	m_CreatureSSPK[ 0 ].SetPixel(lpSurface, lPitch, TempSurface.GetWidth(), TempSurface.GetHeight());
	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Save  Creature SpritePack	
	//------------------------------------------------------------
	std::ofstream	creatureShadowFile(FILE_SSPRITE_CREATURE, ios::binary);	
	std::ofstream	creatureShadowIndexFile(FILE_SSPRITEINDEX_CREATURE, ios::binary);
	m_CreatureSSPK.SaveToFile(creatureShadowFile, creatureShadowIndexFile);
	creatureShadowFile.close();
	creatureShadowIndexFile.close();
	*/

	//------------------------------------------------------------
	// Test: normal SpritePack --> ShadowSpritePack
	//------------------------------------------------------------
	// Male
	/*
	CSpritePack CreatureShadowSPK;
	std::ifstream	CreatureShadowFile2;//(FILE_ISPRITE_ADDON, ios::binary);
	if (!FileOpenBinary("Data\\Image\\CreatureShadow.spk", CreatureShadowFile2))
		return false;
	CreatureShadowSPK.LoadFromFile(CreatureShadowFile2);
	CreatureShadowFile2.close();

	m_CreatureSSPK.Init( CreatureShadowSPK.GetSize() );

	for (int i=0; i<CreatureShadowSPK.GetSize(); i++)
	{
		m_CreatureSSPK[i].SetPixel( CreatureShadowSPK[i] );
	}

	//------------------------------------------------------------	
	// Male - Save ShadowSpritePack
	//------------------------------------------------------------
	std::ofstream	CreatureShadowFile(FILE_SSPRITE_CREATURE, ios::binary);	
	std::ofstream	CreatureShadowIndexFile(FILE_SSPRITEINDEX_CREATURE, ios::binary);	

	m_CreatureSSPK.SaveToFile(CreatureShadowFile, CreatureShadowIndexFile);

	CreatureShadowFile.close();
	CreatureShadowIndexFile.close();
	*/

	//------------------------------------------------------------
	// Load Creature Shadow SpritePack
	//------------------------------------------------------------
	// If 3D acceleration is available, use m_pCreatureShadowManager,
	// otherwise use m_CreatureSSPK.
	//------------------------------------------------------------
	// If 3D acceleration is available, use m_pAlphaEffectTextureManager,
	// otherwise use m_EffectAlphaSPK.

	// Only reserve the total count.
	/*
	std::ifstream CreatureShadowPackIndexFile;//(FILE_ISPRITEINDEX_CREATURE, ios::binary);
	if (!FileOpenBinary(FILE_SSPRITEINDEX_CREATURE, CreatureShadowPackIndexFile))
		return false;
	CreatureShadowPackIndexFile.read((char*)&size, 2);	// Number of Sprites
	CreatureShadowPackIndexFile.close();	

	m_CreatureSSPK.Init( size );
	*/
	/*
	// Temporary Load
	// Part that loads everything
	std::ifstream	CreatureShadowFile2;//(FILE_SSPRITE_Creature, ios::binary);
	if (!FileOpenBinary(FILE_SSPRITE_CREATURE, CreatureShadowFile2))
		return false;
	m_CreatureSSPK.LoadFromFile(CreatureShadowFile2);
	CreatureShadowFile2.close();
	*/
//
//	
	{
		/*
		std::ifstream	CreatureShadowFile2;//(FILE_SSPRITE_Creature, ios::binary);
		if (!FileOpenBinary(FILE_SSPRITE_CREATURE, CreatureShadowFile2))
			return false;
		m_CreatureSSPK.LoadFromFile(CreatureShadowFile2);
		CreatureShadowFile2.close();
		*/
		/*
		std::ifstream indexFile;//(indexFilename, ios::binary);
		if (!FileOpenBinary(FILE_SSPRITEINDEX_CREATURE, indexFile))
			return false;

		int packSize;
		indexFile.read((char*)&packSize, 2);
		indexFile.close();

		m_CreatureSSPK.Init( packSize );
		*/
		//------------------------------------------------------------
		// Load shadow
		//------------------------------------------------------------
//		if (!FileOpenBinary(FILE_SSPRITE_CREATURE, m_CreatureSSPKFile))
//			return false;
//
//		TYPE_SPRITEID	numCreatureSSPK;
//
//		m_CreatureSSPKFile.read((char*)&numCreatureSSPK, SIZE_SPRITEID);
// 		m_CreatureSSPK.Init( numCreatureSSPK );			
		if (!m_CreatureSSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_SSPRITE_CREATURE").c_str())) return false;
	}

//	UI_DrawProgress(4);
//	DrawTitleLoading();

	//------------------------------------------------------------
	//
	//
	//                   Clothes SpritePack
	//
	//
	//------------------------------------------------------------
	/*
	CSpritePack	ClothesSPK;
	ClothesSPK.Init( 160, CSDLGraphics::Is565() );

	//------------------------------------------------------------
	// Shirts
	//------------------------------------------------------------
	TempSurface.InitFromBMP("Shirts.bmp", 640,480, DDSCAPS_SYSTEMMEMORY);
	TempSurface.LockW(lpSurface, lPitch);

	spriteID = 0;
	for (i=0; i<4; i++)
	{
		lpSurfaceTemp = lpSurface;
		for (j=0; j<10; j++)
		{
			ClothesSPK[spriteID++].SetPixel(lpSurfaceTemp, lPitch, 64, 64);

			lpSurfaceTemp += 64;
		}
		lpSurface = (WORD*)((BYTE*)lpSurface + lPitch*64);
	}
	TempSurface.Unlock();

	//------------------------------------------------------------
	// Pants
	//------------------------------------------------------------
	TempSurface.InitFromBMP("Pants.bmp", 640,480, DDSCAPS_SYSTEMMEMORY);
	TempSurface.LockW(lpSurface, lPitch);

	for (i=0; i<4; i++)
	{
		lpSurfaceTemp = lpSurface;
		for (j=0; j<10; j++)
		{
			ClothesSPK[spriteID++].SetPixel(lpSurfaceTemp, lPitch, 64, 64);

			lpSurfaceTemp += 64;
		}
		lpSurface = (WORD*)((BYTE*)lpSurface + lPitch*64);
	}
	TempSurface.Unlock();


	//------------------------------------------------------------
	// Boots
	//------------------------------------------------------------
	TempSurface.InitFromBMP("Boots.bmp", 640,480, DDSCAPS_SYSTEMMEMORY);
	TempSurface.LockW(lpSurface, lPitch);

	for (i=0; i<4; i++)
	{
		lpSurfaceTemp = lpSurface;
		for (j=0; j<10; j++)
		{
			ClothesSPK[spriteID++].SetPixel(lpSurfaceTemp, lPitch, 64, 64);

			lpSurfaceTemp += 64;
		}
		lpSurface = (WORD*)((BYTE*)lpSurface + lPitch*64);
	}
	TempSurface.Unlock();

	//------------------------------------------------------------
	// Cloak
	//------------------------------------------------------------
	TempSurface.InitFromBMP("Cloak.bmp", 640,480, DDSCAPS_SYSTEMMEMORY);
	TempSurface.LockW(lpSurface, lPitch);

	for (i=0; i<4; i++)
	{
		lpSurfaceTemp = lpSurface;
		for (j=0; j<10; j++)
		{
			ClothesSPK[spriteID++].SetPixel(lpSurfaceTemp, lPitch, 64, 64);

			lpSurfaceTemp += 64;
		}
		lpSurface = (WORD*)((BYTE*)lpSurface + lPitch*64);
	}
	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Save  Clothes SpritePack	
	//------------------------------------------------------------
	std::ofstream	clothesFile("Clothes.spk", ios::binary);	
	std::ofstream	clothesIndexFile("Clothes.spki", ios::binary);	

	ClothesSPK.SaveToFile(clothesFile, clothesIndexFile);

	clothesFile.close();
	clothesIndexFile.close();
	*/	

	//------------------------------------------------------------	
	// Load  Clothes SpritePack	
	//------------------------------------------------------------
	// slayer
/*	
	if (m_AddonSPK.GetSize()==0)
	{
		std::ifstream	AddonFile2;//(FILE_ISPRITE_ADDON, ios::binary);
		if (!FileOpenBinary(FILE_ISPRITE_ADDON, AddonFile2))
			return false;

		// Only reserve the count.
		TYPE_SPRITEID addonSize;
		AddonFile2.read((char*)&addonSize, SIZE_SPRITEID);
		m_AddonSPK.Init( addonSize, CSDLGraphics::Is565() );
		AddonFile2.close();
	}
*/	
//
//
//	// Only open the file.
//	//------------------------------------------------------------
//	// sprite load
//	//------------------------------------------------------------
//	if (!FileOpenBinary(FILE_ISPRITE_ADDON, m_AddonSPKFile))
//		return false;	
//
//		m_AddonSPKFile.read((char*)&numAddonSPK, SIZE_SPRITEID);
//		m_AddonSPK.Init( numAddonSPK, CSDLGraphics::Is565() );	
//	}
	if (!m_AddonSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_ISPRITE_ADDON").c_str() )) return false;
	if (!m_OustersSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_ISPRITE_OUSTERS").c_str() )) return false;

	if (!m_AdvancementSlayerManSPK.LoadFromFileRunning( g_pFileDef->getProperty( "FILE_ISPRITE_ADVANCEMENT_CLASS_SLAYER_MAN" ).c_str() )) return false;
	if (!m_AdvancementSlayerManSSPK.LoadFromFileRunning( g_pFileDef->getProperty( "FILE_SSPRITE_ADVANCEMENT_CLASS_SLAYER_MAN" ).c_str() )) return false;

	if (!m_AdvancementSlayerWomanSPK.LoadFromFileRunning( g_pFileDef->getProperty( "FILE_ISPRITE_ADVANCEMENT_CLASS_SLAYER_WOMAN" ).c_str() )) return false;
	if (!m_AdvancementSlayerWomanSSPK.LoadFromFileRunning( g_pFileDef->getProperty( "FILE_SSPRITE_ADVANCEMENT_CLASS_SLAYER_WOMAN" ).c_str() )) return false;

//	if (!true)
	{
		//------------------------------------------------------------
		// Load shadow
		//------------------------------------------------------------
//		if (!FileOpenBinary(FILE_SSPRITE_ADDON, m_AddonSSPKFile))
//			return false;
//
//			m_AddonSSPKFile.read((char*)&numAddonSSPK, SIZE_SPRITEID);
//			m_AddonSSPK.Init( numAddonSSPK );	
//		}
		if (!m_AddonSSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_SSPRITE_ADDON").c_str())) return false;
		if (!m_OustersSSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_SSPRITE_OUSTERS").c_str())) return false;
	}

	if (!m_AdvancementOustersSPK.LoadFromFileRunning( g_pFileDef->getProperty( "FILE_ISPRITE_ADVANCEMENT_CLASS_OUSTERS" ).c_str() )) return false;
	if (!m_AdvancementOustersSSPK.LoadFromFileRunning( g_pFileDef->getProperty( "FILE_SSPRITE_ADVANCEMENT_CLASS_OUSTERS" ).c_str() )) return false;

	if (!m_AdvancementVampireManSPK.LoadFromFileRunning( g_pFileDef->getProperty( "FILE_ISPRITE_ADVANCEMENT_CLASS_VAMPIRE_MAN" ).c_str() )) return false;
	if (!m_AdvancementVampireManSSPK.LoadFromFileRunning( g_pFileDef->getProperty( "FILE_SSPRITE_ADVANCEMENT_CLASS_VAMPIRE_MAN" ).c_str() )) return false;

	if (!m_AdvancementVampireWomanSPK.LoadFromFileRunning( g_pFileDef->getProperty( "FILE_ISPRITE_ADVANCEMENT_CLASS_VAMPIRE_WOMAN" ).c_str() )) return false;
	if (!m_AdvancementVampireWomanSSPK.LoadFromFileRunning( g_pFileDef->getProperty( "FILE_SSPRITE_ADVANCEMENT_CLASS_VAMPIRE_WOMAN" ).c_str() )) return false;

/*
	m_AdvancementSlayerSPK.LoadFromFileRunning( g_pFileDef->getProperty( "FILE_ISPRITE_ADVANCEMENT_CLASS_SLAYER" ).c_str() );

	m_AdvancementSlayerSSPK.LoadFromFileRunning( g_pFileDef->getProperty( "FILE_SSPRITE_ADVANCEMENT_CLASS_SLAYER" ).c_str() );
	m_AdvancementVampireSSPK.LoadFromFileRunning( g_pFileDef->getProperty( "FILE_SSPRITE_ADVANCEMENT_CLASS_VAMPIRE" ).c_str() );

*/

	//------------------------------------------------------------	
	// Shadow - Load Clothes SpritePack
	//------------------------------------------------------------
	// Male
	/*
	std::ifstream	AddonMaleShadowFile2;//(FILE_ISPRITE_ADDON, ios::binary);
	if (!FileOpenBinary(FILE_SSPRITE_ADDON_MALE, AddonMaleShadowFile2))
		return false;
	m_AddonMaleShadowSPK.LoadFromFile(AddonMaleShadowFile2);
	AddonMaleShadowFile2.close();
	*/

	//------------------------------------------------------------
	// Test: normal SpritePack --> ShadowSpritePack
	//------------------------------------------------------------
	/*
	// Male
	CSpritePack AddonMaleShadowSPK;
	std::ifstream	AddonMaleShadowFile2;//(FILE_ISPRITE_ADDON, ios::binary);
	if (!FileOpenBinary("Data\\Image\\addonMaleShadow.spk", AddonMaleShadowFile2))
		return false;
	AddonMaleShadowSPK.LoadFromFile(AddonMaleShadowFile2);
	AddonMaleShadowFile2.close();

	m_AddonMaleSSPK.Init( AddonMaleShadowSPK.GetSize() );

	for (int i=0; i<AddonMaleShadowSPK.GetSize(); i++)
	{
		m_AddonMaleSSPK[i].SetPixel( AddonMaleShadowSPK[i] );
	}

	//------------------------------------------------------------	
	// Male - Save ShadowSpritePack
	//------------------------------------------------------------
	std::ofstream	AddonMaleShadowFile(FILE_SSPRITE_ADDON_MALE, ios::binary);	
	std::ofstream	AddonMaleShadowIndexFile(FILE_SSPRITEINDEX_ADDON_MALE, ios::binary);	

	m_AddonMaleSSPK.SaveToFile(AddonMaleShadowFile, AddonMaleShadowIndexFile);

	AddonMaleShadowFile.close();
	AddonMaleShadowIndexFile.close();
	*/

	//------------------------------------------------------------
	// Load Addon Shadow SpritePack
	//------------------------------------------------------------
	// Male
	/*
	std::ifstream	AddonShadowFile2;//(FILE_ISPRITE_ADDON, ios::binary);
	if (!FileOpenBinary(FILE_SSPRITE_ADDON, AddonShadowFile2))
		return false;
	m_AddonSSPK.LoadFromFile(AddonShadowFile2);
	AddonShadowFile2.close();
	*/

	{
		/*
		std::ifstream	AddonShadowFile2;//(FILE_ISPRITE_ADDON, ios::binary);
		if (!FileOpenBinary(FILE_SSPRITE_ADDON, AddonShadowFile2))
			return false;
		m_AddonSSPK.LoadFromFile(AddonShadowFile2);
		AddonShadowFile2.close();
		*/
	}

//	UI_DrawProgress(7);
//	DrawTitleLoading();

	//------------------------------------------------------------
	//
	//
	//                  ItemTile SpritePack
	//
	//
	//------------------------------------------------------------	
	/*
	m_ItemTileSPK.Init( 5, CSDLGraphics::Is565() );


	TempSurface.InitFromBMP("ItemTile.bmp", 800,600, DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);

	lpSurfaceTemp = (WORD*)((BYTE*)lpSurface + lPitch*96);

	CSprite::SetColorkey(0x07E0);

	spriteID = 0;
	for (i=0; i<1; i++)
	{
		lpSurfaceTemp = lpSurface;
		for (j=0; j<5; j++)
		{			
			m_ItemTileSPK[spriteID++].SetPixel(lpSurfaceTemp, lPitch, 64, 64);

			lpSurfaceTemp += 64;
		}
		lpSurface = (WORD*)((BYTE*)lpSurface + lPitch*64);
	}

	CSprite::SetColorkey(0);

	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Save  ItemTile SpritePack	
	//------------------------------------------------------------
	std::ofstream	itemTileFile(FILE_SPRITE_ITEMTILE, ios::binary);	
	std::ofstream	itemTileIndexFile(FILE_SPRITEINDEX_ITEMTILE, ios::binary);	

	m_ItemTileSPK.SaveToFile(itemTileFile, itemTileIndexFile);

	itemTileFile.close();
	itemTileIndexFile.close();
	*/

	//------------------------------------------------------------	
	// Load  ItemTile SpritePack	
	//------------------------------------------------------------
	if (m_ItemTileISPK.GetSize()==0)
	{
		if (!m_ItemTileISPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_ISPRITE_ITEMTILE").c_str())) return false;
	}

	//------------------------------------------------------------
	//
	//
	//                    ItemDrop SpritePack
	//
	//
	//------------------------------------------------------------		

	//------------------------------------------------------------	
	// Load  itemDrop SpritePack	
	//------------------------------------------------------------
	if (m_ItemDropISPK.GetSize()==0)
	{
		if (!m_ItemDropISPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_ISPRITE_ITEMDROP").c_str())) return false;
	}

	//------------------------------------------------------------
	//
	//
	//                    ItemBroken SpritePack
	//
	//
	//------------------------------------------------------------		

	//------------------------------------------------------------	
	// Load  itemBroken SpritePack	
	//------------------------------------------------------------
	if (m_ItemBrokenSPK.GetSize()==0)
	{
		if (!m_ItemBrokenSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_SPRITE_ITEMBROKEN").c_str())) return false;
	}

//	UI_DrawProgress(8);
//	DrawTitleLoading();

	//------------------------------------------------------------
	//
	//
	//                    Tile Sprite
	//
	//
	//------------------------------------------------------------		
	/*

	//------------------------------------------------------------		
	//
	//                 Tile SpritePack
	//
	//------------------------------------------------------------		
	m_TileSPK.Init( 20, CSDLGraphics::Is565() );


	TempSurface.InitFromBMP("Tile.bmp", 800,600, DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);

	lpSurfaceTemp = (WORD*)((BYTE*)lpSurface + lPitch*96);

	spriteID = 0;
	for (i=0; i<2; i++)
	{
		lpSurfaceTemp = lpSurface;
		for (j=0; j<10; j++)
		{			
			m_TileSPK[spriteID++].SetPixel(lpSurfaceTemp, lPitch, 64, 32);

			lpSurfaceTemp += 64;
		}
		lpSurface = (WORD*)((BYTE*)lpSurface + lPitch*32);
	}

	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Save  Tile SpritePack	
	//------------------------------------------------------------
	std::ofstream	tileFile(FILE_SPRITE_TILE, ios::binary);	
	std::ofstream	tileIndexFile(FILE_SPRITEINDEX_TILE, ios::binary);	

	m_TileSPK.SaveToFile(tileFile, tileIndexFile);

	tileFile.close();
	tileIndexFile.close();	

	//------------------------------------------------------------	
	// Load  Tile SpritePack	
	//------------------------------------------------------------	
	std::ifstream	tileFile2(FILE_SPRITE_TILE, ios::binary);
	m_TileSPK.LoadFromFile(tileFile2);
	tileFile2.close();
	*/


	//------------------------------------------------------------	
	//
	//                 Tile SpriteSet
	//
	//------------------------------------------------------------
	/*
	CSpriteFilePositionArray	TileSFPArray;
	TileSFPArray.Init( 6 );

	// Load the TileSPK Index.
	CFileIndexTable		TileIndex;

	std::ifstream TilePackIndexFile(FILE_SPRITEINDEX_TILE, ios::binary);
	TileIndex.LoadFromFile( TilePackIndexFile );
	TilePackIndexFile.close();	

	// Creates a Set from only the Tiles with specific SpriteIDs in the TileSPK.
	TYPE_SPRITEID	TileSpriteID[6] = { 9, 10, 11, 12, 13, 19 };
	for (TYPE_SPRITEID t=0; t<6; t++)
	{
		TileSFPArray[t].SpriteID		= TileSpriteID[t];
		TileSFPArray[t].FilePosition	= TileIndex[ TileSpriteID[t] ];
	}		
	*/

	//------------------------------------------------------------
	//  Init Tile SpriteSet
	//------------------------------------------------------------
	//int size;
	///*
//		
//		//-----------------------------------------------------------
//		// Load the Index.
//		//-----------------------------------------------------------
//		m_TileSPKI.LoadFromFile( TilePackIndexFile );
//		TilePackIndexFile.close();	
//
//		m_TileSPK.Init( m_TileSPKI.GetSize() );
//
//		/*
//		*/
//

//	UI_DrawProgress(9);
//	DrawTitleLoading();

	//*/

	/* Create the TILE INDEX
	CSpritePack spk;
	std::ifstream TilePackIndexFile(FILE_SPRITE_TILE, ios::binary);
	spk.LoadFromFile(TilePackIndexFile);	
	TilePackIndexFile.close();	

	std::ofstream TilePackIndexFile2(FILE_SPRITE_TILE, ios::binary);
	std::ofstream TilePackIndexFile3(FILE_SPRITEINDEX_TILE, ios::binary);
	spk.SaveToFile(TilePackIndexFile2, TilePackIndexFile3);
	TilePackIndexFile2.close();
	TilePackIndexFile3.close();

	m_TileSPK.Init( spk.GetSize(), CSDLGraphics::Is565() );
	*/


	/*
	std::ifstream	TilePackFile(FILE_SPRITE_TILE, ios::binary);		
	m_TileSPK.LoadFromFilePart(TilePackFile, TileSFPArray);
	TilePackFile.close();
	*/
	if (!m_TileSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_SPRITE_TILE").c_str() )) return false;

	//----------------------------------------------------------------------
	// Create TileRenderer instance (Phase 4 integration)
	//----------------------------------------------------------------------
	m_pTileRenderer = new TileRenderer();
	if (m_pTileRenderer == NULL)
	{
		DEBUG_ADD_ERR("ERROR: Failed to create TileRenderer!");
		return false;
	}
	DEBUG_ADD("TileRenderer instance created");

	//----------------------------------------------------------------------
	// Initialize TileRenderer (moved from InitSurfaces to fix initialization order)
	//----------------------------------------------------------------------
	// Note: m_pTileSurface should already be created by InitSurfaces() at this point
	if (m_pTileSurface != NULL)
	{
		printf("[InitSprites] Calling TileRenderer::Init with m_pTileSurface=%p, &m_TileSPK=%p\n",
			m_pTileSurface, &m_TileSPK);

		if (!m_pTileRenderer->Init(m_pTileSurface, &m_TileSPK))
		{
			printf("[InitSprites] ERROR: Failed to initialize TileRenderer!\n");
			DEBUG_ADD_ERR("ERROR: Failed to initialize TileRenderer!");
			delete m_pTileRenderer;
			m_pTileRenderer = NULL;
			return false;
		}
		else
		{
			// Set tile dimensions
			m_pTileRenderer->SetTileDimensions(TILE_X, TILE_Y);

			// Set null tile sprite pack (m_EtcSPK for SPRITEID_TILE_NULL)
			m_pTileRenderer->SetNullTileSpritePack(&m_EtcSPK, SPRITEID_TILE_NULL);

			printf("[InitSprites] TileRenderer initialized successfully\n");
			DEBUG_ADD("TileRenderer initialized successfully");
		}
	}
	else
	{
		printf("[InitSprites] WARNING: m_pTileSurface is NULL, TileRenderer will be initialized later\n");
		// TileRenderer will be initialized in InitSurfaces after m_pTileSurface is created
	}



	//------------------------------------------------------------
	//
	//
	//                 ImageObject SpriteSet
	//
	//------------------------------------------------------------
	//
	// Create an Index for an SPK without an Index
	/*
	CSpritePack tempSPK;
	std::ifstream	ioFile2(FILE_SPRITE_IMAGEOBJECT, ios::binary);
	tempSPK.LoadFromFile(ioFile2);
	ioFile2.close();

	std::ofstream	ioFile(FILE_SPRITE_IMAGEOBJECT, ios::binary);	
	std::ofstream	ioIndexFile(FILE_SPRITEINDEX_IMAGEOBJECT, ios::binary);	

	tempSPK.SaveToFile(ioFile, ioIndexFile);

	ioFile.close();
	ioIndexFile.close();	
	*/

	/*
	CSpriteFilePositionArray	ImageObjectSFPArray;
	ImageObjectSFPArray.Init( 21 );

	// Load the ImageObjectSPK Index.
	CFileIndexTable		ImageObjectIndex;

	std::ifstream ImageObjectPackIndexFile(FILE_SPRITEINDEX_IMAGEOBJECT, ios::binary);
	ImageObjectIndex.LoadFromFile( ImageObjectPackIndexFile );
	ImageObjectPackIndexFile.close();	

	// Creates a Set from only the ImageObjects with specific SpriteIDs in the ImageObjectSPK.
	TYPE_SPRITEID	ImageObjectSpriteID[21] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 };
	for (TYPE_SPRITEID io=0; io<21; io++)
	{
		ImageObjectSFPArray[io].SpriteID		= ImageObjectSpriteID[io];
		ImageObjectSFPArray[io].FilePosition	= ImageObjectIndex[ ImageObjectSpriteID[io] ];
	}		
	*/

	//------------------------------------------------------------
	//  Init ImageObject SpriteSet
	//------------------------------------------------------------	
	//int size;
	if (m_ImageObjectSPK.GetSize()==0)
	{

//
//		//-----------------------------------------------------------
//		// Load the Index.
//		//-----------------------------------------------------------
//		m_ImageObjectSPKI.LoadFromFile( ImageObjectPackIndexFile );
//		ImageObjectPackIndexFile.close();	
//
//		m_ImageObjectSPK.Init( m_ImageObjectSPKI.GetSize(), CSDLGraphics::Is565() );
//
//		m_ImageObjectSPKFile.open(FILE_SPRITE_IMAGEOBJECT, ios::binary);
		if (!m_ImageObjectSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_SPRITE_IMAGEOBJECT").c_str() )) return false;

		//m_ImageObjectSPK.LoadFromFile( m_ImageObjectSPKFile );

	}




//	UI_DrawProgress(10);
//	DrawTitleLoading();

	/*
	std::ifstream	ImageObjectPackFile(FILE_SPRITE_IMAGEOBJECT, ios::binary);		
	m_ImageObjectSPK.LoadFromFilePart(ImageObjectPackFile, ImageObjectSFPArray);
	ImageObjectPackFile.close();	
	*/

	//------------------------------------------------------------
	//
	//
	//                   EffectSPK initialization
	//
	//
	//------------------------------------------------------------
	/*
	m_EffectAlphaSPK.Init( 67, CSDLGraphics::Is565() );

	spriteID = 0;
	CAlphaSprite::SetColorkey( 0x001F );
	//------------------------------------------------------------
	// FireEffect
	//------------------------------------------------------------
	TempSurface.InitFromBMP("FireTest.bmp", DDSCAPS_SYSTEMMEMORY);	
	//TempSurface.InitFromBMP("FireEffect.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);

	lpSurfaceTemp = (WORD*)((BYTE*)lpSurface + lPitch*154);

	for (i=0; i<8; i++)
	{		
		m_EffectAlphaSPK[spriteID++].SetPixel(lpSurface, lPitch, 
								lpSurfaceTemp, lPitch,
								110, 154);
		lpSurface += 110;
		lpSurfaceTemp += 110;
	}		

	TempSurface.Unlock();
	CAlphaSprite::SetColorkey( 0 );

	//------------------------------------------------------------
	// Lightning Effect
	//------------------------------------------------------------	
	TempSurface.InitFromBMP("Lightning.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);

	lpSurfaceTemp = (WORD*)((BYTE*)lpSurface + lPitch*96);

	for (i=0; i<8; i++)
	{		
		m_EffectAlphaSPK[spriteID++].SetPixel(lpSurface, lPitch, 
								lpSurfaceTemp, lPitch,
								64, 96);
		lpSurface += 64;
		lpSurfaceTemp += 64;
	}		

	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Light + Darkness Effect
	//------------------------------------------------------------	
	TempSurface.InitFromBMP("LightEffect.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);

	lpSurfaceTemp = (WORD*)((BYTE*)lpSurface + lPitch*64);

	CAlphaSprite::SetColorkey( 0x001F );
	for (i=0; i<4; i++)
	{		
		m_EffectAlphaSPK[spriteID++].SetPixel(lpSurface, lPitch, 
								lpSurfaceTemp, lPitch,
								64, 64);
		lpSurface += 64;
		lpSurfaceTemp += 64;
	}		
	CAlphaSprite::SetColorkey( 0 );

	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Aura Effect
	//------------------------------------------------------------	
	TempSurface.InitFromBMP("Aura.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);

	lpSurfaceTemp2 = lpSurface;
	lpSurfaceTemp = (WORD*)((BYTE*)lpSurface + lPitch*32*4);

	CAlphaSprite::SetColorkey( 0xFFFF );
	for (i=0; i<3; i++)
	{		
		m_EffectAlphaSPK[spriteID++].SetPixel(lpSurface, lPitch, 
								lpSurfaceTemp, lPitch,
								64*3, 32*4);
		lpSurface += 64*3;
		lpSurfaceTemp += 64*3;
	}	


	lpSurface = (WORD*)((BYTE*)lpSurfaceTemp2 + lPitch*32*8);
	lpSurfaceTemp = (WORD*)((BYTE*)lpSurfaceTemp2 + lPitch*32*4);

	for (i=0; i<3; i++)
	{		
		m_EffectAlphaSPK[spriteID++].SetPixel(lpSurface, lPitch, 
								lpSurfaceTemp, lPitch,
								64*3, 32*4);
		lpSurface += 64*3;
		lpSurfaceTemp += 64*3;
	}	
	CAlphaSprite::SetColorkey( 0 );

	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Sword Effect
	//------------------------------------------------------------	
	TempSurface.InitFromBMP("SwordEffect.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);

	lpSurfaceTemp = (WORD*)((BYTE*)lpSurface + lPitch*64);

	CAlphaSprite::SetColorkey( 0xF800 );
	for (i=0; i<12; i++)
	{		
		m_EffectAlphaSPK[spriteID++].SetPixel(lpSurface, lPitch, 
								lpSurfaceTemp, lPitch,
								64, 64);
		lpSurface += 64;
		lpSurfaceTemp += 64;
	}		
	CAlphaSprite::SetColorkey( 0 );

	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Comet Effect
	//------------------------------------------------------------	
	TempSurface.InitFromBMP("CometEffect.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);

	lpSurfaceTemp = (WORD*)((BYTE*)lpSurface + lPitch*256);

	CAlphaSprite::SetColorkey( 0xFFFF );
	for (i=0; i<7; i++)
	{		
		m_EffectAlphaSPK[spriteID++].SetPixel(lpSurface, lPitch, 
								lpSurfaceTemp, lPitch,
								256, 256);
		lpSurface += 256;
		lpSurfaceTemp += 256;
	}		
	CAlphaSprite::SetColorkey( 0 );

	TempSurface.Unlock();


	//------------------------------------------------------------	
	// Star Effect
	//------------------------------------------------------------	
	TempSurface.InitFromBMP("StarEffect.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);

	lpSurfaceTemp = (WORD*)((BYTE*)lpSurface + lPitch*120);

	CAlphaSprite::SetColorkey( 0x001F );
	for (i=0; i<12; i++)
	{		
		m_EffectAlphaSPK[spriteID++].SetPixel(lpSurface, lPitch, 
								lpSurfaceTemp, lPitch,
								160, 120);
		lpSurface += 160;
		lpSurfaceTemp += 160;
	}		
	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Flare Effect
	//------------------------------------------------------------	
	TempSurface.InitFromBMP("FlareEffect.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);

	lpSurfaceTemp = (WORD*)((BYTE*)lpSurface + lPitch*100);

	CAlphaSprite::SetColorkey( 0xF800 );
	for (i=0; i<10; i++)
	{		
		m_EffectAlphaSPK[spriteID++].SetPixel(lpSurface, lPitch, 
								lpSurfaceTemp, lPitch,
								100, 100);
		lpSurface += 100;
		lpSurfaceTemp += 100;
	}		
	TempSurface.Unlock();


	//------------------------------------------------------------
	//
	// Save  EffectSPK
	//
	//------------------------------------------------------------
	std::ofstream	effectFile(FILE_ASPRITE_ALPHAEFFECT, ios::binary);	
	std::ofstream	effectIndexFile(FILE_ASPRITEINDEX_ALPHAEFFECT, ios::binary);	

	m_EffectAlphaSPK.SaveToFile(effectFile, effectIndexFile);

	effectFile.close();
	effectIndexFile.close();
	*/

	//------------------------------------------------------------
	//
	// Load  EffectPack
	//
	//------------------------------------------------------------
	// If 3D acceleration is available, use m_pAlphaEffectTextureManager,
	// otherwise use m_EffectAlphaSPK.
/*
	std::ifstream	effectFile2;//(FILE_ASPRITE_ALPHAEFFECT, ios::binary);
	if (!FileOpenBinary(FILE_ASPRITE_ALPHAEFFECT, effectFile2))
		return false;
	m_EffectAlphaSPK.LoadFromFile(effectFile2);
	effectFile2.close();	
	*/
	//------------------------------------------------------------
	// AlphaEffect
	//------------------------------------------------------------

//	SAFE_DELETE( m_pAlphaEffectTextureManager );
//	SAFE_DELETE( m_pImageObjectTextureManager );
//	m_EffectAlphaSPKI.Release();
//	m_EffectAlphaSPK.Release();


//
//		
//	}
//	else
	{
//		
//		// Only load the Index File, and
//		m_EffectAlphaSPKI.LoadFromFile( effectFileIndex );
//
//		// only initialize as many as the count.
//		m_EffectAlphaSPK.Init( m_EffectAlphaSPKI.GetSize(), CSDLGraphics::Is565() );
//
//		effectFileIndex.close();	
//
//		//------------------------------------------------------------
//		// Keep the file open for Run-time loading.
//		//------------------------------------------------------------
//		if (!FileOpenBinary(FILE_ASPRITE_ALPHAEFFECT, m_EffectAlphaSPKFile))
//			return false;
		if (!m_EffectAlphaSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_ASPRITE_ALPHAEFFECT").c_str())) return false;
		if (!m_EffectAlphaPPK.LoadFromFileRunning( g_pFileDef->getProperty("FILE_PALETTE_ALPHAEFFECT").c_str() )) return false;
	}

	//------------------------------------------------------------
	// ScreenEffect
	//------------------------------------------------------------


	{
//		
//		// Only load the Index File, and
//		m_EffectScreenSPKI.LoadFromFile( effectFileIndex );
//
//		// only initialize as many as the count.
//		m_EffectScreenSPK.Init( m_EffectScreenSPKI.GetSize(), CSDLGraphics::Is565() );
//
//		effectFileIndex.close();	
//
//		//------------------------------------------------------------
//		// Keep the file open for Run-time loading.
//		//------------------------------------------------------------
//		if (!FileOpenBinary(FILE_SPRITE_SCREENEFFECT, m_EffectScreenSPKFile))
//			return false;
		if (!m_EffectScreenSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_SPRITE_SCREENEFFECT").c_str())) return false;
		//m_EffectScreenSPK.LoadFromFile(g_pFileDef->getProperty("FILE_SPRITE_SCREENEFFECT").c_str());
		if (!m_EffectScreenPPK.LoadFromFile(g_pFileDef->getProperty("FILE_PALETTE_SCREENEFFECT").c_str())) {
			m_EffectScreenPPK.Release();
			return false;
		}
	}

	//------------------------------------------------------------
	//
	//		ShadowEffect
	//
	//------------------------------------------------------------
	{
		// This small pack loads directly, without depending on its legacy index.
		const std::string& filename = g_pFileDef->getProperty("FILE_SSPRITE_SHADOWEFFECT");
		std::ifstream shadowFile;
		if (!FileOpenBinary(filename.c_str(), shadowFile)) return false;
		if (!m_EffectShadowSPK.LoadFromFile(shadowFile)) {
			LOG_ERROR("Rejected shadow effect sprite pack: %s", filename.c_str());
			m_EffectShadowSPK.Release();
			return false;
		}
	}

//	UI_DrawProgress(12);
//	DrawTitleLoading();

	//std::ifstream	effectFile2(FILE_ASPRITE_ALPHAEFFECT, ios::binary);
	//m_EffectAlphaSPK.LoadFromFile(effectFile2);
	//effectFile2.close();	

	//------------------------------------------------------------
	//
	// Init Effect TextureSurface
	//
	//------------------------------------------------------------
	/*
	if (true)
	{	
		//--------------------------------------------
		// Creates the TexturePack from the AlphaSPK.
		//--------------------------------------------
		if (!m_EffectTPK.Init( m_EffectAlphaSPK ))
		{
			MessageBox(g_hWnd, "Can't Init EffectTexture!", NULL, MB_OK);
			return false;
		}

		//------------------------------------------------------------	
		// Deletes the EffectAlphaSprite from memory.
		// --> Since the TextureSurface will be used.. it is not needed.
		//------------------------------------------------------------		
		m_EffectAlphaSPK.Release();
	}
	*/

	//------------------------------------------------------------		
	//
	//                 Etc SpritePack
	//
	//------------------------------------------------------------	
	/*
	m_EtcSPK.Init( 13, CSDLGraphics::Is565() );


	TempSurface.InitFromBMP("Etc.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);

	lpSurfaceTemp = (WORD*)((BYTE*)lpSurface + lPitch*96);

	spriteID = 0;
	CSprite::SetColorkey( 0x001F );
	for (i=0; i<2; i++)
	{
		lpSurfaceTemp = lpSurface;
		int endj = (i==0)? 5 : 8;					
		for (j=0; j<endj; j++)
		{	
			//if (spriteID==0)
				m_EtcSPK[spriteID++].SetPixel(lpSurfaceTemp, lPitch, 64, 32);
			//else
			//	m_EtcSPK[spriteID++].SetPixel(lpSurfaceTemp, lPitch, 64, 96);

			lpSurfaceTemp += 64;
		}
		lpSurface = (WORD*)((BYTE*)lpSurface + lPitch*32);
	}	
	CSprite::SetColorkey( 0 );

	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Save  Etc SpritePack	
	//------------------------------------------------------------
	std::ofstream	EtcFile(FILE_SPRITE_ETC, ios::binary);	
	std::ofstream	EtcIndexFile(FILE_SPRITEINDEX_ETC, ios::binary);	

	m_EtcSPK.SaveToFile(EtcFile, EtcIndexFile);

	EtcFile.close();
	EtcIndexFile.close();	
	*/

	//------------------------------------------------------------	
	// Load  Etc SpritePack	
	//------------------------------------------------------------	
	if (m_EtcSPK.GetSize()==0)
	{
		const std::string& filename = g_pFileDef->getProperty("FILE_SPRITE_ETC");
		std::ifstream EtcFile2;
		if (!FileOpenBinary(filename.c_str(), EtcFile2)) return false;
		if (!m_EtcSPK.LoadFromFile(EtcFile2) || m_EtcSPK.GetSize() == 0) {
			LOG_ERROR("Rejected miscellaneous sprite pack: %s", filename.c_str());
			m_EtcSPK.Release();
			return false;
		}
	}

	//------------------------------------------------------------		
	//
	//                 InteractionObject SpritePack
	//
	//------------------------------------------------------------

//	if (m_InteractionObjectSSPK.GetSize()==0)
//	{
//		/*
//		*/
//	}

	/*	
	//------------------------------------------------------------
	//
	//      Effect Normal
	//
	//------------------------------------------------------------
	m_EffectNormalSPK.Init( 3, CSDLGraphics::Is565() );

	CSprite::SetColorkey( 0 );
	//------------------------------------------------------------
	// EffectNormalSPK
	//------------------------------------------------------------
	TempSurface.InitFromBMP("Blood1.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	m_EffectNormalSPK[ 0 ].SetPixel(lpSurface, lPitch, TempSurface.GetWidth(), TempSurface.GetHeight());
	TempSurface.Unlock();

	//------------------------------------------------------------
	// EffectNormalSPK
	//------------------------------------------------------------
	TempSurface.InitFromBMP("Blood2.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	m_EffectNormalSPK[ 1 ].SetPixel(lpSurface, lPitch, TempSurface.GetWidth(), TempSurface.GetHeight());
	TempSurface.Unlock();

	//------------------------------------------------------------
	// EffectNormalSPK
	//------------------------------------------------------------
	TempSurface.InitFromBMP("Blood3.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	m_EffectNormalSPK[ 2 ].SetPixel(lpSurface, lPitch, TempSurface.GetWidth(), TempSurface.GetHeight());
	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Save  NormalEffect Shadow SpritePack	
	//------------------------------------------------------------

	std::ofstream	NormalEffectFile(FILE_SPRITE_NORMALEFFECT, ios::binary);	
	std::ofstream	NormalEffectIndexFile(FILE_SPRITEINDEX_NORMALEFFECT, ios::binary);	

	m_EffectNormalSPK.SaveToFile(NormalEffectFile, NormalEffectIndexFile);

	NormalEffectFile.close();
	NormalEffectIndexFile.close();
	*/

	//------------------------------------------------------------	
	// Load  Clothes SpritePack	
	//------------------------------------------------------------
	if (m_EffectNormalSPK.GetSize()==0)
	{
		if (!m_EffectNormalSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_SPRITE_NORMALEFFECT").c_str())) return false;
	}


	//------------------------------------------------------------
	//
	//
	//                   Weather SpritePack initialization
	//
	//
	//------------------------------------------------------------
	//------------------------------------------------------------	
	// Load  Clothes SpritePack	
	//------------------------------------------------------------
	if (m_WeatherSPK.GetSize()==0)
	{
		const std::string& filename = g_pFileDef->getProperty("FILE_SPRITE_WEATHER");
		std::ifstream WeatherFile2;
		if (!FileOpenBinary(filename.c_str(), WeatherFile2)) return false;
		if (!m_WeatherSPK.LoadFromFile(WeatherFile2) || m_WeatherSPK.GetSize() == 0) {
			LOG_ERROR("Rejected weather sprite pack: %s", filename.c_str());
			m_WeatherSPK.Release();
			return false;
		}
	}

	//------------------------------------------------------------
	//
	//
	//                   Shadow Test initialization
	//
	//
	//------------------------------------------------------------
	/*
	CAlphaSpritePack	ShadowASPK;
	ShadowASPK.Init( 1, CSDLGraphics::Is565() );

	spriteID = 0;
	CAlphaSprite::SetColorkey( 0xFFFF );
	//------------------------------------------------------------
	// FireEffect
	//------------------------------------------------------------
	TempSurface.InitFromBMP("Shadow.bmp", DDSCAPS_SYSTEMMEMORY);	
	//TempSurface.InitFromBMP("FireEffect.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);

	lpSurfaceTemp = (WORD*)((BYTE*)lpSurface + lPitch*32);

	for (i=0; i<1; i++)
	{		
		ShadowASPK[spriteID++].SetPixel(lpSurface, lPitch, 
								lpSurfaceTemp, lPitch,
								32, 32);
		lpSurface += 32;
		lpSurfaceTemp += 32;
	}		

	TempSurface.Unlock();
	CAlphaSprite::SetColorkey( 0 );

	//--------------------------------------------
	// Creates the TexturePack from the AlphaSPK.
	//--------------------------------------------
	if (!m_ShadowTPK.Init( ShadowASPK ))
	{
		MessageBox(g_hWnd, "Can't Init ShadowTexture!", NULL, MB_OK);
		return false;
	}

	//------------------------------------------------------------	
	// Deletes the EffectAlphaSprite from memory.
	// --> Since the TextureSurface will be used.. it is not needed.
	//------------------------------------------------------------		
	ShadowASPK.Release();
	*/

	//------------------------------------------------------------
	//
	//				Minimap	
	//
	//------------------------------------------------------------
//


	/*	
	CSprite::SetColorkey( 0x001F );


	//------------------------------------------------------------
	// MinimapSprite
	//------------------------------------------------------------
	TempSurface.InitFromBMP("a-minimap.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	m_pMinimapSPR->SetPixel(lpSurface, lPitch, TempSurface.GetWidth(), TempSurface.GetHeight());
	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Save  Minimap Sprite
	//------------------------------------------------------------	
	std::ofstream	MapTest1("Data\\Image\\map_a.spr", ios::binary);		
	m_pMinimapSPR->SaveToFile( MapTest1 );
	MapTest1.close();

	//------------------------------------------------------------
	// MinimapSprite
	//------------------------------------------------------------
	TempSurface.InitFromBMP("h-minimap.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	m_pMinimapSPR->SetPixel(lpSurface, lPitch, TempSurface.GetWidth(), TempSurface.GetHeight());
	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Save  Minimap Sprite
	//------------------------------------------------------------	
	std::ofstream	MapTest2("Data\\Image\\map_h.spr", ios::binary);		
	m_pMinimapSPR->SaveToFile( MapTest2 );
	MapTest2.close();

	//------------------------------------------------------------
	// MinimapSprite
	//------------------------------------------------------------
	TempSurface.InitFromBMP("e-minimap.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	m_pMinimapSPR->SetPixel(lpSurface, lPitch, TempSurface.GetWidth(), TempSurface.GetHeight());
	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Save  Minimap Sprite
	//------------------------------------------------------------	
	std::ofstream	MapTest3("Data\\Image\\map_e.spr", ios::binary);		
	m_pMinimapSPR->SaveToFile( MapTest3 );
	MapTest3.close();

	//------------------------------------------------------------
	// MinimapSprite
	//------------------------------------------------------------
	TempSurface.InitFromBMP("c-minimap.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	m_pMinimapSPR->SetPixel(lpSurface, lPitch, TempSurface.GetWidth(), TempSurface.GetHeight());
	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Save  Minimap Sprite
	//------------------------------------------------------------	
	std::ofstream	MapTest4("Data\\Image\\map_c.spr", ios::binary);		
	m_pMinimapSPR->SaveToFile( MapTest4 );
	MapTest4.close();

	//------------------------------------------------------------
	// MinimapSprite
	//------------------------------------------------------------
	TempSurface.InitFromBMP("d-minimap.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);	
	m_pMinimapSPR->SetPixel(lpSurface, lPitch, TempSurface.GetWidth(), TempSurface.GetHeight());
	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Save  Minimap Sprite
	//------------------------------------------------------------	
	std::ofstream	MapTest5("Data\\Image\\map_d.spr", ios::binary);		
	m_pMinimapSPR->SaveToFile( MapTest5 );
	MapTest5.close();

	*/

	//------------------------------------------------------------
	//
	//
	//                   Guild SpritePack initialization
	//
	//
	//------------------------------------------------------------
	//------------------------------------------------------------	
	// Load  Guild SpritePack	
	//------------------------------------------------------------
	/*
	if (m_GuildSPK.GetSize()==0)
	{
		std::ifstream	guildFile2;//(FILE_SPRITE_WEATHER, ios::binary);
		if (!FileOpenBinary(FILE_SPRITE_GUILD, guildFile2))
			return false;
		m_GuildSPK.LoadFromFile(guildFile2);
		guildFile2.close();
	}
	*/

//	UI_DrawProgress(13);
//	DrawTitleLoading();

	return true;
}

//----------------------------------------------------------------------
// Filter initialization
//----------------------------------------------------------------------
bool
MTopView::InitFilters()
{
	// Validate the resource before releasing live lighting buffers. Surface
	// restoration also calls this function and may retain the previous pack.
	{
		const std::string& filename = g_pFileDef->getProperty("FILE_FILTER_LIGHT2D");
		std::ifstream file;
		if (!FileOpenBinary(filename.c_str(), file)) return false;
		if (!m_LightFTP.LoadFromFile(file) || m_LightFTP.GetSize() == 0) {
			LOG_ERROR("Rejected light-filter resource: %s", filename.c_str());
			return false;
		}
	}

	/*
	TYPE_FILTERID	filterID;

	CSpriteSurface	TempSurface;

	WORD	*lpSurface, 
			*lpSurfaceTemp;

	WORD	lPitch;

	int i,j;
	*/

	//------------------------------------------------------------		
	//
	//             Filter Tile SpritePack
	//
	//------------------------------------------------------------	
	/*
	m_Filter.Init( 105 );

	TempSurface.InitFromBMP("FogTile48.bmp", DDSCAPS_SYSTEMMEMORY);	
	TempSurface.LockW(lpSurface, lPitch);

	lpSurfaceTemp = (WORD*)((BYTE*)lpSurface + lPitch * TILE_Y*3);

	filterID = 0;

	for (i=0; i<12; i++)
	{
		lpSurfaceTemp = lpSurface;
		for (j=0; j<10; j++)
		{				
			m_Filter[filterID++].SetFilter(lpSurfaceTemp, lPitch, TILE_X, TILE_Y);

			lpSurfaceTemp += TILE_X;

			if (filterID%35==0) break;
		}
		lpSurface = (WORD*)((BYTE*)lpSurface + lPitch*TILE_Y);
	}

	TempSurface.Unlock();

	//------------------------------------------------------------	
	// Save  Light2D FilterPack
	//------------------------------------------------------------
	std::ofstream	FilterLight2DFile(FILE_FILTER_LIGHT2D, ios::binary);		
	m_Filter.SaveToFile(FilterLight2DFile);
	FilterLight2DFile.close();	
	*/

	//------------------------------------------------------------	
	// Load  Light2D FilterPack
	//------------------------------------------------------------	
	//std::ifstream	FilterLight2DFile2(FILE_FILTER_LIGHT2D, ios::binary);
	//m_Filter.LoadFromFile(FilterLight2DFile2);
	//FilterLight2DFile2.close();


	//------------------------------------------------------------	
	//
	//  Light Filter for 3D sight processing
	//
	//------------------------------------------------------------	
	//-----------------------------------------------
	// LightBuffer Texture initialization
	//-----------------------------------------------	
	if (m_pLightBufferTexture!=NULL)
	{
		//m_pLightBufferTexture->Restore();
		delete m_pLightBufferTexture;
		m_pLightBufferTexture = NULL;
	}
	//------------------------------------------------------------	
	// Screen pixel size one point occupies in 2D light
	//------------------------------------------------------------	
	if (m_p2DLightPixelWidth!=NULL)
	{
		delete [] m_p2DLightPixelWidth;
		m_p2DLightPixelWidth = NULL;
	}

	if (m_p2DLightPixelHeight!=NULL)
	{
		delete [] m_p2DLightPixelHeight;
		m_p2DLightPixelHeight = NULL;
	}


// SDL2: Unified 2D rendering path for all platforms
	// Load Light2D FilterPack
	{
		//------------------------------------------------------
		// lightBuffer initialization
		//------------------------------------------------------
		m_LightBufferFilter.Init( SCREENLIGHT_WIDTH, SCREENLIGHT_HEIGHT );

		m_p2DLightPixelWidth = new int [SCREENLIGHT_WIDTH];
		m_p2DLightPixelHeight = new int [SCREENLIGHT_HEIGHT];

		//----------------------------------------------------------------
		// Horizontal pixel length on screen that one point of the LightBuffer stands for
		//----------------------------------------------------------------
		// Sum of the widths = 100 * 8 = 800
		BYTE x1,y1=0;
		if(g_MyFull)
		{
			x1=16;
			y1=16;
		}
		else
		{
			x1=12;
			y1=13;
		}
		const int pPixelWidth[SCREENLIGHT_WIDTH] = 
		{
			x1, y1, x1, y1, x1,	y1, x1, y1, 
			x1, y1, x1, y1, x1, y1, x1, y1, 
			x1, y1, x1, y1, x1, y1, x1, y1, 
			x1, y1, x1, y1, x1, y1, x1, y1, 
			x1, y1, x1, y1, x1, y1, x1, y1, 
			x1, y1, x1, y1, x1, y1, x1, y1, 
			x1, y1, x1, y1, x1, y1, x1, y1, 
			x1, y1, x1, y1, x1, y1, x1, y1 
		};
/*
		const int pPixelWidth[SCREENLIGHT_WIDTH] = 
		{
			16, 16, 16, 16, 16,	16, 16, 16, 
			16, 16, 16, 16, 16, 16, 16, 16, 
			16, 16, 16, 16, 16, 16, 16, 16, 
			16, 16, 16, 16, 16, 16, 16, 16, 
			16, 16, 16, 16, 16, 16, 16, 16, 
			16, 16, 16, 16, 16, 16, 16, 16, 
			16, 16, 16, 16, 16, 16, 16, 16, 
			16, 16, 16, 16, 16, 16, 16, 16 
		};
*/

		//----------------------------------------------------------------
		// Vertical pixel length on screen that one point of the LightBuffer stands for
		//----------------------------------------------------------------
		// The count is SCREENLIGHT_HEIGHT
		// The total only has to equal CLIPSURFACE_HEIGHT.
		// Currently 600.  10*24 + 9*40
		//----------------------------------------------------------------
		if(g_MyFull)
		{
			x1=12;
			y1=12;
		}
		else
		{
			x1=10;
			y1=9;
		}
		const int pPixelHeight[SCREENLIGHT_HEIGHT] = 
		{
			x1,y1,y1,x1, y1,y1,x1,y1,
			x1,y1,y1,x1, y1,y1,x1,y1,
			x1,y1,y1,x1, y1,y1,x1,y1,
			x1,y1,y1,x1, y1,y1,x1,y1,
			x1,y1,y1,x1, y1,y1,x1,y1,
			x1,y1,y1,x1, y1,y1,x1,y1,
			x1,y1,y1,x1, y1,y1,x1,y1,
			x1,y1,y1,x1, y1,y1,x1,y1,
		};
		/*
		//Changed to 768
		//----------------------------------------------------------------
		const int pPixelHeight[SCREENLIGHT_HEIGHT] = 
		{

			12,12,12,12, 12,12,12,12,
			12,12,12,12, 12,12,12,12,
			12,12,12,12, 12,12,12,12,
			12,12,12,12, 12,12,12,12,
			12,12,12,12, 12,12,12,12,
			12,12,12,12, 12,12,12,12,
			12,12,12,12, 12,12,12,12,
			12,12,12,12, 12,12,12,12,

		};
		*/

		int i;
		for (i=0; i<SCREENLIGHT_WIDTH; i++)
		{
			m_p2DLightPixelWidth[i] = pPixelWidth[i];
		}

		for (i=0; i<SCREENLIGHT_HEIGHT; i++)
		{
			m_p2DLightPixelHeight[i] = pPixelHeight[i];
		}

	}



	//------------------------------------------------------------
	//
	// Filter that handles the ImageObjects covering the Player
	//
	//------------------------------------------------------------
	/*
	register int i;
	register int k;

	//
	m_ImageObjectFilter.Init(200,200);
	for (i=0; i<200; i++)
	{			
		for (int j=0; j<200; j++)
		{							
			k = sqrt(abs(i-100)*abs(i-100) ,
					abs(j-100)*abs(j-100));
			//k = sqrt(abs(i-100)*abs(i-100) ,
					//abs(j-200)/2*abs(j-200)/2);

			//k -= 50;	// Empty space

			k >>= 2;	// Soft(?) space

			k += 5;


			if (k>32) k=32;
			else if (k<1) k=1;

			//	k = (rand()%(77 - k*3))? 26:32;
			//else if (k>15 && k<21)
			//	k = (rand()%(102 - k*5))? 26:32;

			// Only the values 0 and 1...
			if (k > 26) 
				k = 1;
			else
				k = 0;

			m_ImageObjectFilter.SetFilter(j,i, k);
		}				
	}

	//------------------------------------------------------------	
	// Save  Light3D FilterPack
	//------------------------------------------------------------
	std::ofstream	ImageObjectFilterFile(FILE_FILTER_IMAGEOBJECT, ios::binary);
	m_ImageObjectFilter.SaveToFile(ImageObjectFilterFile);
	ImageObjectFilterFile.close();	
	*/

	//------------------------------------------------------------	
	// Load  Light3D FilterPack
	//------------------------------------------------------------	
	if (m_ImageObjectFilter.IsNotInit())
	{
		std::ifstream	ImageObjectFilterFile2;//(FILE_FILTER_IMAGEOBJECT, ios::binary);
		if (!FileOpenBinary(g_pFileDef->getProperty("FILE_FILTER_IMAGEOBJECT").c_str(), ImageObjectFilterFile2))
			return false;
		m_ImageObjectFilter.LoadFromFile(ImageObjectFilterFile2);
		ImageObjectFilterFile2.close();
	}

	return true;
}

//----------------------------------------------------------------------
// Creates the Character information.
//----------------------------------------------------------------------
// m_CreatureFrame[type][Action][Direction][Frame]
//----------------------------------------------------------------------
bool
MTopView::InitCreatureFrames()
{		
	//------------------------------------------------------------
	//
	//  Creates the Frame information of the Creature Body.
	//
	//------------------------------------------------------------

	/*
	m_CreatureFPK.Init(MAX_CREATURE_BODY);	

	//-----------------------
	// First Creature (Woman)
	//-----------------------
	// Define the number of actions
	m_CreatureFPK[0].Init(ACTION_MAX);

	// n-th Sprite
	WORD n = 0;
	int i,j,k;

	// Each action has a FrameArray for 8 directions.
	for (k=0; k<ACTION_MAX; k++)
		m_CreatureFPK[0][k].Init(8);

	// Directions 0~8 each have 8 Frames.
	for (i=0; i<8; i++)
	{
		for (k=0; k<ACTION_MAX; k++)
			m_CreatureFPK[0][k][i].Init(8);		

		// Sets the information for the 8 Frames.
		for (j=0; j<8; j++)
		{
			for (k=0; k<ACTION_MAX; k++)
				m_CreatureFPK[0][k][i][j].Set(n, 0,-40);			

			n ++;
		}  		
	}	

	//-----------------------
	// Second Creature (Skeleton)
	//-----------------------
	// Define the number of actions
	m_CreatureFPK[1].Init(ACTION_MAX);

	// Each action has a FrameArray for 8 directions.
	for (k=0; k<ACTION_MAX; k++)
		m_CreatureFPK[1][k].Init(8);	

	// sprite order
	int step[] = { 2,3,4,3,2,1,0,1 };


	// n-th Sprite
	n = 64;

	// Directions 0~8 each have 8 Frames.
	for (i=0; i<8; i++)
	{
		for (k=0; k<ACTION_MAX; k++)
			m_CreatureFPK[1][k][i].Init(8);		

		// Sets the information for the 8 Frames.
		for (j=0; j<8; j++)
		{			
			for (k=0; k<ACTION_MAX; k++)			
				m_CreatureFPK[1][k][i][j].Set(n,step[j], 0,-40);
		}  

		n += 5;
	}	


	//-----------------------
	// Third Creature (Vamp1)
	//-----------------------
	// Define the number of actions
	m_CreatureFPK[2].Init(ACTION_MAX_VAMPIRE);

	// n-th Sprite
	n = 104;

	// Each action has a FrameArray for 8 directions.
	for (k=0; k<ACTION_MAX_VAMPIRE; k++)
		m_CreatureFPK[2][k].Init(8);

	//------------------------------------------------
	// ACTION_STAND - for the 8 directions
	//------------------------------------------------
	for (i=0; i<8; i++)
	{
		// Number of Frames per direction
		m_CreatureFPK[2][ACTION_STAND][i].Init(16);

		int index = 0;
		// Sets the information for the 4 Frames.
		for (j=0; j<4; j++)
		{			
			m_CreatureFPK[2][ACTION_STAND][i][index++].Set(n, 15,-60);
			m_CreatureFPK[2][ACTION_STAND][i][index++].Set(n, 15,-60);
			m_CreatureFPK[2][ACTION_STAND][i][index++].Set(n, 15,-60);
			m_CreatureFPK[2][ACTION_STAND][i][index++].Set(n, 15,-60);
			n ++;
		}
	}

	//------------------------------------------------
	// ACTION_MOVE - for the 8 directions
	//------------------------------------------------
	for (i=0; i<8; i++)
	{
		// Number of Frames per direction
		m_CreatureFPK[2][ACTION_MOVE][i].Init(8);

		// Sets the information for the 8 Frames.
		for (j=0; j<7; j++)
		{
			// 0~7 frame
			m_CreatureFPK[2][ACTION_MOVE][i][j].Set(n, 15,-60);
			n ++;
		}
		// 8th frame
		m_CreatureFPK[2][ACTION_MOVE][i][7].Set(n-7, 15,-60);
	}

	//------------------------------------------------
	// ACTION_ATTACK - for the 8 directions
	//------------------------------------------------
	for (i=0; i<8; i++)
	{
		// Number of Frames per direction
		m_CreatureFPK[2][ACTION_ATTACK][i].Init(8);

		// Sets the information for the 8 Frames.
		for (j=0; j<8; j++)
		{
			m_CreatureFPK[2][ACTION_ATTACK][i][j].Set(n, 15,-60);
			n ++;
		}
	}

	//------------------------------------------------
	// ACTION_MAGIC - for the 8 directions
	//------------------------------------------------
	for (i=0; i<8; i++)
	{
		// Number of Frames per direction
		m_CreatureFPK[2][ACTION_MAGIC][i].Init(8);

		// Sets the information for the 8 Frames.
		k=0;
		int kk=6;
		//int index = 0;
		for (j=0; j<8; j++)
		{
			//m_CreatureFPK[2][ACTION_MAGIC][i][index++].Set(n, 15,-60-k);
			//m_CreatureFPK[2][ACTION_MAGIC][i][index++].Set(n, 15,-60-k);
			m_CreatureFPK[2][ACTION_MAGIC][i][j].Set(n, 15,-60-k);
			n ++;

			if (j==4) kk=-6;
			k += kk;
		}
	}

	//------------------------------------------------
	// ACTION_DAMAGED - for the 8 directions
	//------------------------------------------------
	for (i=0; i<8; i++)
	{
		// Number of Frames per direction
		m_CreatureFPK[2][ACTION_DAMAGED][i].Init(6);

		// Sets the information for the 6 Frames.
		for (j=0; j<6; j++)
		{
			m_CreatureFPK[2][ACTION_DAMAGED][i][j].Set(n, 15,-60);
			n ++;
		}
	}

	//------------------------------------------------
	// ACTION_DRAINED - for the 8 directions
	//------------------------------------------------
	for (i=0; i<8; i++)
	{
		// Number of Frames per direction
		m_CreatureFPK[2][ACTION_DRAINED][i].Init(7);

		// Sets the information for the 7 Frames.
		for (j=0; j<5; j++)
		{
			m_CreatureFPK[2][ACTION_DRAINED][i][j].Set(n, 15,-60);
			n ++;
		}

		m_CreatureFPK[2][ACTION_DRAINED][i][5].Set(n-3, 15,-60);
		m_CreatureFPK[2][ACTION_DRAINED][i][6].Set(n-4, 15,-60);
	}

	//------------------------------------------------
	// ACTION_DIE - for the 8 directions
	//------------------------------------------------
	for (i=0; i<8; i++)
	{
		// Number of Frames per direction
		m_CreatureFPK[2][ACTION_DIE][i].Init(4);

		// Sets the information for the 4 Frames.
		for (j=0; j<4; j++)
		{
			m_CreatureFPK[2][ACTION_DIE][i][j].Set(n, 15,-60);
			//n ++;
		}
	}

	//------------------------------------------------
	// ACTION_VAMPIRE_DRAIN - for the 8 directions
	//------------------------------------------------
	for (i=0; i<8; i++)
	{
		// Number of Frames per direction
		m_CreatureFPK[2][ACTION_VAMPIRE_DRAIN][i].Init(7);

		// Sets the information for the 7 Frames.
		for (j=0; j<5; j++)
		{
			m_CreatureFPK[2][ACTION_VAMPIRE_DRAIN][i][j].Set(n, 15,-60);
			n ++;
		}

		m_CreatureFPK[2][ACTION_VAMPIRE_DRAIN][i][5].Set(n-3, 15,-60);
		m_CreatureFPK[2][ACTION_VAMPIRE_DRAIN][i][6].Set(n-4, 15,-60);
	}


	std::ofstream packFile(FILE_CFRAME_CREATURE, ios::binary);
	std::ofstream indexFile(FILE_CFRAMEINDEX_CREATURE, ios::binary);
	m_CreatureFPK.SaveToFile(packFile, indexFile);	
	packFile.close();
	indexFile.close();	
	*/

	///*
	// Load from File
	std::ifstream file;//(FILE_CFRAME_CREATURE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_CREATURE").c_str(), file))
		return false;
	m_CreatureFPK.LoadFromFile(file);
	file.close();
	//*/	
	WorldEvents::DrawTitleLoading();
	//m_CreatureFPK.InfoToFile("log\\CreatureFPK.txt");

	// Shift the death action of Frame2 (zombie) by 40 in the y coordinate.
	/*
	ACTION_FRAME_ARRAY &zombie = m_CreatureFPK[2];

	DIRECTION_FRAME_ARRAY &dead = zombie[ACTION_DIE];

	for (int d=0; d<8; d++)
	{
		FRAME_ARRAY &fr = dead[d];

		for (int f=0; f<fr.GetSize(); f++)
		{
			fr[f].Set( fr[f].GetSpriteID(), fr[f].GetCX(), fr[f].GetCY(),40 );
		}
	}

	std::ofstream packFile(FILE_CFRAME_CREATURE, ios::binary);
	std::ofstream indexFile(FILE_CFRAMEINDEX_CREATURE, ios::binary);
	m_CreatureFPK.SaveToFile(packFile, indexFile);	
	packFile.close();
	indexFile.close();
	*/


	/*
	// Swap Damaged 0,1 to 1,0.
	for (int ct=0; ct<m_CreatureFPK.GetSize(); ct++)
	{
		DIRECTION_FRAME_ARRAY& damaged = m_CreatureFPK[ct][ACTION_DAMAGED];

		for (int d=0; d<8; d++)
		{
			FRAME_ARRAY& dfr = damaged[d];

			// Swap frames 0 and 1.
			CFrame frame0 = dfr[0];
			CFrame frame1 = dfr[1];

			CFrame temp = frame0;			
			frame0 = frame1;
			frame1 = temp;
		}
	}
	*/

	//m_CreatureFPK.InfoToFile("creature.txt");


	// Turn the 4 stand-still frames into 6 frames, then into 18 frames again..
	/*
	DIRECTION_FRAME_ARRAY& stand = m_CreatureFPK[1][ACTION_STAND];
	DIRECTION_FRAME_ARRAY standTemp;
	DIRECTION_FRAME_ARRAY standTemp2;
	standTemp = stand;
	standTemp2.Init( 8 );
	for (int d=0; d<8; d++)
	{		
		// 4 frames into 6 frames..
		standTemp2[d].Init( standTemp[d].GetSize(),2 );
		for (int f=0; f<standTemp[d].GetSize(); f++)
		{
			standTemp2[d][f] = standTemp[d][f];			
		}
		standTemp2[d][4] = standTemp[d][2];	
		standTemp2[d][5] = standTemp[d][1];	

		// 6 frames into 18 frames..
		stand[d].Init( standTemp2[d].GetSize()*3 );
		int ff=0;
		for (f=0; f<standTemp2[d].GetSize(); f++)
		{
			stand[d][ff++] = standTemp2[d][f];			
			stand[d][ff++] = standTemp2[d][f];
			stand[d][ff++] = standTemp2[d][f];
		}
	}	

	// Turn the 6 walking frames into 12 frames...
	DIRECTION_FRAME_ARRAY& move = m_CreatureFPK[1][ACTION_MOVE];
	DIRECTION_FRAME_ARRAY moveTemp;
	moveTemp = move;
	for (d=0; d<8; d++)
	{
		move[d].Init( moveTemp[d].GetSize()*3 );

		int ff = 0;
		for (int f=0; f<moveTemp[d].GetSize(); f++)
		{
			move[d][ff++] = moveTemp[d][f];
			move[d][ff++] = moveTemp[d][f];
			move[d][ff++] = moveTemp[d][f];
		}
	}

	std::ofstream packFile(FILE_CFRAME_CREATURE, ios::binary);
	std::ofstream indexFile(FILE_CFRAMEINDEX_CREATURE, ios::binary);
	m_CreatureFPK.SaveToFile(packFile, indexFile);	
	packFile.close();
	indexFile.close();	
	*/

	//------------------------------------------------------------
	//
	// Creature Shadow FPK - Loading
	//
	//------------------------------------------------------------
	std::ifstream fileShadow;//(FILE_CFRAME_CREATURE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_CREATURE_SHADOW").c_str(), fileShadow))
		return false;
	m_CreatureShadowFPK.LoadFromFile(fileShadow);
	fileShadow.close();
	WorldEvents::DrawTitleLoading();
	//m_CreatureShadowFPK.InfoToFile("log\\CreatureShadowFPK.txt");

	//------------------------------------------------------------
	//
	//  Creates the Frame information of the addon.
	//
	//------------------------------------------------------------
	//
	/*
	m_AddonFPK.Init(MAX_CREATURE_ADDON);


	//--------------
	// First clothes
	//--------------
	// There is only 1 action.
	m_AddonFPK[0].Init(1);
	m_AddonFPK[1].Init(1);
	m_AddonFPK[2].Init(1);
	m_AddonFPK[3].Init(1);

	// That 1 action has a FrameArray for 8 directions.
	m_AddonFPK[0][0].Init(8);
	m_AddonFPK[1][0].Init(8);
	m_AddonFPK[2][0].Init(8);
	m_AddonFPK[3][0].Init(8);

	// n-th tile
	//n = n + 80;
	n = 0;

	// Directions 0~8 each have 8 Frames.
	for (i=0; i<8; i++)
	{
		m_AddonFPK[0][0][i].Init(8);
		m_AddonFPK[1][0][i].Init(8);
		m_AddonFPK[2][0][i].Init(8);
		m_AddonFPK[3][0][i].Init(8);

		// Sets the information for the 8 Frames.
		for (int j=0; j<8; j++)
		{
			// If there is a Shirt picture at 83~122
			m_AddonFPK[0][0][i][j].Set(n,step[j], 0,-40);

			// If there is a Pants picture at 123~162
			m_AddonFPK[1][0][i][j].Set(n,40,step[j], 0,-40);

			// Boots
			m_AddonFPK[2][0][i][j].Set(n,80,step[j], 0,-40);

			// Cloak
			m_AddonFPK[3][0][i][j].Set(n,120,step[j], 0,-40);
		}

		n+=5;
	}	

	packFile.open(FILE_CFRAME_ADDON, ios::binary);
	indexFile.open(FILE_CFRAMEINDEX_ADDON, ios::binary);
	m_AddonFPK.SaveToFile(packFile, indexFile);	
	packFile.close();
	indexFile.close();
	*/

	///*

	std::ifstream AdvancementOustersFile;//(FILE_CFRAME_ADDON_MALE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_OUSTERS").c_str(), AdvancementOustersFile))
		return false;
	m_AdvancementOustersFPK.LoadFromFile(AdvancementOustersFile);
	AdvancementOustersFile.close();	

	std::ifstream AdvancementOustersShadowFile;//(FILE_CFRAME_ADDON_MALE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_OUSTERS_SHADOW").c_str(), AdvancementOustersFile))
		return false;
	m_AdvancementOustersShadowFPK.LoadFromFile(AdvancementOustersFile);
	AdvancementOustersFile.close();	

	std::ifstream AdvancementVampireManFile;
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_VAMPIRE_MAN").c_str(), 
		AdvancementVampireManFile))
		return false;
	m_AdvancementVampireManFPK.LoadFromFile( AdvancementVampireManFile );
	AdvancementVampireManFile.close();


	std::ifstream AdvancementVampireManShadowFile;
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_VAMPIRE_MAN_SHADOW").c_str(), 
		AdvancementVampireManShadowFile))
		return false;
	m_AdvancementVampireManShadowFPK.LoadFromFile( AdvancementVampireManShadowFile );
	AdvancementVampireManShadowFile.close();

	std::ifstream AdvancementVampireWomanFile;
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_VAMPIRE_WOMAN").c_str(), 
		AdvancementVampireWomanFile))
		return false;
	m_AdvancementVampireWomanFPK.LoadFromFile( AdvancementVampireWomanFile );
	AdvancementVampireWomanFile.close();

	std::ifstream AdvancementVampireWomanShadowFile;
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_VAMPIRE_WOMAN_SHADOW").c_str(), 
		AdvancementVampireWomanShadowFile))
		return false;
	m_AdvancementVampireWomanShadowFPK.LoadFromFile( AdvancementVampireWomanShadowFile );
	AdvancementVampireWomanShadowFile.close();


	std::ifstream AdvancementSlayerManFile;
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_SLAYER_MAN").c_str(), 
		AdvancementSlayerManFile))
		return false;
	m_AdvancementSlayerManFPK.LoadFromFile( AdvancementSlayerManFile );
	AdvancementSlayerManFile.close();

	std::ifstream AdvancementSlayerManShadowFile;
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_SLAYER_MAN_SHADOW").c_str(), 
		AdvancementSlayerManShadowFile))
		return false;
	m_AdvancementSlayerManShadowFPK.LoadFromFile( AdvancementSlayerManShadowFile );
	AdvancementSlayerManShadowFile.close();


	std::ifstream AdvancementSlayerWomanFile;
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_SLAYER_WOMAN").c_str(), 
		AdvancementSlayerWomanFile))
		return false;
	m_AdvancementSlayerWomanFPK.LoadFromFile( AdvancementSlayerWomanFile );
	AdvancementSlayerWomanFile.close();

	std::ifstream AdvancementSlayerWomanShadowFile;
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_SLAYER_WOMAN_SHADOW").c_str(), 
		AdvancementSlayerWomanShadowFile))
		return false;
	m_AdvancementSlayerWomanShadowFPK.LoadFromFile( AdvancementSlayerWomanShadowFile );
	AdvancementSlayerWomanShadowFile.close(); 

	/*
	//------------------------------------------------
	// Load from File
	//------------------------------------------------
	std::ifstream AdvancementSlayerFile;//(FILE_CFRAME_ADDON_MALE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_SLAYER").c_str(), AdvancementSlayerFile))
		return false;
	m_AdvancementSlayerFPK.LoadFromFile(AdvancementSlayerFile);
	AdvancementSlayerFile.close();

	std::ifstream AdvancementVampireFile;//(FILE_CFRAME_ADDON_MALE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_VAMPIRE").c_str(), AdvancementVampireFile))
		return false;
	m_AdvancementVampireFPK.LoadFromFile(AdvancementVampireFile);
	AdvancementVampireFile.close();

	std::ifstream AdvancementSlayerShadowFile;//(FILE_CFRAME_ADDON_MALE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_SLAYER_SHADOW").c_str(), AdvancementSlayerFile))
		return false;
	m_AdvancementSlayerShadowFPK.LoadFromFile(AdvancementSlayerFile);
	AdvancementSlayerFile.close();

	std::ifstream AdvancementVampireShadowFile;//(FILE_CFRAME_ADDON_MALE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADVANCEMENT_CLASS_VAMPIRE_SHADOW").c_str(), AdvancementVampireFile))
		return false;
	m_AdvancementVampireShadowFPK.LoadFromFile(AdvancementVampireFile);
	AdvancementVampireFile.close();

*/
	//------------------------------------------------
	// Load from File
	//------------------------------------------------
	std::ifstream AddonFile2;//(FILE_CFRAME_ADDON_MALE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADDON").c_str(), AddonFile2))
		return false;
	m_AddonFPK.LoadFromFile(AddonFile2);
	AddonFile2.close();
	std::ifstream OustersFile2;//(FILE_CFRAME_ADDON_MALE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_OUSTERS").c_str(), OustersFile2))
		return false;
	m_OustersFPK.LoadFromFile(OustersFile2);
	OustersFile2.close();
	WorldEvents::DrawTitleLoading();

	//------------------------------------------------
	// Shadow - Load from File
	//------------------------------------------------
	std::ifstream AddonShadowFile2;//(FILE_CFRAME_ADDON_MALE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_ADDON_SHADOW").c_str(), AddonShadowFile2))
		return false;
	m_AddonShadowFPK.LoadFromFile(AddonShadowFile2);
	AddonShadowFile2.close();

	std::ifstream OustersShadowFile2;//(FILE_CFRAME_ADDON_MALE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_CFRAME_OUSTERS_SHADOW").c_str(), OustersShadowFile2))
		return false;
	m_OustersShadowFPK.LoadFromFile(OustersShadowFile2);
	OustersShadowFile2.close();


	WorldEvents::DrawTitleLoading();

	// Add the hair-loss case to the total actions
	// Even so something doesn't work!!
	/*
	int add;
	for (add=0; add<MAX_ADDONID_MALE; add++)
	{
		for (int i=0; i<8; i++)
		{
			if (add!=ADDONID_GUN_AR_MALE)		
				m_AddonMaleFPK[add][ACTION_SLAYER_GUN_AR][i] = m_AddonMaleFPK[add][ACTION_SLAYER_GUN_TR][i];

			if (add!=ADDONID_GUN_SMG_MALE)		
				m_AddonMaleFPK[add][ACTION_SLAYER_GUN_SMG][i] = m_AddonMaleFPK[add][ACTION_SLAYER_GUN_TR][i];
		}
	}

	for (add=0; add<MAX_ADDONID_FEMALE; add++)
	{
		for (int i=0; i<8; i++)
		{
			if (add!=ADDONID_GUN_AR_FEMALE)		
				m_AddonFemaleFPK[add][ACTION_SLAYER_GUN_AR][i] = m_AddonMaleFPK[add][ACTION_SLAYER_GUN_TR][i];

			if (add!=ADDONID_GUN_SMG_FEMALE)		
				m_AddonFemaleFPK[add][ACTION_SLAYER_GUN_SMG][i] = m_AddonMaleFPK[add][ACTION_SLAYER_GUN_TR][i];
		}
	}
	*/

	// Changed..
	/*
	int add, a, d, f;
	for (add=0; add<MAX_ADDONID_MALE; add++)
	{
		ACTION_FRAME_ARRAY& AFA = m_AddonMaleFPK[add];

		// Add an action for the motorcycle
		if (add==ADDONID_MOTORCYCLE_MALE)
		{			
			for (a=0; a<ACTION_MAX_SLAYER; a++)
			{
				if (a==1)
				{
					DIRECTION_FRAME_ARRAY& DFA = AFA[a];

					for (d=0; d<8; d++)
					{
						FRAME_ARRAY& FA = DFA[d];
						FRAME_ARRAY newFA;
						newFA.Init( 18 );

						newFA[0] = FA[0];
						newFA[1] = FA[1];
						newFA[2] = FA[2];
						newFA[3] = FA[1];
						for (f=4; f<18; f++)
						{
							newFA[f] = FA[f%4];
						}

						FA = newFA;
					}
				}
				else
				{
					DIRECTION_FRAME_ARRAY& DFA = AFA[a];

					for (d=0; d<8; d++)
					{
						FRAME_ARRAY& FA = DFA[d];
						FA.Init( 18 );

						FA[0] = AFA[1][d][0];
						FA[1] = AFA[1][d][1];
						FA[2] = AFA[1][d][2];
						FA[3] = AFA[1][d][1];
						for (f=4; f<18; f++)
						{
							FA[f] = FA[f%4];
						}
					}
				}
			}
		}
		else
		{
			for (a=0; a<AFA.GetSize(); a++)
			{
				DIRECTION_FRAME_ARRAY& DFA = AFA[a];

				for (d=0; d<DFA.GetSize(); d++)
				{
					FRAME_ARRAY& FA = DFA[d];

					// Change the motor action from 3 --> 4 frames.
					if (a==ACTION_SLAYER_MOTOR_MOVE)
					{
						if (FA.GetSize()!=0)
						{
							FRAME_ARRAY newFA;
							newFA.Init( 4 );
							// The 3-entry FA must be made into 4.
							for (f=0; f<FA.GetSize(); f++)
							{
								CFrame& frame = FA[f];

								newFA[f] = FA[f];
							}

							newFA[3] = FA[2];

							FA = newFA;
						}
					}

				}
			}
		}
	}
	*/

	//m_AddonMaleFPK.InfoToFile("log\\addonMale.txt");
	//m_AddonFemaleFPK.InfoToFile("log\\addonFemale.txt");
	//m_AddonMaleShadowFPK.InfoToFile("log\\addonMaleShadow.txt");
	//m_AddonFemaleShadowFPK.InfoToFile("log\\addonFemaleShadow.txt");

	/*
	// [Male] Change the sword action to 6 frames...
	for (int add=0; add<m_AddonMaleFPK.GetSize(); add++)
	{	
		ACTION_FRAME_ARRAY &addon = m_AddonMaleFPK[add];

		DIRECTION_FRAME_ARRAY &SWORD = addon[ACTION_SLAYER_SWORD];
		DIRECTION_FRAME_ARRAY &BLADE = addon[ACTION_SLAYER_BLADE];

		addon[ACTION_SLAYER_SWORD_2] = SWORD;
		addon[ACTION_SLAYER_BLADE_2] = BLADE;

		for (int d=0; d<8; d++)
		{
			FRAME_ARRAY &fr1 = SWORD[d];
			FRAME_ARRAY &fr2 = BLADE[d];

			if (fr1.GetSize()!=0)
			{			
				FRAME_ARRAY temp;
				temp.Init( 6 );

				temp[0] = fr1[0];
				temp[1] = fr1[1];
				temp[2] = fr1[2];
				temp[3] = fr1[3];
				temp[4] = fr1[4];
				temp[5] = fr1[fr1.GetSize()-1];

				fr1 = temp;
			}

			if (fr2.GetSize()!=0)
			{			
				FRAME_ARRAY temp;
				temp.Init( 6 );

				temp[0] = fr2[0];
				temp[1] = fr2[1];
				temp[2] = fr2[2];
				temp[3] = fr2[3];
				temp[4] = fr2[4];
				temp[5] = fr2[fr2.GetSize()-1];

				fr2 = temp;
			}
		}
	}

	// [Female] Change the sword action to 6 frames...
	for (add=0; add<m_AddonFemaleFPK.GetSize(); add++)
	{	
		ACTION_FRAME_ARRAY &addon = m_AddonFemaleFPK[add];

		DIRECTION_FRAME_ARRAY &SWORD = addon[ACTION_SLAYER_SWORD];
		DIRECTION_FRAME_ARRAY &BLADE = addon[ACTION_SLAYER_BLADE];

		addon[ACTION_SLAYER_SWORD_2] = SWORD;
		addon[ACTION_SLAYER_BLADE_2] = BLADE;

		for (int d=0; d<8; d++)
		{
			FRAME_ARRAY &fr1 = SWORD[d];
			FRAME_ARRAY &fr2 = BLADE[d];

			if (fr1.GetSize()!=0)
			{			
				FRAME_ARRAY temp;
				temp.Init( 6 );

				temp[0] = fr1[0];
				temp[1] = fr1[1];
				temp[2] = fr1[2];
				temp[3] = fr1[3];
				temp[4] = fr1[4];
				temp[5] = fr1[fr1.GetSize()-1];

				fr1 = temp;
			}

			if (fr2.GetSize()!=0)
			{			
				FRAME_ARRAY temp;
				temp.Init( 6 );

				temp[0] = fr2[0];
				temp[1] = fr2[1];
				temp[2] = fr2[2];
				temp[3] = fr2[3];
				temp[4] = fr2[4];
				temp[5] = fr2[fr2.GetSize()-1];

				fr2 = temp;
			}
		}
	}

	std::ofstream packFileMale(FILE_CFRAME_ADDON_MALE, ios::binary);
	std::ofstream indexFileMale(FILE_CFRAMEINDEX_ADDON_MALE, ios::binary);
	m_AddonMaleFPK.SaveToFile(packFileMale, indexFileMale);	
	packFileMale.close();
	indexFileMale.close();

	std::ofstream packFileFemale(FILE_CFRAME_ADDON_FEMALE, ios::binary);
	std::ofstream indexFileFemale(FILE_CFRAMEINDEX_ADDON_FEMALE, ios::binary);
	m_AddonFemaleFPK.SaveToFile(packFileFemale, indexFileFemale);	
	packFileFemale.close();
	indexFileFemale.close();

	*/


	// Change the female gun action
	/*
	m_AddonFemaleFPK[ADDONID_GUN_TR_FEMALE][14] = m_AddonFemaleFPK[ADDONID_GUN_TR_FEMALE][7];
	m_AddonFemaleFPK[ADDONID_GUN_TR_FEMALE][7].Release();

	m_AddonFemaleFPK[ADDONID_GUN_SG_FEMALE][13] = m_AddonFemaleFPK[ADDONID_GUN_SG_FEMALE][8];
	m_AddonFemaleFPK[ADDONID_GUN_SG_FEMALE][8].Release();

	m_AddonFemaleFPK[ADDONID_GUN_AR_FEMALE][8] = m_AddonFemaleFPK[ADDONID_GUN_AR_FEMALE][13];
	m_AddonFemaleFPK[ADDONID_GUN_AR_FEMALE][13].Release();

	m_AddonFemaleFPK[ADDONID_GUN_SMG_FEMALE][7] = m_AddonFemaleFPK[ADDONID_GUN_SMG_FEMALE][14];
	m_AddonFemaleFPK[ADDONID_GUN_SMG_FEMALE][14].Release();
	*/

	//*/

	//m_AddonMaleFPK.InfoToFile("log\\addonMale.txt");
	//m_AddonFemaleFPK.InfoToFile("log\\addonFemale.txt");


	return true;
}

//----------------------------------------------------------------------
// Init Image Frames
//----------------------------------------------------------------------
// CImageFramePack			m_ItemTileFPK;			// For Items on the Tile..
// m_ImageFrame[type]
//----------------------------------------------------------------------
bool
MTopView::InitImageFrames()
{
	//------------------------------------------------------------
	//
	//  Creates the Item on Tile information.
	//
	//------------------------------------------------------------
	/*
	m_ItemTileFPK.Init( 28 );

	// Item 0	
	for (int i=0; i<28; i++)
		m_ItemTileFPK[i].Set(i, 10, 10);

	std::ofstream packFile(FILE_IFRAME_ITEMTILE, ios::binary);
	std::ofstream indexFile(FILE_IFRAMEINDEX_ITEMTILE, ios::binary);

	m_ItemTileFPK.SaveToFile(packFile, indexFile);

	packFile.close();
	indexFile.close();
	*/

	///*
	// Load from File
	std::ifstream file2;//(FILE_IFRAME_ITEMTILE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_IFRAME_ITEMTILE").c_str(), file2))
		return false;
	m_ItemTileFPK.LoadFromFile(file2);
	file2.close();
	//*/



	return true;
}

//----------------------------------------------------------------------
// Init Animation Frames
//----------------------------------------------------------------------
// m_ImageObjectFPK[type][Frame]
//----------------------------------------------------------------------
bool
MTopView::InitAnimationFrames()
{		
	//------------------------------------------------------------
	//
	//  Creates the Animation Frame information.
	//
	//------------------------------------------------------------

	//------------------------------------------------------------
	//
	//  This information is a FrameSet.
	//
	//  It must hold the SpriteID within the SpriteSet.
	//
	//------------------------------------------------------------

	std::ifstream file2;//(FILE_AFRAME_ANIMATIONOBJECT, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_AFRAME_ANIMATIONOBJECT").c_str(), file2))
		return false;
	m_ImageObjectFPK.LoadFromFile(file2);
	file2.close();

	std::ifstream file3;//(FILE_AFRAME_ANIMATIONOBJECT, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_AFRAME_ANIMATIONOBJECT_SHADOW").c_str(), file3))
		return false;
	m_ImageObjectShadowFPK.LoadFromFile(file3);
	file3.close();	

//#ifdef OUTPUT_DEBUG
//	m_InteractionObjectFPK.Init( 1 );
//	
//	//------------------------------------------
//	// First Animation Object
//	//------------------------------------------
//	m_InteractionObjectFPK[0].Init(10);	
//	m_InteractionObjectFPK[0][0].Set(0, 0, -48);
//	m_InteractionObjectFPK[0][1].Set(1, 0, -48);
//	m_InteractionObjectFPK[0][2].Set(2, 0, -48);
//	m_InteractionObjectFPK[0][3].Set(3, 0, -48);
//	m_InteractionObjectFPK[0][4].Set(4, 0, -48);
//	m_InteractionObjectFPK[0][5].Set(4, 0, -48);
//	m_InteractionObjectFPK[0][6].Set(3, 0, -48);
//	m_InteractionObjectFPK[0][7].Set(2, 0, -48);
//	m_InteractionObjectFPK[0][8].Set(1, 0, -48);
//	m_InteractionObjectFPK[0][9].Set(0, 0, -48);
//	
//

	///*
	//*/

	//------------------------------------------------------------
	//
	// Item Drop
	//
	//------------------------------------------------------------	
	std::ifstream itemdropfile2;//(FILE_AFRAME_ANIMATIONOBJECT, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_AFRAME_ITEMDROP").c_str(), itemdropfile2))
		return false;
	m_ItemDropFPK.LoadFromFile(itemdropfile2);
	itemdropfile2.close();

//			int spriteID = Frame.GetSpriteID()+(kkk-802)*4;
//			Frame.Set(spriteID, Frame.GetCX(), Frame.GetCY());
//		}
//

	//------------------------------------------------------------	
	// Create again
	//------------------------------------------------------------	
	/*
	const int numItems	= 121;
	const int numFrames	= 6;

	// frame change value
	// There are 4 sprites per item but 6 frames must be made.
	int nFrame[numFrames] = { 3, 0, 1, 2, 3, 0 };

	m_ItemDropFPK.Init( numItems );	// ani frames for the total number of items

	int spriteID = 0;
	for (int i=0; i<numItems; i++)
	{
		m_ItemDropFPK[i].Init( numFrames );		// 6 frames each

		for (int f=0; f<numFrames; f++)
		{
			int sid = spriteID+nFrame[f];

			// Correct the coordinates so it is centered..
			int cx = 24 - (m_ItemDropSPK[sid].GetWidth()>>1);
			int cy = 24 - (m_ItemDropSPK[sid].GetHeight()>>1);

			m_ItemDropFPK[i][f].Set( sid, cx, cy );
		}

		spriteID += 4;
	}

	//------------------------------------------------------------	
	// Save
	//------------------------------------------------------------	
	std::ofstream packFile(FILE_AFRAME_ITEMDROP, ios::binary);
	std::ofstream indexFile(FILE_AFRAMEINDEX_ITEMDROP, ios::binary);
	m_ItemDropFPK.SaveToFile(packFile, indexFile);
	packFile.close();
	indexFile.close();
	*/

		//------------------------------------------------------------
	//
	//  Creates the Item Broken information.
	//
	//------------------------------------------------------------
	///*
#if defined(OUTPUT_DEBUG) && defined(_DEBUG)
	const int maxSlayerItemBroken = (MSlayerGear::MAX_GEAR_SLAYER+2-12) * 3;
	const int maxVampireItemBroken = (MVampireGear::MAX_GEAR_VAMPIRE+2-12) * 3;
	const int maxOustersItemBroken = (MOustersGear::MAX_GEAR_OUSTERS+1-12) * 3;

	m_ItemBrokenFPK.Init( 5 );	// Slayer male, female + Vampire male, female


	// { SpriteID, CX, CY }
	//------------------------------------------------------------
	// Slayer male
	//------------------------------------------------------------
	int slayerMale[maxSlayerItemBroken][3] =
	{
		// The normal ones
		{ SPRITEID_NULL,	33, 1 },	// hat,
		{ SPRITEID_NULL,	 38, 18 },	// necklace
		{ SPRITEID_NULL,	16, 19 },	// top
		{ 1,	62, 19 },	// shield
		{ 2,	19, 11 },	// sword
		{ SPRITEID_NULL,	9, 49 },	// gloves
		{ SPRITEID_NULL,	27, 54 },	// belt
		{ SPRITEID_NULL,	26, 59 },	// bottoms
		{ 5,	59, 51 },	// bracelet2
		{ 5,	24, 51 },	// bracelet1
		{ 6,	20, 60 },	// ring1
		{ 6,	28, 60 },	// ring2
		{ 6,	55, 60 },	// ring3
		{ 6,	63, 60 },	// ring4
		{ SPRITEID_NULL,	20, 122 },	// shoes
		{ 3,	18, 12 },	// cross
		{ 4,	20, 12 },	// gun

		// The slightly broken ones
		{ 9,	40, 10 },	// hat,
		{ 10,	42, 19 },	// necklace
		{ 7,	33, 18 },	// top
		{ 14,	62, 19 },	// shield
		{ 15,	19, 11 },	// sword
		{ 11,	30, 35 },	// gloves
		{ 12,	37, 36 },	// belt
		{ 8,	36, 35 },	// bottoms
		{ 18,	59, 51 },	// bracelet2
		{ 18,	24, 51 },	// bracelet1
		{ 19,	20, 60 },	// ring1
		{ 19,	28, 60 },	// ring2
		{ 19,	55, 60 },	// ring3
		{ 19,	63, 60 },	// ring4
		{ 13,	36, 72 },	// shoes
		{ 16,	18, 12 },	// cross
		{ 17,	20, 12 },	// gun

		// The almost completely broken ones
		{ 22,	40, 10 },	// hat,
		{ 23,	42, 19 },	// necklace
		{ 20,	33, 18 },	// top
		{ 27,	62, 19 },	// shield
		{ 28,	19, 11 },	// sword
		{ 24,	30, 35 },	// gloves
		{ 25,	37, 36 },	// belt
		{ 21,	36, 35 },	// bottoms
		{ 31,	59, 51 },	// bracelet2
		{ 31,	24, 51 },	// bracelet1
		{ 32,	20, 60 },	// ring1
		{ 32,	28, 60 },	// ring2
		{ 32,	55, 60 },	// ring3
		{ 32,	63, 60 },	// ring4
		{ 26,	36, 72 },	// shoes
		{ 29,	18, 12 },	// cross
		{ 30,	20, 12 },	// gun
	};

	//------------------------------------------------------------
	// Slayer female
	//------------------------------------------------------------
	int slayerFemale[maxSlayerItemBroken][3] =
	{
		// The normal ones
		{ SPRITEID_NULL,	33, 1 },	// hat,
		{ SPRITEID_NULL,	 38, 18 },	// necklace
		{ SPRITEID_NULL,	16, 19 },	// top
		{ 1,	62, 19 },	// shield
		{ 2,	19, 11 },	// sword
		{ SPRITEID_NULL,	9, 49 },	// gloves
		{ SPRITEID_NULL,	27, 54 },	// belt
		{ SPRITEID_NULL,	26, 59 },	// bottoms
		{ 5,	59, 51 },	// bracelet2
		{ 5,	24, 51 },	// bracelet1
		{ 6,	20, 60 },	// ring1
		{ 6,	28, 60 },	// ring2
		{ 6,	55, 60 },	// ring3
		{ 6,	63, 60 },	// ring4
		{ SPRITEID_NULL,	20, 122 },	// shoes
		{ 3,	18, 12 },	// cross
		{ 4,	20, 12 },	// gun

		// The slightly broken ones
		{ 36,	40, 10 },	// hat,
		{ 37,	41, 20 },	// necklace
		{ 34,	33, 20 },	// top
		{ 14,	62, 19 },	// shield
		{ 15,	19, 11 },	// sword
		{ 38,	30, 37 },	// gloves
		{ 39,	40, 32 },	// belt
		{ 35,	38, 35 },	// bottoms
		{ 18,	59, 51 },	// bracelet2
		{ 18,	24, 51 },	// bracelet1
		{ 19,	20, 60 },	// ring1
		{ 19,	28, 60 },	// ring2
		{ 19,	55, 60 },	// ring3
		{ 19,	63, 60 },	// ring4
		{ 40,	39, 55 },	// shoes
		{ 16,	18, 12 },	// cross
		{ 17,	20, 12 },	// gun

		// The almost completely broken ones
		{ 43,	40, 10 },	// hat,
		{ 44,	41, 20 },	// necklace
		{ 41,	33, 20 },	// top
		{ 14,	62, 19 },	// shield
		{ 15,	19, 11 },	// sword
		{ 45,	30, 37 },	// gloves
		{ 46,	40, 32 },	// belt
		{ 42,	38, 35 },	// bottoms
		{ 18,	59, 51 },	// bracelet2
		{ 18,	24, 51 },	// bracelet1
		{ 19,	20, 60 },	// ring1
		{ 19,	28, 60 },	// ring2
		{ 19,	55, 60 },	// ring3
		{ 19,	63, 60 },	// ring4
		{ 47,	39, 55 },	// shoes
		{ 16,	18, 12 },	// cross
		{ 17,	20, 12 },	// gun
	};

	//------------------------------------------------------------
	// Vampire male
	//------------------------------------------------------------
	int vampireMale[maxVampireItemBroken][3] =
	{
		// The normal ones
		{ SPRITEID_NULL,	40, 22 },	// necklace
		{ SPRITEID_NULL,	19, 10 },	// top
		{ 49,	22, 45 },	// bracelet1
		{ 49,	62, 45 },	// bracelet2
		{ 50,	18, 54 },	// ring1
		{ 50,	26, 54 },	// ring2
		{ 50,	58, 54 },	// ring3
		{ 50,	66, 54 },	// ring4
		{ 62,	22, 18 },	// earring1
		{ 62,	62, 18 },	// earring2
		{ 65,	16, 22 },	// Weapon1
		{ 65,	64, 22 },	// Weapon2
		{ 68,	18, 63 },	// Amulet1
		{ 68,	26, 63 },	// Amulet2
		{ 68,	58, 63 },	// Amulet3
		{ 68,	66, 63 },	// Amulet4

		// The slightly broken ones
		{ 52,	43, 18 },	// necklace
		{ 51,	30, 14 },	// top
		{ 53,	22, 45 },	// bracelet1
		{ 53,	62, 45 },	// bracelet2
		{ 54,	18, 54 },	// ring1
		{ 54,	26, 54 },	// ring2
		{ 54,	58, 54 },	// ring3
		{ 54,	66, 54 },	// ring4
		{ 63,	22, 18 },	// earring1
		{ 63,	62, 18 },	// earring2
		{ 66,	16, 22 },	// Weapon1
		{ 66,	64, 22 },	// Weapon2
		{ 69,	18, 63 },	// Amulet1
		{ 69,	26, 63 },	// Amulet2
		{ 69,	58, 63 },	// Amulet3
		{ 69,	66, 63 },	// Amulet4

		// The completely broken ones
		{ 56,	43, 18 },	// necklace
		{ 55,	30, 14 },	// top
		{ 57,	22, 45 },	// bracelet1
		{ 57,	62, 45 },	// bracelet2
		{ 58,	18, 54 },	// ring1
		{ 58,	26, 54 },	// ring2
		{ 58,	58, 54 },	// ring3
		{ 58,	66, 54 },	// ring4
		{ 64,	22, 18 },	// earring1
		{ 64,	62, 18 },	// earring2
		{ 67,	16, 22 },	// Weapon1
		{ 67,	64, 22 },	// Weapon2
		{ 70,	18, 63 },	// Amulet1
		{ 70,	26, 63 },	// Amulet2
		{ 70,	58, 63 },	// Amulet3
		{ 70,	66, 63 },	// Amulet4
	};

	//------------------------------------------------------------
	// Vampire female
	//------------------------------------------------------------
	int vampireFemale[maxVampireItemBroken][3] =
	{
		// The normal ones
		{ SPRITEID_NULL,	40, 22 },	// necklace
		{ SPRITEID_NULL,	19, 10 },	// top
		{ 49,	22, 45 },	// bracelet1
		{ 49,	62, 45 },	// bracelet2
		{ 50,	18, 54 },	// ring1
		{ 50,	58, 54 },	// ring3
		{ 50,	26, 54 },	// ring2
		{ 50,	66, 54 },	// ring4
		{ 62,	22, 18 },	// earring1
		{ 62,	62, 18 },	// earring2
		{ 65,	16, 22 },	// Weapon1
		{ 65,	64, 22 },	// Weapon2
		{ 68,	18, 63 },	// Amulet1
		{ 68,	26, 63 },	// Amulet2
		{ 68,	58, 63 },	// Amulet3
		{ 68,	66, 63 },	// Amulet4

		// The slightly broken ones
		{ 52,	42, 18 },	// necklace
		{ 60,	31, 19 },	// top
		{ 53,	22, 45 },	// bracelet1
		{ 53,	62, 45 },	// bracelet2
		{ 54,	18, 54 },	// ring1
		{ 54,	58, 54 },	// ring3
		{ 54,	26, 54 },	// ring2
		{ 54,	66, 54 },	// ring4
		{ 63,	22, 18 },	// earring1
		{ 63,	62, 18 },	// earring2
		{ 66,	16, 22 },	// Weapon1
		{ 66,	64, 22 },	// Weapon2
		{ 69,	18, 63 },	// Amulet1
		{ 69,	26, 63 },	// Amulet2
		{ 69,	58, 63 },	// Amulet3
		{ 69,	66, 63 },	// Amulet4

		// The completely broken ones
		{ 56,	42, 18 },	// necklace
		{ 61,	31, 19 },	// top
		{ 57,	22, 45 },	// bracelet1
		{ 57,	62, 45 },	// bracelet2
		{ 58,	18, 54 },	// ring1
		{ 58,	58, 54 },	// ring3
		{ 58,	26, 54 },	// ring2
		{ 58,	66, 54 },	// ring4
		{ 64,	22, 18 },	// earring1
		{ 64,	62, 18 },	// earring2
		{ 67,	16, 22 },	// Weapon1
		{ 67,	64, 22 },	// Weapon2
		{ 70,	18, 63 },	// Amulet1
		{ 70,	26, 63 },	// Amulet2
		{ 70,	58, 63 },	// Amulet3
		{ 70,	66, 63 },	// Amulet4
	};		

	//------------------------------------------------------------
	// Ousters
	//------------------------------------------------------------
	int Ousters[maxOustersItemBroken][3] =
	{
		// The normal ones
		{ 77,	39, 8 },				// circlet
		{ SPRITEID_NULL ,	34, 18 },				// clothes
		{ 89,	17, 33 },				// left hand				// wristlet is 92
		{ 89,	61, 33 },				// right hand
		{ SPRITEID_NULL,	40, 69 },				// shoes
		{ 74,	19, 18},				// armsband1
		{ 74,	57, 18},				// armsband2
		{ 83,	19, 63 },				// ring1
		{ 83,	65, 63 },				// ring2
		{ 86,	35, 22 },				// necklace1
		{ 86,	42, 22 },				// necklace2
		{ 86,	49, 22 },				// necklace3
		{ 80,	26, 63 },				// elemental stone1
		{ 80,	58, 63 },				// elemental stone2
		{ 80,	26, 71 },				// elemental stone3
		{ 80,	58, 71 },				// elemental stone 4
		{ 92,	61, 33 },				// wristlet

		// Medium
		{ 78,	40, 22 },				// circlet
		{ 72,	19, 10 },	// clothes
		{ 90,	22, 45 },				// left hand				// wristlet is 92
		{ 90,	62, 45 },				// right hand
		{ 95,	18, 54 },				// shoes
		{ 75,	26, 54 },				// armsband1
		{ 75,	58, 54 },				// armsband2
		{ 84,	66, 54 },				// ring1
		{ 84,	22, 18 },				// ring2
		{ 87,	62, 18 },				// necklace1
		{ 87,	16, 22 },				// necklace2
		{ 87,	16, 22 },				// necklace3

		{ 81,	26, 63 },				// elemental stone1
		{ 81,	58, 63 },				// elemental stone2
		{ 81,	26, 71 },				// elemental stone3
		{ 81,	58, 71 },				// elemental stone 4

		{ 93,	61, 33 },				// wristlet

		// The bad ones
		{ 79,	40, 22 },				// circlet
		{ 73,	19, 10 },	// clothes
		{ 91,	22, 45 },				// left hand				// wristlet is 92
		{ 91,	62, 45 },				// right hand
		{ 96,	18, 54 },				// shoes
		{ 76,	26, 54 },				// armsband1
		{ 76,	58, 54 },				// armsband2
		{ 85,	66, 54 },				// ring1
		{ 85,	22, 18 },				// ring2
		{ 88,	62, 18 },				// necklace1
		{ 88,	16, 22 },				// necklace2
		{ 88,	16, 22 },				// necklace3
		{ 82,	26, 63 },				// elemental stone1
		{ 82,	58, 63 },				// elemental stone2
		{ 82,	26, 71 },				// elemental stone3
		{ 82,	58, 71 },				// elemental stone 4
		{ 94,	61, 33 },				// wristlet
	};		


	int j;	

	m_ItemBrokenFPK[0].Init( maxSlayerItemBroken );
	m_ItemBrokenFPK[1].Init( maxSlayerItemBroken );
	m_ItemBrokenFPK[2].Init( maxVampireItemBroken );
	m_ItemBrokenFPK[3].Init( maxVampireItemBroken );
	m_ItemBrokenFPK[4].Init( maxOustersItemBroken );

	for (j=0; j<maxSlayerItemBroken; j++)
	{		
		m_ItemBrokenFPK[0][j].Set(slayerMale[j][0], slayerMale[j][1]-30, slayerMale[j][2]-10);			
	}

	for (j=0; j<maxSlayerItemBroken; j++)
	{		
		m_ItemBrokenFPK[1][j].Set(slayerFemale[j][0], slayerFemale[j][1]-30, slayerFemale[j][2]-10);			
	}

	for (j=0; j<maxVampireItemBroken; j++)
	{		
		m_ItemBrokenFPK[2][j].Set(vampireMale[j][0], vampireMale[j][1]-30, vampireMale[j][2]-10);			
	}

	for (j=0; j<maxVampireItemBroken; j++)
	{		
		m_ItemBrokenFPK[3][j].Set(vampireFemale[j][0], vampireFemale[j][1]-30, vampireFemale[j][2]-10);			
	}

	for (j=0; j<maxOustersItemBroken; j++)
	{
		m_ItemBrokenFPK[4][j].Set(Ousters[j][0], Ousters[j%(maxOustersItemBroken/3)][1]-30, Ousters[j%(maxOustersItemBroken/3)][2]-10);
	}

	std::ofstream packFile(g_pFileDef->getProperty("FILE_AFRAME_ITEMBROKEN").c_str(), ios::binary);
	std::ofstream indexFile(g_pFileDef->getProperty("FILE_AFRAMEINDEX_ITEMBROKEN").c_str(), ios::binary);

	m_ItemBrokenFPK.SaveToFile(packFile, indexFile);

	packFile.close();
	indexFile.close();
#endif
	//*/

	///*
	// Load from File
	std::ifstream fileItemBroken2;//(FILE_IFRAME_ITEMTILE, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_AFRAME_ITEMBROKEN").c_str(), fileItemBroken2))
		return false;
	m_ItemBrokenFPK.LoadFromFile(fileItemBroken2);
	fileItemBroken2.close();
	//*/


	return true;
}

//----------------------------------------------------------------------
// Init Effect Frames
//----------------------------------------------------------------------
// m_EffectAlphaFPK[type][Direction][Frame]
//----------------------------------------------------------------------
bool
MTopView::InitEffectFrames()
{
	//------------------------------------------------
	//
	//   Effect NormalSPK
	//
	//------------------------------------------------	
	// No. 0: HolyWater flying effect
	// No. 1: Bomb flying effect
	/*
	m_EffectNormalFPK.Init( 2 );

	for (int e=0; e<2; e++)
	{
		m_EffectNormalFPK[e].Init( 8 );

		for (int d=0; d<8; d++)
		{
			m_EffectNormalFPK[e][d].Init( 4 );	// 4 frames each

			for (int f=0; f<4; f++)
			{
				int spriteID = e*4 + f;
				int cx = -(m_EffectNormalSPK[spriteID].GetWidth()>>1);
				int cy = -(m_EffectNormalSPK[spriteID].GetHeight()>>1);

				m_EffectNormalFPK[e][d][f].Set(spriteID, cx, cy, 0);
			}
		}	
	}

	std::ofstream packNormalFile(FILE_EFRAME_NORMALEFFECT, ios::binary);
	std::ofstream indexNormalFile(FILE_EFRAMEINDEX_NORMALEFFECT, ios::binary);
	m_EffectNormalFPK.SaveToFile(packNormalFile, indexNormalFile);
	packNormalFile.close();
	indexNormalFile.close();
	*/
	/*
	m_EffectNormalFPK.Init(MAX_NORMALEFFECT);

	//-------------------
	// First Effect
	//-------------------
	// There are 8 directions.
	m_EffectNormalFPK[0].Init(8);		// Bomb

	for (int d=0; d<8; d++)
	{		
		// Bomb
		m_EffectNormalFPK[0][d].Init(5);
		m_EffectNormalFPK[0][d][0].Set(0, 0, -48);
		m_EffectNormalFPK[0][d][1].Set(1, 0, -48);
		m_EffectNormalFPK[0][d][2].Set(2, 0, -48);
		m_EffectNormalFPK[0][d][3].Set(1, 0, -48);
		m_EffectNormalFPK[0][d][4].Set(0, 0, -48);
	}

	std::ofstream packNormalFile(FILE_EFRAME_NORMALEFFECT, ios::binary);
	std::ofstream indexNormalFile(FILE_EFRAMEINDEX_NORMALEFFECT, ios::binary);
	m_EffectNormalFPK.SaveToFile(packNormalFile, indexNormalFile);
	packNormalFile.close();
	indexNormalFile.close();
	*/

	//--------------------------------------------------
	// Read a single FRR..
	// and convert it to an EFRR..
	// to create the EFPK.
	//--------------------------------------------------
	// [ TEST CODE ]
	/*
	std::ifstream fileFA;//(FILE_EFRAME_NORMALEFFECT, ios::binary);
	if (!FileOpenBinary("Data\\bomb.frr", fileFA))
		return false;

	FRAME_ARRAY FA;
	FA.LoadFromFile( fileFA );
	fileFA.close();

	m_EffectNormalFPK.Init( 1 );
	// FRR --> EFRR
	m_EffectNormalFPK[0].Init( 8 );
	for (int d=0; d<8; d++)
	{
		m_EffectNormalFPK[0][d].Init( FA.GetSize() );
		for (int f=0; f<FA.GetSize(); f++)
		{
			CFrame& frame = FA[f];
			CEffectFrame& eframe = m_EffectNormalFPK[0][d][f];

			eframe.Set( frame.GetSpriteID(), frame.GetCX(), frame.GetCY(), 0);
		}
	}

	std::ofstream packNormalFile(FILE_EFRAME_NORMALEFFECT, ios::binary);
	std::ofstream indexNormalFile(FILE_EFRAMEINDEX_NORMALEFFECT, ios::binary);
	m_EffectNormalFPK.SaveToFile(packNormalFile, indexNormalFile);
	packNormalFile.close();
	indexNormalFile.close();
	*/

	///*
	// Load from NormalFile
	std::ifstream NormalFile2;//(FILE_EFRAME_NORMALEFFECT, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_EFRAME_NORMALEFFECT").c_str(), NormalFile2))
		return false;
	m_EffectNormalFPK.LoadFromFile(NormalFile2);
	NormalFile2.close();
	//*/

	// Print the details
	/*
	std::ofstream infoFile("Log\\EffectNormal.txt");	

	for (int type=0; type<m_EffectNormalFPK.GetSize(); type++)
	{
		for (int dir=0; dir<m_EffectNormalFPK[type].GetSize(); dir++)
		{
			for (int frame=0; frame<m_EffectNormalFPK[type][dir].GetSize(); frame++)
			{
				int spriteID = m_EffectNormalFPK[type][dir][frame].GetSpriteID();
				int cx = m_EffectNormalFPK[type][dir][frame].GetCX();
				int cy = m_EffectNormalFPK[type][dir][frame].GetCY();
				infoFile << "[" << type << "][" << dir << "][" << frame << "] = ( " << spriteID << ",  " << cx << ", " << cy << " ) " << endl;
			}
		}	
	}
	infoFile.close();
	*/

	//------------------------------------------------
	//
	//   Effect AlphaSPK
	//
	//------------------------------------------------
	/*
	m_EffectAlphaFPK.Init(MAX_ALPHAEFFECT);

	//-------------------
	// First Effect
	//-------------------
	// There are 8 directions.
	m_EffectAlphaFPK[0].Init(8);		// Fire	
	m_EffectAlphaFPK[1].Init(8);		// Lightning
	m_EffectAlphaFPK[2].Init(8);		// Light
	m_EffectAlphaFPK[3].Init(8);		// Darkness
	m_EffectAlphaFPK[4].Init(8);		// Aura(Blue)
	m_EffectAlphaFPK[5].Init(8);		// Aura(Red)
	m_EffectAlphaFPK[6].Init(8);		// Sword	
	m_EffectAlphaFPK[7].Init(8);		// Comet
	m_EffectAlphaFPK[8].Init(8);		// Star
	m_EffectAlphaFPK[9].Init(8);		// Flare

	for (d=0; d<8; d++)
	{
		// 8 Frames for each of those 8 directions
		m_EffectAlphaFPK[0][d].Init(8);
		m_EffectAlphaFPK[1][d].Init(8);

		m_EffectAlphaFPK[0][d][0].Set(0, -35,-120, 1);		// Fire
		m_EffectAlphaFPK[0][d][1].Set(1, -35,-120, 1);		// Fire
		m_EffectAlphaFPK[0][d][2].Set(2, -35,-120, 1);		// Fire
		m_EffectAlphaFPK[0][d][3].Set(3, -35,-120, 2);		// Fire
		m_EffectAlphaFPK[0][d][4].Set(4, -35,-120, 2);		// Fire
		m_EffectAlphaFPK[0][d][5].Set(5, -35,-120, 1);		// Fire
		m_EffectAlphaFPK[0][d][6].Set(6, -35,-120, 1);		// Fire
		m_EffectAlphaFPK[0][d][7].Set(7, -35-120, 1);		// Fire


		for (int f=0; f<8; f++)
		{					
			m_EffectAlphaFPK[1][d][f].Set(f,8, 0,-64, 1);	// Lightning
		}

		// Light
		m_EffectAlphaFPK[2][d].Init(2);
		m_EffectAlphaFPK[2][d][0].Set(16, 0, -48, 1);
		m_EffectAlphaFPK[2][d][1].Set(17, 0, -48, 1);
		//m_EffectAlphaFPK[2][d][1].Set(17, 0, -50);
		//m_EffectAlphaFPK[2][d][2].Set(16, 0, -54);
		//m_EffectAlphaFPK[2][d][3].Set(17, 0, -60);
		//m_EffectAlphaFPK[2][d][4].Set(16, 0, -62);
		//m_EffectAlphaFPK[2][d][5].Set(17, 0, -58);
		//m_EffectAlphaFPK[2][d][6].Set(16, 0, -50);
		//m_EffectAlphaFPK[2][d][7].Set(17, 0, -48);

		// Darkness
		m_EffectAlphaFPK[3][d].Init(2);
		m_EffectAlphaFPK[3][d][0].Set(18, 0, -48, 0);
		m_EffectAlphaFPK[3][d][1].Set(19, 0, -48, 0);

		// Aura(Blue)
		m_EffectAlphaFPK[4][d].Init(5);
		m_EffectAlphaFPK[4][d][0].Set(20, -60, -80, 2);
		m_EffectAlphaFPK[4][d][1].Set(22, -60, -80, 2);
		m_EffectAlphaFPK[4][d][2].Set(21, -60, -80, 2);
		m_EffectAlphaFPK[4][d][3].Set(22, -60, -80, 2);
		m_EffectAlphaFPK[4][d][4].Set(20, -60, -80, 2);

		// Aura(Red)
		m_EffectAlphaFPK[5][d].Init(5);
		m_EffectAlphaFPK[5][d][0].Set(23, -60, -80, 2);
		m_EffectAlphaFPK[5][d][1].Set(25, -60, -80, 2);
		m_EffectAlphaFPK[5][d][2].Set(24, -60, -80, 2);
		m_EffectAlphaFPK[5][d][3].Set(25, -60, -80, 2);
		m_EffectAlphaFPK[5][d][4].Set(23, -60, -80, 2);

		// Sword
		m_EffectAlphaFPK[6][d].Init(12);
		for (f=0; f<12; f++)
		{
			m_EffectAlphaFPK[6][d][f].Set(26,f, 0, -48, 1);
		}

		// Comet
		m_EffectAlphaFPK[7][d].Init(7);
		for (f=0; f<7; f++)
		{
			m_EffectAlphaFPK[7][d][f].Set(38,f, -20, -160, 2);
		}

		// Star
		m_EffectAlphaFPK[8][d].Init(24);
		for (f=0; f<24; f++)
		{
			m_EffectAlphaFPK[8][d][f].Set(45,(f>>1), -50, -70, 1);
		}		

		// Flare
		m_EffectAlphaFPK[9][d].Init(10);		
		m_EffectAlphaFPK[9][d][0].Set(57, -21, -105, 4);
		m_EffectAlphaFPK[9][d][1].Set(58, -21, -106, 4);
		m_EffectAlphaFPK[9][d][2].Set(59, -21, -108, 4);
		m_EffectAlphaFPK[9][d][3].Set(60, -21, -112, 4);
		m_EffectAlphaFPK[9][d][4].Set(61, -21, -115, 5);
		m_EffectAlphaFPK[9][d][5].Set(62, -21, -112, 4);
		m_EffectAlphaFPK[9][d][6].Set(63, -21, -108, 4);
		m_EffectAlphaFPK[9][d][7].Set(64, -21, -106, 4);
		m_EffectAlphaFPK[9][d][8].Set(65, -21, -105, 4);
		m_EffectAlphaFPK[9][d][9].Set(66, -21, -104, 4);
	}

	std::ofstream packAlphaFile(FILE_EFRAME_ALPHAEFFECT, ios::binary);
	std::ofstream indexAlphaFile(FILE_EFRAMEINDEX_ALPHAEFFECT, ios::binary);
	m_EffectAlphaFPK.SaveToFile(packAlphaFile, indexAlphaFile);
	packAlphaFile.close();
	indexAlphaFile.close();
	*/

	///*
	// Load from AlphaFile
	std::ifstream AlphaFile2;//(FILE_EFRAME_ALPHAEFFECT, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_EFRAME_ALPHAEFFECT").c_str(), AlphaFile2))
		return false;
	//std::ifstream AlphaFile2("effect.efpk", ios::binary);
	m_EffectAlphaFPK.LoadFromFile(AlphaFile2);
	AlphaFile2.close();
	//*/

	/*
	// Print the details
	//m_EffectAlphaFPK.InfoToFile("Log\\Effect.txt");
	std::ofstream infoFile("Log\\Effect.txt");	

	for (int type=0; type<m_EffectAlphaFPK.GetSize(); type++)
	{
		for (int dir=0; dir<m_EffectAlphaFPK[type].GetSize(); dir++)
		{
			for (int frame=0; frame<m_EffectAlphaFPK[type][dir].GetSize(); frame++)
			{
				int spriteID = m_EffectAlphaFPK[type][dir][frame].GetSpriteID();
				int cx = m_EffectAlphaFPK[type][dir][frame].GetCX();
				int cy = m_EffectAlphaFPK[type][dir][frame].GetCY();
				infoFile << "[" << type << "][" << dir << "][" << frame << "] = ( " << spriteID << ",  " << cx << ", " << cy << " ) " << endl;
			}
		}	
	}
	infoFile.close();
	*/
	/*
	//---------------------------------------------------------------
	// Print the frame count of each effect
	//---------------------------------------------------------------
	std::ofstream file("log\\EffectList.txt");
	for (int e=0; e<m_EffectAlphaFPK.GetSize(); e++)
	{
		DIRECTION_EFFECTFRAME_ARRAY& DEA = m_EffectAlphaFPK[e];

		file << "[" << e << "] " << (int)DEA[0].GetSize() << endl;
	}
	file.close();
	*/


	//---------------------------------------------------------------
	// Adjust the light
	//---------------------------------------------------------------
	/*
	int light;

	// effect type
	for (int e=0; e<m_EffectAlphaFPK.GetSize(); e++)
	{
		DIRECTION_EFFECTFRAME_ARRAY& effect = m_EffectAlphaFPK[e];

		// per direction
		for (int d=0; d<8; d++)
		{
			EFFECTFRAME_ARRAY& EA = effect[d];

			// per frame
			for (int f=0; f<EA.GetSize(); f++)
			{
				CEffectFrame &frame = EA[f];

				switch (e)
				{
					case EFFECTSPRITETYPE_LIGHT_1 :
						light = 5 + (f & 0x01);
					break;
					case EFFECTSPRITETYPE_LIGHT_2 :
						light = 6 + (f & 0x01);
					break;
					case EFFECTSPRITETYPE_LIGHT_3 :
						light = 4 + (f & 0x01);
					break;
					default : 
						light = 3;
				}

				frame.Set( frame.GetSpriteID(), frame.GetCX(), frame.GetCY(), light);
			}	
		}
	}

	std::ofstream packAlphaFile(FILE_EFRAME_ALPHAEFFECT, ios::binary);
	std::ofstream indexAlphaFile(FILE_EFRAMEINDEX_ALPHAEFFECT, ios::binary);
	m_EffectAlphaFPK.SaveToFile(packAlphaFile, indexAlphaFile);
	packAlphaFile.close();
	indexAlphaFile.close();
	*/

	/*
	//---------------------------------------------------------------
	// Insert duplicate frames of the blood sprite
	//---------------------------------------------------------------
	DIRECTION_EFFECTFRAME_ARRAY& effect = m_EffectAlphaFPK[EFFECTSPRITETYPE_SLAYER_DIE];

	for (int d=0; d<8; d++)
	{
		EFFECTFRAME_ARRAY& EA = effect[d];

		int frameMax = EA.GetSize()*3 + 30;

		EFFECTFRAME_ARRAY newEA;

		newEA.Init( frameMax );

		// All end actions...
		for (int i=0; i<frameMax; i++)
		{
			newEA[i] = EA[EA.GetSize()-1];
		}

		// Two frames each, keep the still frame, one frame reversed
		for (i=0; i<EA.GetSize(); i++)
		{
			newEA[i*2] = EA[i];
			newEA[i*2,1] = EA[i];

			newEA[frameMax-1 - i] = EA[i];
		}	

		EA = newEA;
	}

	std::ofstream packAlphaFile(FILE_EFRAME_ALPHAEFFECT, ios::binary);
	std::ofstream indexAlphaFile(FILE_EFRAMEINDEX_ALPHAEFFECT, ios::binary);
	m_EffectAlphaFPK.SaveToFile(packAlphaFile, indexAlphaFile);
	packAlphaFile.close();
	indexAlphaFile.close();
	*/


//	#ifdef OUTPUT_DEBUG
//
//		m_EffectShadowFPK.Init( 1 );
//
//
//
//		
//

	///*
	// Load from shadowEffectFile
	std::ifstream shadowEffectFile2;
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_EFRAME_SHADOWEFFECT").c_str(), shadowEffectFile2))
		return false;
	m_EffectShadowFPK.LoadFromFile(shadowEffectFile2);
	shadowEffectFile2.close();

	// Load from ScreenFile
	std::ifstream ScreenFile2;//(FILE_EFRAME_ScreenEFFECT, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_EFRAME_SCREENEFFECT").c_str(), ScreenFile2))
		return false;
	//std::ifstream ScreenFile2("effect.efpk", ios::binary);
	m_EffectScreenFPK.LoadFromFile(ScreenFile2);
	ScreenFile2.close();

	return true;
}

//----------------------------------------------------------------------
// Load Minimap
//----------------------------------------------------------------------
void
MTopView::LoadMinimap(const char* filename)//, MZoneInfo* pZoneInfo)
{
	(void)filename;
	/* // Commented out 2001.7.14
	//------------------------------------------------------------
	//
	//				Minimap	
	//
	//------------------------------------------------------------
	//------------------------------------------------------------	
	// Load  Clothes SpritePack	
	//------------------------------------------------------------
	std::ifstream	MapTest1;
	if (!FileOpenBinary(filename, MapTest1))
		return;
	m_pMinimapSPR->LoadFromFile( MapTest1 );
	*/

	//------------------------------------------------------------	
	// If there is a ZoneInfo to display
	//------------------------------------------------------------	
	/*
	if (pZoneInfo!=NULL)
	{
		register int i;
		register int j;

		int spWidth = m_pMinimapSPR->GetWidth();
		int spHeight = m_pMinimapSPR->GetHeight();
		int zoneWidth = pZoneInfo->GetWidth();
		int zoneHeight = pZoneInfo->GetHeight();

		WORD* pPixel;
		int	count, transCount, colorCount;

		int spX, spY, zoneX, zoneY;

		// (256,128) --> (256,256)
		// (spX, spX) ---> (zoneX, zoneY)
		// zoneX = spX * zoneHeight / spHeight
		CSpriteSurface::s_Value1 = 16;
		CSpriteSurface::s_Value2 = 16;
		WORD green = CSDLGraphics::Color(0, 20, 0);

		//----------------------------------------------------------------
		// Changes the Sprite itself. - -;
		//----------------------------------------------------------------
		for (spY=0; spY<spHeight; spY++)
		{
			zoneY = spY * zoneHeight / spHeight;

			spX = 0;
			pPixel = m_pMinimapSPR->GetPixelLine( spY );

			count = *pPixel++;		// repeat count

			//----------------------------------------------------------------
			// Check for one line..
			//----------------------------------------------------------------
			for (i=0; i<count; i++)
			{
				transCount = *pPixel++;
				colorCount = *pPixel++;

				spX += transCount;

				for (j=0; j<colorCount; j++)
				{
					zoneX = spX * zoneWidth / spWidth;

					//---------------------------------------------------------
					// If it is a safe zone, translucent green..
					//---------------------------------------------------------
					if (pZoneInfo->IsSafeSector(zoneX, zoneY))
					{
						// alpha blending one point to green - -;
						CSpriteSurface::memcpyAlpha(pPixel, &green, 1);
					}

					// Next point
					pPixel++;
					spX++;
				}
			}
		}
	}
	*/

	/* // Commented out 2001.7.14
	//------------------------------------------------------------
	// If 3D acceleration...
	//------------------------------------------------------------
	if (true)
	{
		InitMinimapTexture();
	}

	// m_SectorToMinimapWidth - how many pixels wide is one sector?
	// m_SectorToMinimapHeight - how many pixels tall is one sector?
	if (g_pZone->GetWidth()==0 || g_pZone->GetHeight()==0)
	{
		m_SectorToMinimapWidth	= 0;
		m_SectorToMinimapHeight = 0;
	}
	else
	{
		m_SectorToMinimapWidth = (float)m_pMinimapSPR->GetWidth() / (float)g_pZone->GetWidth();
		m_SectorToMinimapHeight = (float)m_pMinimapSPR->GetHeight() / (float)g_pZone->GetHeight();
	}

	MapTest1.close();
	*/
}

//----------------------------------------------------------------------
// Init MinimapTexture
//----------------------------------------------------------------------
//void
//MTopView::InitMinimapTexture(CSpriteSurface *minimap_surface)
//			WORD *pSurface = (WORD*)minimap_surface->GetSurfacePointer();
//			m_pMinimapSPR->SetPixelNoColorkey(pSurface, minimap_surface->GetSurfacePitch(), minimap_surface->GetWidth(), minimap_surface->GetHeight());
//			
//
//	//------------------------------------------------------------
//	// If 3D acceleration...
//	//------------------------------------------------------------
//	if (!true)
//		return;
//
//
//	m_pMinimapTexture = new CSpriteSurface;
//
//		
//		return;
//	}
//
//
//
//	//-----------------------------------------------------------
//	//
//	// Super ultimate dirty test code.. hmm... - -;;
//	//
//	//-----------------------------------------------------------
//	//
//	// Output the minimap to m_pMinimapTexture.
//	//
//	//-----------------------------------------------------------
//	//-----------------------------------------------------------
//	// If only powers of 2 are supported..
//	//-----------------------------------------------------------
//
//
//		//-----------------------------------------------------------
//		// If the Sprite size is larger than the Texture size..
//		//-----------------------------------------------------------
//
//	//-----------------------------------------------------------
//	// If the size doesn't matter
//	//-----------------------------------------------------------
//			
//
//	//---------------------------------------------------
//	// Create the TextureSurface
//	//---------------------------------------------------		
//
////	int i;
//	
//
//	//---------------------------------------------------
//	// Part painted black..
//	//---------------------------------------------------
//	//if (bDifferentSize)
//	{
//		/*
//		// Paint black everything except the area the Sprite occupies~~
//		DWORD width2 = (width - spWidth) << 1;	// *2 
//		pSurface += spWidth;
//
//				pSurface = (WORD*)((BYTE*)pSurface + pitch);
//			} while (--i);
//		}
//
//
//				pSurface = (WORD*)((BYTE*)pSurface + pitch);
//			} while (--i);
//		}
//		*/
//		
//		//---------------------------------------------------
//		// Initialize the Texture Surface
//		//---------------------------------------------------
//		/*// Commented out 2001.7.14
//		WORD *pSurface = (WORD*)m_pMinimapTexture->GetSurfacePointer();
//				//,	*pSurfaceTemp;
//		long pitch	= m_pMinimapTexture->GetSurfacePitch();
//
//		int width2 = width << 1;
//
//			pSurface = (WORD*)((BYTE*)pSurface + pitch);
//		}
//		*/
//		
//	}
//	
//	//---------------------------------------------------
//	// Sprite output
//	//---------------------------------------------------
//	// Taking the size into account..
//
//
//
//	//---------------------------------------------------------------
//	// Size when outputting
//	//---------------------------------------------------------------
//
//
//
//	m_MinimapTextureWidth = width;
//	m_MinimapTextureHeight = height;
//	
//	m_pMinimapTexture->Unlock();
//	
//}

//----------------------------------------------------------------------
// Use HalfFrame
//----------------------------------------------------------------------
/*
void
MTopView::UseHalfFrame(bool bUse)
{
	// Something has to be done about Addon too - -;
	// And Effect too... - -;

	//----------------------------------------------------------
	// Remove the ones already Loaded
	//----------------------------------------------------------
	m_CreatureSPK.Release();
	m_CreatureSSPK.Release();

	if (true)
	{
		m_pCreatureShadowManager->Release();

		m_pCreatureShadowManager->Init( FILE_SSPRITE_CREATURE,
							FILE_SSPRITEINDEX_CREATURE,  
							g_pClientConfig->MAX_TEXTUREPART_CREATURESHADOW);
	}

	//----------------------------------------------------------
	// Mark that nothing is Loaded
	//----------------------------------------------------------
	int num = g_pCreatureSpriteTable->GetSize();

	for (int i=0; i<num; i++)
	{
		(*g_pCreatureSpriteTable)[i].bLoad = false;
	}

	if (bUse)
	{
		// Remove the ones already Loaded
		std::ifstream file(FILE_CFRAME_CREATURE2, ios::binary);
		m_CreatureFPK.LoadFromFile(file);
		file.close();		
	}
	else
	{
		// Remove the ones already Loaded
		std::ifstream file(FILE_CFRAME_CREATURE, ios::binary);
		m_CreatureFPK.LoadFromFile(file);
		file.close();
	}
}
*/

//----------------------------------------------------------------------
// Load From File CreatureSPK
//----------------------------------------------------------------------
// Loads the Sprites for the Creatures that will appear in the current Zone.
//----------------------------------------------------------------------
void
MTopView::LoadFromFileCreatureSPK(int spriteType)
{
	if (spriteType<0 || spriteType >= (*g_pCreatureSpriteTable).GetSize())
	{
		DEBUG_ADD_FORMAT_ERR("[Error] Wrong SpriteType=%d", spriteType);

		return;
	}

	//int spriteType = (*g_pCreatureTable)[n].SpriteType;

	//----------------------------------------------------------------------
	// Check whether it has been Loaded.
	//----------------------------------------------------------------------
	if ((*g_pCreatureSpriteTable)[spriteType].bLoad)
	{
		// If already Loaded
	}
	//----------------------------------------------------------------------
	// If it has to be Loaded
	//----------------------------------------------------------------------
	// Since g_pCreatureActionSpriteTable will be used according to CreatureAction,
	// it does not actually load.  2001.11.21
	//----------------------------------------------------------------------			
	else
	{
		if(g_pUserOption->IsPreLoadMonster)
		{

			// Just Load the Sprites needed in the current Zone.
//			std::ifstream	creatureFile;//(FILE_ISPRITE_CREATURE, ios::binary);
//			std::ifstream	creatureShadowFile;
//
//			//------------------------------------------------------------
//			// sprite load
//			//------------------------------------------------------------
//			if (!FileOpenBinary(FILE_ISPRITE_CREATURE, creatureFile))
//				return;	


			//------------------------------------------------------------
			// When using Half Frame
			//------------------------------------------------------------
			if (0)//g_pUserOption->UseHalfFrame)
			{
				/*
				int frameID = (*g_pCreatureSpriteTable)[spriteType].FrameID;

				//------------------------------------------------------------	
				// Creates the needed EFPK and also obtains the SpriteIDs that were used.
				//------------------------------------------------------------
				COrderedList<int> intList;
				ACTION_FRAME_ARRAY& AFA = CreatureFPK[frameID];
				int numAction = AFA.GetSize();

				// Reversed to improve the efficiency of the orderedList..
				for (int a=AFA.GetSize()-1; a>=0; a--)
				{
					DIRECTION_FRAME_ARRAY& DFA = AFA[a];

					for (int d=DFA.GetSize()-1; d>=0; d--)
					{
						FRAME_ARRAY& EA = DFA[d];

						for (int f=FA.GetSize()-1; f>=0; f--)
						{
							CFrame& Frame = FA[f];

							int spriteID = Frame.GetSpriteID();

							intList.Add( spriteID );
						}
					}
				}

				COrderedList<int>::DATA_LIST::const_iterator iID = intList.GetIterator();
				int numID = intList.GetSize();

				CSpriteFilePositionArray SFPA;	
				SFPA.Init( numID );

				std::ifstream CreaturePackIndexFile(FILE_ISPRITEINDEX_CREATURE, ios::binary);			

				long fp;
				for (int i=0; i<numID; i++)
				{
					int id = *iID

					CreaturePackIndexFile.seekg( 2 + (id<<2));	// 2 + id*4
					CreaturePackIndexFile.read((char*)&fp, 4);


					SFPA[id].SpriteID		= id;
					SFPA[id].FilePosition	= fp;
				}			

				m_CreatureSPK.LoadFromFile(creatureFile, SFPA);

				CreaturePackIndexFile.close();
				*/
			}
			//------------------------------------------------------------
			// Use all Frames
			//------------------------------------------------------------
			else
			{

				TYPE_SPRITEID first = (*g_pCreatureSpriteTable)[spriteType].FirstSpriteID;
				TYPE_SPRITEID last	= (*g_pCreatureSpriteTable)[spriteType].LastSpriteID;

				if (!m_CreatureSPK.LoadFromFilePart(first, last)) {
					LOG_ERROR("Rejected creature preload: type=%d", spriteType);
					return;
				}
				// Load the n-th creature
//				m_CreatureSPK.LoadFromFilePart(creatureFile, 
//												fp,
//												first, 
//												last
//												);

			}

// 			creatureFile.close();

			//------------------------------------------------------------
			// Load shadow
			//------------------------------------------------------------
//			if (!FileOpenBinary(FILE_SSPRITE_CREATURE, creatureShadowFile))
//				return;	


//			if (!true)
			{
				//------------------------------------------------------------
				// Use Half Frame
				//------------------------------------------------------------
				if (g_pUserOption->UseHalfFrame)
				{
				}
				//------------------------------------------------------------
				// Use all Frames
				//------------------------------------------------------------
				else
				{
					TYPE_SPRITEID first = (*g_pCreatureSpriteTable)[spriteType].FirstShadowSpriteID;
					TYPE_SPRITEID last	= (*g_pCreatureSpriteTable)[spriteType].LastShadowSpriteID;

					if (!m_CreatureSSPK.LoadFromFilePart(first, last)) {
						LOG_ERROR("Rejected creature shadow preload: type=%d", spriteType);
						return;
					}
//					m_CreatureSSPK.LoadFromFilePart(creatureShadowFile, 
//													(*g_pCreatureSpriteTable)[spriteType].SpriteShadowFilePosition,
//													first, 
//													last
//													);
				}
			}
		}	

		// Mark it as Loaded.
		if (auto* entry = g_pCreatureSpriteTable->GetMutable(spriteType)) {
			entry->bLoad = TRUE;
		}

		//--------------------------------------------------------
		// Do not add it if it is the Player picture.
		//--------------------------------------------------------
		//if (spriteType!=(*g_pCreatureTable)[CREATURETYPE_SLAYER_MALE].SpriteType
		//	&& spriteType!=(*g_pCreatureTable)[CREATURETYPE_SLAYER_FEMALE].SpriteType
		//	&& spriteType!=(*g_pCreatureTable)[CREATURETYPE_VAMPIRE_MALE].SpriteType
		//	&& spriteType!=(*g_pCreatureTable)[CREATURETYPE_VAMPIRE_FEMALE].SpriteType
		//	&& spriteType!=(*g_pCreatureTable)[CREATURETYPE_WOLF].SpriteType
		//	&& spriteType!=(*g_pCreatureTable)[CREATURETYPE_BAT].SpriteType
		//	&& spriteType!=(*g_pCreatureTable)[CREATURETYPE_SLAYER_OPERATOR].SpriteType
		//	&& spriteType!=(*g_pCreatureTable)[CREATURETYPE_VAMPIRE_OPERATOR].SpriteType)
		if (!(*g_pCreatureSpriteTable)[spriteType].IsPlayerSprite())
		{
			m_listLoadedCreatureSprite.Add( spriteType );

			if ((*g_pCreatureSpriteTable)[spriteType].IsMonsterSprite())
			{
				m_listLoadedMonsterSprite.Add( spriteType );
			}
		}


		/*
		//------------------------------------------------------------
		// Create the shadow Pack
		//------------------------------------------------------------
		m_CreatureSSPK.InitPart(m_CreatureSPK, 
								(*g_pCreatureTable)[n].FirstSpriteID,
								(*g_pCreatureTable)[n].LastSpriteID);

		//------------------------------------------------------------
		//
		// Init shadow TextureSurface
		//
		//------------------------------------------------------------
		if (true)
		{	
			//--------------------------------------------
			// Creates the TexturePack from the AlphaSPK.
			//--------------------------------------------
			if (!m_CreatureTPK.InitPart( m_CreatureSSPK,
										(*g_pCreatureTable)[n].FirstSpriteID,
										(*g_pCreatureTable)[n].LastSpriteID))
			{
				MessageBox(g_hWnd, "Can't Init ShadowTexture!", NULL, MB_OK);
				return;
			}

			//------------------------------------------------------------	
			// Deletes the EffectAlphaSprite from memory.
			// --> Since the TextureSurface will be used.. it is not needed.
			//------------------------------------------------------------		
			m_CreatureSSPK.ReleasePart((*g_pCreatureTable)[n].FirstSpriteID,
										(*g_pCreatureTable)[n].LastSpriteID);
		}
		*/
	}
}

//----------------------------------------------------------------------
// Load From File AddonSPK
//----------------------------------------------------------------------
// Loads the currently visible Addon.
//----------------------------------------------------------------------
//	int frameID = frame*ACTION_MAX_SLAYER + action;
//
//	if (frameID<0 || frameID >= (*g_pAddonSpriteTable).GetSize())
//	{
//		DEBUG_ADD_FORMAT("[Error] Wrong AddonType=%d, %d", frame, action);
//		
//		return;
//	}
//
//	//int spriteType = (*g_pCreatureTable)[n].SpriteType;
//
//	//----------------------------------------------------------------------
//	// Check whether it has been Loaded.
//	//----------------------------------------------------------------------
//	if ((*g_pAddonSpriteTable)[frameID].bLoad)
//	//----------------------------------------------------------------------
//	// If it has to be Loaded
//	//----------------------------------------------------------------------
//	else
//	{		
//		TYPE_SPRITEID first = (*g_pAddonSpriteTable)[frameID].FirstSpriteID;
//		TYPE_SPRITEID last	= (*g_pAddonSpriteTable)[frameID].LastSpriteID;
//		long			fp	= (*g_pAddonSpriteTable)[frameID].SpriteFilePosition;
//
//		DEBUG_ADD_FORMAT("[LoadAddonAction] frameID=%d, action=%d, spr=%d~%d", frameID, action, first, last);
//
//		__BEGIN_PROFILE("LoadAddonAction")
//
//		m_AddonSPK.LoadFromFilePart(first, last);
////		m_AddonSPK.LoadFromFilePart(m_AddonSPKFile, 
////										fp,
////										first, 
////										last
////										);
//
//		__END_PROFILE("LoadAddonAction")
//
//		if (!true)
//		{			
//			TYPE_SPRITEID first = (*g_pAddonSpriteTable)[frameID].FirstShadowSpriteID;
//			TYPE_SPRITEID last	= (*g_pAddonSpriteTable)[frameID].LastShadowSpriteID;
//			long			fp	= (*g_pAddonSpriteTable)[frameID].SpriteShadowFilePosition;
//
//			DEBUG_ADD_FORMAT("[LoadAddonActionShadow] spr=%d~%d", frameID, action, first, last);
//
//			__BEGIN_PROFILE("LoadAddonActionShadow")
//
////			m_AddonSSPK.LoadFromFilePart(m_AddonSSPKFile, 
////											fp,
////											first, 
////											last
////											);			
//			m_AddonSSPK.LoadFromFilePart(first, last);
//
//			__END_PROFILE("LoadAddonActionShadow")
//		}
//
//		(*g_pAddonSpriteTable)[frameID].bLoad = TRUE;
//	}
//}

//----------------------------------------------------------------------
// Load From File CreatureActionSPK
//----------------------------------------------------------------------
// Loads the currently visible CreatureAction.
//----------------------------------------------------------------------
//	int frameID = frame*ACTION_MAX_VAMPIRE + action;
//
//	if (frameID<0 || frameID >= (*g_pCreatureActionSpriteTable).GetSize())
//	{
//		DEBUG_ADD_FORMAT("[Error] Wrong CreatureActionType=%d, %d", frame, action);
//		
//		return;
//	}
//
//	//int spriteType = (*g_pCreatureTable)[n].SpriteType;
//
//	//----------------------------------------------------------------------
//	// Check whether it has been Loaded.
//	//----------------------------------------------------------------------
//	if ((*g_pCreatureActionSpriteTable)[frameID].bLoad)
//	//----------------------------------------------------------------------
//	// If it has to be Loaded
//	//----------------------------------------------------------------------
//	else
//	{		
//		TYPE_SPRITEID first = (*g_pCreatureActionSpriteTable)[frameID].FirstSpriteID;
//		TYPE_SPRITEID last	= (*g_pCreatureActionSpriteTable)[frameID].LastSpriteID;
//		long			fp	= (*g_pCreatureActionSpriteTable)[frameID].SpriteFilePosition;
//
//		DEBUG_ADD_FORMAT("[LoadCreatureAction] frameID=%d, action=%d, spr=%d~%d", frameID, action, first, last);
//
//		__BEGIN_PROFILE("LoadCreatureAction")
//
////		m_CreatureSPK.LoadFromFilePart(m_CreatureSPKFile, 
////										fp,
////										first, 
////										last
////										);
//		m_CreatureSPK.LoadFromFilePart(first, last);
//
//		__END_PROFILE("LoadCreatureAction")
//
//		if (!true)
//		{			
//			TYPE_SPRITEID first = (*g_pCreatureActionSpriteTable)[frameID].FirstShadowSpriteID;
//			TYPE_SPRITEID last	= (*g_pCreatureActionSpriteTable)[frameID].LastShadowSpriteID;
//			long			fp	= (*g_pCreatureActionSpriteTable)[frameID].SpriteShadowFilePosition;
//
//			DEBUG_ADD_FORMAT("[LoadCreatureActionShadow] spr=%d~%d", first, last);
//
//			__BEGIN_PROFILE("LoadCreatureActionShadow")
//
////			m_CreatureSSPK.LoadFromFilePart(m_CreatureSSPKFile, 
////											fp,
////											first, 
////											last
////											);		
//			m_CreatureSSPK.LoadFromFilePart(first, last);
//			
//			__END_PROFILE("LoadCreatureActionShadow")
//		}
//
//		(*g_pCreatureActionSpriteTable)[frameID].bLoad = TRUE;
//
//
//
////----------------------------------------------------------------------
//// Release CreatureActionSPK
////----------------------------------------------------------------------
//	int frameID = frame*ACTION_MAX_VAMPIRE + action;
//
//	if (frameID<0 || frameID >= (*g_pCreatureActionSpriteTable).GetSize())
//	{
//		DEBUG_ADD_FORMAT("[Error] Wrong CreatureActionType=%d, %d", frame, action);
//		
//		return;
//	}
//
//	//int spriteType = (*g_pCreatureTable)[n].SpriteType;
//
//	//----------------------------------------------------------------------
//	// Check whether it has been Loaded.
//	//----------------------------------------------------------------------
//	if ((*g_pCreatureActionSpriteTable)[frameID].bLoad)
//	{
//		TYPE_SPRITEID first = (*g_pCreatureActionSpriteTable)[frameID].FirstSpriteID;
//		TYPE_SPRITEID last	= (*g_pCreatureActionSpriteTable)[frameID].LastSpriteID;
//		
//		m_CreatureSPK.ReleasePart(first, last);
//
//		if (!true)
//		{			
//			TYPE_SPRITEID first = (*g_pCreatureActionSpriteTable)[frameID].FirstShadowSpriteID;
//			TYPE_SPRITEID last	= (*g_pCreatureActionSpriteTable)[frameID].LastShadowSpriteID;
//			
// 			m_CreatureSSPK.ReleasePart(first, last);			
//		}
//
//		(*g_pCreatureActionSpriteTable)[frameID].bLoad = FALSE;
//	}
//}

//----------------------------------------------------------------------
// Release From CreatureSPK
//----------------------------------------------------------------------
// Frees from memory the Creature Sprites currently Loaded.
//----------------------------------------------------------------------
void
MTopView::ReleaseCreatureSPK(int n)
{
//	//--------------------------------------------------------
//	// Do not remove it if it is the Player picture.
//	//--------------------------------------------------------
//
//	int spriteType = (*g_pCreatureTable)[n].SpriteType;
//
//	// Do not remove it if it is the Player picture.
//	if ((*g_pCreatureSpriteTable)[spriteType].IsPlayerSprite())
//
//	//----------------------------------------------------------------------
//	// Check whether it has been Loaded.
//	//----------------------------------------------------------------------
//	if ((*g_pCreatureSpriteTable)[spriteType].bLoad)
//	{
//		/*
//		// If already Loaded
//		// Free the n-th creature's sprite from memory
//		m_CreatureSPK.ReleasePart((*g_pCreatureSpriteTable)[spriteType].FirstSpriteID,
//									(*g_pCreatureSpriteTable)[spriteType].LastSpriteID);
//
//		m_CreatureSSPK.ReleasePart((*g_pCreatureSpriteTable)[spriteType].FirstSpriteID,
//									(*g_pCreatureSpriteTable)[spriteType].LastSpriteID);
//
//		*/
////		int frameID = (*g_pCreatureSpriteTable)[spriteType].FrameID;
////
//			TYPE_SPRITEID first = (*g_pCreatureSpriteTable)[spriteType].FirstSpriteID;
//			TYPE_SPRITEID last	= (*g_pCreatureSpriteTable)[spriteType].LastSpriteID;
//			long			fp	= (*g_pCreatureSpriteTable)[spriteType].SpriteFilePosition;
//			
//			m_CreatureSPK.ReleasePart(first, last);
//// 		}
//
//		// Mark it as not loaded.
//		(*g_pCreatureSpriteTable)[spriteType].bLoad = FALSE;
//
//		m_listLoadedCreatureSprite.Remove( spriteType );
//
//		if ((*g_pCreatureSpriteTable)[spriteType].IsMonsterSprite())
	//--------------------------------------------------------
	// Do not remove it if it is the Player picture.
	//--------------------------------------------------------

	//int spriteType = (*g_pCreatureTable)[n].SpriteTypes[0];
	int spriteTypeCount = (*g_pCreatureTable)[n].SpriteTypes.GetSize();

	for(int FrameIndex = 0;FrameIndex < spriteTypeCount; FrameIndex++ )
	{

		int spriteType = (*g_pCreatureTable)[n].SpriteTypes[FrameIndex];
		// Do not remove it if it is the Player picture.
		if ((*g_pCreatureSpriteTable)[spriteType].IsPlayerSprite())
		{
			return;
		}

		//----------------------------------------------------------------------
		// Check whether it has been Loaded.
		//----------------------------------------------------------------------
		if ((*g_pCreatureSpriteTable)[spriteType].bLoad)
		{
		/*
		// If already Loaded
		// Free the n-th creature's sprite from memory
		m_CreatureSPK.ReleasePart((*g_pCreatureSpriteTable)[spriteType].FirstSpriteID,
		(*g_pCreatureSpriteTable)[spriteType].LastSpriteID);

		  m_CreatureSSPK.ReleasePart((*g_pCreatureSpriteTable)[spriteType].FirstSpriteID,
		  (*g_pCreatureSpriteTable)[spriteType].LastSpriteID);

			*/
			//		int frameID = (*g_pCreatureSpriteTable)[spriteType].FrameID;
			//
			TYPE_SPRITEID first = (*g_pCreatureSpriteTable)[spriteType].FirstSpriteID;
			TYPE_SPRITEID last	= (*g_pCreatureSpriteTable)[spriteType].LastSpriteID;

			m_CreatureSPK.ReleasePart(first, last);
			// 		}

			// Mark it as not loaded.
			if (auto* entry = g_pCreatureSpriteTable->GetMutable(spriteType)) {
				entry->bLoad = FALSE;
			}

			m_listLoadedCreatureSprite.Remove( spriteType );

			if ((*g_pCreatureSpriteTable)[spriteType].IsMonsterSprite())
			{
				m_listLoadedMonsterSprite.Remove( spriteType );
			}
		}
		else
		{
			// If not Loaded
		}




	}
}

//----------------------------------------------------------------------
// Add MonsterSpriteTypes
//----------------------------------------------------------------------
// It will be referenced from Hallu...
//----------------------------------------------------------------------
void		
MTopView::AddMonsterSpriteTypes(const INT_ORDERED_LIST& listUse)
{		
	INT_ORDERED_LIST::DATA_LIST::const_iterator iUse = listUse.GetIterator();

	for (int i=0; i<listUse.GetSize(); i++)
	{
		int useSpriteType = *iUse;

		if ((*g_pCreatureSpriteTable)[useSpriteType].IsMonsterSprite())
		{
			m_listLoadedMonsterSprite.Add( useSpriteType );
		}

		iUse++;
	}
}

//----------------------------------------------------------------------
// Release Useless CreatureSPK Except
//----------------------------------------------------------------------
// The list holds the sprite types of the creatures in use.
//----------------------------------------------------------------------
void
MTopView::ReleaseUselessCreatureSPKExcept(const INT_ORDERED_LIST& listUse)
{
	// There are few mob+NPC sprites for now, so they are not released.
	//return;

	//--------------------------------------------------------
	// Of those that are loaded, leave out the ones in use..
	//--------------------------------------------------------
	m_listLoadedCreatureSprite -= listUse;

	//--------------------------------------------------------
	// Remove the rest.
	//--------------------------------------------------------
	INT_ORDERED_LIST::DATA_LIST::const_iterator iSpriteType = m_listLoadedCreatureSprite.GetIterator();

	for (int i=0; i<m_listLoadedCreatureSprite.GetSize(); i++)
	{
		int spriteType = *iSpriteType;

		//--------------------------------------------------------
		// Do not remove it if it is the Player picture.
		//--------------------------------------------------------
		//if (spriteType==(*g_pCreatureTable)[CREATURETYPE_SLAYER_MALE].SpriteType
		//	|| spriteType==(*g_pCreatureTable)[CREATURETYPE_SLAYER_FEMALE].SpriteType
		//	|| spriteType==(*g_pCreatureTable)[CREATURETYPE_VAMPIRE_MALE].SpriteType
		//	|| spriteType==(*g_pCreatureTable)[CREATURETYPE_VAMPIRE_FEMALE].SpriteType
		//	|| spriteType==(*g_pCreatureTable)[CREATURETYPE_WOLF].SpriteType
		//	|| spriteType==(*g_pCreatureTable)[CREATURETYPE_BAT].SpriteType
		//	|| spriteType==(*g_pCreatureTable)[CREATURETYPE_SLAYER_OPERATOR].SpriteType
		//	|| spriteType==(*g_pCreatureTable)[CREATURETYPE_VAMPIRE_OPERATOR].SpriteType)
		if ((*g_pCreatureSpriteTable)[spriteType].IsPlayerSprite())
		{
			DEBUG_ADD_FORMAT("[ReleaseUselessCreatureSPK] Skip spriteType=%d", spriteType);

			iSpriteType++;
			continue;
		}

		DEBUG_ADD_FORMAT("[ReleaseUselessCreatureSPK] Release spriteType=%d", spriteType);


		if ((*g_pCreatureSpriteTable)[spriteType].bLoad)
		{
			/*
			int first, last;
			// If already Loaded
			// Free the n-th creature's sprite from memory

			//--------------------------------------------------------
			// Remove Creature SPK
			//--------------------------------------------------------
			first = (*g_pCreatureSpriteTable)[spriteType].FirstSpriteID;
			last = (*g_pCreatureSpriteTable)[spriteType].LastSpriteID;

			if (last > first && first!=SPRITEID_NULL && last!=SPRITEID_NULL)
			{
				m_CreatureSPK.ReleasePart(first, last);
			}

			//--------------------------------------------------------
			// Remove Creature SSPK
			//--------------------------------------------------------
			first = (*g_pCreatureSpriteTable)[spriteType].FirstSpriteID;
			last = (*g_pCreatureSpriteTable)[spriteType].LastSpriteID;

			if (last > first && first!=SPRITEID_NULL && last!=SPRITEID_NULL)
			{			
				m_CreatureSSPK.ReleasePart( first, last );
			}
			*/
			// Remove only the actions that were loaded
			// 2001.11.21
			int frameID = (*g_pCreatureSpriteTable)[spriteType].FrameID;

			if (frameID!=FRAMEID_NULL)
			{
				ReleaseCreatureSPK( frameID );
			}

			// Mark it as not loaded.
			if (auto* entry = g_pCreatureSpriteTable->GetMutable(spriteType)) {
				entry->bLoad = FALSE;
			}
		}

		iSpriteType++;
	}

	//--------------------------------------------------------
	// Set the loaded ones again
	//--------------------------------------------------------	
	m_listLoadedCreatureSprite.Release();
	m_listLoadedMonsterSprite.Release();

	INT_ORDERED_LIST::DATA_LIST::const_iterator iUse = listUse.GetIterator();

	int i;
	for (i=0; i<listUse.GetSize(); i++)
	{
		int useSpriteType = *iUse;

		m_listLoadedCreatureSprite.Add( useSpriteType );

		if ((*g_pCreatureSpriteTable)[useSpriteType].IsMonsterSprite())
		{
			m_listLoadedMonsterSprite.Add( useSpriteType );
		}

		iUse++;
	}
}

//----------------------------------------------------------------------
// Load From File  Tile & ImageObject Set
//----------------------------------------------------------------------
// Loads some of the Tiles and ImageObjects.
//----------------------------------------------------------------------
bool		
MTopView::LoadFromFileTileAndImageObjectSet(const CSpriteSetManager &TileSSM, const CSpriteSetManager &ImageObjectSSM)
{
	(void)ImageObjectSSM;
	//--------------------------------------------------------
	//
	// Load some Tiles
	//
	//--------------------------------------------------------
//
//	if (!bLoad)
//		return false;
//
	if (!m_TileSPK.LoadFromFilePart(TileSSM)) return false;

	//--------------------------------------------------------
	//
	// Load some ImageObjects
	//
	//--------------------------------------------------------
	return m_ImageObjectSPK.LoadFromFileRunning(g_pFileDef->getProperty("FILE_SPRITE_IMAGEOBJECT").c_str());
}

//----------------------------------------------------------------------
// Load From File Tile LargeZone
//----------------------------------------------------------------------
// Loads from the File the TileSprites used only in the LargeZone.
//----------------------------------------------------------------------
bool
MTopView::LoadFromFileTileSPKLargeZone(std::ifstream & file)
{
	//------------------------------------------------------------
	// Loads from the File the information about the TileSprites.
	//------------------------------------------------------------
	CSpriteFilePositionArray* pOldTileSFPA = m_pTileSFPArrayLargeZone;

	m_pTileSFPArrayLargeZone = new CSpriteFilePositionArray;

	m_pTileSFPArrayLargeZone->LoadFromFile( file );

	//------------------------------------------------------------
	// Remove what is not needed
	//------------------------------------------------------------
	if (pOldTileSFPA!=NULL)
	{
		DEBUG_ADD( "Remove Old-Tile SFPArray LargeZone");

		COrderedList<TYPE_SPRITEID>	oldTileID;
		COrderedList<TYPE_SPRITEID>	newTileID;

		int i;

		DEBUG_ADD( "Sort Part1");

		// Sort
		for (i=pOldTileSFPA->GetSize()-1; i>=0; i--)
		{
			oldTileID.Add( (*pOldTileSFPA)[i].SpriteID );
		}

		DEBUG_ADD( "Sort Part2");

		// Sort
		for (i=m_pTileSFPArrayLargeZone->GetSize()-1; i>=0; i--)
		{
			newTileID.Add( (*m_pTileSFPArrayLargeZone)[i].SpriteID );
		}

		DEBUG_ADD( "Subtract");

		// Remove newTileID from oldTileID.
		oldTileID -= newTileID;

		DEBUG_ADD( "Release Part");

		// Remove the TileIDs that exist only in the old zone.
		m_TileSPK.ReleasePart( oldTileID );

		DEBUG_ADD( "Delete pOldTilSFPA");

		delete pOldTileSFPA;
		pOldTileSFPA = NULL;
	}

	DEBUG_ADD_FORMAT( "Load Tile SFPArray LargeZone : size=%d", m_pTileSFPArrayLargeZone->GetSize() );

	//------------------------------------------------------------
	//  Load Tile SpriteSet
	//------------------------------------------------------------
	/*
	std::ifstream	TilePackFile;//(FILE_SPRITE_TILE, ios::binary);	
	if (!FileOpenBinary(FILE_SPRITE_TILE, TilePackFile))
		return false;

	bool bLoad = m_TileSPK.LoadFromFilePart(TilePackFile, m_pTileSFPArrayLargeZone);

	TilePackFile.close();

	return bLoad;
	*/

	return true;
}

//----------------------------------------------------------------------
// Load From File ImageObject LargeZone
//----------------------------------------------------------------------
// Loads from the File the ImageObjectSprites used only in the LargeZone.
//----------------------------------------------------------------------
bool
MTopView::LoadFromFileImageObjectSPKLargeZone(std::ifstream & file)
{
	//------------------------------------------------------------
	// Loads from the File the information about the ImageObjectSprites.
	//------------------------------------------------------------
	CSpriteFilePositionArray* pOldImageObjectSFPA = m_pImageObjectSFPArrayLargeZone;

	m_pImageObjectSFPArrayLargeZone = new CSpriteFilePositionArray;

	m_pImageObjectSFPArrayLargeZone->LoadFromFile( file );

	//------------------------------------------------------------
	// Remove what is not needed
	//------------------------------------------------------------
	if (pOldImageObjectSFPA!=NULL)
	{
		DEBUG_ADD( "Remove Old-ImageObject SFPArray LargeZone");

		COrderedList<TYPE_SPRITEID>	oldImageObjectID;
		COrderedList<TYPE_SPRITEID>	newImageObjectID;

		int i;

		// Sort
		for (i=pOldImageObjectSFPA->GetSize()-1; i>=0; i--)
		{
			oldImageObjectID.Add( (*pOldImageObjectSFPA)[i].SpriteID );

			//DEBUG_ADD_FORMAT( "Old: %d", (*pOldImageObjectSFPA)[i].SpriteID );			
		}

		// Sort
		for (i=m_pImageObjectSFPArrayLargeZone->GetSize()-1; i>=0; i--)
		{
			newImageObjectID.Add( (*m_pImageObjectSFPArrayLargeZone)[i].SpriteID );

			//	DEBUG_ADD_FORMAT( "New: %d", (*m_pImageObjectSFPArrayLargeZone)[i].SpriteID );

		}

		// Remove newImageObjectID from oldImageObjectID.
		oldImageObjectID -= newImageObjectID;

		/*
		#ifdef OUTPUT_DEBUG
			COrderedList<TYPE_SPRITEID>::DATA_LIST::const_iterator iData = oldImageObjectID.GetIterator();

			for (i=0; i<oldImageObjectID.GetSize(); i++)
			{
				TYPE_SPRITEID spriteID = *iData;

				DEBUG_ADD_FORMAT( "Release: %d", spriteID );

				iData++;				
			}							
		#endif
		*/

		// Remove the ImageObjectIDs that exist only in the old zone.
		m_ImageObjectSPK.ReleasePart( oldImageObjectID );

		delete pOldImageObjectSFPA;
		pOldImageObjectSFPA = NULL;
	}

	DEBUG_ADD_FORMAT( "Load ImageObject SFPArray LargeZone : size=%d", m_pImageObjectSFPArrayLargeZone->GetSize() );

	//------------------------------------------------------------
	//  Load ImageObject SpriteSet
	//------------------------------------------------------------
	/*
	std::ifstream	ImageObjectPackFile;//(FILE_SPRITE_IMAGEOBJECT, ios::binary);	
	if (!FileOpenBinary(FILE_SPRITE_IMAGEOBJECT, ImageObjectPackFile))
		return false;

	bool bLoadOK = m_ImageObjectSPK.LoadFromFilePart(ImageObjectPackFile, m_pImageObjectSFPArrayLargeZone);

	ImageObjectPackFile.close();

	return bLoadOK;
	*/

	return true;
}

//----------------------------------------------------------------------
// Load From File Tile SmallZone
//----------------------------------------------------------------------
// Loads from the File the TileSprites used only in the SmallZone.
//----------------------------------------------------------------------
bool
MTopView::LoadFromFileTileSPKSmallZone(std::ifstream & file)
{
	//------------------------------------------------------------
	// Loads from the File the information about the TileSprites.
	//------------------------------------------------------------
	CSpriteFilePositionArray* pOldTileSFPA = m_pTileSFPArraySmallZone;

	m_pTileSFPArraySmallZone = new CSpriteFilePositionArray;

	m_pTileSFPArraySmallZone->LoadFromFile( file );

	//------------------------------------------------------------
	// Remove what is not needed
	//------------------------------------------------------------
	if (pOldTileSFPA!=NULL)
	{
		DEBUG_ADD( "Remove Old-Tile SFPArray SmallZone");

		COrderedList<TYPE_SPRITEID>	oldTileID;
		COrderedList<TYPE_SPRITEID>	newTileID;

		int i;

		DEBUG_ADD( "Sort Part1");

		// Sort
		for (i=pOldTileSFPA->GetSize()-1; i>=0; i--)
		{
			oldTileID.Add( (*pOldTileSFPA)[i].SpriteID );
		}

		DEBUG_ADD( "Sort Part2");

		// Sort
		for (i=m_pTileSFPArraySmallZone->GetSize()-1; i>=0; i--)
		{
			newTileID.Add( (*m_pTileSFPArraySmallZone)[i].SpriteID );
		}

		DEBUG_ADD( "Subtract");

		// Remove newTileID from oldTileID.
		oldTileID -= newTileID;

		DEBUG_ADD( "Release");

		// Remove the TileIDs that exist only in the old zone.
		m_TileSPK.ReleasePart( oldTileID );

		DEBUG_ADD( "Delete pOldTilSFPA");

		delete pOldTileSFPA;
		pOldTileSFPA = NULL;
	}


	DEBUG_ADD_FORMAT( "Load Tile SFPArray SmallZone : size=%d", m_pTileSFPArraySmallZone->GetSize() );

	//------------------------------------------------------------
	//  Load Tile SpriteSet
	//------------------------------------------------------------
	/*
	std::ifstream	TilePackFile;//(FILE_SPRITE_TILE, ios::binary);	
	if (!FileOpenBinary(FILE_SPRITE_TILE, TilePackFile))
		return false;

	bool bLoad = m_TileSPK.LoadFromFilePart(TilePackFile, m_pTileSFPArraySmallZone);

	TilePackFile.close();

	return bLoad;
	*/

	return true;
}

//----------------------------------------------------------------------
// Load From File ImageObject SmallZone
//----------------------------------------------------------------------
// Loads from the File the ImageObjectSprites used only in the SmallZone.
//----------------------------------------------------------------------
bool
MTopView::LoadFromFileImageObjectSPKSmallZone(std::ifstream & file)
{
	//------------------------------------------------------------
	// Loads from the File the information about the ImageObjectSprites.
	//------------------------------------------------------------
	CSpriteFilePositionArray* pOldImageObjectSFPA = m_pImageObjectSFPArraySmallZone;

	m_pImageObjectSFPArraySmallZone = new CSpriteFilePositionArray;

	m_pImageObjectSFPArraySmallZone->LoadFromFile( file );

	//------------------------------------------------------------
	// Remove what is not needed
	//------------------------------------------------------------
	if (pOldImageObjectSFPA!=NULL)
	{
		DEBUG_ADD( "Remove Old-ImageObject SFPArray SmallZone");

		COrderedList<TYPE_SPRITEID>	oldImageObjectID;
		COrderedList<TYPE_SPRITEID>	newImageObjectID;

		int i;

		// Sort
		for (i=pOldImageObjectSFPA->GetSize()-1; i>=0; i--)
		{
			oldImageObjectID.Add( (*pOldImageObjectSFPA)[i].SpriteID );
		}

		// Sort
		for (i=m_pImageObjectSFPArraySmallZone->GetSize()-1; i>=0; i--)
		{
			newImageObjectID.Add( (*m_pImageObjectSFPArraySmallZone)[i].SpriteID );
		}

		// Remove newImageObjectID from oldImageObjectID.
		oldImageObjectID -= newImageObjectID;

		// Remove the ImageObjectIDs that exist only in the old zone.
		m_ImageObjectSPK.ReleasePart( oldImageObjectID );

		delete pOldImageObjectSFPA;
		pOldImageObjectSFPA = NULL;
	}


	DEBUG_ADD_FORMAT( "Load ImageObject SFPArray SmallZone : size=%d", m_pImageObjectSFPArraySmallZone->GetSize() );

	//------------------------------------------------------------
	//  Load ImageObject SpriteSet
	//------------------------------------------------------------
	/*
	std::ifstream	ImageObjectPackFile;//(FILE_SPRITE_IMAGEOBJECT, ios::binary);	
	if (!FileOpenBinary(FILE_SPRITE_IMAGEOBJECT, ImageObjectPackFile))
		return false;

	bool bLoadOK = m_ImageObjectSPK.LoadFromFilePart(ImageObjectPackFile, m_pImageObjectSFPArraySmallZone);

	ImageObjectPackFile.close();

	return bLoadOK;
	*/
	return true;
}

//----------------------------------------------------------------------
// Release TileSPK LargeZone
//----------------------------------------------------------------------
void	
MTopView::ReleaseTileSPKLargeZone()
{
	/*
	if (m_pTileSFPArrayLargeZone!=NULL)
	{		
		DEBUG_ADD_FORMAT( "Release TileSPK LargeZone : size=%d", m_pTileSFPArrayLargeZone->GetSize() );

		m_TileSPK.ReleasePart( m_pTileSFPArrayLargeZone );	

		DEBUG_ADD( "ReleasePart OK" );

		delete m_pTileSFPArrayLargeZone;		

		m_pTileSFPArrayLargeZone = NULL;
	}
	*/
	DEBUG_ADD( "Release TileSPK LargeZone OK" );
}

//----------------------------------------------------------------------
// Release ImageObjectSPK LargeZone
//----------------------------------------------------------------------
void	
MTopView::ReleaseImageObjectSPKLargeZone()
{
	/*
	if (m_pImageObjectSFPArrayLargeZone!=NULL)
	{
		DEBUG_ADD_FORMAT( "ReleaseImageObjectSPKLargeZone size=%d", m_pImageObjectSFPArrayLargeZone->GetSize() );

		m_ImageObjectSPK.ReleasePart( m_pImageObjectSFPArrayLargeZone );

		DEBUG_ADD( "ReleasePart OK" );

		delete m_pImageObjectSFPArrayLargeZone;

		m_pImageObjectSFPArrayLargeZone = NULL;
	}
	*/
	DEBUG_ADD( "ReleaseImageObjectSPKLargeZone OK" );
}

//----------------------------------------------------------------------
// Release TileSPK SmallZone
//----------------------------------------------------------------------
// Removes the Tiles that exist only in the SmallZone.
//----------------------------------------------------------------------
void	
MTopView::ReleaseTileSPKSmallZone()
{
	/*
	DEBUG_ADD( "ReleaseTileSPKSmallZone" );

	// Sort m_pTileSFPArraySmallZone and m_pTileSFPArrayLargeZone, and
	// do SmallZone - LargeZone and remove what is left over.
	COrderedList<TYPE_SPRITEID>	SmallZoneTileID;
	COrderedList<TYPE_SPRITEID>	LargeZoneTileID;

	register int i;

	// Sort
	for (i=m_pTileSFPArraySmallZone->GetSize()-1; i>=0; i--)
	{
		SmallZoneTileID.Add( m_pTileSFPArraySmallZone[i].SpriteID );
	}

	// Sort
	for (i=m_pTileSFPArrayLargeZone->GetSize()-1; i>=0; i--)
	{
		LargeZoneTileID.Add( m_pTileSFPArrayLargeZone[i].SpriteID );
	}

	// Remove LargeZoneTileID from SmallZoneTileID.
	SmallZoneTileID -= LargeZoneTileID;

	// Remove the TileIDs that exist only in the SmallZone.
	m_TileSPK.ReleasePart( SmallZoneTileID );

	m_pTileSFPArraySmallZone->Release();
	*/
	DEBUG_ADD( "ReleaseTileSPKSmallZone OK" );
}

//----------------------------------------------------------------------
// Release ImageObjectSPK SmallZone
//----------------------------------------------------------------------
// Removes the ImageObjects that exist only in the SmallZone.
//----------------------------------------------------------------------
void	
MTopView::ReleaseImageObjectSPKSmallZone()
{
	/*
	DEBUG_ADD( "ReleaseImageObjectSPKSmallZone" );

	// Sort m_pImageObjectSFPArraySmallZone and m_pImageObjectSFPArrayLargeZone, and
	// do SmallZone - LargeZone and remove what is left over.
	COrderedList<TYPE_SPRITEID>	SmallZoneImageObjectID;
	COrderedList<TYPE_SPRITEID>	LargeZoneImageObjectID;

	register int i;

	// Sort
	for (i=m_pImageObjectSFPArraySmallZone->GetSize()-1; i>=0; i--)
	{
		SmallZoneImageObjectID.Add( m_pImageObjectSFPArraySmallZone[i].SpriteID );
	}

	// Sort
	for (i=m_pImageObjectSFPArrayLargeZone->GetSize()-1; i>=0; i--)
	{
		LargeZoneImageObjectID.Add( m_pImageObjectSFPArrayLargeZone[i].SpriteID );
	}

	// Remove LargeZoneImageObjectID from SmallZoneImageObjectID.
	SmallZoneImageObjectID -= LargeZoneImageObjectID;

	// Remove the ImageObjectIDs that exist only in the SmallZone.
	m_ImageObjectSPK.ReleasePart( SmallZoneImageObjectID );

	m_pImageObjectSFPArraySmallZone->Release();
	*/
	DEBUG_ADD( "ReleaseImageObjectSPKSmallZone OK" );
}

//----------------------------------------------------------------------
// Sets the Zone to show.
//----------------------------------------------------------------------
void
MTopView::SetZone(MZone* pZone)
{
	m_pZone		= pZone;

	// Redraw everything.
	m_bFirstTileDraw = true;

	ClearOutputCreature();

	ClearItemNameList();

	// Set the coordinates
	//----------------------------------------
	// Sight related..
	//----------------------------------------
}

//----------------------------------------------------------------------
// Map to Pixel  :  (Xm, Ym) ---> (Xp, Yp)
//----------------------------------------------------------------------
// inline

//----------------------------------------------------------------------
// Pixel to Map  :  (Xp, Yp) ---> (Xm, Ym)
//----------------------------------------------------------------------
// inline


//----------------------------------------------------------------------
// Get Screen Point
//----------------------------------------------------------------------
// Which point in Zone pixel coordinates is the screen coordinate (x,y)?
//----------------------------------------------------------------------
POINT	
MTopView::ScreenToPixel(int x, int y)
{
	// Obtains the coordinate of the current screen's (x,y).
	POINT zonePixel;	// Zone pixel coordinate that the first Sector on the screen stands for

	// Obtain the Pixel coordinate on the Map that the first Sector of the screen stands for, and
	// adding the screen coordinate (x,y) to it
	// gives the Pixel coordinate on the Map that the on-screen coordinate stands for


	// + g_DrawCamGap: back out the camera-interpolation bias, exact tick space
	zonePixel.x = m_FirstZonePixel.x + g_DrawCamGapX + x;
	zonePixel.y = m_FirstZonePixel.y + g_DrawCamGapY + y;

	return zonePixel;
}

//----------------------------------------------------------------------
// Pixel To Screen
//----------------------------------------------------------------------
// Converts a Zone pixel coordinate to a screen coordinate.
//----------------------------------------------------------------------
POINT
MTopView::PixelToScreen(int x, int y)
{
	POINT screenPixel;

	// From the zone pixel coordinate,
	// just subtract the zone coordinate the first point of the screen points at.
	screenPixel.x = x - m_FirstZonePixel.x;
	screenPixel.y = y - m_FirstZonePixel.y;

	return screenPixel;
}

//----------------------------------------------------------------------
// Map To Screen
//----------------------------------------------------------------------
// Converts a Map sector coordinate to a screen coordinate.
//----------------------------------------------------------------------
POINT
MTopView::MapToScreen(int sX, int sY)
{
	POINT screenPixel;

	// Converts a Map Sector coordinate to a Pixel coordinate.
	screenPixel = MapToPixel(sX, sY);

	// Converts a Pixel coordinate to a Screen coordinate.
	screenPixel.x -= m_FirstZonePixel.x;
	screenPixel.y -= m_FirstZonePixel.y;

	return screenPixel;
}

//----------------------------------------------------------------------
// The position that screen coordinate (x,y) points at
// is which (sX,sY) Sector in the Zone?
//----------------------------------------------------------------------
POINT
MTopView::GetSelectedSector(int x, int y)
{
	POINT point;

	//-------------------------------------------------
	// Prepared values
	//-------------------------------------------------
	// First Sector on the screen : m_FirstSector
	// Coordinate correction value : m_PlusPoint
	//-------------------------------------------------

	//-------------------------------------------------
	// Method
	//-------------------------------------------------
	// 1. Find the pixel-unit coordinate that
	//    screen (0,0) stands for in the Zone. (pX,pY) - MapToPixel
	//
	// 2. Compute (pX,pY) + (x,y).
	//
	// 3. Find the Sector coordinate that
	//    (pX,x, pY,y) stands for in the Zone.           - PixelToMap
	//-------------------------------------------------

	point = MapToPixel(m_FirstSector.x, m_FirstSector.y);
	// + g_DrawCamGap: back out the camera-interpolation bias, exact tick space
	point.x += m_PlusPoint.x + g_DrawCamGapX;
	point.y += m_PlusPoint.y + g_DrawCamGapY;

	point.x += x;
	point.y += y;

	point = PixelToMap(point.x, point.y);

	if (g_pZone->IsPositionInZone( static_cast<TYPE_SECTORPOSITION>(point.x), static_cast<TYPE_SECTORPOSITION>(point.y) ))
	{
		return point;
	}

	point.x = SECTORPOSITION_NULL;
	point.y = SECTORPOSITION_NULL;

	return point;
}

//----------------------------------------------------------------------
// Set Direction To Creature
//----------------------------------------------------------------------
// Look toward another Creature.
//----------------------------------------------------------------------
BYTE
MTopView::GetDirectionToPosition(int originX, int originY, int destX, int destY)
{
	return SelectFacingDirection(originX, originY, destX, destY);
}

//----------------------------------------------------------------------
// Set Fade Start
//----------------------------------------------------------------------
// Fades from start to end by step, towards the colour (r,g,b):
// 0 is the full colour, 31 none. An end of -1 with a delay is the
// Gilles de Rais darkening, which holds at 1 before it ends.
//----------------------------------------------------------------------
void			
MTopView::SetFadeStart(signed char start, signed char end, signed char step, BYTE r, BYTE g, BYTE b, WORD delay)
{	
	m_Fade.Start(start, end, step, delay);
// SDL2: Unified path - always use SDL color format
	m_FadeColor = CSDLGraphics::Color(r,g,b);
}


//----------------------------------------------------------------------
// Draw Fade
//----------------------------------------------------------------------
// Draws the fade and advances it
//----------------------------------------------------------------------	
void 
MTopView::DrawFade()
{
	if (g_pEventManager->GetEventByFlag(EVENTFLAG_NOT_FADE_SCREEN) || !m_Fade.IsActive())
		return;

	RECT rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = g_GameRect.right;
	rect.bottom = g_GameRect.bottom;

	m_pSurface->Lock();
	// The SDL path only blits non-black fades, as before the extraction.
	if (m_FadeColor != 0)
		m_pSurface->BltColorAlpha(&rect, m_FadeColor, m_Fade.Value());
	m_pSurface->Unlock();

	// Draw the current shade before testing the next step and end boundary.
	m_Fade.Advance(g_CurrentFrame, g_bFrameChanged);
}
//----------------------------------------------------------------------
// If the position that screen coordinate (x,y) points at is selected,
// which (sX,sY) Object of the Zone gets selected?
//----------------------------------------------------------------------
MObject*
MTopView::GetSelectedObject(int x, int y)
{
	//--------------------------------------------------------------	
	// 1. Find the Sector (sX,sY) coordinate in the Zone of screen coordinate (x,y).
	// 
	// 2. If there is an Object near (sX,sY),
	//    find that Object's on-screen coordinate, and
	//    if (x,y) falls within it, "select it"
	//    otherwise, the next nearby Sector must be checked.
	//
	// (!) The Player is excluded. Check as if it does not exist.
	//--------------------------------------------------------------

	//--------------------------------------------------------------
	// Order in which nearby Sectors are searched
	//--------------------------------------------------------------	
	// - The lower the Sector a picture is in, the later it is drawn,
	//   so the higher its selection priority.
	// 
	//      .  .  .  
	//      .  X  . 
	//      .  .  .  
	//      .  .  .  
	//      .  .  .     When X is selected, Ground, Item etc... check range
	//
	//
	//               
	//         X    
	//               
	//      .  .  .  
	//      .  .  .  
	//      .  .  .  
	//      .  .  .     When Flying floats 64 pixels (two tiles) high..
	//      .  .  .     When X is selected, Flying... check range
	//
	//--------------------------------------------------------------

	//--------------------------------------------------------------
	// What has to be checked for each Sector
	//--------------------------------------------------------------
	/*

	   while ( ExistNextSector )
	   {			
			if ( IsExistObject )
			{
				if ( IsObjectCreature && ClickPoint In TheCreatureRect)
					Select Creature

				else if ( IsObjectItem && ClickPoint In TheItemRect )
					Select Item
			}			

			CurrentSector = NextSector
		}

		if ( Not Selected )
			return NULL;

		return Object;
	*/
	//--------------------------------------------------------------

	//--------------------------------------------------------------
	// If the game screen has not been drawn yet..
	// If the Player is in darkness..
	// Just return
	//--------------------------------------------------------------
//	bool bSlayerPlayer = g_pPlayer->IsSlayer();

	if ((m_bFirstTileDraw
		|| (!g_pPlayer->IsVampire()&& g_pPlayer->IsInDarkness() && (!g_pPlayer->HasEffectStatus( EFFECTSTATUS_LIGHTNESS )||g_pZone->GetID() == 3001)
		&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
		&& !g_bLight
#endif
		))
		)		
	{
		return NULL;
	}

	POINT firstZonePixel;	// Zone pixel coordinate that the first Sector on the screen stands for
	firstZonePixel = MapToPixel(m_FirstSector.x, m_FirstSector.y);
	// + g_DrawCamGap: back out the camera-interpolation bias, exact tick space
	firstZonePixel.x += m_PlusPoint.x + g_DrawCamGapX;
	firstZonePixel.y += m_PlusPoint.y + g_DrawCamGapY;


	POINT	pixelPoint;			// Pixel Position In Zone
	POINT	sectorPoint;		// Sector Position In Zone

	//--------------------------------------------------------------
	// Obtain the Zone pixel coordinate of (x,y).
	//--------------------------------------------------------------
	pixelPoint = MapToPixel(m_FirstSector.x, m_FirstSector.y);
	pixelPoint.x += m_PlusPoint.x + g_DrawCamGapX + x;
	pixelPoint.y += m_PlusPoint.y + g_DrawCamGapY + y;

	//--------------------------------------------------------------
	// Obtain the Zone sector coordinate of (x,y).
	//--------------------------------------------------------------						
	sectorPoint = PixelToMap(pixelPoint.x, pixelPoint.y);

	g_MouseSector = sectorPoint;

	//--------------------------------------------------------------
	// Portal check
	//--------------------------------------------------------------
	// It's funny to do it here.. -_-;;
	// For ease of processing.. temporarily... hehe... ㅡ.ㅡ;;
	//--------------------------------------------------------------
	g_bMouseInPortal = false;

	if (sectorPoint.x >= 0 
		&& sectorPoint.y >= 0
		&& sectorPoint.x < g_pZone->GetWidth() 
		&& sectorPoint.y < g_pZone->GetHeight())
	{
		#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
			DEBUG_ADD("ptChk");
		#endif

		const MSector& sector = m_pZone->GetSector(static_cast<TYPE_SECTORPOSITION>(sectorPoint.x), static_cast<TYPE_SECTORPOSITION>(sectorPoint.y));

		if (sector.IsPortal())
		{
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
				DEBUG_ADD("hasPt");
			#endif

			int numPortal = sector.GetPortalSize();


			/*				
			for (int i=0; i<numPortal; i++)
			{
				// ID of the zone that can be moved to
				PORTAL_INFO portalInfo = *iPortal;			

				//-------------------------------------------------------
				// Temporary output
				//-------------------------------------------------------
				DEBUG_ADD_FORMAT( "[Portal] Type=%d, ZoneID=%d", portalInfo.Type, portalInfo.ZoneID );

				iPortal++;
			}
			*/

			if (numPortal != 0)
			{
				PORTAL_LIST::const_iterator	iPortal = sector.GetPortalBegin();

				PORTAL_INFO portalInfo = *iPortal;

				bool canMove = false;
				bool bSiegePotal = false;
				//-------------------------------------------------------
				// Check whether the place can be reached depending on the portal type
				//-------------------------------------------------------			
				switch (portalInfo.Type)				
				{
					case MPortal::TYPE_BATTLE_PORTAL :
					case MPortal::TYPE_NOMAL :
					case MPortal::TYPE_CLIENT_ONLY :
						canMove = true;
					break;

					case MPortal::TYPE_SLAYER_ONLY :
						canMove = g_pPlayer->IsSlayer();
					break;

					case MPortal::TYPE_VAMPIRE_ONLY :
						canMove = g_pPlayer->IsVampire();
					break;

					case MPortal::TYPE_OUSTERS_ONLY :
						canMove = g_pPlayer->IsOusters();
						break;

					case MPortal::TYPE_MULTI_PORTAL :
						canMove = g_pPlayer->IsSlayer();
					break;

					case MPortal::TYPE_GUILD_PORTAL:
						canMove = (g_pUserInformation->GuildName.GetLength()>0);
					break;
					case MPortal::TYPE_CLIENT_SIEGE_POTAL_1:
					case MPortal::TYPE_CLIENT_SIEGE_POTAL_2:
					case MPortal::TYPE_CLIENT_SIEGE_POTAL_3:
					case MPortal::TYPE_CLIENT_SIEGE_POTAL_4:
						canMove = true;
						bSiegePotal = true;
						break;

				}

				//-------------------------------------------------------			
				// If it can be reached, show the portal cursor
				//-------------------------------------------------------			
				if (canMove)
				{
					ZONETABLE_INFO* pZoneInfo = g_pZoneTable->Get( portalInfo.ZoneID );

					if (pZoneInfo!=NULL)
					{
						//char pZoneName[80];
						int zoneID = portalInfo.ZoneID;

						if(bSiegePotal)
						{
							if(g_pSystemAvailableManager->ZoneFiltering( zoneID )&& zoneID == 1500)
								gpC_mouse_pointer->SetCursorPortal( zoneID , portalInfo.Type);
						}
						else
						{
							if (numPortal <= 1)
							{
								//strcpy(pZoneName, pZoneInfo->Name.GetString());
							}
							else
							{
								// Ultimate hard-coding~~
								zoneID = 60001;
								//strcpy(pZoneName, "엘리베이터");
							}


							if(// (!g_pUserInformation->IsNetmarble || g_mapPremiumZone.find(zoneID) == g_mapPremiumZone.end())
								/*&&*/ g_pSystemAvailableManager->ZoneFiltering( zoneID ))
								gpC_mouse_pointer->SetCursorPortal( zoneID );
						}
					}
				}
				else
				{

					if (!g_bMouseInPortal)
					{
					}
				}
			}

			g_bMouseInPortal = true;

			#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
				DEBUG_ADD("ptK");
			#endif
		}		

		#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
			DEBUG_ADD("ptK2");
		#endif
	}


	//--------------------------------------------------------------
	//
	// check
	//
	//--------------------------------------------------------------

	MCreature*	pCreature = NULL;
	CFrame			frame;
//	TYPE_SPRITEID	sprite;

	// Area of the Object
//	POINT	objectPixelPoint;
	//RECT	rect;

	// Player Creature's ID
	TYPE_OBJECTID	pid = g_pPlayer->GetID();

	//std::ofstream file("log.txt");


	// Coordinate currently being checked
	int currentX;
	int currentY;

	int numCreature = 0;

	//--------------------------------------------------------------
	//
	// Flying Creature Check 
	//
	// : when at the same height
	//   can exist in a Sector lower than a normal Object.
	//
	//--------------------------------------------------------------
	int	sX1 = sectorPoint.x - 3,		// Increased it by one more row. Hmm~
			sX2 = sectorPoint.x + 3;

	// Added + because of the bat height..
	int	sY1 = sectorPoint.y + 4,
			sY2 = sectorPoint.y + 8;

	//------------------------------------------------------
	// Skip if outside the Zone area...
	//------------------------------------------------------
	if (sX1 < 0) 
	{			
		sX1 = 0;	
	}

	if (sX2 >= m_pZone->GetWidth())
	{
		sX2 = m_pZone->GetWidth()-1;
	}

	if (sY1 < 0)
	{	
		sY1 = 0;	
	}

	if (sY2 >= m_pZone->GetHeight())
	{
		sY2 = m_pZone->GetHeight()-1;
	}


	//------------------------------------------------------	
	// Check the Sectors where it is likely to be.
	//------------------------------------------------------	
	for (currentY=sY2; currentY>=sY1; currentY--)	
	{	
		for (currentX=sX1; currentX<=sX2; currentX++)	
		{			
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
				DEBUG_ADD_FORMAT("ckF1(%d,%d)", currentX, currentY);
			#endif

			const MSector& sector = m_pZone->GetSector(currentX, currentY);

			// If there is some Object
			if (sector.IsExistObject() )			
			{		

				//file << "Exist Object!" << endl;

				// Check differently depending on the Object type.
				// Only Creatures and Items can be selected.

				//------------------------------------------------
				//
				// If there is a Flying Creature
				//
				//------------------------------------------------
				numCreature = sector.GetFlyingCreatureSize();

				if (numCreature > 0)
				{
					#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
						DEBUG_ADD("hasFC");
					#endif

					OBJECT_MAP::const_iterator iCreature = sector.GetFlyingCreatureIterator();

					for (int c=0; c<numCreature; c++, iCreature++)
					{
						if (iCreature==sector.GetObjectEnd() 
							|| iCreature->first > MSector::POSITION_FLYINGCREATURE_MAX)
							break;

						pCreature = (MCreature*)iCreature->second;

						// Do not select it if it is the Player.
						if (pCreature->GetID()!=pid)	
						{
							// Obtain the area the FlyingCreature occupies.
							// The information probably has to be read from the FRAME_PACK!!
							// The FlyingCreature's Sector coordinate must be converted to a Pixel coordinate.

							// Calculate the Zone coordinate of the FlyingCreature.
							/*
							objectPixelPoint = MapToPixel(pCreature->GetX(), pCreature->GetY());
							objectPixelPoint.x += m_PlusPoint.x + pCreature->GetSX();
							objectPixelPoint.y += m_PlusPoint.y + pCreature->GetSY();

							// Convert the Zone coordinate to a screen coordinate.
							objectPixelPoint.x -= firstZonePixel.x;
							objectPixelPoint.y -= firstZonePixel.y;

							// Calculate the area where the Creature exists on screen.
							frame = m_CreatureFPK[(*g_pCreatureTable)[pCreature->GetCreatureType()].FrameID][pCreature->GetAction()][pCreature->GetDirection()][pCreature->GetFrame()];

							// Position where the current Sprite is drawn on screen
							objectPixelPoint.x += frame.GetCX();
							objectPixelPoint.y += frame.GetCY() - FLYINGCREATURE_HEIGHT;
												// Since the height is set in the FramePack,
												// the Flying case is the same.

							// Calculate the area where the Creature exists on screen.
							rect.left	= objectPixelPoint.x;
							rect.top	= objectPixelPoint.y;
							rect.right	= rect.left + m_CreatureSPK[ frame.GetSpriteID() ].GetWidth();
							rect.bottom	= rect.top + m_CreatureSPK[ frame.GetSpriteID() ].GetHeight();
							*/

							// If in darkness
							// If the selected position falls within the Creature's area
							// In attack mode, select only characters that can be attacked
							// otherwise anyone.
							if (
								((g_pPlayer->IsVampire()&&g_pZone->GetID() != 3001) ||
								(!g_pPlayer->IsVampire()&& !(!pCreature->IsNPC() && pCreature->IsInDarkness())) ||
								(!g_pPlayer->IsVampire()&& g_pPlayer->HasEffectStatus( EFFECTSTATUS_LIGHTNESS )) 
								|| g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
								|| g_bLight
#endif
								)
								&& !pCreature->IsCutHeight()	// If it is in the vanishing state
								&& g_pObjectSelector->CanSelect(pCreature)
								&& pCreature->IsPointInScreenRect(x,y)
								&& !(pCreature->IsOusters() && pCreature->IsInGroundElemental() && !g_pPlayer->IsOusters()))
							//if (x > rect.left && x < rect.right
								//&& y > rect.top && y < rect.bottom)
							{											
								return pCreature;
							}				
						}
					}
				}	
			}
		}
	}

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
		DEBUG_ADD("chk2");
	#endif

	//--------------------------------------------------------------
	//
	// Creature / Item Check data
	//
	//--------------------------------------------------------------
	sY1 = sectorPoint.y - 3,
	sY2 = sectorPoint.y + 4;		// Increased it by one more row.

	//------------------------------------------------------
	// Skip if outside the Zone area...
	//------------------------------------------------------	
	if (sY1 < 0)
	{	
		sY1 = 0;	
	}

	if (sY2 >= m_pZone->GetHeight())
	{
		sY2 = m_pZone->GetHeight()-1;
	}

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
		DEBUG_ADD("ckIO9");
	#endif

	//------------------------------------------------------
	// ImageObject Iterator subject to Check
	//------------------------------------------------------
	IMAGEOBJECT_OUTPUT_MAP::reverse_iterator iImageObjectCheck
		= m_mapImageObject.rbegin();

	//------------------------------------------------------
	// Check the ImageObjects whose output time
	// is less than sY1.
	// m_mapImageObject is sorted with the Viewpoint as the key.
	//------------------------------------------------------
	while (iImageObjectCheck != m_mapImageObject.rend())
	{		

//			MInteractionObject* const pInteractionObject = (MInteractionObject* const)pImageObject;
//
//				/*
//
//
//						
//
//				*/		
//

		iImageObjectCheck ++;
	}

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
		DEBUG_ADD("ckSe");
	#endif

	//------------------------------------------------------	
	// Check the Sectors where it is likely to be.
	//------------------------------------------------------	
	for (currentY=sY2; currentY>=sY1; currentY--)	
	{	
		for (currentX=sX1; currentX<=sX2; currentX++)	
		{				
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
				DEBUG_ADD_FORMAT("gS(%d,%d)", currentX, currentY);
			#endif

			const MSector& sector = m_pZone->GetSector(currentX, currentY);

			// If there is some Object
			if (sector.IsExistObject() )			
			{		

				//file << "Exist Object!" << endl;

				// Check differently depending on the Object type.
				// Only Creatures and Items can be selected.

				//------------------------------------------------
				//
				// If there is a Flying Creature
				//
				//------------------------------------------------
				#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
					DEBUG_ADD("ckFc");
				#endif

				numCreature = sector.GetFlyingCreatureSize();

				if (numCreature > 0)
				{
					OBJECT_MAP::const_iterator iCreature = sector.GetFlyingCreatureIterator();

					for (int c=0; c<numCreature; c++, iCreature++)
					{
						if (iCreature==sector.GetObjectEnd() 
							|| iCreature->first > MSector::POSITION_FLYINGCREATURE_MAX)
							break;

						pCreature = (MCreature*)iCreature->second;

						// Do not select it if it is the Player.
						if (pCreature->GetID()!=pid)	
						{
							/*
							// Obtain the area the FlyingCreature occupies.
							// The information probably has to be read from the FRAME_PACK!!
							// The FlyingCreature's Sector coordinate must be converted to a Pixel coordinate.

							// Calculate the Zone coordinate of the FlyingCreature.
							objectPixelPoint = MapToPixel(pCreature->GetX(), pCreature->GetY());
							objectPixelPoint.x += m_PlusPoint.x + pCreature->GetSX();
							objectPixelPoint.y += m_PlusPoint.y + pCreature->GetSY();

							// Convert the Zone coordinate to a screen coordinate.
							objectPixelPoint.x -= firstZonePixel.x;
							objectPixelPoint.y -= firstZonePixel.y;

							// Calculate the area where the Creature exists on screen.
							frame = m_CreatureFPK[(*g_pCreatureTable)[pCreature->GetCreatureType()].FrameID][pCreature->GetAction()][pCreature->GetDirection()][pCreature->GetFrame()];

							// Position where the current Sprite is drawn on screen
							objectPixelPoint.x += frame.GetCX();
							objectPixelPoint.y += frame.GetCY() - FLYINGCREATURE_HEIGHT;
												// Since the height is set in the FramePack,
												// the Flying case is the same.

							// Calculate the area that exists on screen.
							rect.left	= objectPixelPoint.x;
							rect.top	= objectPixelPoint.y;
							rect.right	= rect.left + m_CreatureSPK[ frame.GetSpriteID() ].GetWidth();
							rect.bottom	= rect.top + m_CreatureSPK[ frame.GetSpriteID() ].GetHeight();
							*/

							// If the selected position falls within the Creature's area
							if ((
								(g_pPlayer->IsVampire()&&g_pZone->GetID() != 3001)|| 
								(!g_pPlayer->IsVampire()&& !(!pCreature->IsNPC() && pCreature->IsInDarkness())) ||
								(!g_pPlayer->IsVampire()&& g_pPlayer->HasEffectStatus( EFFECTSTATUS_LIGHTNESS )) 
								|| g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
								|| g_bLight
#endif
								)
								&& !pCreature->IsCutHeight()	// If it is in the vanishing state
								&& g_pObjectSelector->CanSelect(pCreature)
								&& pCreature->IsPointInScreenRect(x,y)
								&& !(pCreature->IsOusters() && pCreature->IsInGroundElemental() && !g_pPlayer->IsOusters()))
							//if (x > rect.left && x < rect.right
							//	&& y > rect.top && y < rect.bottom)
							{
								#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
									DEBUG_ADD("reFc");
								#endif

								return pCreature;
							}
						}
					}
				}				

				//------------------------------------------------
				//
				// If there is a Creature
				//
				//------------------------------------------------
				#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
					DEBUG_ADD("ckGc");
				#endif

				numCreature = sector.GetGroundCreatureSize();

				if (numCreature > 0)
				{
					OBJECT_MAP::const_iterator iCreature = sector.GetGroundCreatureIterator();

					for (int c=0; c<numCreature; c++, iCreature++)
					{
						if (iCreature==sector.GetObjectEnd() 
							|| iCreature->first > MSector::POSITION_GROUNDCREATURE_MAX)
							break;

						pCreature = (MCreature*)iCreature->second;

						// Do not select it if it is the Player.
						if (pCreature->GetID()!=pid)	
						{
							/*
							// Obtain the area the Creature occupies.
							// The information probably has to be read from the FRAME_PACK!!
							// The Creature's Sector coordinate must be converted to a Pixel coordinate.

							// Calculate the Zone coordinate of the Creature.
							objectPixelPoint = MapToPixel(pCreature->GetX(), pCreature->GetY());
							objectPixelPoint.x += m_PlusPoint.x + pCreature->GetSX();
							objectPixelPoint.y += m_PlusPoint.y + pCreature->GetSY();

							// Convert the Zone coordinate to a screen coordinate.
							objectPixelPoint.x -= firstZonePixel.x;
							objectPixelPoint.y -= firstZonePixel.y;

							// Calculate the area where the Creature exists on screen.
							frame = m_CreatureFPK[(*g_pCreatureTable)[pCreature->GetCreatureType()].FrameID][pCreature->GetAction()][pCreature->GetDirection()][pCreature->GetFrame()];

							// Position where the current Sprite is drawn on screen
							objectPixelPoint.x += frame.GetCX();
							objectPixelPoint.y += frame.GetCY();

							// Calculate the area that exists on screen.
							rect.left	= objectPixelPoint.x;
							rect.top	= objectPixelPoint.y;
							rect.right	= rect.left + m_CreatureSPK[ frame.GetSpriteID() ].GetWidth();
							rect.bottom	= rect.top + m_CreatureSPK[ frame.GetSpriteID() ].GetHeight();
							*/			
							// If the selected position falls within the Creature's area
							if (((g_pPlayer->IsVampire()&&g_pZone->GetID() != 3001) ||
								(!g_pPlayer->IsVampire()&& !(!pCreature->IsNPC() && pCreature->IsInDarkness())) ||
								(!g_pPlayer->IsVampire()&& g_pPlayer->HasEffectStatus( EFFECTSTATUS_LIGHTNESS )) 
								|| g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
								|| g_bLight
#endif
								)
								&& !pCreature->IsCutHeight()	// If it is in the vanishing state
								&& g_pObjectSelector->CanSelect(pCreature)
								&& pCreature->IsPointInScreenRect(x,y)
								&& !(pCreature->IsOusters() && pCreature->IsInGroundElemental() && !g_pPlayer->IsOusters()))
							//if (x > rect.left && x < rect.right
							//	&& y > rect.top && y < rect.bottom)
							{
								#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
									DEBUG_ADD("reGc");
								#endif

								return pCreature;
							}
						}
					}
				}

				//------------------------------------------------
				//
				// If there is an Effect
				//
				//------------------------------------------------				
				#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
					DEBUG_ADD("ckE");
				#endif

				if (sector.IsExistEffect())
				{	
					int	effectNum = sector.GetEffectSize();

					EFFECT_LIST::const_iterator iEffect = sector.GetEffectIterator();

					for (int e=0; e<effectNum; e++)
					{
						MEffect* pEffect = *iEffect;

						if (pEffect->IsSelectable())
						{
							if (pEffect->IsPointInScreenRect(x,y))							
							{
								#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
									DEBUG_ADD("reE");
								#endif

								return pEffect;
							}							
						}

						iEffect++;
					}					
				}

				//------------------------------------------------
				//
				// If there is an UndergroundCreature
				//
				//------------------------------------------------
				#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
					DEBUG_ADD("ckUc");
				#endif

				numCreature = sector.GetUndergroundCreatureSize();

				if (numCreature > 0)
				{
					OBJECT_MAP::const_iterator iCreature = sector.GetUndergroundCreatureIterator();

					for (int c=0; c<numCreature; c++, iCreature++)
					{
						if (iCreature==sector.GetObjectEnd() 
							|| iCreature->first > MSector::POSITION_UNDERGROUNDCREATURE_MAX)
							break;

						pCreature = (MCreature*)iCreature->second;

						// Do not select it if it is the Player.
						if (pCreature->GetID()!=pid)	
						{							
							// If the selected position falls within the Creature's area
							if (((g_pPlayer->IsVampire() &&g_pZone->GetID() != 3001)||
								(!g_pPlayer->IsVampire()&& !(!pCreature->IsNPC() && pCreature->IsInDarkness()))
								|| (!g_pPlayer->IsVampire() && g_pPlayer->HasEffectStatus( EFFECTSTATUS_LIGHTNESS )) 								 
								||g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
								|| g_bLight
#endif
								)
								&& g_pObjectSelector->CanSelect(pCreature)
								&& pCreature->IsPointInScreenRect(x,y)
								&& !(pCreature->IsOusters() && pCreature->IsInGroundElemental() && !g_pPlayer->IsOusters())
								)
							{
								#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
									DEBUG_ADD("uC");
								#endif

								return pCreature;
							}
						}
					}
				}

				//------------------------------------------------
				//
				// If there is an Item
				//
				//------------------------------------------------
				#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
					DEBUG_ADD("ckIt");
				#endif

				MItem* const pItem = sector.GetItem();

				if (pItem != NULL)
				{	
					#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
						DEBUG_ADD("IT");
					#endif

					//file << "Exist Item!" << endl;

					// Obtain the area the Item occupies.
					// The information probably has to be read from the FRAME_PACK!!
					// The Item's Sector coordinate must be converted to a Pixel coordinate.
					/*
					// Calculate the Zone coordinate of the Item.
					objectPixelPoint = MapToPixel(pItem->GetX(), pItem->GetY());
					objectPixelPoint.x += m_PlusPoint.x;// + pItem->GetSX();
					objectPixelPoint.y += m_PlusPoint.y;// + pItem->GetSY();

					// Convert the Zone coordinate to a screen coordinate.
					objectPixelPoint.x -= firstZonePixel.x;
					objectPixelPoint.y -= firstZonePixel.y;

					// Calculate the area where the Item exists on screen.
					frame = m_ItemTileFPK[ pItem->GetTileFrameID() ];

					// Position where the current Sprite is drawn on screen
					objectPixelPoint.x += frame.GetCX();
					objectPixelPoint.y += frame.GetCY();

					// Calculate the area that exists on screen.
					rect.left	= objectPixelPoint.x;
					rect.top	= objectPixelPoint.y;
					rect.right	= rect.left + m_ItemTileSPK[ frame.GetSpriteID() ].GetWidth();
					rect.bottom	= rect.top + m_ItemTileSPK[ frame.GetSpriteID() ].GetHeight();
					*/			

					//------------------------------------------------
					// If it is a Vampire, or
					// a Slayer who is in a situation where it can be seen..
					//------------------------------------------------
					if ((g_pPlayer->IsVampire()&&g_pZone->GetID() != 3001)
						|| (!g_pPlayer->IsVampire() && g_pPlayer->ShowInDarkness(currentX, currentY)) 
						|| g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
						|| g_bLight
#endif
						)
					{
						//------------------------------------------------
						// If it is a corpse
						// the corpse Creature's area must be looked at.
						//------------------------------------------------
						if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
						{
							#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
								DEBUG_ADD("corp");
							#endif

							MCorpse* pCorpse = (MCorpse*)pItem;

							MCreature* pCreature = pCorpse->GetCreature();

							if (pCreature!=NULL)
							{
								if (pCreature->IsPointInScreenRect(x,y))
								{
									#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
										DEBUG_ADD("reIT0");
									#endif

									return pItem;
								}
							}
						}
						else
						{
							// If the selected position falls within the Item's area
							if (pItem->IsPointInScreenRect(x,y))
							//if (x > rect.left && x < rect.right
								//&& y > rect.top && y < rect.bottom)
							{
								#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
									DEBUG_ADD("reIT");
								#endif

								return pItem;
							}					
						}
					}
				}				

			}
		}

		#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
			DEBUG_ADD("ckIO0");
		#endif

		//------------------------------------------------------
		// Those whose output time is less than currentY
		// is less than sY1.
		// m_mapImageObject is sorted with the Viewpoint as the key.
		//------------------------------------------------------
//		while (iImageObjectCheck != m_mapImageObject.rend())
//		{
//			MImageObject* const pImageObject = (MImageObject* const)((*iImageObjectCheck).second);
//
//				MInteractionObject* const pInteractionObject = (MInteractionObject* const)pImageObject;
//
//					/*
//
//
//							
//
//					*/			
//
//
//			iImageObjectCheck ++;
//		}
	}	

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
		DEBUG_ADD("ckIO");
	#endif

	//------------------------------------------------------
	// Those whose output time is less than currentY
	// is less than sY1.
	// m_mapImageObject is sorted with the Viewpoint as the key.
	//------------------------------------------------------
//	while (iImageObjectCheck != m_mapImageObject.rend())
//	{
//		MImageObject* const pImageObject = (MImageObject* const)((*iImageObjectCheck).second);
//
//			MInteractionObject* const pInteractionObject = (MInteractionObject* const)pImageObject;
//		
//			// Fit the picture's coordinate to the current screen coordinate
//			/*
//			objectPixelPoint.x = pInteractionObject->GetPixelX() - m_FirstZonePixel.x;
//			objectPixelPoint.y = pInteractionObject->GetPixelY() - m_FirstZonePixel.y;
//
//
//							
//
//			*/				
//
//
//		iImageObjectCheck ++;
//	}

	//------------------------------------------------------
	// If nothing is there.. also check GroundEffect.
	//------------------------------------------------------
	#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
		DEBUG_ADD("chkGE");
	#endif

	int effectNum = g_pZone->GetGroundEffectCount();
	MZone::EFFECT_MAP::const_iterator iEffect = g_pZone->GetGroundEffects();
//	MZone::EFFECT_MAP::const_iterator iEffectEnd = g_pZone->GetGroundEffectsEnd();
//
//	while(iEffect != iEffectEnd)
//	{
//		MEffect* pEffect = iEffect->second;
//
	for (int e=0; e<effectNum; e++)
	{
		MEffect* pEffect = iEffect->second;
		if(pEffect != NULL)
		{
			if (pEffect->IsSelectable()
				&& pEffect->IsPointInScreenRect(x, y))
			{
				#ifdef OUTPUT_DEBUG_DRAW_PROCESS_INPUT
					DEBUG_ADD("reGE");
				#endif

				return pEffect;			
			}
		}
		iEffect++;
	}

	if(IsRequestInfo())
	{
		//------------------------------------------------------
		// If nothing is there.. also check FakeCreature (Pet).
		//------------------------------------------------------
		int fakeCreatureNum = g_pZone->GetFakeCreatureNumber();
		MZone::CREATURE_MAP::const_iterator iFakeCreature = g_pZone->GetFakeCreatureBegin();
		for(int fc = 0; fc < fakeCreatureNum; fc++)
		{
			MFakeCreature *pFakeCreature = (MFakeCreature *)iFakeCreature->second;

			// Search only if it is a pet
			if(pFakeCreature->GetOwnerID() != OBJECTID_NULL && pFakeCreature->IsPointInScreenRect(x, y))
			{
				return pFakeCreature;
			}

			iFakeCreature++;
		}
	}

	return NULL;
}


//----------------------------------------------------------------------
// If the position that screen coordinate (x,y) points at is selected,
// which (sX,sY) Object of the Zone gets selected?
//
// Read the pixel information from the Sprite and compare.
//
//----------------------------------------------------------------------
MObject*
MTopView::GetSelectedObjectSprite(int x, int y)
{
	//--------------------------------------------------------------	
	// 1. Find the Sector (sX,sY) coordinate in the Zone of screen coordinate (x,y).
	// 
	// 2. If there is an Object near (sX,sY),
	//    find that Object's on-screen coordinate, and
	//    if (x,y) falls within it, "select it"
	//    otherwise, the next nearby Sector must be checked.
	//
	// (!) The Player is excluded. Check as if it does not exist.
	//--------------------------------------------------------------

	//--------------------------------------------------------------
	// Order in which nearby Sectors are searched
	//--------------------------------------------------------------	
	// - The lower the Sector a picture is in, the later it is drawn,
	//   so the higher its selection priority.
	// 
	//      .  .  .  
	//      .  X  . 
	//      .  .  .  
	//      .  .  .  
	//      .  .  .     When X is selected, Ground, Item etc... check range
	//
	//
	//               
	//         X    
	//               
	//      .  .  .  
	//      .  .  .  
	//      .  .  .  
	//      .  .  .     When Flying floats 64 pixels (two tiles) high..
	//      .  .  .     When X is selected, Flying... check range
	//
	//--------------------------------------------------------------

	//--------------------------------------------------------------
	// What has to be checked for each Sector
	//--------------------------------------------------------------
	/*

	   while ( ExistNextSector )
	   {			
			if ( IsExistObject )
			{
				if ( IsObjectCreature && ClickPoint In TheCreatureRect)
					Select Creature

				else if ( IsObjectItem && ClickPoint In TheItemRect )
					Select Item
			}			

			CurrentSector = NextSector
		}

		if ( Not Selected )
			return NULL;

		return Object;
	*/
	//--------------------------------------------------------------

	POINT firstZonePixel;	// Zone pixel coordinate that the first Sector on the screen stands for
	firstZonePixel = MapToPixel(m_FirstSector.x, m_FirstSector.y);
	// + g_DrawCamGap: back out the camera-interpolation bias, exact tick space
	firstZonePixel.x += m_PlusPoint.x + g_DrawCamGapX;
	firstZonePixel.y += m_PlusPoint.y + g_DrawCamGapY;


	POINT	pixelPoint;			// Pixel Position In Zone
	POINT	sectorPoint;		// Sector Position In Zone

	//--------------------------------------------------------------
	// Obtain the Zone pixel coordinate of (x,y).
	//--------------------------------------------------------------
	pixelPoint = MapToPixel(m_FirstSector.x, m_FirstSector.y);
	pixelPoint.x += m_PlusPoint.x + g_DrawCamGapX + x;
	pixelPoint.y += m_PlusPoint.y + g_DrawCamGapY + y;

	//--------------------------------------------------------------
	// Obtain the Zone sector coordinate of (x,y).
	//--------------------------------------------------------------						
	sectorPoint = PixelToMap(pixelPoint.x, pixelPoint.y);

	//--------------------------------------------------------------
	//
	// check
	//
	//--------------------------------------------------------------

	MObject*	pObject = NULL;	
	MCreature*	pCreature = NULL;
	MItem*		pItem	= NULL;
	CFrame			frame;
//	TYPE_SPRITEID	sprite;

	// Area of the Object
	POINT	objectPixelPoint;

	// Player Creature's ID
	TYPE_OBJECTID	pid = g_pPlayer->GetID();

	//std::ofstream file("log.txt");


	// Coordinate currently being checked
	int currentX;
	int currentY;

	int numCreature = 0;

	//--------------------------------------------------------------
	//
	// Flying Creature Check 
	//
	// : when at the same height
	//   can exist in a Sector lower than a normal Object.
	//
	//--------------------------------------------------------------
	int	sX1 = sectorPoint.x - 1,
			sX2 = sectorPoint.x + 1;
	int	sY1 = sectorPoint.y + 4,
			sY2 = sectorPoint.y + 5;

	//------------------------------------------------------
	// Skip if outside the Zone area...
	//------------------------------------------------------
	if (sX1 < 0) 
	{			
		sX1 = 0;	
	}

	if (sX2 >= m_pZone->GetWidth())
	{
		sX2 = m_pZone->GetWidth()-1;
	}

	if (sY1 < 0)
	{	
		sY1 = 0;	
	}

	if (sY2 >= m_pZone->GetHeight())
	{
		sY2 = m_pZone->GetHeight()-1;
	}


	//------------------------------------------------------	
	// Check the Sectors where it is likely to be.
	//------------------------------------------------------	
	for (currentY=sY2; currentY>=sY1; currentY--)	
	{	
		for (currentX=sX1; currentX<=sX2; currentX++)	
		{				
			const MSector& sector = m_pZone->GetSector(currentX, currentY);

			// If there is some Object
			if (sector.IsExistObject() )			
			{		

				//file << "Exist Object!" << endl;

				// Check differently depending on the Object type.
				// Only Creatures and Items can be selected.

				//------------------------------------------------
				//
				// If there is a Flying Creature
				//
				//------------------------------------------------
				numCreature = sector.GetFlyingCreatureSize();

				if (numCreature > 0)
				{
					OBJECT_MAP::const_iterator iCreature = sector.GetFlyingCreatureIterator();

					for (int c=0; c<numCreature; c++, iCreature++)
					{
						if (iCreature==sector.GetObjectEnd() 
							|| iCreature->first > MSector::POSITION_FLYINGCREATURE_MAX)
							break;

						pCreature = (MCreature*)iCreature->second;

						// Do not select it if it is the Player.
						if (pCreature->GetID()!=pid)	
						{
							// Obtain the area the FlyingCreature occupies.
							// The information probably has to be read from the FRAME_PACK!!
							// The FlyingCreature's Sector coordinate must be converted to a Pixel coordinate.

							// Calculate the Zone coordinate of the FlyingCreature.
							objectPixelPoint = MapToPixel(pCreature->GetX(), pCreature->GetY());
							objectPixelPoint.x += m_PlusPoint.x + pCreature->GetSX();
							objectPixelPoint.y += m_PlusPoint.y + pCreature->GetSY();

							// Convert the Zone coordinate to a screen coordinate.
							objectPixelPoint.x -= firstZonePixel.x;
							objectPixelPoint.y -= firstZonePixel.y;

							// Calculate the area where the Creature exists on screen.
							frame = m_CreatureFPK[pCreature->GetCreatureFrameID(0)][pCreature->GetAction()][pCreature->GetDirection()][pCreature->GetFrame()];

							// Position where the current Sprite is drawn on screen
							objectPixelPoint.x += frame.GetCX();
							objectPixelPoint.y += frame.GetCY() - pCreature->GetZ();//FLYINGCREATURE_HEIGHT;
												// Since the height is set in the FramePack,
												// the Flying case is the same.

							// Check whether the mouse coordinate belongs to the Sprite
							// When the coordinate in the Sprite has a color
							if (m_CreatureSPK[ frame.GetSpriteID() ].IsColorPixel( 
									static_cast<short>(x - objectPixelPoint.x), 
									static_cast<short>(y - objectPixelPoint.y) 
								))
							{
								return pCreature;
							}				
						}
					}
				}	
			}
		}
	}


	//--------------------------------------------------------------
	//
	// Creature / Item Check data
	//
	//--------------------------------------------------------------
	sY1 = sectorPoint.y - 1,
	sY2 = sectorPoint.y + 3;

	//------------------------------------------------------
	// Skip if outside the Zone area...
	//------------------------------------------------------	
	if (sY1 < 0)
	{	
		sY1 = 0;	
	}

	if (sY2 >= m_pZone->GetHeight())
	{
		sY2 = m_pZone->GetHeight()-1;
	}

	//------------------------------------------------------
	// ImageObject Iterator subject to Check
	//------------------------------------------------------
	IMAGEOBJECT_OUTPUT_MAP::reverse_iterator iImageObjectCheck
		= m_mapImageObject.rbegin();

	//------------------------------------------------------
	// Check the ImageObjects whose output time
	// is less than sY1.
	// m_mapImageObject is sorted with the Viewpoint as the key.
	//------------------------------------------------------
	while (iImageObjectCheck != m_mapImageObject.rend())
	{		

//			MInteractionObject* const pInteractionObject = (MInteractionObject* const)pImageObject;
//
//
//						
//									

		iImageObjectCheck ++;
	}

	//------------------------------------------------------	
	// Check the Sectors where it is likely to be.
	//------------------------------------------------------	
	for (currentY=sY2; currentY>=sY1; currentY--)	
	{	
		for (currentX=sX1; currentX<=sX2; currentX++)	
		{				
			const MSector& sector = m_pZone->GetSector(currentX, currentY);

			// If there is some Object
			if (sector.IsExistObject() )			
			{		

				//file << "Exist Object!" << endl;

				// Check differently depending on the Object type.
				// Only Creatures and Items can be selected.

				//------------------------------------------------
				//
				// If there is a Flying Creature
				//
				//------------------------------------------------
				numCreature = sector.GetFlyingCreatureSize();

				if (numCreature > 0)
				{
					OBJECT_MAP::const_iterator iCreature = sector.GetFlyingCreatureIterator();

					for (int c=0; c<numCreature; c++, iCreature++)
					{
						if (iCreature==sector.GetObjectEnd() 
							|| iCreature->first > MSector::POSITION_FLYINGCREATURE_MAX)
							break;

						pCreature = (MCreature*)iCreature->second;

						// Do not select it if it is the Player.
						if (pCreature->GetID()!=pid)	
						{

							// Obtain the area the FlyingCreature occupies.
							// The information probably has to be read from the FRAME_PACK!!
							// The FlyingCreature's Sector coordinate must be converted to a Pixel coordinate.

							// Calculate the Zone coordinate of the FlyingCreature.
							objectPixelPoint = MapToPixel(pCreature->GetX(), pCreature->GetY());
							objectPixelPoint.x += m_PlusPoint.x + pCreature->GetSX();
							objectPixelPoint.y += m_PlusPoint.y + pCreature->GetSY();

							// Convert the Zone coordinate to a screen coordinate.
							objectPixelPoint.x -= firstZonePixel.x;
							objectPixelPoint.y -= firstZonePixel.y;

							// Calculate the area where the Creature exists on screen.
							frame = m_CreatureFPK[pCreature->GetCreatureFrameID(0)][pCreature->GetAction()][pCreature->GetDirection()][pCreature->GetFrame()];

							// Position where the current Sprite is drawn on screen
							objectPixelPoint.x += frame.GetCX();
							objectPixelPoint.y += frame.GetCY() - pCreature->GetZ();//FLYINGCREATURE_HEIGHT;
												// Since the height is set in the FramePack,
												// the Flying case is the same.

							// Check whether the mouse coordinate belongs to the Sprite
							// When the coordinate in the Sprite has a color
							if (m_CreatureSPK[ frame.GetSpriteID() ].IsColorPixel( 
									static_cast<short>(x - objectPixelPoint.x), 
									static_cast<short>(y - objectPixelPoint.y) 
								))
							{
								return pCreature;
							}
						}
					}
				}				

				//------------------------------------------------
				//
				// If there is a Creature
				//
				//------------------------------------------------
				numCreature = sector.GetGroundCreatureSize();

				if (numCreature > 0)
				{
					OBJECT_MAP::const_iterator iCreature = sector.GetGroundCreatureIterator();

					for (int c=0; c<numCreature; c++, iCreature++)
					{
						if (iCreature==sector.GetObjectEnd() 
							|| iCreature->first > MSector::POSITION_GROUNDCREATURE_MAX)
							break;

						pCreature = (MCreature*)iCreature->second;

						// Do not select it if it is the Player.
						if (pCreature->GetID()!=pid)	
						{

							// Obtain the area the Creature occupies.
							// The information probably has to be read from the FRAME_PACK!!
							// The Creature's Sector coordinate must be converted to a Pixel coordinate.

							// Calculate the Zone coordinate of the Creature.
							objectPixelPoint = MapToPixel(pCreature->GetX(), pCreature->GetY());
							objectPixelPoint.x += m_PlusPoint.x + pCreature->GetSX();
							objectPixelPoint.y += m_PlusPoint.y + pCreature->GetSY();

							// Convert the Zone coordinate to a screen coordinate.
							objectPixelPoint.x -= firstZonePixel.x;
							objectPixelPoint.y -= firstZonePixel.y;

							// Calculate the area where the Creature exists on screen.
							frame = m_CreatureFPK[pCreature->GetCreatureFrameID(0)][pCreature->GetAction()][pCreature->GetDirection()][pCreature->GetFrame()];

							// Position where the current Sprite is drawn on screen
							objectPixelPoint.x += frame.GetCX();
							objectPixelPoint.y += frame.GetCY();

							// Check whether the mouse coordinate belongs to the Sprite
							// When the coordinate in the Sprite has a color
							if (m_CreatureSPK[ frame.GetSpriteID() ].IsColorPixel( 
									static_cast<short>(x - objectPixelPoint.x), 
									static_cast<short>(y - objectPixelPoint.y) 
								))
							{
								return pCreature;
							}
						}
					}
				}

				//------------------------------------------------
				//
				// If there is an Underground Creature
				//
				//------------------------------------------------
				numCreature = sector.GetUndergroundCreatureSize();

				if (numCreature > 0)
				{
					OBJECT_MAP::const_iterator iCreature = sector.GetUndergroundCreatureIterator();

					for (int c=0; c<numCreature; c++, iCreature++)
					{
						if (iCreature==sector.GetObjectEnd() 
							|| iCreature->first > MSector::POSITION_UNDERGROUNDCREATURE_MAX)
							break;

						pCreature = (MCreature*)iCreature->second;

						// Do not select it if it is the Player.
						if (pCreature->GetID()!=pid)	
						{

							// Obtain the area the Creature occupies.
							// The information probably has to be read from the FRAME_PACK!!
							// The Creature's Sector coordinate must be converted to a Pixel coordinate.

							// Calculate the Zone coordinate of the Creature.
							objectPixelPoint = MapToPixel(pCreature->GetX(), pCreature->GetY());
							objectPixelPoint.x += m_PlusPoint.x + pCreature->GetSX();
							objectPixelPoint.y += m_PlusPoint.y + pCreature->GetSY();

							// Convert the Zone coordinate to a screen coordinate.
							objectPixelPoint.x -= firstZonePixel.x;
							objectPixelPoint.y -= firstZonePixel.y;



							// Check whether the mouse coordinate belongs to the Sprite
							// When the coordinate in the Sprite has a color
							if (m_EtcSPK[ SPRITEID_CREATURE_BURROW ].IsColorPixel( 
									static_cast<short>(x - objectPixelPoint.x), 
									static_cast<short>(y - objectPixelPoint.y) 
								))
							{
								return pCreature;
							}
						}
					}
				}

				//------------------------------------------------
				//
				// If there is an Item
				//
				//------------------------------------------------				
				pObject = (MObject*)sector.GetItem();

				if (pObject != NULL)
				{	
					//file << "Exist Item!" << endl;

					pItem = (MItem*)pObject;

					// Obtain the area the Item occupies.
					// The information probably has to be read from the FRAME_PACK!!
					// The Item's Sector coordinate must be converted to a Pixel coordinate.

					// Calculate the Zone coordinate of the Item.
					objectPixelPoint = MapToPixel(pItem->GetX(), pItem->GetY());
					objectPixelPoint.x += m_PlusPoint.x;// + pItem->GetSX();
					objectPixelPoint.y += m_PlusPoint.y;// + pItem->GetSY();

					// Convert the Zone coordinate to a screen coordinate.
					objectPixelPoint.x -= firstZonePixel.x;
					objectPixelPoint.y -= firstZonePixel.y;

					// Calculate the area where the Item exists on screen.
					frame = m_ItemTileFPK[ pItem->GetTileFrameID() ];

					// Position where the current Sprite is drawn on screen
					objectPixelPoint.x += frame.GetCX();
					objectPixelPoint.y += frame.GetCY();

					// Check whether the mouse coordinate belongs to the Sprite
					// When the coordinate in the Sprite has a color
					if (m_ItemTileISPK[ frame.GetSpriteID() ].IsColorPixel( 
							static_cast<short>(x - objectPixelPoint.x), 
							static_cast<short>(y - objectPixelPoint.y) 
						))
					{

						return pItem;
					}					
				}				

			}
		}

		//------------------------------------------------------
		// Those whose output time is less than currentY
		// is less than sY1.
		// m_mapImageObject is sorted with the Viewpoint as the key.
		//------------------------------------------------------
		while (iImageObjectCheck != m_mapImageObject.rend())
		{

//				MInteractionObject* const pInteractionObject = (MInteractionObject* const)pImageObject;
//
//
//							
//										

			iImageObjectCheck ++;
		}
	}	

	//------------------------------------------------------
	// Those whose output time is less than currentY
	// is less than sY1.
	// m_mapImageObject is sorted with the Viewpoint as the key.
	//------------------------------------------------------
	while (iImageObjectCheck != m_mapImageObject.rend())
	{

//			MInteractionObject* const pInteractionObject = (MInteractionObject* const)pImageObject;
//		
//
//							
//									

		iImageObjectCheck ++;
	}

	return NULL;
}

//----------------------------------------------------------------------
// Clear ItemNameList
//----------------------------------------------------------------------
// Everything must be erased.
//----------------------------------------------------------------------
void			
MTopView::ClearItemNameList()
{
	DRAWITEMNAME_LIST::iterator iItemName = m_listDrawItemName.begin();

	// Erase all nodes.
	while (iItemName != m_listDrawItemName.end())
	{
		DRAWITEMNAME_NODE* pNode = *iItemName;

		// Remove the node from memory
		delete pNode;

		iItemName ++;
	}

	m_listDrawItemName.clear();

}

//----------------------------------------------------------------------
// Add ItemName
//----------------------------------------------------------------------
// new must be done from outside.
//
// Must be added sorted by pNode->GetDistance.
// Ascending order..
//----------------------------------------------------------------------
// When there are many items, checking from the back is faster.
// Because the MAX value is fixed...
// But.. can't be bothered to fix it.. --;
// I could have fixed it in the time it took to type this.. hmm..
//----------------------------------------------------------------------
void		 	
MTopView::AddItemName(DRAWITEMNAME_NODE* pNode)
{		
	DRAWITEMNAME_LIST::iterator iItemName = m_listDrawItemName.begin();

	BOOL bInsertEnd = TRUE;

	//-------------------------------------------------------
	// Add the ItemName while comparing distances.
	//-------------------------------------------------------
	while (iItemName != m_listDrawItemName.end())
	{
		DRAWITEMNAME_NODE* pOldNode = *iItemName;

		//------------------------------------------
		// Compare compare~
		//------------------------------------------
		if (pNode->GetDistance() < pOldNode->GetDistance())
		{			
			m_listDrawItemName.insert( iItemName, pNode );

			bInsertEnd = FALSE;

			break;
		}

		iItemName ++;
	}

	if (bInsertEnd)
	{
		// Add at the very end
		m_listDrawItemName.push_back( pNode );
	}

	//-------------------------------------------------------
	// If it exceeded MAX
	//-------------------------------------------------------
	if (m_listDrawItemName.size() > static_cast<size_t>(g_pClientConfig->MAX_DRAWITEMNAME))
	{
		// Remove the last one.
		DRAWITEMNAME_NODE* pDeleteNode = m_listDrawItemName.back();

		delete pDeleteNode;

		m_listDrawItemName.pop_back();
	}
}

//----------------------------------------------------------------------
// DrawItemNameList
//----------------------------------------------------------------------
// Do AddText for each Item.
//----------------------------------------------------------------------
void			
MTopView::DrawItemNameList()
{
	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "Start DrawItemNameList" );
	#endif


	DRAWITEMNAME_LIST::iterator iItemName = m_listDrawItemName.begin();

	int x, x2;
	int y = 0, y2;

	// When drawing the minimap..
	// Because of the coordinates...
	// Draw from the bottom..

	// Commented out 2001.7.14
	//if (g_pUserOption->DrawMinimap)// && g_pPlayer->IsSlayer())
	{
		// hehe hard-coding~~
		y = 160;
	}

	while (iItemName != m_listDrawItemName.end())
	{
		DRAWITEMNAME_NODE* pNode = *iItemName;

		MItem* pItem = pNode->GetItem();


		const char* pItemName = pItem->GetName();

		// Ones with an option are blue..
		COLORREF nameColor;
		if (pItem->IsSpecialColorItem())
		{
			WORD temp_color;

			temp_color = CIndexSprite::ColorSet[pItem->GetSpecialColorItemColorset()][16];
			nameColor = RGB(CSDLGraphics::Red(temp_color)<<3, CSDLGraphics::Green(temp_color)<<3, CSDLGraphics::Blue(temp_color)<<3);
		}
		else
		if (pItem->IsEmptyItemOptionList())
		{
			nameColor = m_ColorNameItem;
		}
		else if(pItem->GetItemOptionListCount() ==2)
		{
			nameColor = g_pClientConfig->COLOR_NAME_ITEM_RARE_OPTION;
		}
		// add by Sonic 2006.10.28 added display of three-attribute equipment in red
		else if(pItem->GetItemOptionListCount() > 2)
		{
			nameColor = g_pClientConfig->COLOR_NAME_VAMPIRE; //Red
		}
		// end by Sonic 2006.10.28 added display of three-attribute equipment in red
		else
		{
			nameColor = m_ColorNameItemOption;
		}

		if (pItemName != NULL)
		{
			// So that the text fits inside the screen..
			x = g_GameRect.right - g_GetStringWidth( pItemName, g_ClientPrintInfo[FONTID_ITEM]->hfont );
			x2 = g_GameRect.right;

			y2 = y + g_pClientConfig->FONT_ITEM_HEIGHT;

			// Black box area
			RECT rect = {
				x, 
				y,
				x2,
				y2
			};

			// If it is the Item selected with the mouse
			if (m_SelectItemID == pItem->GetID())		
			{
				DRAWTEXT_NODE* pTextNodeBK = new DRAWTEXT_NODE(
										x,
										y+1,
										pItemName,										
										0,		// black
										FONTID_ITEM
				);

				AddText( pTextNodeBK );

				DRAWTEXT_NODE* pTextNode = new DRAWTEXT_NODE(
										x+1,
										y+1+1,
										pItemName,										
										nameColor,
										FONTID_ITEM
				);

				AddText( pTextNode );
			}
			else
			{
				DRAWTEXT_NODE* pTextNode = new DRAWTEXT_NODE(
											x,
											y+1,
											pItemName,										
											nameColor,
											FONTID_ITEM
				);

				pTextNode->SetBox( rect, m_ColorUIBoxBg );		

				AddText( pTextNode );
			}	

			// Selection area of the item Name
			pNode->SetRect( rect );
		}

		// Next..
		iItemName ++;
		y += g_pClientConfig->FONT_ITEM_HEIGHT;
	}

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "End DrawItemNameList" );
	#endif	

}

//----------------------------------------------------------------------
// Select Item Name
//----------------------------------------------------------------------
// When screen coordinate (x,y) is selected
// Is there an ItemName at that coordinate?
//----------------------------------------------------------------------
MItem*
MTopView::SelectItemName(int x, int y)
{
	DRAWITEMNAME_LIST::iterator iItemName = m_listDrawItemName.begin();

	while (iItemName != m_listDrawItemName.end())
	{
		DRAWITEMNAME_NODE* pNode = *iItemName;

		//--------------------------------------------
		// If it falls within the ItemName's selection area?
		//--------------------------------------------
		if (pNode->IsPointInRect(x,y))
		{			
			// Hand over the item pointer.
			// It may not be in the zone, so...
			// it has to be read from the zone.
			return m_pZone->GetItem( pNode->GetID() );		
		}

		// Next item name
		iItemName ++;
	}

	return NULL;
}

//----------------------------------------------------------------------
// Clear TextList
//----------------------------------------------------------------------
// Everything must be erased.
//----------------------------------------------------------------------
void			
MTopView::ClearTextList()
{
	while (!m_pqDrawText.empty())
	{
		DRAWTEXT_NODE* pNode = m_pqDrawText.top();

		// Remove the node from memory
		delete pNode;

		m_pqDrawText.pop();
	}
}

//----------------------------------------------------------------------
// Add Text
//----------------------------------------------------------------------
// Adds the information of the string to output to the DrawTextList.
// new must be done from outside.
//----------------------------------------------------------------------
void			
MTopView::AddText(DRAWTEXT_NODE* pNode)
{
	/*
	const char* str = pNode->GetString();

	if (str==NULL)
	{
		delete pNode;
		return;
	}
	else if (str[0]=='\0')
	{
		delete pNode;
		return;
	}
	*/

	// Have it drawn later
	if (pNode->GetTextTime()==0)
	{
		QWORD time = ((QWORD)(g_CurrentFrame+160) << 41) 
							+ m_pqDrawText.size();

		pNode->SetTextTime( time );
	}

	m_pqDrawText.push( pNode );
}

//----------------------------------------------------------------------
// DrawTextList
//----------------------------------------------------------------------
// Assumes output happens in the Unlock state.
//----------------------------------------------------------------------
void			
MTopView::DrawTextList()
{
	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "Start DrawTextList" );
	#endif

	//DRAWTEXT_PQ::iterator iText = m_pqDrawText.begin();

	//--------------------------------------------------------
	// If using 3D acceleration...
	// For now, darken it with a swoosh.. --;
	// To use Lock/Unlock less..
	//
	// But!
	// When drawing text, the box must be drawn just before. Ugh...
	//--------------------------------------------------------
	/*
	if (true)
	{
		m_pSurface->Unlock();

		// Erase all nodes.
		while (iText != m_pqDrawText.end())
		{
			DRAWTEXT_NODE* pNode = *iText;

			//--------------------------------------------------------
			// Draw a black box where the text is output
			//--------------------------------------------------------
			RECT rect = { pNode->GetX(), pNode->GetY(), 
						pNode->GetXPlusWidth(), pNode->GetY(),15 };

			// Draw the black box..
			DrawBox3D(&rect, 0x7000);			

			iText++;
		}

		m_pSurface->Lock();	

		// Back to the start..
		iText = m_pqDrawText.begin();
	}
	*/

	//m_pSurface->Unlock();
	//--------------------------------------------------------
	// Output and erase all nodes.
	//--------------------------------------------------------
	RECT rect2;

	while (!m_pqDrawText.empty())
	{
		DRAWTEXT_NODE* pNode = m_pqDrawText.top();

		//--------------------------------------------------------
		// Should the Box be drawn?
		//--------------------------------------------------------
		if (pNode->IsExistBox())
		{
			//--------------------------------------------------------
			// Draw a black box where the text is output
			//--------------------------------------------------------
			RECT rect = pNode->GetBox();

			//--------------------------------------------------------
			// If it is a transparent box
			//--------------------------------------------------------
			if (pNode->IsTransBox())
			{
				//--------------------------------------------------------
				// In the case of 3D acceleration
				//--------------------------------------------------------
//
//
//					// Lay the base
//					WORD boxColor = pNode->GetBoxColor();
//
//					rect2 = rect;
//					DrawBox3D(&rect2, boxColor);
//
//
//						
//						// XXXXXX
//						// XXXXXX
//						// .XXXX.
//						// .XXXX.
//						// ..XX..
//						// ..XX..
//
//
//
//
//
				{	
					WORD boxColor = pNode->GetBoxColor();

					if (pNode->IsExistBoxOutline())
					{
						WORD color = pNode->GetBoxOutlineColor();

						// Draw the outer box
						rect2 = rect;
						DRAW_CHAT_BOX_OUTLINE( rect2, color )

						// Lock
						m_pSurface->Lock();

						WORD boxColor = pNode->GetBoxColor();

						// Draw the base
						rect2 = rect;
						DRAW_ALPHA_BOX_2D( rect2, boxColor );

						// If it is black
						if (boxColor==0)
						{
							// XXXXXX
							// XXXXXX
							// .XXXX.
							// .XXXX.
							// ..XX..
							// ..XX..

							// 2D 5:6:5
							if (CSDLGraphics::Is565())
							{
								rect2 = rect;
								//m_pSurface->GammaBox565(&rect2, 15);

								rect2.left	= rect.left + CHAT_BOX_TAIL_X;
								rect2.top	= rect.bottom;
								rect2.right = rect.left + CHAT_BOX_TAIL_X + 6;
								rect2.bottom = rect.bottom + 2;								
								//m_pSurface->GammaBox565(&rect2, 15);

								rect2.left	= rect.left + CHAT_BOX_TAIL_X+1;
								rect2.top	= rect.bottom + 2;
								rect2.right = rect.left + CHAT_BOX_TAIL_X+1 + 4;
								rect2.bottom = rect.bottom + 2 + 2;
								//m_pSurface->GammaBox565(&rect2, 15);

								rect2.left	= rect.left + CHAT_BOX_TAIL_X+2;
								rect2.top	= rect.bottom + 4;
								rect2.right = rect.left + CHAT_BOX_TAIL_X+2 + 2;
								rect2.bottom = rect.bottom + 4 + 2;
								//m_pSurface->GammaBox565(&rect2, 15);								
							}
							// 2D 5:5:5
							else
							{
								rect2 = rect;
								//m_pSurface->GammaBox555(&rect2, 15);

								rect2.left	= rect.left + CHAT_BOX_TAIL_X;
								rect2.top	= rect.bottom;
								rect2.right = rect.left + CHAT_BOX_TAIL_X + 6;
								rect2.bottom = rect.bottom + 2;								
								//m_pSurface->GammaBox555(&rect2, 15);

								rect2.left	= rect.left + CHAT_BOX_TAIL_X+1;
								rect2.top	= rect.bottom + 2;
								rect2.right = rect.left + CHAT_BOX_TAIL_X+1 + 4;
								rect2.bottom = rect.bottom + 2 + 2;
								//m_pSurface->GammaBox555(&rect2, 15);

								rect2.left	= rect.left + CHAT_BOX_TAIL_X+2;
								rect2.top	= rect.bottom + 4;
								rect2.right = rect.left + CHAT_BOX_TAIL_X+2 + 2;
								rect2.bottom = rect.bottom + 4 + 2;
								//m_pSurface->GammaBox555(&rect2, 15);
							}					


						}
						// If it is another color
						else
						{
							m_pSurface->BltColorAlpha(&rect, boxColor, 15);
						}

						// Line
						DRAW_CHAT_BOX_TAIL_LOCKED( rect, color );

						m_pSurface->Unlock();						
					}
					else
					{
						m_pSurface->Lock();

						DRAW_ALPHA_BOX_2D( rect, boxColor );

						m_pSurface->Unlock();
					}					
				}
			}
			//--------------------------------------------------------
			// If it is not a transparent box
			//--------------------------------------------------------
			else
			{
				m_pSurface->FillRect(&rect, pNode->GetBoxColor());				
			}
		}

		//--------------------------------------------------------
		// If there is text to output..
		//--------------------------------------------------------
		const char* pString = pNode->GetString();

		if (pString!=NULL && pString[0]!='\0')
		{
			//--------------------------------------------------------
			// Output coordinate
			//--------------------------------------------------------
			int x = pNode->GetX();
			int y = pNode->GetY();

			//--------------------------------------------------------
			// font setting
			//--------------------------------------------------------	
			PrintInfo* pPrintInfo = g_ClientPrintInfo[pNode->GetFont()];				

			//--------------------------------------------------------
			// Processing according to the flag must also be done....
			//--------------------------------------------------------
			if (pNode->IsOutline())
			{
				// Set inverse color
				pPrintInfo->text_color = (~pNode->GetColor()) & 0x00FFFFFF;

				// Output
				g_Print(x-1, y-1, pString, pPrintInfo);
				g_Print(x+1, y+1, pString, pPrintInfo);
			}

			// Set text color
			pPrintInfo->text_color = pNode->GetColor();

			//--------------------------------------------------------
			// Output string
			//--------------------------------------------------------
			g_Print(x, y, pString, pPrintInfo);
		}

		// Remove from memory.
		delete pNode;

		// Next..
		m_pqDrawText.pop();
	}

	// Remove all nodes.
	//m_pqDrawText.clear();

	// Lock state
	//m_pSurface->Lock();

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "End DrawTextList" );
	#endif
}

//----------------------------------------------------------------------
// Clear LightBufferFilter
//----------------------------------------------------------------------
// Initializes the LightBuffer.
// By default the Player's sight can be shown.
//----------------------------------------------------------------------
void
MTopView::ClearLightBufferFilter3D()
{
	//if (true && 
	//if	(m_DarkBits)
	{
		//--------------------------------------------------
		// Set the light value
		//--------------------------------------------------
		BYTE DarkColor;// = 0;
		///*
		//int maxLight[MAX_LIGHT3D_SET]	= { 7, 11, 14 };
		//int maxCenter[MAX_LIGHT3D_SET]	= { 3, 6, 9 };
		/*
		switch (m_DarkBits)
		{
			case 0 : DarkColor = 0; break;
			case 1 : DarkColor = 4; break;
			case 2 : DarkColor = 8; break;
			case 3 : DarkColor = 14; break;
			case 4 : DarkColor = 14; break;
		}
		*/

		//DarkColor = m_DarkBits;
		int darkBits = m_DarkBits;

		if (g_pPlayer->IsInDarkness() && (!g_pPlayer->HasEffectStatus( EFFECTSTATUS_LIGHTNESS )||g_pZone->GetID() == 3001 ))
		{
			darkBits = 15;
		}

		DarkColor = max(0, darkBits - (g_pPlayer->GetLightSight() - g_pPlayer->GetTimeLightSight()));

		//--------------------------------------------------
		// Initialization - light that fills the whole screen..
		//--------------------------------------------------
		BYTE* pBuffer;

		int i = SCREENLIGHT_HEIGHT-1;
		do {		
			pBuffer = m_LightBufferFilter.GetFilter( i );
			memset(pBuffer, DarkColor, SCREENLIGHT_WIDTH);				
		} while (i--);

		m_nLight = 0;
	}
}

//----------------------------------------------------------------------
// Clear LightBufferFilter
//----------------------------------------------------------------------
// Initializes the LightBuffer.
// By default the Player's sight can be shown.
//----------------------------------------------------------------------
void
MTopView::ClearLightBufferFilter2D()
{
	//if (1)//true && m_b3DLight && m_DarkBits)
	{
		//--------------------------------------------------
		// Set the light value
		//--------------------------------------------------
		BYTE DarkColor;// = 0;
		///*
		//int maxLight[MAX_LIGHT3D_SET]	= { 7, 11, 14 };
		//int maxCenter[MAX_LIGHT3D_SET]	= { 3, 6, 9 };
		/*
		switch (m_DarkBits)
		{
			case 0 : DarkColor = 31; break;		// 31-0
			case 1 : DarkColor = 23; break;	// 31-4*2
			case 2 : DarkColor = 15; break;	// 31-8*2
			case 3 : DarkColor = 5; break;	// 31-14*2
			case 4 : DarkColor = 5; break;	// 31-14*2
		}
		*/
		int darkBits = m_DarkBits;

		if (g_pPlayer->IsInDarkness() && (!g_pPlayer->HasEffectStatus( EFFECTSTATUS_LIGHTNESS) ||g_pZone->GetID() == 3001))
		{
			darkBits = 15;
		}

		DarkColor = 31 - (darkBits<<1);

		DarkColor = max(0, DarkColor - ((g_pPlayer->GetLightSight() - g_pPlayer->GetTimeLightSight())<<1));

		//--------------------------------------------------
		// Check if filter is initialized (SDL backend defensive check)
		//--------------------------------------------------
		if (m_LightBufferFilter.IsNotInit()) {
			m_nLight = 0;
			return;
		}

		//--------------------------------------------------
		// Initialization - light that fills the whole screen..
		//--------------------------------------------------
		BYTE* pBuffer;

		int i = SCREENLIGHT_HEIGHT-1;
		do {
			pBuffer = m_LightBufferFilter.GetFilter( i );
			memset(pBuffer, DarkColor, SCREENLIGHT_WIDTH);
		} while (i--);

		m_nLight = 0;
	}
}

//----------------------------------------------------------------------
// Add LightFilter To Screen
//----------------------------------------------------------------------
// Adds a LightFilter.
// The Screen coordinate must be converted to fit the LightBuffer.
//
// (CLIPSURFACE_WIDTH, CLIPSURFACE_WIDTH) 
//  --> to (SCREENLIGHT_WIDTH, SCREENLIGHT_HEIGHT)..
//----------------------------------------------------------------------
void			
MTopView::AddLightFilter2D(int x, int y, BYTE range, bool bMapPixel, bool bForceLight)
{
	if (m_LightFTP.GetSize() == 0) return;
	if (!bForceLight)
	{
		range = min( max(g_pPlayer->GetSight()-3, 0), range );
	}

	if (//true && m_b3DLight && 
		(!g_pPlayer->IsInDarkness()
		&& range!=0 
		&& (m_DarkBits || g_pPlayer->IsInDarkness()) 
		&& m_nLight < g_pClientConfig->MAX_LIGHT_DRAW)
		|| bForceLight) // Light that must always be output
	{

		// Do not go beyond the range of the Light Filter.
		if (range >= m_LightFTP.GetSize())
		{
			range = m_LightFTP.GetSize()-1;
		}		

		//-----------------------------------------------------
		// The lightFilter to output must be decided according to the range.
		//-----------------------------------------------------		
		int filterID = range;

		//-----------------------------------------------------
		// Decide the LightFilter output position
		/// And, it must be converted to filter coordinate (fx,fy)..
		// the filter's center must be fitted to come at (x,y).
		//-----------------------------------------------------
		int fx, fy;

		// The calculation differs depending on which coordinate (x,y) is.
		if (bMapPixel)
		{
			// mapPixel --> screen
			POINT screenPoint = PixelToScreen(x,y);	

			// screen -> filter
			fx = static_cast<int>((float)screenPoint.x / s_LightWidth);
			fy = static_cast<int>((float)screenPoint.y / s_LightHeight);
		}
		else
		{
			// screen -> filter
			fx = static_cast<int>((float)x / s_LightWidth);
			fy = static_cast<int>((float)y / s_LightHeight);
		}

		// Fit the filter center
		fx = fx - (m_LightFTP[filterID].GetWidth() >> 1);
		fy = fy - (m_LightFTP[filterID].GetHeight() >> 1);

		//-----------------------------------------------------
		// Adds a Light filter to the BufferFilter.
		//-----------------------------------------------------
		m_LightBufferFilter.BltFilterAdd( fx, fy, m_LightFTP[filterID] );		

		m_nLight++;
	}
}

//----------------------------------------------------------------------
// Add LightFilter To Screen
//----------------------------------------------------------------------
// Adds a LightFilter.
// The Screen coordinate must be converted to fit the LightBuffer.
//
// (CLIPSURFACE_WIDTH, CLIPSURFACE_WIDTH) 
//  --> to (SCREENLIGHT_WIDTH, SCREENLIGHT_HEIGHT)..
//----------------------------------------------------------------------
void			
MTopView::AddLightFilter3D(int x, int y, BYTE range, bool bMapPixel, bool bForceLight)
{
	if (m_LightFTP.GetSize() == 0) return;
	if (!bForceLight)
	{
		range = min( max(g_pPlayer->GetSight()-3, 0), range );
	}

	if (//true && 
		(!g_pPlayer->IsInDarkness()
		&& range!=0
		&&	(m_DarkBits	|| g_pPlayer->IsInDarkness()) 
		&& m_nLight < g_pClientConfig->MAX_LIGHT_DRAW)
		|| bForceLight)	// Light that must always be output
	{

		// Do not go beyond the range of the Light Filter.
		if (range >= m_LightFTP.GetSize())
		{
			range = m_LightFTP.GetSize()-1;
		}		

		//-----------------------------------------------------
		// The lightFilter to output must be decided according to the range.
		//-----------------------------------------------------		
		int filterID = range;

		//-----------------------------------------------------
		// Decide the LightFilter output position
		/// And, it must be converted to filter coordinate (fx,fy)..
		// the filter's center must be fitted to come at (x,y).
		//-----------------------------------------------------
		int fx, fy;

		// The calculation differs depending on which coordinate (x,y) is.
		if (bMapPixel)
		{
			// mapPixel --> screen
			POINT screenPoint = PixelToScreen(x,y);	

			// screen -> filter
			fx = static_cast<int>((float)screenPoint.x / s_LightWidth);
			fy = static_cast<int>((float)screenPoint.y / s_LightHeight);
		}
		else
		{
			// screen -> filter
			fx = static_cast<int>((float)x / s_LightWidth);
			fy = static_cast<int>((float)y / s_LightHeight);
		}

		// Fit the filter center
		fx = fx - (m_LightFTP[filterID].GetWidth() >> 1);
		fy = fy - (m_LightFTP[filterID].GetHeight() >> 1);

		//-----------------------------------------------------
		// Adds a Light filter to the BufferFilter.
		//-----------------------------------------------------
		m_LightBufferFilter.BltFilterSub( fx, fy, m_LightFTP[filterID] );

		m_nLight++;
	}
}

//----------------------------------------------------------------------
// Add CreatureOutput All
//----------------------------------------------------------------------
// Add the whole Creature map of m_pZone in one go~
//----------------------------------------------------------------------
// The ID in the OutputMap is the Creature's..
//
//    consists of 2 bytes + 2 bytes    = 4 bytes.
// [pixel y coordinate][object id]
//
// For a normal creature, whether to add it is decided by a number..
// A FakeCreature is not added to the tile.. so it must always be added here.
//----------------------------------------------------------------------
void	
MTopView::AddOutputCreatureAll()
{
	// Initialization
	m_mapCreature.clear();

	// [New skill 3] While in a coffin, only oneself is drawn.
	if (g_pPlayer->IsInCasket())
	{
		m_bTileSearchForCreature = false;	

		QWORD key = GetOutputCreatureID(g_pPlayer);

		// Add (key value, pCreature).
		m_mapCreature.insert(
			CREATURE_OUTPUT_MAP::value_type( key, g_pPlayer )
		);

		return;
	}


	MZone::CREATURE_MAP::const_iterator iCreature;

	MCreature* pCreature;

	int num = m_pZone->GetCreatureNumber();

	// If there are 20 or more...
	// Judged it is better to just search.. really? - -;
	if (num >= 20)
	{
//		m_bTileSearchForCreature = true;		
		m_bTileSearchForCreature = false;		
	}
	else
	{
		m_bTileSearchForCreature = false;
	}

	//---------------------------------------------------------
	// Unless the Tiles are searched for output..
	// All Creatures must be added to the OutputMap.
	//---------------------------------------------------------
	if (!m_bTileSearchForCreature)
	{
		iCreature = m_pZone->GetCreatureBegin();

		for (int i=0; i<num; i++)
		{
			pCreature = iCreature->second;

			// key value = [pixel Y coordinate][object id]
			QWORD key = GetOutputCreatureID(pCreature);

			// Add (key value, pCreature).
			m_mapCreature.insert(
				CREATURE_OUTPUT_MAP::value_type( key, pCreature )
			);

			// Next Creature
			iCreature++;
		}
	}

	//---------------------------------------------------------
	// Fake Creature
	//---------------------------------------------------------
	iCreature = m_pZone->GetFakeCreatureBegin();
	num = m_pZone->GetFakeCreatureNumber();

	//---------------------------------------------------------
	// All Creatures must be added to the OutputMap.
	//---------------------------------------------------------
	for (int i=0; i<num; i++)
	{
		pCreature = iCreature->second;

		// key value = [pixel Y coordinate][object id]
		QWORD key = GetOutputCreatureID(pCreature);

		// Add (key value, pCreature).
		m_mapCreature.insert(
			CREATURE_OUTPUT_MAP::value_type( key, pCreature )
		);

		// Next Creature
		iCreature++;
	}
}

//----------------------------------------------------------------------
// Add OutputCreature
//----------------------------------------------------------------------
// Add pCreature.
//----------------------------------------------------------------------
bool	
MTopView::AddOutputCreature(MCreature* pCreature)
{	
	// Generate the key value
	QWORD key = GetOutputCreatureID(pCreature);

	// Check whether it already exists, and add if not.
	if (m_mapCreature.find( key ) == m_mapCreature.end())
	{			
		// Add (key value, pCreature).
		m_mapCreature.insert(
			CREATURE_OUTPUT_MAP::value_type( key, pCreature )
		);

		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Remove OutputCreature
//----------------------------------------------------------------------
// Remove the creature related to id.
//----------------------------------------------------------------------
bool	
MTopView::RemoveOutputCreature(MCreature* pCreature)
{
	// Check whether it already exists, and remove if so
	CREATURE_OUTPUT_MAP::iterator iCreature 
		= m_mapCreature.find( GetOutputCreatureID(pCreature) );

	if (iCreature	!= m_mapCreature.end())
	{			
		m_mapCreature.erase( iCreature );

		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Updates that the coordinate of the Creature with ID id changed from y0 to y1.
//----------------------------------------------------------------------
// Updates the information of the creature related to id (id related to the output coordinate).
// If it exists.. it must be removed.. and added again.
//----------------------------------------------------------------------
bool			
MTopView::UpdateOutputCreature(TYPE_OBJECTID id, int y0, int y1)
{
	// Check whether it already exists, and update if so
	CREATURE_OUTPUT_MAP::iterator iCreature 
		= m_mapCreature.find( GetOutputCreatureID(id, y0) );

	if (iCreature != m_mapCreature.end())
	{			
		MCreature* pCreature = (*iCreature).second;

		// Remove
		m_mapCreature.erase( iCreature );

		// Add (key value, pCreature) again.
		m_mapCreature.insert(
			CREATURE_OUTPUT_MAP::value_type( 
					GetOutputCreatureID(id, y1),	// key value
					pCreature 
			)
		);

		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// DrawBox3D
//----------------------------------------------------------------------
// Draws a black translucent box. 3D
//----------------------------------------------------------------------
//void
//MTopView::DrawBox3D(RECT* pRect, WORD pixel)
//
//
//	CSpriteSurface* pUsingSurface = NULL;
//
//	//-------------------------------------------------
//	// Color Black Bar
//	//-------------------------------------------------
//	//-------------------------------------------------
//	// Color HP Bar
//	//-------------------------------------------------
//	//-------------------------------------------------
//	// If it is a different color.. set again..
//	//------------------------------------------------		
//
//
//	
//

//----------------------------------------------------------------------
// Draw Alpha Box
//----------------------------------------------------------------------
// Regardless of 2D/3D acceleration...
// Works regardless of the current Lock/Unlock state...
//
// On m_pSurface,
// draws a transparent box in the pRect area according to (r,g,b) and the alpha value (1~31).
//----------------------------------------------------------------------
void
MTopView::DrawAlphaBox(RECT* pRect, BYTE r, BYTE g, BYTE b, BYTE alpha)
{
	WORD color;

	//--------------------------------------------------------
	// On Win2000.. a huge slowdown happens here.. why?
	// For now.. AlphaBox is not used.
	// Test result.. this was not the problem... what is the problem. - -;
	//--------------------------------------------------------
	/*
	if (g_WindowsNT)
	{
		//------------------------------------------------
		// Make it the Unlock state.
		//------------------------------------------------
		BOOL bLock = m_pSurface->IsLock();
		if (bLock)
		{
			m_pSurface->Unlock();
		}

		color = CSDLGraphics::Color(r,g,b);

		m_pSurface->FillRect(pRect, color);

		//------------------------------------------------
		// If it was originally locked, restore it.
		//------------------------------------------------
		if (bLock)
		{
			m_pSurface->Lock();
		}
	}
	*/
	//--------------------------------------------------------
	// In the 98 case..
	//--------------------------------------------------------
	//else
	{
		//--------------------------------------------------------
		// In the case of 3D acceleration
		//--------------------------------------------------------
//		if (true)
//		{
//			//------------------------------------------------
//			// Make it the Unlock state.
//			//------------------------------------------------
//
//			//------------------------------------------------
//			// 4:4:4:4
//			//------------------------------------------------
//			//						1 1110b
//			WORD color = ((32-alpha) & 0x1E) << 11;		// ((32-alpha)>>1) << 12
//
//
//			
//			CSpriteSurface* pUsingSurface = NULL;
//
//			//-------------------------------------------------
//			// Color Black Bar
//			//-------------------------------------------------
//			//-------------------------------------------------
//			// Color HP Bar
//			//-------------------------------------------------
//			//-------------------------------------------------
//			// If it is a different color.. set again..
//			//------------------------------------------------		
//
//
//
//			DRAW_TEXTURE_SURFACE( pUsingSurface, pRect )			
//
//			//------------------------------------------------
//			// If it was originally locked, restore it.
//			//------------------------------------------------
//		//--------------------------------------------------------
//		// 2D
//		//--------------------------------------------------------
//		else
		{
			//------------------------------------------------
			// Make it the Lock state.
			//------------------------------------------------
			BOOL bUnlock = !m_pSurface->IsLock();
			if (bUnlock)
			{
				m_pSurface->Lock();
			}

			color = CSDLGraphics::Color(r,g,b);

			//-------------------------------------------------
			// If it is black.. it is easy~
			//-------------------------------------------------
			if (color==0)
			{
				// 2D 5:6:5
				if (CSDLGraphics::Is565())
				{
					//m_pSurface->GammaBox565(pRect, alpha);
				}
				// 2D 5:5:5
				else
				{
					//m_pSurface->GammaBox555(pRect, alpha);
				}
			}
			//-------------------------------------------------
			// Otherwise...
			//-------------------------------------------------
			else
			{
				m_pSurface->BltColorAlpha(pRect, color, alpha);
			}

			//------------------------------------------------
			// Restore the original Lock state.
			//------------------------------------------------
			if (bUnlock)
			{
				m_pSurface->Unlock();
			}
		}
	}
}

//----------------------------------------------------------------------
// Draw Information
//----------------------------------------------------------------------
// Outputs the information needed for Debug.
//
// When drawing text it is in the Unlock state..
//----------------------------------------------------------------------
void
MTopView::DrawInformation()
{
	if(g_pEventManager->GetEventByFlag(EVENTFLAG_NOT_DRAW_INFORMATION))
		return;

	char str[128];

	//-----------------------------------------------------------------
	//							Lock
	//-----------------------------------------------------------------	

	//-----------------------------------------------------------------
	// Select Font
	//-----------------------------------------------------------------
	PrintInfo* pPrintInfo = g_ClientPrintInfo[FONTID_INFO];	

	int y;//2;

	if (g_pPlayer->IsSlayer())
	{
		y = 45;
	}
	else
	{
		y = 48;
	}


	//-----------------------------------------------------------------
	// [ TEST CODE ]
	//-----------------------------------------------------------------
	// Draw the Zone name
	//-----------------------------------------------------------------
	/* // Commented out 2001.7.14
	if (g_pUserOption->DrawZoneName)
	{
		if (g_bZonePlayerInLarge)
		{
			SafeFormat::Format(str, GetGameString(STRING_DRAW_ZONE_NAME),
							g_pZoneTable->Get( g_nZoneLarge )->Name.GetString(),
							g_pPlayer->GetX(), g_pPlayer->GetY());		
		}
		else
		{
			SafeFormat::Format(str, GetGameString(STRING_DRAW_ZONE_NAME),
							g_pZoneTable->Get( g_nZoneSmall )->Name.GetString(),
							g_pPlayer->GetX(), g_pPlayer->GetY());
		}
		const COLORREF color	= RGB(18<<3, 14<<3, 30<<3);
		const COLORREF bkcolor = RGB(0, 0, 0);

		pPrintInfo->text_color	= bkcolor;
		g_Print(6, y+1, str, pPrintInfo);

		pPrintInfo->text_color	= color;
		g_Print(5, y, str, pPrintInfo);

		y += 18;
	}
	*/


	//-----------------------------------------------------------------
	// Draw the game time
	//-----------------------------------------------------------------
	/*
	static DWORD lastDisplayGameTime = g_CurrentTime;

	if (g_pUserOption->DrawGameTime)
	{
		const COLORREF timeColor	= RGB(12<<3, 8<<3, 25<<3);
		const COLORREF timeBkColor = RGB(0, 0, 0);

		g_pGameTime->SetCurrentTime( g_CurrentTime );


		SafeFormat::Format(str, GetGameString(STRING_DRAW_GAME_TIME),
						g_pGameTime->GetHour(),
						g_pGameTime->GetMinute(),
						g_pGameTime->GetSecond()
				);

		pPrintInfo->text_color = timeBkColor;
		g_Print(6, y+1, str, pPrintInfo);

		pPrintInfo->text_color = timeColor;
		g_Print(5, y, str, pPrintInfo);

		lastDisplayGameTime = g_CurrentTime;

		y += 18;
	}
	else
	{
		// The time is changed once every 30 seconds.
		if (g_CurrentTime - lastDisplayGameTime > 30000)
		{
			g_pGameTime->SetCurrentTime( g_CurrentTime );

			lastDisplayGameTime = g_CurrentTime;
		}
	}
	*/

	//-----------------------------------------------------------------
	//
	// Draw broken Items
	//
	//-----------------------------------------------------------------
	y += 10;
	DrawItemBroken(5, y);

	y += 70;	// Size of ItemBroken.. too lazy so just.. lol

	//-----------------------------------------------------------------
	//
	// Draw Event Message
	//
	//-----------------------------------------------------------------
	int strY = 30;
	int strX = g_GameRect.right /2;

	// Draw Event String
	DrawEventString( strX, strY );

	//-----------------------------------------------------------------
	//
	// Draw System Message
	//
	//-----------------------------------------------------------------

	strY = y+20;
	for (int c=0; c<g_pSystemMessage->GetSize(); c++)
	{
		if ((*g_pSystemMessage)[c][0] != '\0')
		{
			const COLORREF color = RGB(29<<3, 8<<3, 12<<3);

			pPrintInfo->text_color = 0;

			g_Print(11, strY+1, (*g_pSystemMessage)[c], pPrintInfo);

			pPrintInfo->text_color = color;
			g_Print(10, strY, (*g_pSystemMessage)[c], pPrintInfo);

			strY+=20;
		}		
	}

	//-----------------------------------------------------------------
	//
	// Player Message
	//
	//-----------------------------------------------------------------
	// new version

	int x = 10;//((g_GameRect.right /2) /2) - 50;
	strY = 40;
	int iColorType = 0;
	char* pColorType=NULL;
	char message[300];
	int c;
	for (c=0; c<g_pPlayerMessage->GetSize(); c++)
	{
		if ((*g_pPlayerMessage)[c][0] != '\0')
		{
			// Take out the information
			SafeFormat::Copy(message,(*g_pPlayerMessage)[c]);
			// Take out the color type
			pColorType = &message[strlen(message)-1];
			iColorType = atoi(pColorType);
			pColorType[0]='\0';
			COLORREF color = RGB(20<<3,31<<3,12<<3);
			switch(iColorType)
			{
			case 0: // Green
				color = RGB_GREEN;
				break;
			case 1: // Blue
				color = RGB(50,50,200);//RGB_BLUE;
				break;
			case 2: // Yellow
				color = RGB_YELLOW;
				break;
			}

			pPrintInfo->text_color = 0;
			g_Print(x+1, strY+1, message, pPrintInfo);

			pPrintInfo->text_color = color;
			g_Print(x, strY, message, pPrintInfo);

			strY+=20;
		}		
	}

	PrintInfo* pNoticeInfo = g_ClientPrintInfo[FONTID_LARGE_CHAT];	


	for( c = 0; c< g_pNoticeMessage->GetSize(); c++ )
	{
		if( (*g_pNoticeMessage)[c][0] != '\0' )
		{
			const COLORREF color = RGB(20<<3,31<<3,12<<3);

			pNoticeInfo->text_color = 0;
			g_Print(11, strY+1, (*g_pNoticeMessage)[c], pNoticeInfo );
			pNoticeInfo->text_color = color;
			g_Print(10, strY, (*g_pNoticeMessage)[c], pNoticeInfo );
			strY += 16;
		}
	}

	// scroll every 5 seconds
	static MonotonicClock::TimePoint lastTime = g_FrameNow;
	if (g_FrameNow - lastTime >= MonotonicClock::Millis(g_pClientConfig->DELAY_SYSTEMMESSAGE))
	{
		g_pSystemMessage->Add("\0");		
		lastTime = g_FrameNow;
	}

	// scroll every 7 seconds
	static MonotonicClock::TimePoint lastNoticeTime = g_FrameNow;
	if (g_FrameNow - lastNoticeTime >= MonotonicClock::Millis(g_pClientConfig->DELAY_SYSTEMMESSAGE + 2000))
	{
		g_pNoticeMessage->Add("\0");
		lastNoticeTime = g_FrameNow;
	}

	// scroll every 7 seconds
	static MonotonicClock::TimePoint lastPlayerTimer = g_FrameNow;
	if (g_FrameNow - lastPlayerTimer >= MonotonicClock::Millis(g_pClientConfig->DELAY_SYSTEMMESSAGE + 10000))
	{
		g_pPlayerMessage->Add("\0");		
		lastPlayerTimer = g_FrameNow;
	}
	//-----------------------------------------------------------------
	//
	// Draw Game Message
	//
	//-----------------------------------------------------------------
	switch(g_pPlayer->GetRace())
	{
	case RACE_SLAYER:
		strX = 10;
		strY = 410;
		if (g_pPlayer->GetBonusPoint()!=0)
		{
			// Position of the button pressed to raise bonus points
			strX = 85;
		}	
		if (g_MyFull)
		{
			strY = 578;
		}
		break;

	case RACE_VAMPIRE:
		strX = 10;
		strY = 440;
		if (g_pPlayer->GetBonusPoint()!=0)
		{
			// Position of the button pressed to raise bonus points
			strX = 85;
		}
		if (g_MyFull)
		{
			strY = 608;
		}	
		break;

	case RACE_OUSTERS:
		strX = 10;
		strY = 410;
		if (g_pPlayer->GetBonusPoint()!=0)
		{
			// Position of the button pressed to raise bonus points
			strX = 85;
		}
		if (g_MyFull)
		{
			strY = 578;
		}	
		break;
	default:
		break;
	}

	for (c=g_pGameMessage->GetSize()-1; c>=0; c--)
	{
		if ((*g_pGameMessage)[c][0] != '\0')
		{
			const COLORREF color = RGB(8<<3, 28<<3, 8<<3);

			pPrintInfo->text_color = 0;
			g_Print(strX+1, strY+1, (*g_pGameMessage)[c], pPrintInfo);

			pPrintInfo->text_color = color;
			g_Print(strX, strY, (*g_pGameMessage)[c], pPrintInfo);

			strY-=20;
		}		
	}

	// scroll every 5 seconds
	static MonotonicClock::TimePoint gamelastTime = g_FrameNow;
	if (g_FrameNow - gamelastTime >= MonotonicClock::Millis(g_pClientConfig->DELAY_GAMEMESSAGE))
	{
		g_pGameMessage->Add("\0");
		gamelastTime = g_FrameNow;
	}

	//-----------------------------------------------------------------
	//
	// Draw remaining bullets
	//
	//-----------------------------------------------------------------
	if (g_pCurrentMagazine!=NULL)
	{
		POINT point = { 225, 9 };

		m_pSurface->Lock();
		m_pSurface->BltSprite(&point, &m_EtcSPK[SPRITEID_BULLET]);
		m_pSurface->Unlock();

		point.x += m_EtcSPK[SPRITEID_BULLET].GetWidth() + 2;
		point.y += m_EtcSPK[SPRITEID_BULLET].GetHeight() - 18;

		SafeFormat::Format(str, "%d", g_pCurrentMagazine->GetNumber());

		const COLORREF bulletColor = RGB(30<<3, 28<<3, 30<<3);

		pPrintInfo->text_color = 0;		
		g_Print(point.x+1, point.y+1, str, pPrintInfo);

		pPrintInfo->text_color = bulletColor;		
		g_Print(point.x, point.y, str, pPrintInfo);
	}

	//-----------------------------------------------------------------
	// Draw character status
	//-----------------------------------------------------------------
	#ifdef OUTPUT_DEBUG
		if (g_pSDLInput->KeyDown(DIK_P) && 
			(g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL)))
		{
			const int statusNum = 3;
			char statusName[statusNum][5] =
			{
				"STR",
				"DEX",
				"INT"
			};

			int statusValue[statusNum];

			statusValue[0] = g_pPlayer->GetSTR();
			statusValue[1] = g_pPlayer->GetDEX();
			statusValue[2] = g_pPlayer->GetINT();

			const COLORREF color = RGB(8<<3, 28<<3, 8<<3);


			strY = 50;

			for (int status=0; status<statusNum; status++)
			{
				SafeFormat::Format(str, "%s = %d", statusName[status], statusValue[status]);

				pPrintInfo->text_color = 0;
				g_Print(701, strY+1, str, pPrintInfo);

				pPrintInfo->text_color = color;
				g_Print(700, strY, str, pPrintInfo);

				strY+=20;
			}
		}

	#endif

	//-----------------------------------------------------------------
	//							UnLock
	//-----------------------------------------------------------------
	//m_pSurface->Unlock();
}

#if defined(OUTPUT_DEBUG) && defined(_LIB)
	extern int g_SurfaceCount;
	extern int g_EffectCount;
#endif

void
MTopView::DrawEventString(int& strX, int& strY)
{
	char str[256] = {0,};

	// Delay String
	PrintInfo* pPrintInfo = g_ClientPrintInfo[FONTID_INFO];	

	const MEvent *event = g_pEventManager->GetEventByFlag(EVENTFLAG_SHOW_DELAY_STRING, 0);

	if(event != NULL)
	{
		for(int eventMessageCount = 0; static_cast<size_t>(eventMessageCount) < (event->m_StringsID.empty()?1:event->m_StringsID.size()); eventMessageCount++)
		{
			str[0] = '\0';

			switch(event->eventFlag & EVENTFLAG_SHOW_DELAY_STRING)
			{
			case EVENTFLAG_SHOW_STRING:
				if(!event->m_StringsID.empty())
				{
					// The table is only as long as String.inf made it, which
					// need not reach MAX_GAME_STRING, so the id has to be
					// judged against the size actually loaded.
					const int stringID = event->m_StringsID[eventMessageCount];

					if(stringID >= 0 && stringID < g_pGameStringTable->GetSize()
						&& (*g_pGameStringTable)[stringID].GetString() != NULL)
						SafeFormat::Copy(str, (*g_pGameStringTable)[stringID].GetString());
				}
				break;

			case EVENTFLAG_SHOW_DELAY:
				SafeFormat::Format(str, "%d", (event->eventDelay - event->ElapsedMillis()+999)/1000);
				break;

			case EVENTFLAG_SHOW_DELAY_STRING:
				if(!event->m_StringsID.empty())
				{
					// See above: MAX_GAME_STRING is what the client knows,
					// GetSize() is what the data file supplied.
					const int stringID = event->m_StringsID[eventMessageCount];

					if(stringID >= 0 && stringID < g_pGameStringTable->GetSize()
						&& (*g_pGameStringTable)[stringID].GetString() != NULL)
						SafeFormat::Format(str, GetGameString(stringID), (event->eventDelay - event->ElapsedMillis()+999)/1000);
				}
				break;
			}			
			if (str[0] != '\0')
			{
				const COLORREF color = RGB(29<<3, 29<<3, 12<<3);

				const int strWidth = g_GetStringWidth(str, pPrintInfo->hfont)/2;
				pPrintInfo->text_color = 0;
				g_Print(strX-strWidth+1, strY+1, str, pPrintInfo);

				pPrintInfo->text_color = color;
				g_Print(strX-strWidth, strY, str, pPrintInfo);

				strY+=20;
			}		
		}
	}
}

//----------------------------------------------------------------------
// Draw Information
//----------------------------------------------------------------------
// Outputs the information needed for Debug.
//----------------------------------------------------------------------
void MTopView::DrawDebugInfo(void* pSurface)
{
	(void)pSurface;
	#ifdef	OUTPUT_DEBUG		
		//----------------------------------------------------------------
		// code for debug
		//----------------------------------------------------------------
		///*
		char str[128];
		char attackMode[20];

		if (g_pPlayer->IsAttackModeNormal())
		{
			SafeFormat::Copy(attackMode, "Normal");
		}
		else if (g_pPlayer->IsAttackModeAggress())
		{
			SafeFormat::Copy(attackMode, "Aggress");
		}
		else if (g_pPlayer->IsAttackModePeace())
		{
			SafeFormat::Copy(attackMode, "Peace");
		}		

		switch (g_pPlayer->GetWeaponSpeed())
		{
			case MCreature::WEAPON_SPEED_SLOW : SafeFormat::Format(attackMode, "Slow(%d)", g_pPlayer->GetAttackSpeed()); break;
			case MCreature::WEAPON_SPEED_NORMAL : SafeFormat::Format(attackMode, "Normal(%d)", g_pPlayer->GetAttackSpeed()); break;
			case MCreature::WEAPON_SPEED_FAST : SafeFormat::Format(attackMode, "Fast(%d)", g_pPlayer->GetAttackSpeed()); break;
		}

		SafeFormat::Format(attackMode, "gtx(%d. %d %d)", gtx_op, gtx_src, gtx_dest);

		int zoneID = (g_bZonePlayerInLarge)? g_nZoneLarge : g_nZoneSmall;

SafeFormat::Format(str, "ID = %d / %d명 [Weapon=%s] [align=%d]", g_pPlayer->GetID(), m_pZone->GetCreatureNumber(), attackMode, g_pPlayer->GetAlignment());
		// pSurfaceCast->GDI_Text(10,35, str, RGB(220,220,220));
		TextSystem::TextService::RenderText(g_pLast, 10, 35, str);

		SafeFormat::Format(str, " [ZoneID=%d] XY=(%d, %d), sXY=(%d, %d), SerXY=(%d, %d)", zoneID, g_pPlayer->GetX(), g_pPlayer->GetY(), g_pPlayer->GetSX(), g_pPlayer->GetSY(), g_pPlayer->GetServerX(), g_pPlayer->GetServerY());
		// pSurfaceCast->GDI_Text(10,56, str, RGB(220,220,220));
		TextSystem::TextService::RenderText(g_pLast, 10, 56, str);

		SafeFormat::Format(str, "SX = %d, SY = %d, HP=%d, MP=%d", g_pPlayer->GetSX(), g_pPlayer->GetSY(), g_pPlayer->GetHP(), g_pPlayer->GetMP());
		//sprintf(str, "[Wait Move = %d]", g_pPlayer->GetSendMove());
		// pSurfaceCast->GDI_Text(10,72, str, RGB(220,220,220));
		TextSystem::TextService::RenderText(g_pLast, 10, 72, str);

		SafeFormat::Format(str, "ActionCount = [%d] %d/%d", g_pPlayer->GetAction(), g_pPlayer->GetActionCount(), g_pPlayer->GetActionCountMax());
		// pSurfaceCast->GDI_Text(10,88, str, RGB(220,220,220));
		TextSystem::TextService::RenderText(g_pLast, 10, 88, str);

		SafeFormat::Format(str, "MoveCount = %d/%d", g_pPlayer->GetMoveCount(), g_pPlayer->GetMoveCountMax());
		// pSurfaceCast->GDI_Text(10,104, str, RGB(220,220,220));
		TextSystem::TextService::RenderText(g_pLast, 10, 104, str);

		SafeFormat::Format(str, "Dark/Light/Sight = %d/%d/%d", m_DarkBits, g_pPlayer->GetLightSight() + g_pPlayer->GetItemLightSight(), g_pPlayer->GetSight());
		// pSurfaceCast->GDI_Text(10,120, str, RGB(220,220,220));
		TextSystem::TextService::RenderText(g_pLast, 10, 120, str);

		if (g_pProfiler!=NULL && g_pProfiler->HasProfileInfo(g_ProfileInfoName.GetString()))
		{
			DWORD totalTime = g_pProfiler->GetTotalTime(g_ProfileInfoName.GetString());
			DWORD times = g_pProfiler->GetTimes(g_ProfileInfoName.GetString());
			float avgTime = g_pProfiler->GetAverageTime(g_ProfileInfoName.GetString());

			SafeFormat::Format(str, "%s = %d / %d = %7.3f", g_ProfileInfoName.GetString(), totalTime, times, avgTime);
			// pSurfaceCast->GDI_Text(10,136, str, RGB(220,220,220));		
			TextSystem::TextService::RenderText(g_pLast, 10, 136, str);
		}		


//
//
//
//

		if (g_pRequestServerPlayerManager!=NULL)
		{
			SafeFormat::Format(str, "Request Other = %d", g_pRequestServerPlayerManager->GetSize());
			// pSurfaceCast->GDI_Text(10,225, str, RGB(220,220,220));	
			TextSystem::TextService::RenderText(g_pLast, 10, 225, str);
		}




		//sprintf(str, "FirstPoint : X = %d, Y = %d", firstPointX, firstPointY);
		//pSurface->GDI_Text(9,109, str, RGB(220,220,220));
		//*/	




		//sprintf(str, "ImageObject=%d개", m_mapImageObject.size());
		//pSurface->GDI_Text(100,580, str, 0xFFFFFF);

		char infoStr[4][2][20] =
		{
			// FALSE, TRUE
			{ "Mode=555", "Mode=565" },
			{ "WinMM", "SWSynth" },
			{ "Game", "UI" },
			{ "One", "Slide" }
		};

		SafeFormat::Format(str, "%s, %s, InputFocus=%s, SCShot=%s",
						infoStr[0][CSDLGraphics::Is565()],
						infoStr[1][g_SDLMusic.IsSoftwareSynth()],
						infoStr[2][g_bUIInput],
						infoStr[3][g_bSlideScreenShot]
				);

		// pSurfaceCast->GDI_Text(150,580, str, 0xFFFFFF);		
		TextSystem::TextService::RenderText(g_pLast, 150, 580, str);

		// Test remaining Texture memory
		DDSCAPS2 ddsCaps2;
		DWORD dwTotal;
		DWORD dwFree;
		ZeroMemory(&ddsCaps2, sizeof(ddsCaps2)); 
		ddsCaps2.dwCaps = DDSCAPS_TEXTURE; 
		HRESULT hr = CSDLGraphics::GetDD()->GetAvailableVidMem(&ddsCaps2, &dwTotal, &dwFree);

		SafeFormat::Format(str, "Texture = %ld/%ld", dwFree, dwTotal);
		// pSurfaceCast->GDI_Text(500,580, str, 0xFFFFFF);		
		TextSystem::TextService::RenderText(g_pLast, 500, 580, str);


	#endif
}


//----------------------------------------------------------------------
// Draw Test Help
//----------------------------------------------------------------------
// Help for ClientTest
//----------------------------------------------------------------------
void
MTopView::DrawTestHelp()
{
	#ifdef OUTPUT_DEBUG
		//--------------------------------------------------------------------
		//
		// F1 is held --> draw help
		//
		//--------------------------------------------------------------------	
		if (g_pSDLInput->KeyDown(DIK_F11)
			&& (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL)))
		{
			RECT rect = { 250, 25, 530, 410 };

			if (true)
			{
				//DrawBox3D(&rect, 0x7000);
				DrawAlphaBox(&rect, 0, 0, 0, 20);
			}
			else
			{
				m_pSurface->Lock();

				if (CSDLGraphics::Is565())
				{
					//m_pSurface->GammaBox565(&rect, 15);
				}
				else
				{
					//m_pSurface->GammaBox555(&rect, 15);
				}

				m_pSurface->Unlock();
			}

			const int maxHelp = 19;
			const char helpStr[maxHelp][128] = 
			{
				"---------------------------------------------------------------------",
				"         ALPHA-TEST CLIENT help",
				"---------------------------------------------------------------------",
				"  [ESC] Menu",
				"  [F1~F8] Use belt item",
				"  [Scroll Lock] Screenshot (--> \\ScreenShot)",
				"  [Ctrl + M/T/Z] Map / game time / zone name",
				"  [Tab] Inventory on/off",			
				"  [Ctrl + Tab] Gear window on/off",			
				"  [L-Ctrl + ~] Belt on/off",			
				"  [L-Ctrl + S] Pick a skill icon",			
				"  [L-Ctrl + Space] Inventory and gear window on/off",
				"---------------------------------------------------------------------",
				"  [L-Button] Move, chase, attack",
				"  [L-Shift + L-Button] Forced attack",
				"  [L-Ctrl + L-Button] Attack enemies only",
				"  [R-Button] Magic, skills",
				"---------------------------------------------------------------------",
				"  [When chatting] start with ! for zone chat"
			};

			POINT point;
			point.x = 250;		
			point.y = 25;
			for (int i=0; i<maxHelp; i++)
			{
				//pSurface->GDI_Text(point.x,1, point.y,1, helpStr[i], RGB(0,0,0));
				// m_pSurface->GDI_Text(point.x, point.y, helpStr[i], RGB(220,220,220));
				TextSystem::TextService::RenderText(g_pLast, point.x, point.y, helpStr[i]);
				point.y += 20;
			}
		}
	#endif
}

//----------------------------------------------------------------------
// Draw
//----------------------------------------------------------------------
// Whether to redraw everything
// or to draw only part of it using the previous Tile is chosen.
//----------------------------------------------------------------------
void
MTopView::Draw(int firstPointX,int firstPointY)
{ 
	if (g_pZone==NULL)
	{
		#ifdef OUTPUT_DEBUG_DRAW_PROCESS
			DEBUG_ADD("g_pZone is NULL");
		#endif

		return;
	}

	#ifdef OUTPUT_DEBUG
/*		if (g_pSDLInput->KeyDown(DIK_RCONTROL) && g_pSDLInput->KeyDown(DIK_RSHIFT))
		{
			if (g_WipeValue==0 || g_WipeValue==64)
			{
				if (g_WipeStep==0)	
				{
					if (g_WipeValue==0)
						g_WipeStep = 1;
					else
						g_WipeStep = -1;
				}
				else g_WipeStep *= -1;
			}
		}	
		else
		{
			g_WipeValue += g_WipeStep;

			if (g_WipeValue<0) g_WipeValue = 0;
			else if (g_WipeValue>64) g_WipeValue=64;
		}
*/
		// Draw portal information on tiles
		if (g_pSDLInput->KeyDown(DIK_T) && g_pSDLInput->KeyDown(DIK_LCONTROL))
		{
			SetFirstDraw();
		}
	#endif	

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD("Draw");
	#endif

	//------------------------------------------------------------
	// Unofficial (-_-;) English font not used - -;
	//------------------------------------------------------------
	//gC_font.NotUseNoPublicEng();		

	//#ifdef OUTPUT_DEBUG
		//	DEBUG_ADD("[TempDebug] Before Draw");
	//#endif	

	//------------------------------------------------------------
	// Output only inside the Clip area.
	//------------------------------------------------------------

	//------------------------------------------------------------
	// When the Tile is first drawn, draw it all~.
	//------------------------------------------------------------
	if (m_bFirstTileDraw)
	{
		DEBUG_ADD("Draw m_bFirstTileDraw: Restore");

		CSDLGraphics::RestoreAllSurfaces();
// CDirect3D::Restore() removed (SDL2)

		DEBUG_ADD("Draw m_bFirstTileDraw: Clear");

		m_mapImageObject.clear();
		m_mapCreature.clear();

		DEBUG_ADD("Draw m_bFirstTileDraw: DrawTileSurface");

		__BEGIN_PROFILE("DrawTileSurface")

		DrawTileSurface();

		__END_PROFILE("DrawTileSurface")

		DEBUG_ADD("Draw m_bFirstTileDraw: DetermineImageObject");

		DetermineImageObject();
		m_bFirstTileDraw = false;

		DEBUG_ADD("Draw m_bFirstTileDraw: OK");

		/*
		// CDirect3D::GetDevice()->LightEnable() removed (SDL2)

		// CDirect3D::GetDevice()->SetRenderState() removed (SDL2)



		D3DLIGHT7 light;
	    D3DUtil_InitLight( light, D3DLIGHT_POINT, 0.0f, 0.0f, -10.0f );

		light.dltType      = D3DLIGHT_SPOT;
		light.dcvDiffuse.r   = 1.0f;
		light.dcvDiffuse.g   = 1.0f;
		light.dcvDiffuse.b   = 1.0f;
		light.dcvSpecular = light.dcvDiffuse;
		light.dvPosition.x = 0.0f;
		light.dvPosition.y = 0.0f;
		light.dvPosition.z = -10.0f;
		light.dvDirection.x = 0.0f;	
		light.dvDirection.y = 0.0f;
		light.dvDirection.z = 1.0f;
		light.dvTheta =       0.5f; //Size of the center of the cone
		light.dvPhi =         0.8f; //Size of the outer edge of the cone
		light.dvAttenuation0 = 1.0f;
		light.dvFalloff		= 1.0f;

		// CDirect3D::GetDevice()->SetLight() removed (SDL2)

		*/
		//*/
	}
	//------------------------------------------------------------
	// Reuse the previously drawn Tile.
	//------------------------------------------------------------
	//else
	{			
		if (s_bUse3DLight)// && m_b3DLight)
		{
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD( "IsLost?" );
			#endif

			// Will it help..
			if (false)
			{
				#ifdef OUTPUT_DEBUG_DRAW_PROCESS
					DEBUG_ADD( "CDirect3D::Restore" );
				#endif
// CDirect3D::Restore() removed (SDL2)

				#ifdef OUTPUT_DEBUG_DRAW_PROCESS
					DEBUG_ADD( "RestoreSurface" );
				#endif

				RestoreSurface();
			}

			//------------------------------------------------
			// BeginScene
			//------------------------------------------------
			/*
			if (false)  // CDirect3D::GetDevice()->BeginScene() removed (SDL2)
			{
				CSDLGraphics::RestoreAllSurfaces();
// CDirect3D::Restore() removed (SDL2)

				RestoreSurface();

				m_pSurface->SetClipNULL();
				return;
			}			
			*/


			///*
			///*
			/*
			D3DRECT rect3d;
			rect3d.x1 = 0;
			rect3d.y1 = 0;
			rect3d.x2 = CLIPSURFACE_WIDTH;
			rect3d.y2 = CLIPSURFACE_HEIGHT;
			// CDirect3D::GetDevice()->Clear() removed (SDL2)

			*/
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD( "Clear 3D" );
			#endif

			// SDL: Not needed - removed D3D device clear call
			// // CDirect3D::GetDevice()->Clear() removed (SDL2)

			//*/


			//DrawZone(firstPointX, firstPointY);

			//*/


			/*
			if (g_pSDLInput->KeyDown(DIK_Z))
			{
				D3DLIGHT7 light;
				// CDirect3D::GetDevice()->GetLight() removed (SDL2)


				light.dvTheta -=       0.01f; //Size of the center of the cone
				light.dvPhi -=         0.01f; //Size of the outer edge of the cone

				// CDirect3D::GetDevice()->SetLight() removed (SDL2)

			}

			if (g_pSDLInput->KeyDown(DIK_A))
			{
				D3DLIGHT7 light;
				// CDirect3D::GetDevice()->GetLight() removed (SDL2)


				light.dvTheta +=       0.01f; //Size of the center of the cone
				light.dvPhi +=         0.01f; //Size of the outer edge of the cone

				// CDirect3D::GetDevice()->SetLight() removed (SDL2)

			}
			*/

			///*
			//test.Rotate( 0.1f, 0.1f, 0 );

			//------------------------------------------------
			// LightBuffer initialization
			//------------------------------------------------
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD( "Clear LightBufferFilter3D" );
			#endif

			__BEGIN_PROFILE("ClearLBF3D")

			ClearLightBufferFilter3D();

			__END_PROFILE("ClearLBF3D")

			//------------------------------------------------
			// Draw Zone
			//------------------------------------------------
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD( "DrawZone" );
			#endif

			__BEGIN_PROFILE("DrawZone")

			DrawZone(firstPointX, firstPointY);	

			__END_PROFILE("DrawZone")


			// Light at the mouse coordinate
			//AddLightFilter( m_FirstZonePixel.x,g_x, m_FirstZonePixel.y,g_y, 4 );
			//------------------------------------------------
			// Fade
			//------------------------------------------------
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD( "DrawFade" );
			#endif

			__BEGIN_PROFILE("DrawFade")

			DrawFade();

			__END_PROFILE("DrawFade")

			//------------------------------------------------
			// Minimap
			//------------------------------------------------
			/* // Commented out 2001.7.14
			if (g_pUserOption->DrawMinimap)
			{
				DrawMinimap( CLIPSURFACE_WIDTH-m_pMinimapSPR->GetWidth(),
							0,//440-m_pMinimapSPR->GetHeight(),
							1);	// scale
			}
			*/

			//------------------------------------------------
			// help for testing
			//------------------------------------------------
			if (g_pSDLInput->KeyDown(DIK_F11))
			{
				DrawTestHelp();
			}


			//------------------------------------------------
			// EndScene
			//------------------------------------------------
			//// CDirect3D::GetDevice()->EndScene() removed (SDL2)


			//------------------------------------------------
			// The removed Surfaces must be erased.
			// It must be erased after EndScene() ends.
			//------------------------------------------------
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD( "Remove RemovedTexture" );
			#endif

			__BEGIN_PROFILE("ClearTexture")


			__END_PROFILE("ClearTexture")
		}
		else
		{
			//------------------------------------------------
			// LightBuffer initialization
			//------------------------------------------------
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD( "Clear LightBufferFilter2D");
			#endif

			__BEGIN_PROFILE("ClearLBF2D")

			ClearLightBufferFilter2D();

			__END_PROFILE("ClearLBF2D")


			//------------------------------------------------
			// Draw Zone
			//------------------------------------------------
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD( "DrawZone" );
			#endif

			__BEGIN_PROFILE("DrawZone")

			DrawZone(firstPointX, firstPointY);		

			__END_PROFILE("DrawZone")

			//------------------------------------------------
			// Fade
			//------------------------------------------------
			#ifdef OUTPUT_DEBUG_DRAW_PROCESS
				DEBUG_ADD( "DrawFade" );
			#endif

			__BEGIN_PROFILE("DrawFade")

			DrawFade();	

			__BEGIN_PROFILE("DrawFade")

			//------------------------------------------------
			// Minimap
			//------------------------------------------------
			/* // Commented out 2001.7.14
			if (g_pUserOption->DrawMinimap)
			{
				DrawMinimap( CLIPSURFACE_WIDTH-m_pMinimapSPR->GetWidth(),
							0, //440-m_pMinimapSPR->GetHeight(),
							1);	// scale
			}
			*/

			//------------------------------------------------
			// help for testing
			//------------------------------------------------
		}	
	}

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "Draw Information" );
	#endif

	//------------------------------------------------
	// Draw information
	//------------------------------------------------
	DrawInformation();

	//#ifdef OUTPUT_DEBUG
		//	DEBUG_ADD("[TempDebug] After Draw");
	//#endif	



	//------------------------------------------------------------
	// Normal output
	//------------------------------------------------------------
	//m_pSurface->SetClipNULL();
	// SDL2: Clipping is handled differently - removed Windows-specific call

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD("Draw OK");
	#endif
}

//----------------------------------------------------------------------
// Draw LightBuffer
//----------------------------------------------------------------------
// Blt m_LightBufferFilter to m_pLightBufferTexture, and
// set m_pLightBufferTexture as the texture, so that
// the LightBufferFilter ends up drawn on the screen.
//----------------------------------------------------------------------
void
MTopView::DrawLightBuffer3D()
{
	// SDL uses the same light-grid shader for both legacy entry points.
	DrawLightBuffer2D();
}

//----------------------------------------------------------------------
// Draw LightBuffer
//----------------------------------------------------------------------
// Blt m_LightBufferFilter to m_pLightBufferTexture, and
// set m_pLightBufferTexture as the texture, so that
// the LightBufferFilter ends up drawn on the screen.
//----------------------------------------------------------------------
void
MTopView::DrawLightBuffer2D()
{
	if (m_DarkBits || g_pPlayer->IsInDarkness())
		m_pSurface->ApplyLightGrid(m_LightBufferFilter, m_p2DLightPixelWidth, m_p2DLightPixelHeight);
}


//----------------------------------------------------------------------
// Determine ImageObject
//----------------------------------------------------------------------
// Search all the ImageObjects on the screen.
//----------------------------------------------------------------------
void
MTopView::DetermineImageObject()
{	
	//---------------------------------------------------------------	
	// The first Sector in the Zone that will be drawn at firstPoint
	//---------------------------------------------------------------	
	// Because drawing must be centered on the Player,
	// draw starting from the left~~~~ top~.
	//---------------------------------------------------------------	
	POINT	firstSector;
	firstSector.x = g_pPlayer->GetX() + g_SECTOR_SKIP_PLAYER_LEFT;
	firstSector.y = g_pPlayer->GetY() + g_SECTOR_SKIP_PLAYER_UP;

	//---------------------------------------------------------------	
	// When at the edge of the Zone,
	// keep the black part from showing..
	//---------------------------------------------------------------	
	// Fit the X coordinate..
	//---------------------------------------------------------------	
	if (firstSector.x <= 0)
	{	
		firstSector.x = 0;			
	}
	else if (firstSector.x+g_SECTOR_WIDTH+1 >= m_pZone->GetWidth())
	{
		firstSector.x = m_pZone->GetWidth()-g_SECTOR_WIDTH-1;
	}

	//---------------------------------------------------------------	
	// Fit the Y coordinate
	//---------------------------------------------------------------	
	if (firstSector.y <= 0)
	{
		firstSector.y = 0;
	}
	else if (firstSector.y+g_SECTOR_HEIGHT+1 >= m_pZone->GetHeight())
	{
		firstSector.y = m_pZone->GetHeight()-g_SECTOR_HEIGHT-1;	
	}

	//----------------------------------------------------------------------
	//
	//                         ImageObject search
	//
	//----------------------------------------------------------------------	
	//POINT firstSector = PixelToMap(m_FirstZonePixel.x, m_FirstZonePixel.y);
	int sX1 = firstSector.x + SECTOR_SKIP_LEFT;
	int sY1 = firstSector.y + SECTOR_SKIP_UP;
	int sX2 = firstSector.x + g_SECTOR_WIDTH+1;
	int sY2 = firstSector.y + g_SECTOR_HEIGHT+1;

	//------------------------------------------------------
	// Skip if outside the Zone area...
	//------------------------------------------------------
	if (sX1 < 0) 
	{					
		//tilePoint.x += -sX1 * TILE_X;
		sX1 = 0;	
	}

	if (sX2 >= m_pZone->GetWidth())
	{
		sX2 = m_pZone->GetWidth()-1;
	}

	if (sY1 < 0)
	{
		//tilePoint.y += -sY1 * TILE_Y;
		sY1 = 0;	
	}

	if (sY2 >= m_pZone->GetHeight())
	{
		sY2 = m_pZone->GetHeight()-1;
	}

	//------------------------------------------------------
	// Erase all the previous ImageObjects.
	//------------------------------------------------------
	m_mapImageObject.clear();	

	//------------------------------------------------------
	// Search the ImageObjects of each Sector
	//------------------------------------------------------
	int y,x,i;
	for (y=sY1; y<=sY2; y++)
	{				
		for (x=sX1; x<=sX2; x++)
		{				
			const MSector& sector = m_pZone->GetSector(x,y);

			// If there is an ImageObject.. remember them all~.
			// However, duplicates are not allowed.
			if (sector.IsExistImageObject())
			{
				OBJECT_MAP::const_iterator iImageObject = sector.GetImageObjectIterator();

				// Search all the ImageObjects in the Sector.
				for (i=0; i<sector.GetImageObjectSize(); i++)
				{
					MImageObject* const pImageObject = (MImageObject*)((*iImageObject).second);

					//----------------------------------------
					// The output time should be checked,
					// but some ImageObjects
					// have a viewpoint that is not
					// their own ViewSector. -_-;
					// So.. add unconditionally.
					//----------------------------------------
					{						
						// Check whether it already exists, and add if not.
						QWORD key = GetOutputImageObjectID( pImageObject );

						if (m_mapImageObject.find( key )
							== m_mapImageObject.end())
						{			
							//----------------------------------------
							// Key value = (Viewpoint << 32) | ID
							//----------------------------------------
							// Sorted by Viewpoint.
							// Can be removed by Viewpoint and ID.
							//----------------------------------------
							m_mapImageObject.insert(
								IMAGEOBJECT_OUTPUT_MAP::value_type(
									key,	// key value
									pImageObject
								)
							);
						}
					}

					iImageObject++;
				}
			}
		}
	}
}

//----------------------------------------------------------------------
// Update ImageObject
//----------------------------------------------------------------------
// m_FirstPoint is the immediately previous~ screen coordinate..
// and the parameter firstSector is the current screen coordinate.
//
// ImageObjects that leave the screen are removed from m_mapImageObject, and
// ImageObjects that appear on the screen are added to m_mapImageObject.
//----------------------------------------------------------------------
void
MTopView::UpdateImageObject(const POINT &newFirstSector)
{
	//#ifdef OUTPUT_DEBUG
		//	DEBUG_ADD("[TempDebug] Before Update ImageObject");
	//#endif	



	int x,y,i;

	//---------------------------------------------------------------
	// 
	//               ImageObject refresh
	//
	//---------------------------------------------------------------
	// Compare newFirstSector with m_FirstSector (the immediately previous coordinate),
	// and if they differ... check~~ is needed.
	//---------------------------------------------------------------
	// When the coordinate changed vertically
	//---------------------------------------------------------------
	if (newFirstSector.y != m_FirstSector.y)
	{		
		int	sX1=newFirstSector.x + SECTOR_SKIP_LEFT, 
				sX2=newFirstSector.x + g_SECTOR_WIDTH+1, 
				sX01=m_FirstSector.x + SECTOR_SKIP_LEFT, 
				sX02=m_FirstSector.x + g_SECTOR_WIDTH+1, 
				eraseY1, eraseY2,		// The row that has things to erase
				lastY,					// The last row of the screen (things that must not be erased)
				newY1, newY2;			// The row that has newly appearing things

		// Is there a need to check?
		bool bCheckLast		= true;
		bool bCheckErase	= true;
		bool bCheckNew		= true;

		//-----------------------------------
		// If it goes outside the Zone area
		//-----------------------------------
		if (sX2 >= m_pZone->GetWidth())
		{
			sX2 = m_pZone->GetWidth()-1;
		}

		if (sX1 < 0) 
		{				
			sX1 = 0;	
		}

		if (sX02 >= m_pZone->GetWidth())
		{
			sX02 = m_pZone->GetWidth()-1;
		}

		if (sX01 < 0) 
		{				
			sX01 = 0;	
		}

		//-----------------------------------
		// If it moved upward
		//-----------------------------------
		if (newFirstSector.y - m_FirstSector.y < 0)
		{

			// To bring back
			lastY	= newFirstSector.y + g_SECTOR_HEIGHT+1;	// The last row of the current screen

			// To erase: lasyY+1 ~ eraseY
			eraseY1	= lastY+1;							
			eraseY2	= m_FirstSector.y + g_SECTOR_HEIGHT+1;	// The last row of the previous screen

			// Newly appearing ones
			newY1	= newFirstSector.y + SECTOR_SKIP_UP;
			newY2	= m_FirstSector.y + SECTOR_SKIP_UP - 1;

			//------------------------------------------------------
			// Skip if outside the Zone area...
			//------------------------------------------------------
			if (lastY >= m_pZone->GetHeight())
			{
				//lastY = m_pZone->GetHeight()-1;				
				// No need to bring back.
				bCheckLast = false;
			}

			if (eraseY1 >= m_pZone->GetHeight())
			{
				//eraseY1 = m_pZone->GetHeight()-1;
				// No need to erase.
				bCheckErase = false;
			}
			else if (eraseY2 >= m_pZone->GetHeight())
			{
				eraseY2 = m_pZone->GetHeight()-1;								
			}		

			if (newY2 < 0) 
			{				
				//newY2 = 0;
				// No need to new.
				bCheckNew = false;
			}		
			else if (newY1 < 0) 
			{				
				newY1 = 0;	
			}

		}
		//-----------------------------------
		// If it moved downward
		//-----------------------------------
		else
		{	
			// To bring back
			lastY	= newFirstSector.y + SECTOR_SKIP_UP;	// The last row of the current screen

			// To erase: eraseY ~ lastY-1
			eraseY1	= m_FirstSector.y + SECTOR_SKIP_UP;	// The last row of the previous screen
			eraseY2	= lastY-1;		

			// Newly appearing ones
			newY1	= m_FirstSector.y + g_SECTOR_HEIGHT+1 + 1;
			newY2	= newFirstSector.y + g_SECTOR_HEIGHT+1;

			//------------------------------------------------------
			// Skip if outside the Zone area...
			//------------------------------------------------------				
			if (lastY < 0) 
			{				
				//lastY = 0;	
				bCheckLast = false;
			}

			if (eraseY2 < 0) 
			{				
				//eraseY2 = 0;	
				bCheckErase = false;
			}
			else if (eraseY1 < 0) 
			{				
				eraseY1 = 0;	
			}			

			if (newY1 >= m_pZone->GetHeight())
			{
				//newY1 = m_pZone->GetHeight()-1;				
				bCheckNew = false;
			}
			else if (newY2 >= m_pZone->GetHeight())
			{
				newY2 = m_pZone->GetHeight()-1;				
			}		

		}


		//----------------------------------------------
		// erase1 ~ erase2 are the ones worth(?) erasing.
		//----------------------------------------------
		// First erase from m_mapImageObject.
		//----------------------------------------------
		if (bCheckErase)
		{
			for (y=eraseY1; y<=eraseY2; y++)
			{
				for (x=sX01; x<=sX02; x++)
				{
					const MSector& sector = m_pZone->GetSector(x,y);

					// If there is an ImageObject.. remember them all~.
					// However, duplicates are not allowed.
					if (sector.IsExistImageObject())
					{
						OBJECT_MAP::const_iterator iImageObject = sector.GetImageObjectIterator();

						// Search all the ImageObjects in the Sector.
						for (int i=0; i<sector.GetImageObjectSize(); i++)
						{
							MImageObject* const pImageObject = (MImageObject*)((*iImageObject).second);

							//----------------------------------------
							// Key value = (Viewpoint << 32) | ID
							//----------------------------------------
							// Check whether it already exists, and remove if so.
							IMAGEOBJECT_OUTPUT_MAP::iterator iDelete = m_mapImageObject.find( GetOutputImageObjectID( pImageObject ) );
							if (iDelete	!= m_mapImageObject.end())
							{			
								m_mapImageObject.erase( iDelete );
							}

							iImageObject++;
						}
					}
				}
			}
		}

		//----------------------------------------------
		// lastY is added again.
		//----------------------------------------------
		if (bCheckLast)
		{
			for (x=sX1; x<=sX2; x++)
			{
				const MSector& sector = m_pZone->GetSector(x, lastY);

				// If there is an ImageObject..
				if (sector.IsExistImageObject())
				{
					OBJECT_MAP::const_iterator iImageObject = sector.GetImageObjectIterator();

					// Search all the ImageObjects in the Sector.
					for (i=0; i<sector.GetImageObjectSize(); i++)
					{
						MImageObject* const pImageObject = (MImageObject*)((*iImageObject).second);

						{		
							QWORD key = GetOutputImageObjectID(pImageObject);
							// Check whether it already exists, and add if not.
							if (m_mapImageObject.find( key )
								== m_mapImageObject.end())
							{			
								//----------------------------------------
								// Key value = (Viewpoint << 32) | ID
								//----------------------------------------
								// Sorted by Viewpoint.
								// Can be removed by Viewpoint and ID.
								//----------------------------------------
								m_mapImageObject.insert(
									IMAGEOBJECT_OUTPUT_MAP::value_type(
										key,	// key value
										pImageObject
									)
								);
							}
						}

						iImageObject++;
					}
				}
			}
		}

		//----------------------------------------------
		// Newly appearing ones: newY1 ~ newY2
		//----------------------------------------------
		if (bCheckNew)
		{
			for (y=newY1; y<=newY2; y++)
			{
				for (x=sX1; x<=sX2; x++)
				{
					const MSector& sector = m_pZone->GetSector(x, y);

					// If there is an ImageObject..
					if (sector.IsExistImageObject())
					{
						OBJECT_MAP::const_iterator iImageObject = sector.GetImageObjectIterator();

						// Search all the ImageObjects in the Sector.
						for (i=0; i<sector.GetImageObjectSize(); i++)
						{
							MImageObject* const pImageObject = (MImageObject*)((*iImageObject).second);

							{						
								// Check whether it already exists, and add if not.
								QWORD key = GetOutputImageObjectID( pImageObject );

								if (m_mapImageObject.find( key ) == m_mapImageObject.end())
								{			
									//----------------------------------------
									// Key value = (Viewpoint << 32) | ID
									//----------------------------------------
									// Sorted by Viewpoint.
									// Can be removed by Viewpoint and ID.
									//----------------------------------------
									m_mapImageObject.insert(
										IMAGEOBJECT_OUTPUT_MAP::value_type(
											key,	// key value
											pImageObject
										)
									);
								}
							}

							iImageObject++;
						}
					}
				}
			}
		}

	}

	//---------------------------------------------------------------
	// When the coordinate changed horizontally
	//---------------------------------------------------------------
	if (newFirstSector.x != m_FirstSector.x)
	{
		int	sY1=newFirstSector.y + SECTOR_SKIP_UP, 
				sY2=newFirstSector.y + g_SECTOR_HEIGHT+1, 
				sY01=m_FirstSector.y + SECTOR_SKIP_UP, 
				sY02=m_FirstSector.y + g_SECTOR_HEIGHT+1, 
				eraseX1, eraseX2,		// The row that has things to erase
				lastX,					// The last row of the screen (things that must not be erased)
				newX1, newX2;			// The row that has newly appearing things

		// Is there a need to check?
		bool bCheckLast		= true;
		bool bCheckErase	= true;
		bool bCheckNew		= true;

		//-----------------------------------
		// If it goes outside the Zone area
		//-----------------------------------
		if (sY2 >= m_pZone->GetHeight())
		{
			sY2 = m_pZone->GetHeight()-1;
		}

		if (sY1 < 0) 
		{				
			sY1 = 0;	
		}

		if (sY02 >= m_pZone->GetHeight())
		{
			sY02 = m_pZone->GetHeight()-1;
		}

		if (sY01 < 0) 
		{				
			sY01 = 0;	
		}

		//-----------------------------------
		// If it moved left
		//-----------------------------------
		if (newFirstSector.x - m_FirstSector.x < 0)
		{

			// To bring back
			lastX	= newFirstSector.x + g_SECTOR_WIDTH+1;	// The last row of the current screen

			// To erase: lasyX+1 ~ eraseX
			eraseX1	= lastX+1;							
			eraseX2	= m_FirstSector.x + g_SECTOR_WIDTH+1;	// The last row of the previous screen

			// Newly appearing ones
			newX1	= newFirstSector.x + SECTOR_SKIP_LEFT;
			newX2	= m_FirstSector.x + SECTOR_SKIP_LEFT - 1;

			//------------------------------------------------------
			// Skip if outside the Zone area...
			//------------------------------------------------------
			if (lastX >= m_pZone->GetWidth())
			{
				//lastX = m_pZone->GetWidth()-1;
				bCheckLast = false;
			}

			if (eraseX1 >= m_pZone->GetWidth())
			{
				//eraseX1 = m_pZone->GetWidth()-1;
				bCheckErase = false;
			}
			else if (eraseX2 >= m_pZone->GetWidth())
			{
				eraseX2 = m_pZone->GetWidth()-1;
			}		

			if (newX2 < 0) 
			{				
				//newX2 = 0;	
				bCheckNew = false;
			}	
			else if (newX1 < 0) 
			{				
				newX1 = 0;	
			}				

		}
		//-----------------------------------
		// If it moved right
		//-----------------------------------
		else
		{	
			// remove             add      add
			// eraseX1~eraseX2~lastX ~~~~ newX1~newX2
			// To bring back
			lastX	= newFirstSector.x + SECTOR_SKIP_LEFT;	// The last row of the current screen

			// To erase: eraseX ~ lastX-1
			eraseX1	= m_FirstSector.x + SECTOR_SKIP_LEFT;	// The last row of the previous screen
			eraseX2	= lastX-1;		

			// Newly appearing ones
			newX1	= m_FirstSector.x + g_SECTOR_WIDTH+1 + 1;
			newX2	= newFirstSector.x + g_SECTOR_WIDTH+1;

			//------------------------------------------------------
			// Skip if outside the Zone area...
			//------------------------------------------------------				
			if (lastX < 0) 
			{				
				//lastX = 0;
				bCheckLast = false;
			}

			if (eraseX2 < 0) 
			{				
				//eraseX2 = 0;
				bCheckErase = false;
			}
			else if (eraseX1 < 0) 
			{				
				eraseX1 = 0;	
			}

			if (newX1 >= m_pZone->GetWidth())
			{
				//newX1 = m_pZone->GetWidth()-1;
				bCheckNew = false;
			}
			else if (newX2 >= m_pZone->GetWidth())
			{
				newX2 = m_pZone->GetWidth()-1;
			}	
		}


		//----------------------------------------------
		// erase1 ~ erase2 are the ones worth(?) erasing.
		//----------------------------------------------
		// First erase from m_mapImageObject.
		//----------------------------------------------
		if (bCheckErase)
		{
			for (x=eraseX1; x<=eraseX2; x++)
			{
				for (y=sY01; y<=sY02; y++)
				{
					const MSector& sector = m_pZone->GetSector(x,y);

					// If there is an ImageObject.. remember them all~.
					// However, duplicates are not allowed.
					if (sector.IsExistImageObject())
					{
						OBJECT_MAP::const_iterator iImageObject = sector.GetImageObjectIterator();

						// Search all the ImageObjects in the Sector.
						for (i=0; i<sector.GetImageObjectSize(); i++)
						{
							MImageObject* const pImageObject = (MImageObject*)((*iImageObject).second);

							//----------------------------------------
							// Key value = (Viewpoint << 32) | ID
							//----------------------------------------
							// Check whether it already exists, and remove if so.
							IMAGEOBJECT_OUTPUT_MAP::iterator iDelete = m_mapImageObject.find( GetOutputImageObjectID( pImageObject ) );
							if (iDelete	!= m_mapImageObject.end())
							{			
								m_mapImageObject.erase( iDelete );
							}

							iImageObject++;
						}
					}
				}
			}
		}

		//----------------------------------------------
		// lastX is added again.
		//----------------------------------------------
		if (bCheckLast)
		{
			for (y=sY1; y<=sY2; y++)
			{
				const MSector& sector = m_pZone->GetSector(lastX, y);

				// If there is an ImageObject..
				if (sector.IsExistImageObject())
				{
					OBJECT_MAP::const_iterator iImageObject = sector.GetImageObjectIterator();

					// Search all the ImageObjects in the Sector.
					for (i=0; i<sector.GetImageObjectSize(); i++)
					{
						MImageObject* const pImageObject = (MImageObject*)((*iImageObject).second);

						{						
							// Check whether it already exists, and add if not.
							QWORD key = GetOutputImageObjectID( pImageObject );

							if (m_mapImageObject.find( key )
								== m_mapImageObject.end())
							{			
								//----------------------------------------
								// Key value = (Viewpoint << 32) | ID
								//----------------------------------------
								// Sorted by Viewpoint.
								// Can be removed by Viewpoint and ID.
								//----------------------------------------
								m_mapImageObject.insert(
									IMAGEOBJECT_OUTPUT_MAP::value_type(
										key,	// key value
										pImageObject
									)
								);
							}
						}

						iImageObject++;
					}
				}
			}
		}

		//----------------------------------------------
		// Newly appearing ones: newX1 ~ newX2
		//----------------------------------------------
		if (bCheckNew)
		{
			for (x=newX1; x<=newX2; x++)
			{
				for (y=sY1; y<=sY2; y++)
				{
					const MSector& sector = m_pZone->GetSector(x, y);

					// If there is an ImageObject..
					if (sector.IsExistImageObject())
					{
						OBJECT_MAP::const_iterator iImageObject = sector.GetImageObjectIterator();

						// Search all the ImageObjects in the Sector.
						for (i=0; i<sector.GetImageObjectSize(); i++)
						{
							MImageObject* const pImageObject = (MImageObject*)((*iImageObject).second);

							{								
								// Check whether it already exists, and add if not.
								QWORD key = GetOutputImageObjectID( pImageObject );

								if (m_mapImageObject.find( key )
									== m_mapImageObject.end())
								{			
									//----------------------------------------
									// Key value = (Viewpoint << 32) | ID
									//----------------------------------------
									// Sorted by Viewpoint.
									// Can be removed by Viewpoint and ID.
									//----------------------------------------
									m_mapImageObject.insert(
										IMAGEOBJECT_OUTPUT_MAP::value_type(
											key,	// key value
											pImageObject
										)
									);
								}
							}

							iImageObject++;
						}
					}
				}
			}
		}
	}

	//#ifdef OUTPUT_DEBUG
		//	DEBUG_ADD("[TempDebug] After Determine ImageObject");
	//#endif	
}

//----------------------------------------------------------------------
// Draw Zone
//----------------------------------------------------------------------
// The Pixel coordinate (Xp, Yp)
//       is placed at the screen's (0, 0), and the remaining part is drawn.
//
// Draw onto pSurface at (X,Y) starting from the Zone's (Xp,Yp).
//----------------------------------------------------------------------
//
// Build the current Tile from the previously drawn Tile,
// then draw the other Objects.
//
//----------------------------------------------------------------------
void	
MTopView::DrawZone(int firstPointX,int firstPointY)
{
	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "Start DrawZone" );
	#endif

	//-------------------------------------------------
	// Clear the ItemName list.
	//-------------------------------------------------
	ClearItemNameList();	


	// Character selected with the mouse..
	m_pSelectedCreature = NULL;


	//-------------------------------------------------
	//
	// Information for drawing Objects
	//
	//-------------------------------------------------
	// pointer variable
	//MCreature*		pCreature;
	MItem*			pItem;

	// sprite point
	POINT	tilePoint, point;
	RECT	rect;

	int x;
	int y;
	int i;

	//-------------------------------------------------
	// Information about the ImageObjects covering the Player
	//-------------------------------------------------


	//-------------------------------------------------
	// Correct the coordinates where the first Sector is drawn (for smooth scrolling)
	// Determined with the Player's coordinates as the center.
	//-------------------------------------------------
	m_PlusPoint.x = g_pPlayer->GetSX() - firstPointX;
	m_PlusPoint.y = g_pPlayer->GetSY() - firstPointY;
	firstPointX -= g_pPlayer->GetSX();
	firstPointY -= g_pPlayer->GetSY();


	//---------------------------------------------------------------	
	// First Sector in the Zone to be drawn at firstPoint
	//---------------------------------------------------------------	
	// Because drawing must be centered on the Player,
	// draw from the left~~~~ top~.
	//---------------------------------------------------------------	
	POINT	firstSector;
	firstSector.x = g_pPlayer->GetX() + g_SECTOR_SKIP_PLAYER_LEFT;
	firstSector.y = g_pPlayer->GetY() + g_SECTOR_SKIP_PLAYER_UP;

	//---------------------------------------------------------------	
	// When at the Zone's border,
	// keep the black part from showing..
	//---------------------------------------------------------------	
	// Adjust the X coordinate..
	//---------------------------------------------------------------	
	if (firstSector.x <= 0)
	{
		if (firstSector.x==0)
		{
			if (g_pPlayer->GetSX() < 0)
			{
				m_PlusPoint.x = 0;
				firstPointX = 0;				
			}			
		}
		else
		{
			m_PlusPoint.x = 0;		
			firstPointX = 0;			
		}

		firstSector.x = 0;			
	}
	if (firstSector.x+g_SECTOR_WIDTH+1 >= m_pZone->GetWidth())
	{
		if (firstSector.x+g_SECTOR_WIDTH+1==m_pZone->GetWidth())
		{
			if (g_pPlayer->GetSX() > 0)
			{
				m_PlusPoint.x = 0;
				firstPointX = 0;				
			}			
		}
		else
		{
			m_PlusPoint.x = 0;	
			firstPointX = 0;			
		}

		firstSector.x = m_pZone->GetWidth()-g_SECTOR_WIDTH-1;
	}

	//---------------------------------------------------------------	
	// Adjust the Y coordinate
	//---------------------------------------------------------------	
	if (firstSector.y <= 0)
	{
		if (firstSector.y==0)
		{
			if (g_pPlayer->GetSY() < 0)
			{
				m_PlusPoint.y = 0;
				firstPointY = 0;
			}			
		}
		else
		{
			m_PlusPoint.y = 0;
			firstPointY = 0;
		}

		firstSector.y = 0;
	}
	else if (firstSector.y+g_SECTOR_HEIGHT >= m_pZone->GetHeight())
	{
		if (firstSector.y+g_SECTOR_HEIGHT == m_pZone->GetHeight())
		{
			if (g_pPlayer->GetSY() > 0)
			{
				m_PlusPoint.y = 0;
				firstPointY = 0;
			}			
		}
		else
		{
			m_PlusPoint.y = 0;	
			firstPointY = 0;
		}

		firstSector.y = m_pZone->GetHeight()-g_SECTOR_HEIGHT;	
	}

	//---------------------------------------------------------------
	// Search for ImageObjects that newly appear or disappear from the screen
	//---------------------------------------------------------------
	// 2001.8.22

	__BEGIN_PROFILE("UpdateImageObject")

	UpdateImageObject(firstSector);	

	__END_PROFILE("UpdateImageObject")


	//---------------------------------------------------------------
	// Create the information needed from outside..
	// Coordinates of the Sector drawn at the first position on the screen
	//---------------------------------------------------------------
	m_FirstSector	= firstSector;	

	//---------------------------------------------------------------
	// Zone Pixel coordinates that the screen's (0,0) represents
	//---------------------------------------------------------------
	m_FirstZonePixel = MapToPixel(firstSector.x, firstSector.y);
	m_FirstZonePixel.x += m_PlusPoint.x;
	m_FirstZonePixel.y += m_PlusPoint.y;


	//---------------------------------------------------------------
	// Output coordinates of the Filter that handles the ImageObjects covering the Player
	//---------------------------------------------------------------
	m_FilterPosition.x = g_pPlayer->GetPixelX() - m_FirstZonePixel.x - (m_ImageObjectFilter.GetWidth()>>1) + 24;
	m_FilterPosition.y = g_pPlayer->GetPixelY() - m_FirstZonePixel.y - (m_ImageObjectFilter.GetHeight()>>1) 
						- (g_pPlayer->IsFlyingCreature()? 3:1 )*TILE_Y;
	//filterPoint.x = 310;	// 410 - 100;
	//filterPoint.y = 116;	// 204 - 100;


	//short	sX,
	//		sY = (short)firstSector.y;

	// Position on the Surface to draw to
	tilePoint.y = firstPointY;


	//--------------------------------------------------
	// TileSurface area : m_TileSurfaceFirstPixelXY
	//                      + (TILESURFACE_WIDTH+ TILESURFACE_HEIGHT)
	//
	// Current screen area   : m_FirstZonePixel + (800,600)
	//--------------------------------------------------
	//
	// If the current screen area lies within the TileSurface,
	// part of the TileSurface can be used as it is.
	//
	//
	//--------------------------------------------------
	__BEGIN_PROFILE("ReuseTileSurface")

	RECT rectTileSurface;
	RECT rectScreen;
	RECT	rectReuse;

	// Area of the TileSurface in the Zone
	rectTileSurface.left	= m_TileSurfaceFirstZonePixelX;
	rectTileSurface.top		= m_TileSurfaceFirstZonePixelY;
	rectTileSurface.right	= m_TileSurfaceFirstZonePixelX + g_TILESURFACE_WIDTH;
	rectTileSurface.bottom	= m_TileSurfaceFirstZonePixelY + g_TILESURFACE_HEIGHT;

	// Area of the current screen
	rectScreen.left		= m_FirstZonePixel.x;
	rectScreen.top		= m_FirstZonePixel.y;
	rectScreen.right	= m_FirstZonePixel.x + g_GameRect.right;
	rectScreen.bottom	= m_FirstZonePixel.y + g_GameRect.bottom;

	// Decide the area of the TileSurface to use for drawing the current screen.
	// First point in the TileSurface that will be the screen's (0,0)
	rectReuse.left		= rectScreen.left - rectTileSurface.left;
	rectReuse.top		= rectScreen.top - rectTileSurface.top;
	rectReuse.right		= rectReuse.left + g_GameRect.right;
	rectReuse.bottom	= rectReuse.top + g_GameRect.bottom;

	//----------------------------------------------------------------	
	// The part other than rectReuse must be painted black.
	//----------------------------------------------------------------
	/*
	// Top
	if (rectReuse.top != 0)
	{
		rect.left = 0;
		rect.right = SURFACE_WIDTH;
		rect.top = 0;
		rect.bottom = rectReuse.top;

		m_pSurface->FillRect(&rect, 0);
	}

	// Bottom
	if (rectReuse.bottom != SURFACE_HEIGHT)
	{
		rect.left = 0;
		rect.right = SURFACE_WIDTH;
		rect.top = rectReuse.bottom;
		rect.bottom = SURFACE_HEIGHT;

		m_pSurface->FillRect(&rect, 0);
	}

	// Left
	if (rectReuse.left != 0)
	{
		rect.left = 0;
		rect.right = rectReuse.left;
		rect.top = rectReuse.top;
		rect.bottom = rectReuse.bottom;	

		m_pSurface->FillRect(&rect, 0);
	}

	// Right
	if (rectReuse.right != SURFACE_WIDTH)
	{
		rect.left = rectReuse.right;
		rect.right = SURFACE_WIDTH;
		rect.top = rectReuse.top;
		rect.bottom = rectReuse.bottom;	

		m_pSurface->FillRect(&rect, 0);
	}
	*/



	//--------------------------------------------------
	// Check whether the TileSurface can be used as it is.
	//--------------------------------------------------
	// Mark the insufficient part
	BYTE bLack = 0;

	int leftGap = rectScreen.left - rectTileSurface.left;
	int rightGap = rectScreen.right - rectTileSurface.right;
	int topGap = rectScreen.top - rectTileSurface.top;
	int bottomGap = rectScreen.bottom - rectTileSurface.bottom;

	if (leftGap <= 0) bLack |= FLAG_TILESURFACE_LACK_LEFT;
	if (rightGap >= 0) bLack |= FLAG_TILESURFACE_LACK_RIGHT;
	if (topGap <= 0) bLack |= FLAG_TILESURFACE_LACK_UP;
	if (bottomGap >= 0) bLack |= FLAG_TILESURFACE_LACK_DOWN;	


	//---------------------------------------------------------------------
	//
	// When the current screen lies within the TileSurface
	//
	//---------------------------------------------------------------------
	if (bLack==0)
	{
		//....
		//---------------------------------------------------------------
		// Search for ImageObjects that newly appear or disappear from the screen
		//---------------------------------------------------------------
		// 2001.8.22
		//UpdateImageObject(firstSector);	

	}
	/*
	//---------------------------------------------------------------------
	//
	// When too much must be updated
	//
	//---------------------------------------------------------------------
	else if (abs(leftGap) >= (TILE_X<<2)
			|| abs(rightGap) >= (TILE_X<<2)
			|| abs(topGap) >= (TILE_Y<<2)
			|| abs(bottomGap) >= (TILE_Y<<2)
			)
	{
		DEBUG_ADD("[DrawZone] Too Far Move");

		m_mapImageObject.clear();
		m_mapCreature.clear();

		// Redraw all the Tiles.
		DrawTileSurface();

		DetermineImageObject();		

		m_bFirstTileDraw = false;

		DEBUG_ADD("[DrawZone] Too Far Move - DrawTileSurface OK");		
	}
	*/
	//---------------------------------------------------------------------
	//
	// When the current screen does not lie within the TileSurface
	//
	//---------------------------------------------------------------------
	else
	{
		//---------------------------------------------------------------
		// Search for ImageObjects that newly appear or disappear from the screen
		//---------------------------------------------------------------
		// 2001.8.22
		//UpdateImageObject(firstSector);	

		//----------------------------------------
		// Information needed to update only partially
		//----------------------------------------
		BYTE	changeCount = 0;	// 1 or 2
		int		sX1[2], sX2[2], sY1[2], sY2[2];
		POINT	firstTilePoint[2];			


		//--------------------------
		// LEFT....
		//--------------------------
		if (bLack & FLAG_TILESURFACE_LACK_LEFT)
		{
			//--------------------------
			// LEFT + UP insufficient
			//--------------------------
			if (bLack & FLAG_TILESURFACE_LACK_UP)
			{
				//--------------------------------------------------
				// From the previous Tile Surface,
				// keep the part that can be used
				// in the Tile Surface being rebuilt.
				//--------------------------------------------------
				point.x = TILESURFACE_OUTLINE_LEFT;
				point.y = TILESURFACE_OUTLINE_UP;
				rect.left	= 0;
				rect.top	= 0;
				rect.right	= g_TILESURFACE_OUTLINE_RIGHT;
				rect.bottom = g_TILESURFACE_OUTLINE_DOWN;
				m_pTileSurface->BltNoColorkey(&point, m_pTileSurface, &rect);

				//--------------------------------------------------
				//
				// Draw the insufficient part.
				//
				//--------------------------------------------------				

				changeCount = 2;

				//--------------------------------------------------
				// Left part
				//--------------------------------------------------
				// Draw sectors (sX1,sY) ~ (sX2, sY2)
				//--------------------------------------------------			
				sX1[0] = m_TileSurfaceFirstSectorX - TILESURFACE_SECTOR_EDGE;
				sY1[0] = m_TileSurfaceFirstSectorY - TILESURFACE_SECTOR_EDGE;
				sX2[0] = sX1[0]	+ TILESURFACE_SECTOR_EDGE;
				sY2[0] = sY1[0] + g_TILESURFACE_SECTOR_HEIGHT;

				//--------------------------------------------------
				// First output coordinates
				//--------------------------------------------------
				firstTilePoint[0].x = 0;
				firstTilePoint[0].y = 0;

				//--------------------------------------------------
				// Up part
				//--------------------------------------------------
				// Draw sectors (sX1,sY) ~ (sX2, sY2)
				//--------------------------------------------------			
				sX1[1] = m_TileSurfaceFirstSectorX - TILESURFACE_SECTOR_EDGE;
				sY1[1] = m_TileSurfaceFirstSectorY - TILESURFACE_SECTOR_EDGE;
				sX2[1] = sX1[1]	+ g_TILESURFACE_SECTOR_WIDTH;
				sY2[1] = sY1[1] + TILESURFACE_SECTOR_EDGE;

				//--------------------------------------------------
				// First output coordinates
				//--------------------------------------------------
				firstTilePoint[1].x = 0;
				firstTilePoint[1].y = 0;


				//--------------------------------------------------
				// Reset the TileSurface area to use for drawing
				//--------------------------------------------------
				rectReuse.left += TILESURFACE_OUTLINE_LEFT;
				rectReuse.right += TILESURFACE_OUTLINE_LEFT;
				rectReuse.top += TILESURFACE_OUTLINE_UP;
				rectReuse.bottom += TILESURFACE_OUTLINE_UP;

				//--------------------------------------------------
				// Set the coordinates for the updated TileSurface
				//--------------------------------------------------
				m_TileSurfaceFirstSectorX -= TILESURFACE_SECTOR_EDGE;
				m_TileSurfaceFirstZonePixelX -= TILESURFACE_OUTLINE_LEFT;
				m_TileSurfaceFirstSectorY -= TILESURFACE_SECTOR_EDGE;
				m_TileSurfaceFirstZonePixelY -= TILESURFACE_OUTLINE_UP;
			}			
			//--------------------------
			// LEFT + DOWN insufficient
			//--------------------------
			else if (bLack & FLAG_TILESURFACE_LACK_DOWN)
			{
				//--------------------------------------------------
				// From the previous Tile Surface,
				// keep the part that can be used
				// in the Tile Surface being rebuilt.
				//--------------------------------------------------
				point.x = TILESURFACE_OUTLINE_LEFT;
				point.y = 0;
				rect.left	= 0;
				rect.top	= TILESURFACE_OUTLINE_UP;
				rect.right	= g_TILESURFACE_OUTLINE_RIGHT;
				rect.bottom = g_TILESURFACE_HEIGHT;			
				m_pTileSurface->BltNoColorkey(&point, m_pTileSurface, &rect);

				//--------------------------------------------------
				//
				// Draw the insufficient part.
				//
				//--------------------------------------------------

				changeCount = 2;

				//--------------------------------------------------
				// Left part
				//--------------------------------------------------
				// Draw sectors (sX1,sY) ~ (sX2, sY2)
				//--------------------------------------------------			
				sX1[0] = m_TileSurfaceFirstSectorX - TILESURFACE_SECTOR_EDGE;
				sY1[0] = m_TileSurfaceFirstSectorY + TILESURFACE_SECTOR_EDGE;
				sX2[0] = sX1[0]	+ TILESURFACE_SECTOR_EDGE;
				sY2[0] = sY1[0] + g_TILESURFACE_SECTOR_HEIGHT;

				//--------------------------------------------------
				// First output coordinates
				//--------------------------------------------------
				firstTilePoint[0].x = 0;
				firstTilePoint[0].y = 0;

				//--------------------------------------------------
				// Down part
				//--------------------------------------------------
				// Draw sectors (sX1,sY) ~ (sX2, sY2)
				//--------------------------------------------------			
				sX1[1] = m_TileSurfaceFirstSectorX - TILESURFACE_SECTOR_EDGE;
				sY1[1] = m_TileSurfaceFirstSectorY + g_TILESURFACE_SECTOR_HEIGHT;
				sX2[1] = sX1[1]	+ g_TILESURFACE_SECTOR_WIDTH;
				sY2[1] = sY1[1] + TILESURFACE_SECTOR_EDGE;

				//--------------------------------------------------
				// First output coordinates
				//--------------------------------------------------
				firstTilePoint[1].x = 0;
				firstTilePoint[1].y = g_TILESURFACE_OUTLINE_DOWN;

				//--------------------------------------------------
				// Reset the TileSurface area to use for drawing
				//--------------------------------------------------
				rectReuse.left += TILESURFACE_OUTLINE_LEFT;
				rectReuse.right += TILESURFACE_OUTLINE_LEFT;
				rectReuse.top -= TILESURFACE_OUTLINE_UP;
				rectReuse.bottom -= TILESURFACE_OUTLINE_UP;

				//--------------------------------------------------
				// Set the coordinates for the updated TileSurface
				//--------------------------------------------------
				m_TileSurfaceFirstSectorX -= TILESURFACE_SECTOR_EDGE;
				m_TileSurfaceFirstZonePixelX -= TILESURFACE_OUTLINE_LEFT;
				m_TileSurfaceFirstSectorY += TILESURFACE_SECTOR_EDGE;
				m_TileSurfaceFirstZonePixelY += TILESURFACE_OUTLINE_UP;
			}
			//--------------------------
			// LEFT insufficient
			//--------------------------
			else
			{
				//--------------------------------------------------
				// From the previous Tile Surface,
				// keep the part that can be used
				// in the Tile Surface being rebuilt.
				//--------------------------------------------------
				point.x = TILESURFACE_OUTLINE_LEFT;
				point.y = 0;
				rect.left	= 0;
				rect.top	= 0;
				rect.right	= g_TILESURFACE_OUTLINE_RIGHT;
				rect.bottom = g_TILESURFACE_HEIGHT;			
				m_pTileSurface->BltNoColorkey(&point, m_pTileSurface, &rect);

				//--------------------------------------------------
				//
				// Draw the insufficient part.
				//
				//--------------------------------------------------

				changeCount = 1;

				//--------------------------------------------------
				// Draw sectors (sX1,sY) ~ (sX2, sY2)
				//--------------------------------------------------			
				sX1[0] = m_TileSurfaceFirstSectorX - TILESURFACE_SECTOR_EDGE;
				sY1[0] = m_TileSurfaceFirstSectorY;
				sX2[0] = sX1[0]	+ TILESURFACE_SECTOR_EDGE;
				sY2[0] = sY1[0] + g_TILESURFACE_SECTOR_HEIGHT;

				//--------------------------------------------------
				// First output coordinates
				//--------------------------------------------------
				firstTilePoint[0].x = 0;
				firstTilePoint[0].y = 0;

				//--------------------------------------------------
				// Reset the TileSurface area to use for drawing
				//--------------------------------------------------
				rectReuse.left += TILESURFACE_OUTLINE_LEFT;
				rectReuse.right += TILESURFACE_OUTLINE_LEFT;

				//--------------------------------------------------
				// Set the coordinates for the updated TileSurface
				//--------------------------------------------------
				m_TileSurfaceFirstSectorX -= TILESURFACE_SECTOR_EDGE;
				m_TileSurfaceFirstZonePixelX -= TILESURFACE_OUTLINE_LEFT;
			}
		}		
		//--------------------------
		// RIGHT....
		//--------------------------
		else if (bLack & FLAG_TILESURFACE_LACK_RIGHT)
		{
			//--------------------------
			// RIGHT + UP insufficient
			//--------------------------
			if (bLack & FLAG_TILESURFACE_LACK_UP)
			{
				//--------------------------------------------------
				// From the previous Tile Surface,
				// keep the part that can be used
				// in the Tile Surface being rebuilt.
				//--------------------------------------------------
				point.x = 0;
				point.y = TILESURFACE_OUTLINE_UP;
				rect.left	= TILESURFACE_OUTLINE_LEFT;
				rect.top	= 0;
				rect.right	= g_TILESURFACE_WIDTH;
				rect.bottom = g_TILESURFACE_OUTLINE_DOWN;
				m_pTileSurface->BltNoColorkey(&point, m_pTileSurface, &rect);

				//--------------------------------------------------
				//
				// Draw the insufficient part.
				//
				//--------------------------------------------------

				changeCount = 2;

				//--------------------------------------------------
				// Right part
				//--------------------------------------------------
				// Draw sectors (sX1,sY) ~ (sX2, sY2)
				//--------------------------------------------------			
				sX1[0] = m_TileSurfaceFirstSectorX + g_TILESURFACE_SECTOR_WIDTH;
				sY1[0] = m_TileSurfaceFirstSectorY - TILESURFACE_SECTOR_OUTLINE_UP;
				sX2[0] = sX1[0]	+ TILESURFACE_SECTOR_EDGE;
				sY2[0] = sY1[0] + g_TILESURFACE_SECTOR_HEIGHT;

				//--------------------------------------------------
				// First output coordinates
				//--------------------------------------------------
				firstTilePoint[0].x = g_TILESURFACE_OUTLINE_RIGHT;
				firstTilePoint[0].y = 0;

				//--------------------------------------------------
				// UP part
				//--------------------------------------------------
				// Draw sectors (sX1,sY) ~ (sX2, sY2)
				//--------------------------------------------------			
				sX1[1] = m_TileSurfaceFirstSectorX + TILESURFACE_SECTOR_OUTLINE_LEFT;
				sY1[1] = m_TileSurfaceFirstSectorY - TILESURFACE_SECTOR_EDGE;
				sX2[1] = sX1[1]	+ g_TILESURFACE_SECTOR_WIDTH;
				sY2[1] = sY1[1] + TILESURFACE_SECTOR_EDGE;

				//--------------------------------------------------
				// First output coordinates
				//--------------------------------------------------
				firstTilePoint[1].x = 0;
				firstTilePoint[1].y = 0;

				//--------------------------------------------------
				// Reset the TileSurface area to use for drawing
				//--------------------------------------------------
				rectReuse.left -= TILESURFACE_OUTLINE_LEFT;
				rectReuse.right -= TILESURFACE_OUTLINE_LEFT;
				rectReuse.top += TILESURFACE_OUTLINE_UP;
				rectReuse.bottom += TILESURFACE_OUTLINE_UP;

				//--------------------------------------------------
				// Set the coordinates for the updated TileSurface
				//--------------------------------------------------
				m_TileSurfaceFirstSectorX += TILESURFACE_SECTOR_EDGE;
				m_TileSurfaceFirstZonePixelX += TILESURFACE_OUTLINE_LEFT;
				m_TileSurfaceFirstSectorY -= TILESURFACE_SECTOR_EDGE;
				m_TileSurfaceFirstZonePixelY -= TILESURFACE_OUTLINE_UP;
			}
			//--------------------------
			// RIGHT + DOWN insufficient
			//--------------------------
			else if (bLack & FLAG_TILESURFACE_LACK_DOWN)
			{
				//--------------------------------------------------
				// From the previous Tile Surface,
				// keep the part that can be used
				// in the Tile Surface being rebuilt.
				//--------------------------------------------------
				point.x = 0;
				point.y = 0;
				rect.left	= TILESURFACE_OUTLINE_LEFT;
				rect.top	= TILESURFACE_OUTLINE_UP;
				rect.right	= g_TILESURFACE_WIDTH;
				rect.bottom = g_TILESURFACE_HEIGHT;			
				m_pTileSurface->BltNoColorkey(&point, m_pTileSurface, &rect);

				//--------------------------------------------------
				//
				// Draw the insufficient part.
				//
				//--------------------------------------------------

				changeCount = 2;

				//--------------------------------------------------
				// Right part
				//--------------------------------------------------
				// Draw sectors (sX1,sY) ~ (sX2, sY2)
				//--------------------------------------------------			
				sX1[0] = m_TileSurfaceFirstSectorX + g_TILESURFACE_SECTOR_WIDTH;
				sY1[0] = m_TileSurfaceFirstSectorY + TILESURFACE_SECTOR_OUTLINE_UP;
				sX2[0] = sX1[0]	+ TILESURFACE_SECTOR_EDGE;
				sY2[0] = sY1[0] + g_TILESURFACE_SECTOR_HEIGHT;

				//--------------------------------------------------
				// First output coordinates
				//--------------------------------------------------
				firstTilePoint[0].x = g_TILESURFACE_OUTLINE_RIGHT;
				firstTilePoint[0].y = 0;

				//--------------------------------------------------
				// Down part
				//--------------------------------------------------
				// Draw sectors (sX1,sY) ~ (sX2, sY2)
				//--------------------------------------------------			
				sX1[1] = m_TileSurfaceFirstSectorX + TILESURFACE_SECTOR_OUTLINE_LEFT;
				sY1[1] = m_TileSurfaceFirstSectorY + g_TILESURFACE_SECTOR_HEIGHT;
				sX2[1] = sX1[1]	+ g_TILESURFACE_SECTOR_WIDTH;
				sY2[1] = sY1[1] + TILESURFACE_SECTOR_EDGE;

				//--------------------------------------------------
				// First output coordinates
				//--------------------------------------------------
				firstTilePoint[1].x = 0;
				firstTilePoint[1].y = g_TILESURFACE_OUTLINE_DOWN;

				//--------------------------------------------------
				// Reset the TileSurface area to use for drawing
				//--------------------------------------------------
				rectReuse.left -= TILESURFACE_OUTLINE_LEFT;
				rectReuse.right -= TILESURFACE_OUTLINE_LEFT;
				rectReuse.top -= TILESURFACE_OUTLINE_UP;
				rectReuse.bottom -= TILESURFACE_OUTLINE_UP;

				//--------------------------------------------------
				// Set the coordinates for the updated TileSurface
				//--------------------------------------------------
				m_TileSurfaceFirstSectorX += TILESURFACE_SECTOR_EDGE;
				m_TileSurfaceFirstZonePixelX += TILESURFACE_OUTLINE_LEFT;
				m_TileSurfaceFirstSectorY += TILESURFACE_SECTOR_EDGE;
				m_TileSurfaceFirstZonePixelY += TILESURFACE_OUTLINE_UP;
			}
			//--------------------------
			// RIGHT insufficient
			//--------------------------
			else
			{
				//--------------------------------------------------
				// From the previous Tile Surface,
				// keep the part that can be used
				// in the Tile Surface being rebuilt.
				//--------------------------------------------------
				point.x = 0;
				point.y = 0;
				rect.left	= TILESURFACE_OUTLINE_LEFT;
				rect.top	= 0;
				rect.right	= g_TILESURFACE_WIDTH;
				rect.bottom = g_TILESURFACE_HEIGHT;			
				m_pTileSurface->BltNoColorkey(&point, m_pTileSurface, &rect);

				//--------------------------------------------------
				//
				// Draw the insufficient part.
				//
				//--------------------------------------------------

				changeCount = 1;

				//--------------------------------------------------
				// Draw sectors (sX1,sY) ~ (sX2, sY2)
				//--------------------------------------------------			
				sX1[0] = m_TileSurfaceFirstSectorX + g_TILESURFACE_SECTOR_WIDTH;
				sY1[0] = m_TileSurfaceFirstSectorY;
				sX2[0] = sX1[0]	+ TILESURFACE_SECTOR_EDGE;
				sY2[0] = sY1[0] + g_TILESURFACE_SECTOR_HEIGHT;

				//--------------------------------------------------
				// First output coordinates
				//--------------------------------------------------
				firstTilePoint[0].x = g_TILESURFACE_OUTLINE_RIGHT;
				firstTilePoint[0].y = 0;

				//--------------------------------------------------
				// Reset the TileSurface area to use for drawing
				//--------------------------------------------------
				rectReuse.left -= TILESURFACE_OUTLINE_LEFT;
				rectReuse.right -= TILESURFACE_OUTLINE_LEFT;

				//--------------------------------------------------
				// Set the coordinates for the updated TileSurface
				//--------------------------------------------------
				m_TileSurfaceFirstSectorX += TILESURFACE_SECTOR_EDGE;
				m_TileSurfaceFirstZonePixelX += TILESURFACE_OUTLINE_LEFT;
			}
		}
		//--------------------------
		// UP insufficient
		//--------------------------
		else if (bLack & FLAG_TILESURFACE_LACK_UP)
		{
			//--------------------------------------------------
			// From the previous Tile Surface,
			// keep the part that can be used
			// in the Tile Surface being rebuilt.
			//--------------------------------------------------
			point.x = 0;
			point.y = TILESURFACE_OUTLINE_UP;
			rect.left	= 0;
			rect.top	= 0;
			rect.right	= g_TILESURFACE_WIDTH;
			rect.bottom = g_TILESURFACE_OUTLINE_DOWN;
			m_pTileSurface->BltNoColorkey(&point, m_pTileSurface, &rect);

			//--------------------------------------------------
			//
			// Draw the insufficient part.
			//
			//--------------------------------------------------

			changeCount = 1;

			//--------------------------------------------------
			// Draw sectors (sX1,sY) ~ (sX2, sY2)
			//--------------------------------------------------			
			sX1[0] = m_TileSurfaceFirstSectorX;
			sY1[0] = m_TileSurfaceFirstSectorY - TILESURFACE_SECTOR_EDGE;
			sX2[0] = sX1[0]	+ g_TILESURFACE_SECTOR_WIDTH;
			sY2[0] = sY1[0] + TILESURFACE_SECTOR_EDGE;

			//--------------------------------------------------
			// First output coordinates
			//--------------------------------------------------
			firstTilePoint[0].x = 0;
			firstTilePoint[0].y = 0;

			//--------------------------------------------------
			// Reset the TileSurface area to use for drawing
			//--------------------------------------------------
			rectReuse.top += TILESURFACE_OUTLINE_UP;
			rectReuse.bottom += TILESURFACE_OUTLINE_UP;

			//--------------------------------------------------
			// Set the coordinates for the updated TileSurface
			//--------------------------------------------------
			m_TileSurfaceFirstSectorY -= TILESURFACE_SECTOR_EDGE;
			m_TileSurfaceFirstZonePixelY -= TILESURFACE_OUTLINE_UP;
		}
		//--------------------------
		// DOWN insufficient
		//--------------------------
		else if (bLack & FLAG_TILESURFACE_LACK_DOWN)
		{
			//--------------------------------------------------
			// From the previous Tile Surface,
			// keep the part that can be used
			// in the Tile Surface being rebuilt.
			//--------------------------------------------------
			point.x = 0;
			point.y = 0;
			rect.left	= 0;
			rect.top	= TILESURFACE_OUTLINE_UP;
			rect.right	= g_TILESURFACE_WIDTH;
			rect.bottom = g_TILESURFACE_HEIGHT;
			m_pTileSurface->BltNoColorkey(&point, m_pTileSurface, &rect);

			//--------------------------------------------------
			//
			// Draw the insufficient part.
			//
			//--------------------------------------------------

			changeCount = 1;

			//--------------------------------------------------
			// Draw sectors (sX1,sY) ~ (sX2, sY2)
			//--------------------------------------------------			
			sX1[0] = m_TileSurfaceFirstSectorX;
			sY1[0] = m_TileSurfaceFirstSectorY + g_TILESURFACE_SECTOR_HEIGHT;
			sX2[0] = sX1[0]	+ g_TILESURFACE_SECTOR_WIDTH;
			sY2[0] = sY1[0] + TILESURFACE_SECTOR_EDGE;

			//--------------------------------------------------
			// First output coordinates
			//--------------------------------------------------
			firstTilePoint[0].x = 0;
			firstTilePoint[0].y = g_TILESURFACE_OUTLINE_DOWN;

			//--------------------------------------------------
			// Reset the TileSurface area to use for drawing
			//--------------------------------------------------
			rectReuse.top -= TILESURFACE_OUTLINE_UP;
			rectReuse.bottom -= TILESURFACE_OUTLINE_UP;

			//--------------------------------------------------
			// Set the coordinates for the updated TileSurface
			//--------------------------------------------------
			m_TileSurfaceFirstSectorY += TILESURFACE_SECTOR_EDGE;
			m_TileSurfaceFirstZonePixelY += TILESURFACE_OUTLINE_UP;
		}


		//------------------------------------------------------
		//
		//   Update the Tiles partially..
		//
		//------------------------------------------------------
		int n;
		for (n=0; n<changeCount; n++)
		{			

			//------------------------------------------------------
			// Skip when outside the Zone area...
			//------------------------------------------------------
			if (sX1[n] < 0) 
			{	
				// Paint the part outside the Zone black.
				rect.left = firstTilePoint[n].x;
				rect.top = 0;	

				firstTilePoint[n].x += -sX1[n] * TILE_X;

				rect.right = firstTilePoint[n].x;
				rect.bottom = g_TILESURFACE_HEIGHT;				 

				m_pTileSurface->FillRect(&rect, 0);

				sX1[n] = 0;	
			}

			if (sX2[n] > m_pZone->GetWidth())
			{
				sX2[n] = m_pZone->GetWidth();//-1;				

				// Paint the part outside the Zone black.
				rect.left = firstTilePoint[n].x + (sX2[n]-sX1[n])*TILE_X;
				rect.top = 0;					
				rect.right = g_TILESURFACE_WIDTH;
				rect.bottom = g_TILESURFACE_HEIGHT;

				m_pTileSurface->FillRect(&rect, 0);				
			}

			if (sY1[n] < 0)
			{
				// Paint the part outside the Zone black.
				rect.left = firstTilePoint[n].x;
				rect.top = firstTilePoint[n].y;	

				firstTilePoint[n].y += -sY1[n] * TILE_Y;

				rect.right = firstTilePoint[n].x + (sX2[n]-sX1[n])*TILE_X;
				rect.bottom = firstTilePoint[n].y;
				m_pTileSurface->FillRect(&rect, 0);

				sY1[n] = 0;	
			}

			if (sY2[n] > m_pZone->GetHeight())
			{
				// Paint the part outside the Zone black.
				rect.left = firstTilePoint[n].x;
				rect.top = firstTilePoint[n].y;					
				rect.right = firstTilePoint[n].x + (sX2[n]-sX1[n])*TILE_X;
				rect.bottom = g_TILESURFACE_HEIGHT;
				m_pTileSurface->FillRect(&rect, 0);

				sY2[n] = m_pZone->GetHeight();//-1;
			}

			// First row

			//---------------------------------------
			// LOCK
			//---------------------------------------
			if (!m_pTileSurface->Lock()) return;

			//----------------------------------------------------------------------
			// Use TileRenderer for incremental updates
			// Ensures consistency with DrawTileSurface()
			//----------------------------------------------------------------------
			if (m_pTileRenderer != NULL && m_pTileRenderer->IsInit())
			{
				// Set the zone provider
				m_zoneTileProvider.SetZone(m_pZone);

				// Draw new edge tiles using TileRenderer
				m_pTileRenderer->DrawTilesNoLock(
					&m_zoneTileProvider,
					sX1[n], sY1[n],
					sX2[n] - sX1[n],
					sY2[n] - sY1[n],
					firstTilePoint[n].x,
					firstTilePoint[n].y
				);
			}
			else
			{
				printf("[IncrementalUpdate] ERROR: TileRenderer not initialized! Cannot render tiles.\n");
			}

			// UNLOCK
			//---------------------------------------
			m_pTileSurface->Unlock();
		}

//		m_bFogChanged = true;
	}

	//----------------------------------------------------------------
	//
	// Tile Image to Current Surface
	//
	//----------------------------------------------------------------	
	point.x = 0;
	point.y = 0;


	////m_pSurface->BltDarkness(&point, m_pTileSurface, &rectReuse, DARK_VALUE);

	//----------------------------------------------------------------	
	//
	// [ TEST CODE ]  Perspective
	//
	//----------------------------------------------------------------	


	bool bDrawBackGround = DrawEvent();

	__BEGIN_PROFILE("ReuseBltTileSurface")
	// Adam's cloud was drawn behind the tiles here (2004). The SDL port
	// never drew it: its image went into a CDirectDrawSurface, a stub that
	// holds no pixels, and the blit read that as a CSpriteSurface without
	// a backend surface, so it returned at once. Task 4.224 retired that
	// path; restoring the cloud needs its JPEG decoded into a
	// CSpriteSurface. The tiles are blitted the same either way.
	if(bDrawBackGround)
	{
		m_pSurface->BltNoColorkey(&point, m_pTileSurface, &rectReuse);
	}


	__END_PROFILE("ReuseBltTileSurface")

	__END_PROFILE("ReuseTileSurface")

	POINT tilePointTemp;
	int sX1, sX2, sY1, sY2;	

	//------------------------------------------------------
	// Set the screen coordinates for drawing Objects
	//------------------------------------------------------
	sX1 = firstSector.x + SECTOR_SKIP_LEFT;
	sY1 = firstSector.y + SECTOR_SKIP_UP;
	sX2 = firstSector.x + g_SECTOR_WIDTH;
	sY2 = firstSector.y + g_SECTOR_HEIGHT;

	// Position on the Surface to draw to
	tilePoint.x = firstPointX + TILE_X*SECTOR_SKIP_LEFT;
	tilePoint.y = firstPointY + TILE_Y*SECTOR_SKIP_UP;

	//------------------------------------------------------
	// Skip when outside the Zone area...
	//------------------------------------------------------
	if (sX1 < 0) 
	{					
		tilePoint.x += -sX1 * TILE_X;
		sX1 = 0;	
	}

	if (sX2 >= m_pZone->GetWidth())
	{
		sX2 = m_pZone->GetWidth()-1;
	}

	if (sY1 < 0)
	{
		tilePoint.y += -sY1 * TILE_Y;
		sY1 = 0;	
	}

	if (sY2 >= m_pZone->GetHeight())
	{
		sY2 = m_pZone->GetHeight()-1;
	}



	//------------------------------------------------------
	//
	//                  Object output
	//
	//------------------------------------------------------

	//BYTE DarkBits;

	//---------------------------------------
	// LOCK
	//---------------------------------------
	if (!m_pSurface->Lock()) return;

// 2004, 03, 24, sobeit start
	//------------------------------------------------------
	// Create the Creature OutputMap.
	//------------------------------------------------------
	__BEGIN_PROFILE("AddOutputCreature")

	AddOutputCreatureAll();

	__END_PROFILE("AddOutputCreature")

	BOOL bUnlockStatus = true && g_pUserOption->BlendingShadow;
	if (bUnlockStatus)
	{	
		m_pSurface->Unlock();
	}


	CREATURE_OUTPUT_MAP::const_iterator iCreatureOutput = m_mapCreature.begin();

	//------------------------------------------------------
	// Creature - ghost draw.
	//------------------------------------------------------
	while (iCreatureOutput != m_mapCreature.end())
	{
		MCreature* const pCreature = (MCreature*)((*iCreatureOutput).second);

		if(pCreature->GetCreatureType() == CREATURETYPE_GHOST)
		{
			// Draw when the output viewpoint is less than sY1..
			// Fit the picture's coordinates to the current screen's coordinates
			point.x = pCreature->GetPixelX() - m_FirstZonePixel.x;
			point.y = pCreature->GetPixelY() - m_FirstZonePixel.y;

			// Decide the brightness.
			//int DarkBits = (m_pZone->GetSector(pCreature->GetX(),pCreature->GetY()).GetLight()==0)?m_DarkBits:0;

			DrawCreature(&point, pCreature);//, DarkBits);
		}

		iCreatureOutput ++;
	}
// 2004, 03, 24, sobeit end


	// Save the Surface information.
	//SetSurfaceInfo(&m_SurfaceInfo, m_pSurface->GetDDSD());

	//------------------------------------------------------
	//
	//			Floor object output
	//
	//------------------------------------------------------	
	//------------------------------------------------------
	// ImageObject Iterator that is the output target
	//------------------------------------------------------
	IMAGEOBJECT_OUTPUT_MAP::const_iterator iImageObjectOutput0 = m_mapImageObject.begin();

	//------------------------------------------------------
	// For output viewpoint 0, i.e. floor objects,
	// draw the ImageObjects.
	// m_mapImageObject is sorted with the Viewpoint as the key.
	//------------------------------------------------------
	while (bDrawBackGround && iImageObjectOutput0 != m_mapImageObject.end())
	{
		MImageObject* const pImageObject = (MImageObject*)((*iImageObjectOutput0).second);

		// Draw when the output viewpoint is 0..
		if (pImageObject->GetViewpoint() == 0)
		{
			// Fit the picture's coordinates to the current screen's coordinates
			point.x = pImageObject->GetPixelX() - m_FirstZonePixel.x;
			point.y = pImageObject->GetPixelY() - m_FirstZonePixel.y;

			DrawImageObject(&point, pImageObject);
		}

		iImageObjectOutput0 ++;
	}


	//------------------------------------------------------
	//
	//			Ground Effect output
	//
	//------------------------------------------------------	
	// When the player is inside a coffin [new skill 3]
	//------------------------------------------------------
	bool bPlayerInCasket = g_pPlayer->IsInCasket();

	if (bPlayerInCasket)
	{
		// [new skill 3]
		// Darken the whole screen. Tiles only - -;;
		rect.left = 0;
		rect.right = g_GameRect.right;
		rect.top = 0;
		rect.bottom = g_GameRect.bottom;

		DrawAlphaBox(&rect, 0, 0, 0, 20);	
	}
	else
	{	
		__BEGIN_PROFILE("DrawGroundEffect")

		DrawGroundEffect();

		__END_PROFILE("DrawGroundEffect")
	}


	//------------------------------------------------------
	//
	// Mark the selected Sector.
	//
	//------------------------------------------------------
	// When there is a selected position.. and no selected Object...
	//if (g_pUserInformation->Invisible)
	{
	}
	//else


//	if (!g_pUserInformation->Invisible)
	{
		if (m_SelectSector.x!=SECTORPOSITION_NULL 
			&& m_SelectSector.y!=SECTORPOSITION_NULL)		
		{
			POINT selectedPoint = MapToScreen( m_SelectSector.x, m_SelectSector.y );

			// [ TEST CODE ]
			// 0,1,2,3,4,5
			// 2,2,3,3,4,4		
			const int MaxClickFrame = 6;
			const int frameID[MaxClickFrame] =
			{
				4, 4, 5, 5, 6, 6
			};

			static int clickFrame = 0;

			if(g_pEventManager->GetEventByFlag(EVENTFLAG_NOT_DRAW_CREATURE) == NULL)
			{
				// center coordinate correction
				CSprite* pSprite = &m_EtcSPK[ frameID[clickFrame] ];

				selectedPoint.x += 24 - (pSprite->GetWidth()>>1);
				selectedPoint.y += 24 - (pSprite->GetHeight()>>1);


				m_pSurface->BltSprite(&selectedPoint, pSprite);
			}

			static MonotonicClock::TimePoint lastTime = g_FrameNow;
			if (g_FrameNow - lastTime >= MonotonicClock::Millis(g_UpdateDelay))
			{
				if (++clickFrame==MaxClickFrame) clickFrame = 0;
				lastTime = g_FrameNow;
			}


			// If the Player has arrived at the selected position..
			if (g_pPlayer->GetX()==m_SelectSector.x && g_pPlayer->GetY()==m_SelectSector.y)
			{
				m_SelectSector.x = SECTORPOSITION_NULL;
				m_SelectSector.y = SECTORPOSITION_NULL;
			}

		}	
	}


	//------------------------------------------------------
	// 
	//       ImageObject shadow output
	//
	//------------------------------------------------------
	//------------------------------------------------------
	// ImageObject Iterator that is the output target
	//------------------------------------------------------
	__BEGIN_PROFILE("DrawImageObjectShadow")

	IMAGEOBJECT_OUTPUT_MAP::const_iterator iImageObjectShadowOutput = m_mapImageObject.begin();

	while (bDrawBackGround && iImageObjectShadowOutput != m_mapImageObject.end())
	{
		MImageObject* const pImageObject = (MImageObject*)((*iImageObjectShadowOutput).second);

		// Fit the picture's coordinates to the current screen's coordinates
		point.x = pImageObject->GetPixelX() - m_FirstZonePixel.x;
		point.y = pImageObject->GetPixelY() - m_FirstZonePixel.y;

		if (pImageObject->GetSpriteID()==SPRITEID_NULL)
		{
			/*
			if (pImageObject->IsBltTypeNormal())
			{
				sprite = m_ImageObjectFPK[ pImageObject->GetFrameID() ][ pImageObject->GetFrame() ].GetSpriteID();
			}
			else
			{
				sprite = m_EffectAlphaFPK[ pImageObject->GetFrameID() ][ 0 ][ pImageObject->GetFrame() ].GetSpriteID();
			}
			*/
			iImageObjectShadowOutput ++;
			continue;
		}


		iImageObjectShadowOutput ++;
	}

	__END_PROFILE("DrawImageObjectShadow")

	//------------------------------------------------------
	//
	// Draw the ImageObjects that are above the output viewpoint.
	//
	//------------------------------------------------------
	//------------------------------------------------------
	// ImageObject Iterator that is the output target
	//------------------------------------------------------
	IMAGEOBJECT_OUTPUT_MAP::const_iterator iImageObjectOutput = m_mapImageObject.begin();

	//------------------------------------------------------
	// For output viewpoints less than sY1,
	// draw the ImageObjects.
	// m_mapImageObject is sorted with the Viewpoint as the key.
	//------------------------------------------------------
	while (bDrawBackGround && iImageObjectOutput != m_mapImageObject.end())
	{
		MImageObject* const pImageObject = (MImageObject*)((*iImageObjectOutput).second);

		// Draw when the output viewpoint is less than sY1..
		if (pImageObject->GetViewpoint() < sY1)
		{
			if(pImageObject->GetViewpoint() != 0)
			{
				// Fit the picture's coordinates to the current screen's coordinates
				point.x = pImageObject->GetPixelX() - m_FirstZonePixel.x;
				point.y = pImageObject->GetPixelY() - m_FirstZonePixel.y;

				DrawImageObject(&point, pImageObject);
			}
		}
		// Otherwise.. it is drawn when Objects are drawn.
		else
		{
			break;
		}

		iImageObjectOutput ++;
	}

	// To match the Frame of looping animation objects among the animation objects
	if(g_bFrameChanged)
		MAnimationObject::NextLoopFrame();

/*	- 2004, 4, 24 sobeit block - handled above because of ghost
	//------------------------------------------------------
	// Unless the player is inside a coffin.. [new skill 3]
	//------------------------------------------------------
	//------------------------------------------------------
	// Create the Creature OutputMap.
	//------------------------------------------------------
	__BEGIN_PROFILE("AddOutputCreature")

	AddOutputCreatureAll();

	__END_PROFILE("AddOutputCreature")

	//------------------------------------------------------
	//
	//			Draw the shadows of all creatures.
	//
	//------------------------------------------------------
	// Draw the shadows of all creatures.
	// (!) Must be executed after AddOutputCreatureAll().
	//
	// Assumed to run in the Locked state.
	//----------------------------------------------------
	// With 3D acceleration it is made Unlock.
	//----------------------------------------------------
	BOOL bUnlockStatus = true && g_pUserOption->BlendingShadow;
	if (bUnlockStatus)
	{	
		m_pSurface->Unlock();
	}
*/
	//------------------------------------------------------
	// Creature Iterator that is the output target
	//------------------------------------------------------
	__BEGIN_PROFILE("DrawCreatureShadowAll")

	//CREATURE_OUTPUT_MAP::const_iterator iCreatureOutput = m_mapCreature.begin();
	iCreatureOutput = m_mapCreature.begin();

	//------------------------------------------------------
	// Draw all character shadows.
	//------------------------------------------------------
	// When drawing by checking the tiles
	//------------------------------------------------------
	if (m_bTileSearchForCreature)
	{
		for (y=sY1; y<=sY2; y++)
		{
			for (x=sX1; x<=sX2; x++)
			{	
				int darknessCount = g_pPlayer->GetDarknessCount();

				// When visible regardless of darkness. effect is +2
				if (darknessCount < 0
					|| max(abs(g_pPlayer->GetX()-x), abs(g_pPlayer->GetY()-y)) <= darknessCount+2)
				//g_pPlayer->ShowInDarkness(x, y))
				{			
					const MSector& sector = m_pZone->GetSector(x,y);

					if (sector.IsExistObject())
					{
						//------------------------------------------------------
						// Underground Creature
						//------------------------------------------------------
						MCreature* pCreature;

						/*
						pCreature = sector.GetUndergroundCreature();

						if (pCreature!=NULL)
						{
							// When it is the player or visible regardless of darkness
							if (pCreature==g_pPlayer
								|| g_pPlayer->ShowInDarkness(pCreature->GetX(), pCreature->GetY()))
							{				
								// Fit the picture's coordinates to the current screen's coordinates
								point.x = pCreature->GetPixelX() - m_FirstZonePixel.x;
								point.y = pCreature->GetPixelY() - m_FirstZonePixel.y;

								DrawCreatureShadow(&point, pCreature);//, DarkBits);
							}
						}
						*/

						//------------------------------------------------------
						// Ground Creature
						// Flying Creature
						//------------------------------------------------------
						int numCreature = sector.GetCreatureSize();

						if (numCreature > 0)
						{
							OBJECT_MAP::const_iterator iCreature = sector.GetCreatureIterator();

							for (int c=0; c<numCreature; c++, iCreature++)
							{
								if (iCreature==sector.GetObjectEnd() 
									|| iCreature->first > MSector::POSITION_FLYINGCREATURE_MAX)
									break;

								// underground has no shadow.
								if (iCreature->first >= MSector::POSITION_GROUNDCREATURE)
								{
									pCreature = (MCreature*)iCreature->second;

									// When it is the player or visible regardless of darkness
									if (
										(pCreature==g_pPlayer
										|| g_pPlayer->ShowInDarkness(pCreature->GetX(), pCreature->GetY())) &&
										!pCreature->IsFakeCreature()										
										)
									{				
										// Fit the picture's coordinates to the current screen's coordinates
										point.x = pCreature->GetPixelX() - m_FirstZonePixel.x;
										point.y = pCreature->GetPixelY() - m_FirstZonePixel.y;

										DrawCreatureShadow(&point, pCreature);//, DarkBits);
									}
								}							
							}
						}
					}					
				}
			}
		}
		if( m_pZone->GetFakeCreatureNumber() > 0 )
		{
			int numFakeCreature = m_pZone->GetFakeCreatureNumber();

			MZone::CREATURE_MAP::const_iterator itr = m_pZone->GetFakeCreatureBegin();

			for(int i = 0; i < numFakeCreature; i++,itr++)
			{
				if( itr->first < MSector::POSITION_GROUNDCREATURE ||
					itr->first > MSector::POSITION_FLYINGCREATURE_MAX )
					continue;

				MCreature *pCreature = itr->second;

				point.x = pCreature->GetPixelX() - m_FirstZonePixel.x;
				point.y = pCreature->GetPixelY() - m_FirstZonePixel.y;

				DrawCreatureShadow(&point, pCreature);
			}
		}

		// Fit the picture's coordinates to the current screen's coordinates
		point.x = g_pPlayer->GetPixelX() - m_FirstZonePixel.x;
		point.y = g_pPlayer->GetPixelY() - m_FirstZonePixel.y;

		DrawCreatureShadow(&point, g_pPlayer);//, DarkBits);		
	}
	//------------------------------------------------------
	// When using the character OutputMap
	//------------------------------------------------------
	else
	{
		while (iCreatureOutput != m_mapCreature.end())
		{
			MCreature* const pCreature = (MCreature*)((*iCreatureOutput).second);

			if (pCreature==g_pPlayer
				|| g_pPlayer->ShowInDarkness(pCreature->GetX(), pCreature->GetY()))
			{
				// Draw when the output viewpoint is less than sY1..
				// Fit the picture's coordinates to the current screen's coordinates
				point.x = pCreature->GetPixelX() - m_FirstZonePixel.x;
				point.y = pCreature->GetPixelY() - m_FirstZonePixel.y;

				// Decide the brightness.
				//int DarkBits = (m_pZone->GetSector(pCreature->GetX(),pCreature->GetY()).GetLight()==0)?m_DarkBits:0;

				DrawCreatureShadow(&point, pCreature);//, DarkBits);
			}

			iCreatureOutput ++;
		}
	}

	__END_PROFILE("DrawCreatureShadowAll")

	//----------------------------------------------------
	// Keep the original Locked state.
	//----------------------------------------------------
	if (bUnlockStatus)
	{	
		m_pSurface->Lock();
	}

	// For Item shadow output
	//------------------------------------------------------
	// While searching all the Sectors on the screen,
	// draw the Objects present in each Sector.
	//------------------------------------------------------
	// First row
	tilePointTemp.y = tilePoint.y;

	//----------------------------------------------------------------
	//
	// When drawing characters by searching the tiles
	//
	//----------------------------------------------------------------
	if (m_bTileSearchForCreature)
	{
		for (y=sY1; y<=sY2; y++)
		{				
			// First Sector of a row
			tilePointTemp.x = tilePoint.x;			

			for (x=sX1; x<=sX2; x++)
			{	
				const MSector& sector = m_pZone->GetSector(x,y);			

				//------------------------------------------------
				//
				//              Object output
				//
				//------------------------------------------------

				if (sector.IsExistObject())
				{				

					//----------------------------------------
					// For an Item
					//----------------------------------------					
					pItem = sector.GetItem();
					if (pItem != NULL
						&& g_pPlayer->ShowInDarkness(x, y))
					{		
						//----------------------------------------
						// For a corpse
						// Drawn after the Effect is drawn.
						//
						// 2002.1.23
						// When did the Effect drawing code go missing..
						// Anyway, the corpse-related code is removed.
						//----------------------------------------
						//if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
						{
						//	pCorpseItem = pItem;
						}
						//else
						{
							point.x = tilePointTemp.x;
							point.y = tilePointTemp.y;


							// Decide the brightness.
							//DarkBits = (sector.GetLight()==0)?m_DarkBits:0;

							// cx,cy must be corrected from the Frame information later.
							DrawItemShadow(&point, pItem);//, DarkBits);
						}
					}
				}


				//------------------------------------			
				// Move the coordinates to draw at
				//------------------------------------
				tilePointTemp.x += TILE_X;
			}


			// Next row
			tilePointTemp.y += TILE_Y;					
		}		
	}
	//----------------------------------------------------------------
	//
	// When drawing characters using the OutputCreatureMap without searching the tiles
	//
	//----------------------------------------------------------------
	else
	{	
		for (y=sY1; y<=sY2; y++)
		{
			// Inside a coffin there are no objects drawn by plain searching.
			// AddOutputCreatureAll() sets m_bTileSearchForCreature to false,
			// so... the OutputCreatureMap can be used.
			if (!bPlayerInCasket)	// [new skill 3]
			{			
				// First Sector of a row
				tilePointTemp.x = tilePoint.x;			

				for (x=sX1; x<=sX2; x++)
				{	
					const MSector& sector = m_pZone->GetSector(x,y);

					//------------------------------------------------
					//
					//              Object output
					//
					//------------------------------------------------
					//MItem* pCorpseItem = NULL;

					if (sector.IsExistObject())
					{				
						//----------------------------------------
						// For an Item
						//----------------------------------------					
						pItem = sector.GetItem();
						if (pItem != NULL
							&& g_pPlayer->ShowInDarkness(x, y))
						{		
							//----------------------------------------
							// For a corpse
							// Drawn after the Effect is drawn.
							//
							// 2002.1.23
							// When did the Effect drawing code go missing..
							// Anyway, the corpse-related code is removed.
							//----------------------------------------
							//if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
							{
							//	pCorpseItem = pItem;
							}
							//else
							{
								point.x = tilePointTemp.x;
								point.y = tilePointTemp.y;

								// cx,cy must be corrected from the Frame information later.
								DrawItemShadow(&point, pItem);//, DarkBits);
							}
						}
					}


					//------------------------------------			
					// Move the coordinates to draw at
					//------------------------------------
					tilePointTemp.x += TILE_X;
				}
			}	// bPlayerInCasket

			// Next row
			tilePointTemp.y += TILE_Y;
		}
	}


	//------------------------------------------------------
	//
	//				Actual sprite output
	//
	//------------------------------------------------------
	//------------------------------------------------------
	// Creature Iterator that is the output target
	//------------------------------------------------------
	iCreatureOutput = m_mapCreature.begin();

	//------------------------------------------------------
	// For output viewpoints less than sY1,
	// draw the Creatures.
	// m_mapCreature is sorted with the Viewpoint as the key.
	//------------------------------------------------------
	while (iCreatureOutput != m_mapCreature.end())
	{
		MCreature* const pCreature = (MCreature*)((*iCreatureOutput).second);

		// 2004, 04, 24 sobeit add start - when ghost
		if(pCreature->GetCreatureType() == CREATURETYPE_GHOST)
		{
			iCreatureOutput ++;
			continue;
		}
		// 2004, 04, 24 sobeit add end - when ghost

		// Draw when the output viewpoint is less than sY1..
		if (pCreature->GetY() < sY1)
		{
			// When it is the player or visible regardless of darkness
			if (pCreature==g_pPlayer
				|| g_pPlayer->ShowInDarkness(pCreature->GetX(), pCreature->GetY()))
			{					
				// Fit the picture's coordinates to the current screen's coordinates
				point.x = pCreature->GetPixelX() - m_FirstZonePixel.x;
				point.y = pCreature->GetPixelY() - m_FirstZonePixel.y;

				// Decide the brightness.
				//int DarkBits = (m_pZone->GetSector(pCreature->GetX(),pCreature->GetY()).GetLight()==0)?m_DarkBits:0;

				DrawCreature(&point, pCreature);//, DarkBits);
			}
		}
		// Otherwise.. it is drawn when drawing after a row's output ends.
		else
		{
			break;
		}

		iCreatureOutput ++;
	}

	//------------------------------------------------------
	// While searching all the Sectors on the screen,
	// draw the Objects present in each Sector.
	//------------------------------------------------------
	// First row
	tilePointTemp.y = tilePoint.y;

	//----------------------------------------------------------------
	//
	// When drawing characters by searching the tiles
	//
	//----------------------------------------------------------------
	if (m_bTileSearchForCreature)
	{
		for (y=sY1; y<=sY2; y++)
		{				
			// First Sector of a row
			tilePointTemp.x = tilePoint.x;			

			for (x=sX1; x<=sX2; x++)
			{	
				const MSector& sector = m_pZone->GetSector(x,y);			

				//------------------------------------------------------
				// Rectangle at the server block coordinates
				//------------------------------------------------------
				#ifdef OUTPUT_DEBUG
					if (g_pSDLInput->KeyDown(DIK_LCONTROL) && g_pSDLInput->KeyDown(DIK_V))
					{		
						if (sector.IsBlockServerGround())
						{
							m_pSurface->Unlock();
							RECT rect2 = { tilePointTemp.x, tilePointTemp.y, tilePointTemp.x + TILE_X, tilePointTemp.y + TILE_Y };
							m_pSurface->DrawRect(&rect2, CSDLGraphics::Color(31,20,20));
							m_pSurface->Lock();
						}

						if (sector.IsExistAnyCreature())
						{
							m_pSurface->Unlock();
							RECT rect2 = { tilePointTemp.x+2, tilePointTemp.y+2, tilePointTemp.x + TILE_X - 2, tilePointTemp.y + TILE_Y - 2 };
							m_pSurface->DrawRect(&rect2, CSDLGraphics::Color(20,20,31));
							m_pSurface->Lock();
						}
					}
				#endif

				//------------------------------------------------------
				// Mark "X" on the ViewSector? of a specific ImageObject
				//------------------------------------------------------
				#if defined(OUTPUT_DEBUG) && defined(_DEBUG)
					if (g_pSDLInput->KeyDown(DIK_A) && 
						(g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL)))
					{
						if (sector.IsExistImageObject())
						{
							OBJECT_MAP::const_iterator iImageObject = sector.GetImageObjectIterator();

							// Search all the ImageObjects in the Sector.
							for (i=0; i<sector.GetImageObjectSize(); i++)
							{
								MImageObject* const pImageObject = (MImageObject* const)((*iImageObject).second);

								if (pImageObject->GetObjectType()==MObject::TYPE_IMAGEOBJECT
									&& pImageObject->GetImageObjectID()==g_ShowImageObjectID)
								{
									m_pSurface->Unlock();
									// m_pSurface->GDI_Text(tilePointTemp.x, 
									// 						tilePointTemp.y, 
									// 						"X", 														
									// 						0xFFFFFF);	
									TextSystem::TextService::RenderText(g_pLast, tilePointTemp.x, tilePointTemp.y, "X");
									m_pSurface->Lock();
								}

								iImageObject++;
							}
						}
					}
				#endif				

				//------------------------------------------------
				// Draw the light intensity.
				//------------------------------------------------

				//------------------------------------------------
				//
				//              Object output
				//
				//------------------------------------------------
				//MItem* pCorpseItem = NULL;

				if (sector.IsExistObject())
				{				
					//----------------------------------------
					// test code : mark Object position
					//----------------------------------------
					//
					//----------------------------------------

					//----------------------------------------
					// For an Item
					//----------------------------------------					
					pItem = sector.GetItem();
					if (pItem != NULL
						&& g_pPlayer->ShowInDarkness(x, y))
					{		
						//----------------------------------------
						// For a corpse
						// Drawn after the Effect is drawn.
						//
						// 2002.1.23
						// When did the Effect drawing code go missing..
						// Anyway, the corpse-related code is removed.
						//----------------------------------------
						//if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
						{
						//	pCorpseItem = pItem;
						}
						//else
						{
							point.x = tilePointTemp.x;
							point.y = tilePointTemp.y;

							/*
							// wobble wobble~~
							int size = g_CurrentFrame & 0x00000007;

							int x0 = point.x + 24;
							int y0 = point.y + 24;
							int size4 = size << 4;
							int size2 = size << 3;
							RECT rect = 
							{ 
								x0 - size4,
								y0 - size2,
								x0 + size4,
								y0 + size2
							};
							m_pSurface->ChangeBrightnessBit( &rect, g_CurrentFrame );
							*/

							// Decide the brightness.
							//DarkBits = (sector.GetLight()==0)?m_DarkBits:0;

							// cx,cy must be corrected from the Frame information later.
							DrawItem(&point, pItem);//, DarkBits);
						}
					}
				}

				//------------------------------------------------
				//
				//              Effect output
				//
				//------------------------------------------------
	//
	//				DrawEffect(&point, sector.GetEffectIterator(), sector.GetEffectSize());
	//			}

				//------------------------------------------------
				// Corpse
				//------------------------------------------------
				// Hmm!! A shortcut of dirty code.. hmm.. I don't know either
				//------------------------------------------------
				/*
				if (pCorpseItem)
				{
					point.x = tilePointTemp.x;
					point.y = tilePointTemp.y;

					// Decide the brightness.
					//DarkBits = (sector.GetLight()==0)?m_DarkBits:0;

					// cx,cy must be corrected from the Frame information later.
					DrawItem(&point, pItem);//, DarkBits);
				}
				*/

				//------------------------------------			
				// Move the coordinates to draw at
				//------------------------------------
				tilePointTemp.x += TILE_X;
			}

			//-----------------------------------------------------
			// Every time a row ends,
			// draw the ImageObjects of that row that must be drawn.
			//-----------------------------------------------------	
			while (bDrawBackGround && iImageObjectOutput != m_mapImageObject.end())
			{
				MImageObject* const pImageObject = (MImageObject*)((*iImageObjectOutput).second);

				// Draw when the output viewpoint equals y
				if (pImageObject->GetViewpoint() <= y)
				{
					if(pImageObject->GetViewpoint() != 0)
					{
						// Fit the picture's coordinates to the current screen's coordinates
						point.x = pImageObject->GetPixelX() - m_FirstZonePixel.x;
						point.y = pImageObject->GetPixelY() - m_FirstZonePixel.y;				

						DrawImageObject(&point, pImageObject);

					}
				}
				// Otherwise.. it is another row.
				else
				{				
					break;
				}

				iImageObjectOutput ++;
			}

			//------------------------------------------------------
			// Every time a row ends,
			// draw the Creatures of that row that must be drawn.
			//------------------------------------------------------
			while (iCreatureOutput != m_mapCreature.end())
			{
				MCreature* const pCreature = (MCreature*)((*iCreatureOutput).second);

				// Draw when the output viewpoint is less than sY1..
				if (pCreature->GetY() <= y)
				{
					// 2004, 04, 24 sobeit add start - when ghost
					if(pCreature->GetCreatureType() == CREATURETYPE_GHOST)
					{
						iCreatureOutput ++;
						continue;
					}
					// 2004, 04, 24 sobeit add end - when ghost

					// When it is the player or visible regardless of darkness
					if (pCreature==g_pPlayer
						|| g_pPlayer->ShowInDarkness(pCreature->GetX(), pCreature->GetY()))
					{				
						// Fit the picture's coordinates to the current screen's coordinates
						point.x = pCreature->GetPixelX() - m_FirstZonePixel.x;
						point.y = pCreature->GetPixelY() - m_FirstZonePixel.y;

						// Decide the brightness.
						//int DarkBits = (m_pZone->GetSector(pCreature->GetX(),pCreature->GetY()).GetLight()==0)?m_DarkBits:0;

						DrawCreature(&point, pCreature);//, DarkBits);
					}
				}
				// Otherwise.. it is drawn when drawing after a row's output ends.
				else
				{
					break;
				}

				iCreatureOutput ++;
			}

			//------------------------------------------------------
			// Because of the output order.. the character is checked here.
			// Sector Effect output
			//------------------------------------------------------
			// because it must be drawn later than the character...
			//------------------------------------------------------
			//------------------------------------------------------
			// The Player does not belong to a Tile, so it is checked separately.
			//------------------------------------------------------
			if (g_pPlayer->GetY() == y)
			{
				// Fit the picture's coordinates to the current screen's coordinates
				point.x = g_pPlayer->GetPixelX() - m_FirstZonePixel.x;
				point.y = g_pPlayer->GetPixelY() - m_FirstZonePixel.y;

				// Decide the brightness.
				//int DarkBits = (m_pZone->GetSector(pCreature->GetX(),pCreature->GetY()).GetLight()==0)?m_DarkBits:0;

				DrawCreature(&point, g_pPlayer);//, DarkBits);
			}

			tilePointTemp.x = tilePoint.x;
			for (x=sX1; x<=sX2; x++)
			{	
				int darknessCount = g_pPlayer->GetDarknessCount();

				// When visible regardless of darkness. effect is +2
				if (darknessCount < 0
					|| max(abs(g_pPlayer->GetX()-x), abs(g_pPlayer->GetY()-y)) <= darknessCount+2)
				//g_pPlayer->ShowInDarkness(x, y))
				{			
					const MSector& sector = m_pZone->GetSector(x,y);

					if (sector.IsExistObject())
					{							
						//------------------------------------------------------
						// Underground Creature
						// Ground Creature
						// Flying Creature
						//------------------------------------------------------
						int numCreature = sector.GetCreatureSize();

						if (numCreature > 0)
						{
							OBJECT_MAP::const_iterator iCreature = sector.GetCreatureIterator();

							for (int c=0; c<numCreature; c++, iCreature++)
							{
								if (iCreature==sector.GetObjectEnd() 
									|| iCreature->first > MSector::POSITION_FLYINGCREATURE_MAX)
									break;

								MCreature* pCreature = (MCreature*)iCreature->second;

								// 2004, 04, 24 sobeit add start - when ghost
								if(pCreature->GetCreatureType() == CREATURETYPE_GHOST)
									continue;
								// 2004, 04, 24 sobeit add end - when ghost

								// When it is the player or visible regardless of darkness
								if (pCreature==g_pPlayer
									|| g_pPlayer->ShowInDarkness(pCreature->GetX(), pCreature->GetY()))
								{				
									// Fit the picture's coordinates to the current screen's coordinates
									point.x = pCreature->GetPixelX() - m_FirstZonePixel.x;
									point.y = pCreature->GetPixelY() - m_FirstZonePixel.y;

									DrawCreature(&point, pCreature);//, DarkBits);
								}
							}
						}
					}

					//------------------------------------------------
					//
					//              Effect output
					//
					//------------------------------------------------
					if (sector.IsExistEffect())
					{				
						point = tilePointTemp;				

	 					DrawEffect(&point, sector.GetEffectIterator(), sector.GetEffectSize());
					}
				}

				//------------------------------------			
				// Move the coordinates to draw at
				//------------------------------------
				tilePointTemp.x += TILE_X;
			}

			// Next row
			tilePointTemp.y += TILE_Y;					
		}		
	}
	//----------------------------------------------------------------
	//
	// When drawing characters using the OutputCreatureMap without searching the tiles
	//
	//----------------------------------------------------------------
	else
	{	
		for (y=sY1; y<=sY2; y++)
		{
			// Inside a coffin there are no objects drawn by plain searching.
			// AddOutputCreatureAll() sets m_bTileSearchForCreature to false,
			// so... the OutputCreatureMap can be used.
			if (!bPlayerInCasket)	// [new skill 3]
			{			
				// First Sector of a row
				tilePointTemp.x = tilePoint.x;			

				for (x=sX1; x<=sX2; x++)
				{	
					const MSector& sector = m_pZone->GetSector(x,y);

					//------------------------------------------------------
					// Rectangle at the server block coordinates
					//------------------------------------------------------
					#ifdef OUTPUT_DEBUG
						if (g_pSDLInput->KeyDown(DIK_LCONTROL) && g_pSDLInput->KeyDown(DIK_V))
						{		
							if (sector.IsBlockServerGround())
							{
								m_pSurface->Unlock();
								RECT rect2 = { tilePointTemp.x, tilePointTemp.y, tilePointTemp.x + TILE_X, tilePointTemp.y + TILE_Y };
								m_pSurface->DrawRect(&rect2, CSDLGraphics::Color(31,20,20));
								m_pSurface->Lock();
							}
						}
					#endif

					//------------------------------------------------------
					// Mark "X" on the ViewSector? of a specific ImageObject
					//------------------------------------------------------
					#if defined(OUTPUT_DEBUG) && defined(_DEBUG)
						if (g_pSDLInput->KeyDown(DIK_A) && 
							(g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL)))
						{
							if (sector.IsExistImageObject())
							{
								OBJECT_MAP::const_iterator iImageObject = sector.GetImageObjectIterator();

								// Search all the ImageObjects in the Sector.
								for (i=0; i<sector.GetImageObjectSize(); i++)
								{
									MImageObject* const pImageObject = (MImageObject* const)((*iImageObject).second);

									if (pImageObject->GetObjectType()==MObject::TYPE_IMAGEOBJECT
										&& pImageObject->GetImageObjectID()==g_ShowImageObjectID)
									{
										m_pSurface->Unlock();
										// m_pSurface->GDI_Text(tilePointTemp.x, 
										// 						tilePointTemp.y, 
										// 						"X", 														
										// 						0xFFFFFF);	
										TextSystem::TextService::RenderText(g_pLast, tilePointTemp.x, tilePointTemp.y, "X");
										m_pSurface->Lock();
									}

									iImageObject++;
								}
							}
						}
					#endif				

					//------------------------------------------------
					// Draw the light intensity.
					//------------------------------------------------

					//------------------------------------------------
					//
					//              Object output
					//
					//------------------------------------------------
					//MItem* pCorpseItem = NULL;

					if (sector.IsExistObject())
					{				
						//----------------------------------------
						// test code : mark Object position
						//----------------------------------------
						//
						//----------------------------------------

						//----------------------------------------
						// For an Item
						//----------------------------------------					
						pItem = sector.GetItem();
						if (pItem != NULL
							&& g_pPlayer->ShowInDarkness(x, y))
						{		
							//----------------------------------------
							// For a corpse
							// Drawn after the Effect is drawn.
							//
							// 2002.1.23
							// When did the Effect drawing code go missing..
							// Anyway, the corpse-related code is removed.
							//----------------------------------------
							//if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
							{
							//	pCorpseItem = pItem;
							}
							//else
							{
								point.x = tilePointTemp.x;
								point.y = tilePointTemp.y;

								/*
								// wobble wobble~~
								int size = g_CurrentFrame & 0x00000007;

								int x0 = point.x + 24;
								int y0 = point.y + 24;
								int size4 = size << 4;
								int size2 = size << 3;
								RECT rect = 
								{ 
									x0 - size4,
									y0 - size2,
									x0 + size4,
									y0 + size2
								};
								m_pSurface->ChangeBrightnessBit( &rect, g_CurrentFrame );
								*/

								// Decide the brightness.
								//DarkBits = (sector.GetLight()==0)?m_DarkBits:0;

								// cx,cy must be corrected from the Frame information later.
								DrawItem(&point, pItem);//, DarkBits);
							}
						}
					}

					//------------------------------------------------
					//
					//              Effect output
					//
					//------------------------------------------------
		//
		//				DrawEffect(&point, sector.GetEffectIterator(), sector.GetEffectSize());
		//			}

					//------------------------------------------------
					// Corpse
					//------------------------------------------------
					// Hmm!! A shortcut of dirty code.. hmm.. I don't know either
					//------------------------------------------------
					/*
					if (pCorpseItem)
					{
						point.x = tilePointTemp.x;
						point.y = tilePointTemp.y;

						// Decide the brightness.
						//DarkBits = (sector.GetLight()==0)?m_DarkBits:0;

						// cx,cy must be corrected from the Frame information later.
						DrawItem(&point, pItem);//, DarkBits);
					}
					*/

					//------------------------------------			
					// Move the coordinates to draw at
					//------------------------------------
					tilePointTemp.x += TILE_X;
				}
			}	// bPlayerInCasket

			//-----------------------------------------------------
			// Every time a row ends,
			// draw the ImageObjects of that row that must be drawn.
			//-----------------------------------------------------	
			while (bDrawBackGround && iImageObjectOutput != m_mapImageObject.end())
			{
				MImageObject* const pImageObject = (MImageObject*)((*iImageObjectOutput).second);

				// Draw when the output viewpoint equals y
				if (pImageObject->GetViewpoint() <= y)
				{
					if(pImageObject->GetViewpoint() != 0)
					{
						// Fit the picture's coordinates to the current screen's coordinates
						point.x = pImageObject->GetPixelX() - m_FirstZonePixel.x;
						point.y = pImageObject->GetPixelY() - m_FirstZonePixel.y;				

						DrawImageObject(&point, pImageObject);

					}
				}
				// Otherwise.. it is another row.
				else
				{				
					break;
				}

				iImageObjectOutput ++;
			}

			//------------------------------------------------------
			// Every time a row ends,
			// draw the Creatures of that row that must be drawn.
			//------------------------------------------------------
			while (iCreatureOutput != m_mapCreature.end())
			{
				MCreature* const pCreature = (MCreature*)((*iCreatureOutput).second);

				// 2004, 04, 24 sobeit add start - when ghost
				if(pCreature->GetCreatureType() == CREATURETYPE_GHOST)
				{
					iCreatureOutput ++;
					continue;
				}
				// 2004, 04, 24 sobeit end start - when ghost

				// Draw when the output viewpoint is less than sY1..
				if (pCreature->GetY() <= y)
				{
					// When it is the player or visible regardless of darkness
					if (pCreature==g_pPlayer
						|| g_pPlayer->ShowInDarkness(pCreature->GetX(), pCreature->GetY()))
					{				
						// Fit the picture's coordinates to the current screen's coordinates
						point.x = pCreature->GetPixelX() - m_FirstZonePixel.x;
						point.y = pCreature->GetPixelY() - m_FirstZonePixel.y;

						// Decide the brightness.
						//int DarkBits = (m_pZone->GetSector(pCreature->GetX(),pCreature->GetY()).GetLight()==0)?m_DarkBits:0;

						DrawCreature(&point, pCreature);//, DarkBits);
					}
				}
				// Otherwise.. it is drawn when drawing after a row's output ends.
				else
				{
					break;
				}

				iCreatureOutput ++;
			}

			//------------------------------------------------------
			// Sector Effect output
			//------------------------------------------------------
			// because it must be drawn later than the character...
			//------------------------------------------------------
			if (!bPlayerInCasket)	// [new skill 3] Effects are not drawn while inside a coffin.
			{			
				tilePointTemp.x = tilePoint.x;
				for (x=sX1; x<=sX2; x++)
				{	
					int darknessCount = g_pPlayer->GetDarknessCount();

					// When visible regardless of darkness. effect is +2
					if (darknessCount < 0
						|| max(abs(g_pPlayer->GetX()-x), abs(g_pPlayer->GetY()-y)) <= darknessCount+2)
					//g_pPlayer->ShowInDarkness(x, y))
					{			
						const MSector& sector = m_pZone->GetSector(x,y);

						//------------------------------------------------
						//
						//              Effect output
						//
						//------------------------------------------------
						if (sector.IsExistEffect())
						{				
							point = tilePointTemp;				

	 						DrawEffect(&point, sector.GetEffectIterator(), sector.GetEffectSize());
						}
					}

					//------------------------------------			
					// Move the coordinates to draw at
					//------------------------------------
					tilePointTemp.x += TILE_X;
				}
			}

			// Next row
			tilePointTemp.y += TILE_Y;
		}
	}


	//------------------------------------------------------
	// For output viewpoints greater than sY2,
	// draw the Creatures.
	// m_mapCreature is sorted with the Viewpoint as the key.
	//------------------------------------------------------
	while (iCreatureOutput != m_mapCreature.end())
	{
		MCreature* const pCreature = (MCreature*)((*iCreatureOutput).second);

		// 2004, 04, 24 sobeit add start - when ghost
		if(pCreature->GetCreatureType() == CREATURETYPE_GHOST)
		{
			iCreatureOutput ++;
			continue;
		}
		// 2004, 04, 24 sobeit add end - when ghost

		// When it is the player or visible regardless of darkness
		if (pCreature==g_pPlayer
			|| g_pPlayer->ShowInDarkness(pCreature->GetX(), pCreature->GetY()))
		{
			// Fit the picture's coordinates to the current screen's coordinates
			point.x = pCreature->GetPixelX() - m_FirstZonePixel.x;
			point.y = pCreature->GetPixelY() - m_FirstZonePixel.y;

			// Decide the brightness.
			//int DarkBits = (m_pZone->GetSector(pCreature->GetX(),pCreature->GetY()).GetLight()==0)?m_DarkBits:0;

			DrawCreature(&point, pCreature);//, DarkBits);		
		}

		iCreatureOutput ++;
	}


	//------------------------------------------------------
	// For output viewpoints greater than sY2,
	// draw the ImageObjects.
	// m_mapImageObject is sorted with the Viewpoint as the key.
	//------------------------------------------------------
	while (bDrawBackGround && iImageObjectOutput != m_mapImageObject.end())
	{
		MImageObject* const pImageObject = (MImageObject*)((*iImageObjectOutput).second);

		// Fit the picture's coordinates to the current screen's coordinates
		point.x = pImageObject->GetPixelX() - m_FirstZonePixel.x;
		point.y = pImageObject->GetPixelY() - m_FirstZonePixel.y;

		DrawImageObject(&point, pImageObject);

		iImageObjectOutput ++;
	}

	//------------------------------------------------
	// Draw once more only the outline of what the mouse selected.
	//------------------------------------------------
	if (m_SOM.IsInit())
	{
		#ifdef OUTPUT_DEBUG_DRAW_PROCESS
			DEBUG_ADD( "m_SOM.Clear()" );
		#endif
		//m_pSurface->BltSpriteOutlineOnly( &m_SOM,  m_SOMOutlineColor );

		// Remove
		m_SOM.Clear();
	}

	//----------------------------------------------------------------
	// Unless the player is inside a coffin.. [new skill 3]
	//----------------------------------------------------------------
	if (!bPlayerInCasket && bDrawBackGround)
	{			
		//----------------------------------------------------------------
		//
		//          Weather - weather effect output
		//
		//----------------------------------------------------------------

		#ifdef OUTPUT_DEBUG_DRAW_PROCESS
			DEBUG_ADD( "Start DrawWeather" );
		#endif

		__BEGIN_PROFILE("DrawWeather")

		if (g_pWeather->IsActive())
		{
			// Horizontal coordinate correction..
			// Compare with the current Player's coordinates.
			//int gapX = ((g_pPlayer->GetX() - g_pWeather->GetStartX()) * TILE_X + g_pPlayer->GetSX()) % SURFACE_WIDTH;
			//int gapY = ((g_pPlayer->GetY() - g_pWeather->GetStartY()) * TILE_Y + g_pPlayer->GetSY()) % SURFACE_HEIGHT;
			int gapX = (m_FirstZonePixel.x - g_pWeather->GetStartX()) % g_GameRect.right;
			int gapY = (m_FirstZonePixel.y - g_pWeather->GetStartY()) % g_GameRect.bottom;

			for (i=0; i<g_pWeather->GetSize(); i++)
			{
				const MAP_EFFECT& MapEffect = (*g_pWeather)[i];

				// If it has not stopped...
				if (MapEffect.IsActive())
				{
					point.x = MapEffect.GetX() - gapX;
					point.y = MapEffect.GetY() - gapY;

					if (point.x < 0) point.x += g_GameRect.right;
					else if (point.x > g_GameRect.right) point.x -= g_GameRect.right;

					if (point.y < 0) point.y += g_GameRect.bottom;
					else if (point.y > g_GameRect.bottom) point.y -= g_GameRect.bottom;

					if(g_pWeather->GetWeatherType() == MWeather::WEATHER_SPOT)
					{
						CSpriteSurface::SetEffect(CSpriteSurface::EFFECT_SCREEN_ALPHA);
						CSpriteSurface::s_Value1 = MapEffect.GetCount()*32/MapEffect.GetMaxCount();
						m_pSurface->BltSpriteEffect(&point, &m_WeatherSPK[ MapEffect.GetSpriteID() ]);
					}
//						m_pSurface->BltSpriteAlpha(&point, &m_WeatherSPK[ MapEffect.GetSpriteID() ], MapEffect.GetCount()*10/60);
					else
						m_pSurface->BltSprite(&point, &m_WeatherSPK[ MapEffect.GetSpriteID() ]);
					//m_pSurface->BltSpriteAlpha(&point, &m_WeatherSPK[ MapEffect.GetSpriteID() ], 24);

					/*
					#ifdef OUTPUT_DEBUG
						if (MapEffect.GetSpriteID()>20)
						{
							DEBUG_ADD_FORMAT("Weather[%d] : spriteID=%d", i, MapEffect.GetSpriteID());
						}
					#endif
					*/
				}
			}
		}

		__END_PROFILE("DrawWeather")

		#ifdef OUTPUT_DEBUG_DRAW_PROCESS
			DEBUG_ADD( "End DrawWeather" );
		#endif

#ifdef __METROTECH_TEST__
		if(!g_bLight)
		{
#endif
		if((g_pZone->GetID() != 1006 || !g_pPlayer->IsInSafeSector()) && !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST ))
		{
			//----------------------------------------------------------------
			//
			// Draw LightFilter to Screen
			//
			//----------------------------------------------------------------
			__BEGIN_PROFILE("DrawLightFilter")

				//----------------------------------------------------------------
				// Set the player's light coordinates
				//----------------------------------------------------------------
				int pX = g_pPlayer->GetPixelX() - m_FirstZonePixel.x + 24;
			int pY = g_pPlayer->GetPixelY() - m_FirstZonePixel.y - TILE_Y;
			// add by Sonic 2006.9.30 fix the character sight focus
			if(g_MyFull)
			{
				pX-=110;
				pY-=75;
			}
			// End by Sonic 2006.9.30
			//MessageBox(0,sh,"",MB_OK);
			//----------------------------------------------------------------
			// 3D 
			//----------------------------------------------------------------
			g_pPlayer->CalculateLightSight();
			int playerLight = g_pPlayer->GetLightSight();// + g_pPlayer->GetItemLightSight() + addLight);

			if (g_pPlayer->IsInDarkness())
			{
				playerLight = min(playerLight, g_pPlayer->GetDarknessCount());
			}
//			if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_BLINDNESS))
//				playerLight = 1;

		//	DEBUG_ADD_FORMAT("######## Sight  (%d)", playerLight);
			if (s_bUse3DLight)
			{
				AddLightFilter3D( pX,
					pY - (g_pPlayer->IsFlyingCreature()? 72:0 ),	//g_pPlayer->GetZ(), 
					playerLight, 
					false,	// screenPixel coordinates
					true);	// light that must always be drawn

				// motorcycle light
				ADD_MOTORCYCLE_LIGHT_XY_3D( g_pPlayer, pX, pY, true );		

				//---------------------------------------
				// UNLOCK
				//---------------------------------------
				m_pSurface->Unlock();

				//---------------------------------------
				// 3D sight handling output
				//---------------------------------------
				point.x = 0;
				point.y = 0;
				rect.left = 0;
				rect.top = 0;
				rect.right = g_GameRect.right;
				rect.bottom = g_GameRect.bottom;
				g_pBack->BltNoColorkey( &point, g_pLast, &rect );		

				g_pTopView->DrawLightBuffer3D();

				g_pLast->BltNoColorkey( &point, g_pBack, &rect );		

//				g_pLast->FillSurface(0x001F);

//				DrawLightBuffer3D();

				// Save the Surface information.
				//SetSurfaceInfo(&m_SurfaceInfo, m_pSurface->GetDDSD());
			}
			//----------------------------------------------------------------
			// 2D
			//----------------------------------------------------------------
			else
			{
				// For a bat, sight +3
				AddLightFilter2D( pX, 
					pY - g_pPlayer->GetZ(), 
					playerLight,  
					false,	// screenPixel coordinates
					true);	// light that must always be drawn

				// motorcycle light
				ADD_MOTORCYCLE_LIGHT_XY_2D( g_pPlayer, pX, pY, true );	

				DrawLightBuffer2D();

				//---------------------------------------
				// UNLOCK
				//---------------------------------------
				m_pSurface->Unlock();
			}

			__END_PROFILE("DrawLightFilter")
#ifdef __METROTECH_TEST__
		}
#endif
		}
		m_pSurface->Unlock();


		//----------------------------------------------------------------	
		// It is in the Unlock state..
		//----------------------------------------------------------------		

		//----------------------------------------------------------------
		// If the character is selected with the Mouse.. draw bright text
		//----------------------------------------------------------------			
		if(IsRequestInfo())
			DrawCreatureMyName();
		if (m_pSelectedCreature != NULL)
		{
			__BEGIN_PROFILE("DrawSelectedCreature")

			DrawCreatureName( m_pSelectedCreature );

			__END_PROFILE("DrawSelectedCreature")
		}
		else
		{
			m_bDrawRequest = false;
		}


		//----------------------------------------------------------------
		// Have the Item names drawn.
		//----------------------------------------------------------------
		if (m_bDrawItemNameList && !g_pPlayer->IsInDarkness())
		{
			__BEGIN_PROFILE("DrawItemNameList")

			// called in the unlock state..
			DrawItemNameList();		

			__END_PROFILE("DrawItemNameList")
		}
	} // bPlayerInCasket
	else 
	{
		// [new skill 3]
		m_pSurface->Unlock();
	}

	//----------------------------------------------------------------	
	// Lock
	//----------------------------------------------------------------	
	//m_pSurface->Lock();

	//----------------------------------------------------------------
	// Draw the text that appears on the screen.
	//----------------------------------------------------------------
	__BEGIN_PROFILE("DrawTextList")

	DrawTextList();

	__END_PROFILE("DrawTextList")

	//-------------------------------------------------
	// Clear everything remembered in the string to output.
	//-------------------------------------------------
	ClearTextList();

	//-------------------------------------------------
	// guild mark output test
	//-------------------------------------------------
//	#ifdef _DEBUG
//
//
//			//-------------------------------------------------
//			// Check whether it is loaded.
//			//-------------------------------------------------
//			CSprite* pSprite = g_pGuildMarkManager->GetGuildMark(guildID);
//
//			if (pSprite==NULL)
//			{		
//				//-------------------------------------------------
//				// Check whether it is in the file.
//				//-------------------------------------------------
//				g_pGuildMarkManager->LoadGuildMark(guildID);
//
//				//-------------------------------------------------
//				// Check again whether it was loaded from the file
//				//-------------------------------------------------
//				pSprite = g_pGuildMarkManager->GetGuildMark(guildID);
//
//				//-------------------------------------------------
//				// When it is not in the file either..
//				// Fetch from the guildMark management server?.
//				//-------------------------------------------------
//				CSprite* pSpriteSmall = g_pGuildMarkManager->GetGuildMarkSmall(guildID);
//
//				m_pSurface->Lock();
//
//				POINT point = { 100, 100 };
//
//				m_pSurface->BltSprite(&point, pSprite);
//
//


	//----------------------------------------------------------------
	// Light output
	//----------------------------------------------------------------

	/*
	point.x = 210;
	point.y = 200;
	rect.left = 0;
	rect.top = 0;
	rect.right = 400;
	rect.bottom = 200;
	m_pSurface->CopyTransAlpha(&point, m_pBuffer, &rect, 8);
	*/


	//----------------------------------------------------------------
	// Object output
	//----------------------------------------------------------------
	// Must be sorted by Y coordinate and drawn.
	//----------------------------------------------------------------
	// Using a priority queue instead of a list seems better! Automatic sort~~
	// But! The list has an order when things are added... so it seems not to matter!
	//----------------------------------------------------------------




	//----------------------------------------------------------------
	// Effect Test
	//----------------------------------------------------------------
	// sword
	/*
	static int swordFrameID = 5;
	static int swordD = 0;
	static int swordFrame = 0;

	point.x = 380;
	point.y = 260;;
	m_pSurface->BltAlphaSprite(&point, &m_EffectAlphaSPK[ m_EffectAlphaFPK[swordFrameID][swordD][swordFrame].GetSpriteID() ]);

	if (++swordFrame == 12) swordFrame = 0;

	// Aura
	static int auraFrameID = 3;
	static int auraD = 0;
	static int auraFrame = 0;

	point.x = 320;
	point.y = 220;;	
	m_pSurface->BltAlphaSprite(&point, &m_EffectAlphaSPK[ m_EffectAlphaFPK[auraFrameID][auraD][auraFrame].GetSpriteID() ]);

	if (++auraFrame == 5) auraFrame = 0;
	*/

	//----------------------------------------------------------------
	//
	// TEST CODE  - SpriteOutlineManager Test
	//
	//----------------------------------------------------------------
	/*
	static	CSpriteOutlineManager	SOM;
	static int f = 0;

	if (f==0)
	{
		SOM.Add( 100,70, &m_ImageObjectSPK[38] );
		SOM.Add( 80,100, &m_ImageObjectSPK[38] );
		SOM.Add( 130,100, &m_ImageObjectSPK[39] );
		SOM.Add( 100,130, &m_ImageObjectSPK[39] );

		SOM.Generate();

		f=1;
	}

	m_pSurface->BltSpriteOutline( &SOM, 0xFFFF );
	*/

	/*
	// Shadow Test
	if (g_pSDLInput->KeyDown(DIK_SPACE))
	{
		point.x = g_x;
		point.y = g_y;

		if (true)
		{
			sprite = 0;//rand()%m_pImageObjectShadowManager->GetMaxIndex();

			m_pSurface->Unlock();
			// Set EffectTexture
			//// CDirect3D::GetDevice()->SetTexture() removed (SDL2)

			CSpriteSurface* pSurface = m_pImageObjectShadowManager->GetTexture(sprite);		
			// CDirect3D::GetDevice()->SetTexture() removed (SDL2)


			// Set coordinates
			m_SpriteVertices[0].sx = point.x;	
			m_SpriteVertices[0].sy = point.y;
			//m_SpriteVertices[1].sx = point.x,m_EffectTPK[sprite].GetWidth();
			m_SpriteVertices[1].sx = point.x + m_pImageObjectShadowManager->GetWidth(sprite);
			m_SpriteVertices[1].sy = point.y;
			m_SpriteVertices[2].sx = point.x;
			//m_SpriteVertices[2].sy = point.y,m_EffectTPK[sprite].GetHeight();
			m_SpriteVertices[2].sy = point.y + m_pImageObjectShadowManager->GetHeight(sprite);
			m_SpriteVertices[3].sx = m_SpriteVertices[1].sx;
			m_SpriteVertices[3].sy = m_SpriteVertices[2].sy;		

			// Output
			// CDirect3D::GetDevice()->DrawPrimitive() removed (SDL2)


			m_pSurface->Lock();			
		}
		else
		{
			sprite = 0;//rand()%m_ImageObjectSSPK.GetSize();

			m_pSurface->BltShadowSpriteDarkness( &point, &m_ImageObjectSSPK[sprite], 1);
		}
	}	
	*/	

	//------------------------------------------------------
	// (cursor)
	// When the moving direction keeps changing --> show the cursor
	//
	//------------------------------------------------------
	/*
	if (!bPutSelectedSector && g_bLButtonDown)			
	{
		POINT cursorPoint;
		GetCursorPos(&cursorPoint);
		// And, this is the case where the Player's current position is not selected.
		point = GetSelectedSector(cursorPoint.x, cursorPoint.y);
		if (g_pPlayer->GetX()!=point.x || g_pPlayer->GetY()!=point.y)
		{
			// Show the cursor shape in the direction the player is moving
			int direction = g_pPlayer->GetDirection();

			// [ TEST CODE ]
			point.x = cursorPoint.x - 32;
			point.y = cursorPoint.y - 16;
			m_pSurface->BltSprite(&point, &m_EtcSPK[5,direction]);
		}
	}
	*/


	//---------------------------------------
	// UNLOCK
	//---------------------------------------
	//m_pSurface->Unlock();



	//---------------------------------------
	// Filter area rectangle
	//---------------------------------------

	/*
	// Show Creature Sprite
	static int s = 3;

	if (++s == 43) s=3;

	point.x = 500;
	point.y = 400;
	m_pSurface->BltSprite(&point, m_SpritePack.GetSprite( s ));
	*/

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "End DrawZone" );
	#endif
}

//----------------------------------------------------------------------
// Draw TileSurface
//----------------------------------------------------------------------
// - Centered on the Player coordinates...
//   Only draw the Tiles into m_pTileSurface.
//
// - Search the Objects on the current screen and remember them so they can be used for drawing.
//----------------------------------------------------------------------
void	
MTopView::DrawTileSurface()
{
	//-------------------------------------------------
	//
	// Information for drawing Objects
	//
	//-------------------------------------------------
	// sprite point
	POINT	tilePoint, point;
	RECT	rect;

	//---------------------------------------------------------------
	// Get the first Sector coordinates for keeping the Player at the center.
	// Zone Pixel coordinates that the screen's (0,0) represents
	//---------------------------------------------------------------
	m_FirstZonePixel = MapToPixel(g_pPlayer->GetX()+g_SECTOR_SKIP_PLAYER_LEFT, 
									g_pPlayer->GetY()+g_SECTOR_SKIP_PLAYER_UP);
	m_FirstZonePixel.x += g_pPlayer->GetSX();
	m_FirstZonePixel.y += g_pPlayer->GetSY();

	//---------------------------------------------------------------	
	// Sector drawn at the screen's (0,0)
	//---------------------------------------------------------------		
	POINT	firstSector;
	firstSector = PixelToMap(m_FirstZonePixel.x, m_FirstZonePixel.y);
	m_FirstSector = firstSector;

	//---------------------------------------------------------------
	// Sector coordinates drawn at TileSurface (0,0)
	//---------------------------------------------------------------
	firstSector.x -= TILESURFACE_SECTOR_EDGE;
	firstSector.y -= TILESURFACE_SECTOR_EDGE;

	//---------------------------------------------------------------
	// Remember the pixel coordinates in the Zone of TileSurface (0,0).
	//---------------------------------------------------------------
	m_TileSurfaceFirstSectorX		= firstSector.x;
	m_TileSurfaceFirstSectorY		= firstSector.y;

	point = MapToPixel(m_TileSurfaceFirstSectorX, m_TileSurfaceFirstSectorY);
	m_TileSurfaceFirstZonePixelX	= point.x;
	m_TileSurfaceFirstZonePixelY	= point.y;


	//----------------------------------------------------------------------
	//
	//                         Tile output
	//
	//----------------------------------------------------------------------
	//----------------------------------------------------------------------
	// The first Sector must always come at the TileSurface's (0,0).
	//----------------------------------------------------------------------
	int	sX1 = firstSector.x, 
			sX2 = firstSector.x + g_TILESURFACE_SECTOR_WIDTH,		// sX1 ~ sX2
			sY1 = firstSector.y, 
			sY2 = firstSector.y + g_TILESURFACE_SECTOR_HEIGHT;	// sY1 ~ sY2

	// Position on the Surface to draw to
	tilePoint.x = 0;
	tilePoint.y = 0;

	//------------------------------------------------------
	// Skip when outside the Zone area...
	//------------------------------------------------------					
	if (sX1 < 0) 
	{			
		// Paint the part outside the Zone black.
		rect.left = tilePoint.x;
		rect.top = 0;	

		tilePoint.x += -sX1 * TILE_X;

		rect.right = tilePoint.x;
		rect.bottom = g_TILESURFACE_HEIGHT;				 

		m_pTileSurface->FillRect(&rect, 0);

		// CRITICAL FIX: Sync m_TileSurfaceFirstSectorX with adjusted sX1
		m_TileSurfaceFirstSectorX += (-sX1);
		sX1 = 0;	
	}

	if (sX2 > m_pZone->GetWidth())
	{
		sX2 = m_pZone->GetWidth();//-1;

		// Paint the part outside the Zone black.
		rect.left = tilePoint.x + (sX2-sX1)*TILE_X;
		rect.top = 0;					
		rect.right = g_TILESURFACE_WIDTH;
		rect.bottom = g_TILESURFACE_HEIGHT;

		m_pTileSurface->FillRect(&rect, 0);
	}

	if (sY1 < 0)
	{
		// Paint the part outside the Zone black.
		rect.left = tilePoint.x;
		rect.top = tilePoint.y;	

		tilePoint.y += -sY1 * TILE_Y;

		rect.right = tilePoint.x + (sX2-sX1)*TILE_X;
		rect.bottom = tilePoint.y;
		m_pTileSurface->FillRect(&rect, 0);

		// CRITICAL FIX: Sync m_TileSurfaceFirstSectorY with adjusted sY1
		m_TileSurfaceFirstSectorY += (-sY1);
		sY1 = 0;	
	}

	if (sY2 > m_pZone->GetHeight())
	{
		// Paint the part outside the Zone black.
		rect.left = tilePoint.x;
		rect.top = tilePoint.y;					
		rect.right = tilePoint.x + (sX2-sX1)*TILE_X;
		rect.bottom = g_TILESURFACE_HEIGHT;
		m_pTileSurface->FillRect(&rect, 0);

		sY2 = m_pZone->GetHeight();//-1;
	}			

	//------------------------------------------------------
	// First paint the whole thing black.
	//------------------------------------------------------
	/*
	rect.left = 0;
	rect.top = 0;
	rect.right = TILESURFACE_WIDTH;	
	rect.bottom = TILESURFACE_HEIGHT;

	m_pTileSurface->FillRect(&rect, 0);
	*/

	//------------------------------------------------------
	// Draw each Sector
	//------------------------------------------------------	
	// First row

	DEBUG_ADD_FORMAT("[DrawTileSurface] (%d, %d) ~ (%d, %d)", sX1, sX2, sY1, sY2);

	//---------------------------------------
	// LOCK
	//---------------------------------------
	if (!m_pTileSurface->Lock()) return;

	//----------------------------------------------------------------------
	// Use TileRenderer for unified tile rendering (Phase 4 integration)
	//----------------------------------------------------------------------
	if (m_pTileRenderer != NULL && m_pTileRenderer->IsInit())
	{
		// Set the zone provider
		m_zoneTileProvider.SetZone(m_pZone);

		// Draw tiles using TileRenderer
		// Note: DrawTilesNoLock is used because surface is already locked
		m_pTileRenderer->DrawTilesNoLock(
			&m_zoneTileProvider,
			sX1, sY1,
			sX2 - sX1,
			sY2 - sY1,
			tilePoint.x,
			tilePoint.y
		);
	}
	else
	{
		printf("[DrawTileSurface] ERROR: TileRenderer not initialized! Cannot render tiles.\n");
	}

	//---------------------------------------
	// UNLOCK
	//---------------------------------------
	m_pTileSurface->Unlock();

	//----------------------------------------------------------------------
	// NOTE: Tile surface to main screen blit is handled in DrawZone()
	// via the bDrawBackGround condition. Do NOT duplicate here.
	//----------------------------------------------------------------------
}

//----------------------------------------------------------------------
// Draw PartyHP
//----------------------------------------------------------------------
void
MTopView::DrawPartyHP(POINT* pPoint, MCreature* pCreature)
{
	pPoint->x += 10;

	//-----------------------------------------------------
	// HP calculation
	//-----------------------------------------------------
	int currentHP		= pCreature->GetHP();
	int maxHP			= pCreature->GetMAX_HP();

	//-----------------------------------------------------
	// Those underground are shown as if their HP were full.
	//-----------------------------------------------------
	if (pCreature->IsUndergroundCreature())
	{
		currentHP = 1;
		maxHP = 1;
	}

	if (currentHP > maxHP)
	{
		currentHP = maxHP;
	}

	//-----------------------------------------------------
	// Calculate the pixels actually shown
	//-----------------------------------------------------
	CSprite* pHPBackSprite	= &m_EtcSPK[SPRITEID_PARTY_HP_BACKGROUND];
	CSprite* pHPSprite		= &m_EtcSPK[SPRITEID_PARTY_HP];

	int maxPixels		= pHPBackSprite->GetWidth();
	int currentPixels	= (maxHP==0)? 0 : maxPixels * currentHP / maxHP;

	//-----------------------------------------------------
	// Output
	//-----------------------------------------------------
	POINT pointTemp;

	pointTemp.x = pPoint->x;
	pointTemp.y = pPoint->y;
	m_pSurface->BltSprite(&pointTemp, pHPBackSprite);

	pointTemp.x = pPoint->x;
	pointTemp.y = pPoint->y;

	int clipRight = pointTemp.x + currentPixels;

	if (clipRight > 0)
	{
		if (clipRight < m_pSurface->GetWidth())
		{
			int oldClipRight = m_pSurface->GetClipRight();
			m_pSurface->SetClipRight( clipRight );

			m_pSurface->BltSprite(&pointTemp, pHPSprite);

			m_pSurface->SetClipRight( oldClipRight );
		}
		else
		{
			m_pSurface->BltSprite(&pointTemp, pHPSprite);
		}
	}
}


// 2004, 11, 22, sobeit add start
void
MTopView::DrawBloodBibleEffect_InGear(POINT* pPoint)
{
	int _FrameID = EFFECTSPRITETYPE_BLOODY_BIBLE_IN_GEAR;
	int _Direction = 0;
	int _Frame = (g_FrameCount % 112)>>3; 
	CEffectFrame& Frame = m_EffectAlphaFPK[_FrameID][_Direction][_Frame];
	int spriteID = Frame.GetSpriteID();
	POINT point = *pPoint;
	if (spriteID != SPRITEID_NULL)
	{
		point.x += Frame.GetCX();
		point.y += Frame.GetCY();

		DRAW_ALPHASPRITEPAL(&point, spriteID, m_EffectAlphaSPK, m_EffectAlphaPPK[_FrameID])//, m_EffectAlphaSPKI, m_EffectAlphaSPKFile)
	}
}
// 2004, 11, 22, sobeit add end
//----------------------------------------------------------------------
// Draw Chat String
//----------------------------------------------------------------------
int
	MTopView::DrawChatString(POINT* pPoint, MCreature* pCreature, COLORREF color, BYTE flag)
{

	int y2 = pPoint->y;
	// Draw slightly to the left...
	//pPoint->x -= 50;

	// 2005, 1, 20, sobeit add start
	if(g_pUserOption->DoNotShowPersnalShopMsg && pCreature->CurPernalShop())
	{
		//str = "...";
		return y2;
	}
	// 2005, 1, 20, sobeit add end
	//---------------------------------------------------------
	//
	//			Font setup
	//
	//---------------------------------------------------------	
	FONTID	font;
	//---------------------------------------------------------
	// Use a different Font for Vampire and Slayer.
	//---------------------------------------------------------
	if (pCreature->IsVampire())
	{
		//gC_font.NotUseNoPublicEng();

		font	= FONTID_VAMPIRE_CHAT;
	}
	else //if (pCreature->IsSlayer() || pCreature->IsNPC())	
	{
		font	= FONTID_SLAYER_CHAT;
	}	

	bool	bLargeFont = pCreature->HasEffectStatus( EFFECTSTATUS_LOUD );
	if( bLargeFont )
		font = FONTID_LARGE_CHAT;

	const int MinWidthChatBox = bLargeFont ? MIN_CHAT_LARGE_BOX_WIDTH : MIN_CHAT_BOX_WIDTH;
	const int ChatFontHeight = bLargeFont ? g_pClientConfig->FONT_HEIGHT + 5 : g_pClientConfig->FONT_HEIGHT ;
	//---------------------------------------------------------
	//
	//		Get the length of the longest string.
	//
	//---------------------------------------------------------
	int numString = g_pClientConfig->MAX_CHATSTRING;
	int maxWidth = 0;
	for (int i=g_pClientConfig->MAX_CHATSTRING_MINUS_1; i>=0; i--)
	{
		const char *str = pCreature->GetChatString(i);		

		//---------------------------------------------------------
		// If there is something.. it must be drawn.
		//---------------------------------------------------------
		if (str[0] != '\0')
		{
			int width = max( MinWidthChatBox, g_GetStringWidth(str, g_ClientPrintInfo[font]->hfont) );

			if (width > maxWidth)
			{
				maxWidth = width;
			}			
		}
		else
		{	
			numString = g_pClientConfig->MAX_CHATSTRING_MINUS_1 - i;

			//---------------------------------------------------------
			// When there is nothing to draw
			//---------------------------------------------------------
			if (numString==0)
			{
				return y2;
			}

			break;
		}
	}

	//---------------------------------------------------------
	//
	//			Set the output information
	//
	//---------------------------------------------------------	
	int firstY = y2 - numString * ChatFontHeight;

	int x = pPoint->x;
	int y = firstY;



	//---------------------------------------------------------
	// So that the text fits inside the screen..
	//---------------------------------------------------------
	if (x<0) x=0;

	if (m_SelectCreatureID == pCreature->GetID())
	{
		const int FontHeight = ChatFontHeight;

		if (pCreature->HasLevelName())
		{
			const int FontHeight2 = FontHeight<<1;

			if (y<FontHeight2)
			{
				firstY = FontHeight2;

				y2 = (numString+2) * FontHeight;
				y = FontHeight2;
			}
		}
		else if (y<FontHeight)
		{
			firstY = FontHeight;

			y2 = (numString+1) * FontHeight;
			y = FontHeight;		
		}
	}
	else	
	{
		if (y<0)
		{
			firstY = 0;

			y2 = numString * ChatFontHeight;		
			y=0;
		}
	}

	int x2 = x + maxWidth;//gC_font.GetStringmaxWidth( str ),

	if (x2 >= g_GameRect.right)
	{
		x -= x2-g_GameRect.right;
		x2 = g_GameRect.right;
	}

	QWORD timeBase;

	if (pCreature==m_pSelectedCreature)
	{
		timeBase = ((QWORD)(g_CurrentFrame+160) << 41);
	}
	else
	{
		if( pCreature->CurPernalShop() == 1 )
			timeBase = ((QWORD)(g_CurrentFrame+pCreature->GetPersonalShopMsgTime()) << 41);
		else		
			timeBase = ((QWORD)pCreature->GetChatTime() << 41) 
						| ((QWORD)pCreature->GetID() << 9);
	}

	//---------------------------------------------------------
	//  Draw each string...
	//---------------------------------------------------------
	int start = g_pClientConfig->MAX_CHATSTRING-numString;
	for (int i=start; i<g_pClientConfig->MAX_CHATSTRING; i++)
	{
		const char *str = pCreature->GetChatString(i);

		// If there is something.. it must be drawn.
		//if (str[0] != NULL)
		{
			//m_pSurface->GDI_Text(pointTemp.x, pointTemp.y, str, 0xFFFF);
			//gC_font.PrintStringNoConvert(&m_SurfaceInfo, str, pPoint->x, pPoint->y, color);
			DRAWTEXT_NODE*	pNode = new DRAWTEXT_NODE (
				x,
				y+3,
				str,
				color,
				font,
				flag// | FLAG_DRAWTEXT_HANGUL_JOHAP
			);	

			//---------------------------------------------------------
			// 33 was used because the first bit of the DWORD time seems unnecessary,
			// and the Creature Object ID makes each character draw in order,
			// and the output order i is added so that they are drawn in order.
			//
			// older < per character < line order
			//---------------------------------------------------------
			QWORD time = timeBase + m_pqDrawText.size();
			pNode->SetTextTime( time );

			//---------------------------------------------------------
			// black box (garbled in the original; the Chinese gloss reads: black background box - chat bubble)
			//---------------------------------------------------------
			if (i==start)
			{
				RECT rect = {
					x - 4,
					firstY - 2,
					x2 + 4,
					y2 +2
				};

				pNode->SetBox( rect, m_ColorUIBoxBg );  // gray in RGB565 format

				//---------------------------------------------------------
				// Draw an outline box around the chat
				//---------------------------------------------------------
				if (g_pUserOption->DrawChatBoxOutline)
				{
					DWORD color_degree = pCreature->GetOriChatColor();
					BYTE	r, 
							//g, 
							b = (color_degree&0xFF000000)>>24;//GetBValue(  ) >> 3;

					b = min(max(b-8, 0), 20);
					r = min(24, b+4);
					//g = b;
					if(pCreature->CurPernalShop()==1)
						pNode->SetBoxOutline( CSDLGraphics::Color(255, 0, 200) );
					else
						pNode->SetBoxOutline( CSDLGraphics::Color(r, b, b) );

				}
			}

			AddText( pNode );
		}

		// Move down one line at a time..
		y += ChatFontHeight;
	}

	return firstY;
}

//----------------------------------------------------------------------
// Draw Item : draw the Item
//----------------------------------------------------------------------
// Draw pItem at pPoint of pSurface.
//----------------------------------------------------------------------
void	
MTopView::DrawItem(POINT* pPoint, MItem* pItem)
{
	if(g_pEventManager->GetEventByFlag(EVENTFLAG_NOT_DRAW_ITEM))
		return;

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "Start DrawItem" );
	#endif

	__BEGIN_PROFILE("DrawItem")

	//------------------------------------------------
	// A Slayer cannot see inside Darkness.
	//------------------------------------------------
	if (g_pZone->GetSector(pItem->GetX(), pItem->GetY()).HasDarkness() &&
		IsAffectFromDarkness() &&
		!(
			pItem->GetItemClass() == ITEM_CLASS_CORPSE && 
			((MCorpse *)pItem)->GetCreature() != NULL &&			
			(
				(((MCorpse *)pItem)->GetCreature()->GetCreatureType() >= 526 && ((MCorpse *)pItem)->GetCreature()->GetCreatureType() <= 549) ||
				(((MCorpse *)pItem)->GetCreature()->GetCreatureType() >= 371 && ((MCorpse *)pItem)->GetCreature()->GetCreatureType() <= 376) ||
				(((MCorpse *)pItem)->GetCreature()->GetCreatureType() >= 560 && ((MCorpse *)pItem)->GetCreature()->GetCreatureType() <= 563) ||
				((MCorpse *)pItem)->GetCreature()->GetCreatureType() == 670 ||
				((MCorpse *)pItem)->GetCreature()->GetCreatureType() == 672 ||
				((MCorpse *)pItem)->GetCreature()->GetCreatureType() == 673
			)
		)
		)
	{
		#ifdef OUTPUT_DEBUG_DRAW_PROCESS
			DEBUG_ADD( "End DrawItem(D)" );
		#endif

		return;
	}

	//---------------------------------------------------------
	// For a corpse, draw the Creature.
	//---------------------------------------------------------
	if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
	{
		MCorpse* pCorpse = (MCorpse*)pItem;

		MCreature* pCreature = pCorpse->GetCreature();

		POINT orPoint = *pPoint;

		if (pCreature!=NULL)
		{
			//---------------------------------------------------------
			// A PC Vampire is shown as 'dust'.
			// 
			// In the Teen version everything is shown as 'dust'.
			//---------------------------------------------------------
//			if (//pCreature->GetCreatureType()==CREATURETYPE_VAMPIRE_MALE 
//				//|| pCreature->GetCreatureType()==CREATURETYPE_VAMPIRE_FEMALE)
//				(*g_pCreatureSpriteTable)[(*g_pCreatureTable)[pCreature->GetCreatureType()].SpriteType].IsPlayerVampireOnlySprite()
//				|| g_pUserInformation->GoreLevel == false)
//			{
//				CSprite* pSprite = &m_EtcSPK[ SPRITEID_VAMPIRE_DUST ];
//
//				POINT pointTemp;
//
//					
//
//					
//					m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );
//					
//					m_SOM.Generate();
//
			//---------------------------------------------------------
			// Dead appearance of a normal character..
			//---------------------------------------------------------
//			else
			{
				// If it is the selected one
				// Draw it as if the Creature were selected.. it's a cheat. - -;
				if (m_SelectItemID == pItem->GetID())
				{
					m_SelectCreatureID = pCreature->GetID();

//						POINT pointTemp = *pPoint;
//						DrawCreatureShadow(&pointTemp, pCreature);
//					}

					DrawCreature(pPoint, pCreature);

					m_SelectCreatureID = OBJECTID_NULL;			
				}
				else
				{
//						POINT pointTemp = *pPoint;
//						DrawCreatureShadow(&pointTemp, pCreature);
//					}

					DrawCreature(pPoint, pCreature);
				}
			}

			//------------------------------------------------
			// For a dead character the selection rectangle
			// is limited to the size of one Tile.
			//------------------------------------------------
			if(!((pCreature->GetCreatureType() >= 371 && pCreature->GetCreatureType() <= 376) || (pCreature->GetCreatureType() >= 560 && pCreature->GetCreatureType() <= 563) || (pCreature->GetCreatureType() >= 526 && pCreature->GetCreatureType() <= 549) || pCreature->GetCreatureType() == 672 || pCreature->GetCreatureType() == 673 ))
			{
				if(pCreature->GetCreatureType() == 482 || pCreature->GetCreatureType() == 650 )	// so the tree cannot be selected
				{
					RECT rect;
					rect.left	= orPoint.x;
					rect.top	= orPoint.y;
					rect.right	= orPoint.x;
					rect.bottom = orPoint.y;
					pCreature->SetScreenRect( &rect );			
				}
				else
				{
					RECT rect;
					rect.left	= orPoint.x;
					rect.top	= orPoint.y;
					rect.right	= orPoint.x + TILE_X;
					rect.bottom = orPoint.y + TILE_Y;
					pCreature->SetScreenRect( &rect );			
				}
			}
		}

		#ifdef OUTPUT_DEBUG_DRAW_PROCESS
			DEBUG_ADD( "End DrawItem" );
		#endif

		if(pCreature != NULL)
			pCreature->FadeChatString();	

		return;
	}

	//---------------------------------------------------------
	// If it is still falling..
	//---------------------------------------------------------
	// Uses m_ItemDropFPK and m_ItemDropSPK..
	//---------------------------------------------------------
	if (pItem->IsDropping())
	{
		int dropFrameID = pItem->GetDropFrameID();
		int frame = pItem->GetFrame();

		//#ifdef OUTPUT_DEBUG
			if (dropFrameID >= m_ItemDropFPK.GetSize())
			{
				DEBUG_ADD_FORMAT_ERR("[Error] DrawDropItem:item exceed Max : id=%d, itemFrame=%d", pItem->GetID(), dropFrameID);
				return;
			}
			else if (frame >= m_ItemDropFPK[dropFrameID].GetSize())
			{
				DEBUG_ADD_FORMAT_ERR("[Error] DrawDropItem:frame exceed Max : id=%d, itemFrame=%d, Frame=%d", pItem->GetID(), dropFrameID, frame);
				return;
			}
		//#endif

		//---------------------------------------------------------
		// After fixing this.. in MZone::AddItem( ...)
		// the commented-out part after if (bDropping) must be revived..
		//---------------------------------------------------------
		CFrame &Frame = m_ItemDropFPK[ dropFrameID ][ frame ];
		//CFrame &Frame = m_ItemTileFPK[ pItem->GetTileFrameID() ];

		POINT pointTemp;
		pointTemp.x = pPoint->x + Frame.GetCX();
		pointTemp.y = pPoint->y + Frame.GetCY() - pItem->GetDropHeight();

		#ifdef OUTPUT_DEBUG
			if (Frame.GetSpriteID() >= m_ItemDropISPK.GetSize())
			{
				DEBUG_ADD_FORMAT_ERR("[Error] DrawDropItem:sprite exceed Max : id=%d, spriteid=%d", pItem->GetID(), Frame.GetSpriteID());
				return;
			}			
		#endif

		CIndexSprite* pSprite = &m_ItemDropISPK[ Frame.GetSpriteID() ];
		//CSprite* pSprite = &m_ItemTileSPK[ Frame.GetSpriteID() ];

		// Output
		int colorSet = pItem->GetItemOptionColorSet();


		if(pItem->IsSpecialColorItem() )
			colorSet = pItem->GetSpecialColorItemColorset();

		CIndexSprite::SetUsingColorSet( colorSet, colorSet );

		m_pSurface->BltIndexSprite(&pointTemp, pSprite);							
	}
	//---------------------------------------------------------
	// Item on the ground
	//---------------------------------------------------------
	// Uses m_ItemTileFPK and m_ItemTileSPK
	//---------------------------------------------------------
	else
	{

		int tileFrameID = pItem->GetTileFrameID();
		//#ifdef OUTPUT_DEBUG
			if (tileFrameID >= m_ItemTileFPK.GetSize())
			{
				DEBUG_ADD_FORMAT_ERR("[Error] DrawItem:frame exceed Max : id=%d, frame=%d", pItem->GetID(), tileFrameID);
				return;
			}			
		//#endif

		CFrame &Frame = m_ItemTileFPK[ tileFrameID ];

		/*
		DEBUG_ADD_FORMAT("class=%d, type=%d, frame=%d, id=%d", pItem->GetItemClass(),
			pItem->GetItemType(),
			pItem->GetTileFrameID(),
			Frame.GetSpriteID());
		*/

		// Coordinate correction
		POINT pointTemp;
		pointTemp.x = pPoint->x + Frame.GetCX();
		pointTemp.y = pPoint->y + Frame.GetCY();

		#ifdef OUTPUT_DEBUG
			if (Frame.GetSpriteID() >= m_ItemTileISPK.GetSize())
			{
				DEBUG_ADD_FORMAT_ERR("[Error] DrawItem:sprite exceed Max : id=%d, spriteid=%d", pItem->GetID(), Frame.GetSpriteID());
				return;
			}			
		#endif

		CIndexSprite* pSprite = &m_ItemTileISPK[ Frame.GetSpriteID() ];

		int colorSet = pItem->GetItemOptionColorSet();


		if(pItem->IsSpecialColorItem() )
			colorSet = pItem->GetSpecialColorItemColorset();

		CIndexSprite::SetUsingColorSet( colorSet, colorSet );

		//---------------------------------------- 		
		// Set the Item selection rectangle area
		// Limit the size of the item's selection area.
		//---------------------------------------- 	
		RECT rect;
		rect.left	= max(pointTemp.x, pPoint->x);
		rect.top	= max(pointTemp.y, pPoint->y);
		rect.right	= min(pointTemp.x + pSprite->GetWidth(), pPoint->x + TILE_X);
		rect.bottom = min(pointTemp.y + pSprite->GetHeight(), pPoint->y + TILE_Y);		

		pItem->SetScreenRect( &rect );

		// Pick the appropriate sprite from m_SpritePack and draw it.

		//-------------------------------------------------------
		// Item the Mouse is pointing at
		//-------------------------------------------------------
		if (m_SelectItemID == pItem->GetID())		
		{		
			m_SOMOutlineColor = m_ColorOutlineItem;

			m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );

			m_SOM.Generate();

			//if (true || DarkBits==0)
			//{
				m_pSurface->BltSpriteOutline( &m_SOM, m_SOMOutlineColor );

			//-------------------------------------
			// Show the information of the Item the mouse is pointing at
			//-------------------------------------
			/*
			const char* pItemName = pItem->GetName();

			// Items with an option are blue..
			COLORREF nameColor;
			if (pItem->GetItemOption()==0)
			{
				nameColor = m_ColorNameItem;
			}
			else
			{
				nameColor = m_ColorNameItemOption;
			}

			if (pItemName != NULL)
			{
				FONTID	font = FONTID_ITEM;

				int x = pointTemp.x + (pSprite->GetWidth()>>1);
				int y = pointTemp.y - g_pClientConfig->FONT_ITEM_HEIGHT;			

				// So that the text fits inside the screen..
				if (x<0) x=0;
				if (y<0) y=0;

				//--------------------------------------------------
				// GetStringWidth requires unlock.
				//--------------------------------------------------
				m_pSurface->Unlock();
				int x2 = x + g_GetStringWidth(pItemName, g_ClientPrintInfo[font]->hfont);
				m_pSurface->Lock();

				int y2 = y + g_pClientConfig->FONT_ITEM_HEIGHT;

				if (x2 >= CLIPSURFACE_WIDTH)
				{
					x -= x2-CLIPSURFACE_WIDTH;
					x2 = CLIPSURFACE_WIDTH;
				}

				DRAWTEXT_NODE* pNode = new DRAWTEXT_NODE(
											x,
											y+1,
											pItemName,										
											nameColor,
											font
				);


				// Black box
				RECT rect = {
					x, 
					y,
					x2,
					y2
				};

				pNode->SetBox( rect, m_ColorBlackHalf );

				AddText( pNode );
			}	
			*/

		}
		else
		{	
			//-------------------------------------------------------
			// Mark the Item being chased
			//-------------------------------------------------------
			//if (g_pPlayer->IsTraceItem() && g_pPlayer->GetTraceID()==pItem->GetID())
			{	
				// RED
			//	m_pSurface->BltSpriteColor(&pointTemp, &m_ItemTileSPK[ Frame.GetSpriteID() ], 0);
			}	
			//-------------------------------------------------------
			// Normal output
			//-------------------------------------------------------
			//else 
			{
				//if (true || DarkBits==0)
				//{			

				//-------------------------------------------------------
				// If it is an installed mine..
				//-------------------------------------------------------
				if (pItem->GetItemClass()==ITEM_CLASS_MINE
					&& ((MMine*)pItem)->IsInstalled())
				{
					// Draw it in red.
					m_pSurface->BltIndexSpriteColor(&pointTemp, pSprite, 0);
				}
				//-------------------------------------------------------	
				// Ordinary item
				//-------------------------------------------------------
				else
				{					
					m_pSurface->BltIndexSprite(&pointTemp, pSprite);
				}
			}
		}	

		// [ TEST CODE ]
		//-------------------------------------
		// Show the information of the Item the mouse is pointing at
		//-------------------------------------
		/*
		DRAWTEXT_NODE* pNode = new DRAWTEXT_NODE(
									pointTemp.x + (m_ItemTileSPK[ Frame.GetSpriteID() ].GetWidth()>>1),
									pointTemp.y - 20,
									pItem->GetName(),
									0xFFFF					
		);

		AddText( pNode );
		*/
	}

	//--------------------------------------------------------------------
	// Add to ItemNameList
	//--------------------------------------------------------------------
	if (m_bDrawItemNameList)
	{
		DWORD distance = max( abs(g_pPlayer->GetX()-pItem->GetX()),
								abs(g_pPlayer->GetY()-pItem->GetY()) );

		AddItemName( new DRAWITEMNAME_NODE(distance, pItem) );
	}

	//#ifdef OUTPUT_DEBUG
		//	DEBUG_ADD("[TempDebug] After Draw Item");
	//#endif	

	__END_PROFILE("DrawItem")

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "End DrawItem" );
	#endif


}


//----------------------------------------------------------------------
// Draw ItemShadow : draw the Item shadow
//----------------------------------------------------------------------
// Draw pItem at pPoint of pSurface.
//----------------------------------------------------------------------
void	
MTopView::DrawItemShadow(POINT* pPoint, MItem* pItem)
{
	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "Start DrawItemShadow" );
	#endif

	__BEGIN_PROFILE("DrawItemShadow")

	//------------------------------------------------
	// A Slayer cannot see inside Darkness.
	//------------------------------------------------
	if (g_pZone->GetSector(pItem->GetX(), pItem->GetY()).HasDarkness() 
		&&IsAffectFromDarkness())
	{
		#ifdef OUTPUT_DEBUG_DRAW_PROCESS
			DEBUG_ADD( "End DrawItemShadow(D)" );
		#endif

		return;
	}

	//---------------------------------------------------------
	// For a corpse, draw the Creature.
	//---------------------------------------------------------
	if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
	{
		MCorpse* pCorpse = (MCorpse*)pItem;

		MCreature* pCreature = pCorpse->GetCreature();

		if (pCreature!=NULL)
		{
			//---------------------------------------------------------
			// Dead appearance of a normal character..
			//---------------------------------------------------------
			{
				// If it is the selected one
				// Draw it as if the Creature were selected.. it's a cheat. - -;
				if (m_SelectItemID == pItem->GetID())
				{
					m_SelectCreatureID = pCreature->GetID();

					if (
							(
							(pCreature->GetCreatureType() >= 371 && pCreature->GetCreatureType() <= 376)	// relic
							|| (pCreature->GetCreatureType() >= 560 && pCreature->GetCreatureType() <= 563)	// relic
							|| (pCreature->GetCreatureType() >= 526 && pCreature->GetCreatureType() <= 549)	// altar
							|| pCreature->GetCreatureType() == 482	// Christmas tree
							|| pCreature->GetCreatureType() == 650	// bulletin board
							|| pCreature->GetCreatureType() == 672
							|| pCreature->GetCreatureType() == 673
							|| pCreature->GetCreatureType() == 730 // waypoint 1
							|| pCreature->GetCreatureType() == 731 // waypoint 2
							|| pCreature->GetCreatureType() == 732 // waypoint 3
							)
						||
							(
							pCreature->GetActionCount() < pCreature->GetActionCountMax()
							// In the Teen version the Slayer corpse shadow must be drawn
							|| (g_pUserInformation->GoreLevel == false
							&& pCreature->GetActionCount()>=pCreature->GetActionCountMax()/2
							&& pCreature->IsDead())
							)
						)
					{
						POINT pointTemp = *pPoint;
						DrawCreatureShadow(&pointTemp, pCreature);
					}

					m_SelectCreatureID = OBJECTID_NULL;			
				}
				else
				{
					if (
							(
							(pCreature->GetCreatureType() >= 371 && pCreature->GetCreatureType() <= 376)	// relic
							|| (pCreature->GetCreatureType() >= 560 && pCreature->GetCreatureType() <= 563)	// relic
							|| (pCreature->GetCreatureType() >= 526 && pCreature->GetCreatureType() <= 549)	// relic
							|| pCreature->GetCreatureType() == 482	// Christmas tree
							|| pCreature->GetCreatureType() == 650  // bulletin board
							|| pCreature->GetCreatureType() == 672
							|| pCreature->GetCreatureType() == 673
							|| pCreature->GetCreatureType() == 730 // waypoint 1
							|| pCreature->GetCreatureType() == 731 // waypoint 2
							|| pCreature->GetCreatureType() == 732 // waypoint 3
							)
						||
							(
							pCreature->GetActionCount() < pCreature->GetActionCountMax()
							// In the Teen version the Slayer corpse shadow must be drawn
							|| (g_pUserInformation->GoreLevel == false
							&& pCreature->GetActionCount()>=pCreature->GetActionCountMax()/2
							&& pCreature->IsDead())
							)
						)
					{
						POINT pointTemp = *pPoint;
						DrawCreatureShadow(&pointTemp, pCreature);
					}
				}
			}

		}

		#ifdef OUTPUT_DEBUG_DRAW_PROCESS
			DEBUG_ADD( "End DrawItemShadow" );
		#endif



		return;
	}


	__END_PROFILE("DrawItemShadow")

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "End DrawItemShadow" );
	#endif


}

//----------------------------------------------------------------------
// Draw ImageObject : draw the picture
//----------------------------------------------------------------------
// Draw pImageObject at pPoint of pSurface.
//----------------------------------------------------------------
void	
MTopView::DrawImageObject(POINT* pPoint, MImageObject* pImageObject)
{		
	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "Start DrawImageObject" );
	#endif

	__BEGIN_PROFILE("DrawImageObject")

	#if defined(OUTPUT_DEBUG) && defined(_DEBUG)
		if (g_pSDLInput->KeyDown(DIK_T) && g_pSDLInput->KeyDown(DIK_LCONTROL))
		{
			return;
		}
	#endif


	// Pick the appropriate sprite from m_SpritePack and draw it.
	// Semi-transparent handling for a picture that covers the Player

	//if (pImageObject->GetImageObjectID() == m_BehindImageObjectID)


	//------------------------------------------------------
	// 1. Is the ImageObject one that can become transparent?
	// 2. Is the Viewpoint below the Player position?
	// 3. Does the Sprite cover the Player?
	//------------------------------------------------------
	// When 1, 2 and 3 are all satisfied.. handle as semi-transparent.
	//------------------------------------------------------
	bool bTrans;



	//------------------------------------------------------
	// Specify an overall dark value.
	// However, if the ImageObject's coordinates (Sector coordinates in the Zone)
	// are on a bright Tile (Light is not 0), set DarkBits=0.
	//------------------------------------------------------
	//BYTE DarkBits;

	//------------------------------------------------------
	// [ TEST CODE ]
	//------------------------------------------------------
		// This is unnecessary code.
		// It is enough to make it properly in the MapEditor.
	/*
	if (pImageObject->GetX()!=SECTORPOSITION_NULL && pImageObject->GetY()!=SECTORPOSITION_NULL
		&&
		m_pZone->GetSector(pImageObject->GetX(), pImageObject->GetY()).GetLight()!=0)
	{
		DarkBits = 0;
	}
	else
	{
		DarkBits = m_DarkBits;
	}

	// With 3D acceleration.. always draw the original colors as they are.
	if (true)
	{
		DarkBits = 0;
	}
	*/

	//----------------------------------------------------				
	// Buildings are always semi-transparent
	//----------------------------------------------------				
	BOOL bBlendingShadow = 1;//g_pUserOption->BlendingShadow;

	//------------------------------------------------------
	//
	//  ShadowObject output
	//
	//------------------------------------------------------
	if (pImageObject->GetObjectType()==MObject::TYPE_SHADOWOBJECT)
	{
		int spriteID = pImageObject->GetSpriteID();

		if (spriteID != SPRITEID_NULL)	
		{			
//
//
//					/*
//					*/
//
//					
//							
			{	
				if (bBlendingShadow)
				{
					m_pSurface->BltShadowSpriteDarkness( pPoint, &m_ImageObjectSSPK[spriteID], 1 );
				}
				else
				{
					m_pSurface->BltShadowSprite( pPoint, &m_ImageObjectSSPK[spriteID] );
				}

				#ifdef OUTPUT_DEBUG
					if (g_pSDLInput->KeyDown(DIK_S) && 
						(g_pSDLInput->KeyDown(DIK_RCONTROL)))
					{
						m_pSurface->Unlock();

						char str[128];
						SafeFormat::Format(str, "iid=%d, view=%d", (int)pImageObject->GetImageObjectID(), (int)pImageObject->GetViewpoint());
						// m_pSurface->GDI_Text(pPoint->x, pPoint->y, str, 0xFFFF);
						TextSystem::TextService::RenderText(g_pLast, pPoint->x, pPoint->y, str);

						m_pSurface->Lock();
					}
				#endif
			}	
		}
	}
	//------------------------------------------------------
	//
	//          ShadowAnimationObject output
	//
	//------------------------------------------------------
	else if (pImageObject->GetObjectType()==MObject::TYPE_SHADOWANIMATIONOBJECT)
	{
	}
	//------------------------------------------------------
	//
	//          InteractionObject output
	//
	//------------------------------------------------------
//	else if (pImageObject->GetObjectType()==MObject::TYPE_INTERACTIONOBJECT)
//	{
//		MInteractionObject* pInteractionObject = (MInteractionObject*)pImageObject;
//
//		TYPE_SPRITEID sprite;
//	
//		switch (pInteractionObject->GetBltType())
//		{
//			//--------------------------------
//			// Blt Normal
//			//--------------------------------
//				
//				/*
//				*/
//
//
//				CSprite* pSprite = &m_InteractionObjectSPK[ sprite ];		
//				
//				//---------------------------------------- 		
//				// Set the InteractionObject selection rectangle area
//				//---------------------------------------- 	
//
//
//				// semi-transparent check
//				bTrans = pImageObject->IsTrans();
//
//				//--------------------------------
//				// For the selected InteractionObject...
//				//--------------------------------
//
//						m_SOM.Add( pPoint->x, pPoint->y, pSprite );
//						
//						m_SOM.Generate();
//
//				//--------------------------------
//				// Transparent handling
//				//--------------------------------
//
//				//--------------------------------
//				// Plain output
//				//--------------------------------
//						///*
//
//						//int sight = g_pPlayer->GetMaxLightSight();
//						//int pX = sight * TILE_X;
//						//int pY = sight * TILE_Y;
//						//int sX = 410-(pX>>1) - pPoint->x;
//						//int sY = 270-(pY>>1) - pPoint->y;			
//
//
//			//--------------------------------
//			// Blt Channel 
//			//--------------------------------
//			case BLT_EFFECT :	
//				/*
//				//--------------------------------
//				// For the selected InteractionObject...
//				//--------------------------------
//				*/
//			break;
//		}
//
	else
	{
		//------------------------------------------------------
		//
		//          ImageObject : Sprite output
		//
		//------------------------------------------------------
		int spriteID = pImageObject->GetSpriteID();
		if (spriteID != SPRITEID_NULL)	
		{			
			//---------------------------------------
			// Load the ImageObject whose sprite is ID.
			//---------------------------------------
//			#ifdef	OUTPUT_DEBUG
//				char str[256];
//			#endif
//
//
//				m_ImageObjectSPKFile.seekg(m_ImageObjectSPKI[spriteID], ios::beg);				
//				//--------------------------------------------------
//				// When Loading succeeded
//				//--------------------------------------------------
//
//				//--------------------------------------------------
//				// When it failed --> it is already being Loaded.
//				//--------------------------------------------------
//				/*
//	
//						DEBUG_ADD( str );
//					#endif
//
//					// Raise the file thread priority.
//					//SetThreadPriority(g_hFileThread, THREAD_PRIORITY_HIGHEST);
//					MLoadingSPKWorkNode3* pNode = new MLoadingSPKWorkNode3(spriteID, m_ImageObjectSPKI[spriteID]);
//
//
//						
//
//
//				}
//				*/
//			}

			bTrans = pImageObject->IsTrans()
						&& pImageObject->GetViewpoint() >= g_pPlayer->GetY()
						// 2001.9.5 added
						&& pImageObject->IsWallTransPosition(g_pPlayer->GetX(), g_pPlayer->GetY());



			//--------------------------------
			// Transparent handling
			//--------------------------------
			if (bTrans)
			{	
				///*
				//m_pSurface->BltSpriteHalf(pPoint, &m_ImageObjectSPK[ spriteID ]);


				// Correct the filter coordinates to fit the picture
				CSprite::SetFilter(static_cast<short>(m_FilterPosition.x - pPoint->x),
									static_cast<short>(m_FilterPosition.y - pPoint->y),
									&m_ImageObjectFilter);

				if (m_ImageObjectSPK[ spriteID ].IsIntersectFilter())
				{
					//if (DarkBits==0)
					//{
					#if defined(OUTPUT_DEBUG) && defined(_DEBUG)
						if (g_pSDLInput->KeyDown(DIK_A) && 
							(g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL)))
						{
							// Pressing CTRL+A draws nothing.
						}
						else
					#endif
						{
							m_pSurface->BltSpriteAlphaFilter(pPoint, 
														&m_ImageObjectSPK[ spriteID ]);							
						}						

				}
				else
				{
					//if (DarkBits==0)
					//{
					if( pImageObject->IsHalf() )
						m_pSurface->BltSpriteHalf(pPoint, &m_ImageObjectSPK[ spriteID ]);
					else
						m_pSurface->BltSprite(pPoint, &m_ImageObjectSPK[ spriteID ]);
				}
				//*/

				//m_pSurface->BltSpriteHalf(pPoint, &m_ImageObjectSPK[ spriteID ]);
				//m_pSurface->BltSprite(pPoint, &m_ImageObjectOutlineSPK[ 0 ]);
			}
			//--------------------------------
			// Plain output
			//--------------------------------
			else
			{					
				//if (m_ImageObjectSPK[ spriteID ].IsInit())
				{
					///*

					//int sight = g_pPlayer->GetMaxLightSight();
					//int pX = sight * TILE_X;
					//int pY = sight * TILE_Y;
					//int sX = 410-(pX>>1) - pPoint->x;
					//int sY = 270-(pY>>1) - pPoint->y;			


					//{
#if defined(OUTPUT_DEBUG) && defined(_DEBUG)
					if (g_pSDLInput->KeyDown(DIK_A) && 
						(g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL)))
					{
						// Pressing CTRL+A draws nothing.
					}
					else
#endif
					{
						if( pImageObject->IsHalf() )
							m_pSurface->BltSpriteHalf(pPoint, &m_ImageObjectSPK[ spriteID ]);
						else
							m_pSurface->BltSprite(pPoint, &m_ImageObjectSPK[ spriteID ]);
						/*							
						static int gray = 0;
						static int g = 1;

						CSpriteSurface::s_Value1 = gray;

						CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_GRAY_SCALE_VARIOUS );

						m_pSurface->BltSpriteEffect(pPoint, &m_ImageObjectSPK[ spriteID ]);

						static DWORD lastFrame = g_CurrentFrame;

						if (g_CurrentFrame - lastFrame > 2)
						{
							gray += g;

							if (gray == 32 || gray == 0)
							{
								g = -g;
							}						

							lastFrame = g_CurrentFrame;
						}
						*/
					}

/*
#ifdef OUTPUT_DEBUG
	if (g_pSDLInput->KeyDown(DIK_RCONTROL))
	{
		static int value = 0;
		static int step = 0;

		if (value==0)
		{
			m_pSurface->BltSprite(pPoint, &m_ImageObjectSPK[ spriteID ]);
		}
		else if (value==64)
		{
			CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_SIMPLE_OUTLINE );

			m_pSurface->BltSpriteEffect(pPoint, &m_ImageObjectSPK[ spriteID ]);
		}
		else
		{
			CSpriteSurface::s_Value1 = value;

			CSpriteSurface::SetEffect( CSpriteSurface::EFFECT_WIPE_OUT );

			m_pSurface->BltSpriteEffect(pPoint, &m_ImageObjectSPK[ spriteID ]);
		}

		if (g_pSDLInput->KeyDown(DIK_RSHIFT))
		{
			if (value==0 || value==64)
			{
				if (step==0)	
				{
					if (value==0)
						step = 1;
					else
						step = -1;
				}
				else step *= -1;
			}
		}	
		else
		{
			value += step;

			if (value<0) value = 0;
			else if (value>64) value=64;
		}
	}
	else
#endif
	{
		m_pSurface->BltSprite(pPoint, &m_ImageObjectSPK[ spriteID ]);
	}
	*/

				}
			}

			#ifdef OUTPUT_DEBUG
				if (g_pSDLInput->KeyDown(DIK_I) && 
					(g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL)))
				{
					char str[128];
					SafeFormat::Format(str, "iid=%d, sid=%d, vp=%d", (int)pImageObject->GetImageObjectID(), (int)spriteID, (int)pImageObject->GetViewpoint());

					m_pSurface->Unlock();
					// m_pSurface->GDI_Text(pPoint->x, pPoint->y, str, 0xFFFF);
					TextSystem::TextService::RenderText(g_pLast, pPoint->x, pPoint->y, str);
					m_pSurface->Lock();
				}
			#endif
		}


		//------------------------------------------------------------
		//
		//     For an AnimationObject the Frame must be drawn too.
		//
		//------------------------------------------------------------
		if (pImageObject->IsAnimation())
		{
			MAnimationObject* pAnimationObject = (MAnimationObject*)pImageObject;

			TYPE_SPRITEID sprite;

			POINT pointTemp = *pPoint;

			/*******************start of shadow output******************/
			if (pAnimationObject->GetFrameID() < m_ImageObjectShadowFPK.GetSize()
				&& pAnimationObject->GetFrame() < m_ImageObjectShadowFPK[pAnimationObject->GetFrameID()].GetSize())
			{
				CFrame &FrameShadow = m_ImageObjectShadowFPK[ pAnimationObject->GetFrameID() ][ pAnimationObject->GetFrame() ];
				sprite = FrameShadow.GetSpriteID();

				if (sprite != SPRITEID_NULL)	
				{			
					pointTemp.x += (FrameShadow.GetCX());
					pointTemp.y += (FrameShadow.GetCY());
					if (bBlendingShadow)
					{
						m_pSurface->BltShadowSpriteDarkness( &pointTemp, &m_ImageObjectSSPK[sprite], 1 );
					}
					else
					{
						m_pSurface->BltShadowSprite( &pointTemp, &m_ImageObjectSSPK[sprite] );
					}
				}
			}
			/*******************end of shadow output******************/

			switch (pAnimationObject->GetBltType())
			{
				//--------------------------------
				// Blt Normal
				//--------------------------------
				case BLT_NORMAL :
				{
					int currentFrame = g_CurrentFrame % m_ImageObjectFPK[ pAnimationObject->GetFrameID() ].GetSize();

					CFrame &Frame = m_ImageObjectFPK[ pAnimationObject->GetFrameID() ][ currentFrame ];
					sprite = Frame.GetSpriteID();
					if(sprite >= m_ImageObjectSPK.GetSize())
						break;
					//---------------------------------------
					// Load the ImageObject whose sprite is ID.
					//---------------------------------------
//
//						m_ImageObjectSPKFile.seekg(m_ImageObjectSPKI[sprite], ios::beg);						
//						
//						//--------------------------------------------------
//						// When Loading succeeded
//						//--------------------------------------------------
//						//--------------------------------------------------
//						// When it failed --> it is already being Loaded.
//						//--------------------------------------------------
//						
////
////							// Raise the file thread priority.
////							//SetThreadPriority(g_hFileThread, THREAD_PRIORITY_HIGHEST);
////							MLoadingSPKWorkNode3* pNode = new MLoadingSPKWorkNode3(sprite, m_ImageObjectSPKI[sprite]);
////
////
////								
////
////
////						}
//						
//					}
					// Coordinate correction
					pPoint->x += Frame.GetCX();
					pPoint->y += Frame.GetCY();

					// When there is no spriteID, check semi-transparency again
					if (spriteID==SPRITEID_NULL)
					{
						bTrans = true;
					}				

					//--------------------------------
					// Transparent handling
					//--------------------------------
					if (bTrans)
					{
						CSprite::SetFilter(static_cast<short>(m_FilterPosition.x - pPoint->x),
											static_cast<short>(m_FilterPosition.y - pPoint->y),
											&m_ImageObjectFilter);

							m_pSurface->BltSpriteAlphaFilter(pPoint, 
															&m_ImageObjectSPK[ sprite ]);

					}
					//--------------------------------
					// Plain output
					//--------------------------------
					else
					{
						///*
						{
							//CSprite& spriteRef = m_ImageObjectSPK[ sprite ];

							//int sight = g_pPlayer->GetMaxLightSight();
							//int pX = sight * TILE_X;
							//int pY = sight * TILE_Y;
							//int sX = 410-(pX>>1) - pPoint->x;
							//int sY = 270-(pY>>1) - pPoint->y;			

								m_pSurface->BltSprite(pPoint, &m_ImageObjectSPK[ spriteID ]);
						}
					}
				}
				break;

				//--------------------------------
				// Blt Channel 
				//--------------------------------
				case BLT_EFFECT :
				{
					int fid = pAnimationObject->GetFrameID();
					if( fid < m_EffectAlphaFPK.GetSize() )
					{
						CEffectFrame &Frame = m_EffectAlphaFPK[ fid ][ 0 ][ pAnimationObject->GetFrame() ];

						sprite = Frame.GetSpriteID();

						// Coordinate correction
						pPoint->x += Frame.GetCX();
						pPoint->y += Frame.GetCY();

						DRAW_ALPHASPRITEPAL(pPoint, sprite, m_EffectAlphaSPK, m_EffectAlphaPPK[fid])//, m_EffectAlphaSPKI, m_EffectAlphaSPKFile)					

						//-------------------------------------------------------
						// If H/W acceleration is available...
						//-------------------------------------------------------
						if (s_bUse3DLight)
						{
//
//							DRAW_TEXTURE_SPRITEPAL_LOCKED(pPoint->x, pPoint->y,
//								sprite, m_pAlphaEffectTextureManager, fid )//m_EffectAlphaPPK[fid])
//								


							//------------------------------------------------
							// Add a LightFilter.
							//------------------------------------------------
							AddLightFilter3D( pPoint->x + 24, 
								pPoint->y + 24, 
								Frame.GetLight(),			// light brightness
								false);		// false = screen coordinates

						}
						//-------------------------------------------------------
						// 2D output
						//-------------------------------------------------------
						else
						{
//							DRAW_ALPHASPRITEPAL(pPoint, sprite, m_EffectAlphaSPK, m_EffectAlphaPPK[fid])//, m_EffectAlphaSPKI, m_EffectAlphaSPKFile)					

								AddLightFilter2D( pPoint->x + 24, 
								pPoint->y + 24, 
								Frame.GetLight(),			// light brightness
								false);		// false = screen coordinates
						}
					}
					if (g_bFrameChanged)
					{
						pAnimationObject->NextFrame();
					}

				}
				break;

				//--------------------------------
				// Blt Channel 
				//--------------------------------
				case BLT_SCREEN :
				{
					int fid = pAnimationObject->GetFrameID();
					if(fid < m_EffectScreenFPK.GetSize())
					{
						CEffectFrame &Frame = m_EffectScreenFPK[fid][0][pAnimationObject->GetFrame()];					
						sprite = Frame.GetSpriteID();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetSpriteID();

						if (sprite!=SPRITEID_NULL)
						{
							// Frame coordinate correction
							pPoint->x += Frame.GetCX();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetCX();
							pPoint->y += Frame.GetCY();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetCY();

							DRAW_NORMALSPRITEPAL_EFFECT(pPoint, 
								sprite, 
								m_EffectScreenSPK, 
//									m_EffectScreenSPKI, 
//									m_EffectScreenSPKFile,
								m_EffectScreenPPK[fid],
								CSpriteSurface::EFFECT_SCREEN)
							//-------------------------------------------------------
							// If H/W acceleration is available...
							//-------------------------------------------------------
							if (s_bUse3DLight)
							{
//
//
//								DRAW_TEXTURE_SPRITEPAL_LOCKED(pPoint->x, pPoint->y,
//									sprite, m_pScreenEffectTextureManager, fid ) //m_EffectScreenPPK[fid])
//									

								//------------------------------------------------
								// Add a LightFilter.
								//------------------------------------------------
								AddLightFilter3D( pPoint->x + 24, 
									pPoint->y + 24, 
									Frame.GetLight(),			// light brightness
									false);		// false = screen coordinates

							}
							//-------------------------------------------------------
							// If H/W acceleration is not available...
							//-------------------------------------------------------
							else
							{
//								DRAW_NORMALSPRITEPAL_EFFECT(pPoint, 
//									sprite, 
//									m_EffectScreenSPK, 
////									m_EffectScreenSPKI, 
////									m_EffectScreenSPKFile,
//									m_EffectScreenPPK[fid],
//									CSpriteSurface::EFFECT_SCREEN)

									AddLightFilter2D( pPoint->x + 24, 
									pPoint->y + 24, 
									Frame.GetLight(),			// light brightness
									false);		// false = screen coordinates
							}
						}
						if (g_bFrameChanged)
						{
							pAnimationObject->NextFrame();
						}
					}


				}
				break;
			}

			//---------------------------------------
			// Test Code : Next Frame
			//---------------------------------------
		}

	}


	//#ifdef OUTPUT_DEBUG
		//	DEBUG_ADD("[TempDebug] After Draw ImageObject");
	//#endif

	__END_PROFILE("DrawImageObject")

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "End DrawImageObject" );
	#endif


}

//----------------------------------------------------------------
// SurfaceLock For AlphaEffect
//----------------------------------------------------------------
// If 3D and locked, make it Unlock...
// If not 3D and not locked, make it lock..
//
// The return value is the original Lock state
//----------------------------------------------------------------
/*
BOOL
MTopView::SurfaceLockForAlphaEffect()
{
	BOOL bLock = m_pSurface->IsLock();

	if (true)
	{
		if (bLock)
		{
			// If 3D and locked, make it Unlock...
			m_pSurface->Unlock();
		}
	}
	else
	{	
		if (!bLock)
		{
			// If not 3D and not locked, make it lock..
			m_pSurface->Lock();
		}
	}

	return bLock;
}
*/

//----------------------------------------------------------------
// SurfaceLock
//----------------------------------------------------------------
// The return value is the original lock state
//----------------------------------------------------------------
/*
BOOL
MTopView::SurfaceLock()
{
	BOOL bLock = m_pSurface->IsLock();

	if (!bLock)
	{
		m_pSurface->Lock();
	}

	return bLock;
}
*/

//----------------------------------------------------------------
// SurfaceLock Restore
//----------------------------------------------------------------
// Restore the original Lock state.
//----------------------------------------------------------------
/*
void
MTopView::SurfaceLockRestore( BOOL bOldLock )
{
	if (bOldLock)
	{
		if (!m_pSurface->IsLock())
		{
			// If it was originally Locked but is not Locked
			m_pSurface->Lock();
		}
	}
	else
	{
		if (m_pSurface->IsLock())
		{
			// If it was not originally Locked but is Locked..
			m_pSurface->Unlock();
		}
	}
}
*/

//----------------------------------------------------------------
// Effect output
//----------------------------------------------------------------
void	
MTopView::DrawEffect(POINT* pPoint, EFFECT_LIST::const_iterator iEffect, BYTE size)
{	
	(void)pPoint;
	if(g_pEventManager->GetEventByFlag(EVENTFLAG_NOT_DRAW_EFFECT))
		return;

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "Start DrawEffect" );
	#endif


	POINT point;

 	for (int i=0; i<size; i++)
	{
		MEffect* pEffect = *iEffect;

		if(pEffect->IsDelayFrame() == true)
		{
			iEffect++;
			continue;
		}
		//------------------------------------------------------------------
		// Output coordinates in Tile units
		//------------------------------------------------------------------
		//if (pEffect->GetEffectType()==MEffect::EFFECT_SECTOR)
		//{			
		//	point = *pPoint;
		//}
		//------------------------------------------------------------------
		// Screen output coordinates
		//------------------------------------------------------------------
		//else 
		if (pEffect->GetEffectType()==MEffect::EFFECT_SCREEN)
		{			
			MScreenEffect* pScreenEffect = (MScreenEffect*)pEffect;
			//MScreenEffect::SetScreenBasis( pPoint->x, pPoint->y );	// It only needs to be done once.. hmm.
			point.x = pScreenEffect->GetScreenX();
			point.y = pScreenEffect->GetScreenY();
		}
		//------------------------------------------------------------------
		// Output coordinates in Pixel units
		//------------------------------------------------------------------
		else
		{
			//MMovingEffect* pMovingEffect = (MMovingEffect* const)pEffect;

			// Convert the Pixel-unit coordinates to screen coordinates.
			point.x = pEffect->GetPixelX() - m_FirstZonePixel.x;
			point.y = pEffect->GetPixelY() - pEffect->GetPixelZ() - m_FirstZonePixel.y;
		}

		//------------------------------------------------------------------
		// Output
		//------------------------------------------------------------------
		DrawEffect( &point, pEffect, pEffect->IsSelectable() );		

		// Next Effect
		iEffect++;
	}	

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "End DrawEffect" );
	#endif


}


//----------------------------------------------------------------
// Draw GroundEffect
//----------------------------------------------------------------
// Draw the GroundEffect attached directly to the Tile
//----------------------------------------------------------------
void	
MTopView::DrawGroundEffect()
{	
	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "Start DrawGroundEffect" );
	#endif

	POINT point;
	POINT pointTemp;

	int size = g_pZone->GetGroundEffectCount();
	MZone::EFFECT_MAP::const_iterator iEffect = g_pZone->GetGroundEffects();

	for (int i=0; i<size; i++)
	{
		MEffect* pEffect = iEffect->second;

		if(pEffect->IsSkipDraw())
		{
			iEffect++;
			continue;
		}
		// Convert the Pixel-unit coordinates to screen coordinates.
		// center point
		point.x = pEffect->GetPixelX() - m_FirstZonePixel.x;
		point.y = pEffect->GetPixelY() - pEffect->GetPixelZ() - m_FirstZonePixel.y;			

		pointTemp = point;

		//------------------------------------------------------------------
		// Output
		//------------------------------------------------------------------
		DrawEffect( &pointTemp, pEffect, pEffect->IsSelectable());

		//------------------------------------------------------------------
		// For a selectable Effect
		//------------------------------------------------------------------
		if (pEffect->IsSelectable()
			&& m_SelectEffectID==pEffect->GetID())
		{
			MEffectTarget* pEffectTarget = pEffect->GetEffectTarget();

			if (pEffectTarget!=NULL)
			{
				//------------------------------------------------------------------
				// For a Portal
				//------------------------------------------------------------------
				if (pEffectTarget->GetEffectTargetType()==MEffectTarget::EFFECT_TARGET_PORTAL
					&& g_pPlayer->IsVampire())
				{
					MPortalEffectTarget* pPortalEffectTarget = (MPortalEffectTarget*)pEffectTarget;

					char ownerName[20];
					SafeFormat::Copy(ownerName, pPortalEffectTarget->GetOwnerName());
					int zoneID = pPortalEffectTarget->GetZoneID();


					int zoneX = pPortalEffectTarget->GetZoneX();
					int zoneY = pPortalEffectTarget->GetZoneY();

					if(//( !g_pUserInformation->IsNetmarble || g_mapPremiumZone.find(zoneID) == g_mapPremiumZone.end() )
						/*&&*/ g_pSystemAvailableManager->ZoneFiltering( zoneID ))
						gpC_mouse_pointer->SetCursorPortal( zoneID, zoneX, zoneY, ownerName );

				}
			}
		}
		// Next Effect
		iEffect++;
	}	

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "End DrawGroundEffect" );
	#endif


}

//----------------------------------------------------------------
// Draw AttachEffect
//----------------------------------------------------------------
// Effect output
// [the essence of hardcoding] - -;;
// type - 0 - always draw
//        1 - draw only the fore ground
//        2 - draw only the back ground
//----------------------------------------------------------------
// For now..
// PairFrame is drawn only in AttachEffect..
// ActionFrame is also drawn only in AttachEffect..
// ActionFrame is ignored in NormalEffect. There is no need to draw it yet, and it is a hassle --;;
//----------------------------------------------------------------
void	
MTopView::DrawAttachEffect(POINT* pPoint, ATTACHEFFECT_LIST::const_iterator iEffect, BYTE size, MCreature* pCreature, int type)
{	
	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "Start DrawAttachEffect" );
	#endif

	if(pCreature == NULL)	
		return;
	POINT pointTemp;
	for (int i=0; i<size; i++)	
	{
		MEffect* pEffect = *iEffect;

		//----------------------------------------------------------------
		// Draw only Effects that are expressed as Sprites.
		// Not Attach.. or, even if Attach, drawn as a Sprite
		//----------------------------------------------------------------		
		if (pEffect->GetEffectType()!=MEffect::EFFECT_ATTACH ||
			(pEffect->GetEffectType()==MEffect::EFFECT_ATTACH && ((MAttachEffect*)pEffect)->IsEffectSprite())


			)
		{
			int direction = pEffect->GetDirection();
			int frameID = pEffect->GetFrameID();
			int frame = pEffect->GetFrame();

			// 2004, 9, 14, sobeit add start - do not show the effect for an install turret
			int TempSecreenEffect = GET_EFFECTSPRITETYPE_SCREEN( frameID );

			if(pCreature->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
			{
				if(frameID != EFFECTSPRITETYPE_INSTALL_TURRET_FIRE1 &&
					frameID != EFFECTSPRITETYPE_INSTALL_TURRET_FIRE2 &&
					TempSecreenEffect != EFFECTSPRITETYPE_INSTALL_TURRET_BULLET)
				{
					iEffect++;
					continue;
				}
			}

			switch ((*iEffect)->GetBltType())
			{
				//--------------------------------------------------------
				// NORMAL
				//--------------------------------------------------------
				case BLT_NORMAL :
				{
					POINT pointTemp2 = *pPoint;

					AFFECT_ORBIT_EFFECT_POSITION( pEffect, pointTemp )

					DRAW_SPRITE_WITH_EFFECTFRAME(
											&pointTemp2,
											m_EffectNormalSPK,
											m_EffectNormalFPK,
											frameID, direction, frame )

					// FIX: Use the saved table index from effect creation time
					// instead of recalculating from FrameID using macro
					int est;
					if (pEffect->GetEffectType() == MEffect::EFFECT_ATTACH) {
						// MAttachEffect saves the table index in m_EffectSpriteType
						est = ((MAttachEffect*)pEffect)->GetEffectSpriteType();
					} else {
						// For other effect types, fall back to macro (may need fixing too)
						est = GET_EFFECTSPRITETYPE_NORMAL(frameID);
					}

					// Bounds check
					if (est < 0 || est >= g_pEffectSpriteTypeTable->GetSize()) {
						LOG_ERROR("[Effect BLT_NORMAL] est=%d out of bounds (tableSize=%d), frameID=%d, frame=%d, dir=%d",
						         est, g_pEffectSpriteTypeTable->GetSize(), frameID, frame, direction);
						break;  // Skip this effect to avoid crash
					}

					DRAW_SPRITE_WITH_EFFECTFRAME_PAIR(
											est,
											&pointTemp2,
											m_EffectNormalSPK,
											m_EffectNormalFPK,
											direction, frame )
				}
				break;

				//--------------------------------------------------------
				// EFFECT
				//--------------------------------------------------------
				case BLT_EFFECT :
				{
					// FIX: Use the saved table index from effect creation time
					// instead of recalculating from FrameID using macro
					int est;
					if (pEffect->GetEffectType() == MEffect::EFFECT_ATTACH) {
						// MAttachEffect saves the table index in m_EffectSpriteType
						est = ((MAttachEffect*)pEffect)->GetEffectSpriteType();
					} else {
						// For other effect types, fall back to macro (may need fixing too)
						est = GET_EFFECTSPRITETYPE_EFFECT(frameID);
					}

					// Bounds check
					if (g_pEffectSpriteTypeTable != NULL) {
						int tableSize = g_pEffectSpriteTypeTable->GetSize();
						if (est < 0 || est >= tableSize) {
							LOG_ERROR("[Effect BLT_EFFECT] est=%d out of bounds (tableSize=%d), frameID=%d, frame=%d, dir=%d",
							         est, tableSize, frameID, frame, direction);
							break;
						}
					}

					// Picks the effectFrameID that matches the action.
					int aest = GET_ACTION_EFFECTSPRITETYPE(est);
					const bool bBack = (*g_pEffectSpriteTypeTable)[est].bPairFrameBack;

					// FIX: sest should use the same table index as est
					int sest = est;

					if (aest!=FRAMEID_NULL)
					{
						// Check for FRAMEID_NULL. min( , ) -_-;
						int action = pCreature->GetAction();
						if(action == ACTION_ADVANCEMENT_SLAYER_MAGIC_ATTACK || 
							action == ACTION_ADVANCEMENT_OUSTERS_MAGIC_ATTACK || 
							action == ACTION_ADVANCEMENT_MAGIC_ATTACK)
							action = ACTION_MAGIC;

						if( aest == ACTIONEFFECTSPRITETYPE_EXTREME_MALE ||
							aest == ACTIONEFFECTSPRITETYPE_EXTREME_FEMALE )
						{
							if( GET_ACTION_EFFECTSPRITETYPE_FRAMEID( aest, action ) == EFFECTSPRITETYPE_NULL ||
								pCreature->GetCreatureType() == CREATURETYPE_BAT ||
								pCreature->GetCreatureType() == CREATURETYPE_WOLF ||
								pCreature->GetCreatureType() == CREATURETYPE_WER_WOLF||
								pCreature->IsAdvancementClass())
							{
								iEffect++;
								continue;
							}
						}

						int actionFrameID = GET_ACTION_EFFECTSPRITETYPE_FRAMEID( aest, action );

						direction = pCreature->GetDirection();

						if (actionFrameID!=FRAMEID_NULL
							)//&& actionFrameID!=frameID)
						{
							frameID = actionFrameID;

							int actionCount = ((action==ACTION_MOVE || action==ACTION_SLAYER_MOTOR_MOVE)? pCreature->GetMoveCount() : pCreature->GetActionCount());
							frame = max( 0, min( actionCount, pEffect->GetMaxFrame()-1 ) );

						}
					}

					// It is hardcoded for now, but if a feature for frame synchronization
					// between the effect sprite type and the motion frames is added later, the hardcoding can be avoided.
					// If the synchronization feature is added, it should be possible to set the motions in which the effect attaches to the character..

					bool		bLarSlash = est >= EFFECTSPRITETYPE_LAR_SLASH_MALE_FAST && est <= EFFECTSPRITETYPE_LAR_SLASH_FEMALE_SLOW;
					bool		bRediance = est >= EFFECTSPRITETYPE_REDIANCE_MALE_FAST && est <= EFFECTSPRITETYPE_REDIANCE_FEMALE_NORMAL_ATTACK_SLOW;

					if( bRediance || bLarSlash )
					{						
						if( (bRediance && (pCreature->GetAction() == ACTION_SLAYER_SWORD ||  pCreature->GetAction() == ACTION_SLAYER_SWORD_FAST || pCreature->GetAction() == ACTION_SLAYER_SWORD_SLOW)) ||
							pCreature->GetAction() == ACTION_SLAYER_SWORD_2 || pCreature->GetAction() == ACTION_SLAYER_SWORD_2_SLOW || pCreature->GetAction() == ACTION_SLAYER_SWORD_2_FAST )
						{
							frame = max(0,min(pCreature->GetActionCount(),pEffect->GetMaxFrame()-1) );
						}
						else
						{
							// For other motions, just skip them sensibly.
							iEffect++;
							continue;
						}
					}

					EFFECTFRAME_ARRAY &EFA = m_EffectAlphaFPK[frameID][direction];

					frame = min( frame, EFA.GetSize()-1 );

					CEffectFrame &Frame = EFA[frame];

					bool bFrameBackground = Frame.IsBackground();
					AFFECT_ORBIT_EFFECT_BACKGROUND(pEffect, bFrameBackground);

					// Check when it gets drawn.
					if (HAS_PAIR_EFFECTSPRITETYPE(sest) && bBack == true )
					{
						const EFFECTSPRITETYPETABLE_INFO::FRAMEID_LIST& idList = (*g_pEffectSpriteTypeTable)[sest].PairFrameIDList;

						EFFECTSPRITETYPETABLE_INFO::FRAMEID_LIST::const_iterator iID = idList.begin();

						while (iID != idList.end())
						{
							int pairFrameID = *iID;	//(*g_pEffectSpriteTypeTable)[sest].PairFrameID;
							EFFECTFRAME_ARRAY& EA = m_EffectAlphaFPK[pairFrameID][direction];

							int tempFrame = frame % EA.GetSize();

							CEffectFrame &Frame = EA[tempFrame];

							bool bFrameBackground = Frame.IsBackground();
							AFFECT_ORBIT_EFFECT_BACKGROUND(pEffect, bFrameBackground);

							// Check when it gets drawn.
							if (type==0 
								|| (type==1 && !bFrameBackground)
								|| (type==2 && bFrameBackground))
							{
								int sprite = Frame.GetSpriteID();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetSpriteID();

								if (sprite!=SPRITEID_NULL)
								{
									// Frame coordinate correction
									pointTemp = *pPoint;
									pointTemp.x += Frame.GetCX();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetCX();
									pointTemp.y += Frame.GetCY();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetCY();

									AFFECT_ORBIT_EFFECT_POSITION( pEffect, pointTemp )

									DRAW_ALPHASPRITEPAL(&pointTemp, 
														sprite, 
														m_EffectAlphaSPK, 
														m_EffectAlphaPPK[pairFrameID])
//															m_EffectAlphaSPKI, 
//															m_EffectAlphaSPKFile)
									//-------------------------------------------------------
									// If H/W acceleration is available...
									//-------------------------------------------------------
									if (s_bUse3DLight)
									{
//							
//										DRAW_TEXTURE_SPRITEPAL_LOCKED(pointTemp.x, pointTemp.y, 
//																	sprite, m_pAlphaEffectTextureManager, pairFrameID ) //m_EffectAlphaPPK[pairFrameID])
//


										//------------------------------------------------
										// Add a LightFilter.
										//------------------------------------------------
										AddLightFilter3D( pPoint->x + 24, 
														pPoint->y + 24, 
														Frame.GetLight(),			// light brightness
														false);		// false = screen coordinates

									}
									//-------------------------------------------------------
									// If H/W acceleration is not available...
									//-------------------------------------------------------
									else
									{
//										DRAW_ALPHASPRITEPAL(&pointTemp, 
//															sprite, 
//															m_EffectAlphaSPK, 
//															m_EffectAlphaPPK[pairFrameID])
////															m_EffectAlphaSPKI, 
////															m_EffectAlphaSPKFile)

										AddLightFilter2D( pPoint->x + 24, 
													pPoint->y + 24, 
													Frame.GetLight(),			// light brightness
													false);		// false = screen coordinates
									}
								}
							}

							iID++;
						}						
					}

					// DEBUG: Log condition result before checking
					bool conditionMet = (type==0 || (type==1 && !bFrameBackground) || (type==2 && bFrameBackground));
					if (conditionMet)
					{
						int sprite = Frame.GetSpriteID();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetSpriteID();

						if (sprite!=SPRITEID_NULL)
						{
							// Frame coordinate correction
							pointTemp = *pPoint;
							pointTemp.x += Frame.GetCX();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetCX();
							pointTemp.y += Frame.GetCY();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetCY();

							AFFECT_ORBIT_EFFECT_POSITION( pEffect, pointTemp )

							// Call the actual draw function (expanded from macro for logging)
							{
								CAlphaSpritePal* pSprite = &m_EffectAlphaSPK[sprite];
								m_pSurface->BltAlphaSpritePal(&pointTemp, pSprite, m_EffectAlphaPPK[frameID]);
							}
//													m_EffectAlphaSPKI, 
//													m_EffectAlphaSPKFile)
							//-------------------------------------------------------
							// If H/W acceleration is available...
							//-------------------------------------------------------
							if (s_bUse3DLight)
							{
//					
//								DRAW_TEXTURE_SPRITEPAL_LOCKED(pointTemp.x, pointTemp.y, 
//															sprite, m_pAlphaEffectTextureManager, frameID ) //m_EffectAlphaPPK[frameID])
//


								//------------------------------------------------
								// Add a LightFilter.
								//------------------------------------------------
								AddLightFilter3D( pPoint->x + 24, 
												pPoint->y + 24, 
												Frame.GetLight(),			// light brightness
												false);		// false = screen coordinates

							}
							//-------------------------------------------------------
							// If H/W acceleration is not available...
							//-------------------------------------------------------
							else
							{
								DRAW_ALPHASPRITEPAL(&pointTemp, 
													sprite, 
													m_EffectAlphaSPK, 
													m_EffectAlphaPPK[frameID])
//													m_EffectAlphaSPKI, 
//													m_EffectAlphaSPKFile)

								AddLightFilter2D( pPoint->x + 24, 
											pPoint->y + 24, 
											Frame.GetLight(),			// light brightness
											false);		// false = screen coordinates
							}
						}
					}

					//--------------------------------------------------------
					// Pair Frame output
					//--------------------------------------------------------
//					sest = GET_EFFECTSPRITETYPE_EFFECT( frameID );

					if (HAS_PAIR_EFFECTSPRITETYPE(sest) && bBack == false )
					{
						const EFFECTSPRITETYPETABLE_INFO::FRAMEID_LIST& idList = (*g_pEffectSpriteTypeTable)[sest].PairFrameIDList;

						EFFECTSPRITETYPETABLE_INFO::FRAMEID_LIST::const_iterator iID = idList.begin();

						while (iID != idList.end())
						{
							int pairFrameID = *iID;	//(*g_pEffectSpriteTypeTable)[sest].PairFrameID;
							EFFECTFRAME_ARRAY& EA = m_EffectAlphaFPK[pairFrameID][direction];

							int tempFrame = frame % EA.GetSize();

							CEffectFrame &Frame = EA[tempFrame];

							bool bFrameBackground = Frame.IsBackground();
							AFFECT_ORBIT_EFFECT_BACKGROUND(pEffect, bFrameBackground);

							// Check when it gets drawn.
							if (type==0 
								|| (type==1 && !bFrameBackground)
								|| (type==2 && bFrameBackground))
							{
								int sprite = Frame.GetSpriteID();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetSpriteID();

								if (sprite!=SPRITEID_NULL)
								{
									// Frame coordinate correction
									pointTemp = *pPoint;
									pointTemp.x += Frame.GetCX();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetCX();
									pointTemp.y += Frame.GetCY();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetCY();

									AFFECT_ORBIT_EFFECT_POSITION( pEffect, pointTemp )

									DRAW_ALPHASPRITEPAL(&pointTemp, 
														sprite, 
														m_EffectAlphaSPK, 
														m_EffectAlphaPPK[pairFrameID])
//															m_EffectAlphaSPKI, 
//															m_EffectAlphaSPKFile)
									//-------------------------------------------------------
									// If H/W acceleration is available...
									//-------------------------------------------------------
									if (s_bUse3DLight)
									{
//							
//										DRAW_TEXTURE_SPRITEPAL_LOCKED(pointTemp.x, pointTemp.y, 
//																	sprite, m_pAlphaEffectTextureManager, pairFrameID ) //m_EffectAlphaPPK[pairFrameID])
//


										//------------------------------------------------
										// Add a LightFilter.
										//------------------------------------------------
										AddLightFilter3D( pPoint->x + 24, 
														pPoint->y + 24, 
														Frame.GetLight(),			// light brightness
														false);		// false = screen coordinates

									}
									//-------------------------------------------------------
									// If H/W acceleration is not available...
									//-------------------------------------------------------
									else
									{
//										DRAW_ALPHASPRITEPAL(&pointTemp, 
//															sprite, 
//															m_EffectAlphaSPK, 
//															m_EffectAlphaPPK[pairFrameID])
////															m_EffectAlphaSPKI, 
////															m_EffectAlphaSPKFile)

										AddLightFilter2D( pPoint->x + 24, 
													pPoint->y + 24, 
													Frame.GetLight(),			// light brightness
													false);		// false = screen coordinates
									}
								}
							}

							iID++;
						}						
					}
				}
				break;			

				//--------------------------------------------------------
				// SCREEN
				//--------------------------------------------------------
				case BLT_SCREEN :
				{
					// FIX: Use the saved table index from effect creation time
					// instead of recalculating from FrameID using macro
					int est;
					if (pEffect->GetEffectType() == MEffect::EFFECT_ATTACH) {
						// MAttachEffect saves the table index in m_EffectSpriteType
						est = ((MAttachEffect*)pEffect)->GetEffectSpriteType();
					} else {
						// For other effect types, fall back to macro (may need fixing too)
						est = GET_EFFECTSPRITETYPE_SCREEN(frameID);
					}

					// Bounds check
					if (g_pEffectSpriteTypeTable != NULL) {
						int tableSize = g_pEffectSpriteTypeTable->GetSize();
						if (est < 0 || est >= tableSize) {
							LOG_ERROR("[Effect BLT_SCREEN] est=%d out of bounds (tableSize=%d), frameID=%d, frame=%d, dir=%d",
							         est, tableSize, frameID, frame, direction);
							break;
						}
					}


					// Picks the effectFrameID that matches the action.
					int aest = GET_ACTION_EFFECTSPRITETYPE(est);
					if (aest!=FRAMEID_NULL)
					{
						int action = pCreature->GetAction();
						if(action == ACTION_ADVANCEMENT_SLAYER_MAGIC_ATTACK || 
							action == ACTION_ADVANCEMENT_OUSTERS_MAGIC_ATTACK || 
							action == ACTION_ADVANCEMENT_MAGIC_ATTACK)
							action = ACTION_MAGIC;

						int actionFrameID = GET_ACTION_EFFECTSPRITETYPE_FRAMEID( aest, action );

						direction = pCreature->GetDirection();

						if (actionFrameID!=FRAMEID_NULL
							&& actionFrameID!=frameID)
						{
							frameID = actionFrameID;

							int actionCount = ((action==ACTION_MOVE || action==ACTION_SLAYER_MOTOR_MOVE)? pCreature->GetMoveCount() : pCreature->GetActionCount());
							frame = max( 0, min( actionCount, pEffect->GetMaxFrame()-1 ) );
						}
					}

					EFFECTFRAME_ARRAY &EFA = m_EffectScreenFPK[frameID][direction];
					frame = min( frame, EFA.GetSize()-1 );

					CEffectFrame &Frame = EFA[frame];

					bool bFrameBackground = Frame.IsBackground();
					AFFECT_ORBIT_EFFECT_BACKGROUND(pEffect, bFrameBackground);

					// Check when it gets drawn.
					if (type==0 
						|| (type==1 && !bFrameBackground)
						|| (type==2 && bFrameBackground))
					{
						int sprite = Frame.GetSpriteID();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetSpriteID();

						if (sprite!=SPRITEID_NULL)
						{
							// Frame coordinate correction
							pointTemp = *pPoint;
							pointTemp.x += Frame.GetCX();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetCX();
							pointTemp.y += Frame.GetCY();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetCY();

							AFFECT_ORBIT_EFFECT_POSITION( pEffect, pointTemp )							

							DRAW_NORMALSPRITEPAL_EFFECT(&pointTemp, 
												sprite, 
												m_EffectScreenSPK, 
												m_EffectScreenPPK[frameID],
//													m_EffectScreenSPKI, 
//													m_EffectScreenSPKFile,
												CSpriteSurface::EFFECT_SCREEN)
							//-------------------------------------------------------
							// If H/W acceleration is available...
							//-------------------------------------------------------
							if (s_bUse3DLight)
							{
//
//					
//								DRAW_TEXTURE_SPRITEPAL_LOCKED(pointTemp.x, pointTemp.y, 
//															sprite, m_pScreenEffectTextureManager, frameID ) // m_EffectScreenPPK[frameID])
//

								//------------------------------------------------
								// Add a LightFilter.
								//------------------------------------------------
								AddLightFilter3D( pPoint->x + 24, 
												pPoint->y + 24, 
												Frame.GetLight(),			// light brightness
												false);		// false = screen coordinates

							}
							//-------------------------------------------------------
							// If H/W acceleration is not available...
							//-------------------------------------------------------
							else
							{
								// The sprite was already drawn above, unconditionally,
								// and s_bUse3DLight is a constant false, so a draw here
								// screen-blended the same pixels twice. Only the light
								// filter belongs on this branch, as at the frame draw
								// in DrawEffect and the first site in this function.

								AddLightFilter2D( pPoint->x + 24, 
											pPoint->y + 24, 
											Frame.GetLight(),			// light brightness
											false);		// false = screen coordinates
							}
						}
					}

					//--------------------------------------------------------
					// Pair Frame output
					//--------------------------------------------------------

					if (HAS_PAIR_EFFECTSPRITETYPE(est))
					{
						const EFFECTSPRITETYPETABLE_INFO::FRAMEID_LIST& idList = (*g_pEffectSpriteTypeTable)[est].PairFrameIDList;

						EFFECTSPRITETYPETABLE_INFO::FRAMEID_LIST::const_iterator iID = idList.begin();

						while (iID != idList.end())
						{
							int pairFrameID = *iID;//(*g_pEffectSpriteTypeTable)[*iID].PairFrameID;

							EFFECTFRAME_ARRAY& EA = m_EffectScreenFPK[pairFrameID][direction];

							int tempFrame = frame % EA.GetSize();

							CEffectFrame &Frame = EA[tempFrame];

							bool bFrameBackground = Frame.IsBackground();
							AFFECT_ORBIT_EFFECT_BACKGROUND(pEffect, bFrameBackground);

							// Check when it gets drawn.
							if (type==0 
								|| (type==1 && !bFrameBackground)
								|| (type==2 && bFrameBackground))
							{
								int sprite = Frame.GetSpriteID();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetSpriteID();

								if (sprite!=SPRITEID_NULL)
								{
									// Frame coordinate correction
									pointTemp = *pPoint;
									pointTemp.x += Frame.GetCX();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetCX();
									pointTemp.y += Frame.GetCY();	//m_EffectAlphaFPK[(*iEffect)->GetFrameID()][direction][(*iEffect)->GetFrame()].GetCY();

									AFFECT_ORBIT_EFFECT_POSITION( pEffect, pointTemp )

									DRAW_NORMALSPRITEPAL_EFFECT(&pointTemp, 
														sprite, 
														m_EffectScreenSPK, 
														m_EffectScreenPPK[pairFrameID],
//															m_EffectScreenSPKI, 
//															m_EffectScreenSPKFile,
														CSpriteSurface::EFFECT_SCREEN)
									//-------------------------------------------------------
									// If H/W acceleration is available...
									//-------------------------------------------------------
									if (s_bUse3DLight)
									{
//
//							
//										DRAW_TEXTURE_SPRITEPAL_LOCKED(pointTemp.x, pointTemp.y, 
//																	sprite, m_pScreenEffectTextureManager, pairFrameID )//m_EffectScreenPPK[pairFrameID])
//

										//------------------------------------------------
										// Add a LightFilter.
										//------------------------------------------------
										AddLightFilter3D( pPoint->x + 24, 
														pPoint->y + 24, 
														Frame.GetLight(),			// light brightness
														false);		// false = screen coordinates

									}
									//-------------------------------------------------------
									// If H/W acceleration is not available...
									//-------------------------------------------------------
									else
									{
										// Same as the frame above: drawn already, only the
										// light filter belongs here.
										AddLightFilter2D( pPoint->x + 24,
													pPoint->y + 24, 
													Frame.GetLight(),			// light brightness
													false);		// false = screen coordinates
									}
								}
							}

							iID++;
						}
					}
				}
				break;
			}
		}

		// Next Effect
		iEffect++;
	}	

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD( "End DrawAttachEffect" );
	#endif


}

//----------------------------------------------------------------
// Draw the Minimap of m_pZone at (x,y).
//----------------------------------------------------------------
// Test Function
//----------------------------------------------------------------
void
MTopView::DrawMinimap(int x, int y, BYTE scale)
{
	(void)x;
	(void)y;
	(void)scale;

	//------------------------------------------------
	// Not drawn for a vampire.
	//------------------------------------------------

	//------------------------------------------------
	// 3D acceleration
	//------------------------------------------------
//	
//			// Set Texture
//			// CDirect3D::GetDevice()->SetTexture() removed (SDL2)

//
//
//			m_TextureEffect.DrawEffect2D( &rect );		
//			
//			// CDirect3D::GetDevice()->EndScene() removed (SDL2)

//
//			// CDirect3D::GetDevice()->SetTexture() removed (SDL2)

//
//
//			//------------------------------------------------
//			// Plot a dot at the Player position
//			//------------------------------------------------
////			if (!m_pSurface->Lock())
////				return;
////
////			POINT point = { x, y };
////
////			WORD*	pSurface	= (WORD*)m_pSurface->GetSurfacePointer();
////			long	lPitch		= m_pSurface->GetSurfacePitch();
////
////			// Player's coordinates on the Minimap
////			point.x = x + g_pPlayer->GetX()*m_SectorToMinimapWidth;
////			point.y = y + g_pPlayer->GetY()*m_SectorToMinimapHeight;
////
////			// Plot a dot
////			WORD*	pSurfaceTemp;	
////			pSurfaceTemp = (WORD*)((BYTE*)pSurface + lPitch*point.y + (point.x<<1));
////			*pSurfaceTemp++ = 0xFFFF;
////			*pSurfaceTemp = 0xFFFF;
////			pSurfaceTemp = (WORD*)((BYTE*)pSurface + lPitch*(point.y+1) + (point.x<<1));
////			*pSurfaceTemp++ = 0xFFFF;
////			*pSurfaceTemp = 0xFFFF;
////
//	//------------------------------------------------
//	// 2D
//	//------------------------------------------------
//
//			POINT point = { x, y };
//			m_pSurface->BltSpriteHalf(&point, m_pMinimapSPR);
//
//			//------------------------------------------------
//			// Plot a dot at the Player position
//			//------------------------------------------------	
////			WORD*	pSurface	= (WORD*)m_pSurface->GetSurfacePointer();
////			long	lPitch		= m_pSurface->GetSurfacePitch();
////
////			// Player's coordinates on the Minimap
////			point.x = x + g_pPlayer->GetX()*m_SectorToMinimapWidth;
////			point.y = y + g_pPlayer->GetY()*m_SectorToMinimapHeight;
////
////			// Plot a dot
////			WORD*	pSurfaceTemp;	
////			pSurfaceTemp = (WORD*)((BYTE*)pSurface + lPitch*point.y + (point.x<<1));
////			*pSurfaceTemp++ = 0xFFFF;
////			*pSurfaceTemp = 0xFFFF;
////			pSurfaceTemp = (WORD*)((BYTE*)pSurface + lPitch*(point.y+1) + (point.x<<1));
////			*pSurfaceTemp++ = 0xFFFF;
////			*pSurfaceTemp = 0xFFFF;
////


	/*
	// Picture output
	m_pSurface->Lock();

	POINT point = { x, y };

	// Minimap output
	m_pSurface->BltSprite( &point, m_pMinimapSPR );

	int width = m_pMinimapSPR->GetWidth();
	int height = m_pMinimapSPR->GetHeight();

	WORD*	pSurface	= (WORD*)m_pSurface->GetSurfacePointer();
	long	lPitch		= m_pSurface->GetSurfacePitch();

	// Player's coordinates on the Minimap
	point.x = x + g_pPlayer->GetX()*m_SectorToMinimapWidth;
	point.y = y + g_pPlayer->GetY()*m_SectorToMinimapHeight;

	// Plot a dot
	WORD*	pSurfaceTemp;	
	pSurfaceTemp = (WORD*)((BYTE*)pSurface + lPitch*point.y + (point.x<<1));
	*pSurfaceTemp++ = 0xFFFF;
	*pSurfaceTemp = 0xFFFF;
	pSurfaceTemp = (WORD*)((BYTE*)pSurface + lPitch*(point.y,1) + (point.x<<1));
	*pSurfaceTemp++ = 0xFFFF;
	*pSurfaceTemp = 0xFFFF;

	m_pSurface->Unlock();
	*/

	/*
	WORD	*lpSurface, 
			*lpSurfaceTemp;

	WORD	pitch;

	WORD	color = 0;

	m_pSurface->LockW(lpSurface, pitch);

	lpSurface = (WORD*)((BYTE*)lpSurface + y*pitch + (x<<1));
	//lpSurface += (m_pZone->GetHeight()-1)<<scale;

	TYPE_SECTORPOSITION		pX = g_pPlayer->GetX(),
							pY = g_pPlayer->GetY();

	register int i,j;

	// Draw Minimap
	for (i=0; i<m_pZone->GetHeight(); i++)
	{
		lpSurfaceTemp = lpSurface;

		for (j=0; j<m_pZone->GetWidth(); j++)
		{			
			// player position
			if (pX==j && pY==i)
			{
				color = 0xFFFF;
			}			
			else
			{				
				// by tile color..


				 const MSector &sector = m_pZone->GetSector(j,i);

				// picture
				if (sector.IsExistImageObject() && sector.IsBlockAny())
				{
					color = 0x07E0;	// G
				}
				// Creature
				else if (sector.IsExistAnyCreature())
				{
					color = 0xF000;	// R
				}
				// Item
				else if (sector.IsExistItem())
				{
					color = 0x001F;	// B
				}				
				else 
				{
					color = 0;
					//lpSurfaceTemp = (WORD*)((BYTE*)lpSurfaceTemp + pitch*scale + scale*4);
					//continue;
				}
			}

			*lpSurfaceTemp = color;

			// next dot
			lpSurfaceTemp = (WORD*)((BYTE*)lpSurfaceTemp + scale*4);
		}

		// Next row
		lpSurface = (WORD*)((BYTE*)lpSurface + pitch*scale);
	}

	m_pSurface->Unlock();
	*/
}

//----------------------------------------------------------------
// Draw Item Broken
//----------------------------------------------------------------
void	
MTopView::DrawItemBroken(int x, int y)
{	
//	return;
	/*
		The -10 on MAX_GEAR subtracts the number of core zaps + blood bibles..^^;
	*/
	// Draw the item about to break
	MPlayerGear* pGear = NULL;
	int spriteID;
	int frameType;
	int toSomewhatBroken;
	int toAlmostBroken;

	//----------------------------------------------------------------
	// Slayer
	//----------------------------------------------------------------
	switch(g_pPlayer->GetRace())
	{
	case RACE_SLAYER:
	{
		if (g_pPlayer->IsMale())
		{
			frameType = 0;
			spriteID = SPRITEID_ITEM_BROKEN_SLAYER_MALE;		
		}
		else
		{
			frameType = 1;
			spriteID = SPRITEID_ITEM_BROKEN_SLAYER_FEMALE;		
		}

		pGear = g_pSlayerGear;
		toSomewhatBroken = MSlayerGear::MAX_GEAR_SLAYER + 2-12;		
		toAlmostBroken = toSomewhatBroken << 1;
	}
	break;

	case RACE_VAMPIRE:
	{
		if (g_pPlayer->IsMale())
		{
			frameType = 2;
			spriteID = SPRITEID_ITEM_BROKEN_VAMPIRE_MALE;		
		}
		else
		{
			frameType = 3;
			spriteID = SPRITEID_ITEM_BROKEN_VAMPIRE_FEMALE;		
		}

		pGear = g_pVampireGear;
		toSomewhatBroken = MVampireGear::MAX_GEAR_VAMPIRE-12;
		toAlmostBroken = toSomewhatBroken << 1;
	}
	break;

	case RACE_OUSTERS:
		{
			spriteID = SPRITEID_ITEM_BROKEN_OUSTERS;
			frameType = 4;
			pGear = g_pOustersGear;
			toSomewhatBroken = MOustersGear::MAX_GEAR_OUSTERS + 1-12;
			toAlmostBroken = toSomewhatBroken << 1;
		}
		break;
	default:
		return;	// no gear for a non-player race (RACE_MAX)
	}

	//----------------------------------------------------------------
	// If there is an item about to break, draw it.
	//----------------------------------------------------------------
	if (pGear->HasBrokenItem() )
	{
		//---------------------------------------------
		// Lock
		//---------------------------------------------
		m_pSurface->Lock();

		//---------------------------------------------
		// Draw the basic shell (-_-;)
		//---------------------------------------------
		POINT pointBasis = { x+15, y };
		POINT point = { pointBasis.x, pointBasis.y };

		m_pSurface->BltSprite( &point, &m_ItemBrokenSPK[spriteID] );


		int size = pGear->GetSize();

		//---------------------------------------------
		// Check every slot of the Gear
		//---------------------------------------------
		for (int i=0; i<size-4-6; i++)
		{			
			const MItem* pItem = pGear->GetItem( (BYTE)i );	

			//---------------------------------------------
			// Draw only when something is being worn.
			//---------------------------------------------
			if (pItem!=NULL)
			{			
				MPlayerGear::ITEM_STATUS itemStatus = pGear->GetItemStatus( i );

				//---------------------------------------------
				// For a weapon.. the picture differs by type.
				//---------------------------------------------
				int frameID;

				if (g_pPlayer->IsSlayer())
				{
					//---------------------------------------------
					// It is the left hand, but.
					//---------------------------------------------
					if (i==MSlayerGear::GEAR_SLAYER_LEFTHAND)
					{
						// Draw if it is a shield
						if (pItem->GetItemClass()==ITEM_CLASS_SHIELD)
						{
							frameID = i;
						}
						// If it is not a shield.. do not draw.
						else
						{
							continue;
						}
					}
					//---------------------------------------------
					// The right hand is drawn depending on the weapon
					//---------------------------------------------
					else if (i==MSlayerGear::GEAR_SLAYER_RIGHTHAND)
					{				
						// gun?
						if (pItem->IsGunItem())
						{
							frameID = MSlayerGear::MAX_GEAR_SLAYER + 1 - 12;
						}
						// cross?
						else if (pItem->GetItemClass()==ITEM_CLASS_CROSS
								|| pItem->GetItemClass()==ITEM_CLASS_MACE)
						{
							frameID = MSlayerGear::MAX_GEAR_SLAYER -12;
						}
						// Otherwise.. a sword or a blade
						else
						{
							frameID = i;
						}
					}
					else
					{
						frameID = i;
					}
				}
				else if (g_pPlayer->IsOusters() )
				{
					if( i == MOustersGear::GEAR_OUSTERS_RIGHTHAND )
					{
						if( pItem->GetItemClass() == ITEM_CLASS_OUSTERS_WRISTLET )
							frameID = MOustersGear::MAX_GEAR_OUSTERS -12;
						else
							frameID = i;
					} else
					{
						frameID = i;
					}
				} else
				{
					frameID = i;
				}

				//---------------------------------------------
				// If normal, draw the normal one..
				//---------------------------------------------
				if (itemStatus==MPlayerGear::ITEM_STATUS_OK || pItem->GetItemClass() == ITEM_CLASS_COUPLE_RING ||
					pItem->GetItemClass() == ITEM_CLASS_VAMPIRE_COUPLE_RING)
				{
					CFrame& frame = m_ItemBrokenFPK[frameType][frameID];

					spriteID = frame.GetSpriteID();

					if (spriteID!=SPRITEID_NULL)
					{
						point.x = pointBasis.x + frame.GetCX();
						point.y = pointBasis.y + frame.GetCY();

						m_pSurface->BltSprite( &point, &m_ItemBrokenSPK[spriteID] );
					}
				}
				//---------------------------------------------
				// If it is breaking.. draw it with color~..
				//---------------------------------------------
				else
				{
					int plusFrameID = ((itemStatus==MPlayerGear::ITEM_STATUS_SOMEWHAT_BROKEN)? toSomewhatBroken : toAlmostBroken);

					CFrame& frame = m_ItemBrokenFPK[frameType][frameID + plusFrameID];

					spriteID = frame.GetSpriteID();

					if (spriteID!=SPRITEID_NULL)
					{
						point.x = pointBasis.x + frame.GetCX();
						point.y = pointBasis.y + frame.GetCY();

						m_pSurface->BltSprite( &point, &m_ItemBrokenSPK[spriteID] );
					}
				}
			}
		}

		//---------------------------------------------
		// Unlock
		//---------------------------------------------
		m_pSurface->Unlock();
	}
}

//----------------------------------------------------------------
// Get MaxEffectFrame
//----------------------------------------------------------------
int				
MTopView::GetMaxEffectFrame(BLT_TYPE bltType, TYPE_FRAMEID frameID) const
{
	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD_FORMAT("GetMaxEffectFrame(%d, %d)", bltType, frameID);
	#endif

	int numFrame = 0;

	// Check bounds to prevent crash (SDL backend defensive fix)
	switch (bltType)
	{
		case BLT_SCREEN :
			if (frameID >= m_EffectScreenFPK.GetSize()) return 0;
			numFrame = m_EffectScreenFPK[frameID][0].GetSize();
		break;

		case BLT_EFFECT :
			if (frameID >= m_EffectAlphaFPK.GetSize()) return 0;
			numFrame = m_EffectAlphaFPK[frameID][0].GetSize();
		break;

		case BLT_NORMAL :
			if (frameID >= m_EffectNormalFPK.GetSize()) return 0;
			numFrame = m_EffectNormalFPK[frameID][0].GetSize();
		break;

		case BLT_SHADOW :
			if (frameID >= m_EffectShadowFPK.GetSize()) return 0;
			numFrame = m_EffectShadowFPK[frameID][0].GetSize();
		break;
	}

	#ifdef OUTPUT_DEBUG_DRAW_PROCESS
		DEBUG_ADD_FORMAT("numFrame = %d", numFrame);
	#endif

	return numFrame;
}

//----------------------------------------------------------------
// Get EffectLight
//----------------------------------------------------------------
int
MTopView::GetEffectLight(BLT_TYPE bltType, TYPE_FRAMEID frameID, int dir, int frame) const
{
	// Defensive check for SDL backend
	switch (bltType)
	{
		case BLT_SCREEN :
			if (frameID >= m_EffectScreenFPK.GetSize()) return 0;
			if (dir >= m_EffectScreenFPK[frameID].GetSize()) return 0;
			if (frame >= m_EffectScreenFPK[frameID][dir].GetSize()) return 0;
			return m_EffectScreenFPK[frameID][dir][frame].GetLight();

		case BLT_EFFECT :
			if (frameID >= m_EffectAlphaFPK.GetSize()) return 0;
			if (dir >= m_EffectAlphaFPK[frameID].GetSize()) return 0;
			if (frame >= m_EffectAlphaFPK[frameID][dir].GetSize()) return 0;
			return m_EffectAlphaFPK[frameID][dir][frame].GetLight();

		case BLT_NORMAL :
			if (frameID >= m_EffectNormalFPK.GetSize()) return 0;
			if (dir >= m_EffectNormalFPK[frameID].GetSize()) return 0;
			if (frame >= m_EffectNormalFPK[frameID][dir].GetSize()) return 0;
			return m_EffectNormalFPK[frameID][dir][frame].GetLight();

		case BLT_SHADOW :
			if (frameID >= m_EffectShadowFPK.GetSize()) return 0;
			if (dir >= m_EffectShadowFPK[frameID].GetSize()) return 0;
			if (frame >= m_EffectShadowFPK[frameID][dir].GetSize()) return 0;
			return m_EffectShadowFPK[frameID][dir][frame].GetLight();
	}

	return 0;
}

//----------------------------------------------------------------
// Get EffectSpriteType
//----------------------------------------------------------------
int				
MTopView::GetEffectSpriteType(BLT_TYPE bltType, TYPE_FRAMEID frameID) const
{
	switch (bltType)
	{
		case BLT_SCREEN :
			return frameID + MAX_EFFECTSPRITETYPE_ALPHAEFFECT;

		case BLT_EFFECT :
			return frameID;

		case BLT_NORMAL :
			return frameID + MAX_EFFECTSPRITETYPE_SCREENEFFECT;

		case BLT_SHADOW :
			return frameID + MAX_EFFECTSPRITETYPE_NORMALEFFECT;
	}

	return 0;
}

//----------------------------------------------------------------
// DrawEffect ( point, MEffect* )
//----------------------------------------------------------------
// Assumed to be called in the m_pSurface->IsLock() state.
//----------------------------------------------------------------
void
MTopView::DrawEffect(POINT* pPoint, MEffect* pEffect, bool bSelectable)
{
	if(g_pEventManager->GetEventByFlag(EVENTFLAG_NOT_DRAW_EFFECT))
		return;

	if (!pEffect || !pPoint) {
		return;
	}

	POINT point = *pPoint;

	switch (pEffect->GetBltType())
	{
		//------------------------------------------------------------------
		//
		//						Normal Effect
		//
		//------------------------------------------------------------------
		case BLT_NORMAL :
		{
			TYPE_FRAMEID frameID = pEffect->GetFrameID();
			BYTE direction = pEffect->GetDirection();
			BYTE frame = pEffect->GetFrame();

			// Boundary check
			if (frameID >= m_EffectNormalFPK.GetSize()) {
#ifdef OUTPUT_DEBUG
				DEBUG_ADD_FORMAT_WAR("DrawEffect(BLT_NORMAL): Invalid frameID=%d", frameID);
#endif
				pEffect->ClearScreenRect();
				break;
			}

			CFrame& Frame = m_EffectNormalFPK[frameID][direction][frame];
			int spriteID = Frame.GetSpriteID();

			if (spriteID != SPRITEID_NULL)
			{
				// Boundary check for sprite
				if (spriteID < 0 || static_cast<DWORD>(spriteID) >= m_EffectNormalSPK.GetSize()) {
#ifdef OUTPUT_DEBUG
					DEBUG_ADD_FORMAT_WAR("DrawEffect(BLT_NORMAL): Invalid spriteID=%d", spriteID);
#endif
					pEffect->ClearScreenRect();
					break;
				}

				CSprite* pSprite = &m_EffectNormalSPK[spriteID];

				point.x += Frame.GetCX();
				point.y += Frame.GetCY();

				m_pSurface->BltSprite(&point, pSprite);

				//----------------------------------------
				// Set selectable area
				//----------------------------------------
				if (bSelectable)
				{
					RECT rect;
					rect.left	= point.x;
					rect.top	= point.y;
					rect.right	= rect.left + pSprite->GetWidth();
					rect.bottom = rect.top + pSprite->GetHeight();

					pEffect->SetScreenRect(&rect);
				}
			}
			else
			{
				pEffect->ClearScreenRect();
			}
		}
		break;

		//------------------------------------------------------------------
		//
		//						Alpha Effect
		//
		//------------------------------------------------------------------
		case BLT_EFFECT :
		{
			TYPE_FRAMEID frameID = pEffect->GetFrameID();
			BYTE direction = pEffect->GetDirection();
			BYTE frame = pEffect->GetFrame();

			// Boundary check
			if (frameID >= m_EffectAlphaFPK.GetSize()) {
#ifdef OUTPUT_DEBUG
				DEBUG_ADD_FORMAT_WAR("DrawEffect(BLT_EFFECT): Invalid frameID=%d", frameID);
#endif
				break;
			}

			CEffectFrame& Frame = m_EffectAlphaFPK[frameID][direction][frame];
			int spriteID = Frame.GetSpriteID();

			if (spriteID != SPRITEID_NULL)
			{
				// Boundary check for palette
				if (frameID >= m_EffectAlphaPPK.GetSize()) {
#ifdef OUTPUT_DEBUG
					DEBUG_ADD_FORMAT_WAR("DrawEffect(BLT_EFFECT): Invalid palette frameID=%d", frameID);
#endif
					break;
				}

				// Boundary check for sprite
				if (spriteID < 0 || static_cast<DWORD>(spriteID) >= m_EffectAlphaSPK.GetSize()) {
#ifdef OUTPUT_DEBUG
					DEBUG_ADD_FORMAT_WAR("DrawEffect(BLT_EFFECT): Invalid spriteID=%d", spriteID);
#endif
					break;
				}

				point.x += Frame.GetCX();
				point.y += Frame.GetCY();

				DRAW_ALPHASPRITEPAL(&point, spriteID, m_EffectAlphaSPK, m_EffectAlphaPPK[frameID])

				//----------------------------------------
				// Set selectable area
				//----------------------------------------
				if (bSelectable)
				{
					CAlphaSpritePal* pSprite = &m_EffectAlphaSPK[spriteID];

					RECT rect;
					rect.left	= point.x;
					rect.top	= point.y;
					rect.right	= rect.left + pSprite->GetWidth();
					rect.bottom = rect.top + pSprite->GetHeight();

					pEffect->SetScreenRect(&rect);
				}

				//-------------------------------------------------------
				// Hardware acceleration enabled
				//-------------------------------------------------------
				if (s_bUse3DLight)
				{
//					
//					DRAW_TEXTURE_SPRITEPAL_LOCKED(point.x, point.y, spriteID, m_pAlphaEffectTextureManager, pEffect->GetFrameID() ) //m_EffectAlphaPPK[pEffect->GetFrameID()])
//

					//---------------------------------------- 		
					// Set the effect selection rectangle area
					//---------------------------------------- 	
//
//						pEffect->SetScreenRect( &rect );							
//					}

					//------------------------------------------------
					// Add a LightFilter.
					//------------------------------------------------
					AddLightFilter3D( pPoint->x + 24, 
									pPoint->y + 24, 
									Frame.GetLight(),			// light brightness
									false);	// false = screen coordinates

				}
				//-------------------------------------------------------
				// If H/W acceleration is not available...
				//-------------------------------------------------------
				else
				{
//					DRAW_ALPHASPRITEPAL(&point, spriteID, m_EffectAlphaSPK, m_EffectAlphaPPK[pEffect->GetFrameID()])//, m_EffectAlphaSPKI, m_EffectAlphaSPKFile)

//					//---------------------------------------- 		
//					// Set the effect selection rectangle area
//					//---------------------------------------- 	
//					if (bSelectable)
//					{
//						CAlphaSpritePal* pSprite = &m_EffectAlphaSPK[ spriteID ];
//
//
//						pEffect->SetScreenRect( &rect );
//					}


					AddLightFilter2D( pPoint->x + 24, 
									pPoint->y + 24, 
									Frame.GetLight(),			// light brightness
									false);		// false = screen coordinates
				}
			}
		}
		break;

		//------------------------------------------------------------------
		//
		//						Screen Effect
		//
		//------------------------------------------------------------------
		case BLT_SCREEN :
		{
			TYPE_FRAMEID frameID = pEffect->GetFrameID();
			BYTE direction = pEffect->GetDirection();
			BYTE frame = pEffect->GetFrame();

			// Boundary check
			if (frameID >= m_EffectScreenFPK.GetSize()) {
#ifdef OUTPUT_DEBUG
				DEBUG_ADD_FORMAT_WAR("DrawEffect(BLT_SCREEN): Invalid frameID=%d", frameID);
#endif
				break;
			}

			CEffectFrame& Frame = m_EffectScreenFPK[frameID][direction][frame];
			int spriteID = Frame.GetSpriteID();

			if (spriteID != SPRITEID_NULL)
			{
				// Boundary check for palette
				if (frameID >= m_EffectScreenPPK.GetSize()) {
#ifdef OUTPUT_DEBUG
					DEBUG_ADD_FORMAT_WAR("DrawEffect(BLT_SCREEN): Invalid palette frameID=%d", frameID);
#endif
					break;
				}

				// Boundary check for sprite
				if (spriteID < 0 || static_cast<DWORD>(spriteID) >= m_EffectScreenSPK.GetSize()) {
#ifdef OUTPUT_DEBUG
					DEBUG_ADD_FORMAT_WAR("DrawEffect(BLT_SCREEN): Invalid spriteID=%d", spriteID);
#endif
					break;
				}

				point.x += Frame.GetCX();
				point.y += Frame.GetCY();

				DRAW_NORMALSPRITEPAL_EFFECT(&point,
									spriteID,
									m_EffectScreenSPK,
									m_EffectScreenPPK[frameID],
									CSpriteSurface::EFFECT_SCREEN)

				//----------------------------------------
				// Set selectable area
				//----------------------------------------
				if (bSelectable)
				{
					CSpritePal* pSprite = &m_EffectScreenSPK[spriteID];

					RECT rect;
					rect.left	= point.x;
					rect.top	= point.y;
					rect.right	= rect.left + pSprite->GetWidth();
					rect.bottom = rect.top + pSprite->GetHeight();

					pEffect->SetScreenRect(&rect);
				}

				//-------------------------------------------------------
				// Hardware acceleration enabled
				//-------------------------------------------------------
				if (s_bUse3DLight)
				{
//
//					
//					DRAW_TEXTURE_SPRITEPAL_LOCKED(point.x, point.y, spriteID, m_pScreenEffectTextureManager, pEffect->GetFrameID() ) //m_EffectScreenPPK[pEffect->GetFrameID()])
//

					//---------------------------------------- 		
					// Set the effect selection rectangle area
					//---------------------------------------- 	
//
//						pEffect->SetScreenRect( &rect );							
//					}

					//------------------------------------------------
					// Add a LightFilter.
					//------------------------------------------------
					AddLightFilter3D( pPoint->x + 24, 
									pPoint->y + 24, 
									Frame.GetLight(),			// light brightness
									false);	// false = screen coordinates

				}
				//-------------------------------------------------------
				// If H/W acceleration is not available...
				//-------------------------------------------------------
				else
				{

//					//---------------------------------------- 		
//					// Set the effect selection rectangle area
//					//---------------------------------------- 	
//					if (bSelectable)
//					{
//						CSpritePal* pSprite = &m_EffectScreenSPK[ spriteID ];
//
//
//						pEffect->SetScreenRect( &rect );
//					}


					AddLightFilter2D( pPoint->x + 24, 
									pPoint->y + 24, 
									Frame.GetLight(),			// light brightness
									false);		// false = screen coordinates
				}

			}
		}
		break;

		//------------------------------------------------------------------
		//
		//						Shadow Effect
		//
		//------------------------------------------------------------------
		case BLT_SHADOW :
		{
			TYPE_FRAMEID frameID = pEffect->GetFrameID();
			BYTE direction = pEffect->GetDirection();
			BYTE frame = pEffect->GetFrame();

			// Boundary check
			if (frameID >= m_EffectShadowFPK.GetSize()) {
#ifdef OUTPUT_DEBUG
				DEBUG_ADD_FORMAT_WAR("DrawEffect(BLT_SHADOW): Invalid frameID=%d", frameID);
#endif
				break;
			}

			CEffectFrame& Frame = m_EffectShadowFPK[frameID][direction][frame];
			int spriteID = Frame.GetSpriteID();

			if (spriteID != SPRITEID_NULL)
			{
				// Boundary check for sprite
				if (spriteID < 0 || static_cast<DWORD>(spriteID) >= m_EffectShadowSPK.GetSize()) {
#ifdef OUTPUT_DEBUG
					DEBUG_ADD_FORMAT_WAR("DrawEffect(BLT_SHADOW): Invalid spriteID=%d", spriteID);
#endif
					break;
				}

				point.x += Frame.GetCX();
				point.y += Frame.GetCY();

				CShadowSprite* pSprite = &m_EffectShadowSPK[spriteID];

				if (pSprite->IsInit())
				{
					m_pSurface->BltShadowSpriteDarkness(&point, pSprite, 1);
				}

				//-------------------------------------------------------
				// Hardware acceleration enabled
				//-------------------------------------------------------
				if (s_bUse3DLight)
				{
//					DRAW_TEXTURE_SPRITE_LOCKED(point.x, point.y, spriteID, m_pEffectShadowManager)		

					//------------------------------------------------
					// Add a LightFilter.
					//------------------------------------------------
					AddLightFilter3D( pPoint->x + 24, 
									pPoint->y + 24, 
									Frame.GetLight(),			// light brightness
									false);	// false = screen coordinates

				}
				//-------------------------------------------------------
				// If H/W acceleration is not available...
				//-------------------------------------------------------
				else
				{
//					CShadowSprite* pSprite = &m_EffectShadowSPK[spriteID];
//
//						
						AddLightFilter2D( pPoint->x + 24, 
										pPoint->y + 24, 
										Frame.GetLight(),			// light brightness
										false);		// false = screen coordinates
//					}
				}
			}
		}
		break;
	}
}

//----------------------------------------------------------------------
// Get Random MonsterType In Zone
//----------------------------------------------------------------------
int				
MTopView::GetRandomMonsterTypeInZone() const
{
	int numSprites = m_listLoadedMonsterSprite.GetSize();

	int spriteType;

	if (numSprites==0)
	{
		// Blood Warlock picture..
		spriteType = 27;
	}
	else
	{
		int nth = rand()%numSprites;

		INT_ORDERED_LIST::DATA_LIST::const_iterator	iID = m_listLoadedMonsterSprite.GetIterator();

		for (int i=0; i<nth; i++)
		{			
			iID ++;
		}

		spriteType = *iID;
	}

	// Pick one of the monsters whose spriteID is sprite.
	return g_pCreatureSpriteTypeMapper->GetRandomCreatureType( spriteType );
}

void		
MTopView::DrawCreatureHPModify(POINT *point, MCreature* pCreature)
{	
//	return;

	if(!g_pPlayer->HasEffectStatus( EFFECTSTATUS_VIEW_HP ) || pCreature->IsEmptyHPModifyList())
		return;

	MCreature::HPMODIFYLIST *pList = (MCreature::HPMODIFYLIST *)pCreature->GetHPModifyList();

	MCreature::HPMODIFYLIST::iterator itr = pList->begin();

	int py = static_cast<int>(point->y - (pList->size()-1)*15);

	g_FL2_GetDC();

	while(itr != pList->end())
	{
		char str[128];
		COLORREF color = 0;

		FormatHPModifyLabel(itr->modify, str, color);

		g_PrintColorStrOut(point->x + 24 - g_GetStringWidth(str, gpC_base->m_chatting_pi.hfont)/2, py, str, gpC_base->m_chatting_pi, color, RGB_BLACK);

		py += 15;

		if((DWORD)(MonotonicClock::Now() - itr->TickCount).count() > g_pClientConfig->HPModifyListTime)
		{
			pList->pop_front();
			itr = pList->begin();
		}
		else
			itr++;
	}

	g_FL2_ReleaseDC();
}
// 2004, 08, 18, sobeit add start 
//----------------------------------------------------------------------
// DrawGuildMark
/*----------------------------------------------------------------------
	- Shows the guild mark above a character in a siege war.
	- Always, whether the character is selected or not.
	- Another siege-war creature's mark shows only while g_pPlayer is in
	  the siege war too.
//----------------------------------------------------------------------*/
void		
MTopView::DrawGuildMarkInSiegeWar(MCreature* pCreature, int YPos)
{	
	// With EFFECTSTATUS_GHOST, others cannot see me and I cannot see others
	if(pCreature == NULL) return;
	if(pCreature->HasEffectStatus(EFFECTSTATUS_GHOST))return;

	//-----------------------------------------------------
	// Under Hallu the guild mark and rank mark are not drawn
	//-----------------------------------------------------
	if(!g_pPlayer->HasEffectStatus(EFFECTSTATUS_HALLUCINATION)
		&& pCreature->GetCreatureType() != CREATURETYPE_VAMPIRE_OPERATOR
		&& pCreature->GetCreatureType() != CREATURETYPE_SLAYER_OPERATOR
		&& pCreature->GetCreatureType() != CREATURETYPE_OUSTERS_OPERATOR
		&& g_pPlayer->GetIsInSiegeWar()
		&& pCreature->GetIsInSiegeWar()
		)
	{
		//-----------------------------------------------------
		// If there is a Guild - draw the Guild Mark
		//-----------------------------------------------------
		int guildID = pCreature->GetGuildNumber();

		if (guildID > 0)
		{
			//-------------------------------------------------
			// Check whether it is loaded.
			//-------------------------------------------------
			CSprite* pSprite = g_pGuildMarkManager->GetGuildMarkSmall(guildID);

			if (pSprite!=NULL)
			{							
				POINT pointTemp = { pCreature->GetPixelX() - m_FirstZonePixel.x  + 15, YPos - 23};

				m_pSurface->BltSprite(&pointTemp, pSprite);				

			}
			else
			{
				if (!g_pGuildMarkManager->HasGuildMark(guildID))
				{
					//-------------------------------------------------
					// Check whether it is in the file.
					//-------------------------------------------------
					g_pGuildMarkManager->LoadGuildMark(guildID);

					//-------------------------------------------------
					// Check again whether it was loaded from the file
					//-------------------------------------------------
					pSprite = g_pGuildMarkManager->GetGuildMark(guildID);

					//-------------------------------------------------
					// When it is not in the file either..
					// Fetch from the guildMark management server?.
					//-------------------------------------------------
					//if (pSprite==NULL) {}
				}				
			}
		}
	}
}
// 2004, 08, 18, sobeit add end
//----------------------------------------------------------------------
// Draw CreatureName
//----------------------------------------------------------------------
// A function called in the Unlock state.
//
// Draw the selected character's name.
// The name includes information such as HP, alignment, guild.. etc.
// As a bonus (-_-;) it also draws an icon according to RequestMode.
//----------------------------------------------------------------------
void		
MTopView::DrawCreatureName(MCreature* pCreature)
{	
	// With EFFECTSTATUS_GHOST, others cannot see me and I cannot see others
	if(pCreature == NULL) return;
	if(pCreature->HasEffectStatus(EFFECTSTATUS_GHOST))return;

	// 2004, 08, 05 sobeit add start - it is odd if the castle gate is shown?
	if( pCreature->GetCreatureType() >= 726 &&
		pCreature->GetCreatureType() <= 729)
		return;
	// 2004, 08, 05 sobeit add end - castle gate
	if(
		(g_pPlayer->HasEffectStatus(EFFECTSTATUS_YELLOW_POISON_TO_CREATURE) || 
		g_pPlayer->HasEffectStatus(EFFECTSTATUS_FLARE )||
		g_pPlayer->HasEffectStatus(EFFECTSTATUS_BLINDNESS))
		&& pCreature != g_pPlayer 
		)
	{
		// Decide whether to draw according to distance.
		int sx,sy,ex,ey;

		sx = g_pPlayer->GetX() - 1;
		ex = g_pPlayer->GetX() + 1;

		sy = g_pPlayer->GetY() - 1;
		ey = g_pPlayer->GetY() + 1;

		if( pCreature->GetX() < sx || pCreature->GetX() > ex ||
			pCreature->GetY() < sy || pCreature->GetY() > ey )
			return;
	}

	//---------------------------------------------------------------
	// Those underground have no name either. - -;
	//---------------------------------------------------------------
	if (pCreature->IsUndergroundCreature())
	{
		return;
	}

	POINT point = m_pointChatString;
	int yPoint = DrawChatString(&point, 
								pCreature, 
								RGB_WHITE,
								FLAG_DRAWTEXT_OUTLINE);
	point.y = yPoint;
	DrawCreatureHPModify(&point, pCreature);

	m_bDrawRequest = false;
	//------------------------------------------------
	// Draw the mark indicating an item exchange
	//------------------------------------------------
	if (IsRequestMode())	
	{
		bool bRequest = false;

		if(pCreature->GetClassType() == MCreature::CLASS_FAKE)
		{
			MFakeCreature *pFakeCreature = (MFakeCreature *)pCreature;
			if(pFakeCreature->GetOwnerID() != OBJECTID_NULL)	// it is a pet
			{
				bRequest = true;
			}
		}
		//------------------------------------------------
		// Must not be drawn yet.
		//------------------------------------------------			
		else if (// not an NPC and
			!pCreature->IsNPC()
			// only if it is a Player
			//&& pCreature->GetCreatureType()<=CREATURETYPE_VAMPIRE_FEMALE
			//&& (*g_pCreatureSpriteTable)[(*g_pCreatureTable)[pCreature->GetCreatureType()].SpriteType].IsPlayerOnlySprite()
			&& (*g_pCreatureSpriteTable)[(*g_pCreatureTable)[pCreature->GetCreatureType()].SpriteTypes[0]].IsPlayerOnlySprite()
			// When inside a coffin
			&& !pCreature->IsInCasket()
			&& pCreature->IsAlive()
			&& pCreature->GetCreatureType() != CREATURETYPE_SLAYER_OPERATOR
			&& pCreature->GetCreatureType() != CREATURETYPE_VAMPIRE_OPERATOR
			&& pCreature->GetCreatureType() != CREATURETYPE_OUSTERS_OPERATOR
			)
		{
			bRequest = true;
		}

		if(bRequest == true)
		{
			POINT pointTemp;
			TYPE_SPRITEID	RequestSpriteID = 0;

			//---------------------------------------------------------------
			// Icon for requesting a trade
			//---------------------------------------------------------------
			if (IsRequestTrade())
			{
				const int maxRequestFrames = SPRITEID_ITEM_TRADE_LAST - SPRITEID_ITEM_TRADE + 1;
				RequestSpriteID = SPRITEID_ITEM_TRADE + ((g_CurrentFrame>>1) % (maxRequestFrames<<1));

				if (RequestSpriteID > SPRITEID_ITEM_TRADE_LAST)
				{
					RequestSpriteID = SPRITEID_ITEM_TRADE_LAST - (RequestSpriteID - SPRITEID_ITEM_TRADE_LAST);

					if (RequestSpriteID < SPRITEID_ITEM_TRADE)
					{
						RequestSpriteID = SPRITEID_ITEM_TRADE;
					}
				}					
			}
			//---------------------------------------------------------------
			// Icon for requesting a party
			//---------------------------------------------------------------
			else if(IsRequestParty())
			{
				const int maxRequestFrames = SPRITEID_PARTY_REQUEST_LAST - SPRITEID_PARTY_REQUEST + 1;
				RequestSpriteID = SPRITEID_PARTY_REQUEST + ((g_CurrentFrame>>1) % (maxRequestFrames+4));

				if (RequestSpriteID > SPRITEID_PARTY_REQUEST_LAST)
				{
					RequestSpriteID = SPRITEID_PARTY_REQUEST_LAST;					
				}
			}
			//---------------------------------------------------------------
			// Icon for requesting Info
			//---------------------------------------------------------------
			else if(IsRequestInfo())
			{
				const int maxRequestFrames = SPRITEID_INFO_REQUEST_LAST - SPRITEID_INFO_REQUEST + 1;
				RequestSpriteID = SPRITEID_INFO_REQUEST + ((g_CurrentFrame>>1) % (maxRequestFrames+4));

				if (RequestSpriteID > SPRITEID_INFO_REQUEST_LAST)
				{
					RequestSpriteID = SPRITEID_INFO_REQUEST_LAST;					
				}
			}

			pointTemp.x = g_x + 27 - m_EtcSPK[RequestSpriteID].GetWidth();
			pointTemp.y = g_y + 15 - m_EtcSPK[RequestSpriteID].GetHeight();

			m_pSurface->Lock();
			m_pSurface->BltSprite( &pointTemp, &m_EtcSPK[RequestSpriteID] );
			m_pSurface->Unlock();

			m_bDrawRequest = true;
		}			
	}


	//------------------------------------------------
	// Character name output
	//------------------------------------------------
	//gC_font.PrintStringNoConvert(&m_SurfaceInfo, pCreature->GetName(), pointTemp.x, pointTemp.y, 0xFFFF);	
	bool bMasterWords = pCreature->GetCreatureType()==CREATURETYPE_SLAYER_OPERATOR
						|| pCreature->GetCreatureType()==CREATURETYPE_VAMPIRE_OPERATOR
						|| pCreature->GetCreatureType() == CREATURETYPE_OUSTERS_OPERATOR
						|| strncmp( pCreature->GetName(), (*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME].GetString(), (*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME].GetLength() ) == 0;
//						|| (strstr(pCreature->GetName(), "운영자")!=NULL);

	bool bHalluName = (g_pPlayer->HasEffectStatus(EFFECTSTATUS_HALLUCINATION) || 
		(!bMasterWords && 
		!g_pUserInformation->IsMaster && 
		(pCreature->GetClassType() == MCreature::CLASS_PLAYER || 
		pCreature->GetClassType() == MCreature::CLASS_CREATUREWEAR) && 
		g_pPlayer->GetRace() != pCreature->GetRace())) &&
		g_pZoneTable->Get( g_pZone->GetID() )->ChatMaskByRace &&
		g_pPlayer->GetCreatureType() != CREATURETYPE_SLAYER_OPERATOR &&
		g_pPlayer->GetCreatureType() != CREATURETYPE_VAMPIRE_OPERATOR &&
		g_pPlayer->GetCreatureType() != CREATURETYPE_OUSTERS_OPERATOR;						// only when the chat mask is applied
	if(g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST ))
		bHalluName = false;
#ifdef __METROTECH_TEST__
	if(g_bLight)
		bHalluName = false;
#endif
	const char* pName = (bHalluName?pCreature->GetHalluName() : pCreature->GetName());

	COLORREF	color;
	FONTID		font;

	/*
	if (pCreature->IsVampire())
	{
		color	= m_ColorNameVampire;
		font	= FONTID_VAMPIRE_NAME;
	}
	else if (pCreature->IsSlayer())
	{
		color = m_ColorNameSlayer;
		font	= FONTID_SLAYER_NAME;
	}
	else if (pCreature->IsNPC())
	{
		color = m_ColorNameNPC;
		font	= FONTID_NPC_NAME;
	}
	*/

	//-----------------------------------------------------
	// For an NPC
	//-----------------------------------------------------
	if (pCreature->IsNPC())
	{
		// For Bathory.. hardcoded - -;;
		if (pCreature->GetCreatureType()==217)
		{
			color	= m_ColorNameAlignment[1];
			font	= FONTID_VAMPIRE_NAME;
		}
		else
		{
			color = m_ColorNameNPC;
			font	= FONTID_NPC_NAME;			
		}
	}
	//-----------------------------------------------------
	// For an operator
	//-----------------------------------------------------
	else if (pCreature->GetCompetence()==0)
	{
		color = CSDLGraphics::Color( 31, 23, 3 );	// gold?
		font	= FONTID_NPC_NAME;
	}
	//-----------------------------------------------------
	// Under Hallu
	//-----------------------------------------------------
	else if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_HALLUCINATION))
	{
		color	= m_ColorNameAlignment[1];
		font	= FONTID_VAMPIRE_NAME;
	}
	//-----------------------------------------------------
	// For a Slayer
	//-----------------------------------------------------
	else if (g_pPlayer->IsSlayer())
	{
		//-----------------------------------------------------
		// By race
		//-----------------------------------------------------
		if (g_pPlayer->CanAttackTribe( pCreature )
			|| g_pJusticeAttackManager->HasCreature( pCreature->GetName() ))
//		if (g_pObjectSelector->CanAttack( pCreature ) )
		{
			color	= m_ColorNameAlignment[1];
			font	= FONTID_VAMPIRE_NAME;
		}
		//-----------------------------------------------------
		// Otherwise.. display according to alignment
		//-----------------------------------------------------
		else
		{
			int alignment = pCreature->GetAlignment();

			//alignment = min( 4, max(0, alignment) );

			color = m_ColorNameAlignment[alignment];
			font	= FONTID_SLAYER_NAME;				
		}
	}
	else if (g_pPlayer->IsOusters() )
	{
		if (g_pPlayer->CanAttackTribe( pCreature )
			|| g_pJusticeAttackManager->HasCreature( pCreature->GetName() ))
		{
			color	= m_ColorNameAlignment[1];
			font	= FONTID_VAMPIRE_NAME;
		}
		//-----------------------------------------------------
		// Otherwise.. display according to alignment
		//-----------------------------------------------------
		else
		{
			int alignment = pCreature->GetAlignment();

			color = m_ColorNameAlignment[alignment];
			font	= FONTID_VAMPIRE_NAME;				
		}
	}
	//-----------------------------------------------------
	// For a vampire
	//-----------------------------------------------------
	else
	{
		//-----------------------------------------------------
		// By guild
		//-----------------------------------------------------
		if (g_pPlayer->CanAttackGuild( pCreature )
			|| g_pJusticeAttackManager->HasCreature( pCreature->GetName() ))
//		if(g_pObjectSelector->CanAttack( pCreature ) )
		{
			color	= m_ColorNameAlignment[1];
			font	= FONTID_VAMPIRE_NAME;
		}
		//-----------------------------------------------------
		// Otherwise.. display according to alignment
		//-----------------------------------------------------
		else
		{
			int alignment = pCreature->GetAlignment();

			color = m_ColorNameAlignment[alignment];
			font	= FONTID_VAMPIRE_NAME;					
		}			
	}

	if (pName!=NULL)
	{
		//-----------------------------------------------------
		// When there is a Level Name
		//-----------------------------------------------------
		if (pCreature->HasLevelName())
		{
			const COLORREF levelNameColor = RGB(200, 250, 200);

			int yPoint_40 = yPoint - (g_pClientConfig->FONT_HEIGHT << 1)-3;

			DRAWTEXT_NODE* pLevelNameNode = new DRAWTEXT_NODE (
													m_pointChatString.x+1,
													yPoint_40+3,//2+1,
													pCreature->GetLevelName(),
													levelNameColor,
													font
													);			

			AddText( pLevelNameNode );
		}

		int yPoint_20 = yPoint - 23;	// g_pClientConfig->FONT_HEIGHT

		//int MAX_HP_BAR = pCreature->GetHPBarWidth(); //g_pClientConfig->MAX_HP_BAR_PIXEL;
		int POSITION_HP_BAR = g_pClientConfig->POSITION_HP_BAR;

		int namePixel = g_GetStringWidth(pName, g_ClientPrintInfo[font]->hfont);
		int MAX_HP_BAR = max(100, namePixel + 20);

		int guildID = 0;
		//-----------------------------------------------------
		// Box of the full (name) size laid under the name
		//-----------------------------------------------------
		int rectLeft	= m_pointChatString.x + POSITION_HP_BAR -14 ;
		int rectRight	= rectLeft + MAX_HP_BAR;//m_pointChatString.x+5 + namePixel;
		int rectTop		= yPoint_20;
		int rectBottom	= yPoint_20 + g_pClientConfig->FONT_HEIGHT;

		//-----------------------------------------------------
		// Under Hallu the guild mark and rank mark are not drawn
		//-----------------------------------------------------
		if(!g_pPlayer->HasEffectStatus(EFFECTSTATUS_HALLUCINATION)
			&& pCreature->GetCreatureType() != CREATURETYPE_VAMPIRE_OPERATOR
			&& pCreature->GetCreatureType() != CREATURETYPE_SLAYER_OPERATOR
			&& pCreature->GetCreatureType() != CREATURETYPE_OUSTERS_OPERATOR
			&& pCreature->GetCreatureType() != 672
			&& pCreature->GetCreatureType() != 673
			)
		{
			//-----------------------------------------------------
			// If there is a Guild - draw the Guild Mark
			//-----------------------------------------------------
			guildID = pCreature->GetGuildNumber();
			if(	g_pPlayer->GetRace() != pCreature->GetRace() && pCreature->IsPlayer() &&
				g_pZoneTable->Get( g_pZone->GetID() )->ChatMaskByRace == true
				)
				guildID = 0;

			// grade output
			int gradeID = pCreature->GetGrade()-1;	// It runs 1~50, so subtract 1 to fit 0~49
			// Only players show a rank; bats and wolves do not
			if(!pCreature->IsPlayerOnly())
				gradeID = -1;

				/*
				if (guildID>=0)
				{
				// Run-time loading seems fine too..
				if (guildID < m_GuildSPK.GetSize())
				{
				m_pSurface->Lock();

				  POINT pointTemp = { rectLeft-20, yPoint_20 };

					m_pSurface->BltSprite( &pointTemp, &m_GuildSPK[guildID] );

					  m_pSurface->Unlock();
					  }
					  }
			*/
			if (guildID > 0)
			{
				//-------------------------------------------------
				// Check whether it is loaded.
				//-------------------------------------------------
				CSprite* pSprite = g_pGuildMarkManager->GetGuildMarkSmall(guildID);

				if (pSprite!=NULL)
				{			
					m_pSurface->Lock();

					POINT pointTemp = { rectLeft-20, yPoint_20 };

					m_pSurface->BltSprite(&pointTemp, pSprite);				

					m_pSurface->Unlock();
				}
				else
				{
					if (!g_pGuildMarkManager->HasGuildMark(guildID))
					{
						//-------------------------------------------------
						// Check whether it is in the file.
						//-------------------------------------------------
						g_pGuildMarkManager->LoadGuildMark(guildID);

						//-------------------------------------------------
						// Check again whether it was loaded from the file
						//-------------------------------------------------
						pSprite = g_pGuildMarkManager->GetGuildMark(guildID);

						//-------------------------------------------------
						// When it is not in the file either..
						// Fetch from the guildMark management server?.
						//-------------------------------------------------
						//if (pSprite==NULL) {}
					}				
				}
			}

			// If there is a rank, draw the rank mark
			if(gradeID > -1 && gradeID <= GRADE_MARK_MAX)
			{
				CSprite* pSprite = g_pGuildMarkManager->GetGradeMarkSmall(gradeID, pCreature->GetRace());

				if (pSprite!=NULL)
				{			
					m_pSurface->Lock();

					POINT pointTemp = { rectRight, yPoint_20 };

					m_pSurface->BltSprite(&pointTemp, pSprite);				

					m_pSurface->Unlock();
				}
			}

		}
		// For a pet, draw the level mark
		if(pCreature->GetClassType() == MCreature::CLASS_FAKE)
		{
			MFakeCreature *pFakeCreature = (MFakeCreature *)pCreature;

			if(pFakeCreature->GetOwnerID() != OBJECTID_NULL)
			{
				MPetItem *pPetItem = pFakeCreature->GetPetItem();

				if(pPetItem != NULL)
				{
					int petLevel = pPetItem->GetNumber();
					if(petLevel > -1 && petLevel <= 50)
					{
						CSprite* pSprite = g_pGuildMarkManager->GetLevelMarkSmall(petLevel);

						if (pSprite!=NULL)
						{			
							m_pSurface->Lock();

							POINT pointTemp = { rectLeft-20, yPoint_20 };

							m_pSurface->BltSprite(&pointTemp, pSprite);				

							m_pSurface->Unlock();
						}
					}
				}
			}
		}

		//-----------------------------------------------------
		// HP calculation
		//-----------------------------------------------------
		int currentHP		= max(0,int(pCreature->GetHP()));
		int maxHP			= pCreature->GetMAX_HP();

		#ifdef OUTPUT_DEBUG
			if (g_pSDLInput->KeyDown(DIK_H) &&
				(g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL)))
			{
				char str[128];
				SafeFormat::Format(str, "HP=%d/%d", currentHP, maxHP);
				// m_pSurface->GDI_Text(rectLeft, rectTop-20, str, 0xFFFF);
				TextSystem::TextService::RenderText(g_pLast, rectLeft, rectTop-20, str);
			}
		#endif

		//-----------------------------------------------------
		// FIX: Handle maxHP being 0 - set to currentHP to show full bar
		//-----------------------------------------------------
		if (maxHP == 0 && currentHP > 0)
		{
			maxHP = currentHP;
		}
		else if (maxHP == 0 && currentHP == 0)
		{
			// Both are 0, show empty bar
			maxHP = 1;
			currentHP = 0;
		}

		//-----------------------------------------------------
		// Those underground are shown as if their HP were full.
		//-----------------------------------------------------

		if (currentHP > maxHP)
		{
			currentHP = maxHP;
		}

		//-----------------------------------------------------
		// Calculate the pixels actually shown
		//-----------------------------------------------------
		int maxPixels		= rectRight - rectLeft;
		int currentPixels	= max(0,int((maxHP==0)? 0 : maxPixels * currentHP / maxHP));

		int nameX = //rectLeft+(MAX_HP_BAR>>1) - (namePixel>>1);
					rectLeft + ((MAX_HP_BAR - namePixel)>>1);


		//-----------------------------------------------------
		// When HP is shown completely full
		//-----------------------------------------------------
		if (currentPixels==maxPixels)
		{
			// No need to lay a background color.. but to make the name easy to see.. --
			DRAWTEXT_NODE* pNodeBase = new DRAWTEXT_NODE (
										nameX+1,
										yPoint_20+4+1,
										NULL,//pName,
										0,
										font);

			AddText( pNodeBase );
		}
		//-----------------------------------------------------
		// Show the depleted part of HP
		//-----------------------------------------------------
		else
		{
			//-----------------------------------------------------
			//
			//			Show only a Box the size of Max HP
			//
			//-----------------------------------------------------
			DRAWTEXT_NODE* pNodeBase = new DRAWTEXT_NODE (
										nameX+1,
										yPoint_20+4+1,
										NULL,//pName,
										0,
										font);

			//-----------------------------------------------------
			// Depleted part of HP
			//-----------------------------------------------------
			RECT rect = {	rectLeft + currentPixels, 
							rectTop,
							rectRight,
							rectBottom 
			};


			pNodeBase->SetBox( rect, m_ColorHPBarBg, false );  // opaque mode
			AddText( pNodeBase );
		}

		//-----------------------------------------------------
		//
		//		Draw a Box of the current HP and the character name
		//
		//-----------------------------------------------------
		// Draw a Box of the current HP and the character name (in black)
		//-----------------------------------------------------	
		DRAWTEXT_NODE* pNode = new DRAWTEXT_NODE (
									nameX + 1,
									yPoint_20+4 + 1,
									pName,
									0,		//color,
									font);	//FLAG_DRAWTEXT_HANGUL_JOHAP);

		//-----------------------------------------------------
		// Draw the box if it is not 0
		//-----------------------------------------------------							
		if (currentPixels!=0)				
		{
			//-----------------------------------------------------
			// Box for the current HP
			//-----------------------------------------------------
			RECT rectHP = {	rectLeft,
							rectTop,
							rectLeft + currentPixels,
							rectBottom
			};

			//-----------------------------------------------------
			// FIX: Force non-transparent HP bar for better visibility
			// Force non-transparent mode regardless of user option
			//-----------------------------------------------------
			pNode->SetBox( rectHP, m_ColorHPBar, false );
		}

		//-----------------------------------------------------
		// Set it to be drawn..
		//-----------------------------------------------------
		AddText( pNode );			

		// 2004, 10, 28, sobeit add start - mark the target monster of a monster-kill quest.
//		if(pCreature->IsAlive() && UI_IsMonsterKillQuest_Monster((*g_pCreatureTable)[pCreature->GetCreatureType()].SpriteTypes[0]))
		//-----------------------------------------------------
		// Draw the name again
		//-----------------------------------------------------
		DRAWTEXT_NODE* pNode2 = new DRAWTEXT_NODE (
									nameX,
									yPoint_20+4,
									pName,
									color,
									font);	//FLAG_DRAWTEXT_HANGUL_JOHAP);

		AddText( pNode2 );
		// 2004, 6, 17, sobeit add start - about nick name - modified so the nickname shows only for the same race.
		if(pCreature->GetNickNameType() != NicknameInfo::NICK_NONE)// && (/*g_pPlayer->GetRace() == pCreature->GetRace()|| */pCreature->IsFakeCreature()))
		{
			if(yPoint-42<0) return; // just don't draw it..lol
			BYTE bType = pCreature->GetNickNameType();
			const char* szNickName = pCreature->GetNickName().c_str();
			if(strlen(szNickName)>0)
			{
				font = FONTID_USER_ID;

				namePixel = g_GetStringWidth(szNickName, g_ClientPrintInfo[font]->hfont);
				WORD bgColor = 0;

				switch(bType)
				{
				case NicknameInfo::NICK_BUILT_IN:
					color = RGB_YELLOW;
					bgColor = CSDLGraphics::Color(255,0,0);

					break;
				case NicknameInfo::NICK_QUEST:
					color = RGB(150,150,150);
					bgColor = CSDLGraphics::Color(0,0,255);
					break;
				case NicknameInfo::NICK_FORCED:
				case NicknameInfo::NICK_CUSTOM_FORCED:
					color = RGB_RED;
					break;
				case NicknameInfo::NICK_CUSTOM:
					color = RGB(50,170,230);
				//	bgColor = CSDLGraphics::Color(255,0,0);
					break;
				default:
					color = RGB_YELLOW;
					break;
				}
					nameX = rectLeft + ((MAX_HP_BAR - namePixel)>>1);

				DRAWTEXT_NODE* pNode = new DRAWTEXT_NODE (
												nameX ,
												yPoint-38,
												szNickName,
												color,		//color,
												font);	//FLAG_DRAWTEXT_HANGUL_JOHAP);
				if(bgColor != 0)
				{

					RECT rectHP = {	rectLeft -20,
									yPoint-42,
									rectRight + 20 ,
									yPoint-23	};

					// FIX: Force non-transparent box for better visibility
					pNode->SetBox( rectHP, bgColor, false );
				}	
				AddText( pNode );			
			}
		}
		// 2004, 6, 17, sobeit add end - about nick name
	}
}


//----------------------------------------------------------------------
// GetChangeValueToDirection
//----------------------------------------------------------------------
// This function doesn't belong here, but.. - -;
//----------------------------------------------------------------------
POINT
MTopView::GetChangeValueToDirection(int direction)
{
	return WorldTileGeometry::DirectionOffset(direction);
}


void
MTopView::DrawUndergroundCreature(POINT *pPoint, MCreature *pCreature)
{
	//----------------------------------------
	// fixed burrow picture  -_-;;
	//----------------------------------------
	POINT pointTemp;
	RECT rect;

	CSprite* pSprite = &m_EtcSPK[ SPRITEID_CREATURE_BURROW ];	

	//----------------------------------------
	// Coordinate correction
	//----------------------------------------
	int cx = 4;
	int cy = 4;
	pointTemp.x = pPoint->x + cx;
	pointTemp.y = pPoint->y + cy;

	//---------------------------------------- 		
	// Set the character selection rectangle area
	//---------------------------------------- 	
	rect.left	= pointTemp.x;
	rect.top	= pointTemp.y;
	rect.right	= rect.left + pSprite->GetWidth();
	rect.bottom = rect.top + pSprite->GetHeight();
	pCreature->SetScreenRect( &rect );				

	//---------------------------------------- 	
	// If selected
	//---------------------------------------- 	
	if (m_SelectCreatureID == pCreature->GetID() )
	{
		// Add to SpriteOutlineManager

		// Those underground are always vampires.
		if (g_pObjectSelector->CanAttack(pCreature))
		{
			m_SOMOutlineColor = m_ColorOutlineAttackPossible;
		}
		else
		{
			m_SOMOutlineColor = m_ColorOutlineAttackImpossible;
		}			

		m_SOM.Add( pointTemp.x, pointTemp.y, pSprite );
		m_SOM.Generate();

		m_pSurface->BltSpriteOutline( &m_SOM,  m_SOMOutlineColor );

		//---------------------------------------- 	
		// Set the coordinates for drawing the name
		//---------------------------------------- 	
		const int FontHeight = g_pClientConfig->FONT_HEIGHT;
		const int FontHeight2 = FontHeight << 1;

		pointTemp.x = pPoint->x;			
		if (pointTemp.x<0) pointTemp.x=0;

		//---------------------------------------- 	
		// Also calculate where to draw the Level Name
		//---------------------------------------- 	
		if (pCreature->HasLevelName())
		{
			pointTemp.y = pPoint->y - FontHeight2;

			if (pointTemp.y < FontHeight2)
			{
				pointTemp.y = FontHeight2;
			}
		}
		//---------------------------------------- 	
		// When drawing just the name
		//---------------------------------------- 	
		else
		{
			pointTemp.y = pPoint->y - FontHeight;

			if (pointTemp.y < FontHeight) 
			{
				pointTemp.y = FontHeight;
			}
		}

		m_pointChatString	= pointTemp;
		m_pSelectedCreature = pCreature;
	}
	//----------------------------------------
	// Plain output
	//----------------------------------------
	else
	{
		m_pSurface->BltSprite(&pointTemp, pSprite);
	}

	//----------------------------------------
	// Draw the Effects attached to the body
	//----------------------------------------
	/*
	if (pCreature->IsExistAttachEffect())
	{				
	pointTemp = *pPoint;
	//pointTemp.x += pCreature->GetSX();
	//pointTemp.y += pCreature->GetSY();

	  DrawAttachEffect(&pointTemp, pCreature->GetAttachEffectIterator(), pCreature->GetAttachEffectSize());
	  }
	*/	
}

void		
MTopView::DrawCreatureMyName()
{	
	// With EFFECTSTATUS_GHOST, others cannot see me and I cannot see others
	if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_GHOST))return;

	const char* pCreatureName;
	pCreatureName = g_pUserInformation->CharacterID.GetString();
	COLORREF	color;
	FONTID		font = FONTID_USER_ID;

	if (pCreatureName!=NULL)
	{
		int alignment = g_char_slot_ingame.alignment;
		int POSITION_HP_BAR = g_pClientConfig->POSITION_HP_BAR;

		color = m_ColorNameAlignment[alignment];
		POINT TempPoint = {g_pPlayer->GetPixelX() - m_FirstZonePixel.x , g_pPlayer->GetPixelY() - g_pPlayer->GetHeight() - m_FirstZonePixel.y};
		const int firstY = DrawChatString(&TempPoint, g_pPlayer, g_pPlayer->GetChatColor());

		int yPoint = firstY/*g_pPlayer->GetPixelY() - g_pPlayer->GetHeight() - m_FirstZonePixel.y*/;
		int rectLeft, rectRight, rectTop, rectBottom;
		int gradeID = 0;
		int guildID = 0;

		int yPoint_20 = yPoint - 23;	// g_pClientConfig->FONT_HEIGHT

		int namePixel = g_GetStringWidth(pCreatureName, g_ClientPrintInfo[font]->hfont);
		int MAX_HP_BAR = max(100, namePixel + 20);


		//-----------------------------------------------------
		// Box of the full (name) size laid under the name
		//-----------------------------------------------------
		{
			rectLeft	= g_pPlayer->GetPixelX() - m_FirstZonePixel.x + POSITION_HP_BAR - 14;
			rectRight	= rectLeft + MAX_HP_BAR;//m_pointChatString.x+5 + namePixel;
			rectTop		= yPoint_20;
			rectBottom	= yPoint_20+ g_pClientConfig->FONT_HEIGHT;
		}
		//-----------------------------------------------------
		// Under Hallu the guild mark and rank mark are not drawn
		//-----------------------------------------------------
		if(!g_pPlayer->HasEffectStatus(EFFECTSTATUS_HALLUCINATION))
		{
			//-----------------------------------------------------
			// If there is a Guild - draw the Guild Mark
			//-----------------------------------------------------
			guildID = g_pPlayer->GetGuildNumber();

			gradeID = g_pPlayer->GetGrade()-1;	// It runs 1~50, so subtract 1 to fit 0~49
			if(!g_pPlayer->IsPlayerOnly())
				gradeID = -1;

			if (guildID > 0)
			{
				//-------------------------------------------------
				// Check whether it is loaded.
				//-------------------------------------------------
				CSprite* pSprite = g_pGuildMarkManager->GetGuildMarkSmall(guildID);

				if (pSprite!=NULL)
				{			
					m_pSurface->Lock();

					POINT pointTemp = { rectLeft-20, yPoint_20 };

					m_pSurface->BltSprite(&pointTemp, pSprite);				

					m_pSurface->Unlock();
				}
				else
				{
					if (!g_pGuildMarkManager->HasGuildMark(guildID))
					{
						g_pGuildMarkManager->LoadGuildMark(guildID);
						pSprite = g_pGuildMarkManager->GetGuildMark(guildID);

					}				
				}
			}

			// If there is a rank, draw the rank mark
			if(gradeID > -1 && gradeID <= GRADE_MARK_MAX)
			{
				CSprite* pSprite = g_pGuildMarkManager->GetGradeMarkSmall(gradeID, g_pPlayer->GetRace());

				if (pSprite!=NULL)
				{			
					m_pSurface->Lock();

					POINT pointTemp = { rectRight, yPoint_20 };

					m_pSurface->BltSprite(&pointTemp, pSprite);				

					m_pSurface->Unlock();
				}
			}

		}
		//-----------------------------------------------------
		// HP calculation
		//-----------------------------------------------------
		int currentHP		= max(0,int(g_pPlayer->GetHP()));
		int maxHP			= g_pPlayer->GetMAX_HP();

		#ifdef OUTPUT_DEBUG
			if (g_pSDLInput->KeyDown(DIK_H) && 
				(g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL)))
			{
				char str[128];
				SafeFormat::Format(str, "HP=%d/%d", currentHP, maxHP);
				// m_pSurface->GDI_Text(rectLeft, rectTop-20, str, 0xFFFF);
				TextSystem::TextService::RenderText(g_pLast, rectLeft, rectTop-20, str);
			}					
		#endif

		if (currentHP > maxHP)
		{
			currentHP = maxHP;
		}

		//-----------------------------------------------------
		// Calculate the pixels actually shown
		//-----------------------------------------------------
		int maxPixels		= rectRight - rectLeft;
		int currentPixels	= max(0,int((maxHP==0)? 0 : maxPixels * currentHP / maxHP));

		int nameX = //rectLeft+(MAX_HP_BAR>>1) - (namePixel>>1);
					rectLeft + ((MAX_HP_BAR - namePixel)>>1);


		//-----------------------------------------------------
		// When HP is shown completely full
		//-----------------------------------------------------
		if (currentPixels==maxPixels)
		{
			// No need to lay a background color.. but to make the name easy to see.. --
			DRAWTEXT_NODE* pNodeBase = new DRAWTEXT_NODE (
										nameX+1,
										yPoint_20+4+1,
										NULL,//pName,
										0,
										font);

			AddText( pNodeBase );
		}
		//-----------------------------------------------------
		// Show the depleted part of HP
		//-----------------------------------------------------
		else
		{
			//-----------------------------------------------------
			//
			//			Show only a Box the size of Max HP
			//
			//-----------------------------------------------------
			DRAWTEXT_NODE* pNodeBase = new DRAWTEXT_NODE (
										nameX+1,
										yPoint_20+4+1,
										NULL,//pName,
										0,
										font);

			//-----------------------------------------------------
			// Depleted part of HP
			//-----------------------------------------------------
			RECT rect = {	rectLeft + currentPixels, 
							rectTop,
							rectRight,
							rectBottom 
			};


			pNodeBase->SetBox( rect, m_ColorHPBarBg, false );  // opaque mode
			AddText( pNodeBase );
		}

		//-----------------------------------------------------
		//
		//		Draw a Box of the current HP and the character name
		//
		//-----------------------------------------------------
		// Draw a Box of the current HP and the character name (in black)
		//-----------------------------------------------------			
		DRAWTEXT_NODE* pNode = new DRAWTEXT_NODE (
									nameX + 1,
									yPoint_20+4 + 1,
									pCreatureName,
									0,		//color,
									font);	//FLAG_DRAWTEXT_HANGUL_JOHAP);

		//-----------------------------------------------------
		// Draw the box if it is not 0
		//-----------------------------------------------------							
		if (currentPixels!=0)				
		{
			//-----------------------------------------------------
			// Box for the current HP
			//-----------------------------------------------------
			RECT rectHP = {	rectLeft,
							rectTop,
							rectLeft + currentPixels,
							rectBottom
			};

			//-----------------------------------------------------
			// FIX: Force non-transparent HP bar for better visibility
			// Force non-transparent mode regardless of user option
			//-----------------------------------------------------
			pNode->SetBox( rectHP, m_ColorHPBar, false );
		}

		//-----------------------------------------------------
		// Set it to be drawn..
		//-----------------------------------------------------
		AddText( pNode );			


		//-----------------------------------------------------
		// Draw the name again
		//-----------------------------------------------------
		DRAWTEXT_NODE* pNode2 = new DRAWTEXT_NODE (
									nameX,
									yPoint_20+4,
									pCreatureName,
									color,
									font);	//FLAG_DRAWTEXT_HANGUL_JOHAP);

		AddText( pNode2 );
		// 2004, 6, 17, sobeit add start - about nick name
		if(g_pPlayer->GetNickNameType() != NicknameInfo::NICK_NONE)
		{
			BYTE bType = g_pPlayer->GetNickNameType();
			const char* szNickName = g_pPlayer->GetNickName().c_str();
			if(strlen(szNickName)>0)
			{
				namePixel = g_GetStringWidth(szNickName, g_ClientPrintInfo[font]->hfont);
				WORD bgColor = 0;

				switch(bType)
				{
				case NicknameInfo::NICK_BUILT_IN:
					color = RGB_YELLOW;
					bgColor = CSDLGraphics::Color(255,0,0);

					break;
				case NicknameInfo::NICK_QUEST:
					color = RGB(150,150,150);
					bgColor = CSDLGraphics::Color(0,0,255);
					break;
				case NicknameInfo::NICK_FORCED:
				case NicknameInfo::NICK_CUSTOM_FORCED:
					color = RGB_RED;
					break;
				case NicknameInfo::NICK_CUSTOM:
					color = RGB(50,170,230);
				//	bgColor = CSDLGraphics::Color(255,0,0);
					break;
				default:
					color = RGB_YELLOW;
					break;
				}
					nameX = rectLeft + ((MAX_HP_BAR - namePixel)>>1);

				DRAWTEXT_NODE* pNode = new DRAWTEXT_NODE (
												nameX ,
												yPoint-38,
												szNickName,
												color,		//color,
												font);	//FLAG_DRAWTEXT_HANGUL_JOHAP);
				if(bgColor != 0)
				{

					RECT rectHP = {	rectLeft -20, 
									yPoint-42,
									rectRight + 20 ,
									yPoint-23	};

					// FIX: Force non-transparent box for better visibility
					pNode->SetBox( rectHP, bgColor, false );
				}	
				AddText( pNode );			
			}

		}
		// 2004, 6, 17, sobeit add end - about nick name
	}

}
bool
MTopView::DrawEvent()
{
	bool bDrawBackGround = true;
	MEvent *AdvancementQuestEndingEvent = const_cast<MEvent *>(g_pEventManager->GetEvent(EVENTID_ADVANCEMENT_QUEST_ENDING));
	if(AdvancementQuestEndingEvent!= NULL)
	{
		bDrawBackGround = ExcuteAdvancementQuestEnding((void *)AdvancementQuestEndingEvent);
	}
	else
		bDrawBackGround = ExcuteOustersFinEvent();

	// 2005, 2, 18, sobeit add start - handling while a WebBrowser is up
	if(UI_IsRunning_WebBrowser())
	{
		//bDrawBackGround = false; // The background is not drawn. The UI is drawn..
		int TempValue = UI_GetMouseCursorInfo_WebBrowser();

		if(TempValue == 1) // The cursor entered the web screen
		{
			UI_ShowWindowCursor(); // show the cursor..
		}
		else if( TempValue == -1) // The cursor left the web screen.
		{
			UI_HiddenWindowCursor(); // do not show the cursor.
		}
//		if(UI_IsMouseIn_WebBrowser())
//		{
//			

	// 2005, 2, 18, sobeit add end	
	}
	return bDrawBackGround;
}

bool
MTopView::ExcuteAdvancementQuestEnding(void *pVoid)
{
	MEvent *AdvancementQuestEndingEvent = (MEvent*)pVoid;

	if(AdvancementQuestEndingEvent!= NULL)
	{
		bool bFinEnd = false;
		const auto cancelEnding = [this]() {
			UI_REQUEST_DIE_TIMER_RESET();
			g_pEventManager->RemoveEvent(EVENTID_ADVANCEMENT_QUEST_ENDING);
			m_AdvacementQuestEnding.Release();
			return true;
		};

		int SpkIndex = AdvancementQuestEndingEvent->parameter4;
		if(m_AdvacementQuestEnding.GetSize() == 0)
		{
			const std::string& filename = g_pFileDef->getProperty("FILE_SPRITE_ADVANCEMENT_QUEST");
			std::ifstream	FinFile;
			if (!FileOpenBinary(filename.c_str(), FinFile)) return cancelEnding();
			// The three race endings use pairs 1/2, 3/4 and 5/6, plus scroll 0.
			if (!m_AdvacementQuestEnding.LoadFromFile(FinFile) || m_AdvacementQuestEnding.GetSize() < 7) {
				LOG_ERROR("Rejected advancement ending sprite pack: %s", filename.c_str());
				return cancelEnding();
			}
		}
		// Check before adding one, including a malformed event's INT_MAX index.
		if (SpkIndex < 0 || SpkIndex >= int(m_AdvacementQuestEnding.GetSize()) - 1) {
			LOG_ERROR("Rejected advancement ending sprite pair: id=%d", SpkIndex);
			return cancelEnding();
		}
		const TYPE_SOUNDID soundID = SOUND_SLAYER_ENCHANT_B2;

		static BYTE fadeColor[3] = {255, 255, 255};
		static int fadeSpeed = 10;
		static int fadeDirect = 0;
		POINT pointZero = { 0, 0 };

		if(AdvancementQuestEndingEvent->parameter1 == 0)
		{
			fadeColor[0] = 255;
			fadeColor[1] = 255;
			fadeColor[2] = 255;
			fadeSpeed = 10;
			fadeDirect = 0;

			//AdvancementQuestEndingEvent->parameter1 = g_pPlayer->IsSlayer()?1000:2000;
			AdvancementQuestEndingEvent->parameter1 = 9998;
			AdvancementQuestEndingEvent->parameter2 = 0;
			AdvancementQuestEndingEvent->parameter3 = 1000;
//				g_pPlayer->SetChatString((*g_pGameStringTable)[g_pPlayer->IsSlayer()?STRING_MESSAGE_RIPATY_SCRIPT_1:STRING_MESSAGE_AMATA_SCRIPT_1].GetString());
//
//				g_pMP3->Stop();
//				g_pMP3->Open( "Data\\Music\\blood.wav" );


			if( g_SDLAudio.IsInit() )
			{
				if( g_pOGG != NULL )
					g_pOGG->streamClose();

				if( g_pSoundBufferForOGG == NULL )
					g_pSoundBufferForOGG = NULL; // SDL backend: CDirectSoundBuffer not implemented

				if( g_pOGG == NULL )
#ifdef _MT
					g_pOGG = new COGGSTREAM(g_hWnd, g_pSoundBufferForOGG, 44100, 11025, 8800, 1);
#else
					g_pOGG = new COGGSTREAM(g_hWnd, g_pSoundBufferForOGG, 44100, 11025, 8800,1);
#endif

				if( g_oggfile != NULL)
					fclose(g_oggfile );
				g_oggfile = Basic::OpenFile( Basic::NormalizeDataPath( "Data\\Music\\Silence_of_Battlefield.ogg" ).c_str(), "rb");
				g_pOGG->streamLoad(g_oggfile, NULL);
				g_pOGG->streamPlay(0);
				int volume = (g_pUserOption->VolumeMusic - 15) * 250;
				g_pOGG->streamVolume( max( -10000, min( -1, volume ) ) );
			}
			WorldEvents::PlaySoundForce(soundID);
			AdvancementQuestEndingEvent->eventStartTickCount = MonotonicClock::Now();


		}
//
		m_pSurface->Lock();
		if(AdvancementQuestEndingEvent->parameter3 > 32)
		{
			m_pSurface->BltSprite( &pointZero, &m_AdvacementQuestEnding[SpkIndex+1] );
		}
		else if(AdvancementQuestEndingEvent->parameter3 == 0)
		{
			m_pSurface->BltSprite( &pointZero, &m_AdvacementQuestEnding[SpkIndex] );
		}
		else
		{
			m_pSurface->BltSprite( &pointZero, &m_AdvacementQuestEnding[SpkIndex] );
			m_pSurface->BltSpriteAlpha( &pointZero, &m_AdvacementQuestEnding[SpkIndex+1], static_cast<BYTE>(AdvancementQuestEndingEvent->parameter3) );
		}
		m_pSurface->Unlock();
		// Was gated on g_FrameCount & 0x01 when draws only happened on logic
		// ticks (every other draw = every other tick). Draws now run per
		// refresh for 60 fps interpolation, so gate on the tick counter to
		// keep the original crossfade pace.
		if(AdvancementQuestEndingEvent->parameter3>0 && g_bFrameChanged && (g_CurrentFrame & 0x01))
			AdvancementQuestEndingEvent->parameter3 --;
		switch(AdvancementQuestEndingEvent->parameter1)
		{
		case 9998:
			if(m_pSurface->Lock())
			{
				int scroll_progress = -500+AdvancementQuestEndingEvent->ElapsedMillis()/66;

				const int scroll_x = g_GameRect.right/2-m_AdvacementQuestEnding[0].GetWidth()/2, scroll_y = 50;

				int scroll = ( (scroll_progress < 0)? 0 : scroll_progress );
				int scroll2 = ( (scroll_progress < 0)? scroll_progress : scroll_progress );
				int scroll3 = ( (scroll_progress < 0)? 500+scroll_progress : 500 );

				Rect rect(0, scroll, m_AdvacementQuestEnding[0].GetWidth(), min(scroll3, m_AdvacementQuestEnding[0].GetHeight() - scroll));

				int x = scroll_x, y = scroll_y-scroll2;
				RECT rt;
				rt.left = max(-x, rect.x);
				rt.top = max(-y, rect.y);
				rt.right = min(rect.x+rect.w, g_GameRect.right-x);
				rt.bottom = min(rect.y+rect.h, g_GameRect.bottom-y);

				if(rt.bottom < rt.top)
				{
					AdvancementQuestEndingEvent->parameter1 = 9999;
				}

				if(rt.left < rt.right && rt.top < rt.bottom)
				{
					POINT point{x, y};
					m_pSurface->BltSpriteClip(&point, &m_AdvacementQuestEnding[0], &rt);
				}
				m_pSurface->Unlock();
			}
			break;

		case 9999:
			bFinEnd = true;
			break;
		}

		RECT	rect;
		rect.left = 0;
		rect.right = g_GameRect.right;
		rect.top = 0;
		rect.bottom = g_GameRect.bottom;

		DrawAlphaBox(&rect, fadeColor[0], fadeColor[1], fadeColor[2], static_cast<BYTE>(min(DWORD(31), AdvancementQuestEndingEvent->ElapsedMillis() /fadeSpeed)^fadeDirect));

		if(bFinEnd)
		{
			UI_REQUEST_DIE_TIMER_RESET();
			g_pEventManager->RemoveEvent(EVENTID_ADVANCEMENT_QUEST_ENDING);
			m_AdvacementQuestEnding.Release();
		}
		return false;
	}
	return true;
}
bool 
MTopView::ExcuteOustersFinEvent()
{
	bool bDrawBackGround = true;
	const MEvent *event = g_pEventManager->GetEventByFlag(EVENTFLAG_ONLY_EVENT_BACKGROUND);
	if(event != NULL)
	{
		if((event->eventFlag & EVENTFLAG_ONLY_EVENT_BACKGROUND) == EVENTFLAG_NOT_DRAW_BACKGROUND)
		{
			m_pSurface->FillSurface(0);	//	clear the screen
		}
		// The event's background image (parameter4, an EVENTBACKGROUND_ID)
		// was drawn here. As with Adam's cloud above, the SDL port never
		// drew one (task 4.224 retired the path).

		if(event->eventFlag & EVENTFLAG_ONLY_EVENT_BACKGROUND)
			bDrawBackGround = false;
	}

	MEvent *OustersFinEvent = const_cast<MEvent *>(g_pEventManager->GetEvent(EVENTID_OUSTERS_FIN));

	if(OustersFinEvent != NULL)
	{
		bool bFinEnd = false;
		const auto cancelEnding = [this]() {
			UI_REQUEST_DIE_TIMER_RESET();
			g_pEventManager->RemoveEvent(EVENTID_OUSTERS_FIN);
			m_OustersFinSPK.Release();
			return true;
		};

		if(m_OustersFinSPK.GetSize() == 0)
		{
			const std::string& filename = g_pFileDef->getProperty("FILE_SPRITE_OUSTERS_FIN");
			std::ifstream FinFile;
			if (!FileOpenBinary(filename.c_str(), FinFile)) return cancelEnding();
			// This sequence uses sprite IDs 0 through 10.
			if (!m_OustersFinSPK.LoadFromFile(FinFile) || m_OustersFinSPK.GetSize() < 11) {
				LOG_ERROR("Rejected Ousters ending sprite pack: %s", filename.c_str());
				return cancelEnding();
			}
		}

		const TYPE_SOUNDID soundID = SOUND_SLAYER_ENCHANT_B2;

		static BYTE fadeColor[3] = {255, 255, 255};
		static int fadeSpeed = 10;
		static int fadeDirect = 0;

		if(OustersFinEvent->parameter1 == 0)
		{
			fadeColor[0] = 255;
			fadeColor[1] = 255;
			fadeColor[2] = 255;
			fadeSpeed = 10;
			fadeDirect = 0;

			//OustersFinEvent->parameter1 = g_pPlayer->IsSlayer()?1000:2000;
			OustersFinEvent->parameter1 = 9998;
			OustersFinEvent->parameter2 = 0;
//				g_pPlayer->SetChatString((*g_pGameStringTable)[g_pPlayer->IsSlayer()?STRING_MESSAGE_RIPATY_SCRIPT_1:STRING_MESSAGE_AMATA_SCRIPT_1].GetString());
//
//				g_pMP3->Stop();
//				g_pMP3->Open( "Data\\Music\\blood.wav" );


			if( g_SDLAudio.IsInit() )
			{
				if( g_pOGG != NULL )
					g_pOGG->streamClose();

				if( g_pSoundBufferForOGG == NULL )
					g_pSoundBufferForOGG = NULL; // SDL backend: CDirectSoundBuffer not implemented

				if( g_pOGG == NULL )
#ifdef _MT
					g_pOGG = new COGGSTREAM(g_hWnd, g_pSoundBufferForOGG, 44100, 11025, 8800, 1);
#else
					g_pOGG = new COGGSTREAM(g_hWnd, g_pSoundBufferForOGG, 44100, 11025, 8800,1);
#endif

				if( g_oggfile != NULL)
					fclose(g_oggfile );
				g_oggfile = Basic::OpenFile( Basic::NormalizeDataPath( "Data\\Music\\chaos.ogg" ).c_str(), "rb");
				g_pOGG->streamLoad(g_oggfile, NULL);
				g_pOGG->streamPlay(0);
				int volume = (g_pUserOption->VolumeMusic - 15) * 250;
				g_pOGG->streamVolume( max( -10000, min( -1, volume ) ) );
			}
			WorldEvents::PlaySoundForce(soundID);
			OustersFinEvent->eventStartTickCount = MonotonicClock::Now();
		}

		switch(OustersFinEvent->parameter1)
		{
		case 1000:
			{
				if(OustersFinEvent->ElapsedMillis()/4000 > 0)
				{
					OustersFinEvent->parameter2++;
					if(OustersFinEvent->parameter2 > 8)
					{
						g_pPlayer->ClearChatString();

						if(OustersFinEvent->parameter2 == 9)
						{
							g_pPlayer->SetDead();
							WorldEvents::PlaySoundForce(g_pPlayer->IsFemale()?SOUND_SLAYER_DIE_FEMALE:SOUND_SLAYER_DIE_MALE);
							fadeSpeed = 32*4;
							fadeColor[0] = 255;
							fadeColor[1] = 0;
							fadeColor[2] = 0;
						}

						if(OustersFinEvent->parameter2 > 9)
						{
							fadeColor[0] = 0;
							fadeColor[1] = 0;
							fadeColor[2] = 0;
							fadeDirect = 31;

							if(OustersFinEvent->parameter2 > 10)
							{
								OustersFinEvent->eventStartTickCount = MonotonicClock::Now();
								OustersFinEvent->parameter1 = 9998;
								OustersFinEvent->parameter4 = EVENTBACKGROUNDID_OUSTERS_SLAYER;
								fadeDirect = 0;
							}
						}
					}
					else
					{
						WorldEvents::PlaySoundForce(soundID);
						g_pPlayer->SetChatString((*g_pGameStringTable)[STRING_MESSAGE_RIPATY_SCRIPT_1+OustersFinEvent->parameter2].GetString());
					}

					if(OustersFinEvent->parameter2 < 11)
						OustersFinEvent->eventStartTickCount = MonotonicClock::Now();
				}
				if(m_pSurface->Lock())
				{
					const int char_plus_x = 10, char_plus_y = 24;

					POINT p[8] = 
					{
						{ 400 - m_OustersFinSPK[3].GetWidth()/2+char_plus_x, 160-m_OustersFinSPK[3].GetHeight()+char_plus_y},
						{ 300 - m_OustersFinSPK[4].GetWidth()/2+char_plus_x, 210-m_OustersFinSPK[4].GetHeight()+char_plus_y},
						{ 240 - m_OustersFinSPK[5].GetWidth()/2+char_plus_x, 300-m_OustersFinSPK[5].GetHeight()+char_plus_y},
						{ 300 - m_OustersFinSPK[6].GetWidth()/2+char_plus_x, 390-m_OustersFinSPK[6].GetHeight()+char_plus_y},
						{ 400 - m_OustersFinSPK[7].GetWidth()/2+char_plus_x, 440-m_OustersFinSPK[7].GetHeight()+char_plus_y},
						{ 500 - m_OustersFinSPK[8].GetWidth()/2+char_plus_x, 390-m_OustersFinSPK[8].GetHeight()+char_plus_y},
						{ 560 - m_OustersFinSPK[9].GetWidth()/2+char_plus_x, 300-m_OustersFinSPK[9].GetHeight()+char_plus_y},
						{ 500 - m_OustersFinSPK[10].GetWidth()/2+char_plus_x, 210-m_OustersFinSPK[10].GetHeight()+char_plus_y},
					};

					m_pSurface->BltSprite(&p[0], &m_OustersFinSPK[3]);
					POINT strPoint = p[0];
					strPoint.x -= 15;
					strPoint.y -= 15;
					DrawChatString(&strPoint, g_pPlayer, RGB(255, 255, 255));

					if(OustersFinEvent->parameter2 > 0)
					{
						for(int i = 0; i < 7; i++)
						{
							m_pSurface->BltSprite(&p[i+1], &m_OustersFinSPK[4+i]);
						}
					}

					MCreature *pCreature = g_pPlayer;

					int action = pCreature->GetAction(), direction = 6, frame = pCreature->GetFrame();

					MCreatureWear*	pCreatureWear = (MCreatureWear*)pCreature;

					// Draw the add-ons matching the Creature's Action.
					//action = pCreature->GetAction();

					WORD clothes;
					BYTE clothesType;

					for (int i=0; i<ADDON_MAX; i++)
					{
						// Depending on the Creature's current direction...
						// the order in which clothes are drawn may differ.
						clothesType = MCreatureWear::s_AddonOrder[direction][i];

						// If it is wearing the i-th kind of clothes, draw it.
						const MCreatureWear::ADDON_INFO& addonInfo = pCreatureWear->GetAddonInfo(clothesType);

						if (addonInfo.bAddon)
						{
							clothes = addonInfo.FrameID;

							FRAME_ARRAY &FA = m_AddonFPK[clothes][action][direction];

							// If the motion exists
							if (FA.GetSize() > frame)
							{
								CFrame &Frame = FA[frame];					
								int sprite	= Frame.GetSpriteID();	//m_AddonFPK[clothes][action][direction][frame].GetSpriteID();
								int cx		= Frame.GetCX();	//m_AddonFPK[clothes][action][direction][frame].GetCX();
								int cy		= Frame.GetCY();	//m_AddonFPK[clothes][action][direction][frame].GetCY();


								CIndexSprite* pSprite = &m_AddonSPK[ sprite ];					



								POINT pointTemp;

								// Coordinate correction
								pointTemp.x = 384+cx;
								pointTemp.y = 312+cy;

								{
									int colorSet1 = addonInfo.ColorSet1, colorSet2 = addonInfo.ColorSet2;
									if(colorSet2 == UNIQUE_ITEM_COLOR || colorSet2 == QUEST_ITEM_COLOR)
									{

										colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );
										if(addonInfo.ItemClass != ITEM_CLASS_COAT && addonInfo.ItemClass != ITEM_CLASS_TROUSER)
											colorSet1 = colorSet2;
									}

									CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );

									// Draw darkened
									if (pCreature->IsFade())
									{
										m_pSurface->BltIndexSpriteDarkness(&pointTemp, pSprite, 1);
									}
									else
									{
										m_pSurface->BltIndexSprite(&pointTemp, pSprite);
									}
								}						


							}
						}

					}

					m_pSurface->Unlock();
				}
			}
			break;

		case 2000:
			{
				if(OustersFinEvent->ElapsedMillis()/4000 > 0)
				{
					OustersFinEvent->parameter2++;

					if(OustersFinEvent->parameter2 > 8)
					{
						g_pPlayer->ClearChatString();

						if(OustersFinEvent->parameter2 == 9)
						{
							g_pPlayer->SetDead();
							WorldEvents::PlaySoundForce(g_pPlayer->IsFemale()?SOUND_VAMPIRE_DIE_FEMALE:SOUND_VAMPIRE_DIE_MALE);
							fadeSpeed = 32*4;
							fadeColor[0] = 255;
							fadeColor[1] = 0;
							fadeColor[2] = 0;
						}

						if(OustersFinEvent->parameter2 > 9)
						{
							fadeColor[0] = 0;
							fadeColor[1] = 0;
							fadeColor[2] = 0;
							fadeDirect = 31;

							if(OustersFinEvent->parameter2 > 10)
							{
								OustersFinEvent->eventStartTickCount = MonotonicClock::Now();
								OustersFinEvent->parameter1 = 9998;
								OustersFinEvent->parameter4 = EVENTBACKGROUNDID_OUSTERS_VAMPIRE;
								fadeDirect = 0;
							}
						}
					}
					else
					{
						WorldEvents::PlaySoundForce(soundID);
						g_pPlayer->SetChatString((*g_pGameStringTable)[STRING_MESSAGE_AMATA_SCRIPT_1+OustersFinEvent->parameter2].GetString());
					}

					if(OustersFinEvent->parameter2 < 11)
						OustersFinEvent->eventStartTickCount = MonotonicClock::Now();
				}

				if(m_pSurface->Lock())
				{
					const int char_plus_x = 10, char_plus_y = 24;

					POINT p[8] = 
					{
						{ 400 - m_OustersFinSPK[2].GetWidth()/2+char_plus_x, 160-m_OustersFinSPK[2].GetHeight()+char_plus_y},
						{ 300 - m_OustersFinSPK[4].GetWidth()/2+char_plus_x, 210-m_OustersFinSPK[4].GetHeight()+char_plus_y},
						{ 240 - m_OustersFinSPK[5].GetWidth()/2+char_plus_x, 300-m_OustersFinSPK[5].GetHeight()+char_plus_y},
						{ 300 - m_OustersFinSPK[6].GetWidth()/2+char_plus_x, 390-m_OustersFinSPK[6].GetHeight()+char_plus_y},
						{ 400 - m_OustersFinSPK[7].GetWidth()/2+char_plus_x, 440-m_OustersFinSPK[7].GetHeight()+char_plus_y},
						{ 500 - m_OustersFinSPK[8].GetWidth()/2+char_plus_x, 390-m_OustersFinSPK[8].GetHeight()+char_plus_y},
						{ 560 - m_OustersFinSPK[9].GetWidth()/2+char_plus_x, 300-m_OustersFinSPK[9].GetHeight()+char_plus_y},
						{ 500 - m_OustersFinSPK[10].GetWidth()/2+char_plus_x, 210-m_OustersFinSPK[10].GetHeight()+char_plus_y},
					};

					m_pSurface->BltSprite(&p[0], &m_OustersFinSPK[2]);
					POINT strPoint = p[0];
					strPoint.x -= 15;
					strPoint.y -= 15;
					DrawChatString(&strPoint, g_pPlayer, RGB(255, 255, 255));

					if(OustersFinEvent->parameter2 > 0)
					{
						for(int i = 0; i < 7; i++)
						{
							m_pSurface->BltSprite(&p[i+1], &m_OustersFinSPK[4+i]);
						}
					}

					int body, action, direction, frame;

					MCreature *pCreature = g_pPlayer;


					body		= pCreature->GetCreatureFrameID(0);
					action		= pCreature->GetAction();
					direction	= 6;//pCreature->GetDirection();
					frame		= pCreature->GetFrame();//%m_CreatureFPK[body][action][direction].GetCount();

					// vampire
					FRAME_ARRAY& FA = m_CreatureFPK[body][action][direction];

					if (FA.GetSize() > frame)
					{
						CFrame& Frame =	FA[frame];
						int sprite = Frame.GetSpriteID(),	//m_CreatureFPK[body][action][direction][frame].GetSpriteID(),
							cx		= Frame.GetCX(),	//m_CreatureFPK[body][action][direction][frame].GetCX(),
							cy		= Frame.GetCY();	//m_CreatureFPK[body][action][direction][frame].GetCY();

						CIndexSprite* pSprite = &m_CreatureSPK[ sprite ];


						int colorSet1 = pCreature->GetBodyColor1();
						int colorSet2 = pCreature->GetBodyColor2();
						if( colorSet1 == QUEST_ITEM_COLOR || colorSet1 == UNIQUE_ITEM_COLOR )
							colorSet1 = MItem::GetSpecialColorItemColorset( colorSet1 );

						if( colorSet2 == QUEST_ITEM_COLOR || colorSet2 == UNIQUE_ITEM_COLOR )
							colorSet2 = MItem::GetSpecialColorItemColorset( colorSet2 );

						POINT pointTemp;

						// Coordinate correction
						pointTemp.x = 384+cx;
						pointTemp.y = 312+cy;

						CIndexSprite::SetUsingColorSet( colorSet1, colorSet2 );
						m_pSurface->BltIndexSprite(&pointTemp, pSprite);
					}

					m_pSurface->Unlock();
				}
			}
			break;

		case 9998:
			if(m_pSurface->Lock())
			{
				int scroll_progress = -500+OustersFinEvent->ElapsedMillis()/66;

				const int spriteID = 0;
				const int scroll_x = g_GameRect.right/2-m_OustersFinSPK[spriteID].GetWidth()/2, scroll_y = 50;

				int scroll = ( (scroll_progress < 0)? 0 : scroll_progress );
				int scroll2 = ( (scroll_progress < 0)? scroll_progress : scroll_progress );
				int scroll3 = ( (scroll_progress < 0)? 500+scroll_progress : 500 );

				Rect rect(0, scroll, m_OustersFinSPK[spriteID].GetWidth(), min(scroll3, m_OustersFinSPK[spriteID].GetHeight() - scroll));

				int x = scroll_x, y = scroll_y-scroll2;
				RECT rt;
				rt.left = max(-x, rect.x);
				rt.top = max(-y, rect.y);
				rt.right = min(rect.x+rect.w, g_GameRect.right-x);
				rt.bottom = min(rect.y+rect.h, g_GameRect.bottom-y);

				if(rt.bottom < rt.top)
				{
					OustersFinEvent->parameter1 = 9999;
				}

				if(rt.left < rt.right && rt.top < rt.bottom)
				{
					POINT point{x, y};
					m_pSurface->BltSpriteClip(&point, &m_OustersFinSPK[spriteID], &rt);
				}

				Rect rect2(0, 0, m_OustersFinSPK[1].GetWidth(), min(scroll3 - (m_OustersFinSPK[spriteID].GetHeight() - scroll) -50, m_OustersFinSPK[1].GetHeight()));

				x = scroll_x +m_OustersFinSPK[spriteID].GetWidth()/2-m_OustersFinSPK[1].GetWidth()/2;
				y = scroll_y + max(200, (m_OustersFinSPK[spriteID].GetHeight() - scroll + 50) );
				rt.left = max(-x, rect2.x);
				rt.top = max(-y, rect2.y);
				rt.right = min(rect2.x+rect2.w, g_GameRect.right-x);
				rt.bottom = min(rect2.y+rect2.h, g_GameRect.bottom-y);

				if(rt.left < rt.right && rt.top < rt.bottom)
				{
					POINT point{x, y};
					m_pSurface->BltSpriteClip(&point, &m_OustersFinSPK[1], &rt);
				}

				m_pSurface->Unlock();
			}
			break;

		case 9999:
			bFinEnd = true;
			break;
		}

		RECT	rect;
		rect.left = 0;
		rect.right = g_GameRect.right;
		rect.top = 0;
		rect.bottom = g_GameRect.bottom;

		DrawAlphaBox(&rect, fadeColor[0], fadeColor[1], fadeColor[2], static_cast<BYTE>(min(DWORD(31), OustersFinEvent->ElapsedMillis() /fadeSpeed)^fadeDirect));

		if(bFinEnd)
		{
			UI_REQUEST_DIE_TIMER_RESET();
			g_pEventManager->RemoveEvent(EVENTID_OUSTERS_FIN);
			m_OustersFinSPK.Release();
		}

	}
	return bDrawBackGround;
}
