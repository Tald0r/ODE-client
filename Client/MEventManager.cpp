//----------------------------------------------------------------------
// MEventManager.cpp - live event services and background-image ownership.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MEventManager.h"
#include "CJpeg.h"
#include "DataPath.h"
#include "MPlayer.h"
#include "PacketFunction.h"

MEventManager* g_pEventManager = NULL;

namespace {
const MEventHost eventHost{
	.SetAddGammaRamp = [](WORD red, WORD green, WORD blue) {
		CSDLGraphics::SetAddGammaRamp(red, green, blue);
	},
	.HasEffectStatus = [](DWORD effect) {
		// With no player, keep effect events until their explicit expiry.
		return !g_pPlayer || g_pPlayer->HasEffectStatus(static_cast<EFFECTSTATUS>(effect));
	},
	.SetFadeStart = ::SetFadeStart,
};
}

MEventManager::MEventManager() : MEventQueue(&eventHost)
{
}

MEventManager::~MEventManager()
{
	RemoveAllEvent();
}

bool MEventManager::AssertEventBackground(EVENTBACKGROUND_ID id)
{
	static std::string strFilename[EVENTBACKGROUNDID_MAX] = 
	{
		"Data\\Image\\EventBackgroundCosmos.jps",
		"Data\\Image\\EventBackgroundOustersSlayer.jps",
		"Data\\Image\\EventBackgroundOustersVampire.jps",
		"Data\\Image\\EventBackgroundQuest2.jps",
		"Data\\Image\\EventBackgroundCloud.jps",
	
	};

	if(id < 0 || id >= EVENTBACKGROUNDID_MAX)
		return false;

	if(m_EventBackGround.GetSize() == 0)
	{
		m_EventBackGround.Init(EVENTBACKGROUNDID_MAX);
	}

	CDirectDrawSurface* entry = m_EventBackGround.GetMutable(id);
	if (entry == nullptr)
		return false;
	if(entry->GetSurface() != NULL)
		return true;

	// The table spells the paths the Windows way; resolved for the disk
	// (basic/DataPath.h): the identity on Windows, and elsewhere the
	// separators folded and each component matched case-insensitively
	// (EventBackgroundQuest2 ships as .JPS).
	CJpeg jpg;
	bool bOpen = jpg.Open(Basic::NormalizeDataPath(strFilename[id]).c_str());
	if(bOpen == true && jpg.GetWidth() > 0 && jpg.GetHeight() > 0 && jpg.GetHeight() > 0)
	{
		CDirectDrawSurface &surface = *entry;
		const int bpp = jpg.GetBpp(), width = jpg.GetWidth(), height = jpg.GetHeight(), pitch = width*bpp;

		if (surface.InitOffsurface(width, height, DDSCAPS_SYSTEMMEMORY))
		{
			if (surface.Lock())
			{
				WORD *pSurface = (WORD *)surface.GetSurfacePointer();
				unsigned char *pData = jpg.GetImage(), *pDataTemp;
				WORD *pSurfaceTemp;
				
				int surfacePitch = surface.GetSurfacePitch();
				
				if (pSurface)
				{
					if (bpp == 1)
					{
						for (int y = 0; y < height; y++)
						{
							pDataTemp = pData;
							pSurfaceTemp = pSurface;								
							
							for (int x = 0; x < width; x++)
							{
								BYTE temp_data = *pDataTemp++;	//p_data[y*(pitch)+x];
								BYTE r = temp_data>>3;
								BYTE g = r;
								BYTE b = r;
								
								*pSurfaceTemp++ = CSDLGraphics::Color(r, g, b);
							}
							
							pData = pData + pitch;
							pSurface = (WORD*)((BYTE*)pSurface + surfacePitch);
						}
					}
					else if (bpp == 3)
					{
						for (int y = 0; y < height; y++)
						{
							pDataTemp = pData;
							pSurfaceTemp = pSurface;	
							
							for (int x = 0; x < width; x++)
							{
								//char *temp_data = &p_data[y*pitch+x*bpp];
								BYTE r = *(pDataTemp+2) >> 3;		//temp_data[2]>>3;
								BYTE g = *(pDataTemp+1) >> 3;	//temp_data[1]>>3;
								BYTE b = *pDataTemp >> 3;	//temp_data[0]>>3;
								
								pDataTemp += bpp;
								
								*pSurfaceTemp++ = CSDLGraphics::Color(r, g, b);
							}
							
							pData = pData + pitch;
							pSurface = (WORD*)((BYTE*)pSurface + surfacePitch);
						}
					}
					
				}
				
				surface.Unlock();
			}
		}
	}

	return true;
}