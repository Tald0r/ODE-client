//--------------------------------------------------------------------------------
// RequestServerPlayerManager.cpp
//--------------------------------------------------------------------------------
#include "Client_PCH.h"
#include "RequestServerPlayerManager.h"
#include "WireHost.h"
#include "DebugLog.h"

// Platform-specific threading includes. Off Windows the Win32 thread
// names this file uses (TerminateThread, GetCurrentThread,
// SetThreadPriority, CloseHandle and the THREAD_PRIORITY_* constants)
// come from Platform.h's shim, and SDL_Delay from the SDL header it
// includes; this file used to carry a private pthread-flavoured copy of
// them, applied to a thread that platform_thread_create had made as an
// SDL_Thread*.
#ifdef PLATFORM_WINDOWS
	#include <windows.h>
	#include <process.h>
#elif defined(PLATFORM_POSIX)
	#include <unistd.h>
#endif

//--------------------------------------------------------------------------------
// Global
//--------------------------------------------------------------------------------
RequestServerPlayerManager*	g_pRequestServerPlayerManager = NULL;

LONG					WaitRequestThreadProc(LPVOID lpParameter);

//--------------------------------------------------------------------------------
//
// constructor / destructor
//
//--------------------------------------------------------------------------------
RequestServerPlayerManager::RequestServerPlayerManager()
{
	m_pServerSocket		= NULL;
	m_hRequestThread	= NULL;
	m_bThreadRunning	= false;

	InitializeCriticalSection(&m_Lock);
}

RequestServerPlayerManager::~RequestServerPlayerManager()
{
	Release();

	DeleteCriticalSection(&m_Lock);
}

//--------------------------------------------------------------------------------
// Release
//--------------------------------------------------------------------------------
void
RequestServerPlayerManager::Release()
{
	// Signal thread to stop gracefully
	m_bThreadRunning = false;

	// Wait for thread to exit (with timeout)
	if (m_hRequestThread != NULL)
	{
#ifdef PLATFORM_WINDOWS
		// Windows: Wait up to 2 seconds for thread to exit
		DWORD waitResult = WaitForSingleObject(m_hRequestThread, 2000);
		if (waitResult == WAIT_TIMEOUT)
		{
			// Thread didn't exit gracefully, force terminate
			TerminateThread(m_hRequestThread, 0);
		}
		CloseHandle(m_hRequestThread);
#else
		// macOS/Linux: pthread_join with timeout simulation
		// Note: We don't force terminate on POSIX as it can corrupt heap
		// Just close the handle and let the thread exit on its own
		CloseHandle(m_hRequestThread);
#endif
		m_hRequestThread = NULL;
	}

	try {
		if (m_pServerSocket!=NULL)
		{
			delete m_pServerSocket;
			m_pServerSocket = NULL;
		}
	} catch (Throwable&) {
	}

	Lock();

	RequestServerPlayer_LIST::iterator iPlayer = m_listRequestServerPlayer.begin();

	while (iPlayer != m_listRequestServerPlayer.end())
	{
		RequestServerPlayer* pPlayer = *iPlayer;

		try {
			delete pPlayer;
		} catch (Throwable&) {
		}

		iPlayer++;
	}

	m_listRequestServerPlayer.clear();

	Unlock();
}

//--------------------------------------------------------------------------------
// Add RequestServerPlayer
//--------------------------------------------------------------------------------
bool
RequestServerPlayerManager::AddRequestServerPlayer(RequestServerPlayer* pRequestServerPlayer)
{
	if (1)//g_Mode==MODE_GAME)
	{
		Lock();

		// 넘 많을 경우는 더 이상 요청을 안 받도록 해야한다.
		if (m_listRequestServerPlayer.size() < static_cast<RequestServerPlayer_LIST::size_type>(Wire::MaxRequestService()))
		{
			// 일단 list에 넣어둔다.
			m_listRequestServerPlayer.push_back( pRequestServerPlayer );

			Unlock();
			return true;
		}
		else
		{
			pRequestServerPlayer->disconnect( UNDISCONNECTED );
			delete pRequestServerPlayer;
		}
		
		Unlock();		
	}
	/*
	else
	{
		pRequestServerPlayer->disconnect( UNDISCONNECTED );
		delete pRequestServerPlayer;
	}
	*/

	return false;	
}

//--------------------------------------------------------------------------------
// Disconnect
//--------------------------------------------------------------------------------
void		
RequestServerPlayerManager::Disconnect(const char* pName)
{
	Lock();


	RequestServerPlayer_LIST::iterator iPlayer = m_listRequestServerPlayer.begin();
		
	while (iPlayer != m_listRequestServerPlayer.end())
	{
		RequestServerPlayer* pPlayer = *iPlayer;

		// 같은 이름의 player의 접속을 해제시킨다.
		if (pPlayer->getName()==pName)
		{
			pPlayer->disconnect(UNDISCONNECTED);
			delete pPlayer;

			m_listRequestServerPlayer.erase( iPlayer );

			Unlock();
			return;
		}
		
		iPlayer++;
	}


	Unlock();
}

//--------------------------------------------------------------------------------
// Broadcast
//--------------------------------------------------------------------------------
void		
RequestServerPlayerManager::Broadcast(Packet* pPacket)
{
	Lock();

	// 나에게 접속한 모든 player들에게 packet을 전송한다.
	RequestServerPlayer_LIST::iterator iPlayer = m_listRequestServerPlayer.begin();
		
	while (iPlayer != m_listRequestServerPlayer.end())
	{
		RequestServerPlayer* pPlayer = *iPlayer;

		pPlayer->sendPacket( pPacket );
		
		iPlayer++;
	}

	Unlock();
}


