//---------------------------------------------------------------------------
// MHelpStringTable.h
//---------------------------------------------------------------------------

#ifndef __MHELPSTRINGTABLE_H__
#define __MHELPSTRINGTABLE_H__

#include "MStringArray.h"
#include "MHelpDef.h"
#include <vector>

class MHelpStringTable : public MStringArray {
	public :
		MHelpStringTable();
		~MHelpStringTable();
		MHelpStringTable(const MHelpStringTable&) = delete;
		MHelpStringTable& operator=(const MHelpStringTable&) = delete;

		//------------------------------------------------------
		// Init
		//------------------------------------------------------
		void		Init( int size );
		void		Release();

		//------------------------------------------------------
		// Displayed
		//------------------------------------------------------
		void		ClearDisplayed();
		bool IsDisplayed(HELP_OUTPUT ho) const
		{
			const int index = static_cast<int>(ho);
			return index >= 0 && index < m_Size
				&& static_cast<std::size_t>(index) < m_Displayed.size() && m_Displayed[index];
		}

		//-------------------------------------------------------
		// Reference
		//-------------------------------------------------------		
		const MString&	operator [] (int type);
		const MString&	Get(int type);

		//-------------------------------------------------------
		// File I/O
		//-------------------------------------------------------
		//void			SaveToFile(std::ofstream& file);
		// Publish text and cleared display history only after a complete read.
		void			LoadFromFile(std::ifstream& file);

	private :
		void Swap(MHelpStringTable& other) noexcept;
		std::vector<bool> m_Displayed;
};

extern MHelpStringTable*	g_pHelpStringTable;

#endif
