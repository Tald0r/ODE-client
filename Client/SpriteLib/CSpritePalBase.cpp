#include "Client_PCH.h"
#include "CSpritePalBase.h"
#include "CSpriteSurface.h"
#include "SpriteGpu.h"
#include <algorithm>


BYTE CSpritePalBase::s_Colorkey = 0xFF;

CSpritePalBase::CSpritePalBase()
{
	m_Width = 0;		// 가로 pixel수
	m_Height = 0;		// 세로 pixel수		
	m_Size = 0;			// 스프라이트의 size
	
	m_pPixels = NULL;		// pixels point array
	m_pData = NULL;			// data
	
	m_bInit = false;		// data가 있는가?
	m_bLoading = false;		// Loading중인가?
}

CSpritePalBase::~CSpritePalBase()
{
	Release();
}

void CSpritePalBase::Release()
{
	SpriteGpu::ForgetPaletteSprite(this);
	m_Width = 0;		// 가로 pixel수
	m_Height = 0;		// 세로 pixel수		
	m_Size = 0;			// 스프라이트의 size

	if(m_pData != NULL)
	{
		delete [] m_pData;
		m_pData = NULL;			// data
		m_pPixels = NULL;
	}
	
	m_bInit = false;		// is there data?
	m_bLoading = false;		// still loading?
}

void CSpritePalBase::AllocateDataAndScanlineTable(DWORD size, WORD height)
{
	// new BYTE[] is aligned for any object; the table only has to start
	// at a multiple of the pointer alignment within the block.
	const size_t	tableOffset =
		((size_t)size + alignof(BYTE*) - 1) & ~(size_t)(alignof(BYTE*) - 1);

	m_pData = new BYTE[tableOffset + sizeof(BYTE*) * height];
	m_pPixels = (BYTE**)(m_pData + tableOffset);
}

void CSpritePalBase::SetEmptySprite()
{
	Release();
	m_bInit = true;
}

bool CSpritePalBase::LoadFromFile(std::ifstream &file)
{
	Release();

	// The header goes into locals so nothing is allocated from it until
	// it has been checked.
	DWORD	size	= 0;
	WORD	width	= 0;
	WORD	height	= 0;

	file.read((char *)&size, 4);
	file.read((char *)&width, 2);
	file.read((char *)&height, 2);

	if (!file)
	{
		SetEmptySprite();
		return false;
	}

	// The body is size bytes of packed pixel data followed by one WORD
	// per scanline. Reject a header the file cannot back with that much
	// data, rather than sizing an allocation from a number the file is
	// free to invent.
	const std::streampos	afterHeader	= file.tellg();

	file.seekg(0, std::ios::end);

	const std::streampos	endOfFile	= file.tellg();

	file.seekg(afterHeader, std::ios::beg);

	if (!file)
	{
		SetEmptySprite();
		return false;
	}

	const unsigned long long	required =
		(unsigned long long)size + (unsigned long long)height * 2ULL;

	if (required > (unsigned long long)(endOfFile - afterHeader))
	{
		SetEmptySprite();
		return false;
	}

	m_Size		= size;
	m_Width		= width;
	m_Height	= height;

	AllocateDataAndScanlineTable(m_Size, m_Height);

	file.read((char *)m_pData, m_Size);

	WORD *indexArray = new WORD[m_Height];

	file.read((char *)indexArray, m_Height<<1);

	if (!file)
	{
		delete []indexArray;
		SetEmptySprite();
		return false;
	}

	// Build the scanline table.
	//
	// The per-scanline byte counts come from the file and are summed to
	// give each scanline's offset, so the running total has to stay
	// inside the pixel data. Without this check the pointers stored in
	// m_pPixels[] can address anything, and the Blt routines dereference
	// them later. The accumulator is wider than DWORD because height
	// entries of up to 0xFFFF can otherwise wrap it.
	unsigned long long	offset	= 0;
	bool			bValid	= true;

	for (int i=0; i<m_Height; i++)
	{
		// One past the end is tolerated here so the table can be built
		// and inspected. ValidateScanlineData() below is stricter and
		// rejects it, because a scanline with no segment count byte
		// cannot be drawn.
		if (offset > (unsigned long long)m_Size)
		{
			bValid = false;
			break;
		}

		m_pPixels[i] = m_pData + offset;

		offset += indexArray[i];
	}

	delete []indexArray;

	if (!bValid)
	{
		SetEmptySprite();
		return false;
	}

	// The scanline table is in range. Now check what it points at:
	// the run length data itself has to describe a sprite of this
	// width and stay inside the pixel data, because every blit routine
	// walks it and writes into the destination surface from it.
	if (!ValidateScanlineData())
	{
		SetEmptySprite();
		return false;
	}

	m_bInit = true;

	return true;
}

