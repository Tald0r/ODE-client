
/*----------------------------------------------------------

		UI STRING added - 2002.08-31				by sonee

	These are the descriptions shown in the game.
	If you need a string, search here first and reuse the
	existing entry; if there is none, add it here and refer
	to it, which keeps translating to another language easy.

	(searching in the CPP file is usually easier)

  ----------------------------------------------------------*/

//----------------------------------------------------------------------
// MGameStringTable.h
//----------------------------------------------------------------------

#ifndef __MGAMESTRINGTABLE_H__
#define	__MGAMESTRINGTABLE_H__

#include "MStringArray.h"

extern MStringArray*		g_pGameStringTable;
//2004, 6, 18, sobeit add start - about nick name string table
extern MStringArray*		g_pNickNameStringTable;
//2004, 6, 18, sobeit add end - about nick name string table
extern void				InitGameStringTable();

//----------------------------------------------------------------------
// True when the built-in English text should be used instead of the
// localised text the game data supplies. It gates the string table above
// and the NPC script table, which have the same problem: the shipped data
// is Korean and the translation lives in the client.
//
// The decision is read from the language file (Data/Info/Language.inf);
// the Properties overload looks its path up under FILE_LANGUAGE_INFO,
// which is how the executable calls it with g_pFileDef. A NULL table, a
// missing property or an unreadable file all mean English (the build's
// default), so the client never starts without UI text.
//----------------------------------------------------------------------
class Properties;
extern bool				UseEnglishText(const Properties* pFileDef);
extern bool				UseEnglishTextFrom(const char* szLanguageInfoFile);

//----------------------------------------------------------------------
// Replaces every entry of the game string table that holds a printf
// conversion specification too dangerous to hand to sprintf.
//
// Data/Info/String.inf supplies the entries and roughly 600 call sites pass
// them as the FORMAT argument of sprintf/wsprintf. Removed are the entries
// that could write through an argument ("%n"), take a field width from an
// argument ("%*d"), demand large argument-driven expansion (a wide field
// width or precision, whether in one specification or summed over the
// entry), or retype an argument - the wide conversions ("%S", "%ls"), which
// make the CRT read a char* as a wchar_t*, and the floating point ones
// ("%f"), which are unbounded without a width and on x64 read a register
// the call site never wrote. Call this once, right after the file has been
// read and before anything can format with it.
//
// It does NOT verify that an entry's specifier count matches the call site
// that formats it: the table does not know which call site takes which
// entry, so arity cannot be checked at load time. An entry with a "%s" the
// call site supplies no argument for therefore survives this pass and still
// makes sprintf read a stack word as a char*. Closing that is a per-call-
// site change - the call sites that pass no varargs at all are being
// converted to bounded "%s" copies separately.
//----------------------------------------------------------------------
extern void				SanitizeGameStringTable();

//----------------------------------------------------------------------
// Reads one entry from the game string table, or "" when the table is too
// short to hold it or the entry is empty (an empty MString holds NULL), so
// the answer is always text.
//
// String.inf decides how many entries the table has, so a data file built
// for an older client can be shorter than GAME_STRINGID. CTypeTable range
// checks only under _DEBUG - which MSVC defines for Debug builds but not
// for Release - so in Release an out-of-range id indexes past the array and
// ids added after the shipped String.inf must be read through here.
//----------------------------------------------------------------------
extern const char*		GetGameString(int stringID);

//----------------------------------------------------------------------
//----------------------------------------------------------------------
enum GAME_STRINGID
{
	STRING_NETWORK_CONDITION_BAD,
	STRING_RESURRECTION_AFTER_SECONDS,
	STRING_DRAW_ZONE_NAME,
	STRING_DRAW_GAME_DATE,
	STRING_DRAW_GAME_TIME,
	STRING_DRAW_ITEM_NAME_MONEY,

	STRING_USER_REGISTER_DENY,
	STRING_USER_REGISTER_HOMEPAGE,
	STRING_USER_REGISTER_EMPTY_FIELD,
	STRING_USER_REGISTER_INVALID_ID,
	STRING_USER_REGISTER_ID_LENGTH,
	STRING_USER_REGISTER_ID_SPECIAL,
	STRING_USER_REGISTER_PASSWORD_LENGTH,
	STRING_USER_REGISTER_PASSWORD_SPECIAL,
	STRING_USER_REGISTER_PASSWORD_NUMBER,
	STRING_USER_REGISTER_NAME_LENGTH,
	STRING_USER_REGISTER_INVALID_SSN,
	STRING_USER_REGISTER_SSN_FORMAT,

	STRING_MESSAGE_CANNOT_BUY_NO_SPACE,
	STRING_MESSAGE_CANNOT_BUY_NO_MONEY,
	STRING_MESSAGE_CANNOT_BUY_NO_ITEM,
	STRING_MESSAGE_CANNOT_SELL,
	STRING_MESSAGE_CANNOT_REPAIR,
	STRING_MESSAGE_CANNOT_SILVERING,
	STRING_MESSAGE_CANNOT_BUY_MORE,
	STRING_MESSAGE_CANNOT_STORAGE,
	STRING_MESSAGE_STORAGE_BUY,
	STRING_MESSAGE_NO_STORAGE,
	STRING_MESSAGE_TRADE_REJECTED,
	STRING_MESSAGE_TRADE_NOBODY,		
	STRING_MESSAGE_TRADE_CANNOT_ON_MOTORCYCLE,
	STRING_MESSAGE_TRADE_SAFETY_ZONE_ONLY,
	STRING_MESSAGE_TRADE_BUSY,
	STRING_MESSAGE_CANNOT_TRADE,
	STRING_MESSAGE_CANNOT_TRADE_ALREADY_TRADING,
	STRING_MESSAGE_CANNOT_TRADE_NO_SPACE,
	STRING_MESSAGE_SKILL_DIFFER_DOMAIN,
	STRING_MESSAGE_SKILL_EXCEED_LEVEL,
	STRING_MESSAGE_SKILL_CANNOT_LEARN,
	STRING_MESSAGE_SKILL_NOT_SUPPORT,
	STRING_MESSAGE_FIND_MOTOR_NO_WHERE,
	STRING_MESSAGE_FIND_MOTOR_NO_KEY,
	STRING_MESSAGE_FIND_MOTOR_OK,
	STRING_MESSAGE_CANNOT_USE_BONUS_POINT,
	STRING_MESSAGE_WHAT_SKILL_TO_LEARN,
	STRING_MESSAGE_NO_SKILL_TO_LEARN,
	STRING_MESSAGE_NEW_SKILL_AVAILABLE,
	STRING_MESSAGE_NEW_DOMAIN_LEVEL_1,
	STRING_MESSAGE_NEW_DOMAIN_LEVEL_2,
	STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_RELIC,			// You cannot enter your own safety zone while carrying a relic!

	STRING_MESSAGE_TEAM_REGIST_FAIL_ALREADY_JOIN,			// Let me see... it says here you already belong to another team.
	STRING_MESSAGE_TEAM_REGIST_FAIL_QUIT_TIMEOUT,			// You left your last team only moments ago. Think it over before you act.
	STRING_MESSAGE_TEAM_REGIST_FAIL_CANCEL_TIMEOUT,		// Your team was disbanded only moments ago. Train until you meet the requirements, and wait for a better opening.
	STRING_MESSAGE_TEAM_REGIST_FAIL_LEVEL,				// You are capable, but not yet leader material. Come back when you are stronger.
	STRING_MESSAGE_TEAM_REGIST_FAIL_MONEY,				// Founding a team takes a great deal of money, and you do not seem to have it.
	STRING_MESSAGE_TEAM_REGIST_FAIL_FAME,					// %s, is it... I have never heard that name. That makes you a novice. Come back when you have made a name for yourself.
	STRING_MESSAGE_TEAM_REGIST_FAIL_NAME,					// That team name is already taken. Think of another one.
	STRING_MESSAGE_TEAM_REGIST_FAIL_DENY,					// Your application was rejected.

	STRING_MESSAGE_TEAM_STARTING_FAIL_ALREADY_JOIN,		// You already belong to another team.
	STRING_MESSAGE_TEAM_STARTING_FAIL_QUIT_TIMEOUT,		// You left your last team only moments ago. Think it over before you act.
	STRING_MESSAGE_TEAM_STARTING_FAIL_CANCEL_TIMEOUT,		// Your team was disbanded only moments ago. Train until you meet the requirements, and wait for a better opening.
	STRING_MESSAGE_TEAM_STARTING_FAIL_LEVEL,				// You still have a lot to learn. Come back when you have trained a while longer.
	STRING_MESSAGE_TEAM_STARTING_FAIL_MONEY,				// %s, registering a team costs more money than that.
	STRING_MESSAGE_TEAM_STARTING_FAIL_FAME,				// %s, is it... I have never heard that name. That makes you a novice. Come back when you have made a name for yourself.
	STRING_MESSAGE_TEAM_STARTING_FAIL_DENY,					// Your application was rejected.

	STRING_MESSAGE_CLAN_REGIST_FAIL_ALREADY_JOIN,			// Let me see... it says here you are already sworn to another clan.
	STRING_MESSAGE_CLAN_REGIST_FAIL_QUIT_TIMEOUT,			// You left your last clan only moments ago. Drifting from clan to clan does you no good. Take your time.
	STRING_MESSAGE_CLAN_REGIST_FAIL_CANCEL_TIMEOUT,		// Your clan was disbanded only moments ago. Wait for a better opening.
	STRING_MESSAGE_CLAN_REGIST_FAIL_LEVEL,				// You are capable, but not yet fit to lead. Come back when you are stronger.
	STRING_MESSAGE_CLAN_REGIST_FAIL_MONEY,				// Registering a clan takes a great deal of money, and you do not seem to have it.
	STRING_MESSAGE_CLAN_REGIST_FAIL_FAME,					// %s, is it... still a young vampire, I see. Drink more blood and come back to me.
	STRING_MESSAGE_CLAN_REGIST_FAIL_NAME,					// That clan name is already taken. Think of another one.
	STRING_MESSAGE_CLAN_REGIST_FAIL_DENY,					// Your application was rejected.

	STRING_MESSAGE_CLAN_STARTING_FAIL_ALREADY_JOIN,		// You already belong to another clan.
	STRING_MESSAGE_CLAN_STARTING_FAIL_QUIT_TIMEOUT,		// You left your last clan only moments ago. Drifting from clan to clan does you no good. Take your time.
	STRING_MESSAGE_CLAN_STARTING_FAIL_CANCEL_TIMEOUT,		// Your clan was disbanded only moments ago. Wait for a better opening.
	STRING_MESSAGE_CLAN_STARTING_FAIL_LEVEL,				// You lack the makings of a good second. Go and train some more.
	STRING_MESSAGE_CLAN_STARTING_FAIL_MONEY,				// However skilled its members, a clan without money to keep it running will collapse.
	STRING_MESSAGE_CLAN_STARTING_FAIL_FAME,				// %s, is it... I have never heard that name. That makes you a novice. Come back when you have made a name for yourself.
	STRING_MESSAGE_CLAN_STARTING_FAIL_DENY,					// Your application was rejected.

	STRING_MESSAGE_GUILD_REGIST_FAIL_INTRO,					// Please enter an introduction.

	STRING_STATUS_HP_MAX_1,
	STRING_STATUS_HP_MAX_2,
	STRING_STATUS_MP_MAX_1,
	STRING_STATUS_MP_MAX_2,
	STRING_STATUS_STR_1,
	STRING_STATUS_STR_2,
	STRING_STATUS_DEX_1,
	STRING_STATUS_DEX_2,
	STRING_STATUS_INT_1,
	STRING_STATUS_INT_2,
	STRING_STATUS_LEVEL,
	STRING_LEARN_SKILL,
	STRING_CHANGE_ALIGNMENT,
	STRING_CHANGE_ALIGNMENT2,
	STRING_CHANGE_TO_BAD_ALIGNMENT,
	STRING_CHANGE_TO_GOOD_ALIGNMENT,

	STRING_ERROR_INVALID_ID_PASSWORD,
	STRING_ERROR_ALREADY_CONNECTED,
	STRING_ERROR_ALREADY_REGISTER_ID,
	STRING_ERROR_ALREADY_REGISTER_SSN,
	STRING_ERROR_EMPTY_ID,
	STRING_ERROR_SMALL_ID_LENGTH,
	STRING_ERROR_EMPTY_PASSWORD,
	STRING_ERROR_SMALL_PASSWORD_LENGTH,
	STRING_ERROR_EMPTY_NAME,
	STRING_ERROR_EMPTY_SSN,
	STRING_ERROR_INVALID_SSN,
	STRING_ERROR_NOT_FOUND_PLAYER,
	STRING_ERROR_NOT_FOUND_ID,
	STRING_ERROR_LOGIN_DENY,
	STRING_ERROR_ETC_ERROR,
	STRING_ERROR_NOT_ALLOW_ACCOUNT,
	STRING_ERROR_NOT_PAY_ACCOUNT,

	STRING_MESSAGE_ITEM_BROKEN,	
	STRING_MESSAGE_WHISPER_FAILED,
	STRING_MESSAGE_WHISPER_SELF,
	STRING_MESSAGE_CHAT_IGNORE,
	STRING_MESSAGE_CHAT_ACCEPT,
	STRING_MESSAGE_CHAT_IGNORE_ALL,
	STRING_MESSAGE_CHAT_ACCEPT_ALL,
	STRING_MESSAGE_CHAT_BE_GOOD,
	STRING_MESSAGE_CHAT_ACCEPT_CURSE,
	STRING_MESSAGE_CHAT_FILTER_CURSE,
	STRING_MESSAGE_CANNOT_LOGOUT_DIED,
	STRING_MESSAGE_CANNOT_GLOBAL_SAY,

	STRING_MESSAGE_WAIT,

	STRING_MESSAGE_CONNECTING_SERVER,

	STRING_MESSAGE_DONATION_OK,
	STRING_MESSAGE_DONATION_FAIL,

	STRING_MESSAGE_PARTY_REJECTED,
	STRING_MESSAGE_PARTY_NOBODY,		
	STRING_MESSAGE_PARTY_SAFETY_ZONE_ONLY,
	STRING_MESSAGE_PARTY_BUSY,
	STRING_MESSAGE_RACE_DIFFER,
	STRING_MESSAGE_NOT_NORMAL_FORM,
	STRING_MESSAGE_NO_AUTHORITY,
	STRING_MESSAGE_ERROR_PARTY,
	STRING_MESSAGE_KICK_PARTY_MEMBER_OK,
	STRING_MESSAGE_KICKED_FROM_PARTY,
	STRING_MESSAGE_KICK_PARTY_MEMBER,
	STRING_MESSAGE_REMOVE_PARTY,
	STRING_MESSAGE_REMOVE_PARTY_HIMSELF,
	STRING_MESSAGE_REMOVE_PARTY_MYSELF,
	STRING_MESSAGE_IN_ANOTHER_PARTY,
	STRING_MESSAGE_PARTY_FULL,
	STRING_MESSAGE_SOMEONE_JOINED_PARTY,

	STRING_MESSAGE_LOGOUT_AFTER_SECOND,

	STRING_MESSAGE_CANNOT_BUY_NO_STAR,
	STRING_MESSAGE_CANNOT_BUY,

	STRING_MESSAGE_CAN_REGISTER_NAME,

	STRING_MESSAGE_HELP_KEY,
	STRING_MESSAGE_HOW_TO_GET_BASIC_WEAPON,
	STRING_MESSAGE_WAIT_FOR_CHARACTER_SELECT_MODE,

	STRING_MESSAGE_ITEM_TO_ITEM_IMPOSIBLE,

	
	UI_STRING_MESSAGE_ONLY_PICK_UP_ITEM_ONE,       // You have to carry them one at a time.
	UI_STRING_MESSAGE_CAN_ENCHANT,					// This can be enchanted.
	UI_STRING_MESSAGE_SELL_CONFIRM,					// Sell this item?
	UI_STRING_MESSAGE_ALL_SELL_CONFIRM,				// Sell them?
	UI_STRING_MESSAGE_ALL_PRICE,					// That comes to $%s in total.
	UI_STRING_MESSAGE_REPAIR_CONFIRM,				// Repair it?
	UI_STRING_MESSAGE_CLICK_TRADE_ITEM,				// Click the item you want to trade.
	UI_STRING_MESSAGE_CLICK_REPAIR_ITEM,			// Click the item you want to repair.
	UI_STRING_MESSAGE_CLICK_OK_BUTTON_TO_END,		// Press the OK button when you are done.
	UI_STRING_MESSAGE_THIS_ITEM_REPAIR_CONFIRM,		// Repair this item?
	UI_STRING_MESSAGE_THIS_ITEM_CHARGE_CONFIRM,		// Recharge this item?
	UI_STRING_MESSAGE_THIS_ITEM_SILVERING_CONFIRM,	// Silver-plate this item?
	UI_STRING_MESSAGE_CLICK_SILVERING_ITEM,			// Click the item you want silver-plated.
	UI_STRING_MESSAGE_SHOW_ALPHA_WINDOW,			// Draw windows semi-transparent.
	UI_STRING_MESSAGE_SHOW_NO_ALPHA_WINDOW,			// Do not draw windows semi-transparent.
	UI_STRING_MESSAGE_CAN_EQUIP,					// You can equip this.
	UI_STRING_MESSAGE_CANNOT_EQUIP,					// You cannot equip this.
	UI_STRING_MESSAGE_THROW_MONEY,					// Drop money
	UI_STRING_MESSAGE_SHOW_ITEM_DESCRIPTION,		// View the item description
	UI_STRING_MESSAGE_DEPOSIT_MONEY,				// Deposit money
	UI_STRING_MESSAGE_INVITE_PARTY,					// You have a party invitation.
	UI_STRING_MESSAGE_REQUEST_PARTY,				// You have a party join request.
	UI_STRING_MESSAGE_RESURRECTION,					// Resurrect at the set location
	UI_STRING_MESSAGE_EVACUATION,					// Evacuate to the set location
	UI_STRING_MESSAGE_CANNOT_UP_LEVEL,				// You cannot level up any further
	UI_STRING_MESSAGE_CANNOT_UP_STAT,				// That stat cannot go any higher.
	UI_STRING_MESSAGE_TEAM_INFO_JOIN,				// Join

	// UI TIP
	UI_STRING_MESSAGE_TIP_SELL_ALL_VAMPIRE_HEAD,	// TIP : right-click to sell all vampire heads at once.
	UI_STRING_MESSAGE_TIP_REPAIR_ALL_ITEM,			// TIP : right-click to repair all your gear at once.


	UI_STRING_MESSAGE_AUTO_HIDE_ON,					// Auto-hide on
	UI_STRING_MESSAGE_AUTO_HIDE_OFF,				// Auto-hide off
	UI_STRING_MESSAGE_GAME_MENU,					// Game Menu
	UI_STRING_MESSAGE_MENU,							// Menu
	UI_STRING_MESSAGE_EXP,							// Exp
	UI_STRING_MESSAGE_TEAM,							// Team
	UI_STRING_MESSAGE_MESSAGE,						// Message
	UI_STRING_MESSAGE_HELP,							// Help
	UI_STRING_MESSAGE_INVENTORY,					// Inventory
	UI_STRING_MESSAGE_GEAR_WINDOW,					// Gear Window
	UI_STRING_MESSAGE_INFO_WINDOW,					// Info Window
	UI_STRING_MESSAGE_PARTY_WINDOW,					// Party Window
	UI_STRING_MESSAGE_QUEST_WINDOW,					// Quest Window
	UI_STRING_MESSAGE_TEAM_INFO,					// Team Info
	UI_STRING_MESSAGE_TEAM_MEMBER_LIST,				// Team Member List
	UI_STRING_MESSAGE_BASIC_HELP,					// Basic Help
	UI_STRING_MESSAGE_CHATTING_HELP,				// Chat Help
	UI_STRING_MESSAGE_WAR_HELP,						// Combat Help
	UI_STRING_MESSAGE_SKILL_HELP,					// Skill Help
	UI_STRING_MESSAGE_TEAM_HELP,					// Team Help
	UI_STRING_MESSAGE_CLAN,							// Clan
	UI_STRING_MESSAGE_CLAN_INFO,					// Clan Info
	UI_STRING_MESSAGE_CLAN_MEMBER_LIST,				// Clan Member List
	UI_STRING_MESSAGE_CLAN_HELP,					// Clan Help
	UI_STRING_MESSAGE_MAGIC_HELP,					// Magic Help

