//----------------------------------------------------------------------
// TempInformation.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "TempInformation.h"
//#include <fstream.h>

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
TempInformation*		g_pTempInformation = NULL;

//----------------------------------------------------------------------
// 
// constructor
//
//----------------------------------------------------------------------
TempInformation::TempInformation()
{
	Mode = MODE_NULL;

	// A reply handler may read a slot before any dialog has written it
	// (GC_PARTY_INVITE_ACCEPT looks up PartyInviter unconditionally), so
	// every scalar slot starts defined: no value, no inviter, no pointer.
	Value1 = 0;
	Value2 = 0;
	Value3 = 0;
	Value4 = 0;
	PartyInviter = 0;
	pValue = NULL;
}

TempInformation::~TempInformation()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
void TempInformation::SetMode(TempInformation::TEMP_MODE mode)
{
#ifdef OUTPUT_DEBUG
//	DEBUG_ADD_FORMAT("[TempInformation] Setmode : %d", mode);
#endif

	Mode = mode;
}

TempInformation::TEMP_MODE TempInformation::GetMode() const
{
	return Mode;
}
