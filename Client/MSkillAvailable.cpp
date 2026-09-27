//----------------------------------------------------------------------
// MSkillAvailable.cpp
//----------------------------------------------------------------------
//
// The half of MSkillSet that asks the executable what the player can
// use right now (docs/RESTRUCTURING.md task 4.4): the weapon in hand,
// the inventory, the zone, the gear and the war bonuses all decide
// which skills are enabled. The rest of the skill core - the info
// table, the skill set itself, the domains and the tree - is in
// gamemodel; these three methods stay here, where the player is.
//
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MSkillManager.h"
#include "MTypeDef.h"

#include "MPlayer.h"
#include "MSlayerGear.h"
#include "MInventory.h"
#include "ServerInfo.h"
#include "MHelpManager.h"
#include "Properties.h"
#include "MItemFinder.h"
#include "UserInformation.h"
#include "VS_UI.h"
#include "MZone.h"		// the rare zone refuses invisibility
#include "DebugInfo.h"

extern MItem* UI_GetMouseItem();
extern bool IsBombMaterial(const MItem* pItem);

//----------------------------------------------------------------------
// The war bonuses the server grants, read by the skill check below and
// by the handlers that set them.
//----------------------------------------------------------------------
bool	g_abHolyLandBonusSkills[HOLYLAND_BONUS_MAX] = { false, };
bool	g_abSweeperBonusSkills[SWEEPER_BONUS_MAX] = { false, };