	// Simple Info
	UI_STRING_MESSAGE_CURRENT_EXP,					// Current Exp:
	UI_STRING_MESSAGE_NEXT_LEVEL,					// Next Level:
	UI_STRING_MESSAGE_LEFT_EXP,						// Exp Remaining:

	// Inventory
	UI_STRING_MESSAGE_EQUIP_HELM,					// Wear a helmet
	UI_STRING_MESSAGE_EQUIP_NECKLACE,				// Wear a necklace
	UI_STRING_MESSAGE_EQUIP_ARMOR,					// Wear armor
	UI_STRING_MESSAGE_EQUIP_SHIELD,					// Equip a shield
	UI_STRING_MESSAGE_EQUIP_WEAPON,					// Equip a weapon
	UI_STRING_MESSAGE_EQUIP_GLOVE,					// Wear gloves
	UI_STRING_MESSAGE_EQUIP_BELT,					// Wear a belt
	UI_STRING_MESSAGE_EQUIP_TROUSER,				// Wear trousers
	UI_STRING_MESSAGE_EQUIP_BRACELET,				// Wear a bracelet
	UI_STRING_MESSAGE_EQUIP_RING,					// Wear a ring
	UI_STRING_MESSAGE_EQUIP_SHOES,					// Wear shoes
	UI_STRING_MESSAGE_EQUIP_VAMPIRE_COAT,			// Wear a coat
	UI_STRING_MESSAGE_EQUIP_EARRING,				// Wear an earring
	UI_STRING_MESSAGE_EQUIP_AMULET,					// Wear an amulet
	UI_STRING_MESSAGE_FIRST_GEAR_SET,				// First weapon set
	UI_STRING_MESSAGE_SECOND_GEAR_SET,				// Second weapon set

	// Chating Description
	UI_STRING_MESSAGE_SPECIAL_CHARACTER,			// Special characters (Control+X)
	UI_STRING_MESSAGE_SET_LETTER_COLOR,				// Set text color
	UI_STRING_MESSAGE_CHANGE_INPUT_LANGUAGE,		// KOR/ENG
	UI_STRING_MESSAGE_SHOW_WHISPER_ID,				// Show IDs you have whispered
	UI_STRING_MESSAGE_NORMAL_CHATING,				// Normal chat (Control+C)
	UI_STRING_MESSAGE_ZONE_CHATTING,				// Zone chat (Control+Z)
	UI_STRING_MESSAGE_WHISPER_CHATTING,				// Whisper (Control+W)
	UI_STRING_MESSAGE_PARTY_CHATTING,				// Party chat
	UI_STRING_MESSAGE_TEAM_CHATTING,				// Team chat (Control+G)
	UI_STRING_MESSAGE_SHOW_NORMAL_CHATTING,			// Show normal chat
	UI_STRING_MESSAGE_SHOW_ZONE_CHATTING,			// Show zone chat
	UI_STRING_MESSAGE_SHOW_WHISPER_CHATTING,		// Show whispers
	UI_STRING_MESSAGE_SHOW_PARTY_CHATTING,			// Show party chat
	UI_STRING_MESSAGE_SHOW_TEAM_CHATTING,			// Show team chat
	UI_STRING_MESSAGE_NOT_SHOW_NORMAL_CHATTING,		// Hide normal chat
	UI_STRING_MESSAGE_NOT_SHOW_ZONE_CHATTING,		// Hide zone chat
	UI_STRING_MESSAGE_NOT_SHOW_WHISPER_CHATTING,	// Hide whispers
	UI_STRING_MESSAGE_NOT_SHOW_PARTY_CHATTING,		// Hide party chat
	UI_STRING_MESSAGE_NOT_SHOW_TEAM_CHATTING,		// Hide team chat
	UI_STRING_MESSAGE_NOT_SHOW_CLAN_CHATTING,		// Hide clan chat
	UI_STRING_MESSAGE_SHOW_CLAN_CHATTING,			// Show clan chat
	UI_STRING_MESSAGE_CLAN_CHATTING,				// Clan chat
	UI_STRING_MESSAGE_EXPEL,						// Leave
	UI_STRING_MESSAGE_SECEDE,						// Expel
	
	// Party
	UI_STRING_MESSAGE_SET_INVITE_DENY_MODE,			// Turn on invitation refusal
	UI_STRING_MESSAGE_CANCEL_INVITE_DENY_MODE,		// Turn off invitation refusal
	UI_STRING_MESSAGE_SET_JOIN_DENY_MODE,			// Turn on join refusal
	UI_STRING_MESSAGE_CANCEL_JOIN_DENY_MODE,		// Turn off join refusal
	

	// Level Up	
	UI_STRING_MESSAGE_UP_STR,						// Raise strength
	UI_STRING_MESSAGE_UP_DEX,						// Raise dexterity
	UI_STRING_MESSAGE_UP_INT,						// Raise intelligence

	
	// INFO WINDOW
	UI_STRING_MESSAGE_SHOW_SKILL_INFO_WINDOW,		// Open the skill info window
	UI_STRING_MESSAGE_SHOW_CHARACTER_INFO_WINDOW,	// Open the character info window
	UI_STRING_MESSAGE_SHOW_MAGIC_INFO_WINDOW,		// Open the magic info window
	UI_STRING_MESSAGE_SHOW_ADVANTE_INFO_WINODW,		// Open the second-class info window
	UI_STRING_MESSAGE_SHOW_BLADE_INFO,				// Show the blade skill line
	UI_STRING_MESSAGE_SHOW_SWORD_INFO,				// Show the sword skill line
	UI_STRING_MESSAGE_SHOW_GUN_INFO,				// Show the gun skill line
	UI_STRING_MESSAGE_SHOW_HEAL_INFO,				// Show the healing skill line
	UI_STRING_MESSAGE_SHOW_ENCHANT_INFO,			// Show the blessing skill line
	UI_STRING_MESSAGE_SHOW_ALL_INFO,				// Show every skill line
	UI_STRING_MESSAGE_SHOW_POISON_INFO,				// Show the poison skill line
	UI_STRING_MESSAGE_SHOW_ACID_INFO,				// Show the acid skill line
	UI_STRING_MESSAGE_SHOW_CURSE_INFO,				// Show the curse skill line
	UI_STRING_MESSAGE_SHOW_SUMMON_INFO,				// Show the summoning skill line
	UI_STRING_MESSAGE_SHOW_BLOOD_INFO,				// Show the blood skill line
	UI_STRING_MESSAGE_SHOW_ESSENCE_INFO,			// Show the innate skill line
	
	

	// SHOW HELP WINDOW
	UI_STRING_MESSAGE_SHOW_HELP_MAGIC_INFO_WINDOW,	// Show help for the magic info window
	UI_STRING_MESSAGE_SHOW_HELP_SKILL_INFO_WINDOW,	// Show help for the skill info window
	UI_STRING_MESSAGE_SHOW_HELP_INVENTORY_WINDOW,	// Show help for the inventory window
	UI_STRING_MESSAGE_SHOW_HELP_PARTY_MANAGER,		// Show help for the party manager window
	UI_STRING_MESSAGE_SHOW_HELP_CHARACTER_INFO_WINDOW,	// Show help for the character info window
	UI_STRING_MESSAGE_SHOW_HELP_GEAR_WINDOW,			// Show help for the gear window
	UI_STRING_MESSAGE_SHOW_HELP_STORAGE_WINDOW,			// Show help for the storage window
	UI_STRING_MESSAGE_SHOW_HELP_SHOP_WINDOW,			// Show help for the shop window
	UI_STRING_MESSAGE_DESC_SHOW_EXCHANGE_WINDOW,			// Show help for the exchange window
	


	// CLOSE WINDOW
	UI_STRING_MESSAGE_CLOSE_INVENTORY_WINDOW,			// Close the inventory window
	UI_STRING_MESSAGE_CLOSE_PARTY_MANAGER,				// Close the party manager window
	UI_STRING_MESSAGE_CLOSE_CHARACTER_INFO_WINDOW,		// Close the character info window
	UI_STRING_MESSAGE_CLOSE_GEAR_WINDOW,				// Close the gear window
	UI_STRING_MESSAGE_CLOSE_MAGIC_INFO_WINDOW,			// Close the magic info window
	UI_STRING_MESSAGE_CLOSE_SKILL_INFO_WINDOW,			// Close the skill info window
	UI_STRING_MESSAGE_CLOSE_DESC_WINDOW,				// Close the description window
	UI_STRING_MESSAGE_CLOSE_STORAGE_WINDOW,				// Close the storage window
	UI_STRING_MESSAGE_CLOSE_SHOP_WINDOW,				// Close the shop window
	UI_STRING_MESSAGE_CLOSE_BOOKCASE,					// Close the bookcase
	UI_STRING_MESSAGE_CLOSE_BOOK,						// Close the book
	
	
	
	UI_STRING_MESSAGE_NOT_JOIN_ANY_TEAM,				// No team registered
	UI_STRING_MESSAGE_NOT_JOIN_ANY_CLAN,				// No clan registered
	UI_STRING_MESSAGE_INFRA_RED_HELMET,					// Infrared Scanning Helmet
	UI_STRING_MESSAGE_INFRA_HELMET,						// Infra Scanning Helmet
	UI_STRING_MESSAGE_QUIT_COMPUTER,					// Shutting down the computer

	UI_STRING_MESSAGE_SHOW_NEXT_PAGE,					// Go to the next page
	UI_STRING_MESSAGE_SHOW_PREV_PAGE,					// Go to the previous page


	UI_STRING_MESSAGE_ENG_STR,						// STR
	UI_STRING_MESSAGE_ENG_DEX,						// DEX
	UI_STRING_MESSAGE_ENG_INT,						// INT
	
	UI_STRING_MESSAGE_MASTER_NAME,					// GM
	UI_STRING_MESSAGE_LIMIT_STRING_COUNT,			// Do not spam!!!
	UI_STRING_MESSAGE_HELP_MESSAGE,					// Help
	UI_STRING_MESSAGE_STR,							// Strength
	UI_STRING_MESSAGE_DEX,							// Dexterity
	UI_STRING_MESSAGE_INT,							// Intelligence
	UI_STRING_MESSAGE_HP,							// Health
	UI_STRING_MESSAGE_MP,							// Mana
	UI_STRING_MESSAGE_ACCURACY,						// Accuracy
	UI_STRING_MESSAGE_DAMAGE,						// Damage
	UI_STRING_MESSAGE_AVOID,						// Evasion
	UI_STRING_MESSAGE_DEFENCE,						// Defense
	UI_STRING_MESSAGE_ALIGNMENT,					// Alignment

	UI_STRING_MESSAGE_ALIGN_VERY_BAD,				// Very Evil
	UI_STRING_MESSAGE_ALIGN_BAD,					// Evil
	UI_STRING_MESSAGE_ALIGN_NORMAL,					// Neutral
	UI_STRING_MESSAGE_ALIGN_GOOD,					// Good
	UI_STRING_MESSAGE_ALIGN_VERY_GOOD,				// Very Good

	UI_STRING_MESSAGE_STR_PURE,						// Base Strength : %d
	UI_STRING_MESSAGE_DEX_PURE,						// Base Dexterity : %d
	UI_STRING_MESSAGE_INT_PURE,						// Base Intelligence : %d
	UI_STRING_MESSAGE_STATUS_SUM_PURE,				// Base Stat Total : %d
	UI_STRING_MESSAGE_STATUS_SUM,					// Stat Total : %d

	UI_STRING_MESSAGE_SPEED_SLOW,					// Slow
	UI_STRING_MESSAGE_SPEED_NORMAL,					// Normal
	UI_STRING_MESSAGE_SPEED_FAST,					// Fast

	UI_STRING_MESSAGE_ATTACK_SPEED,					// Attack Speed : %s[%d]

	// HpBar	
	UI_STRING_MESSAGE_PASSIVE,						// "Passive"
	UI_STRING_MESSAGE_HAN_PASSIVE,					// Passive

	UI_STRING_MESSAGE_HPBAR_LEVEL_DESCRIPTION,		// "LEVEL:%d"
	UI_STRING_MESSAGE_HPBAR_EXP_DESCRIPTION,		// EXP:%s/%s (remaining:%s)
	UI_STRING_MESSAGE_HPBAR_MP_DESCRIPTION,			// "MP:%d/%d"
	UI_STRING_MESSAGE_HPBAR_HP_DESCRIPTION_WITH_SILVERING,			// "HP:%d/%d(S:%d)"
	UI_STRING_MESSAGE_HPBAR_HP_DESCRIPTION,							// "HP:%d/%d"	
	UI_STRING_MESSAGE_HPBAR_CHANGE_VAMPIRE_DAY,		// You will turn into a vampire in %d days %d hours %d minutes.
	UI_STRING_MESSAGE_HPBAR_CHANGE_VAMPIRE_HOUR,	// You will turn into a vampire in %d hours %d minutes.
	UI_STRING_MESSAGE_HPBAR_CHANGE_VAMPIRE_MINUTE,	// You will turn into a vampire in %d minutes.
	UI_STRING_MESSAGE_HPBAR_CHANGE_VAMPIRE_SOON,	// You will turn into a vampire soon.
	UI_STRING_MESSAGE_HPBAR_SET_LARGE,				// Show the HP bar vertically
	UI_STRING_MESSAGE_HPBAR_SET_SMALL,				// Show a small HP bar
	UI_STRING_MESSAGE_HPBAR_SET_WIDTH,				// Show the HP bar horizontally
	UI_STRING_MESSAGE_HPBAR_SET_HEIGHT,				// Show a large HP bar

	
	UI_STRING_MESSAGE_SHOW_WIDTH,					// Show horizontally.
	UI_STRING_MESSAGE_SHOW_HEIGHT,					// Show vertically.

	// Guild
	UI_STRING_MESSAGE_SHOW_HELP,					// Show the help.
	UI_STRING_MESSAGE_CLOSE_WINDOW,					// Close the window.
	UI_STRING_MESSAGE_REGIST,						// Register.
	UI_STRING_MESSAGE_LIST_UP,						// Previous page
	UI_STRING_MESSAGE_LIST_DOWN,					// Next page
	UI_STRING_MESSAGE_REFRESH_LIST,					// Fetch the list again.
	UI_STRING_MESSAGE_SORT_TEAM_NAME,				// Sort by team name.
	UI_STRING_MESSAGE_SORT_LEADER_NAME,				// Sort by leader name.
	UI_STRING_MESSAGE_SORT_EXPIRE_DATE,				// Sort by expiry date.
	UI_STRING_MESSAGE_SORT_NUMBER_MEMBER,			// Sort by member count.
	UI_STRING_MESSAGE_SORT_RANKING,					// Sort by rank.

	UI_STRING_MESSAGE_CHANGE_FIND_MODE,				// Change the search mode
	UI_STRING_MESSAGE_FIND,							// Search

	UI_STRING_MESSAGE_TEAM_GRADE_MASTER,			// Master
	UI_STRING_MESSAGE_TEAM_GRADE_SUB_MASTER,		// Sub Master
	UI_STRING_MESSAGE_TEAM_GRADE_WAIT,				// Awaiting approval
	UI_STRING_MESSAGE_TEAM_GRADE_MEMBER,			// Member

	UI_STRING_MESSAGE_TEAM_INFO_GRADE,				//Grade : %s
	UI_STRING_MESSAGE_TEAM_INFO_NAME,				//Name : %s
	UI_STRING_MESSAGE_TEAM_INFO_LEADER,				//Leader : %s
	UI_STRING_MESSAGE_TEAM_INFO_REG_FEE,			//Reg. Fee : $%s
	UI_STRING_MESSAGE_TEAM_INFO_MEMBERS,			//Members : "
	UI_STRING_MESSAGE_TEAM_INFO_INTRODUCTION,		//Introduction : "
	UI_STRING_MESSAGE_TEAM_INFO_CLAN_INTRODUCTION,	//Clan Introduction : "
	UI_STRING_MESSAGE_TEAM_INFO_TEAM_INTRODUCTION,	//Team Introduction : "
	UI_STRING_MESSAGE_TEAM_INFO_RANKING,			//Ranking : %d

	UI_STRING_MESSAGE_TEAM_INFO_TEAM_NAME,			//Team Name : %s"
	UI_STRING_MESSAGE_TEAM_INFO_CLAN_NAME,			//Clan Name : %s"
	UI_STRING_MESSAGE_TEAM_INFO_REGISTRATION_FEE,	//Registration Fee : $%s"
	UI_STRING_MESSAGE_TEAM_INFO_SELF_INTRODUCTION,	//Self Introduction : "

	UI_STRING_MESSAGE_UP,							// Up
	UI_STRING_MESSAGE_DOWN,							// Down
	UI_STRING_MESSAGE_TEAM_INFO_EXPEL,				// Expel them.
	UI_STRING_MESSAGE_TEAM_INFO_JOIN_ACCEPT,		// Accept the application
	UI_STRING_MESSAGE_TEAM_INFO_JOIN_DENY,			// Reject the application
	UI_STRING_MESSAGE_CANCEL,						// Cancel

	
	// Zone Name

	UI_STRING_MESSAGE_LIMBOLAIR,					// Limbo Lair
	UI_STRING_MESSAGE_ESLANIA,						// Eslania
	UI_STRING_MESSAGE_RODIN,						// Mount Rodin
	UI_STRING_MESSAGE_DROBETA,						// Drobeta
	UI_STRING_MESSAGE_PERONA,						// Perona Highway
	UI_STRING_MESSAGE_TIMORE,						// Lake Timore

	UI_STRING_MESSAGE_ZONEINFO_XY,					// X:%d Y:%d

	// Accelerator Name
	UI_STRING_MESSAGE_ACCELERATOR_NULL,				//"AcceleratorNULL",
	UI_STRING_MESSAGE_ACCELERATOR_F1,				//"F1",
	UI_STRING_MESSAGE_ACCELERATOR_F2,				//"F2",
	UI_STRING_MESSAGE_ACCELERATOR_F3,				//"F3",
	UI_STRING_MESSAGE_ACCELERATOR_F4,				//"F4",
	UI_STRING_MESSAGE_ACCELERATOR_F5,				//"F5",
	UI_STRING_MESSAGE_ACCELERATOR_F6,				//"F6",
	UI_STRING_MESSAGE_ACCELERATOR_F7,				//"F7",
	UI_STRING_MESSAGE_ACCELERATOR_F8,				//"F8",
	UI_STRING_MESSAGE_ACCELERATOR_F9,				//"F9",
	UI_STRING_MESSAGE_ACCELERATOR_F10,				//"F10",
	UI_STRING_MESSAGE_ACCELERATOR_F11,				//"F11",
	UI_STRING_MESSAGE_ACCELERATOR_F12,				//"F12",
	UI_STRING_MESSAGE_ACCELERATOR_ESC,				//"ESC",
	UI_STRING_MESSAGE_ACCELERATOR_SKILL,			//"SKILL",
	UI_STRING_MESSAGE_ACCELERATOR_INVENTORY,		//"INVENTORY",
	UI_STRING_MESSAGE_ACCELERATOR_GEAR,				//"GEAR",
	UI_STRING_MESSAGE_ACCELERATOR_CHARINFO,			//"CHARINFO",
	UI_STRING_MESSAGE_ACCELERATOR_SKILLINFO,		//"SKILLINFO",
	UI_STRING_MESSAGE_ACCELERATOR_MINIMAP,			//"MINIMAP",
	UI_STRING_MESSAGE_ACCELERATOR_PARTY,			//"PARTY",
	UI_STRING_MESSAGE_ACCELERATOR_MARK,				//"MARK",
	UI_STRING_MESSAGE_ACCELERATOR_HELP,				//"HELP",
	UI_STRING_MESSAGE_ACCELERATOR_QUICKITEM_SLOT,	//"QUICKITEM_SLOT",
	UI_STRING_MESSAGE_ACCELERATOR_EXTEND_CHAT,		//"EXTEND_CHAT",
	UI_STRING_MESSAGE_ACCELERATOR_CHAR,				//"CHAT",
	UI_STRING_MESSAGE_ACCELERATOR_GUILD_CHAT,		//"GUILD_CHAT",
	UI_STRING_MESSAGE_ACCELERATOR_ZONE_CHAT,		//"ZONE_CHAT",
	UI_STRING_MESSAGE_ACCELERATOR_WHISPER,			//"WHISPER",

	// ITEM CLASS NAME

