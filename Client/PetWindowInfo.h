#pragma once

#include <string>
#include "MTypeDef.h"

// What the pet window shows of a pet (VS_UI's C_VS_UI_PET_INFO keeps a
// copy). The player fills one for a pet it clicks on (task 4.247 moved
// the struct here from VS_UI_GameCommon2.h, so gamemodel can build it).
struct PETINFO
{
	bool EXIST;
	int HP;
	int ATTR;
	int ATTR_VALUE;
	int LEVEL;
	int FOODTYPE;
	int EXP_REMAIN;
	bool CUT_HEAD;
	bool GAMBLE;
	int OPTION;
	int ITEM_TYPE;
	TYPE_OBJECTID ITEM_ID;
	std::string NAME;
	bool ATTACK;
	std::string NICK_NAME;
	bool ChangeAbleNickName; // whether it is the player's own pet
};
