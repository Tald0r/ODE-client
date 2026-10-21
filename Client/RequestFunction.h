//--------------------------------------------------------------------------------
// RequestFunction.h
//--------------------------------------------------------------------------------
// Functions on the peer connections.
//--------------------------------------------------------------------------------

#ifndef __REQUESTFUNCTION_H__
#define __REQUESTFUNCTION_H__

//--------------------------------------------------------------------------------
// Drop the connection the character called Name has to this client.
// (RequestConnect, which dialled a peer, went with the outbound peer
// side - docs/reviews/RESTRUCTURING.md task 5.2, eighth slice.)
//--------------------------------------------------------------------------------
void	RequestDisconnect(const char* pName);

#endif
