//-----------------------------------------------------------------------------
// MZoneSoundManager.cpp
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "MZoneSoundManager.h"
#include "MSoundTable.h"
#include "DebugLog.h"
#include <cstdlib>

//-----------------------------------------------------------------------------
// global
//-----------------------------------------------------------------------------
MZoneSoundTable*	g_pZoneSoundTable = NULL;
MZoneSoundManager*	g_pZoneSoundManager = NULL;

namespace {
const ZoneSoundHost* s_pHost = NULL;
}

const ZoneSoundHost*
ZONESOUND_NODE::SetHost(const ZoneSoundHost* pHost)
{
	const ZoneSoundHost* pOld = s_pHost;
	s_pHost = pHost;
	return pOld;
}

//-----------------------------------------------------------------------------
//
//						ZONESOUND_NODE
//
//-----------------------------------------------------------------------------
ZONESOUND_NODE::ZONESOUND_NODE(TYPE_SOUNDID id)
{
	m_SoundID	= id;
	m_pBuffer	= NULL;
	m_X			= -1;
	m_Y			= -1;
	m_bLoop		= false;
	m_bContinueLoop = false;
}

ZONESOUND_NODE::~ZONESOUND_NODE()
{
	if (m_pBuffer!=NULL && s_pHost!=NULL)
	{
		s_pHost->Stop(m_pBuffer);
		s_pHost->Release(m_pBuffer);
		m_pBuffer = NULL;
	}
}

//-----------------------------------------------------------------------------
// Save To File
//-----------------------------------------------------------------------------
void
ZONESOUND_NODE::SaveToFile(std::ofstream& file)
{
	(void)file;
}

//-----------------------------------------------------------------------------
// Load From File
//-----------------------------------------------------------------------------
void
ZONESOUND_NODE::LoadFromFile(std::ifstream& file)
{
	(void)file;
}

