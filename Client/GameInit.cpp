//---------------------------------------------------------------------------
// GameInit.cpp
//---------------------------------------------------------------------------
// 게임 관련 부분(주로 시스템 쪽)의 초기화 / Release
//---------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Include files
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "SafeFormat.h"
#ifdef PLATFORM_WINDOWS
// <MMSystem.h> not included: no real MM_ symbols are used in this file, and
// including it conflicts with basic/Platform.h's timeGetTime()/GetTickCount()
// macros (see basic/Platform.h).
#else
// macOS: BSD sockets headers for network functions
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif
#include "DebugLog.h"
#include <memory>
#include "TextEncoding.h"
#include "DirectoryListing.h"
#include "Client.h"
#include "GameObject.h"
#include "AddonDef.h"
#include "ServerInfo.h"
#include "Packet/WireHost.h"
#include "DXLib/DXInputHost.h"
#include "../VS_UI/src/InputFocusManager.h"
#include "PacketDef.h"
#include "PacketHandlerRegistry.h"
#include "VS_UI.h"
#include "COpeningUpdate.h"
#include "CGameUpdate.h"
#include "CWaitPacketUpdate.h"
#include "UserInformation.h"
#include "TempInformation.h"
#include "CWaitUIUpdate.h"
#include "UIDialog.h"
#include "MMusic.h"
#include "MGameStringTable.h"
#include "MMoneyManager.h"
#include "MPriceManager.h"
#include "MChatManager.h"
#include "MWeather.h"
#include "MViewDef.h"
#include "MTradeManager.h"
#include "UIMessageManager.h"
#include "MStorage.h"
#include "MHelpManager.h"
#include "MHelpStringTable.h"
#include "MCompareManager.h"
#include "MHelpDisplayer.h"
#include "MObjectSelector.h"
#include "MLevelNameTable.h"
#include "MonsterNameTable.h"
#include "ExperienceTable.h"
#include "CServerInformation.h"
#include "GameObject.h"
#include "MZoneSoundManager.h"
#include "MNPCScriptTable.h"
#include "MParty.h"
#include "UiRuntime.h"
#include "MFakeCreature.h"
#include "SpriteIDDef.h"
#include "MathTable.h"
#include "ModifyStatusManager.h"
#include "RequestServerPlayerManager.h"
#include "WebSocketTransport.h"
#include <cstdlib>
#include "ClientCommunicationManager.h"
#include "KeyAccelerator.h"
#include "AcceleratorManager.h"
#include "ProfileManager.h"
#include "MGuildMarkManager.h"
#include "MEventManager.h"
#include "MEffectTarget.h"
#include "MEffect.h"
#include "MGuidanceEffect.h"
#include "MParabolaEffect.h"
#include "MAttachEffect.h"
#include "MScreenEffectManager.h"
#include "MStopInventoryEffectGenerator.h"
#include "MStopZoneEffectGenerator.h"
#include "MStopZoneCrossEffectGenerator.h"
#include "MStopZoneXEffectGenerator.h"
#include "MStopZoneRhombusEffectGenerator.h"
#include "MStopZoneEmptyCrossEffectGenerator.h"
#include "MStopZoneEmptyRectEffectGenerator.h"
#include "MStopZoneRectEffectGenerator.h"
#include "MStopZoneWallEffectGenerator.h"
#include "MStopZoneMultipleEffectGenerator.h"
#include "MStopZoneRandomEffectGenerator.h"
#include "MStopZoneSelectableEffectGenerator.h"
#include "MSpreadOutEffectGenerator.h"
#include "MAroundZoneEffectGenerator.h"
#include "MFollowPathEffectGenerator.h"
#include "MMeteorDropEffectGenerator.h"
#include "MAttackCreatureParabolaEffectGenerator.h"
#include "MRippleZoneWideEffectGenerator.h"
#include "MBloodyBreakerEffectGenerator.h"
#include "MStopZoneEmptyHorizontalWallEffectGenerator.h"
#include "MStopZoneEmptyVerticalEffectGenerator.h"
#include "MFallingEffectGenerator.h"
#include "MAttackZoneEffectGenerator.h"
#include "MAttackZoneParabolaEffectGenerator.h"
#include "MAttackZoneBombEffectGenerator.h"
#include "MMultipleFallingEffectGenerator.h"
#include "MRisingEffectGenerator.h"
#include "MInventory.h"
#include "VS_UI_GameCommon.h"
#include "MEffectGeneratorTable.h"
#include "MEffectSpriteTypeTable.h"
#include "EffectSpriteTypeDef.h"
#include "PacketFunction.h"
#include "SkillDef.h"
#include "RequestFileManager.h"
#include "Packet/RequestServerPlayer.h"
#include "RequestUserManager.h"
#include "MJusticeAttackManager.h"
#include "Profiler.h"
#include "WavePackFileManager.h"
//#include "MFileDef.h"
#include "MGuildInfoMapper.h"
#include "PCConfigTable.h"
#include "MAttachOrbitEffect.h"
#include "DebugInfo.h"
#include "MTestDef.h"
#include "CMP3.h"
#include "RankBonusTable.h"
#include "MMonsterKillQuestInfo.h"
#include "MTimeItemManager.h"
#include "MItem.h"
#include "MTopView.h"
#include "MPlayer.h"
#include "MSkillManager.h"
#include "FameInfo.h"
#include "MWarManager.h"
#include "CSprite555.h"
#include "CSprite565.h"
#include "Properties.h"
#include "UIFunction.h"
#include "SoundSetting.h"
#include "SystemAvailabilities.h"
#include "RarFile.h"
#include <sstream>
#include "ShrineInfoManager.h"
#include "ServerInfoFileParser.h"
#include "LoginEndpoint.h"

//yckou
#include "DebugKit.h"

extern BYTE g_macAddress[6];
extern BOOL GetMacAddressFromSock();
extern DWORD g_dwSeqNumL;
extern DWORD g_dwSeqNumG;
extern BOOL g_MyFull;
extern RECT g_GameRect;
//#include "CJpeg.h"
//#define	new			DEBUG_NEW

// ToT SafeDelete 도 없고..ToT 2003.5.11 by sonee

extern int					g_Dimension ;
// g_bEnable3DHAL was originally defined in VS_UI_Title.cpp when _LIB is not defined
// Now that VS_UI is built with _LIB defined, we need to define it here
BOOL g_bEnable3DHAL = FALSE;

//----------------------------------------------------------------------
// Title Loading - 2001.8.20 우헤헤 자꾸 늘어나는 global - -;;
//----------------------------------------------------------------------
//CSpritePack		g_TitleSPK;
//CSpritePack		g_TitleAniSPK;
//int				g_TitleSpriteID = 0;
//int				g_TitleSpriteDarkness = 4;
bool			g_bTitleLoading = false;
CSprite*		g_pTitleLoadingSprite = NULL;
int				g_TitleSpriteAlpha = 32;
//CDirectDrawSurface	g_TitleSurface;


// [Futec수정]
extern char g_FutecIP[20];
extern unsigned int g_FutecPort;

#ifdef OUTPUT_DEBUG
	extern CRITICAL_SECTION		g_Lock;
#endif

//----------------------------------------------------------------------
// AddonSPK 분할 로딩 - 2001.8.20
//----------------------------------------------------------------------
TYPE_SPRITEID		g_AddonSPKNum = 0;
const int			g_AddonSPKLoadingTimes = 20;	// 20등분으로 나눠서 loading한다.
bool				g_AddonSPKLoaded[g_AddonSPKLoadingTimes] = { false, };
int					g_AddonSPKIndexFirst[g_AddonSPKLoadingTimes] = { 0, };
int					g_AddonSPKIndexLast[g_AddonSPKLoadingTimes] = { 0, };
long				g_AddonSPKIndexFP[g_AddonSPKLoadingTimes] = { 0, };
bool				g_AddonSPKAllLoaded = false;

bool				g_bHALAvailable = false;
extern bool				g_bHALAvailable;
extern WORD g_wAuthKeyMap;

void	StartTitleLoading();
void	DrawTitleLoading();
void	EndTitleLoading(bool SendLogin = false);
bool	PrepareLoadingAddonSPK();
bool	LoadingAddonSPK(bool bLoadingAll);
extern bool CheckInvalidProcess();
extern void ClearDebugInfo();

extern int  g_nGameVersion;
//#define	CLIENT_VERSION		"[TestVersion] : 2000.12.05.09"
//---------------------------------------------------------------------------
// Start TitleLoading
//---------------------------------------------------------------------------
// 2001.8.20
//---------------------------------------------------------------------------
void
StartTitleLoading()
{
//	std::ifstream	titleFile;//(FILE_SSPRITE_IMAGEOBJECT, ios::binary);
//	if (!FileOpenBinary("data\\ui\\spk\\Title.spk", titleFile))
//		return;
//
//	g_TitleSPK.LoadFromFile(titleFile);
//	titleFile.close();
//
//	std::ifstream	titleAniFile;//(FILE_SSPRITE_IMAGEOBJECT, ios::binary);
//	if (!FileOpenBinary("data\\ui\\spk\\TitleAni.spk", titleAniFile))
//		return;
//
//	g_TitleAniSPK.LoadFromFile(titleAniFile);
//	titleAniFile.close();
//	
//
//	g_TitleSpriteID = 0;
//	g_TitleSpriteDarkness = 4;
	g_bTitleLoading = true;

//	CJpeg jpg;
//	bool bOpen = jpg.Open(g_pFileDef->getProperty("FILE_JPG_SOFTON_CI").c_str());
//	if(bOpen == true && jpg.GetWidth() > 0 && jpg.GetHeight() > 0 && jpg.GetHeight() > 0)
//	{
//		g_bTitleLoading = true;
//		CDirectDrawSurface &surface = g_TitleSurface;
//		const int bpp = jpg.GetBpp(), width = jpg.GetWidth(), height = jpg.GetHeight(), pitch = width*bpp;
//
//		if (surface.InitOffsurface(width, height, DDSCAPS_SYSTEMMEMORY))
//		{
//			if (surface.Lock())
//			{
//				WORD *pSurface = (WORD *)surface.GetSurfacePointer();
//				unsigned char *pData = jpg.GetImage(), *pDataTemp;
//				WORD *pSurfaceTemp;
//				
//				int surfacePitch = surface.GetSurfacePitch();
//				
//				if (pSurface)
//				{
//					if (bpp == 1)
//					{
//						for (register int y = 0; y < height; y++)
//						{
//							pDataTemp = pData;
//							pSurfaceTemp = pSurface;								
//							
//							for (register int x = 0; x < width; x++)
//							{
//								BYTE temp_data = *pDataTemp++;	//p_data[y*(pitch)+x];
//								BYTE r = temp_data>>3;
//								BYTE g = r;
//								BYTE b = r;
//								
//								*pSurfaceTemp++ = CSDLGraphics::Color(r, g, b);
//							}
//							
//							pData = pData + pitch;
//							pSurface = (WORD*)((BYTE*)pSurface + surfacePitch);
//						}
//					}
//					else if (bpp == 3)
//					{
//						for (register int y = 0; y < height; y++)
//						{
//							pDataTemp = pData;
//							pSurfaceTemp = pSurface;	
//							
//							for (register int x = 0; x < width; x++)
//							{
//								//char *temp_data = &p_data[y*pitch+x*bpp];
//								BYTE r = *(pDataTemp+2) >> 3;		//temp_data[2]>>3;
//								BYTE g = *(pDataTemp+1) >> 3;	//temp_data[1]>>3;
//								BYTE b = *pDataTemp >> 3;	//temp_data[0]>>3;
//								
//								pDataTemp += bpp;
//								
//								*pSurfaceTemp++ = CSDLGraphics::Color(r, g, b);
//							}
//							
//							pData = pData + pitch;
//							pSurface = (WORD*)((BYTE*)pSurface + surfacePitch);
//						}
//					}
//					
//				}
//				
//				surface.Unlock();
//			}
//		}
//	}
	
	if (g_pTitleLoadingSprite==NULL)
	{
		CFileIndexTable	FIT;
		const std::string indexPath = g_pFileDef->getProperty("FILE_SPRITEINDEX_UI");
		std::ifstream indexFile(indexPath, ios::binary);
		const unsigned id = g_MyFull ? 5 : SPRITEID_CI;
		long offset = 0;
		if (!FIT.LoadFromFile(indexFile) || !FIT.TryGetOffset(id, offset)) {
			LOG_ERROR("Rejected title sprite index: source=%s id=%u", indexPath.c_str(), id);
			return;
		}

		std::unique_ptr<CSprite> sprite;
		if (CSDLGraphics::Is565()) sprite = std::make_unique<CSprite565>();
		else sprite = std::make_unique<CSprite555>();
		const std::string path = g_pFileDef->getProperty("FILE_SPRITE_UI");
		std::ifstream spkFile(path, ios::binary);
		spkFile.seekg(offset);
		if (!CTypePackDetail::LoadElement(*sprite, spkFile, path.c_str(), id)) return;
		g_pTitleLoadingSprite = sprite.release();
	}
}
	
//---------------------------------------------------------------------------
// Draw TitleLoading
//---------------------------------------------------------------------------
// 2001.8.20
//---------------------------------------------------------------------------
void
DrawTitleLoading()
{
	if (g_bTitleLoading)
	{
		//--------------------------------------------------------
		// 2001.8.20
		//--------------------------------------------------------
		POINT pointZero = { 0, 0 };
		// add by Sonic 2006.9.26
//		if(g_MyFull)
//		{
//			RECT rect = {0, 0, 1024, 768};
//		}
//		else
//		{
//		}
		// end

//		g_pBack->FillSurface( CSDLGraphics::Color(0,0,0) );
		
//		g_pBack->Lock();

//		g_pBack->Blt(&pointZero, &g_TitleSurface, &rect);
//		g_pBack->BltSprite( &pointZero, &g_TitleSPK[ 0 ] );

//		if (g_TitleSpriteDarkness==0)
//		{
//			g_pBack->BltSprite( &point, &g_TitleAniSPK[g_TitleSpriteID] );
//		}
//		else
//		{
//			g_pBack->BltSpriteDarkness( &point, &g_TitleAniSPK[g_TitleSpriteID], g_TitleSpriteDarkness );
//		}

//		g_pBack->Unlock();

		// Loading 출력
		/*
		if (gpC_base!=NULL)
		{
			if (!true)
			{
				g_SetFL2Surface( g_pBack );
			}

			PrintInfo* pPrintInfo = &gpC_base->m_info_pi;
			const COLORREF txColor = RGB(100,250,100);				
			const COLORREF txColor2 = RGB(20,50,20);

			pPrintInfo->text_color	= txColor2;
			g_Print(331, 451, "잠시 기다려주세요.", pPrintInfo);
			pPrintInfo->text_color	= txColor;
			g_Print(330, 450, "잠시 기다려주세요.", pPrintInfo);
			
			//char str[80];
			//sprintf(str, "[%d]", g_TitleSpriteID);
			//g_Print(330, 480, str, pPrintInfo);	
			
			if (!true)
			{
				g_SetFL2Surface( g_pLast );
			}
		}
		*/
		
		if (g_pTitleLoadingSprite!=NULL)
		{
//			point.x = 400 - (g_pTitleLoadingSprite->GetWidth()>>1);
//			point.y = 460 - (g_pTitleLoadingSprite->GetHeight()>>1);
//			
			CSpriteSurface *pSurface = NULL;
//			if (!true)
			if(g_TitleSpriteAlpha != 32)
			{
				pSurface = g_pLast;
			}
			else
			{
				pSurface = g_pBack;
			}

			pSurface->Lock();
			if(g_TitleSpriteAlpha == 32)
			{
				pSurface->BltSprite( &pointZero, g_pTitleLoadingSprite );
			}
			else
			{
				pSurface->BltSpriteAlpha( &pointZero, g_pTitleLoadingSprite, g_TitleSpriteAlpha );
			}
			pSurface->Unlock();
		}

		if(g_TitleSpriteAlpha == 32)
			CSDLGraphics::Flip();

		
		// frame 바꿈
//		if (++g_TitleSpriteID>=g_TitleAniSPK.GetSize())
//		{
//			g_TitleSpriteID = 0;			
//		}
//			
//		// 밝기 조절
//		if (g_TitleSpriteID%3==2)
//		{
//			if (--g_TitleSpriteDarkness < 0)
//			{
//				g_TitleSpriteDarkness = 0;
//			}
//		}			
	}
}