	UI_STRING_MESSAGE_ITEM_CLASS_SWORD,				// Sword (SWORD)
	UI_STRING_MESSAGE_ITEM_CLASS_BLADE,				// Blade (BLADE)
	UI_STRING_MESSAGE_ITEM_CLASS_CROSS,				// Cross (CROSS)
	UI_STRING_MESSAGE_ITEM_CLASS_MACE,				// Mace (MACE)
	UI_STRING_MESSAGE_ITEM_CLASS_MINE,				// Mine (MINE)
	UI_STRING_MESSAGE_ITEM_CLASS_BOMB,				// Bomb (BOMB)
	UI_STRING_MESSAGE_ITEM_CLASS_MINE_MATERIAL,		// Mine material (MATERIAL)
	UI_STRING_MESSAGE_ITEM_CLASS_BOMB_MATERIAL,		// Bomb material (MATERIAL)
	UI_STRING_MESSAGE_ITEM_CLASS_SG,				// Shotgun (SG)
	UI_STRING_MESSAGE_ITEM_CLASS_SMG,				// Submachine gun (SMG)
	UI_STRING_MESSAGE_ITEM_CLASS_AR,				// Assault rifle (AR)
	UI_STRING_MESSAGE_ITEM_CLASS_SR,				// Sniper rifle (SR)
	UI_STRING_MESSAGE_DESC_CLASS,					// Class : 
	UI_STRING_MESSAGE_DESC_DURABILITY,				// Durability : 
	UI_STRING_MESSAGE_DESC_SILVERING,				// Silver plating : 
	UI_STRING_MESSAGE_DESC_DAMAGE,					// Damage : 
	UI_STRING_MESSAGE_DESC_CRITICALHIT,				// Critical hit : 
	UI_STRING_MESSAGE_DESC_DEFENSE,					// Defense : 
	UI_STRING_MESSAGE_DESC_PROTECTION,				// Protection : 
	UI_STRING_MESSAGE_DESC_ACCURACY,				// Accuracy : 
	UI_STRING_MESSAGE_DESC_HP,						//HP : "
	UI_STRING_MESSAGE_DESC_MP,						//MP : "
	UI_STRING_MESSAGE_DESC_RANGE,					// Range : 
	UI_STRING_MESSAGE_DESC_MAGAZINE_NUM,			// Rounds : 
	UI_STRING_MESSAGE_DESC_POCKET_NUM,				// Pockets : 
	UI_STRING_MESSAGE_DESC_ITEM_NUM,				// Items : 
	UI_STRING_MESSAGE_DESC_LEFT_NUM,				// Uses left : 
	UI_STRING_MESSAGE_DESC_ARRIVAL_LOCATION,		// Destination : 
	UI_STRING_MESSAGE_DESC_NOT_EXIST,				// None

	UI_STRING_MESSAGE_DESC_NUMBER,						// 
	UI_STRING_MESSAGE_DESC_MAGAZINE_COUNT,				//  rounds
	UI_STRING_MESSAGE_DESC_TILE_PIECE,					//  tiles
	UI_STRING_MESSAGE_DESC_PARTY_NAME,					// Party

	UI_STRING_MESSAGE_DESC_OPTION,						// Option : 
	UI_STRING_MESSAGE_DESC_REQUIRE,						// Requires : 
	UI_STRING_MESSAGE_DESC_REQUIRE_STAT,					// %d or more
	UI_STRING_MESSAGE_DESC_ONLY_MALE,					// Male only
	UI_STRING_MESSAGE_DESC_ONLY_FEMALE,					// Female only
	UI_STRING_MESSAGE_DESC_ONLY_SLAYER,					// Slayers only
	UI_STRING_MESSAGE_DESC_ONLY_VAMPIRE,				// Vampires only
	UI_STRING_MESSAGE_DESC_ALL_STAT_SUM,				// Stat total
	UI_STRING_MESSAGE_DESC_LEVEL,						// Level
	UI_STRING_MESSAGE_DESC_PRICE,						// Price : 
	UI_STRING_MESSAGE_DESC_SILVERING_PRICE,				// Silver plating price : 
	UI_STRING_MESSAGE_DESC_CHARGE_PRICE,				// Recharge price : 
	UI_STRING_MESSAGE_DESC_REPAIR_PRICE,				// Repair price : 

	UI_STRING_MESSAGE_CONSUME_HP,						// HP cost : 
	UI_STRING_MESSAGE_CONSUME_MP,						// MP cost : 

	UI_STRING_MESSAGE_REQUIRE_LEVEL,					// Required level : 
	UI_STRING_MESSAGE_DESC_EXP,							// Exp : 
	UI_STRING_MESSAGE_DESC_LIMIT_LEVEL,					// Growth limit level : 
	UI_STRING_MESSAGE_DESC_CANNOT_LEARN_SKILL,			// You cannot learn this yet.
	UI_STRING_MESSAGE_DESC_CAN_LEARN_SKILL,				// You can learn this.

	UI_STRING_MESSAGE_MOVE,								// Move there
	UI_STRING_MESSAGE_CANCEL_MOVE,						// Cancel the move
	UI_STRING_MESSAGE_MOVE_F1,							// Go to the 1st floor
	UI_STRING_MESSAGE_MOVE_F2,							// Go to the 2nd floor
	UI_STRING_MESSAGE_MOVE_F3,							// Go to the 3rd floor
	UI_STRING_MESSAGE_MOVE_F4,							// Go to the 4th floor
	UI_STRING_MESSAGE_MOVE_B1,							// Go to basement level 1

	UI_STRING_MESSAGE_DESC_CLICK_EXCHANGE,					// Click here to exchange
	UI_STRING_MESSAGE_DESC_CANCEL_OK_BUTTON,				// Take back your OK
	UI_STRING_MESSAGE_DESC_CANCEL_EXCHANGE,					// Cancel the exchange
	UI_STRING_MESSAGE_DESC_BRING_MONEY,						// Take back the money you offered
	UI_STRING_MESSAGE_DESC_EXCHANGE_MONEY,					// Offer money
	UI_STRING_MESSAGE_DESC_EXCHANGE_YOUR_MONEY,				// Money the other player is offering

	UI_STRING_MESSAGE_DESC_LEFT_MONEY_AFTER_EXCHANGE,		// Money left after the exchange
	UI_STRING_MESSAGE_DESC_WILL_EXCHANGE_MONEY,				// Amount to exchange

	UI_STRING_MESSAGE_TRHOW_MONEY_IN_DIALOG,				// Enter the amount to drop.
	UI_STRING_MESSAGE_SAVE_MONEY_IN_DIALOG,					// Enter the amount to deposit.
	UI_STRING_MESSAGE_BRING_MONEY_IN_DIALOG,				// Enter the amount to withdraw.
	UI_STRING_MESSAGE_TRADE_MONEY_IN_DIALOG,				// Enter the amount to trade.
	UI_STRING_MESSAGE_DIVIDE_MONEY_IN_DIALOG,				// Enter the amount to split off.
	
	UI_STRING_MESSAGE_BUY_ITEM,							// Buy this item?
	UI_STRING_MESSAGE_BUY_ITEM_NUM,						// Buy            of this item?

	UI_STRING_MESSAGE_ASK_DIALOG_BUY_STORAGE,			// Buy a storage box for $%d?
	UI_STRING_MESSAGE_ASK_DIALOG_TRADE_OTHER_PLAYER,	// Trade with %s?
	UI_STRING_MESSAGE_ASK_DIALOG_WAIT_OTHER_PLAYER,		// Waiting for %s to answer.
	UI_STRING_MESSAGE_ASK_DIALOG_CANCEL,				// Press Cancel to call it off.
	UI_STRING_MESSAGE_ASK_DIALOG_REQUEST_JOIN,			// %s is asking to join your %s.
	UI_STRING_MESSAGE_ASK_DIALOG_INVITE,				// %s has invited you to %s.
	UI_STRING_MESSAGE_CANNOT_FIND_RESULT,				// No search results found.
	UI_STRING_MESSAGE_ENCHANT_CONFIRM,					// Enchant this item?

	UI_STRING_MESSAGE_DESC_DIALOG_ITEM_CLASS_SG,		//                         Class : SG
	UI_STRING_MESSAGE_DESC_DIALOG_ITEM_CLASS_SMG,		//                         Class : SMG
	UI_STRING_MESSAGE_DESC_DIALOG_ITEM_CLASS_AR,		//                         Class : AR
	UI_STRING_MESSAGE_DESC_DIALOG_ITEM_CLASS_SR,		//                         Class : SR

	UI_STRING_MESSAGE_DESC_DIALOG_DURABILITY,			//                         Durability : %d
	UI_STRING_MESSAGE_DESC_DIALOG_SILVERING,			//                         Silver plating : %d
	UI_STRING_MESSAGE_DESC_DIALOG_DAMAGE,				//                         Damage : %d~%d
	UI_STRING_MESSAGE_DESC_DIALOG_CRITICALHIT,			//                         Critical hit : %d
	UI_STRING_MESSAGE_DESC_DIALOG_DEFENSE,				//                         Defense : %d
	UI_STRING_MESSAGE_DESC_DIALOG_PROTECTION,			//                         Protection : %d
	UI_STRING_MESSAGE_DESC_DIALOG_ACCURACY,				//                         Accuracy : %d
	UI_STRING_MESSAGE_DESC_DIALOG_HP,					//"\tHP : +%d"
	UI_STRING_MESSAGE_DESC_DIALOG_MP,					//"\tMP : +%d"
	UI_STRING_MESSAGE_DESC_DIALOG_RANGE,				//                         Range : %d tiles
	UI_STRING_MESSAGE_DESC_DIALOG_MAGAZINE_NUM,			//                         Rounds : %d
	UI_STRING_MESSAGE_DESC_DIALOG_POCKET_NUM,			//                         Pockets : %d
	UI_STRING_MESSAGE_DESC_DIALOG_OPTION,				//                         Option : %s +%d
	UI_STRING_MESSAGE_DESC_DIALOG_OPTION_EMPTY,			//"\t       %s +%d"
	UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_EMPTY,		//"\t             ");
	UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE,				//                         Requires : 
	UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_STR,			// STR %d or more
	UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_DEX,			// DEX %d or more
	UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_INT,			// INT %d or more
	UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_ALL_STAT_SUM,	// Stat total %d or more
	UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_LEVEL,		// Level %d or more
	UI_STRING_MESSAGE_DESC_DIALOG_PRICE,				//                         Price : %s

	UI_STRING_MESSAGE_FILE_DIALOG_SELECT_PROFILE_PICTURE,	// Choose the picture to use on your profile.
	UI_STRING_MESSAGE_FILE_DIALOG_SELECT_FILE,				// Choose the files to send (CTRL for multiple).
	UI_STRING_MESSAGE_FILE_DIALOG_SELECT_FILE_OK,			// Select this file.
	UI_STRING_MESSAGE_FILE_DIALOG_CANCEL,					// Cancel and close the window.

	UI_STRING_MESSAGE_ENG_DOMAIN_BLADE,					//BLADE
	UI_STRING_MESSAGE_ENG_DOMAIN_SWORD,					//SWORD
	UI_STRING_MESSAGE_ENG_DOMAIN_ENCHANT,				//ENCHANT
	UI_STRING_MESSAGE_ENG_DOMAIN_GUN,					//GUN
	UI_STRING_MESSAGE_ENG_DOMAIN_HEAL,					//HEAL

	UI_STRING_MESSAGE_NOT_SUPPORT_MENU,					// This menu is not supported yet.
	UI_STRING_MESSAGE_NOT_INPUT_ID_OR_PASSWORD,			// You did not enter an ID or password.
	UI_STRING_MESSAGE_WRONG_SSN,						// The ID number is not correct.
	UI_STRING_MESSAGE_INPUT_NAME,						// Please enter a name.
	UI_STRING_MESSAGE_CANNOT_CONNECT_SERVER,			// Could not connect to the server.
	UI_STRING_MESSAGE_SELECT_CHARACTER,					// Please select a character.
	UI_STRING_MESSAGE_DELETE_CHARACTER,					// The character was deleted.
	UI_STRING_MESSAGE_ALL_SLOT_EMPTY,					// You must create a character first, then select it.
	UI_STRING_MESSAGE_NO_MORE_CREATE_CHARACTER,			// You cannot create any more characters.
	UI_STRING_MESSAGE_FAIL_CREATE_CHARACTER,			// Character creation failed.
	UI_STRING_MESSAGE_NO_INPUT_NEED_INFO,				// Not all required fields were filled in.
	UI_STRING_MESSAGE_ALREADY_USE_ID,					// That ID is already in use.
	UI_STRING_MESSAGE_CAN_USE_ID,						// That ID is available.
	UI_STRING_MESSAGE_RE_INPUT_PASSWORD,				// Please enter the password again.
	


	//Monster Name
	UI_STRING_MESSAGE_MONSTER_NAME_TNDEAD,				// Turning Dead
	UI_STRING_MESSAGE_MONSTER_NAME_ARKHAN,				// Arkhan
	UI_STRING_MESSAGE_MONSTER_NAME_ESTROIDER,		// Estroider
	UI_STRING_MESSAGE_MONSTER_NAME_GOLEMER,				// Golemer
	UI_STRING_MESSAGE_MONSTER_NAME_DARKSCREAMER,		// Dark Screamer
	UI_STRING_MESSAGE_MONSTER_NAME_DEADBODY,			// Dead Body
	UI_STRING_MESSAGE_MONSTER_NAME_MODERAS,				// Moderas
	UI_STRING_MESSAGE_MONSTER_NAME_VANDALIZER,			// Vandalizer
	UI_STRING_MESSAGE_MONSTER_NAME_DIRTYSTRIDER,		// Dirty Strider
	UI_STRING_MESSAGE_MONSTER_NAME_HELLWIZARD,			// Hell Wizard
	UI_STRING_MESSAGE_MONSTER_NAME_TNSOUL,				// Turning Soul
	UI_STRING_MESSAGE_MONSTER_NAME_IRONTEETH,			// Iron Teeth
	UI_STRING_MESSAGE_MONSTER_NAME_REDEYE,				// Red Eye
	UI_STRING_MESSAGE_MONSTER_NAME_CRIMSONSLAUGTHER,	// Crimson Slaughter
	UI_STRING_MESSAGE_MONSTER_NAME_HELLGUARDIAN,		// Hell Guardian
	UI_STRING_MESSAGE_MONSTER_NAME_SOLDIER,				// Soldier
	UI_STRING_MESSAGE_MONSTER_NAME_RIPPER,				// Ripper
	UI_STRING_MESSAGE_MONSTER_NAME_BIGFANG,				// Big Fang
	UI_STRING_MESSAGE_MONSTER_NAME_LORDCHAOS,			// Lord Chaos
	UI_STRING_MESSAGE_MONSTER_NAME_CHAOSGUARDIAN,		// Chaos Guardian
	UI_STRING_MESSAGE_MONSTER_NAME_HOBBLE,				// Hobble
	UI_STRING_MESSAGE_MONSTER_NAME_CHAOSNIGHT,			// Chaos Knight
	UI_STRING_MESSAGE_MONSTER_NAME_WIDOWS,				// Widows
	UI_STRING_MESSAGE_MONSTER_NAME_KID,					// Kid
	UI_STRING_MESSAGE_MONSTER_NAME_SHADOWWING,			// Shadow Wing

	//Server Status
	UI_STRING_MESSAGE_SERVER_STATUS_VERY_GOOD,			// Very good
	UI_STRING_MESSAGE_SERVER_STATUS_GOOD,				// Good
	UI_STRING_MESSAGE_SERVER_STATUS_NORMAL,				// Smooth
	UI_STRING_MESSAGE_SERVER_STATUS_BAD,				// Busy
	UI_STRING_MESSAGE_SERVER_STATUS_VERY_BAD,			// Full
	UI_STRING_MESSAGE_SERVER_STATUS_DOWN,				// Server down
	UI_STRING_MESSAGE_SERVER_STATUS_OPEN,				// Open
	UI_STRING_MESSAGE_SERVER_STATUS_CLOSE,				// Closed


	// ItemType
	UI_STRING_MESSAGE_ITEM_TYPE_NORMAL,					// Normal item
	UI_STRING_MESSAGE_ITEM_TYPE_SPECIAL,				// Special item
	UI_STRING_MESSAGE_ITEM_TYPE_MISTERIOUS,				// Unknown item
	
	
	// Storage
	UI_STRING_MESSAGE_STORAGE_FIRST,					// First storage box
	UI_STRING_MESSAGE_STORAGE_SECOND,					// Second storage box
	UI_STRING_MESSAGE_STORAGE_THIRD,					// Third storage box
	UI_STRING_MESSAGE_BRING_MONEY_FROM_STORAGE,			// Withdraw money.

	// Char Manager
	UI_STRING_MESSAGE_CHAR_DELETE,						// Delete.
	UI_STRING_MESSAGE_CHAR_CANCEL,						// Cancel.
	UI_STRING_MESSAGE_CHAR_DELETE_CONFIRM,				// Do you really want to delete this character?
	UI_STRING_MESSAGE_RE_INPUT_CORRECT_SSN,				// Please enter your ID number correctly.
	UI_STRING_MESSAGE_CHAR_MANAGER_NAME,					// Name : 
	UI_STRING_MESSAGE_CHAR_MANAGER_LEVEL,					// Level : 
	UI_STRING_MESSAGE_CHAR_MANAGER_STR_PURE,				// Base STR : 
	UI_STRING_MESSAGE_CHAR_MANAGER_DEX_PURE,				// Base DEX : 
	UI_STRING_MESSAGE_CHAR_MANAGER_INT_PURE,				// Base INT : 
	UI_STRING_MESSAGE_CHAR_MANAGER_SWORD_LEVEL,				// Sword domain level : 
	UI_STRING_MESSAGE_CHAR_MANAGER_BLADE_LEVEL,				// Blade domain level : 
	UI_STRING_MESSAGE_CHAR_MANAGER_GUN_LEVEL,				// Gun domain level : 
	UI_STRING_MESSAGE_CHAR_MANAGER_HEAL_LEVEL,				// Heal domain level : 
	UI_STRING_MESSAGE_CHAR_MANAGER_ENCHANT_LEVEL,			// Enchant domain level : 
	UI_STRING_MESSAGE_CHAR_MANAGER_FAME,					// Fame : 
	UI_STRING_MESSAGE_CHAR_MANAGER_CREATEMSG1,				// Press Create to make a
	UI_STRING_MESSAGE_CHAR_MANAGER_CREATEMSG2,				// new character.
	UI_STRING_MESSAGE_CHAR_MANAGER_GRADE,					// Rank : 
	UI_STRING_MESSAGE_CHAR_MANAGER_GRADE_EXP,				// Rank exp


	// Option Menu
	UI_STRING_MESSAGE_OPTION_MENU_ENTER_CHATTING,			// Enter-key chat
	UI_STRING_MESSAGE_OPTION_MENU_NORMAL_CHATTING,			// Normal chat
	UI_STRING_MESSAGE_OPTION_MENU_3D_ACCEL,					// Use 3D acceleration
	UI_STRING_MESSAGE_OPTION_MENU_ALPHA_HPBAR,				// Transparent HP(MP) bar
	UI_STRING_MESSAGE_OPTION_MENU_SHED_BLOOD,				// Bleed below 30% HP
	UI_STRING_MESSAGE_OPTION_MENU_HIDE_SOFT,				// Smooth window auto-hide
	UI_STRING_MESSAGE_OPTION_MENU_GAME_BRIGHT,				// Game brightness
	UI_STRING_MESSAGE_OPTION_MENU_CHATTING_TALK,			// Show chat in speech bubbles
	UI_STRING_MESSAGE_OPTION_MENU_PUT_FPS,					// Show FPS
	UI_STRING_MESSAGE_OPTION_MENU_WINDOW_ALPHA,				// Semi-transparent windows
	UI_STRING_MESSAGE_OPTION_MENU_DENSITY_ALPHA,			// Transparency level
	UI_STRING_MESSAGE_OPTION_MENU_SOUND_VOLUME,				// Sound effect volume
	UI_STRING_MESSAGE_OPTION_MENU_MUSIC_VOLUME,				// Background music volume
	UI_STRING_MESSAGE_OPTION_MENU_SHOW_BASIC_HELP,			// Show beginner help
	UI_STRING_MESSAGE_OPTION_MENU_NO_LISTEN_BAD_TALK,		// Filter out bad language
	UI_STRING_MESSAGE_OPTION_MENU_LOAD_ALL_IMAGE,			// Preload monster images on map change
	UI_STRING_MESSAGE_OPTION_MENU_CHATTING_COLOR_WHITE,		// Show all chat in white
	UI_STRING_MESSAGE_OPTION_MENU_RUN_TEEN_VERSION,			// Run the teen version
	UI_STRING_MESSAGE_OPTION_MENU_OPEN_WINDOW_WHEN_WHISPER,	// Open the chat window on whisper
	UI_STRING_MESSAGE_OPTION_MENU_ACCEL_NAME,				// Action : 
	UI_STRING_MESSAGE_OPTION_MENU_ACCEL_KEY,				// Shortcut : 
	UI_STRING_MESSAGE_OPTION_MENU_MSG1,						// Press the new shortcut key.
	UI_STRING_MESSAGE_OPTION_MENU_MSG2,						// Press ESC to cancel.
	UI_STRING_MESSAGE_OPTION_NOT_SEND_MY_INFO,				// Keep my character info private

