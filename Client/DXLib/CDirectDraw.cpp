//----------------------------------------------------------------------
// CDirectDraw.cpp
//
// SDL2 Implementation (Cross-platform)
// Windows DirectX implementation removed - using SDL2 on all platforms
// NOTE: Static member definitions are in CDirectDraw_StaticMembers.cpp
//----------------------------------------------------------------------

#include "CDirectDraw.h"
#include <stdio.h>
// spritectl_init() and the frame presentation. This file is compiled into
// the standalone dxlib library (no /IClient, no SPRITELIB_BACKEND_SDL), so
// it does not include SpriteLib/CSpriteSurface.h: Flip asks the source the
// executable installs for the back buffer's backend surface (task 4.211).
#include "../SpriteLib/SpriteLibBackend.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(int, BrowserCanvasDimension, (int vertical), {
	const rect = Module['canvas'].getBoundingClientRect();
	const scale = Math.min(devicePixelRatio || 1, 4096 / Math.max(1, rect.width), 4096 / Math.max(1, rect.height));
	return Math.max(1, Math.round((vertical ? rect.height : rect.width) * scale));
});
extern "C" EMSCRIPTEN_KEEPALIVE void darkeden_resize_canvas(int width, int height)
{
	CSDLGraphics::ResizeBrowserCanvas(width, height);
}
void CSDLGraphics::ResizeBrowserCanvas(int width, int height)
{
	if (!m_pSDLWindow || width <= 0 || height <= 0 || width > 4096 || height > 4096) return;
	int oldWidth = 0, oldHeight = 0;
	SDL_GetWindowSize(m_pSDLWindow, &oldWidth, &oldHeight);
	if (oldWidth != width || oldHeight != height) SDL_SetWindowSize(m_pSDLWindow, width, height);
}
#endif

//-----------------------------------------------------------------------------
// Static member initialization for DirectDraw objects
// Note: These are opaque pointers/stubs for SDL2 backend
//-----------------------------------------------------------------------------
LPDIRECTDRAW7					CSDLGraphics::m_pDD					= NULL;
LPDIRECTDRAWSURFACE7			CSDLGraphics::m_pDDSPrimary			= NULL;
LPDIRECTDRAWSURFACE7			CSDLGraphics::m_pDDSBack				= NULL;
LPDIRECTDRAWGAMMACONTROL	CSDLGraphics::m_pDDGammaControl		= NULL;

DDSURFACEDESC2					CSDLGraphics::m_ddsd;

SDL_Window*						CSDLGraphics::m_pSDLWindow			= NULL;
SDL_Renderer*					CSDLGraphics::m_pSDLRenderer			= NULL;

HWND								CSDLGraphics::m_hWnd					= NULL;

bool								CSDLGraphics::m_bFullscreen			= true;
WORD								CSDLGraphics::m_ScreenWidth			= 0;
WORD								CSDLGraphics::m_ScreenHeight			= 0;
bool								CSDLGraphics::m_b565					= true;
bool								CSDLGraphics::m_b3D					= true;
bool								CSDLGraphics::m_bMMX					= false;
bool								CSDLGraphics::m_bGammaControl		= false;
DDGAMMARAMP						CSDLGraphics::m_DDGammaRamp;
WORD								CSDLGraphics::m_GammaStep				= 0;
WORD								CSDLGraphics::m_AddGammaStep[3];

RECT								CSDLGraphics::m_rcWindow;
RECT								CSDLGraphics::m_rcScreen;
RECT								CSDLGraphics::m_rcViewport;

// Note: Color mask static members are defined in CDirectDraw_StaticMembers.cpp

//-----------------------------------------------------------------------------
// Constructor/Destructor (stub - not implemented)
//-----------------------------------------------------------------------------
CSDLGraphics::CSDLGraphics()
{
}

CSDLGraphics::~CSDLGraphics()
{
}

//-----------------------------------------------------------------------------
// Init
//
// On Windows hWnd is a real native window already created by
// CreateWindowEx() before this is called; SDL_CreateWindowFrom() wraps it
// instead of creating a new window, so SDL renders into the same window
// Win32 message handling uses. Off Windows there is no native window -
// InitApp skips CreateWindowEx and hWnd is NULL - so the window is SDL's
// own, created here at the requested size: fullscreen as a borderless
// desktop-sized window, which is what SDL scales the 800x600 or 1024x768
// frame into, and window mode at the frame's own size. SDLMain.cpp used to
// create a window of its own beside this class, which then had none.
//-----------------------------------------------------------------------------
void CSDLGraphics::Init(HWND hWnd, WORD width, WORD height, SCREENMODE mode, bool bUseHAL, bool bUseIME)
{
	(void)bUseHAL;
	(void)bUseIME;

	ReleaseAll();

	spritectl_init();
	bool selectedOpenGL = false;
	// The programmable effect path shares SDL's OpenGL context. Keep an
	// explicit SDL_RENDER_DRIVER override, and retain SDL's normal fallback
	// selection on systems where OpenGL is unavailable.
	if (!SDL_GetHint(SDL_HINT_RENDER_DRIVER)) {
		for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
			SDL_RendererInfo info{};
			if (SDL_GetRenderDriverInfo(i, &info) == 0 && SDL_strcmp(info.name, "opengl") == 0) {
				selectedOpenGL = SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl") == SDL_TRUE;
				break;
			}
		}
	}

