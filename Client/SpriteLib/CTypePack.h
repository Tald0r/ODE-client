#ifndef __CTYPEPACK_H__
#define __CTYPEPACK_H__

#ifdef PLATFORM_WINDOWS
	#include <windows.h>
	#include <fstream>
#else
	#include "../basic/Platform.h"
	#include <fstream>
	#include <cstring>
	#include <cstdio>
#endif
#include "CSpriteSetManager.h"
#include "COrderedList.h"
#ifdef PLATFORM_WINDOWS
	// CDirectDraw include removed - using ColorDraw instead
#else
	// CDirectDraw include removed - using ColorDraw instead
#endif
#include <algorithm>
#include <vector>
#include <memory>
#include "DebugLog.h"
#include "DataPath.h"

using std::ifstream;
using std::ios;
using std::ofstream;

#include "../../basic/ColorDraw.h"
#include <cstdint>

// The file names reaching the loaders below are the game's spelling of a
// path (Data\Image\..., from FileDef.inf and VS_UI_filepath.h). Each open
// goes through Basic::NormalizeDataPath: the identity on Windows, and
// elsewhere the separator folding and case-insensitive component
// resolution of basic/DataPath.h.
namespace CTypePackDetail {
inline bool OpenIndexedEntry(int fileID, LPCTSTR packFilename, LPCTSTR indexFilename,
	std::ifstream& dataFile)
{
	if (fileID < 0 || !packFilename || !indexFilename) return false;
	dataFile.open(Basic::NormalizeDataPath(packFilename), std::ios::binary | std::ios::ate);
	if (!dataFile) return false;
	const auto dataSize = dataFile.tellg();
	if (dataSize <= std::streampos(2)) return false;
	std::ifstream indexFile(Basic::NormalizeDataPath(indexFilename), std::ios::binary);
	std::uint16_t count = 0;
	if (!indexFile.read(reinterpret_cast<char*>(&count), sizeof count) || fileID >= count)
		return false;
	const auto indexOffset = std::streamoff(2) + std::streamoff(fileID) * 4;
	std::int32_t offset = 0;
	if (!indexFile.seekg(indexOffset) ||
		!indexFile.read(reinterpret_cast<char*>(&offset), sizeof offset) ||
		offset < 2 || std::streampos(offset) >= dataSize)
		return false;
	return static_cast<bool>(dataFile.seekg(static_cast<std::streamoff>(offset)));
}
inline bool ReadRunningIndex(const char* filename, std::ifstream& data,
	std::vector<int>& offsets)
{
	if (!filename || !*filename) return false;
	data.open(Basic::NormalizeDataPath(filename), std::ios::binary | std::ios::ate);
	if (!data) return false;
	const auto end = data.tellg();
	std::uint16_t count = 0, dataCount = 0;
	std::ifstream index(Basic::NormalizeDataPath(std::string(filename) + 'i'), std::ios::binary);
	if (!index.read(reinterpret_cast<char*>(&count), 2) ||
		!data.seekg(0) || !data.read(reinterpret_cast<char*>(&dataCount), 2) ||
		count != dataCount) return false;
	offsets.resize(count);
	for (auto& offset : offsets) {
		std::int32_t value = 0;
		if (!index.read(reinterpret_cast<char*>(&value), 4) || value < 2 ||
			std::streampos(value) >= end) return false;
		offset = value;
	}
	return true;
}

template<class Type> bool LoadElement(Type& value, std::ifstream& file,
	const char* source, unsigned id)
{
	const auto offset = file.tellg();
	try {
		if (file && value.LoadFromFile(file) && file) return true;
	} catch (...) {}
	LOG_ERROR("Rejected pack element: source=%s id=%u offset=%lld",
		source, id, static_cast<long long>(static_cast<std::streamoff>(offset)));
	return false;
}
} // namespace CTypePackDetail

template <class Type>
class CTypePack
{
public:
	CTypePack();
	virtual ~CTypePack();
	
	//--------------------------------------------------------
	// Init/Release
	//--------------------------------------------------------
	virtual void	Init(WORD size);
	virtual void	Release();
	
	//--------------------------------------------------------
	// Size
	//--------------------------------------------------------
	DWORD		GetSize() const { return m_Size; }
	
