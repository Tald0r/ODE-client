/*-----------------------------------------------------------------------------

	FL2.h

	FL2 master header file.

	2000.10.4. KJTINC

-----------------------------------------------------------------------------*/

#ifndef __FL2_H__
#define __FL2_H__

#include "Ci.h"
#include "StringReduction.h"
#include <string>

// NOTE: Do not include SP.h here as it defines a stub PrintInfo that conflicts
// with the full definition below. Files that need SP.h should include it separately.

// g_SetFL2Surface/gpC_fl2_surface only ever had an SDL-backed (void*)
// implementation (FL2Rendering.cpp); the LPDIRECTDRAWSURFACE7
// variant lived in VS_UI/src/hangul/FL2.cpp, which is excluded from the
// build now that USE_SDL_BACKEND is mandatory on all platforms.
void	g_SetFL2Surface(void* surface);
extern void*	gpC_fl2_surface;

// Where text goes before the UI sets an FL2 surface: the game installs
// its surfaces (task 4.228). None installed: such text is not drawn.
class CSpriteSurface;
void	g_SetFL2FallbackSurface(CSpriteSurface* (*source)());

extern HDC gh_FL2_DC;

// PrintInfo definition - skip if already defined in SP.h
#ifndef PrintInfo_DEFINED
#define PrintInfo_DEFINED
struct PrintInfo
{
	void*		hfont;
	COLORREF	text_color;
	COLORREF	back_color;
	int		bk_mode;
	UINT		text_align;
};
#endif

//----------------------------------------------------------------------------
// Public
//----------------------------------------------------------------------------
int	g_DBCSLen(const char_t * p_dbcs);
int	g_GetByteLenth(const char_t * p_dbcs, int dbcs_len);
int	g_Convert_DBCS_Ascii2SingleByte(const char_t * p_dbcs, int dbcs_len, char * &p_new_buf);
int	g_ConvertAscii2DBCS(const char * p_ascii, int ascii_len, char_t * &p_new_buf);
void	g_Print(int x, int y, const char * sz_str, PrintInfo * p_print_info=NULL);
int		g_GetStringWidth(const char * sz_str, void* hfont=NULL);
int		g_GetStringHeight(const char * sz_str, void* hfont=NULL);
bool	g_FL2_GetDC();
bool	g_FL2_ReleaseDC();
int g_PrintColorStr(int x, int y, const char * sz_str, PrintInfo &pi, COLORREF str_rgb);
int g_PrintColorStrOut(int x, int y, const char * sz_str, PrintInfo &pi, COLORREF str_rgb, COLORREF out_rgb);
int g_PrintColorStrShadow(int x, int y, const char * sz_str, PrintInfo &pi, COLORREF str_rgb, COLORREF shadow_rgb = 0xFFFFFFFF);
std::string g_GetNumberString(int number);
std::string g_GetStringByMoney(DWORD dwMoney);


#endif