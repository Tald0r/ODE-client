//---------------------------------------------------------------------------
// RequestFileManager.cpp
//---------------------------------------------------------------------------
#include "Client_PCH.h"
#include "RequestFileManager.h"
#include "RequestServerPlayer.h"
#include "DataPath.h"

#include "Packet/Rpackets/RCRequestedFile.h"
#include "Packet/Rpackets/RCRequestVerify.h"

#include "DebugLog.h"

//---------------------------------------------------------------------------
// Global
//---------------------------------------------------------------------------
RequestFileManager* g_pRequestFileManager = NULL;

#define	MAX_BUFFER	4096		// 4K

//---------------------------------------------------------------------------
//
//				Request SendInfo
//
//---------------------------------------------------------------------------
RequestSendInfo::RequestSendInfo(const char* pRequestUser)
{
	m_RequestUser = pRequestUser;
}

RequestSendInfo::~RequestSendInfo()
{
	while (!m_FileInfos.empty()) 
	{ 
		delete m_FileInfos.front(); 
		m_FileInfos.pop_front(); 
	}		
}

//---------------------------------------------------------------------------
// Make RCRequestedFile Packet
//---------------------------------------------------------------------------
void				
RequestSendInfo::MakeRCRequestedFilePacket(RCRequestedFile& packet) const
{
	std::list<SendFileInfo*>::const_iterator iInfo = m_FileInfos.begin();

	// Only the first file is announced.
	//while (iInfo != m_FileInfos.end())
	if (iInfo != m_FileInfos.end())
	{
		SendFileInfo* pInfo = *iInfo;

		RCRequestedFileInfo* pFileInfo = new RCRequestedFileInfo;

		pFileInfo->setRequestFileType( pInfo->GetFileType() );
		pFileInfo->setVersion( 0 );
		pFileInfo->setFilename ( pInfo->GetFilename() );
		pFileInfo->setFileSize ( pInfo->GetFileSizeLeft() );

		packet.addInfo( pFileInfo );

		iInfo ++;
	}
}

SendFileInfo::SendFileInfo(const char* pFilename, 
							REQUEST_FILE_TYPE fileType)
{
	m_Mode		= REQUEST_FILE_MODE_BEFORE;
	m_FileType	= REQUEST_FILE_NULL;

	m_Filename	= pFilename;
	m_FileType	= fileType;
}

SendFileInfo::~SendFileInfo()
{
	// close the file
	if (m_File.IsOpen())
	{
		m_File.Close();
	}
}

//---------------------------------------------------------------------------
// Start Send
//---------------------------------------------------------------------------
void		
SendFileInfo::StartSend()
{
	m_Mode = REQUEST_FILE_MODE_SEND;

	// m_Filename stays as ProfileManager spelled it ("<dir>\<file>"),
	// since that is the name sent to the peer; only the open resolves
	// it for the disk (basic/DataPath.h, the identity on Windows).
	m_File.Open( Basic::NormalizeDataPath(m_Filename) );
}

//---------------------------------------------------------------------------
// Send
//---------------------------------------------------------------------------
DWORD		
SendFileInfo::Send(char* pBuffer)
{
	return m_File.Read(pBuffer, MAX_BUFFER);
}

//---------------------------------------------------------------------------
// Send
//---------------------------------------------------------------------------
// -_-;;
//---------------------------------------------------------------------------
void
SendFileInfo::SendBack(DWORD nBack)
{
	m_File.Unread( nBack );
}

//---------------------------------------------------------------------------
// End Send
//---------------------------------------------------------------------------
void		
SendFileInfo::EndSend()
{
	m_Mode = REQUEST_FILE_MODE_AFTER;

	m_File.Close();
}

//---------------------------------------------------------------------------
//
//				RequestFileManager
//
//---------------------------------------------------------------------------
RequestFileManager::RequestFileManager()
{
}

RequestFileManager::~RequestFileManager()
{
	Release();
}

//---------------------------------------------------------------------------
// Release
//---------------------------------------------------------------------------
void			
RequestFileManager::Release()
{
	//------------------------------------------------------------
	// OtherRequest
	//------------------------------------------------------------
	REQUEST_SEND_MAP::iterator iOther = m_OtherRequests.begin();

	while (iOther != m_OtherRequests.end())
	{
		RequestSendInfo* pInfo = iOther->second;

		delete pInfo;

		iOther++;
	}

	m_OtherRequests.clear();
}

