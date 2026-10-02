//----------------------------------------------------------------------
// MInteractionObjectTable.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MInteractionObjectTable.h"
#include <bit>
#include <cstdint>

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
INTERACTIONOBJECT_TABLE* 	g_pInteractionObjectTable = NULL;


//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
void		
INTERACTIONOBJECTTABLE_INFO::SaveToFile(std::ofstream& file)
{
	const auto property = static_cast<std::uint32_t>(Property);
	const unsigned char encoded[]{
		Type, static_cast<unsigned char>(FrameID), static_cast<unsigned char>(FrameID >> 8),
		static_cast<unsigned char>(property), static_cast<unsigned char>(property >> 8),
		static_cast<unsigned char>(property >> 16), static_cast<unsigned char>(property >> 24),
		static_cast<unsigned char>(SoundID), static_cast<unsigned char>(SoundID >> 8), 0, 0
	};
	file.write(reinterpret_cast<const char*>(encoded), sizeof(encoded));
}

//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
void			
INTERACTIONOBJECTTABLE_INFO::LoadFromFile(std::ifstream& file)
{
	unsigned char encoded[11]{};
	if (!file.read(reinterpret_cast<char*>(encoded), sizeof(encoded))) return;
	const std::uint32_t property = std::uint32_t(encoded[3]) | (std::uint32_t(encoded[4]) << 8)
		| (std::uint32_t(encoded[5]) << 16) | (std::uint32_t(encoded[6]) << 24);
	Type = encoded[0];
	FrameID = static_cast<TYPE_FRAMEID>(encoded[1] | (unsigned(encoded[2]) << 8));
	Property = std::bit_cast<std::int32_t>(property);
	// Older writers copied object padding into the upper sound-slot word.
	SoundID = static_cast<TYPE_SOUNDID>(encoded[7] | (unsigned(encoded[8]) << 8));
}
