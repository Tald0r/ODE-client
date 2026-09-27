//----------------------------------------------------------------------
// RenderingFunctions.cpp - macOS rendering functions
//----------------------------------------------------------------------
#include "../Client_PCH.h"
#include "../basic/BasicException.h"
#include "../basic/BasicData.h"
#include "../basic/2d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "../VS_UI/src/hangul/Fl2.h"
#include "TextSystem/TextService.h"
#include "TextSystem/RenderTargetSpriteSurface.h"
#include "TextSystem/TextTypes.h"
#include "TextSystem/FontHandleUtil.h"

class CSpriteSurface;
extern CSpriteSurface* g_pLast;
extern CSpriteSurface* g_pBack;

//----------------------------------------------------------------------
// Surface/DC Management
//----------------------------------------------------------------------
static void* g_pFL2Surface = NULL;
static void* g_pFL2DC = NULL;

// SDL/TextSystem 백엔드는 draw 시점마다 surface를 즉시 resolve하므로
// 원본처럼 실제 GDI HDC를 오래 들고 있을 필요가 없다.
// 다만 호출부(FL2.cpp 등)는 "최초 GetDC만 true, 중첩 호출은 false"라는
// 반환값 규약에 기대어 Release 짝을 맞추므로, 그 상태만 흉내낸다.
bool g_FL2_GetDC()
{
	if (g_pFL2DC != NULL)
	{
		return false;
	}

	g_pFL2DC = reinterpret_cast<void*>(1);

	return true;
}

bool g_FL2_ReleaseDC()
{
	if (g_pFL2DC == NULL)
	{
		return false;
	}

	g_pFL2DC = NULL;

	return true;
}

void g_SetFL2Surface(void* pSurface)
{
    g_pFL2Surface = pSurface;
}

//----------------------------------------------------------------------
// OpenGL Initialization
//----------------------------------------------------------------------
bool InitializeGL(int width, int height, int bpp, int fullscreen)
{
    (void)width;
    (void)height;
    (void)bpp;
    (void)fullscreen;
    // Stub: SDL backend handles this
    return true;
}

//----------------------------------------------------------------------
// Rectangle Drawing
//----------------------------------------------------------------------
void rectangle(S_SURFACEINFO* pSurfaceInfo, Rect* pRect, int color)
{
    (void)pSurfaceInfo;
    (void)pRect;
    (void)color;
    // Stub: Would draw rectangle outline
}

void FillRect(S_SURFACEINFO* pSurfaceInfo, Rect* pRect, int color)
{
    (void)pSurfaceInfo;
    (void)pRect;
    (void)color;
    // Stub: Would fill rectangle
}

//----------------------------------------------------------------------
// Text Rendering
//----------------------------------------------------------------------

static CSpriteSurface* ResolveTextSurface()
{
	if (g_pFL2Surface != NULL)
		return reinterpret_cast<CSpriteSurface*>(g_pFL2Surface);
	if (g_pLast != NULL)
		return g_pLast;
	if (g_pBack != NULL)
		return g_pBack;
	return NULL;
}

static int ApproximateWidth(const char* pStr)
{
	if (pStr == NULL)
		return 0;

	int width = 0;
	const char* p = pStr;
	int maxLen = 1024;
	int count = 0;

	while (*p && count < maxLen)
	{
		if ((*p & 0x80) != 0)
		{
			if (*(p + 1) != 0) {
				width += 16;
				p += 2;
				count += 2;
			} else {
				width += 8;
				p++;
				count++;
			}
		}
		else
		{
			width += 8;
			p++;
			count++;
		}
	}

	return width;
}

static TextSystem::TextStyle BuildStyleFromPrintInfo(const PrintInfo* pInfo)
{
	TextSystem::TextService& service = TextSystem::TextService::Get();
	TextSystem::TextStyle style = service.GetDefaultStyle();

	int size = TextSystem::DecodeFontSizeHandle(pInfo ? pInfo->hfont : NULL, 16);
	TextSystem::FontHandle font = service.GetFont(size);
	if (font.IsValid())
		style.font = font;

	if (pInfo)
		style.color = TextSystem::ColorFromCOLORREF(pInfo->text_color);

	style.align = TextSystem::TextAlign::Left;
	return style;
}

