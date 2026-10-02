#include "CreatureNameSelection.h"
#include "MonsterNameTable.h"
#include <cstring>

namespace {

int SelectIndex(int size, int randomValue)
{
	return size > 0 && randomValue >= 0 ? randomValue % size : 0;
}

} // namespace

void CreatureNameSelection::SelectLevelName(const MLevelNameTable& names, int randomValue)
{
	m_LevelName = SelectIndex(names.GetSize(), randomValue);
}

void CreatureNameSelection::SelectHallucinationName(const MonsterNameTable& names, int randomValue)
{
	m_HallucinationName = SelectIndex(names.GetLastNameSize(), randomValue);
}

const char* CreatureNameSelection::GetLevelName(const MLevelNameTable& names) const
{
	return names[m_LevelName].GetString();
}

const char* CreatureNameSelection::GetHallucinationName(const char* actualName,
	const MString& operatorPrefix, const MonsterNameTable* names) const
{
	const char* prefix = operatorPrefix.GetString();
	if (actualName != nullptr && prefix != nullptr && prefix[0] != '\0'
		&& operatorPrefix.GetLength() != 0
		&& std::strncmp(actualName, prefix, operatorPrefix.GetLength()) == 0)
		return actualName;
	return names == nullptr ? nullptr : names->GetLastName(m_HallucinationName);
}
