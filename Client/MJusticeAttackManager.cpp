//----------------------------------------------------------------------
// MJusticeAttackManager.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MJusticeAttackManager.h"

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
MJusticeAttackManager* g_pJusticeAttackManager = NULL;

//----------------------------------------------------------------------
//
// constructor / destructor
// 
//----------------------------------------------------------------------
MJusticeAttackManager::MJusticeAttackManager()
{
}

MJusticeAttackManager::~MJusticeAttackManager()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
MJusticeAttackManager::Release()
{
	m_Creatures.clear();
}

//----------------------------------------------------------------------
// Add Creature
//----------------------------------------------------------------------
void		
MJusticeAttackManager::AddCreature(const char* pName)
{
	m_Creatures.insert(std::string(pName));
}

//----------------------------------------------------------------------
// Remove Creature
//----------------------------------------------------------------------
bool		
MJusticeAttackManager::RemoveCreature(const char* pName)
{
	auto iName = m_Creatures.find( std::string(pName) );

	if (iName != m_Creatures.end())
	{
		m_Creatures.erase( iName );

		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Has Creature
//----------------------------------------------------------------------
bool
MJusticeAttackManager::HasCreature(const char* pName) const
{
	auto iName = m_Creatures.find( std::string(pName) );

	if (iName != m_Creatures.end())
	{
		return true;
	}

	return false;
}