static int AdjustXForAlignment(int x, const char* text, const TextSystem::TextStyle& style, UINT align)
{
	if (text == NULL || *text == '\0')
		return x;

	if (align & TA_CENTER) {
		TextSystem::Metrics metrics = TextSystem::TextService::Get().MeasureText(text, style, 0);
		return x - metrics.width / 2;
	}
	if (align & TA_RIGHT) {
		TextSystem::Metrics metrics = TextSystem::TextService::Get().MeasureText(text, style, 0);
		return x - metrics.width;
	}
	return x;
}

// Basic print function (PrintInfo version)
void g_Print(int x, int y, const char* pStr, PrintInfo* pInfo)
{
	if (pStr == NULL || *pStr == '\0')
		return;

	CSpriteSurface* surface = ResolveTextSurface();
	if (surface == NULL)
		return;

	TextSystem::TextStyle style = BuildStyleFromPrintInfo(pInfo);
	int drawX = x;
	if (pInfo != NULL)
		drawX = AdjustXForAlignment(x, pStr, style, pInfo->text_align);

	TextSystem::SpriteSurfaceRenderTarget target(surface);
	TextSystem::TextService::Get().DrawLine(target, pStr, drawX, y, 0, style);
}

// Basic print function (void* version for legacy callers)
void g_Print(int x, int y, const char* pStr, void* pInfo)
{
	g_Print(x, y, pStr, reinterpret_cast<PrintInfo*>(pInfo));
}

// Colored string print (PrintInfo version)
int g_PrintColorStr(int x, int y, const char* pStr, PrintInfo& info, DWORD color)
{
	PrintInfo useInfo = info;
	useInfo.text_color = color;
	g_Print(x, y, pStr, &useInfo);
	return x + g_GetStringWidth(pStr, useInfo.hfont);
}

// Colored string print (void* version)
void g_PrintColorStr(int x, int y, const char* pStr, void* info, unsigned long color)
{
	if (info == NULL)
		return;
	PrintInfo* pi = reinterpret_cast<PrintInfo*>(info);
	g_PrintColorStr(x, y, pStr, *pi, static_cast<DWORD>(color));
}

// Colored string with shadow
int g_PrintColorStrShadow(int x, int y, const char* pStr, PrintInfo& info,
						  DWORD color, DWORD shadowColor)
{
	PrintInfo useInfo = info;

	if (shadowColor == 0xFFFFFFFF)
	{
		int r = (color & 0xFF0000) >> 16;
		int g = (color & 0xFF00) >> 8;
		int b = color & 0xFF;
		shadowColor = RGB(r >> 2, g >> 2, b >> 2);
	}

	useInfo.text_color = shadowColor;
	g_Print(x + 1, y + 1, pStr, &useInfo);

	useInfo.text_color = color;
	g_Print(x, y, pStr, &useInfo);

	return x + g_GetStringWidth(pStr, useInfo.hfont);
}

void g_PrintColorStrShadow(int x, int y, const char* pStr, void* info,
							unsigned long color, unsigned long shadowColor)
{
	if (info == NULL)
		return;
	PrintInfo* pi = reinterpret_cast<PrintInfo*>(info);
	g_PrintColorStrShadow(x, y, pStr, *pi, static_cast<DWORD>(color), static_cast<DWORD>(shadowColor));
}

// Colored string with outline
int g_PrintColorStrOut(int x, int y, const char* pStr, PrintInfo& info,
					   DWORD color, DWORD outColor)
{
	PrintInfo useInfo = info;

	useInfo.text_color = outColor;
	g_Print(x - 1, y, pStr, &useInfo);
	g_Print(x + 1, y, pStr, &useInfo);
	g_Print(x, y - 1, pStr, &useInfo);
	g_Print(x, y + 1, pStr, &useInfo);

	useInfo.text_color = color;
	g_Print(x, y, pStr, &useInfo);

	return x + g_GetStringWidth(pStr, useInfo.hfont);
}

void g_PrintColorStrOut(int x, int y, const char* pStr, void* info,
						 unsigned long color, unsigned long outColor)
{
	if (info == NULL)
		return;
	PrintInfo* pi = reinterpret_cast<PrintInfo*>(info);
	g_PrintColorStrOut(x, y, pStr, *pi, static_cast<DWORD>(color), static_cast<DWORD>(outColor));
}

