//---------------------------------------------------------------------------
// MHelpDisplayer.cpp
//---------------------------------------------------------------------------
#include "Client_PCH.h"
#include "MHelpDisplayer.h"
#include "MHelpStringTable.h"
#include "../basic/CMessageArray.h"
#include "FrameClock.h"
#include "GameMessages.h"

//---------------------------------------------------------------------------
// Global
//---------------------------------------------------------------------------
MHelpDisplayer*		g_pHelpDisplayer = NULL;

//---------------------------------------------------------------------------
// Define
//---------------------------------------------------------------------------
// How long a shown help keeps the next one back
#define	DELAY_HELP_OUTPUT_LAST		10000		// 10 seconds

//---------------------------------------------------------------------------
//
// constructor / destructor
//
//---------------------------------------------------------------------------
MHelpDisplayer::MHelpDisplayer()
{
	m_DelayTime = MonotonicClock::TimePoint();
}

MHelpDisplayer::~MHelpDisplayer()
{
}

//---------------------------------------------------------------------------
//
// member functions
//
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// OutputHelp (HELP_OUTPUT)
//---------------------------------------------------------------------------
// Shows one HELP_OUTPUT, or not: too many messages at once would be
// confusing, so a help that comes while the last one is still up is
// dropped. A help already shown is not shown again.
//---------------------------------------------------------------------------
void	
MHelpDisplayer::OutputHelp(HELP_OUTPUT ho)
{
	if (ho < g_pHelpStringTable->GetSize())
	{
		if (g_FrameNow > m_DelayTime)
		{
			if (g_pHelpStringTable->IsDisplayed(ho))
			{			
				// Already shown: skip it.
			}
			else
			{
				const MString& helpString = (*g_pHelpStringTable)[ho];

				g_pHelpMessage->Add( helpString.GetString() );

				m_DelayTime = g_FrameNow + MonotonicClock::Millis(DELAY_HELP_OUTPUT_LAST);
			}					
		}	
		else
		{
			// The last help is still up: drop this one.
		}
	}
}