bool CSpritePalBase::ValidateScanlines(int bytesPerPixel) const
{
	if (m_pData == NULL || m_pPixels == NULL)
		return true;

	const BYTE* const	pDataEnd = m_pData + m_Size;

	for (int i=0; i<m_Height; i++)
	{
		const BYTE*	pPixels = m_pPixels[i];

		// Every scanline carries at least its segment count byte, so a
		// pointer at or past the end of the data is not drawable.
		if (pPixels < m_pData || pPixels >= pDataEnd)
			return false;

		const int	count = *pPixels++;

		// Pixels the scanline decodes to so far.
		int		x = 0;

		for (int j=0; j<count; j++)
		{
			// Both run lengths have to be readable.
			if (pPixels+1 >= pDataEnd)
				return false;

			const int	transparentCount = *pPixels++;
			const int	colourCount	 = *pPixels++;

			// A scanline never decodes to more than the sprite's own
			// width.
			//
			// The encoder normally emits exactly width pixels per row,
			// but its per-row segment count is stored in a byte, so a
			// row needing more than 255 segments truncates and decodes
			// to fewer. Only the upper bound is enforced here, which is
			// what the blit routines actually need.
			x += transparentCount + colourCount;

			if (x > (int)m_Width)
				return false;

			// The colours themselves have to lie inside the data.
			if (pPixels + colourCount*bytesPerPixel > pDataEnd)
				return false;

			pPixels += colourCount*bytesPerPixel;
		}
	}

	return true;
}

bool CSpritePalBase::DecodeGpuPixels(std::span<uint32_t> pixels, bool withAlpha) const
{
	if (!IsInit() || !m_pData || !m_pPixels || pixels.size() != size_t(m_Width) * m_Height
		|| !ValidateScanlines(withAlpha ? 2 : 1)) return false;
	std::fill(pixels.begin(), pixels.end(), 0);
	for (int y = 0; y < m_Height; ++y) {
		const BYTE* source = m_pPixels[y];
		const int runs = *source++;
		int x = 0;
		for (int run = 0; run < runs; ++run) {
			x += *source++;
			const int count = *source++;
			for (int i = 0; i < count; ++i) {
				const unsigned alpha = withAlpha ? *source++ : 32;
				if (alpha > 32) return false; // Preserve legacy handling of malformed alpha on the CPU.
				const unsigned index = *source++;
				pixels[size_t(y) * m_Width + x++] = 0xff000000u | (index << 16) | (alpha << 8);
			}
		}
	}
	return true;
}

bool CSpritePalBase::SaveToFile(std::ofstream &file)
{
	if(IsNotInit())
	{
		MessageBox(NULL, "아무것도 없는데 멀 저장해-_-", "CSpritePalBase", MB_OK);
		return false;
	}

	if(IsEmptySprite())
	{
		return false;
	}

	file.write((const char *)&m_Size, 4);

//	// size가 0이면 리턴하쟈
//	if(m_Size == 0)
//		return true;

	file.write((const char *)&m_Width, 2);
	file.write((const char *)&m_Height, 2);
	file.write((const char *)m_pData, m_Size);

	WORD index;

	for (int i=0; i<m_Height; i++)
	{
		if(i == m_Height -1)
		{
			index = static_cast<WORD>((m_pData+m_Size) - m_pPixels[i]);
		}
		else
			index = static_cast<WORD>(m_pPixels[i+1] - m_pPixels[i]);
		// byte수와 실제 data를 저장한다.
		file.write((const char*)&index, 2);
	}

	return true;
}

void CSpritePalBase::operator = (const CSpritePalBase& sprite)
{
	// 메모리 해제
	Release();

	m_Size = sprite.m_Size;
	m_Width = sprite.m_Width;
	m_Height = sprite.m_Height;
	m_bInit = true;

	AllocateDataAndScanlineTable(m_Size, m_Height);
	
	memcpy(m_pData, sprite.m_pData, m_Size);

	int i;
	for(i = 0; i < m_Height; i++)
	{
		m_pPixels[i] = m_pData + (sprite.m_pPixels[i]-sprite.m_pData);
	}
}