	//--------------------------------------------------------
	// operator
	//--------------------------------------------------------
	Type&		operator [] (WORD n);//		{ return m_pSpritePals[n]; }
	Type&		Get(WORD n);

	//--------------------------------------------------------
	// file I/O
	//--------------------------------------------------------
	virtual bool LoadFromFile(std::ifstream&file);
	virtual bool SaveToFile(std::ofstream&dataFile, std::ofstream&indexFile);
	
	virtual bool LoadFromFileRunning(LPCTSTR lpszFilename);
	virtual bool LoadFromFile(LPCTSTR lpszFilename);
	virtual bool LoadFromFilePart(int first, int last);
	virtual bool LoadFromFilePart(const CSpriteSetManager& SSM);
	virtual bool LoadFromFileData(int dataID, int fileID, LPCTSTR packFilename, LPCTSTR indexFilename);

	virtual bool ReleasePart(int first, int last);
	virtual bool ReleasePart(COrderedList<TYPE_SPRITEID> list);

	virtual bool SaveToFile(LPCTSTR lpszFilename);
	
protected:
	Type *			m_pData;
	WORD			m_Size;
	bool			m_bRunningLoad;

	// runtime loading
	WORD			m_nLoadData;	// Loading 된 CSprite의 개수
	std::ifstream *m_file;
	int*			m_file_index;
	std::vector<unsigned char> m_LoadState; // unread, accepted, rejected
	std::string m_SourceFilename;
};

template <class Type>
CTypePack<Type>::CTypePack()
{
	m_pData = NULL;
	m_Size = 0;
	m_bRunningLoad = false;
	m_nLoadData = 0;
	m_file_index = NULL;
	m_file = NULL;
}

template <class Type>
CTypePack<Type>::~CTypePack()
{
	Release();
}

template <class Type>
void CTypePack<Type>::Release()
{
	m_LoadState.clear();
	m_SourceFilename.clear();
	m_nLoadData = 0;
	m_bRunningLoad = false;
	
	if(m_file != NULL)
	{
		delete m_file;
		m_file = NULL;
	}

	if(m_file_index != NULL)
	{
		delete []m_file_index;
		m_file_index = NULL;
	}

	if(m_pData != NULL)
	{
		delete []m_pData;
		m_pData = NULL;
		m_Size = 0;
	}
}

template <class Type>
void CTypePack<Type>::Init(WORD size)
{
	std::unique_ptr<Type[]> pending(size ? new Type[size] : nullptr);
	Release();
	m_pData = pending.release();
	m_Size = size;
}

template <class Type>
Type &CTypePack<Type>::operator [] (WORD n)
{
	return Get(n);
}

template <class Type>
Type &CTypePack<Type>::Get(WORD n)
{
	// Get returns a reference, so it cannot report a bad index back to
	// the caller. An out of range index, or any index at all before
	// Init() has allocated the array, is answered with a shared empty
	// element instead of indexing past the end of the array or through a
	// null pointer.
	static Type	s_OutOfRange;

	if(m_pData == NULL || n >= m_Size)
		return s_OutOfRange;

	if (m_bRunningLoad && !m_pData[n].IsInit() && m_LoadState[n] != 2)
	{
		m_file->clear();
		m_file->seekg(m_file_index[n]);
		const bool loaded = CTypePackDetail::LoadElement(m_pData[n], *m_file,
			m_SourceFilename.c_str(), n);
		if (m_LoadState[n] == 0) ++m_nLoadData;
		m_LoadState[n] = loaded ? 1 : 2;
		if (m_nLoadData >= m_Size) {
			m_bRunningLoad = false;
			delete m_file;
			m_file = nullptr;
			delete []m_file_index;
			m_file_index = nullptr;
		}
	}
	
	return m_pData[n];
}

template <class Type>
bool CTypePack<Type>::LoadFromFile(LPCTSTR lpszFilename)
{
	if (!lpszFilename) return false;
	std::ifstream file(Basic::NormalizeDataPath(lpszFilename), std::ios::binary);
	const bool loaded = LoadFromFile(file);
	if (!loaded) LOG_ERROR("Rejected pack file: source=%s", lpszFilename);
	return loaded;
}

