//----------------------------------------------------------------------
// CUsingColorSetTable.h
//----------------------------------------------------------------------
// Of the ColorSets CIndexSprite uses,
// so that some sprites may use only certain colours..
// MAX_COLORSET bools are kept
// and a usable COLORSET's is set.
//----------------------------------------------------------------------

#ifndef	__CUSINGCOLORSETTABLE_H__
#define	__CUSINGCOLORSETTABLE_H__

#include "ColorSetDef.h"
#include "CTypeTable.h"

#include <fstream>


class USINGCOLORSET_NODE {
	public :
		USINGCOLORSET_NODE();
		~USINGCOLORSET_NODE();

		//-------------------------------------------------
		// Enable / Disable
		//-------------------------------------------------
		void			Enable(int set);
		void			Disable(int set);

		//-------------------------------------------------
		// Enable / Disable a range
		//-------------------------------------------------
		void			Enable(int start, int end);
		void			Disable(int start, int end);

		//-------------------------------------------------
		// File I/O
		//-------------------------------------------------
		void			SaveToFile(std::ofstream& file);
		bool			LoadFromFile(std::ifstream& file);

	protected :
		bool			m_bEnable[MAX_COLORSET];
};


typedef CTypeTable<USINGCOLORSET_NODE>		USINGCOLORSET_TABLE;
extern 	USINGCOLORSET_TABLE*	g_pUsingColorSetTable;


//----------------------------------------------------------------------
// the kinds of UsingColorSet...
//----------------------------------------------------------------------
enum USINGCOLORSET
{
	USINGCOLORSET_NONE,
	USINGCOLORSET_ALL,
	USINGCOLORSET_SKIN,
	USINGCOLORSET_ITEM,
	MAX_USINGCOLORSET
};


#endif

