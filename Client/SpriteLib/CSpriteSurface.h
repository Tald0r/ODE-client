#include <fstream>
//----------------------------------------------------------------------
// CDirectDrawSurface.h
//----------------------------------------------------------------------
//
//
// 5:6:5 와  5:5:5의 차이는 
// Bit Mask에 의해 처리되는 부분에서 발생하는데
// CDirectDraw class의 InitMask()에서 Video Card에 맞는
// 적절한 Mask를 생성하므로 그냥~ CDirectDraw의 Mask를 사용하면 된다.
//
//
//
//----------------------------------------------------------------------
/*

  < DDSurface가 생성되는 MEMORY >

	- DDSCAPS_SYSTEMMEMORY는 Lock을 사용할 때 빠르다.
	- DDSCAPS_VIDEOMEMORY는 Blt를 사용할 때 빠르다.
	  단, VIDEOMEMORY가 부족하면 Surface를 생성하지 않으므로 error!
	- 지정을 안 해 주면 VIDEOMEMORY가 사용가능하면 사용하고 
   	  아니면 SYSTEMMEMORY를 사용한다.
	
  < Alpha Blending >
    - 사용하려면 DDSCAPS_SYSTEMMEMORY를 사용하는게 좋다.
	- Surface --> Surface로의 Alpha Blending보다
	  Memory(SpriteFile) --> Surface로의 Alpha Blending이 빠르다.

  < Sprite File >
    - 0번 압축 된 것과 안 된 것 두가지의 경우가 있다.
	- 0번 압축 된 것은 투명색이 있는 모든 경우에 빠르다.
	  단, MMX instruction으로 투명색을 제거할 경우가 더 빠르겠지...

  < 사용시 주의 사항 >
    - Clipping이 되는 함수와 안 되는 함수가 있는데
	  적절한 경우에 맞춰서 사용해야할 것이다. 속도 문제가 있으니까!


*/
//----------------------------------------------------------------------

#ifndef	__CSPRITESURFACE_H__
#define	__CSPRITESURFACE_H__

// The project builds one SDL surface type on every platform.
#include "SpriteLibBackend.h"
#include "../../basic/2d.h"

#include "CSpritePal.h"
#include "CAlphaSpritePal.h"
#include "MPalettePack.h"

class CSprite;
class CAlphaSprite;
class CIndexSprite;
class CShadowSprite;
class CSpriteOutlineManager;
class CFilter;

typedef void (*FUNCTION_MEMCPYEFFECT)(WORD*, WORD*, WORD);
typedef void (*FUNCTION_MEMCPYPALEFFECT)(WORD*, BYTE*, WORD, MPalette &);

/* ============================================================================
 * SDL Backend: Standalone CSpriteSurface (not inheriting from CDirectDrawSurface)
 * ============================================================================ */

class CSpriteSurface {
	public :
		CSpriteSurface();
		~CSpriteSurface();

		//------------------------------------------------------------
		// Surface Initialization
		//------------------------------------------------------------
		bool Init(int width, int height);
		bool InitFromFile(const char* filename);
		void Release();

		//------------------------------------------------------------
		// Drawing Functions
		//------------------------------------------------------------
		void	DrawRect(RECT* rect, WORD color);
		void	HLine(int x, int y, int length, WORD color);
		void	VLine(int x, int y, int length, WORD color);
		void	Line(int x1, int y1, int x2, int y2, WORD color);
		void	FillRect(RECT* rect, WORD color);

		//------------------------------------------------------------
		// BltFast methods (stubs for compatibility)
		// Note: In SDL backend, use CSpriteSurface* instead of CDirectDrawSurface*
		//------------------------------------------------------------
		void	BltHalf(POINT* pPoint, CSpriteSurface* SourceSurface, RECT* pRect);
		void	BltNoColorkey(POINT* pPoint, CSpriteSurface* SourceSurface, RECT* pRect);
		void	BltDarkness(POINT* pPoint, CSpriteSurface* SourceSurface, RECT* pRect, BYTE DarkBits);
		void	BltBrightness(POINT* pPoint, CSpriteSurface* SourceSurface, RECT* pRect, BYTE BrightBits);
		void	BltDarknessFilter(POINT* pPoint, CSpriteSurface* SourceSurface, RECT* pRect, WORD TransColor=0);