	// Grade
	UI_STRING_MESSAGE_GRADE_PIVATE,								// Private
	UI_STRING_MESSAGE_GRADE_SERENT,								// Serent
	UI_STRING_MESSAGE_GRADE_FEACEL,								// Feacel
	UI_STRING_MESSAGE_GRADE_LITENA,								// Litena
	UI_STRING_MESSAGE_GRADE_KAINEL,								// Kainel
	UI_STRING_MESSAGE_GRADE_GENEAL,								// Geneal
	UI_STRING_MESSAGE_GRADE_FORE_GENEAL,						// Fore Geneal
	UI_STRING_MESSAGE_GRADE_MAJORIS_GENEAL,						// Majoris Geneal
	UI_STRING_MESSAGE_GRADE_CLOEL_GENEAL,						// Cloel Geneal
	UI_STRING_MESSAGE_GRADE_MARSHAL ,							// Marshal

	UI_STRING_MESSAGE_GRADE_RITTER,								// Ritter
	UI_STRING_MESSAGE_GRADE_REICHSRITTER,						// Reichsritter
	UI_STRING_MESSAGE_GRADE_BARONET,							// Baronet
	UI_STRING_MESSAGE_GRADE_PREYHER,							// Freiherr
	UI_STRING_MESSAGE_GRADE_GRAF,								// Graf
	UI_STRING_MESSAGE_GRADE_MARKGRAF,							// Markgraf
	UI_STRING_MESSAGE_GRADE_PFALZGRAF,							// Pfalzgraf
	UI_STRING_MESSAGE_GRADE_FURST,								// Furst
	UI_STRING_MESSAGE_GRADE_HERZOG,								// Herzog
	UI_STRING_MESSAGE_GRADE_LANDESHER,							// Landesherr



	// Client Info
	UI_STRING_MESSAGE_CLIENT_VERSION,							//Version
	UI_STRING_MESSAGE_NETMARBLE_CLIENT_VERSION,

	UI_STRING_MESSAGE_OTHER_INFO_STR_PURE,						// Base STR : 
	UI_STRING_MESSAGE_OTHER_INFO_DEX_PURE,						// Base DEX : 
	UI_STRING_MESSAGE_OTHER_INFO_INT_PURE,						// Base INT : 

	UI_STRING_MESSAGE_OTHER_INFO_STR_CUR,						// Current STR : 
	UI_STRING_MESSAGE_OTHER_INFO_DEX_CUR,						// Current DEX : 
	UI_STRING_MESSAGE_OTHER_INFO_INT_CUR,						// Current INT : 
	UI_STRING_MESSAGE_OTHER_INFO_FAME,							// Fame
	UI_STRING_MESSAGE_OTHER_INFO_TEAM_NAME,						// Team name : 
	UI_STRING_MESSAGE_OTHER_INFO_CLAN_NAME,						// Clan name : 

	UI_STRING_MESSAGE_TIP_CHANGE_PICTURE_CLICK_HERE,			// You can change your picture here

	STRING_MESSAGE_UP_TO_GRADE,									// You have been promoted.
	UI_STRING_MESSAGE_GRADE_NAME,								// Rank name :
	UI_STRING_MESSAGE_GRADE_LEVEL,								// Rank level :

	UI_STRING_MESSAGE_ENG_DOMAIN_BLADE2,						//"BLADE";
	UI_STRING_MESSAGE_ENG_DOMAIN_SWORD2,						//"SWORD";
	UI_STRING_MESSAGE_ENG_DOMAIN_ENCHANT2,						//"ENCHANT";
	UI_STRING_MESSAGE_ENG_DOMAIN_GUN2,							//"GUN";
	UI_STRING_MESSAGE_ENG_DOMAIN_HEAL2,							//"HEAL";

	UI_STRING_MESSAGE_DESC_CLASS2,								// Class
	STRING_MESSAGE_ITEM_TO_ITEM_FAIL_NO_PREMIUM_SLAYER,			// You must be a premium user, and it cannot be used inside a guild.
	STRING_MESSAGE_ITEM_TO_ITEM_FAIL_NO_PREMIUM_VAMPIRE,		// You must be a premium user, and it cannot be used inside a village.

	STRING_MESSAGE_DISMISS_AFTER_SECOND,							// You will be thrown out in %d seconds.
	UI_STRING_MESSAGE_MODIFY_INFO,							// Change the introduction text.
	
	UI_STRING_MESSAGE_START_TRACE,								// Start tracking
	UI_STRING_MESSAGE_CANCEL_TRACE,								// Cancel tracking
	UI_STRING_MESSAGE_TRACE,									// Now tracking %s.
	UI_STRING_MESSAGE_CANNOT_TRACE,								// %s could not be found.

	UI_STRING_MESSAGE_OPTION_DO_NOT_WAR_MSG,					// Hide war messages
	UI_STRING_MESSAGE_OPTION_DO_NOT_LAIR_MSG,					// Hide lair master messages

	UI_STRING_MESSAGE_SHOW_GRADE1_INFO_WINDOW,					// Open the rank skill window.
	UI_STRING_MESSAGE_SHOW_GRADE2_INFO_WINDOW,					// Open the rank skill window.

	UI_STRING_MESSAGE_INCREASE_CRITICAL_10,						// Critical damage +10
	UI_STRING_MESSAGE_INCREASE_DEFENSE_5,						// Defense +5
	UI_STRING_MESSAGE_INCREASE_TOHIT_5,							// To hit +5
	UI_STRING_MESSAGE_INCREASE_HP_10,							// HP +10
	UI_STRING_MESSAGE_INCREASE_MP_15,							// MP +15
	UI_STRING_MESSAGE_INCREASE_DAMAGE_3,						// Damage +3
	UI_STRING_MESSAGE_INCREASE_ATTACKSPEED_15,					// Attack speed +15
	UI_STRING_MESSAGE_INCREASE_HP_20,							// HP +20
	UI_STRING_MESSAGE_INCREASE_DEFENSE_10,						// Defense +10
	UI_STRING_MESSAGE_INCREASE_PROTECTION_10,					// Protection +10
	UI_STRING_MESSAGE_DECREASE_HP_EXHAUSTION_10_PERCENT,		// HP cost of skills -10%
	UI_STRING_MESSAGE_INCREASE_ENCHANT_DAMAGE_10_PERCENT,		// Enchant attack damage +10%
	UI_STRING_MESSAGE_INCREASE_HEAL_DAMAGE_10_PERCENT,			// Heal attack damage +10%
	UI_STRING_MESSAGE_INCREASE_NEAR_ATTACK_DAMAGE_10_PERCENT,	// Melee skill damage +10%
	UI_STRING_MESSAGE_INCREASE_HP_RESTORE_SPEED_15_PERCENT,		// HP recovery speed +15%
	UI_STRING_MESSAGE_INCREASE_RESISTANCE_ACID_15_PERCENT,		// Acid resistance +15%
	UI_STRING_MESSAGE_INCREASE_RESISTANCE_BLOODY_15_PERCENT,	// Bloody resistance +15%
	UI_STRING_MESSAGE_INCREASE_RESISTANCE_CURSE_15_PERCENT,		// Curse resistance +15%
	UI_STRING_MESSAGE_INCREASE_RESISTANCE_POISON_15_PERCENT,	// Poison resistance +15%
	UI_STRING_MESSAGE_INCREASE_DAMAGE_STORM_20_PERCENT,			// Storm skill damage +20%
	UI_STRING_MESSAGE_INCREASE_RANGE_STORM_5_BY_5,				// Storm skill range increased to 5*5
	UI_STRING_MESSAGE_INCREASE_SUCCESS_RATIO_POISON_10_PERCENT,	// Poison success rate +10%
	UI_STRING_MESSAGE_INCREASE_SUCCESS_RATIO_ACID_10_PERCENT,	// Acid success rate +10%
	UI_STRING_MESSAGE_INCREASE_SUCCESS_RATIO_CURSE_10_PERCENT,	// Curse success rate +10%
	UI_STRING_MESSAGE_INCREASE_SUCCESS_RATIO_BLOODY_10_PERCENT,	// Bloody success rate +10%
	UI_STRING_MESSAGE_INCREASE_SUCCESS_RATIO_INNATE_10_PERCENT,	// Innate success rate +10%
	UI_STRING_MESSAGE_INCREASE_SUCCESS_RATIO_SUMMON_10_PERCENT,	// Summon success rate +10%
	UI_STRING_MESSAGE_DECREASE_MP_EXHAUSTION_10_PERCENT,		// MP cost of skills -10%
	UI_STRING_MESSAGE_INCREASE_HP_STEAL_2_PERCENT,				// HP steal +2%
	UI_STRING_MESSAGE_INCREASE_MP_STEAL_2_PERCENT,				// MP steal +2%
	UI_STRING_MESSAGE_INCREASE_LUCKY_2,							// Lucky +2
	UI_STRING_MESSAGE_INCREASE_KEEP_TIME_ACID_SWAMP_20_PERCENT,	// Acid Swamp duration +20%
	UI_STRING_MESSAGE_INCREASE_KEEP_TIME_PARALYZE_20_PERCENT,	// Paralyze duration +20%
	UI_STRING_MESSAGE_INCREASE_KEEP_TIME_DARKNESS_30_PERCENT,	// Darkness duration +30%
	UI_STRING_MESSAGE_INCREASE_RANGE_DARKNESS_5_BY_5,			// Darkness range increased to 5*5
	UI_STRING_MESSAGE_CANNOT_UP_GRADE,							// Your rank cannot go any higher.

	STRING_MESSAGE_SELECT_PC_CANNOT_PLAY,						// You cannot play because this is not a paid account.
	STRING_MESSAGE_SELECT_PC_NOT_BILLING_CHECK,					// Your payment has not been confirmed yet. Please wait a moment.
	STRING_MESSAGE_SELECT_PC_CANNOT_PLAY_BY_ATTR,				// This character has reached the free service limit, so a paid subscription is required to log in.

	UI_STRING_MESSAGE_NEVER_CANNOT_LEARN_SKILL,					// This skill cannot be learned.
	UI_STRING_MESSAGE_ALREADY_LEARNED_SKILL,					// You have already learned this skill.
	UI_STRING_MESSAGE_CANNOT_LEARN_SKILL_YET,					// You cannot learn this yet.
	UI_STRING_MESSAGE_CAN_LEARN_SKILL_NOW,						// You can learn this now.
	UI_STRING_MESSAGE_ACCELERATOR_GRADE1,
	UI_STRING_MESSAGE_ACCELERATOR_GRADE2,
	UI_STRING_MESSAGE_LEARN_GRADE_SKILL_CONFIRM,				// Learning this skill locks you out of the other skills of your current rank. Learn it anyway?

	UI_STRING_MESSAGE_TREE_OK,									// Use the tree.
	UI_STRING_MESSAGE_TREE_CANCEL,								// Do not use the tree.

    STRING_MESSAGE_TRADE_GIFT_BOX_OK,             // Merry Christmas!
    STRING_MESSAGE_TRADE_GIFT_BOX_NO_ITEM,        // Come back once a friend has given you a gift!
    STRING_MESSAGE_TRADE_GIFT_BOX_ALREADY_TRADE,  // You greedy thing, off with you!

	STRING_MESSAGE_XMAS_TREE_CANNOT_USE,			// This is too close to another tree.
	STRING_MESSAGE_XMAS_CARD_CANNOT_USE,			// Some fields are still empty. Please fill them all in.
	STRING_MESSAGE_NOT_USE_SAFETY_POSITION,			// This cannot be used in a safety zone.
	STRING_MESSAGE_NOT_USE_SAFETY_ZONE,				// This cannot be used inside a village or a guild.

	UI_STRING_MESSAGE_ENCHANT_CONFIRM_2,					// The item may be destroyed if it fails.

	UI_STRING_MESSAGE_DEPOSIT_LIMIT,				// You cannot store more than 2 billion in the storage box.
	UI_STRING_MESSAGE_WITHDRAW_LIMIT,				// You cannot carry more than 2 billion.

	// 2003.1.14
	STRING_MESSAGE_EVENT_PREMIUM_HALF_SLAYER,		// Ampoules are half price in this zone.
	STRING_MESSAGE_EVENT_PREMIUM_HALF_VAMPIRE,		// Serum is half price in this zone.
	STRING_MESSAGE_EVENT_PREMIUM_HALF_SLAYER_END,	// The half-price ampoule event has ended.
	STRING_MESSAGE_EVENT_PREMIUM_HALF_VAMPIRE_END,	// The half-price serum event has ended.

	STRING_MESSAGE_REWARD_OK,						// You received your reward.
	STRING_MESSAGE_REWARD_FAIL,						// You cannot receive the reward.
    STRING_MESSAGE_NO_EMPTY_SLOT,					// There is no free slot.

	UI_STRING_MESSAGE_OPTION_DO_NOT_HOLY_LAND_MSG,	// Hide Adam's Holy Land messages
	UI_STRING_MESSAGE_HOLY_LAND_TOTAL_FEE,			// Total tax
	UI_STRING_MESSAGE_HOLY_LAND_CAN_BRING_FEE,		// Tax to collect
	UI_STRING_MESSAGE_HOLY_LAND_TOTAL_FEE_DESC,		// Total tax available to collect
	UI_STRING_MESSAGE_HOLY_LAND_INPUT_BRING_FEE,	// Enter the tax you want to collect
	UI_STRING_MESSAGE_HOLY_LAND_CLICK_INPUT_FEE,	// Click to enter the tax you want to collect
	UI_STRING_MESSAGE_HOLY_LAND_OK,					// Collect the tax you entered
	
	STRING_MESSAGE_NO_TEAM,							// You do not belong to a team.
	STRING_MESSAGE_NO_CLAN,							// You do not belong to a clan.
	STRING_MESSAGE_NOT_TEAM_MASTER,					// You are not the team master.
	STRING_MESSAGE_NOT_CLAN_MASTER,					// You are not the clan master.
	STRING_MESSAGE_TEAM_HAS_NO_CASTLE,				// Your team does not hold a castle.
	STRING_MESSAGE_CLAN_HAS_NO_CASTLE,				// Your clan does not hold a castle.
	STRING_MESSAGE_TEAM_NOT_YOUR_CASTLE,			// This castle does not belong to your team.
	STRING_MESSAGE_CLAN_NOT_YOUR_CASTLE,			// This castle does not belong to your clan.

	STRING_MESSAGE_SUCCESS_BRING_FEE,				// You collected the tax.
	STRING_MESSAGE_FAIL_BRING_FEE,					// Collecting the tax failed.
	
	UI_STRING_MESSAGE_BRING_FEE_MSG,				// The team or clan master that owns this castle can collect the tax it earns. Press the button to the right of the tax field to take up to the full amount. The tax you collect plus the money you already carry cannot exceed 2 billion.

	UI_STRING_MESSAGE_BRING_FEE_LIMIT,				// The tax you collect plus the money you carry cannot exceed 2 billion.
	UI_STRING_MESSAGE_RACE_WAR,						// Race War
	
	STRING_MESSAGE_WAR_SCHEDULE_FULL,						// The war schedule is full.
	STRING_MESSAGE_WAR_ALREADY_REGISTERED,					// You have already applied for a war.
	STRING_MESSAGE_WAR_NOT_ENOUGH_MONEY,					// You do not have enough money.
	STRING_MESSAGE_WAR_REGISTRATION_OK,						// You have been added to the war schedule.

	STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_BLOOD_BIBLE,		// You cannot enter your own safety zone while carrying the Blood Bible!
	STRING_MESSAGE_ALREADY_HAS_CASTLE,						// You already hold a castle.
	STRING_MESSAGE_WAR_UNAVAILABLE,							// War applications are not being accepted right now.

	UI_STRING_MESSAGE_STATUS_TIME_FORMAT,					// %dh %dm
	UI_STRING_MESSAGE_STATUS_LEFT_TIME,						// Time left
	UI_STRING_MESSAGE_GUILD_WAR,							// Guild War

	STRING_MESSAGE_BLOOD_BIBLE_BONUS_ARMEGA,				// MP(HP) cost of skills -50%
	STRING_MESSAGE_BLOOD_BIBLE_BONUS_MIHOLE,				// Lucky +10
	STRING_MESSAGE_BLOOD_BIBLE_BONUS_KIRO,					// INT+5, DEX +5  
	STRING_MESSAGE_BLOOD_BIBLE_BONUS_INI,					// Physical attack damage +10
	STRING_MESSAGE_BLOOD_BIBLE_BONUS_GREGORI,				// Sight +5, all stats +4
	STRING_MESSAGE_BLOOD_BIBLE_BONUS_CONCILIA,				// HP +50  
	STRING_MESSAGE_BLOOD_BIBLE_BONUS_LEGIOS,				// Magic attack damage +10
	STRING_MESSAGE_BLOOD_BIBLE_BONUS_HILLEL,				// Translates other races' speech
	STRING_MESSAGE_BLOOD_BIBLE_BONUS_JAVE,					// Gambling costs halved
	STRING_MESSAGE_BLOOD_BIBLE_BONUS_NEMA,					// Potions cost halved
	STRING_MESSAGE_BLOOD_BIBLE_BONUS_AROSA,					// HP +50
	STRING_MESSAGE_BLOOD_BIBLE_BONUS_CHASPA,				// STR +5, INT +5


	UI_STRING_MESSAGE_BLOOD_BIBLE_ARMEGA,				// Armega
	UI_STRING_MESSAGE_BLOOD_BIBLE_MIHOLE,				// Mihole
	UI_STRING_MESSAGE_BLOOD_BIBLE_KIRO,					// Kiro
	UI_STRING_MESSAGE_BLOOD_BIBLE_INI,					// Ini
	UI_STRING_MESSAGE_BLOOD_BIBLE_GREGORI,				// Gregori
	UI_STRING_MESSAGE_BLOOD_BIBLE_CONCILIA,				// Concilia
	UI_STRING_MESSAGE_BLOOD_BIBLE_LEGIOS,				// Legios
	UI_STRING_MESSAGE_BLOOD_BIBLE_HILLEL,				// Hillel
	UI_STRING_MESSAGE_BLOOD_BIBLE_JAVE,					// Jave
	UI_STRING_MESSAGE_BLOOD_BIBLE_NEMA,					// Nema
	UI_STRING_MESSAGE_BLOOD_BIBLE_AROSA,				// Arosa
	UI_STRING_MESSAGE_BLOOD_BIBLE_CHASPA,				// Chaspa


	UI_STRING_MESSAGE_BLOOD_BIBLE_ARMEGA_ENG,				// ARMEGA
	UI_STRING_MESSAGE_BLOOD_BIBLE_MIHOLE_ENG,				// MIHOLE
	UI_STRING_MESSAGE_BLOOD_BIBLE_KIRO_ENG,					// KIRO
	UI_STRING_MESSAGE_BLOOD_BIBLE_INI_ENG,					// INI
	UI_STRING_MESSAGE_BLOOD_BIBLE_GREGORI_ENG,				// GREGORI
	UI_STRING_MESSAGE_BLOOD_BIBLE_CONCILIA_ENG,				// CONCILIA
	UI_STRING_MESSAGE_BLOOD_BIBLE_LEGIOS_ENG,				// LEGIOS
	UI_STRING_MESSAGE_BLOOD_BIBLE_HILLEL_ENG,				// HILLEL
	UI_STRING_MESSAGE_BLOOD_BIBLE_JAVE_ENG,					// JAVE
	UI_STRING_MESSAGE_BLOOD_BIBLE_NEMA_ENG,					// NEMA
	UI_STRING_MESSAGE_BLOOD_BIBLE_AROSA_ENG,				// AROSA
	UI_STRING_MESSAGE_BLOOD_BIBLE_CHASPA_ENG,				// CHASPA

	UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_DROP,				// Lying on the ground
	UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_HAS_SLAYER,		// Held by a slayer
	UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_HAS_VAMPIRE,		// Held by a vampire
	UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_NONE,				// No information available
	UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_SLAYER,			// In the slayers' shrine
	UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_VAMPIRE,			// In the vampires' shrine
	
