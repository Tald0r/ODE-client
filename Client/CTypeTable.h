//----------------------------------------------------------------------
// CTypeTable.h
//----------------------------------------------------------------------
//행행 326220963
#ifndef	__CTYPETABLE_H__
#define	__CTYPETABLE_H__

//#include "DebugInfo.h"
//#define	 new DEBUG_NEW
#include <fstream>
#include <cstdint>

//----------------------------------------------------------------------
//
// Info에 대한 정보 Table
//
//----------------------------------------------------------------------
template <class Type>
class CTypeTable {
	public :
		CTypeTable();
		~CTypeTable();

		//-------------------------------------------------------
		// Init / Release
		//-------------------------------------------------------
		void			Init(int size);
		void			Release();

		//-------------------------------------------------------
		// Size
		//-------------------------------------------------------
		int				GetSize() const		{ return m_Size; }

		//-------------------------------------------------------
		// Debug/Internal access
		//-------------------------------------------------------
		Type*			GetInternalPointer() { return m_pTypeInfo; }
		const Type*		GetInternalPointer() const { return m_pTypeInfo; }
		
		//-------------------------------------------------------
		// Reference
		//
		// m_Size is whatever the data file the table was loaded
		// from declared, while the ids indexing it come from
		// packets and from compile-time enums a shorter file does
		// not cover, so the range test has to hold in every
		// configuration. It used to be #ifdef _DEBUG - which MSVC
		// defines for Debug and not for Release - so the only
		// build without the check was the one that ships.
		//
		// Reads yield an immutable default out of range. Mutation
		// uses GetMutable/Set and cannot write into a shared fallback.
		// A default MString holds a NULL string, so copying out of
		// a lookup still requires the caller to check that string.
		//-------------------------------------------------------
		const Type&	operator [] (int type) const {
			if (m_pTypeInfo == NULL || type < 0 || type >= m_Size) {
				static const Type dummy{};
				return dummy;
			}
			return m_pTypeInfo[type];
		}
		const Type&	Get(int type) const {
			return (*this)[type];
		}
		Type*	GetMutable(int type) {
			if (m_pTypeInfo == NULL || type < 0 || type >= m_Size)
				return nullptr;
			return &m_pTypeInfo[type];
		}
		template <class Value>
		bool	Set(int type, const Value& value) {
			Type* entry = GetMutable(type);
			if (entry == nullptr)
				return false;
			*entry = value;
			return true;
		}


		//-------------------------------------------------------
		// File I/O
		//-------------------------------------------------------
		void			SaveToFile(std::ofstream& file);
		void			LoadFromFile(std::ifstream& file);
		void			SaveToFile(const char *filename);
		void			LoadFromFile(const char *filename);
		bool			LoadFromFile_NickNameString(std::ifstream& file);
	protected :
		//-------------------------------------------------------
		// Is an entry count read out of a file plausible?
		//
		// Every entry costs at least one byte on disk, so a count
		// larger than what is left of the file cannot describe
		// real entries however small Type is. Nickname records also
		// carry an index. Reject failed or unmeasurable input before
		// allocation, including small counts and incomplete prefixes.
		// This is a lower bound, not validation of each entry's body.
		//-------------------------------------------------------
		static bool	IsEntryCountSane(std::ifstream& file, int count,
			std::streamoff minimumBytesPerEntry = 1)
		{
			if (!file.good() || count < 0 || minimumBytesPerEntry <= 0)
				return false;

			std::streamoff	cur = file.tellg();

			if (cur < 0)
				return false;

			file.seekg(0, std::ios::end);

			std::streamoff	end = file.tellg();

			if (!file.good() || end < cur)
				return false;

			// Preserve the next record's position without clearing an I/O failure.
			file.seekg(cur, std::ios::beg);
			return file.good() && count <= (end - cur) / minimumBytesPerEntry;
		}

		int			m_Size;					// number of Types held
		Type*		m_pTypeInfo;			// the Type information

};