		// Change brightness
		void	ChangeBrightnessBit(RECT* pRect, BYTE DarkBits);
		void	BltColorAlpha(RECT* pRect, WORD color, BYTE alpha2);
		// Opt out of reset checkpoints only for surfaces redrawn before use.
		void	SetTransient(bool transient);
		void	ApplyLightGrid(const CFilter& filter, const int* widths, const int* heights);
		void	ColorBox(const RECT* rect, BYTE rgb);

		//------------------------------------------------------------
		// Sprite Blitting Methods
		//------------------------------------------------------------
		// Sprite
		void	BltSprite(POINT* pPoint, CSprite* pSprite);
		// Clip in sprite coordinates without exposing destination pixels.
		void	BltSpriteClip(POINT* point, CSprite* sprite, const RECT* source);
		void	BltSpriteColorClip(POINT* point, CSprite* sprite, const RECT* source, BYTE rgb);
		void	BltSpriteEffectClip(POINT* point, CSprite* sprite, const RECT* source);
		void	BltSpriteNoClip(POINT* pPoint, CSprite* pSprite);
		void	BltSpriteHalf(POINT* pPoint, CSprite* pSprite);
		void	BltSpriteAlpha(POINT* pPoint, CSprite* pSprite, BYTE alphaDepth);
		void	BltSpriteColor(POINT* pPoint, CSprite* pSprite, BYTE rgb);
		void	BltSpriteDarkness(POINT* pPoint, CSprite* pSprite, BYTE DarkBits);
		void	BltSpriteColorSet(POINT* pPoint, CSprite* pSprite, WORD colorSet);
		void	BltSpriteEffect(POINT* pPoint, CSprite* pSprite);
		void	BltSpriteScale(POINT* pPoint, CSprite* pSprite, int scale=256);
		void	BltSpriteAlpha4444SmallNotTrans(POINT* pPoint, CSprite* pSprite, BYTE alpha, BYTE shift);
		void	BltSpriteAlpha4444NotTrans(POINT* pPoint, CSprite* pSprite, BYTE alpha);
		void	BltSprite1555SmallNotTrans(POINT* pPoint, CSprite* pSprite, BYTE shift);
		void	BltSprite1555NotTrans(POINT* pPoint, CSprite* pSprite);

		void	BltSpritePalEffect(POINT* pPoint, CSpritePal* pSprite, MPalette &pal);
		void	BltSpritePal1555SmallNotTrans(POINT* pPoint, CSpritePal* pSprite, BYTE shift, MPalette &pal);
		void	BltSpritePal1555NotTrans(POINT* pPoint, CSpritePal* pSprite, MPalette &pal);

		// Filter
		void	BltSpriteAlphaFilter(POINT* pPoint, CSprite* pSprite);
		void	BltSpriteAlphaFilterDarkness(POINT* pPoint, CSprite* pSprite, BYTE DarkBits);
		void	BltSpriteDarkerFilter(POINT* pPoint, CSprite* pSprite);

		// AlphaSprite
		void	BltAlphaSprite(POINT* pPoint, CAlphaSprite* pSprite);
		void	BltAlphaSpriteAlpha(POINT* pPoint, CAlphaSprite* pSprite, BYTE alpha);
		void	BltAlphaSprite4444(POINT* pPoint, CAlphaSprite* pSprite);
		void	BltAlphaSprite4444NotTrans(POINT* pPoint, CAlphaSprite* pSprite);
		void	BltAlphaSprite4444SmallNotTrans(POINT* pPoint, CAlphaSprite* pSprite, BYTE shift);

