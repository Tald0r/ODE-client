#include "Client_PCH.h"
#include "CreatureTypeSelection.h"
#include "DebugLog.h"
#include "ItemClassDef.h"
#include "MCreatureTable.h"
#include "MItemTable.h"
#include "Packet/Types/CreatureTypes.h"

//-----------------------------------------------------------------------------
// Get VampireCreatureType
//-----------------------------------------------------------------------------
// A person (man, woman), a wolf, a bat..
//-----------------------------------------------------------------------------
int
GetVampireCreatureType(int shape, bool bMale, int coatType)
{
	DEBUG_ADD_FORMAT("Shape=%d. Male=%d", shape, (int)bMale);
	
	//--------------------------------------------------
	// Set the CreatureType
	//--------------------------------------------------
	switch ( shape )
	{
	case SHAPE_NORMAL :
		if(bMale)
		{
			return (*g_pItemTable)[ITEM_CLASS_VAMPIRE_COAT][coatType].AddonMaleFrameID;//(bMale)? CREATURETYPE_VAMPIRE_MALE : CREATURETYPE_VAMPIRE_FEMALE;
		}
		else
		{
			return (*g_pItemTable)[ITEM_CLASS_VAMPIRE_COAT][coatType].AddonFemaleFrameID;//(bMale)? CREATURETYPE_VAMPIRE_MALE : CREATURETYPE_VAMPIRE_FEMALE;
		}		
		break;
	case SHAPE_BAT :
		return CREATURETYPE_BAT;
		break;
	case SHAPE_WOLF :
		return CREATURETYPE_WOLF;
		break;
	case SHAPE_WERWOLF :
		return CREATURETYPE_WER_WOLF;
		break;
	} 

	return SHAPE_MAX;
}


//-----------------------------------------------------------------------------
// Get OustersCreatureType
//-----------------------------------------------------------------------------
int
GetOustersCreatureType(int coatType)
{
	(void)coatType;
	//--------------------------------------------------
	// Set the CreatureType
	//--------------------------------------------------
	return CREATURETYPE_OUSTERS;
	//	return (*g_pItemTable)[ITEM_CLASS_VAMPIRE_COAT][coatType].AddonMaleFrameID;
}
