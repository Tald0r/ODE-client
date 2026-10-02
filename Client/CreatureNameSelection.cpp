#include "CreatureNameSelection.h"
#include "MonsterNameTable.h"
#include <cstring>

void CreatureNameSelection::SelectLevelName(const MLevelNameTable& names, int randomValue)
{
	m_LevelName = randomValue % names.GetSize();
}

void CreatureNameSelection::SelectHallucinationName(const MonsterNameTable& names, int randomValue)
{
	m_HallucinationName = static_cast<std::uint16_t>(randomValue % names.GetLastNameSize());
}

const char* CreatureNameSelection::GetLevelName(const MLevelNameTable& names) const
{
	return names[m_LevelName].GetString();
}

const char* CreatureNameSelection::GetHallucinationName(const char* actualName,
	const MString& operatorPrefix, const MonsterNameTable* names) const
{
	if (std::strncmp(actualName, operatorPrefix.GetString(), operatorPrefix.GetLength()) == 0)
		return actualName;
	return names->GetLastName(m_HallucinationName);
}
