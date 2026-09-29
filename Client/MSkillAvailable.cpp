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
// Finds every skill that is usable right now and adds it.
//
// - From the weapon currently held, enables every matching domain in the
//   SkillTree and disables the rest.
// - Looks through the inventory for items that grant skills.
// - Anything else that grants a skill.
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
	// The player's current MP
	//--------------------------------------------------
	int playerMP;		
	BYTE flag;
	
	if (g_pPlayer->GetRace() != RACE_VAMPIRE)
	{
		playerMP = g_pPlayer->GetMP();	

		// Under EFFECTSTATUS_SACRIFICE each HP counts as 2 MP.
		if (g_pPlayer->HasEffectStatus(EFFECTSTATUS_SACRIFICE))
		{
			playerMP += (g_pPlayer->GetHP() << 1);
		}		
	}
	else
	{
		// A vampire spends HP instead of MP.
		playerMP = g_pPlayer->GetHP();	
	}

	// What a vampire's skill costs takes its current INT (GetVampireConsumeMP).
	const int playerINT = (int)g_pPlayer->GetINT();

	// Clear every skill.
	clear();
	
	if( g_pZone != NULL && g_pZone->GetID() == 3001 )
		return;


	//-----------------------------------------------------
	//
	//					Slayer
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
			// 2004, 9, 16, sobeit add start - the skills while installed as a turret
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
			// 2004, 9, 16, sobeit add end - the skills while installed as a turret
			//-----------------------------------------------------
			//
			// Enable by domain
			//
			//-----------------------------------------------------
			BYTE fDomain[MAX_SKILLDOMAIN];
			
			// The item in hand
			const MItem* pItem = (*g_pSlayerGear).GetItem( (MSlayerGear::GEAR_SLAYER)MSlayerGear::GEAR_SLAYER_RIGHTHAND );

			//-----------------------------------------------------
			// Only gun, sword and blade depend on it.
			//-----------------------------------------------------
			fDomain[SKILLDOMAIN_GUN]	= 0;
			fDomain[SKILLDOMAIN_BLADE]	= 0;
			fDomain[SKILLDOMAIN_SWORD]	= 0;

			if (pItem!=NULL && pItem->IsAffectStatus())
			{	
				//-----------------------------------------------------
				// A gun enables only the gun domain
				//-----------------------------------------------------
				if (pItem->IsGunItem())
				{
					fDomain[SKILLDOMAIN_GUN]	= FLAG_SKILL_ENABLE;
				}
				//-----------------------------------------------------
				// A sword enables only the sword domain
				//-----------------------------------------------------
				else if (pItem->GetItemClass()==ITEM_CLASS_SWORD)
				{
					fDomain[SKILLDOMAIN_SWORD]	= FLAG_SKILL_ENABLE;
				}
				//-----------------------------------------------------
				// A blade enables only the blade domain
				//-----------------------------------------------------
				else if (pItem->GetItemClass()==ITEM_CLASS_BLADE)
				{
					fDomain[SKILLDOMAIN_BLADE]	= FLAG_SKILL_ENABLE;
				}
			}
			
			//-----------------------------------------------------
			//
			// Walk the skill tree
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

				// Learned
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

				// Next
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

				// Learned
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

				// Next
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
				
				// Learned
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

				// Next
				gunDomain.Next();
			}

			//-----------------------------------------------------
			// Enchant - add every learned skill.
			//-----------------------------------------------------
			auto* pEnchantDomain = g_pSkillManager->GetMutable(SKILLDOMAIN_ENCHANT);
			if (pEnchantDomain == nullptr) return;
			MSkillDomain& enchantDomain = *pEnchantDomain;

			enchantDomain.SetBegin();		
			while (enchantDomain.IsNotEnd())
			{
				MSkillDomain::SKILLSTATUS	status	= enchantDomain.GetSkillStatus();

				// Learned
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

				// Next
				enchantDomain.Next();
			}

			
			//-----------------------------------------------------
			// Heal - add every learned skill.
			//-----------------------------------------------------
			auto* pHealDomain = g_pSkillManager->GetMutable(SKILLDOMAIN_HEAL);
			if (pHealDomain == nullptr) return;
			MSkillDomain& healDomain = *pHealDomain;

			healDomain.SetBegin();		
			while (healDomain.IsNotEnd())
			{
				MSkillDomain::SKILLSTATUS	status	= healDomain.GetSkillStatus();

				// Learned
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

				// Next
				healDomain.Next();
			}

			//-----------------------------------------------------
			// Etc - add every learned skill.
			//-----------------------------------------------------
			auto* pEtcDomain = g_pSkillManager->GetMutable(SKILLDOMAIN_ETC);
			if (pEtcDomain == nullptr) return;
			MSkillDomain& etcDomain = *pEtcDomain;

			etcDomain.SetBegin();		
			while (etcDomain.IsNotEnd())
			{
				MSkillDomain::SKILLSTATUS	status	= etcDomain.GetSkillStatus();

				// Learned
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

				// Next
				etcDomain.Next();
			}

			//-----------------------------------------------------
			//
			// Search the inventory
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

			#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags inside the inventory
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

					// [Help] using an item on the belt
