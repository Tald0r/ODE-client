#include "Client_PCH.h"
#include "ZoneStateHost.h"
#include "MActionResult.h"
#include "MStatus.h"

namespace ZoneState {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

bool Exists()
{
	return s_Host && s_Host->Exists ? s_Host->Exists() : false;
}

MCreature* GetCreature(TYPE_OBJECTID id)
{
	return s_Host && s_Host->GetCreature ? s_Host->GetCreature(id) : nullptr;
}

bool RemoveTileEffect(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int effectStatus, int serverID)
{
	return s_Host && s_Host->RemoveTileEffect ? s_Host->RemoveTileEffect(sX, sY, effectStatus, serverID) : false;
}

bool AddHelicopter(TYPE_OBJECTID creatureID, int x0, int y0)
{
	return s_Host && s_Host->AddHelicopter ? s_Host->AddHelicopter(creatureID, x0, y0) : false;
}

void RemoveHelicopter(TYPE_OBJECTID creatureID)
{
	if (s_Host && s_Host->RemoveHelicopter) s_Host->RemoveHelicopter(creatureID);
}

bool AddCorpseFromCreature(TYPE_OBJECTID id)
{
	return s_Host && s_Host->AddCorpseFromCreature ? s_Host->AddCorpseFromCreature(id) : false;
}

int GetID()
{
	return s_Host && s_Host->GetID ? s_Host->GetID() : ZONEID_NULL;
}

MCreature* GetCorpseCreature(TYPE_OBJECTID itemID)
{
	return s_Host && s_Host->GetCorpseCreature ? s_Host->GetCorpseCreature(itemID) : nullptr;
}

MItem* GetItem(TYPE_OBJECTID itemID)
{
	return s_Host && s_Host->GetItem ? s_Host->GetItem(itemID) : nullptr;
}

bool RemoveItem(TYPE_OBJECTID itemID)
{
	return s_Host && s_Host->RemoveItem ? s_Host->RemoveItem(itemID) : false;
}

bool AddItem(MItem* pItem, bool bDropping)
{
	return s_Host && s_Host->AddItem ? s_Host->AddItem(pItem, bDropping) : false;
}

bool RemoveCreature(TYPE_OBJECTID id)
{
	return s_Host && s_Host->RemoveCreature ? s_Host->RemoveCreature(id) : false;
}

TYPE_OBJECTID GetCreatureID(const char* pName, int flag)
{
	return s_Host && s_Host->GetCreatureID ? s_Host->GetCreatureID(pName, flag) : OBJECTID_NULL;
}

MCreature* GetFakeCreature(TYPE_OBJECTID id)
{
	return s_Host && s_Host->GetFakeCreature ? s_Host->GetFakeCreature(id) : nullptr;
}

MCreature* NewFakeCreature(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	return s_Host && s_Host->NewFakeCreature ? s_Host->NewFakeCreature(pCreature, x, y) : nullptr;
}

bool AddFakeCreature(MCreature* pFakeCreature)
{
	return s_Host && s_Host->AddFakeCreature && s_Host->AddFakeCreature(pFakeCreature);
}

void DeleteFakeCreature(MCreature* pFakeCreature)
{
	if (s_Host && s_Host->DeleteFakeCreature) s_Host->DeleteFakeCreature(pFakeCreature);
}

void SetGroundElemental(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	if (s_Host && s_Host->SetGroundElemental) s_Host->SetGroundElemental(x, y);
}

void SetDarknessForbidden(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	if (s_Host && s_Host->SetDarknessForbidden) s_Host->SetDarknessForbidden(x, y);
}

void SetFuryOfGnome(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	if (s_Host && s_Host->SetFuryOfGnome) s_Host->SetFuryOfGnome(x, y);
}

MCreature* NewCreature()
{
	return s_Host && s_Host->NewCreature ? s_Host->NewCreature() : nullptr;
}

MCreature* NewCreatureWear()
{
	return s_Host && s_Host->NewCreatureWear ? s_Host->NewCreatureWear() : nullptr;
}

bool AddCreature(MCreature* pCreature)
{
	return s_Host && s_Host->AddCreature && s_Host->AddCreature(pCreature);
}

void DeleteCreature(MCreature* pCreature)
{
	if (s_Host && s_Host->DeleteCreature) s_Host->DeleteCreature(pCreature);
}

MCreature* GetCreatureOnly(TYPE_OBJECTID id)
{
	return s_Host && s_Host->GetCreatureOnly ? s_Host->GetCreatureOnly(id) : NULL;
}

TYPE_OBJECTID GetItemID(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	return s_Host && s_Host->GetItemID ? s_Host->GetItemID(x, y) : OBJECTID_NULL;
}

MItem* NewCorpse(MCreature* pCreature)
{
	return s_Host && s_Host->NewCorpse ? s_Host->NewCorpse(pCreature) : NULL;
}

MCreature* NewNPC()
{
	return s_Host && s_Host->NewNPC ? s_Host->NewNPC() : NULL;
}

void SetPlayer()
{
	if (s_Host && s_Host->SetPlayer) s_Host->SetPlayer();
}

void PickupItem(TYPE_OBJECTID id)
{
	if (s_Host && s_Host->PickupItem) s_Host->PickupItem(id);
}

}

