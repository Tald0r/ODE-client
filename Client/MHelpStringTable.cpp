#include "Client_PCH.h"
#include "MHelpStringTable.h"
#include <algorithm>
#include <utility>

MHelpStringTable* g_pHelpStringTable = NULL;

MHelpStringTable::MHelpStringTable() = default;
MHelpStringTable::~MHelpStringTable() = default;

void MHelpStringTable::Swap(MHelpStringTable& other) noexcept
{
	std::swap(m_Size, other.m_Size);
	std::swap(m_pTypeInfo, other.m_pTypeInfo);
	m_Displayed.swap(other.m_Displayed);
}

void MHelpStringTable::Init(int size)
{
	// Preserve the existing nonpositive-size reset: keep the text, clear flags.
	if (size <= 0)
	{
		ClearDisplayed();
		return;
	}
	MHelpStringTable next;
	next.MStringArray::Init(size);
	next.m_Displayed.assign(static_cast<std::size_t>(size), false);
	Swap(next);
}

void MHelpStringTable::Release()
{
	MStringArray::Release();
	m_Displayed.clear();
}

void MHelpStringTable::ClearDisplayed()
{
	std::fill(m_Displayed.begin(), m_Displayed.end(), false);
}

void MHelpStringTable::LoadFromFile(std::ifstream& file)
{
	MHelpStringTable next;
	next.MStringArray::LoadFromFile(file);
	if (!file.good()) return;
	next.m_Displayed.assign(static_cast<std::size_t>(next.m_Size), false);
	Swap(next);
}

const MString& MHelpStringTable::operator[](int type)
{
	if (type >= 0 && type < m_Size && static_cast<std::size_t>(type) < m_Displayed.size())
		m_Displayed[type] = true;
	return MStringArray::Get(type);
}

const MString& MHelpStringTable::Get(int type)
{
	return (*this)[type];
}
