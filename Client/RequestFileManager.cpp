//---------------------------------------------------------------------------
// RequestFileManager.cpp
//---------------------------------------------------------------------------
#include "Client_PCH.h"
#include "RequestFileManager.h"
#include "RequestServerPlayer.h"
#include "ProfileManager.h"
#include "DataPath.h"

#include "Packet/Rpackets/RCRequestedFile.h"
#include "Packet/Rpackets/RCRequestVerify.h"

#include "ServerInfo.h"
#include "DebugInfo.h"
#include "MTypeDef.h"
#include "UIFunction.h"

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

	// 하나만 넣어둔다.
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

	m_FileSizeLeft = 0;
}

SendFileInfo::~SendFileInfo()
{
	// 화일 닫기
	if (m_FileStream.is_open())
	{
		m_FileStream.close();
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
	m_FileStream.open( Basic::NormalizeDataPath(m_Filename).c_str(), std::ios::in | std::ios::binary);// |  );

	if (m_FileStream.is_open())
	{
		m_FileStream.seekg( 0, std::ios::end );

		m_FileSizeLeft = static_cast<DWORD>(m_FileStream.tellg());	// to find out the file size

		m_FileStream.seekg( 0, std::ios::beg );
	}
	else
	{
		m_FileSizeLeft = 0;
	}
}

//---------------------------------------------------------------------------
// Send
//---------------------------------------------------------------------------
DWORD		
SendFileInfo::Send(char* pBuffer)
{
	m_FileStream.read(pBuffer, MAX_BUFFER);

	DWORD nRead = static_cast<DWORD>(m_FileStream.gcount());

	m_FileSizeLeft -= nRead;

	return nRead;
}

//---------------------------------------------------------------------------
// Send
//---------------------------------------------------------------------------
// -_-;;
//---------------------------------------------------------------------------
void
SendFileInfo::SendBack(DWORD nBack)
{
	m_FileStream.seekg( -nBack, std::ios::cur );
	m_FileSizeLeft += nBack;
}

//---------------------------------------------------------------------------
// End Send
//---------------------------------------------------------------------------
void		
SendFileInfo::EndSend()
{
	m_Mode = REQUEST_FILE_MODE_AFTER;

	m_FileStream.close();
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
// return값이 true이면 화일을 보내는 중이라는 의미이다.
// RequestServerPlayer의 processCommand를 처리할 필요가 없다.
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
			// 화일 보내기 전에
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
			// 화일 보내는 중
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
						// 엽기일까.. - -;
						// socketInputStream
						pFileInfo->SendBack( nRead - nSent );
					}
				}

				if (nRead < MAX_BUFFER)	// 다 읽은 경우
				{
					if (pFileInfo->GetFileSizeLeft()==0)
					{
						pFileInfo->EndSend();

						pInfo->DeleteFront();

						if (pInfo->IsEnd())
						{
							// 다 보냈다~..
							RCRequestVerify rcRequestVerify;
							rcRequestVerify.setCode( REQUEST_VERIFY_PROFILE_DONE );

							pRequestServerPlayer->sendPacket( &rcRequestVerify );

							// 정보 제거
							delete pInfo;
							m_OtherRequests.erase( iOther );
						}
						else
						{
							//pFileInfo = pInfo->GetFront();

							// 덜 보냈으면 다음꺼 또 보낸다.							
						}
					}
					else
					{
						DEBUG_ADD_FORMAT("[Error] FileSizeLeft = %d", pFileInfo->GetFileSizeLeft());
					}
				}			
			}
			return true;
			
			//------------------------------------------------------------------
			// 화일 보내고 나서
			//------------------------------------------------------------------
			case REQUEST_FILE_MODE_AFTER :
			return false;
			default:
				break;
		}
	}

	return false;
}