//---------------------------------------------------------------------------
// End TitleLoading
//---------------------------------------------------------------------------
void
EndTitleLoading(bool SendLogin)
{
//	g_TitleAniSPK.Release();
//	g_TitleSPK.Release();
//	g_TitleSpriteID = 0;
//	g_TitleSpriteDarkness = 4;
	g_bTitleLoading = false;

	if (g_pTitleLoadingSprite!=NULL)
	{
		delete g_pTitleLoadingSprite;
		g_pTitleLoadingSprite = NULL;
	}
	// 2004, 7, 15, sobeit modify start
	// 2006.11.07  Coffee Edit 혼뇜踏狗폘땡빈菱땡되쩍
	
	if(SendLogin && true == g_pUserInformation->IsAutoLogIn) // 자동 로그인
	{
		static LOGIN	login;
		login.sz_id = new char[  g_pUserInformation->UserID.GetLength() + 1 ];
		login.sz_password = new char[g_pUserInformation->AutoLogInKeyValue.GetLength() + 1];
		
		SafeFormat::Copy(login.sz_id, g_pUserInformation->UserID.GetLength() + 1, g_pUserInformation->UserID.GetString());
		SafeFormat::Copy(login.sz_password, g_pUserInformation->AutoLogInKeyValue.GetLength() + 1, g_pUserInformation->AutoLogInKeyValue.GetString());
		
		gpC_base->SendMessage(UI_LOGIN, 0, 0, (void *)&login);
	}
	
	// 2006.11.07  Coffee Edit
	// 2004, 7, 15, sobeit modify end
}

//---------------------------------------------------------------------------
// PrepareLoadingAddonSPK
//---------------------------------------------------------------------------
bool
PrepareLoadingAddonSPK()
{
	return true;

	#ifdef OUTPUT_DEBUG
		if (g_pDebugMessage)
		{
			DEBUG_ADD("Prepare LoadingAddonSPK");
		}
	#endif

	g_AddonSPKAllLoaded = false;

	if (g_pTopView==NULL)
	{
		#ifdef OUTPUT_DEBUG
			if (g_pDebugMessage)
			{
				DEBUG_ADD("[Error] g_pTopView is NULL");
			}
		#endif

		return false;
	}

	//------------------------------------------------------------
	// 나눠서 로딩하기 위한 준비.. 2001.8.20 추가
	//------------------------------------------------------------
	if (g_AddonSPKNum==0)
	{
		std::ifstream	AddonFileIndex2;//(FILE_ISPRITE_ADDON, ios::binary);
		if (!FileOpenBinary(g_pFileDef->getProperty("FILE_ISPRITEINDEX_ADDON").c_str(), AddonFileIndex2))
			return false;

		AddonFileIndex2.read((char*)&g_AddonSPKNum, SIZE_SPRITEID);

		// 개수를 잡아둔다.
		g_pTopView->m_AddonSPK.Init( g_AddonSPKNum);		

		// loading하는 회수에 맞게 file index를 구한다.
		int first = 0;
		for (int i=0; i<g_AddonSPKLoadingTimes; i++)
		{
			// 1/3
			int last = (int)g_AddonSPKNum * (i+1) / g_AddonSPKLoadingTimes;
			
			// 더 클 일은 없겠지만.. 혹시.. - -;;
			if (last >= g_AddonSPKNum)
			{
				last = g_AddonSPKNum - 1;
			}

			long fp;
			// 개수(2 bytes) + ID * (4 bytes)
			AddonFileIndex2.seekg( 2 + first*4 , ios::beg );
			AddonFileIndex2.read((char*)&fp, 4);

			// file index설정			
			g_AddonSPKIndexFirst[i] = first;
			g_AddonSPKIndexLast[i] = last;
			g_AddonSPKIndexFP[i] = fp;
			

			// 다음 꺼.
			first = last+1;
		}

		AddonFileIndex2.close();
	}

	return true;
}

//---------------------------------------------------------------------------
// LoadingAddonSPK
//---------------------------------------------------------------------------
bool
LoadingAddonSPK(bool bLoadingAll)
{
	return true;

	#ifdef OUTPUT_DEBUG
		if (g_pDebugMessage)
		{
			if (bLoadingAll)
			{
				DEBUG_ADD("LoadingAddonSPK - LoadingAll");
			}
			else
			{
				DEBUG_ADD("LoadingAddonSPK - LoadingPart");
			}
		}
	#endif

	if (g_pTopView==NULL)
	{
		#ifdef OUTPUT_DEBUG
			if (g_pDebugMessage)
			{
				DEBUG_ADD("[Error] g_pTopView is NULL");
			}
		#endif

		return false;
	}

	for (int i=0; i<g_AddonSPKLoadingTimes; i++)
	{
		// loading 안 된 부분이 있으면 loading한다.
		if (!g_AddonSPKLoaded[i])
		{
//			std::ifstream	addonFile;
//
//			if (!FileOpenBinary(FILE_ISPRITE_ADDON, addonFile))
//				return false;	

			TYPE_SPRITEID first = g_AddonSPKIndexFirst[i];
			TYPE_SPRITEID last	= g_AddonSPKIndexLast[i];
			[[maybe_unused]] long	fp	= g_AddonSPKIndexFP[i];

			#ifdef OUTPUT_DEBUG
				if (g_pDebugMessage)
				{
					DEBUG_ADD_FORMAT("LoadingAddonSPK: [%d ~ %d](fp=%d). num=%d", first, last, fp, g_AddonSPKNum);
				}
			#endif

			if (!g_pTopView->m_AddonSPK.LoadFromFilePart(first, last)) {
				LOG_ERROR("Rejected addon preload: first=%u last=%u",
					static_cast<unsigned>(first), static_cast<unsigned>(last));
				return false;
			}
		
			g_AddonSPKLoaded[i] = true;

			// 전부 loading해야하는 경우가 아니면 한부분만 loading한다.
			if (!bLoadingAll)
			{
				#ifdef OUTPUT_DEBUG
					if (g_pDebugMessage)
					{
						DEBUG_ADD("LoadingAddonSPK Part OK");
					}
				#endif

				return true;
			}
		}
	}

	g_AddonSPKAllLoaded = true;

	#ifdef OUTPUT_DEBUG
		if (g_pDebugMessage)
		{
			DEBUG_ADD("All AddonSPK is Loaded");
		}
	#endif

	return true;
}

//---------------------------------------------------------------------------
// Init Volume
//---------------------------------------------------------------------------
BOOL
InitVolume()
{
	// Debug Message
	DEBUG_ADD("[ InitGame ]  Volume");

	// Volume control is now handled by SDL_mixer
	// No Windows mixer support needed

	return TRUE;
}

//---------------------------------------------------------------------------
// Init Input
//---------------------------------------------------------------------------
BOOL
InitInput()
{
	DXInput::SetHost({
		.mousePosition = [](int x, int y) { g_x = x; g_y = y; },
		.activeApp = [](bool active) { g_bActiveApp = active ? TRUE : FALSE; },
		.hasTextFocus = []() { return g_GetInputFocusManager().HasFocus(); },
		.keyDown = [](unsigned int key) { g_GetInputFocusManager().HandleKeyDown(key); },
		.textInput = [](const char* text) { g_GetInputFocusManager().HandleTextInput(text); },
		.textEditing = [](const char* text, int start, int length) {
			g_GetInputFocusManager().HandleTextEditing(text, start, length);
		},
		.graphicsReset = []() { if (g_pTopView) g_pTopView->SetFirstDraw(); }
	});
	// Debug Message
	DEBUG_ADD("[ InitGame ]  Input");

	if (g_pSDLInput==NULL)
	{
		g_pSDLInput = new CSDLInput;
	}

	//----------------------------
	// DirectInput
	//----------------------------
	//g_pSDLInput->Init(g_hInstance);
	//g_pSDLInput->InitKeyDevice(g_hWnd);
//	g_pSDLInput->InitJoyDevice(g_hWnd);
	//g_pSDLInput->InitMouseDevice(g_hWnd);

	if (!g_pSDLInput->Init( g_hWnd, g_hInstance, CSDLInput::NONEXCLUSIVE ))
	{
		InitFail("DirectInput Error!");
		return false;
	}

	//g_pSDLInput->SetMouseMoveLimit(SURFACE_WIDTH, SURFACE_HEIGHT);

	g_pSDLInput->SetAcquire(true);

	return TRUE;
}


//---------------------------------------------------------------------------
// Surface의 정보를 생성한다.
//---------------------------------------------------------------------------
BOOL
InitSurface()
{
	DEBUG_ADD("[ InitGame ]  Surface");

	//--------------------------------------------------------
	// Back Surface
	//--------------------------------------------------------
	if (g_pBack!=NULL) 
	{
		delete g_pBack;
	}
	g_pBack = new CSpriteSurface;
	// g_pBack is the final present-to-screen buffer (see SDLMain.cpp's
	// g_pBack->Blt(&origin, g_pLast, NULL) frame flip). It must match
	// g_pLast's size (g_GameRect), not a hardcoded 800x600, or any mode
	// larger than 800x600 (e.g. 1024x768) gets cropped on screen.
	g_pBack->Init(g_GameRect.right, g_GameRect.bottom);

	//--------------------------------------------------------
	// 임시로 로딩화면 구성..
	// 2001.8.20
	//--------------------------------------------------------

	StartTitleLoading();
	DrawTitleLoading();


	g_pBack->SetTransparency( 0 );
	g_pBack->FillSurface( CSDLGraphics::Color(0,0,0) );

	//--------------------------------------------------------
	// Buffer로 사용할 Surface
	//--------------------------------------------------------
	DEBUG_ADD("[ InitGame ]  Surface - new g_pTopView");

		
	if (g_pTopView==NULL)
	{		
		g_pTopView = new MTopView;

		// addonSPK 분할 로딩 준비 - 2001.8.20
		if (!PrepareLoadingAddonSPK())
		{
			return FALSE;
		}
	}


	// H/W가속 되는 경우
//	if (true)
//	{
//		g_pTopView->SetSurface( g_pBack );
//	}
//	// H/W가속 안 되는 경우
//	else
	{		
		if (g_pLast!=NULL) 
		{
			delete g_pLast;
		}
		g_pLast = new CSpriteSurface;
		// SDL2: Unified offscreen surface initialization (always system memory)
		g_pLast->InitOffsurface(g_GameRect.right, g_GameRect.bottom);
		// This composition buffer is rebuilt before every copy to g_pBack.
		// Reading it back for device recovery stalls the GPU every frame.
		g_pLast->SetTransient(true);
		g_pLast->SetTransparency( 0 );
		g_pLast->FillSurface( CSDLGraphics::Color(30,30,30) );

		g_pTopView->SetSurface( g_pLast );
	}

	//--------------------------------------------------------
	// 감마값 설정
	//--------------------------------------------------------
	if (g_pUserOption->UseGammaControl
		&& g_pUserOption->GammaValue!=100)
	{
		CSDLGraphics::SetGammaRamp( g_pUserOption->GammaValue );
	}

	//--------------------------------------------------------
	//                UI
	//--------------------------------------------------------	
	DEBUG_ADD("[ InitGame ]  Surface - Initialize Font");
	DEBUG_ADD("[ InitGame ]  Surface - UI");

	// 슬레이어 그림 약간 loading
	LoadingAddonSPK( false );
	DrawTitleLoading();


	// 일단 제거.. - -;
	gC_vs_ui.Release();

	DEBUG_ADD("[ InitGame ]  Surface - InitD3D");
//	if (true)
//	{
//		gC_vs_ui.Init(g_pBack, UI_ResultReceiver);		
//	} 
//	else
	{
		gC_vs_ui.Init(g_pLast, UI_ResultReceiver);
	}

	//--------------------------------------------------------
	// 2001.8.20
	//--------------------------------------------------------	
	DrawTitleLoading();	

//	gC_vs_ui.SetVersion(CLIENT_VERSION);




	///*
	//--------------------------------------------------------
	// 새로운 IndexSprite 생성 방법 TEST
	//--------------------------------------------------------
	// indexTable은 555와 565가 다르기 때문에...
	// 잘~~골라서 해야한다. = =;
	CIndexSprite::SetColorSet();

	//#ifdef OUTPUT_DEBUG
	//	std::ofstream indexTable(FILE_INDEXTABLE, ios::binary);
	//	CIndexSprite::SaveIndexTableToFile( indexTable );
	///	indexTable.close();
	//#endif
	
	//std::ifstream indexTable2;//(FILE_INFO_USINGCOLORSET, ios::binary);
	//if (!FileOpenBinary(FILE_INDEXTABLE, indexTable2))
	//	return FALSE;
	//CIndexSprite::LoadIndexTableFromFile( indexTable2 );
	//indexTable2.close();

	/*
	CSpriteSurface imageSurface;
	CSpriteSurface indexSurface1;
	//CSpriteSurface indexSurface2;
	
	WORD *lpImage, imagePitch;
	WORD *lpIndex1, indexPitch1;
	//WORD *lpIndex2, indexPitch2;

	imageSurface.InitFromBMP("test\\0.bmp", DDSCAPS_SYSTEMMEMORY);	
	indexSurface1.InitFromBMP("test\\0i.bmp", DDSCAPS_SYSTEMMEMORY);	
	//indexSurface2.InitFromBMP("Gauntlet2.bmp", DDSCAPS_SYSTEMMEMORY);	

	imageSurface.LockW(lpImage, imagePitch);
	indexSurface1.LockW(lpIndex1, indexPitch1);
	//indexSurface2.LockW(lpIndex2, indexPitch2);

	CIndexSprite565 is;

	RECT rect;

	CSprite::GetTightColorRect(lpImage, imagePitch, 
								imageSurface.GetWidth(), imageSurface.GetHeight(),
								0x001F, 
								rect);


	lpImage = (WORD*)((BYTE*)lpImage + imagePitch*rect.top + (rect.left << 1));
	lpIndex1 = (WORD*)((BYTE*)lpIndex1 + indexPitch1*rect.top + (rect.left << 1));
	//lpIndex2 = (WORD*)((BYTE*)lpIndex2 + indexPitch2*rect.top + (rect.left << 1));

	CIndexSprite::SetColorkey( 0x001F );
	//CIndexSprite::SetColorkey( ((32>>3) << 11) | ((32>>3) << 6) | (32>>3) );
	is.SetPixel(lpImage, imagePitch,
				lpIndex1, indexPitch1,
				NULL, 0, //lpIndex2, indexPitch2,
				rect.right-rect.left, rect.bottom-rect.top);
	
	imageSurface.Unlock();
	indexSurface1.Unlock();
	//indexSurface2.Unlock();

	std::ofstream file("test.ispr", ios::binary);
	is.SaveToFile( file );
	file.close();

	POINT point;
	point.x = 100;
	point.y = 100;
	WORD a=40, b=150;
	while (1)
	{
		UpdateInput();

		if (g_pSDLInput->KeyDown(DIK_ESCAPE))
			break;

		if (g_pSDLInput->KeyDown(DIK_SPACE))
		{
			a = rand()%MAX_COLORSET;
			b = rand()%MAX_COLORSET;
		}

		g_pBack->FillSurface( 0 );	

		g_pBack->Lock();
		CIndexSprite::SetUsingColorSet( a, b );
		g_pBack->BltIndexSprite( &point, &is );
		g_pBack->Unlock();

		CSDLGraphics::Flip();
	}

	return FALSE;
	*/	

	//--------------------------------------------------------
	// View의 정보를 초기화 시킨다.
	//--------------------------------------------------------
	//g_pTopView->SetSurface(g_pLast);	
	if (g_Mode==MODE_CHANGE_OPTION	// 옵션 바꿀때는 g_pTopView->Init()는 안 한다.
		&& !g_pTopView->Init())
	{
		return FALSE;
	}			

	DrawTitleLoading();	
//	EndTitleLoading();
	
	//g_pTopView->SetDarkBits(5);
		

	// ColorSet을 초기화시킨다.
	CIndexSprite::SetColorSet();
	
	//--------------------------------------------------------
	// Cursor Surface 초기화
	//--------------------------------------------------------
	// 2개의 Flip Surface, (32*32)
	if (CSDLGraphics::IsFullscreen())
	{
		if (g_pCursorSurface!=NULL)
		{
			delete g_pCursorSurface;
			g_pCursorSurface = NULL;
		}

		//g_pCursorSurface = new CStorageSurface;
	}
	//g_pCursorSurface.Init(2, CURSOR_SURFACE_WIDTH, CURSOR_SURFACE_HEIGHT);
	DrawTitleLoading();	

	return TRUE;
}