#ifdef PLATFORM_WINDOWS
	const char* driver = SDL_GetHint(SDL_HINT_RENDER_DRIVER);
	if (driver && SDL_strcmp(driver, "opengl") == 0)
		SDL_SetHint(SDL_HINT_VIDEO_FOREIGN_WINDOW_OPENGL, "1");
	m_pSDLWindow = SDL_CreateWindowFrom((void*)hWnd);
#else
	// ALLOW_HIGHDPI: on a Retina display (and a scaled Wayland desktop)
	// the renderer's output is then the display's native pixel grid and
	// the letterbox upscale in spritectl_present_surface has every pixel
	// to work with, instead of drawing at the window's point size and
	// being magnified by the compositor. Window and mouse coordinates
	// stay in points; spritectl_window_to_game_coords maps them through
	// the point-to-pixel ratio, so the two do not have to agree. The
	// cost: the xBRZ filter's factor (FrameUpscaler::ScaleFactor) is
	// chosen from the pixel size, so a 2x display can ask for 3x or 4x
	// where it asked for 2x, on the CPU, every frame.
	Uint32 flags = SDL_WINDOW_SHOWN;
#ifndef __EMSCRIPTEN__
	flags |= SDL_WINDOW_ALLOW_HIGHDPI;
	if (mode == FULLSCREEN)
	{
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
	}
#endif
	m_pSDLWindow = SDL_CreateWindow("Dark Eden",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
#ifdef __EMSCRIPTEN__
		BrowserCanvasDimension(0), BrowserCanvasDimension(1), flags);
#else
		width, height, flags);
#endif
	if (m_pSDLWindow == NULL)
	{
		fprintf(stderr, "CSDLGraphics::Init: SDL_CreateWindow failed: %s\n", SDL_GetError());
	}