//--------------------------------------------------------------------------------
// ProcessMode
//--------------------------------------------------------------------------------
void
RequestServerPlayerManager::ProcessMode(RequestServerPlayer* pPlayer)
{
	(void)pPlayer;
	// Upstream left this entire body commented out: a periodic
	// position broadcast that was never finished. Deleted with the
	// file's move into packetwire, because the dead lines named
	// g_pPlayer and g_CurrentTime and a library source naming an
	// executable global is what ratchet R4 exists to flag. Recover it
	// from history if the feature is ever wanted.
}


//--------------------------------------------------------------------------------
// Update
//--------------------------------------------------------------------------------
void
RequestServerPlayerManager::Update()
{
	Lock();

	if (m_listRequestServerPlayer.empty())
	{
		Unlock();
		return;
	}

	try {

		RequestServerPlayer_LIST::iterator iPlayer = m_listRequestServerPlayer.begin();

		while (iPlayer != m_listRequestServerPlayer.end())
		{
			RequestServerPlayer* pPlayer = *iPlayer;

			try {

				if (!pPlayer->getSocket()->isValid())
				{
					throw SocketException("sock error");
				}

				ProcessMode( pPlayer );

				pPlayer->processInput();
				pPlayer->processCommand();
				pPlayer->processOutput();

			} catch (NonBlockingIOException& t) {

				// 무시..
				DEBUG_ADD_ERR( t.toString().c_str() );

			} catch (Throwable &t) 	{

				DEBUG_ADD_ERR( t.toString().c_str() );


				// exception이 나면 무조건 잘라버린다. --;
				pPlayer->disconnect(UNDISCONNECTED);
				delete pPlayer;

				RequestServerPlayer_LIST::iterator iTemp = iPlayer;
				iPlayer++;

				m_listRequestServerPlayer.erase( iTemp );

				continue;
			}

			iPlayer++;
		}

	} catch (Throwable&t) {
		DEBUG_ADD_ERR( t.toString().c_str() );
	}

	Unlock();
}

//--------------------------------------------------------------------------------
// Init
//--------------------------------------------------------------------------------
void
RequestServerPlayerManager::Init(int port)
{
	if (m_pServerSocket!=NULL)
	{
		delete m_pServerSocket;
	}

	m_pServerSocket = new ServerSocket( port );

	// Set running flag before creating thread
	m_bThreadRunning = true;

	DWORD dwChildThreadID;	// 의미 없당 -- ;

#ifdef PLATFORM_WINDOWS
	m_hRequestThread = CreateThread(NULL,
								0,	// default stack size
								(LPTHREAD_START_ROUTINE)WaitRequestThreadProc,
								this,
								NULL,
								&dwChildThreadID);

	// priority는 낮게
	SetThreadPriority(m_hRequestThread, THREAD_PRIORITY_LOWEST);
#else
	// Non-Windows: Use platform_thread_create
	m_hRequestThread = (HANDLE)platform_thread_create(
		(platform_thread_func_t)WaitRequestThreadProc,
		this
	);
	(void)dwChildThreadID; // unused on non-Windows
#endif
}

//--------------------------------------------------------------------------------
// Wait Request
//--------------------------------------------------------------------------------
void
RequestServerPlayerManager::WaitRequest()
{
	// Check if socket is still valid (may be NULL during shutdown)
	if (m_pServerSocket == NULL || !m_bThreadRunning)
	{
		return;
	}

	Socket* pSocket = m_pServerSocket->accept();

	// accept() returns NULL if no connection or socket closed
	if (pSocket == NULL)
	{
		return;
	}

	// request에 등록
	RequestServerPlayer* pRequestServerPlayer = new RequestServerPlayer( pSocket );

	pRequestServerPlayer->setPlayerStatus( CPS_REQUEST_SERVER_BEGIN_SESSION );

	pSocket->setNonBlocking();

#ifdef PLATFORM_WINDOWS
	SetThreadPriority(m_hRequestThread, THREAD_PRIORITY_NORMAL);
#else
	// Non-Windows: Thread priority not supported via platform_thread_create
#endif

	if (AddRequestServerPlayer( pRequestServerPlayer ))
	{
		// g_pDebugMessage에 lock걸어야 한다. - -;
		//DEBUG_ADD_FORMAT("[Request] New Connection from %s:%d", pSocket->getHost().c_str(), pSocket->getPort());
	}

	SetThreadPriority(m_hRequestThread, THREAD_PRIORITY_LOWEST);
}

//--------------------------------------------------------------------------------
// WaitRequest
//--------------------------------------------------------------------------------
LONG
WaitRequestThreadProc(LPVOID lpParameter)
{
	RequestServerPlayerManager* pRequestServerPlayerManager = (RequestServerPlayerManager*)lpParameter;

	while (pRequestServerPlayerManager != NULL && pRequestServerPlayerManager->IsThreadRunning())
	{
		pRequestServerPlayerManager->WaitRequest();

		// Add delay to prevent CPU busy-wait
		// On non-Windows platforms, sleep for 10ms to reduce CPU usage
#ifndef PLATFORM_WINDOWS
		SDL_Delay(10);  // 10ms delay to reduce CPU from busy-wait loop
#endif
	}

	return 0L;
}