template <class Type>
bool CTypePack<Type>::SaveToFile(LPCTSTR lpszFilename)
{
	char szIndexFilename[512];
	snprintf(szIndexFilename, sizeof(szIndexFilename), "%si", lpszFilename);

	std::ofstream dataFile(Basic::NormalizeDataPath(lpszFilename), std::ios::binary);
	std::ofstream indexFile(Basic::NormalizeDataPath(szIndexFilename), std::ios::binary);

	bool re = SaveToFile(dataFile, indexFile);

	dataFile.close();
	indexFile.close();

	return re;
}


template <class Type>
bool CTypePack<Type>::LoadFromFile(std::ifstream&file)
{
	WORD count = 0;
	if (!file.read(reinterpret_cast<char*>(&count), 2)) {
		LOG_ERROR("Rejected pack header from stream");
		return false;
	}
	Init(count);
	bool accepted = true;
	for (unsigned i = 0; i < m_Size; ++i) {
		if (!CTypePackDetail::LoadElement(m_pData[i], file, "<stream>", i)) accepted = false;
		if (!file) break;
	}
	return accepted;
}

//----------------------------------------------------------------------
// Load From File Running
//----------------------------------------------------------------------
// 실시간 로딩
//----------------------------------------------------------------------
template <class Type>
bool CTypePack<Type>::LoadFromFileRunning(LPCTSTR lpszFilename)
{
	try {
		auto file = std::make_unique<std::ifstream>();
		std::vector<int> offsets;
		if (!CTypePackDetail::ReadRunningIndex(lpszFilename, *file, offsets)) {
			LOG_ERROR("Rejected pack index: source=%s", lpszFilename ? lpszFilename : "<null>");
			return false;
		}
		auto index = std::make_unique<int[]>(offsets.size());
		std::copy(offsets.begin(), offsets.end(), index.get());
		std::vector<unsigned char> states(offsets.size(), 0);
		std::string source(lpszFilename);
		Init(static_cast<WORD>(offsets.size()));
		if (offsets.empty()) return true;
		m_file = file.release();
		m_file_index = index.release();
		m_LoadState = std::move(states);
		m_SourceFilename = std::move(source);
		m_bRunningLoad = true;
		return true;
	} catch (...) {
		LOG_ERROR("Cannot prepare pack: source=%s", lpszFilename ? lpszFilename : "<null>");
		return false;
	}
}

template <class Type>
bool CTypePack<Type>::SaveToFile(std::ofstream&dataFile, std::ofstream&indexFile)
{
	//--------------------------------------------------
	// index file을 생성하기 위한 정보
	//--------------------------------------------------
//	long*	pIndex = new long [m_Size];
	std::vector<DWORD> vIndex;

	//--------------------------------------------------
	// Size 저장
	//--------------------------------------------------
	dataFile.write((const char *)&m_Size, 2);
	indexFile.write((const char *)&m_Size, 2);
	WORD realSize = m_Size;
	DWORD index = 0;
	int i;  // Declare at function scope for both loops

	for(i = 0; i < m_Size; i++)
	{
		index = dataFile.tellp();
		if(m_pData[i].SaveToFile(dataFile) == false)
		{
			realSize--;
		}
		else
		{
			vIndex.push_back(index);
		}
	}
	
	if(realSize != m_Size)
	{
		char szTemp[512];
		snprintf(szTemp, sizeof(szTemp), "real size : %d size : %d", realSize, m_Size);
		MessageBox(NULL, szTemp, "CTypePack", MB_OK);

		dataFile.seekp(0);
		dataFile.write((const char *)&realSize, 2);
		indexFile.seekp(0);
		indexFile.write((const char *)&realSize, 2);
	}

	//--------------------------------------------------
	// index 저장
	//--------------------------------------------------
	for (i=0; static_cast<size_t>(i)<vIndex.size(); i++)
	{
		indexFile.write((const char*)&vIndex[i], 4);
	}
	
//	delete [] pIndex;
	
//	indexFile.close();
//	dataFile.close();

	if(m_bRunningLoad)
	{
		m_bRunningLoad = false;
		m_file->close();
		delete m_file;
		m_file = NULL;
		delete []m_file_index;
		m_file_index = NULL;
	}

	return true;
}

