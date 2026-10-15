//-----------------------------------------------------------------------------
// UtilityFunction.cpp
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "CrtCompat.h"

#include "UtilityFunction.h"
#include "DataPath.h"
#include <SDL.h>


//-----------------------------------------------------------------------------
// RemoveStringSpace
//-----------------------------------------------------------------------------
// string 앞 뒤의 공백을 제거한다.
//-----------------------------------------------------------------------------
void
RemoveStringSpace(char*& str)
{
	// 앞쪽 공백 제거
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

	// 뒤쪽 공백 제거 
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
// 4~10글자 공백이 있으면 안된다.
// 다 한글이거나
// 다 영어야 할까??
/*
	char temp[3];
	strncpy(temp, str, 2);
	temp[2] = NULL;

	if (strcmp(temp, "가") >= 0
		&& strcmp(temp, "힝") <= 0)
	{
		// 제대로된 한글
		str += 2;					
		continue;	
	}
	else
	{
		// 2바이트 문자
		str += 2;
		continue;
	}
*/
//-----------------------------------------------------------------------------
int 
IsValidID(const char* strID, const char* strPermit)
{
	const int minLength = 4;
	const int maxLength = 10;

	// 어떠한 경우이든 허용되는 문자들
	//const char* strPermit = "_-";

	int len = 0;

	char strtempID[128];

	snprintf(strtempID, sizeof(strtempID), "%s", strID);

	char* str = strtempID;

	//--------------------------------------------------------
	// 공백이 들어가면 안된다.
	// 길이도 알아낸다. *_*;
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
	// 길이가 잘못된 경우
	//--------------------------------------------------------
	if (len<minLength || len>maxLength)
	{
		return 0;
	}

	//--------------------------------------------------------
	// 첫글자가 숫자이면 안된다.
	//--------------------------------------------------------
	if (strtempID[0]>='0' && strtempID[0]<='9')
	{
		return 0;
	}

	//--------------------------------------------------------
	// 한글을 제외한 특수문자가 들어가면 안된다.
	//--------------------------------------------------------
	int bExistHangul = 0;
	int bExistEnglish = 0;

	str = strtempID;
	while (*str != '\0')
	{
		char ch = *str;
		//--------------------------------------------------------
		// 첫 bit가 '1'이면 한글..일까?
		//--------------------------------------------------------
		if (ch & 0x80)
		{
			// 한글이므로 두 byte 다음 거 체크
			str++;

			if (*str=='\0')
			{
				// 다음게 없으면 잘못된 ID이다.
				return 0;				
			}

			//--------------------------------------------------------
			// 자음, 모음만 있는 경우라면 잘못된 아이디다.
			//--------------------------------------------------------
			const char* badKor = "ㅂㅃㅈㅉㄷㄸㄱㄲㅅㅆㅛㅕㅑㅐㅒㅔㅖㅁㄴㅇㄹㅎㅗㅓㅏㅣㅋㅌㅊㅍㅠㅜㅡㅙㅝㅟㅢ";

			char strKor[3] = { ch, *str };

			const char* findPtr = strstr(badKor, strKor);

			if (findPtr!=NULL)
			{
				if (!((findPtr - badKor) & 0x01))	// 짝수라면..
				{
					return 0;
				}
			}

			str++;

			bExistHangul = 1;
		}
		//--------------------------------------------------------
		// 아니면..
		//--------------------------------------------------------
		else
		{
			//--------------------------------------------------------
			// 영어 대소문자..
			//--------------------------------------------------------
			if ((ch>='a' && ch<='z') 
				|| (ch>='A' && ch<='Z'))
			{				
				bExistEnglish = 1;
			}
			//--------------------------------------------------------
			// 숫자거나
			// 허용된 문자인 경우는 괜찮다..
			//--------------------------------------------------------
			else if ((ch>='0' && ch<='9')
					|| (strPermit!=NULL && strchr(strPermit, ch)!=NULL))
			{
			}
			//--------------------------------------------------------
			// 이상한 문자 쓰면 안된다.				
			//--------------------------------------------------------
			else
			{
				return 0;
			}

			str++;

		}
	}

	//--------------------------------------------------------
	// 한글을 사용한 경우에는 영어를 사용할 수 없다.
	//--------------------------------------------------------
	if (bExistHangul && bExistEnglish)
	{
		return 0;
	}

	//--------------------------------------------------------
	// 정상적인 ID
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
	// 허용안되는 특수문자를 사용하면 안된다.
	//--------------------------------------------------
	if (strchr(strPWD, '\\')!=NULL
		|| strchr(strPWD, '\'')!=NULL)
	{
		return 0;
	}

	//--------------------------------------------------
	// 숫자만 사용하면 안된다.
	//--------------------------------------------------
	// 흑흑.. 이미 숫자가 입력된 사람들 때메.. T_T;;
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

	if (AllNumber)	// 전부 숫자인 경우..
	{
		return 0;
	}
	*/

	return 1;
}

// SDL backend implementation of LoadImageToSurface (USE_SDL_BACKEND is
// mandatory on all platforms, including PLATFORM_WINDOWS)
#include "SpriteLib/CSpriteSurface.h"

bool LoadImageToSurface(const char* pFilename, CSpriteSurface& surface)
{
	if (pFilename == NULL) {
		return false;
	}

	// Resolved for the disk (basic/DataPath.h): the identity on Windows;
	// off Windows the separators folded and each component matched
	// case-insensitively. SDL_LoadBMP() below opens it.
	const std::string sResolved = Basic::NormalizeDataPath(pFilename);
	pFilename = sResolved.c_str();

	int fileLen = static_cast<int>(strlen(pFilename));

	if (fileLen < 8) {
		return false;
	}

	// Check file extension
	char checkStr[10];
	snprintf(checkStr, sizeof(checkStr), "%s", pFilename + fileLen - 4);
	for (int i = 0; static_cast<size_t>(i) < strlen(checkStr); i++) {
		if (checkStr[i] >= 'A' && checkStr[i] <= 'Z') {
			checkStr[i] += 'a' - 'A';
		}
	}

	// Only support .bmp files for now
	if (strncmp(".bmp", checkStr, 4) != 0) {
		return false;
	}

	// Load image using SDL_image
	SDL_Surface* loaded = SDL_LoadBMP(pFilename);
	if (loaded == NULL) {
		return false;
	}

	// Convert to RGB565 format
	SDL_Surface* converted = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGB565, 0);
	if (converted == NULL) {
		SDL_FreeSurface(loaded);
		return false;
	}

	DWORD pitch = 0;
	auto* dest = static_cast<BYTE*>(surface.Lock(nullptr, &pitch));
	if (!dest) {
		SDL_FreeSurface(converted);
		SDL_FreeSurface(loaded);
		return false;
	}
	const int width = min(converted->w, surface.GetWidth());
	const int height = min(converted->h, surface.GetHeight());
	for (int y = 0; y < height; ++y)
		memcpy(dest + y * pitch, static_cast<BYTE*>(converted->pixels) + y * converted->pitch, width * sizeof(WORD));
	surface.Unlock();

	SDL_FreeSurface(converted);
	SDL_FreeSurface(loaded);

	return true;
}
