//----------------------------------------------------------------------
// MGameStringTable.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "CrtCompat.h"
#include "MGameStringTable.h"
#include "Properties.h"
#include "DebugLog.h"

#include <string_view>


//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
MStringArray*		g_pGameStringTable = NULL;
//2004, 6, 18, sobeit add start - about nick name string table
MStringArray*		g_pNickNameStringTable = NULL;
//2004, 6, 18, sobeit add end - about nick name string table


//----------------------------------------------------------------------
// Should the built-in English text replace the text the game data ships?
//
// Data/Info/Language.inf picks the language with a "LANGUAGE <n>" line,
// numbered as DARKEDEN_LANGUAGE in Client.cpp: 0 Korean, 1 Chinese,
// 2 Japanese, 3 English, 4 Taiwan. English is this build's default, so the
// built-in table wins whenever the file is missing or selects English; any
// other value leaves the strings that String.inf supplied untouched.
//----------------------------------------------------------------------
bool
UseEnglishText(const Properties* pFileDef)
{
	if (pFileDef == NULL)
	{
		return true;
	}

	std::string fileName;

	try
	{
		fileName = pFileDef->getProperty("FILE_LANGUAGE_INFO");
	}
	catch (...)
	{
		// No Language.inf configured at all.
		return true;
	}

	return UseEnglishTextFrom(fileName.c_str());
}

bool
UseEnglishTextFrom(const char* szLanguageInfoFile)
{
	enum { LANGUAGE_ENGLISH = 3 };

	if (szLanguageInfoFile == NULL)
	{
		return true;
	}
	std::string fileName = szLanguageInfoFile;

	FILE* pFile = Basic::OpenFile(fileName.c_str(), "r");

	if (pFile == NULL)
	{
		return true;
	}

	int language = LANGUAGE_ENGLISH;
	char szLine[512];

	while (fgets(szLine, sizeof(szLine), pFile) != NULL)
	{
		const std::string_view	svLine( szLine );

		if (svLine.starts_with(';'))
		{
			continue;
		}

		if (svLine.starts_with("LANGUAGE"))
		{
			Basic::ScanString(szLine + 8, "%d", &language);
			break;
		}
	}

	fclose(pFile);

	return (language == LANGUAGE_ENGLISH);
}


//----------------------------------------------------------------------
// Does this table entry hold a printf conversion specification that is
// unsafe to hand to sprintf as the format string?
//
// An entry is rejected when one of its specifications can write through an
// argument, take its width from an argument, demand more argument-driven
// expansion than the call sites' buffers can hold, or retype the argument
// the call site actually passed:
//
//   %n          writes through a pointer argument - an arbitrary write
//   %*d, %.*f   takes the width or precision from an argument; no call site
//               passes one, so it is whatever happens to sit in the
//               argument list
//   %32d        one width or precision of MAX_CONVERSION_WIDTH or more. The
//               built-in English table further down this file is the
//               reference for what an entry legitimately needs: its whole
//               specifier set is %d, %s and %02d, so the largest real width
//               is 2. 32 leaves that enormous headroom and still sits well
//               under the smallest buffer a call site prints into, the
//               char sz_buf[50] in VS_UI/src/VS_UI_Description.cpp.
//   %30d%30d...	widths compose - the check is per specification, so the
//               widths and precisions of the whole entry are summed too and
//               the entry is rejected once the sum passes
//               MAX_ENTRY_WIDTH_BUDGET. 256 is two orders of magnitude above
//               what any real entry asks for, and it is what bounds the
//               entry as a whole rather than one specification at a time.
//   %f %e %g    the floating point conversions need no width at all to be
//   %a and the  huge: "%f" with a double of 1e300 prints 308 digits and
//   uppercase   "%.512f" prints 814. They are also the one family that
//   forms       changes which register the argument is read from - on x64 a
//               %f reads an XMM register, so an entry that grew one reads a
//               register the call site never set.
//   %S %C       the wide conversions keep the argument count unchanged, so
//   %ls %lc     every call site still "matches", but the CRT then reads a
//   %ws %wc     char* argument as a wchar_t* and scans past the end of the
//               buffer the call site passed. MSVC treats the uppercase S
//               and C as wide in the narrow printf family, and l/w/I in
//               front of s or c the same way.
//
// The length modifiers are parsed only so that they cannot hide the
// conversion character behind them: without consuming "I64", "%I64n" would
// scan as the conversion 'I' followed by the ordinary letters "64n" and the
// %n would be missed. MSVC accepts the "I", "I32" and "I64" forms as well as
// the standard ones, so all of them are skipped here.
//
// What this check does NOT do: it does not check that an entry's specifier
// count matches the call site that formats it. The table does not know which
// call site takes which entry, so arity is unknowable at load time, and an
// entry carrying a %s that its call site passes no argument for still makes
// sprintf read a stack word as a char*. That hole is closed per call site,
// not here - the call sites that pass no varargs at all are being converted
// to bounded "%s" copies separately.
//
// Multi-byte text is safe to walk one byte at a time: neither GBK nor CP949
// uses 0x25 ('%') as a trail byte, so no character of the Korean or Chinese
// data can be mistaken for the start of a conversion specification.
//
// A literal percent the data did not double is a conversion specification to
// the CRT and to this check alike, so an entry reading "50% Sale" is rejected
// - ' ' is a flag and 'S' is then the wide-string conversion. That is a false
// positive in intent, but not in effect: the CRT really would read an
// argument as a wchar_t* there.
//----------------------------------------------------------------------
static bool
IsUnsafeFormatEntry(const char* pString)
{
	// Largest width or precision one specification may name, and the
	// largest sum of them the whole entry may name. See the banner above
	// for how both numbers were chosen.
	enum { MAX_CONVERSION_WIDTH		= 32 };
	enum { MAX_ENTRY_WIDTH_BUDGET	= 256 };

	if (pString == NULL)
	{
		return false;
	}

	const char* p = pString;
	int nTotalWidth = 0;

	while (*p != '\0')
	{
		if (*p != '%')
		{
			p++;
			continue;
		}

		p++;

		//------------------------------------------------------
		// "%%" is a literal percent and a trailing '%' formats
		// nothing - neither starts a specification.
		//------------------------------------------------------
		if (*p == '\0')
		{
			break;
		}

		if (*p == '%')
		{
			p++;
			continue;
		}

		//------------------------------------------------------
		// flags
		//------------------------------------------------------
		while (*p=='-' || *p=='+' || *p==' ' || *p=='0' || *p=='#')
		{
			p++;
		}

		//------------------------------------------------------
		// field width
		//------------------------------------------------------
		if (*p == '*')
		{
			return true;
		}

		int nWidth = 0;

		while (*p >= '0' && *p <= '9')
		{
			// Stop accumulating once the cap is already reached, so
			// that a very long run of digits cannot overflow the
			// counter. The value is rejected below either way.
			if (nWidth < MAX_CONVERSION_WIDTH)
			{
				nWidth = nWidth * 10 + (*p - '0');
			}

			p++;
		}

		if (nWidth >= MAX_CONVERSION_WIDTH)
		{
			return true;
		}

		//------------------------------------------------------
		// precision
		//------------------------------------------------------
		int nPrecision = 0;

		if (*p == '.')
		{
			p++;

			if (*p == '*')
			{
				return true;
			}

			while (*p >= '0' && *p <= '9')
			{
				if (nPrecision < MAX_CONVERSION_WIDTH)
				{
					nPrecision = nPrecision * 10 + (*p - '0');
				}

				p++;
			}

			if (nPrecision >= MAX_CONVERSION_WIDTH)
			{
				return true;
			}
		}

		//------------------------------------------------------
		// whole-entry budget
		//
		// Each specification is under the per-conversion cap by
		// here, but they add up: five "%30d" in one entry are five
		// separate 30-byte expansions into the same buffer. Sum
		// them and reject the entry once the total passes the
		// budget. Both terms are already below
		// MAX_CONVERSION_WIDTH, so the sum cannot overflow before
		// the test fires.
		//------------------------------------------------------
		nTotalWidth += nWidth + nPrecision;

		if (nTotalWidth > MAX_ENTRY_WIDTH_BUDGET)
		{
			return true;
		}

		//------------------------------------------------------
		// length modifier
		//
		// cLength keeps the first byte of it, because l, w and I in
		// front of s or c are what make those conversions wide.
		//------------------------------------------------------
		char cLength = '\0';

		if (*p=='h' || *p=='l')
		{
			cLength = *p;

			p++;

			// "hh" and "ll"
			if (*p == cLength)
			{
				p++;
			}
		}
		else if (*p=='j' || *p=='z' || *p=='t' || *p=='L' || *p=='w')
		{
			cLength = *p;

			p++;
		}
		else if (*p == 'I')
		{
			cLength = *p;

			p++;

			if ((p[0]=='6' && p[1]=='4') || (p[0]=='3' && p[1]=='2'))
			{
				p += 2;
			}
		}

		//------------------------------------------------------
		// conversion character
		//------------------------------------------------------
		const char cConversion = *p;

		// Writes through an argument.
		if (cConversion == 'n')
		{
			return true;
		}

		// Floating point: unbounded without a width, and read from a
		// different register file than the integers the call sites pass.
		if (cConversion=='e' || cConversion=='E'
		 || cConversion=='f' || cConversion=='F'
		 || cConversion=='g' || cConversion=='G'
		 || cConversion=='a' || cConversion=='A')
		{
			return true;
		}

		// Wide: same argument count, but a char* argument is then read
		// as a wchar_t* and scanned past the end of its buffer.
		if (cConversion=='S' || cConversion=='C')
		{
			return true;
		}

		if ((cConversion=='s' || cConversion=='c')
		 && (cLength=='l' || cLength=='w' || cLength=='I'))
		{
			return true;
		}

		if (cConversion != '\0')
		{
			p++;
		}
	}

	return false;
}


//----------------------------------------------------------------------
// Sanitize Game String Table
//----------------------------------------------------------------------
// Scrubs the format strings the game data supplies before anything can
// print with them. Data/Info/String.inf is read straight off disk and its
// entries reach sprintf all over the client, so a tampered - or merely
// corrupt - file can turn a UI label into an arbitrary write. Every entry
// IsUnsafeFormatEntry() rejects is replaced whole, because a specification
// cannot be edited out of a string without changing how many arguments the
// call site consumes.
//
// What this removes: entries that could write through an argument (%n),
// take a width from an argument (%*d), demand large argument-driven
// expansion (a big width or precision, singly or summed over the entry), or
// retype an argument (the wide and the floating point conversions).
//
// What this does NOT do: it does not check that an entry's specifier count
// matches the call site that formats it - the table does not know which
// call site takes which entry, so arity is unknowable here. An entry that
// carries a %s the call site passes no argument for survives this pass and
// still makes sprintf read a stack word as a char*. What actually closes
// that hole is converting the call sites that pass no varargs at all to
// bounded "%s" copies, which is a separate change per call site.
//
// The placeholder is deliberately English and built in: the table that
// would hold its translation is the one under suspicion.
//----------------------------------------------------------------------
void
SanitizeGameStringTable()
{
	if (g_pGameStringTable == NULL)
	{
		return;
	}

	const int nSize = (*g_pGameStringTable).GetSize();
	int nRemoved = 0;

	for (int i=0; i<nSize; i++)
	{
		// GetString() is NULL only for an entry that was never set -
		// MString's default constructor leaves m_pString NULL. An entry
		// the file left empty is not NULL: MString::LoadFromFile gives a
		// zero length string its own "" buffer. IsUnsafeFormatEntry
		// handles both, the NULL by returning false and the "" by finding
		// no '%'.
		const char* pString = (*g_pGameStringTable)[i].GetString();

		if (!IsUnsafeFormatEntry(pString))
		{
			continue;
		}

		g_pGameStringTable->Set(i, "(removed unsafe string)");
		nRemoved++;

		// Replacing an unsafe UI string is a data error, so keep its
		// diagnostic visible at the error level.
		DEBUG_ADD_FORMAT_ERR("[StringTable] entry %d holds an unsafe format string - replaced", i);
	}

	if (nRemoved != 0)
	{
		DEBUG_ADD_FORMAT_ERR("[StringTable] %d unsafe format string(s) replaced", nRemoved);
	}
}


