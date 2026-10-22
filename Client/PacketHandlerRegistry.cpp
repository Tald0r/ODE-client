//////////////////////////////////////////////////////////////////////////////
// Filename    : PacketHandlerRegistry.cpp
// Description : see PacketHandlerRegistry.h. One DE_REGISTER line per
//               migrated packet; a packet whose execute() has been
//               deleted (ratchet R2) MUST be registered here, or
//               receiving it throws InvalidProtocolException from the
//               Packet base - a mis-migration surfaces as a disconnect
//               in the very first live run, not as a silent no-op.
//
//               The CG and CL directions (the packets the client only
//               writes) are deliberately absent, with one exception
//               below. Honest basis for that (the adversarial review
//               corrected an earlier claim that the validator gates
//               them): PacketValidator does NOT reliably reject their
//               ids - CPS_NORMAL is effectively accept-anything and
//               Player::processCommand has no validator at all - but
//               the server never sends them, and their deleted
//               execute() bodies were empty on the client (server-side
//               code behind #ifndef __GAME_CLIENT__, or calls into the
//               no-op stubs that lived in the deleted
//               CGHandlersStub.cpp). So the only behavior change is
//               for a protocol-violating peer: such a packet used to
//               be silently swallowed and now disconnects - the same
//               deliberate trade the server repo made in its task 2.3.
//
//               The exception is CGConnectSetKey: the client itself
//               constructs one and calls the dispatcher on the login
//               paths (UIMessageManager.cpp), so it is registered as
//               the no-op its stub always was.
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "PacketHandlerRegistry.h"

#include "PacketDispatcher.h"
#include "PacketDiagnostics.h"

#include "Cpackets/CGConnectSetKey.h"

