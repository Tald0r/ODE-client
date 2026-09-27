//----------------------------------------------------------------------
// RequestUserManager.h
//----------------------------------------------------------------------
// The other clients this one knows an address for: name, IP and the
// UDP port their party datagrams go to.
//
// The book is written by CRWhisperHandler, when a peer whispers to this
// client over a direct connection (IP only; the port stays 0), and by
// the three UDP party handlers (RCPositionInfo, RCCharacterInfo,
// RCStatusHP), which fill in IP and port for a name that is already
// there. It is read by GCPartyPositionHandler and by the party
// broadcasts in MPlayer and UIMessageManager, for the port to send to.
// In a fleet built from this source it is inert: no client here ever
// whispers a peer directly, and this client sends its own party
// datagrams with the name unset, so no entry is ever created and every
// port lookup falls back to the configured default. That was already so
// before the outbound side went.
// It used to carry more: the connection status of each peer, the TCP
// port this client would dial it on, and a second map of names whose IP
// had been asked of the game server and what for. All of that served
// the outbound peer side - this client dialling peers - which upstream
// had compiled out and docs/RESTRUCTURING.md task 5.2's eighth slice
// deleted, together with RemoveRequestUser, RemoveRequestUserLater and
// Update.
//----------------------------------------------------------------------

#ifndef __REQUEST_USER_MANAGER_H__
#define __REQUEST_USER_MANAGER_H__


#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include <map>
#include <string>

//----------------------------------------------------------------------
// RequestUserInfo
//----------------------------------------------------------------------
// What is known about one other client.
//----------------------------------------------------------------------
class RequestUserInfo {
	public :
		RequestUserInfo()
		{
			UDPPort = 0;
		}

	public :
		std::string		Name;
		std::string		IP;
		int				UDPPort;		// client communication UDP port
};

//----------------------------------------------------------------------
// RequestUserManager
//----------------------------------------------------------------------
class RequestUserManager {
	public :
		typedef std::map<std::string, RequestUserInfo*>		REQUEST_USER_MAP;

	public :
		RequestUserManager();
		~RequestUserManager();

		//-------------------------------------------------------------
		// Release
		//-------------------------------------------------------------
		void				Release();

		//-------------------------------------------------------------
		// Add User
		//-------------------------------------------------------------
		bool				HasRequestUser(const char* pName) const;
		void				AddRequestUser(const char* pName, const char* pIP, int UDPPort=0);

		//-------------------------------------------------------------
		// Get
		//-------------------------------------------------------------
		RequestUserInfo*	GetUserInfo(const char* pName) const;

	protected :
		//----------------------------------------------------------------------
		// Lock / Unlock
		//----------------------------------------------------------------------
		void		Lock()					{ EnterCriticalSection(&m_Lock); }
		void		Unlock()				{ LeaveCriticalSection(&m_Lock); }

	private :
		CRITICAL_SECTION		m_Lock;

		REQUEST_USER_MAP		m_RequestUsers;		// the peers whose IP is known
};

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
extern RequestUserManager*		g_pRequestUserManager;


#endif
