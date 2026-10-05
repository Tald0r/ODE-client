//-----------------------------------------------------------------------------
// CSoundPartManager.cpp
//-----------------------------------------------------------------------------
#include "CSoundPartManager.h"
#include "CDirectSound.h"

//-----------------------------------------------------------------------------
// OnReleaseData
//-----------------------------------------------------------------------------
// CPartManager::Release() 가 slot마다 불러준다.
//-----------------------------------------------------------------------------
void
CSoundPartManager::OnReleaseData(LPDIRECTSOUNDBUFFER& buffer)
{
	if (buffer!=NULL)
	{
		g_SDLAudio.Stop(buffer);
		g_SDLAudio.Release(buffer);
		buffer = NULL;
	}
}

//-----------------------------------------------------------------------------
// Stop
//-----------------------------------------------------------------------------
void
CSoundPartManager::Stop()
{
	//---------------------------------------------------
	// 내부에 있는 LPDIRECTSOUNDBUFFER를 다 stop한다.
	//---------------------------------------------------
	if (m_pData!=NULL)
	{
		for (int i=0; i<m_nPart; i++)
		{
			if (m_pData[i]!=NULL)
			{
				g_SDLAudio.Stop(m_pData[i]);
			}
		}
	}
}