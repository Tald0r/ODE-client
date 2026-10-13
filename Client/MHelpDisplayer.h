//---------------------------------------------------------------------------
// MHelpDisplayer.h
//---------------------------------------------------------------------------
// Shows the help messages.
//---------------------------------------------------------------------------

#ifndef __MHELPDISPLAYER_H__
#define __MHELPDISPLAYER_H__

#include "MHelpDef.h"
#include "MonotonicClock.h"

class MHelpDisplayer {
	public :
		MHelpDisplayer();
		~MHelpDisplayer();

		//-----------------------------------------------------
		// Output Help
		//-----------------------------------------------------
		void	OutputHelp(HELP_OUTPUT ho);
		
	protected :
		MonotonicClock::TimePoint	m_DelayTime;	// when the shown help may be replaced
};

extern MHelpDisplayer*		g_pHelpDisplayer;

#endif

