#include <fstream>
#ifndef __MPALETTE_H__
#define __MPALETTE_H__

#ifdef PLATFORM_WINDOWS
	#include <windows.h>
#else
	#include "../basic/Platform.h"
#endif

class MPalette
{
public:
	MPalette();

	// MPalette owns m_pColor and frees it in the destructor, so it needs
	// a copy constructor. The compiler-supplied one copies the pointer,
	// which shares one table between both instances and double frees it.
	MPalette(const MPalette& pal);

	~MPalette();
	
	//--------------------------------------------------------
	// Init/Release
	//--------------------------------------------------------
	void	Release();
	void	Init(BYTE size);
	
	//--------------------------------------------------------
	// Size
	//--------------------------------------------------------
	BYTE		GetSize() const { return m_Size; }
	
	//--------------------------------------------------------
	// operator
	//--------------------------------------------------------
	// The index is a full byte while the table holds only m_Size
	// entries, and sprite pixel bytes are used as palette indices
	// directly, so an index past the end is reachable straight from
	// file data. Such an index is answered with a shared spare entry
	// rather than reading past the table.
	WORD&		operator [] (BYTE n)		{ return Entry(n); }
	WORD&		operator [] (BYTE n) const { return Entry(n); }
	void		operator = (const MPalette& pal);
	
	//--------------------------------------------------------
	// file I/O
	//--------------------------------------------------------
	virtual bool	SaveToFile(std::ofstream& file) { (void)file; return false; };
	virtual bool	LoadFromFile(std::ifstream& file) { (void)file; return false; };		

	bool IsInit() const { return (m_Size == 0)?false:true; }
	
protected:
	//--------------------------------------------------------
	// Bounds checked table access shared by both operator[]
	// overloads. Returns a spare entry when the palette has no table
	// or the index is outside it.
	//--------------------------------------------------------
	WORD&		Entry(BYTE n) const
	{
		if (m_pColor == NULL || n >= m_Size)
			return m_OutOfRange;

		return m_pColor[n];
	}

	WORD *		m_pColor;
	BYTE		m_Size;

	// Spare entry handed back for an out of range index. Held per
	// palette rather than in a shared static: operator[] returns a
	// non-const reference, so a caller writing through a bad index
	// would otherwise change what every other palette in the process
	// reads back for its own bad indices.
	mutable WORD	m_OutOfRange;
};

class MPalette555 : public MPalette
{
public:
	//--------------------------------------------------------
	// file I/O
	//--------------------------------------------------------
	bool LoadFromFile(std::ifstream &file);
	bool SaveToFile(std::ofstream &file);
};

class MPalette565 : public MPalette
{
public:
	//--------------------------------------------------------
	// file I/O
	//--------------------------------------------------------
	bool LoadFromFile(std::ifstream &file);
	bool SaveToFile(std::ofstream &file);
};

#endif