//---------------------------------------------------------------------------
// Init Sound
//---------------------------------------------------------------------------
BOOL
InitSound()
{
	// Debug Message
	DEBUG_ADD("[ InitGame ]  InitSound");

	//--------------------------------------------------------
	//
	//                 DirectSound
	//
	//--------------------------------------------------------
	/*
	if (g_pWavePackFileManager==NULL)
	{
		g_pWavePackFileManager = new WavePackFileManager;

		g_pWavePackFileManager->LoadFromFileInfo(FILE_WAVE_PACK_SOUND_INDEX);
		g_pWavePackFileManager->SetDataFilename(FILE_WAVE_PACK_SOUND);
	}
	*/

	if (g_SDLAudio.IsInit())
	{
		DEBUG_ADD("[ InitGame ]  Already Init");

		return TRUE;
	}
	else
	{
		if (g_SDLAudio.Init(g_hWnd))
		{	
			// filename에 따라서.. Wav file을 Load한다.
			//for (int i=0; i<(*g_pSoundTable).GetSize(); i++)
			//{			
			//	(*g_pSoundTable)[i].pDSBuffer = g_Sound.LoadWav( (*g_pSoundTable)[i].Filename );
			//}
			int value = g_pUserOption->VolumeSound;	

			LONG volume = value*SOUND_DEGREE + SOUND_MIN;

			g_SDLAudio.SetVolumeLimit( volume );	

#ifndef __USE_MP3__
			//-----------------------------------------------------------
			// The OGG music streamer. InitMusic() below would create it,
			// but nothing calls InitMusic(); and once IsInit() is true the
			// music call sites (PlayMusicCurrentZone and friends) call
			// g_pOGG->streamClose() without a null check.
			//-----------------------------------------------------------
			if (g_pOGG == NULL)
			{
				g_pOGG = new COGGSTREAM(NULL, g_pSoundBufferForOGG, 44100, 11025, 8800, 1);

				int musicVolume = (g_pUserOption->VolumeMusic - 15) * 250;
				g_pOGG->streamVolume( max( -10000, min( -1, musicVolume ) ) );
			}
#endif
		}
		else
		{
			DEBUG_ADD("[Error] Init Sound Error");

			return FALSE;
		}

		//-----------------------------------------------------------
		// RAM 체크해서.. 적당하게 잡아준다.
		//-----------------------------------------------------------
		// SDL2: Unified - use default sound part count for all platforms
		// DirectDraw memory status not available in SDL2
		g_pClientConfig->MAX_SOUNDPART = 100;

		// ( 전체 개수, 메모리 허용 개수 )
		g_pSoundManager->Init( g_pSoundTable->GetSize(), g_pClientConfig->MAX_SOUNDPART );

		/*
		for (int i=0; i<2; i++)
		for (int soundID=0; soundID<(*g_pClientConfig).MAX_SOUNDPART; soundID++)
		{
			//-----------------------------------------------------------
			// 없으면 --> Load & Play
			//-----------------------------------------------------------
			if (g_pSoundManager.IsDataNULL(soundID))
			{
				// 다시 load						
 				LPDIRECTSOUNDBUFFER pBuffer = g_SDLAudio.LoadWav( (*g_pSoundTable)[soundID].Filename );

				// Load에 성공 했으면...
				if (pBuffer!=NULL)
				{
					// Replace됐으면 원래것을 메모리에서 지운다.
					LPDIRECTSOUNDBUFFER pOld;
					if (g_pSoundManager.SetData( soundID, pBuffer, pOld ))
					{
						g_SDLAudio.Release(pOld);
					}
				}
			}
			//-----------------------------------------------------------
			// 있는 경우 --> Play
			//-----------------------------------------------------------
			else
			{
				LPDIRECTSOUNDBUFFER pBuffer;
				if (g_pSoundManager.GetData(soundID, pBuffer))
				{
				
				}		
			}
		}

		return FALSE;
		*/

		return TRUE;
	}
	
}

//---------------------------------------------------------------------------
// Init Music
//---------------------------------------------------------------------------
BOOL
InitMusic()
{
	// Debug Message
	DEBUG_ADD("[ InitGame ]  Music");
	
	//--------------------------------------------------------
	//
	//                 Music
	//
	//--------------------------------------------------------
	//g_Music.Init(g_hWnd);	
		/*
	if (g_bMusicSW)
	{
		g_SDLMusic.Init(g_hWnd, DIRECTMUSIC_TYPE_SW);
	}
	else
	{
		g_SDLMusic.Init(g_hWnd, DIRECTMUSIC_TYPE_HW);
	}
	*/
	g_Music.Init( g_hWnd );

	//int volume = g_Music.GetVolume();
	//			DEBUG_ADD_FORMAT("[MIDI] Volume in Init = %x", volume);

	int value = g_pUserOption->VolumeMusic;
				
	WORD volume = value*0x1000 + 0x0FFF;

	g_Music.SetVolume( volume );

	
//	if (g_pSDLStream==NULL)
//	{
//		g_pSDLStream = new CDirectSoundStream;
//
//		int value = g_pUserOption->VolumeMusic;	
//
//		LONG volume = value*SOUND_DEGREE + SOUND_MIN;
//
//		g_pSDLStream->SetVolumeLimit( volume );	
//	}

	DEBUG_ADD("MP3 new");
#ifdef __USE_MP3__
	if (g_pMP3 == NULL)
	{
		g_pMP3 = new CMP3;

	DEBUG_ADD("MP3 new OK");
		int value = g_pUserOption->VolumeMusic;	

		LONG volume = value*16*257;// value*SOUND_DEGREE + SOUND_MIN;

	DEBUG_ADD("MP3 SetVolume1");
		g_pMP3->SetVolume( volume );	
	DEBUG_ADD("MP3 SetVolume1 OK");
	}
#else
	if( g_SDLAudio.IsInit() )
	{
		// SDL backend stub - OGG streaming not implemented
		if( g_pSoundBufferForOGG == NULL )
		{
			// g_pSoundBufferForOGG = new CDirectSoundBuffer(g_hWnd, SOUND_STEREO, SOUND_44K, SOUND_16BIT);
			g_pSoundBufferForOGG = NULL; // Stub: not implemented on SDL
		}
		if( g_pOGG == NULL )
		{
#ifdef _MT
			g_pOGG = new COGGSTREAM(NULL, g_pSoundBufferForOGG, 44100, 11025, 8800, 1);
#else
			g_pOGG = new COGGSTREAM(NULL, g_pSoundBufferForOGG, 44100, 11025, 8800,1);
#endif
			int volume = (g_pUserOption->VolumeMusic - 15) * 250;

			g_pOGG->streamVolume( max( -10000, min( -1, volume ) ) );
		}
	}
#endif

	return TRUE;
}

//---------------------------------------------------------------------------
// Init Draw
//---------------------------------------------------------------------------
BOOL
InitDraw()
{
	// Debug Message
	DEBUG_ADD("[ InitGame ]  Draw");

	//--------------------------------------------------------
	// 3D가속 여부를 ClientConfig에서 읽어온다.
	//--------------------------------------------------------
	g_bHAL = (g_pUserOption->Use3DHAL)? true : false;

//	bool g_bUseIMEWindow = (g_pUserOption->Chinese) ? true : false;

	//bool g_bUseIMEWindow = gC_ci->IsChinese() == true;
	bool g_bUseIMEWindow = true;

	//--------------------------------------------------------
	//
	//                 DirectDraw
	//
	//--------------------------------------------------------
// The display, through CSDLGraphics on every platform: on Windows it wraps
// the Win32 window InitApp made, off Windows it creates the SDL window
// itself. The three PLATFORM_WINDOWS guards that used to fence this step
// (with an "SDL backend - no DirectX initialization" message in the gap)
// dated from when SDLMain.cpp made its own window; nothing here is Win32.
	//if (g_bHAL)
	if( g_bHAL )
	{
		if (g_bFullScreen)
		{
			// add by Sonic 2006.9.26
			if(g_MyFull)
			{
				CSDLGraphics::Init(g_hWnd, 1024, 768, CSDLGraphics::FULLSCREEN, true, g_bUseIMEWindow);
			}
			else
			{
				CSDLGraphics::Init(g_hWnd, 800, 600, CSDLGraphics::FULLSCREEN, true, g_bUseIMEWindow);
			}
		}
		else
		{
			if(g_MyFull)
			{
				CSDLGraphics::Init(g_hWnd, 1024, 768, CSDLGraphics::WINDOWMODE, true, g_bUseIMEWindow);
			}
			else
			{
				CSDLGraphics::Init(g_hWnd, 800, 600, CSDLGraphics::WINDOWMODE, true, g_bUseIMEWindow);
			}
		}
	}

		//--------------------------------------------------------
		// Video Memory 얼마인가?
		//--------------------------------------------------------
		// CSDLGraphics::GetDD()는 항상 nullptr을 반환하는 스텁이고
		// IDirectDraw는 정의가 없는 불완전 타입이라 실제 DirectDraw VidMem
		// 조회는 더 이상 불가능함(SDL2 마이그레이션으로 죽은 코드).
		// 최신 GPU 기준 아래 8M 문턱값은 항상 넘기므로 충분하다고 간주한다.
		DWORD dwTotal = 256 * 1024 * 1024;
		DWORD dwFree = 256 * 1024 * 1024;

		DEBUG_ADD_FORMAT("[VidemoMemory] SDL2 - assuming sufficient video memory = %d/%d", dwFree, dwTotal);

		DEBUG_ADD("[ InitGame ]  Init Draw OK");

		BOOL enoughMemory = TRUE;

		//--------------------------------------------------------
		// 8M 이상 남아있는 경우만 3D 가속한다.. 흠.. - -;;
		//--------------------------------------------------------
		if (dwFree < 8388608)
		{
			enoughMemory = FALSE;
		}

		//--------------------------------------------------------
		// 하드웨어 가속 사용 가능
		//--------------------------------------------------------
		bool bUse3D = ( enoughMemory && g_bHAL && ( true ));

		if (bUse3D)
		{
			DEBUG_ADD("[ InitGame ]  Init 3D");

			// CDirect3D::Init() removed (SDL2) - SDL2 always uses hardware acceleration
			g_bEnable3DHAL = TRUE;
			g_bHALAvailable = true;

			/*
			D3DDEVICEDESC7 devDesc;
			CDirect3D::GetDevice()->GetCaps( &devDesc );

			#ifdef	OUTPUT_DEBUG
				if (devDesc.dwDevCaps & D3DDEVCAPS_TEXTURENONLOCALVIDMEM)
				{
					DEBUG_ADD_FORMAT("D3DDEVCAPS_TEXTURENONLOCALVIDMEM Enable");
				}
				else
				{
					DEBUG_ADD_FORMAT("D3DDEVCAPS_TEXTURENONLOCALVIDMEM Disable");
				}
			#endif
			*/
		}
		
		//--------------------------------------------------------
		// 하드웨어 가속 사용 불가능
		//--------------------------------------------------------
		// Only after the HAL path above created the display: without g_bHAL
		// the block below this one does the single 2D init, and this must not
		// add a second (it used to - see the comment on that block).
		if (g_bHAL && !bUse3D)
		{
			DEBUG_ADD("[ InitGame ]  3D Failed. Release Draw and Re-Init 2D");

			CSDLGraphics::ReleaseAll();

			if (g_bFullScreen)
			{
				// add by Sonic 2006.9.26
				if(g_MyFull)
				{
					CSDLGraphics::Init(g_hWnd, 1024, 768, CSDLGraphics::FULLSCREEN, false, g_bUseIMEWindow);
				}
				else
				{
					CSDLGraphics::Init(g_hWnd, 800, 600, CSDLGraphics::FULLSCREEN, false, g_bUseIMEWindow);
				}
			}
			else
			{
				if(g_MyFull)
				{
					CSDLGraphics::Init(g_hWnd, 1024, 768, CSDLGraphics::WINDOWMODE, true, g_bUseIMEWindow);
				}
				else
				{
					CSDLGraphics::Init(g_hWnd, 800, 600, CSDLGraphics::WINDOWMODE, true, g_bUseIMEWindow);
				}
			}

			g_bEnable3DHAL = FALSE;
		}

	//--------------------------------------------------------
	//
	// No 3D acceleration at all: the plain 2D display. This was an `else`
	// that paired with `if (!bUse3D)` above rather than with `if (g_bHAL)`,
	// so with the HAL on the display was initialised twice at start-up
	// (create, destroy, create off Windows), and with it off the 3D-failed
	// block above did the work and this one was skipped.
	//
	//--------------------------------------------------------
	if (!g_bHAL)
	{
		DEBUG_ADD("[ InitGame ]  Init 2D");

		if (g_bFullScreen)
		{
			if(g_MyFull)
			{
				CSDLGraphics::Init(g_hWnd, 1024, 768, CSDLGraphics::FULLSCREEN, false, g_bUseIMEWindow);
			}
			else
			{
				CSDLGraphics::Init(g_hWnd, 800, 600, CSDLGraphics::FULLSCREEN, false, g_bUseIMEWindow);
			}
		}
		else
		{
			if(g_MyFull)
			{
				CSDLGraphics::Init(g_hWnd, 1024, 768, CSDLGraphics::WINDOWMODE, true, g_bUseIMEWindow);
			}
			else
			{
				CSDLGraphics::Init(g_hWnd, 800, 600, CSDLGraphics::WINDOWMODE, true, g_bUseIMEWindow);
			}
		}
	}

	CSpriteSurface::InitEffectTable();

	// Platform-neutral (it reads CSDLGraphics::Is565() and picks a function);
	// it was behind PLATFORM_WINDOWS, leaving s_GammaFunction NULL elsewhere.
	CDirectDrawSurface::SetGammaFunction();

	return TRUE;
}

//-----------------------------------------------------------------------------
// LoadingThreadProc
//-----------------------------------------------------------------------------
LONG
LoadingThreadProc(LPVOID lpParameter)
{
	(void)lpParameter;
	if (g_pLoadingThread!=NULL)
	{
		g_pLoadingThread->Execute();
	}

	return 0L;
}

//-----------------------------------------------------------------------------
// Init Thread
//-----------------------------------------------------------------------------
BOOL
InitThread()
{
	// Debug Message
	DEBUG_ADD("[ InitGame ]  Thread");

	/*
	DWORD	dwChildThreadID;

	// init event for nonsignaled
	g_hFileEvent = CreateEvent(NULL, TRUE, FALSE, NULL);	

	g_hFileThread = CreateThread(NULL, 
								0,	// default stack size
								(LPTHREAD_START_ROUTINE)FileThreadProc,
								NULL,
								NULL,
								&dwChildThreadID);

	SetThreadPriority(g_hFileThread, 
						//THREAD_PRIORITY_NORMAL
						THREAD_PRIORITY_BELOW_NORMAL
						);
	*/

	if (g_pLoadingThread!=NULL)
	{
		delete g_pLoadingThread;
	}
	g_pLoadingThread = new MWorkThread;
	g_pLoadingThread->Init( (LPTHREAD_START_ROUTINE)LoadingThreadProc );

	g_pLoadingThread->SetPriority( THREAD_PRIORITY_LOWEST );

	return TRUE;
}

//---------------------------------------------------------------------------
// Stop LoadingThread
//---------------------------------------------------------------------------
void
StopLoadingThread()
{
	//-------------------------------------------------------------
	// Loading 중이던것 모두 제거
	//-------------------------------------------------------------
	if (g_pLoadingThread!=NULL)
	{
		DEBUG_ADD_FORMAT("Thread-ReleaseWork: size=%d, working=%d, finish=%d, stop=%d", 
									g_pLoadingThread->GetSize(),
									(int)g_pLoadingThread->IsWorking(),
									(int)g_pLoadingThread->IsFinishCurrentWork(),
									(int)g_pLoadingThread->IsStopWork());
		
		g_pLoadingThread->SetPriority( THREAD_PRIORITY_HIGHEST );
		g_pLoadingThread->ReleaseWork();
		g_pLoadingThread->SetPriority( THREAD_PRIORITY_LOWEST );

		//-------------------------------------------------------------
		// 작업 중지가 끝날때까지 기다림
		//-------------------------------------------------------------
		DEBUG_ADD_FORMAT("Thread-WaitingForStop: size=%d, working=%d, finish=%d, stop=%d", 
									g_pLoadingThread->GetSize(),
									(int)g_pLoadingThread->IsWorking(),
									(int)g_pLoadingThread->IsFinishCurrentWork(),
									(int)g_pLoadingThread->IsStopWork());
		
		while (g_pLoadingThread->IsStopWork());

		DEBUG_ADD("Thread-End Waiting");
	}
}

