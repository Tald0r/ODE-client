#ifndef DARKEDEN_CREATURE_NAME_SELECTION_H
#define DARKEDEN_CREATURE_NAME_SELECTION_H

#include "MLevelNameTable.h"
#include <cstdint>

class MonsterNameTable;

// The executable supplies each random draw and the live name tables.
// Returned strings borrow their storage from the tables or the actual name.
class CreatureNameSelection
{
public:
	void SelectLevelName(const MLevelNameTable& names, int randomValue);
	void SelectHallucinationName(const MonsterNameTable& names, int randomValue);
	// Zero means no title; the legacy query returns the selected index.
	int HasLevelName() const { return m_LevelName; }
	const char* GetLevelName(const MLevelNameTable& names) const;
	// An operator keeps its actual name even when no hallucination table exists.
	const char* GetHallucinationName(const char* actualName, const MString& operatorPrefix,
		const MonsterNameTable* names) const;

private:
	int m_LevelName = 0;
	std::uint16_t m_HallucinationName = 0;
};

#endif