#include "Gpackets/GCActiveGuildList.h"
#include "Gpackets/GCAddBat.h"
#include "Gpackets/GCAddBurrowingCreature.h"
#include "Gpackets/GCAddEffect.h"
#include "Gpackets/GCAddEffectToTile.h"
#include "Gpackets/GCAddGearToInventory.h"
#include "Gpackets/GCAddGearToZone.h"
#include "Gpackets/GCAddHelicopter.h"
#include "Gpackets/GCAddInjuriousCreature.h"
#include "Gpackets/GCAddInstalledMineToZone.h"
#include "Gpackets/GCAddItemToItemVerify.h"
#include "Gpackets/GCAddMonster.h"
#include "Gpackets/GCAddMonsterCorpse.h"
#include "Gpackets/GCAddMonsterFromBurrowing.h"
#include "Gpackets/GCAddMonsterFromTransformation.h"
#include "Gpackets/GCAddNPC.h"
#include "Gpackets/GCAddNewItemToZone.h"
#include "Gpackets/GCAddNickname.h"
#include "Gpackets/GCAddOusters.h"
#include "Gpackets/GCAddOustersCorpse.h"
#include "Gpackets/GCAddSlayer.h"
#include "Gpackets/GCAddSlayerCorpse.h"
#include "Gpackets/GCAddStoreItem.h"
#include "Gpackets/GCAddVampire.h"
#include "Gpackets/GCAddVampireCorpse.h"
#include "Gpackets/GCAddVampireFromBurrowing.h"
#include "Gpackets/GCAddVampireFromTransformation.h"
#include "Gpackets/GCAddVampirePortal.h"
#include "Gpackets/GCAddWolf.h"
#include "Gpackets/GCAddressListVerify.h"
#include "Gpackets/GCAttack.h"
#include "Gpackets/GCAttackArmsOK1.h"
#include "Gpackets/GCAttackArmsOK2.h"
#include "Gpackets/GCAttackArmsOK3.h"
#include "Gpackets/GCAttackArmsOK4.h"
#include "Gpackets/GCAttackArmsOK5.h"
#include "Gpackets/GCAttackMeleeOK1.h"
#include "Gpackets/GCAttackMeleeOK2.h"
#include "Gpackets/GCAttackMeleeOK3.h"
#include "Gpackets/GCAuthKey.h"
#include "Gpackets/GCBloodBibleList.h"
#include "Gpackets/GCBloodBibleSignInfo.h"
#include "Gpackets/GCBloodBibleStatus.h"
#include "Gpackets/GCBloodDrainOK1.h"
#include "Gpackets/GCBloodDrainOK2.h"
#include "Gpackets/GCBloodDrainOK3.h"
#include "Gpackets/GCCannotAdd.h"
#include "Gpackets/GCCannotUse.h"
#include "Gpackets/GCCastingSkill.h"
#include "Gpackets/GCChangeDarkLight.h"
#include "Gpackets/GCChangeShape.h"
#include "Gpackets/GCChangeWeather.h"
#include "Gpackets/GCCreateItem.h"
#include "Gpackets/GCCreatureDied.h"
#include "Gpackets/GCCrossCounterOK1.h"
#include "Gpackets/GCCrossCounterOK2.h"
#include "Gpackets/GCCrossCounterOK3.h"
#include "Gpackets/GCDeleteEffectFromTile.h"
#include "Gpackets/GCDeleteInventoryItem.h"
#include "Gpackets/GCDeleteObject.h"
#include "Gpackets/GCDeleteandPickUpOK.h"
#include "Gpackets/GCDisconnect.h"
#include "Gpackets/GCDownSkillFailed.h"
#include "Gpackets/GCDownSkillOK.h"
#include "Gpackets/GCDropItemToZone.h"
#include "Gpackets/GCEnterVampirePortal.h"
#include "Gpackets/GCExchangeBuy.h"
#include "Gpackets/GCExchangeList.h"
#include "Gpackets/GCExecuteElement.h"
#include "Gpackets/GCFakeMove.h"
#include "Gpackets/GCFastMove.h"
#include "Gpackets/GCFlagWarStatus.h"
#include "Gpackets/GCFriendChatting.h"
#include "Gpackets/GCGQuestInventory.h"
#include "Gpackets/GCGQuestStatusInfo.h"
#include "Gpackets/GCGQuestStatusModify.h"
#include "Gpackets/GCGetDamage.h"
#include "Gpackets/GCGetOffMotorCycle.h"
#include "Gpackets/GCGetOffMotorCycleFailed.h"
#include "Gpackets/GCGetOffMotorCycleOK.h"
#include "Gpackets/GCGlobalChat.h"
#include "Gpackets/GCGoodsList.h"
#include "Gpackets/GCGuildChat.h"
#include "Gpackets/GCGuildMemberList.h"
#include "Gpackets/GCGuildResponse.h"
#include "Gpackets/GCHPRecoveryEndToOthers.h"
#include "Gpackets/GCHPRecoveryEndToSelf.h"
#include "Gpackets/GCHPRecoveryStartToOthers.h"
#include "Gpackets/GCHPRecoveryStartToSelf.h"
#include "Gpackets/GCHolyLandBonusInfo.h"
#include "Gpackets/GCKickMessage.h"
#include "Gpackets/GCKnockBack.h"
#include "Gpackets/GCKnocksTargetBackOK1.h"
#include "Gpackets/GCKnocksTargetBackOK2.h"
#include "Gpackets/GCKnocksTargetBackOK4.h"
#include "Gpackets/GCKnocksTargetBackOK5.h"
#include "Gpackets/GCLearnSkillFailed.h"
#include "Gpackets/GCLearnSkillOK.h"
#include "Gpackets/GCLearnSkillReady.h"
#include "Gpackets/GCLightning.h"
#include "Gpackets/GCMPRecoveryEnd.h"
#include "Gpackets/GCMPRecoveryStart.h"
#include "Gpackets/GCMakeItemFail.h"
#include "Gpackets/GCMakeItemOK.h"
#include "Gpackets/GCMineExplosionOK1.h"
#include "Gpackets/GCMineExplosionOK2.h"
#include "Gpackets/GCMiniGameScores.h"
#include "Gpackets/GCModifyGuildMemberInfo.h"
#include "Gpackets/GCModifyInformation.h"
#include "Gpackets/GCModifyNickname.h"
#include "Gpackets/GCMonsterKillQuestInfo.h"
#include "Gpackets/GCMorph1.h"
#include "Gpackets/GCMorphSlayer2.h"
#include "Gpackets/GCMorphVampire2.h"
#include "Gpackets/GCMove.h"
#include "Gpackets/GCMoveError.h"
#include "Gpackets/GCMoveOK.h"
#include "Gpackets/GCMyStoreInfo.h"
#include "Gpackets/GCNPCAsk.h"
#include "Gpackets/GCNPCAskDynamic.h"
#include "Gpackets/GCNPCAskVariable.h"
#include "Gpackets/GCNPCInfo.h"
#include "Gpackets/GCNPCResponse.h"
#include "Gpackets/GCNPCSay.h"
#include "Gpackets/GCNPCSayDynamic.h"
#include "Gpackets/GCNicknameList.h"
#include "Gpackets/GCNicknameVerify.h"
#include "Gpackets/GCNoticeEvent.h"
#include "Gpackets/GCNotifyWin.h"
#include "Gpackets/GCOtherGuildName.h"
#include "Gpackets/GCOtherModifyInfo.h"
#include "Gpackets/GCOtherStoreInfo.h"
#include "Gpackets/GCPartyError.h"
#include "Gpackets/GCPartyInvite.h"
#include "Gpackets/GCPartyJoined.h"
#include "Gpackets/GCPartyLeave.h"
#include "Gpackets/GCPartyPosition.h"
#include "Gpackets/GCPartySay.h"
#include "Gpackets/GCPetInfo.h"
#include "Gpackets/GCPetStashList.h"
#include "Gpackets/GCPetStashVerify.h"
#include "Gpackets/GCPetUseSkill.h"
#include "Gpackets/GCPhoneConnected.h"
#include "Gpackets/GCPhoneConnectionFailed.h"
#include "Gpackets/GCPhoneDisconnected.h"
#include "Gpackets/GCPhoneSay.h"
#include "Gpackets/GCQuestStatus.h"
#include "Gpackets/GCRankBonusInfo.h"
#include "Gpackets/GCRealWearingInfo.h"
#include "Gpackets/GCReconnect.h"
#include "Gpackets/GCReconnectLogin.h"
#include "Gpackets/GCRegenZoneStatus.h"
#include "Gpackets/GCReloadOK.h"
#include "Gpackets/GCRemoveCorpseHead.h"
#include "Gpackets/GCRemoveEffect.h"
#include "Gpackets/GCRemoveFromGear.h"
#include "Gpackets/GCRemoveInjuriousCreature.h"
#include "Gpackets/GCRemoveStoreItem.h"
#include "Gpackets/GCRequestFailed.h"
#include "Gpackets/GCRequestPowerPointResult.h"
#include "Gpackets/GCRequestedIP.h"
#include "Gpackets/GCRideMotorCycle.h"
#include "Gpackets/GCRideMotorCycleFailed.h"
#include "Gpackets/GCRideMotorCycleOK.h"
#include "Gpackets/GCRing.h"
#include "Gpackets/GCSMSAddressList.h"
#include "Gpackets/GCSay.h"
#include "Gpackets/GCSearchMotorcycleFail.h"
#include "Gpackets/GCSearchMotorcycleOK.h"
#include "Gpackets/GCSelectQuestID.h"
#include "Gpackets/GCSelectRankBonusFailed.h"
#include "Gpackets/GCSelectRankBonusOK.h"
#include "Gpackets/GCSetPosition.h"
#include "Gpackets/GCShopBought.h"
#include "Gpackets/GCShopBuyFail.h"
#include "Gpackets/GCShopBuyOK.h"
#include "Gpackets/GCShopList.h"
#include "Gpackets/GCShopListMysterious.h"
#include "Gpackets/GCShopMarketCondition.h"
#include "Gpackets/GCShopSellFail.h"
#include "Gpackets/GCShopSellOK.h"
#include "Gpackets/GCShopSold.h"
#include "Gpackets/GCShopVersion.h"
#include "Gpackets/GCShowGuildInfo.h"
#include "Gpackets/GCShowGuildJoin.h"
#include "Gpackets/GCShowGuildMemberInfo.h"
#include "Gpackets/GCShowMessageBox.h"
#include "Gpackets/GCShowUnionInfo.h"
#include "Gpackets/GCShowWaitGuildInfo.h"
#include "Gpackets/GCSkillFailed1.h"
#include "Gpackets/GCSkillFailed2.h"
#include "Gpackets/GCSkillInfo.h"
#include "Gpackets/GCSkillToInventoryOK1.h"
#include "Gpackets/GCSkillToInventoryOK2.h"
#include "Gpackets/GCSkillToObjectOK1.h"
#include "Gpackets/GCSkillToObjectOK2.h"
#include "Gpackets/GCSkillToObjectOK3.h"
#include "Gpackets/GCSkillToObjectOK4.h"
#include "Gpackets/GCSkillToObjectOK5.h"
#include "Gpackets/GCSkillToObjectOK6.h"
#include "Gpackets/GCSkillToSelfOK1.h"
#include "Gpackets/GCSkillToSelfOK2.h"
#include "Gpackets/GCSkillToSelfOK3.h"
#include "Gpackets/GCSkillToTileOK1.h"
#include "Gpackets/GCSkillToTileOK2.h"
#include "Gpackets/GCSkillToTileOK3.h"
#include "Gpackets/GCSkillToTileOK4.h"
#include "Gpackets/GCSkillToTileOK5.h"
#include "Gpackets/GCSkillToTileOK6.h"
#include "Gpackets/GCStashList.h"
#include "Gpackets/GCStashSell.h"
#include "Gpackets/GCStatusCurrentHP.h"
#include "Gpackets/GCSubInventoryInfo.h"
#include "Gpackets/GCSweeperBonusInfo.h"
#include "Gpackets/GCSystemAvailabilities.h"
#include "Gpackets/GCSystemMessage.h"
#include "Gpackets/GCTakeOff.h"
#include "Gpackets/GCTakeOutFail.h"
#include "Gpackets/GCTakeOutOK.h"
#include "Gpackets/GCTeachSkillInfo.h"
#include "Gpackets/GCThrowBombOK1.h"
#include "Gpackets/GCThrowBombOK2.h"
#include "Gpackets/GCThrowBombOK3.h"
#include "Gpackets/GCThrowItemOK1.h"
#include "Gpackets/GCThrowItemOK2.h"
#include "Gpackets/GCThrowItemOK3.h"
#include "Gpackets/GCTimeLimitItemInfo.h"
#include "Gpackets/GCTradeAddItem.h"
#include "Gpackets/GCTradeError.h"
#include "Gpackets/GCTradeFinish.h"
#include "Gpackets/GCTradeMoney.h"
#include "Gpackets/GCTradePrepare.h"
#include "Gpackets/GCTradeRemoveItem.h"
#include "Gpackets/GCTradeVerify.h"
#include "Gpackets/GCUnburrowFail.h"
#include "Gpackets/GCUnburrowOK.h"
#include "Gpackets/GCUnionOfferList.h"
#include "Gpackets/GCUntransformFail.h"
#include "Gpackets/GCUntransformOK.h"
#include "Gpackets/GCUpdateInfo.h"
#include "Gpackets/GCUseBonusPointFail.h"
#include "Gpackets/GCUseBonusPointOK.h"
#include "Gpackets/GCUseOK.h"
#include "Gpackets/GCUsePowerPointResult.h"
#include "Gpackets/GCVisibleFail.h"
#include "Gpackets/GCVisibleOK.h"
#include "Gpackets/GCWaitGuildList.h"
#include "Gpackets/GCWarList.h"
#include "Gpackets/GCWarScheduleList.h"
#include "Gpackets/GCWhisper.h"
#include "Gpackets/GCWhisperFailed.h"
#include "Gpackets/GLIncomingConnectionError.h"
#include "Gpackets/GLIncomingConnectionOK.h"
#include "Lpackets/LCCreatePCError.h"
#include "Lpackets/LCCreatePCOK.h"
#include "Lpackets/LCDeletePCError.h"
#include "Lpackets/LCDeletePCOK.h"
#include "Lpackets/LCLoginError.h"
#include "Lpackets/LCLoginOK.h"
#include "Lpackets/LCPCList.h"
#include "Lpackets/LCQueryResultCharacterName.h"
#include "Lpackets/LCQueryResultPlayerID.h"
#include "Lpackets/LCReconnect.h"
#include "Lpackets/LCRegisterPlayerError.h"
#include "Lpackets/LCRegisterPlayerOK.h"
#include "Lpackets/LCSelectPCError.h"
#include "Lpackets/LCServerList.h"
#include "Lpackets/LCVersionCheckError.h"
#include "Lpackets/LCVersionCheckOK.h"
#include "Lpackets/LCWorldList.h"
#include "Lpackets/LGIncomingConnection.h"
#include "Rpackets/CRConnect.h"
#include "Rpackets/CRDisconnect.h"
#include "Rpackets/CRRequest.h"
#include "Rpackets/CRWhisper.h"
#include "Rpackets/RCCharacterInfo.h"
#include "Rpackets/RCPositionInfo.h"
#include "Rpackets/RCSay.h"
#include "Rpackets/RCStatusHP.h"
#include "Upackets/CURequestLoginMode.h"
#include "Upackets/UCRequestLoginMode.h"

