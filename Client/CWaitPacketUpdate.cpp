//-----------------------------------------------------------------------------
// CWaitPacketUpdate.cpp
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif
#include "CWaitPacketUpdate.h"
#include "FrameClock.h"
#include "DebugLog.h"

// GameInit creates it with its host (the executable's until task 4.262).
CWaitPacketUpdate*		g_pCWaitPacketUpdate = NULL;

//-----------------------------------------------------------------------------
// Init
//-----------------------------------------------------------------------------
void
CWaitPacketUpdate::Init()
{
	m_pHost->ClearInputReceivers();
}


//-----------------------------------------------------------------------------
// Update WaitPosition
//-----------------------------------------------------------------------------
// Before the game starts: wait for the server, e.g. for the player's position.
//-----------------------------------------------------------------------------
void 
CWaitPacketUpdate::Update()
{
	//DEBUG_ADD("[WaitPacketUpdate] Begin");

	//------------------------------------------	
	// Past the time limit
	//------------------------------------------
	if (g_FrameNow > m_DelayLimit)
	{
		DEBUG_ADD_FORMAT("[WaitPacket] timed out: %d ms past the limit", (int)(g_FrameNow - m_DelayLimit).count());			

		m_pHost->SetMode( MODE_MAINMENU );
		m_pHost->UpdateDisconnected();

	}

	//------------------------------------------
	// Sound Stream
	//------------------------------------------
//	if (g_pSDLStream!=NULL)
//	{
//		//DEBUG_ADD("[WaitPacketUpdate] SoundStream");
//
//		g_pSDLStream->Update();
//	}

	//------------------------------------------
	// The socket
	//------------------------------------------	
	if (!m_pHost->UpdateSocketInput())
	{
		//DEBUG_ADD("[WaitPacketUpdate] SocketInput");

		return;
	}

	//------------------------------------------	
	// The mode changed
	//------------------------------------------	
	if (g_ModeNext!=MODE_NULL)
	{
		DEBUG_ADD_FORMAT("[WaitPacketUpdate] g_ModeNext is Not MODE_NULL(%d)", (int)g_ModeNext);

		m_pHost->SetMode( g_ModeNext );
		g_ModeNext = MODE_NULL;

		return;
	}

	//DEBUG_ADD("[WaitPacketUpdate] SocketOutput");
	
	m_pHost->UpdateSocketOutput();

	//DEBUG_ADD("[WaitPacketUpdate] End");
}

//-----------------------------------------------------------------------------
// Set Delay
//-----------------------------------------------------------------------------
// Keep updating for delay milliseconds from now; if the server has not
// answered by then, give up.
//-----------------------------------------------------------------------------
void		
CWaitPacketUpdate::SetDelay(DWORD delay)
{
	m_pHost->StampFrameClock();

	m_DelayLimit = g_FrameNow + MonotonicClock::Millis(delay);

	DEBUG_ADD_FORMAT("[WaitPacket] Limit=%u ms from now", delay);
}