//---------------------------------------------------------------------------
// Add OtherRequest
//---------------------------------------------------------------------------
bool
RequestFileManager::AddOtherRequest(RequestSendInfo* pInfo)
{
	REQUEST_SEND_MAP::iterator iOther = m_OtherRequests.find( pInfo->GetRequestUser() );

	if (iOther == m_OtherRequests.end())
	{
		m_OtherRequests[pInfo->GetRequestUser()] = pInfo;

		return true;
	}

	delete pInfo;

	return false;
}

//---------------------------------------------------------------------------
// Remove OtherRequest
//---------------------------------------------------------------------------
bool
RequestFileManager::RemoveOtherRequest(const std::string& name)
{
	REQUEST_SEND_MAP::iterator iOther = m_OtherRequests.find( name );

	if (iOther != m_OtherRequests.end())
	{
		RequestSendInfo* pInfo = iOther->second;

		delete pInfo;

		m_OtherRequests.erase( iOther );

		return true;
	}

	return false;
}

//---------------------------------------------------------------------------
// Has OtherRequest
//---------------------------------------------------------------------------
bool
RequestFileManager::HasOtherRequest(const std::string& name) const
{
	REQUEST_SEND_MAP::const_iterator iOther = m_OtherRequests.find( name );

	if (iOther != m_OtherRequests.end())
	{
		return true;
	}

	return false;
}

//---------------------------------------------------------------------------
// Send OtherRequest
//---------------------------------------------------------------------------
// True means a file is being sent, so the
// RequestServerPlayer need not process its commands.
//---------------------------------------------------------------------------
bool			
RequestFileManager::SendOtherRequest(const std::string& name, RequestServerPlayer* pRequestServerPlayer)
{
	REQUEST_SEND_MAP::iterator iOther = m_OtherRequests.find( name );

	if (iOther == m_OtherRequests.end())
	{
		return false;
	}

	RequestSendInfo* pInfo = iOther->second;	

	SendFileInfo* pFileInfo = pInfo->GetFront();

	if (pFileInfo!=NULL)
	{
		switch (pFileInfo->GetMode())
		{
			//------------------------------------------------------------------
			// Before the file is sent
			//------------------------------------------------------------------
			case REQUEST_FILE_MODE_BEFORE :
			{
				pFileInfo->StartSend();

				if (pInfo->GetSize()!=0)
				{
					RCRequestedFile rcRequestedFile;

					pInfo->MakeRCRequestedFilePacket(rcRequestedFile);
					
					pRequestServerPlayer->sendPacket( &rcRequestedFile );
				}
				else
				{
					throw ConnectException("No File to Send");
				}
			}
			return false;

			//------------------------------------------------------------------
			// While the file is sent
			//------------------------------------------------------------------
			case REQUEST_FILE_MODE_SEND :
			{
				char buf[MAX_BUFFER+1];	// 10k
				
				DWORD nRead = pFileInfo->Send(buf);

				if (nRead > 0)
				{
					DWORD nSent = pRequestServerPlayer->send( buf , nRead );

					if (nSent!=nRead)
					{
						// Not all of it went out; read the rest again.
						// socketInputStream
						pFileInfo->SendBack( nRead - nSent );
					}
				}

				if (nRead < MAX_BUFFER)	// the file is read to its end
				{
					if (pFileInfo->GetFileSizeLeft()==0)
					{
						pFileInfo->EndSend();

						pInfo->DeleteFront();

						if (pInfo->IsEnd())
						{
							// Every file is sent.
							RCRequestVerify rcRequestVerify;
							rcRequestVerify.setCode( REQUEST_VERIFY_PROFILE_DONE );

							pRequestServerPlayer->sendPacket( &rcRequestVerify );

							// Forget the request.
							delete pInfo;
							m_OtherRequests.erase( iOther );
						}
						else
						{
							//pFileInfo = pInfo->GetFront();

							// Files remain: the next call announces the next one.							
						}
					}
					else
					{
						DEBUG_ADD_FORMAT_ERR("[Error] FileSizeLeft = %d", pFileInfo->GetFileSizeLeft());
					}
				}			
			}
			return true;
			
			//------------------------------------------------------------------
			// After the file is sent
			//------------------------------------------------------------------
			case REQUEST_FILE_MODE_AFTER :
			return false;
			default:
				break;
		}
	}

	return false;
}
