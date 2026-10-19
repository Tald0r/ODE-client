//-----------------------------------------------------------------------------
// UIMessageManager.h
//-----------------------------------------------------------------------------

#ifndef __UIMESSAGEMANAGER_H__
#define __UIMESSAGEMANAGER_H__

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include "VS_UI_UIMessage.h"
#include "ClientMode.h"
#include "MTypeDef.h"

//-----------------------------------------------------------------------------
// What the UI's messages need from the program around them, installed by
// GameInit (task 4.258). A missing host or entry skips the call: InitSocket
// answers FALSE and the view selects nothing (OBJECTID_NULL). The weather,
// the zone's music and sending what is queued go through WorldEvents' and
// LoginPresentation's hosts.
//-----------------------------------------------------------------------------
namespace UIMessages {

struct Host
{
	// Makes the connection to the login server anew; FALSE when it fails.
	BOOL (*InitSocket)() = nullptr;
	// Loads the add-on sprites, part of them or (bLoading) all.
	bool (*LoadingAddonSPK)(bool bLoading) = nullptr;
	// Releases the game's objects (the zone, the weather, the party...) on
	// a logout, and stops the loading thread before it.
	void (*ReleaseGameObject)() = nullptr;
	void (*StopLoadingThread)() = nullptr;
	// Remembers the character slot chosen, for the next character screen.
	void (*SaveLastSelectedCharacter)(int slot) = nullptr;
	// Enters the mode now, with its side effects.
	void (*SetMode)(CLIENT_MODE mode) = nullptr;
	// The creature and the item the view has selected.
	TYPE_OBJECTID (*GetSelectedCreature)() = nullptr;
	TYPE_OBJECTID (*GetSelectedItemID)() = nullptr;
};

// Borrowed until replaced; answers the host it replaces.
const Host* SetHost(const Host* host);

}

class UIMessageManager {
	public :
		// intptr_t, not int: several senders put a pointer in left/right and int
		// truncates it on 64-bit Windows. See the comment on struct MESSAGE.
		typedef	void (*UI_MESSAGE_FUNCTION)(intptr_t, intptr_t, void *);

	public :
		UIMessageManager();
		~UIMessageManager();

		//-----------------------------------------------------------
		// 초기화 - 필수~~
		//-----------------------------------------------------------
		void			Init();

		//-----------------------------------------------------------
		// UI Message 처리
		//-----------------------------------------------------------
		void			Execute(DWORD message, intptr_t left, intptr_t right, void* void_ptr);

