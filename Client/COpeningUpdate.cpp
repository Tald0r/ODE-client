//---------------------------------------------------------------------------
// COpeningUpdate.cpp
//---------------------------------------------------------------------------
// Opening 동영상을 보여줄때의 loop
//---------------------------------------------------------------------------
#include "Client_PCH.h"
#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif
#include "Client.h"
#include "ServerInfo.h"
#include "COpeningUpdate.h"

#include "DebugInfo.h"
//#define	new			DEBUG_NEW
//#define	delete		DEBUG_DELETE

// Global
COpeningUpdate*		g_pCOpeningUpdate;


//-----------------------------------------------------------------------------
// Init
//-----------------------------------------------------------------------------
void
COpeningUpdate::Init()
{
	// mouse event 처리
	g_pSDLInput->SetMouseEventReceiver( NULL );

	// keyboard event 처리
	g_pSDLInput->SetKeyboardEventReceiver( NULL );
}

//---------------------------------------------------------------------------
// Opening AVI
//---------------------------------------------------------------------------
void
COpeningUpdate::PlayMPG(const char* filename)
{
	// AVI
	if (g_pAvi != NULL)
	{
		//#ifdef DEBUG_NEW
		//	operator delete ( g_pAvi, __FILE__, __LINE__ );
		//#else
			delete g_pAvi;
		//#endif
	}

	g_pAvi = new CAVI;
	if(!g_pAvi->OpenMPG(g_hWnd, filename, 640, 480))
	{
		// CAVI::OpenMPG is a permanent stub (CAvi.cpp) - MPG playback is
		// deliberately unimplemented, so this fires on every entry to the
		// opening screen. Log it instead of blocking on a modal error box.
		char str[256];
		snprintf(str, sizeof(str), "Not Found %s", filename);
		DEBUG_ADD(str);
		return;
	}
	g_pAvi->Play();
}


//-----------------------------------------------------------------------------
// UpdateOpening
//-----------------------------------------------------------------------------
// 접속 전..
//-----------------------------------------------------------------------------
void		
COpeningUpdate::Update()
{
	//------------------------------------------
	// Input
	//------------------------------------------	
	[[maybe_unused]] extern bool	g_bTestMode;
	if (g_bActiveGame
#ifdef OUTPUT_DEBUG
		|| g_bTestMode
#endif
		)
	{
		UpdateInput();
		UpdateMouse();

		ProcessInput();
	}

	if (!g_bActiveApp)
		return;

	//------------------------------------------
	// Draw
	//------------------------------------------
	//if (g_bActiveGame)
	//	UpdateLoginDraw();	
}


//---------------------------------------------------------------------------
// Update Opening Input
//---------------------------------------------------------------------------
void
COpeningUpdate::ProcessInput()
{
}