		void	BltAlphaSpritePal(POINT* pPoint, CAlphaSpritePal* pSprite, MPalette &pal);
		void	BltAlphaSpritePalAlpha(POINT* pPoint, CAlphaSpritePal* pSprite, BYTE alpha, MPalette &pal);
		void	BltAlphaSpritePal4444(POINT* pPoint, CAlphaSpritePal* pSprite, MPalette &pal);
		void	BltAlphaSpritePal4444NotTrans(POINT* pPoint, CAlphaSpritePal* pSprite, MPalette &pal);
		void	BltAlphaSpritePal4444SmallNotTrans(POINT* pPoint, CAlphaSpritePal* pSprite, BYTE shift, MPalette &pal);

		// IndexSprite
		void	BltIndexSprite(POINT* pPoint, CIndexSprite* pSprite);
		void	BltIndexSpriteDarkness(POINT* pPoint, CIndexSprite* pSprite, BYTE DarkBits);
		void	BltIndexSpriteAlpha(POINT* pPoint, CIndexSprite* pSprite, BYTE alpha);
		void	BltIndexSpriteColor(POINT* pPoint, CIndexSprite* pSprite, BYTE rgb);
		void	BltIndexSpriteColorSet(POINT* pPoint, CIndexSprite* pSprite, WORD colorSet);
		void	BltIndexSpriteEffect(POINT* pPoint, CIndexSprite* pSprite);
		void	BltIndexSpriteBrightness(POINT* pPoint, CIndexSprite* pSprite, BYTE BrightBits);

		// Sprite Outline
		void	BltSpriteOutline(CSpriteOutlineManager *pSOM, WORD color);
		void	BltSpriteOutlineOnly(CSpriteOutlineManager* pSOM, WORD color);
		void	BltSpriteOutlineDarkness(CSpriteOutlineManager* pSOM, WORD color, BYTE DarkBits);

		// ShadowSprite
		void	BltShadowSprite(POINT* pPoint, CShadowSprite* pSprite);
		void	BltShadowSpriteSmall(POINT* pPoint, CShadowSprite* pSprite, BYTE shift);
		void	BltShadowSpriteDarkness(POINT* pPoint, CShadowSprite* pSprite, BYTE DarkBits);
		void	BltShadowSprite4444(POINT* pPoint, CShadowSprite* pSprite, WORD pixel);
		void	BltShadowSpriteSmall4444(POINT* pPoint, CShadowSprite* pSprite, WORD pixel, BYTE shift);

