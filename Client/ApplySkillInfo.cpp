//----------------------------------------------------------------------
// ApplySkillInfo.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "ApplySkillInfo.h"
#include "Gpackets/GCSkillInfo.h"
#include "MSkillManager.h"
#include "UserInformation.h"
#include "ConvertDuration.h"

//----------------------------------------------------------------------
// Is SkillInTable ( skillType )
//----------------------------------------------------------------------
// A wire skill type at or past the info table's size names no skill of
// this client. The size is MIN_RESULT_ACTIONINFO (512) as constructed,
// and LoadFromFile resizes the table to the count its data file
// declares; given a skill info file of at least the server's SKILL_MAX
// (397) rows, every type the server sends lies below it. Such an entry
// is skipped whole, before its type is taken for an ACTIONINFO - 2048
// and up are past the enum's range of values - and before a learn it
// cannot pass leaves the domain's new-skill flag set.
//----------------------------------------------------------------------
static bool
IsSkillInTable(int skillType)
{
	return skillType >= 0 && skillType < g_pSkillInfoTable->GetSize();
}

//----------------------------------------------------------------------
// Apply SkillInfo ( GCSkillInfo* )
//----------------------------------------------------------------------
void
ApplySkillInfo(GCSkillInfo* pPacket)
{
	g_pUserInformation->HasSkillRestore = false;
	g_pUserInformation->HasMagicGroundAttack = false;
	g_pUserInformation->HasMagicHallu = false;
	g_pUserInformation->HasMagicBloodyWarp = false;
	g_pUserInformation->HasMagicBloodySnake = false;
				
	
	//--------------------------------------------------
	// Set up each domain the packet lists.
	//--------------------------------------------------
	int domainNum = pPacket->getListNum();

	int pcType = pPacket->getPCType();

	g_pSkillManager->InitSkillList();
	
	for (int d=0; d<domainNum; d++)
	{
		PCSkillInfo* pSkillInfo = pPacket->popFrontListElement();

		//--------------------------------------------------
		// By race...
		//--------------------------------------------------
		int i;

		if (pSkillInfo!=NULL)
		{
			switch (pcType)
			{
				//--------------------------------------------------
				//
				//						Slayer
				//
				//--------------------------------------------------
				case PC_SLAYER :
				{
					SlayerSkillInfo* pSlayerSkillInfo = (SlayerSkillInfo*)pSkillInfo;
		
					int domainType = pSlayerSkillInfo->getDomainiType();

					//--------------------------------------------------
					// The skills learned..
					//--------------------------------------------------
					int num = pSlayerSkillInfo->getListNum();

					for (i=0; i<num; i++)
					{
						SubSlayerSkillInfo* pInfo = pSlayerSkillInfo->popFrontListElement();

						if (pInfo!=NULL)
						{
							int skillType	= pInfo->getSkillType();

							if (!IsSkillInTable( skillType ))
							{
								delete pInfo;
								continue;
							}

							int skillExp	= pInfo->getSkillExp();
							int ExpLevel	= pInfo->getSkillExpLevel();
							DWORD delayTime = ConvertDurationToMillisecond( pInfo->getSkillTurn() );
							int currentDelay = ConvertDurationToMillisecond( pInfo->getCastingTime() );
							bool bEnable	= pInfo->getEnable();
							
//							if(skillType == SKILL_SOUL_CHAIN)
//							{
//								int a = 0;
//								bEnable = true;
//							}

							// Mark the skill learned.

							if((*g_pSkillInfoTable)[skillType].GetSkillStep() == SKILL_STEP_ETC)
							{
								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_BLADE)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}

								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_SWORD)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}
								
								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_GUN)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}
								
								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_HEAL)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}
								
								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_ENCHANT)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}
								
								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_VAMPIRE)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}

								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_ETC)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}
								
							}
							else
							{
								if (auto* entry = g_pSkillManager->GetMutable(domainType)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}
							}

							if (auto* entry = g_pSkillInfoTable->GetMutable(skillType)) {
								entry->SetExpLevel( ExpLevel );
								entry->SetSkillExp( skillExp );

								entry->SetDelayTime( delayTime );
								entry->SetEnable( bEnable );
							}

							if (auto* entry = g_pSkillInfoTable->GetMutable(skillType)) {
								entry->SetAvailableTime( currentDelay );
							}

							switch (skillType)
							{
								case MAGIC_RESTORE :
									g_pUserInformation->HasSkillRestore = true;
								break;

								case SKILL_THROW_BOMB :
								{
									if (auto* entry = g_pSkillInfoTable->GetMutable(BOMB_SPLINTER)) {
										entry->SetExpLevel( ExpLevel );
									}
									if (auto* entry = g_pSkillInfoTable->GetMutable(BOMB_ACER)) {
										entry->SetExpLevel( ExpLevel );
									}
									if (auto* entry = g_pSkillInfoTable->GetMutable(BOMB_BULLS)) {
										entry->SetExpLevel( ExpLevel );
									}
									if (auto* entry = g_pSkillInfoTable->GetMutable(BOMB_STUN)) {
										entry->SetExpLevel( ExpLevel );
									}
									if (auto* entry = g_pSkillInfoTable->GetMutable(BOMB_CROSSBOW)) {
										entry->SetExpLevel( ExpLevel );
									}
								}
								break;

								case SKILL_INSTALL_MINE :
									if (auto* entry = g_pSkillInfoTable->GetMutable(MINE_ANKLE_KILLER)) {
										entry->SetExpLevel( ExpLevel );
									}
									if (auto* entry = g_pSkillInfoTable->GetMutable(MINE_POMZ)) {
										entry->SetExpLevel( ExpLevel );
									}
									if (auto* entry = g_pSkillInfoTable->GetMutable(MINE_AP_C1)) {
										entry->SetExpLevel( ExpLevel );
									}
									if (auto* entry = g_pSkillInfoTable->GetMutable(MINE_DIAMONDBACK)) {
										entry->SetExpLevel( ExpLevel );
									}
									if (auto* entry = g_pSkillInfoTable->GetMutable(MINE_SWIFT_EX)) {
										entry->SetExpLevel( ExpLevel );
									}
								break;
							}
							
							delete pInfo;
						}
					}

					//--------------------------------------------------
					// Is there a new skill to learn?
					//--------------------------------------------------
					if (pSlayerSkillInfo->isLearnNewSkill())
					{
						if (auto* entry = g_pSkillManager->GetMutable(domainType)) {
							entry->SetNewSkill();
						}
					}

				}
				break;

				//--------------------------------------------------
				//
				//						Vampire
				//
				//--------------------------------------------------
				case PC_VAMPIRE :
				{
					VampireSkillInfo* pVampireSkillInfo = (VampireSkillInfo*)pSkillInfo;

					int domainType = SKILLDOMAIN_VAMPIRE;

					//--------------------------------------------------
					// The skills learned..
					//--------------------------------------------------
					int num = pVampireSkillInfo->getListNum();

					for (i=0; i<num; i++)
					{
						SubVampireSkillInfo* pInfo = pVampireSkillInfo->popFrontListElement();

						if (pInfo!=NULL)
						{
							int skillType	= pInfo->getSkillType();

							if (!IsSkillInTable( skillType ))
							{
								delete pInfo;
								continue;
							}

							DWORD delayTime = ConvertDurationToMillisecond( pInfo->getSkillTurn() );
							int currentDelay = ConvertDurationToMillisecond( pInfo->getCastingTime() );							
							
							// Mark the skill learned.
							if((*g_pSkillInfoTable)[skillType].GetSkillStep() == SKILL_STEP_ETC)
							{
								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_BLADE)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}

								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_SWORD)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}
								
								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_GUN)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}
								
								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_HEAL)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}
								
								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_ENCHANT)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}
								
								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_VAMPIRE)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}

								if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_ETC)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}
								
							}
							else
							{
								if (auto* entry = g_pSkillManager->GetMutable(domainType)) {
									entry->SetNewSkill();
									entry->LearnSkill( (ACTIONINFO)skillType );
								}
							}
							
							if (auto* entry = g_pSkillInfoTable->GetMutable(skillType)) {
								entry->SetDelayTime( delayTime );
								entry->SetAvailableTime( currentDelay );
							}

							switch (skillType)
							{
								case MAGIC_GROUND_ATTACK :
									g_pUserInformation->HasMagicGroundAttack = true;
								break;

//								case MAGIC_HALLUCINATION :
//									g_pUserInformation->HasMagicHallu = true;
//								break;

								case MAGIC_BLOODY_SNAKE:
									g_pUserInformation->HasMagicBloodySnake = true;
									break;

								case MAGIC_BLOODY_WARP:
									g_pUserInformation->HasMagicBloodyWarp = true;
									break;
							}

							delete pInfo;
						}
					}

					//--------------------------------------------------
					// Is there a new skill to learn?
					//--------------------------------------------------
					if (pVampireSkillInfo->isLearnNewSkill())
					{
						if (auto* entry = g_pSkillManager->GetMutable(domainType)) {
							entry->SetNewSkill();
						}
					}
				}
				break;

				//--------------------------------------------------
				//
				//						Ousters
				//
				//--------------------------------------------------
				case PC_OUSTERS :
					{
						OustersSkillInfo* pOustersSkillInfo = (OustersSkillInfo*)pSkillInfo;
						
						int domainType = SKILLDOMAIN_OUSTERS;
						
						//--------------------------------------------------
						// The skills learned..
						//--------------------------------------------------
						int num = pOustersSkillInfo->getListNum();
						
						for (i=0; i<num; i++)
						{
							SubOustersSkillInfo* pInfo = pOustersSkillInfo->popFrontListElement();
							
							if (pInfo!=NULL)
							{
								int skillType	= pInfo->getSkillType();

								if (!IsSkillInTable( skillType ))
								{
									delete pInfo;
									continue;
								}

								DWORD delayTime = ConvertDurationToMillisecond( pInfo->getSkillTurn() );
								int currentDelay = ConvertDurationToMillisecond( pInfo->getCastingTime() );							
								int	expLevel	= pInfo->getExpLevel();

								// Mark the skill learned.
								if((*g_pSkillInfoTable)[skillType].GetSkillStep() == SKILL_STEP_ETC)
								{
									if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_BLADE)) {
										entry->SetNewSkill();
										entry->LearnSkill( (ACTIONINFO)skillType );
									}
									
									if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_SWORD)) {
										entry->SetNewSkill();
										entry->LearnSkill( (ACTIONINFO)skillType );
									}
									
									if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_GUN)) {
										entry->SetNewSkill();
										entry->LearnSkill( (ACTIONINFO)skillType );
									}
									
									if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_HEAL)) {
										entry->SetNewSkill();
										entry->LearnSkill( (ACTIONINFO)skillType );
									}
									
									if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_ENCHANT)) {
										entry->SetNewSkill();
										entry->LearnSkill( (ACTIONINFO)skillType );
									}
									
									if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_VAMPIRE)) {
										entry->SetNewSkill();
										entry->LearnSkill( (ACTIONINFO)skillType );
									}
									
									if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_OUSTERS)) {
										entry->SetNewSkill();
										entry->LearnSkill( (ACTIONINFO)skillType );
									}
									
									if (auto* entry = g_pSkillManager->GetMutable(SKILL_DOMAIN_ETC)) {
										entry->SetNewSkill();
										entry->LearnSkill( (ACTIONINFO)skillType );
									}
									
								}
								else
								{
									if (auto* entry = g_pSkillManager->GetMutable(domainType)) {
										entry->SetNewSkill();
										entry->LearnSkill( (ACTIONINFO)skillType );
									}
								}
								
								if (auto* entry = g_pSkillInfoTable->GetMutable(skillType)) {
									entry->SetExpLevel( expLevel );
								
									entry->SetDelayTime( delayTime );
								
									entry->SetAvailableTime( currentDelay );
								}
								
								delete pInfo;
							}
						}
						
						//--------------------------------------------------
						// Is there a new skill to learn?
						//--------------------------------------------------
						if (pOustersSkillInfo->isLearnNewSkill())
						{
							if (auto* entry = g_pSkillManager->GetMutable(domainType)) {
								entry->SetNewSkill();
							}
						}
					}
					break;
			}
		}

		delete pSkillInfo;
	}
}