	UI_STRING_MESSAGE_BLOOD_BIBLE_DESC_POSITION,			// Location : 
	UI_STRING_MESSAGE_BLOOD_BIBLE_DESC_STATUS,				// Status : 
	UI_STRING_MESSAGE_BLOOD_BIBLE_DESC_PLAYER,				// Carried by : 

	UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_ATTACK_GUILD,		// Attacking guild
	UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_DEFENSE_GUILD,		// Defending guild
	
	STRING_MESSAGE_RACE_WAR_JOIN_FAILED,					// The race war roster for %s's level range is full.
	STRING_MESSAGE_RACE_WAR_JOIN_OK,						// You have signed up for the race war.

	UI_STRING_MESSAGE_BLOOD_BIBLE_DESC_SHRINE_RACE,			// Last owner : 

	STRING_MESSAGE_SLAYER,									// Slayer
	STRING_MESSAGE_VAMPIRE,									// Vampire

	STRING_MESSAGE_RACE_WAR_GO_FIRST_SERVER,				// You can only sign up for and join the race war on the first server of each world.

	STRING_MESSAGE_GIVE_EVENT_ITEM_FAIL_NOW,				// You cannot receive the event item right now.
	STRING_MESSAGE_GIVE_EVENT_ITEM_FAIL,					// You cannot receive the event item.
	STRING_MESSAGE_GIVE_EVENT_ITEM_OK,						// You received the event item.
	STRING_MESSAGE_GIVE_PREMIUM_USER_ONLY,					// Only premium service users can receive this.

	STRING_MESSAGE_COUPLE_MEET_SUCCESS,						// You are now a couple.
	STRING_MESSAGE_COUPLE_CANNOT_MEET,						// You cannot become a couple.
	STRING_MESSAGE_MEET_WAIT_TIME_EXPIRED,					// The request timed out and was cancelled.

	STRING_MESSAGE_COUPLE_APART_SUCCESS,					// You have broken up.
	STRING_MESSAGE_NOT_COUPLE,								// You are not a couple, so you cannot break up.
	STRING_MESSAGE_APART_WAIT_TIME_EXPIRED,					// The request timed out and was cancelled.

	UI_STRING_MESSAGE_HOPE_COUPLE_MSG,						// Becoming a couple takes the consent of both partners. Enter the name of the one you wish to pair with; if they come to me and agree within one minute, the bond is sealed. Now, please enter the name of the one you love.
	UI_STRING_MESSAGE_BREAK_UP_COUPLE_MSG,					// A mutual parting takes the consent of both partners. Enter the name of the one you wish to part from; if they come to me and agree within one minute, the parting is sealed. The couple ring, the token of your love, disappears with it. Now, please enter your partner's name.
	UI_STRING_MESSAGE_COMPLETE,						// Done
	UI_STRING_MESSAGE_OTHER_PLAYER_NAME,					// The other character's name

	UI_STRING_MESSAGE_FORCE_BREAK_UP_COUPLE,				// A one-sided parting needs no consent, but it costs you a large part of your alignment. Enter your partner's name and the parting is sealed. The couple ring, the token of your love, disappears with it. Now, please enter your partner's name.

	STRING_MESSAGE_COUPLE,									// Couple
	STRING_MESSAGE_MOVE_DELAY_SEC,							// You will be moved in %d seconds.
	STRING_MESSAGE_COUPLE_MOVE_START,						// Looking for where your partner is.
	STRING_MESSAGE_COUPLE_CAN_NOT_FIND,						// Your partner could not be found.

	STRING_MESSAGE_COUPLE_NOT_EVENT_TERM,			// The couple event is not running.
	STRING_MESSAGE_COUPLE_ALREADY_WAITING,				// You are already waiting for someone.
	STRING_MESSAGE_COUPLE_LOGOFF,						// The other player is not logged in.
	STRING_MESSAGE_COUPLE_DIFFERENT_RACE,				// You are of different races.
	STRING_MESSAGE_COUPLE_SAME_SEX,					// Only a man and a woman can become a couple.
	STRING_MESSAGE_COUPLE_NOT_PAY_PLAYER,				// You are not a paying user.
	STRING_MESSAGE_COUPLE_ALREADY_COUPLE,				// You are already in a couple.
	STRING_MESSAGE_COUPLE_WAS_COUPLE,					// You have been in a couple before.
	STRING_MESSAGE_COUPLE_NOT_ENOUGH_GOLD,				// You do not have enough money.
	STRING_MESSAGE_COUPLE_NOT_ENOUGH_ATTR,				// Your stats are too low.
	STRING_MESSAGE_COUPLE_NOT_ENOUGH_LEVEL,			// Your level is too low.
	STRING_MESSAGE_COUPLE_INVENTORY_FULL,				// There is no room for the couple ring.
	STRING_MESSAGE_COUPLE_NO_WAITING,					// No partner is waiting for you.
	STRING_MESSAGE_COUPLE_NOT_COUPLE,					// You are not in a couple.

	UI_STRING_MESSAGE_LOVE_CHAIN,						// A skill that moves you to where your partner is.
	UI_STRING_MESSAGE_WILL_YOU_GO_BILING_PAGE,			// Go to the payment page?
	UI_STRING_MESSAGE_GO_BILING_PAGE,					// Go to the payment page.
	STRING_MESSAGE_LOGOUT_BY_PAYTIME,					// Your paid play time has expired.
	STRING_MESSAGE_LOGOUT_BY_FREEPLAY_LEVEL,			// Your stats have reached the free service limit.
	STRING_MESSAGE_LOGOUT_BY_LEVEL,						// Your level has reached the free service limit.
	
	UI_STRING_MESSAGE_CANNOT_UP_LEVEL_BY_FAME,			// Your fame is too low to level up.
	UI_STRING_MESSAGE_NEED_FAME,						// Fame required

	
	UI_STRING_MESSAGE_QUEST_HAN,						// Quest
	UI_STRING_MESSAGE_QUEST_ENG,						// Quest
	UI_STRING_MESSAGE_DAY,								// %dd
	UI_STRING_MESSAGE_HOUR,								// %dh
	UI_STRING_MESSAGE_MINUTE,							// %dm
	UI_STRING_MESSAGE_SECOND,							// %ds
	UI_STRING_MESSAGE_LEFT_TIME,						// Time left :
	UI_STRING_MESSAGE_EXPIRED_ITEM,						// This disappears when you log out.
	
	UI_STRING_MESSAGE_SELECT_QUEST_SLAYER,				// E.V.E. has issued new orders in response to the rapidly growing vampire population. Pick a hunt and wipe out the number of vampires it names. You may only run one hunt at a time, and logging out cancels it. The number of vampires depends on the hunt you choose, and E.V.E. pays a special reward for finishing one. Choose the hunt you want.
	UI_STRING_MESSAGE_SELECT_QUEST_VAMPIRE,				// Will you take on the work I have for you? Hah... call it a hobby of mine, but I cannot abide the imperfect. For the purity and nobility of our vampire blood, the lesser ones must be cleared away. So, will you help me clear them out? You may only run one hunt at a time, and logging out cancels it. The number varies with the kind of creature, so just go and hunt them down. The pay is generous. Choose your hunt.
	UI_STRING_MESSAGE_CANCEL_SELECT_QUEST,				// I will take part another time.
	UI_STRING_MESSAGE_CANCEL_LEARN_SKILL,				// I will learn it next time.
	UI_STRING_MESSAGE_NUMBER_OF_ANIMALS,				// 

	UI_STRING_MESSAGE_EXPIRED_TIME_MONSTER_KILL_QUEST,			// The monster hunt quest has run out of time.
	UI_STRING_MESSAGE_FINISH_MONSTER_KILL_QUEST,				// You have hunted every target monster.
	UI_STRING_MESSAGE_CAN_REQUITAL_FROM_NPC,					// You can claim your reward from the NPC.
	UI_STRING_MESSAGE_START_MONSTER_KILL_QUEST,					// The monster hunt quest has begun.
	UI_STRING_MESSAGE_FAIL_ALREADY_START_MONSTER_KILL_QUEST,	// A quest is already in progress.
	UI_STRING_MESSAGE_FAIL_MONSTER_KILL_QUEST_BY_STATUS,		// Your stats do not qualify you for this quest.
	UI_STRING_MESSAGE_SUCCESS_MONSTER_KILL_QUEST,				// You completed the monster hunt quest.
	UI_STRING_MESSAGE_NOT_YET_COMPLETE_MONSTER_KILL_QUEST,		// The monster hunt quest is not finished yet.
	UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST,		// There is no room in your inventory.
	UI_STRING_MESSAGE_NOT_IN_QUEST,								// You are not on a quest.
	UI_STRING_MESSAGE_FAIL_QUEST_EXPIRED_TIME,					// You ran out of time and failed the quest.
	UI_STRING_MESSAGE_TIME_LIMIT,								// Time limit :
	UI_STRING_MESSAGE_CANNOT_APPLY_QUEST,						// You cannot take a quest right now.
	
	UI_STRING_MESSAGE_COMPLETE_QUEST,
	UI_STRING_MESSAGE_FAIL_TIME_OVER_QUEST,
	UI_STRING_MESSAGE_MEET_NPC,
	
	UI_STRING_MESSAGE_FAIL_INVALID_NPC,
	UI_STRING_MESSAGE_FAIL_BUG,
	
	UI_STRING_MESSAGE_GATHER_ITEM,								// Find %s
	STRING_MESSAGE_MONSTER_KILL_QUEST_STRING_SET,				// Then bring me %s, %d of them, within %s.
	STRING_MESSAGE_MONSTER_KILL_QUEST_STRING_SET_VAMPIRE,		// Then bring me %s, %d of them, within %s.
	STRING_MESSAGE_CANCEL_MONSTER_KILL_QUEST,					// I will check my gear and come back.
	STRING_MESSAGE_YES_I_SEE,									// Yes, understood.

	
	UI_STRING_MESSAGE_QUEST_MONSTER_KILL,						// Monster hunt quest
	UI_STRING_MESSAGE_QUEST_MEET_NPC,							// Meet an NPC quest
	UI_STRING_MESSAGE_QUEST_GATHER_ITEM,						// Gather items quest
	
	UI_STRING_MESSAGE_QUEST_DESCRIPTION_TIME_TOTAL,				// Total time limit : 
	UI_STRING_MESSAGE_QUEST_DESCRIPTION_TIME_ELAPSE,			// Time elapsed : 
	UI_STRING_MESSAGE_QUEST_DESCRIPTION_TIME_REMAIN,			// Time remaining : 
	UI_STRING_MESSAGE_QUEST_DESCRIPTION_TIME_NO_REMAIN,			// None
	UI_STRING_MESSAGE_FAIL_QUEST,								// Quest failed
	UI_STRING_MESSAGE_SELECT_EVENT_GIFT,						// Choose one of the stage %d event prizes
	UI_STRING_MESSAGE_PLEASE_SCRATCH_IMAGE,						// Please scratch the picture above.
	UI_STRING_MESSAGE_WIN_A_PRIZE_SAME_IMAGE,					// Three matching pictures wins a prize.
	UI_STRING_MESSAGE_CONGRATULATIONS,							// Congratulations.
	UI_STRING_MESSAGE_WIN_A_PRIZE,								//  has won.

	STRING_MESSAGE_MEET_NPC_SLAYER,								// A resident living near Eslania has reported seeing a strangely featured person, thought to be an Ousters. Find the witness at once and get as much information out of them as you can.
	STRING_MESSAGE_MEET_NPC_VAMPIRE,							// Two residents who claim to have seen a strangely featured person, thought to be an Ousters, are wandering near Limbo Lair without a care. Find them at once and get as much information out of them as you can.

	STRING_MESSAGE_CANCEL_QUEST_VAMPIRE,						// I will replenish my magic and come back.

	UI_STRING_MESSAGE_FAIL_LOTTERY,
	UI_STRING_MESSAGE_WIN_A_PRIZE2,								//  has won a prize.
	UI_STRING_MESSAGE_USER_1,									// 

	STRING_MESSAGE_RIPATY_SCRIPT_1,
	STRING_MESSAGE_RIPATY_SCRIPT_2,
	STRING_MESSAGE_RIPATY_SCRIPT_3,
	STRING_MESSAGE_RIPATY_SCRIPT_4,
	STRING_MESSAGE_RIPATY_SCRIPT_5,
	STRING_MESSAGE_RIPATY_SCRIPT_6,
	STRING_MESSAGE_RIPATY_SCRIPT_7,
	STRING_MESSAGE_RIPATY_SCRIPT_8,
	STRING_MESSAGE_RIPATY_SCRIPT_9,

	STRING_MESSAGE_AMATA_SCRIPT_1,
	STRING_MESSAGE_AMATA_SCRIPT_2,
	STRING_MESSAGE_AMATA_SCRIPT_3,
	STRING_MESSAGE_AMATA_SCRIPT_4,
	STRING_MESSAGE_AMATA_SCRIPT_5,
	STRING_MESSAGE_AMATA_SCRIPT_6,
	STRING_MESSAGE_AMATA_SCRIPT_7,
	STRING_MESSAGE_AMATA_SCRIPT_8,
	STRING_MESSAGE_AMATA_SCRIPT_9,
	
	UI_STRING_MESSAGE_QUEST_NAME_FIND_ANCIENT_DOCUMENT,					// Find the ancient document quest
	UI_STRING_MESSAGE_QUEST_NAME_FIND_ANCIENT_MAP,						// Find the ancient map quest
	UI_STRING_MESSAGE_QUEST_NAME_FIND_SOUL_STONE,						// Find the spirit stone quest

	UI_STRING_MESSAGE_NOT_IN_QUEST2,							// You have no quest in progress.
//	UI_STRING_MESSAGE_EVENT_QUEST_2,
//	UI_STRING_MESSAGE_EVENT_QUEST_3,
//	UI_STRING_MESSAGE_EVENT_QUEST_4,
//	UI_STRING_MESSAGE_EVENT_QUEST_5,
	
	UI_STRING_MESSAGE_TAKE_OUT_OK,							// The item was moved to your inventory.
	UI_STRING_MESSAGE_TAKE_OUT_FAIL,						// The item could not be moved to your inventory.
	STRING_MESSAGE_CANNOT_TAKE_OUT_ITEM_FROM_SHOP,			// The item you bought cannot be collected right now.
	UI_STRING_MESSAGE_CONFIRM_SELECT_ITEM_FROM_SHOP,		// Take the item you selected?
	UI_STRING_MESSAGE_ITEM_SHOP,							// Click an item name to take that item.

	STRING_MESSAGE_CLEAR_RANK_BONUS_OK,           // The rank skill you selected was removed.
    STRING_MESSAGE_NO_RANK_BONUS,                 // You do not qualify for that.
    STRING_MESSAGE_ALREADY_CLEAR_RANK_BONUS,      // You have already removed that rank skill once.

	UI_STRING_MESSAGE_BULLETIN_BOARD_OK,					// Use the bulletin board.
	UI_STRING_MESSAGE_BULLETIN_BOARD_CANCEL,				// Do not use the bulletin board.

	STRING_MESSAGE_BULLETIN_BOARD_CANNOT_USE,				// This is too close to another bulletin board.
	UI_STRING_MESSAGE_CAN_TRANS,
	
	UI_STRING_MESSAGE_TRANS_ITEM,
	UI_STRING_MESSAGE_RESURRECT_BY_ELIXIR,
	UI_STRING_MESSAGE_RESURRECT_BY_SCROLL,
	UI_STRING_CANNOT_USE,

	UI_STRING_MESSAGE_MIXING_FORGE_OK,							// Use the mixing forge.
	UI_STRING_MESSAGE_MIXING_FORGE_CANCEL,						// Do not use the mixing forge.

	UI_STRING_MESSAGE_COMPLETE_MERGE_ITEM,						// The two items were merged successfully.
	UI_STRING_MESSAGE_FAIL_MERGE_ITEM,							// Merging the items failed.
	STRING_MESSAGE_USE_GUILD_MEMBER_ONLY,						// Only members of the guild that owns the castle can use this.
	STRING_MESSAGE_CANNOT_USE_RIDE_MOTORCYCLE,					// You cannot use this while riding a motorcycle.
	UI_STRING_MESSAGE_CANNOT_MIXING_SPECIAL_ITEM,				// Unique and limited items cannot be mixed.

	UI_STRING_MESSAGE_CANNOT_MIXING_OPTION_COUNT,				// Only items with exactly one option can be mixed.
	UI_STRING_MESSAGE_CANNOT_MIXING_ITEM_CLASS,					// The items are of different classes, so they cannot be mixed.
	UI_STRING_MESSAGE_CANNOT_MIXING_ITEM_TYPE,					// The items are of different types, so they cannot be mixed.
	UI_STRING_MESSAGE_CANNOT_MIXING_ITEM_OPTION,				// Both items have the same option, so you cannot select it.
	
	UI_STRING_MESSAGE_WHAT_OPTION_REMOVE,						// Which option do you want to remove?
	UI_STRING_MESSAGE_CONFIRM_REMOVE_OPTION,					// Do you really want to remove the option you selected?
	UI_STRING_MESSAGE_CAN_REMOVE_OPTION,						// This item's option can be removed.
	UI_STRING_MESSGAE_CANNOT_REMOVE_OPTION,						// This item's option cannot be removed.

	UI_STRING_MESSAGE_SELECT_OPTION,							// the %s option
	UI_STRING_MESSAGE_QUESTION_REMOVE_OPTION,					// Remove it?
	STRING_MESSAGE_FAILED_REMOVE_OPTION,						// Removing the option failed.
	STRING_MESSAGE_SUCCESS_REMOVE_OPTION,						// The option you selected was removed.

	UI_STRING_MESSAGE_DESC_OTHER_TRIBE,					// Other races only
	UI_STRING_MESSAGE_CONSUME_EP,						// EP cost : 

	UI_STRING_MESSAGE_EQUIP_OUSTERS_CIRCLET,			// Wear a circlet.
	UI_STRING_MESSAGE_EQUIP_OUSTERS_COAT,				// Wear a coat.
	UI_STRING_MESSAGE_EQUIP_OUSTERS_WEAPON,				// Equip a chakram/wristlet.
	UI_STRING_MESSAGE_EQUIP_OUSTERS_BOOTS,				// Wear boots.
	UI_STRING_MESSAGE_EQUIP_OUSTERS_ARMSBAND,			// Wear an armsband.
	UI_STRING_MESSAGE_EQUIP_OUSTERS_RING,				// Wear a ring.
	UI_STRING_MESSAGE_EQUIP_OUSTERS_PENDENT,			// Wear a pendant.
	UI_STRING_MESSAGE_EQUIP_OUSTERS_STONE,				// Equip a spirit stone.

	UI_STRING_MESSAGE_OUSTERS_STONE,				// %s spirit level :
	
	UI_STRING_MESSAGE_GRADE_MALCHUT,					// Malchut
	UI_STRING_MESSAGE_GRADE_YESOD,						// Yesod
	UI_STRING_MESSAGE_GRADE_HOD,						// Hod
	UI_STRING_MESSAGE_GRADE_NETRETH,					// Netzach
	UI_STRING_MESSAGE_GRADE_TIPHRETH,					// Tiphereth
	UI_STRING_MESSAGE_GRADE_GEBURAH,					// Geburah
	UI_STRING_MESSAGE_GRADE_CHESED,						// Chesed
	UI_STRING_MESSAGE_GRADE_BINAH,						// Binah
	UI_STRING_MESSAGE_GRADE_CHOKMA,						// Chokmah
	UI_STRING_MESSAGE_GRADE_KEATHER,					// Kether

	UI_STRING_MESSAGE_SKILL_LEVEL,						// Skill Level : %d
	UI_STRING_MESSAGE_REQUIRE_ELEMENTAL_LEVEL,			// Required element level (%s)(%d)
	UI_STRING_MESSAGE_LEARN_SKILL,						// Learn this skill?
	UI_STRING_MESSAGE_LEARN_SKILL2,						// Learning this skill fixes your line. From the next skill on you can only learn skills from this line.
	UI_STRING_MESSAGE_SKILL_LEVEL_UP,					// Raise the level of this skill?
	UI_STRING_MESSAGE_REQUIRE_SKILL_POINT,				// Skill points required : %d Point

	UI_STRING_MESSAGE_ELEMENTAL_FIRE,					// Fire
	UI_STRING_MESSAGE_ELEMENTAL_WATER,					// Water
	UI_STRING_MESSAGE_ELEMENTAL_EARTH,					// Earth
	UI_STRING_MESSAGE_ELEMENTAL_WIND,					// Wind
	UI_STRING_MESSAGE_ELEMENTAL_SUM,					// Total
	
