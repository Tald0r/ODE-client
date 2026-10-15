//----------------------------------------------------------------------
// MSector.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include "MObject.h"
#include "MItem.h"
#include "ZoneStateHost.h"
#include "MImageObject.h"
#include "MEffect.h"
#include "MSector.h"
// Note: MTopView.h removed - MSector now uses EffectSpriteTypeHelper (decoupled for demo/game flexibility)
#include "EffectSpriteTypeDef.h"
#include "EffectSpriteTypeHelper.h"
#include <fstream>

// A corpse of creature type 672 (the sweeper) blocks the ground while it
// lies on the sector.
static bool IsBlockingCorpse(const MItem* pItem)
{
	if (pItem->GetItemClass() != ITEM_CLASS_CORPSE)
		return false;

	MCreature* pCreature = MItem::CorpseCreature(pItem);
	return pCreature != NULL && CreatureState::GetCreatureType(pCreature) == 672;
}

//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------
MSector::MSector(TYPE_SPRITEID	spriteID)
{
	m_SpriteID = spriteID;

	m_fProperty		= 0;

	// The number of objects in the sector
	m_nImageObject	= 0;

	//m_FilterSpriteID = SPRITEID_NULL;

	m_Light = 0;

	m_fProperty2 = 0;

	m_bDarkness = false;
	m_bSanctuary = false;	// [new skill]
	m_bGroundElemental = false;	// [new skill]
	m_bDarknessForbidden = false;	// [new skill]

	m_nGroundCreature = 0;
	m_nUndergroundCreature = 0;
	m_nFlyingCreature = 0;
	m_bVisitedFlag = false;
	m_bFuryOfGnome = false;
	m_bDisableTileImage = false;
}

MSector::~MSector()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
void
MSector::Set(TYPE_SPRITEID spriteID, BYTE fObject)
{
	m_SpriteID	= spriteID;
	m_fProperty	= fObject;

	// m_mapObject.clear();
}

//----------------------------------------------------------------------
// Remove All Object
//----------------------------------------------------------------------
void
MSector::RemoveAllObject()
{
	m_mapObject.clear();
	m_nImageObject = 0;
	m_fProperty = 0;
	m_fProperty2 = 0;

	m_nGroundCreature = 0;
	m_nUndergroundCreature = 0;
	m_nFlyingCreature = 0;
}

//----------------------------------------------------------------------
// Save to File
//----------------------------------------------------------------------
// Saves only the sprite ID, the block and portal flags and the light.
//----------------------------------------------------------------------
void
MSector::SaveToFile(ofstream& file)
{
	BYTE flag = 0;

	// With the underground block flag
	if (m_fProperty & FLAG_SECTOR_BLOCK_UNDERGROUND)
	{
		// Save only the block flag.
		flag |= FLAG_SECTOR_BLOCK_UNDERGROUND;
	}

	// With the ground block flag
	if (m_fProperty & FLAG_SECTOR_BLOCK_GROUND)
	{
		// Save only the block flag.
		flag |= FLAG_SECTOR_BLOCK_GROUND;
	}

	// With the flying block flag
	if (m_fProperty & FLAG_SECTOR_BLOCK_FLYING)
	{
		// Save only the block flag.
		flag |= FLAG_SECTOR_BLOCK_FLYING;
	}

	// With the portal flag
	if (m_fProperty & FLAG_SECTOR_PORTAL)
	{
		// Save only the block flag.
		flag |= FLAG_SECTOR_PORTAL;
	}


	file.write((const char*)&m_SpriteID, SIZE_SPRITEID);
	file.write((const char*)&flag, 1);
	file.write((const char*)&m_Light, 1);
}

//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
// Loads only the sprite ID, the block flags and the light.
//----------------------------------------------------------------------
void
MSector::LoadFromFile(ifstream& file)
{
	//----------------------------------------
	// The object list should be cleared....
	// but no sector is loaded, given objects
	// and loaded again,
	// so for speed it is not cleared.
	//----------------------------------------
	// m_listObject.clear();

	file.read((char*)&m_SpriteID, SIZE_SPRITEID);
	file.read((char*)&m_fProperty, 1);
	file.read((char*)&m_Light, 1);
}


//----------------------------------------------------------------------
// Assign Operator
//----------------------------------------------------------------------
// Only the tile's look is assigned, and the block flags are added to
// the sector's own.
//----------------------------------------------------------------------
void
MSector::operator = (const MSector& sector)
{
	m_SpriteID = sector.m_SpriteID;

	// undeground block
	if (sector.m_fProperty & FLAG_SECTOR_BLOCK_UNDERGROUND)
		m_fProperty |= FLAG_SECTOR_BLOCK_UNDERGROUND;

	// ground block
	if (sector.m_fProperty & FLAG_SECTOR_BLOCK_GROUND)
		m_fProperty |= FLAG_SECTOR_BLOCK_GROUND;

	// flying block
	if (sector.m_fProperty & FLAG_SECTOR_BLOCK_FLYING)
		m_fProperty |= FLAG_SECTOR_BLOCK_FLYING;
}

//----------------------------------------------------------------------
// Can Stand UndergroundCreature
//----------------------------------------------------------------------
// Whether an underground creature can enter this sector:
//----------------------------------------------------------------------
// False if blocked,
// false if an underground creature is there,
// true otherwise.
//----------------------------------------------------------------------
bool
MSector::CanStandUndergroundCreature() const
{
	if (m_fProperty & FLAG_SECTOR_BLOCK_UNDERGROUND
		|| m_fProperty & FLAG_SECTOR_UNDERGROUNDCREATURE
		|| m_fProperty2 & FLAG_SECTOR_BLOCK_SERVER_UNDERGROUND)
		return false;

	return true;
}

//----------------------------------------------------------------------
// Can Stand Creature
//----------------------------------------------------------------------
// Whether a ground creature can enter this sector:
//----------------------------------------------------------------------
// False if blocked,
// false if a ground creature is there,
// true otherwise.
//----------------------------------------------------------------------
bool
MSector::CanStandGroundCreature() const
{
	if (m_fProperty & FLAG_SECTOR_BLOCK_GROUND
		|| m_fProperty & FLAG_SECTOR_GROUNDCREATURE
		|| m_fProperty2 & FLAG_SECTOR_BLOCK_SERVER_GROUND)
		return false;

	return true;
}