//---------------------------------------------------------------------------
// update loop의 정보를 설정한다.
//---------------------------------------------------------------------------
BOOL
InitGameUpdate()
{
	if (g_pCOpeningUpdate!=NULL)
	{
		delete	g_pCOpeningUpdate;
	}

	if (g_pCGameUpdate!=NULL)
	{
		delete	g_pCGameUpdate;
	}

	if (g_pCWaitPacketUpdate!=NULL)
	{
		delete	g_pCWaitPacketUpdate;
	}

	if (g_pCWaitUIUpdate!=NULL)
	{
		delete	g_pCWaitUIUpdate;
	}

	g_pCOpeningUpdate		= new COpeningUpdate;
	g_pCGameUpdate			= new CGameUpdate;
	g_pCWaitPacketUpdate	= new CWaitPacketUpdate;
	g_pCWaitUIUpdate		= new CWaitUIUpdate;

	return TRUE;
}

//---------------------------------------------------------------------------
// Surface의 정보를 생성한다.
//---------------------------------------------------------------------------
BOOL
InitGame()
{
	//---------------------------------------------------------------------
	// Initialize logging system first (before everything else)
	//---------------------------------------------------------------------
	log_init();

	// Always enable console output and INFO level logging
	// This ensures DEBUG_ADD macros and panic messages are visible
	log_set_console_output(true);

	// Debug builds also write the same lines to a file beside the executable.
	// The client is normally run from the Visual Studio debugger, where the
	// stderr console is easy to lose; the file is overwritten on every launch.
#ifdef _DEBUG
	log_set_file_output("DarkEden-debug.log");
#endif

// Conditional compilation: Remove DEBUG logs in Release builds
#ifdef _DEBUG
	log_set_level(LOG_LEVEL_DEBUG);
#else
	log_set_level(LOG_LEVEL_INFO);
#endif

	g_pFileDef = new Properties;
	g_pFileDef->setProperty("RESOURCE_TEXT_ENCODING", "CP949");
	g_pFileDef->load("Data/Info/FileDef.inf");
	const auto resourceEncoding = g_pFileDef->getProperty("RESOURCE_TEXT_ENCODING");
	if (!TextEncoding::SetResourceEncoding(resourceEncoding))
		throw Error("Unsupported RESOURCE_TEXT_ENCODING in Data/Info/FileDef.inf");

	//---------------------------------------------------------------------
	// Profiler
	//---------------------------------------------------------------------
	#ifdef OUTPUT_DEBUG
		if (g_pProfiler!=NULL)
		{
			g_pProfiler->Release();
		}
		else
		{
			g_pProfiler = new Profiler;
		}
	#endif

	//---------------------------------------------------------------------
	// MathTable
	//---------------------------------------------------------------------
	MathTable::FCreateSines();

	//---------------------------------------------------------------------
	// Effect Orbit Table
	//---------------------------------------------------------------------
	MAttachOrbitEffect::InitOrbitPosition();

	//---------------------------------------------------------------------
	// Init 
	//---------------------------------------------------------------------
	if (g_pGameStringTable==NULL)
	{
		g_pGameStringTable = new MStringArray;
	}

	if (g_pSystemMessage==NULL)
	{
		g_pSystemMessage = new CMessageArray;
	}

	if (g_pPlayerMessage==NULL)
	{
		g_pPlayerMessage = new CMessageArray;
	}
	
	if (g_pGameMessage==NULL)
	{
		g_pGameMessage = new CMessageArray;
	}

	if (g_pHelpMessage==NULL)
	{
		g_pHelpMessage = new CMessageArray;
	}
	
	if (g_pNoticeMessage == NULL )
	{
		g_pNoticeMessage = new CMessageArray;
	}

	// 2004, 6, 18 sobeit add start - nick name table 
	if (g_pNickNameStringTable==NULL)
	{
		g_pNickNameStringTable = new MStringArray;
	}
	//yckou begin: check invalid *.dll
	// An older, shorter copy of the DLL whitelist in Client.cpp; here an
	// unlisted name only breaks out of the walk, and g_wAuthKeyMap is set
	// only when every entry of a non-empty listing is whitelisted.
	std::string InvalidDll;
	std::vector<Basic::SDirectoryEntry> vDllEntries;

	if (Basic::ListDirectory(".", "*.dll", vDllEntries, Basic::LIST_FILES_AND_DIRECTORIES))
	{
		for (size_t iDll=0; iDll<vDllEntries.size(); iDll++)
		{
			InvalidDll = vDllEntries[iDll].sName;

			size_t iLen = InvalidDll.size();
			for (size_t j=0;j<iLen;j++)
			{
				if(isupper((unsigned char)InvalidDll[j]) != 0)
					InvalidDll[j] = (char)tolower((unsigned char)InvalidDll[j]);
			}

			if(InvalidDll != "timer.dll" &&
				InvalidDll != "msvcrtd.dll" &&
				InvalidDll != "msvcrt.dll" &&
				InvalidDll != "msvcp60.dll" &&
				InvalidDll != "msvcirt.dll" &&
				InvalidDll != "ifc22.dll" &&
				InvalidDll != "gl.dll" &&
				InvalidDll != "dsetup32.dll" &&
				InvalidDll != "dsetup.dll" &&
				InvalidDll != "deutil.dll" &&
				InvalidDll != "basics.dll" &&
				InvalidDll != "npx.dll" &&
				InvalidDll != "npupdate0.dll" &&
				InvalidDll != "nppsk.dll" &&
				InvalidDll != "npgmup.dll" &&
				InvalidDll != "npfgm9x.dll" &&
				InvalidDll != "npcipher.dll" &&
				InvalidDll != "mother.dll" &&
				InvalidDll != "fl.dll" &&
				InvalidDll != "dbghelp.dll" &&
				InvalidDll != "darkedenweb.dll" &&
				InvalidDll != "npchk.dll" &&
				InvalidDll != "xerces-c_2_4_0.dll")
				break;

			if (iDll + 1 == vDllEntries.size())
			{
				g_wAuthKeyMap = 0x5154;
			}
		}
	}
	//yckou end
	//---------------------------------------------------------------------
	// nick name string table Loading
	//---------------------------------------------------------------------
	std::ifstream gameStringTableTable;//(FILE_INFO_gameStringTable, ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_INFO_NICKNAME_STRING").c_str(), gameStringTableTable))
		return FALSE;
	(*g_pNickNameStringTable).LoadFromFile_NickNameString(gameStringTableTable);
	gameStringTableTable.close();


	// 2004, 6, 18 sobeit add end - nick name table 
	//---------------------------------------------------------------------
	// GameStringTable Loading
	//---------------------------------------------------------------------
	// String.inf carries the localised UI strings that ship with the game data.
	// In English the built-in table from MGameStringTable.cpp is applied on top
	// of it, so the English UI does not depend on an English String.inf.
	const bool bUseEnglishStrings = UseEnglishText(g_pFileDef);

	std::ifstream gameStringTableTable2;//(FILE_INFO_gameStringTable, ios::binary);
	if (FileOpenBinary(g_pFileDef->getProperty("FILE_INFO_STRING").c_str(), gameStringTableTable2))
	{
		(*g_pGameStringTable).LoadFromFile(gameStringTableTable2);

		// The entries just read are used as sprintf format strings in
		// hundreds of places, so drop the ones that could write through an
		// argument, take a width from an argument, demand large argument
		// driven expansion, or retype an argument. Only the file data needs
		// this; the built-in table applied below is source.
		//
		// Note what this does and does not reach. English is this build's
		// default (see UseEnglishText), and on that path InitGameStringTable
		// below opens with (*g_pGameStringTable).Init(MAX_GAME_STRING + 1),
		// whose CTypeTable::Init calls Release() and reallocates the array -
		// so every entry this pass just scrubbed is destroyed a few lines
		// later and rebuilt from the source literals. The scrub therefore has
		// no effect in the default build; it protects LANGUAGE != 3, where
		// String.inf really is the table the client formats with. The
		// ordering is deliberate and stays: the built-in English strings are
		// source, not data, and do not need gating.
		//
		// It also does not check that an entry's specifier count matches the
		// call site - that is unknowable here - so an entry with a %s the
		// call site passes no argument for still reads a stack word as a
		// char*. The zero-vararg call sites are being converted to bounded
		// "%s" copies separately, and that is what closes that hole.
		SanitizeGameStringTable();

		gameStringTableTable2.close();
	}
	else if (!bUseEnglishStrings)
	{
		// Without String.inf and without the built-in strings there is no UI
		// text at all, so a missing file is still fatal for other languages.
		return FALSE;
	}

	if (bUseEnglishStrings)
	{
		InitGameStringTable();
	}


	//---------------------------------------------------
	// System Message
	//---------------------------------------------------
	g_pSystemMessage->Init(MAX_SYSTEMMESSAGE, 256);

	//---------------------------------------------------
	// Player Message
	//---------------------------------------------------
	g_pPlayerMessage->Init(MAX_PLAYERMESSAGE, 256);

	g_pNoticeMessage->Init(7, 256);

	//---------------------------------------------------
	// Game Message
	//---------------------------------------------------
	g_pGameMessage->Init(MAX_GAMEMESSAGE, 256);

	//---------------------------------------------------
	// Help Message
	//---------------------------------------------------
	g_pHelpMessage->Init(MAX_HELPMESSAGE, 256);
	
	// Debug Message
	DEBUG_ADD("---------------[   InitGame   ]---------------");

	//----------------------------------------------------------------------
	// Socket initialization (mingw socket on Windows, BSD sockets on Unix)
	//----------------------------------------------------------------------
	DEBUG_ADD("[ InitGame ]  Socket - Socket initialized");

	// Note: on Windows the socket library is started on demand by
	// SocketAPI::socket_ex(), which every socket this client creates goes
	// through. Winsock does require WSAStartup even when building with mingw.

//	#ifdef _DEBUG
//		bool bMerge = false;
//
//		if (bMerge)
//		{
//			MGuildMarkManager::MergeGuildMark( FILE_SPRITE_GUILD_MARK, 
//												FILE_INFO_GUILD_INFO_MAPPER,
//												"data\\GuildMark.spk",
//												"data\\GuildMark.inf");
//		}
//	#endif

	//--------------------------------------------------------
	//
	//                 Init...
	//
	//--------------------------------------------------------
	if (!InitClientConfig()) return false;
	if (!InitInfomation()) return false;
	if (!InitUserOption()) return false;

	if (!InitVolume()) return false;
	if (!InitInput()) return false;
	// if (!InitSound()) return false;
	if (!InitDraw()) return false;

	// InitThread() // 2001.8.20 주석처리 - 로딩 Thread사용 안함

	// GameObject must be initialized before Surface
	// because gC_vs_ui.Init() in InitSurface() depends on g_pMoneyManager
	if (!InitGameObject()) return false;
	if (!InitSurface()) return false;

	if (!InitGameUpdate()) return false;

	// --- 모든 초기화 성공 후 로직 계속 ---
	{	
		//yckou
		CheckInvalidProcess();
		UI_AffectUserOption();
		//----------------------------------------
		// guild 마크 생성부분
		//----------------------------------------
		/*
		#ifdef _DEBUG
			g_pGuildMarkManager->CreateGuildMark( 1, "Data\\Guild\\Guild1.bmp" );
			g_pGuildMarkManager->CreateGuildMark( 2, "Data\\Guild\\Guild2.bmp" );
			g_pGuildMarkManager->CreateGuildMark( 3, "Data\\Guild\\Guild3.bmp" );
			g_pGuildMarkManager->CreateGuildMark( 4, "Data\\Guild\\Guild4.bmp" );
			g_pGuildMarkManager->CreateGuildMark( 5, "Data\\Guild\\Guild5.bmp" );
			g_pGuildMarkManager->CreateGuildMark( 6, "Data\\Guild\\Guild6.bmp" );
			g_pGuildMarkManager->CreateGuildMark( 7, "Data\\Guild\\Guild7.bmp" );
		#endif
		*/


		//----------------------------------------
		// 내 profile 초기화
		//----------------------------------------
		ProfileManager::DeleteProfiles();		// 기존에거 모두 제거
		ProfileManager::InitProfiles();			// profile 생성


			//----------------------------------------
			// Player 캐릭터 그림 Load
			//----------------------------------------
			//UI_DrawProgress(70);

			//LoadCreature( 0 );
			//UI_DrawProgress(80);

			//LoadCreature( 1 );
			//UI_DrawProgress(90);

			//LoadCreature( 2 );
			//UI_DrawProgress(95);

			//LoadCreature( 3 );
			//UI_DrawProgress(100);
		
			//gC_vs_ui.EndProgress();

			// Login Mode로 시작한다.
			//SetMode( MODE_OPENING );

			// Debug: Check g_Mode before and after SetMode
			extern enum CLIENT_MODE g_Mode;
			printf("DEBUG: Before SetMode, g_Mode = %d\n", g_Mode);
			SetMode( MODE_MAINMENU );
			printf("DEBUG: After SetMode, g_Mode = %d\n", g_Mode);
			//SetMode( MODE_WAIT_POSITION );
		///*
		// Server에 접속이 안될 때 사용하는 code
		//*/

		// Debug Message
		#ifdef	OUTPUT_DEBUG			
			// Fullscreen?
			if (CSDLGraphics::IsFullscreen())	
			{
				DEBUG_ADD("# Fullscreen.");
			}
			else
			{
				DEBUG_ADD("# Windowed Mode.");
			}

			// 565?
			if (CSDLGraphics::Is565())	
			{
				DEBUG_ADD("# 5:6:5 mode");
			}
			else
			{
				DEBUG_ADD("# 5:5:5 mode or ???.");
			}

			// 3D HAL?
			if (true)	
			{
				DEBUG_ADD("# 3D Hardware Accel.");
			}
			
			DEBUG_ADD("---------------[  InitGame OK  ]---------------");
		#endif
	
		return TRUE;
	}

	// Init가 실패한 경우
	return FALSE;	
}