	UI_STRING_MESSAGE_EP,								// Spirit

	UI_STRING_MESSAGE_NOT_SHOW_GUILD_CHATTING,				// Hide guild chat
	UI_STRING_MESSAGE_SHOW_GUILD_CHATTING,					// Show guild chat
	UI_STRING_MESSAGE_GUILD_CHATTING,						// Guild chat
	
	UI_STRING_MESSAGE_RANK_BONUS_WOOD_SKIN,					// Protection +15
    UI_STRING_MESSAGE_RANK_BONUS_WIND_SENSE,				// Defense +10
    UI_STRING_MESSAGE_RANK_BONUS_HOMING_EYE,				// To Hit +10
	UI_STRING_MESSAGE_RANK_BONUS_LIFE_ENERGY,				// HP +15
	UI_STRING_MESSAGE_RANK_BONUS_SOUL_ENERGY,				// EP +25
	UI_STRING_MESSAGE_RANK_BONUS_STONE_MAUL,				// Combat skill damage +5
	UI_STRING_MESSAGE_RANK_BONUS_SWIFT_ARM,					// Attack speed +20
	UI_STRING_MESSAGE_RANK_BONUS_FIRE_ENDOW,				// Fire attack magic damage +3
	UI_STRING_MESSAGE_RANK_BONUS_WATER_ENDOW,				// Water attack magic damage +3
	UI_STRING_MESSAGE_RANK_BONUS_EARTH_ENDOW,				// Earth attack magic damage +3
	UI_STRING_MESSAGE_RANK_BONUS_ANTI_ACID_SKIN,			// Acid resistance +15%
	UI_STRING_MESSAGE_RANK_BONUS_ANTI_BLOODY_SKIN,			// Bloody resistance +15%
	UI_STRING_MESSAGE_RANK_BONUS_ANTI_CURSE_SKIN,			// Curse resistance +15%
	UI_STRING_MESSAGE_RANK_BONUS_ANTI_POISON_SKIN,			// Poison resistance +15%
	UI_STRING_MESSAGE_RANK_BONUS_ANTI_SILVER_DAMAGE_SKIN,	// Silver damage -20%
	UI_STRING_MESSAGE_RANK_BONUS_BLESS_OF_NATURE,			// EP cost of skills -20%
	UI_STRING_MESSAGE_RANK_BONUS_LIFE_ABSORB,				// HP steal +2%
	UI_STRING_MESSAGE_RANK_BONUS_SOUL_ABSORB,				// EP steal +2%
	UI_STRING_MESSAGE_RANK_BONUS_MYSTIC_RULE,				// Lucky +2

	UI_STRING_MESSAGE_HPBAR_EP_DESCRIPTION,					// "EP:%d/%d"

	UI_STRING_MESSAGE_LEFT_BONUS_POINT,						// You still have bonus points to spend.

	UI_STRING_MESSAGE_GUILD,								// Guild
	UI_STRING_MESSAGE_GUILD_INFO,							// Guild Info
	UI_STRING_MESSAGE_GUILD_MEMBER_LIST,					// Guild Member List
	UI_STRING_MESSAGE_GUILD_HELP,							// Guild Help
	
	UI_STRING_MESSAGE_ELEMENTAL_FIRE_DESCRIPTION,			// Fire:%d";
	UI_STRING_MESSAGE_ELEMENTAL_WATER_DESCRIPTION,			// Water:%d";
	UI_STRING_MESSAGE_ELEMENTAL_EARTH_DESCRIPTION,			// Earth:%d";
	UI_STRING_MESSAGE_ELEMENTAL_WIND_DESCRIPTION,			// Wind:%d";
	
	UI_STRING_MESSAGE_DESC_EP,								// EP :

	UI_STRING_MESSAGE_PREV_MAP,								// Show the previous map.
	UI_STRING_MESSAGE_NEXT_MAP,								// Show the next map.
	UI_STRING_MESSAGE_CLOSE_HORN,							// Put away the horn of the earth spirit.
	
	UI_STRING_MESSAGE_DESC_ATTACK_SPEED,					// Attack speed : 

	STRING_STATUS_EP_MAX_1,									// Your maximum EP is now %d.
	STRING_STATUS_EP_MAX_2,									// Your maximum EP is now %d.
	UI_STRING_MESSAGE_DESC_MAGIC_DAMAGE,					// Magic damage :
	
	STRING_MESSAGE_OPEN_LAIR,								// %s has opened.
	STRING_MESSAGE_CLOSED_LAIR,								// %s has closed.
	STRING_MESSAGE_LEFT_TIME_LAIR,							// %s stays open for another %d minutes.
	
	STRING_MESSAGE_CONTRACT_GNOMES_HORN,					// You must go to Sioram and sign a contract before you can use this.
	STRING_MESSAGE_CONTRACT_GNOMES_HORN_OK,					// You signed the contract to use the horn of the earth spirit.
	
	STRING_MESSAGE_CANNOT_DOWN_SKILL,						// The skill level cannot be lowered.
	STRING_MESSAGE_NOT_ENOUGH_MONEY_FOR_DOWN_SKILL,			// You do not have enough money to lower the skill level.
	UI_STRING_MESSAGE_CONFIRM_DOWN_SKILL,					// The level of %s will change from %d to %d. It costs $%s. Change it?
	STRING_MESSAGE_SUCCESS_CHANGE,							// The change was successful.
	UI_STRING_MESSAGE_WITHDRAW_POINT,						// Skill points refunded : %d
	UI_STRING_MESSAGE_DESC_DOWN_SKILL,						// Lower the level of this skill?
	STRING_MESSAGE_NOT_OUSTERS,								// You are not an Ousters.
	STRING_MESSAGE_TOO_LOW_SKILL_LEVEL,						// The skill level is too low.
	STRING_MESSAGE_TOO_HIGH_SKILL_LEVEL,					// The skill level is too high.
	STRING_MESSAGE_INVALID_SKILL,							// That skill is not valid.
	STRING_MESSAGE_NOT_LEARNED_SKILL,						// You have not learned that skill yet.
	UI_STRING_MESSAGE_CONFIRM_UP_TO_LAST_SKILL_LEVEL,			// Once a skill reaches its maximum level of 30, its points can no longer be refunded. Raise it to level 30?
	STRING_MESSAGE_CANNOT_USE_OUSTERS,						// Ousters cannot use this.
	STRING_MESSAGE_MIXING_FORGE_FAILED_SAME_OPTION_GROUP,	// The items share an option group, so they cannot be mixed.
	UI_STRING_MESSAGE_CONFIRM_CHANGE_SEX,					// Do you really want to change gender?
	STRING_MESSAGE_CANNOT_CHANGE_SEX_BY_WEAR,				// You cannot change gender while wearing clothes.
	STRING_MESSAGE_CANNOT_CHANGE_SEX_BY_COUPLE,				// A character in a couple cannot change gender.

	UI_STRING_MESSAGE_EVENT_QUEST2_1,						// 1. Judgement of Wisdom\nGoal: solve the puzzle you are given
	UI_STRING_MESSAGE_EVENT_QUEST2_2,						// 2. Key of the Barrier\nGoal: collect the ores set for your level (8 of them)
	UI_STRING_MESSAGE_EVENT_QUEST2_3,						// 3. The Invisible Wall\nGoal: cross the maze and obtain Rifinium
	UI_STRING_MESSAGE_EVENT_QUEST2_4,						// 4. The Talking Doll\nGoal: find and assemble the puzzle pieces in the given dungeon
	UI_STRING_MESSAGE_EVENT_QUEST2_5,						// 5. Gate to the Future\nGoal: find mana stones on monsters of a certain class and assemble the code table

	STRING_MESSAGE_SELECT_MINI_GAME,						// There are two trials prepared. Choose one of them.
	STRING_MESSAGE_SELECT_ARROW_TILES,						// Arrow Tiles. Follow the arrows and reach the goal safely.
	STRING_MESSAGE_SELECT_CRAZY_MINE,						// Crazy Mine. A puzzle game mixing number baseball with minesweeping.

	STRING_MESSAGE_GET_RIFINIUM,							// You obtained Rifinium.
	UI_STRING_MESSAGE_QUEST_STATUS_ARROW_TILES,				// Arrow Tiles
	UI_STRING_MESSAGE_QUEST_STATUS_CRAZY_MINE,				// Crazy Mine
	UI_STRING_MESSAGE_SUCCESS_MINIGAME,						// "All Stage Clear!"

	STRING_MESSAGE_GUILD_REGIST_FAIL_ALREADY_JOIN,		// Let me see... it says here you already belong to the %s guild.
	STRING_MESSAGE_GUILD_REGIST_FAIL_QUIT_TIMEOUT,		// You left your last guild only moments ago. Think it over before you act.
	STRING_MESSAGE_GUILD_REGIST_FAIL_CANCEL_TIMEOUT,		// Your guild was disbanded only moments ago. Train until you meet the requirements, and wait for a better opening.
	STRING_MESSAGE_GUILD_REGIST_FAIL_LEVEL,				// You are capable, but not yet leader material. Come back when you are stronger.
	STRING_MESSAGE_GUILD_REGIST_FAIL_MONEY,				// Founding a guild takes a great deal of money, and you do not seem to have it.
	STRING_MESSAGE_GUILD_REGIST_FAIL_FAME,				// %s, is it... I have never heard that name. That makes you a novice. Come back when you have made a name for yourself.
	STRING_MESSAGE_GUILD_REGIST_FAIL_NAME,				// That guild name is already taken. Think of another one.
	STRING_MESSAGE_GUILD_REGIST_FAIL_DENY,				// Your application was rejected.

	STRING_MESSAGE_GUILD_STARTING_FAIL_ALREADY_JOIN,		// You already belong to another guild.
	STRING_MESSAGE_GUILD_STARTING_FAIL_QUIT_TIMEOUT,		// You left your last guild only moments ago. Think it over before you act.
	STRING_MESSAGE_GUILD_STARTING_FAIL_CANCEL_TIMEOUT,	// Your guild was disbanded only moments ago. Train until you meet the requirements, and wait for a better opening.
	STRING_MESSAGE_GUILD_STARTING_FAIL_LEVEL,				// You still have a lot to learn. Come back when you have trained a while longer.
	STRING_MESSAGE_GUILD_STARTING_FAIL_MONEY,				// %s, registering a guild costs more money than that.
	STRING_MESSAGE_GUILD_STARTING_FAIL_FAME,				// %s, is it... I have never heard that name. That makes you a novice. Come back when you have made a name for yourself.
	
	UI_STRING_MESSAGE_CLICK_TO_SHOW_DETIAL,					// Click to see the details.
	
	STRING_MESSAGE_OUSTERS,									// Ousters
	
	STRING_MESSAGE_EVENT_FLAG_WAR_READY,					// Capture the Flag starts in 5 minutes!
	STRING_MESSAGE_EVENT_FLAG_WAR_START,					// Capture the Flag has begun!
	STRING_MESSAGE_EVENT_FLAG_WAR_FINISH,					// Capture the Flag is over. Items drop in 3 minutes.
	STRING_MESSAGE_EVENT_FLAG_WAR_WINNER,					// The %s gathered %d flags and won.
	STRING_MESSAGE_EVENT_FLAG_WILL_POUR_ITEM_AFTER_3MIN,	// Items burst out at the flag area in 3 minutes.
	STRING_MESSAGE_EVENT_FLAG_POURED_ITEM,					// The Capture the Flag event items have appeared.

	UI_STRING_MESSAGE_GET_EVENT_FLAG_STATUS,				// Flags captured : %d
	
	UI_STRING_MESSAGE_MUTE,									// A GM has muted your chat.
	STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_FLAG,			// You cannot enter a safety zone while carrying a flag.
	STRING_MESSAGE_CANNOT_DROP_ITEM_BY_FLAG,				// You cannot drop items near the flagpole.
	STRING_MESSAGE_POUR_ITEM_AFTER_SECOND,					// Items burst out at the flag area in %d seconds.

	UI_STRING_MESSAGE_CANNOT_ACTION_MOTORCYCLE_FLAG,		// You cannot mount or dismount a motorcycle near the flagpole.
	
	UI_STRING_MESSAGE_TEAM_INFO_GUILD_NAME,					// Guild Name : %s"
	UI_STRING_MESSAGE_TEAM_INFO_GUILD_INTRODUCTION,			// Guild Introduction : "
	UI_STRING_MESSAGE_OTHER_INFO_GUILD_NAME,				// Guild name : 
	UI_STRING_MESSAGE_NOT_JOIN_ANY_GUILD,					// No guild registered
	
	UI_STRING_MESSAGE_USE_ETERNITY_FOR_RESURRECT,			// You are revived by the Eternity skill.
	UI_STRING_MESSAGE_CANNOT_RELOAD_BY_VIVID_MAGAZINE,		// You must learn VIVID MAGAZINE before you can load this.
	STRING_MESSAGE_RESURRECT_AFTER_SECONDS,					// You will resurrect in %d seconds.
	
	STRING_MESSAGE_CANNOT_TRADE_SUMMON_SYLPH,				// You cannot trade while riding the wind spirit.
	STRING_MESSAGE_CANNOT_USE_SUMMON_SYLPH,					// You cannot use this while riding the wind spirit.
	
	STRING_MESSAGE_MODIFY_SKILL_LEVEL_1,					// Your %s skill level is now %d.
	STRING_MESSAGE_MODIFY_SKILL_LEVEL_2,					// Your %s skill level is now %d.

	// 2003.09.29
	UI_STRING_MESSAGE_GO_TO_BEGINNER_ZONE,					// Move to the beginners' hunting ground.
	UI_STRING_MESSAGE_LEFT_PREMIUM_DAYS,					// %d days left on your premium service.
	UI_STRING_MESSAGE_NOT_PREMIUM_USER,						// You are not a premium service user.
	UI_STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_TODAY,			// Your premium service ends today.

	STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_MESSAGE_1,		// Renew before your premium service ends and you keep your mileage as well as
	STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_MESSAGE_2,		// item lottery tickets and other benefits.
	STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_MESSAGE_3,		// See the Dark Eden homepage (www.darkeden.com) for details.

	STRING_MESSAGE_LEVEL_WAR_ZONE_NAME,						// Caligo Dungeon %dF

	UI_STRING_MESSAGE_SWEEPER_BONUS_1,						// All stats + 2
	UI_STRING_MESSAGE_SWEEPER_BONUS_2,						// HP + 20
	UI_STRING_MESSAGE_SWEEPER_BONUS_3,						// Damage + 3
	UI_STRING_MESSAGE_SWEEPER_BONUS_4,						// INT + 7, DEX + 7
	UI_STRING_MESSAGE_SWEEPER_BONUS_5,						// HP + 50
	UI_STRING_MESSAGE_SWEEPER_BONUS_6,						// Lucky + 7
	UI_STRING_MESSAGE_SWEEPER_BONUS_7,						// Magic attack damage + 10
	UI_STRING_MESSAGE_SWEEPER_BONUS_8,						// INT + 7, STR + 7
	UI_STRING_MESSAGE_SWEEPER_BONUS_9,						// HP steal + 15
	UI_STRING_MESSAGE_SWEEPER_BONUS_10,						// Physical attack damage + 10
	UI_STRING_MESSAGE_SWEEPER_BONUS_11,						// All resistances + 7
	UI_STRING_MESSAGE_SWEEPER_BONUS_12,						// Sight + 5, all stats + 4

	UI_STRING_MESSAGE_JOIN_LEVEL_WAR,						// Join the war.
	
	UI_STRING_MESSAGE_CLICK_TO_WARP_REGEN_TOWER,			// Click to move to that regen zone tower.

	UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_HAS_OUSTERS,		// Held by an Ousters
	UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_OUSTERS,			// In the Ousters' shrine

	UI_STRING_MESSAGE_MAILBOX,									// Mail Box
	UI_STRING_MESSAGE_CLOSE_MAILBOX,							// Close the mail box.
	UI_STRING_MESSAGE_MAILBOX_TAB_MAIL,							// Check your mail.
	UI_STRING_MESSAGE_MAILBOX_TAB_HELP,							// Check the help.
	UI_STRING_MESSAGE_MAILBOX_TAB_MEMO,							// Check your notes.

	STRING_MESSAGE_SUCCESS_CHANGED_BAT_COLOR,				// The bat's color has changed.

	UI_STRING_MESSAGE_LEVEL_WAR,							// Level War
	
	UI_STRING_MESSAGE_LEFT_FAMILY_DAYS,						// %d days left on your family service.
	UI_STRING_MESSAGE_EXPIRE_FAMILY_TODAY,					// Your family service ends today.

	UI_STRING_MESSAGE_TOO_FAR,								// You need to get closer.

	UI_STRING_MESSAGE_PET_DESC_DURABILITY,					// Food remaining : 
	UI_STRING_MESSAGE_PET_ATTR,								// Attribute : 
	
	UI_STRING_MESSAGE_SHOW_PET_INFO_WINDOW,					// Open the pet info window
	
	STRING_MESSAGE_ENCHANT_FAIL,							// The enchant failed.
	UI_STRING_MESSAGE_PET_DESC_DURABILITY_2,				// Food remaining

	UI_STRING_MESSAGE_PET_RESSURECT,						// Revive your pet?
	UI_STRING_MESSAGE_CLOSE_PET_INFO,						// Close the pet info window
	
	UI_STRING_MESSAGE_CAN_ENCHANT_PET,						// You can train your pet.
	UI_STRING_MESSAGE_CANNOT_ENCHANT_PET,					// You can only train your pet after it levels up.

	STRING_MESSAGE_NEW_PET_LEVEL_1,							// %s is now level %d.
	STRING_MESSAGE_NEW_PET_LEVEL_2,							// %s is now level %d.

	STRING_MESSAGE_OPTION_NAME_LUCK_3,						// Lucky
	STRING_MESSAGE_OPTION_NAME_LUCK_4,						// Minion

	STRING_MESSAGE_OPTION_NAME_ATTR_3,						// Nut
	STRING_MESSAGE_OPTION_NAME_ATTR_4,						// Crunch

	STRING_MESSAGE_PET_DIE_WARNING,							// %s has only %s worth of food left.
	STRING_MESSAGE_PET_REQUEST_REFILL,						// Please top up its food.
	STRING_MESSAGE_PET_DIE,									// %s has died.
	STRING_MESSAGE_PET_SUMMON,								// You summoned %s.
	STRING_MESSAGE_PET_CAN_GET_ATTR,						// You can give your pet an attribute.
	STRING_MESSAGE_PET_GAMBLE_OK,							// You gave %s a secondary ability.
	STRING_MESSAGE_PET_GAMBLE_DESC,							// You can loot monster heads along with items.
	STRING_MESSAGE_PET_GAMBLE_DESC_TEEN,					// You can loot soul stones along with items.
	STRING_MESSAGE_PET_CAN_GET_OPTION,						// You can give %s an option.
	UI_STRING_MESSAGE_HPBAR_EXP_DESCRIPTION_NEW,			// Exp remaining : %s (%s%%)
	STRING_MESSAGE_PET_GAMBLE_FAIL,							// Granting the secondary ability failed.

	STRING_MESSAGE_OPTION_ENAME_LUCK_3,						// Lucky
	STRING_MESSAGE_OPTION_ENAME_LUCK_4,						// Minion

	STRING_MESSAGE_OPTION_ENAME_ATTR_3,						// Nut
	STRING_MESSAGE_OPTION_ENAME_ATTR_4,						// Crunch

	UI_STRING_MESSAGE_CAN_PET_REVIVAL,						// This can be revived.
	UI_STRING_MESSAGE_CANNOT_PET_REVIVAL,					// This can only be used when the pet is dead.

	UI_STRING_MESSAGE_PET_CAN_CUT_HEAD,						// Has a secondary ability
	UI_STRING_MESSAGE_PET_CANNOT_CUT_HEAD,					// No secondary ability
	
	UI_STRING_MESSAGE_REMOVE_PET_OPTION,					// Remove your pet's option?

	STRING_MESSAGE_CANNOT_EVENT_GIFT_BOX,

	STRING_MESSAGE_EVENT_NETMARBLE_1,
	STRING_MESSAGE_EVENT_NETMARBLE_2,
	STRING_MESSAGE_EVENT_NETMARBLE_3,
	STRING_MESSAGE_EVENT_NETMARBLE_4,
	
	UI_STRING_MESSAGE_CAN_USE_PET_FOOD,
	UI_STRING_MESSAGE_DETACH_PET_FOOD,
	UI_STRING_MESSAGE_USE_PET_FOOD,
	UI_STRING_MESSAGE_CANNOT_DETACH,

