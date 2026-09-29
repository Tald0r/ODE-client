//----------------------------------------------------------------------
// test_status_manager.cpp
//----------------------------------------------------------------------
//
// MStatusManager, the client's derivation of the combat stats. In game
// the server sends them; the client computes them for the
// character-select slots from what the character list carries, and
// MPlayer::CalculateStatus takes its attack speed from here. These tests
// pin what it computes, through a local instance, and what the three
// character-select slots feed it.
//
// To-hit, defense, protection and damage are the server's own per-race
// rules (decore's Formulas.cpp, which AbilityBalance.cpp calls), with
// the server's cap of 10000 and a combat damage bonus of 0, the server's
// setting the client never sees. The inputs are rows of
// third_party/decore/domain/vectors/stats.tsv, which decore_tests
// asserts on every toolchain; each check names its row. The attack speed
// is still the client's own: it has no rows, and the plan deletes
// CalculateStatus's recompute rather than sharing it.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MStatusManager.h"
#include "SkillDef.h"
#include "PCSlayerInfo.h"
#include "PCVampireInfo.h"
#include "PCOustersInfo.h"

namespace {

MStatusManager	Slayer(int domain, int domainLevel, int str, int dex, int intel)
{
	MStatusManager status;
	status.SetCurrentWeaponDomain(domain, domainLevel);
	status.Set(str, dex, intel);
	return status;
}

MStatusManager	Vampire(int level, int str, int dex, int intel)
{
	MStatusManager status;
	status.SetCurrentWeaponDomain(SKILLDOMAIN_VAMPIRE, level);
	status.Set(str, dex, intel);
	return status;
}

MStatusManager	Ousters(int level, int str, int dex, int intel)
{
	MStatusManager status;
	status.SetCurrentWeaponDomain(SKILLDOMAIN_OUSTERS, level);
	status.Set(str, dex, intel);
	return status;
}

// A slayer at STR 30, DEX 57 and INT 10 whose six domain levels differ,
// so a preview shows which one it read.
PCSlayerInfo	SlayerSlot(WeaponType weapon)
{
	PCSlayerInfo info;
	info.setSTR(30);
	info.setDEX(57);
	info.setINT(10);
	info.setSkillDomainLevel(SKILL_DOMAIN_BLADE, 11);
	info.setSkillDomainLevel(SKILL_DOMAIN_SWORD, 33);
	info.setSkillDomainLevel(SKILL_DOMAIN_GUN, 44);
	info.setSkillDomainLevel(SKILL_DOMAIN_HEAL, 55);
	info.setSkillDomainLevel(SKILL_DOMAIN_ENCHANT, 66);
	info.setSkillDomainLevel(SKILL_DOMAIN_ETC, 7);
	info.setWeaponType(weapon);
	return info;
}

} // namespace

//----------------------------------------------------------------------
// Slayer
//----------------------------------------------------------------------
TEST(StatusManager, SlayerToHitAddsTheWeaponDomainLevel)
{
	CHECK_EQ(28, Slayer(MAX_SKILLDOMAIN, 0, 30, 57, 10).GetTOHIT());
	CHECK_EQ(77, Slayer(SKILLDOMAIN_SWORD, 33, 30, 57, 10).GetTOHIT());
	CHECK_EQ(77, Slayer(SKILLDOMAIN_BLADE, 33, 30, 57, 10).GetTOHIT());
	CHECK_EQ(77, Slayer(SKILLDOMAIN_HEAL, 33, 30, 57, 10).GetTOHIT());
	CHECK_EQ(77, Slayer(SKILLDOMAIN_ENCHANT, 33, 30, 57, 10).GetTOHIT());
	CHECK_EQ(77, Slayer(SKILLDOMAIN_GUN, 33, 30, 57, 10).GetTOHIT());
	CHECK_EQ(11, Slayer(SKILLDOMAIN_SWORD, 1, 30, 20, 10).GetTOHIT());
	CHECK_EQ(370, Slayer(SKILLDOMAIN_BLADE, 150, 30, 290, 10).GetTOHIT());
	// No bonus for a weapon of no family the rule knows
	// (slayer-tohit-other-ignores-domain), where the client read 0.
	CHECK_EQ(28, Slayer(SKILLDOMAIN_ETC, 40, 30, 57, 10).GetTOHIT());
}