//----------------------------------------------------------------------
// Can Stand FlyingCreature
//----------------------------------------------------------------------
// Whether a flying creature can enter this sector:
//----------------------------------------------------------------------
// False if blocked,
// false if a flying creature is there,
// true otherwise.
//----------------------------------------------------------------------
bool
MSector::CanStandFlyingCreature() const
{
	if (m_fProperty & FLAG_SECTOR_BLOCK_FLYING
		|| m_fProperty & FLAG_SECTOR_FLYINGCREATURE
		|| m_fProperty2 & FLAG_SECTOR_BLOCK_SERVER_FLYING)
		return false;

	return true;
}



//----------------------------------------------------------------------
//
//                        Add Object
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Add Item
//----------------------------------------------------------------------
// No item can be added when one is already there.
// Otherwise pItem is added
// Set the item flag.
//----------------------------------------------------------------------
bool
MSector::AddItem(const MItem* pItem)
{
	// An item already there: it cannot be added
	if (m_fProperty & FLAG_SECTOR_ITEM)
		return false;

	// Add the item at the item's position.
	m_mapObject.insert( OBJECT_MAP::value_type(POSITION_ITEM, (MObject*)pItem) );

	// Set the item flag.
	m_fProperty |= FLAG_SECTOR_ITEM;
	if (IsBlockingCorpse(pItem))
		m_fProperty |= FLAG_SECTOR_BLOCK_GROUND;

	return true;
}

//----------------------------------------------------------------------
// Add UndergroundCreature
//----------------------------------------------------------------------
// Adds an underground creature at the slot its count points to or, when that is
// taken, just below the first taken slot after a free one (false past
// the layer's last slot)
// Set the underground creature flag.
//----------------------------------------------------------------------
bool
MSector::AddUndergroundCreature(const MCreature* pUndergroundCreature)
{
	BYTE pos = POSITION_UNDERGROUNDCREATURE+m_nUndergroundCreature;

	if (pos > POSITION_UNDERGROUNDCREATURE_MAX)
		return false;


	int beforeSize = static_cast<int>(m_mapObject.size());

	// Add it at the underground creatures' next position.
	m_mapObject.insert( OBJECT_MAP::value_type(pos, CreatureState::ObjectOf(pUndergroundCreature)) );

	int afterSize = static_cast<int>(m_mapObject.size());

	// If it was not added (the slot is taken)..
	if (afterSize==beforeSize)
	{
		BYTE previousPos = POSITION_UNDERGROUNDCREATURE-1;

		// search them all for a free slot and add it there.
		OBJECT_MAP::const_iterator iCreature = GetUndergroundCreatureIterator();
		BYTE currentPos = 0;
		int i;

		for (i=0; i<beforeSize; i++)
		{
			currentPos = iCreature->first;

			// A jump in the positions means a free slot:
			// currentPos-1 is certainly free.
			if (currentPos - previousPos > 1)
			{
				pos = currentPos - 1;
				m_mapObject.insert( OBJECT_MAP::value_type(pos, CreatureState::ObjectOf(pUndergroundCreature)) );

				break;
			}

			previousPos = currentPos;

			iCreature ++;
		}

		// Put it after the last one.
		if (i==beforeSize)
		{
			pos = currentPos + 1;
			m_mapObject.insert( OBJECT_MAP::value_type(pos, CreatureState::ObjectOf(pUndergroundCreature)) );
		}
	}

	// Set the underground creature flag.
	m_fProperty |= FLAG_SECTOR_UNDERGROUNDCREATURE;

	m_nUndergroundCreature ++;

	return true;
}

//----------------------------------------------------------------------
// Add Creature
//----------------------------------------------------------------------
// Adds a ground creature at the slot its count points to or, when that is
// taken, just below the first taken slot after a free one (false past
// the layer's last slot)
// Set the ground creature flag.
//----------------------------------------------------------------------
bool
MSector::AddGroundCreature(const MCreature* pCreature)
{
	BYTE pos = POSITION_GROUNDCREATURE+m_nGroundCreature;

	if (pos > POSITION_GROUNDCREATURE_MAX)
		return false;

	int beforeSize = static_cast<int>(m_mapObject.size());

	// Add it at the ground creatures' next position.
	m_mapObject.insert( OBJECT_MAP::value_type(pos, CreatureState::ObjectOf(pCreature)) );

	int afterSize = static_cast<int>(m_mapObject.size());

	// If it was not added (the slot is taken)..
	if (afterSize==beforeSize)
	{
		BYTE previousPos = POSITION_GROUNDCREATURE-1;

		// search them all for a free slot and add it there.
		OBJECT_MAP::const_iterator iCreature = GetGroundCreatureIterator();
		BYTE currentPos = 0;
		int i;

		for (i=0; i<beforeSize; i++)
		{
			currentPos = iCreature->first;

			// A jump in the positions means a free slot:
			// currentPos-1 is certainly free.
			if (currentPos - previousPos > 1)
			{
				pos = currentPos - 1;
				m_mapObject.insert( OBJECT_MAP::value_type(pos, CreatureState::ObjectOf(pCreature)) );

				break;
			}

			previousPos = currentPos;

			iCreature ++;
		}

		// Put it after the last one.
		if (i==beforeSize)
		{
			pos = currentPos + 1;
			m_mapObject.insert( OBJECT_MAP::value_type(pos, CreatureState::ObjectOf(pCreature)) );
		}
	}


	// Set the ground creature flag.
	m_fProperty |= FLAG_SECTOR_GROUNDCREATURE;

	m_nGroundCreature ++;

	return true;
}

//----------------------------------------------------------------------
// Add FlyingCreature
//----------------------------------------------------------------------
// Adds a flying creature at the slot its count points to or, when that is
// taken, just below the first taken slot after a free one (false past
// the layer's last slot)
// Set the flying creature flag.
//----------------------------------------------------------------------
bool
MSector::AddFlyingCreature(const MCreature* pFlyingCreature)
{
	BYTE pos = POSITION_FLYINGCREATURE+m_nFlyingCreature;

	if (pos > POSITION_FLYINGCREATURE_MAX)
		return false;

	int beforeSize = static_cast<int>(m_mapObject.size());

	// Add it at the flying creatures' next position.
	m_mapObject.insert( OBJECT_MAP::value_type(pos, CreatureState::ObjectOf(pFlyingCreature)) );

	int afterSize = static_cast<int>(m_mapObject.size());

	// If it was not added (the slot is taken)..
	if (afterSize==beforeSize)
	{
		BYTE previousPos = POSITION_FLYINGCREATURE-1;

		// search them all for a free slot and add it there.
		OBJECT_MAP::const_iterator iCreature = GetFlyingCreatureIterator();
		BYTE currentPos = 0;
		int i;

		for (i=0; i<beforeSize; i++)
		{
			currentPos = iCreature->first;

			// A jump in the positions means a free slot:
			// currentPos-1 is certainly free.
			if (currentPos - previousPos > 1)
			{
				pos = currentPos - 1;
				m_mapObject.insert( OBJECT_MAP::value_type(pos, CreatureState::ObjectOf(pFlyingCreature)) );

				break;
			}

			previousPos = currentPos;

			iCreature ++;
		}

		// Put it after the last one.
		if (i==beforeSize)
		{
			pos = currentPos + 1;
			m_mapObject.insert( OBJECT_MAP::value_type(pos, CreatureState::ObjectOf(pFlyingCreature)) );
		}
	}

	// Set the flying creature flag.
	m_fProperty |= FLAG_SECTOR_FLYINGCREATURE;

	m_nFlyingCreature ++;

	return true;
}

