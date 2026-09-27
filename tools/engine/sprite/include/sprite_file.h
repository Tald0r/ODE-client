/*
 * sprite_file.h - fopen for the sprite tools without MSVC's deprecation.
 *
 * On Windows fopen opens with _SH_DENYNO; _fsopen with that mode is the
 * same call, and MSVC does not deprecate it. Elsewhere this is fopen.
 */
#ifndef SPRITE_FILE_H
#define SPRITE_FILE_H

#include <stdio.h>
#ifdef _WIN32
#include <share.h>
#endif

static inline FILE* sprite_open_file(const char* path, const char* mode)
{
#ifdef _WIN32
    return _fsopen(path, mode, _SH_DENYNO);
#else
    return fopen(path, mode);
#endif
}

#endif
