//----------------------------------------------------------------------
// AffectModifyInfo.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "AffectModifyInfo.h"
#include "MStatus.h"
#include "Packet/ModifyInfo.h"
#include "DebugLog.h"

//------------------------------------------------------------------
// Affect ModifyInfo ( MStatus*, ModifyInfo* )
//------------------------------------------------------------------
void
AffectModifyInfo(MStatus* pStatus, ModifyInfo* pInfo)
{
	//------------------------------------------------------------------
	// Change the status values.
	//------------------------------------------------------------------
	int i;

	SHORTDATA sData;
	LONGDATA lData;

	DEBUG_ADD("AMo");

	int shortNum = pInfo->getShortCount();
	int longNum = pInfo->getLongCount();


	for (i=0; i<shortNum; i++)
	{
		pInfo->popShortData( sData );

		pStatus->SetStatus( sData.type, sData.value );
	}

	DEBUG_ADD("LD");

	for (i=0; i<longNum; i++)
	{
		pInfo->popLongData( lData );

		pStatus->SetStatus( lData.type, lData.value );
	}

	DEBUG_ADD("AM_ok");
}