TEST(StatusManager, SlayerDefenseIsHalfTheDex)
{
	CHECK_EQ(28, Slayer(MAX_SKILLDOMAIN, 0, 30, 57, 10).GetDefense());
	CHECK_EQ(28, Slayer(SKILLDOMAIN_SWORD, 150, 30, 57, 10).GetDefense());
}

// slayer-protection-is-str and -weapon-and-level-ignored: bare hands
// too, where the client showed 0.
TEST(StatusManager, SlayerProtectionIsTheStr)
{
	CHECK_EQ(57, Slayer(MAX_SKILLDOMAIN, 0, 57, 30, 10).GetProtection());
	CHECK_EQ(57, Slayer(SKILLDOMAIN_GUN, 150, 57, 30, 10).GetProtection());
}

// slayer-min-damage-* and slayer-max-damage-*: bare hands deal STR/15
// at least, where the client showed 1; a gun (Arms) ignores the STR.
TEST(StatusManager, SlayerDamage)
{
	CHECK_EQ(9, Slayer(MAX_SKILLDOMAIN, 0, 149, 30, 10).GetMinDAM());
	CHECK_EQ(14, Slayer(MAX_SKILLDOMAIN, 0, 149, 30, 10).GetMaxDAM());
	CHECK_EQ(9, Slayer(SKILLDOMAIN_SWORD, 70, 149, 30, 10).GetMinDAM());
	CHECK_EQ(14, Slayer(SKILLDOMAIN_SWORD, 70, 149, 30, 10).GetMaxDAM());
	CHECK_EQ(9, Slayer(SKILLDOMAIN_HEAL, 70, 149, 30, 10).GetMinDAM());
	CHECK_EQ(14, Slayer(SKILLDOMAIN_ENCHANT, 70, 149, 30, 10).GetMaxDAM());
	CHECK_EQ(1, Slayer(SKILLDOMAIN_GUN, 70, 149, 30, 10).GetMinDAM());
	CHECK_EQ(2, Slayer(SKILLDOMAIN_GUN, 70, 149, 30, 10).GetMaxDAM());
	CHECK_EQ(0, Slayer(SKILLDOMAIN_SWORD, 0, 9, 30, 10).GetMinDAM());
	CHECK_EQ(0, Slayer(SKILLDOMAIN_SWORD, 0, 9, 30, 10).GetMaxDAM());
}

// The server caps at 10000, not the client's 500: the *-above-client-cap-500
// rows, slayer-tohit-at-cap and -past-cap, slayer-protection-past-cap.
TEST(StatusManager, SlayerCapsAreTheServers)
{
	CHECK_EQ(551, Slayer(SKILLDOMAIN_SWORD, 1, 30, 1100, 10).GetTOHIT());
	CHECK_EQ(600, Slayer(MAX_SKILLDOMAIN, 0, 30, 1201, 10).GetDefense());
	CHECK_EQ(750, Slayer(MAX_SKILLDOMAIN, 0, 750, 30, 10).GetProtection());
	CHECK_EQ(600, Slayer(MAX_SKILLDOMAIN, 0, 9000, 30, 10).GetMinDAM());
	CHECK_EQ(900, Slayer(MAX_SKILLDOMAIN, 0, 9000, 30, 10).GetMaxDAM());
	CHECK_EQ(10000, Slayer(SKILLDOMAIN_SWORD, 1, 30, 19998, 10).GetTOHIT());
	CHECK_EQ(10000, Slayer(SKILLDOMAIN_SWORD, 2, 30, 19998, 10).GetTOHIT());
	CHECK_EQ(10000, Slayer(MAX_SKILLDOMAIN, 0, 10001, 30, 10).GetProtection());
}

