#pragma once

class CMessageArray;

// The message logs the game prints into (CMessageArray, basic's). The
// executable creates them at start-up; the pointers are gamemodel's so
// that the replies that write to them can be tested.
extern CMessageArray*		g_pSystemMessage;
extern CMessageArray*		g_pPlayerMessage;
extern CMessageArray*		g_pNoticeMessage;
extern CMessageArray*		g_pGameMessage;
extern CMessageArray*		g_pHelpMessage;
