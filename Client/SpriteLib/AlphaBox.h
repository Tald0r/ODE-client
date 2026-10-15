//-----------------------------------------------------------------------------
// AlphaBox.h
//-----------------------------------------------------------------------------
#ifndef __ALPHA_BOX_H__
#define __ALPHA_BOX_H__

#include "../../basic/Platform.h"

class CSpriteSurface;

// Draws a translucent box of the colour (RGB565) over the surface, inside
// the screen (one short of its right and bottom edges). The alpha (0 to
// 31) is how strong the colour is. A black box darkens what is under it
// (CSpriteSurface::GammaBox565); a coloured one blends over it.
void	DrawAlphaBox(CSpriteSurface& surface, const RECT& screen, RECT box, WORD color, BYTE alpha);

#endif
