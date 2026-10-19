//-----------------------------------------------------------------------------
// CWaitPacketUpdate.h
//-----------------------------------------------------------------------------
// The loop that waits for a message from the server. It is gamemodel's;
// what it does to the program around it goes through CWaitPacketHost,
// which GameInit installs (task 4.209).
//-----------------------------------------------------------------------------

#ifndef	__CWAITPACKETUPDATE_H__
#define	__CWAITPACKETUPDATE_H__

#include "WinLib/CWinUpdate.h"
#include "MonotonicClock.h"
#include "ClientMode.h"

struct CWaitPacketHost
{
	void (*ClearInputReceivers)();		// no mouse or keyboard receiver while waiting
	void (*StampFrameClock)();			// g_FrameNow = now
	void (*SetMode)(CLIENT_MODE mode);
	void (*UpdateDisconnected)();
	bool (*UpdateSocketInput)();		// false: nothing more this frame
	bool (*UpdateSocketOutput)();
};

class CWaitPacketUpdate : public CWinUpdate {
	public :
		explicit CWaitPacketUpdate(const CWaitPacketHost* pHost) : m_pHost(pHost)	{ m_DelayLimit = (MonotonicClock::TimePoint::max)(); }  // Will be set by SetDelay()
		~CWaitPacketUpdate() {}

		// No input receivers while waiting.
		void		Init();

		// Wait delay milliseconds from now, then give up.
		void		SetDelay(DWORD delay);		

		// On timeout, back to the main menu, disconnected; then the socket
		// input, the queued next mode, or the socket output.
		void		Update();		

	protected :				
		const CWaitPacketHost*		m_pHost;
		MonotonicClock::TimePoint	m_DelayLimit;	// give up waiting past this point
};


// The one instance, which GameInit creates with its host (the executable's
// until task 4.262).
extern CWaitPacketUpdate*		g_pCWaitPacketUpdate;

#endif