//-----------------------------------------------------------------------------
// Init Socket
//-----------------------------------------------------------------------------
BOOL
InitSocket()
{
	// 일단 release
	ReleaseSocket();

	// Debug Message
	DEBUG_ADD("[ InitGame ]  Socket");


	//----------------------------------------------------------------------
	// 환경 파일을 읽어들인다.
	// 실행 파일은 $VSHOME/bin에, 환경 파일은 $VSHOME/conf 에 존재해야 한다.?	// command line 에서 환경 파일을 지정할 수 있도록 한다.
	//----------------------------------------------------------------------

	try {
		DEBUG_ADD("[ InitGame ]  Socket - Before new Properties");

		std::string configFilename;

		if( !g_pUserInformation->bKorean )
		{
			configFilename = g_pFileDef->getProperty("FILE_INFO_SERVERINFO").c_str();
			g_pConfigForeign = new ServerInfoFileParser( configFilename );
		}
		else
		{
			switch(g_Dimension)
			{
				//		case 0:
				//			configFilename = FILE_INFO_GAMECLIENT;
				//			break;
				
			case 1:
				configFilename = g_pFileDef->getProperty("FILE_INFO_GAMECLIENT2").c_str();
				break;
				
			default:
				configFilename = g_pFileDef->getProperty("FILE_INFO_GAMECLIENT").c_str();
				break;
			}
			
			g_pConfigKorean = new Properties();
			g_pConfigKorean->load(configFilename);			
		}

	} catch ( Throwable& t ) 	
	{
		//InitFail(t.toString().c_str());				

		DEBUG_ADD_ERR(t.toString().c_str());

		SetMode( MODE_MAINMENU );
		UpdateDisconnected();


		return FALSE;
	}	

	//----------------------------------------------------------------------
	// Create Socket 
	//----------------------------------------------------------------------
	Socket * pSocket = NULL;
	try {		
		DEBUG_ADD("[ InitGame ]  Socket - Before new PacketFactoryManager");

		// 패킷 팩토리 매니저를 생성, 초기화한다.
		g_pPacketFactoryManager = new PacketFactoryManager();

		// Composition root for migrated packet handlers (RESTRUCTURING.md
		// task 2.2): fill the PacketDispatcher table before any
		// connection can deliver a packet.
		registerClientPacketHandlers();

		DEBUG_ADD("[ InitGame ]  Socket - Before new PacketValidator");

		g_pPacketValidator = new PacketValidator();

		DEBUG_ADD("[ InitGame ]  Socket - Before Init...");

		g_pPacketFactoryManager->init();
		g_pPacketValidator->init();
		
		DEBUG_ADD("[ InitGame ]  Socket - Before new Socket...");

		//---------------------------------------------------------
		// Address 골라서 접속하기
		//---------------------------------------------------------
		// 최근에 접속시도를 했던 서버 주소 번호..
		static std::uint64_t previousTryServer = 0;

		{
			try {
				const LoginEndpointOptions options = {
					.attempt = previousTryServer,
					.launcherAddress = g_FutecIP,
					.launcherPort = g_FutecPort,
					.hostOverride = NetworkTransport::ReadEnvironment("DARKEDEN_LOGIN_HOST"),
					.portOverride = NetworkTransport::ReadEnvironment("DARKEDEN_LOGIN_PORT"),
				};
				const auto endpoint = g_pUserInformation->bKorean
					? SelectLoginEndpoint(*g_pConfigKorean, options)
					: SelectLoginEndpoint(*g_pConfigForeign, g_Dimension, options);
				std::string ServerAddress = endpoint.address;
				const uint port = endpoint.port;
				// Resolve a domain name here. Through a gateway the name is
				// passed on unchanged; the gateway routes the advertised endpoint.
				if (!NetworkTransport::UsesWebSocket() && (ServerAddress[0] < '0' || ServerAddress[0] > '9'))
				{
					struct hostent* h;

					if ((h=gethostbyname(ServerAddress.c_str()))==NULL)
					{
						// -_-;
						throw ConnectException("LoginServer의 주소를 찾을 수 없습니다.");
					}

					char* pIP = (char*)inet_ntoa(*((struct in_addr*)h->h_addr));

					ServerAddress = pIP;
				}
				
				//yckou: prohibit user connecting to local machine
				//--------------------------------------------------------------------
/*				struct hostent * phost;
				char szhostname[128];
				struct in_addr MyInAddr;
				gethostname(szhostname, 128); 
				phost = gethostbyname(szhostname);
				
				hostent& he = *phost;
				//in_addr* addr = (in_addr*) *(phost->h_addr_list)
				//add by sonic 2006.4.10 쇱꿎踏狗젯쌈륩蛟포뒈囹
				//ServerAddress = cmpServerAddress;//퓻契립令륩蛟포뒈囹,렝岺놔댄
				//ServerAddress = "59.34.148.238";
				if ( ServerAddress[0] == 0x31 
					&& ServerAddress[1] == 0x32
					&& ServerAddress[2] == 0x37)
				{
					ServerAddress = "0.0.0.0";
				}
				if(ServerAddress == "127.0.0.1")
				{
//					abort();
					g_bNeedUpdate = TRUE;
					SetMode(MODE_QUIT);
					g_ModeNext = MODE_QUIT;
					return FALSE;
					//ServerAddress = "211.155.231.173";
				}
				else
				{
					for(int nAdapter=0; he.h_addr_list[nAdapter]; nAdapter++)
					{
						//MyInAddr.s_addr=*((unsigned long *)phost->h_addr_list[nAdapter]);
						memcpy ( &MyInAddr.s_addr, he.h_addr_list[nAdapter],he.h_length);
						if(ServerAddress == inet_ntoa(MyInAddr))
						{
//							ServerAddress = "211.155.231.173";
//							break;
//							abort();
							g_bNeedUpdate = TRUE;
							SetMode(MODE_QUIT);
							g_ModeNext = MODE_QUIT;
							return FALSE;
						}	
//					}
				}
*/
				//--------------------------------------------------------------------
				DEBUG_ADD_FORMAT("Conneting( %s : %d )", ServerAddress.c_str() , port);				

				pSocket = new Socket( ServerAddress , port );

				DEBUG_ADD("[ InitGame ]  Socket - Before connect");

				// try to connect to server
				pSocket->connect();
/*				if ( ServerAddress[0] == 0x31 
					&& ServerAddress[1] == 0x32
					&& ServerAddress[2] == 0x37)
				{
					return FALSE;
				}
*/
//				SOCKADDR_IN sa;
//				int lenSA = sizeof(sa);
//				getsockname( pSocket->getSOCKET(), (sockaddr*)&sa, &lenSA );
//				char str[128];
//				sprintf(str, inet_ntoa(sa.sin_addr));	// str : 내IP

				// connect가 된 경우..
				//break;

			} catch ( ConnectException& ) {

				if (pSocket!=NULL)
				{
					delete pSocket;
					pSocket = NULL;
				}

				// 실패했으니까 다음 주소로..
				previousTryServer ++;

				throw;
			}
		}



		// make nonblocking socket
	    pSocket->setNonBlocking();

		// make no-linger socket
		pSocket->setLinger(0);

		DEBUG_ADD("[ InitGame ]  Socket - new ClientPlayer");

	    // create player
		g_pSocket = new ClientPlayer(pSocket);
		pSocket = NULL;
	 
	} catch ( Throwable & t ) {	
		//MessageBox( g_hWnd, t.toString().c_str(), NULL, MB_OK );
		//InitFail(t.toString().c_str());
		DEBUG_ADD(t.toString().c_str());

		if (pSocket!=NULL)
		{
			delete pSocket;
		}

		SetMode( MODE_MAINMENU );		
		UpdateDisconnected();


		return FALSE;
	}

	//----------------------------------------------------------------------
	// RequestServerPlayerManager
	//----------------------------------------------------------------------

	if (g_pRequestServerPlayerManager!=NULL)
	{
		DEBUG_ADD("[ InitGame ]  delete RequestServerPlayerManager");
		delete g_pRequestServerPlayerManager;
		g_pRequestServerPlayerManager = nullptr;
	}

	if (g_pClientCommunicationManager!=NULL)
	{
		DEBUG_ADD("[ InitGame ]  delete ClientCommunicationManager");
		delete g_pClientCommunicationManager;
	}

	if (g_pRequestUserManager!=NULL)
	{
		DEBUG_ADD("[ InitGame ]  delete g_pRequestUserManager");
		delete g_pRequestUserManager;		
	}

	if (g_pRequestFileManager!=NULL)
	{
		DEBUG_ADD("[ InitGame ]  delete g_pRequestFileManager");
		delete g_pRequestFileManager;		
	}
	

	DEBUG_ADD("[ InitGame ] new ClientCommunicationManager");
	g_pClientCommunicationManager = new ClientCommunicationManager;

	#ifdef __METROTECH_TEST__
		g_UDPTest.Init();
	#endif

	// RequestServer functionality is for server mode only - Windows only
#ifdef PLATFORM_WINDOWS
	if (g_pClientConfig->MAX_REQUEST_SERVICE > 0 && !NetworkTransport::UsesWebSocket())
	{
		DEBUG_ADD("[ InitGame ] new RequestServerPlayerManager");
		g_pRequestServerPlayerManager = new RequestServerPlayerManager;

		DEBUG_ADD("[ InitGame ] RequestServerPlayerManager Init");
		g_pRequestServerPlayerManager->Init();
	}
#else
	// The request service is Windows-only; the managers still compile
	// everywhere as packetwire members.
	DEBUG_ADD("[ InitGame ] RequestServer functionality Windows only");
#endif


	DEBUG_ADD("[ InitGame ] new g_pRequestUserManager");
	g_pRequestUserManager = new RequestUserManager;

	DEBUG_ADD("[ InitGame ] new g_pRequestFileManager");
	g_pRequestFileManager = new RequestFileManager;



	DEBUG_ADD("[ InitGame ]  Socket OK");

	return TRUE;
}


//-----------------------------------------------------------------------------
// ReleaseGameUpdate()
//-----------------------------------------------------------------------------
void
ReleaseGameUpdate()
{
	if (g_pCOpeningUpdate!=NULL)
	{
		delete	g_pCOpeningUpdate;
		g_pCOpeningUpdate = NULL;
	}

	if (g_pCGameUpdate!=NULL)
	{
		delete	g_pCGameUpdate;
		g_pCGameUpdate = NULL;
	}

	if (g_pCWaitPacketUpdate!=NULL)
	{
		delete	g_pCWaitPacketUpdate;
		g_pCWaitPacketUpdate = NULL;
	}

	if (g_pCWaitUIUpdate!=NULL)
	{
		delete	g_pCWaitUIUpdate;
		g_pCWaitUIUpdate = NULL;
	}

}

