#include "Client_PCH.h"
#include "MPalette.h"
#include <fstream>

MPalette::MPalette()
{
	m_Size = 0;
	m_pColor = NULL;
	m_OutOfRange = 0;
}

MPalette::MPalette(const MPalette& pal)
{
	m_Size = 0;
	m_pColor = NULL;
	m_OutOfRange = 0;

	if (pal.m_Size == 0 || pal.m_pColor == NULL)
		return;

	m_Size = pal.m_Size;
	m_pColor = new WORD[m_Size];

	memcpy(m_pColor, pal.m_pColor, m_Size * sizeof(WORD));
}

MPalette::~MPalette()
{
	Release();
}

void MPalette::Release()
{
	if(m_pColor != NULL)
	{ 
		delete []m_pColor;
		m_pColor = NULL;
		m_Size = 0;
	}
}

void MPalette::Init(BYTE size)
{
	if (size==0) return;
	Release();
	
	m_Size = size;
	m_pColor = new WORD[m_Size];
}

void MPalette::operator = (const MPalette& pal)
{
	// Release() below frees the colour table and zeroes the size. When
	// the source and the destination are the same object that empties
	// the source too, so the size read back afterwards would be zero and
	// the palette would be silently discarded.
	if (this == &pal)
		return;

	// Release the current table.
	Release();

	if (pal.m_Size == 0 || pal.m_pColor == NULL)
		return;

	m_Size = pal.m_Size;

	m_pColor = new WORD[m_Size];

	memcpy(m_pColor, pal.m_pColor, m_Size * sizeof(WORD));
}


// file save는 565를 기준으로 한다.
bool MPalette555::LoadFromFile(std::ifstream &file)
{
	Release();
	
	file.read((char *)&m_Size, 1);
	
	Init(m_Size);
	
	for(int i = 0; i < m_Size; i++)
	{
		file.read((char *)&m_pColor[i], 2);
		m_pColor[i] = ((m_pColor[i] & 0xffc0) >> 1) | (m_pColor[i] & 0x1f);
	}
	
	return true;
}

// file save는 565를 기준으로 한다.
bool MPalette555::SaveToFile(std::ofstream &file)
{
	file.write((const char *)&m_Size, 1);
	
	WORD color = 0;
	
	for(int i = 0; i < m_Size; i++)
	{
//		color = m_pColor[i];
		color = ((m_pColor[i] & 0xffe0) << 1) | (m_pColor[i] & 0x1f);
		file.write((const char *)&color, 2);
	}
	
	return true;
}

bool MPalette565::LoadFromFile(std::ifstream &file)
{
	Release();
	
	file.read((char *)&m_Size, 1);
	
	Init(m_Size);
	
	for(int i = 0; i < m_Size; i++)
	{
		file.read((char *)&m_pColor[i], 2);
	}

	return true;
}

bool MPalette565::SaveToFile(std::ofstream &file)
{
	file.write((const char *)&m_Size, 1);
	
	for(int i = 0; i < m_Size; i++)
	{
		file.write((const char *)&m_pColor[i], 2);
	}

	return true;
}

