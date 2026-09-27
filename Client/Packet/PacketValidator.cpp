//----------------------------------------------------------------------
//
// Filename    : PacketValidator.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "PacketValidator.h"

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
PacketValidator::PacketValidator () 
{
	__BEGIN_TRY

	m_PacketIDSets.resize(PLAYER_STATUS_MAX, NULL);

	__END_CATCH
}

//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
PacketValidator::~PacketValidator () 
{

	for ( uint i = 0 ; i < PLAYER_STATUS_MAX ; i ++ ) {
		if ( m_PacketIDSets[i] != NULL ) {
			delete m_PacketIDSets[i];
			m_PacketIDSets[i] = NULL;
		}
	}

	m_PacketIDSets.clear();

}

//----------------------------------------------------------------------
// initialize packet validators
//----------------------------------------------------------------------
void PacketValidator::init ()
{
	__BEGIN_TRY

	PacketIDSet * pPacketIDSet = NULL;

	__BEGIN_DEBUG

		//----------------------------------------------------------------------
		// CPS_NONE
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_NONE , PacketIDSet::PIST_NONE );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// CPS_BEGIN_SESSION ( 로그인 패킷을 보낸 직후 )
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_BEGIN_SESSION , PacketIDSet::PIST_NONE );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// CPS_AFTER_SENDING_CL_LOGIN ( CLLogin 를 보낸 직후 )
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_AFTER_SENDING_CL_LOGIN );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_VERSION_CHECK_OK );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_VERSION_CHECK_ERROR );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_LOGIN_OK );
		//pPacketIDSet->addPacketID( Packet::PACKET_LC_GUEST_LOGIN_OK );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_LOGIN_ERROR );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_WORLD_LIST );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_SERVER_LIST );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_QUERY_RESULT_CHARACTER_NAME );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// CPS_AFTER_SENDING_CL_QUERY_PLAYER_ID ( CLQueryPlayerID 를 보낸 직후 )
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_AFTER_SENDING_CL_QUERY_PLAYER_ID );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_QUERY_RESULT_PLAYER_ID );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// CPS_AFTER_SENDING_CL_REGISTER_PLAYER ( CLRegisterPlayer 를 보낸 직후 )
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_AFTER_SENDING_CL_REGISTER_PLAYER );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_VERSION_CHECK_OK );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_VERSION_CHECK_ERROR );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_REGISTER_PLAYER_OK );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_REGISTER_PLAYER_ERROR );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// CPS_AFTER_SENDING_CL_GET_PC_LIST ( CLGetPCList 를 보낸 직후 )
		// CLChangeServer를 보낸 직후에도 이 상태를 유지하도록 한다.
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_AFTER_SENDING_CL_GET_PC_LIST );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_PC_LIST );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_SERVER_LIST );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_QUERY_RESULT_CHARACTER_NAME );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// CPS_AFTER_SENDING_CL_CREATE_PC ( CLCreatePC 를 보낸 직후 )
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_AFTER_SENDING_CL_CREATE_PC );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_CREATE_PC_OK );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_CREATE_PC_ERROR );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_QUERY_RESULT_CHARACTER_NAME );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// CPS_AFTER_SENDING_CL_DELETE_PC ( CLDeletePC 를 보낸 직후 )
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_AFTER_SENDING_CL_DELETE_PC );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_DELETE_PC_OK );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_DELETE_PC_ERROR );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_QUERY_RESULT_CHARACTER_NAME );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// CPS_AFTER_SENDING_CL_SELECT_PC ( CLSelectPC 를 보낸 직후 )
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_AFTER_SENDING_CL_SELECT_PC );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_RECONNECT );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_SELECT_PC_ERROR );
		pPacketIDSet->addPacketID( Packet::PACKET_LC_QUERY_RESULT_CHARACTER_NAME );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// CPS_AFTER_SENDING_CG_CONNECT
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_AFTER_SENDING_CG_CONNECT );
		pPacketIDSet->addPacketID( Packet::PACKET_GC_UPDATE_INFO );
		pPacketIDSet->addPacketID( Packet::PACKET_GC_DISCONNECT );
		pPacketIDSet->addPacketID( Packet::PACKET_GC_SYSTEM_AVAILABILITIES );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// CPS_WAITING_FOR_LOADING
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_WAITING_FOR_LOADING , PacketIDSet::PIST_NONE );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// CPS_WAITING_FOR_GC_SET_POSITION
		//----------------------------------------------------------------------
		// 2002.2.2 , PacketIDSet::PIST_IGNORE_EXCEPT 추가
		pPacketIDSet = new PacketIDSet( CPS_WAITING_FOR_GC_SET_POSITION, PacketIDSet::PIST_IGNORE_EXCEPT );		
		pPacketIDSet->addPacketID( Packet::PACKET_GC_SET_POSITION );
		pPacketIDSet->addPacketID( Packet::PACKET_GC_PARTY_INVITE );
		pPacketIDSet->addPacketID( Packet::PACKET_GC_PARTY_LEAVE );
		pPacketIDSet->addPacketID( Packet::PACKET_GC_PARTY_JOINED );
		pPacketIDSet->addPacketID( Packet::PACKET_GC_PARTY_ERROR );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// CPS_WAITING_FOR_GC_RECONNECT_LOGIN
		//----------------------------------------------------------------------
		// 2002.2.2 , PacketIDSet::PIST_IGNORE_EXCEPT 추가
		pPacketIDSet = new PacketIDSet( CPS_WAITING_FOR_GC_RECONNECT_LOGIN, PacketIDSet::PIST_IGNORE_EXCEPT );
		pPacketIDSet->addPacketID( Packet::PACKET_GC_RECONNECT_LOGIN );
		pPacketIDSet->addPacketID( Packet::PACKET_GC_PARTY_INVITE );
		pPacketIDSet->addPacketID( Packet::PACKET_GC_PARTY_LEAVE );
		pPacketIDSet->addPacketID( Packet::PACKET_GC_PARTY_JOINED );
		pPacketIDSet->addPacketID( Packet::PACKET_GC_PARTY_ERROR );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );


		//----------------------------------------------------------------------
		// CPS_NORMAL (게임에 들어가자!)
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_NORMAL , PacketIDSet::PIST_ANY );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// CPS_END_SESSION
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_END_SESSION , PacketIDSet::PIST_NONE );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );


		//----------------------------------------------------------------------
		//						Request Server/Client
		//----------------------------------------------------------------------
		// 접속 하기 전
		//----------------------------------------------------------------------		,
		pPacketIDSet = new PacketIDSet( CPS_REQUEST_SERVER_BEGIN_SESSION);
		pPacketIDSet->addPacketID( Packet::PACKET_CR_CONNECT );
		pPacketIDSet->addPacketID( Packet::PACKET_CR_REQUEST );
		pPacketIDSet->addPacketID( Packet::PACKET_CR_WHISPER );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );		

		//----------------------------------------------------------------------
		// 접속 하기 전
		//----------------------------------------------------------------------		,
		pPacketIDSet = new PacketIDSet( CPS_REQUEST_CLIENT_BEGIN_SESSION , PacketIDSet::PIST_NONE );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// RequestServer에 접속 요청 후
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_REQUEST_CLIENT_AFTER_SENDING_CONNECT );
		pPacketIDSet->addPacketID( Packet::PACKET_RC_CONNECT_VERIFY );
		pPacketIDSet->addPacketID( Packet::PACKET_RC_REQUEST_VERIFY );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );
		
		//----------------------------------------------------------------------
		// RequestServer정상 상태
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_REQUEST_SERVER_NORMAL );
		pPacketIDSet->addPacketID( Packet::PACKET_CR_REQUEST );
		pPacketIDSet->addPacketID( Packet::PACKET_CR_DISCONNECT );
		pPacketIDSet->addPacketID( Packet::PACKET_CR_WHISPER );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );
		
		//----------------------------------------------------------------------
		// RequestClient정상 상태
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_REQUEST_CLIENT_NORMAL );
		pPacketIDSet->addPacketID( Packet::PACKET_RC_REQUEST_VERIFY );
		pPacketIDSet->addPacketID( Packet::PACKET_RC_REQUESTED_FILE );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

		//----------------------------------------------------------------------
		// Client Communication 상태
		//----------------------------------------------------------------------
		pPacketIDSet = new PacketIDSet( CPS_CLIENT_COMMUNICATION_NORMAL );
		pPacketIDSet->addPacketID( Packet::PACKET_RC_POSITION_INFO );
		pPacketIDSet->addPacketID( Packet::PACKET_RC_SAY );
		pPacketIDSet->addPacketID( Packet::PACKET_RC_STATUS_HP );
		pPacketIDSet->addPacketID( Packet::PACKET_RC_CHARACTER_INFO );
		addPacketIDSet( pPacketIDSet->getPlayerStatus(), pPacketIDSet );

	__END_DEBUG

	__END_CATCH
}

