//----------------------------------------------------------------------
//
// Filename    : GCSkillInfoHandler.cpp
// Written By  : elca
// Description : 
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillInfo.h"
#include "ClientDef.h"
#include "MSkillManager.h"
#include "UserInformation.h"

//----------------------------------------------------------------------
// 클라이언트가 게임 서버로부터 GCSkillInfo 패킷을 받게 되면,
// 패킷 안의 데이터들을 클라이언트에 저장한 후, 데이터 로딩이
// 끝이 나면 게임 서버로 CGReady 패킷을 보내면 된다.
//----------------------------------------------------------------------
void GCSkillInfoHandler::execute ( GCSkillInfo * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	g_pUserInformation->HasSkillRestore = false;
	g_pUserInformation->HasMagicGroundAttack = false;
	g_pUserInformation->HasMagicHallu = false;
	g_pUserInformation->HasMagicBloodyWarp = false;
	g_pUserInformation->HasMagicBloodySnake = false;
				
	
	//--------------------------------------------------
	// 각각에 domain에 대한 정보를 설정한다.
	//--------------------------------------------------
	int domainNum = pPacket->getListNum();

	int pcType = pPacket->getPCType();

	g_pSkillManager->InitSkillList();
	
	for (int d=0; d<domainNum; d++)
	{
		PCSkillInfo* pSkillInfo = pPacket->popFrontListElement();

		//--------------------------------------------------
		// 종족에 따라서...
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
					// 배운 skill들 체크..
					//--------------------------------------------------
					int num = pSlayerSkillInfo->getListNum();

					for (i=0; i<num; i++)
					{
						SubSlayerSkillInfo* pInfo = pSlayerSkillInfo->popFrontListElement();

						if (pInfo!=NULL)
						{
							int skillType	= pInfo->getSkillType();
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

							// Skill 배웠다는걸 체크한다.

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
					// 새로 배울 수 있는 skill이 있나?
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
					// 배운 skill들 체크..
					//--------------------------------------------------
					int num = pVampireSkillInfo->getListNum();

					for (i=0; i<num; i++)
					{
						SubVampireSkillInfo* pInfo = pVampireSkillInfo->popFrontListElement();

						if (pInfo!=NULL)
						{
							int skillType	= pInfo->getSkillType();
							DWORD delayTime = ConvertDurationToMillisecond( pInfo->getSkillTurn() );
							int currentDelay = ConvertDurationToMillisecond( pInfo->getCastingTime() );							
							
							// Skill 배웠다는걸 체크한다.
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
					// 새로 배울 수 있는 skill이 있나?
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
						// 배운 skill들 체크..
						//--------------------------------------------------
						int num = pOustersSkillInfo->getListNum();
						
						for (i=0; i<num; i++)
						{
							SubOustersSkillInfo* pInfo = pOustersSkillInfo->popFrontListElement();
							
							if (pInfo!=NULL)
							{
								int skillType	= pInfo->getSkillType();
								DWORD delayTime = ConvertDurationToMillisecond( pInfo->getSkillTurn() );
								int currentDelay = ConvertDurationToMillisecond( pInfo->getCastingTime() );							
								int	expLevel	= pInfo->getExpLevel();

								// Skill 배웠다는걸 체크한다.
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
						// 새로 배울 수 있는 skill이 있나?
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

	//--------------------------------------------------
	// 홀리랜드 보너스 존이동 할때 reset
	//--------------------------------------------------
//	for(int i = 0; i < HOLYLAND_BONUS_MAX; i++)
//	{
//		g_abHolyLandBonusSkills[i] = false;
//	}
	
	for( int i = 0 ; i < SWEEPER_BONUS_MAX; i ++ )
		g_abSweeperBonusSkills[i] = false;

	//--------------------------------------------------
	// 현재 사용 가능한 skill들을 다시 체크한다.
	//--------------------------------------------------

	g_pSkillAvailable->SetAvailableSkills();

	__END_CATCH
}
