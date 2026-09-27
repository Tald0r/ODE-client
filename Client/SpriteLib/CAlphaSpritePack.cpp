//----------------------------------------------------------------------
// CAlphaSpritePack.cpp
//----------------------------------------------------------------------

#include "CSpriteSurface.h"
#include "CAlphaSprite555.h"
#include "CAlphaSprite565.h"
#include "CAlphaSpritePack.h"
#include "CTypePack.h"
#include "DataPath.h"
#include <stdexcept>
#include <utility>
#include <fstream>
#include <cstdint>

//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------

CAlphaSpritePack::CAlphaSpritePack()
{
	m_nSprites = 0;
}

CAlphaSpritePack::~CAlphaSpritePack()
{
	// array를 메모리에서 제거한다.
	Release();
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Init
//----------------------------------------------------------------------
void
CAlphaSpritePack::Init(TYPE_SPRITEID count, bool b565)
{
	CAlphaSpritePack pending;
	pending.m_rgb565 = b565;
	if (count) {
		if (b565) pending.m_sprites565 = std::make_unique<CAlphaSprite565[]>(count);
		else pending.m_sprites555 = std::make_unique<CAlphaSprite555[]>(count);
	}
	pending.m_nSprites = count;
	Swap(pending);
}

void CAlphaSpritePack::Swap(CAlphaSpritePack& other) noexcept
{
	std::swap(m_nSprites, other.m_nSprites);
	std::swap(m_rgb565, other.m_rgb565);
	m_sprites565.swap(other.m_sprites565);
	m_sprites555.swap(other.m_sprites555);
}

CAlphaSprite& CAlphaSpritePack::operator[](TYPE_SPRITEID n)
{
	if (n >= m_nSprites) throw std::out_of_range("alpha sprite pack index");
	// Both array indexing and destruction use the allocated concrete type.
	if (m_rgb565) return m_sprites565[n];
	return m_sprites555[n];
}


//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
CAlphaSpritePack::Release()
{
	m_sprites565.reset();
	m_sprites555.reset();
	m_nSprites = 0;
}

//----------------------------------------------------------------------
// Release Part
//----------------------------------------------------------------------
// firstSpriteID ~ lastSpriteID까지만 메모리에서 제거한다.
//----------------------------------------------------------------------
void			
CAlphaSpritePack::ReleasePart(TYPE_SPRITEID firstSpriteID, TYPE_SPRITEID lastSpriteID)
{
	// SpritePack의 memory가 잡혀있지 않으면 그냥 return한다.	
	if (firstSpriteID >= m_nSprites)
		return;

	int last = min(lastSpriteID, m_nSprites-1);

	for (int id=firstSpriteID; id<=last; id++)
	{
		(*this)[static_cast<TYPE_SPRITEID>(id)].Release();
	}
}

//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
// map 전체를 따라가면서 file에 저장해야한다.
//----------------------------------------------------------------------
bool		
CAlphaSpritePack::SaveToFile(ofstream& spkFile, ofstream& indexFile)
{
	// 초기화되지 않았으면 
	if (m_nSprites==0)
		return false;
	
	//--------------------------------------------------
	// index file을 생성하기 위한 정보
	//--------------------------------------------------
	long*	pIndex = new long [m_nSprites];


	//--------------------------------------------------
	// Size 저장
	//--------------------------------------------------
	spkFile.write((const char *)&m_nSprites, SIZE_SPRITEID); 
	indexFile.write((const char *)&m_nSprites, SIZE_SPRITEID); 

	//--------------------------------------------------
	//
	// SpritePack에 Array의 모든 Sprite를 저장한다.
	//
	//--------------------------------------------------
	for (TYPE_SPRITEID i=0; i<m_nSprites; i++)
	{
		// SpritePack file에 쓰여지는 index를 저장
		pIndex[i] = static_cast<long>(spkFile.tellp());

		// The record header also represents an empty sprite.
		// CAlphaSprite 내부적으로 길이만 저장하므로 
		// 다음에 Load할 때 문제가 없을 것이다.

		(*this)[i].SaveToFile(spkFile);
	}

	//--------------------------------------------------
	// index 저장
	//--------------------------------------------------
	for (int i=0; i<m_nSprites; i++)
	{
		indexFile.write((const char*)&pIndex[i], 4);
	}

	delete [] pIndex;

	return true;
}

//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
// map 전체를 따라가면서 file에 저장해야한다.
//----------------------------------------------------------------------
bool
CAlphaSpritePack::SaveToFileSpriteOnly(ofstream& spkFile, int32_t &filePosition)
{
	// 초기화되지 않았으면 
	if (m_nSprites==0)
		return false;

	// SpritePack file에 쓰여지는 index를 저장
	filePosition = static_cast<int32_t>(spkFile.tellp());
	
	//--------------------------------------------------
	//
	// SpritePack에 Array의 모든 Sprite를 저장한다.
	//
	//--------------------------------------------------
	for (TYPE_SPRITEID i=0; i<m_nSprites; i++)
	{
		// The record header also represents an empty sprite.
		// CSprite 내부적으로 길이만 저장하므로 
		// 다음에 Load할 때 문제가 없을 것이다.

		(*this)[i].SaveToFile(spkFile);
	}

	return true;
}

//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
// file에서 ID와 Sprite를 읽어와서 map에 하나씩 저장한다.
//----------------------------------------------------------------------
bool
CAlphaSpritePack::LoadFromFile(ifstream& file)
{
	try {
		TYPE_SPRITEID count = 0;
		if (!file.read(reinterpret_cast<char*>(&count), SIZE_SPRITEID)) return false;
		const auto start = file.tellg();
		if (start == std::streampos(-1) || !file.seekg(0, std::ios::end)) return false;
		const auto end = file.tellg();
		if (end < start || !file.seekg(start) || std::streamoff(count) > (end - start) / 4) return false;
		CAlphaSpritePack pending;
		pending.Init(count, ColorDraw::Is565());
		bool accepted = true;
		for (unsigned i = 0; i < count; ++i) {
			if (!CTypePackDetail::LoadElement(pending[static_cast<TYPE_SPRITEID>(i)],
				file, "<alpha pack>", i)) accepted = false;
			if (!file) break;
		}
		if (!accepted) return false;
		Swap(pending);
		return true;
	} catch (...) {
		LOG_ERROR("Cannot read alpha sprite pack");
		return false;
	}
}


//----------------------------------------------------------------------
// Load From File Part
//----------------------------------------------------------------------
// file에서 일부의 Sprite들만 읽어들인다.
// 
// file의 filePosition에서부터 읽어들이고..
// FirstSpriteID부터 SpriteSize개만큼만 읽어들인다.
//----------------------------------------------------------------------
bool
CAlphaSpritePack::LoadFromFilePart(ifstream& file, int32_t filePosition,
							  TYPE_SPRITEID firstSpriteID, TYPE_SPRITEID lastSpriteID)
{
	if (firstSpriteID > lastSpriteID || lastSpriteID >= m_nSprites || filePosition < 0) return false;
	try {
		if (!file || !file.seekg(0, std::ios::end)) return false;
		const auto end = file.tellg();
		if (std::streampos(filePosition) >= end || !file.seekg(filePosition)) return false;
		bool accepted = true;
		for (unsigned id = firstSpriteID; id <= lastSpriteID; ++id) {
			if (!CTypePackDetail::LoadElement((*this)[static_cast<TYPE_SPRITEID>(id)],
				file, "<alpha pack range>", id)) accepted = false;
			if (!file) break;
		}
		return accepted;
	} catch (...) {
		LOG_ERROR("Cannot read alpha sprite pack range");
		return false;
	}
}

//----------------------------------------------------------------------
// LoadFromFile Sprite
//----------------------------------------------------------------------
// indexFile을 이용해서 spkFile에서 spriteID번째 sprite를 읽어온다.
//----------------------------------------------------------------------
bool
CAlphaSpritePack::LoadFromFileSprite(int spriteID, int fileSpriteID, std::ifstream& spkFile, std::ifstream& indexFile)
{
	if (spriteID < 0 || spriteID >= m_nSprites || fileSpriteID < 0) return false;
	try {
		if (!spkFile || !indexFile || !spkFile.seekg(0, std::ios::end)) return false;
		const auto end = spkFile.tellg();
		TYPE_SPRITEID count = 0, dataCount = 0;
		if (!indexFile.seekg(0) || !indexFile.read(reinterpret_cast<char*>(&count), SIZE_SPRITEID) ||
			!spkFile.seekg(0) || !spkFile.read(reinterpret_cast<char*>(&dataCount), SIZE_SPRITEID) ||
			count != dataCount || fileSpriteID >= count) return false;
		int32_t offset = 0;
		if (!indexFile.seekg(std::streamoff(2) + std::streamoff(fileSpriteID) * 4) ||
			!indexFile.read(reinterpret_cast<char*>(&offset), 4) ||
			offset < 2 || std::streampos(offset) >= end || !spkFile.seekg(offset)) return false;
		return CTypePackDetail::LoadElement((*this)[static_cast<TYPE_SPRITEID>(spriteID)],
			spkFile, "<indexed alpha pack>", static_cast<unsigned>(fileSpriteID));
	} catch (...) {
		LOG_ERROR("Cannot read indexed alpha sprite");
		return false;
	}
}

//----------------------------------------------------------------------
// LoadFromFile Sprite
//----------------------------------------------------------------------
// indexFile을 이용해서 spkFile에서 spriteID번째 sprite를 읽어온다.
//----------------------------------------------------------------------
bool
CAlphaSpritePack::LoadFromFileSprite(int spriteID, int fileSpriteID, const char* spkFilename, const char* indexFilename)
{
	if (spriteID < 0 || spriteID >= m_nSprites || fileSpriteID < 0 || !spkFilename || !indexFilename)
	{
		return false;
	}

	// The game's spelling of the path, resolved to the disk's (basic/DataPath.h).
	std::ifstream spkFile(Basic::NormalizeDataPath(spkFilename), ios::binary);

	if (!spkFile.is_open())
	{
		return false;
	}

	std::ifstream indexFile(Basic::NormalizeDataPath(indexFilename), ios::binary);

	if (!indexFile.is_open())
	{
		return false;
	}

	return LoadFromFileSprite( spriteID, fileSpriteID, spkFile, indexFile );
}
