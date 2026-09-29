//----------------------------------------------------------------------
// MCreatureSpriteTable.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MCreatureSpriteTable.h"

#include <cstdint>

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
CREATURESPRITE_TABLE*	g_pCreatureSpriteTable = NULL;
// CREATURESPRITE_TABLE*	g_pAddonSpriteTable = NULL;
//CREATURESPRITE_TABLE*	g_pCreatureActionSpriteTable = NULL;

//----------------------------------------------------------------------
//
// constructor / destructor
//
//----------------------------------------------------------------------
CREATURESPRITETABLE_INFO::CREATURESPRITETABLE_INFO()
{
	bLoad = FALSE;
	CreatureType = 0;
}

CREATURESPRITETABLE_INFO::~CREATURESPRITETABLE_INFO()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
void			
CREATURESPRITETABLE_INFO::SaveToFile(std::ofstream& file)
{
	file.write((const char*)&FrameID, SIZE_FRAMEID);	
	file.write((const char*)&SpriteFilePosition, 4);
	file.write((const char*)&SpriteShadowFilePosition, 4);
	file.write((const char*)&FirstSpriteID, SIZE_SPRITEID);
	file.write((const char*)&LastSpriteID, SIZE_SPRITEID);
	file.write((const char*)&FirstShadowSpriteID, SIZE_SPRITEID);
	file.write((const char*)&LastShadowSpriteID, SIZE_SPRITEID);
	file.write((const char*)&CreatureType, 1);	
}

//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
void			
CREATURESPRITETABLE_INFO::LoadFromFile(std::ifstream& file)
{
	file.read((char*)&FrameID, SIZE_FRAMEID);

	// The positions are four bytes on disk and a long in memory, which
	// is eight bytes on LP64 targets (macOS, Linux) and four on Windows
	// and wasm32: read the signed 32-bit value whole, so the upper half
	// of an eight-byte long is its sign and not what the member held.
	std::int32_t position = 0;
	if (file.read((char*)&position, 4))
		SpriteFilePosition = position;
	if (file.read((char*)&position, 4))
		SpriteShadowFilePosition = position;
	file.read((char*)&FirstSpriteID, SIZE_SPRITEID);
	file.read((char*)&LastSpriteID, SIZE_SPRITEID);
	file.read((char*)&FirstShadowSpriteID, SIZE_SPRITEID);
	file.read((char*)&LastShadowSpriteID, SIZE_SPRITEID);
	file.read((char*)&CreatureType, 1);
	//add by viva  //McreatureSpriteTable  NewVampire180 CreatureType:0xFC,0xFD
//	if(FrameID==247||FrameID==248)
//		CreatureType = 18;
}