//----------------------------------------------------------------------
// Vampire
//----------------------------------------------------------------------
// The sums truncate once, after adding (the *-sums-before-truncating
// rows), and nothing floors the damage at 1 (vampire-*-damage-zero).
TEST(StatusManager, VampireStats)
{
	CHECK_EQ(22, Vampire(7, 20, 20, 20).GetTOHIT());				// vampire-tohit-level-7-truncates
	CHECK_EQ(360, Vampire(150, 200, 300, 150).GetTOHIT());			// vampire-tohit-level-150
	CHECK_EQ(1, Vampire(3, 20, 1, 20).GetDefense());				// vampire-defense-dex-1-level-3-...
	CHECK_EQ(11, Vampire(4, 20, 21, 20).GetDefense());				// vampire-defense-dex-21-level-4
	CHECK_EQ(21, Vampire(9, 20, 20, 20).GetProtection());			// vampire-protection-level-over-5
	CHECK_EQ(330, Vampire(150, 300, 20, 20).GetProtection());		// vampire-protection-level-150
	CHECK_EQ(3, Vampire(1, 20, 20, 20).GetMinDAM());				// vampire-min-damage-str-and-level
	CHECK_EQ(1, Vampire(4, 5, 20, 20).GetMinDAM());					// vampire-min-damage-sums-before-...
	CHECK_EQ(80, Vampire(150, 301, 20, 20).GetMinDAM());			// vampire-min-damage-level-150
	CHECK_EQ(0, Vampire(0, 0, 0, 0).GetMinDAM());					// vampire-min-damage-zero
	CHECK_EQ(5, Vampire(1, 20, 20, 20).GetMaxDAM());				// vampire-max-damage-str-and-level
	CHECK_EQ(2, Vampire(4, 5, 20, 20).GetMaxDAM());					// vampire-max-damage-sums-before-...
	CHECK_EQ(105, Vampire(150, 301, 20, 20).GetMaxDAM());			// vampire-max-damage-level-150
	CHECK_EQ(0, Vampire(0, 0, 0, 0).GetMaxDAM());					// vampire-max-damage-zero
}

// The server caps at 10000, not the client's 1000.
TEST(StatusManager, VampireCapsAreTheServers)
{
	CHECK_EQ(1540, Vampire(100, 20, 1500, 20).GetTOHIT());			// vampire-tohit-above-client-cap-1000
	CHECK_EQ(10000, Vampire(10, 20, 19999, 20).GetDefense());		// vampire-defense-past-cap
	CHECK_EQ(10000, Vampire(10, 9999, 20, 20).GetProtection());		// vampire-protection-past-cap
	CHECK_EQ(10000, Vampire(150, 70000, 20, 20).GetMinDAM());		// vampire-min-damage-past-cap
	CHECK_EQ(10000, Vampire(150, 70000, 20, 20).GetMaxDAM());		// vampire-max-damage-past-cap
}

//----------------------------------------------------------------------
// Ousters
//----------------------------------------------------------------------
// As for the vampire: one truncation after the sum, no floor at 1.
TEST(StatusManager, OustersStats)
{
	CHECK_EQ(41, Ousters(13, 10, 57, 10).GetTOHIT());				// ousters-tohit-dex-half-plus-level
	CHECK_EQ(300, Ousters(150, 200, 300, 150).GetTOHIT());			// ousters-tohit-level-150
	CHECK_EQ(1, Ousters(3, 10, 1, 10).GetDefense());				// ousters-defense-dex-1-level-3-...
	CHECK_EQ(25, Ousters(14, 10, 45, 10).GetDefense());				// ousters-defense-dex-45-level-14
	CHECK_EQ(11, Ousters(19, 10, 20, 10).GetProtection());			// ousters-protection-level-over-10
	CHECK_EQ(315, Ousters(150, 300, 20, 10).GetProtection());		// ousters-protection-level-150
	CHECK_EQ(1, Ousters(1, 10, 20, 10).GetMinDAM());				// ousters-min-damage-str-and-level
	CHECK_EQ(1, Ousters(7, 5, 20, 10).GetMinDAM());					// ousters-min-damage-sums-before-...
	CHECK_EQ(45, Ousters(150, 301, 20, 10).GetMinDAM());			// ousters-min-damage-level-150
	CHECK_EQ(0, Ousters(0, 0, 0, 0).GetMinDAM());					// ousters-min-damage-zero
	CHECK_EQ(2, Ousters(7, 5, 20, 10).GetMaxDAM());					// ousters-max-damage-sums-before-...
	CHECK_EQ(75, Ousters(150, 301, 20, 10).GetMaxDAM());			// ousters-max-damage-level-150
	CHECK_EQ(0, Ousters(0, 0, 0, 0).GetMaxDAM());					// ousters-max-damage-zero
}