	protected :
		//-----------------------------------------------------------
		// 실제로 message처리하는 함수들
		//-----------------------------------------------------------
		static void	Execute_UI_NEW_CHARACTER(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_RUN_NEWUSER_REGISTRATION(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CHECK_EXIST_ID(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_INFO_CLOSE(intptr_t left, intptr_t right, void* void_ptr);		
		static void	Execute_UI_DELETE_CHARACTER(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_TERMINATION(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_NEW_USER_REGISTRATION(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_LOGIN(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CHARACTER_MANAGER_FINISHED(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CONNECT(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CHAT_RETURN(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_CHAT_SELECT_NAME(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_LOGOUT(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_SELECT_SKILL(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CANCEL_SELECT_SKILL(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_DROP_TO_CLIENT(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_DROP_TO_INVENTORY(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_DROP_TO_QUICKSLOT(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_DROP_TO_GEAR(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_PICKUP_FROM_QUICKSLOT(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_PICKUP_FROM_INVENTORY(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_PICKUP_FROM_GEAR(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_USE(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_USE_QUICKSLOT(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_INSERT_FROM_INVENTORY(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_INSERT_FROM_GEAR(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_INSERT_FROM_QUICKSLOT(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CLOSE_SHOP(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_BUY_ITEM(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_SELL_FINISHED(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_SELL_ITEM(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_SELL_ALL_ITEM(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_REMOVE_BACKGROUND_MOUSE_FOCUS(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_BACKGROUND_MOUSE_FOCUS(intptr_t left, intptr_t right, void* void_ptr);
//		static void	Execute_UI_CLOSE_SKILL_VIEW(intptr_t left, intptr_t right, void* void_ptr);
//		static void	Execute_UI_LEARN_SLAYER_SKILL(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_PDS_CLOSED(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_PLEASE_SET_SLAYER_VALUE(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_SEND_PCS_NUMBER(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_END_PCS(int left, int right, void* void_ptr) ;
		//static void	Execute_UI_QUIT_PCS_ONLINE_MODE(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_CHANGE_PCS_CONNECTED_SLOT(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_PLEASE_PCS_CONNECT_ME(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_GAMEMENU_CONTINUE(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_MINIMAP_TOGGLE(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_DROP_MONEY(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CLICK_BONUS_POINT(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_INFO(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_REPAIR_ITEM(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_SILVERING_ITEM(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_REPAIR_FINISHED(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_SILVERING_FINISHED(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_FINISH_LEVELUP_BUTTON(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_RUNNING_GAMEMENU(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_STORAGE_BUY(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_SELECT_STORAGE_SLOT(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_DEPOSIT_MONEY(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_WITHDRAW_MONEY(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CLOSE_STORAGE(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_EXCHANGE_REQUEST_CANCEL(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_EXCHANGE_ACCEPT(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_EXCHANGE_MONEY(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_SELECT_EXCHANGE(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_ITEM_PICKUP_FROM_TRADEGRID(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_ITEM_INSERT_FROM_TRADEGRID(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_ITEM_DROP_TO_TRADEGRID(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_OK_EXCHANGE(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CANCEL_EXCHANGE(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CLOSE_EXCHANGE(intptr_t left, intptr_t right, void* void_ptr);
//		static void	Execute_UI_CHANGE_GAME_OPTION(intptr_t left, intptr_t right, void* void_ptr);
//		static void	Execute_UI_CLOSE_GAME_OPTION(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CHANGE_OPTION(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CLOSE_OPTION(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CLOSE_BOOKCASE(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CLOSE_BRIEFING(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CLOSE_COMPUTER(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CLOSE_TUTORIAL_EXIT(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_SELECT_EXPLOSIVE(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_CLOSE_SELECT_EXPLOSIVE(intptr_t left, intptr_t right, void* void_ptr);

		static void	Execute_UI_CLOSE_DESC_DIALOG(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CLOSE_ELEVATOR(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_SELECT_ELEVATOR(intptr_t left, intptr_t right, void* void_ptr);

		//static void	Execute_UI_SELECT_SERVER(intptr_t left, intptr_t right, void* void_ptr);
		//static void	Execute_UI_REQUEST_SERVER_LIST(intptr_t left, intptr_t right, void* void_ptr);

		static void	Execute_UI_ITEM_TO_QUICKITEMSLOT(intptr_t left, intptr_t right, void* void_ptr);
		
		static void	Execute_UI_CLOSE_SLAYER_PORTAL(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_SLAYER_PORTAL(intptr_t left, intptr_t right, void* void_ptr);

		static void	Execute_UI_FINISH_REQUEST_PARTY_BUTTON(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_PARTY_REQUEST_CANCEL(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_PARTY_ACCEPT(intptr_t left, intptr_t right, void* void_ptr);	
		static void	Execute_UI_CLOSE_PARTY_MANAGER(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_AWAY_PARTY(intptr_t left, intptr_t right, void* void_ptr);

		static void	Execute_UI_FINISH_REQUEST_DIE_BUTTON(intptr_t left, intptr_t right, void* void_ptr);

		static void	Execute_UI_CONNECT_SERVER(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CLOSE_SERVER_SELECT(intptr_t left, intptr_t right, void* void_ptr);

		static void	Execute_UI_NEWCHARACTER_CHECK(intptr_t left, intptr_t right, void* void_ptr);

		static void Execute_UI_CLOSE_TEAM_LIST(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_TEAM_INFO(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_TEAM_MEMBER_INFO(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_TEAM_REGIST(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_TEAM_MEMBER_LIST(intptr_t left, intptr_t right, void *void_ptr);
	
		static void Execute_UI_REQUEST_GUILD_INFO(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_REQUEST_GUILD_MEMBER_LIST(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_MODIFY_GUILD_MEMBER(intptr_t left, intptr_t right, void *void_ptr);

		static void Execute_UI_SELECT_TEAM_MEMBER_LIST(intptr_t left, intptr_t right, void *void_ptr);		// void_ptr = MEMBER_NAME
		static void Execute_UI_SELECT_READY_TEAM_LIST(intptr_t left, intptr_t right, void *void_ptr);		// void_ptr = TEAM_NAME
		static void Execute_UI_SELECT_REGIST_TEAM_LIST(intptr_t left, intptr_t right, void *void_ptr);		// void_ptr = TEAM_NAME

		//add by viva
		static void Execute_UI_CLOSE_FRIEND_CHATTING_INFO(intptr_t left, intptr_t right, void* void_ptr);
		static void Execute_UI_OPEN_FRIEND_CHATTING_INFO(intptr_t left, intptr_t right, void* void_ptr);
		////friend message
		static void Execute_UI_FRIEND_CHATTING_SEND_MESSAGE(intptr_t left, intptr_t right, void* void_ptr);
		static void Execute_UI_FRIEND_CHATTING_UPDATE(intptr_t left, intptr_t right, void* void_ptr);
		static void Execute_UI_FRIEND_CHATTING_ADD_FRIEND(intptr_t left, intptr_t right, void* void_ptr);
		/////ask_friend_request
		static void Execute_UI_FRIEND_REQUEST_ACCEPT(intptr_t left, intptr_t right, void* void_ptr);
		//////ask_friend_ask_close
		static void	Execute_UI_FRIEND_ASK_CLOSE(intptr_t left, intptr_t right, void* void_ptr);
		///////ask_friend_delete
		static void Execute_UI_FRIEND_DELETE_ASK(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_FRIEND_DELETE_ACCEPT(intptr_t left, intptr_t right, void* void_ptr);

		//end

		static void Execute_UI_JOIN_READY_TEAM(intptr_t left, intptr_t right, void *void_ptr);				// void_ptr = TEAM_NAME
		static void Execute_UI_JOIN_REGIST_TEAM(intptr_t left, intptr_t right, void *void_ptr);			// void_ptr = TEAM_NAME

		static void Execute_UI_REGIST_GUILD_MEMBER(intptr_t left, intptr_t right, void *void_ptr);			// void_ptr = introduction max:150byte 창 닫아줄것!
		static void Execute_UI_REGIST_GUILD_TEAM(intptr_t left, intptr_t right, void *void_ptr);			// left = TEAM_NAME, void_ptr = introduction max:150byte 창 닫아줄것!

		static void Execute_UI_CLOSE_FILE_DIALOG(intptr_t left, intptr_t right, void *void_ptr);

		static void	Execute_UI_ENCHANT_ACCEPT(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ENCHANT_CANCEL(intptr_t left, intptr_t right, void* void_ptr);

		static void Execute_UI_MESSAGE_BOX(intptr_t left, intptr_t right, void* void_ptr);
		static void Execute_UI_CLOSE_OTHER_INFO(intptr_t left, intptr_t right, void* void_ptr);
		
		static void Execute_UI_MODIFY_TEAM_INFO(intptr_t left, intptr_t right, void* void_ptr);
		static void Execute_UI_MODIFY_GUILD_MEMBER_INTRO(intptr_t left, intptr_t right, void* void_ptr);

		static void Execute_UI_SEND_NAME_FOR_SOUL_CHAIN(intptr_t left, intptr_t right, void* void_ptr);
		static void Execute_UI_CLOSE_TRACE_WINDOW(intptr_t left, intptr_t right, void* void_ptr);

	// 넷마블용 수정
		static void Execute_UI_RUN_CONNECT(intptr_t left, intptr_t right, void* void_ptr);
		static void Excute_UI_SELECT_GRADE_SKILL(intptr_t left, intptr_t right, void* void_ptr);

		static void Excute_UI_USE_XMAS_TREE(intptr_t left, intptr_t right, void* void_ptr);				// left = to, right = from, void_ptr = message
		static void Excute_UI_CLOSE_XMAS_CARD_WINDOW(intptr_t left, intptr_t right, void* void_ptr);


		// 2003.1.21
		static void	Excute_UI_SEND_BRING_FEE(intptr_t left,intptr_t right, void * void_ptr);
		static void Excute_UI_CLOSE_BRING_FEE_WINDOW(intptr_t left,intptr_t right, void * void_ptr);

		// 2003.1.24
		static void Excute_UI_CLOSE_WAR_LIST(intptr_t left, intptr_t right, void *void_ptr);

		static void	Execute_UI_CLOSE_BLOOD_BIBLE_STATUS(intptr_t left, intptr_t right, void *void_ptr);
		static void	Execute_UI_SEND_NAME_FOR_COUPLE(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_INPUT_NAME_WINDOW(intptr_t left, intptr_t right, void *void_ptr);

		static void Execute_UI_ITEM_USE_GEAR(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_GO_BILING_PAGE(intptr_t left, intptr_t right, void *void_ptr);
		
		static void Execute_UI_CLOSE_POPUP_MESSAGE(intptr_t left, intptr_t right, void *void_ptr);
		static void	Execute_UI_CLOSE_QUEST_STATUS( intptr_t left, intptr_t right, void *void_ptr);

		static void Execute_UI_CLOSE_LOTTERY_CARD(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_LOTTERY_CARD_STATUS(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_FINISH_SCRATCH_LOTTERY(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_IMAGE_NOTICE(intptr_t left,intptr_t right, void *void_ptr);
		static void Execute_UI_SELECT_ITEM_FROM_SHOP(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_ITEM_LIST(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_BULLETIN_BOARD(intptr_t left, intptr_t right, void *void_ptr);
		static void	Execute_UI_TRANS_ITEM_CANCEL(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_TRANS_ITEM_ACCEPT(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_REQUEST_RESURRECT(intptr_t left, intptr_t right, void *void_ptr);

		static void Execute_UI_CLOSE_MIXING_FORGE(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_MIXING_FORGE(intptr_t left, intptr_t right, void* void_ptr);
		
		static void	Execute_UI_CLOSE_REMOVE_OPTION(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_SEND_REMOVE_OPTION(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_OUSTERS_SKILL_INFO(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_LEARN_OUSTERS_SKILL(intptr_t left, intptr_t right, void *void_ptr);

		static void Execute_UI_RUN_LEVELUP(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_HORN(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_OUSTERS_DOWN_SKILL(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLEAR_ALL_STAGE(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_FINDING_MINE(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_NEMONEMO(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_PUSHPUSH(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_CRAZY_MINE(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLOSE_ARROW_TILE(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_CLEAR_STAGE(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_FORCE_DIE(intptr_t left, intptr_t right, void *void_ptr);
		static void Execute_UI_ADD_ITEM_TO_CODE_SHEET(intptr_t left, intptr_t right, void *void_ptr);
		
		static void Execute_UI_SEND_BUG_REPORT(intptr_t left,intptr_t right, void *void_ptr);
		static void Execute_UI_GO_BEGINNER_ZONE(intptr_t left, intptr_t right, void* void_ptr);
		static void Execute_UI_POPUP_MESSAGE_OK(intptr_t left, intptr_t right, void* void_ptr);
		static void Execute_UI_CLOSE_SHRINE_MINIMAP(intptr_t left, intptr_t right, void* void_ptr);
		static void Execute_UI_WARP_TO_REGEN_TOWER(intptr_t left, intptr_t right, void* void_ptr);
		static void Execute_UI_CLOSE_MAILBOX(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_CLOSE_PET_INFO(intptr_t left, intptr_t right, void* void_ptr);		
		static void	Execute_UI_PET_GAMBLE(intptr_t left, intptr_t right, void* void_ptr);		
		static void	Execute_UI_CLOSE_USE_PET_FOOD(intptr_t left, intptr_t right, void* void_ptr);	
		
		static void	 Execute_UI_CLOSE_PETSTORAGE(intptr_t left, intptr_t right, void* void_ptr);	
		static void	 Execute_UI_CLOSE_KEEP_PETITEM(intptr_t left, intptr_t right, void* void_ptr);	
		static void	 Execute_UI_CLOSE_GET_KEEP_PETITEM(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_KEEP_PETITEM(intptr_t left, intptr_t right, void* void_ptr);	
		static void	 Execute_UI_GET_KEEP_PETITEM(intptr_t left, intptr_t right, void* void_ptr);

		static void	 Execute_UI_CLOSE_SMS_MESSAGE(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_SEND_SMS_MESSAGE(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_SMS_OPEN_LIST(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_SMS_RECORD(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_SMS_DELETE(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_CLOSE_SMS_LIST(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_CLOSE_SMS_RECORD(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_SMS_ADD_SEND_LIST(intptr_t left, intptr_t right, void* void_ptr);

		static void	 Execute_UI_CLOSE_NAMING(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_CHANGE_CUSTOM_NAMING(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_SELECT_NAMING(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_CLOSE_NAMING_CHANGE(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_RUN_NAMING_CHANGE(intptr_t left, intptr_t right, void* void_ptr);

		static void	 Execute_UI_CLOSE_QUEST_MANAGER(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_CLOSE_QUEST_LIST(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_CLOSE_QUEST_DETAIL(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_CLOSE_QUEST_MISSION(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_CLOSE_QUEST_ITEM(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_CLOSE_QUEST_ICON(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_GQUEST_ACCEPT(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_GQUEST_GIVEUP(intptr_t left, intptr_t right, void* void_ptr);
		
		static void	 Execute_UI_ITEM_USE_GQUEST_ITEM(intptr_t left, intptr_t right, void* void_ptr);

		static void	Execute_UI_ITEM_USE_REQUEST_GUILD_LIST(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_USE_UNION_INFO(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_USE_REQUEST_UNION(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_USE_QUIT(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_USE_EXPER(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_USE_UNION_ACCEPT(intptr_t left, intptr_t right, void* void_ptr);	
		static void	Execute_UI_ITEM_USE_UNION_DENY(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_USE_UNION_QUIT_ACCEPT(intptr_t left, intptr_t right, void* void_ptr);		
		static void	Execute_UI_ITEM_USE_UNION_QUIT_DENY(intptr_t left, intptr_t right, void* void_ptr);

		static void	 Execute_UI_RECALL_BY_NAME(intptr_t left, intptr_t right, void* void_ptr);

		static void	 Execute_UI_UI_MODIFY_TAX(intptr_t left, intptr_t right, void* void_ptr);

		static void	 Execute_UI_APPOINT_SUBMASTER(intptr_t left, intptr_t right, void* void_ptr);

		static void	Execute_UI_DISPLAY_ITEM(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_UNDISPLAY_ITEM(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_STORE_SIGN(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_STORE_OPEN(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_STORE_CLOSE(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_REQUEST_STORE_INFO(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_MY_STORE_INFO(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_OTHER_STORE_INFO(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_BUY_STORE_ITEM(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_REMOVE_STORE_ITEM(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ADD_STORE_ITEM(intptr_t left, intptr_t right, void* void_ptr);		
		static void Execute_UI_CLOSE_PERSNALSHOP(intptr_t left, intptr_t right, void* void_ptr);		


		static void	 Execute_UI_CLOSE_POWER_JJANG(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_REQUEST_POWER_JJANG_POINT(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_POWER_JJANG_GAMBLE(intptr_t left, intptr_t right, void* void_ptr);

		static void	 Execute_UI_CLOSE_SWAPADVANCEMENTITEM(intptr_t left, intptr_t right, void* void_ptr);
		static void	 Execute_UI_SWAPADVANCEMENTITEM(intptr_t left, intptr_t right, void* void_ptr);
		//2005.1.5
		static void	 Execute_UI_LEARN_ADVANCE_SKILL(intptr_t left, intptr_t right, void* void_ptr);	
		
		static void	 Execute_UI_CAMPAIGN_HELP(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_RUN_NEXT_GQUEST_EXCUTE_ELEMENT(intptr_t left, intptr_t right, void* void_ptr);

		static void	Execute_UI_GQUEST_SET_ACTION(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_GQUEST_ENDING_EVENT(intptr_t left, intptr_t right, void* void_ptr);

		static void	Execute_UI_REQUEST_EVENT_ITEM(intptr_t left, intptr_t right, void* void_ptr);

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 藤속관櫓관

		static void	Execute_UI_CLOSE_INVENTORY_SUB(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_DROP_TO_INVENTORY_SUB(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_PICKUP_FROM_INVENTORY_SUB(intptr_t left, intptr_t right, void* void_ptr);
		static void	Execute_UI_ITEM_USE_SUBINVENTORY(intptr_t left, intptr_t right, void* void_ptr);
	#endif
		

		
		

		
	protected :
		UI_MESSAGE_FUNCTION		m_UIMessageFunction[MAX_UI_MESSAGE];
	
};

extern UIMessageManager*	g_pUIMessageManager;

#endif