//----------------------------------------------------------------------
// String Measurements
//----------------------------------------------------------------------
int g_GetStringWidth(const char* pStr, void* pFont)
{
	if (pStr == NULL)
		return 0;

	TextSystem::TextService& service = TextSystem::TextService::Get();
	TextSystem::TextStyle style = service.GetDefaultStyle();
	int size = TextSystem::DecodeFontSizeHandle(pFont, 16);
	TextSystem::FontHandle font = service.GetFont(size);

	if (font.IsValid())
		style.font = font;

	TextSystem::Metrics metrics = service.MeasureText(pStr, style, 0);
	if (metrics.width == 0 && pStr[0] != '\0')
		return ApproximateWidth(pStr);
	return metrics.width;
}

int g_GetStringHeight(const char* pStr, void* pFont)
{
    TextSystem::TextService& service = TextSystem::TextService::Get();
    TextSystem::TextStyle style = service.GetDefaultStyle();
    int size = TextSystem::DecodeFontSizeHandle(pFont, 16);
    TextSystem::FontHandle font = service.GetFont(size);

    if (font.IsValid())
        style.font = font;

    TextSystem::Metrics metrics = service.MeasureText(pStr ? pStr : "", style, 0);
    if (metrics.height == 0)
        return size > 0 ? size : 16;
    return metrics.height;
}

//----------------------------------------------------------------------
// String Manipulation
//----------------------------------------------------------------------

int g_DBCSLen(const char_t* p_dbcs)
{
	if (p_dbcs == NULL)
		return 0;

	int len = 0;
	while (*p_dbcs++ != 0)
		len++;

	return len;
}

// Get byte length (note: "Lenth" is intentional spelling from original code)
int g_GetByteLenth(const char_t* p_dbcs, int dbcs_len)
{
    if (p_dbcs == NULL || dbcs_len <= 0)
        return 0;

    int count = 0;
    for (int i = 0; i < dbcs_len; ++i)
    {
        if ((p_dbcs[i] & 0xFF00) != 0)
            count += 2;
        else
            count += 1;
    }

    return count;
}

// Convert DBCS to single byte
int g_Convert_DBCS_Ascii2SingleByte(const char_t* p_dbcs, int dbcs_len, char*& p_new_buf)
{
	if (p_dbcs == NULL || dbcs_len <= 0)
		return 0;

	int max_size = dbcs_len * 2 + 1;
	char* p_temp = new char[max_size];
	CheckMemAlloc(p_temp);
	memset(p_temp, 0, max_size);

	for (int i = 0, c = 0; i < dbcs_len; i++)
	{
		if ((p_dbcs[i] & 0xFF00) != 0)
		{
			*(char_t*)(p_temp + i + c) = p_dbcs[i];
			c++;
		}
		else
		{
			p_temp[i + c] = (char)(p_dbcs[i] & 0xFF);
		}
	}

	int len = static_cast<int>(strlen(p_temp));
	p_new_buf = new char[len + 1];
	CheckMemAlloc(p_new_buf);
	memcpy(p_new_buf, p_temp, static_cast<size_t>(len) + 1);
	DeleteNewArray(p_temp);
	return len;
}

int g_ConvertAscii2DBCS(const char* p_ascii, int ascii_len, char_t*& p_new_buf)
{
	if (p_ascii == NULL || ascii_len <= 0)
		return 0;

	int size = ascii_len + 1;
	char_t* p_temp = new char_t[size];
	int dbcs = 0;

	for (int i = 0; i < ascii_len; i++, dbcs++)
	{
		if (isascii(p_ascii[i]) != 0)
		{
			p_temp[dbcs] = (char_t)p_ascii[i];
		}
		else
		{
			p_temp[dbcs] = *((char_t*)(p_ascii + i));
			i++;
		}
	}

	p_new_buf = new char_t[dbcs + 1];
	for (int m = 0; m < dbcs; m++)
		p_new_buf[m] = p_temp[m];

	DeleteNewArray(p_temp);
	p_new_buf[dbcs] = 0;
	return dbcs;
}

// Number to string
std::string g_GetNumberString(int number)
{
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", number);
    return std::string(buffer);
}

// Money formatting
//
// The Korean original grouped the amount into 억/만 (10^8 / 10^4) units, which
// has no English equivalent, so group the digits in threes instead.
std::string g_GetStringByMoney(DWORD money)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)money);

    std::string sstr = buffer;

    for (int i = (int)sstr.size() - 3; i > 0; i -= 3)
    {
        sstr.insert(i, ",");
    }

    return sstr;
}

// g_BasicException and g_SetNewHandler are defined in basic/BasicException.cpp