	UI_STRING_MESSAGE_COMPUTER_STRING_0,
	UI_STRING_MESSAGE_COMPUTER_STRING_1,
	UI_STRING_MESSAGE_COMPUTER_STRING_2,
	UI_STRING_MESSAGE_COMPUTER_STRING_3,
	UI_STRING_MESSAGE_COMPUTER_STRING_4,
	UI_STRING_MESSAGE_COMPUTER_STRING_5,
	UI_STRING_MESSAGE_COMPUTER_STRING_6,
	UI_STRING_MESSAGE_COMPUTER_STRING_7,
	UI_STRING_MESSAGE_COMPUTER_STRING_8,
	UI_STRING_MESSAGE_COMPUTER_STRING_9,
	UI_STRING_MESSAGE_COMPUTER_STRING_10,
	UI_STRING_MESSAGE_COMPUTER_STRING_11,
	UI_STRING_MESSAGE_COMPUTER_STRING_12,
	UI_STRING_MESSAGE_COMPUTER_STRING_13,
	UI_STRING_MESSAGE_COMPUTER_STRING_14,
	UI_STRING_MESSAGE_COMPUTER_STRING_15,
	UI_STRING_MESSAGE_COMPUTER_STRING_16,
	UI_STRING_MESSAGE_COMPUTER_STRING_17,
	UI_STRING_MESSAGE_COMPUTER_STRING_18,
	UI_STRING_MESSAGE_COMPUTER_STRING_19,
	UI_STRING_MESSAGE_COMPUTER_STRING_20,
	UI_STRING_MESSAGE_COMPUTER_STRING_21,
	UI_STRING_MESSAGE_COMPUTER_STRING_22,
	UI_STRING_MESSAGE_COMPUTER_STRING_23,
	UI_STRING_MESSAGE_COMPUTER_STRING_24,
	UI_STRING_MESSAGE_COMPUTER_STRING_25,
	UI_STRING_MESSAGE_COMPUTER_STRING_26,
	UI_STRING_MESSAGE_COMPUTER_STRING_27,
	UI_STRING_MESSAGE_COMPUTER_STRING_28,
	UI_STRING_MESSAGE_COMPUTER_STRING_29,

	UI_STRING_MESSAGE_BOOK_NAME_0,
	UI_STRING_MESSAGE_BOOK_NAME_1,
	UI_STRING_MESSAGE_BOOK_NAME_2,
	UI_STRING_MESSAGE_BOOK_NAME_3,
	UI_STRING_MESSAGE_BOOK_NAME_4,
	UI_STRING_MESSAGE_BOOK_NAME_5,
	UI_STRING_MESSAGE_BOOK_NAME_6,
	UI_STRING_MESSAGE_BOOK_NAME_7,
	UI_STRING_MESSAGE_BOOK_NAME_8,
	UI_STRING_MESSAGE_BOOK_NAME_9,
	UI_STRING_MESSAGE_BOOK_NAME_10,
	UI_STRING_MESSAGE_BOOK_NAME_11,
	UI_STRING_MESSAGE_BOOK_NAME_12,
	UI_STRING_MESSAGE_BOOK_NAME_13,
	UI_STRING_MESSAGE_BOOK_NAME_14,
	UI_STRING_MESSAGE_BOOK_NAME_15,
	UI_STRING_MESSAGE_BOOK_NAME_16,
	UI_STRING_MESSAGE_BOOK_NAME_17,
	UI_STRING_MESSAGE_BOOK_NAME_18,
	UI_STRING_MESSAGE_BOOK_NAME_19,
	UI_STRING_MESSAGE_BOOK_NAME_20,

	//Monster Name
	UI_STRING_MESSAGE_MONSTER_ENAME_TNDEAD,				// TNDEAD
	UI_STRING_MESSAGE_MONSTER_ENAME_ARKHAN,				// ARKHAN
	UI_STRING_MESSAGE_MONSTER_ENAME_ESTROIDER,		// ESTROIDER
	UI_STRING_MESSAGE_MONSTER_ENAME_GOLEMER,				// GOLEMER
	UI_STRING_MESSAGE_MONSTER_ENAME_DARKSCREAMER,		// DARKSCREAMER
	UI_STRING_MESSAGE_MONSTER_ENAME_DEADBODY,			// DEADBODY
	UI_STRING_MESSAGE_MONSTER_ENAME_MODERAS,				// MODERAS
	UI_STRING_MESSAGE_MONSTER_ENAME_VANDALIZER,			// VANDALIZER
	UI_STRING_MESSAGE_MONSTER_ENAME_DIRTYSTRIDER,		// DIRTYSTRIDER
	UI_STRING_MESSAGE_MONSTER_ENAME_HELLWIZARD,			// HELLWIZARD
	UI_STRING_MESSAGE_MONSTER_ENAME_TNSOUL,				// TNSOUL
	UI_STRING_MESSAGE_MONSTER_ENAME_IRONTEETH,			// IRONTEETH
	UI_STRING_MESSAGE_MONSTER_ENAME_REDEYE,				// REDEYE
	UI_STRING_MESSAGE_MONSTER_ENAME_CRIMSONSLAUGTHER,	// CRIMSONSLAUGTHER
	UI_STRING_MESSAGE_MONSTER_ENAME_HELLGUARDIAN,		// HELLGUARDIAN
	UI_STRING_MESSAGE_MONSTER_ENAME_SOLDIER,				// SOLDIER
	UI_STRING_MESSAGE_MONSTER_ENAME_RIPPER,				// RIPPER
	UI_STRING_MESSAGE_MONSTER_ENAME_BIGFANG,				// BIGFANG
	UI_STRING_MESSAGE_MONSTER_ENAME_LORDCHAOS,			// LORDCHAOS
	UI_STRING_MESSAGE_MONSTER_ENAME_CHAOSGUARDIAN,		// CHAOSGUARDIAN
	UI_STRING_MESSAGE_MONSTER_ENAME_HOBBLE,				// HOBBLE
	UI_STRING_MESSAGE_MONSTER_ENAME_CHAOSNIGHT,			// CHAOSNIGHT
	UI_STRING_MESSAGE_MONSTER_ENAME_WIDOWS,				// WIDOWS
	UI_STRING_MESSAGE_MONSTER_ENAME_KID,					// KID
	UI_STRING_MESSAGE_MONSTER_ENAME_SHADOWWING,			// SHADOWWING

	UI_STRING_MESSAGE_HAN_DOMAIN_BLADE, // Blade
	UI_STRING_MESSAGE_HAN_DOMAIN_SWORD, // Sword
	UI_STRING_MESSAGE_HAN_DOMAIN_ENCHANT, // Enchant
	UI_STRING_MESSAGE_HAN_DOMAIN_GUN, // Gun
	UI_STRING_MESSAGE_HAN_DOMAIN_HEAL, // Heal
	UI_STRING_MESSAGE_HAN_DOMAIN_ETC, // Etc
	UI_STRING_MESSAGE_HAN_DOMAIN_VAMPIRE, // Vampire
	UI_STRING_MESSAGE_HAN_DOMAIN_OUSTERS, // Ousters
	
	UI_STRING_MESSAGE_ENG_DOMAIN_ETC, // ]="Etc";
	UI_STRING_MESSAGE_ENG_DOMAIN_VAMPIRE, // ]="Vampire";
	UI_STRING_MESSAGE_ENG_DOMAIN_OUSTERS, // ]="Ousters";
	
	STRING_MESSAGE_SOUL_STONE,
	
	UI_STRING_MESSAGE_CAN_PET_MUTANT,	// This can be transformed.
	UI_STRING_MESSAGE_CANNOT_PET_MUTANT,	// This cannot be transformed.
	UI_STRING_MESSAGE_PET_MUTANT,

	// 2004, 03, 26 sobeit add start
	STRING_ERROR_CANNOT_AUTHORIZE_BILLING,   // Your billing information could not be found.
    STRING_ERROR_CANNOT_CREATE_PC_BILLING,    // You cannot create a character because this is not a paid account.
	// 2004, 03, 26 sobeit add end
	
	UI_STRING_MESSAGE_DESC_PET_DEAD_DAY,	// Dead for %d days

	// 2004, 04, 3 sobeit add start
	UI_STRING_MESSAGE_DESC_LUCKY,	// Lucky : 
	// 2004, 04, 3 sobeit add end

	// 2004, 4, 13 sobeit add start
	UI_STRING_MESSAGE_PET_QUEST_NPC_SLAYER, // Gruber
	UI_STRING_MESSAGE_PET_QUEST_NPC_VAMPIRE, // Kapatini
	UI_STRING_MESSAGE_PET_QUEST_NPC_OUSTERS,// Amata

	UI_STRING_MESSAGE_PET_QUEST_SLAYER,		// Centauro quest / Goal: catch (%d) (%s) within %d minutes
	UI_STRING_MESSAGE_PET_QUEST_VAMPIRE,	// Sturge quest / Goal: catch (%d) (%s) within %d minutes
	UI_STRING_MESSAGE_PET_QUEST_OUSTERS,	// Pixie quest / Goal: catch (%d) (%s) within %d minutes
	UI_STRING_MESSAGE_PET_QUEST_CLEAR,		// Return to the quest giver.

	UI_STRING_MESSAGE_CAN_3RD_ENCHANT_PET, // You can grant a third ability.
	UI_STRING_MESSAGE_CANNOT_3RD_ENCHANT_PET, // You cannot grant a third ability yet.
	STRING_MESSAGE_PET_3RD_GAMBLE_FAIL,// Granting the third ability failed.
	STRING_MESSAGE_PET_3RD_GAMBLE_OK,// You gave %s a third ability.
	// 2004, 4, 13 sobeit add end
	
	UI_STRING_MESSAGE_CANNOT_SUMMON_2ND_PET, // You cannot summon a second-tier pet below level 40.
	STRING_MESSAGE_SEARCHING_MINE,			// Mines to find : %d   Mines flagged : %d
	UI_STRING_MESSAGE_CANNOT_MIXING_GRADE_VALUE, // Items more than two grades apart cannot be mixed.

	// 2004, 4, 29 sobeit add start
	STRING_MESSAGE_MINIGAME_GAME_OVER,	//"game over";
	STRING_MESSAGE_MINIGAME_ALL_STAGE_CLEAR, // "All stage clear";
	// 2004, 4, 29 sobeit add end

	// 2004, 5, 6, sobeit add start
	UI_STRING_MESSAGE_SELECT_QUEST_OUSTERS, // Monster numbers have grown sharply of late, and the barrier weakens because of them. For our people to rise again we must cull these monsters and hold the barrier. The task is void if you log out partway through, so take care. Bring back the number of monsters the quest names and I will give you a fine reward. So, which quest will you take?
	// 2004, 5, 6, sobeit add end
	// 2004, 6, 22 By Csm 
	STRING_ERROR_CHILDGUARD_DENYED, // "User who age under 18 will not able to play after 22.00 O'clock.";
	// end 
	STRING_MESSAGE_KEEP_PETITEM,// Store your pet?
	STRING_MESSAGE_GET_KEEP_PETITEM,// Collect your stored pet?
	
	STRING_MESSAGE_EXIST_ITEM_ALREADY, // There is already another item there.

	UI_STRING_MESSAGE_ITEM_GRADE , //"  ⅠⅡⅢⅣⅤⅥⅦⅧⅨⅩ";
	UI_STRING_MESSAGE_ITEM_GRADE_DESC, // Grade :

	UI_STRING_MESSAGE_SMS_SEND_OK,// The message was sent.
	UI_STRING_MESSAGE_SMS_SEND_FAIL,// The message could not be sent.
	UI_STRING_MESSAGE_SMS_ADD_FAIL,// Could not add it to the list.
	UI_STRING_MESSAGE_SMS_DELETE_FAIL,// Could not delete it from the list.
	
	UI_STRING_MESSAGE_SMS_FAIL_MAX_NUM_EXCEEDED,// You have run out of storable numbers.
	UI_STRING_MESSAGE_SMS_FAIL_INVALID_DATA,// That information is not valid.
	UI_STRING_MESSAGE_SMS_FAIL_NO_SUCH_EID,// The information could not be found.
	UI_STRING_MESSAGE_SMS_FAIL_NOT_ENOUGH_CHARGE,// You do not have enough credit.
	UI_STRING_MESSAGE_SMS_WINDOW,//"SMS(SMS Widow)";
	UI_STRING_HELP_SMS_SEND,// Send the message.
	UI_STRING_HELP_SMS_VIEW_LIST,// Open the phone book.
	UI_STRING_HELP_SMS_WINDOW,// Pick a special character.
	UI_STRING_HELP_SMS_ADDSEND,// Add the selected number to the send list.
	UI_STRING_HELP_SMS_DELETE,// Delete the selected number.
	UI_STRING_HELP_SMS_NEW,// Register a new number.

	UI_STRING_MESSAGE_USE_SMSITEM, // Use the SMS item?
	UI_STRING_MESSAGE_NAMING_WINDOW, //"NAMING(NAMING Window)";
	UI_STRING_MESSAGE_CHANGE_PET_NICKNAME,// Change your pet's nickname.
	UI_STRING_MESSAGE_CHANGE_PLAYER_NICKNAME,// Change your free-choice nickname.
	UI_STRING_MESSAGE_ADD_PLAYER_NICKNAME,// Add a nickname.
	UI_STRING_MESSAGE_SELECT_PLAYER_NICKNAME,// Switch to the selected nickname.
	UI_STRING_MESSAGE_NICKNAME_CHANGE_OK, // Your nickname was changed.
	UI_STRING_MESSAGE_NICKNAME_SELECT_FAIL_FORCED,// A nickname assigned by a GM cannot be changed.
	UI_STRING_MESSAGE_USE_NAMINGITEM, // Use the naming item?

	UI_STRING_MESSAGE_PET_NAMING_SUMMON,// You can only rename a pet while it is summoned.
	UI_STRING_MESSAGE_PET_NAMING_WOLVERINE,// A wolverine can be renamed without a pen item.
	UI_STRING_MESSAGE_FAIL_SEARCH_ITEM,// The item could not be found.

	UI_STRING_CORE_ZAP_BLACK,// Physical attack +%d
	UI_STRING_CORE_ZAP_RED,// Magic attack +%d
	UI_STRING_CORE_ZAP_BLUE,// Physical defense +%d
	UI_STRING_CORE_ZAP_GREEN,// Magic defense +%d

	UI_STRING_CORE_ZAP_REWARD_ALL_STAT,// All stats +%d (4 Set)
	UI_STRING_CORE_ZAP_REWARD_ALL_REG,// All resistances +%d (4 Set)
	// 2004, 7,1 sobeit add start

	UI_STRING_MESSAGE_DESC_DIALOG_OPTION_EMPTY2, //"\t       %s";
	UI_STRING_MESSAGE_DESC_DIALOG_OPTION2, //         Option : %s

	UI_STRING_GQUEST_CAN_NOT,// Unavailable
	UI_STRING_GQUEST_CAN_ACCEPT,// Available
	UI_STRING_GQUEST_DOING,// In progress
	UI_STRING_GQUEST_SUCCESS,// Success
	UI_STRING_GQUEST_COMPLETE,// Complete
	UI_STRING_GQUEST_FAIL,// Failed
	UI_STRING_GQUEST_CAN_REPLAY,// Repeatable
	UI_STRING_GQUEST_MISSION,// Mission %d:%s

	UI_STRING_GQUEST_BUTTON_ACCEPT,// Take the quest.
	UI_STRING_GQUEST_BUTTON_GIVEUP,// Give up the quest.
	UI_STRING_GQUEST_TAB_PROCESS,// Show the quests in progress.
	UI_STRING_GQUEST_TAB_COMPLETE,// Show the completed quests.

	UI_STRING_NOTICE_EVENT_GOLD_MEDALS,// You have collected %d gold medals.
	STRING_ERROR_KEY_EXPIRED,// The authentication key has expired. Please log in again from the web.
	STRING_ERROR_NOT_FOUND_KEY,// The authentication key could not be found. Please log in again from the web.

	UI_STRING_GQUEST_UPDATE, // The quest information was updated.

	UI_STRING_CHANGE_EVENTITEM_PRICE,// The event item price changed to %d.
	
	STRING_MESSAGE_TOO_MANY_GUILD_REGISTERED,// Too many guilds have applied for the siege, so you cannot apply.
	STRING_MESSAGE_REINFORCE_DENYED,// Your application to join the defenders was already refused, so you cannot apply again.
	STRING_MESSAGE_ALREADY_REINFORCE_ACCEPTED,// A guild has already been accepted to join the defenders, so you cannot apply.
	STRING_MESSAGE_NO_WAR_REGISTERED,// No guild has applied for the siege, so you cannot apply to defend.
	STRING_MESSAGE_CANNOT_ACCEPT,// You cannot accept the application to join the defenders.
	STRING_MESSAGE_ACCEPT_OK,// You accepted the application to join.
	
	STRING_MESSAGE_CANNOT_DENY,							// You cannot refuse the application to join the defenders.
    STRING_MESSAGE_DENY_OK,                               // You refused the application to join.

	STRING_MESSAGE_SIEGE_POTAL_200,// Move to the trap zone
	STRING_MESSAGE_SIEGE_POTAL_201,// Move to the inner castle gate
	STRING_MESSAGE_SIEGE_POTAL_202,// Move inside the inner castle
	STRING_MESSAGE_SIEGE_POTAL_203,// Move inside the inner castle
	STRING_MESSAGE_LOGIN_ERROR_NONPK, // Your character's level is too high to log in here. Please use another server.

	STRING_MESSAGE_DAUM_EVENT_1,// The Dark Eden beginner event is here! We are giving new players a small gift along with a lottery ticket. Use the tickets you earn to claim one of the many prizes on offer, and more await at every new level. Collect the whole set of event items to try out some high-performance gear. The event runs in eight parts, handed out as you level. Press Ctrl + Q for the quest window. Enjoy your time in Dark Eden, and do not miss the chance to grow your character and collect gifts along the way.
	STRING_MESSAGE_DAUM_EVENT_2,// The Dark Eden beginner event! You have reached level %d, so a %s box has been added for you. Press Ctrl + Q to check it. If you still have an earlier box, you can claim its items too. The items we hand out are time-limited: they are stronger than the usual level-restricted gear, and because they come from the event they cannot be traded or exchanged. Once you find something better you can sell them to an NPC. The time remaining is shown under each item, and it keeps counting down even while you are away from Dark Eden, so use it well. Enjoy the rest of your stay.
	STRING_MESSAGE_DAUM_EVENT_3,// You have reached level %d, and a gift box has been added for you. Press Ctrl + Q to check it. If you still have an earlier box, you can claim its items too.
	STRING_MESSAGE_DAUM_EVENT_BOX1,// red
	STRING_MESSAGE_DAUM_EVENT_BOX2,// orange
	STRING_MESSAGE_DAUM_EVENT_BOX3,// yellow
	STRING_MESSAGE_DAUM_EVENT_BOX4,// green
	STRING_MESSAGE_DAUM_EVENT_BOX5,// blue
	STRING_MESSAGE_DAUM_EVENT_BOX6,// indigo
	STRING_MESSAGE_DAUM_EVENT_BOX7,// violet
	STRING_MESSAGE_DAUM_EVENT_BOX8,// black
		
	UI_STRING_MESSAGE_RANK_BONUS_PERCEPTION,// All stats +2
    UI_STRING_MESSAGE_RANK_BONUS_STONE_OF_SAGE,// INT +5
    UI_STRING_MESSAGE_RANK_BONUS_FOOT_OF_RANGER,// DEX +5
    UI_STRING_MESSAGE_RANK_BONUS_WARRIORS_FIST,// STR +5

    UI_STRING_MESSAGE_RANK_BONUS_ACID_INQUIRY,// Acid resistance +10%
    UI_STRING_MESSAGE_RANK_BONUS_BLOODY_INQUIRY,// Blood resistance +10%
    UI_STRING_MESSAGE_RANK_BONUS_CURSE_INQUIRY,// Curse resistance +10%
    UI_STRING_MESSAGE_RANK_BONUS_POISON_INQUIRY,// Poison resistance +10%
	UI_STRING_MESSAGE_RANK_BONUS_INQUIRY_MASTERY,// All resistances +3%

    UI_STRING_MESSAGE_RANK_BONUS_POWER_OF_SPIRIT,// Protection +5%
    UI_STRING_MESSAGE_RANK_BONUS_WIND_OF_SPIRIT,// Defense +5%
    UI_STRING_MESSAGE_RANK_BONUS_PIXIES_EYES,// To hit +5%
    UI_STRING_MESSAGE_RANK_BONUS_GROUND_OF_SPIRIT,// MP +5%
    UI_STRING_MESSAGE_RANK_BONUS_FIRE_OF_SPIRIT,// Critical damage +5%

