//-----------------------------------------------------------------------------
// UtilityFunction.cpp
//-----------------------------------------------------------------------------
// The account name and password input rules (task 4.232, gamemodel's since
// then).
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "CrtCompat.h"

#include "UtilityFunction.h"
#include <cstring>


//-----------------------------------------------------------------------------
// RemoveStringSpace
//-----------------------------------------------------------------------------
// Removes the spaces before and after the string.
//-----------------------------------------------------------------------------
void
RemoveStringSpace(char*& str)
{
	// The leading spaces
	int bExistChar = 0;
	while (*str != '\0')
	{
		if (*str == ' ')
		{
			str++;
		}
		else
		{
			bExistChar = 1;
			break;
		}
	}

	// The trailing spaces
	if (bExistChar)
	{
		char* strTemp = str;
		strTemp += strlen(str)-1;

		while (*strTemp == ' ')
		{
			strTemp--;
		}

		*(strTemp+1) = '\0';
	}
}

//-----------------------------------------------------------------------------
// Is Valid ID
//-----------------------------------------------------------------------------
// 4 to 10 bytes, no spaces; all hangul, or all English (with digits and
// the permitted characters).
//-----------------------------------------------------------------------------
int
IsValidID(const char* strID, const char* strPermit)
{
	const int minLength = 4;
	const int maxLength = 10;

	// The characters permitted in any case
	//const char* strPermit = "_-";

	int len = 0;

	char strtempID[128];

	snprintf(strtempID, sizeof(strtempID), "%s", strID);

	char* str = strtempID;

	//--------------------------------------------------------
	// No spaces.
	// Counts the length too.
	//--------------------------------------------------------
	while (*str != '\0')
	{
		if (*str == ' ')
		{
			return 0;
		}

		len++;
		str++;
	}

	//--------------------------------------------------------
	// A wrong length
	//--------------------------------------------------------
	if (len<minLength || len>maxLength)
	{
		return 0;
	}

	//--------------------------------------------------------
	// The first character may not be a digit.
	//--------------------------------------------------------
	if (strtempID[0]>='0' && strtempID[0]<='9')
	{
		return 0;
	}

	//--------------------------------------------------------
	// No special characters other than hangul.
	//--------------------------------------------------------
	int bExistHangul = 0;
	int bExistEnglish = 0;

	str = strtempID;
	while (*str != '\0')
	{
		char ch = *str;
		//--------------------------------------------------------
		// A high first bit: hangul?
		//--------------------------------------------------------
		if (ch & 0x80)
		{
			// Hangul: two bytes, so check the one after it
			str++;

			if (*str=='\0')
			{
				// Nothing after it: a wrong ID.
				return 0;
			}

			//--------------------------------------------------------
			// A lone consonant or vowel (jamo) is a wrong ID.
			//--------------------------------------------------------
			const char* badKor = "ㅂㅃㅈㅉㄷㄸㄱㄲㅅㅆㅛㅕㅑㅐㅒㅔㅖㅁㄴㅇㄹㅎㅗㅓㅏㅣㅋㅌㅊㅍㅠㅜㅡㅙㅝㅟㅢ";

			char strKor[3] = { ch, *str };

			const char* findPtr = strstr(badKor, strKor);

			if (findPtr!=NULL)
			{
				if (!((findPtr - badKor) & 0x01))	// at an even offset
				{
					return 0;
				}
			}

			str++;

			bExistHangul = 1;
		}
		//--------------------------------------------------------
		// Otherwise
		//--------------------------------------------------------
		else
		{
			//--------------------------------------------------------
			// English, upper or lower case
			//--------------------------------------------------------
			if ((ch>='a' && ch<='z')
				|| (ch>='A' && ch<='Z'))
			{
				bExistEnglish = 1;
			}
			//--------------------------------------------------------
			// A digit, or a permitted character,
			// is fine.
			//--------------------------------------------------------
			else if ((ch>='0' && ch<='9')
					|| (strPermit!=NULL && strchr(strPermit, ch)!=NULL))
			{
			}
			//--------------------------------------------------------
			// No other characters.
			//--------------------------------------------------------
			else
			{
				return 0;
			}

			str++;

		}
	}

	//--------------------------------------------------------
	// With hangul, no English.
	//--------------------------------------------------------
	if (bExistHangul && bExistEnglish)
	{
		return 0;
	}

	//--------------------------------------------------------
	// A valid ID
	//--------------------------------------------------------
	return 1;
}

//-----------------------------------------------------------------------------
// Is Valid Password
//-----------------------------------------------------------------------------
int
IsValidPassword(const char* strPWD)
{
	//--------------------------------------------------
	// No special characters that are not allowed.
	//--------------------------------------------------
	if (strchr(strPWD, '\\')!=NULL
		|| strchr(strPWD, '\'')!=NULL)
	{
		return 0;
	}

	//--------------------------------------------------
	// Not digits only:
	//--------------------------------------------------
	// disabled, since some already chose all-digit passwords.
	/*
	char* str = strPWD;

	char ch;

	bool AllNumber = TRUE;

	while (ch=*str++, ch)
	{
		if (ch<'0' || ch>'9')
		{
			AllNumber = FALSE;
			break;
		}
	}

	if (AllNumber)	// all digits
	{
		return 0;
	}
	*/

	return 1;
}
