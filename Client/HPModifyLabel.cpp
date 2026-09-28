//----------------------------------------------------------------------
// HPModifyLabel.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "HPModifyLabel.h"
#include "SafeFormat.h"

//----------------------------------------------------------------------
// FormatHPModifyLabel
//----------------------------------------------------------------------
void
FormatHPModifyLabel(int modify, char (&str)[128], COLORREF& color)
{
	if(modify < 0 )
	{
		SafeFormat::Format(str, "%d", modify);
		color = RGB(255, 150, 150);
	}
	else
	{
		SafeFormat::Format(str, "+%d", modify);
		RGB(150, 255, 150);
	}
}
