// CImm.cpp: implementation of the CImm class.
//
//////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"

#include "CImm.h"
#include "VS_UI_filepath.h"
#include "CSoundPartManager.h"
#include "MSoundTable.h"


#ifdef PLATFORM_WINDOWS
// Keep the device pointer with its implementation so standalone UI components
// can link their real feedback adapter without pulling in the game UI loop.
CImm *gpC_Imm = NULL;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// The real Immersion Force Feedback classes this file used to drive
// (CImmDevice/CImmPeriodic/CImmProject/CImmEffect - see VS_UI/src/Imm/
// Imm*.h) are declared DLLIFC (__declspec(dllimport)) against Immersion
// Corporation's proprietary IFC runtime DLL. That DLL/.lib was never part
// of this project and doesn't exist anywhere in this environment, so any
// code that actually constructs one of those types (which requires their
// vtable/constructor to be linkable) fails at link time - LNK2019 for
// CImmDevice::CreateDevice()/CImmPeriodic/CImmProject's own constructors,
// and LNK2001 for the rest of CImmPeriodic/CImmEffect's virtual functions
// (needed just to emit those types' vtables). See 참고자료/작업필요stub.md.
//
// This class now never instantiates any of them: m_pDevice is always
// NULL, so IsDevice() is always false, and every other method below is a
// no-op via its own IsDevice() guard - matching the already-established
// pattern for other Windows features that lost their backing
// implementation (CSDLGraphics::GetDD(); WavePackFileInfo::LoadFromFileData()
// was another until task 4.263 retired the wave-pack manager).

CImm::CImm()
{
	m_bPlay = false;
	m_pDevice = NULL;

	m_ProjectAction = NULL;
	m_ProjectSkill = NULL;
	m_ProjectInventory = NULL;
	m_ProjectUseItem = NULL;
}

CImm::~CImm()
{
	// m_pDevice is always NULL (see constructor above) - nothing to release.
}


void CImm::Enable()
{
	if(!IsDevice())return;
}

void CImm::Disable()
{
	if(!IsDevice())return;
}

// The bodies below used to call into CImmProject::Start()/CImmPeriodic::
// Start() (via m_vUI[]) - direct (non-virtual) calls into the same
// unavailable Immersion DLL, so referencing them at all is enough to fail
// the link even behind an always-false guard (m_pDevice/m_ProjectXxx are
// always NULL - see the constructor above). Kept as no-ops instead of
// deleting the parameters outright, in case a future real implementation
// wants the sound_id/ID inputs back.

//UI
void CImm::ForceUI(const unsigned int ID) const
{
	(void)ID;
}


void CImm::ForceAction(const int sound_id) const
{
	(void)sound_id;
}


void CImm::ForceSkill(const int sound_id) const
{
	(void)sound_id;
}


void CImm::ForceUseItem(const int sound_id) const
{
	(void)sound_id;
}


void CImm::ForceInventory(const int sound_id) const
{
	(void)sound_id;
}