//----------------------------------------------------------------------
// Add ImageObject
//----------------------------------------------------------------------
// An image object is always added, whether or not there are some.
// Since there may be some already,
// it goes after the ones there (by their count).
// Adding it increments m_nImageObject.
//
// (Past the last image object position it is refused.)
//
//----------------------------------------------------------------------
bool
MSector::AddImageObject(const MImageObject* pImageObject)
{
	BYTE pos = POSITION_IMAGEOBJECT+m_nImageObject;

	if (pos >= POSITION_EFFECT)
		return false;

	// Add it at the image objects' next position.
	m_mapObject.insert(
		OBJECT_MAP::value_type(pos, (MObject*)pImageObject)
	);

	// Count it.
	m_nImageObject ++;

	return true;
}


//----------------------------------------------------------------------
//
//                         Get Object
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Get Item
//----------------------------------------------------------------------
// Returns the item when the flag says there is one.
//----------------------------------------------------------------------
MItem*
MSector::GetItem() const
{
	// If there is an item...
	if (m_fProperty & FLAG_SECTOR_ITEM)
	{
		// Find the object at the item's position.
		OBJECT_MAP::const_iterator iObject = m_mapObject.find(POSITION_ITEM);

		// None: return NULL.
		if (iObject == m_mapObject.end())
			return NULL;

		// If there is, return the object.
		return (MItem*)((*iObject).second);
	}

	return NULL;
}

//----------------------------------------------------------------------
// Get Creature
//----------------------------------------------------------------------
// Returns it when the flag says there is a creature.
//----------------------------------------------------------------------
OBJECT_MAP::const_iterator
MSector::GetCreatureIterator() const
{
	// If there is any creature...
	if (IsExistAnyCreature())
	{
		// Find the object at the underground creatures' position.
		OBJECT_MAP::const_iterator iObject = m_mapObject.lower_bound(POSITION_UNDERGROUNDCREATURE);

		if (iObject!=m_mapObject.end()
			&& iObject->first <= POSITION_FLYINGCREATURE_MAX)
		{
			return iObject;
		}
	}

	return m_mapObject.end();
}

//----------------------------------------------------------------------
// Get UndergroundCreature
//----------------------------------------------------------------------
// Returns it when the flag says there is an underground creature.
//----------------------------------------------------------------------
OBJECT_MAP::const_iterator
MSector::GetUndergroundCreatureIterator() const
{
	// If there is an underground creature...
	if (m_fProperty & FLAG_SECTOR_UNDERGROUNDCREATURE)
	{
		// Find the object at the underground creatures' position.
		OBJECT_MAP::const_iterator iObject = m_mapObject.lower_bound(POSITION_UNDERGROUNDCREATURE);

		// If there is one and it is an underground creature..
		if (iObject!=m_mapObject.end()
			&& iObject->first <= POSITION_UNDERGROUNDCREATURE_MAX)
		{
			return iObject;
		}
	}

	return m_mapObject.end();
}

//----------------------------------------------------------------------
// Get Creature
//----------------------------------------------------------------------
// Returns it when the flag says there is a ground creature.
//----------------------------------------------------------------------
OBJECT_MAP::const_iterator
MSector::GetGroundCreatureIterator() const
{
	// If there is a ground creature...
	if (m_fProperty & FLAG_SECTOR_GROUNDCREATURE)
	{
		// Find the object at the ground creatures' position.
		OBJECT_MAP::const_iterator iObject = m_mapObject.lower_bound(POSITION_GROUNDCREATURE);

		// If there is one and it is a ground creature..
		if (iObject!=m_mapObject.end()
			&& iObject->first <= POSITION_GROUNDCREATURE_MAX)
		{
			return iObject;
		}
	}

	return m_mapObject.end();
}

//----------------------------------------------------------------------
// Get FlyingCreature
//----------------------------------------------------------------------
// Returns it when the flag says there is a flying creature.
//----------------------------------------------------------------------
OBJECT_MAP::const_iterator
MSector::GetFlyingCreatureIterator() const
{
	// If there is a flying creature...
	if (m_fProperty & FLAG_SECTOR_FLYINGCREATURE)
	{
		// Find the object at the flying creatures' position.
		OBJECT_MAP::const_iterator iObject = m_mapObject.lower_bound(POSITION_FLYINGCREATURE);

		if (iObject!=m_mapObject.end()
			&& iObject->first <= POSITION_FLYINGCREATURE_MAX)
		{
			return iObject;
		}
	}

	return m_mapObject.end();
}

//----------------------------------------------------------------------
// Get Creature
//----------------------------------------------------------------------
// Returns it when the flag says there is a creature.
//----------------------------------------------------------------------
OBJECT_MAP::iterator
MSector::GetCreatureIterator()
{
	// If there is any creature...
	if (IsExistAnyCreature())
	{
		// Find the object at the underground creatures' position.
		OBJECT_MAP::iterator iObject = m_mapObject.lower_bound(POSITION_UNDERGROUNDCREATURE);

		if (iObject!=m_mapObject.end()
			&& iObject->first <= POSITION_FLYINGCREATURE_MAX)
		{
			return iObject;
		}
	}

	return m_mapObject.end();
}