//-----------------------------------------------------------------------------
// Name: ReleaseAllObjects()
// Desc: Finished with all objects we use; release them
//-----------------------------------------------------------------------------
void ReleaseAllObjects()
{
	DEBUG_ADD("---------------[ Start ReleaseAllObjects ]---------------");

	g_bActiveGame = false;

	EndTitleLoading();
	
	//----------------------------------------------------------------
	// GameUpdate 제거
	//
	// update loop 안에서 이걸 실행하면 안된다.
	//----------------------------------------------------------------
	ReleaseGameUpdate();

	/*
	if (g_pMP3 != NULL)
	{
		g_pMP3->Pause();
		delete g_pMP3;
	}
	*/

#ifndef __USE_MP3__

	if(g_pOGG != NULL )
	{		
		g_pOGG->streamClose();
		delete g_pOGG;
		g_pOGG = NULL;
		if( g_oggfile != NULL )
		{
			fclose(g_oggfile);
			g_oggfile = NULL;
		}
		g_pSoundBufferForOGG = NULL;
	}
#endif

	//----------------------------------------------------------------
	//
	//				 thread
	//
	//----------------------------------------------------------------
	/*
	if (g_hFileThread != 0)
	{
		StopFileThread();
		TerminateThread(g_hFileThread, 0);
		CloseHandle(g_hFileEvent);
	}
	*/
	DEBUG_ADD("[Release] Work Thread");

	if (g_pLoadingThread!=NULL)
	{
		delete g_pLoadingThread;
		g_pLoadingThread = NULL;
	}

	
	//----------------------------------------------------------------
	// 
	//		Global 정보 제거
	//
	//----------------------------------------------------------------
	ReleasePacketItemTable();

	
	//----------------------------------------------------------------
	// UI 제거
	//----------------------------------------------------------------
	if (g_pUIDialog!=NULL)
	{
		DEBUG_ADD("[Release] UI Dialog");
		g_pUIDialog->Release();
	}

	DEBUG_ADD("[Release] UI");
	gC_vs_ui.Release();


	//----------------------------------------------------------------
	//
	//					Game관련 정보
	//
	//----------------------------------------------------------------
	
	//----------------------------------------------------------------
	// Weather
	//----------------------------------------------------------------
	DEBUG_ADD("[Release] Weather");
	if (g_pWeather!=NULL)
	{
		delete g_pWeather;
		g_pWeather = NULL;
	}

	//----------------------------------------------------------------
	// EffectManager
	//----------------------------------------------------------------
	DEBUG_ADD("[Release] Inventory Effect Manager");
	if (g_pInventoryEffectManager != NULL)
	{
		delete g_pInventoryEffectManager;
		g_pInventoryEffectManager = NULL;
	}
	
	//----------------------------------------------------------------
	// Player 삭제
	//----------------------------------------------------------------
	DEBUG_ADD("[Release] g_pPlayer");
	if (g_pPlayer!=NULL)
	{
		if (g_pZone!=NULL)
		{
			g_pZone->RemoveCreature( g_pPlayer->GetID() );
		}
		SAFE_DELETE( g_pPlayer );		
	}

	//----------------------------------------------------------------
	// Zone
	//----------------------------------------------------------------
	DEBUG_ADD("[Release] Zone");
	SAFE_DELETE( g_pZoneSmall );
	SAFE_DELETE( g_pZoneLarge );
	g_pZone = NULL;
	
	DEBUG_ADD("[Release] MCreature::MoveTable");
	MCreature::ReleaseMoveTable();

	
	DEBUG_ADD("[Release] DirectInput");

	//----------------------------------------------------------------
	//
	//				System	& Util..
	//
	//----------------------------------------------------------------
	//----------------------------------------------------------------
	// Input
	//----------------------------------------------------------------
	SAFE_DELETE( g_pSDLInput );
	//----------------------------------------------------------------
	// Music
	//----------------------------------------------------------------
	DEBUG_ADD("[Release] Music");
	g_Music.Stop();
	g_Music.UnInit();
	//g_SDLMusic.Stop();

	//----------------------------------------------------------------
	// Sound
	//----------------------------------------------------------------
//	DEBUG_ADD("[Release] SoundStream");
//	if (g_pSDLStream!=NULL)
//	{
//		delete g_pSDLStream;
//		g_pSDLStream = NULL;
//	}
	DEBUG_ADD("[Release] MP3");
	SAFE_DELETE( g_pMP3 );	
	DEBUG_ADD("[Release] ZoneSound");


	SAFE_DELETE( g_pZoneSoundTable );
	SAFE_DELETE( g_pZoneSoundManager );
#ifdef PLATFORM_WINDOWS
	// WavePackFileManager.cpp is Windows-only (mmio); off Windows the class's
	// vtable is never emitted, the pointer is never set, and UBSan's vptr
	// check on this virtual delete would need the typeinfo to link.
	SAFE_DELETE( g_pWavePackFileManager );
#endif
	
	DEBUG_ADD("[Release] Sound");

	UnInitSound();

	

	//----------------------------------------------------------------
	// Avi
	//----------------------------------------------------------------
	DEBUG_ADD("[Release] AVI");

	if (g_pAvi!=NULL)
	{
		g_pAvi->Close();
		delete g_pAvi;
		g_pAvi = NULL;
	}


	//----------------------------------------------------------------
	// UnInit DirectInput
	//----------------------------------------------------------------
	//	this->g_di.UnInitJoyDevice();
	//g_pSDLInput->UnInitMouseDevice();
	//g_pSDLInput->UnInitKeyDevice();
	//g_pSDLInput->UnInit();

	//----------------------------------------------------------------
	// View
	//----------------------------------------------------------------
	DEBUG_ADD("[Release] MTopView");
	
	if (g_pTopView!=NULL)
	{
		// updater를 실행할때만 지워준다. - -;
		// release 안 해주는게 더 빨리 제거돼서(-_-;) 안했었는데
		// updater할때는 확실히 제거해주는게 낫지 싶어서리..
		//if (g_bNeedUpdate)
		{
			g_pTopView->Release();
			delete g_pTopView;
		}

		g_pTopView = NULL;
	}

	

	//----------------------------------------------------------------
	// Cursor Surface
	//----------------------------------------------------------------
	DEBUG_ADD("[Release] g_pCursorSurface");
	SAFE_DELETE(g_pCursorSurface );
	
	//----------------------------------------------------------------
	// delete Surfaces
	//----------------------------------------------------------------
	DEBUG_ADD("[Release] Back Surface");
	SAFE_DELETE( g_pBack );
	
	DEBUG_ADD("[Release] Last Surface");
	SAFE_DELETE( g_pLast );
	

	//----------------------------------------------------------------
	// Socket
	//----------------------------------------------------------------
	DEBUG_ADD("[Release] Socket");
	ReleaseSocket();

	//----------------------------------------------------------------
	// Socket cleanup (mingw socket doesn't need explicit cleanup)
	//----------------------------------------------------------------
	DEBUG_ADD("[Release] Socket cleaned up");

	//----------------------------------------------------------------
	// Volume
	//----------------------------------------------------------------
	DEBUG_ADD("[Release] Volume");
	// Volume control is now handled by SDL_mixer

	//----------------------------------------------------------------
	// global 제거...
	//----------------------------------------------------------------
	//if (g_bNeedUpdate)
	{
		if (g_pActionInfoTable) g_pActionInfoTable->Release();	
		if (g_pCreatureSpriteTable!=NULL) g_pCreatureSpriteTable->Release();
//		if (g_pAddonSpriteTable!=NULL) g_pAddonSpriteTable->Release();
//		if (g_pCreatureActionSpriteTable!=NULL) g_pCreatureActionSpriteTable->Release();
		if (g_pCreatureTable) g_pCreatureTable->Release();
		if (g_pCreatureSpriteTypeMapper) g_pCreatureSpriteTypeMapper->Release();
		// FIXED: Delete immediately after Release to prevent use-after-free
		// Previously: Release() was called here, but SAFE_DELETE was 50+ lines later
		// This created a window where the table existed with m_pTypeInfo=NULL
		SAFE_DELETE( g_pEffectSpriteTypeTable );
		SAFE_DELETE( g_pActionEffectSpriteTypeTable );
		if (g_pEffectStatusTable) g_pEffectStatusTable->Release();
		if (g_pGameMessage!=NULL) g_pGameMessage->Release();
		if (g_pGameStringTable) g_pGameStringTable->Release();
		if (g_pInteractionObjectTable) g_pInteractionObjectTable->Release();
		if (g_pItemOptionTable) g_pItemOptionTable->Release();
		if (g_pNPCTable) g_pNPCTable->Release();
		if (g_pNPCTalkBox) g_pNPCTalkBox->Release();
		if (g_pPCTalkBox) g_pPCTalkBox->Release();
		if (g_pShopTemplateTable) g_pShopTemplateTable->Release();
		if (g_pSkillAvailable) g_pSkillAvailable->clear();
		if (g_pSkillInfoTable) g_pSkillInfoTable->Release();
		if (g_pSkillManager) g_pSkillManager->Release();
		if (g_pSlayerGear) g_pSlayerGear->Release();
		if (g_pVampireGear) g_pVampireGear->Release();
		if (g_pOustersGear) g_pOustersGear->Release();
		if (g_pInventory) g_pInventory->Release();
		if (g_pSoundManager) g_pSoundManager->Release();
		if (g_pSoundTable) g_pSoundTable->Release();
		if (g_pSystemMessage) g_pSystemMessage->Release();	
		if (g_pPlayerMessage) g_pPlayerMessage->Release();
		if (g_pUsingColorSetTable) g_pUsingColorSetTable->Release();
		if (g_pZoneTable) g_pZoneTable->Release();
		if (g_pZoneSoundManager) g_pZoneSoundManager->Release();
		if (g_pNPCScriptTable) g_pNPCScriptTable->Release();
		
		// 2004, 06, 18, sobeit add start - nick name table
		if (g_pNickNameStringTable!=NULL) g_pNickNameStringTable->Release();
		// 2004, 06, 18, sobeit add end - nick name table

	}

	SAFE_DELETE( g_pRegenTowerInfoManager );
	SAFE_DELETE( g_pSystemAvailableManager );
	SAFE_DELETE( g_pWorldPlayerConfigTable );
	SAFE_DELETE( g_pServerInformation );
	SAFE_DELETE( g_pUserInformation );
	SAFE_DELETE( g_pTempInformation );
	SAFE_DELETE( g_pGameMessage );
	SAFE_DELETE( g_pHelpMessage );
	SAFE_DELETE( g_pNoticeMessage );
	SAFE_DELETE( g_pSystemMessage );
	SAFE_DELETE( g_pPlayerMessage );
	SAFE_DELETE( g_pGameStringTable );
	SAFE_DELETE( g_pClientConfig );
	SAFE_DELETE( g_pUserOption );
	SAFE_DELETE( g_pUIDialog );
	SAFE_DELETE( g_pProfileManager );
	SAFE_DELETE( g_pGuildMarkManager );
	SAFE_DELETE( g_pGuildInfoMapper );
	SAFE_DELETE( g_pEventManager );
	// g_pEffectSpriteTypeTable and g_pActionEffectSpriteTypeTable already deleted above
	SAFE_DELETE( g_pCreatureTable );
	SAFE_DELETE( g_pCreatureSpriteTypeMapper );
	SAFE_DELETE( g_pItemTable );
	SAFE_DELETE( g_pCreatureSpriteTable );
//	SAFE_DELETE( g_pAddonSpriteTable );
//	SAFE_DELETE( g_pCreatureActionSpriteTable );
	SAFE_DELETE( g_pEffectStatusTable );
	SAFE_DELETE( g_pInteractionObjectTable );
	SAFE_DELETE( g_pItemOptionTable );
	SAFE_DELETE( g_pNPCTable );
	SAFE_DELETE( g_pJusticeAttackManager );
	SAFE_DELETE( g_pNPCScriptTable );	
	SAFE_DELETE( g_pShopTemplateTable );
	SAFE_DELETE( g_pSkillInfoTable );
	SAFE_DELETE( g_pRankBonusTable );
	SAFE_DELETE( g_pExperienceTable );
	SAFE_DELETE( g_pSoundTable );
	SAFE_DELETE( g_pMusicTable );
	SAFE_DELETE( g_pZoneTable );
	SAFE_DELETE( g_pLevelNameTable );
	SAFE_DELETE( g_pQuestInfoManager );
	SAFE_DELETE( g_pSoundManager );
	SAFE_DELETE( g_pSkillAvailable );
	SAFE_DELETE( g_pSkillManager );
	SAFE_DELETE( g_pSlayerGear );
	SAFE_DELETE( g_pVampireGear );
	SAFE_DELETE( g_pOustersGear );
	SAFE_DELETE( g_pInventory );
	SAFE_DELETE( g_pParty );
	SAFE_DELETE( g_pTradeManager );	
	SAFE_DELETE( g_pStorage );
	SAFE_DELETE( g_pStorage2 );
	SAFE_DELETE( g_pGameTime );
	SAFE_DELETE( g_pKeyAccelerator );
	SAFE_DELETE( g_pAcceleratorManager );
	SAFE_DELETE( g_pModifyStatusManager );
	SAFE_DELETE( g_pEffectGeneratorTable );
	SAFE_DELETE( g_pUsingColorSetTable );
	SAFE_DELETE( g_pActionInfoTable );
	SAFE_DELETE( g_pPriceManager );
	SAFE_DELETE( g_pMoneyManager );
	SAFE_DELETE( g_pPCTalkBox );
	SAFE_DELETE( g_pNPCTalkBox );
	SAFE_DELETE( g_pChatManager );	
	SAFE_DELETE( g_pHelpManager );
	SAFE_DELETE( g_pHelpStringTable );	
//	SAFE_DELETE( g_pCompareManager );
	SAFE_DELETE( g_pHelpDisplayer );
	SAFE_DELETE( g_pObjectSelector );	
	SAFE_DELETE( g_pUIMessageManager );	
	SAFE_DELETE( g_pMonsterNameTable  );
	SAFE_DELETE( g_pQuestInfoManager  );
	SAFE_DELETE( g_pTimeItemManager  );	
	SAFE_DELETE( g_pFameInfoTable );
	SAFE_DELETE( g_pWarManager );

	// 2004, 6, 18, sobeit add start
	SAFE_DELETE( g_pNickNameStringTable );
	// 2004, 6, 18, sobeit add end
	//----------------------------------------------------------------
	// DirectDraw
	//----------------------------------------------------------------
	DEBUG_ADD("[Release] CDirect3D");
	// CDirect3D::Release() removed (SDL2)

	DEBUG_ADD("[Release] CDirectDraw");
	CSDLGraphics::ReleaseAll();

	//---------------------------------------------------------------------
	// Profiler
	//---------------------------------------------------------------------
	DEBUG_ADD("[Release] Profiler");
	SAFE_DELETE( g_pProfiler );	
	DEBUG_ADD("[Release] All OK");		

	SAFE_DELETE(g_pFileDef);

	// All game-owned items are gone. Do not call into UI globals from any
	// subsequent static teardown; a later initialization installs the host again.
	MItem::SetHost(nullptr);
	MChatManager::SetHost(nullptr);
	MWeather::SetHost(nullptr);
	MWarManager::SetHost(nullptr);
	MEffectTarget::SetHost(nullptr);
	MEffect::SetHost(nullptr);
	MGuidanceEffect::SetHost(nullptr);
	MParabolaEffect::SetHost(nullptr);
	MAttachEffect::SetHost(nullptr);
	MScreenEffectManager::SetHost(nullptr);
	MStopInventoryEffectGenerator::SetHost(nullptr);
	MStopZoneEffectGenerator::SetHost(nullptr);
	MStopZoneCrossEffectGenerator::SetHost(nullptr);
	MStopZoneXEffectGenerator::SetHost(nullptr);
	MStopZoneRhombusEffectGenerator::SetHost(nullptr);
	MStopZoneEmptyCrossEffectGenerator::SetHost(nullptr);
	MStopZoneEmptyRectEffectGenerator::SetHost(nullptr);
	MStopZoneRectEffectGenerator::SetHost(nullptr);
	MStopZoneWallEffectGenerator::SetHost(nullptr);
	MStopZoneMultipleEffectGenerator::SetHost(nullptr);
	MStopZoneRandomEffectGenerator::SetHost(nullptr);
	MStopZoneSelectableEffectGenerator::SetHost(nullptr);
	MSpreadOutEffectGenerator::SetHost(nullptr);
	MAroundZoneEffectGenerator::SetHost(nullptr);
	MFollowPathEffectGenerator::SetHost(nullptr);
	MMeteorDropEffectGenerator::SetHost(nullptr);
	MAttackCreatureParabolaEffectGenerator::SetHost(nullptr);
	MRippleZoneWideEffectGenerator::SetHost(nullptr);
	MBloodyBreakerEffectGenerator::SetHost(nullptr);
	MStopZoneEmptyHorizontalWallEffectGenerator::SetHost(nullptr);
	MStopZoneEmptyVerticalWallEffectGenerator::SetHost(nullptr);
	MFallingEffectGenerator::SetHost(nullptr);
	MAttackZoneEffectGenerator::SetHost(nullptr);
	MAttackZoneBombEffectGenerator::SetHost(nullptr);
	MAttackZoneParabolaEffectGenerator::SetHost(nullptr);
	MMultipleFallingEffectGenerator::SetHost(nullptr);
	MRisingEffectGenerator::SetHost(nullptr);
	MParty::SetHost(nullptr);
	UiRuntime::SetHost(nullptr);

#ifdef DEBUG_INFO
	ClearDebugInfo();
#endif
}

//-----------------------------------------------------------------------------
// Release Socket
//-----------------------------------------------------------------------------
void
ReleaseSocket()
{
	//----------------------------------------------------------------
	//
	//							socket
	//
	//----------------------------------------------------------------
		DEBUG_ADD("----- Start ReleaseSocket -----");
		
		if (g_pSocket!=NULL) 
		{			
			//g_pSocket->setPlayerStatus( CPS_END_SESSION );

			g_pSocket->disconnect(UNDISCONNECTED);

			//g_pSocket->getSocket()->close();			
			DEBUG_ADD("delete g_pSocket");
			
			delete g_pSocket;
			g_pSocket = NULL;
		}

		SAFE_DELETE( g_pConfigKorean );		
		SAFE_DELETE( g_pConfigForeign );

		if (g_pPacketFactoryManager != NULL)
		{
			DEBUG_ADD("delete g_pPacketFactoryManager");
			
			delete g_pPacketFactoryManager;
			g_pPacketFactoryManager = NULL;
		}			

		if (g_pPacketValidator != NULL)
		{
			DEBUG_ADD("delete g_pPacketValidator");
			
			delete g_pPacketValidator;
			g_pPacketValidator = NULL;
		}		

		if (g_pRequestServerPlayerManager!=NULL)
		{
			DEBUG_ADD("delete g_pRequestServerPlayerManager");
			delete g_pRequestServerPlayerManager;
			g_pRequestServerPlayerManager = NULL;
		}

		if (g_pClientCommunicationManager!=NULL)
		{
			DEBUG_ADD("delete g_pClientCommunicationManager");
			delete g_pClientCommunicationManager;
			g_pClientCommunicationManager = NULL;
		}

		if (g_pRequestFileManager!=NULL)
		{
			DEBUG_ADD("delete g_pRequestFileManager");
			delete g_pRequestFileManager;
			g_pRequestFileManager = NULL;
		}

		if (g_pRequestUserManager!=NULL)
		{
			DEBUG_ADD("delete g_pRequestUserManager");
			delete g_pRequestUserManager;
			g_pRequestUserManager = NULL;
		}

		
		DEBUG_ADD("----- End ReleaseSocket -----");
		
}

//-----------------------------------------------------------------------------
// UnInit Sound
//-----------------------------------------------------------------------------
void
UnInitSound()
{
	//-------------------------------------
	// UnInit DirectSound
	//-------------------------------------
	if (g_pSoundManager!=NULL)
	{
		g_pSoundManager->Release();	
	}

	g_SDLAudio.Release();
}

//-----------------------------------------------------------------------------
// Read the system-availability filter script out of the game data and
// hand its text to the manager. The archive reader and the
// file-definition table are the executable's; the parser
// (SystemAvailabilitiesManager::LoadFromStream, gamemodel) sees only the
// lines, so it can be tested without either.
//-----------------------------------------------------------------------------
static bool
LoadSystemAvailabilities(SystemAvailabilitiesManager* pManager, const char* szFileName)
{
	CRarFile rarfile;
	rarfile.SetRAR(g_pFileDef->getProperty("FILE_INFO_DATA").c_str(), "darkeden");
	rarfile.Open(szFileName);
	if (!rarfile.IsSet())
		return false;

	std::string text;
	char szLine[512];
	while (rarfile.GetString(szLine, sizeof(szLine)))
	{
		text += szLine;
		text += '\n';
	}
	rarfile.Release();

	std::istringstream in(text);
	return pManager->LoadFromStream(in);
}

//----------------------------------------------------------------------
// The item model lives in gamemodel and cannot reach the animation
// clock, the top view or the player (docs/RESTRUCTURING.md task 4.4);
// MItem asks through this host, installed in InitGameObject before any
// item exists.
//----------------------------------------------------------------------
static int	ItemDropFrameCount(TYPE_FRAMEID dropID)
{
	if (g_pTopView==NULL)
	{
		return 0;
	}

	return g_pTopView->m_ItemDropFPK[dropID].GetSize();
}

static void	RefreshAffect(MItem* pItem)
{
	if (g_pPlayer!=NULL)
	{
		g_pPlayer->CheckAffectStatus(pItem);
	}
}

static void	PlayItemSound(TYPE_SOUNDID soundID)
{
	PlaySound(soundID);
}

static void	RecalculateStatus()
{
	if (g_pPlayer!=NULL)
	{
		g_pPlayer->CalculateStatus();
	}
}

static void	ResetQuickItemSlot()
{
	UI_ResetQuickItemSlot();
}

static void	RepairHint()
{
	ExecuteHelpEvent( HELP_EVENT_ITEM_REPAIR );
}

// A gun worn without a magazine gets an empty one: the first magazine
// type that fits the gun, no options, no rounds (the loop the slayer
// gear ran itself before it moved into gamemodel).
static MMagazine*	EmptyMagazineFor(MItem* pGun)
{
	MMagazine* pMagazine = (MMagazine*)MItem::NewItem( (ITEM_CLASS)ITEM_CLASS_MAGAZINE );

	if (pMagazine==NULL)
	{
		return NULL;
	}

	pMagazine->SetID( 0 );

	for (int j=0; j<(*g_pItemTable)[ITEM_CLASS_MAGAZINE].GetSize(); j++)
	{
		pMagazine->SetItemType( j );

		if (pMagazine->IsInsertToItem( pGun ))
		{
			break;
		}
	}

	pMagazine->ClearItemOption();
	pMagazine->SetNumber( 0 );

	return pMagazine;
}

// MUsePotionItem::UseInventory's and MPetItem::UseInventory's bodies
// (MItemUse.cpp); the members are the library's.
void	UsePotionFromInventory(MItem* pPotion);
void	UsePetFromInventory(MItem* pPet);

static const MItemHost	s_ItemHost = {
	.pCurrentFrame			= &g_CurrentFrame,
	.DropFrameCount			= ItemDropFrameCount,
	.RefreshAffect			= RefreshAffect,
	.PlayItemSound			= PlayItemSound,
	.pCurrentTime			= &g_FrameNow,
	.RecalculateStatus		= RecalculateStatus,
	.ResetQuickItemSlot		= ResetQuickItemSlot,
	.RepairHint				= RepairHint,
	.EmptyMagazineFor		= EmptyMagazineFor,
	.UsePotionFromInventory	= UsePotionFromInventory,
	.UsePetFromInventory	= UsePetFromInventory,
	.ItemDestroyed			= [](MItem* item) noexcept { UI_RemoveDescriptor(item); },
};

