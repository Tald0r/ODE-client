//----------------------------------------------------------------------
// HPModifyLabel.h
//----------------------------------------------------------------------
// The text and colour of one HP-modify number that
// MTopView::DrawCreatureHPModify draws over a creature, kept free of
// game state so that it can be tested on its own.
//----------------------------------------------------------------------

#ifndef __HPMODIFYLABEL_H__
#define __HPMODIFYLABEL_H__

#include "Platform.h"

//----------------------------------------------------------------------
// FormatHPModifyLabel
//----------------------------------------------------------------------
// Writes modify as signed text into str and sets color to match: "-7"
// in light red for damage, "+12" (and "+0") in light green for a heal.
//----------------------------------------------------------------------
void	FormatHPModifyLabel(int modify, char (&str)[128], COLORREF& color);

#endif