//----------------------------------------------------------------------
//
//    constructor/destructor
//
//----------------------------------------------------------------------
template <class Type>
CTypeTable<Type>::CTypeTable()
{
	m_pTypeInfo	= NULL;
	m_Size		= 0;
}

template <class Type>
CTypeTable<Type>::~CTypeTable()
{
	Release();
}

//----------------------------------------------------------------------
//
//  member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Init
//----------------------------------------------------------------------
template <class Type>
void
CTypeTable<Type>::Init(int size)
{
	// nothing to hold; a negative count would make new Type[] undefined
	if (size<=0)
		return;

	// release what is held first
	Release();

	// m_Size is only published once the array exists, so a throwing
	// allocation cannot leave a size behind with no table under it
	Type*	pTypeInfo = new Type [size]();

	m_pTypeInfo	= pTypeInfo;
	m_Size		= size;
}


//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
template <class Type>
void
CTypeTable<Type>::Release()
{
	if (m_pTypeInfo != NULL)
	{
		// 모든 CSprite를 지운다.
		delete [] m_pTypeInfo;
		m_pTypeInfo = NULL;
		
		m_Size = 0;
	}
}

//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
template <class Type>
void			
CTypeTable<Type>::SaveToFile(std::ofstream& file)
{
	// size 저장
	file.write((const char*)&m_Size, 4);

	// 아무 것도 없는 경우
	if (m_pTypeInfo==NULL)
		return;

	// 각각의 정보 저장
	for (int i=0; i<m_Size; i++)
	{
		m_pTypeInfo[i].SaveToFile(file);
	}
}

//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
template <class Type>
void			
CTypeTable<Type>::LoadFromFile(std::ifstream& file)
{
	int numSize=0;

	// read the size
	file.read((char*)&numSize, 4);

	// the count is whatever the file says, and Init allocates from it
	if (!IsEntryCountSane(file, numSize))
	{
		file.setstate(std::ios::failbit);
		return;
	}

	// reallocate when the size differs from what is currently held
	if (m_Size != numSize)
	{
		// release the memory
		Release();

		// allocate
		Init( numSize );
	}

	// read each entry from the file
	for (int i=0; i<m_Size && file.good(); i++)
	{
 		m_pTypeInfo[i].LoadFromFile( file );
	}
}

template <class Type>
void
CTypeTable<Type>::LoadFromFile(const char* lpszFilename)
{
	std::ifstream file(lpszFilename, std::ios::binary );
	if(file.is_open())
	{
		LoadFromFile(file);
		file.close();
	}
}

template <class Type>
void
CTypeTable<Type>::SaveToFile(const char* lpszFilename)
{
	std::ofstream file(lpszFilename, std::ios::binary);
	SaveToFile(file);
	file.close();
}
// 2004, 6, 18 sobeit add start - nick name - 파일 구조가 쩜 틀려서 전용으로 만듬..^^:
//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
template <class Type>
bool			
CTypeTable<Type>::LoadFromFile_NickNameString(std::ifstream& file)
{
	int numSize=0;
	// read the size
	file.read((char*)&numSize, 4);

	// the count is whatever the file says, and Init allocates from it
	if (!IsEntryCountSane(file, numSize, sizeof(std::uint16_t) + 1))
	{
		file.setstate(std::ios::failbit);
		return false;
	}

	// reallocate when the size differs from what is currently held
	if (m_Size != numSize)
	{
		// release the memory
		Release();

		// allocate
		Init( numSize );
	}


	for (int i=0; i<m_Size; i++)
	{
		std::uint16_t wIndex = 0;
		if (!file.read((char*)&wIndex, sizeof(wIndex)))
			return false;
		if (wIndex >= m_Size)
		{
			file.setstate(std::ios::failbit);
			return false;
		}
 		m_pTypeInfo[wIndex].LoadFromFile( file );
		if (!file.good())
			return false;
	}
	return true;
}
// 2004, 6, 18 sobeit add start - nick name - 파일 구조가 쩜 틀려서 전용으로 만듬..^^:
#endif