//-----------------------------------------------------------------------------
// The price manager's host (docs/RESTRUCTURING.md task 4.2): the player's
// race, level and stat sums, the two half-price sources and the shop tax,
// read the way MPriceManager read them before it moved into gamemodel.
//-----------------------------------------------------------------------------
// Between MODE_WAIT_UPDATEINFO and the next GCUpdateInfo there is no
// player; nothing prices an item then, but the answers are "none"
// rather than a crash.
static int	PriceRace()
{
	if (g_pPlayer==NULL)		return -1;
	if (g_pPlayer->IsSlayer())	return RACE_SLAYER;
	if (g_pPlayer->IsVampire())	return RACE_VAMPIRE;
	if (g_pPlayer->IsOusters())	return RACE_OUSTERS;
	return -1;
}

static int	PriceLevel()			{ return g_pPlayer!=NULL ? (int)g_pPlayer->GetLEVEL() : 0; }
static int	PriceStatSum()			{ return g_pPlayer!=NULL ? (int)(g_pPlayer->GetSTR() + g_pPlayer->GetDEX() + g_pPlayer->GetINT()) : 0; }
static int	PriceBasicStatSum()		{ return g_pPlayer!=NULL ? (int)(g_pPlayer->GetBASIC_STR() + g_pPlayer->GetBASIC_DEX() + g_pPlayer->GetBASIC_INT()) : 0; }

static bool	PricePotionHalf()
{
	return g_pEventManager->IsEvent(EVENTID_PREMIUM_HALF)
		|| g_pSkillAvailable->IsEnableSkill(SKILL_HOLYLAND_BLOOD_BIBLE_NEMA);
}

static bool	PriceGambleHalf()		{ return g_pSkillAvailable->IsEnableSkill(SKILL_HOLYLAND_BLOOD_BIBLE_JAVE); }

// Three inputs of the server's price rule the server never sends; these
// are the documented defaults (MPriceManager.h).
// The server never tells the client that it gave an item away.
static bool	PriceIsCreateTypeGame(const MItem*)	{ return false; }
// The server's GamePlayer::isPayPlaying always answers yes.
static bool	PriceIsPayPlaying()		{ return true; }
// The server never sends the player's Blood Bible potion percentage.
static int	PricePotionPriceRatio()	{ return 0; }

static const MPriceHost	s_PriceHost = {
	.Race				= PriceRace,
	.Level				= PriceLevel,
	.StatSum			= PriceStatSum,
	.BasicStatSum		= PriceBasicStatSum,
	.IsPotionHalfPrice	= PricePotionHalf,
	.IsGambleHalfPrice	= PriceGambleHalf,
	.IsCreateTypeGame	= PriceIsCreateTypeGame,
	.IsPayPlaying		= PriceIsPayPlaying,
	.PotionPriceRatio	= PricePotionPriceRatio,
};

//-----------------------------------------------------------------------------
// The chat filter's host (docs/RESTRUCTURING.md task 4.13): the player's
// "filter bad words" option. Without options (before InitUserOption and
// after they are released) the answer is the option's default, on.
//-----------------------------------------------------------------------------
static bool	ChatFilteringCurse()
{
	return g_pUserOption==NULL || g_pUserOption->FilteringCurse!=FALSE;
}

static const MChatHost	s_ChatHost = {
	.FilteringCurse		= ChatFilteringCurse,
};

// Particle simulation uses pixels and viewport bounds without owning a player
// or renderer. Read both from live state each time the library asks.
static bool WeatherOrigin(int& x, int& y)
{
	if (!g_pPlayer) return false;
	x = g_pPlayer->GetX() * TILE_X;
	y = g_pPlayer->GetY() * TILE_Y;
	return true;
}

static const MWeatherHost s_WeatherHost = {
	.ReadOrigin = WeatherOrigin,
	.Width = []() { return static_cast<int>(g_GameRect.right); },
	.Height = []() { return static_cast<int>(g_GameRect.bottom); },
};

// War records and display rows live in gamemodel; the executable keeps
// the live-zone query, dialogs, chat and skill refresh.
static const MWarHost s_WarHost = {
	.ReadZone = [](ZoneID_t& id) {
		if (!g_pZone) return false;
		id = g_pZone->GetID();
		return true;
	},
	.RaceWarNotice = [](DWORD startTime) {
		UI_RunNotice(6, startTime);
		UI_DeleteNotice(5);
	},
	.RaceWarStarted = []() {
		if (g_pZone != nullptr)
		{
			if (g_pZoneTable->Get(g_pZone->GetID())->HolyLand)
			{
				UI_RunBloodBibleStatus();
				for (int i = 0; i < HOLYLAND_BONUS_MAX; ++i)
					g_abHolyLandBonusSkills[i] = false;
				g_pSkillAvailable->SetAvailableSkills();
			}
			else
				UI_CloseBloodBibleStatus();
		}
		UI_RunRangerChat();
	},
	.RaceWarEnded = []() {
		UI_SetBloodBibleStatusTimer(30);
		UI_CloseRangerChat();
	},
};

static const MEffectHost s_EffectHost = {
	.CurrentFrame = []() { return g_CurrentFrame; },
	.Light = [](BYTE bltType, TYPE_FRAMEID frameID, BYTE direction, BYTE frame) {
		return g_pTopView
			? g_pTopView->GetEffectLight(static_cast<BLT_TYPE>(bltType), frameID, direction, frame)
			: 0;
	},
};

static const MGuidanceEffectHost s_GuidanceEffectHost = {
	.CreaturePosition = [](TYPE_OBJECTID id, int& x, int& y, int& z) {
		if (!g_pZone) return false;
		const MCreature* creature = g_pZone->GetCreature(id);
		if (!creature) return false;
		x = creature->GetPixelX();
		y = creature->GetPixelY();
		z = creature->GetZ();
		return true;
	},
};

static const MParabolaEffectHost s_ParabolaEffectHost = {
	.SmokeSprite = [](MParabolaSmokeSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			g_pEffectSpriteTypeTable->GetSize() <= EFFECTSPRITETYPE_CANNONADE_SMOKE)
			return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_CANNONADE_SMOKE];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.QueueSmoke = [](std::unique_ptr<MEffect> smoke, DWORD waitCount) {
		if (g_pZone) g_pZone->AddEffect(smoke.release(), waitCount);
	},
	.CannonadeImpact = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		ExecuteActionInfoFromMainNode(RESULT_SKILL_GUN_SHOT_GUIDANCE_BOMB,
			x, y, 0, 0, 0, x, y, 0, 1000, nullptr, false);
	},
};

static bool AttachCreaturePosition(const MCreature* creature, MAttachCreaturePosition& position)
{
	if (!creature) return false;
	position.id = creature->GetID();
	position.x = creature->GetPixelX();
	position.y = creature->GetPixelY();
	position.z = creature->GetZ();
	return true;
}

static const MAttachEffectHost s_AttachEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MAttachEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize())
			return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.LiveCreature = [](TYPE_OBJECTID id, MAttachCreaturePosition& position) {
		return g_pZone && AttachCreaturePosition(g_pZone->GetCreature(id), position);
	},
	.FakeCreature = [](TYPE_OBJECTID id, MAttachCreaturePosition& position) {
		return g_pZone && AttachCreaturePosition(g_pZone->GetFakeCreature(id), position);
	},
	.CorpseCreature = [](TYPE_OBJECTID id, MAttachCreaturePosition& position) {
		if (!g_pZone) return false;
		const MItem* item = g_pZone->GetItem(id);
		return item && item->GetItemClass() == ITEM_CLASS_CORPSE &&
			AttachCreaturePosition(static_cast<const MCorpse*>(item)->GetCreature(), position);
	},
	.CreaturePosition = AttachCreaturePosition,
};

static const MScreenEffectManagerHost s_ScreenEffectManagerHost = {
	.CurrentFrame = []() { return g_CurrentFrame; },
	.GenerateNext = [](MEffect* effect) {
		if (g_pEffectGeneratorTable) g_pEffectGeneratorTable->GenerateNext(effect);
	},
};

static const MInventoryEffectHost s_InventoryEffectHost = {
	.Placement = [](int x, int y, MInventoryEffectPlacement& placement) {
		if (!g_pInventory) return false;
		placement.basis = UI_GetInventoryPosition();
		placement.cell = UI_GetInventoryGridPosition(x, y);
		placement.cellWidth = C_VS_UI_INVENTORY::GRID_UNIT_PIXEL_X;
		placement.cellHeight = C_VS_UI_INVENTORY::GRID_UNIT_PIXEL_Y;
		const MItem* item = g_pInventory->GetItem(x, y);
		if (item)
		{
			placement.itemWidth = item->GetGridWidth();
			placement.itemHeight = item->GetGridHeight();
		}
		return true;
	},
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MInventoryEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.Manager = []() { return g_pInventoryEffectManager; },
};

static const MFallingEffectHost s_FallingEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFallingEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MStopMultipleEffectHost s_StopMultipleEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MStopMultipleEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.AddEvent = [](MEvent& event) {
		if (g_pEventManager) g_pEventManager->AddEvent(event);
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MStopWallEffectHost s_StopWallEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MStopWallEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MEmptyWallEffectHost s_EmptyWallEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MEmptyWallEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MAroundZoneEffectHost s_AroundZoneEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MAroundZoneEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect, DWORD delay) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release(), delay);
	},
};

static const MSelectableZoneEffectHost s_SelectableZoneEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MSelectableZoneEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.repeatFrame = info.RepeatFrame;
		return true;
	},
	.MaxFrames = [](BYTE blt, TYPE_FRAMEID frameID, int& count) {
		if (!g_pTopView) return false;
		count = g_pTopView->GetMaxEffectFrame(static_cast<BLT_TYPE>(blt), frameID);
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MFixedZoneEffectHost s_FixedZoneEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MMeteorDropEffectHost s_MeteorDropEffectHost = {
	.Sprite = s_FixedZoneEffectHost.Sprite,
	.Queue = s_FixedZoneEffectHost.Queue,
	.AddEvent = [](MEvent& event) {
		if (g_pEventManager) g_pEventManager->AddEvent(event);
	},
};

static const MCreatureParabolaEffectHost s_CreatureParabolaEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MCreatureParabolaEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		return true;
	},
	.Creature = [](TYPE_OBJECTID id, MCreatureParabolaPosition& position) {
		if (!g_pZone) return false;
		const MCreature* creature = g_pZone->GetCreature(id);
		if (!creature) return false;
		position = {creature->GetX(), creature->GetY(), creature->GetZ()};
		return true;
	},
	.MaxFrames = [](BYTE blt, TYPE_FRAMEID frameID, int& count) {
		if (!g_pTopView) return false;
		count = g_pTopView->GetMaxEffectFrame(static_cast<BLT_TYPE>(blt), frameID);
		return true;
	},
	.Queue = s_FixedZoneEffectHost.Queue,
};

static const MBloodyBreakerEffectHost s_BloodyBreakerEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MBloodyBreakerEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		return true;
	},
	.MaxFrames = s_CreatureParabolaEffectHost.MaxFrames,
	.Queue = s_FixedZoneEffectHost.Queue,
};

static const MWideRippleEffectHost s_WideRippleEffectHost = {
	.Sprite = s_FixedZoneEffectHost.Sprite,
	.Bounds = [](MWideRippleEffectBounds& bounds) {
		if (!g_pZone) return false;
		bounds = {g_pZone->GetWidth(), g_pZone->GetHeight()};
		return true;
	},
	.Queue = s_FixedZoneEffectHost.Queue,
};

static const MRectZoneEffectHost s_RectZoneEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MRectZoneEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		return true;
	},
	.MaxFrames = [](BYTE blt, TYPE_FRAMEID frameID, int& count) {
		if (!g_pTopView) return false;
		count = g_pTopView->GetMaxEffectFrame(static_cast<BLT_TYPE>(blt), frameID);
		return true;
	},
	.Bounds = [](MRectZoneEffectBounds& bounds) {
		if (!g_pZone) return false;
		bounds.width = g_pZone->GetWidth();
		bounds.height = g_pZone->GetHeight();
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect, DWORD delay) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release(), delay);
	},
};

