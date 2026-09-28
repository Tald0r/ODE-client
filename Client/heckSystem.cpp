// heckSystem.cpp: implementation of the CheckSystem class.
//
//////////////////////////////////////////////////////////////////////

//#include "stdafx.h"
//#include "checkSystemVer.h"
#include "heckSystem.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CheckSystem::CheckSystem()
{

}

CheckSystem::~CheckSystem()
{

}

BOOL CheckSystem::GetSystem()
{
	// Every branch of the old version switch was commented out, so the
	// GetVersionEx query it made - deprecated - never changed the result.
	return FALSE;
}
