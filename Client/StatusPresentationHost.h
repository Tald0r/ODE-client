#pragma once

// What a change of the player's status shows (task 4.235): the HP and MP
// bars, the character window's values, the level-up mark and the points,
// an ouster's elements, and the character sheet's fields. The executable
// installs it; a test installs its own. A missing service skips it.

namespace StatusPresentation {

// The character sheet's fields a status change sets.
enum SheetField
{
	SHEET_STR_PURE, SHEET_STR_MAX, SHEET_STR_EXP_REMAIN,
	SHEET_DEX_PURE, SHEET_DEX_MAX, SHEET_DEX_EXP_REMAIN,
	SHEET_INT_PURE, SHEET_INT_MAX, SHEET_INT_EXP_REMAIN,
	SHEET_EXP_REMAIN,			// a vampire's, an ouster's or the advancement's goal
	SHEET_GRADE, SHEET_GRADE_EXP_REMAIN,
	SHEET_GUILD_ID,
	SHEET_ALIGNMENT,			// the step, 0 (evil+) to 4 (good+)
	SHEET_ALIGNMENT_NUM,		// the value, -10000 to 10000
	SHEET_ADVANCEMENT_LEVEL,
};

struct Host
{
	// The HP bar: current of max.
	void (*SetHP)(int current, int maximum) = nullptr;
	// The MP bar: current of max.
	void (*SetMP)(int current, int maximum) = nullptr;
	// The character window's values.
	void (*SetCharInfoHP)(int value) = nullptr;
	void (*SetCharInfoMP)(int value) = nullptr;
	void (*SetCharInfoHP_max)(int value) = nullptr;
	void (*SetCharInfoMP_max)(int value) = nullptr;
	void (*SetCharInfoSTR)(int value) = nullptr;
	void (*SetCharInfoDEX)(int value) = nullptr;
	void (*SetCharInfoINT)(int value) = nullptr;
	void (*SetCharInfoFAME)(int value) = nullptr;
	void (*SetCharInfoTOHIT)(int value) = nullptr;
	void (*SetCharInfoDefense)(int value) = nullptr;
	void (*SetCharInfoProtection)(int value) = nullptr;
	void (*SetCharInfoLevel)(int value) = nullptr;
	void (*SetCharInfoSilverDamage)(int value) = nullptr;
	// The level-up mark, and the bonus and skill points to spend.
	void (*LevelUp)() = nullptr;
	void (*SetBonusPoint)(int points) = nullptr;
	void (*SetSkillPoint)(int points) = nullptr;
	// An ouster's element (an ITEMTABLE_INFO::ELEMENTAL_TYPE) at the value.
	void (*SetElementalValue)(int element, int value) = nullptr;
	// A field of the character sheet (the UI's g_char_slot_ingame).
	void (*SetSheet)(SheetField field, int value) = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
void SetHP(int current, int maximum);
void SetMP(int current, int maximum);
void SetCharInfoHP(int value);
void SetCharInfoMP(int value);
void SetCharInfoHP_max(int value);
void SetCharInfoMP_max(int value);
void SetCharInfoSTR(int value);
void SetCharInfoDEX(int value);
void SetCharInfoINT(int value);
void SetCharInfoFAME(int value);
void SetCharInfoTOHIT(int value);
void SetCharInfoDefense(int value);
void SetCharInfoProtection(int value);
void SetCharInfoLevel(int value);
void SetCharInfoSilverDamage(int value);
void LevelUp();
void SetBonusPoint(int points);
void SetSkillPoint(int points);
void SetElementalValue(int element, int value);
void SetSheet(SheetField field, int value);

}