//----------------------------------------------------------------------
// Get UndergroundCreature
//----------------------------------------------------------------------
// Returns it when the flag says there is an underground creature.
//----------------------------------------------------------------------
OBJECT_MAP::iterator
MSector::GetUndergroundCreatureIterator()
{
	// If there is an underground creature...
	if (m_fProperty & FLAG_SECTOR_UNDERGROUNDCREATURE)
	{
		// Find the object at the underground creatures' position.
		OBJECT_MAP::iterator iObject = m_mapObject.lower_bound(POSITION_UNDERGROUNDCREATURE);

		// If there is one and it is an underground creature..
		if (iObject!=m_mapObject.end()
			&& iObject->first <= POSITION_UNDERGROUNDCREATURE_MAX)
		{
			return iObject;
		}
	}

	return m_mapObject.end();
}

//----------------------------------------------------------------------
// Get Creature
//----------------------------------------------------------------------
// Returns it when the flag says there is a ground creature.
//----------------------------------------------------------------------
OBJECT_MAP::iterator
MSector::GetGroundCreatureIterator()
{
	// If there is a ground creature...
	if (m_fProperty & FLAG_SECTOR_GROUNDCREATURE)
	{
		// Find the object at the ground creatures' position.
		OBJECT_MAP::iterator iObject = m_mapObject.lower_bound(POSITION_GROUNDCREATURE);

		// If there is one and it is a ground creature..
		if (iObject!=m_mapObject.end()
			&& iObject->first <= POSITION_GROUNDCREATURE_MAX)
		{
			return iObject;
		}
	}

	return m_mapObject.end();
}

//----------------------------------------------------------------------
// Get FlyingCreature
//----------------------------------------------------------------------
// Returns it when the flag says there is a flying creature.
//----------------------------------------------------------------------
OBJECT_MAP::iterator
MSector::GetFlyingCreatureIterator()
{
	// If there is a flying creature...
	if (m_fProperty & FLAG_SECTOR_FLYINGCREATURE)
	{
		// Find the object at the flying creatures' position.
		OBJECT_MAP::iterator iObject = m_mapObject.lower_bound(POSITION_FLYINGCREATURE);

		if (iObject!=m_mapObject.end()
			&& iObject->first <= POSITION_FLYINGCREATURE_MAX)
		{
			return iObject;
		}
	}

	return m_mapObject.end();
}

//----------------------------------------------------------------------
// Get Creature
//----------------------------------------------------------------------
// Returns it when the flag says there is a creature.
//----------------------------------------------------------------------
MCreature *
MSector::GetCreature() const
{
	// If there is any creature...
	if (IsExistAnyCreature())
	{
		// Find the object at the underground creatures' position.
		OBJECT_MAP::const_iterator iObject = m_mapObject.lower_bound(POSITION_UNDERGROUNDCREATURE);

		if (iObject!=m_mapObject.end()
			&& iObject->first <= POSITION_FLYINGCREATURE_MAX)
		{
			return CreatureState::CreatureOf(iObject->second);
		}
	}

	return NULL;
}

//----------------------------------------------------------------------
// Get UndergroundCreature
//----------------------------------------------------------------------
// Returns it when the flag says there is an underground creature.
//----------------------------------------------------------------------
MCreature *
MSector::GetUndergroundCreature() const
{
	// If there is an underground creature...
	if (m_fProperty & FLAG_SECTOR_UNDERGROUNDCREATURE)
	{
		// Find the object at the underground creatures' position.
		OBJECT_MAP::const_iterator iObject = m_mapObject.lower_bound(POSITION_UNDERGROUNDCREATURE);

		// If there is one and it is an underground creature..
		if (iObject!=m_mapObject.end()
			&& iObject->first <= POSITION_UNDERGROUNDCREATURE_MAX)
		{
			return CreatureState::CreatureOf(iObject->second);
		}
	}

	return NULL;
}

//----------------------------------------------------------------------
// Get Creature
//----------------------------------------------------------------------
// Returns it when the flag says there is a ground creature.
//----------------------------------------------------------------------
MCreature *
MSector::GetGroundCreature() const
{
	// If there is a ground creature...
	if (m_fProperty & FLAG_SECTOR_GROUNDCREATURE)
	{
		// Find the object at the ground creatures' position.
		OBJECT_MAP::const_iterator iObject = m_mapObject.lower_bound(POSITION_GROUNDCREATURE);

		// If there is one and it is a ground creature..
		if (iObject!=m_mapObject.end()
			&& iObject->first <= POSITION_GROUNDCREATURE_MAX)
		{
			return CreatureState::CreatureOf(iObject->second);
		}
	}

	return NULL;
}

//----------------------------------------------------------------------
// Get FlyingCreature
//----------------------------------------------------------------------
// Returns it when the flag says there is a flying creature.
//----------------------------------------------------------------------
MCreature *
MSector::GetFlyingCreature() const
{
	// If there is a flying creature...
	if (m_fProperty & FLAG_SECTOR_FLYINGCREATURE)
	{
		// Find the object at the flying creatures' position.
		OBJECT_MAP::const_iterator iObject = m_mapObject.lower_bound(POSITION_FLYINGCREATURE);

		if (iObject!=m_mapObject.end()
			&& iObject->first <= POSITION_FLYINGCREATURE_MAX)
		{
			return CreatureState::CreatureOf(iObject->second);
		}
	}

	return NULL;
}

//----------------------------------------------------------------------
// Get ImageObject
//----------------------------------------------------------------------
// When there are image objects,
// returns the one with the ID wanted.
//----------------------------------------------------------------------
MImageObject*
MSector::GetImageObject(TYPE_OBJECTID id) const
{
	// If there are image objects...
	if (m_nImageObject)
	{
		// Find the object at the image objects' position.
		OBJECT_MAP::const_iterator iObject = m_mapObject.find(POSITION_IMAGEOBJECT);

		// None: return NULL.
		if (iObject == m_mapObject.end())
			return NULL;

		// If there are, compare as many as there are..
		// and return the one whose ID is id.
		for (BYTE i=0; i<m_nImageObject; i++)
		{
			// The same ID: return it.
			if ( ((*iObject).second)->GetID() == id )
				return (MImageObject*)((*iObject).second);

			// The next image object
			iObject++;
		}

	}

	return NULL;
}