//-----------------------------------------------------------------------------
// Play
//-----------------------------------------------------------------------------
// The sound at sector (x, y), heard from the player's sector: panned by
// how far right or left it is (3 sectors either way are the centre), and
// quieter the farther it is (4 steps a sector, times the volume option).
// From 40 sectors away it is not heard. While the player stands still, a
// playing sound keeps its nearer source.
//-----------------------------------------------------------------------------
void
ZONESOUND_NODE::Play(int x, int y, bool bLoop)
{
	if (s_pHost == NULL
		|| !s_pHost->AudioReady()
		|| m_SoundID >= g_pSoundTable->GetSize()
		|| !s_pHost->SoundOn())
	{
		return;
	}

	int playerX = 0, playerY = 0;
	s_pHost->PlayerPosition(playerX, playerY);

	int gapX = x - playerX;
	int gapY = y - playerY;
	int dist = (std::max)(abs(gapX), abs(gapY));

	//------------------------------------------------------------
	// Playing, and the player has not moved: keep the nearer source.
	//------------------------------------------------------------
	if (m_pBuffer!=NULL
		&& m_playerX==playerX && m_playerY==playerY)
	{
		int oldGapX = m_X - m_playerX;
		int oldGapY = m_Y - m_playerY;
		int oldDist = (std::max)(abs(oldGapX), abs(oldGapY));

		if (oldDist < dist)
		{
			return;
		}
	}

	m_X				= x;
	m_Y				= y;
	m_bLoop			= bLoop;
	m_bContinueLoop = bLoop;
	m_playerX		= playerX;
	m_playerY		= playerY;

	if (dist < 40)
	{
		//------------------------------------------------------------
		// No buffer yet: duplicate the cached one, loading it first if
		// the cache lacks it.
		//------------------------------------------------------------
		if (m_pBuffer==NULL)
		{
			DEBUG_ADD("[ZONESOUND_NODE] Play:New Buffer");

			if (!s_pHost->IsCached(m_SoundID))
			{
				DEBUG_ADD("[ZONESOUND_NODE] Load Wave");

				// GameMain releases the table Filename when its own load
				// fails, so it can be NULL here - don't pass that to
				// LoadWav or format it with %s below.
				const char* pFilename = (*g_pSoundTable)[m_SoundID].Filename;
				if (pFilename==NULL)
				{
					return;
				}

				LPDIRECTSOUNDBUFFER pBuffer = s_pHost->LoadWav( pFilename );

				if (pBuffer==NULL)
				{
					DEBUG_ADD_FORMAT_ERR("[Error] Failed to Load WAV. id=%d, fn=%s", m_SoundID, pFilename );

					return;
				}
				else
				{
					// The buffer the cache pushes out is released.
					LPDIRECTSOUNDBUFFER pOld = NULL;
					if (s_pHost->Cache( m_SoundID, pBuffer, pOld ))
					{
						s_pHost->Release(pOld);
					}

					// Not auto-released: the node releases it.
					m_pBuffer = s_pHost->Duplicate(pBuffer);
				}
			}
			else
			{
				DEBUG_ADD("[ZONESOUND_NODE] Already Exist Buffer");

				LPDIRECTSOUNDBUFFER pBuffer;
				if (s_pHost->GetCached(m_SoundID, pBuffer))
				{
					m_pBuffer = s_pHost->Duplicate(pBuffer);
				}
			}
		}

		DEBUG_ADD("[ZONESOUND_NODE] Arrange Volume & Dist");

		if (m_pBuffer!=NULL)
		{
			//------------------------------------------------------------
			// Pan
			//------------------------------------------------------------
			if (gapX > 3)
			{
				s_pHost->CenterToRightPan( m_pBuffer, (gapX-3) << 7 );
			}
			else if (gapX < -3)
			{
				s_pHost->CenterToLeftPan( m_pBuffer, (abs(gapX+3)) << 7 );
			}
			else
			{
				s_pHost->CenterPan( m_pBuffer );
			}

			//------------------------------------------------------------
			// Volume
			//------------------------------------------------------------
			int sub = (dist << 2) * s_pHost->SoundVolume();
			s_pHost->SubVolumeFromMax(m_pBuffer, sub);

			DEBUG_ADD("[ZONESOUND_NODE] Play Buffer");

			if (!s_pHost->IsPlaying( m_pBuffer ))
			{
				s_pHost->Play( m_pBuffer, m_bLoop );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Stop
//-----------------------------------------------------------------------------
void
ZONESOUND_NODE::Stop()
{
	if (m_pBuffer!=NULL && s_pHost!=NULL)
	{
		if (s_pHost->IsPlaying( m_pBuffer ))
		{
			s_pHost->Stop( m_pBuffer );
		}
	}

	m_bContinueLoop = false;
}

//-----------------------------------------------------------------------------
// Stop Loop
//-----------------------------------------------------------------------------
// It plays to its end once more instead of stopping.
//-----------------------------------------------------------------------------
void
ZONESOUND_NODE::StopLoop()
{
	if (m_pBuffer!=NULL && s_pHost!=NULL)
	{
		if (s_pHost->IsPlaying( m_pBuffer ))
		{
			s_pHost->Play(m_pBuffer, false);	// stops the loop
		}
	}

	m_bContinueLoop = false;
}

//-----------------------------------------------------------------------------
//
//							MZoneSoundTable
//
//-----------------------------------------------------------------------------
MZoneSoundTable::MZoneSoundTable()
{
}

MZoneSoundTable::~MZoneSoundTable()
{
}

//-----------------------------------------------------------------------------
//
//							MZoneSoundManager
//
//-----------------------------------------------------------------------------
MZoneSoundManager::MZoneSoundManager()
{
	m_LastX = -1;
	m_LastY = -1;
	m_LastUpdateTime = MonotonicClock::TimePoint();
}

MZoneSoundManager::~MZoneSoundManager()
{
}

//-----------------------------------------------------------------------------
// Update Sound
//-----------------------------------------------------------------------------
// Every 5 seconds, or when the player moves: each sound heard in the
// player's sector whose time has come plays from its place and schedules
// its next time; one outside its hours stops; a looping one within its
// hours keeps looping. A looping sound no longer heard from here stops
// looping.
//-----------------------------------------------------------------------------
void
MZoneSoundManager::UpdateSound()
{
	int playerX = 0, playerY = 0;
	if (s_pHost == NULL || !s_pHost->PlayerPosition(playerX, playerY))
	{
		return;
	}

	const MonotonicClock::TimePoint now = s_pHost->Now();

	if (now - m_LastUpdateTime > MonotonicClock::Millis(5000)
		|| playerX!=m_LastX
		|| playerY!=m_LastY)
	{
		// Without a zone nothing changes.
		const SECTORSOUND_LIST* pSectorSounds = s_pHost->SectorSounds(playerX, playerY);
		if (pSectorSounds == NULL)
		{
			return;
		}

		//------------------------------------------------------------
		// No loop continues unless it is heard again.
		//------------------------------------------------------------
		CTypeMap<ZONESOUND_NODE>::iterator iPlaySound = begin();

		while (iPlaySound != end())
		{
			ZONESOUND_NODE* pNode = iPlaySound->second;

			pNode->UnSetContinueLoop();

			iPlaySound++;
		}

		//------------------------------------------------------------
		// The sounds heard in the player's sector
		//------------------------------------------------------------
		const SECTORSOUND_LIST& listSectorSound = *pSectorSounds;
		SECTORSOUND_LIST::const_iterator iSound = listSectorSound.begin();

		while (iSound != listSectorSound.end())
		{
			const SECTORSOUND_INFO& soundInfo = *iSound;

			int zoneSoundID = soundInfo.ZoneSoundID;
			int x = soundInfo.X;
			int y = soundInfo.Y;

			ZONESOUND_INFO* pInfo = g_pZoneSoundTable->GetData( zoneSoundID );

			if (pInfo!=NULL)
			{
				const std::optional<BYTE> hour = s_pHost->GameHour();

				if (pInfo->IsShowTime(now, hour))
				{
					ZONESOUND_NODE* pSound = GetData( zoneSoundID );

					if (pSound==NULL)
					{
						pSound = new ZONESOUND_NODE( pInfo->SoundID );
						AddData( zoneSoundID, pSound );
					}

					pSound->Play( x, y, pInfo->Loop );

					pInfo->SetNextShowTime(now);
				}
				else if (!pInfo->IsShowHour(hour))
				{
					// Outside its hours.
					ZONESOUND_NODE* pSound = GetData( zoneSoundID );

					if (pSound!=NULL)
					{
						pSound->Stop();
					}
				}
				else
				{
					ZONESOUND_NODE* pSound = GetData( zoneSoundID );

					if (pSound!=NULL && pSound->IsLoop())
					{
						pSound->SetContinueLoop();
					}
				}
			}

			iSound++;
		}

		//------------------------------------------------------------
		// A loop not heard again stops looping.
		//------------------------------------------------------------
		iPlaySound = begin();

		while (iPlaySound != end())
		{
			ZONESOUND_NODE* pNode = iPlaySound->second;

			if (pNode->IsLoop() && !pNode->IsContinueLoop())
			{
				pNode->StopLoop();
			}

			iPlaySound++;
		}

		m_LastX = static_cast<TYPE_SECTORPOSITION>(playerX);
		m_LastY = static_cast<TYPE_SECTORPOSITION>(playerY);
		m_LastUpdateTime = now;
	}
}