//----------------------------------------------------------------------
// Set Avaliable Skills
//----------------------------------------------------------------------
// 현재 사용 가능한 모든 skill들을 찾아서 추가한다.
//
// - 현재 들고 있는 무기를 보고
//   SkillTree에서 적절한 domain을 모두 enable / 나머지는 disable
// - inventory에서 skill에 관련된 기술을 찾는다.
// - 기타.. skill ?
//----------------------------------------------------------------------
void
MSkillSet::SetAvailableSkills()
{

	if (g_pPlayer==NULL 		
		|| g_pSkillManager==NULL
		|| g_pSkillManager->GetSize()==0		
		|| g_pSkillInfoTable==NULL
		|| g_pSkillInfoTable->GetSize()==0)
	{
		return;
	}

	//--------------------------------------------------
	// player의 현재 MP
	//--------------------------------------------------
	int playerMP;		
	BYTE flag;
	
	if (g_pPlayer->GetRace() != RACE_VAMPIRE)
	{
		playerMP = g_pPlayer->GetMP();	

		// EFFECTSTATUS_SACRIFICE 사용중이면 HP 1이 MP 2가 된다.
		if (g_pPlayer->HasEffectStatus(EFFECTSTATUS_SACRIFICE))
		{
			playerMP += (g_pPlayer->GetHP() << 1);
		}		
	}
	else
	{
		// vampire인 경우는 HP를 MP대신에 쓴다.
		playerMP = g_pPlayer->GetHP();	
	}

	// 모든 skill들을 지운다.
	clear();
	
	if( g_pZone != NULL && g_pZone->GetID() == 3001 )
		return;


	//-----------------------------------------------------
	//
	//					slayer인 경우
	//
	//-----------------------------------------------------
	switch(g_pPlayer->GetRace())
	{
		case RACE_SLAYER:
		{
			if (g_pSlayerGear==NULL
				|| g_pInventory==NULL)
			{
				return;
			}
			// 2004, 9, 16, sobeit add start - 인스톨 터렛일때 스킬 정보 갱신
			if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
			{
				insert(SKILLID_MAP::value_type( MAGIC_UN_TRANSFORM, SKILLID_NODE(MAGIC_UN_TRANSFORM, FLAG_SKILL_ENABLE) ));
				if ((*g_pSkillInfoTable)[SKILL_TURRET_FIRE].GetMP() > playerMP)
					insert(SKILLID_MAP::value_type( SKILL_TURRET_FIRE, SKILLID_NODE(SKILL_TURRET_FIRE, 0) ));
				else
					insert(SKILLID_MAP::value_type( SKILL_TURRET_FIRE, SKILLID_NODE(SKILL_TURRET_FIRE, FLAG_SKILL_ENABLE) ));
				insert(SKILLID_MAP::value_type( SKILL_VIVID_MAGAZINE, SKILLID_NODE(SKILL_VIVID_MAGAZINE, FLAG_SKILL_ENABLE) ));
				return;
			}
			// 2004, 9, 16, sobeit add end - 인스톨 터렛일때 스킬 정보 갱신
			//-----------------------------------------------------
			//
			// Domain에 따른 enable 체크..
			//
			//-----------------------------------------------------
			BYTE fDomain[MAX_SKILLDOMAIN];
			
			// 현재 들고 있는 item
			const MItem* pItem = (*g_pSlayerGear).GetItem( (MSlayerGear::GEAR_SLAYER)MSlayerGear::GEAR_SLAYER_RIGHTHAND );

			//-----------------------------------------------------
			// gun/sword/blade 만 체크하면 된다.
			//-----------------------------------------------------
			fDomain[SKILLDOMAIN_GUN]	= 0;
			fDomain[SKILLDOMAIN_BLADE]	= 0;
			fDomain[SKILLDOMAIN_SWORD]	= 0;

			if (pItem!=NULL && pItem->IsAffectStatus())
			{	
				//-----------------------------------------------------
				// 총이면.. 총만 enable
				//-----------------------------------------------------
				if (pItem->IsGunItem())
				{
					fDomain[SKILLDOMAIN_GUN]	= FLAG_SKILL_ENABLE;
				}
				//-----------------------------------------------------
				// sword이면 sword만 enable
				//-----------------------------------------------------
				else if (pItem->GetItemClass()==ITEM_CLASS_SWORD)
				{
					fDomain[SKILLDOMAIN_SWORD]	= FLAG_SKILL_ENABLE;
				}
				//-----------------------------------------------------
				// blade이면 blade만 enable
				//-----------------------------------------------------
				else if (pItem->GetItemClass()==ITEM_CLASS_BLADE)
				{
					fDomain[SKILLDOMAIN_BLADE]	= FLAG_SKILL_ENABLE;
				}
			}
			
			//-----------------------------------------------------
			//
			// SkillTree 검색
			//
			//-----------------------------------------------------
			//-----------------------------------------------------
			// Blade
			//-----------------------------------------------------
			auto* pBladeDomain = g_pSkillManager->GetMutable(SKILLDOMAIN_BLADE);
			if (pBladeDomain == nullptr) return;
			MSkillDomain& bladeDomain = *pBladeDomain;

			bladeDomain.SetBegin();		
			while (bladeDomain.IsNotEnd())
			{
				MSkillDomain::SKILLSTATUS	status	= bladeDomain.GetSkillStatus();

				// 배웠으면..
				if (status == MSkillDomain::SKILLSTATUS_LEARNED)
				{
					ACTIONINFO id = bladeDomain.GetSkillID();
		
					if ((*g_pSkillInfoTable)[id].GetMP() > playerMP)
					{
						flag = 0;
					}
					else
					{
						flag = fDomain[SKILLDOMAIN_BLADE];

						if ((*g_pSkillInfoTable)[id].IsActive())
						{
							flag |= FLAG_SKILL_ENABLE;
						}					
					}

					insert(SKILLID_MAP::value_type( id, SKILLID_NODE(id, flag) ));
				}

				// 다음
				bladeDomain.Next();
			}

			//-----------------------------------------------------
			// Sword
			//-----------------------------------------------------
			auto* pSwordDomain = g_pSkillManager->GetMutable(SKILLDOMAIN_SWORD);
			if (pSwordDomain == nullptr) return;
			MSkillDomain& swordDomain = *pSwordDomain;

			swordDomain.SetBegin();		
			while (swordDomain.IsNotEnd())
			{
				MSkillDomain::SKILLSTATUS	status	= swordDomain.GetSkillStatus();

				// 배웠으면..
				if (status == MSkillDomain::SKILLSTATUS_LEARNED)
				{
					ACTIONINFO id = swordDomain.GetSkillID();

					if ((*g_pSkillInfoTable)[id].GetMP() > playerMP)
					{
						flag = 0;
					}
					else
					{
						flag = fDomain[SKILLDOMAIN_SWORD];

						if ((*g_pSkillInfoTable)[id].IsActive())
						{
							flag |= FLAG_SKILL_ENABLE;
						}					
					}

					insert(SKILLID_MAP::value_type( id, SKILLID_NODE(id, flag) ));
				}

				// 다음
				swordDomain.Next();
			}

			//-----------------------------------------------------
			// Gun
			//-----------------------------------------------------
			auto* pGunDomain = g_pSkillManager->GetMutable(SKILLDOMAIN_GUN);
			if (pGunDomain == nullptr) return;
			MSkillDomain& gunDomain = *pGunDomain;

			gunDomain.SetBegin();		
			while (gunDomain.IsNotEnd())
			{
				MSkillDomain::SKILLSTATUS	status	= gunDomain.GetSkillStatus();
				
				// 배웠으면..
				if (status == MSkillDomain::SKILLSTATUS_LEARNED)
				{
					ACTIONINFO id = gunDomain.GetSkillID();

					if ((*g_pSkillInfoTable)[id].GetMP() > playerMP)
					{
						flag = 0;
					}
					else
					{
						flag = fDomain[SKILLDOMAIN_GUN];

						if ((*g_pSkillInfoTable)[id].IsActive())
						{
							flag |= FLAG_SKILL_ENABLE;
						}					
					}

					insert(SKILLID_MAP::value_type( id, SKILLID_NODE(id, flag) ));
				}

				// 다음
				gunDomain.Next();
			}

			//-----------------------------------------------------
			// Enchant - 그냥 모두 추가하면 된다.
			//-----------------------------------------------------
			auto* pEnchantDomain = g_pSkillManager->GetMutable(SKILLDOMAIN_ENCHANT);
			if (pEnchantDomain == nullptr) return;
			MSkillDomain& enchantDomain = *pEnchantDomain;

			enchantDomain.SetBegin();		
			while (enchantDomain.IsNotEnd())
			{
				MSkillDomain::SKILLSTATUS	status	= enchantDomain.GetSkillStatus();

				// 배웠으면..
				if (status == MSkillDomain::SKILLSTATUS_LEARNED)
				{
					ACTIONINFO id = enchantDomain.GetSkillID();

					if ((*g_pSkillInfoTable)[id].GetMP() > playerMP)
					{
						flag = 0;
					}
					else
					{
						flag = FLAG_SKILL_ENABLE;
					}

					insert(SKILLID_MAP::value_type( id, SKILLID_NODE(id, flag) ));
				}

				// 다음
				enchantDomain.Next();
			}

			
			//-----------------------------------------------------
			// Heal - 그냥 모두 추가하면 된다.
			//-----------------------------------------------------
			auto* pHealDomain = g_pSkillManager->GetMutable(SKILLDOMAIN_HEAL);
			if (pHealDomain == nullptr) return;
			MSkillDomain& healDomain = *pHealDomain;

			healDomain.SetBegin();		
			while (healDomain.IsNotEnd())
			{
				MSkillDomain::SKILLSTATUS	status	= healDomain.GetSkillStatus();

				// 배웠으면..
				if (status == MSkillDomain::SKILLSTATUS_LEARNED)
				{
					ACTIONINFO id = healDomain.GetSkillID();

					if ((*g_pSkillInfoTable)[id].GetMP() > playerMP)
					{
						flag = 0;
					}
					else
					{
						flag = FLAG_SKILL_ENABLE;
					}

					insert(SKILLID_MAP::value_type( id, SKILLID_NODE(id, flag) ));
				}

				// 다음
				healDomain.Next();
			}

			//-----------------------------------------------------
			// Etc - 그냥 모두 추가하면 된다.
			//-----------------------------------------------------
			auto* pEtcDomain = g_pSkillManager->GetMutable(SKILLDOMAIN_ETC);
			if (pEtcDomain == nullptr) return;
			MSkillDomain& etcDomain = *pEtcDomain;

			etcDomain.SetBegin();		
			while (etcDomain.IsNotEnd())
			{
				MSkillDomain::SKILLSTATUS	status	= etcDomain.GetSkillStatus();

				// 배웠으면..
				if (status == MSkillDomain::SKILLSTATUS_LEARNED)
				{
					ACTIONINFO id = etcDomain.GetSkillID();

					if ((*g_pSkillInfoTable)[id].GetMP() > playerMP)
					{
						flag = 0;
					}
					else
					{
						flag = FLAG_SKILL_ENABLE;
					}

					erase(id);
					insert(SKILLID_MAP::value_type( id, SKILLID_NODE(id, flag) ));
				}

				// 다음
				etcDomain.Next();
			}

			//-----------------------------------------------------
			//
			// Inventory 검색
			//
			//-----------------------------------------------------
			BOOL bCheckHolyWater	= TRUE;
			BOOL bCheckPortal		= TRUE;

			BOOL bCheckInstallMine = (gunDomain.GetSkillStatus(SKILL_INSTALL_MINE)==MSkillDomain::SKILLSTATUS_LEARNED);
			BOOL bCheckCreateMine = (gunDomain.GetSkillStatus(SKILL_MAKE_MINE)==MSkillDomain::SKILLSTATUS_LEARNED);
			BOOL bCheckCreateBomb = (gunDomain.GetSkillStatus(SKILL_MAKE_BOMB)==MSkillDomain::SKILLSTATUS_LEARNED);		
			
			BOOL bCheckBomb			= (gunDomain.GetSkillStatus(SKILL_THROW_BOMB)==MSkillDomain::SKILLSTATUS_LEARNED);
			BOOL bCheckBombOrMine   = bCheckBomb || bCheckInstallMine;
			BOOL bCheckBombOrMineMaterial   = bCheckCreateBomb || bCheckCreateMine;
			
			BOOL bHasBomb			= FALSE;
			BOOL bHasMine			= FALSE;
			BOOL bHasMineMaterial	= FALSE;
			BOOL bHasBombMaterial	= FALSE;

			g_pInventory->SetBegin();

			while (g_pInventory->IsNotEnd())
			{
				const MItem* pItem = g_pInventory->Get();

				ITEM_CLASS itemClass = pItem->GetItemClass();

			#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 藤속관櫓관
				if(itemClass == ITEM_CLASS_SUB_INVENTORY)
				{
					MSubInventory* pSubItem = (MSubInventory*)pItem;
					pSubItem->SetBegin();
					while(pSubItem->IsNotEnd())
					{

						const MItem* pSubSbuItem = pSubItem->Get();
						if(NULL != pSubSbuItem && bCheckPortal && pSubSbuItem->GetItemClass() == ITEM_CLASS_SLAYER_PORTAL_ITEM)
						{
							flag = FLAG_SKILL_ENABLE;
							insert(SKILLID_MAP::value_type( SUMMON_HELICOPTER, SKILLID_NODE(SUMMON_HELICOPTER, flag)) );
							bCheckPortal = FALSE;
						}
						pSubItem->Next();
					}
				}
			#endif

				//-----------------------------------------------------
				// Portal
				//-----------------------------------------------------
				if (bCheckPortal && itemClass==ITEM_CLASS_SLAYER_PORTAL_ITEM)
				{				
					flag = FLAG_SKILL_ENABLE;
				
					insert(SKILLID_MAP::value_type( SUMMON_HELICOPTER, SKILLID_NODE(SUMMON_HELICOPTER, flag)) );

					bCheckPortal = FALSE;
				}
				//-----------------------------------------------------
				// HolyWater
				//-----------------------------------------------------			
				else if (bCheckHolyWater && itemClass==ITEM_CLASS_HOLYWATER)
				{
					if ((*g_pSkillInfoTable)[MAGIC_THROW_HOLY_WATER].GetMP() > playerMP)
					{
						flag = 0;
					}
					else
					{
						flag = FLAG_SKILL_ENABLE;
					}

					insert(SKILLID_MAP::value_type( MAGIC_THROW_HOLY_WATER, SKILLID_NODE(MAGIC_THROW_HOLY_WATER, flag)) );				

					// [도움말] 벨트의 아이템 사용
//					__BEGIN_HELP_EVENT
//						ExecuteHelpEvent( HE_ITEM_APPEAR_HOLY_WATER );	
//					__END_HELP_EVENT

					bCheckHolyWater = FALSE;
				}
				//-----------------------------------------------------
				// Bomb / Mine - 종류별로 따로 추가해야한다.
				//-----------------------------------------------------
				else if (bCheckBombOrMine
						&& (itemClass==ITEM_CLASS_BOMB
							|| itemClass==ITEM_CLASS_MINE))
				{
					if (itemClass==ITEM_CLASS_BOMB)
					{
						bHasBomb = TRUE;

						if (bCheckBomb)	flag = FLAG_SKILL_ENABLE;
									else flag = 0;
					}
					else
					{
						bHasMine = TRUE;

						if (bCheckInstallMine) flag = FLAG_SKILL_ENABLE;
										else flag = 0;					
					}

					int skillID = pItem->GetUseActionInfo();

					

					// 폭탄/지뢰 마다 사용 가능한 아이콘을 추가한다.
					if (find((ACTIONINFO)skillID)==end())
					{
						insert(SKILLID_MAP::value_type( (ACTIONINFO)skillID, SKILLID_NODE((ACTIONINFO)skillID, flag)) );
					}
				}
				//-----------------------------------------------------
				// 폭탄/지뢰 재료
				//-----------------------------------------------------
				else if (bCheckBombOrMineMaterial
							&& itemClass==ITEM_CLASS_BOMB_MATERIAL)
				{
					if (IsBombMaterial(pItem))
					{
						bHasBombMaterial = TRUE;
					}
					else
					{
						bHasMineMaterial = TRUE;
					}
				}
				
				// 다음
				g_pInventory->Next();
			}

			// mouse에 있는 아이템도 체크한다.
			MItem* pMouseItem = UI_GetMouseItem();

			if (pMouseItem!=NULL)
			{
				ITEM_CLASS	itemClass	= pMouseItem->GetItemClass();
				bool isBombMaterial = IsBombMaterial(pMouseItem);

				bHasBomb			= bHasBomb || itemClass==ITEM_CLASS_BOMB;
				bHasMine			= bHasMine || itemClass==ITEM_CLASS_MINE;

				bHasMineMaterial	= bHasMineMaterial || (itemClass==ITEM_CLASS_BOMB_MATERIAL && !isBombMaterial);
				bHasBombMaterial	= bHasBombMaterial || (itemClass==ITEM_CLASS_BOMB_MATERIAL && isBombMaterial);
			}

			// 지뢰 설치 기술을 배웠고 지뢰가 있다면 icon을 enable시킨다.
			if (bCheckInstallMine)
			{
				ACTIONINFO skillID = SKILL_INSTALL_MINE;
				flag = (IsEnableSkill(skillID) && bHasMine? FLAG_SKILL_ENABLE : 0);
				
				iterator iSkill = find( skillID );
				if (iSkill != end())
				{			
					iSkill->second.Flag = flag;
				}
				else
				{
					insert(SKILLID_MAP::value_type( skillID, SKILLID_NODE(skillID, flag)) );
				}
			}

			// 지뢰 생성 기술을 배웠고 지뢰 재료가 있다면 icon을 enable시킨다.
			if (bCheckCreateMine)
			{
				ACTIONINFO skillID = SKILL_MAKE_MINE;
				flag = (IsEnableSkill(skillID) && bHasMineMaterial? FLAG_SKILL_ENABLE : 0);
				
				iterator iSkill = find( skillID );
				if (iSkill != end())
				{			
					iSkill->second.Flag = flag;
				}
				else
				{
					insert(SKILLID_MAP::value_type( skillID, SKILLID_NODE(skillID, flag)) );
				}
			}

			// 폭탄 생성 기술을 배웠고 폭탄 재료가 있다면 icon을 enable시킨다.
			if (bCheckCreateBomb)
			{
				ACTIONINFO skillID = SKILL_MAKE_BOMB;
				flag = (IsEnableSkill(skillID) && bHasBombMaterial? FLAG_SKILL_ENABLE : 0);
				
				iterator iSkill = find( skillID );
				if (iSkill != end())
				{			
					iSkill->second.Flag = flag;
				}
				else
				{
					insert(SKILLID_MAP::value_type( skillID, SKILLID_NODE(skillID, flag)) );
				}
			}

			// 폭탄 던지기 기술을 배웠고 폭탄이 있다면 icon을 enable시킨다.
			if (bCheckBomb)
			{
				ACTIONINFO skillID = SKILL_THROW_BOMB;
				flag = (IsEnableSkill(skillID) && bHasBomb? FLAG_SKILL_ENABLE : 0);
				
				iterator iSkill = find( skillID );
				if (iSkill != end())
				{			
					iSkill->second.Flag = flag;
				}
				else
				{
					insert(SKILLID_MAP::value_type( skillID, SKILLID_NODE(skillID, flag)) );
				}
			}

			//-----------------------------------------------------
			// Restore 임의로 추가
			//-----------------------------------------------------
			if (g_pUserInformation->HasSkillRestore)
			{
				if ((*g_pSkillInfoTable)[MAGIC_RESTORE].GetMP() > playerMP)
				{
					flag = 0;
				}
				else
				{
					flag = FLAG_SKILL_ENABLE;
				}

				insert(SKILLID_MAP::value_type( MAGIC_RESTORE, SKILLID_NODE(MAGIC_RESTORE, flag) ));
			}
		}
		break;

	//-----------------------------------------------------
	//
	//					vampire인 경우
	//
	//-----------------------------------------------------
	case RACE_VAMPIRE:
		{		
			//-----------------------------------------------------
			//
			// SkillTree 검색
			//
			//-----------------------------------------------------
			// [새기술3] 관 속에서는 기술 못 쓴다.
			if (g_pPlayer->IsInCasket())
			{
				flag = FLAG_SKILL_ENABLE;

				insert(SKILLID_MAP::value_type( MAGIC_OPEN_CASKET, SKILLID_NODE(MAGIC_OPEN_CASKET, flag) ));
				gC_vs_ui.SelectSkill( MAGIC_OPEN_CASKET );

				return;
			}
			
			// 뱀파이어인 경우만 쓸 수 있다. --> 박쥐나 늑대에서는 사용못한다.
			if (g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_MALE1
				|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_FEMALE1
				|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_MALE2
				|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_FEMALE2
				|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_MALE3
				|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_FEMALE3
				// add by Coffee 2006.12.7
				|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_MALE4
				|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_FEMALE4
				//add by viva
				|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_MALE5
				|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_FEMALE5
				//add by viva
				|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_MALE6
				|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_FEMALE6
				// end
				|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_OPERATOR)

			{
				auto* pVampireDomain = g_pSkillManager->GetMutable(SKILLDOMAIN_VAMPIRE);
				if (pVampireDomain == nullptr) return;
				MSkillDomain& vampireDomain = *pVampireDomain;
				
				vampireDomain.SetBegin();		

			#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 藤속관櫓관
				MItem* pSubInventory = NULL;
			#endif

				while (vampireDomain.IsNotEnd())
				{
					MSkillDomain::SKILLSTATUS	status	= vampireDomain.GetSkillStatus();

					// 배웠으면..
					if (status == MSkillDomain::SKILLSTATUS_LEARNED)
					{
						ACTIONINFO id = vampireDomain.GetSkillID();

						// 레어존에서는 인비저빌리티 못쓰게 한다...하드하드
						if ((*g_pSkillInfoTable)[id].GetMP() > playerMP
							|| (id == MAGIC_INVISIBILITY && (g_pZone->GetID() == 1104 || g_pZone->GetID() == 1106 || g_pZone->GetID() == 1114 || g_pZone->GetID() == 1115)))
						{
							flag = 0;
						}
						else
						{
							flag = FLAG_SKILL_ENABLE;					
						}

						// Item 사용하는거 체크
						switch (id)
						{
							case MAGIC_BLOODY_MARK :
							{
								MVampirePortalItemFinder finder(false);
							#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 藤속관櫓관
								if (NULL == ((MItemManager*)g_pInventory)->FindItemAll( finder , pSubInventory))
							#else
								if (NULL == ((MItemManager*)g_pInventory)->FindItem( finder ))
							#endif
								{
									flag = 0;
								}
							}
							break;

							case MAGIC_BLOODY_TUNNEL :
							{
								// Keep the HP check above; a marked seal is also required.
								MVampirePortalItemFinder finder(true);
							#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 藤속관櫓관
								if (NULL == ((MItemManager*)g_pInventory)->FindItemAll( finder , pSubInventory ))
							#else
								if (NULL == ((MItemManager*)g_pInventory)->FindItem( finder ))
							#endif
								{
									flag = 0;
								}
							}
							break;

							case MAGIC_TRANSFORM_TO_WOLF :
								if (NULL == g_pInventory->FindItem( ITEM_CLASS_VAMPIRE_ETC, 0 ))
								{
									flag = 0;
								}							
							break;

							case MAGIC_TRANSFORM_TO_BAT :

							#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 藤속관櫓관
								if (NULL == g_pInventory->FindItemAll( MItemClassTypeFinder(ITEM_CLASS_VAMPIRE_ETC , 1), pSubInventory ))
							#else
								if (NULL == g_pInventory->FindItem( ITEM_CLASS_VAMPIRE_ETC, 1 ))
							#endif
								{
									flag = 0;
								}							
							break;
							
							case SKILL_TRANSFORM_TO_WERWOLF :
								if(g_pInventory->FindItem( ITEM_CLASS_SKULL, 39) == NULL)
								{
									flag = 0;
								}
							break;

							case MAGIC_HOWL :
								flag=0;
							break;
							
							default:
							break;

						}

						insert(SKILLID_MAP::value_type( id, SKILLID_NODE(id, flag) ));
					}

					// 다음
					vampireDomain.Next();
				}
			}

			//-----------------------------------------------------
			//
			// Inventory 검색
			//
			//-----------------------------------------------------
			/*
			BOOL bCheckPortalMark = TRUE;
			BOOL bCheckPortalTunnel = TRUE;

			g_pInventory->SetBegin();

			while ((*g_pInventory).IsNotEnd())
			{
				const MItem* pItem = g_pInventory->Get();

				ITEM_CLASS itemClass = pItem->GetItemClass();

				//-----------------------------------------------------
				// Portal
				//-----------------------------------------------------
				if (itemClass==ITEM_CLASS_VAMPIRE_PORTAL_ITEM)
				{				
					flag = FLAG_SKILL_ENABLE;
				
					if (bCheckPortalMark && !pItem->IsMarked())
					{
						insert(SKILLID_MAP::value_type( MAGIC_BLOODY_MARK, SKILLID_NODE(MAGIC_BLOODY_MARK, flag)) );
						
						bCheckPortalMark = false;
					}

					if (bCheckPortalTunnel && pItem->IsMarked())
					{
						insert(SKILLID_MAP::value_type( MAGIC_BLOODY_TUNNEL, SKILLID_NODE(MAGIC_BLOODY_TUNNEL, flag)) );
						
						bCheckPortalTunnel = false;
					}				
				}

				// 다음
				g_pInventory->Next();
			}
			*/

			//-----------------------------------------------------
			//
			// 기본 Skill
			//
			//-----------------------------------------------------
			// 흡혈 --> 늑대나 박쥐는 흡혈 못한다.
			

			//-----------------------------------------------------
			// 불기둥 임의로 추가
			//-----------------------------------------------------
			if (g_pUserInformation->HasMagicGroundAttack)
			{
				if ((*g_pSkillInfoTable)[MAGIC_GROUND_ATTACK].GetMP() > playerMP)
				{
					flag = 0;
				}
				else
				{
					flag = FLAG_SKILL_ENABLE;
				}

				insert(SKILLID_MAP::value_type( MAGIC_GROUND_ATTACK, SKILLID_NODE(MAGIC_GROUND_ATTACK, flag) ));
			}
			
			SetAvailableVampireSkills();

			//-----------------------------------------------------
			// 블러디 스네이크 임의로 추가
			//-----------------------------------------------------
			if (g_pUserInformation->HasMagicBloodySnake)
			{
				if ((*g_pSkillInfoTable)[MAGIC_BLOODY_SNAKE].GetMP() > playerMP)
				{
					flag = 0;
				}
				else
				{
					flag = FLAG_SKILL_ENABLE;
				}

				insert(SKILLID_MAP::value_type( MAGIC_BLOODY_SNAKE, SKILLID_NODE(MAGIC_BLOODY_SNAKE, flag) ));
			}

			//-----------------------------------------------------
			// 블러디 워프 임의로 추가
			//-----------------------------------------------------
			if (g_pUserInformation->HasMagicBloodyWarp)
			{
				if ((*g_pSkillInfoTable)[MAGIC_BLOODY_WARP].GetMP() > playerMP)
				{
					flag = 0;
				}
				else
				{
					flag = FLAG_SKILL_ENABLE;
				}

				insert(SKILLID_MAP::value_type( MAGIC_BLOODY_WARP, SKILLID_NODE(MAGIC_BLOODY_WARP, flag) ));
			}
		}
		break;

	case RACE_OUSTERS:
		{		
			//-----------------------------------------------------
			//
			// SkillTree 검색
			//
			//-----------------------------------------------------
			{
				auto* pOustersDomain = g_pSkillManager->GetMutable(SKILLDOMAIN_OUSTERS);
				if (pOustersDomain == nullptr) return;
				MSkillDomain& oustersDomain = *pOustersDomain;
				
				oustersDomain.SetBegin();		
				while (oustersDomain.IsNotEnd())
				{
					MSkillDomain::SKILLSTATUS	status	= oustersDomain.GetSkillStatus();

					// 배웠으면..
					if (status == MSkillDomain::SKILLSTATUS_LEARNED)
					{
						ACTIONINFO id = oustersDomain.GetSkillID();

						SKILLINFO_NODE sInfo = (*g_pSkillInfoTable)[id];
						
						flag = 0;

						if (sInfo.GetMP() <= playerMP)
						{
							if(sInfo.IsActive())
							{
								flag = FLAG_SKILL_ENABLE;					
							}
							else
							{
								// 현재 들고 있는 item
								const MItem* pItem = (*g_pOustersGear).GetItem( (MOustersGear::GEAR_OUSTERS)MOustersGear::GEAR_OUSTERS_RIGHTHAND );
								
								if(sInfo.ElementalDomain == SKILLINFO_NODE::ELEMENTAL_DOMAIN_NO_DOMAIN || sInfo.ElementalDomain == SKILLINFO_NODE::ELEMENTAL_DOMAIN_WIND
									|| sInfo.ElementalDomain == SKILLINFO_NODE::ELEMENTAL_DOMAIN_ETC ||
									sInfo.GetSkillStep() == SKILL_STEP_ETC)
								{
									flag = FLAG_SKILL_ENABLE;
								}
								else
								{
									if (pItem!=NULL && pItem->IsAffectStatus())
									{	
										const int itemClass = pItem->GetItemClass();
										
										if ((itemClass == ITEM_CLASS_OUSTERS_CHAKRAM && sInfo.ElementalDomain == SKILLINFO_NODE::ELEMENTAL_DOMAIN_COMBAT) ||
											(itemClass == ITEM_CLASS_OUSTERS_WRISTLET &&
												(
													(sInfo.ElementalDomain == SKILLINFO_NODE::ELEMENTAL_DOMAIN_FIRE && static_cast<DWORD>(sInfo.Fire) <= g_pPlayer->GetElementalFire()) ||
													(sInfo.ElementalDomain == SKILLINFO_NODE::ELEMENTAL_DOMAIN_WATER && static_cast<DWORD>(sInfo.Water) <= g_pPlayer->GetElementalWater()) ||
													(sInfo.ElementalDomain == SKILLINFO_NODE::ELEMENTAL_DOMAIN_EARTH && static_cast<DWORD>(sInfo.Earth) <= g_pPlayer->GetElementalEarth())
												))
											)
										{
											flag = FLAG_SKILL_ENABLE;
										}
									}
								}
							}
						}

					#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 藤속관櫓관
						if( id == SKILL_SUMMON_SYLPH )
						{
							MItem* pSubInventory = NULL;
							if(NULL == ((MItemManager*)g_pInventory)->FindItemAll( MOustersSummonGemItemFinder(), pSubInventory ))
								flag = 0;
						}
					#else
						if( id == SKILL_SUMMON_SYLPH &&
							NULL == g_pInventory->FindItem( ITEM_CLASS_OUSTERS_SUMMON_ITEM ))
						{
							flag = 0;
						}
					#endif

						insert(SKILLID_MAP::value_type( id, SKILLID_NODE(id, flag) ));
					}

					// 다음
					oustersDomain.Next();
				}
			}

			//-----------------------------------------------------
			//
			// 기본 Skill
			//
			//-----------------------------------------------------
//			// 흡혈 --> 늑대나 박쥐는 흡혈 못한다.
//			if (g_pPlayer->GetCreatureType()!=CREATURETYPE_BAT
//				&& g_pPlayer->GetCreatureType()!=CREATURETYPE_WOLF)
//			{
//				if ((*g_pSkillInfoTable)[SKILL_BLOOD_DRAIN].GetMP() > playerMP)
//				{
//					flag = 0;
//				}
//				else
//				{
//					flag = FLAG_SKILL_ENABLE;
//				}
//				insert(SKILLID_MAP::value_type( SKILL_BLOOD_DRAIN, SKILLID_NODE(SKILL_BLOOD_DRAIN, flag) ));
//			}

		}
		break;

	default:
		break;
	}





	//-----------------------------------------------------
	//
	// 피의 성서 보너스 맘대로 추가-ㅅ-
	//
	//-----------------------------------------------------
	int i;
	for(i = 0; i < HOLYLAND_BONUS_MAX; i++)
	{
		if(g_abHolyLandBonusSkills[i] == true)
		{
			insert(SKILLID_MAP::value_type( (ACTIONINFO)(SKILL_HOLYLAND_BLOOD_BIBLE_ARMEGA+i), SKILLID_NODE((ACTIONINFO)(SKILL_HOLYLAND_BLOOD_BIBLE_ARMEGA+i), FLAG_SKILL_ENABLE) ));
		}
	}

	for(i = 0; i < SWEEPER_BONUS_MAX; i++)
	{
		if( g_abSweeperBonusSkills[i] == true )
		{
			insert( SKILLID_MAP::value_type( (ACTIONINFO)(SKILL_SWEEPER_BONUS_1 + i), SKILLID_NODE( (ACTIONINFO)(SKILL_SWEEPER_BONUS_1+i), FLAG_SKILL_ENABLE) ) );
		}
	}

	if (g_pPlayer->GetCreatureType()!=CREATURETYPE_BAT
		&& g_pPlayer->GetCreatureType()!=CREATURETYPE_WOLF)
	{
		MPlayerGear* pGear = NULL;
		MItemClassFinder itemFinder( ITEM_CLASS_COUPLE_RING );
		
		switch(g_pPlayer->GetRace())
		{
		case RACE_SLAYER:
			pGear = g_pSlayerGear;							
			break;

		case RACE_VAMPIRE:
			pGear = g_pVampireGear;
			itemFinder.SetItemClass( ITEM_CLASS_VAMPIRE_COUPLE_RING);
			break;

		case RACE_OUSTERS:
			pGear = g_pOustersGear;
			break;

		default:
			break;
		}
		
		MItem *pItem = pGear->FindItem( itemFinder );
		if(pItem != NULL)
		{
			insert(SKILLID_MAP::value_type( (ACTIONINFO)SKILL_LOVE_CHAIN, SKILLID_NODE((ACTIONINFO)(SKILL_LOVE_CHAIN), pItem->IsAffectStatus() ) ));
		}
	}
	
//	insert(SKILLID_MAP::value_type( (ACTIONINFO)SKILL_MAGIC_ELUSION, SKILLID_NODE((ACTIONINFO)(SKILL_MAGIC_ELUSION),  FLAG_SKILL_ENABLE)) );

	g_pPlayer->CalculateLightSight();
	

	//CheckMP();

	// 스킬이 비었을때 호출되면 -_- 난리난다.
//	if( size() > 5 )
//		gC_vs_ui.ResetHotKey();
}