//----------------------------------------------------------------------
//
//                       Remove Object
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Remove Item
//----------------------------------------------------------------------
// Removes the item, if there is one, clears its flag
// and hands it back in pItem.
//----------------------------------------------------------------------
bool
MSector::RemoveItem(MItem*& pItem)
{
	// If there is an item
	if (m_fProperty & FLAG_SECTOR_ITEM)
	{
		// Find the object at the item's position.
		OBJECT_MAP::iterator iObject = m_mapObject.find(POSITION_ITEM);

		// None: return false
		if (iObject == m_mapObject.end())
			return false;

		// If there is, store the object in pItem to hand it back.
		pItem =  (MItem*)((*iObject).second);

		// Remove iObject.
		m_mapObject.erase( iObject );

		// Clear the item flag (and a sweeper corpse's ground block).
		if (IsBlockingCorpse(pItem))
			m_fProperty &= ~FLAG_SECTOR_BLOCK_GROUND;

		m_fProperty &= ~FLAG_SECTOR_ITEM;

		return true;
	}

	// None
	return false;
}

//----------------------------------------------------------------------
// Remove Item(id, &Item)
//----------------------------------------------------------------------
// Removes the item if its ID is id, clears its flag
// and hands it back in pItem (which is set to the item even when its
// ID differs).
//----------------------------------------------------------------------
bool
MSector::RemoveItem(TYPE_OBJECTID id, MItem*& pItem)
{
	// If there is an item
	if (m_fProperty & FLAG_SECTOR_ITEM)
	{
		// Find the object at the item's position.
		OBJECT_MAP::iterator iObject = m_mapObject.find(POSITION_ITEM);

		// None: return false
		if (iObject == m_mapObject.end())
			return false;

		pItem =  (MItem*)((*iObject).second);

		// If there is one,
		// when its ID matches,
		// store the object in pItem to hand it back.
		if (((*iObject).second)->GetID() == id)
		{
			// Remove iObject.
			m_mapObject.erase( iObject );

			// Clear the item flag (and a sweeper corpse's ground block).
			if (IsBlockingCorpse(pItem))
				m_fProperty &= ~FLAG_SECTOR_BLOCK_GROUND;

			m_fProperty &= ~FLAG_SECTOR_ITEM;

			return true;
		}
	}

	// None
	return false;
}

//----------------------------------------------------------------------
// Remove Creature(id, &Creature)
//----------------------------------------------------------------------
// Removes the creature (of any kind) whose ID is id, clears its flag
// and hands it back in pCreature.
//----------------------------------------------------------------------
bool
MSector::RemoveCreature(TYPE_OBJECTID id, MCreature*& pCreature)
{
	if (IsExistAnyCreature())
	{
		// Find the object at the underground creatures' position.
		OBJECT_MAP::iterator iObject = GetCreatureIterator();

		while (iObject!=m_mapObject.end()
				&& iObject->first <= POSITION_FLYINGCREATURE_MAX)
		{
			pCreature = CreatureState::CreatureOf(iObject->second);

			// If there is one,
			// when its ID matches,
			// store the object in pCreature to hand it back.
			if (iObject->second->GetID() == id)
			{
				// Underground
				if (iObject->first <= POSITION_UNDERGROUNDCREATURE_MAX)
				{
					if (--m_nUndergroundCreature==0)
					{
						m_fProperty &= ~FLAG_SECTOR_UNDERGROUNDCREATURE;
					}
				}
				// Ground
				else if (iObject->first <= POSITION_GROUNDCREATURE_MAX)
				{
					if (--m_nGroundCreature==0)
					{
						m_fProperty &= ~FLAG_SECTOR_GROUNDCREATURE;
					}
				}
				// Flying
				else
				{
					if (--m_nFlyingCreature==0)
					{
						m_fProperty &= ~FLAG_SECTOR_FLYINGCREATURE;
					}
				}

				// Remove iObject.
				m_mapObject.erase( iObject );

				return true;
			}

			iObject ++;
		}
	}

	// None
	pCreature = NULL;

	return false;
}

//----------------------------------------------------------------------
// Remove UndergroundCreature(id, &UndergroundCreature)
//----------------------------------------------------------------------
// Removes the underground creature whose ID is id, clears its flag
// and hands it back in pUndergroundCreature.
//----------------------------------------------------------------------
bool
MSector::RemoveUndergroundCreature(TYPE_OBJECTID id, MCreature*& pUndergroundCreature)
{
	// If there is an underground creature
	if (m_fProperty & FLAG_SECTOR_UNDERGROUNDCREATURE)
	{
		// Find the object at the underground creatures' position.
		OBJECT_MAP::iterator iObject = GetUndergroundCreatureIterator();

		for (int i=0; iObject!=m_mapObject.end() && i<m_nUndergroundCreature; i++)
		{
			pUndergroundCreature = CreatureState::CreatureOf(iObject->second);

			// If there is one,
			// when its ID matches,
			// store the object in pUndergroundCreature to hand it back.
			if (iObject->second->GetID() == id)
			{
				// Remove iObject.
				m_mapObject.erase( iObject );

				// Clear only the underground creature flag.
				if (--m_nUndergroundCreature==0)
				{
					m_fProperty &= ~FLAG_SECTOR_UNDERGROUNDCREATURE;
				}

				return true;
			}

			iObject ++;
		}
	}

	// None
	pUndergroundCreature = NULL;

	return false;
}

//----------------------------------------------------------------------
// Remove Creature(id, &Creature)
//----------------------------------------------------------------------
// Removes the ground creature whose ID is id, clears its flag
// and hands it back in pGroundCreature.
//----------------------------------------------------------------------
bool
MSector::RemoveGroundCreature(TYPE_OBJECTID id, MCreature*& pGroundCreature)
{
	// If there is a ground creature
	if (m_fProperty & FLAG_SECTOR_GROUNDCREATURE)
	{
		// Find the object at the ground creatures' position.
		OBJECT_MAP::iterator iObject = GetGroundCreatureIterator();

		for (int i=0; iObject!=m_mapObject.end() && i<m_nGroundCreature; i++)
		{
			pGroundCreature = CreatureState::CreatureOf(iObject->second);

			// If there is one,
			// when its ID matches,
			// store the object in pGroundCreature to hand it back.
			if (iObject->second->GetID() == id)
			{
				// Remove iObject.
				m_mapObject.erase( iObject );

				// Clear only the ground creature flag.
				if (--m_nGroundCreature==0)
				{
					m_fProperty &= ~FLAG_SECTOR_GROUNDCREATURE;
				}

				return true;
			}

			iObject ++;
		}
	}

	// None
	pGroundCreature = NULL;

	// None
	return false;
}