namespace CreatureState {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

MStatus* Status(MCreature* pCreature)
{
	return s_Host && s_Host->Status ? s_Host->Status(pCreature) : nullptr;
}

TYPE_OBJECTID GetID(MCreature* pCreature)
{
	return s_Host && s_Host->GetID ? s_Host->GetID(pCreature) : 0;
}

const char* GetName(MCreature* pCreature)
{
	return s_Host && s_Host->GetName ? s_Host->GetName(pCreature) : "";
}

bool IsAlive(MCreature* pCreature)
{
	return s_Host && s_Host->IsAlive ? s_Host->IsAlive(pCreature) : false;
}

void SetDead(MCreature* pCreature)
{
	if (s_Host && s_Host->SetDead) s_Host->SetDead(pCreature);
}

void StopRecoveryHP(MCreature* pCreature)
{
	if (s_Host && s_Host->StopRecoveryHP) s_Host->StopRecoveryHP(pCreature);
}

void SetRecoveryHP(MCreature* pCreature, int amount, int times, std::uint32_t delay)
{
	if (s_Host && s_Host->SetRecoveryHP) s_Host->SetRecoveryHP(pCreature, amount, times, delay);
}

void SetNickName(MCreature* pCreature, std::uint8_t type, char* szNickName)
{
	if (s_Host && s_Host->SetNickName) s_Host->SetNickName(pCreature, type, szNickName);
}

void SetChatString(MCreature* pCreature, const char* str, std::uint32_t color)
{
	if (s_Host && s_Host->SetChatString) s_Host->SetChatString(pCreature, str, color);
}

void PacketMove(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, std::uint8_t direction)
{
	if (s_Host && s_Host->PacketMove) s_Host->PacketMove(pCreature, x, y, direction);
}

void PacketAttackNormal(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, std::uint8_t direction)
{
	if (s_Host && s_Host->PacketAttackNormal) s_Host->PacketAttackNormal(pCreature, x, y, direction);
}

void SetDirectionToPosition(MCreature* pCreature, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	if (s_Host && s_Host->SetDirectionToPosition) s_Host->SetDirectionToPosition(pCreature, sX, sY);
}

void SetStop(MCreature* pCreature)
{
	if (s_Host && s_Host->SetStop) s_Host->SetStop(pCreature);
}

bool MovePosition(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	return s_Host && s_Host->MovePosition ? s_Host->MovePosition(pCreature, x, y) : false;
}

void SetServerPosition(MCreature* pCreature, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	if (s_Host && s_Host->SetServerPosition) s_Host->SetServerPosition(pCreature, sX, sY);
}

bool KnockBackPosition(MCreature* pCreature, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
{
	return s_Host && s_Host->KnockBackPosition ? s_Host->KnockBackPosition(pCreature, sX, sY) : false;
}

void SetStatus(MCreature* pCreature, std::uint32_t n, std::uint32_t value)
{
	if (MStatus* pStatus = Status(pCreature)) pStatus->SetStatus(n, value);
}

TYPE_SECTORPOSITION GetX(MCreature* pCreature)
{
	return s_Host && s_Host->GetX ? s_Host->GetX(pCreature) : 0;
}

TYPE_SECTORPOSITION GetY(MCreature* pCreature)
{
	return s_Host && s_Host->GetY ? s_Host->GetY(pCreature) : 0;
}

void SetEffectDelayFrame(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, std::uint32_t frames)
{
	if (s_Host && s_Host->SetEffectDelayFrame) s_Host->SetEffectDelayFrame(pCreature, actionInfo, frames);
}

void PacketSpecialActionResult(MCreature* pCreature, TYPE_ACTIONINFO resultActionInfo, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t grade)
{
	if (s_Host && s_Host->PacketSpecialActionResult) s_Host->PacketSpecialActionResult(pCreature, resultActionInfo, id, sX, sY, grade);
}

void PacketSpecialActionToInventoryItem(MCreature* pCreature, TYPE_ACTIONINFO actionInfo)
{
	if (s_Host && s_Host->PacketSpecialActionToInventoryItem) s_Host->PacketSpecialActionToInventoryItem(pCreature, actionInfo);
}

bool AddEffectStatus(MCreature* pCreature, EFFECTSTATUS status, std::uint32_t delayFrame)
{
	return s_Host && s_Host->AddEffectStatus ? s_Host->AddEffectStatus(pCreature, status, delayFrame) : false;
}

void SetTurning(MCreature* pCreature, std::uint16_t count)
{
	if (s_Host && s_Host->SetTurning) s_Host->SetTurning(pCreature, count);
}

bool IsNPC(MCreature* pCreature)
{
	return s_Host && s_Host->IsNPC ? s_Host->IsNPC(pCreature) : false;
}

MShop* GetShop(MCreature* pCreature)
{
	return s_Host && s_Host->GetShop ? s_Host->GetShop(pCreature) : nullptr;
}

void ReleaseMoveBuffer(MCreature* pCreature)
{
	if (s_Host && s_Host->ReleaseMoveBuffer) s_Host->ReleaseMoveBuffer(pCreature);
}

void SetVisible(MCreature* pCreature)
{
	if (s_Host && s_Host->SetVisible) s_Host->SetVisible(pCreature);
}

void SetInvisible(MCreature* pCreature)
{
	if (s_Host && s_Host->SetInvisible) s_Host->SetInvisible(pCreature);
}

bool IsUndergroundCreature(MCreature* pCreature)
{
	return s_Host && s_Host->IsUndergroundCreature ? s_Host->IsUndergroundCreature(pCreature) : false;
}

void SetUndergroundCreature(MCreature* pCreature)
{
	if (s_Host && s_Host->SetUndergroundCreature) s_Host->SetUndergroundCreature(pCreature);
}

void SetFlyingCreature(MCreature* pCreature)
{
	if (s_Host && s_Host->SetFlyingCreature) s_Host->SetFlyingCreature(pCreature);
}

void SetGroundCreature(MCreature* pCreature)
{
	if (s_Host && s_Host->SetGroundCreature) s_Host->SetGroundCreature(pCreature);
}

void SetCreatureType(MCreature* pCreature, int creatureType)
{
	if (s_Host && s_Host->SetCreatureType) s_Host->SetCreatureType(pCreature, creatureType);
}

bool IsMale(MCreature* pCreature)
{
	return s_Host && s_Host->IsMale ? s_Host->IsMale(pCreature) : false;
}

void SetMale(MCreature* pCreature, bool bMale)
{
	if (s_Host && s_Host->SetMale) s_Host->SetMale(pCreature, bMale);
}

std::uint16_t GetBodyColor1(MCreature* pCreature)
{
	return s_Host && s_Host->GetBodyColor1 ? s_Host->GetBodyColor1(pCreature) : 0;
}

std::uint16_t GetBodyColor2(MCreature* pCreature)
{
	return s_Host && s_Host->GetBodyColor2 ? s_Host->GetBodyColor2(pCreature) : 0;
}

void SetBodyColor1(MCreature* pCreature, std::uint16_t colorSet)
{
	if (s_Host && s_Host->SetBodyColor1) s_Host->SetBodyColor1(pCreature, colorSet);
}

void SetBodyColor2(MCreature* pCreature, std::uint16_t colorSet)
{
	if (s_Host && s_Host->SetBodyColor2) s_Host->SetBodyColor2(pCreature, colorSet);
}

TYPE_SECTORPOSITION GetServerX(MCreature* pCreature)
{
	return s_Host && s_Host->GetServerX ? s_Host->GetServerX(pCreature) : 0;
}

TYPE_SECTORPOSITION GetServerY(MCreature* pCreature)
{
	return s_Host && s_Host->GetServerY ? s_Host->GetServerY(pCreature) : 0;
}

bool IsInCasket(MCreature* pCreature)
{
	return s_Host && s_Host->IsInCasket ? s_Host->IsInCasket(pCreature) : false;
}

void AddCasket(MCreature* pCreature, int casketType)
{
	if (s_Host && s_Host->AddCasket) s_Host->AddCasket(pCreature, casketType);
}

void RemoveCasket(MCreature* pCreature)
{
	if (s_Host && s_Host->RemoveCasket) s_Host->RemoveCasket(pCreature);
}

void SetBloodyZenith(MCreature* pCreature, int delay)
{
	if (s_Host && s_Host->SetBloodyZenith) s_Host->SetBloodyZenith(pCreature, delay);
}

void PacketSpecialActionToSector(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, MActionResult* pResult)
{
	if (s_Host && s_Host->PacketSpecialActionToSector) s_Host->PacketSpecialActionToSector(pCreature, actionInfo, sX, sY, pResult);
	else delete pResult;
}

void PacketSpecialActionToOther(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, TYPE_OBJECTID id, MActionResult* pResult)
{
	if (s_Host && s_Host->PacketSpecialActionToOther) s_Host->PacketSpecialActionToOther(pCreature, actionInfo, id, pResult);
	else delete pResult;
}

void PacketSpecialActionToNobody(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	if (s_Host && s_Host->PacketSpecialActionToNobody) s_Host->PacketSpecialActionToNobody(pCreature, actionInfo, x, y);
}

void PacketSpecialActionToSelf(MCreature* pCreature, TYPE_ACTIONINFO actionInfo, MActionResult* pResult)
{
	if (s_Host && s_Host->PacketSpecialActionToSelf) s_Host->PacketSpecialActionToSelf(pCreature, actionInfo, pResult);
	else delete pResult;
}

void SetDrainCreatureID(MCreature* pCreature, TYPE_OBJECTID id)
{
	if (s_Host && s_Host->SetDrainCreatureID) s_Host->SetDrainCreatureID(pCreature, id);
}

void ClearStopBloodDrain(MCreature* pCreature)
{
	if (s_Host && s_Host->ClearStopBloodDrain) s_Host->ClearStopBloodDrain(pCreature);
}

TYPE_ACTIONINFO GetBasicActionInfo(MCreature* pCreature)
{
	return s_Host && s_Host->GetBasicActionInfo ? s_Host->GetBasicActionInfo(pCreature) : ACTIONINFO_NULL;
}

bool RemoveEffectStatus(MCreature* pCreature, EFFECTSTATUS status)
{
	return s_Host && s_Host->RemoveEffectStatus ? s_Host->RemoveEffectStatus(pCreature, status) : false;
}

void SetFakeDie(MCreature* pCreature)
{
	if (s_Host && s_Host->SetFakeDie) s_Host->SetFakeDie(pCreature);
}

int GetDirection(MCreature* pCreature)
{
	return s_Host && s_Host->GetDirection ? s_Host->GetDirection(pCreature) : 0;
}

void RemoveHead(MCreature* pCreature)
{
	if (s_Host && s_Host->RemoveHead) s_Host->RemoveHead(pCreature);
}

void SetDirection(MCreature* pCreature, int direction)
{
	if (s_Host && s_Host->SetDirection) s_Host->SetDirection(pCreature, direction);
}

void SetVisibleSoon(MCreature* pCreature)
{
	if (s_Host && s_Host->SetVisibleSoon) s_Host->SetVisibleSoon(pCreature);
}

bool IsWear(MCreature* pCreature)
{
	return s_Host && s_Host->IsWear ? s_Host->IsWear(pCreature) : false;
}

bool SetAddonItem(MCreature* pCreature, MItem* pItem)
{
	return s_Host && s_Host->SetAddonItem ? s_Host->SetAddonItem(pCreature, pItem) : false;
}

void SetWeaponSpeed(MCreature* pCreature, int speed)
{
	if (s_Host && s_Host->SetWeaponSpeed) s_Host->SetWeaponSpeed(pCreature, speed);
}

bool RemoveAddon(MCreature* pCreature, int addon)
{
	return s_Host && s_Host->RemoveAddon ? s_Host->RemoveAddon(pCreature, addon) : false;
}

void SetMoveDevice(MCreature* pCreature, MOVE_DEVICE device)
{
	if (s_Host && s_Host->SetMoveDevice) s_Host->SetMoveDevice(pCreature, device);
}

int GetCreatureType(MCreature* pCreature)
{
	return s_Host && s_Host->GetCreatureType ? s_Host->GetCreatureType(pCreature) : 0;
}

int GetCompetence(MCreature* pCreature)
{
	return s_Host && s_Host->GetCompetence ? s_Host->GetCompetence(pCreature) : 3;
}

void SetCurrentDirection(MCreature* pCreature, int direction)
{
	if (s_Host && s_Host->SetCurrentDirection) s_Host->SetCurrentDirection(pCreature, direction);
}

const char* GetHalluName(MCreature* pCreature)
{
	return s_Host && s_Host->GetHalluName ? s_Host->GetHalluName(pCreature) : "";
}

bool IsSlayer(MCreature* pCreature)
{
	return s_Host && s_Host->IsSlayer ? s_Host->IsSlayer(pCreature) : false;
}

bool IsOusters(MCreature* pCreature)
{
	return s_Host && s_Host->IsOusters ? s_Host->IsOusters(pCreature) : false;
}

int GetRace(MCreature* pCreature)
{
	return s_Host && s_Host->GetRace ? s_Host->GetRace(pCreature) : 0;
}

void SetStopBloodDrain(MCreature* pCreature)
{
	if (s_Host && s_Host->SetStopBloodDrain) s_Host->SetStopBloodDrain(pCreature);
}

void SetStopAbsorbSoul(MCreature* pCreature)
{
	if (s_Host && s_Host->SetStopAbsorbSoul) s_Host->SetStopAbsorbSoul(pCreature);
}

bool IsVampire(MCreature* pCreature)
{
	return s_Host && s_Host->IsVampire ? s_Host->IsVampire(pCreature) : false;
}

bool SetShop(MCreature* pCreature, MShop* pShop)
{
	return s_Host && s_Host->SetShop ? s_Host->SetShop(pCreature, pShop) : false;
}

void CreateFixedShelf(MCreature* pCreature, bool mysterious)
{
	if (s_Host && s_Host->CreateFixedShelf) s_Host->CreateFixedShelf(pCreature, mysterious);
}

bool IsPlayerParty(MCreature* pCreature)
{
	return s_Host && s_Host->IsPlayerParty ? s_Host->IsPlayerParty(pCreature) : false;
}

int GetGuildNumber(MCreature* pCreature)
{
	return s_Host && s_Host->GetGuildNumber ? s_Host->GetGuildNumber(pCreature) : 0;
}

void SetPlayerParty(MCreature* pCreature)
{
	if (s_Host && s_Host->SetPlayerParty) s_Host->SetPlayerParty(pCreature);
}

TYPE_OBJECTID GetPetID(MCreature* pCreature)
{
	return s_Host && s_Host->GetPetID ? s_Host->GetPetID(pCreature) : OBJECTID_NULL;
}

void SetAction(MCreature* pCreature, int action)
{
	if (s_Host && s_Host->SetAction) s_Host->SetAction(pCreature, action);
}

MPetItem* GetPetItem(MCreature* pCreature)
{
	return s_Host && s_Host->GetPetItem ? s_Host->GetPetItem(pCreature) : nullptr;
}

void FastMovePosition(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, bool server)
{
	if (s_Host && s_Host->FastMovePosition) s_Host->FastMovePosition(pCreature, x, y, server);
}

void SetGhost(MCreature* pCreature, int flag, int delay)
{
	if (s_Host && s_Host->SetGhost) s_Host->SetGhost(pCreature, flag, delay);
}

int GetDirectionToPosition(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	return s_Host && s_Host->GetDirectionToPosition ? s_Host->GetDirectionToPosition(pCreature, x, y) : 0;
}

void SetInstallTurretDirect(MCreature* pCreature, int direction)
{
	if (s_Host && s_Host->SetInstallTurretDirect) s_Host->SetInstallTurretDirect(pCreature, direction);
}

void SetInstallTurretCount(MCreature* pCreature, int count)
{
	if (s_Host && s_Host->SetInstallTurretCount) s_Host->SetInstallTurretCount(pCreature, count);
}

void SetFakeFastMove(MCreature* pFakeCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	if (s_Host && s_Host->SetFakeFastMove) s_Host->SetFakeFastMove(pFakeCreature, x, y);
}

void SetTurretDelay(MCreature* pFakeCreature, int delay)
{
	if (s_Host && s_Host->SetTurretDelay) s_Host->SetTurretDelay(pFakeCreature, delay);
}

void SetTurretFinalDirection(MCreature* pFakeCreature, int direction)
{
	if (s_Host && s_Host->SetTurretFinalDirection) s_Host->SetTurretFinalDirection(pFakeCreature, direction);
}

void SetPersnalShop(MCreature* pCreature, int open)
{
	if (s_Host && s_Host->SetPersnalShop) s_Host->SetPersnalShop(pCreature, open);
}

int CurPernalShop(MCreature* pCreature)
{
	return s_Host && s_Host->CurPernalShop ? s_Host->CurPernalShop(pCreature) : 0;
}

void SetPersnalString(MCreature* pCreature, const char* sign, std::uint32_t color)
{
	if (s_Host && s_Host->SetPersnalString) s_Host->SetPersnalString(pCreature, sign, color);
}

void ClearChatString(MCreature* pCreature)
{
	if (s_Host && s_Host->ClearChatString) s_Host->ClearChatString(pCreature);
}

void SetPersnalShopMessage(MCreature* pCreature, const char* message)
{
	if (s_Host && s_Host->SetPersnalShopMessage) s_Host->SetPersnalShopMessage(pCreature, message);
}

void SetName(MCreature* pCreature, const char* name)
{
	// The game's SetName reads the name: an empty MString, a creature
	// table row past the table or never read from its file, converts to
	// NULL.
	if (s_Host && s_Host->SetName) s_Host->SetName(pCreature, name != NULL ? name : "");
}

void SetID(MCreature* pCreature, TYPE_OBJECTID id)
{
	if (s_Host && s_Host->SetID) s_Host->SetID(pCreature, id);
}

void SetPosition(MCreature* pCreature, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	if (s_Host && s_Host->SetPosition) s_Host->SetPosition(pCreature, x, y);
}

void SetGuildNumber(MCreature* pCreature, int guild)
{
	if (s_Host && s_Host->SetGuildNumber) s_Host->SetGuildNumber(pCreature, guild);
}

void SetBatColor(MCreature* pCreature, std::uint16_t colorSet)
{
	if (s_Host && s_Host->SetBatColor) s_Host->SetBatColor(pCreature, colorSet);
}

bool IsPlainCreature(MCreature* pCreature)
{
	return s_Host && s_Host->IsPlainCreature && s_Host->IsPlainCreature(pCreature);
}

int GetZ(MCreature* pCreature)
{
	return s_Host && s_Host->GetZ ? s_Host->GetZ(pCreature) : 0;
}

bool IsPlainCreatureWear(MCreature* pCreature)
{
	return s_Host && s_Host->IsPlainCreatureWear && s_Host->IsPlainCreatureWear(pCreature);
}

void SetAddonToSlayer3(MCreature* pCreature, const PCSlayerInfo3* pInfo)
{
	if (s_Host && s_Host->SetAddonToSlayer3) s_Host->SetAddonToSlayer3(pCreature, pInfo);
}

void SetCompetence(MCreature* pCreature, int competence)
{
	if (s_Host && s_Host->SetCompetence) s_Host->SetCompetence(pCreature, competence);
}

void SetMasterEffectType(MCreature* pCreature, int type)
{
	if (s_Host && s_Host->SetMasterEffectType) s_Host->SetMasterEffectType(pCreature, type);
}

void SetUnionGuildID(MCreature* pCreature, int unionID)
{
	if (s_Host && s_Host->SetUnionGuildID) s_Host->SetUnionGuildID(pCreature, unionID);
}

void SetAddonToOusters3(MCreature* pCreature, const PCOustersInfo3* pInfo)
{
	if (s_Host && s_Host->SetAddonToOusters3) s_Host->SetAddonToOusters3(pCreature, pInfo);
}

void SetCorpse(MCreature* pCreature)
{
	if (s_Host && s_Host->SetCorpse) s_Host->SetCorpse(pCreature);
}

bool IsDead(MCreature* pCreature)
{
	return s_Host && s_Host->IsDead && s_Host->IsDead(pCreature);
}

void SetItemCount(MCreature* pCreature, int count)
{
	if (s_Host && s_Host->SetItemCount) s_Host->SetItemCount(pCreature, count);
}

void SetNPCID(MCreature* pCreature, int npcID)
{
	if (s_Host && s_Host->SetNPCID) s_Host->SetNPCID(pCreature, npcID);
}

void SetAddonToNPC(MCreature* pCreature, int npcID)
{
	if (s_Host && s_Host->SetAddonToNPC) s_Host->SetAddonToNPC(pCreature, npcID);
}

void AttachLastingEffect(MCreature* pCreature, int effectSpriteType)
{
	if (s_Host && s_Host->AttachLastingEffect) s_Host->AttachLastingEffect(pCreature, effectSpriteType);
}

MAttachEffect* CreateAttachEffect(MCreature* pCreature, int effectSpriteType, std::uint32_t delayFrame, std::uint32_t linkDelayFrame, bool bGroundEffect, int effectType)
{
	return s_Host && s_Host->CreateAttachEffect ? s_Host->CreateAttachEffect(pCreature, effectSpriteType, delayFrame, linkDelayFrame, bGroundEffect, effectType) : nullptr;
}

bool IsExistAttachEffect(MCreature* pCreature)
{
	return s_Host && s_Host->IsExistAttachEffect && s_Host->IsExistAttachEffect(pCreature);
}

MAttachEffect* FirstAttachEffect(MCreature* pCreature)
{
	return s_Host && s_Host->FirstAttachEffect ? s_Host->FirstAttachEffect(pCreature) : nullptr;
}

void ClearAttachEffect(MCreature* pCreature)
{
	if (s_Host && s_Host->ClearAttachEffect) s_Host->ClearAttachEffect(pCreature);
}

bool HasEffectStatus(MCreature* pCreature, EFFECTSTATUS status)
{
	return s_Host && s_Host->HasEffectStatus && s_Host->HasEffectStatus(pCreature, status);
}

int GetActionCount(MCreature* pCreature)
{
	return s_Host && s_Host->GetActionCount ? s_Host->GetActionCount(pCreature) : 0;
}

int GetActionCountMax(MCreature* pCreature)
{
	return s_Host && s_Host->GetActionCountMax ? s_Host->GetActionCountMax(pCreature) : 0;
}

void SetShadowCount(MCreature* pCreature, int count)
{
	if (s_Host && s_Host->SetShadowCount) s_Host->SetShadowCount(pCreature, count);
}

MObject* ObjectOf(const MCreature* pCreature)
{
	return s_Host && s_Host->ObjectOf ? s_Host->ObjectOf(pCreature) : nullptr;
}

MCreature* CreatureOf(MObject* pObject)
{
	return s_Host && s_Host->CreatureOf ? s_Host->CreatureOf(pObject) : nullptr;
}

}
