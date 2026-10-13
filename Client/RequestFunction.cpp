//--------------------------------------------------------------------------------
// RequestFunction.cpp
//--------------------------------------------------------------------------------

#include "Client_PCH.h"
#include "RequestServerPlayerManager.h"
#include "DebugLog.h"

//--------------------------------------------------------------------------------
// Request Disconnect
//--------------------------------------------------------------------------------
// Drop the connection the character called Name has to this client - a
// peer that sent CRDisconnect. This used to close the connection in the
// other direction too; that manager went with the outbound peer side
// (docs/RESTRUCTURING.md task 5.2, eighth slice). The format used to
// print the name with %d.
//--------------------------------------------------------------------------------
void	
RequestDisconnect(const char* pName)
{
	DEBUG_ADD_FORMAT("[RequestDisconnect] name=%s", pName);

	if (g_pRequestServerPlayerManager!=NULL)
	{
		g_pRequestServerPlayerManager->Disconnect( pName );
	}
}