//----------------------------------------------------------------------
// Remove FlyingCreature(id, &FlyingCreature)
//----------------------------------------------------------------------
// Removes the flying creature whose ID is id, clears its flag
// and hands it back in pFlyingCreature.
//----------------------------------------------------------------------
bool
MSector::RemoveFlyingCreature(TYPE_OBJECTID id, MCreature*& pFlyingCreature)
{
	// If there is a flying creature
	if (m_fProperty & FLAG_SECTOR_FLYINGCREATURE)
	{
		// Find the object at the flying creatures' position.
		OBJECT_MAP::iterator iObject = GetFlyingCreatureIterator();

		for (int i=0; iObject!=m_mapObject.end() && i<m_nFlyingCreature; i++)
		{
			pFlyingCreature = CreatureState::CreatureOf(iObject->second);

			// If there is one,
			// when its ID matches,
			// store the object in pFlyingCreature to hand it back.
			if (iObject->second->GetID() == id)
			{
				// Remove iObject.
				m_mapObject.erase( iObject );

				// Clear only the flying creature flag.
				if (--m_nFlyingCreature==0)
				{
					m_fProperty &= ~FLAG_SECTOR_FLYINGCREATURE;
				}

				return true;
			}

			iObject ++;
		}
	}

	// None
	pFlyingCreature = NULL;

	// None
	return false;
}

//----------------------------------------------------------------------
// Remove ImageObject(id, &ImageObject)
//----------------------------------------------------------------------
// Removes the image object whose ID is id,
// decrements m_nImageObject
// and hands it back in pImageObject.
//----------------------------------------------------------------------
bool
MSector::RemoveImageObject(TYPE_OBJECTID id, MImageObject*& pImageObject)
{
	// If there are image objects
	if (m_nImageObject)
	{
		// Find the object at the image objects' position.
		OBJECT_MAP::iterator iObject = m_mapObject.find(POSITION_IMAGEOBJECT);

		// None: return false
		if (iObject == m_mapObject.end())
			return false;

		// If there are, compare every image object's ID..
		// when equal, store the object in pImageObject to hand it back.
		for (BYTE i=0; i<m_nImageObject; i++)
		{
			// The same ID..
			if (((*iObject).second)->GetID() == id)
			{
				pImageObject =  (MImageObject*)((*iObject).second);

				// Remove iObject.
				m_mapObject.erase( iObject );

				// One fewer image object.
				m_nImageObject--;

				return true;
			}

			// The next image object
			iObject++;
		}
	}

	// None
	return false;
}


//----------------------------------------------------------------------
// Remove Item(id)
//----------------------------------------------------------------------
// Removes the item if its ID is id, and clears its flag.
//----------------------------------------------------------------------
bool
MSector::RemoveItem(TYPE_OBJECTID id)
{
	// If there is an item
	if (m_fProperty & FLAG_SECTOR_ITEM)
	{
		// Find the object at the item's position.
		OBJECT_MAP::iterator iObject = m_mapObject.find(POSITION_ITEM);

		// None: return false
		if (iObject == m_mapObject.end())
			return false;

		// If there is one,
		// when its ID matches,
		// store the object in pItem to hand it back.
		if (((*iObject).second)->GetID() == id)
		{
			// Remove iObject.
			MItem* pItem = dynamic_cast<MItem*>(iObject->second);
			if (IsBlockingCorpse(pItem))
				m_fProperty &= ~FLAG_SECTOR_BLOCK_GROUND;

			m_mapObject.erase( iObject );

			// Clear the item flag (and a sweeper corpse's ground block).
			m_fProperty &= ~FLAG_SECTOR_ITEM;

			return true;
		}
	}

	// None
	return false;
}


//----------------------------------------------------------------------
// Remove Creature(id)
//----------------------------------------------------------------------
// Removes the creature (of any kind) whose ID is id, and clears its flag.
//----------------------------------------------------------------------
bool
MSector::RemoveCreature(TYPE_OBJECTID id)
{
	if (IsExistAnyCreature())
	{
		// Find the object at the underground creatures' position.
		OBJECT_MAP::iterator iObject = GetCreatureIterator();

		while (iObject!=m_mapObject.end()
				&& iObject->first <= POSITION_FLYINGCREATURE_MAX)
		{
			// If there is one,
			// when its ID matches,
			// store the object in pCreature to hand it back.
			if (iObject->second->GetID() == id)
			{
				// Underground
				if (iObject->first <= POSITION_UNDERGROUNDCREATURE_MAX)
				{
					if (--m_nUndergroundCreature==0)
					{
						m_fProperty &= ~FLAG_SECTOR_UNDERGROUNDCREATURE;
					}
				}
				// Ground
				else if (iObject->first <= POSITION_GROUNDCREATURE_MAX)
				{
					if (--m_nGroundCreature==0)
					{
						m_fProperty &= ~FLAG_SECTOR_GROUNDCREATURE;
					}

				}
				// Flying
				else
				{
					if (--m_nFlyingCreature==0)
					{
						m_fProperty &= ~FLAG_SECTOR_FLYINGCREATURE;
					}
				}

				// Remove iObject.
				m_mapObject.erase( iObject );

				return true;
			}

			iObject ++;
		}
	}

	// None
	return false;
}

//----------------------------------------------------------------------
// Remove UndergroundCreature(id)
//----------------------------------------------------------------------
// Removes the underground creature whose ID is id, and clears its flag.
//----------------------------------------------------------------------
bool
MSector::RemoveUndergroundCreature(TYPE_OBJECTID id)
{
	// If there is an underground creature
	if (m_fProperty & FLAG_SECTOR_UNDERGROUNDCREATURE)
	{
		// Find the object at the underground creatures' position.
		OBJECT_MAP::iterator iObject = GetUndergroundCreatureIterator();

		for (int i=0; iObject!=m_mapObject.end() && i<m_nUndergroundCreature; i++)
		{
			// If there is one,
			// when its ID matches,
			// store the object in pUndergroundCreature to hand it back.
			if (iObject->second->GetID() == id)
			{
				// Remove iObject.
				m_mapObject.erase( iObject );

				// Clear only the underground creature flag.
				if (--m_nUndergroundCreature==0)
				{
					m_fProperty &= ~FLAG_SECTOR_UNDERGROUNDCREATURE;
				}

				return true;
			}

			iObject ++;
		}
	}

	// None
	return false;
}