void
InitGameStringTable()
{
	(*g_pGameStringTable).Init( MAX_GAME_STRING + 1 );
	g_pGameStringTable->Set(STRING_NETWORK_CONDITION_BAD, "The connection is unstable.");
	g_pGameStringTable->Set(STRING_RESURRECTION_AFTER_SECONDS, "You can resurrect in %d seconds.");
	g_pGameStringTable->Set(STRING_DRAW_ZONE_NAME, "%s (%d,%d)");
	g_pGameStringTable->Set(STRING_DRAW_GAME_DATE, "%d/%d/%d");
	g_pGameStringTable->Set(STRING_DRAW_GAME_TIME, "%02d:%02d:%02d");
	g_pGameStringTable->Set(STRING_DRAW_ITEM_NAME_MONEY, "%s(%d)");
	g_pGameStringTable->Set(STRING_USER_REGISTER_DENY, "New accounts cannot be created.");
	g_pGameStringTable->Set(STRING_USER_REGISTER_HOMEPAGE, "You can sign up on the homepage.");
	g_pGameStringTable->Set(STRING_USER_REGISTER_EMPTY_FIELD, "A required field is empty.");
	g_pGameStringTable->Set(STRING_USER_REGISTER_INVALID_ID, "Invalid ID.");
	g_pGameStringTable->Set(STRING_USER_REGISTER_ID_LENGTH, "The ID must be %d to %d characters.");
	g_pGameStringTable->Set(STRING_USER_REGISTER_ID_SPECIAL, "Do not use special characters in the ID.");
	g_pGameStringTable->Set(STRING_USER_REGISTER_PASSWORD_LENGTH, "The password must be %d to %d characters.");
	g_pGameStringTable->Set(STRING_USER_REGISTER_PASSWORD_SPECIAL, "Do not use special characters in the password.");
	g_pGameStringTable->Set(STRING_USER_REGISTER_PASSWORD_NUMBER, "The password cannot be digits only.");
	g_pGameStringTable->Set(STRING_USER_REGISTER_NAME_LENGTH, "The name can be at most %d characters.");
	g_pGameStringTable->Set(STRING_USER_REGISTER_INVALID_SSN, "Invalid ID number.");
	g_pGameStringTable->Set(STRING_USER_REGISTER_SSN_FORMAT, "ID number example : 123456789012345678");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_BUY_NO_SPACE, "There is no free space.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_BUY_NO_MONEY, "You do not have enough money.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_BUY_NO_ITEM, "That item is not in stock.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_SELL, "You cannot sell that.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_REPAIR, "You cannot repair that.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_SILVERING, "You cannot silver-plate that.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_BUY_MORE, "You cannot buy another storage box.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_STORAGE, "You cannot put that in storage.");
	g_pGameStringTable->Set(STRING_MESSAGE_STORAGE_BUY, "You have already bought a storage box.");
	g_pGameStringTable->Set(STRING_MESSAGE_NO_STORAGE, "You have not bought a storage box yet.");
	g_pGameStringTable->Set(STRING_MESSAGE_TRADE_REJECTED, "The other player refused to trade.");
	g_pGameStringTable->Set(STRING_MESSAGE_TRADE_NOBODY, "There is nobody to trade with.");
	g_pGameStringTable->Set(STRING_MESSAGE_TRADE_CANNOT_ON_MOTORCYCLE, "You cannot trade while riding a motorcycle.");
	g_pGameStringTable->Set(STRING_MESSAGE_TRADE_SAFETY_ZONE_ONLY, "You can only trade in a safety zone.");
	g_pGameStringTable->Set(STRING_MESSAGE_TRADE_BUSY, "You cannot trade right now.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_TRADE, "You cannot trade.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_TRADE_ALREADY_TRADING, "A trade is already in progress.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_TRADE_NO_SPACE, "There is not enough trade space.");
	g_pGameStringTable->Set(STRING_MESSAGE_SKILL_DIFFER_DOMAIN, "That skill belongs to a different domain.");
	g_pGameStringTable->Set(STRING_MESSAGE_SKILL_EXCEED_LEVEL, "Your level is too low for that skill.");
	g_pGameStringTable->Set(STRING_MESSAGE_SKILL_CANNOT_LEARN, "You cannot learn that yet.");
	g_pGameStringTable->Set(STRING_MESSAGE_SKILL_NOT_SUPPORT, "That skill is not supported yet.");
	g_pGameStringTable->Set(STRING_MESSAGE_FIND_MOTOR_NO_WHERE, "You have no idea where it is.");
	g_pGameStringTable->Set(STRING_MESSAGE_FIND_MOTOR_NO_KEY, "You have lost the motorcycle key.");
	g_pGameStringTable->Set(STRING_MESSAGE_FIND_MOTOR_OK, "It is in %s at (%d, %d).");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_USE_BONUS_POINT, "You cannot spend bonus points.");
	g_pGameStringTable->Set(STRING_MESSAGE_WHAT_SKILL_TO_LEARN, "Which %s skill would you like to learn?");
	g_pGameStringTable->Set(STRING_MESSAGE_NO_SKILL_TO_LEARN, "There is no skill you can learn right now.");
	g_pGameStringTable->Set(STRING_MESSAGE_NEW_SKILL_AVAILABLE, "You can learn a new skill.");
	g_pGameStringTable->Set(STRING_MESSAGE_NEW_DOMAIN_LEVEL_1, "Your %s domain level is now %d.");
	g_pGameStringTable->Set(STRING_MESSAGE_NEW_DOMAIN_LEVEL_2, "Your %s domain level is now %d.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_RELIC, "You cannot enter your own safety zone while carrying a relic!");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_REGIST_FAIL_ALREADY_JOIN, "Let me see... it says here you already belong to another team.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_REGIST_FAIL_QUIT_TIMEOUT, "You left your last team only moments ago. Think it over before you act.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_REGIST_FAIL_CANCEL_TIMEOUT, "Your team was disbanded only moments ago. Train until you meet the requirements, and wait for a better opening.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_REGIST_FAIL_LEVEL, "You are capable, but not yet leader material. Come back when you are stronger.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_REGIST_FAIL_MONEY, "Founding a team takes a great deal of money, and you do not seem to have it.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_REGIST_FAIL_FAME, "%s, is it... I have never heard that name. That makes you a novice. Come back when you have made a name for yourself.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_REGIST_FAIL_NAME, "That team name is already taken. Think of another one.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_REGIST_FAIL_DENY, "Your application was rejected.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_STARTING_FAIL_ALREADY_JOIN, "You already belong to another team.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_STARTING_FAIL_QUIT_TIMEOUT, "You left your last team only moments ago. Think it over before you act.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_STARTING_FAIL_CANCEL_TIMEOUT, "Your team was disbanded only moments ago. Train until you meet the requirements, and wait for a better opening.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_STARTING_FAIL_LEVEL, "You still have a lot to learn. Come back when you have trained a while longer.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_STARTING_FAIL_MONEY, "%s, registering a team costs more money than that.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_STARTING_FAIL_FAME, "%s, is it... I have never heard that name. That makes you a novice. Come back when you have made a name for yourself.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_STARTING_FAIL_DENY, "Your application was rejected.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_REGIST_FAIL_ALREADY_JOIN, "Let me see... it says here you are already sworn to another clan.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_REGIST_FAIL_QUIT_TIMEOUT, "You left your last clan only moments ago. Drifting from clan to clan does you no good. Take your time.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_REGIST_FAIL_CANCEL_TIMEOUT, "Your clan was disbanded only moments ago. Wait for a better opening.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_REGIST_FAIL_LEVEL, "You are capable, but not yet fit to lead. Come back when you are stronger.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_REGIST_FAIL_MONEY, "Registering a clan takes a great deal of money, and you do not seem to have it.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_REGIST_FAIL_FAME, "%s, is it... still a young vampire, I see. Drink more blood and come back to me.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_REGIST_FAIL_NAME, "That clan name is already taken. Think of another one.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_REGIST_FAIL_DENY, "Your application was rejected.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_STARTING_FAIL_ALREADY_JOIN, "You already belong to another clan.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_STARTING_FAIL_QUIT_TIMEOUT, "You left your last clan only moments ago. Drifting from clan to clan does you no good. Take your time.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_STARTING_FAIL_CANCEL_TIMEOUT, "Your clan was disbanded only moments ago. Wait for a better opening.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_STARTING_FAIL_LEVEL, "You lack the makings of a good second. Go and train some more.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_STARTING_FAIL_MONEY, "However skilled its members, a clan without money to keep it running will collapse.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_STARTING_FAIL_FAME, "%s, is it... I have never heard that name. That makes you a novice. Come back when you have made a name for yourself.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_STARTING_FAIL_DENY, "Your application was rejected.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_REGIST_FAIL_INTRO, "Please enter an introduction.");
	g_pGameStringTable->Set(STRING_STATUS_HP_MAX_1, "Your maximum health (HP) is now %d.");
	g_pGameStringTable->Set(STRING_STATUS_HP_MAX_2, "Your maximum health (HP) is now %d.");
	g_pGameStringTable->Set(STRING_STATUS_MP_MAX_1, "Your maximum mana (MP) is now %d.");
	g_pGameStringTable->Set(STRING_STATUS_MP_MAX_2, "Your maximum mana (MP) is now %d.");
	g_pGameStringTable->Set(STRING_STATUS_STR_1, "Your strength (STR) is now %d.");
	g_pGameStringTable->Set(STRING_STATUS_STR_2, "Your strength (STR) is now %d.");
	g_pGameStringTable->Set(STRING_STATUS_DEX_1, "Your dexterity (DEX) is now %d.");
	g_pGameStringTable->Set(STRING_STATUS_DEX_2, "Your dexterity (DEX) is now %d");
	g_pGameStringTable->Set(STRING_STATUS_INT_1, "Your intelligence (INT) is now %d.");
	g_pGameStringTable->Set(STRING_STATUS_INT_2, "Your intelligence (INT) is now %d.");
	g_pGameStringTable->Set(STRING_STATUS_LEVEL, "You are now level %d.");
	g_pGameStringTable->Set(STRING_LEARN_SKILL, "You learned the [%s] skill.");
	g_pGameStringTable->Set(STRING_CHANGE_ALIGNMENT, "Your alignment changed to %s.");
	g_pGameStringTable->Set(STRING_CHANGE_ALIGNMENT2, "Your alignment changed to %s.");
	g_pGameStringTable->Set(STRING_CHANGE_TO_BAD_ALIGNMENT, "Your alignment shifted a little towards evil.");
	g_pGameStringTable->Set(STRING_CHANGE_TO_GOOD_ALIGNMENT, "Your alignment shifted a little towards good.");
	g_pGameStringTable->Set(STRING_ERROR_INVALID_ID_PASSWORD, "The ID or password is incorrect.");
	g_pGameStringTable->Set(STRING_ERROR_ALREADY_CONNECTED, "That account is already connected.");
	g_pGameStringTable->Set(STRING_ERROR_ALREADY_REGISTER_ID, "That ID is already taken.");
	g_pGameStringTable->Set(STRING_ERROR_ALREADY_REGISTER_SSN, "That ID number is already registered.");
	g_pGameStringTable->Set(STRING_ERROR_EMPTY_ID, "Please enter an ID.");
	g_pGameStringTable->Set(STRING_ERROR_SMALL_ID_LENGTH, "The ID is too short.");
	g_pGameStringTable->Set(STRING_ERROR_EMPTY_PASSWORD, "Please enter a password.");
	g_pGameStringTable->Set(STRING_ERROR_SMALL_PASSWORD_LENGTH, "The password is too short.");
	g_pGameStringTable->Set(STRING_ERROR_EMPTY_NAME, "Please enter a name.");
	g_pGameStringTable->Set(STRING_ERROR_EMPTY_SSN, "Please enter an ID number.");
	g_pGameStringTable->Set(STRING_ERROR_INVALID_SSN, "Invalid ID number.");
	g_pGameStringTable->Set(STRING_ERROR_NOT_FOUND_PLAYER, "No such player.");
	g_pGameStringTable->Set(STRING_ERROR_NOT_FOUND_ID, "No such ID.");
	g_pGameStringTable->Set(STRING_ERROR_LOGIN_DENY, "Your account has been suspended. Please contact customer support.");
	g_pGameStringTable->Set(STRING_ERROR_ETC_ERROR, "An error occurred.");
	g_pGameStringTable->Set(STRING_ERROR_NOT_ALLOW_ACCOUNT, "Your account has been suspended. Please contact customer support.");
	g_pGameStringTable->Set(STRING_ERROR_NOT_PAY_ACCOUNT, "Your subscription has expired. Please contact customer support.");
	g_pGameStringTable->Set(STRING_MESSAGE_ITEM_BROKEN, "The item broke.");
	g_pGameStringTable->Set(STRING_MESSAGE_WHISPER_FAILED, "The whisper could not be delivered.");
	g_pGameStringTable->Set(STRING_MESSAGE_WHISPER_SELF, "You cannot whisper to yourself.");
	g_pGameStringTable->Set(STRING_MESSAGE_CHAT_IGNORE, "You are now ignoring %s.");
	g_pGameStringTable->Set(STRING_MESSAGE_CHAT_ACCEPT, "You are no longer ignoring %s.");
	g_pGameStringTable->Set(STRING_MESSAGE_CHAT_IGNORE_ALL, "You are now ignoring everyone.");
	g_pGameStringTable->Set(STRING_MESSAGE_CHAT_ACCEPT_ALL, "You are no longer ignoring anyone.");
	g_pGameStringTable->Set(STRING_MESSAGE_CHAT_BE_GOOD, "Please keep it civil :)");
	g_pGameStringTable->Set(STRING_MESSAGE_CHAT_ACCEPT_CURSE, "Chat is shown unfiltered.");
	g_pGameStringTable->Set(STRING_MESSAGE_CHAT_FILTER_CURSE, "Swearing is filtered out of chat.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_LOGOUT_DIED, "You cannot log out while dead.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_GLOBAL_SAY, "You cannot shout right now.");
	g_pGameStringTable->Set(STRING_MESSAGE_WAIT, "Please wait.");
	g_pGameStringTable->Set(STRING_MESSAGE_CONNECTING_SERVER, "Connecting to the server.");
	g_pGameStringTable->Set(STRING_MESSAGE_DONATION_OK, "You can donate.");
	g_pGameStringTable->Set(STRING_MESSAGE_DONATION_FAIL, "You cannot donate.");
	g_pGameStringTable->Set(STRING_MESSAGE_PARTY_REJECTED, "Your invitation was declined.");
	g_pGameStringTable->Set(STRING_MESSAGE_PARTY_NOBODY, "There is nobody to invite.");
	g_pGameStringTable->Set(STRING_MESSAGE_PARTY_SAFETY_ZONE_ONLY, "This only works in a safety zone.");
	g_pGameStringTable->Set(STRING_MESSAGE_PARTY_BUSY, "You cannot invite anyone right now.");
	g_pGameStringTable->Set(STRING_MESSAGE_RACE_DIFFER, "You are of a different race.");
	g_pGameStringTable->Set(STRING_MESSAGE_NOT_NORMAL_FORM, "You cannot do that while transformed.");
	g_pGameStringTable->Set(STRING_MESSAGE_NO_AUTHORITY, "You must be in the party for an hour first.");
	g_pGameStringTable->Set(STRING_MESSAGE_ERROR_PARTY, "The party could not be created.");
	g_pGameStringTable->Set(STRING_MESSAGE_KICK_PARTY_MEMBER_OK, "%s was kicked out of the party.");
	g_pGameStringTable->Set(STRING_MESSAGE_KICKED_FROM_PARTY, "You were kicked out by %s.");
	g_pGameStringTable->Set(STRING_MESSAGE_KICK_PARTY_MEMBER, "%s kicked %s out of the party.");
	g_pGameStringTable->Set(STRING_MESSAGE_REMOVE_PARTY, "The party was disbanded.");
	g_pGameStringTable->Set(STRING_MESSAGE_REMOVE_PARTY_HIMSELF, "%s left the party.");
	g_pGameStringTable->Set(STRING_MESSAGE_REMOVE_PARTY_MYSELF, "You left the party.");
	g_pGameStringTable->Set(STRING_MESSAGE_IN_ANOTHER_PARTY, "You are already in another party.");
	g_pGameStringTable->Set(STRING_MESSAGE_PARTY_FULL, "The party is full.");
	g_pGameStringTable->Set(STRING_MESSAGE_SOMEONE_JOINED_PARTY, "%s joined the party.");
	g_pGameStringTable->Set(STRING_MESSAGE_LOGOUT_AFTER_SECOND, "Logging out in %d seconds.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_BUY_NO_STAR, "You do not have enough stars.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_BUY, "You cannot resurrect.");
	g_pGameStringTable->Set(STRING_MESSAGE_CAN_REGISTER_NAME, "The name is available.");
	g_pGameStringTable->Set(STRING_MESSAGE_HELP_KEY, "Press CTRL+H if you need help.");
	g_pGameStringTable->Set(STRING_MESSAGE_HOW_TO_GET_BASIC_WEAPON, "Collect a basic weapon from Jack at the field headquarters.");
	g_pGameStringTable->Set(STRING_MESSAGE_WAIT_FOR_CHARACTER_SELECT_MODE, "   Opening the character selection window. Please wait.");
	g_pGameStringTable->Set(STRING_MESSAGE_ITEM_TO_ITEM_IMPOSIBLE, "That cannot be enchanted.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ONLY_PICK_UP_ITEM_ONE, "You have to carry them one at a time.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAN_ENCHANT, "This can be enchanted.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SELL_CONFIRM, "Sell this item?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ALL_SELL_CONFIRM, "Sell them?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ALL_PRICE, "That comes to $%s in total.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REPAIR_CONFIRM, "Repair it?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLICK_TRADE_ITEM, "Click the item you want to trade.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLICK_REPAIR_ITEM, "Click the item you want to repair.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLICK_OK_BUTTON_TO_END, "Press the OK button when you are done.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_THIS_ITEM_REPAIR_CONFIRM, "Repair this item?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_THIS_ITEM_CHARGE_CONFIRM, "Recharge this item?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_THIS_ITEM_SILVERING_CONFIRM, "Silver-plate this item?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLICK_SILVERING_ITEM, "Click the item you want silver-plated.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_ALPHA_WINDOW, "Draw windows semi-transparent.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_NO_ALPHA_WINDOW, "Do not draw windows semi-transparent.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAN_EQUIP, "You can equip this.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_EQUIP, "You cannot equip this.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_THROW_MONEY, "Drop money");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_ITEM_DESCRIPTION, "View the item description");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DEPOSIT_MONEY, "Deposit money");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INVITE_PARTY, "You have a party invitation.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUEST_PARTY, "You have a party join request.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RESURRECTION, "Resurrect at the set location");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EVACUATION, "Evacuate to the set location");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_UP_LEVEL, "You cannot level up any further");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_UP_STAT, "That stat cannot go any higher.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_JOIN, "Join");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TIP_SELL_ALL_VAMPIRE_HEAD, "TIP : right-click to sell all vampire heads at once.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TIP_REPAIR_ALL_ITEM, "TIP : right-click to repair all your gear at once.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_AUTO_HIDE_ON, "Auto-hide on");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_AUTO_HIDE_OFF, "Auto-hide off");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GAME_MENU, "Game Menu");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MENU, "Menu");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EXP, "Exp");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM, "Team");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MESSAGE, "Message");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HELP, "Help");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INVENTORY, "Inventory");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GEAR_WINDOW, "Gear Window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INFO_WINDOW, "Info Window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PARTY_WINDOW, "Party Window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_WINDOW, "Quest Window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO, "Team Info");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_MEMBER_LIST, "Team Member List");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BASIC_HELP, "Basic Help");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHATTING_HELP, "Chat Help");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_WAR_HELP, "Combat Help");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SKILL_HELP, "Skill Help");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_HELP, "Team Help");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLAN, "Clan");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLAN_INFO, "Clan Info");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLAN_MEMBER_LIST, "Clan Member List");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLAN_HELP, "Clan Help");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MAGIC_HELP, "Magic Help");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CURRENT_EXP, "Current Exp:");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NEXT_LEVEL, "Next Level:");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LEFT_EXP, "Exp Remaining:");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_HELM, "Wear a helmet");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_NECKLACE, "Wear a necklace");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_ARMOR, "Wear armor");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_SHIELD, "Equip a shield");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_WEAPON, "Equip a weapon");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_GLOVE, "Wear gloves");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_BELT, "Wear a belt");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_TROUSER, "Wear trousers");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_BRACELET, "Wear a bracelet");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_RING, "Wear a ring");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_SHOES, "Wear shoes");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_VAMPIRE_COAT, "Wear a coat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_EARRING, "Wear an earring");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_AMULET, "Wear an amulet");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FIRST_GEAR_SET, "First weapon set");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SECOND_GEAR_SET, "Second weapon set");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SPECIAL_CHARACTER, "Special characters (Control+X)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SET_LETTER_COLOR, "Set text color");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHANGE_INPUT_LANGUAGE, "KOR/ENG");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_WHISPER_ID, "Show IDs you have whispered");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NORMAL_CHATING, "Normal chat (Control+C)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ZONE_CHATTING, "Zone chat (Control+Z)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_WHISPER_CHATTING, "Whisper (Control+W)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PARTY_CHATTING, "Party chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_CHATTING, "Team chat (Control+G)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_NORMAL_CHATTING, "Show normal chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_ZONE_CHATTING, "Show zone chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_WHISPER_CHATTING, "Show whispers");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_PARTY_CHATTING, "Show party chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_TEAM_CHATTING, "Show team chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_SHOW_NORMAL_CHATTING, "Hide normal chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_SHOW_ZONE_CHATTING, "Hide zone chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_SHOW_WHISPER_CHATTING, "Hide whispers");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_SHOW_PARTY_CHATTING, "Hide party chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_SHOW_TEAM_CHATTING, "Hide team chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_SHOW_CLAN_CHATTING, "Hide clan chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_CLAN_CHATTING, "Show clan chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLAN_CHATTING, "Clan chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EXPEL, "Leave");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SECEDE, "Expel");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SET_INVITE_DENY_MODE, "Turn on invitation refusal");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANCEL_INVITE_DENY_MODE, "Turn off invitation refusal");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SET_JOIN_DENY_MODE, "Turn on join refusal");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANCEL_JOIN_DENY_MODE, "Turn off join refusal");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_UP_STR, "Raise strength");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_UP_DEX, "Raise dexterity");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_UP_INT, "Raise intelligence");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_SKILL_INFO_WINDOW, "Open the skill info window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_CHARACTER_INFO_WINDOW, "Open the character info window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_MAGIC_INFO_WINDOW, "Open the magic info window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_ADVANTE_INFO_WINODW, "Open the second-class info window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_BLADE_INFO, "Show the blade skill line");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_SWORD_INFO, "Show the sword skill line");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_GUN_INFO, "Show the gun skill line");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_HEAL_INFO, "Show the healing skill line");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_ENCHANT_INFO, "Show the blessing skill line");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_ALL_INFO, "Show every skill line");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_POISON_INFO, "Show the poison skill line");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_ACID_INFO, "Show the acid skill line");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_CURSE_INFO, "Show the curse skill line");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_SUMMON_INFO, "Show the summoning skill line");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_BLOOD_INFO, "Show the blood skill line");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_ESSENCE_INFO, "Show the innate skill line");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_HELP_MAGIC_INFO_WINDOW, "Show help for the magic info window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_HELP_SKILL_INFO_WINDOW, "Show help for the skill info window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_HELP_INVENTORY_WINDOW, "Show help for the inventory window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_HELP_PARTY_MANAGER, "Show help for the party manager window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_HELP_CHARACTER_INFO_WINDOW, "Show help for the character info window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_HELP_GEAR_WINDOW, "Show help for the gear window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_HELP_STORAGE_WINDOW, "Show help for the storage window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_HELP_SHOP_WINDOW, "Show help for the shop window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_SHOW_EXCHANGE_WINDOW, "Show help for the exchange window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_INVENTORY_WINDOW, "Close the inventory window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_PARTY_MANAGER, "Close the party manager window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_CHARACTER_INFO_WINDOW, "Close the character info window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_GEAR_WINDOW, "Close the gear window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_MAGIC_INFO_WINDOW, "Close the magic info window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_SKILL_INFO_WINDOW, "Close the skill info window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_DESC_WINDOW, "Close the description window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_STORAGE_WINDOW, "Close the storage window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_SHOP_WINDOW, "Close the shop window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_BOOKCASE, "Close the bookcase");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_BOOK, "Close the book");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_JOIN_ANY_TEAM, "No team registered");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_JOIN_ANY_CLAN, "No clan registered");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INFRA_RED_HELMET, "Infrared Scanning Helmet");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INFRA_HELMET, "Infra Scanning Helmet");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUIT_COMPUTER, "Shutting down the computer");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_NEXT_PAGE, "Go to the next page");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_PREV_PAGE, "Go to the previous page");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_STR, "STR ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DEX, "DEX ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_INT, "INT ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MASTER_NAME, "GM");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LIMIT_STRING_COUNT, "Do not spam!!!");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HELP_MESSAGE, "Help");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_STR, "Strength");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DEX, "Dexterity");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INT, "Intelligence");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HP, "Health");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MP, "Mana");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCURACY, "Accuracy");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DAMAGE, "Damage");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_AVOID, "Evasion");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DEFENCE, "Defense");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ALIGNMENT, "Alignment");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ALIGN_VERY_BAD, "Very Evil");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ALIGN_BAD, "Evil");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ALIGN_NORMAL, "Neutral");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ALIGN_GOOD, "Good");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ALIGN_VERY_GOOD, "Very Good");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_STR_PURE, "Base Strength : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DEX_PURE, "Base Dexterity : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INT_PURE, "Base Intelligence : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_STATUS_SUM_PURE, "Base Stat Total : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_STATUS_SUM, "Stat Total : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SPEED_SLOW, "Slow");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SPEED_NORMAL, "Normal");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SPEED_FAST, "Fast");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ATTACK_SPEED, "Attack Speed : %s[%d]");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PASSIVE, "Passive");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HAN_PASSIVE, "Passive");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_LEVEL_DESCRIPTION, "LEVEL:%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_EXP_DESCRIPTION, "EXP:%s/%s (remaining:%s)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_MP_DESCRIPTION, "MP:%d/%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_HP_DESCRIPTION_WITH_SILVERING, "HP:%d/%d(S:%d)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_HP_DESCRIPTION, "HP:%d/%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_CHANGE_VAMPIRE_DAY, "You will turn into a vampire in %d days %d hours %d minutes.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_CHANGE_VAMPIRE_HOUR, "You will turn into a vampire in %d hours %d minutes.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_CHANGE_VAMPIRE_MINUTE, "You will turn into a vampire in %d minutes.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_CHANGE_VAMPIRE_SOON, "You will turn into a vampire soon.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_SET_LARGE, "Show the HP bar vertically");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_SET_SMALL, "Show a small HP bar");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_SET_WIDTH, "Show the HP bar horizontally");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_SET_HEIGHT, "Show a large HP bar");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_WIDTH, "Show horizontally.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_HEIGHT, "Show vertically.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_HELP, "Show the help.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_WINDOW, "Close the window.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REGIST, "Register.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LIST_UP, "Previous page");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LIST_DOWN, "Next page");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REFRESH_LIST, "Fetch the list again.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SORT_TEAM_NAME, "Sort by team name.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SORT_LEADER_NAME, "Sort by leader name.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SORT_EXPIRE_DATE, "Sort by expiry date.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SORT_NUMBER_MEMBER, "Sort by member count.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SORT_RANKING, "Sort by rank.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHANGE_FIND_MODE, "Change the search mode");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FIND, "Search");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_GRADE_MASTER, "Master");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_GRADE_SUB_MASTER, "Sub Master");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_GRADE_WAIT, "Awaiting approval");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_GRADE_MEMBER, "Member");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_GRADE, "Grade : %s");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_NAME, "Name : %s");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_LEADER, "Leader : %s");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_REG_FEE, "Reg. Fee : $%s");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_MEMBERS, "Members : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_INTRODUCTION, "Introduction : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_CLAN_INTRODUCTION, "Clan Introduction : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_TEAM_INTRODUCTION, "Team Introduction : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_RANKING, "Ranking : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_TEAM_NAME, "Team Name : %s");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_CLAN_NAME, "Clan Name : %s");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_REGISTRATION_FEE, "Registration Fee : $%s");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_SELF_INTRODUCTION, "Self Introduction : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_UP, "Up");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DOWN, "Down");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_EXPEL, "Expel them.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_JOIN_ACCEPT, "Accept the application");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_JOIN_DENY, "Reject the application");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANCEL, "Cancel");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LIMBOLAIR, "Limbo Lair");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ESLANIA, "Eslania");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RODIN, "Mount Rodin");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DROBETA, "Drobeta");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PERONA, "Perona Highway");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TIMORE, "Lake Timore");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ZONEINFO_XY, "X:%d Y:%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_NULL, "Unassigned");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_F1, "F1");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_F2, "F2");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_F3, "F3");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_F4, "F4");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_F5, "F5");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_F6, "F6");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_F7, "F7");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_F8, "F8");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_F9, "F9");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_F10, "F10");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_F11, "F11");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_F12, "F12");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_ESC, "ESC");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_SKILL, "SKILL");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_INVENTORY, "INVENTORY");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_GEAR, "GEAR");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_CHARINFO, "CHARINFO");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_SKILLINFO, "SKILLINFO");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_MINIMAP, "MINIMAP");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_PARTY, "PARTY");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_MARK, "MARK");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_HELP, "HELP");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_QUICKITEM_SLOT, "QUICK ITEM SLOT");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_EXTEND_CHAT, "EXTEND CHAT");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_CHAR, "CHAT");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_GUILD_CHAT, "GUILD CHAT");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_ZONE_CHAT, "ZONE CHAT");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_WHISPER, "WHISPER");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_CLASS_SWORD, "Sword (SWORD)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_CLASS_BLADE, "Blade (BLADE)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_CLASS_CROSS, "Cross (CROSS)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_CLASS_MACE, "Mace (MACE)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_CLASS_MINE, "Mine (MINE)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_CLASS_BOMB, "Bomb (BOMB)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_CLASS_MINE_MATERIAL, "Mine material (MATERIAL)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_CLASS_BOMB_MATERIAL, "Bomb material (MATERIAL)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_CLASS_SG, "Shotgun (SG)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_CLASS_SMG, "Submachine gun (SMG)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_CLASS_AR, "Assault rifle (AR)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_CLASS_SR, "Sniper rifle (SR)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_CLASS, "Class : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DURABILITY, "Durability : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_SILVERING, "Silver plating : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DAMAGE, "Damage : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_CRITICALHIT, "Critical hit : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DEFENSE, "Defense : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_PROTECTION, "Protection : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_ACCURACY, "Accuracy : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_HP, "HP : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_MP, "MP : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_RANGE, "Range : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_MAGAZINE_NUM, "Rounds : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_POCKET_NUM, "Pockets : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_ITEM_NUM, "Items : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_LEFT_NUM, "Uses left : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_ARRIVAL_LOCATION, "Destination : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_NOT_EXIST, "None");
	// English has no count suffix. Empty assignment stores NULL in MString.
	if (auto* entry = g_pGameStringTable->GetMutable(UI_STRING_MESSAGE_DESC_NUMBER))
		entry->Init(0);
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_MAGAZINE_COUNT, " rounds");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_TILE_PIECE, " tiles");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_PARTY_NAME, "Party");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_OPTION, "Option : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_REQUIRE, "Requires : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_REQUIRE_STAT, "%d or more");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_ONLY_MALE, "Male only");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_ONLY_FEMALE, "Female only");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_ONLY_SLAYER, "Slayers only");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_ONLY_VAMPIRE, "Vampires only");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_ALL_STAT_SUM, "Stat total");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_LEVEL, "Level");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_PRICE, "Price : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_SILVERING_PRICE, "Silver plating price : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_CHARGE_PRICE, "Recharge price : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_REPAIR_PRICE, "Repair price : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CONSUME_HP, "HP cost : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CONSUME_MP, "MP cost : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_LEVEL, "Required level : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_EXP, "Exp : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_LIMIT_LEVEL, "Growth limit level : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_CANNOT_LEARN_SKILL, "You cannot learn this yet.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_CAN_LEARN_SKILL, "You can learn this.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MOVE, "Move there");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANCEL_MOVE, "Cancel the move");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MOVE_F1, "Go to the 1st floor");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MOVE_F2, "Go to the 2nd floor");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MOVE_F3, "Go to the 3rd floor");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MOVE_F4, "Go to the 4th floor");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MOVE_B1, "Go to basement level 1");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_CLICK_EXCHANGE, "Click here to exchange");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_CANCEL_OK_BUTTON, "Take back your OK");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_CANCEL_EXCHANGE, "Cancel the exchange");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_BRING_MONEY, "Take back the money you offered");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_EXCHANGE_MONEY, "Offer money");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_EXCHANGE_YOUR_MONEY, "Money the other player is offering");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_LEFT_MONEY_AFTER_EXCHANGE, "Money left after the exchange");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_WILL_EXCHANGE_MONEY, "Amount to exchange");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TRHOW_MONEY_IN_DIALOG, "Enter the amount to drop.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SAVE_MONEY_IN_DIALOG, "Enter the amount to deposit.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BRING_MONEY_IN_DIALOG, "Enter the amount to withdraw.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TRADE_MONEY_IN_DIALOG, "Enter the amount to trade.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DIVIDE_MONEY_IN_DIALOG, "Enter the amount to split off.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BUY_ITEM, "Buy this item?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BUY_ITEM_NUM, "Buy            of this item?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ASK_DIALOG_BUY_STORAGE, "Buy a storage box for $%d?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ASK_DIALOG_TRADE_OTHER_PLAYER, "Trade with %s?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ASK_DIALOG_WAIT_OTHER_PLAYER, "Waiting for %s to answer.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ASK_DIALOG_CANCEL, "Press Cancel to call it off.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ASK_DIALOG_REQUEST_JOIN, "%s is asking to join your %s.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ASK_DIALOG_INVITE, "%s has invited you to %s.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_FIND_RESULT, "No search results found.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENCHANT_CONFIRM, "Enchant this item?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_ITEM_CLASS_SG, "                        Class : SG");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_ITEM_CLASS_SMG, "                        Class : SMG");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_ITEM_CLASS_AR, "                        Class : AR");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_ITEM_CLASS_SR, "                        Class : SR");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_DURABILITY, "                        Durability : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_SILVERING, "                        Silver plating : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_DAMAGE, "                        Damage : %d~%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_CRITICALHIT, "                        Critical hit : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_DEFENSE, "                        Defense : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_PROTECTION, "                        Protection : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_ACCURACY, "                        Accuracy : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_HP, "                        HP : +%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_MP, "                        MP : +%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_RANGE, "                        Range : %d tiles");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_MAGAZINE_NUM, "                        Rounds : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_POCKET_NUM, "                        Pockets : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_OPTION, "                        Option : %s +%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_OPTION_EMPTY, "                              %s +%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_EMPTY, "                        ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE, "                        Requires : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_STR, "STR %d or more");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_DEX, "DEX %d or more");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_INT, "INT %d or more");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_ALL_STAT_SUM, "Stat total %d or more");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_LEVEL, "Level %d or more");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_PRICE, "                        Price : %s");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FILE_DIALOG_SELECT_PROFILE_PICTURE, "Choose the picture to use on your profile.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FILE_DIALOG_SELECT_FILE, "Choose the files to send (CTRL for multiple).");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FILE_DIALOG_SELECT_FILE_OK, "Select this file.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FILE_DIALOG_CANCEL, "Cancel and close the window.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DOMAIN_BLADE, "BLD");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DOMAIN_SWORD, "SWD");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DOMAIN_ENCHANT, "ENC");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DOMAIN_GUN, "GUN");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DOMAIN_HEAL, "HEL");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_SUPPORT_MENU, "This menu is not supported yet.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_INPUT_ID_OR_PASSWORD, "You did not enter an ID or password.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_WRONG_SSN, "The ID number is not correct.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INPUT_NAME, "Please enter a name.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_CONNECT_SERVER, "Could not connect to the server.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SELECT_CHARACTER, "Please select a character.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DELETE_CHARACTER, "The character was deleted.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ALL_SLOT_EMPTY, "You must create a character first, then select it.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NO_MORE_CREATE_CHARACTER, "You cannot create any more characters.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FAIL_CREATE_CHARACTER, "Character creation failed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NO_INPUT_NEED_INFO, "Not all required fields were filled in.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ALREADY_USE_ID, "That ID is already in use.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAN_USE_ID, "That ID is available.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RE_INPUT_PASSWORD, "Please enter the password again.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_TNDEAD, "Turning Dead");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_ARKHAN, "Arkhan");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_ESTROIDER, "Estroider");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_GOLEMER, "Golemer");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_DARKSCREAMER, "Dark Screamer");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_DEADBODY, "Dead Body");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_MODERAS, "Moderas");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_VANDALIZER, "Vandalizer");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_DIRTYSTRIDER, "Dirty Strider");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_HELLWIZARD, "Hell Wizard");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_TNSOUL, "Turning Soul");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_IRONTEETH, "Iron Teeth");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_REDEYE, "Red Eye");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_CRIMSONSLAUGTHER, "Crimson Slaughter");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_HELLGUARDIAN, "Hell Guardian");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_SOLDIER, "Soldier");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_RIPPER, "Ripper");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_BIGFANG, "Big Fang");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_LORDCHAOS, "Lord Chaos");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_CHAOSGUARDIAN, "Chaos Guardian");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_HOBBLE, "Hobble");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_CHAOSNIGHT, "Chaos Knight");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_WIDOWS, "Widows");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_KID, "Kid");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_NAME_SHADOWWING, "Shadow Wing");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SERVER_STATUS_VERY_GOOD, "Very good");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SERVER_STATUS_GOOD, "Good");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SERVER_STATUS_NORMAL, "Smooth");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SERVER_STATUS_BAD, "Busy");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SERVER_STATUS_VERY_BAD, "Full");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SERVER_STATUS_DOWN, "Server down");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SERVER_STATUS_OPEN, "Open");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SERVER_STATUS_CLOSE, "Closed");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_TYPE_NORMAL, "Normal item");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_TYPE_SPECIAL, "Special item");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_TYPE_MISTERIOUS, "Unknown item");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_STORAGE_FIRST, "First storage box");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_STORAGE_SECOND, "Second storage box");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_STORAGE_THIRD, "Third storage box");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BRING_MONEY_FROM_STORAGE, "Withdraw money.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_DELETE, "Delete.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_CANCEL, "Cancel.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_DELETE_CONFIRM, "Do you really want to delete this character?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RE_INPUT_CORRECT_SSN, "Please enter your ID number correctly.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_NAME, "Name : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_LEVEL, "Level : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_STR_PURE, "Base STR : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_DEX_PURE, "Base DEX : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_INT_PURE, "Base INT : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_SWORD_LEVEL, "Sword domain level : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_BLADE_LEVEL, "Blade domain level : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_GUN_LEVEL, "Gun domain level : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_HEAL_LEVEL, "Heal domain level : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_ENCHANT_LEVEL, "Enchant domain level : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_FAME, "Fame : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_CREATEMSG1, "Press Create to make a");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_CREATEMSG2, "new character.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_GRADE, "Rank : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHAR_MANAGER_GRADE_EXP, "Rank exp");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_ENTER_CHATTING, "Enter-key chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_NORMAL_CHATTING, "Normal chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_3D_ACCEL, "Use 3D acceleration");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_ALPHA_HPBAR, "Transparent HP(MP) bar");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_SHED_BLOOD, "Bleed below 30% HP");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_HIDE_SOFT, "Smooth window auto-hide");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_GAME_BRIGHT, "Game brightness");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_CHATTING_TALK, "Show chat in speech bubbles");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_PUT_FPS, "Show FPS");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_WINDOW_ALPHA, "Semi-transparent windows");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_DENSITY_ALPHA, "Transparency level");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_SOUND_VOLUME, "Sound effect volume");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_MUSIC_VOLUME, "Background music volume");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_SHOW_BASIC_HELP, "Show beginner help");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_NO_LISTEN_BAD_TALK, "Filter out bad language");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_LOAD_ALL_IMAGE, "Preload monster images on map change");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_CHATTING_COLOR_WHITE, "Show all chat in white");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_RUN_TEEN_VERSION, "Run the teen version");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_OPEN_WINDOW_WHEN_WHISPER, "Open the chat window on whisper");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_ACCEL_NAME, "Action : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_ACCEL_KEY, "Shortcut : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_MSG1, "Press the new shortcut key.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_MENU_MSG2, "Press ESC to cancel.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_NOT_SEND_MY_INFO, "Keep my character info private");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_PIVATE, "Private");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_SERENT, "Serent");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_FEACEL, "Feacel");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_LITENA, "Litena");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_KAINEL, "Kainel");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_GENEAL, "Geneal");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_FORE_GENEAL, "Fore Geneal");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_MAJORIS_GENEAL, "Majoris Geneal");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_CLOEL_GENEAL, "Cloel Geneal");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_MARSHAL, "Marshal");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_RITTER, "Ritter");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_REICHSRITTER, "Reichsritter");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_BARONET, "Baronet");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_PREYHER, "Freiherr");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_GRAF, "Graf");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_MARKGRAF, "Markgraf");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_PFALZGRAF, "Pfalzgraf");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_FURST, "Furst");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_HERZOG, "Herzog");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_LANDESHER, "Landesherr");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLIENT_VERSION, "Version");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NETMARBLE_CLIENT_VERSION, "Netmarble version");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OTHER_INFO_STR_PURE, "Base STR : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OTHER_INFO_DEX_PURE, "Base DEX : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OTHER_INFO_INT_PURE, "Base INT : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OTHER_INFO_STR_CUR, "Current STR : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OTHER_INFO_DEX_CUR, "Current DEX : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OTHER_INFO_INT_CUR, "Current INT : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OTHER_INFO_FAME, "Fame");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OTHER_INFO_TEAM_NAME, "Team name : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OTHER_INFO_CLAN_NAME, "Clan name : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TIP_CHANGE_PICTURE_CLICK_HERE, "You can change your picture here");
	g_pGameStringTable->Set(STRING_MESSAGE_UP_TO_GRADE, "You have been promoted.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_NAME, "Rank name :");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_LEVEL, "Rank level :");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DOMAIN_BLADE2, "BLADE");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DOMAIN_SWORD2, "SWORD");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DOMAIN_ENCHANT2, "ENCHANT");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DOMAIN_GUN2, "GUN");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DOMAIN_HEAL2, "HEAL");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_CLASS2, "Class");
	g_pGameStringTable->Set(STRING_MESSAGE_ITEM_TO_ITEM_FAIL_NO_PREMIUM_SLAYER, "You must be a premium user, and it cannot be used inside a guild.");
	g_pGameStringTable->Set(STRING_MESSAGE_ITEM_TO_ITEM_FAIL_NO_PREMIUM_VAMPIRE, "You must be a premium user, and it cannot be used inside a village.");
	g_pGameStringTable->Set(STRING_MESSAGE_DISMISS_AFTER_SECOND, "You will be thrown out in %d seconds.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MODIFY_INFO, "Change the introduction text.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_START_TRACE, "Start tracking");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANCEL_TRACE, "Cancel tracking");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TRACE, "Now tracking %s.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_TRACE, "%s could not be found.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_DO_NOT_WAR_MSG, "Hide war messages");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_DO_NOT_LAIR_MSG, "Hide lair master messages");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_GRADE1_INFO_WINDOW, "Open the rank skill window.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_GRADE2_INFO_WINDOW, "Open the rank skill window.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_CRITICAL_10, "Critical damage +10");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_DEFENSE_5, "Defense +5");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_TOHIT_5, "To hit +5");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_HP_10, "HP +10");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_MP_15, "MP +15");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_DAMAGE_3, "Damage +3");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_ATTACKSPEED_15, "Attack speed +15");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_HP_20, "HP +20");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_DEFENSE_10, "Defense +10");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_PROTECTION_10, "Protection +10");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DECREASE_HP_EXHAUSTION_10_PERCENT, "HP cost of skills -10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_ENCHANT_DAMAGE_10_PERCENT, "Enchant attack damage +10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_HEAL_DAMAGE_10_PERCENT, "Heal attack damage +10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_NEAR_ATTACK_DAMAGE_10_PERCENT, "Melee skill damage +10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_HP_RESTORE_SPEED_15_PERCENT, "HP recovery speed +15%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_RESISTANCE_ACID_15_PERCENT, "Acid resistance +15%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_RESISTANCE_BLOODY_15_PERCENT, "Bloody resistance +15%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_RESISTANCE_CURSE_15_PERCENT, "Curse resistance +15%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_RESISTANCE_POISON_15_PERCENT, "Poison resistance +15%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_DAMAGE_STORM_20_PERCENT, "Storm skill damage +20%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_RANGE_STORM_5_BY_5, "Storm skill range increased to 5*5");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_SUCCESS_RATIO_POISON_10_PERCENT, "Poison success rate +10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_SUCCESS_RATIO_ACID_10_PERCENT, "Acid success rate +10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_SUCCESS_RATIO_CURSE_10_PERCENT, "Curse success rate +10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_SUCCESS_RATIO_BLOODY_10_PERCENT, "Bloody success rate +10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_SUCCESS_RATIO_INNATE_10_PERCENT, "Innate success rate +10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_SUCCESS_RATIO_SUMMON_10_PERCENT, "Summon success rate +10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DECREASE_MP_EXHAUSTION_10_PERCENT, "MP cost of skills -10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_HP_STEAL_2_PERCENT, "HP steal +2%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_MP_STEAL_2_PERCENT, "MP steal +2%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_LUCKY_2, "Lucky +2");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_KEEP_TIME_ACID_SWAMP_20_PERCENT, "Acid Swamp duration +20%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_KEEP_TIME_PARALYZE_20_PERCENT, "Paralyze duration +20%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_KEEP_TIME_DARKNESS_30_PERCENT, "Darkness duration +30%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INCREASE_RANGE_DARKNESS_5_BY_5, "Darkness range increased to 5*5");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_UP_GRADE, "Your rank cannot go any higher.");
	g_pGameStringTable->Set(STRING_MESSAGE_SELECT_PC_CANNOT_PLAY, "You cannot play because this is not a paid account.");
	g_pGameStringTable->Set(STRING_MESSAGE_SELECT_PC_NOT_BILLING_CHECK, "Your payment has not been confirmed yet. Please wait a moment.");
	g_pGameStringTable->Set(STRING_MESSAGE_SELECT_PC_CANNOT_PLAY_BY_ATTR, "This character has reached the free service limit, so a paid subscription is required to log in.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NEVER_CANNOT_LEARN_SKILL, "This skill cannot be learned.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ALREADY_LEARNED_SKILL, "You have already learned this skill.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_LEARN_SKILL_YET, "You cannot learn this yet.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAN_LEARN_SKILL_NOW, "You can learn this now.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_GRADE1, "RANK SKILL 1");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ACCELERATOR_GRADE2, "RANK SKILL 2");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LEARN_GRADE_SKILL_CONFIRM, "Learning this skill locks you out of the other skills of your current rank. Learn it anyway?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TREE_OK, "Use the tree.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TREE_CANCEL, "Do not use the tree.");
	g_pGameStringTable->Set(STRING_MESSAGE_TRADE_GIFT_BOX_OK, "Merry Christmas!");
	g_pGameStringTable->Set(STRING_MESSAGE_TRADE_GIFT_BOX_NO_ITEM, "Come back once a friend has given you a gift!");
	g_pGameStringTable->Set(STRING_MESSAGE_TRADE_GIFT_BOX_ALREADY_TRADE, "You greedy thing, off with you!");
	g_pGameStringTable->Set(STRING_MESSAGE_XMAS_TREE_CANNOT_USE, "This is too close to another tree.");
	g_pGameStringTable->Set(STRING_MESSAGE_XMAS_CARD_CANNOT_USE, "Some fields are still empty. Please fill them all in.");
	g_pGameStringTable->Set(STRING_MESSAGE_NOT_USE_SAFETY_POSITION, "This cannot be used in a safety zone.");
	g_pGameStringTable->Set(STRING_MESSAGE_NOT_USE_SAFETY_ZONE, "This cannot be used inside a village or a guild.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENCHANT_CONFIRM_2, "The item may be destroyed if it fails.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DEPOSIT_LIMIT, "You cannot store more than 2 billion in the storage box.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_WITHDRAW_LIMIT, "You cannot carry more than 2 billion.");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_PREMIUM_HALF_SLAYER, "Ampoules are half price in this zone.");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_PREMIUM_HALF_VAMPIRE, "Serum is half price in this zone.");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_PREMIUM_HALF_SLAYER_END, "The half-price ampoule event has ended.");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_PREMIUM_HALF_VAMPIRE_END, "The half-price serum event has ended.");
	g_pGameStringTable->Set(STRING_MESSAGE_REWARD_OK, "You received your reward.");
	g_pGameStringTable->Set(STRING_MESSAGE_REWARD_FAIL, "You cannot receive the reward.");
	g_pGameStringTable->Set(STRING_MESSAGE_NO_EMPTY_SLOT, "There is no free slot.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OPTION_DO_NOT_HOLY_LAND_MSG, "Hide Adam's Holy Land messages");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HOLY_LAND_TOTAL_FEE, "Total tax");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HOLY_LAND_CAN_BRING_FEE, "Tax to collect");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HOLY_LAND_TOTAL_FEE_DESC, "Total tax available to collect");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HOLY_LAND_INPUT_BRING_FEE, "Enter the tax you want to collect");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HOLY_LAND_CLICK_INPUT_FEE, "Click to enter the tax you want to collect");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HOLY_LAND_OK, "Collect the tax you entered");
	g_pGameStringTable->Set(STRING_MESSAGE_NO_TEAM, "You do not belong to a team.");
	g_pGameStringTable->Set(STRING_MESSAGE_NO_CLAN, "You do not belong to a clan.");
	g_pGameStringTable->Set(STRING_MESSAGE_NOT_TEAM_MASTER, "You are not the team master.");
	g_pGameStringTable->Set(STRING_MESSAGE_NOT_CLAN_MASTER, "You are not the clan master.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_HAS_NO_CASTLE, "Your team does not hold a castle.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_HAS_NO_CASTLE, "Your clan does not hold a castle.");
	g_pGameStringTable->Set(STRING_MESSAGE_TEAM_NOT_YOUR_CASTLE, "This castle does not belong to your team.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLAN_NOT_YOUR_CASTLE, "This castle does not belong to your clan.");
	g_pGameStringTable->Set(STRING_MESSAGE_SUCCESS_BRING_FEE, "You collected the tax.");
	g_pGameStringTable->Set(STRING_MESSAGE_FAIL_BRING_FEE, "Collecting the tax failed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BRING_FEE_MSG, "The team or clan master that owns this castle can collect the tax it earns. Press the button to the right of the tax field to take up to the full amount. The tax you collect plus the money you already carry cannot exceed 2 billion.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BRING_FEE_LIMIT, "The tax you collect plus the money you carry cannot exceed 2 billion.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RACE_WAR, "Race War");
	g_pGameStringTable->Set(STRING_MESSAGE_WAR_SCHEDULE_FULL, "The war schedule is full.");
	g_pGameStringTable->Set(STRING_MESSAGE_WAR_ALREADY_REGISTERED, "You have already applied for a war.");
	g_pGameStringTable->Set(STRING_MESSAGE_WAR_NOT_ENOUGH_MONEY, "You do not have enough money.");
	g_pGameStringTable->Set(STRING_MESSAGE_WAR_REGISTRATION_OK, "You have been added to the war schedule.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_BLOOD_BIBLE, "You cannot enter your own safety zone while carrying the Blood Bible!");
	g_pGameStringTable->Set(STRING_MESSAGE_ALREADY_HAS_CASTLE, "You already hold a castle.");
	g_pGameStringTable->Set(STRING_MESSAGE_WAR_UNAVAILABLE, "War applications are not being accepted right now.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_STATUS_TIME_FORMAT, "%dh %dm");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_STATUS_LEFT_TIME, "Time left");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GUILD_WAR, "Guild War");
	g_pGameStringTable->Set(STRING_MESSAGE_BLOOD_BIBLE_BONUS_ARMEGA, "MP(HP) cost of skills -50%");
	g_pGameStringTable->Set(STRING_MESSAGE_BLOOD_BIBLE_BONUS_MIHOLE, "Lucky +10");
	g_pGameStringTable->Set(STRING_MESSAGE_BLOOD_BIBLE_BONUS_KIRO, "INT +7, DEX +7");
	g_pGameStringTable->Set(STRING_MESSAGE_BLOOD_BIBLE_BONUS_INI, "Physical attack damage +10");
	g_pGameStringTable->Set(STRING_MESSAGE_BLOOD_BIBLE_BONUS_GREGORI, "Sight +5, all stats +4");
	g_pGameStringTable->Set(STRING_MESSAGE_BLOOD_BIBLE_BONUS_CONCILIA, "All resistances +9");
	g_pGameStringTable->Set(STRING_MESSAGE_BLOOD_BIBLE_BONUS_LEGIOS, "Magic attack damage +10");
	g_pGameStringTable->Set(STRING_MESSAGE_BLOOD_BIBLE_BONUS_HILLEL, "Translates other races' speech");
	g_pGameStringTable->Set(STRING_MESSAGE_BLOOD_BIBLE_BONUS_JAVE, "Gambling costs halved");
	g_pGameStringTable->Set(STRING_MESSAGE_BLOOD_BIBLE_BONUS_NEMA, "Potions cost halved");
	g_pGameStringTable->Set(STRING_MESSAGE_BLOOD_BIBLE_BONUS_AROSA, "HP +50");
	g_pGameStringTable->Set(STRING_MESSAGE_BLOOD_BIBLE_BONUS_CHASPA, "STR +7, INT +7");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_ARMEGA, "Armega");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_MIHOLE, "Mihole");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_KIRO, "Kiro");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_INI, "Ini");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_GREGORI, "Gregori");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_CONCILIA, "Concilia");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_LEGIOS, "Legios");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_HILLEL, "Hillel");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_JAVE, "Jave");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_NEMA, "Nema");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_AROSA, "Arosa");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_CHASPA, "Chaspa");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_ARMEGA_ENG, "ARMEGA");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_MIHOLE_ENG, "MIHOLE");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_KIRO_ENG, "KIRO");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_INI_ENG, "INI");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_GREGORI_ENG, "GREGORI");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_CONCILIA_ENG, "CONCILIA");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_LEGIOS_ENG, "LEGIOS");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_HILLEL_ENG, "HILLEL");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_JAVE_ENG, "JAVE");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_NEMA_ENG, "NEMA");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_AROSA_ENG, "AROSA");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_CHASPA_ENG, "CHASPA");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_DROP, "Lying on the ground");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_HAS_SLAYER, "Held by a slayer");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_HAS_VAMPIRE, "Held by a vampire");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_NONE, "No information available");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_SLAYER, "In the slayers' shrine");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_VAMPIRE, "In the vampires' shrine");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_DESC_POSITION, "Location : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_DESC_STATUS, "Status : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_DESC_PLAYER, "Carried by : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_ATTACK_GUILD, "Attacking guild");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_DEFENSE_GUILD, "Defending guild");
	g_pGameStringTable->Set(STRING_MESSAGE_RACE_WAR_JOIN_FAILED, "The race war roster for %s's level range is full.");
	g_pGameStringTable->Set(STRING_MESSAGE_RACE_WAR_JOIN_OK, "You have signed up for the race war.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_DESC_SHRINE_RACE, "Last owner : ");
	g_pGameStringTable->Set(STRING_MESSAGE_SLAYER, "Slayer");
	g_pGameStringTable->Set(STRING_MESSAGE_VAMPIRE, "Vampire");
	g_pGameStringTable->Set(STRING_MESSAGE_RACE_WAR_GO_FIRST_SERVER, "You can only sign up for and join the race war on the first server of each world.");
	g_pGameStringTable->Set(STRING_MESSAGE_GIVE_EVENT_ITEM_FAIL_NOW, "You cannot receive the event item right now.");
	g_pGameStringTable->Set(STRING_MESSAGE_GIVE_EVENT_ITEM_FAIL, "You cannot receive the event item.");
	g_pGameStringTable->Set(STRING_MESSAGE_GIVE_EVENT_ITEM_OK, "You received the event item.");
	g_pGameStringTable->Set(STRING_MESSAGE_GIVE_PREMIUM_USER_ONLY, "Only premium service users can receive this.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_MEET_SUCCESS, "You are now a couple.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_CANNOT_MEET, "You cannot become a couple.");
	g_pGameStringTable->Set(STRING_MESSAGE_MEET_WAIT_TIME_EXPIRED, "The request timed out and was cancelled.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_APART_SUCCESS, "You have broken up.");
	g_pGameStringTable->Set(STRING_MESSAGE_NOT_COUPLE, "You are not a couple, so you cannot break up.");
	g_pGameStringTable->Set(STRING_MESSAGE_APART_WAIT_TIME_EXPIRED, "The request timed out and was cancelled.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HOPE_COUPLE_MSG, "Becoming a couple takes the consent of both partners. Enter the name of the one you wish to pair with; if they come to me and agree within one minute, the bond is sealed. Now, please enter the name of the one you love.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BREAK_UP_COUPLE_MSG, "A mutual parting takes the consent of both partners. Enter the name of the one you wish to part from; if they come to me and agree within one minute, the parting is sealed. The couple ring, the token of your love, disappears with it. Now, please enter your partner's name.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPLETE, "Done");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OTHER_PLAYER_NAME, "The other character's name");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FORCE_BREAK_UP_COUPLE, "A one-sided parting needs no consent, but it costs you a large part of your alignment. Enter your partner's name and the parting is sealed. The couple ring, the token of your love, disappears with it. Now, please enter your partner's name.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE, "Couple");
	g_pGameStringTable->Set(STRING_MESSAGE_MOVE_DELAY_SEC, "You will be moved in %d seconds.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_MOVE_START, "Looking for where your partner is.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_CAN_NOT_FIND, "Your partner could not be found.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_NOT_EVENT_TERM, "The couple event is not running.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_ALREADY_WAITING, "You are already waiting for someone.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_LOGOFF, "The other player is not logged in.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_DIFFERENT_RACE, "You are of different races.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_SAME_SEX, "Only a man and a woman can become a couple.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_NOT_PAY_PLAYER, "You are not a paying user.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_ALREADY_COUPLE, "You are already in a couple.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_WAS_COUPLE, "You have been in a couple before.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_NOT_ENOUGH_GOLD, "You do not have enough money.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_NOT_ENOUGH_ATTR, "Your stats are too low.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_NOT_ENOUGH_LEVEL, "Your level is too low.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_INVENTORY_FULL, "There is no room for the couple ring.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_NO_WAITING, "No partner is waiting for you.");
	g_pGameStringTable->Set(STRING_MESSAGE_COUPLE_NOT_COUPLE, "You are not in a couple.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LOVE_CHAIN, "A skill that moves you to where your partner is.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_WILL_YOU_GO_BILING_PAGE, "Go to the payment page?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GO_BILING_PAGE, "Go to the payment page.");
	g_pGameStringTable->Set(STRING_MESSAGE_LOGOUT_BY_PAYTIME, "Your paid play time has expired.");
	g_pGameStringTable->Set(STRING_MESSAGE_LOGOUT_BY_FREEPLAY_LEVEL, "Your stats have reached the free service limit.");
	g_pGameStringTable->Set(STRING_MESSAGE_LOGOUT_BY_LEVEL, "Your level has reached the free service limit.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_UP_LEVEL_BY_FAME, "Your fame is too low to level up.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NEED_FAME, "Fame required");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_HAN, "Quest");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_ENG, "Quest");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DAY, "%dd");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HOUR, "%dh");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MINUTE, "%dm");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SECOND, "%ds");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LEFT_TIME, "Time left :");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EXPIRED_ITEM, "This disappears when you log out.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SELECT_QUEST_SLAYER, "E.V.E. has issued new orders in response to the rapidly growing vampire population. Pick a hunt and wipe out the number of vampires it names. You may only run one hunt at a time, and logging out cancels it. The number of vampires depends on the hunt you choose, and E.V.E. pays a special reward for finishing one. Choose the hunt you want.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SELECT_QUEST_VAMPIRE, "Will you take on the work I have for you? Hah... call it a hobby of mine, but I cannot abide the imperfect. For the purity and nobility of our vampire blood, the lesser ones must be cleared away. So, will you help me clear them out? You may only run one hunt at a time, and logging out cancels it. The number varies with the kind of creature, so just go and hunt them down. The pay is generous. Choose your hunt.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANCEL_SELECT_QUEST, "I will take part another time.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANCEL_LEARN_SKILL, "I will learn it next time.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NUMBER_OF_ANIMALS, "");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EXPIRED_TIME_MONSTER_KILL_QUEST, "The monster hunt quest has run out of time.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FINISH_MONSTER_KILL_QUEST, "You have hunted every target monster.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAN_REQUITAL_FROM_NPC, "You can claim your reward from the NPC.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_START_MONSTER_KILL_QUEST, "The monster hunt quest has begun.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FAIL_ALREADY_START_MONSTER_KILL_QUEST, "A quest is already in progress.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FAIL_MONSTER_KILL_QUEST_BY_STATUS, "Your stats do not qualify you for this quest.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SUCCESS_MONSTER_KILL_QUEST, "You completed the monster hunt quest.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_YET_COMPLETE_MONSTER_KILL_QUEST, "The monster hunt quest is not finished yet.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST, "There is no room in your inventory.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_IN_QUEST, "You are not on a quest.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FAIL_QUEST_EXPIRED_TIME, "You ran out of time and failed the quest.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TIME_LIMIT, "Time limit :");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_APPLY_QUEST, "You cannot take a quest right now.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPLETE_QUEST, "Quest objective complete");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FAIL_TIME_OVER_QUEST, "Out of time");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MEET_NPC, "Meet %s");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FAIL_INVALID_NPC, "This NPC cannot give you the reward.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FAIL_BUG, "An error occurred. Please try again.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GATHER_ITEM, "Find %s");
	g_pGameStringTable->Set(STRING_MESSAGE_MONSTER_KILL_QUEST_STRING_SET, "Then bring me %s, %d of them, within %s.");
	g_pGameStringTable->Set(STRING_MESSAGE_MONSTER_KILL_QUEST_STRING_SET_VAMPIRE, "Then bring me %s, %d of them, within %s.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANCEL_MONSTER_KILL_QUEST, "I will check my gear and come back.");
	g_pGameStringTable->Set(STRING_MESSAGE_YES_I_SEE, "Yes, understood.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_MONSTER_KILL, "Monster hunt quest");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_MEET_NPC, "Meet an NPC quest");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_GATHER_ITEM, "Gather items quest");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_DESCRIPTION_TIME_TOTAL, "Total time limit : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_DESCRIPTION_TIME_ELAPSE, "Time elapsed : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_DESCRIPTION_TIME_REMAIN, "Time remaining : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_DESCRIPTION_TIME_NO_REMAIN, "None");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FAIL_QUEST, "Quest failed");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SELECT_EVENT_GIFT, "Choose one of the stage %d event prizes");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PLEASE_SCRATCH_IMAGE, "Please scratch the picture above.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_WIN_A_PRIZE_SAME_IMAGE, "Three matching pictures wins a prize.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CONGRATULATIONS, "Congratulations.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_WIN_A_PRIZE, " has won.");
	g_pGameStringTable->Set(STRING_MESSAGE_MEET_NPC_SLAYER, "A resident living near Eslania has reported seeing a strangely featured person, thought to be an Ousters. Find the witness at once and get as much information out of them as you can.");
	g_pGameStringTable->Set(STRING_MESSAGE_MEET_NPC_VAMPIRE, "Two residents who claim to have seen a strangely featured person, thought to be an Ousters, are wandering near Limbo Lair without a care. Find them at once and get as much information out of them as you can.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANCEL_QUEST_VAMPIRE, "I will replenish my magic and come back.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FAIL_LOTTERY, "Sorry, no prize this time. Come again. Finishing another quest earns you another ticket.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_WIN_A_PRIZE2, " has won a prize.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_USER_1, "");
	g_pGameStringTable->Set(STRING_MESSAGE_RIPATY_SCRIPT_1, "The spirit stone you brought has pulled Calisus' soul back from the world of the dead.");
	g_pGameStringTable->Set(STRING_MESSAGE_RIPATY_SCRIPT_2, "Look. Our kin, restored to life... and we owe it all to you.");
	g_pGameStringTable->Set(STRING_MESSAGE_RIPATY_SCRIPT_3, "The thirteenth Blood Bible? Hahahaha...");
	g_pGameStringTable->Set(STRING_MESSAGE_RIPATY_SCRIPT_4, "Humans. How very foolish you are.");
	g_pGameStringTable->Set(STRING_MESSAGE_RIPATY_SCRIPT_5, "It is far too dangerous for such a thing to fall into worldly hands.");
	g_pGameStringTable->Set(STRING_MESSAGE_RIPATY_SCRIPT_6, "Our kind awoke for the sole purpose of guarding it.");
	g_pGameStringTable->Set(STRING_MESSAGE_RIPATY_SCRIPT_7, "Ever since we came here, we have watched over Adam's Holy Land.");
	g_pGameStringTable->Set(STRING_MESSAGE_RIPATY_SCRIPT_8, "You have learned far too much.");
	g_pGameStringTable->Set(STRING_MESSAGE_RIPATY_SCRIPT_9, "May you keep your silence in the eternal dark.");
	g_pGameStringTable->Set(STRING_MESSAGE_AMATA_SCRIPT_1, "The spirit stone you brought has pulled Calisus' soul back from the world of the dead.");
	g_pGameStringTable->Set(STRING_MESSAGE_AMATA_SCRIPT_2, "Look. Our kin, restored to life... and we owe it all to you.");
	g_pGameStringTable->Set(STRING_MESSAGE_AMATA_SCRIPT_3, "The thirteenth Blood Bible? Hahahaha...");
	g_pGameStringTable->Set(STRING_MESSAGE_AMATA_SCRIPT_4, "Five hundred years, and that filthy greed of yours has not changed.");
	g_pGameStringTable->Set(STRING_MESSAGE_AMATA_SCRIPT_5, "It is far too dangerous in the hands of creatures who bathe in blood every night.");
	g_pGameStringTable->Set(STRING_MESSAGE_AMATA_SCRIPT_6, "Our kind awoke for the sole purpose of guarding it.");
	g_pGameStringTable->Set(STRING_MESSAGE_AMATA_SCRIPT_7, "Ever since we came here, we have watched over Adam's Holy Land.");
	g_pGameStringTable->Set(STRING_MESSAGE_AMATA_SCRIPT_8, "You have learned far too much.");
	g_pGameStringTable->Set(STRING_MESSAGE_AMATA_SCRIPT_9, "May you keep your silence in the eternal dark.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_NAME_FIND_ANCIENT_DOCUMENT, "Find the ancient document quest");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_NAME_FIND_ANCIENT_MAP, "Find the ancient map quest");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_NAME_FIND_SOUL_STONE, "Find the spirit stone quest");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_IN_QUEST2, "You have no quest in progress.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TAKE_OUT_OK, "The item was moved to your inventory.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TAKE_OUT_FAIL, "The item could not be moved to your inventory.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_TAKE_OUT_ITEM_FROM_SHOP, "The item you bought cannot be collected right now.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CONFIRM_SELECT_ITEM_FROM_SHOP, "Take the item you selected?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_SHOP, "Click an item name to take that item.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLEAR_RANK_BONUS_OK, "The rank skill you selected was removed.");
	g_pGameStringTable->Set(STRING_MESSAGE_NO_RANK_BONUS, "You do not qualify for that.");
	g_pGameStringTable->Set(STRING_MESSAGE_ALREADY_CLEAR_RANK_BONUS, "You have already removed that rank skill once.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BULLETIN_BOARD_OK, "Use the bulletin board.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BULLETIN_BOARD_CANCEL, "Do not use the bulletin board.");
	g_pGameStringTable->Set(STRING_MESSAGE_BULLETIN_BOARD_CANNOT_USE, "This is too close to another bulletin board.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAN_TRANS, "The gender restriction on this item can be changed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TRANS_ITEM, "Change the gender restriction on this item?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RESURRECT_BY_ELIXIR, "Resurrect with an elixir");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RESURRECT_BY_SCROLL, "Resurrect with a resurrection scroll");
	g_pGameStringTable->Set(UI_STRING_CANNOT_USE, "You cannot use this.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MIXING_FORGE_OK, "Use the mixing forge.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MIXING_FORGE_CANCEL, "Do not use the mixing forge.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPLETE_MERGE_ITEM, "The two items were merged successfully.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FAIL_MERGE_ITEM, "Merging the items failed.");
	g_pGameStringTable->Set(STRING_MESSAGE_USE_GUILD_MEMBER_ONLY, "Only members of the guild that owns the castle can use this.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_USE_RIDE_MOTORCYCLE, "You cannot use this while riding a motorcycle.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_MIXING_SPECIAL_ITEM, "Unique and limited items cannot be mixed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_MIXING_OPTION_COUNT, "Only items with exactly one option can be mixed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_MIXING_ITEM_CLASS, "The items are of different classes, so they cannot be mixed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_MIXING_ITEM_TYPE, "The items are of different types, so they cannot be mixed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_MIXING_ITEM_OPTION, "Both items have the same option, so you cannot select it.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_WHAT_OPTION_REMOVE, "Which option do you want to remove?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CONFIRM_REMOVE_OPTION, "Do you really want to remove the option you selected?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAN_REMOVE_OPTION, "This item's option can be removed.");
	g_pGameStringTable->Set(UI_STRING_MESSGAE_CANNOT_REMOVE_OPTION, "This item's option cannot be removed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SELECT_OPTION, "the %s option");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUESTION_REMOVE_OPTION, "Remove it?");
	g_pGameStringTable->Set(STRING_MESSAGE_FAILED_REMOVE_OPTION, "Removing the option failed.");
	g_pGameStringTable->Set(STRING_MESSAGE_SUCCESS_REMOVE_OPTION, "The option you selected was removed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_OTHER_TRIBE, "Other races only");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CONSUME_EP, "EP cost : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_OUSTERS_CIRCLET, "Wear a circlet.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_OUSTERS_COAT, "Wear a coat.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_OUSTERS_WEAPON, "Equip a chakram/wristlet.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_OUSTERS_BOOTS, "Wear boots.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_OUSTERS_ARMSBAND, "Wear an armsband.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_OUSTERS_RING, "Wear a ring.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_OUSTERS_PENDENT, "Wear a pendant.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_OUSTERS_STONE, "Equip a spirit stone.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OUSTERS_STONE, "%s spirit level :");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_MALCHUT, "Malchut");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_YESOD, "Yesod");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_HOD, "Hod");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_NETRETH, "Netzach");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_TIPHRETH, "Tiphereth");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_GEBURAH, "Geburah");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_CHESED, "Chesed");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_BINAH, "Binah");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_CHOKMA, "Chokmah");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GRADE_KEATHER, "Kether");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SKILL_LEVEL, "Skill Level : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_ELEMENTAL_LEVEL, "Required element level (%s)(%d)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LEARN_SKILL, "Learn this skill?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LEARN_SKILL2, "Learning this skill fixes your line. From the next skill on you can only learn skills from this line.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SKILL_LEVEL_UP, "Raise the level of this skill?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_SKILL_POINT, "Skill points required : %d Point");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ELEMENTAL_FIRE, "Fire");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ELEMENTAL_WATER, "Water");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ELEMENTAL_EARTH, "Earth");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ELEMENTAL_WIND, "Wind");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ELEMENTAL_SUM, "Total");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EP, "Spirit");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_SHOW_GUILD_CHATTING, "Hide guild chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_GUILD_CHATTING, "Show guild chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GUILD_CHATTING, "Guild chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_WOOD_SKIN, "Protection +15");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_WIND_SENSE, "Defense +10");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_HOMING_EYE, "To Hit +10");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_LIFE_ENERGY, "HP +15");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_SOUL_ENERGY, "EP +25");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_STONE_MAUL, "Combat skill damage +5");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_SWIFT_ARM, "Attack speed +20");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_FIRE_ENDOW, "Fire attack magic damage +3");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_WATER_ENDOW, "Water attack magic damage +3");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_EARTH_ENDOW, "Earth attack magic damage +3");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_ANTI_ACID_SKIN, "Acid resistance +15%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_ANTI_BLOODY_SKIN, "Bloody resistance +15%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_ANTI_CURSE_SKIN, "Curse resistance +15%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_ANTI_POISON_SKIN, "Poison resistance +15%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_ANTI_SILVER_DAMAGE_SKIN, "Silver damage -20%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_BLESS_OF_NATURE, "EP cost of skills -20%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_LIFE_ABSORB, "HP steal +2%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_SOUL_ABSORB, "EP steal +2%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_MYSTIC_RULE, "Lucky +2");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_EP_DESCRIPTION, "EP:%d/%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LEFT_BONUS_POINT, "You still have bonus points to spend.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GUILD, "Guild");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GUILD_INFO, "Guild Info");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GUILD_MEMBER_LIST, "Guild Member List");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GUILD_HELP, "Guild Help");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ELEMENTAL_FIRE_DESCRIPTION, "Fire:%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ELEMENTAL_WATER_DESCRIPTION, "Water:%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ELEMENTAL_EARTH_DESCRIPTION, "Earth:%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ELEMENTAL_WIND_DESCRIPTION, "Wind:%d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_EP, "EP : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PREV_MAP, "Show the previous map.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NEXT_MAP, "Show the next map.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_HORN, "Put away the horn of the earth spirit.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_ATTACK_SPEED, "Attack speed : ");
	g_pGameStringTable->Set(STRING_STATUS_EP_MAX_1, "Your maximum EP is now %d.");
	g_pGameStringTable->Set(STRING_STATUS_EP_MAX_2, "Your maximum EP is now %d.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_MAGIC_DAMAGE, "Magic damage :");
	g_pGameStringTable->Set(STRING_MESSAGE_OPEN_LAIR, "%s has opened.");
	g_pGameStringTable->Set(STRING_MESSAGE_CLOSED_LAIR, "%s has closed.");
	g_pGameStringTable->Set(STRING_MESSAGE_LEFT_TIME_LAIR, "%s stays open for another %d minutes.");
	g_pGameStringTable->Set(STRING_MESSAGE_CONTRACT_GNOMES_HORN, "You must go to Sioram and sign a contract before you can use this.");
	g_pGameStringTable->Set(STRING_MESSAGE_CONTRACT_GNOMES_HORN_OK, "You signed the contract to use the horn of the earth spirit.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_DOWN_SKILL, "The skill level cannot be lowered.");
	g_pGameStringTable->Set(STRING_MESSAGE_NOT_ENOUGH_MONEY_FOR_DOWN_SKILL, "You do not have enough money to lower the skill level.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CONFIRM_DOWN_SKILL, "The level of %s will change from %d to %d. It costs $%s. Change it?");
	g_pGameStringTable->Set(STRING_MESSAGE_SUCCESS_CHANGE, "The change was successful.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_WITHDRAW_POINT, "Skill points refunded : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DOWN_SKILL, "Lower the level of this skill?");
	g_pGameStringTable->Set(STRING_MESSAGE_NOT_OUSTERS, "You are not an Ousters.");
	g_pGameStringTable->Set(STRING_MESSAGE_TOO_LOW_SKILL_LEVEL, "The skill level is too low.");
	g_pGameStringTable->Set(STRING_MESSAGE_TOO_HIGH_SKILL_LEVEL, "The skill level is too high.");
	g_pGameStringTable->Set(STRING_MESSAGE_INVALID_SKILL, "That skill is not valid.");
	g_pGameStringTable->Set(STRING_MESSAGE_NOT_LEARNED_SKILL, "You have not learned that skill yet.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CONFIRM_UP_TO_LAST_SKILL_LEVEL, "Once a skill reaches its maximum level of 30, its points can no longer be refunded. Raise it to level 30?");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_USE_OUSTERS, "Ousters cannot use this.");
	g_pGameStringTable->Set(STRING_MESSAGE_MIXING_FORGE_FAILED_SAME_OPTION_GROUP, "The items share an option group, so they cannot be mixed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CONFIRM_CHANGE_SEX, "Do you really want to change gender?");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_CHANGE_SEX_BY_WEAR, "You cannot change gender while wearing clothes.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_CHANGE_SEX_BY_COUPLE, "A character in a couple cannot change gender.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EVENT_QUEST2_1, "1. Judgement of Wisdom\nGoal: solve the puzzle you are given");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EVENT_QUEST2_2, "2. Key of the Barrier\nGoal: collect the ores set for your level (8 of them)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EVENT_QUEST2_3, "3. The Invisible Wall\nGoal: cross the maze and obtain Rifinium");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EVENT_QUEST2_4, "4. The Talking Doll\nGoal: find and assemble the puzzle pieces in the given dungeon");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EVENT_QUEST2_5, "5. Gate to the Future\nGoal: find mana stones on monsters of a certain class and assemble the code table");
	g_pGameStringTable->Set(STRING_MESSAGE_SELECT_MINI_GAME, "There are two trials prepared. Choose one of them.");
	g_pGameStringTable->Set(STRING_MESSAGE_SELECT_ARROW_TILES, "Arrow Tiles. Follow the arrows and reach the goal safely.");
	g_pGameStringTable->Set(STRING_MESSAGE_SELECT_CRAZY_MINE, "Crazy Mine. A puzzle game mixing number baseball with minesweeping.");
	g_pGameStringTable->Set(STRING_MESSAGE_GET_RIFINIUM, "You obtained Rifinium.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_STATUS_ARROW_TILES, "Arrow Tiles");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_QUEST_STATUS_CRAZY_MINE, "Crazy Mine");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SUCCESS_MINIGAME, "All Stage Clear!");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_REGIST_FAIL_ALREADY_JOIN, "Let me see... it says here you already belong to the %s guild.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_REGIST_FAIL_QUIT_TIMEOUT, "You left your last guild only moments ago. Think it over before you act.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_REGIST_FAIL_CANCEL_TIMEOUT, "Your guild was disbanded only moments ago. Train until you meet the requirements, and wait for a better opening.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_REGIST_FAIL_LEVEL, "You are capable, but not yet leader material. Come back when you are stronger.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_REGIST_FAIL_MONEY, "Founding a guild takes a great deal of money, and you do not seem to have it.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_REGIST_FAIL_FAME, "%s, is it... I have never heard that name. That makes you a novice. Come back when you have made a name for yourself.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_REGIST_FAIL_NAME, "That guild name is already taken. Think of another one.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_REGIST_FAIL_DENY, "Your application was rejected.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_STARTING_FAIL_ALREADY_JOIN, "You already belong to another guild.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_STARTING_FAIL_QUIT_TIMEOUT, "You left your last guild only moments ago. Think it over before you act.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_STARTING_FAIL_CANCEL_TIMEOUT, "Your guild was disbanded only moments ago. Train until you meet the requirements, and wait for a better opening.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_STARTING_FAIL_LEVEL, "You still have a lot to learn. Come back when you have trained a while longer.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_STARTING_FAIL_MONEY, "%s, registering a guild costs more money than that.");
	g_pGameStringTable->Set(STRING_MESSAGE_GUILD_STARTING_FAIL_FAME, "%s, is it... I have never heard that name. That makes you a novice. Come back when you have made a name for yourself.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLICK_TO_SHOW_DETIAL, "Click to see the details.");
	g_pGameStringTable->Set(STRING_MESSAGE_OUSTERS, "Ousters");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_FLAG_WAR_READY, "Capture the Flag starts in 5 minutes!");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_FLAG_WAR_START, "Capture the Flag has begun!");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_FLAG_WAR_FINISH, "Capture the Flag is over. Items drop in 3 minutes.");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_FLAG_WAR_WINNER, "The %s gathered %d flags and won.");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_FLAG_WILL_POUR_ITEM_AFTER_3MIN, "Items burst out at the flag area in 3 minutes.");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_FLAG_POURED_ITEM, "The Capture the Flag event items have appeared.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GET_EVENT_FLAG_STATUS, "Flags captured : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MUTE, "A GM has muted your chat.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_FLAG, "You cannot enter a safety zone while carrying a flag.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_DROP_ITEM_BY_FLAG, "You cannot drop items near the flagpole.");
	g_pGameStringTable->Set(STRING_MESSAGE_POUR_ITEM_AFTER_SECOND, "Items burst out at the flag area in %d seconds.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_ACTION_MOTORCYCLE_FLAG, "You cannot mount or dismount a motorcycle near the flagpole.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_GUILD_NAME, "Guild Name : %s");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_INFO_GUILD_INTRODUCTION, "Guild Introduction : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_OTHER_INFO_GUILD_NAME, "Guild name : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_JOIN_ANY_GUILD, "No guild registered");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_USE_ETERNITY_FOR_RESURRECT, "You are revived by the Eternity skill.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_RELOAD_BY_VIVID_MAGAZINE, "You must learn VIVID MAGAZINE before you can load this.");
	g_pGameStringTable->Set(STRING_MESSAGE_RESURRECT_AFTER_SECONDS, "You will resurrect in %d seconds.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_TRADE_SUMMON_SYLPH, "You cannot trade while riding the wind spirit.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_USE_SUMMON_SYLPH, "You cannot use this while riding the wind spirit.");
	g_pGameStringTable->Set(STRING_MESSAGE_MODIFY_SKILL_LEVEL_1, "Your %s skill level is now %d.");
	g_pGameStringTable->Set(STRING_MESSAGE_MODIFY_SKILL_LEVEL_2, "Your %s skill level is now %d.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GO_TO_BEGINNER_ZONE, "Move to the beginners' hunting ground.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LEFT_PREMIUM_DAYS, "%d days left on your premium service.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_PREMIUM_USER, "You are not a premium service user.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_TODAY, "Your premium service ends today.");
	g_pGameStringTable->Set(STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_MESSAGE_1, "Renew before your premium service ends and you keep your mileage as well as");
	g_pGameStringTable->Set(STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_MESSAGE_2, "item lottery tickets and other benefits.");
	g_pGameStringTable->Set(STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_MESSAGE_3, "See the Dark Eden homepage (www.darkeden.com) for details.");
	g_pGameStringTable->Set(STRING_MESSAGE_LEVEL_WAR_ZONE_NAME, "Caligo Dungeon %dF");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWEEPER_BONUS_1, "All stats + 2");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWEEPER_BONUS_2, "HP + 20");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWEEPER_BONUS_3, "Damage + 3");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWEEPER_BONUS_4, "INT + 7, DEX + 7");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWEEPER_BONUS_5, "HP + 50");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWEEPER_BONUS_6, "Lucky + 7");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWEEPER_BONUS_7, "Magic attack damage + 10");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWEEPER_BONUS_8, "INT + 7, STR + 7");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWEEPER_BONUS_9, "HP steal + 15");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWEEPER_BONUS_10, "Physical attack damage + 10");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWEEPER_BONUS_11, "All resistances + 7");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWEEPER_BONUS_12, "Sight + 5, all stats + 4");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_JOIN_LEVEL_WAR, "Join the war.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLICK_TO_WARP_REGEN_TOWER, "Click to move to that regen zone tower.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_HAS_OUSTERS, "Held by an Ousters");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_OUSTERS, "In the Ousters' shrine");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MAILBOX, "Mail Box");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_MAILBOX, "Close the mail box.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MAILBOX_TAB_MAIL, "Check your mail.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MAILBOX_TAB_HELP, "Check the help.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MAILBOX_TAB_MEMO, "Check your notes.");
	g_pGameStringTable->Set(STRING_MESSAGE_SUCCESS_CHANGED_BAT_COLOR, "The bat's color has changed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LEVEL_WAR, "Level War");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_LEFT_FAMILY_DAYS, "%d days left on your family service.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EXPIRE_FAMILY_TODAY, "Your family service ends today.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOO_FAR, "You need to get closer.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_DESC_DURABILITY, "Food remaining : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_ATTR, "Attribute : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_PET_INFO_WINDOW, "Open the pet info window");
	g_pGameStringTable->Set(STRING_MESSAGE_ENCHANT_FAIL, "The enchant failed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_DESC_DURABILITY_2, "Food remaining");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_RESSURECT, "Revive your pet?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLOSE_PET_INFO, "Close the pet info window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAN_ENCHANT_PET, "You can train your pet.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_ENCHANT_PET, "You can only train your pet after it levels up.");
	g_pGameStringTable->Set(STRING_MESSAGE_NEW_PET_LEVEL_1, "%s is now level %d.");
	g_pGameStringTable->Set(STRING_MESSAGE_NEW_PET_LEVEL_2, "%s is now level %d.");
	g_pGameStringTable->Set(STRING_MESSAGE_OPTION_NAME_LUCK_3, "Lucky");
	g_pGameStringTable->Set(STRING_MESSAGE_OPTION_NAME_LUCK_4, "Minion");
	g_pGameStringTable->Set(STRING_MESSAGE_OPTION_NAME_ATTR_3, "Nut");
	g_pGameStringTable->Set(STRING_MESSAGE_OPTION_NAME_ATTR_4, "Crunch");
	g_pGameStringTable->Set(STRING_MESSAGE_PET_DIE_WARNING, "%s has only %s worth of food left.");
	g_pGameStringTable->Set(STRING_MESSAGE_PET_REQUEST_REFILL, "Please top up its food.");
	g_pGameStringTable->Set(STRING_MESSAGE_PET_DIE, "%s has died.");
	g_pGameStringTable->Set(STRING_MESSAGE_PET_SUMMON, "You summoned %s.");
	g_pGameStringTable->Set(STRING_MESSAGE_PET_CAN_GET_ATTR, "You can give your pet an attribute.");
	g_pGameStringTable->Set(STRING_MESSAGE_PET_GAMBLE_OK, "You gave %s a secondary ability.");
	g_pGameStringTable->Set(STRING_MESSAGE_PET_GAMBLE_DESC, "You can loot monster heads along with items.");
	g_pGameStringTable->Set(STRING_MESSAGE_PET_GAMBLE_DESC_TEEN, "You can loot soul stones along with items.");
	g_pGameStringTable->Set(STRING_MESSAGE_PET_CAN_GET_OPTION, "You can give %s an option.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HPBAR_EXP_DESCRIPTION_NEW, "Exp remaining : %s (%s%%)");
	g_pGameStringTable->Set(STRING_MESSAGE_PET_GAMBLE_FAIL, "Granting the secondary ability failed.");
	g_pGameStringTable->Set(STRING_MESSAGE_OPTION_ENAME_LUCK_3, "Lucky");
	g_pGameStringTable->Set(STRING_MESSAGE_OPTION_ENAME_LUCK_4, "Minion");
	g_pGameStringTable->Set(STRING_MESSAGE_OPTION_ENAME_ATTR_3, "Nut");
	g_pGameStringTable->Set(STRING_MESSAGE_OPTION_ENAME_ATTR_4, "Crunch");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAN_PET_REVIVAL, "This can be revived.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_PET_REVIVAL, "This can only be used when the pet is dead.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_CAN_CUT_HEAD, "Has a secondary ability");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_CANNOT_CUT_HEAD, "No secondary ability");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REMOVE_PET_OPTION, "Remove your pet's option?");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_EVENT_GIFT_BOX, "Both of you need a gift box to exchange.");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_NETMARBLE_1, "Congratulations, you have earned a place at the school.");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_NETMARBLE_2, "99 online market vouchers");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_NETMARBLE_3, "Keep it safe until the event ends.");
	g_pGameStringTable->Set(STRING_MESSAGE_EVENT_NETMARBLE_4, "You cannot take part in the event without the card.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAN_USE_PET_FOOD, "Right-click to feed your pet.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DETACH_PET_FOOD, "Right-click to detach the item.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_USE_PET_FOOD, "Feed your pet?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_DETACH, "You can only detach the item while the pet is not out.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_0, "SWORD & BLADE");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_1, "Sword");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_2, "Blade");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_3, "Gun");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_4, "SMG");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_5, "AR");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_6, "SG");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_7, "SR");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_8, "Cross & Mace");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_9, "Cross");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_10, "Mace");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_11, "Armor");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_12, "Helmet");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_13, "Body armor");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_14, "Trousers");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_15, "Shield");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_16, "Gloves");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_17, "Belt");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_18, "Shoes");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_19, "Accessories");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_20, "Necklace");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_21, "Bracelet");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_22, "Ring");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_23, "Other");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_24, "Potions");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_25, "Ampoule & Holy Water");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_26, "Ammunition");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_27, "Bombs & Mines");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_28, "Radio");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_COMPUTER_STRING_29, "Miscellaneous");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_0, "Vampires, Scientifically Proven");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_1, "A Scientific Study: Do Vampire Bats Really Drink Human Blood?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_2, "Vampire Legends of Romania and Czechoslovakia");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_3, "Vlad Tepes of Romania");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_4, "Vampire Legends of Mexico and Arabia");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_5, "Vampires A~Z");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_6, "What a Vampire Is");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_7, "The Powers of the Vampire");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_8, "The Habits of the Vampire");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_9, "Vampire Weaknesses and How to Destroy Them");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_10, "A History of the Vampire");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_11, "Vampire Legends of the West Indies and Polynesia");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_12, "Vampires in the Bible");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_13, "Vampire Tales in Folk Belief");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_14, "Vampire Legends of Japan and the Malay Peninsula");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_15, "Gilles de Rais, the Murderer Called Bluebeard");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_16, "Elizabeth Bathory, the Blood Countess of Hungary");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_17, "How to Guard Against Vampires");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_18, "Legends of the Vampire");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_19, "Traits of the Vampire");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_BOOK_NAME_20, "Vampire Bats and Werewolves");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_TNDEAD, "TNDEAD");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_ARKHAN, "ARKHAN");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_ESTROIDER, "ESTROIDER");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_GOLEMER, "GOLEMER");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_DARKSCREAMER, "DARKSCREAMER");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_DEADBODY, "DEADBODY");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_MODERAS, "MODERAS");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_VANDALIZER, "VANDALIZER");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_DIRTYSTRIDER, "DIRTYSTRIDER");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_HELLWIZARD, "HELLWIZARD");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_TNSOUL, "TNSOUL");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_IRONTEETH, "IRONTEETH");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_REDEYE, "REDEYE");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_CRIMSONSLAUGTHER, "CRIMSONSLAUGTHER");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_HELLGUARDIAN, "HELLGUARDIAN");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_SOLDIER, "SOLDIER");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_RIPPER, "RIPPER");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_BIGFANG, "BIGFANG");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_LORDCHAOS, "LORDCHAOS");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_CHAOSGUARDIAN, "CHAOSGUARDIAN");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_HOBBLE, "HOBBLE");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_CHAOSNIGHT, "CHAOSNIGHT");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_WIDOWS, "WIDOWS");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_KID, "KID");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MONSTER_ENAME_SHADOWWING, "SHADOWWING");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HAN_DOMAIN_BLADE, "Blade");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HAN_DOMAIN_SWORD, "Sword");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HAN_DOMAIN_ENCHANT, "Enchant");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HAN_DOMAIN_GUN, "Gun");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HAN_DOMAIN_HEAL, "Heal");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HAN_DOMAIN_ETC, "Etc");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HAN_DOMAIN_VAMPIRE, "Vampire");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_HAN_DOMAIN_OUSTERS, "Ousters");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DOMAIN_ETC, "Etc");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DOMAIN_VAMPIRE, "Vampire");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ENG_DOMAIN_OUSTERS, "Ousters");
	g_pGameStringTable->Set(STRING_MESSAGE_SOUL_STONE, "Soul Stone");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAN_PET_MUTANT, "This can be transformed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_PET_MUTANT, "This cannot be transformed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_MUTANT, "Transform your pet into a wolfhound?");
	g_pGameStringTable->Set(STRING_ERROR_CANNOT_AUTHORIZE_BILLING, "Your billing information could not be found.");
	g_pGameStringTable->Set(STRING_ERROR_CANNOT_CREATE_PC_BILLING, "You cannot create a character because this is not a paid account.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_PET_DEAD_DAY, "Dead for %d days");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_LUCKY, "Lucky : ");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_QUEST_NPC_SLAYER, "Gruber");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_QUEST_NPC_VAMPIRE, "Kapatini");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_QUEST_NPC_OUSTERS, "Amata");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_QUEST_SLAYER, "Centauro quest / Goal: catch (%d) (%s) within %d minutes");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_QUEST_VAMPIRE, "Sturge quest / Goal: catch (%d) (%s) within %d minutes");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_QUEST_OUSTERS, "Pixie quest / Goal: catch (%d) (%s) within %d minutes");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_QUEST_CLEAR, "Return to the quest giver.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAN_3RD_ENCHANT_PET, "You can grant a third ability.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_3RD_ENCHANT_PET, "You cannot grant a third ability yet.");
	g_pGameStringTable->Set(STRING_MESSAGE_PET_3RD_GAMBLE_FAIL, "Granting the third ability failed.");
	g_pGameStringTable->Set(STRING_MESSAGE_PET_3RD_GAMBLE_OK, "You gave %s a third ability.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_SUMMON_2ND_PET, "You cannot summon a second-tier pet below level 40.");
	g_pGameStringTable->Set(STRING_MESSAGE_SEARCHING_MINE, "Mines to find : %d   Mines flagged : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_MIXING_GRADE_VALUE, "Items more than two grades apart cannot be mixed.");
	g_pGameStringTable->Set(STRING_MESSAGE_MINIGAME_GAME_OVER, "Game over");
	g_pGameStringTable->Set(STRING_MESSAGE_MINIGAME_ALL_STAGE_CLEAR, "All stage clear");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SELECT_QUEST_OUSTERS, "Monster numbers have grown sharply of late, and the barrier weakens because of them. For our people to rise again we must cull these monsters and hold the barrier. The task is void if you log out partway through, so take care. Bring back the number of monsters the quest names and I will give you a fine reward. So, which quest will you take?");
	g_pGameStringTable->Set(STRING_ERROR_CHILDGUARD_DENYED, "Users under 18 cannot play after 22:00.");
	g_pGameStringTable->Set(STRING_MESSAGE_KEEP_PETITEM, "Store your pet?");
	g_pGameStringTable->Set(STRING_MESSAGE_GET_KEEP_PETITEM, "Collect your stored pet?");
	g_pGameStringTable->Set(STRING_MESSAGE_EXIST_ITEM_ALREADY, "There is already another item there.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_GRADE, "  I II III IV V VI VII VIII IX X");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_GRADE_DESC, "Grade :");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SMS_SEND_OK, "The message was sent.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SMS_SEND_FAIL, "The message could not be sent.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SMS_ADD_FAIL, "Could not add it to the list.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SMS_DELETE_FAIL, "Could not delete it from the list.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SMS_FAIL_MAX_NUM_EXCEEDED, "You have run out of storable numbers.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SMS_FAIL_INVALID_DATA, "That information is not valid.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SMS_FAIL_NO_SUCH_EID, "The information could not be found.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SMS_FAIL_NOT_ENOUGH_CHARGE, "You do not have enough credit.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SMS_WINDOW, "SMS Window");
	g_pGameStringTable->Set(UI_STRING_HELP_SMS_SEND, "Send the message.");
	g_pGameStringTable->Set(UI_STRING_HELP_SMS_VIEW_LIST, "Open the phone book.");
	g_pGameStringTable->Set(UI_STRING_HELP_SMS_WINDOW, "Pick a special character.");
	g_pGameStringTable->Set(UI_STRING_HELP_SMS_ADDSEND, "Add the selected number to the send list.");
	g_pGameStringTable->Set(UI_STRING_HELP_SMS_DELETE, "Delete the selected number.");
	g_pGameStringTable->Set(UI_STRING_HELP_SMS_NEW, "Register a new number.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_USE_SMSITEM, "Use the SMS item?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NAMING_WINDOW, "Naming Window");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHANGE_PET_NICKNAME, "Change your pet's nickname.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHANGE_PLAYER_NICKNAME, "Change your free-choice nickname.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADD_PLAYER_NICKNAME, "Add a nickname.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SELECT_PLAYER_NICKNAME, "Switch to the selected nickname.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NICKNAME_CHANGE_OK, "Your nickname was changed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NICKNAME_SELECT_FAIL_FORCED, "A nickname assigned by a GM cannot be changed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_USE_NAMINGITEM, "Use the naming item?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_NAMING_SUMMON, "You can only rename a pet while it is summoned.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PET_NAMING_WOLVERINE, "A wolverine can be renamed without a pen item.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FAIL_SEARCH_ITEM, "The item could not be found.");
	g_pGameStringTable->Set(UI_STRING_CORE_ZAP_BLACK, "Physical attack +%d");
	g_pGameStringTable->Set(UI_STRING_CORE_ZAP_RED, "Magic attack +%d");
	g_pGameStringTable->Set(UI_STRING_CORE_ZAP_BLUE, "Physical defense +%d");
	g_pGameStringTable->Set(UI_STRING_CORE_ZAP_GREEN, "Magic defense +%d");
	g_pGameStringTable->Set(UI_STRING_CORE_ZAP_REWARD_ALL_STAT, "All stats +%d (4 Set)");
	g_pGameStringTable->Set(UI_STRING_CORE_ZAP_REWARD_ALL_REG, "All resistances +%d (4 Set)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_OPTION_EMPTY2, "               %s");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DESC_DIALOG_OPTION2, "        Option : %s");
	g_pGameStringTable->Set(UI_STRING_GQUEST_CAN_NOT, "Unavailable");
	g_pGameStringTable->Set(UI_STRING_GQUEST_CAN_ACCEPT, "Available");
	g_pGameStringTable->Set(UI_STRING_GQUEST_DOING, "In progress");
	g_pGameStringTable->Set(UI_STRING_GQUEST_SUCCESS, "Success");
	g_pGameStringTable->Set(UI_STRING_GQUEST_COMPLETE, "Complete");
	g_pGameStringTable->Set(UI_STRING_GQUEST_FAIL, "Failed");
	g_pGameStringTable->Set(UI_STRING_GQUEST_CAN_REPLAY, "Repeatable");
	g_pGameStringTable->Set(UI_STRING_GQUEST_MISSION, "Mission %d:%s");
	g_pGameStringTable->Set(UI_STRING_GQUEST_BUTTON_ACCEPT, "Take the quest.");
	g_pGameStringTable->Set(UI_STRING_GQUEST_BUTTON_GIVEUP, "Give up the quest.");
	g_pGameStringTable->Set(UI_STRING_GQUEST_TAB_PROCESS, "Show the quests in progress.");
	g_pGameStringTable->Set(UI_STRING_GQUEST_TAB_COMPLETE, "Show the completed quests.");
	g_pGameStringTable->Set(UI_STRING_NOTICE_EVENT_GOLD_MEDALS, "You have collected %d gold medals.");
	g_pGameStringTable->Set(STRING_ERROR_KEY_EXPIRED, "The authentication key has expired. Please log in again from the web.");
	g_pGameStringTable->Set(STRING_ERROR_NOT_FOUND_KEY, "The authentication key could not be found. Please log in again from the web.");
	g_pGameStringTable->Set(UI_STRING_GQUEST_UPDATE, "The quest information was updated.");
	g_pGameStringTable->Set(UI_STRING_CHANGE_EVENTITEM_PRICE, "The event item price changed to %d.");
	g_pGameStringTable->Set(STRING_MESSAGE_TOO_MANY_GUILD_REGISTERED, "Too many guilds have applied for the siege, so you cannot apply.");
	g_pGameStringTable->Set(STRING_MESSAGE_REINFORCE_DENYED, "Your application to join the defenders was already refused, so you cannot apply again.");
	g_pGameStringTable->Set(STRING_MESSAGE_ALREADY_REINFORCE_ACCEPTED, "A guild has already been accepted to join the defenders, so you cannot apply.");
	g_pGameStringTable->Set(STRING_MESSAGE_NO_WAR_REGISTERED, "No guild has applied for the siege, so you cannot apply to defend.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_ACCEPT, "You cannot accept the application to join the defenders.");
	g_pGameStringTable->Set(STRING_MESSAGE_ACCEPT_OK, "You accepted the application to join.");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_DENY, "You cannot refuse the application to join the defenders.");
	g_pGameStringTable->Set(STRING_MESSAGE_DENY_OK, "You refused the application to join.");
	g_pGameStringTable->Set(STRING_MESSAGE_SIEGE_POTAL_200, "Move to the trap zone");
	g_pGameStringTable->Set(STRING_MESSAGE_SIEGE_POTAL_201, "Move to the inner castle gate");
	g_pGameStringTable->Set(STRING_MESSAGE_SIEGE_POTAL_202, "Move inside the inner castle");
	g_pGameStringTable->Set(STRING_MESSAGE_SIEGE_POTAL_203, "Move inside the inner castle");
	g_pGameStringTable->Set(STRING_MESSAGE_LOGIN_ERROR_NONPK, "Your character's level is too high to log in here. Please use another server.");
	g_pGameStringTable->Set(STRING_MESSAGE_DAUM_EVENT_1, "The Dark Eden beginner event is here! We are giving new players a small gift along with a lottery ticket. Use the tickets you earn to claim one of the many prizes on offer, and more await at every new level. Collect the whole set of event items to try out some high-performance gear. The event runs in eight parts, handed out as you level. Press Ctrl + Q for the quest window. Enjoy your time in Dark Eden, and do not miss the chance to grow your character and collect gifts along the way.");
	g_pGameStringTable->Set(STRING_MESSAGE_DAUM_EVENT_2, "The Dark Eden beginner event! You have reached level %d, so a %s box has been added for you. Press Ctrl + Q to check it. If you still have an earlier box, you can claim its items too. The items we hand out are time-limited: they are stronger than the usual level-restricted gear, and because they come from the event they cannot be traded or exchanged. Once you find something better you can sell them to an NPC. The time remaining is shown under each item, and it keeps counting down even while you are away from Dark Eden, so use it well. Enjoy the rest of your stay.");
	g_pGameStringTable->Set(STRING_MESSAGE_DAUM_EVENT_3, "You have reached level %d, and a gift box has been added for you. Press Ctrl + Q to check it. If you still have an earlier box, you can claim its items too.");
	g_pGameStringTable->Set(STRING_MESSAGE_DAUM_EVENT_BOX1, "red");
	g_pGameStringTable->Set(STRING_MESSAGE_DAUM_EVENT_BOX2, "orange");
	g_pGameStringTable->Set(STRING_MESSAGE_DAUM_EVENT_BOX3, "yellow");
	g_pGameStringTable->Set(STRING_MESSAGE_DAUM_EVENT_BOX4, "green");
	g_pGameStringTable->Set(STRING_MESSAGE_DAUM_EVENT_BOX5, "blue");
	g_pGameStringTable->Set(STRING_MESSAGE_DAUM_EVENT_BOX6, "indigo");
	g_pGameStringTable->Set(STRING_MESSAGE_DAUM_EVENT_BOX7, "violet");
	g_pGameStringTable->Set(STRING_MESSAGE_DAUM_EVENT_BOX8, "black");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_PERCEPTION, "All stats +2");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_STONE_OF_SAGE, "INT +5");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_FOOT_OF_RANGER, "DEX +5");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_WARRIORS_FIST, "STR +5");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_ACID_INQUIRY, "Acid resistance +10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_BLOODY_INQUIRY, "Blood resistance +10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_CURSE_INQUIRY, "Curse resistance +10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_POISON_INQUIRY, "Poison resistance +10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_INQUIRY_MASTERY, "All resistances +3%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_POWER_OF_SPIRIT, "Protection +5%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_WIND_OF_SPIRIT, "Defense +5%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_PIXIES_EYES, "To hit +5%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_GROUND_OF_SPIRIT, "MP +5%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_FIRE_OF_SPIRIT, "Critical damage +5%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_EVOLUTION_IMMORTAL_HEART, "HP +5%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_BEHEMOTH_ARMOR_2, "Defense +5%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_DRAGON_EYE_2, "To hit +5%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_EVOLUTION_RELIANCE_BRAIN, "MP +5%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_HEAT_CONTROL, "Critical damage +5%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_ACID_MASTERY, "Enemy Acid resistance -10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_BLOODY_MASTERY, "Enemy Blood resistance -10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_CURSE_MASTERY, "Enemy Curse resistance -10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_POISON_MASTERY, "Enemy Poison resistance -10%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_SKILL_MASTERY, "All enemy resistances -3%");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_SALAMANDERS_KNOWLEDGE, "Fire spirit +1");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_UNDINES_KNOWLEDGE, "Water spirit +1");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANK_BONUS_GNOMES_KNOWLEDGE, "Earth spirit +1");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SIEGE_ATTACK, "(Attackers)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SIEGE_DEFENSE, "(Defenders)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_LOGINED, "(Offline)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GUILD_LIST_ID, "ID");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GUILD_LIST_SERVER, "Server");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GUILD_LIST_GRADE, "Grade");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_COMMAND_WINDOW, "Team Command");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_LIST_WINDOW, "Team List");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_WAIT_LIST_WINDOW, "Wait Team List");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TEAM_UNION_WINDOW, "Team Union");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLAN_COMMAND_WINDOW, "Clan Command");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLAN_LIST_WINDOW, "Clan List");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLAN_WAIT_LIST_WINDOW, "Wait Clan List");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CLAN_UNION_WINDOW, "Clan Union");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GUILD_COMMAND_WINDOW, "Guild Command");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GUILD_LIST_WINDOW, "Guild List");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GUILD_WAIT_LIST_WINDOW, "Wait Guild List");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GUILD_UNION_WINDOW, "Guild Union");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_DESC1, "Join the selected guild union.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_DESC2, "Leave the selected guild union.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_DESC3, "Expel the selected guild from the union.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_ALREADY_IN_UNION, "You already belong to a union.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_ALREADY_OFFER_SOMETHING, "You have already applied to a guild union.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_TARGET_IS_NOT_MASTER, "The other player is not a master.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_NOT_IN_UNION, "You do not belong to a union.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_MASTER_CANNOT_QUIT, "The union's leading guild cannot leave on its own.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_NO_TARGET_UNION, "There is no matching union.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_NOT_YOUR_UNION, "That union is not yours.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_SOURCE_IS_NOT_MASTER, "The applicant is not a master.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_JOIN_ASK, "Once you join a guild union you cannot join another. Join anyway?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_MESSAGE_OK, "Done.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_MESSAGE_REFUSE, "The other guild master refused.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_MESSAGE_SUCCESS, "The guild union was created.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_ASK, "Do you really want to leave?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_MSG, "Ask the union master for approval.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_MSG2, "Leave right now");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_OK, "If you apply and the union master approves, there is no penalty. Apply?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_CANCEL, "You can leave right now, but there is a penalty. Leave now?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_DEPORT_ASK, "Are you sure you want to expel them?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_DEPORT_OK, "%S was expelled.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_UNION_JOIN_MSG, "The %s guild is applying to join the union.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_UNION_DEPORT_MSG, "The %s guild is applying to leave the union.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_UNION_PENALTY, "There is a record of being forcibly removed from a guild union.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ASKING_RECALL, "Summon %s?");
	g_pGameStringTable->Set(UI_STRING_LEARN_SKILL_LEVEL, "Learned at level : %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_ACCEPT, "Accept the request to leave the union");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOTAL_GUILD_LEAVE_DENY, "Refuse the request to leave the union.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUEST_UNION_ERROR_1, "Only a guild master can apply for a union.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUEST_UNION_ERROR_2, "You already belong to a union.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUEST_UNION_ERROR_3, "You are not the union master.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_UNION_ERROR_NO_SLOT, "The union has no free slot.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_BLOOD_BIBLE, "Equip the Blood Bible seal.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RENT_BLOOD_BIBLE, "Borrow the Blood Bible seal.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RENT_BLOOD_BIBLE2, "Borrow the %s seal. (%s)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RENT_LATER_BLOOD_BIBLE, "I will borrow it another time.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANGER_SAY, "Ranger/");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MODIFY_TAX_OK, "The tax rate was changed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MODIFY_TAX_FAIL, "The tax rate could not be changed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MODIFY_TAX, "Please enter the new tax rate.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_RANGER_SAY2, "Ranger");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REMOVE_CURSE_1, "<3");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REMOVE_CURSE_2, "love");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REMOVE_CURSE_3, "love you");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REMOVE_CURSE_4, "I love you");
	g_pGameStringTable->Set(STRING_MESSAGE_RACE_WAR_STARTED_IN_OTHER_SERVER, "The race war has started on the first server.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_APPOINT_SUBMASTER, "Appoint as sub master.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAN_SKILL_DELETE, "Skill points can be refunded");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_SKILL_DELETE, "Skill points cannot be refunded");
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_SKILLTREE_DELETE, "The points cannot be refunded.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOO_MANY_MEMBER, "There are already 50 members, so you cannot join.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CONFIRM_DOWN_SKILL2, "This refunds the points spent on %s. It costs $%s. Proceed?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_UNION_CHATTING, "Union chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SHOW_UNION_CHATTING, "Show union chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_SHOW_UNION_CHATTING, "Hide union chat");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_USE_SIEGE_FOR_RESURRECT, "Resurrect in front of the inner castle gate.");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_GET_POINT, "Collect Powerzzang points");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_POINT, "My Powerzzang points");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_AVAILABLE, "Points available to exchange");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_NUMBER_1, "Mobile");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_NUMBER_2, "number");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_GET_POINT_HELP, "Collect your Powerzzang points.");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_EXCHANGE_HELP, "Exchange Powerzzang points for items.");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_REQUEST_OK, "Your Powerzzang points were applied. Points transferred : %d");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_ERROR_NO_MEMBER, "You are not a Powerzzang member.");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_ERROR_SERVER_ERROR, "The Powerzzang server is having trouble. Try again, and check www.powerzzang.com if it still fails.");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_ERROR_PROCESS_ERROR, "The Powerzzang database is having trouble. Try again, and check www.powerzzang.com if it still fails.");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_ERROR_NO_POINT, "You have no Powerzzang points saved up.");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_ERROR_NO_MATCHING, "No matching information could be found.");
	g_pGameStringTable->Set(UI_STRING_POWER_JJANG_ERROR_CONNECT, "There is a problem connecting to the Powerzzang server.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_UTIL, "Util");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PERSONAL_STORE, "Personal Store");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_POWER_JJANG, "Powerzzang");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NONKOWN1, "Equip a cash shop item");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_NONKOWN2, "Buy a cash shop item");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SELL_MONEY_IN_DIALOG, "Enter the price to sell the item for.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PERSNALSHOP_MESSAGE, "Enter your personal store's advert!!");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PERSNALSHOP_OK, "Open your personal store.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PERSNALSHOP_CANCEL, "Close your personal store.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PERSNALSHOP_WRITE_MESSAGE, "Write your personal store's advert");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_FIND_STORE, "That seller could not be found.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_STORE_CLOSED, "The store has already closed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ITEM_NOT_FOUND, "That item has already been sold or withdrawn by the seller.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_TOO_MUCH_MONEY, "The seller is carrying too much money, so you cannot buy it.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ALREADY_DISPLAYED, "That item is already on display.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE, "All sorts of goods for sale.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GAMEMONEY_WITH_HANGUL, "Show game money in words as well");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANTE_0, "Horus %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANTE_1, "Seth %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANTE_2, "Maat %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANTE_3, "Osiris %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANTE_4, "Thoth %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANTE_5, "Nut %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANTE_6, "Geb %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANTE_7, "Shu %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANTE_8, "Ra %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANTE_9, "Ptah %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANTE_10, "Nun %d");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_PDA, "Equip a PDA.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_SHOULDER, "Wear a shoulder guard.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_DERMIS, "Apply a tattoo.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_PERSONA, "Wear a mask.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_FASCIA, "Wear a waist ornament.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_EQUIP_MITTEN, "Wear mittens.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHINGHO, "Title");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWAP_ADVANCEMENT_ITEM, "Click the item you want to exchange for an advancement item.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWAP_CONFIRM, "Exchange it for an advancement item?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWAP_ERROR, "You have not advanced, so you cannot exchange items.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SWAP_ADVANCEMENT_ITEM_ERROR, "This cannot be exchanged for an advancement item.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAMPAIGN_HELP_REQUEST, "Please enter the amount you wish to donate.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAMPAIGN_HELP_THANKS, "Your donation will go to neighbours in need. Thank you for taking part.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAMPAIGN_HELP_UNITS_SLAYER, "x10,000");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAMPAIGN_HELP_UNITS_VAMPIRE, "x10,000");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CAMPAIGN_HELP_UNITS_OUSTERS, "x10,000");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_0, "Horus grade %d or higher");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_1, "Seth grade %d or higher");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_2, "Maat grade %d or higher");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_3, "Osiris grade %d or higher");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_4, "Thoth grade %d or higher");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_5, "Nut grade %d or higher");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_6, "Geb grade %d or higher");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_7, "Shu grade %d or higher");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_8, "Ra grade %d or higher");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_9, "Ptah grade %d or higher");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUIRE_ADVANCEMENT_LEVEL_10, "Nun grade %d or higher");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANCEMENT_JOB_BLADE, "Splitter");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANCEMENT_JOB_SWORD, "Defender");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANCEMENT_JOB_SOLDER, "Heavy Shooter");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANCEMENT_JOB_HEAL, "Priest");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANCEMENT_JOB_ENCHANT, "Granter");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANCEMENT_JOB_VAMPIRE, "Vamp Noble");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANCEMENT_JOB_COMBAT, "Custos");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANCEMENT_JOB_EARTH, "Terranos");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANCEMENT_JOB_FIRE, "Igniser");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ADVANCEMENT_JOB_WATER, "Aquan");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DO_NOT_SHOW_PERSNALSHOP_MSG, "Hide personal store messages");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_REQUEST_GET_EVENT_ITEM, "Claim the Come Back event item on this character?");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GET_EVENT_ITEM_RECEIVE_OK, "You received the Come Back event item.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GET_EVENT_ITEM_RECEIVE_ALREADY, "You have already claimed the Come Back event item.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GET_EVENT_ITEM_RECEIVE_FAIL, "Claiming the Come Back event item failed.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GET_EVENT_ITEM_NOT_EVENT_USER, "You are not eligible for the Come Back event.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GET_EVENT_ITEM_NOTICE, "Congratulations! You can claim your Come Back 2005 gift.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FAIL_OPEN_WEBPAGE, "The web page could not be opened.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_USE_ADVANCEMENTCLASS, "Advanced characters cannot use this.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_USE_HIGH_GRADE, "Items of grade 6 or above cannot be enchanted.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_USE_ADVANCEMENT_ITEM, "Advancement-only items cannot be enchanted.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_USE_OVER_TWO_OPTION, "Items with two or more options cannot be enchanted.");
	g_pGameStringTable->Set(STRING_ERROR_IP_DENY, "Too many failed logins, or an illegal program was detected. Please try again in 10 minutes.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CHECK_VERSION_ERROR, "The client version does not match the server. Please update the game.");
	g_pGameStringTable->Set(STRING_STATUS_NOT_FIND_SKILL_CRAD, "You need the skill card to use this skill!");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_SYSTEM, "[System]%s");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PLAYER_SAY, "toall/");
	g_pGameStringTable->Set(UI_STRING_NO_ITEM_MESSAGE, "You do not have that item.");

	//add by viva : friend button description
	g_pGameStringTable->Set(UI_STRING_MESSAGE_FRIEND, "Friends");
	//end

	// ESC game menu
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GAME_MENU_OPTION, "Option (O)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GAME_MENU_LOGOUT, "Log out (L)");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_GAME_MENU_CONTINUE, "Continue playing (C)");

	g_pGameStringTable->Set(UI_STRING_MESSAGE_DELETE_INPUT_NAME, "Enter the character's name to confirm.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_DELETE_NAME_MISMATCH, "The name does not match the character you selected.");

	// The friend list's six ask dialogs. Every one of them is raised by a
	// GCFriendChatting packet and names the other player, so the %s is
	// the argument each call site passes and not an entry the operator
	// may drop.
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ASK_FRIEND_REQUEST, "%s wants to add you as a friend.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ASK_FRIEND_REFUSE, "%s has declined your friend request.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ASK_FRIEND_WAIT, "Waiting for %s to answer.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ASK_FRIEND_EXIST, "%s is already on your friend list.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ASK_FRIEND_BLACK, "%s is on your block list.");
	g_pGameStringTable->Set(UI_STRING_MESSAGE_ASK_FRIEND_DELETE, "Remove %s from your friend list?");

	return;
}


//----------------------------------------------------------------------
// Get Game String
//----------------------------------------------------------------------
const char*
GetGameString(int stringID)
{
	if (g_pGameStringTable == NULL
		|| stringID < 0
		|| stringID >= g_pGameStringTable->GetSize())
	{
		return "";
	}

	return (*g_pGameStringTable)[stringID].GetString();
}