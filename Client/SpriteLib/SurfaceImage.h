//-----------------------------------------------------------------------------
// SurfaceImage.h
//-----------------------------------------------------------------------------
#ifndef __SURFACE_IMAGE_H__
#define __SURFACE_IMAGE_H__

class CSpriteSurface;

// Loads a *.bmp file (a name of at least 8 characters, the extension in any
// case) into the surface, converted to RGB565: as much of it as fits, from
// the top left. False for another file, one SDL cannot load, or a surface
// that cannot be locked.
bool	LoadImageToSurface(const char* pFilename, CSpriteSurface& surface);

#endif
