#include "Client_PCH.h"
#include "StatusPresentationHost.h"

namespace StatusPresentation {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

void SetHP(int current, int maximum)
{
	if (s_Host && s_Host->SetHP) s_Host->SetHP(current, maximum);
}

void SetMP(int current, int maximum)
{
	if (s_Host && s_Host->SetMP) s_Host->SetMP(current, maximum);
}

void SetCharInfoHP(int value)
{
	if (s_Host && s_Host->SetCharInfoHP) s_Host->SetCharInfoHP(value);
}

void SetCharInfoMP(int value)
{
	if (s_Host && s_Host->SetCharInfoMP) s_Host->SetCharInfoMP(value);
}

void SetCharInfoHP_max(int value)
{
	if (s_Host && s_Host->SetCharInfoHP_max) s_Host->SetCharInfoHP_max(value);
}

void SetCharInfoMP_max(int value)
{
	if (s_Host && s_Host->SetCharInfoMP_max) s_Host->SetCharInfoMP_max(value);
}

void SetCharInfoSTR(int value)
{
	if (s_Host && s_Host->SetCharInfoSTR) s_Host->SetCharInfoSTR(value);
}

void SetCharInfoDEX(int value)
{
	if (s_Host && s_Host->SetCharInfoDEX) s_Host->SetCharInfoDEX(value);
}

void SetCharInfoINT(int value)
{
	if (s_Host && s_Host->SetCharInfoINT) s_Host->SetCharInfoINT(value);
}

void SetCharInfoFAME(int value)
{
	if (s_Host && s_Host->SetCharInfoFAME) s_Host->SetCharInfoFAME(value);
}

void SetCharInfoTOHIT(int value)
{
	if (s_Host && s_Host->SetCharInfoTOHIT) s_Host->SetCharInfoTOHIT(value);
}

void SetCharInfoDefense(int value)
{
	if (s_Host && s_Host->SetCharInfoDefense) s_Host->SetCharInfoDefense(value);
}

void SetCharInfoProtection(int value)
{
	if (s_Host && s_Host->SetCharInfoProtection) s_Host->SetCharInfoProtection(value);
}

void SetCharInfoLevel(int value)
{
	if (s_Host && s_Host->SetCharInfoLevel) s_Host->SetCharInfoLevel(value);
}

void SetCharInfoSilverDamage(int value)
{
	if (s_Host && s_Host->SetCharInfoSilverDamage) s_Host->SetCharInfoSilverDamage(value);
}

void LevelUp()
{
	if (s_Host && s_Host->LevelUp) s_Host->LevelUp();
}

void SetBonusPoint(int points)
{
	if (s_Host && s_Host->SetBonusPoint) s_Host->SetBonusPoint(points);
}

void SetSkillPoint(int points)
{
	if (s_Host && s_Host->SetSkillPoint) s_Host->SetSkillPoint(points);
}

void SetElementalValue(int element, int value)
{
	if (s_Host && s_Host->SetElementalValue) s_Host->SetElementalValue(element, value);
}

void SetSheet(SheetField field, int value)
{
	if (s_Host && s_Host->SetSheet) s_Host->SetSheet(field, value);
}

}
