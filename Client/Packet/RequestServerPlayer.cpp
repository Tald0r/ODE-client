//--------------------------------------------------------------------------------
// 
// Filename    : RequestServerPlayer.cpp 
// Written By  : sigi
// 
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "RequestServerPlayer.h"
#include "PacketAssert.h"
#include "Packet.h"
#include "PacketDispatcher.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "WireHost.h"
#include "DebugLog.h"
#include <memory>




#define	EXPIRE_DELAY	60000		//60*1000	// 1분


//--------------------------------------------------------------------------------
//
// constructor
//
//--------------------------------------------------------------------------------
RequestServerPlayer::RequestServerPlayer ( Socket * pSocket )
: Player(pSocket), m_PlayerStatus(CPS_NONE)
{
	m_RequestMode = REQUEST_CLIENT_MODE_NULL;

	m_ExpireTime = Wire::CurrentTime() + MonotonicClock::Millis(EXPIRE_DELAY);
}


//--------------------------------------------------------------------------------
//
// destructor
//
//--------------------------------------------------------------------------------
RequestServerPlayer::~RequestServerPlayer () noexcept(false)
{
	__BEGIN_TRY
	

	// 그 어떤 플레이어 객체가 삭제될 때에도, 그 상태는 로그아웃이어야 한다.
	// 즉 어떤 플레이어를 접속 종료 시키려면, 그 상태를 로그아웃으로 만들어야 한다.
	Assert( m_PlayerStatus == CPS_END_SESSION );

	__END_CATCH
}


//--------------------------------------------------------------------------------
//
// parse packet and execute handler for the packet
//
//--------------------------------------------------------------------------------
void RequestServerPlayer::processCommand () 
{
	__BEGIN_TRY

	//-----------------------------------------------------------------------
	// RequestMode에 따른 처리
	//-----------------------------------------------------------------------
	switch (m_RequestMode)
	{
		//-----------------------------------------------------------------
		// Profile을 보내는 중..
		//-----------------------------------------------------------------
		case REQUEST_CLIENT_MODE_PROFILE :
			if (Wire::SendOtherRequest(m_Name, this))
			{
				// the file is being sent, so processCommand() is not needed
				m_ExpireTime = Wire::CurrentTime() + MonotonicClock::Millis(EXPIRE_DELAY);
				return;
			}			
		break;

		default:
		break;
	}	

	std::unique_ptr<Packet> pPacket;

	try {
		try {

			// 헤더를 임시저장할 버퍼 생성
			char header[szPacketHeader];
			PacketID_t packetID;
			PacketSize_t packetSize;
			pPacket.reset();

			//---------------------------------------------------------
			// 이번 Loop에서 처리한 packet의 개수
			//---------------------------------------------------------
			// packet이 너무 많은 경우.. 
			// 다 처리하다보면 .. 시간이 많이 걸려서 client가 안 돌아간다
			// 그래서 일정 개수만 처리한다.
			//---------------------------------------------------------
			int maxProcessPacket = Wire::MaxProcessPacket();
			int processedPacket = 0;

			// 입력버퍼에 들어있는 완전한 패킷들을 모조리 처리한다.
			while ( true ) {
			
				// A partial header is normal transport fragmentation. Leave it in
				// the stream until a later fill completes it.
				if (!m_pInputStream->peek( header , szPacketHeader ))
					break;

				// 패킷아이디 및 패킷크기를 알아낸다.
				// 이때 패킷크기는 헤더를 포함한다.
				memcpy( &packetID   , &header[0] , szPacketID );	
				memcpy( &packetSize , &header[szPacketID] , szPacketSize );

				//#ifdef	__DEBUG_OUTPUT__
					//	DEBUG_ADD_FORMAT("ID=%d (%s), size=%d", packetID, g_pPacketFactoryManager->getPacketName( packetID ), packetSize);
				//#else
					//	DEBUG_ADD_FORMAT("ID=%d, size=%d", packetID, packetSize);
				//#endif
				
				// 패킷 아이디가 이상하면 프로토콜 에러로 간주한다.
				if ( packetID >= Packet::PACKET_MAX )
				{
					DEBUG_ADD_FORMAT_ERR("[PacketError-RequestServerPlayer::processCommand] exceed MAX=%d. packetID=%d", Packet::PACKET_MAX, packetID);
					
					SendBugReport("RSP,Exceed PacketID:%d",packetID);
					
					throw InvalidProtocolException("[PacketError-RequestServerPlayer::processCommand] exceed MAX packetID");
				}

				#ifdef __DEBUG_OUTPUT__
					DEBUG_ADD_FORMAT("[Receive] packet id=%u buffered=%u",
						static_cast<unsigned>(packetID), m_pInputStream->length());
					DEBUG_ADD_FORMAT("[RECEIVE] [ID=%d] %s", packetID, g_pPacketFactoryManager->getPacketName(packetID).c_str());
				#endif
				
				BOOL bExecute = TRUE;

				if ( ! g_pPacketValidator->isValidPacketID( getPlayerStatus() , packetID ) )
				{

					//---------------------------------------------------------------
					// Logout해서 캐릭터 선택화면으로 가는 경우
					//---------------------------------------------------------------
					if (getPlayerStatus()==CPS_WAITING_FOR_GC_RECONNECT_LOGIN)
					{
						// 읽어내고 execute는 하지 않는다.
						bExecute = FALSE;
						
						// 으헤헤.. 무한~~ -_-;
						maxProcessPacket = 0xFFFF;
					}
					//---------------------------------------------------------------
					// 일반적인 경우는 접속을 끊는다.
					//---------------------------------------------------------------
					else
					{
						DEBUG_ADD_FORMAT_ERR("[PacketError] invalid packet ORDER %d", getPlayerStatus());
						throw InvalidProtocolException("invalid packet ORDER");
					}				
				}

				// 패킷 크기가 너무 크면 프로토콜 에러로 간주한다.
				if ( packetSize > g_pPacketFactoryManager->getPacketMaxSize( packetID ) )
				{
					DEBUG_ADD_FORMAT_ERR("[PacketError] too large packet SIZE: %d/%d", (int)packetSize, (int)g_pPacketFactoryManager->getPacketMaxSize( packetID ));
					throw InvalidProtocolException("too large packet SIZE");
				}
				
				// 입력버퍼내에 패킷크기만큼의 데이타가 들어있는지 확인한다.
				// 최적화시 break 를 사용하면 된다. (여기서는 일단 exception을 쓸 것이다.)
				if ( m_pInputStream->length() < szPacketHeader + packetSize )
				{
					DEBUG_ADD_FORMAT("Waiting for complete packet: %d/%d", m_pInputStream->length(), szPacketHeader + packetSize);
		
					throw InsufficientDataException();
				}
				
				//if (g_Mode!=MODE_GAME)	
					//throw InvalidProtocolException("not MODE_GAME");

				// 여기까지 왔다면 입력버퍼에는 완전한 패킷 하나 이상이 들어있다는 뜻이다.
				// 패킷팩토리매니저로부터 패킷아이디를 사용해서 패킷 스트럭처를 생성하면 된다.
				// 패킷아이디가 잘못될 경우는 패킷팩토리매니저에서 처리한다.
				pPacket.reset(g_pPacketFactoryManager->createPacket( packetID ));

				// 이제 이 패킷스트럭처를 초기화한다.
				// 패킷하위클래스에 정의된 read()가 virtual 메커니즘에 의해서 호출되어
				// 자동적으로 초기화된다.
				m_pInputStream->read( pPacket.get() );
				
				// 이제 이 패킷스트럭처를 가지고 패킷핸들러를 수행하면 된다.
				// 패킷아이디가 잘못될 경우는 패킷핸들러매니저에서 처리한다.			

				if (bExecute)
				{
					#ifdef __DEBUG_OUTPUT__
						DEBUG_ADD_FORMAT("[Receive] packet id=%u size=%u",
							static_cast<unsigned>(packetID), static_cast<unsigned>(packetSize));
					#endif

					// Dispatch table (RESTRUCTURING.md tasks 2.1-2.4); an
					// unregistered id throws.
					PacketDispatcher::dispatch( pPacket.get() , this );

					//DEBUG_ADD_FORMAT("[Executed] %s", pPacket->toString().c_str());
					DEBUG_ADD("[PacketExecute OK]");
				}
				
				
				pPacket.reset();

				m_ExpireTime = Wire::CurrentTime() + MonotonicClock::Millis(EXPIRE_DELAY);

				//---------------------------------------------------------	
				// 한번에 처리하는 packet의 한계 개수를 넘어간 경우
				// 나머지는 다음 turn에 처리한다.
				//---------------------------------------------------------
				if (++processedPacket > maxProcessPacket)
				{
					DEBUG_ADD_FORMAT_WAR("[PacketSkip] So many Packets. at RequestServerPlayer");

					break;
				}			
			}

		} catch ( InsufficientDataException & ) {

			// 단지 루프의 탈출 조건일 뿐이다. 상위로 전달할 필요는 없다.
			if (Wire::CurrentTime() > m_ExpireTime)
			{
				throw InvalidProtocolException("timeout - -;;");			
			}

		}

	} catch (Throwable &)	{
		throw;
	}
	__END_CATCH
}
		    