// The server caps at 10000, not the client's 1000.
TEST(StatusManager, OustersCapsAreTheServers)
{
	CHECK_EQ(10000, Ousters(1, 10, 20001, 10).GetTOHIT());			// ousters-tohit-past-cap
	CHECK_EQ(10000, Ousters(10, 10, 19999, 10).GetDefense());		// ousters-defense-past-cap
	CHECK_EQ(10000, Ousters(10, 10000, 20, 10).GetProtection());	// ousters-protection-past-cap
	CHECK_EQ(10000, Ousters(150, 100000, 20, 10).GetMinDAM());		// ousters-min-damage-past-cap
	CHECK_EQ(10000, Ousters(150, 100000, 20, 10).GetMaxDAM());		// ousters-max-damage-past-cap
}

//----------------------------------------------------------------------
// The attack speed MPlayer::CalculateStatus reads is the client's own.
//----------------------------------------------------------------------
TEST(StatusManager, AttackSpeedIsTheClientsOwn)
{
	CHECK_EQ(10, Slayer(MAX_SKILLDOMAIN, 0, 100, 0, 0).GetAttackSpeed());
	CHECK_EQ(15, Slayer(SKILLDOMAIN_SWORD, 25, 100, 0, 0).GetAttackSpeed());
	CHECK_EQ(10, Slayer(SKILLDOMAIN_HEAL, 25, 100, 0, 0).GetAttackSpeed());
	CHECK_EQ(15, Slayer(SKILLDOMAIN_GUN, 25, 0, 100, 0).GetAttackSpeed());
	CHECK_EQ(35, Slayer(SKILLDOMAIN_BLADE, 25, 1000, 0, 0).GetAttackSpeed());
	CHECK_EQ(20, Vampire(50, 0, 100, 0).GetAttackSpeed());
	CHECK_EQ(30, Vampire(50, 0, 1000, 0).GetAttackSpeed());
	CHECK_EQ(15, Ousters(50, 0, 100, 0).GetAttackSpeed());
}

//----------------------------------------------------------------------
// The character-select slots
//----------------------------------------------------------------------
// The slot reads the level of the domain its weapon belongs to, as the
// server's domainLevelOf does: the cross is the heal domain's weapon
// (55 here), not the enchant domain's (66). slayerToHit adds 1.5 times it
// to DEX/2 = 28, truncated (slayer-tohit-*-odd-domain for the sword).
TEST(StatusManager, SlayerSlotReadsTheWeaponDomainLevel)
{
	MStatusManager status;

	status.SetCharacterSelectSlot(SlayerSlot(WEAPON_NONE));
	CHECK_EQ(28, status.GetTOHIT());
	CHECK_EQ(28, status.GetDefense());

	status.SetCharacterSelectSlot(SlayerSlot(WEAPON_SWORD));
	CHECK_EQ(77, status.GetTOHIT());

	status.SetCharacterSelectSlot(SlayerSlot(WEAPON_BLADE1));
	CHECK_EQ(44, status.GetTOHIT());

	status.SetCharacterSelectSlot(SlayerSlot(WEAPON_SR));
	CHECK_EQ(94, status.GetTOHIT());
	CHECK_EQ(1, status.GetMinDAM());
	CHECK_EQ(2, status.GetMaxDAM());

	status.SetCharacterSelectSlot(SlayerSlot(WEAPON_CROSS));
	CHECK_EQ(110, status.GetTOHIT());
	CHECK_EQ(30, status.GetProtection());
}

TEST(StatusManager, VampireSlotReadsTheCharacter)
{
	PCVampireInfo info;
	info.setSTR(20);
	info.setDEX(20);
	info.setINT(20);
	info.setLevel(7);
	info.setExp(1234);

	MStatusManager status;
	status.SetCharacterSelectSlot(info);
	CHECK_EQ(513, status.GetTOHIT());
	CHECK_EQ(256, status.GetDefense());
	CHECK_EQ(266, status.GetProtection());
	CHECK_EQ(250, status.GetMinDAM());
	CHECK_EQ(251, status.GetMaxDAM());
}

TEST(StatusManager, OustersSlotReadsTheLevel)
{
	PCOustersInfo info;
	info.setSTR(10);
	info.setDEX(57);
	info.setINT(10);
	info.setLevel(13);
	info.setExp(999);

	MStatusManager status;
	status.SetCharacterSelectSlot(info);
	CHECK_EQ(41, status.GetTOHIT());
	CHECK_EQ(31, status.GetDefense());
	CHECK_EQ(11, status.GetProtection());
	CHECK_EQ(2, status.GetMinDAM());
	CHECK_EQ(3, status.GetMaxDAM());
}
