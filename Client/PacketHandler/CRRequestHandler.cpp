//////////////////////////////////////////////////////////////////////
//
// Filename    : CRRequestHandler.cc
// Written By  : crazydog
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Rpackets/CRRequest.h"
#include "RequestServerPlayer.h"
#include "UserInformation.h"
#include "RequestFileManager.h"
#include "RequestUserManager.h"
#include "Rpackets/RCRequestVerify.h"
#include "CommunicationPresentationHost.h"
#include "DebugLog.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CRRequestHandler::execute ( CRRequest * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
		

	DEBUG_ADD("CRRequestHandler");

	RequestServerPlayer* pRequestServerPlayer = dynamic_cast<RequestServerPlayer*>( pPlayer );

	// Only a peer's connection asks for files; the game server's connection
	// reads any packet id it is sent, this one too.
	if (pRequestServerPlayer==NULL)
	{
		return;
	}

	// No name: the IP stands for it.
	if (pRequestServerPlayer->getName().c_str()==NULL
		|| pRequestServerPlayer->getName().size()==0)
	{
		DEBUG_ADD("setName");
		pRequestServerPlayer->setName( pPlayer->getSocket()->getHost().c_str() );		
		DEBUG_ADD("setName OK");
	}
	
	const std::string& otherName =	pRequestServerPlayer->getName();


	if (g_pUserInformation!=NULL
		&& CommunicationPresentation::HasProfiles()
		&& g_pRequestFileManager!=NULL
		&& g_pRequestUserManager!=NULL)
	{			
		DEBUG_ADD("Not NULL");

		// Set the user's information again
		//RequestUserInfo* pUserInfo = g_pRequestUserManager->GetUserInfo( otherName.c_str() );

		//if (pUserInfo!=NULL)
		{
		//	Socket* pSocket = pPlayer->getSocket();
		//	pUserInfo->IP = pSocket->getHost().c_str();
		//	pUserInfo->TCPPort = pSocket->getPort();
		}

		switch (pPacket->getCode())
		{
			//------------------------------------------------------------------
			//
			//			A profile asked for,
			//
			//------------------------------------------------------------------
			case CR_REQUEST_FILE_PROFILE :
			{
				DEBUG_ADD("Request Profile");

				const char* pMyName = g_pUserInformation->CharacterID.GetString();

				// No character chosen yet: no profile is the player's.
				if (pMyName!=NULL && pPacket->getRequestName()==pMyName)
				{
					const char* pFilename = CommunicationPresentation::GetProfileFilename( pMyName );

					//-------------------------------------------------------------
					// There is a profile
					//-------------------------------------------------------------
					if (pFilename != NULL)
					{
						DEBUG_ADD("profile not NULL");

						// Nothing asked for already..
						if (!g_pRequestFileManager->HasOtherRequest(otherName))
						{
							DEBUG_ADD("no Request");

							std::string filenameIndex = pFilename;
							filenameIndex += "i";

							RequestSendInfo* pInfo = new RequestSendInfo(otherName.c_str());							
							
							pInfo->AddSendFileInfo( new SendFileInfo(filenameIndex.c_str(), REQUEST_FILE_PROFILE_INDEX) );
							pInfo->AddSendFileInfo( new SendFileInfo(pFilename, REQUEST_FILE_PROFILE) );							

							DEBUG_ADD("add OtherReq");

							if (g_pRequestFileManager->AddOtherRequest(pInfo))
							{
								DEBUG_ADD("add otherReq OK");

								pRequestServerPlayer->setRequestMode(REQUEST_CLIENT_MODE_PROFILE);

								pRequestServerPlayer->setPlayerStatus(CPS_REQUEST_SERVER_NORMAL);
							}
							else
							{
								DEBUG_ADD("not add otherReq");

								//pRequestServerPlayer->disconnect( UNDISCONNECTED );

								DEBUG_ADD("disconnectExp");
								throw DisconnectException("Already Requested");
							}
						}					
					}
					//-------------------------------------------------------------
					// There is no profile at all
					//-------------------------------------------------------------
					else
					{
						DEBUG_ADD("send RCRequestVerify");

						RCRequestVerify rcRequestVerify;
						rcRequestVerify.setCode( REQUEST_VERIFY_PROFILE_NOT_EXIST );

						pRequestServerPlayer->sendPacket( &rcRequestVerify );

						pRequestServerPlayer->setPlayerStatus( CPS_END_SESSION );
						//pRequestServerPlayer->disconnect( UNDISCONNECTED );

						DEBUG_ADD("disconnectExp");
						throw DisconnectException("No Profile");
					}
				}
				//-------------------------------------------------------------
				// Someone else's profile asked for
				//-------------------------------------------------------------
				else
				{
					RCRequestVerify rcRequestVerify;
					rcRequestVerify.setCode( REQUEST_VERIFY_PROFILE_WRONG_USER );

					pRequestServerPlayer->sendPacket( &rcRequestVerify );

					pRequestServerPlayer->setPlayerStatus( CPS_END_SESSION );
					//pRequestServerPlayer->disconnect( UNDISCONNECTED );

					DEBUG_ADD("disconnectExp");
					throw DisconnectException("Wrong Profile requested");
				}
			}
			break;
		
			//------------------------------------------------------------------
			//
			//			A guild mark asked for,
			//
			//------------------------------------------------------------------
			case CR_REQUEST_FILE_GUILD_MARK :

				DEBUG_ADD("Request GuildMark");

				pRequestServerPlayer->setRequestMode(REQUEST_CLIENT_MODE_GUILDMARK);
				pRequestServerPlayer->setPlayerStatus(CPS_REQUEST_SERVER_NORMAL);
			break;

			default:
				break;
		}
	}

	DEBUG_ADD("CRRequestHandler End");


	__END_CATCH
}
