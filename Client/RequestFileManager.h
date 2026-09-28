//---------------------------------------------------------------------------
// RequestFileManager.h
//---------------------------------------------------------------------------
// The files this client sends to peers over the direct connections: a
// peer that dialled this client and asked for a profile file gets it
// through SendOtherRequest(RequestServerPlayer*), a chunk per turn of
// the receive loop.
//
// REQUESTED_SEND_MAP : the files peers have asked this client for.
//
// The receive half - REQUEST_RECEIVE_MAP, the files this client had
// asked a peer for, fed by the request client player's command loop -
// went with the outbound peer side (docs/RESTRUCTURING.md task 5.2,
// eighth slice).
//---------------------------------------------------------------------------

#ifndef __REQUEST_FILE_MANAGER_H__
#define __REQUEST_FILE_MANAGER_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include <string>
#include <list>
#include <map>
#include "FileChunkReader.h"
#include "Types/RequestTypes.h"
#include "Packet/Exception.h"
class RequestServerPlayer;
class RCRequestedFile;

//---------------------------------------------------------------------------
// REQUEST_FILE_MODE
//---------------------------------------------------------------------------
enum REQUEST_FILE_MODE
{
	REQUEST_FILE_MODE_BEFORE,
	REQUEST_FILE_MODE_SEND,
	REQUEST_FILE_MODE_RECEIVE,
	REQUEST_FILE_MODE_AFTER,
};

//---------------------------------------------------------------------------
// SendFileInfo
//---------------------------------------------------------------------------
// 남이 나에게 요청한 것
//---------------------------------------------------------------------------
class SendFileInfo
{
	private :
		REQUEST_FILE_MODE		m_Mode;

		// 보내기 전에
		std::string				m_Filename;		// 보내주는 file이름		
		REQUEST_FILE_TYPE		m_FileType;		// 어떤 file인가?
		
		// while sending
		Basic::FileChunkReader	m_File;			// m_Filename, opened

	public :
		SendFileInfo(const char* pFilename, REQUEST_FILE_TYPE fileType);
		~SendFileInfo();

		void		StartSend();
		bool		IsSendMode() const	{ return m_Mode==REQUEST_FILE_MODE_SEND; }
		DWORD		Send(char* pBuffer);		// Get()이 더 어울리는데.. - -;
		void		SendBack(DWORD nBack);
		void		EndSend();

		// Get
		REQUEST_FILE_MODE	GetMode() const			{ return m_Mode; }
		REQUEST_FILE_TYPE	GetFileType() const		{ return m_FileType; }
		const std::string&	GetFilename() const		{ return m_Filename; }
		DWORD			GetFileSizeLeft() const	{ return m_File.GetBytesLeft(); }
};

//---------------------------------------------------------------------------
// RequestSendInfo
//---------------------------------------------------------------------------
// 남이 나에게 요청한 것
//---------------------------------------------------------------------------
class RequestSendInfo
{
	private :
		std::string				m_RequestUser;	// 내가 file을 보내줄 사람

		std::list<SendFileInfo*>	m_FileInfos;

	public :
		RequestSendInfo(const char* pRequestUser);
		~RequestSendInfo();
		
		void				AddSendFileInfo(SendFileInfo* pInfo)	{ m_FileInfos.push_back( pInfo ); }

		const std::string&	GetRequestUser() const	{ return m_RequestUser; }

		DWORD				GetSize() const		{ return static_cast<DWORD>(m_FileInfos.size()); }
		SendFileInfo*		GetFront()			{ return (m_FileInfos.empty()? NULL : m_FileInfos.front()); }
		void				DeleteFront()		{ if (!m_FileInfos.empty()) { delete m_FileInfos.front(); m_FileInfos.pop_front(); } }		
		bool				IsEnd()	const		{ return m_FileInfos.empty(); }

		void				MakeRCRequestedFilePacket(RCRequestedFile& packet) const;
};


//---------------------------------------------------------------------------
// RequestFileManager
//---------------------------------------------------------------------------
class RequestFileManager {
	public :
		//-----------------------------------------------------------------
		// 다른 사람이 요청한 것 : < 요청한사람, 다른 사람이 요청한file정보 >
		//-----------------------------------------------------------------
		typedef std::map<std::string, RequestSendInfo*>			REQUEST_SEND_MAP;

	public :
		RequestFileManager();
		~RequestFileManager();

		//--------------------------------------------------------------
		// Release
		//--------------------------------------------------------------
		void			Release();

		//--------------------------------------------------------------
		// The receive half - files this client asked a peer for - went
		// with the outbound peer side (docs/RESTRUCTURING.md task 5.2,
		// eighth slice), and the empty Update() with it.
		//--------------------------------------------------------------
		//--------------------------------------------------------------
		// OtherRequest - 다른 사람이 요청한 file처리
		//--------------------------------------------------------------
		bool			AddOtherRequest(RequestSendInfo* pInfo);
		bool			RemoveOtherRequest(const std::string& name);
		bool			HasOtherRequest(const std::string& name) const;
		bool			SendOtherRequest(const std::string& name, RequestServerPlayer* pRequestServerPlayer);

	protected :
		REQUEST_SEND_MAP		m_OtherRequests;	// 다른 사람이 요청한 file들
};

extern RequestFileManager* g_pRequestFileManager;

#endif