//					__BEGIN_HELP_EVENT
//						ExecuteHelpEvent( HE_ITEM_APPEAR_HOLY_WATER );	
//					__END_HELP_EVENT

					bCheckHolyWater = FALSE;
				}
				//-----------------------------------------------------
				// Bomb / Mine - each kind is added separately.
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

					

					// Add a usable icon for each bomb or mine.
					if (find((ACTIONINFO)skillID)==end())
					{
						insert(SKILLID_MAP::value_type( (ACTIONINFO)skillID, SKILLID_NODE((ACTIONINFO)skillID, flag)) );
					}
				}
				//-----------------------------------------------------
				// Bomb and mine materials
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
				
				// Next
				g_pInventory->Next();
			}

			// The item held by the mouse counts too.
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

			// Enable the icon if the mine-laying skill is learned and a mine is at hand.
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

			// Enable the icon if the mine-making skill is learned and mine material is at hand.
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

			// Enable the icon if the bomb-making skill is learned and bomb material is at hand.
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

			// Enable the icon if the bomb-throwing skill is learned and a bomb is at hand.
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
			// Restore, added by hand
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
	//					Vampire
	//
	//-----------------------------------------------------
	case RACE_VAMPIRE:
		{		
			//-----------------------------------------------------
			//
			// Walk the skill tree
			//
			//-----------------------------------------------------
			// [new skill 3] In a casket the only skill is opening it.
			if (g_pPlayer->IsInCasket())
			{
				flag = FLAG_SKILL_ENABLE;

				insert(SKILLID_MAP::value_type( MAGIC_OPEN_CASKET, SKILLID_NODE(MAGIC_OPEN_CASKET, flag) ));
				gC_vs_ui.SelectSkill( MAGIC_OPEN_CASKET );

				return;
			}
			
			// Only in vampire form: not as a bat or a wolf.
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

			#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags inside the inventory
				MItem* pSubInventory = NULL;
			#endif

				while (vampireDomain.IsNotEnd())
				{
					MSkillDomain::SKILLSTATUS	status	= vampireDomain.GetSkillStatus();

					// Learned
					if (status == MSkillDomain::SKILLSTATUS_LEARNED)
					{
						ACTIONINFO id = vampireDomain.GetSkillID();

						// Invisibility is not allowed in the lair zones.
						if (g_pSkillInfoTable->GetVampireConsumeMP(id, playerINT) > playerMP
							|| (id == MAGIC_INVISIBILITY && (g_pZone->GetID() == 1104 || g_pZone->GetID() == 1106 || g_pZone->GetID() == 1114 || g_pZone->GetID() == 1115)))
						{
							flag = 0;
						}
						else
						{
							flag = FLAG_SKILL_ENABLE;					
						}

						// Skills that need an item
						switch (id)
						{
							case MAGIC_BLOODY_MARK :
							{
								MVampirePortalItemFinder finder(false);
							#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags inside the inventory
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
							#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags inside the inventory
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

							#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags inside the inventory
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

					// Next
					vampireDomain.Next();
				}
			}

			//-----------------------------------------------------
			//
			// Search the inventory
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

				// Next
				g_pInventory->Next();
			}
			*/

			//-----------------------------------------------------
			//
			// Basic skills
			//
			//-----------------------------------------------------
			// Blood drain: a wolf or a bat cannot drain.
			

			//-----------------------------------------------------
			// Ground attack (pillar of fire), added by hand
			//-----------------------------------------------------
			if (g_pUserInformation->HasMagicGroundAttack)
			{
				if (g_pSkillInfoTable->GetVampireConsumeMP(MAGIC_GROUND_ATTACK, playerINT) > playerMP)
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
			// Bloody snake, added by hand
			//-----------------------------------------------------
			if (g_pUserInformation->HasMagicBloodySnake)
			{
				if (g_pSkillInfoTable->GetVampireConsumeMP(MAGIC_BLOODY_SNAKE, playerINT) > playerMP)
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
			// Bloody warp, added by hand
			//-----------------------------------------------------
			if (g_pUserInformation->HasMagicBloodyWarp)
			{
				if (g_pSkillInfoTable->GetVampireConsumeMP(MAGIC_BLOODY_WARP, playerINT) > playerMP)
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
			// Walk the skill tree
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

					// Learned
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
								// The item in hand
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

					#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9: bags inside the inventory
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

					// Next
					oustersDomain.Next();
				}
			}

			//-----------------------------------------------------
			//
			// Basic skills
			//
			//-----------------------------------------------------
//			// Blood drain: a wolf or a bat cannot drain.
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
	// The Blood Bible bonuses, added by hand
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
		
		if (pGear != NULL)
		{
			MItem *pItem = pGear->FindItem( itemFinder );
			if(pItem != NULL)
			{
				insert(SKILLID_MAP::value_type( (ACTIONINFO)SKILL_LOVE_CHAIN, SKILLID_NODE((ACTIONINFO)(SKILL_LOVE_CHAIN), pItem->IsAffectStatus() ) ));
			}
		}
	}
	
//	insert(SKILLID_MAP::value_type( (ACTIONINFO)SKILL_MAGIC_ELUSION, SKILLID_NODE((ACTIONINFO)(SKILL_MAGIC_ELUSION),  FLAG_SKILL_ENABLE)) );

	g_pPlayer->CalculateLightSight();
	

	//CheckMP();

	// ResetHotKey goes wrong when called with no skills.
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

		// Under EFFECTSTATUS_SACRIFICE each HP counts as 2 MP.
		if (g_pPlayer->HasEffectStatus(EFFECTSTATUS_SACRIFICE))
		{
			playerMP += (g_pPlayer->GetHP() << 1);
		}		
	}
	else
	{
		// A vampire spends HP instead of MP.
		playerMP = g_pPlayer->GetHP();	
	}

	// What a vampire's skill costs takes its current INT (GetVampireConsumeMP).
	const int playerINT = (int)g_pPlayer->GetINT();

	if (PlayerCreatureType!=CREATURETYPE_BAT
		&& PlayerCreatureType!=CREATURETYPE_WOLF
		&& PlayerCreatureType!=CREATURETYPE_WER_WOLF
		&& PlayerCreatureType!=CREATURETYPE_INSTALL_TURRET)
	{
		if (g_pSkillInfoTable->GetVampireConsumeMP(SKILL_BLOOD_DRAIN, playerINT) > playerMP)
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
	// Invisible?
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
			if (g_pSkillInfoTable->GetVampireConsumeMP(MAGIC_EAT_CORPSE, playerINT) > playerMP)
			{
				flag = 0;
			}
			else
			{
				flag = FLAG_SKILL_ENABLE;
			}
			insert(SKILLID_MAP::value_type( MAGIC_EAT_CORPSE, SKILLID_NODE(MAGIC_EAT_CORPSE, flag) ));
			
			// Howl
			if( vampireDomain.GetSkillStatus( MAGIC_HOWL ) == MSkillDomain::SKILLSTATUS_LEARNED )
			{
				if( g_pSkillInfoTable->GetVampireConsumeMP(MAGIC_HOWL, playerINT) > playerMP)				
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
			if( g_pSkillInfoTable->GetVampireConsumeMP(SKILL_BITE_OF_DEATH, playerINT) > playerMP )
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
				if( g_pSkillInfoTable->GetVampireConsumeMP(MAGIC_RAPID_GLIDING, playerINT) > playerMP )
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
	// Transformed?
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
// Checks the MP cost of the selected skills to decide whether each one
// is usable.
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
