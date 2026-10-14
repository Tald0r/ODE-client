#include "Client_PCH.h"
#include "CAvi.h"

CAVI::CAVI()
{
	bEndFlag=0;
}

// CAVI used to drive the Windows "MCI Digital Video" API (MCI_DGV_*
// structures/commands, MCIAVI.DRV) for the opening intro movie. That
// driver is 16/32-bit Video-for-Windows-era technology that was never
// shipped for 64-bit Windows at all, so even defining the missing
// MCI_DGV_* structures ourselves (they're no longer declared in the
// modern Windows 10 SDK's <mmsystem.h>) would only get this to compile,
// not actually work - mciSendCommand(MCI_OPEN, ...) would just fail at
// runtime on any x64 Windows machine, since the OS-level driver doesn't
// exist there. Stubbed out on all platforms instead of only non-Windows.
// NOTE: COpeningUpdate::PlayMPG() (COpeningUpdate.cpp) logs and returns
// without playing when OpenMPG() fails, which now always happens.
int CAVI::OpenMPG(HWND hwnd, LPCSTR szName, int w, int h) { (void)hwnd; (void)szName; (void)w; (void)h; return FALSE; }
int CAVI::OpenAVI(HWND hwnd, LPCSTR szName, int w, int h) { (void)hwnd; (void)szName; (void)w; (void)h; return FALSE; }
void CAVI::Close() {}
int CAVI::Play() { return FALSE; }
void CAVI::Stop() {}