template <class Type>
bool CTypePack<Type>::LoadFromFilePart(int first, int last)
{
	if (first == 0xFFFF && last == 0xFFFF) return true;
	if (!m_pData || first < 0 || last < first || last >= m_Size) return false;
	bool accepted = true;
	for (int id = first; id <= last; ++id) {
		Get(static_cast<WORD>(id));
		// Successful empty records need not report IsInit(). The lazy read's
		// result remains authoritative after its stream has been closed.
		if (!m_LoadState.empty() && m_LoadState[id] == 2) accepted = false;
	}
	return accepted;
}

template <class Type>
bool CTypePack<Type>::LoadFromFilePart(const CSpriteSetManager& SSM)
{
	bool accepted = true;
	for (auto id = SSM.GetIterator(); id != SSM.GetEndIterator(); ++id) {
		if (*id == 0xFFFF) continue;
		if (*id >= m_Size || !m_pData) { accepted = false; continue; }
		Get(*id);
		if (!m_LoadState.empty() && m_LoadState[*id] == 2) accepted = false;
	}
	return accepted;
}

template <class Type>
bool CTypePack<Type>::ReleasePart(int first, int last)
{
	if(m_pData == NULL)
		return false;

	// Release() is called through m_pData[i], so the range has to be
	// clamped to the pack itself. Capping last at 0xFFFE only bounded it
	// by the index type, which let the loop write through elements past
	// the end of the allocation.
	if(first < 0)
		first = 0;

	if(last >= (int)m_Size)
		last = (int)m_Size - 1;

	for(int i = first; i <= last; i++)
		m_pData[i].Release();

	return true;
}

template <class Type>
bool CTypePack<Type>::ReleasePart(COrderedList<TYPE_SPRITEID> list)
{
	if(m_pData == NULL)
		return false;

	COrderedList<TYPE_SPRITEID>::DATA_LIST::const_iterator iID = list.GetIterator();
	for (int t=0; t<list.GetSize(); t++)
	{
		// An entry naming an element outside the pack is skipped rather
		// than written through.
		if(*iID != 0xFFFF && *iID < m_Size)
			m_pData[*iID].Release();

		// The iterator has to advance. Without this the loop released
		// whatever the first entry named once per pass and ignored every
		// other element in the list.
		++iID;
	}

	return true;
}

template <class Type>
bool CTypePack<Type>::LoadFromFileData(int dataID, int fileID, LPCTSTR packFilename, LPCTSTR indexFilename)
{
	if (!m_pData || dataID < 0 || dataID >= m_Size) return false;
	std::ifstream dataFile;
	if (!CTypePackDetail::OpenIndexedEntry(fileID, packFilename, indexFilename, dataFile)) {
		LOG_ERROR("Rejected indexed pack read: source=%s id=%d",
			packFilename ? packFilename : "<null>", fileID);
		return false;
	}
	return CTypePackDetail::LoadElement(m_pData[dataID], dataFile, packFilename, static_cast<unsigned>(fileID));
}

// CTypePack2
template <class TypeBase, class Type1, class Type2>
class CTypePack2
{
private:
	// Disable copy constructor and copy assignment to prevent issues with m_file pointer
	CTypePack2(const CTypePack2&) = delete;
	CTypePack2& operator=(const CTypePack2&) = delete;

	// Disable move constructor and move assignment to prevent m_file pointer from being moved
	CTypePack2(CTypePack2&&) = delete;
	CTypePack2& operator=(CTypePack2&&) = delete;

public:
	CTypePack2();
	virtual ~CTypePack2();
	
	//--------------------------------------------------------
	// Init/Release
	//--------------------------------------------------------
	virtual void	Init(WORD size );
	virtual void	Release();
	
	//--------------------------------------------------------
	// Size
	//--------------------------------------------------------
	DWORD		GetSize() const { return m_Size; }
	
	//--------------------------------------------------------
	// operator
	//--------------------------------------------------------
	TypeBase&		operator [] (WORD n);//		{ return m_pSpritePals[n]; }
	TypeBase&		Get(WORD n);

	//--------------------------------------------------------
	// file I/O
	//--------------------------------------------------------
	virtual bool LoadFromFile(std::ifstream&file);
	virtual bool SaveToFile(std::ofstream&dataFile, std::ofstream&indexFile);
	