void			
MSkillSet::SetAvailableVampireSkills()
{
	auto* pVampireDomain = g_pSkillManager->GetMutable(SKILLDOMAIN_VAMPIRE);
	if (pVampireDomain == nullptr) return;
	MSkillDomain& vampireDomain = *pVampireDomain;
	TYPE_CREATURETYPE PlayerCreatureType = g_pPlayer->GetCreatureType();

	int playerMP;		
	BYTE flag;
	
	if (g_pPlayer->GetRace() != RACE_VAMPIRE)
	{
		playerMP = g_pPlayer->GetMP();	

		// EFFECTSTATUS_SACRIFICE 사용중이면 HP 1이 MP 2가 된다.
		if (g_pPlayer->HasEffectStatus(EFFECTSTATUS_SACRIFICE))
		{
			playerMP += (g_pPlayer->GetHP() << 1);
		}		
	}
	else
	{
		// vampire인 경우는 HP를 MP대신에 쓴다.
		playerMP = g_pPlayer->GetHP();	
	}

	if (PlayerCreatureType!=CREATURETYPE_BAT
		&& PlayerCreatureType!=CREATURETYPE_WOLF
		&& PlayerCreatureType!=CREATURETYPE_WER_WOLF
		&& PlayerCreatureType!=CREATURETYPE_INSTALL_TURRET)
	{
		if ((*g_pSkillInfoTable)[SKILL_BLOOD_DRAIN].GetMP() > playerMP)
		{
			flag = 0;
		}
		else
		{
			flag = FLAG_SKILL_ENABLE;
		}
		insert(SKILLID_MAP::value_type( SKILL_BLOOD_DRAIN, SKILLID_NODE(SKILL_BLOOD_DRAIN, flag) ));
	}
	
	//-----------------------------------------------------
	// 
	// invisible인가?
	//
	//-----------------------------------------------------
	if (g_pPlayer->IsInvisible())
	{
		flag = FLAG_SKILL_ENABLE;
		
		insert(SKILLID_MAP::value_type( MAGIC_UN_INVISIBILITY, SKILLID_NODE(MAGIC_UN_INVISIBILITY, flag) ));
	}
		
	switch( PlayerCreatureType )
	{
	case CREATURETYPE_WOLF :
		{
			if ((*g_pSkillInfoTable)[MAGIC_EAT_CORPSE].GetMP() > playerMP)
			{
				flag = 0;
			}
			else
			{
				flag = FLAG_SKILL_ENABLE;
			}
			insert(SKILLID_MAP::value_type( MAGIC_EAT_CORPSE, SKILLID_NODE(MAGIC_EAT_CORPSE, flag) ));
			
			// 짖기 - -;
			if( vampireDomain.GetSkillStatus( MAGIC_HOWL ) == MSkillDomain::SKILLSTATUS_LEARNED )
			{
				if( (*g_pSkillInfoTable)[MAGIC_HOWL].GetMP() > playerMP)				
				{
					flag = 0;
				}
				else
				{
					flag = FLAG_SKILL_ENABLE;
				}
				insert(SKILLID_MAP::value_type( MAGIC_HOWL, SKILLID_NODE(MAGIC_HOWL, flag) ));
			}
		}
		break;
	case CREATURETYPE_WER_WOLF :
		{
			if( (*g_pSkillInfoTable)[SKILL_BITE_OF_DEATH].GetMP() > playerMP )
			{
				flag  = 0;
			}
			else
			{
				flag = FLAG_SKILL_ENABLE;
			}
			insert(SKILLID_MAP::value_type( SKILL_BITE_OF_DEATH, SKILLID_NODE(SKILL_BITE_OF_DEATH, flag) ));

			if( vampireDomain.GetSkillStatus( MAGIC_RAPID_GLIDING ) == MSkillDomain::SKILLSTATUS_LEARNED )
			{
				if( (*g_pSkillInfoTable)[MAGIC_RAPID_GLIDING].GetMP() > playerMP )
				{
					flag  = 0;
				}
				else
				{
					flag = FLAG_SKILL_ENABLE;
				}
				insert(SKILLID_MAP::value_type( MAGIC_RAPID_GLIDING, SKILLID_NODE(MAGIC_RAPID_GLIDING, flag) ));
			}
		}
		break;
	}
		
	//-----------------------------------------------------
	// 
	// 변신 중인가?
	//
	//-----------------------------------------------------
	if (PlayerCreatureType!=CREATURETYPE_VAMPIRE_MALE1
		&& PlayerCreatureType!=CREATURETYPE_VAMPIRE_FEMALE1
		&& PlayerCreatureType!=CREATURETYPE_VAMPIRE_MALE2
		&& PlayerCreatureType!=CREATURETYPE_VAMPIRE_FEMALE2
		&& PlayerCreatureType!=CREATURETYPE_VAMPIRE_MALE3
		&& PlayerCreatureType!=CREATURETYPE_VAMPIRE_FEMALE3
		// add by Coffee 2006.12.7
		&& PlayerCreatureType!=CREATURETYPE_VAMPIRE_MALE4
		&& PlayerCreatureType!=CREATURETYPE_VAMPIRE_FEMALE4
		//add by viva
		&& PlayerCreatureType!=CREATURETYPE_VAMPIRE_MALE5
		&& PlayerCreatureType!=CREATURETYPE_VAMPIRE_FEMALE5
		//add by viva
		&& PlayerCreatureType!=CREATURETYPE_VAMPIRE_MALE6
		&& PlayerCreatureType!=CREATURETYPE_VAMPIRE_FEMALE6
		// end 
		&& PlayerCreatureType!=CREATURETYPE_VAMPIRE_OPERATOR)
	{			
		flag = FLAG_SKILL_ENABLE;
		
		insert(SKILLID_MAP::value_type( MAGIC_UN_TRANSFORM, SKILLID_NODE(MAGIC_UN_TRANSFORM, flag) ));
	}	
}