    UI_STRING_MESSAGE_RANK_BONUS_EVOLUTION_IMMORTAL_HEART,// HP +5%
    UI_STRING_MESSAGE_RANK_BONUS_BEHEMOTH_ARMOR_2,// Defense +5%
    UI_STRING_MESSAGE_RANK_BONUS_DRAGON_EYE_2,// To hit +5%
    UI_STRING_MESSAGE_RANK_BONUS_EVOLUTION_RELIANCE_BRAIN,// MP +5%
    UI_STRING_MESSAGE_RANK_BONUS_HEAT_CONTROL,// Critical damage +5%

	UI_STRING_MESSAGE_RANK_BONUS_ACID_MASTERY,// Enemy Acid resistance -10%
    UI_STRING_MESSAGE_RANK_BONUS_BLOODY_MASTERY,// Enemy Blood resistance -10%
    UI_STRING_MESSAGE_RANK_BONUS_CURSE_MASTERY,// Enemy Curse resistance -10%
    UI_STRING_MESSAGE_RANK_BONUS_POISON_MASTERY,// Enemy Poison resistance -10%
    UI_STRING_MESSAGE_RANK_BONUS_SKILL_MASTERY,// All enemy resistances -3%

    UI_STRING_MESSAGE_RANK_BONUS_SALAMANDERS_KNOWLEDGE,// Fire spirit +1
    UI_STRING_MESSAGE_RANK_BONUS_UNDINES_KNOWLEDGE,// Water spirit +1
    UI_STRING_MESSAGE_RANK_BONUS_GNOMES_KNOWLEDGE,// Earth spirit +1
	
	UI_STRING_MESSAGE_SIEGE_ATTACK,// (Attackers)
	UI_STRING_MESSAGE_SIEGE_DEFENSE,// (Defenders)
	
	UI_STRING_MESSAGE_NOT_LOGINED,// (Offline)
	UI_STRING_MESSAGE_GUILD_LIST_ID,// ID
	UI_STRING_MESSAGE_GUILD_LIST_SERVER,// Server
	UI_STRING_MESSAGE_GUILD_LIST_GRADE,// Grade

	UI_STRING_MESSAGE_TEAM_COMMAND_WINDOW,// Team Command
	UI_STRING_MESSAGE_TEAM_LIST_WINDOW,// Team List
	UI_STRING_MESSAGE_TEAM_WAIT_LIST_WINDOW,// Wait Team List
	UI_STRING_MESSAGE_TEAM_UNION_WINDOW,// Team Union

	UI_STRING_MESSAGE_CLAN_COMMAND_WINDOW,// Clan Command
	UI_STRING_MESSAGE_CLAN_LIST_WINDOW,// Clan List
	UI_STRING_MESSAGE_CLAN_WAIT_LIST_WINDOW,// Wait Clan List
	UI_STRING_MESSAGE_CLAN_UNION_WINDOW,// Clan Union

	UI_STRING_MESSAGE_GUILD_COMMAND_WINDOW,// Guild Command
	UI_STRING_MESSAGE_GUILD_LIST_WINDOW,// Guild List
	UI_STRING_MESSAGE_GUILD_WAIT_LIST_WINDOW,// Wait Guild List
	UI_STRING_MESSAGE_GUILD_UNION_WINDOW,// Guild Union

	UI_STRING_MESSAGE_TOTAL_GUILD_DESC1,
	UI_STRING_MESSAGE_TOTAL_GUILD_DESC2,
	UI_STRING_MESSAGE_TOTAL_GUILD_DESC3,
	UI_STRING_MESSAGE_TOTAL_GUILD_ALREADY_IN_UNION,
	UI_STRING_MESSAGE_TOTAL_GUILD_ALREADY_OFFER_SOMETHING,
	UI_STRING_MESSAGE_TOTAL_GUILD_TARGET_IS_NOT_MASTER,   
	UI_STRING_MESSAGE_TOTAL_GUILD_NOT_IN_UNION,
	UI_STRING_MESSAGE_TOTAL_GUILD_MASTER_CANNOT_QUIT,
	UI_STRING_MESSAGE_TOTAL_GUILD_NO_TARGET_UNION,
	UI_STRING_MESSAGE_TOTAL_GUILD_NOT_YOUR_UNION,
	UI_STRING_MESSAGE_TOTAL_GUILD_SOURCE_IS_NOT_MASTER,
	UI_STRING_MESSAGE_TOTAL_GUILD_JOIN_ASK,
	UI_STRING_MESSAGE_TOTAL_GUILD_MESSAGE_OK,
	UI_STRING_MESSAGE_TOTAL_GUILD_MESSAGE_REFUSE,
	UI_STRING_MESSAGE_TOTAL_GUILD_MESSAGE_SUCCESS,
	UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_ASK,
	UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_MSG, 
	UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_MSG2,
	UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_OK,
	UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_CANCEL,
	UI_STRING_MESSAGE_TOTAL_GUILD_DEPORT_ASK,
	UI_STRING_MESSAGE_TOTAL_GUILD_DEPORT_OK,		

	UI_STRING_MESSAGE_TOTAL_UNION_JOIN_MSG,// The %s guild is applying to join the union.
	UI_STRING_MESSAGE_TOTAL_UNION_DEPORT_MSG,// The %s guild is applying to leave the union.
	UI_STRING_MESSAGE_TOTAL_UNION_PENALTY,// There is a record of being forcibly removed from a guild union.
	
	UI_STRING_MESSAGE_ASKING_RECALL,// Summon %s?
	UI_STRING_LEARN_SKILL_LEVEL,// Learned at level : %d
	UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_ACCEPT,// Accept the request to leave the union
	UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_DENY,// Refuse the request to leave the union.

	UI_STRING_MESSAGE_REQUEST_UNION_ERROR_1,// Only a guild master can apply for a union.
	UI_STRING_MESSAGE_REQUEST_UNION_ERROR_2,// You already belong to a union.
	UI_STRING_MESSAGE_REQUEST_UNION_ERROR_3,// You are not the union master.
	UI_STRING_MESSAGE_UNION_ERROR_NO_SLOT,// The union has no free slot.
	
	UI_STRING_MESSAGE_EQUIP_BLOOD_BIBLE,// Equip the Blood Bible seal.
	UI_STRING_MESSAGE_RENT_BLOOD_BIBLE,// Borrow the Blood Bible seal.
	UI_STRING_MESSAGE_RENT_BLOOD_BIBLE2,// Borrow the %s seal. (%s)
	UI_STRING_MESSAGE_RENT_LATER_BLOOD_BIBLE,// I will borrow it another time.

	UI_STRING_MESSAGE_RANGER_SAY,//] = "Ranger/";
	UI_STRING_MESSAGE_MODIFY_TAX_OK,// The tax rate was changed.
	UI_STRING_MESSAGE_MODIFY_TAX_FAIL,// The tax rate could not be changed.
	UI_STRING_MESSAGE_MODIFY_TAX,// Please enter the new tax rate.
	UI_STRING_MESSAGE_RANGER_SAY2,// Ranger

	UI_STRING_MESSAGE_REMOVE_CURSE_1,//] = "♡";
	UI_STRING_MESSAGE_REMOVE_CURSE_2,// love
	UI_STRING_MESSAGE_REMOVE_CURSE_3,// love you
	UI_STRING_MESSAGE_REMOVE_CURSE_4,// I love you

	STRING_MESSAGE_RACE_WAR_STARTED_IN_OTHER_SERVER,// The race war has started on the first server.

	UI_STRING_MESSAGE_APPOINT_SUBMASTER, // Appoint as sub master.
	
	UI_STRING_MESSAGE_CAN_SKILL_DELETE,// Skill points can be refunded
	UI_STRING_MESSAGE_CANNOT_SKILL_DELETE,// Skill points cannot be refunded

	STRING_MESSAGE_CANNOT_SKILLTREE_DELETE,// The points cannot be refunded.
	UI_STRING_MESSAGE_TOO_MANY_MEMBER,// There are already 50 members, so you cannot join.
	UI_STRING_MESSAGE_CONFIRM_DOWN_SKILL2,// This refunds the points spent on %s. It costs $%s. Proceed?

	UI_STRING_MESSAGE_UNION_CHATTING,// Union chat
	UI_STRING_MESSAGE_SHOW_UNION_CHATTING,// Show union chat
	UI_STRING_MESSAGE_NOT_SHOW_UNION_CHATTING,// Hide union chat

	UI_STRING_MESSAGE_USE_SIEGE_FOR_RESURRECT,// Resurrect in front of the inner castle gate.

	UI_STRING_POWER_JJANG_GET_POINT,// Collect Powerzzang points
	UI_STRING_POWER_JJANG_POINT,// My Powerzzang points
	UI_STRING_POWER_JJANG_AVAILABLE,// Points available to exchange
	UI_STRING_POWER_JJANG_NUMBER_1,// Mobile
	UI_STRING_POWER_JJANG_NUMBER_2,// number
	UI_STRING_POWER_JJANG_GET_POINT_HELP,// Collect your Powerzzang points.
	UI_STRING_POWER_JJANG_EXCHANGE_HELP,// Exchange Powerzzang points for items.
	UI_STRING_POWER_JJANG_REQUEST_OK,// Your Powerzzang points were applied. Points transferred : %d

	UI_STRING_POWER_JJANG_ERROR_NO_MEMBER,// You are not a Powerzzang member.
	UI_STRING_POWER_JJANG_ERROR_SERVER_ERROR,// The Powerzzang server is having trouble. Try again, and check www.powerzzang.com if it still fails.
	UI_STRING_POWER_JJANG_ERROR_PROCESS_ERROR,// The Powerzzang database is having trouble. Try again, and check www.powerzzang.com if it still fails.
	UI_STRING_POWER_JJANG_ERROR_NO_POINT,// You have no Powerzzang points saved up.
	UI_STRING_POWER_JJANG_ERROR_NO_MATCHING,// No matching information could be found.
	UI_STRING_POWER_JJANG_ERROR_CONNECT,// There is a problem connecting to the Powerzzang server.

	UI_STRING_MESSAGE_UTIL,// Util
	UI_STRING_MESSAGE_PERSONAL_STORE,// Personal Store
	UI_STRING_MESSAGE_POWER_JJANG,// Powerzzang

	//add by viva for NONKOWNMESSAGE
	UI_STRING_MESSAGE_NONKOWN1,// Equip a cash shop item
	UI_STRING_MESSAGE_NONKOWN2,// Buy a cash shop item
	//end

	UI_STRING_MESSAGE_SELL_MONEY_IN_DIALOG,	  // Enter the price to sell the item for.
	UI_STRING_MESSAGE_PERSNALSHOP_MESSAGE,	  // Enter your personal store's advert!!
	UI_STRING_MESSAGE_PERSNALSHOP_OK,	  // Open your personal store.
	UI_STRING_MESSAGE_PERSNALSHOP_CANCEL,	  // Close your personal store.
	UI_STRING_MESSAGE_PERSNALSHOP_WRITE_MESSAGE,	// Write your personal store's advert
	
	UI_STRING_MESSAGE_CANNOT_FIND_STORE,// That seller could not be found.
	UI_STRING_MESSAGE_STORE_CLOSED,// The store has already closed.
	UI_STRING_MESSAGE_ITEM_NOT_FOUND,// That item has already been sold or withdrawn by the seller.
	UI_STRING_MESSAGE_TOO_MUCH_MONEY,// The seller is carrying too much money, so you cannot buy it.
	UI_STRING_MESSAGE_ALREADY_DISPLAYED,// That item is already on display.
	UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE,// All sorts of goods for sale.
	
	UI_STRING_MESSAGE_GAMEMONEY_WITH_HANGUL,// Show game money in words as well
	UI_STRING_MESSAGE_ADVANTE_0,// Horus %d
	UI_STRING_MESSAGE_ADVANTE_1,// Seth %d
	UI_STRING_MESSAGE_ADVANTE_2,// Maat %d
	UI_STRING_MESSAGE_ADVANTE_3,// Osiris %d
	UI_STRING_MESSAGE_ADVANTE_4,// Thoth %d
	UI_STRING_MESSAGE_ADVANTE_5,// Nut %d
	UI_STRING_MESSAGE_ADVANTE_6,// Geb %d
	UI_STRING_MESSAGE_ADVANTE_7,// Shu %d
	UI_STRING_MESSAGE_ADVANTE_8,// Ra %d
	UI_STRING_MESSAGE_ADVANTE_9,// Ptah %d
	UI_STRING_MESSAGE_ADVANTE_10,// Nun %d

	UI_STRING_MESSAGE_EQUIP_PDA,// Equip a PDA.
	UI_STRING_MESSAGE_EQUIP_SHOULDER,// Wear a shoulder guard.

	UI_STRING_MESSAGE_EQUIP_DERMIS,// Apply a tattoo.
	UI_STRING_MESSAGE_EQUIP_PERSONA,// Wear a mask.

	UI_STRING_MESSAGE_EQUIP_FASCIA,// Wear a waist ornament.
	UI_STRING_MESSAGE_EQUIP_MITTEN,// Wear mittens.
	UI_STRING_MESSAGE_CHINGHO,// Title
	
	UI_STRING_MESSAGE_SWAP_ADVANCEMENT_ITEM,// Click the item you want to exchange for an advancement item.
	UI_STRING_MESSAGE_SWAP_CONFIRM,// Exchange it for an advancement item?
	UI_STRING_MESSAGE_SWAP_ERROR,// You have not advanced, so you cannot exchange items.
	UI_STRING_MESSAGE_SWAP_ADVANCEMENT_ITEM_ERROR,// This cannot be exchanged for an advancement item.

	UI_STRING_MESSAGE_CAMPAIGN_HELP_REQUEST,// Please enter the amount you wish to donate.
	UI_STRING_MESSAGE_CAMPAIGN_HELP_THANKS,// Your donation will go to neighbours in need. Thank you for taking part.
	UI_STRING_MESSAGE_CAMPAIGN_HELP_UNITS_SLAYER,// x10,000
	UI_STRING_MESSAGE_CAMPAIGN_HELP_UNITS_VAMPIRE,// x10,000
	UI_STRING_MESSAGE_CAMPAIGN_HELP_UNITS_OUSTERS,// x10,000

	UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_0,// Horus grade %d or higher
	UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_1,// Seth grade %d or higher
	UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_2,// Maat grade %d or higher
	UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_3,// Osiris grade %d or higher
	UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_4,// Thoth grade %d or higher
	UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_5,// Nut grade %d or higher
	UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_6,// Geb grade %d or higher
	UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_7,// Shu grade %d or higher
	UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_8,// Ra grade %d or higher
	UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_9,// Ptah grade %d or higher
	UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_10,// Nun grade %d or higher
	
	UI_STRING_MESSAGE_ADVANCEMENT_JOB_BLADE,// Splitter
	UI_STRING_MESSAGE_ADVANCEMENT_JOB_SWORD,// Defender
	UI_STRING_MESSAGE_ADVANCEMENT_JOB_SOLDER,// Heavy Shooter
	UI_STRING_MESSAGE_ADVANCEMENT_JOB_HEAL,// Priest
	UI_STRING_MESSAGE_ADVANCEMENT_JOB_ENCHANT,// Granter
	UI_STRING_MESSAGE_ADVANCEMENT_JOB_VAMPIRE,// Vamp Noble
	UI_STRING_MESSAGE_ADVANCEMENT_JOB_COMBAT,// Custos
	UI_STRING_MESSAGE_ADVANCEMENT_JOB_EARTH,// Terranos
	UI_STRING_MESSAGE_ADVANCEMENT_JOB_FIRE,// Igniser
	UI_STRING_MESSAGE_ADVANCEMENT_JOB_WATER,// Aquan

	UI_STRING_MESSAGE_DO_NOT_SHOW_PERSNALSHOP_MSG,// Hide personal store messages

	UI_STRING_MESSAGE_REQUEST_GET_EVENT_ITEM,// Claim the Come Back event item on this character?

	UI_STRING_MESSAGE_GET_EVENT_ITEM_RECEIVE_OK,// You received the Come Back event item.
	UI_STRING_MESSAGE_GET_EVENT_ITEM_RECEIVE_ALREADY,// You have already claimed the Come Back event item.
	UI_STRING_MESSAGE_GET_EVENT_ITEM_RECEIVE_FAIL,// Claiming the Come Back event item failed.
	UI_STRING_MESSAGE_GET_EVENT_ITEM_NOT_EVENT_USER,// You are not eligible for the Come Back event.

	UI_STRING_MESSAGE_GET_EVENT_ITEM_NOTICE,// Congratulations! You can claim your Come Back 2005 gift.


	UI_STRING_MESSAGE_FAIL_OPEN_WEBPAGE,// The web page could not be opened.
	UI_STRING_MESSAGE_CANNOT_USE_ADVANCEMENTCLASS,// Advanced characters cannot use this.

	UI_STRING_MESSAGE_CANNOT_USE_HIGH_GRADE,// Items of grade 6 or above cannot be enchanted.
	UI_STRING_MESSAGE_CANNOT_USE_ADVANCEMENT_ITEM,// Advancement-only items cannot be enchanted.
	UI_STRING_MESSAGE_CANNOT_USE_OVER_TWO_OPTION,// Items with two or more options cannot be enchanted.
	STRING_ERROR_IP_DENY,
	UI_STRING_MESSAGE_CHECK_VERSION_ERROR,
	STRING_STATUS_NOT_FIND_SKILL_CRAD,
	UI_STRING_MESSAGE_SYSTEM,
	UI_STRING_MESSAGE_PLAYER_SAY,
	UI_STRING_NO_ITEM_MESSAGE,				// You do not have that item.
	//add by viva : friend button description
	UI_STRING_MESSAGE_FRIEND,
//	UI_STRING_MESSAGE_FRIEND_LIST,
//	UI_STRING_MESSAGE_FRIEND_BLACKLIST,
//	UI_STRING_MESSAGE_FRIEND_TAG,
//	UI_STRING_MESSAGE_FRIEND_UP,
//	UI_STRING_MESSAGE_FRIEND_DOWN,
//	UI_STRING_MESSAGE_FRIEND_FIND,
//	UI_STRING_MESSAGE_FRIEND_ADD,
//	UI_STRING_MESSAGE_FRIEND_SETUP,
//	//add by viva : ask_friend_dialog
//	UI_STRING_ASK_FRIEND_REQUEST,
//	UI_STRING_ASK_FRIEND_REFUSE,
//	UI_STRING_ASK_FRIEND_WAIT,
//	UI_STRING_ASK_FRIEND_EXSIT,
//	UI_STRING_ASK_FRIEND_BLACK,
//	UI_STRING_ASK_FRIEND_DELETE,
	//end

	// Labels for the ESC game menu, whose buttons are Korean artwork in the
	// skin sprite. Added after the shipped String.inf was generated, so read
	// them through GetGameString().
	UI_STRING_MESSAGE_GAME_MENU_OPTION,						// Option (O)
	UI_STRING_MESSAGE_GAME_MENU_LOGOUT,						// Log out (L)
	UI_STRING_MESSAGE_GAME_MENU_CONTINUE,					// Continue playing (C)

	// The character-delete dialog confirms by asking for the character's
	// name instead of the original ID-number check. Added after the shipped
	// String.inf was generated, so read them through GetGameString().
	UI_STRING_MESSAGE_DELETE_INPUT_NAME,					// Enter the character's name to confirm.
	UI_STRING_MESSAGE_DELETE_NAME_MISMATCH,					// The name does not match the character you selected.

	// The six ask dialogs the friend list raises. Upstream commented
	// their ids out of this enum but left the dialogs themselves live and
	// server-reachable, so C_VS_UI_ASK_DIALOG read six rows of
	// m_sz_question_msg it had never assigned. Added after the shipped
	// String.inf was generated, so read them through GetGameString().
	UI_STRING_MESSAGE_ASK_FRIEND_REQUEST,					// %s wants to add you as a friend.
	UI_STRING_MESSAGE_ASK_FRIEND_REFUSE,					// %s has declined your friend request.
	UI_STRING_MESSAGE_ASK_FRIEND_WAIT,						// Waiting for %s to answer.
	UI_STRING_MESSAGE_ASK_FRIEND_EXIST,						// %s is already on your friend list.
	UI_STRING_MESSAGE_ASK_FRIEND_BLACK,						// %s is on your block list.
	UI_STRING_MESSAGE_ASK_FRIEND_DELETE,					// Remove %s from your friend list?

	MAX_GAME_STRING,
};



#endif
