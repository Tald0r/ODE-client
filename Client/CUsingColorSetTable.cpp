//----------------------------------------------------------------------
// CUsingColorSetTable.cpp
//----------------------------------------------------------------------

#include "Client_PCH.h"
#include "CUsingColorSetTable.h"

//----------------------------------------------------------------------
// global
//----------------------------------------------------------------------
USINGCOLORSET_TABLE*	g_pUsingColorSetTable = NULL;

//----------------------------------------------------------------------
//
// constructor / destructor
//
//----------------------------------------------------------------------
USINGCOLORSET_NODE::USINGCOLORSET_NODE()
{
	// mark them all unusable.
	for (int i=0; i<MAX_COLORSET; i++)
	{
		m_bEnable[i] = false;
	}
}

USINGCOLORSET_NODE::~USINGCOLORSET_NODE()
{
}


//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Enable (set)
//----------------------------------------------------------------------
void			
USINGCOLORSET_NODE::Enable(int set)
{
	if (set>=0 && set<MAX_COLORSET)
	{
		m_bEnable[set] = true;
	}
}

//----------------------------------------------------------------------
// Disable (set)
//----------------------------------------------------------------------
void			
USINGCOLORSET_NODE::Disable(int set)
{
	if (set>=0 && set<MAX_COLORSET)
	{
		m_bEnable[set] = false;
	}
}

//----------------------------------------------------------------------
// Enable ( start ~ end )
//----------------------------------------------------------------------
// enable a whole range.
//----------------------------------------------------------------------
void			
USINGCOLORSET_NODE::Enable(int start, int end)
{
	if (start>=0 && start<MAX_COLORSET
		&& end>=0 && end<MAX_COLORSET
		&& start < end)
	{
		for (int i=start; i<=end; i++)
		{
			m_bEnable[i] = true;
		}
	}
}

//----------------------------------------------------------------------
// Disable ( start ~ end )
//----------------------------------------------------------------------
// disable a whole range.
//----------------------------------------------------------------------
void			
USINGCOLORSET_NODE::Disable(int start, int end)
{
	if (start>=0 && start<MAX_COLORSET
		&& end>=0 && end<MAX_COLORSET
		&& start < end)
	{
		for (int i=start; i<=end; i++)
		{
			m_bEnable[i] = false;
		}
	}
}


//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
void			
USINGCOLORSET_NODE::SaveToFile(ofstream& file)
{
	// save the count
	int size = MAX_COLORSET;
	file.write((const char*)&size, 4);

	// save the enable flags
	for (int i=0; i<MAX_COLORSET; i++)
	{
		file.write((const char*)&m_bEnable[i], 1);
	}
}

//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
bool
USINGCOLORSET_NODE::LoadFromFile(ifstream& file)
{
	// read the count
	int size;
	file.read((char*)&size, 4);

	// a count other than the current colorsets'..
	// still read them from the file.
	if (MAX_COLORSET != size)
	{
		bool temp;
		for (int i=0; i<size; i++)
		{
			file.read((char*)&temp, 1);
		}

		return false;
	}

	// the right count.. read them all.
	for (int i=0; i<size; i++)
	{
		file.read((char*)&m_bEnable[i], 1);
	}

	return true;
}