		//------------------------------------------------------------
		// Static Effect Functions (stubs for compatibility)
		//------------------------------------------------------------
		static void		InitEffectTable();
		static void		memcpyHalf(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyAlpha(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyColor(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyScale(WORD* pDest, WORD destPitch, WORD* pSource, WORD pixels);
		static void		memcpyDarkness(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyBrightness(WORD* pDest, WORD* pSource, WORD pixels);
		static void		Gamma4Pixel565(void *pDest, int len, int p);
		static void		Gamma4Pixel555(void *pDest, int len, int p);
		static WORD		memcpyAlpha1Pixel(WORD pDest, WORD pSource) { (void)pDest; (void)pSource; return 0; }

		// GammaBox - applies Gamma4Pixel565/555 to each row of pRect on this surface
		void	GammaBox565(RECT* pRect, int p);
		void	GammaBox555(RECT* pRect, int p);

		static int		s_Value1;
		static int		s_Value2;
		static int		s_Value3;

	public:
		enum FUNCTION_EFFECT
		{
			EFFECT_DARKER = 0,
			EFFECT_GRAY_SCALE,
			EFFECT_LIGHTEN,
			EFFECT_DARKEN,
			EFFECT_COLOR_DODGE,
			EFFECT_SCREEN,
			EFFECT_DODGE_BURN,
			EFFECT_DIFFERENT,
			EFFECT_GRADATION,
			EFFECT_SIMPLE_OUTLINE,
			EFFECT_WIPE_OUT,
			EFFECT_NET,
			EFFECT_GRAY_SCALE_VARIOUS,
			EFFECT_SCREEN_ALPHA,
			MAX_EFFECT
		};

		static void		SetEffect(enum FUNCTION_EFFECT func);
		static void		SetPalEffect(enum FUNCTION_EFFECT func);
		static void		memcpyEffect(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyPalEffect(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);

		// Effect functions - implemented in CSpriteSurface.cpp
		static void		memcpyEffectDarker(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyEffectLighten(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyEffectDarken(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyEffectColorDodge(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyEffectScreen(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyEffectDodgeBurn(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyEffectDifferent(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyEffectGrayScale(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyEffectSimpleOutline(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyEffectWipeOut(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyEffectNet(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyEffectGrayScaleVarious(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyEffectScreenAlpha(WORD* pDest, WORD* pSource, WORD pixels);
		static void		memcpyEffectGradation(WORD* pDest, WORD* pSource, WORD pixels);

		static void		memcpyPalEffectDarker(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);
		static void		memcpyPalEffectGrayScale(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);
		static void		memcpyPalEffectLighten(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);
		static void		memcpyPalEffectDarken(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);
		static void		memcpyPalEffectColorDodge(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);
		static void		memcpyPalEffectScreen(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);
		static void		memcpyPalEffectDodgeBurn(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);
		static void		memcpyPalEffectDifferent(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);
		static void		memcpyPalEffectGradation(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);
		static void		memcpyPalEffectSimpleOutline(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);
		static void		memcpyPalEffectWipeOut(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);
		static void		memcpyPalEffectNet(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);
		static void		memcpyPalEffectGrayScaleVarious(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);
		static void		memcpyPalEffectScreenAlpha(WORD* pDest, BYTE* pSource, WORD pixels, MPalette &pal);

		static FUNCTION_MEMCPYEFFECT		s_pMemcpyEffectFunction;
		static FUNCTION_MEMCPYPALEFFECT		s_pMemcpyPalEffectFunction;
		static FUNCTION_MEMCPYEFFECT		s_pMemcpyEffectFunctionTable[MAX_EFFECT];
		static FUNCTION_MEMCPYPALEFFECT		s_pMemcpyPalEffectFunctionTable[MAX_EFFECT];

		// The screen blend tables are indexed [dest][source] by
		// channel value. Red and blue are 5 bits on this backend;
		// ColorDraw::Green decodes the full 6-bit 5:6:5 field, so
		// its table is 64 wide - a 32-wide table read with a green
		// of 32 or more runs off its end. InitEffectTable fills them.
		static WORD		s_EffectScreenTableR[32][32];
		static WORD		s_EffectScreenTableG[64][64];
		static WORD		s_EffectScreenTableB[32][32];

		//------------------------------------------------------------
		// Clipping a sprite against the surface
		//
		// The static pixel-buffer helpers below clip to the supplied
		// surface bounds. SDL blits also honor SetClip on their destination.
		// ClipSpriteToSurface reduces a placement to the sprite-local
		// rectangle that is visible, the surface pixel its top-left
		// lands on, and which of the CSpritePal / CAlphaSpritePal
		// clip variants draws it: each variant walks the rows
		// pRect->top..bottom from the pDest it is handed, and the
		// Left/Width variants skip pRect->left source pixels per row.
		//
		// The *To functions draw into an already locked pixel buffer.
		// They are what the member blits call once the backend
		// surface is locked, and they exist as statics so a test can
		// drive the same code against a plain array.
		//------------------------------------------------------------
		enum SPRITE_CLIP
		{
			SPRITE_CLIP_OUTSIDE = 0,	// nothing of the sprite is visible
			SPRITE_CLIP_NONE,			// the whole sprite is visible
			SPRITE_CLIP_HEIGHT,			// rows cut, every column visible
			SPRITE_CLIP_LEFT,			// left columns cut, right edge visible
			SPRITE_CLIP_RIGHT,			// right columns cut, left edge visible
			SPRITE_CLIP_WIDTH			// both edges cut
		};

		static SPRITE_CLIP	ClipSpriteToSurface(int x, int y, int width, int height,
											int surfaceWidth, int surfaceHeight,
											RECT* pRect, POINT* pDest);
		static void		BltSpritePalEffectTo(WORD* pPixels, int pitch,
											int surfaceWidth, int surfaceHeight,
											const POINT* pPoint, CSpritePal* pSprite, MPalette &pal);
		static void		BltAlphaSpritePalTo(WORD* pPixels, int pitch,
											int surfaceWidth, int surfaceHeight,
											const POINT* pPoint, CAlphaSpritePal* pSprite, MPalette &pal);

		//------------------------------------------------------------
		// DirectX Compatibility Methods (Stubs for SDL backend)
		//------------------------------------------------------------
		bool				Restore();
		// No-argument Lock is the legacy drawing-state flag. Direct pixel
		// access uses Lock(nullptr, &pitch) or GetSurfaceInfo, then Unlock.
		bool				Lock();
		void*				Lock(RECT* rect, DWORD* pitch = NULL);
		void				Unlock();
		int					GetSurfacePitch() const;
		void*				GetSurfacePointer();
		int					GetWidth() const;
		int					GetHeight() const;

		// Lock/Unlock methods for SDL backend compatibility (different signatures from Windows)
		bool				LockSDL();  // Renamed to avoid conflict with Lock()
		void				UnlockSDL();
		bool				IsLock();

		// NEW: Query actual lock state (for debugging)
		bool				IsLocked() const { return m_lock_count > 0; }
		int					GetLockCount() const { return m_lock_count; }

		void				GetSurfaceInfo(S_SURFACEINFO* info);
		// GetDDSD compatibility wrapper - returns pointer to internal surface info
		S_SURFACEINFO*		GetDDSD();

		// Get backend surface handle (for presenting to SDL renderer)
		spritectl_surface_t	GetBackendSurface() const { return m_backend_surface; }
		bool				InitTextureSurface(int width, int height, void* pixels = NULL, void* pixelFormat = NULL);

		// Off-screen surface initialization (compatibility with CDirectDrawSurface)
		bool				InitOffsurface(int width, int height);
		void				SetTransparency(int value);
		int					GetTransparency() const;

		// GDI text rendering (stub for SDL backend - text rendering not implemented)
		void				GDI_Text(int x, int y, const char* text, DWORD color);

		// FillSurface (compatibility with CDirectDrawSurface)
		void				FillSurface(WORD color);

		// Clipping methods (compatibility with CDirectDrawSurface)
		void				SetClip(RECT* rect);
		void				SetClipNULL();

		// Blt method (compatibility with CDirectDrawSurface)
		void				Blt(POINT* pPoint, CSpriteSurface* SourceSurface, RECT* pRect);

		//------------------------------------------------------------
		// Clip methods (compatibility with CDirectDrawSurface)
		//------------------------------------------------------------
		inline int		GetClipRight() const		{ return m_width; }  // Stub: return full width
		inline int		GetClipBottom() const		{ return m_height; } // Stub: return full height
		inline void		SetClipRight(int Right)		{ (void)Right; /* Stub: no-op */ }
		inline void		SetClipRightBottom(int Right, int Bottom) { (void)Right; (void)Bottom; /* Stub: no-op */ }
		inline void		SetClipLeftTop(int Left, int Top) { (void)Left; (void)Top; /* Stub: no-op */ }

		// GetSurface method (stub for compatibility - returns nullptr)
		inline void*		GetSurface()				{ return nullptr; }

	protected:
		bool	ClippingRectToPoint(RECT*& pRect, POINT*& pPoint);

		/* Backend surface handle */
		spritectl_surface_t m_backend_surface;
		int m_width;
		int m_height;
		int m_transparency;  // Transparency value for compatibility

		// DirectDraw-style lock flag (0 or 1), never a nesting counter.
		// See the "Lock state model" comment in CSpriteSurface_SDL.cpp.
		int m_lock_count;

		// Clipping rectangles (compatibility with CDirectDrawSurface)
		int m_ClipLeft;
		int m_ClipTop;
		int m_ClipRight;
		int m_ClipBottom;

		// Surface description (compatibility with CDirectDrawSurface)
		S_SURFACEINFO m_ddsd;  // Using SDL-compatible surface info structure
};


#endif /* __CSPRITESURFACE_H__ */