	virtual bool LoadFromFileRunning(LPCTSTR lpszFilename);
	virtual bool LoadFromFile(LPCTSTR lpszFilename);
	virtual bool LoadFromFilePart(int first, int last);
	virtual bool LoadFromFilePart(const CSpriteSetManager& SSM);
	virtual bool LoadFromFileData(int dataID, int fileID, LPCTSTR packFilename, LPCTSTR indexFilename);

	virtual bool ReleasePart(int first, int last);
	virtual bool ReleasePart(COrderedList<TYPE_SPRITEID> list);

	virtual bool SaveToFile(LPCTSTR lpszFilename);

	virtual bool Is565() { return m_bSecond; }
	
protected:
	TypeBase *			m_pData;
	WORD			m_Size;
	bool			m_bRunningLoad;

	// runtime loading
	WORD			m_nLoadData;	// Loading 된 CSprite의 개수
	std::ifstream *m_file;
	int*			m_file_index;
	std::vector<unsigned char> m_LoadState; // unread, accepted, rejected
	std::string m_SourceFilename;
	bool			m_bSecond;
};

template <class TypeBase, class Type1, class Type2>
CTypePack2<TypeBase, Type1, Type2>::CTypePack2()
{
	m_pData = NULL;
	m_Size = 0;
	m_bRunningLoad = false;
	m_nLoadData = 0;
	m_file_index = NULL;
	m_file = NULL;

	// Read by Release() to choose which concrete type to delete[] and
	// by Get() to choose the matching spare element, so it cannot be
	// left holding whatever was on the stack.
	m_bSecond = false;
}

template <class TypeBase, class Type1, class Type2>
CTypePack2<TypeBase, Type1, Type2>::~CTypePack2()
{
	Release();
}

template <class TypeBase, class Type1, class Type2>
void CTypePack2<TypeBase, Type1, Type2>::Release()
{
	m_LoadState.clear();
	m_SourceFilename.clear();
	m_nLoadData = 0;
//	printf("DEBUG Release: this=%p, m_file=%p, m_bRunningLoad=%d\n", this, m_file, m_bRunningLoad);
	m_bRunningLoad = false;

	if(m_file != NULL)
	{
//		printf("DEBUG Release: this=%p, deleting m_file=%p\n", this, m_file);
		delete m_file;
		m_file = NULL;
	}

	if(m_file_index != NULL)
	{
		delete []m_file_index;
		m_file_index = NULL;
	}

	if(m_pData != NULL)
	{
		// IMPORTANT: Delete with correct type to match new Type1[size] or new Type2[size]
		// Using base class pointer to delete derived class array is UB even with virtual destructor
		if(m_bSecond)
			delete [] ((Type2*)m_pData);
		else
			delete [] ((Type1*)m_pData);
		m_pData = NULL;
		m_Size = 0;
	}
}

template <class TypeBase, class Type1, class Type2>
void CTypePack2<TypeBase, Type1, Type2>::Init(WORD size)
{
	const bool second = ColorDraw::Is565();
	std::unique_ptr<Type1[]> firstData;
	std::unique_ptr<Type2[]> secondData;
	if (size) {
		if (second) secondData = std::make_unique<Type2[]>(size);
		else firstData = std::make_unique<Type1[]>(size);
	}
	Release();
	m_bSecond = second;
	m_pData = second ? static_cast<TypeBase*>(secondData.release()) :
		static_cast<TypeBase*>(firstData.release());
	m_Size = size;
}

template <class TypeBase, class Type1, class Type2>
TypeBase &CTypePack2<TypeBase, Type1, Type2>::operator [] (WORD n)
{
	return Get(n);
}