//--------------------------------------------------------------------------------
// disconnect player
//--------------------------------------------------------------------------------
void RequestServerPlayer::disconnect ( bool bDisconnected )
{
	__BEGIN_TRY

	
	// file요청중이던거 있으면 제거한다.
	// The NULL test the host replaces was on the manager itself: with no
	// manager there is nothing registered, which is what HasOtherRequest
	// answers without a host.
	if (Wire::HasOtherRequest(m_Name.c_str()))
	{
		Wire::RemoveOtherRequest(m_Name.c_str());
	}

	if ( bDisconnected == UNDISCONNECTED ) {

		// 클라이언트에게 GCDisconnect 패킷을 전송한다.
		//GCDisconnect gcDisconnect;
		//sendPacket( gcDisconnect );

		// 출력 버퍼에 남아있는 데이타를 전송한다.
		m_pOutputStream->flush();
	}

	// 소켓 연결을 닫는다.
	m_pSocket->close();

	// 플레이어의 상태를 로그아웃으로 만든다.
	//cout<< "PlayerStatus : " << (int)m_PlayerStatus << endl;
	// 2001.10.5 주석처리 ..으흠..
	//Assert( m_PlayerStatus != CPS_END_SESSION );
	m_PlayerStatus = CPS_END_SESSION;

	__END_CATCH
}

uint
RequestServerPlayer::send(const char* pBuffer, uint nBytes) 
{
	__BEGIN_TRY

	#ifdef __DEBUG_OUTPUT__
		DEBUG_ADD_FORMAT("[Send] %d bytes", nBytes);
	#endif

	return m_pOutputStream->write( pBuffer, nBytes );

	__END_CATCH
}

//--------------------------------------------------------------------------------
//
// get debug string
//
//--------------------------------------------------------------------------------
std::string RequestServerPlayer::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	
	msg << "RequestServerPlayer("
		<< "SocketID:" << m_pSocket->getSOCKET() 
		<< ",Host:" << m_pSocket->getHost() 
		<< ")" ;

	return msg.toString();

	__END_CATCH
}
