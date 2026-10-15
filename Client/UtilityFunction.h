//-----------------------------------------------------------------------------
// UtilityFunction.h
//-----------------------------------------------------------------------------

#ifndef __UTILITYFUNCTION_H__
#define __UTILITYFUNCTION_H__

class CSpriteSurface;

//-----------------------------------------------------------------------------
// Removes the spaces before and after a string.
//-----------------------------------------------------------------------------
void	RemoveStringSpace(char*& str);

//-----------------------------------------------------------------------------
// Is it a valid ID?
//-----------------------------------------------------------------------------
int		IsValidID(const char* strID, const char* strPermit);

//-----------------------------------------------------------------------------
// Is Valid Password
//-----------------------------------------------------------------------------
int		IsValidPassword(const char* str);

// Loads a *.bmp into the surface (the SDL backend's; USE_SDL_BACKEND is
// mandatory on all platforms).
bool	LoadImageToSurface(const char* pFilename, CSpriteSurface& surface);

#endif