#endif
	if (m_pSDLWindow == NULL)
	{
		if (selectedOpenGL) SDL_ResetHint(SDL_HINT_RENDER_DRIVER);
		return;
	}

	// The present path reads this window's point size against the
	// renderer's pixel size for the mouse mapping; on Windows the two are
	// equal and the mapping is unchanged.
	spritectl_set_present_window(m_pSDLWindow);

	// Vsync paces the render loop at the monitor refresh rate now that the
	// game draws interpolated frames between its 62 ms logic ticks (see
	// CGameUpdate.cpp). Without it the message loop would spin-present.
	// Renderers that ignore vsync (e.g. SDL_RENDER_DRIVER=software) are paced
	// by the fallback frame cap in CGameUpdate::Update instead.
	m_pSDLRenderer = SDL_CreateRenderer(m_pSDLWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (m_pSDLRenderer == NULL)
	{
		m_pSDLRenderer = SDL_CreateRenderer(m_pSDLWindow, -1, SDL_RENDERER_ACCELERATED);
	}
	// Do not let an unavailable automatic preference prevent startup. Restore
	// the hint after creation so later device recreation probes capabilities too.
	if (selectedOpenGL) SDL_ResetHint(SDL_HINT_RENDER_DRIVER);
	if (m_pSDLRenderer == NULL && selectedOpenGL)
	{
		m_pSDLRenderer = SDL_CreateRenderer(m_pSDLWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	}
	if (m_pSDLRenderer == NULL)
	{
		m_pSDLRenderer = SDL_CreateRenderer(m_pSDLWindow, -1, 0);
	}

	if (m_pSDLRenderer != NULL)
	{
		SDL_SetRenderDrawColor(m_pSDLRenderer, 0, 0, 0, 255);
		spritectl_set_render_device(m_pSDLRenderer);
	}

	m_hWnd = hWnd;
	m_ScreenWidth = width;
	m_ScreenHeight = height;
	m_bFullscreen = (mode == FULLSCREEN);
}

//-----------------------------------------------------------------------------
// Flip
//-----------------------------------------------------------------------------
namespace {
spritectl_surface_t (*s_backBufferSource)() = NULL;
}

void CSDLGraphics::SetBackBufferSource(spritectl_surface_s* (*source)())
{
	s_backBufferSource = source;
}

spritectl_surface_s* CSDLGraphics::BackBuffer()
{
	return s_backBufferSource != NULL ? s_backBufferSource() : SPRITECTL_INVALID_SURFACE;
}

void CSDLGraphics::Flip()
{
	PresentFrame(m_pSDLRenderer, BackBuffer());
}

void CSDLGraphics::PresentFrame(SDL_Renderer* renderer, spritectl_surface_s* backBuffer)
{
	if (renderer == NULL)
	{
		return;
	}

	spritectl_finish_rendering();
	SDL_RenderClear(renderer);

	if (backBuffer != SPRITECTL_INVALID_SURFACE)
	{
		spritectl_present_surface(backBuffer, renderer);
	}

	SDL_RenderPresent(renderer);
}

//-----------------------------------------------------------------------------
// ReleaseAll
//-----------------------------------------------------------------------------
void CSDLGraphics::ReleaseAll()
{
	if (m_pSDLRenderer != NULL)
	{
		spritectl_release_present_resources(m_pSDLRenderer);
		SDL_DestroyRenderer(m_pSDLRenderer);
		m_pSDLRenderer = NULL;
	}

	if (m_pSDLWindow != NULL)
	{
		// On Windows SDL_CreateWindowFrom() wrapped an externally-owned native
		// window, and destroying it here only releases SDL's wrapper, not hWnd
		// itself; off Windows the window is SDL's own (Init above) and this
		// destroys it.
		spritectl_set_present_window(NULL);
		SDL_DestroyWindow(m_pSDLWindow);
		m_pSDLWindow = NULL;
	}
}

//-----------------------------------------------------------------------------
// InitMask
//-----------------------------------------------------------------------------
void CSDLGraphics::InitMask(bool b565)
{
	// 5:6:5 format for SDL2
	s_wMASK_SHIFT[0] = 11;
	s_wMASK_SHIFT[1] = 5;
	s_wMASK_SHIFT[2] = 0;
	s_wMASK_SHIFT[3] = 0;
	s_wMASK_SHIFT[4] = 0;

	s_dwMASK_SHIFT[0] = 0xF800;
	s_dwMASK_SHIFT[1] = 0x07E0;
	s_dwMASK_SHIFT[2] = 0x001F;
	s_dwMASK_SHIFT[3] = 0;
	s_dwMASK_SHIFT[4] = 0;

	s_wMASK_RGB[0] = 0;
	s_wMASK_RGB[1] = 11;
	s_wMASK_RGB[2] = 5;
	s_wMASK_RGB[3] = 0;
	s_wMASK_RGB[4] = 0;
	s_wMASK_RGB[5] = 0;

	s_dwMASK_RGB[0] = 0x0000F800;
	s_dwMASK_RGB[1] = 0x000007E0;
	s_dwMASK_RGB[2] = 0x0000001F;
	s_dwMASK_RGB[3] = 0;
	s_dwMASK_RGB[4] = 0;
	s_dwMASK_RGB[5] = 0;

	s_qwMASK_RGB[0] = 0x000000000000F800;
	s_qwMASK_RGB[1] = 0x0000000000007E0;
	s_qwMASK_RGB[2] = 0x00000000000001F;
	s_qwMASK_RGB[3] = 0;
	s_qwMASK_RGB[4] = 0;
	s_qwMASK_RGB[5] = 0;

	s_bSHIFT_R = 3;
	s_bSHIFT_G = 2;
	s_bSHIFT_B = 3;
	s_bSHIFT_A = 4;

	s_dwMASK_SHIFT_COUNT[0] = 5;
	s_dwMASK_SHIFT_COUNT[1] = 6;
	s_dwMASK_SHIFT_COUNT[2] = 5;
	s_dwMASK_SHIFT_COUNT[3] = 0;
	s_dwMASK_SHIFT_COUNT[4] = 0;

	s_dwMASK_RGB_COUNT[0] = 0;
	s_dwMASK_RGB_COUNT[1] = 5;
	s_dwMASK_RGB_COUNT[2] = 6;
	s_dwMASK_RGB_COUNT[3] = 0;
	s_dwMASK_RGB_COUNT[4] = 0;
	s_dwMASK_RGB_COUNT[5] = 0;

	(void)b565;  // Parameter kept for compatibility
}

//-----------------------------------------------------------------------------
// Bitmask methods for SDL2
//-----------------------------------------------------------------------------

int CSDLGraphics::Get_Count_Rbit()
{
	// For 5:6:5 format, R uses 5 bits
	return 5;
}

int CSDLGraphics::Get_Count_Gbit()
{
	// For 5:6:5 format, G uses 6 bits
	return 6;
}

int CSDLGraphics::Get_Count_Bbit()
{
	// For 5:6:5 format, B uses 5 bits
	return 5;
}

DWORD CSDLGraphics::Get_R_Bitmask()
{
	// 5:6:5 format: R is at bits 11-15
	return 0xF800;
}

DWORD CSDLGraphics::Get_G_Bitmask()
{
	// 5:6:5 format: G is at bits 5-10
	return 0x07E0;
}

DWORD CSDLGraphics::Get_B_Bitmask()
{
	// 5:6:5 format: B is at bits 0-4
	return 0x001F;
}

DWORD CSDLGraphics::Get_BPP()
{
	// SDL2 typically uses 16-bit color
	return 16;
}