template <class TypeBase, class Type1, class Type2>
TypeBase &CTypePack2<TypeBase, Type1, Type2>::Get(WORD n)
{
	// Get returns a reference, so it cannot report a bad index back to
	// the caller. An out of range index, or any index at all before
	// Init() has allocated the array, is answered with a shared empty
	// element instead of indexing past the end of the array or through a
	// null pointer.
	//
	// This has to happen before anything touches m_pData: the range
	// check that used to live further down ran after the element had
	// already been read once to test IsInit(), and only covered the
	// running-load path.
	//
	// One spare of each concrete type, picked by the same flag Init()
	// used to choose what to allocate. Handing back a Type1 while the
	// pack holds Type2 would give the caller an element of a different
	// type from every other element in the pack.
	static Type1	s_OutOfRangeFirst;
	static Type2	s_OutOfRangeSecond;

	if(m_pData == NULL || n >= m_Size)
	{
		if(m_bSecond)
			return s_OutOfRangeSecond;

		return s_OutOfRangeFirst;
	}

	if (m_bRunningLoad && !m_pData[n].IsInit() && m_LoadState[n] != 2)
	{
		m_file->clear();
		m_file->seekg(m_file_index[n]);
		const bool loaded = CTypePackDetail::LoadElement(m_pData[n], *m_file,
			m_SourceFilename.c_str(), n);
		if (m_LoadState[n] == 0) ++m_nLoadData;
		m_LoadState[n] = loaded ? 1 : 2;
		if (m_nLoadData >= m_Size) {
			m_bRunningLoad = false;
			delete m_file;
			m_file = nullptr;
			delete []m_file_index;
			m_file_index = nullptr;
		}
	}

	return m_pData[n];
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::LoadFromFile(LPCTSTR lpszFilename)
{
	if (!lpszFilename) return false;
	std::ifstream file(Basic::NormalizeDataPath(lpszFilename), std::ios::binary);
	const bool loaded = LoadFromFile(file);
	if (!loaded) LOG_ERROR("Rejected pack file: source=%s", lpszFilename);
	return loaded;
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::SaveToFile(LPCTSTR lpszFilename)
{
	char szIndexFilename[512];
	snprintf(szIndexFilename, sizeof(szIndexFilename), "%si", lpszFilename);

	std::ofstream dataFile(Basic::NormalizeDataPath(lpszFilename), std::ios::binary);
	std::ofstream indexFile(Basic::NormalizeDataPath(szIndexFilename), std::ios::binary);

	bool re = SaveToFile(dataFile, indexFile);

	dataFile.close();
	indexFile.close();

	return re;
}


template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::LoadFromFile(std::ifstream&file)
{
	WORD count = 0;
	if (!file.read(reinterpret_cast<char*>(&count), 2)) {
		LOG_ERROR("Rejected pack header from stream");
		return false;
	}
	Init(count);
	bool accepted = true;
	for (unsigned i = 0; i < m_Size; ++i) {
		if (!CTypePackDetail::LoadElement(m_pData[i], file, "<stream>", i)) accepted = false;
		if (!file) break;
	}
	return accepted;
}

//----------------------------------------------------------------------
// Load From File Running
//----------------------------------------------------------------------
// 실시간 로딩
//----------------------------------------------------------------------
template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::LoadFromFileRunning(LPCTSTR lpszFilename)
{
	try {
		auto file = std::make_unique<std::ifstream>();
		std::vector<int> offsets;
		if (!CTypePackDetail::ReadRunningIndex(lpszFilename, *file, offsets)) {
			LOG_ERROR("Rejected pack index: source=%s", lpszFilename ? lpszFilename : "<null>");
			return false;
		}
		auto index = std::make_unique<int[]>(offsets.size());
		std::copy(offsets.begin(), offsets.end(), index.get());
		std::vector<unsigned char> states(offsets.size(), 0);
		std::string source(lpszFilename);
		Init(static_cast<WORD>(offsets.size()));
		if (offsets.empty()) return true;
		m_file = file.release();
		m_file_index = index.release();
		m_LoadState = std::move(states);
		m_SourceFilename = std::move(source);
		m_bRunningLoad = true;
		return true;
	} catch (...) {
		LOG_ERROR("Cannot prepare pack: source=%s", lpszFilename ? lpszFilename : "<null>");
		return false;
	}
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::SaveToFile(std::ofstream&dataFile, std::ofstream&indexFile)
{
	//--------------------------------------------------
	// index file을 생성하기 위한 정보
	//--------------------------------------------------
//	long*	pIndex = new long [m_Size];
	std::vector<DWORD> vIndex;

	//--------------------------------------------------
	// Size 저장
	//--------------------------------------------------
	dataFile.write((const char *)&m_Size, 2);
	indexFile.write((const char *)&m_Size, 2);
	WORD realSize = m_Size;
	DWORD index = 0;
	int i;  // Declare at function scope for both loops

	for(i = 0; i < m_Size; i++)
	{
		index = dataFile.tellp();
		if(m_pData[i].SaveToFile(dataFile) == false)
		{
			realSize--;
		}
		else
		{
			vIndex.push_back(index);
		}
	}
	
	if(realSize != m_Size)
	{
		char szTemp[512];
		snprintf(szTemp, sizeof(szTemp), "real size : %d size : %d", realSize, m_Size);
		MessageBox(NULL, szTemp, "CTypePack2", MB_OK);

		dataFile.seekp(0);
		dataFile.write((const char *)&realSize, 2);
		indexFile.seekp(0);
		indexFile.write((const char *)&realSize, 2);
	}

	//--------------------------------------------------
	// index 저장
	//--------------------------------------------------
	for (i=0; static_cast<size_t>(i)<vIndex.size(); i++)
	{
		indexFile.write((const char*)&vIndex[i], 4);
	}
	
//	delete [] pIndex;
	
//	indexFile.close();
//	dataFile.close();

	if(m_bRunningLoad)
	{
		m_bRunningLoad = false;
		m_file->close();
		delete m_file;
		m_file = NULL;
		delete []m_file_index;
		m_file_index = NULL;
	}

	return true;
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::LoadFromFilePart(int first, int last)
{
	if (first == 0xFFFF && last == 0xFFFF) return true;
	if (!m_pData || first < 0 || last < first || last >= m_Size) return false;
	bool accepted = true;
	for (int id = first; id <= last; ++id) {
		Get(static_cast<WORD>(id));
		if (!m_LoadState.empty() && m_LoadState[id] == 2) accepted = false;
	}
	return accepted;
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::LoadFromFilePart(const CSpriteSetManager& SSM)
{
	bool accepted = true;
	for (auto id = SSM.GetIterator(); id != SSM.GetEndIterator(); ++id) {
		if (*id == 0xFFFF) continue;
		if (*id >= m_Size || !m_pData) { accepted = false; continue; }
		Get(*id);
		if (!m_LoadState.empty() && m_LoadState[*id] == 2) accepted = false;
	}
	return accepted;
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::ReleasePart(int first, int last)
{
	if(m_pData == NULL)
		return false;

	// Release() is called through m_pData[i], so the range has to be
	// clamped to the pack itself. Capping last at 0xFFFE only bounded it
	// by the index type, which let the loop write through elements past
	// the end of the allocation. This is the overload the sprite packs
	// actually instantiate, so it matters more than the CTypePack one.
	if(first < 0)
		first = 0;

	if(last >= (int)m_Size)
		last = (int)m_Size - 1;

	for(int i = first; i <= last; i++)
		m_pData[i].Release();

	return true;
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::ReleasePart(COrderedList<TYPE_SPRITEID> list)
{
	if(m_pData == NULL)
		return false;

	COrderedList<TYPE_SPRITEID>::DATA_LIST::const_iterator iID = list.GetIterator();
	for (int t=0; t<list.GetSize(); t++)
	{
		// An entry naming an element outside the pack is skipped rather
		// than written through.
		if(*iID != 0xFFFF && *iID < m_Size)
			m_pData[*iID].Release();

		// The iterator has to advance. Without this the loop released
		// whatever the first entry named once per pass and ignored every
		// other element in the list.
		++iID;
	}

	return true;
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::LoadFromFileData(int dataID, int fileID, LPCTSTR packFilename, LPCTSTR indexFilename)
{
	if (!m_pData || dataID < 0 || dataID >= m_Size) return false;
	std::ifstream dataFile;
	if (!CTypePackDetail::OpenIndexedEntry(fileID, packFilename, indexFilename, dataFile)) {
		LOG_ERROR("Rejected indexed pack read: source=%s id=%d",
			packFilename ? packFilename : "<null>", fileID);
		return false;
	}
	return CTypePackDetail::LoadElement(m_pData[dataID], dataFile, packFilename, static_cast<unsigned>(fileID));
}

#endif
