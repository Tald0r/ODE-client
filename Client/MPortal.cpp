//----------------------------------------------------------------------
// MPortal.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MPortal.h"

//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------

MPortal::MPortal()
{
	m_ZoneID.clear();
	SetRect(0,0,0,0);
	m_Type = 0;

}

MPortal::MPortal(std::vector<WORD> ZoneID, P_RECT rect, BYTE Type)
{
	m_ZoneID = ZoneID;
	SetRect(rect);
	m_Type = Type;
}

MPortal::MPortal(std::vector<WORD> ZoneID, BYTE left, BYTE top, BYTE right, BYTE bottom, BYTE Type)
{
	m_ZoneID = ZoneID;
	SetRect(left, top, right, bottom);
	m_Type = Type;
}

MPortal::~MPortal()
{
}


//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
void	
MPortal::SaveToFile(ofstream& file)
{
	// Only TYPE_MULTI_PORTAL carries a count; every other type has one ID.
	if ((m_Type == TYPE_MULTI_PORTAL && m_ZoneID.size() > 255) ||
		(m_Type != TYPE_MULTI_PORTAL && m_ZoneID.size() != 1))
	{
		file.setstate(std::ios::failbit);
		return;
	}
	BYTE size = static_cast<BYTE>(m_ZoneID.size());
	if (!file.write(reinterpret_cast<const char*>(&m_Type), 1)) return;
	if (m_Type == TYPE_MULTI_PORTAL &&
		!file.write(reinterpret_cast<const char*>(&size), 1)) return;
	for (WORD id : m_ZoneID)
	{
		const BYTE encoded[]{static_cast<BYTE>(id), static_cast<BYTE>(id >> 8)};
		if (!file.write(reinterpret_cast<const char*>(encoded), 2)) return;
	}
	static_assert(sizeof(P_RECT) == 4);
	file.write(reinterpret_cast<const char*>(&m_Rect), SIZE_P_RECT);
}
		
//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
void	
MPortal::LoadFromFile(ifstream& file)
{
	BYTE type = 0;
	BYTE size = 1;
	if (!file.read(reinterpret_cast<char*>(&type), 1)) return;
	if (type == TYPE_MULTI_PORTAL &&
		!file.read(reinterpret_cast<char*>(&size), 1)) return;
	std::vector<WORD> destinations;
	destinations.reserve(size);
	for (int i = 0; i < size; ++i)
	{
		BYTE encoded[2]{};
		if (!file.read(reinterpret_cast<char*>(encoded), 2)) return;
		destinations.push_back(static_cast<WORD>(encoded[0] | (WORD(encoded[1]) << 8)));
	}
	P_RECT rect{};
	static_assert(sizeof(P_RECT) == 4);
	if (!file.read(reinterpret_cast<char*>(&rect), SIZE_P_RECT)) return;
	m_ZoneID.swap(destinations);
	m_Rect = rect;
	m_Type = type;
}