//----------------------------------------------------------------------
// add packet id set
//----------------------------------------------------------------------
void PacketValidator::addPacketIDSet ( PlayerStatus playerStatus , PacketIDSet * pPacketIDSet ) 
{
	__BEGIN_TRY

	if ( playerStatus >= PLAYER_STATUS_MAX )
		throw Error("invalid player status");

	if ( m_PacketIDSets[playerStatus] != NULL )
		throw DuplicatedException();

	m_PacketIDSets[playerStatus] = pPacketIDSet;

	__END_CATCH
}

//----------------------------------------------------------------------
// delete packet id set
//----------------------------------------------------------------------
void PacketValidator::deletePacketIDSet ( PlayerStatus playerStatus , PacketIDSet * pPacketIDSet ) 
{
	__BEGIN_TRY
	(void)pPacketIDSet;

	if ( playerStatus >= PLAYER_STATUS_MAX )
		throw Error("invalid player status");

	if ( m_PacketIDSets[playerStatus] == NULL )
		throw NoSuchElementException("invalid player status");

	m_PacketIDSets[playerStatus] = NULL;

	__END_CATCH
}

//----------------------------------------------------------------------
// is valid packet?
//----------------------------------------------------------------------
bool PacketValidator::isValidPacketID ( PlayerStatus playerStatus , PacketID_t packetID ) 
{
	__BEGIN_TRY

	if ( playerStatus >= PLAYER_STATUS_MAX )
		throw Error("invalid player status");

	if ( m_PacketIDSets[playerStatus] == NULL )
		throw NoSuchElementException("invalid player status");

	return m_PacketIDSets[playerStatus]->hasPacketID(packetID);

	__END_CATCH
}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
#ifdef __DEBUG_OUTPUT__
	std::string PacketValidator::toString () const
	{
		StringStream msg;

		msg << "PacketValidator(\n";

		for ( uint i = 0 ; i < PLAYER_STATUS_MAX ; i ++ )
		{
			msg << i << " : ";
			msg << ( ( m_PacketIDSets[i] != NULL ) ? m_PacketIDSets[i]->toString() : "NULL" );
		}

		msg << ")";

		return msg.toString();
	}
#endif

//----------------------------------------------------------------------
// global variable definition
//----------------------------------------------------------------------
PacketValidator * g_pPacketValidator = NULL;