static const MEmptyRectEffectHost s_EmptyRectEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MEmptyRectEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.Bounds = [](MEmptyRectEffectBounds& bounds) {
		if (!g_pZone) return false;
		bounds.width = g_pZone->GetWidth();
		bounds.height = g_pZone->GetHeight();
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MCrossZoneEffectHost s_CrossZoneEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MCrossZoneEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.Bounds = [](MCrossZoneEffectBounds& bounds) {
		if (!g_pZone) return false;
		bounds.width = g_pZone->GetWidth();
		bounds.height = g_pZone->GetHeight();
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MStopZoneEffectHost s_StopZoneEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MStopZoneEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.repeatFrame = info.RepeatFrame;
		return true;
	},
	.MaxFrames = [](BYTE blt, TYPE_FRAMEID frameID, int& count) {
		if (!g_pTopView) return false;
		count = g_pTopView->GetMaxEffectFrame(static_cast<BLT_TYPE>(blt), frameID);
		return true;
	},
	.AddEvent = [](MEvent& event) {
		if (g_pEventManager) g_pEventManager->AddEvent(event);
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MZoneParabolaEffectHost s_ZoneParabolaEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MZoneParabolaEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MZoneBombEffectHost s_ZoneBombEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MZoneBombEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MZoneAttackEffectHost s_ZoneAttackEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MZoneAttackEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MMultipleFallingEffectHost s_MultipleFallingEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MMultipleFallingEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MRisingEffectHost s_RisingEffectHost = {
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MRisingEffectSprite& sprite) {
		if (!g_pEffectSpriteTypeTable || !g_pTopView ||
			!g_pEffectSpriteTypeTable->GetInternalPointer() ||
			type >= g_pEffectSpriteTypeTable->GetSize()) return false;
		const auto& info = (*g_pEffectSpriteTypeTable)[type];
		sprite.bltType = static_cast<BYTE>(info.BltType);
		sprite.frameID = info.FrameID;
		sprite.maxFrames = g_pTopView->GetMaxEffectFrame(info.BltType, info.FrameID);
		return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		// AddEffect consumes the pointer even when it rejects or throws.
		return g_pZone && g_pZone->AddEffect(effect.release());
	},
};

static const MEffectTargetHost s_EffectTargetHost = {
	.RemoveFromPlayer = [](BYTE id) {
		if (g_pPlayer) g_pPlayer->RemoveEffectTarget(id);
	},
};

// Live creature actions used by the library-owned party roster.
static bool PartyJoinByID(TYPE_OBJECTID id, MString& name)
{
	if (!g_pZone) return false;
	auto* creature = g_pZone->GetCreature(id);
	if (!creature) return false;
	name = creature->GetName();
	creature->SetPlayerParty();
	return true;
}

static TYPE_OBJECTID PartyJoinByName(const char* name)
{
	if (!g_pZone || !g_pPlayer) return OBJECTID_NULL;
	const auto id = g_pZone->GetCreatureID(name, 1);
	if (auto* creature = g_pZone->GetCreature(id)) creature->SetPlayerParty();
	return id;
}

static void PartyLeaveByID(TYPE_OBJECTID id)
{
	if (!g_pZone) return;
	if (auto* creature = g_pZone->GetCreature(id)) creature->UnSetPlayerParty();
}

static void PartyLeaveByName(const char* name)
{
	if (g_pZone && g_pPlayer) PartyLeaveByID(g_pZone->GetCreatureID(name, 1));
}

static const MPartyHost s_PartyHost = {
	.JoinByID = PartyJoinByID,
	.JoinByName = PartyJoinByName,
	.LeaveByID = PartyLeaveByID,
	.LeaveByName = PartyLeaveByName,
	.CurrentTime = []() { return g_FrameNow; },
};

// The UI asks for live data and borrowed sprites without depending on owners.
static bool UiEventFlagActive(std::uint32_t flag)
{
	return g_pEventManager && g_pEventManager->GetEventByFlag(flag) != nullptr;
}

static CSprite* UiFindGuildMark(std::uint16_t guildID, UiRuntime::MarkSize size)
{
	if (!g_pGuildMarkManager) return nullptr;
	return size == UiRuntime::MarkSize::Small
		? g_pGuildMarkManager->GetGuildMarkSmall(guildID)
		: g_pGuildMarkManager->GetGuildMark(guildID);
}

static void UiLoadGuildMark(std::uint16_t guildID)
{
	if (g_pGuildMarkManager) g_pGuildMarkManager->LoadGuildMark(guildID);
}

static CSprite* UiGradeMark(std::uint16_t grade, int race, UiRuntime::MarkSize size)
{
	if (!g_pGuildMarkManager || race < 0 || race >= RACE_MAX) return nullptr;
	return size == UiRuntime::MarkSize::Small
		? g_pGuildMarkManager->GetGradeMarkSmall(grade, static_cast<Race>(race))
		: g_pGuildMarkManager->GetGradeMark(grade, static_cast<Race>(race));
}

static std::size_t UiHornMapCount()
{
	return g_pZone ? g_pZone->GetHorn().size() : 0;
}

static bool UiReadHornPortals(std::size_t map, UiRuntime::HornPortals& portals)
{
	if (!g_pZone || map >= g_pZone->GetHorn().size()) return false;
	portals = g_pZone->GetHorn()[map];
	return true;
}

static bool UiReadPetProgress(std::uint32_t itemID, UiRuntime::PetProgress& progress)
{
	if (!g_pZone || !g_pPlayer) return false;
	auto* pet = static_cast<MFakeCreature*>(g_pZone->GetFakeCreature(g_pPlayer->GetPetID()));
	if (!pet) return false;
	auto* item = pet->GetPetItem();
	if (!item || item->GetID() != itemID) return false;
	progress.experienceRemaining = static_cast<int>(item->GetPetExpRemain());
	progress.level = item->GetNumber();
	return true;
}

static bool UiSendPacket(Packet& packet)
{
	if (!g_pSocket) return false;
	g_pSocket->sendPacket(&packet);
	return true;
}

static const UiRuntime::Host s_UiRuntimeHost = {
	.EventFlagActive = UiEventFlagActive,
	.FindGuildMark = UiFindGuildMark,
	.LoadGuildMark = UiLoadGuildMark,
	.GradeMark = UiGradeMark,
	.HornMapCount = UiHornMapCount,
	.ReadHornPortals = UiReadHornPortals,
	.ReadPetProgress = UiReadPetProgress,
	.SendPacket = UiSendPacket,
};

//-----------------------------------------------------------------------------
// The wire layer's host (docs/RESTRUCTURING.md task 5.1): three tuning
// values out of the config, and the connection a bug report goes to.
//-----------------------------------------------------------------------------
// The config is built in GameInitInfo and deleted at shutdown, so each
// of these is read when it is asked for rather than copied once. Wire's
// own defaults - ClientConfig's constructor values - answer while there
// is none.
//-----------------------------------------------------------------------------
static int	WireMaxProcessPacket()	{ return g_pClientConfig!=NULL ? g_pClientConfig->MAX_PROCESS_PACKET : WIRE_DEFAULT_MAX_PROCESS_PACKET; }
static int	WireMaxRequestService()	{ return g_pClientConfig!=NULL ? g_pClientConfig->MAX_REQUEST_SERVICE : WIRE_DEFAULT_MAX_REQUEST_SERVICE; }
static uint	WireUDPPort()			{ return g_pClientConfig!=NULL ? (uint)g_pClientConfig->CLIENT_COMMUNICATION_UDP_PORT : static_cast<uint>(WIRE_DEFAULT_UDP_PORT); }
static Player*	WireBugReportTarget()	{ return g_pSocket; }

// The encrypt-seed inputs, and they are asked for on every login:
// Encrypter.h defines __USE_ENCRYPTER__, so ClientPlayer::setEncryptCode()
// IS compiled, and GCUpdateInfoHandler calls it right after
// MoveZone/LoadZone. (The slice that introduced these said the opposite.
// It was wrong, and both reviewers of the next slice caught it.) They
// are also the reason ClientPlayer.cpp no longer includes MZone.h or
// UserInformation.h. Each is guarded anyway: the guards cost nothing,
// and a wrong seed is a silently dead connection rather than a crash.
static ZoneID_t	WireEncryptZoneID()		{ return g_pZone!=NULL ? g_pZone->GetID() : 0; }
static int	WireEncryptServerID()		{ return g_pUserInformation!=NULL ? g_pUserInformation->ServerID : 0; }
static bool	WireEncryptUsesEnglishSeed()
{
	// Of the four original branches only the English one differed; the
	// Netmarble, Chinese and default cases were the same expression.
	return g_pUserInformation!=NULL && g_pUserInformation->bEnglish;
}

//-----------------------------------------------------------------------------
// The request-service family's seams (task 5.1's fourth slice) - the
// inbound side only, since task 5.2's eighth slice deleted the outbound
// half (this client dialling peers; upstream had compiled it out).
//-----------------------------------------------------------------------------
// The peer file-transfer manager stays here: it writes into the profile
// directory and reads the UI's own state. Three calls of it are all the
// wire layer needs, and each is guarded, because g_pRequestFileManager
// is built after start-up and the request players outlive it at
// shutdown - which is what the NULL tests they used to write at the
// call site were for.
//
// The guard is not complete, and saying so is better than implying it
// is. At shutdown the pointer is deleted AND nulled, so the test holds.
// On the re-login path it is deleted without nulling and the accept
// thread is started fifteen lines before it is reassigned, so for that
// window `g_pRequestFileManager != NULL` is a test on a dangling
// pointer. The old call sites tested the same expression, so nothing
// here widens the window - but nothing here closes it either.
//-----------------------------------------------------------------------------
static MonotonicClock::TimePoint	WireCurrentTime()	{ return g_FrameNow; }

static bool	WireSendOtherRequest(const std::string& name, RequestServerPlayer* pPlayer)
		{ return g_pRequestFileManager!=NULL && g_pRequestFileManager->SendOtherRequest(name, pPlayer); }
static bool	WireHasOtherRequest(const std::string& name)
		{ return g_pRequestFileManager!=NULL && g_pRequestFileManager->HasOtherRequest(name); }
static bool	WireRemoveOtherRequest(const std::string& name)
		{ return g_pRequestFileManager!=NULL && g_pRequestFileManager->RemoveOtherRequest(name); }

// Designated, not positional. Five of the 11 entries share a signature
// with another (two bool(const std::string&), three int()), so a
// positional initialiser wired to the wrong one compiles and passes the
// whole suite - which never links this file. A designator out of
// declaration order, or naming a member that does not exist, is a
// compile error instead (C++20).
static const WireHost	s_WireHost = {
	.MaxProcessPacket		= WireMaxProcessPacket,
	.MaxRequestService		= WireMaxRequestService,
	.ClientCommunicationUDPPort	= WireUDPPort,
	.BugReportTarget		= WireBugReportTarget,
	.EncryptZoneID			= WireEncryptZoneID,
	.EncryptServerID		= WireEncryptServerID,
	.EncryptUsesEnglishSeed		= WireEncryptUsesEnglishSeed,
	.CurrentTime			= WireCurrentTime,
	.SendOtherRequest		= WireSendOtherRequest,
	.HasOtherRequest		= WireHasOtherRequest,
	.RemoveOtherRequest		= WireRemoveOtherRequest,
};

//-----------------------------------------------------------------------------
// Init GameObject
//-----------------------------------------------------------------------------
BOOL
InitGameObject()
{
	DEBUG_ADD("[ InitGame ]  GameObject");
	
	//----------------------------------------
	// 
	//----------------------------------------
	if (g_pSystemAvailableManager == NULL )
	{
		g_pSystemAvailableManager = new SystemAvailabilitiesManager;
		LoadSystemAvailabilities(g_pSystemAvailableManager,
			g_pFileDef->getProperty("FILE_FILTER_INFO").c_str());
	}
	if (g_pServerInformation==NULL)
	{
		g_pServerInformation = new CServerInformation;
		g_pServerInformation->SetServerGroupName( "ServerGroup" );
		g_pServerInformation->SetServerName( "Server" );
	}

	if (g_pUserInformation==NULL)
	{
		g_pUserInformation = new UserInformation;
	}
	int nStartV = 73*25;
	int version;
	std::ifstream versionFile;//(g_pFileDef->getProperty("FILE_INFO_ACTION").c_str(), ios::binary);
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_INFO_VERSION").c_str(), versionFile))
	{
		return FALSE;
	}
	versionFile.read((char*)&version, 4);
	versionFile.close();

	int i = 96;
	nStartV = nStartV*i;
//	g_pUserInformation->GameVersion = (406+nStartV)/2+88; //version=87891;
	//g_pUserInformation->GameVersion = 745389;2006-6-20
	
	//modify by viva for GameVersion
	g_pUserInformation->GameVersion = 7457757;
	//end

//	g_pUserInformation->GameVersion = 520909;//745389
#ifdef DEBUG_INFO
	if(g_nGameVersion > 0)
		g_pUserInformation->GameVersion = g_nGameVersion;
#endif

	
	if (g_pTempInformation==NULL)
	{
		g_pTempInformation = new TempInformation;
	}

	if (g_pSkillManager==NULL)
	{
		g_pSkillManager = new MSkillManager;		
	}

	if (g_pSkillAvailable==NULL)
	{
		g_pSkillAvailable = new MSkillSet;
	}

	if (g_pSlayerGear==NULL)
	{
		g_pSlayerGear = new MSlayerGear;
	}

	if (g_pVampireGear==NULL)
	{
		g_pVampireGear = new MVampireGear;
	}

	if (g_pOustersGear==NULL)
	{
		g_pOustersGear = new MOustersGear;
	}
	
	if (g_pInventory==NULL)
	{
		g_pInventory = new MInventory;
	}

	if (g_pParty==NULL)
	{
		g_pParty = new MParty;
	}
	else
	{
		g_pParty->Release();
	}


	//if (g_pTradeManager==NULL)
	//{
	//	g_pTradeManager = new MTradeManager;
	//}	

	if (g_pGameTime==NULL)
	{
		g_pGameTime = new MGameTime;
	}

	if (g_pPriceManager==NULL)
	{
		g_pPriceManager = new MPriceManager;
	}

	if (g_pMoneyManager==NULL)
	{
		g_pMoneyManager = new MMoneyManager;
		// The player's own wallet is the one that suggests a storage box
		// (MMoneyManager lives in gamemodel and cannot reach the help
		// system itself - task 4.2).
		g_pMoneyManager->SetStorageHintHook([]() { ExecuteHelpEvent(HELP_EVENT_STORAGE_BUY); });
	}

	MItem::SetHost(&s_ItemHost);
	Wire::SetHost(&s_WireHost);
	MPriceManager::SetHost(&s_PriceHost);
	MChatManager::SetHost(&s_ChatHost);
	MWeather::SetHost(&s_WeatherHost);
	MWarManager::SetHost(&s_WarHost);
	MEffectTarget::SetHost(&s_EffectTargetHost);
	MEffect::SetHost(&s_EffectHost);
	MGuidanceEffect::SetHost(&s_GuidanceEffectHost);
	MParabolaEffect::SetHost(&s_ParabolaEffectHost);
	MAttachEffect::SetHost(&s_AttachEffectHost);
	MScreenEffectManager::SetHost(&s_ScreenEffectManagerHost);
	MStopInventoryEffectGenerator::SetHost(&s_InventoryEffectHost);
	MStopZoneEffectGenerator::SetHost(&s_StopZoneEffectHost);
	MStopZoneCrossEffectGenerator::SetHost(&s_CrossZoneEffectHost);
	MStopZoneXEffectGenerator::SetHost(&s_FixedZoneEffectHost);
	MStopZoneRhombusEffectGenerator::SetHost(&s_FixedZoneEffectHost);
	MStopZoneEmptyCrossEffectGenerator::SetHost(&s_FixedZoneEffectHost);
	MStopZoneEmptyRectEffectGenerator::SetHost(&s_EmptyRectEffectHost);
	MStopZoneRectEffectGenerator::SetHost(&s_RectZoneEffectHost);
	MStopZoneWallEffectGenerator::SetHost(&s_StopWallEffectHost);
	MStopZoneMultipleEffectGenerator::SetHost(&s_StopMultipleEffectHost);
	MStopZoneRandomEffectGenerator::SetHost(&s_FixedZoneEffectHost);
	MStopZoneSelectableEffectGenerator::SetHost(&s_SelectableZoneEffectHost);
	MSpreadOutEffectGenerator::SetHost(&s_FixedZoneEffectHost);
	MAroundZoneEffectGenerator::SetHost(&s_AroundZoneEffectHost);
	MFollowPathEffectGenerator::SetHost(&s_FixedZoneEffectHost);
	MMeteorDropEffectGenerator::SetHost(&s_MeteorDropEffectHost);
	MAttackCreatureParabolaEffectGenerator::SetHost(&s_CreatureParabolaEffectHost);
	MRippleZoneWideEffectGenerator::SetHost(&s_WideRippleEffectHost);
	MBloodyBreakerEffectGenerator::SetHost(&s_BloodyBreakerEffectHost);
	MStopZoneEmptyHorizontalWallEffectGenerator::SetHost(&s_EmptyWallEffectHost);
	MStopZoneEmptyVerticalWallEffectGenerator::SetHost(&s_EmptyWallEffectHost);
	MFallingEffectGenerator::SetHost(&s_FallingEffectHost);
	MAttackZoneEffectGenerator::SetHost(&s_ZoneAttackEffectHost);
	MAttackZoneBombEffectGenerator::SetHost(&s_ZoneBombEffectHost);
	MAttackZoneParabolaEffectGenerator::SetHost(&s_ZoneParabolaEffectHost);
	MMultipleFallingEffectGenerator::SetHost(&s_MultipleFallingEffectHost);
	MRisingEffectGenerator::SetHost(&s_RisingEffectHost);
	MParty::SetHost(&s_PartyHost);
	UiRuntime::SetHost(&s_UiRuntimeHost);

	if (g_pPCTalkBox==NULL)
	{
		g_pPCTalkBox = new PCTalkBox;
	}

	if (g_pNPCTalkBox==NULL)
	{
		g_pNPCTalkBox = new MStringList;
	}

	if (g_pUIMessageManager==NULL)
	{
		g_pUIMessageManager = new UIMessageManager;
		g_pUIMessageManager->Init();
	}
 
	if (g_pObjectSelector==NULL)
	{
		g_pObjectSelector = new MObjectSelector;
	}

	//----------------------------------------
	// Weather
	//----------------------------------------
	if (g_pWeather!=NULL)
	{
		delete g_pWeather;
	}
	g_pWeather = new MWeather;

	//----------------------------------------
	// ScreenEffectManager
	//----------------------------------------
	if (g_pInventoryEffectManager != NULL)
	{
		delete g_pInventoryEffectManager;			
	}
	g_pInventoryEffectManager = new MScreenEffectManager;

	//----------------------------------------
	// Player 생성
	//----------------------------------------
	if (g_pPlayer!=NULL)
	{	
		if (g_pZone!=NULL)
		{
			g_pZone->RemoveCreature( g_pPlayer->GetID() );
		}
	
		delete g_pPlayer;
	}

	// Player 기본값 초기화
	g_pPlayer = new MPlayer;
	DEBUG_ADD_FORMAT("[g_pPlayer] %p", static_cast<const void*>(g_pPlayer));

	g_pPlayer->SetCreatureType( CREATURETYPE_SLAYER_MALE );


	//--------------------------------------------------
	//
	// The skill tree
	//
	//--------------------------------------------------
	InitSkillTree();

	return TRUE;
}

//-----------------------------------------------------------------------------
// Init SkillTree
//-----------------------------------------------------------------------------
// The skill manager builds the tree itself; the per-domain experience
// the server ships comes from a file, which the executable opens for it
// (docs/RESTRUCTURING.md task 4.4).
//-----------------------------------------------------------------------------
void
InitSkillTree()
{
	g_pSkillManager->Init();

	std::ifstream serverDomainInfoFile;

	if (FileOpenBinary(g_pFileDef->getProperty("FILE_INFO_SKILL_DOMAIN_EXP").c_str(), serverDomainInfoFile))
	{
		g_pSkillManager->LoadFromFileServerDomainInfo( serverDomainInfoFile );
		serverDomainInfoFile.close();
	}
}