//----------------------------------------------------------------------
// Check MP
//----------------------------------------------------------------------
// 선택된 skill들의 MP를 보고
// 사용가능한지 아닌지를 체크한다.
//----------------------------------------------------------------------
void
MSkillSet::CheckMP()
{
	
	// mp 체크할때.. 현재 장비중인 무기도 체크해야되는데
	// 일단은.. 이케 간다. T_T;
	SetAvailableSkills(); 

	/*
	if (g_pPlayer==NULL)
	{
		return;
	}

	//--------------------------------------------------
	// player의 현재 MP
	//--------------------------------------------------
	int playerMP;
	
	if (g_pPlayer->IsSlayer())
	{
		playerMP = g_pPlayer->GetMP();	
	}
	else
	{
		// vampire인 경우는 HP를 MP대신에 쓴다.
		playerMP = g_pPlayer->GetHP();	
	}

	SKILLID_MAP::iterator iID = begin();
	
	//--------------------------------------------------
	// 모든 skill들에 대해서 mp 체크
	//--------------------------------------------------
	while (iID != end())
	{
		ACTIONINFO		id = (*iID).first;
		SKILLID_NODE&	node = (*iID).second;	

		//--------------------------------------------------
		// MP사용량이 현재MP보다 큰 경우.. --> 사용 불가
		//--------------------------------------------------
		if ((*g_pSkillInfoTable)[id].GetMP() > playerMP)
		{
			node.SetDisable();
		}
		//--------------------------------------------------
		// 아니면, 사용 가능하게 표시
		//--------------------------------------------------
		else
		{
			node.SetEnable();
		}

		iID++;
	}
	*/
}
