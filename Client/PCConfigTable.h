//----------------------------------------------------------------------
// PCConfigTable.h
//----------------------------------------------------------------------
// Stores the last selected character slot by world and account identity.
// A successful record write ages the account, saturating at DWORD's maximum;
// a valid character selection resets its age. Saves retain the twenty most
// recent accounts per world, leaving the in-memory tables intact.
//----------------------------------------------------------------------

#ifndef __PC_CONFIG_TABLE_H__
#define __PC_CONFIG_TABLE_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include <map>
#include <string>

#include <fstream>

/*
class CharacterConfig {
	public :
		PCConfig();
		~PCConfig();			
};
*/

//----------------------------------------------------------------------
// PlayerConfig
//----------------------------------------------------------------------
class PlayerConfig {
	public :
		PlayerConfig();
		~PlayerConfig();

		//--------------------------------------------------------
		// PlayerID
		//--------------------------------------------------------
		void				SetPlayerID(const std::string& playerID)	{ m_PlayerID = playerID; }
		const std::string&	GetPlayerID() const							{ return m_PlayerID; }
		
		//--------------------------------------------------------
		// slot
		//--------------------------------------------------------
		int			GetLastSlot() const				{ return m_LastSlot; }
		// Only slots 0..2 count as a new selection; invalid values are ignored.
		void		SetLastSlot(int slot);

		DWORD		GetRecentCount() const						{ return m_RecentCount; }
		
		//--------------------------------------------------------
		// File I/O
		//--------------------------------------------------------
		void		SaveToFile(std::ofstream& file);
		// Failed or invalid input sets failbit and preserves this record.
		void		LoadFromFile(std::ifstream& file);

	protected :
		std::string	m_PlayerID;
		BYTE		m_LastSlot;			// last selected slot, 0..2
		DWORD		m_RecentCount;		// larger values mean older selections
};

//----------------------------------------------------------------------
// PlayerConfigTable
//----------------------------------------------------------------------
// map<PlayerID, PlayerConfig*>
//----------------------------------------------------------------------
class PlayerConfigTable : public std::map<std::string, PlayerConfig*> {
	public :
		PlayerConfigTable();
		~PlayerConfigTable();

		void				Release();

		//--------------------------------------------------------
		// Add/Get PlayerConfigTable
		//--------------------------------------------------------
		// Owns accepted records. Null/unnamed records remain the caller's.
		// Re-adding the same pointer at its existing key is harmless.
		void				AddPlayerConfig(PlayerConfig* pConfig);
		PlayerConfig*		GetPlayerConfig(const char* pPlayerID) const;

		//--------------------------------------------------------
		// Limit PlayerConfig
		//--------------------------------------------------------
		void				LimitPlayerConfig(int limit);

		//--------------------------------------------------------
		// File I/O
		//--------------------------------------------------------
		void		SaveToFile(std::ofstream& file);
		// Reload clears old settings; a failed parse leaves the table empty.
		void		LoadFromFile(std::ifstream& file);

};


//----------------------------------------------------------------------
// WorldPlayerConfigTable
//----------------------------------------------------------------------
// map<WorldID, PlayerConfigTable*>
//----------------------------------------------------------------------
class WorldPlayerConfigTable : public std::map<int, PlayerConfigTable*> {
	public :
		WorldPlayerConfigTable();
		~WorldPlayerConfigTable();

		void				Release();

		//--------------------------------------------------------
		// Add/Get PlayerConfigTable
		//--------------------------------------------------------
		// Owns accepted tables; re-adding the same pointer at its key is harmless.
		void				AddPlayerConfigTable(int worldID, PlayerConfigTable* pTable);
		PlayerConfigTable*	GetPlayerConfigTable(int worldID) const;

		//--------------------------------------------------------
		// File I/O
		//--------------------------------------------------------
		void		SaveToFile(const char* pFilename);
		// Reload clears old settings; only a complete version-2 file is published.
		void		LoadFromFile(const char* pFilename);
};


//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
extern WorldPlayerConfigTable*		g_pWorldPlayerConfigTable;

#endif
