//----------------------------------------------------------------------
// test_status_manager.cpp
//----------------------------------------------------------------------
//
// MStatusManager, the client's own derivation of the combat stats. In
// game the server sends them; the client computes them for the
// character-select slots from what the character list carries, and
// MPlayer::CalculateStatus takes its attack speed from here. These tests
// pin what it computes, through a local instance, and what the three
// character-select slots feed it.
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
}

TEST(StatusManager, SlayerDefenseIsHalfTheDex)
{
	CHECK_EQ(28, Slayer(MAX_SKILLDOMAIN, 0, 30, 57, 10).GetDefense());
	CHECK_EQ(28, Slayer(SKILLDOMAIN_SWORD, 150, 30, 57, 10).GetDefense());
}

TEST(StatusManager, SlayerProtection)
{
	CHECK_EQ(0, Slayer(MAX_SKILLDOMAIN, 0, 57, 30, 10).GetProtection());
	CHECK_EQ(57, Slayer(SKILLDOMAIN_GUN, 150, 57, 30, 10).GetProtection());
}

TEST(StatusManager, SlayerDamage)
{
	CHECK_EQ(1, Slayer(MAX_SKILLDOMAIN, 0, 149, 30, 10).GetMinDAM());
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

TEST(StatusManager, SlayerCaps)
{
	CHECK_EQ(500, Slayer(SKILLDOMAIN_SWORD, 1, 30, 1100, 10).GetTOHIT());
	CHECK_EQ(500, Slayer(MAX_SKILLDOMAIN, 0, 30, 1201, 10).GetDefense());
	CHECK_EQ(500, Slayer(SKILLDOMAIN_SWORD, 0, 750, 30, 10).GetProtection());
	CHECK_EQ(500, Slayer(SKILLDOMAIN_SWORD, 0, 9000, 30, 10).GetMinDAM());
	CHECK_EQ(500, Slayer(SKILLDOMAIN_SWORD, 0, 9000, 30, 10).GetMaxDAM());
}

//----------------------------------------------------------------------
// Vampire
//----------------------------------------------------------------------
TEST(StatusManager, VampireStats)
{
	CHECK_EQ(22, Vampire(7, 20, 20, 20).GetTOHIT());
	CHECK_EQ(360, Vampire(150, 200, 300, 150).GetTOHIT());
	CHECK_EQ(0, Vampire(3, 20, 1, 20).GetDefense());
	CHECK_EQ(10, Vampire(4, 20, 21, 20).GetDefense());
	CHECK_EQ(21, Vampire(9, 20, 20, 20).GetProtection());
	CHECK_EQ(330, Vampire(150, 300, 20, 20).GetProtection());
	CHECK_EQ(3, Vampire(1, 20, 20, 20).GetMinDAM());
	CHECK_EQ(80, Vampire(150, 301, 20, 20).GetMinDAM());
	CHECK_EQ(1, Vampire(0, 0, 0, 0).GetMinDAM());
	CHECK_EQ(5, Vampire(1, 20, 20, 20).GetMaxDAM());
	CHECK_EQ(1, Vampire(4, 5, 20, 20).GetMaxDAM());
	CHECK_EQ(105, Vampire(150, 301, 20, 20).GetMaxDAM());
	CHECK_EQ(1, Vampire(0, 0, 0, 0).GetMaxDAM());
}

TEST(StatusManager, VampireCaps)
{
	CHECK_EQ(1000, Vampire(100, 20, 1500, 20).GetTOHIT());
	CHECK_EQ(1000, Vampire(10, 20, 19999, 20).GetDefense());
	CHECK_EQ(1000, Vampire(10, 9999, 20, 20).GetProtection());
	CHECK_EQ(1000, Vampire(150, 9000, 20, 20).GetMinDAM());
	CHECK_EQ(1000, Vampire(150, 9000, 20, 20).GetMaxDAM());
}

//----------------------------------------------------------------------
// Ousters
//----------------------------------------------------------------------
TEST(StatusManager, OustersStats)
{
	CHECK_EQ(41, Ousters(13, 10, 57, 10).GetTOHIT());
	CHECK_EQ(300, Ousters(150, 200, 300, 150).GetTOHIT());
	CHECK_EQ(0, Ousters(3, 10, 1, 10).GetDefense());
	CHECK_EQ(24, Ousters(14, 10, 45, 10).GetDefense());
	CHECK_EQ(11, Ousters(19, 10, 20, 10).GetProtection());
	CHECK_EQ(315, Ousters(150, 300, 20, 10).GetProtection());
	CHECK_EQ(1, Ousters(1, 10, 20, 10).GetMinDAM());
	CHECK_EQ(45, Ousters(150, 301, 20, 10).GetMinDAM());
	CHECK_EQ(1, Ousters(0, 0, 0, 0).GetMinDAM());
	CHECK_EQ(1, Ousters(7, 5, 20, 10).GetMaxDAM());
	CHECK_EQ(75, Ousters(150, 301, 20, 10).GetMaxDAM());
	CHECK_EQ(1, Ousters(0, 0, 0, 0).GetMaxDAM());
}

TEST(StatusManager, OustersCaps)
{
	CHECK_EQ(1000, Ousters(1, 10, 20001, 10).GetTOHIT());
	CHECK_EQ(1000, Ousters(10, 10, 19999, 10).GetDefense());
	CHECK_EQ(1000, Ousters(10, 10000, 20, 10).GetProtection());
	CHECK_EQ(1000, Ousters(150, 100000, 20, 10).GetMinDAM());
	CHECK_EQ(1000, Ousters(150, 100000, 20, 10).GetMaxDAM());
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
TEST(StatusManager, SlayerSlotReadsTheWeaponDomain)
{
	MStatusManager status;

	status.SetCharacterSelectSlot(SlayerSlot(WEAPON_NONE));
	CHECK_EQ(28, status.GetTOHIT());
	CHECK_EQ(28, status.GetDefense());

	status.SetCharacterSelectSlot(SlayerSlot(WEAPON_SWORD));
	CHECK_EQ(29, status.GetTOHIT());

	status.SetCharacterSelectSlot(SlayerSlot(WEAPON_BLADE1));
	CHECK_EQ(29, status.GetTOHIT());

	status.SetCharacterSelectSlot(SlayerSlot(WEAPON_SR));
	CHECK_EQ(29, status.GetTOHIT());
	CHECK_EQ(1, status.GetMinDAM());
	CHECK_EQ(2, status.GetMaxDAM());

	status.SetCharacterSelectSlot(SlayerSlot(WEAPON_CROSS));
	CHECK_EQ(29, status.GetTOHIT());
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
	CHECK_EQ(249, status.GetMinDAM());
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
	CHECK_EQ(30, status.GetDefense());
	CHECK_EQ(11, status.GetProtection());
	CHECK_EQ(2, status.GetMinDAM());
	CHECK_EQ(3, status.GetMaxDAM());
}