//----------------------------------------------------------------------
// Remove Creature(id)
//----------------------------------------------------------------------
// Removes the ground creature whose ID is id, and clears its flag.
//----------------------------------------------------------------------
bool
MSector::RemoveGroundCreature(TYPE_OBJECTID id)
{
	// If there is a ground creature
	if (m_fProperty & FLAG_SECTOR_GROUNDCREATURE)
	{
		// Find the object at the ground creatures' position.
		OBJECT_MAP::iterator iObject = GetGroundCreatureIterator();

		for (int i=0; iObject!=m_mapObject.end() && i<m_nGroundCreature; i++)
		{
			// If there is one,
			// when its ID matches,
			// store the object in pGroundCreature to hand it back.
			if (iObject->second->GetID() == id)
			{
				// Remove iObject.
				m_mapObject.erase( iObject );

				// Clear only the ground creature flag.
				if (--m_nGroundCreature==0)
				{
					m_fProperty &= ~FLAG_SECTOR_GROUNDCREATURE;
				}

				return true;
			}

			iObject ++;
		}
	}

	// None
	return false;
}

//----------------------------------------------------------------------
// Remove FlyingCreature(id)
//----------------------------------------------------------------------
// Removes the flying creature whose ID is id, and clears its flag.
//----------------------------------------------------------------------
bool
MSector::RemoveFlyingCreature(TYPE_OBJECTID id)
{
	// If there is a flying creature
	if (m_fProperty & FLAG_SECTOR_FLYINGCREATURE)
	{
		// Find the object at the flying creatures' position.
		OBJECT_MAP::iterator iObject = GetFlyingCreatureIterator();

		for (int i=0; iObject!=m_mapObject.end() && i<m_nFlyingCreature; i++)
		{
			// If there is one,
			// when its ID matches,
			// store the object in pFlyingCreature to hand it back.
			if (iObject->second->GetID() == id)
			{
				// Remove iObject.
				m_mapObject.erase( iObject );

				// Clear only the flying creature flag.
				if (--m_nFlyingCreature==0)
				{
					m_fProperty &= ~FLAG_SECTOR_FLYINGCREATURE;
				}


				return true;
			}

			iObject ++;
		}
	}


	// None
	return false;
}

//----------------------------------------------------------------------
// Remove ImageObject(id)
//----------------------------------------------------------------------
// Removes the image object
// whose ID is id.
//----------------------------------------------------------------------
bool
MSector::RemoveImageObject(TYPE_OBJECTID id)
{
	// If there are image objects
	if (m_nImageObject)
	{
		// Find the object at the image objects' position.
		OBJECT_MAP::iterator iObject = m_mapObject.find(POSITION_IMAGEOBJECT);

		// None: return false
		if (iObject == m_mapObject.end())
			return false;

		// If there is one,
		// when its ID matches,
		// store the object in pImageObject to hand it back.
		for (BYTE i=0; i<m_nImageObject; i++)
		{
			if (((*iObject).second)->GetID() == id)
			{
				// Remove iObject.
				m_mapObject.erase( iObject );

				m_nImageObject--;

				return true;
			}

			// The next image object
			iObject ++;
		}
	}

	// None
	return false;
}


//----------------------------------------------------------------------
// Remove Item
//----------------------------------------------------------------------
// Removes the item, if there is one, and clears its flag.
//----------------------------------------------------------------------
bool
MSector::RemoveItem()
{
	// If there is an item
	if (m_fProperty & FLAG_SECTOR_ITEM)
	{
		// Find the object at the item's position.
		OBJECT_MAP::iterator iObject = m_mapObject.find(POSITION_ITEM);

		// None: return false
		if (iObject == m_mapObject.end())
			return false;

		MItem* pItem = dynamic_cast<MItem*>(iObject->second);
		if (IsBlockingCorpse(pItem))
			m_fProperty &= ~FLAG_SECTOR_BLOCK_GROUND;

		// Remove it
		m_mapObject.erase( iObject );

		// Clear the item flag (and a sweeper corpse's ground block).
		m_fProperty &= ~FLAG_SECTOR_ITEM;

		return true;
	}

	// None
	return false;
}

//----------------------------------------------------------------------
//
//                      Iterators
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// ImageObject Iterator
//----------------------------------------------------------------------
// When there are image objects, the iterator to the first
// one.
//----------------------------------------------------------------------
OBJECT_MAP::const_iterator
MSector::GetImageObjectIterator() const
{
	if (m_nImageObject)
	{
		// Find the object at the image objects' position.
		return m_mapObject.find(POSITION_IMAGEOBJECT);
	}

	return m_mapObject.end();
}


//----------------------------------------------------------------------
//
//                       The effect list
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Add Effect
//----------------------------------------------------------------------
// Adds the effect at the end of the effect list.
//----------------------------------------------------------------------
void
MSector::AddEffect(const MEffect* pEffect)
{
	// Only a non-NULL one is added.
	if (pEffect!=NULL)
	{
		m_listEffect.push_back((MEffect*)pEffect);

		// Check for darkness.
		if (!m_bDarkness || !m_bSanctuary)	// [new skill]
		{
			int fid = pEffect->GetFrameID();
			int est = ::GetEffectSpriteType( (BLT_TYPE)pEffect->GetBltType(), fid );

//			if(est >= EFFECTSPRITETYPE_MAP_BLACK_LARGE_SMOKE &&
//				est >= EFFECTSPRITETYPE_MAP_BLACK_SMALL_SMOKE_3)
//				m_bDarkness = true;
			// Darkness
			if (est>=EFFECTSPRITETYPE_DARKNESS_2_1
					&& est<=EFFECTSPRITETYPE_DARKNESS_2_5)
			{
				m_bDarkness = true;
			}

			if( est>= EFFECTSPRITETYPE_GRAY_DARKNESS_2_1 &&
				est<= EFFECTSPRITETYPE_GRAY_DARKNESS_2_5 )
			{
				m_bDarkness = true;
			}

			// Sanctuary
			if (est>=EFFECTSPRITETYPE_SANCTUARY_1
					&& est<=EFFECTSPRITETYPE_SANCTUARY_3)
			{
				m_bSanctuary = true;
			}

		}
	}
}

//----------------------------------------------------------------------
// Get Effect
//----------------------------------------------------------------------
// Finds the effect whose ID is id.
//----------------------------------------------------------------------
MEffect*
MSector::GetEffect(TYPE_OBJECTID id) const
{
	EFFECT_LIST::const_iterator iEffect = m_listEffect.begin();

	while (iEffect != m_listEffect.end())
	{
		// Is it the ID wanted?
		if ((*iEffect)->GetID()==id)
		{
			return *iEffect;
		}

		iEffect++;
	}

	return NULL;
}


