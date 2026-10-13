//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSayHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCFriendChatting.h"
#include "CommunicationPresentationHost.h"


//////////////////////////////////////////////////////////////////////
//
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
//
//////////////////////////////////////////////////////////////////////

void GCFriendChattingHandler::execute ( GCFriendChatting * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;

	switch(pPacket->getCommand())
	{
	case GC_UPDATE:
		{
			CommunicationPresentation::FriendChatting_UpdateFriendList(pPacket);
			break;
		}
	case GC_MESSAGE:
		{
			CommunicationPresentation::FriendChatting_Message(pPacket);
			break;
		}
	case GC_ADD_FRIEND_REQUEST:
		{
			CommunicationPresentation::RunFriendRequest(pPacket);
			break;
		}
	case GC_ADD_FRIEND_REFUSE:
		{
			CommunicationPresentation::RunFriendRefuse(pPacket);
			break;
		}
	case GC_ADD_FRIEND_WAIT:
		{
			CommunicationPresentation::RunFriendWait(pPacket);
			break;
		}
	case GC_ADD_FRIEND_OK:
		{
			CommunicationPresentation::RunFriendOK(pPacket);
			break;
		}
	case GC_ADD_FRIEND_EXIST:
		{
			CommunicationPresentation::RunFriendExist(pPacket);
			break;
		}
	case GC_ADD_FRIEND_BLACK:
		{
			CommunicationPresentation::RunFriendBlack(pPacket);
			break;
		}
	case GC_FRIEND_DELETE:
		{
			CommunicationPresentation::RunFriendDelete(pPacket);
			break;
		}
	default:
		break;
	}

	__END_CATCH
}
