//----------------------------------------------------------------------
// ApplySkillInfo.h
//----------------------------------------------------------------------
// Rebuilds the player's skill model from a GCSkillInfo packet: the skill
// domains (g_pSkillManager), the per-skill state in g_pSkillInfoTable
// and the skill flags of g_pUserInformation. GCSkillInfoHandler calls it
// and then does what reaches the executable: it clears the sweeper
// bonus skills and asks g_pSkillAvailable which skills can be used now.
// Kept apart from the handler so that it can be tested on real packets.
//----------------------------------------------------------------------

#ifndef __APPLYSKILLINFO_H__
#define __APPLYSKILLINFO_H__

class GCSkillInfo;

//----------------------------------------------------------------------
// ApplySkillInfo
//----------------------------------------------------------------------
// In order: clears the five skill flags of g_pUserInformation
// (HasSkillRestore, HasMagicGroundAttack, HasMagicHallu,
// HasMagicBloodyWarp, HasMagicBloodySnake); rebuilds every domain's
// skill tree from its root with nothing learned
// (MSkillManager::InitSkillList); then pops each domain entry off the
// packet and, for the packet's race, learns each skill it lists and
// sets that skill's table entry from the wire.
//
// - A skill type at or past g_pSkillInfoTable's size is skipped: it is
//   learned nowhere and changes no flag.
// - A skill whose table step is SKILL_STEP_ETC is learned in every
//   domain (the ousters list adds the ousters domain; the slayer and
//   vampire lists do not); any other skill in the entry's domain: the
//   slayer entry's wire domain type, SKILLDOMAIN_VAMPIRE or
//   SKILLDOMAIN_OUSTERS. Each learn first sets that domain's new-skill
//   flag, which MSkillDomain::LearnSkill requires and clears when it
//   learns the skill.
// - Slayer: exp level, exp, reuse delay, enable and remaining delay;
//   Restore sets HasSkillRestore, and Throw Bomb and Install Mine pass
//   their exp level on to the five bombs and the five mines.
// - Vampire: reuse delay and remaining delay; Ground Attack, Bloody
//   Snake and Bloody Warp set their flags.
// - Ousters: exp level, reuse delay and remaining delay.
// - An entry that says a new skill can be learned sets its domain's
//   new-skill flag.
//
// The wire's durations are tenths of a second
// (ConvertDurationToMillisecond). Every sub-entry and entry is deleted
// once applied; a race other than the three pops and deletes the
// entries without applying them. The packet is left empty.
//----------------------------------------------------------------------
extern void		ApplySkillInfo(GCSkillInfo* pPacket);

#endif