//----------------------------------------------------------------------
// Remove Effect(id)
//----------------------------------------------------------------------
// Removes the effect whose ID is id from the list.
//----------------------------------------------------------------------
bool
MSector::RemoveEffect(TYPE_OBJECTID id)
{
	EFFECT_LIST::iterator iEffect = m_listEffect.begin();

	while (iEffect != m_listEffect.end())
	{
		MEffect* pEffect = *iEffect;

		// Is it the ID wanted?
		if (pEffect->GetID()==id)
		{
			// Remove it.
			m_listEffect.erase( iEffect );

			// Check for darkness.
			if (m_bDarkness || m_bSanctuary)	// [new skill]
			{
				int fid = pEffect->GetFrameID();
				int est = ::GetEffectSpriteType( (BLT_TYPE)pEffect->GetBltType(), fid );

//				if(est >= EFFECTSPRITETYPE_MAP_BLACK_LARGE_SMOKE &&
//					est <= EFFECTSPRITETYPE_MAP_BLACK_SMALL_SMOKE_3
//					)
//				{
//					m_bDarkness = false;
//				}
				// Darkness
				if (est>=EFFECTSPRITETYPE_DARKNESS_2_1
						&& est<=EFFECTSPRITETYPE_DARKNESS_2_5)
				{
					m_bDarkness = false;
				}

				if (est>=EFFECTSPRITETYPE_GRAY_DARKNESS_2_1
						&& est<=EFFECTSPRITETYPE_GRAY_DARKNESS_2_5)
				{
					m_bDarkness = false;
				}

				// Sanctuary
				if (est>=EFFECTSPRITETYPE_SANCTUARY_1
						&& est<=EFFECTSPRITETYPE_SANCTUARY_3)
				{
					m_bSanctuary = false;
				}
			}

			return true;
		}

		iEffect++;
	}

	return false;
}

//----------------------------------------------------------------------
// Remove Effect(id, &pEffect)
//----------------------------------------------------------------------
// Removes the effect whose ID is id from the list
// and hands it back.
//----------------------------------------------------------------------
bool
MSector::RemoveEffect(TYPE_OBJECTID id, MEffect*& pEffect)
{
	EFFECT_LIST::iterator iEffect = m_listEffect.begin();

	while (iEffect != m_listEffect.end())
	{
		MEffect* pCur = *iEffect;

		// Is it the ID wanted?
		if (pCur->GetID()==id)
		{
			// Hand the removed effect back to the caller.
			pEffect = pCur;

			// Remove it.
			m_listEffect.erase( iEffect );

			// Check for darkness.
			if (m_bDarkness || m_bSanctuary)	// [new skill]
			{
				int fid = pEffect->GetFrameID();
				int est = ::GetEffectSpriteType( (BLT_TYPE)pEffect->GetBltType(), fid );

				// Darkness
				if (est>=EFFECTSPRITETYPE_DARKNESS_2_1
						&& est<=EFFECTSPRITETYPE_DARKNESS_2_5)
				{
					m_bDarkness = false;
				}

				if (est>=EFFECTSPRITETYPE_GRAY_DARKNESS_2_1
						&& est<=EFFECTSPRITETYPE_GRAY_DARKNESS_2_5)
				{
					m_bDarkness = false;
				}

				// Sanctuary
				if (est>=EFFECTSPRITETYPE_SANCTUARY_1
						&& est<=EFFECTSPRITETYPE_SANCTUARY_3)
				{
					m_bSanctuary = false;
				}
			}

			return true;
		}

		iEffect++;
	}

	return false;
}

//----------------------------------------------------------------------
// Add Portal ( zoneID )
//----------------------------------------------------------------------
// One already there is not added again.
// (cut and paste)
//----------------------------------------------------------------------
bool
MSector::AddPortal(int type, int zoneID)
{
	PORTAL_LIST::iterator iData = m_listPortal.begin();

	// Compare with everything already in the list
	// and add it when none is equal.
	while (iData != m_listPortal.end())
	{
		// When the current one is greater than the new one,
		// add it here.
		if (iData->ZoneID > zoneID)
		{
			m_listPortal.insert(iData, PORTAL_INFO(type, zoneID));

			m_fProperty |= FLAG_SECTOR_PORTAL;

			return true;
		}

		// When the value is already there,
		// do not add it.
		if (iData->ZoneID==zoneID)
		{
			return false;
		}

		iData++;
	}

	// Greater than everything in the list,
	// so add it at the end.
	m_listPortal.push_back( PORTAL_INFO(type, zoneID) );

	m_fProperty |= FLAG_SECTOR_PORTAL;

	return true;
}

//----------------------------------------------------------------------
// Clear Portal ( zoneID )
//----------------------------------------------------------------------
void
MSector::ClearPortal()
{
	m_listPortal.clear();

	m_fProperty &= ~FLAG_SECTOR_PORTAL;
}

//----------------------------------------------------------------------
// Add SectorSound (SECTORSOUND_INFO&)
//----------------------------------------------------------------------
bool
MSector::AddSectorSound(const SECTORSOUND_INFO& info)
{
	return AddSectorSound( info.ZoneSoundID, info.X, info.Y );
}

//----------------------------------------------------------------------
// Add SectorSound ( ZoneSoundID, Xo, Yo )
//----------------------------------------------------------------------
// One already there is not added again.
// (cut and paste)
//----------------------------------------------------------------------
bool
MSector::AddSectorSound(int zoneSoundID, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	SECTORSOUND_LIST::iterator iData = m_listSectorSound.begin();

	// Compare with everything already in the list
	// and add it when none is equal.
	while (iData != m_listSectorSound.end())
	{
		// When the current one is greater than the new one,
		// add it here.
		if (iData->ZoneSoundID > zoneSoundID)
		{
			m_listSectorSound.insert(iData, SECTORSOUND_INFO(zoneSoundID, static_cast<unsigned char>(x), static_cast<unsigned char>(y)));

			return true;
		}

		// When the value is already there,
		// do not add it.
		if (iData->ZoneSoundID==zoneSoundID)
		{
			return false;
		}

		iData++;
	}

	// Greater than everything in the list,
	// so add it at the end.
	m_listSectorSound.push_back( SECTORSOUND_INFO(zoneSoundID, static_cast<unsigned char>(x), static_cast<unsigned char>(y)) );

	return true;
}

//----------------------------------------------------------------------
// Clear SectorSound ( zoneID )
//----------------------------------------------------------------------
void
MSector::ClearSectorSound()
{
	m_listSectorSound.clear();
}