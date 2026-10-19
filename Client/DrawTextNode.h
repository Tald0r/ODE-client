//----------------------------------------------------------------------
// DrawTextNode.h
//----------------------------------------------------------------------
// Information for drawing a String on the screen.
//----------------------------------------------------------------------

#ifndef	__DRAWTEXTNODE_H__
#define	__DRAWTEXTNODE_H__

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif

#define	FLAG_DRAWTEXT_OUTLINE			0x01	// outline

class DRAWTEXT_NODE {
	public :
		DRAWTEXT_NODE(int x, int y, const char* str, COLORREF color=0x00FFFFFF, BYTE font=0, int flag=0)
			: String(str)
		{
			X		= x;
			Y		= y;
			Color	= color;
			Font	= font;
			Flag	= flag;	
			bBox	= false;
			bBoxOutline = false;
			TextTime = 0;
		}

		//-------------------------------------------------------
		// Box
		//-------------------------------------------------------
		bool			IsExistBox() const	{ return bBox; }
		void			SetBox(const RECT& rectBox, WORD boxColor, bool bTrans=true)
		{
			bBox		= true;
			Box			= rectBox;
			BoxColor	= boxColor;
			bTransBox	= bTrans;
		}
		RECT			GetBox() const		{ return Box; }
		WORD			GetBoxColor() const	{ return BoxColor; }

		bool			IsExistBoxOutline() const		{ return bBoxOutline; }
		void			SetBoxOutline(WORD color)
		{
			bBoxOutline = true;
			BoxOutlineColor = color;
		}
		
		WORD			GetBoxOutlineColor() const	{ return BoxOutlineColor; }

		//-------------------------------------------------------
		// Time
		//-------------------------------------------------------
		void			SetTextTime(QWORD time)	{ TextTime = time; }

		//-------------------------------------------------------
		// Get
		//-------------------------------------------------------
		int				GetX() const			{ return X; }		
		int				GetY() const			{ return Y; }
		const char*		GetString() const		{ return String; }
		COLORREF		GetColor() const		{ return Color; }
		BYTE			GetFont() const			{ return Font; }
		BYTE			IsOutline()	const		{ return Flag & FLAG_DRAWTEXT_OUTLINE; }
		bool			IsTransBox() const		{ return bTransBox; }
		QWORD			GetTextTime() const		{ return TextTime; }
		
	protected :
		int				X, Y;		// on-screen output coordinates
		const char*		String;		// string to output
		COLORREF		Color;		// text color
		BYTE			Font;		// font type
		BYTE			Flag;		// text effect
		bool			bBox;		// is there a box?
		RECT			Box;		// Box area laid under the text
		WORD			BoxColor;	// box color
		bool			bBoxOutline;	// does the box have a border?
		WORD			BoxOutlineColor;	// box line color
		bool			bTransBox;	// is it a transparent box?
		QWORD			TextTime;	// output time
};

#endif