// The forwarder that used to live here is gone. It existed because
// SendBugReport was defined in this executable and the wire library
// could not call it, so the library formatted a report and handed it
// back through PacketDiagnostics' hook. SendBugReport moved into the
// library (Client/Packet/WireHost.cpp, docs/reviews/RESTRUCTURING.md task 5.1),
// which turned that into the library calling out to the executable
// only to call straight back in; reportBug goes to it directly now.

void registerClientPacketHandlers()
{
	// The caller is InitSocket() (Client/GameInit.cpp), which runs on
	// every login attempt, so this guard is load-bearing, not defensive
	// decoration: registration on a filled slot throws. The flag is
	// only set once every registration below has succeeded - if one
	// ever throws, the next call retries from a half-filled table and
	// the duplicate registrations then throw loudly at the same spot,
	// rather than the failure being silently permanent.
	static bool bRegistered = false;
	if (bRegistered)
		return;

	//------------------------------------------------------------------
	// Standard delegations: the packet's deleted execute() was exactly
	// Cls##Handler::execute(this, pPlayer) inside __BEGIN_TRY/__END_CATCH.
	// (GCUseSkillCardOK is declared in GCUseOK.h - two packet classes
	// share that header and source file.)
	//------------------------------------------------------------------
	DE_REGISTER_PACKET_HANDLER(CRConnect);
	DE_REGISTER_PACKET_HANDLER(CRDisconnect);
	DE_REGISTER_PACKET_HANDLER(CRRequest);
	DE_REGISTER_PACKET_HANDLER(CRWhisper);
	DE_REGISTER_PACKET_HANDLER(CURequestLoginMode);
	DE_REGISTER_PACKET_HANDLER(GCActiveGuildList);
	DE_REGISTER_PACKET_HANDLER(GCAddBat);
	DE_REGISTER_PACKET_HANDLER(GCAddBurrowingCreature);
	DE_REGISTER_PACKET_HANDLER(GCAddEffect);
	DE_REGISTER_PACKET_HANDLER(GCAddEffectToTile);
	DE_REGISTER_PACKET_HANDLER(GCAddGearToInventory);
	DE_REGISTER_PACKET_HANDLER(GCAddGearToZone);
	DE_REGISTER_PACKET_HANDLER(GCAddHelicopter);
	DE_REGISTER_PACKET_HANDLER(GCAddInjuriousCreature);
	DE_REGISTER_PACKET_HANDLER(GCAddInstalledMineToZone);
	DE_REGISTER_PACKET_HANDLER(GCAddItemToItemVerify);
	DE_REGISTER_PACKET_HANDLER(GCAddMonster);
	DE_REGISTER_PACKET_HANDLER(GCAddMonsterCorpse);
	DE_REGISTER_PACKET_HANDLER(GCAddMonsterFromBurrowing);
	DE_REGISTER_PACKET_HANDLER(GCAddMonsterFromTransformation);
	DE_REGISTER_PACKET_HANDLER(GCAddNPC);
	DE_REGISTER_PACKET_HANDLER(GCAddNewItemToZone);
	DE_REGISTER_PACKET_HANDLER(GCAddNickname);
	DE_REGISTER_PACKET_HANDLER(GCAddOusters);
	DE_REGISTER_PACKET_HANDLER(GCAddOustersCorpse);
	DE_REGISTER_PACKET_HANDLER(GCAddSlayer);
	DE_REGISTER_PACKET_HANDLER(GCAddSlayerCorpse);
	DE_REGISTER_PACKET_HANDLER(GCAddStoreItem);
	DE_REGISTER_PACKET_HANDLER(GCAddVampire);
	DE_REGISTER_PACKET_HANDLER(GCAddVampireCorpse);
	DE_REGISTER_PACKET_HANDLER(GCAddVampireFromBurrowing);
	DE_REGISTER_PACKET_HANDLER(GCAddVampireFromTransformation);
	DE_REGISTER_PACKET_HANDLER(GCAddVampirePortal);
	DE_REGISTER_PACKET_HANDLER(GCAddWolf);
	DE_REGISTER_PACKET_HANDLER(GCAddressListVerify);
	DE_REGISTER_PACKET_HANDLER(GCAttack);
	DE_REGISTER_PACKET_HANDLER(GCAttackArmsOK2);
	DE_REGISTER_PACKET_HANDLER(GCAttackArmsOK3);
	DE_REGISTER_PACKET_HANDLER(GCAttackArmsOK4);
	DE_REGISTER_PACKET_HANDLER(GCAttackArmsOK5);
	DE_REGISTER_PACKET_HANDLER(GCAttackMeleeOK1);
	DE_REGISTER_PACKET_HANDLER(GCAttackMeleeOK2);
	DE_REGISTER_PACKET_HANDLER(GCAttackMeleeOK3);
	DE_REGISTER_PACKET_HANDLER(GCAuthKey);
	DE_REGISTER_PACKET_HANDLER(GCBloodBibleList);
	DE_REGISTER_PACKET_HANDLER(GCBloodBibleSignInfo);
	DE_REGISTER_PACKET_HANDLER(GCBloodBibleStatus);
	DE_REGISTER_PACKET_HANDLER(GCBloodDrainOK1);
	DE_REGISTER_PACKET_HANDLER(GCBloodDrainOK2);
	DE_REGISTER_PACKET_HANDLER(GCBloodDrainOK3);
	DE_REGISTER_PACKET_HANDLER(GCCannotAdd);
	DE_REGISTER_PACKET_HANDLER(GCCannotUse);
	DE_REGISTER_PACKET_HANDLER(GCCastingSkill);
	DE_REGISTER_PACKET_HANDLER(GCChangeDarkLight);
	DE_REGISTER_PACKET_HANDLER(GCChangeShape);
	DE_REGISTER_PACKET_HANDLER(GCChangeWeather);
	DE_REGISTER_PACKET_HANDLER(GCCreateItem);
	DE_REGISTER_PACKET_HANDLER(GCCreatureDied);
	DE_REGISTER_PACKET_HANDLER(GCCrossCounterOK1);
	DE_REGISTER_PACKET_HANDLER(GCCrossCounterOK2);
	DE_REGISTER_PACKET_HANDLER(GCCrossCounterOK3);
	DE_REGISTER_PACKET_HANDLER(GCDeleteEffectFromTile);
	DE_REGISTER_PACKET_HANDLER(GCDeleteInventoryItem);
	DE_REGISTER_PACKET_HANDLER(GCDeleteObject);
	DE_REGISTER_PACKET_HANDLER(GCDeleteandPickUpOK);
	DE_REGISTER_PACKET_HANDLER(GCDisconnect);
	DE_REGISTER_PACKET_HANDLER(GCDownSkillFailed);
	DE_REGISTER_PACKET_HANDLER(GCDownSkillOK);
	DE_REGISTER_PACKET_HANDLER(GCDropItemToZone);
	DE_REGISTER_PACKET_HANDLER(GCEnterVampirePortal);
	DE_REGISTER_PACKET_HANDLER(GCExchangeBuy);
	DE_REGISTER_PACKET_HANDLER(GCExecuteElement);
	DE_REGISTER_PACKET_HANDLER(GCFakeMove);
	DE_REGISTER_PACKET_HANDLER(GCFastMove);
	DE_REGISTER_PACKET_HANDLER(GCFlagWarStatus);
	DE_REGISTER_PACKET_HANDLER(GCFriendChatting);
	DE_REGISTER_PACKET_HANDLER(GCGQuestInventory);
	DE_REGISTER_PACKET_HANDLER(GCGQuestStatusInfo);
	DE_REGISTER_PACKET_HANDLER(GCGQuestStatusModify);
	DE_REGISTER_PACKET_HANDLER(GCGetDamage);
	DE_REGISTER_PACKET_HANDLER(GCGetOffMotorCycle);
	DE_REGISTER_PACKET_HANDLER(GCGetOffMotorCycleFailed);
	DE_REGISTER_PACKET_HANDLER(GCGetOffMotorCycleOK);
	DE_REGISTER_PACKET_HANDLER(GCGlobalChat);
	DE_REGISTER_PACKET_HANDLER(GCGuildChat);
	DE_REGISTER_PACKET_HANDLER(GCGuildMemberList);
	DE_REGISTER_PACKET_HANDLER(GCGuildResponse);
	DE_REGISTER_PACKET_HANDLER(GCHPRecoveryEndToOthers);
	DE_REGISTER_PACKET_HANDLER(GCHPRecoveryEndToSelf);
	DE_REGISTER_PACKET_HANDLER(GCHPRecoveryStartToOthers);
	DE_REGISTER_PACKET_HANDLER(GCHPRecoveryStartToSelf);
	DE_REGISTER_PACKET_HANDLER(GCHolyLandBonusInfo);
	DE_REGISTER_PACKET_HANDLER(GCKickMessage);
	DE_REGISTER_PACKET_HANDLER(GCKnockBack);
	DE_REGISTER_PACKET_HANDLER(GCKnocksTargetBackOK2);
	DE_REGISTER_PACKET_HANDLER(GCKnocksTargetBackOK4);
	DE_REGISTER_PACKET_HANDLER(GCKnocksTargetBackOK5);
	DE_REGISTER_PACKET_HANDLER(GCLearnSkillFailed);
	DE_REGISTER_PACKET_HANDLER(GCLearnSkillOK);
	DE_REGISTER_PACKET_HANDLER(GCLearnSkillReady);
	DE_REGISTER_PACKET_HANDLER(GCLightning);
	DE_REGISTER_PACKET_HANDLER(GCMPRecoveryEnd);
	DE_REGISTER_PACKET_HANDLER(GCMPRecoveryStart);
	DE_REGISTER_PACKET_HANDLER(GCMakeItemFail);
	DE_REGISTER_PACKET_HANDLER(GCMakeItemOK);
	DE_REGISTER_PACKET_HANDLER(GCMineExplosionOK1);
	DE_REGISTER_PACKET_HANDLER(GCMineExplosionOK2);
	DE_REGISTER_PACKET_HANDLER(GCMiniGameScores);
	DE_REGISTER_PACKET_HANDLER(GCModifyGuildMemberInfo);
	DE_REGISTER_PACKET_HANDLER(GCModifyInformation);
	DE_REGISTER_PACKET_HANDLER(GCModifyNickname);
	DE_REGISTER_PACKET_HANDLER(GCMonsterKillQuestInfo);
	DE_REGISTER_PACKET_HANDLER(GCMorph1);
	DE_REGISTER_PACKET_HANDLER(GCMorphSlayer2);
	DE_REGISTER_PACKET_HANDLER(GCMorphVampire2);
	DE_REGISTER_PACKET_HANDLER(GCMove);
	DE_REGISTER_PACKET_HANDLER(GCMoveError);
	DE_REGISTER_PACKET_HANDLER(GCMoveOK);
	DE_REGISTER_PACKET_HANDLER(GCMyStoreInfo);
	DE_REGISTER_PACKET_HANDLER(GCNPCAsk);
	DE_REGISTER_PACKET_HANDLER(GCNPCAskDynamic);
	DE_REGISTER_PACKET_HANDLER(GCNPCAskVariable);
	DE_REGISTER_PACKET_HANDLER(GCNPCInfo);
	DE_REGISTER_PACKET_HANDLER(GCNPCResponse);
	DE_REGISTER_PACKET_HANDLER(GCNPCSay);
	DE_REGISTER_PACKET_HANDLER(GCNPCSayDynamic);
	DE_REGISTER_PACKET_HANDLER(GCNicknameList);
	DE_REGISTER_PACKET_HANDLER(GCNicknameVerify);
	DE_REGISTER_PACKET_HANDLER(GCNoticeEvent);
	DE_REGISTER_PACKET_HANDLER(GCNotifyWin);
	DE_REGISTER_PACKET_HANDLER(GCOtherGuildName);
	DE_REGISTER_PACKET_HANDLER(GCOtherModifyInfo);
	DE_REGISTER_PACKET_HANDLER(GCOtherStoreInfo);
	DE_REGISTER_PACKET_HANDLER(GCPartyError);
	DE_REGISTER_PACKET_HANDLER(GCPartyInvite);
	DE_REGISTER_PACKET_HANDLER(GCPartyJoined);
	DE_REGISTER_PACKET_HANDLER(GCPartyLeave);
	DE_REGISTER_PACKET_HANDLER(GCPartyPosition);
	DE_REGISTER_PACKET_HANDLER(GCPartySay);
	DE_REGISTER_PACKET_HANDLER(GCPetInfo);
	DE_REGISTER_PACKET_HANDLER(GCPetStashVerify);
	DE_REGISTER_PACKET_HANDLER(GCPetUseSkill);
	DE_REGISTER_PACKET_HANDLER(GCPhoneConnected);
	DE_REGISTER_PACKET_HANDLER(GCPhoneConnectionFailed);
	DE_REGISTER_PACKET_HANDLER(GCPhoneDisconnected);
	DE_REGISTER_PACKET_HANDLER(GCPhoneSay);
	DE_REGISTER_PACKET_HANDLER(GCQuestStatus);
	DE_REGISTER_PACKET_HANDLER(GCRankBonusInfo);
	DE_REGISTER_PACKET_HANDLER(GCRealWearingInfo);
	DE_REGISTER_PACKET_HANDLER(GCReconnect);
	DE_REGISTER_PACKET_HANDLER(GCReconnectLogin);
	DE_REGISTER_PACKET_HANDLER(GCRegenZoneStatus);
	DE_REGISTER_PACKET_HANDLER(GCReloadOK);
	DE_REGISTER_PACKET_HANDLER(GCRemoveCorpseHead);
	DE_REGISTER_PACKET_HANDLER(GCRemoveEffect);
	DE_REGISTER_PACKET_HANDLER(GCRemoveFromGear);
	DE_REGISTER_PACKET_HANDLER(GCRemoveInjuriousCreature);
	DE_REGISTER_PACKET_HANDLER(GCRemoveStoreItem);
	DE_REGISTER_PACKET_HANDLER(GCRequestFailed);
	DE_REGISTER_PACKET_HANDLER(GCRequestPowerPointResult);
	DE_REGISTER_PACKET_HANDLER(GCRequestedIP);
	DE_REGISTER_PACKET_HANDLER(GCRideMotorCycle);
	DE_REGISTER_PACKET_HANDLER(GCRideMotorCycleFailed);
	DE_REGISTER_PACKET_HANDLER(GCRideMotorCycleOK);
	DE_REGISTER_PACKET_HANDLER(GCRing);
	DE_REGISTER_PACKET_HANDLER(GCSMSAddressList);
	DE_REGISTER_PACKET_HANDLER(GCSay);
	DE_REGISTER_PACKET_HANDLER(GCSearchMotorcycleFail);
	DE_REGISTER_PACKET_HANDLER(GCSearchMotorcycleOK);
	DE_REGISTER_PACKET_HANDLER(GCSelectQuestID);
	DE_REGISTER_PACKET_HANDLER(GCSelectRankBonusFailed);
	DE_REGISTER_PACKET_HANDLER(GCSelectRankBonusOK);
	DE_REGISTER_PACKET_HANDLER(GCSetPosition);
	DE_REGISTER_PACKET_HANDLER(GCShopBought);
	DE_REGISTER_PACKET_HANDLER(GCShopBuyFail);
	DE_REGISTER_PACKET_HANDLER(GCShopBuyOK);
	DE_REGISTER_PACKET_HANDLER(GCShopList);
	DE_REGISTER_PACKET_HANDLER(GCShopListMysterious);
	DE_REGISTER_PACKET_HANDLER(GCShopMarketCondition);
	DE_REGISTER_PACKET_HANDLER(GCShopSellFail);
	DE_REGISTER_PACKET_HANDLER(GCShopSellOK);
	DE_REGISTER_PACKET_HANDLER(GCShopSold);
	DE_REGISTER_PACKET_HANDLER(GCShopVersion);
	DE_REGISTER_PACKET_HANDLER(GCShowGuildInfo);
	DE_REGISTER_PACKET_HANDLER(GCShowGuildJoin);
	DE_REGISTER_PACKET_HANDLER(GCShowGuildMemberInfo);
	DE_REGISTER_PACKET_HANDLER(GCShowMessageBox);
	DE_REGISTER_PACKET_HANDLER(GCShowUnionInfo);
	DE_REGISTER_PACKET_HANDLER(GCShowWaitGuildInfo);
	DE_REGISTER_PACKET_HANDLER(GCSkillFailed1);
	DE_REGISTER_PACKET_HANDLER(GCSkillFailed2);
	DE_REGISTER_PACKET_HANDLER(GCSkillInfo);
	DE_REGISTER_PACKET_HANDLER(GCSkillToInventoryOK1);
	DE_REGISTER_PACKET_HANDLER(GCSkillToInventoryOK2);
	DE_REGISTER_PACKET_HANDLER(GCSkillToObjectOK1);
	DE_REGISTER_PACKET_HANDLER(GCSkillToObjectOK2);
	DE_REGISTER_PACKET_HANDLER(GCSkillToObjectOK3);
	DE_REGISTER_PACKET_HANDLER(GCSkillToObjectOK4);
	DE_REGISTER_PACKET_HANDLER(GCSkillToObjectOK5);
	DE_REGISTER_PACKET_HANDLER(GCSkillToObjectOK6);
	DE_REGISTER_PACKET_HANDLER(GCSkillToSelfOK1);
	DE_REGISTER_PACKET_HANDLER(GCSkillToSelfOK2);
	DE_REGISTER_PACKET_HANDLER(GCSkillToSelfOK3);
	DE_REGISTER_PACKET_HANDLER(GCSkillToTileOK1);
	DE_REGISTER_PACKET_HANDLER(GCSkillToTileOK2);
	DE_REGISTER_PACKET_HANDLER(GCSkillToTileOK3);
	DE_REGISTER_PACKET_HANDLER(GCSkillToTileOK4);
	DE_REGISTER_PACKET_HANDLER(GCSkillToTileOK5);
	DE_REGISTER_PACKET_HANDLER(GCSkillToTileOK6);
	DE_REGISTER_PACKET_HANDLER(GCStatusCurrentHP);
	DE_REGISTER_PACKET_HANDLER(GCSubInventoryInfo);
	DE_REGISTER_PACKET_HANDLER(GCSweeperBonusInfo);
	DE_REGISTER_PACKET_HANDLER(GCSystemAvailabilities);
	DE_REGISTER_PACKET_HANDLER(GCSystemMessage);
	DE_REGISTER_PACKET_HANDLER(GCTakeOff);
	DE_REGISTER_PACKET_HANDLER(GCTakeOutFail);
	DE_REGISTER_PACKET_HANDLER(GCTakeOutOK);
	DE_REGISTER_PACKET_HANDLER(GCTeachSkillInfo);
	DE_REGISTER_PACKET_HANDLER(GCThrowBombOK1);
	DE_REGISTER_PACKET_HANDLER(GCThrowBombOK2);
	DE_REGISTER_PACKET_HANDLER(GCThrowBombOK3);
	DE_REGISTER_PACKET_HANDLER(GCThrowItemOK1);
	DE_REGISTER_PACKET_HANDLER(GCThrowItemOK2);
	DE_REGISTER_PACKET_HANDLER(GCThrowItemOK3);
	DE_REGISTER_PACKET_HANDLER(GCTimeLimitItemInfo);
	DE_REGISTER_PACKET_HANDLER(GCTradeAddItem);
	DE_REGISTER_PACKET_HANDLER(GCTradeError);
	DE_REGISTER_PACKET_HANDLER(GCTradeFinish);
	DE_REGISTER_PACKET_HANDLER(GCTradeMoney);
	DE_REGISTER_PACKET_HANDLER(GCTradePrepare);
	DE_REGISTER_PACKET_HANDLER(GCTradeRemoveItem);
	DE_REGISTER_PACKET_HANDLER(GCTradeVerify);
	DE_REGISTER_PACKET_HANDLER(GCUnburrowFail);
	DE_REGISTER_PACKET_HANDLER(GCUnburrowOK);
	DE_REGISTER_PACKET_HANDLER(GCUnionOfferList);
	DE_REGISTER_PACKET_HANDLER(GCUntransformFail);
	DE_REGISTER_PACKET_HANDLER(GCUntransformOK);
	DE_REGISTER_PACKET_HANDLER(GCUpdateInfo);
	DE_REGISTER_PACKET_HANDLER(GCUseBonusPointFail);
	DE_REGISTER_PACKET_HANDLER(GCUseBonusPointOK);
	DE_REGISTER_PACKET_HANDLER(GCUseOK);
	DE_REGISTER_PACKET_HANDLER(GCUsePowerPointResult);
	DE_REGISTER_PACKET_HANDLER(GCUseSkillCardOK);
	DE_REGISTER_PACKET_HANDLER(GCVisibleFail);
	DE_REGISTER_PACKET_HANDLER(GCVisibleOK);
	DE_REGISTER_PACKET_HANDLER(GCWaitGuildList);
	DE_REGISTER_PACKET_HANDLER(GCWarList);
	DE_REGISTER_PACKET_HANDLER(GCWarScheduleList);
	DE_REGISTER_PACKET_HANDLER(GCWhisper);
	DE_REGISTER_PACKET_HANDLER(GCWhisperFailed);
	DE_REGISTER_PACKET_HANDLER(LCCreatePCError);
	DE_REGISTER_PACKET_HANDLER(LCCreatePCOK);
	DE_REGISTER_PACKET_HANDLER(LCDeletePCError);
	DE_REGISTER_PACKET_HANDLER(LCDeletePCOK);
	DE_REGISTER_PACKET_HANDLER(LCLoginError);
	DE_REGISTER_PACKET_HANDLER(LCLoginOK);
	DE_REGISTER_PACKET_HANDLER(LCPCList);
	DE_REGISTER_PACKET_HANDLER(LCQueryResultCharacterName);
	DE_REGISTER_PACKET_HANDLER(LCQueryResultPlayerID);
	DE_REGISTER_PACKET_HANDLER(LCReconnect);
	DE_REGISTER_PACKET_HANDLER(LCRegisterPlayerError);
	DE_REGISTER_PACKET_HANDLER(LCRegisterPlayerOK);
	DE_REGISTER_PACKET_HANDLER(LCSelectPCError);
	DE_REGISTER_PACKET_HANDLER(LCServerList);
	DE_REGISTER_PACKET_HANDLER(LCVersionCheckError);
	DE_REGISTER_PACKET_HANDLER(LCVersionCheckOK);
	DE_REGISTER_PACKET_HANDLER(LCWorldList);
	DE_REGISTER_PACKET_HANDLER(UCRequestLoginMode);

	//------------------------------------------------------------------
	// Handlers that take only the packet (the datagram-flavored
	// connection packets): deleted execute() called Handler::execute(this).
	//
	// CRRequest2, a dead duplicate class that claimed the same
	// PACKET_CR_REQUEST id as CRRequest (registered above), was deleted
	// with task 2.5: the wire-layout test now constructs every factory
	// and asserts the ids are unique, which it broke.
	//------------------------------------------------------------------
	DE_REGISTER_PACKET_HANDLER_NOPLAYER(GLIncomingConnectionOK);
	DE_REGISTER_PACKET_HANDLER_NOPLAYER(LGIncomingConnection);
	DE_REGISTER_PACKET_HANDLER_NOPLAYER(RCCharacterInfo);
	DE_REGISTER_PACKET_HANDLER_NOPLAYER(RCPositionInfo);
	DE_REGISTER_PACKET_HANDLER_NOPLAYER(RCSay);
	DE_REGISTER_PACKET_HANDLER_NOPLAYER(RCStatusHP);

	//------------------------------------------------------------------
	// These six packets' deleted execute() additionally wrapped the
	// handler in __BEGIN_DEBUG/__END_DEBUG. That pair is a no-op on this
	// build (__WIN32__), but the cout-logging branch exists on other
	// platforms, so the explicit thunks preserve it exactly - the same
	// call the server repo made for its CGStashList.
	//------------------------------------------------------------------
#define DE_REGISTER_PACKET_HANDLER_DEBUG(Cls)                                 \
	{                                                                         \
		struct Thunk {                                                        \
			static void call(Packet* pPacket, Player* pPlayer)                \
			{                                                                 \
				__BEGIN_TRY                                                   \
				__BEGIN_DEBUG                                                 \
				Cls##Handler::execute(static_cast<Cls*>(pPacket), pPlayer);   \
				__END_DEBUG                                                   \
				__END_CATCH                                                   \
			}                                                                 \
		};                                                                    \
		PacketDispatcher::registerHandler(Cls().getPacketID(), &Thunk::call); \
	}

	DE_REGISTER_PACKET_HANDLER_DEBUG(GCAttackArmsOK1);
	DE_REGISTER_PACKET_HANDLER_DEBUG(GCGoodsList);
	DE_REGISTER_PACKET_HANDLER_DEBUG(GCKnocksTargetBackOK1);
	DE_REGISTER_PACKET_HANDLER_DEBUG(GCPetStashList);
	DE_REGISTER_PACKET_HANDLER_DEBUG(GCStashList);
	DE_REGISTER_PACKET_HANDLER_DEBUG(GCStashSell);

#undef DE_REGISTER_PACKET_HANDLER_DEBUG

	//------------------------------------------------------------------
	// GLIncomingConnectionError: its deleted execute() printed a cout
	// trace before delegating (packet-only handler); preserved verbatim.
	//------------------------------------------------------------------
	{
		struct Thunk {
			static void call(Packet* pPacket, Player*)
			{
				__BEGIN_TRY
				cout << "GLIncomingConnectionError::execute() called." << endl;
				GLIncomingConnectionErrorHandler::execute(static_cast<GLIncomingConnectionError*>(pPacket));
				__END_CATCH
			}
		};
		PacketDispatcher::registerHandler(GLIncomingConnectionError().getPacketID(), &Thunk::call);
	}

	//------------------------------------------------------------------
	// GCExchangeList: its deleted execute() was EMPTY on purpose - the
	// exchange UI consumes the parsed packet elsewhere. Registered as an
	// explicit no-op so receiving it stays a silent success instead of
	// becoming a protocol error.
	//------------------------------------------------------------------
	{
		struct Thunk {
			static void call(Packet*, Player*) { }
		};
		PacketDispatcher::registerHandler(GCExchangeList().getPacketID(), &Thunk::call);
	}

	//------------------------------------------------------------------
	// CGConnectSetKey: the client constructs one locally and dispatches
	// it on the login paths (UIMessageManager.cpp:1377, :2105). The
	// linked handler was always the empty stub in the deleted
	// CGHandlersStub.cpp. Transport encryption is absent; preserve
	// the existing no-op dispatch behavior explicitly.
	//------------------------------------------------------------------
	{
		struct Thunk {
			static void call(Packet*, Player*) { }
		};
		PacketDispatcher::registerHandler(CGConnectSetKey().getPacketID(), &Thunk::call);
	}

	bRegistered = true;
}
