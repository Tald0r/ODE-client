//----------------------------------------------------------------------
// MSector.h
//----------------------------------------------------------------------
//
// - It uses an object map.
//
// - Effects are objects too, but structurally..
//   they are better kept apart, in a list..
//
//----------------------------------------------------------------------
// Keeping flags for the objects now in the sector
// makes the checks fast.
//----------------------------------------------------------------------
// A sector can hold several image objects.
// But given the map's layout, one added while the game runs must not be removed.
//
//
// (A map rather than a list.)
//
// Objects are added to and removed from the map by key.
//----------------------------------------------------------------------
//
// The map's keys are defined as follows.
//
//	1			POSITION_ITEM = 1,					// the item's node
//	2			POSITION_UNDERGROUNDCREATURE,		// the underground creatures' nodes
//	3			POSITION_GROUNDCREATURE,			// the ground creatures' nodes
//	4			POSITION_FLYINGCREATURE,			// the flying creatures' nodes
//	5			POSITION_PORTAL,					// where a zone change happens
//	100 ~255	POSITION_IMAGEOBJECT				// the image objects' nodes
//
//
//----------------------------------------------------------------------
//
// [Sight]
//
// m_Light is the brightness of the light shown on this sector.
// At 0 the sector is drawn in fog (m_FilterSpriteID).
// At 1 or more there is no fog.
//
// m_Light is not just true or false because..
// a sector can hold several lights at once,
// and removing overlapping lights several times..
// could reach 0 while a light is still there.
//
// E.g. [added] a fixed object + the player's sight + a magic effect = 3 lights
//     1. the fixed object removed = 2 lights
//     2. the magic effect removed = 1 light
//     3. the player's sight removed = 0 lights
//     4. from now on the fog is drawn with FilterSpriteID.
//
//
// = Should the light of sectors, MTopView and creatures go from BYTE to short?
//   - Too many lights overlapping on one sector is a problem.
//
//----------------------------------------------------------------------


#ifndef	__MSECTOR_H__
#define	__MSECTOR_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include "DrawTypeDef.h"
#include "MTypeDef.h"
#include "MObject.h"
#include "SectorSoundInfo.h"

#include <fstream>
#include <map>

class MItem;
class MCreature;
class MImageObject;
class MEffect;

//----------------------------------------------------------------------
// Flag
//----------------------------------------------------------------------
#define	FLAG_SECTOR_BLOCK_UNDERGROUND		0x01	// no passing underground
#define	FLAG_SECTOR_BLOCK_GROUND			0x02	// no passing on the ground
#define	FLAG_SECTOR_BLOCK_FLYING			0x04	// no passing in the air
#define	FLAG_SECTOR_BLOCK_ALL				0x07	// Block ALL
#define	FLAG_SECTOR_ITEM					0x08	// an item is there
#define	FLAG_SECTOR_UNDERGROUNDCREATURE		0x10	// an underground creature is there
#define	FLAG_SECTOR_GROUNDCREATURE			0x20	// a ground creature is there
#define	FLAG_SECTOR_FLYINGCREATURE			0x40	// a flying creature is there
#define	FLAG_SECTOR_PORTAL					0x80	// where a zone change happens

//----------------------------------------------------------------------
// Flag2
//----------------------------------------------------------------------
#define	FLAG_SECTOR_SAFE_COMMON				0x01	// safe for everyone
#define	FLAG_SECTOR_SAFE_SLAYER				0x02	// safe for slayers only
#define	FLAG_SECTOR_SAFE_VAMPIRE			0x04	// safe for vampires only
#define FLAG_SECTOR_SAFE_NO_PK_ZONE			0x08	// no player killing
#define	FLAG_SECTOR_SAFE_OUSTERS			0x10	// safe for ousters only
#define FLAG_SECTOR_SAFE_ZONE				0x17
// Blocked by the server
#define FLAG_SECTOR_BLOCK_SERVER_UNDERGROUND	0x10
#define FLAG_SECTOR_BLOCK_SERVER_GROUND			0x20
#define FLAG_SECTOR_BLOCK_SERVER_FLYING			0x40


// MObject Map
typedef std::map<BYTE, MObject*>		OBJECT_MAP;

// MEffect List
typedef std::list<MEffect*>		EFFECT_LIST;


//----------------------------------------------------------------------
// PORTAL_INFO
//----------------------------------------------------------------------
class PORTAL_INFO
{
	// type is MPortal's type.
	public :
		int		Type;
		int		ZoneID;

	public :
		PORTAL_INFO(int type, int zoneID)
		{
			Type = type;
			ZoneID = zoneID;
		}
};

// ZoneID List
typedef std::list<PORTAL_INFO>			PORTAL_LIST;

//----------------------------------------------------------------------
//
// The MSector class holds what is in one area
//                 where objects can be.
//                 A sector is drawn as one floor tile.
//
//----------------------------------------------------------------------
class MSector {
	public :
		MSector(TYPE_SPRITEID spriteID=0);
		~MSector();


		//------------------------------------------------
		//
		// file I/O
		//
		//------------------------------------------------
		void	SaveToFile(std::ofstream& file);
		void	LoadFromFile(std::ifstream& file);

		//------------------------------------------------
		//
		// set SpriteID
		//
		//------------------------------------------------
		void				Set(TYPE_SPRITEID spriteID, BYTE fObject=0);
		// get Sprite Info.
		TYPE_SPRITEID		GetSpriteID() const	{ return m_bDisableTileImage?SPRITEID_NULL:m_SpriteID; }

		bool				IsEmpty() const		{ return m_fProperty==0; }

		//------------------------------------------------
		//
		// Block
		//
		//------------------------------------------------
		BYTE	IsBlockAny() const { return (m_fProperty & FLAG_SECTOR_BLOCK_GROUND) ||
											(m_fProperty & FLAG_SECTOR_BLOCK_UNDERGROUND) ||
											(m_fProperty & FLAG_SECTOR_BLOCK_FLYING); }
		// all
		BYTE	IsBlockAll() const	{ return m_fProperty & FLAG_SECTOR_BLOCK_ALL; }
		void	SetBlockAll()		{ m_fProperty |= FLAG_SECTOR_BLOCK_ALL; }
		void	UnSetBlockAll()		{ m_fProperty &= ~FLAG_SECTOR_BLOCK_ALL; }

		// underground
		BYTE	IsBlockUnderground() const	{ return m_fProperty & FLAG_SECTOR_BLOCK_UNDERGROUND; }
		void	SetBlockUnderground()		{ m_fProperty |= FLAG_SECTOR_BLOCK_UNDERGROUND; }
		void	UnSetBlockUnderground()		{ m_fProperty &= ~FLAG_SECTOR_BLOCK_UNDERGROUND; }

		// ground
		BYTE	IsBlockGround() const		{ return m_fProperty & FLAG_SECTOR_BLOCK_GROUND; }
		void	SetBlockGround()			{ m_fProperty |= FLAG_SECTOR_BLOCK_GROUND; }
		void	UnSetBlockGround()			{ m_fProperty &= ~FLAG_SECTOR_BLOCK_GROUND; }

		// flying
		BYTE	IsBlockFlying() const		{ return m_fProperty & FLAG_SECTOR_BLOCK_FLYING; }
		void	SetBlockFlying()			{ m_fProperty |= FLAG_SECTOR_BLOCK_FLYING; }
		void	UnSetBlockFlying()			{ m_fProperty &= ~FLAG_SECTOR_BLOCK_FLYING; }

		// server block underground
		BYTE	IsBlockServerUnderground() const	{ return m_fProperty2 & FLAG_SECTOR_BLOCK_SERVER_UNDERGROUND; }
		void	SetBlockServerUnderground()			{ m_fProperty2 |= FLAG_SECTOR_BLOCK_SERVER_UNDERGROUND; }
		void	UnSetBlockServerUnderground()		{ m_fProperty2 &= ~FLAG_SECTOR_BLOCK_SERVER_UNDERGROUND; }

		// server block Ground
		BYTE	IsBlockServerGround() const	{ return m_fProperty2 & FLAG_SECTOR_BLOCK_SERVER_GROUND; }
		void	SetBlockServerGround()			{ m_fProperty2 |= FLAG_SECTOR_BLOCK_SERVER_GROUND; }
		void	UnSetBlockServerGround()		{ m_fProperty2 &= ~FLAG_SECTOR_BLOCK_SERVER_GROUND; }

		// server block Flying
		BYTE	IsBlockServerFlying() const	{ return m_fProperty2 & FLAG_SECTOR_BLOCK_SERVER_FLYING; }
		void	SetBlockServerFlying()			{ m_fProperty2 |= FLAG_SECTOR_BLOCK_SERVER_FLYING; }
		void	UnSetBlockServerFlying()		{ m_fProperty2 &= ~FLAG_SECTOR_BLOCK_SERVER_FLYING; }


		//------------------------------------------------
		//
		// Are there objects in the sector?
		//
		//------------------------------------------------
		BYTE	IsExistObject() const	{ return !m_mapObject.empty(); }
		BYTE	IsObjectEmpty() const	{ return m_mapObject.empty(); }

		BYTE	IsExistItem() const					{ return m_fProperty & FLAG_SECTOR_ITEM; }
		BYTE	IsExistUndergroundCreature() const	{ return m_fProperty & FLAG_SECTOR_UNDERGROUNDCREATURE; }
		BYTE	IsExistGroundCreature() const		{ return m_fProperty & FLAG_SECTOR_GROUNDCREATURE; }
		BYTE	IsExistFlyingCreature() const		{ return m_fProperty & FLAG_SECTOR_FLYINGCREATURE; }
		BYTE	IsExistImageObject() const			{ return m_nImageObject; }
		BYTE	IsExistAnyCreature() const			{ return (m_fProperty & FLAG_SECTOR_UNDERGROUNDCREATURE) ||
																(m_fProperty & FLAG_SECTOR_GROUNDCREATURE) ||
																(m_fProperty & FLAG_SECTOR_FLYINGCREATURE); }

		// An image object shown on a sector that is not blocked? (drawn translucent)
		//BYTE	IsImageObjectAndNotBlock()	const	{ return (m_nImageObject && (m_fProperty & FLAG_SECTOR_BLOCK)); }


		//------------------------------------------------
		//
		// Add Object to Sector
		//
		//------------------------------------------------
		bool	AddItem(const MItem* pItem);
		bool	AddUndergroundCreature(const MCreature* pCreature);
		bool	AddGroundCreature(const MCreature* pCreature);
		bool	AddFlyingCreature(const MCreature* pCreature);
		bool	AddImageObject(const MImageObject* pImageObject);


		//------------------------------------------------
		//
		// Remove Object from Sector
		//
		//------------------------------------------------
		bool	RemoveItem(MItem*& pItem);
		//bool	RemoveUndergroundCreature(MCreature*& pCreature);
		//bool	RemoveGroundCreature(MCreature*& pCreature);
		//bool	RemoveFlyingCreature(MCreature*& pCreature);

		// ID check and remove
		bool	RemoveItem(TYPE_OBJECTID id, MItem*& pItem);
		bool	RemoveUndergroundCreature(TYPE_OBJECTID id, MCreature*& pCreature);
		bool	RemoveGroundCreature(TYPE_OBJECTID id, MCreature*& pCreature);
		bool	RemoveFlyingCreature(TYPE_OBJECTID id, MCreature*& pCreature);
		bool	RemoveImageObject(TYPE_OBJECTID id, MImageObject*& pImageObject);
		bool	RemoveCreature(TYPE_OBJECTID id, MCreature*& pCreature);

		// ID check and just remove
		bool	RemoveItem(TYPE_OBJECTID id);
		bool	RemoveUndergroundCreature(TYPE_OBJECTID id);
		bool	RemoveGroundCreature(TYPE_OBJECTID id);
		bool	RemoveFlyingCreature(TYPE_OBJECTID id);
		bool	RemoveCreature(TYPE_OBJECTID id);
		bool	RemoveImageObject(TYPE_OBJECTID id);

		// just remove
		bool	RemoveItem();
		//bool	RemoveUndergroundCreature();
		//bool	RemoveGroundCreature();
		//bool	RemoveFlyingCreature();

		// remove all
		void	RemoveAllObject();


		//------------------------------------------------
		//
		// get Object
		//
		//------------------------------------------------
		OBJECT_MAP::iterator		GetObjectEnd()			{ return m_mapObject.end(); }
		OBJECT_MAP::const_iterator	GetObjectEnd() const	{ return m_mapObject.end(); }

		// Object*
		MItem*				GetItem() const;

		BYTE					GetUndergroundCreatureSize() const	{ return m_nUndergroundCreature; }
		BYTE					GetGroundCreatureSize() const		{ return m_nGroundCreature; }
		BYTE					GetFlyingCreatureSize() const		{ return m_nFlyingCreature; }
		int						GetCreatureSize() const				{ return (int)m_nGroundCreature+m_nUndergroundCreature+m_nFlyingCreature; }
		OBJECT_MAP::const_iterator	GetCreatureIterator() const;
		OBJECT_MAP::const_iterator	GetUndergroundCreatureIterator() const;
		OBJECT_MAP::const_iterator	GetGroundCreatureIterator() const;
		OBJECT_MAP::const_iterator	GetFlyingCreatureIterator() const;
		OBJECT_MAP::iterator	GetCreatureIterator();
		OBJECT_MAP::iterator	GetUndergroundCreatureIterator();
		OBJECT_MAP::iterator	GetGroundCreatureIterator();
		OBJECT_MAP::iterator	GetFlyingCreatureIterator();

		MCreature*				GetCreature() const;
		MCreature*				GetUndergroundCreature() const;
		MCreature*				GetGroundCreature() const;
		MCreature*				GetFlyingCreature() const;
		MImageObject*			GetImageObject(TYPE_OBJECTID id) const;

		// How many
		BYTE			GetImageObjectSize() const		{ return m_nImageObject; }

		// The iterator to the first one.
		OBJECT_MAP::const_iterator	GetImageObjectIterator() const;


		// assign operator
		void	operator = (const MSector& sector);

		//------------------------------------------------
		//
		// Effects...
		//
		//------------------------------------------------
		bool		IsExistEffect() const		{ return !m_listEffect.empty(); }
		void		ClearEffect()				{ m_listEffect.clear(); }
		int			GetEffectSize() const		{ return static_cast<int>(m_listEffect.size()); }

		// So callers can walk them with an iterator..
		EFFECT_LIST::const_iterator GetEffectIterator() const { return m_listEffect.begin(); }

		// Add/Get/Remove
		void		AddEffect(const MEffect* pEffect);
		MEffect*	GetEffect(TYPE_OBJECTID id) const;
		bool		RemoveEffect(TYPE_OBJECTID id, MEffect*& pEffect);
		bool		RemoveEffect(TYPE_OBJECTID id);

		// Darkness is tracked apart, to check it fast.
		bool		HasDarkness() const			{ return m_bDarkness; }
		void		SetDarkness()				{ m_bDarkness = true; }
		void		UnSetDarkness()				{ m_bDarkness = false; }

		// So is the earth elemental, to check it fast.
		bool		HasGroundElemental() const			{ return m_bGroundElemental; }
		void		SetGroundElemental()				{ m_bGroundElemental = true; }
		void		UnSetGroundElemental()				{ m_bGroundElemental = false; }

		// And DARKNESS_FORBIDDEN, to check it fast.
		bool		HasDarknessForbidden() const			{ return m_bDarknessForbidden; }
		void		SetDarknessForbidden()				{ m_bDarknessForbidden = true; }
		void		UnSetDarknessForbidden()				{ m_bDarknessForbidden = false; }

		// [new skill] sanctuary too.
		bool		HasSanctuary() const		{ return m_bSanctuary; }
		void		SetSanctuary()				{ m_bSanctuary = true; }
		void		UnSetSanctuary()			{ m_bSanctuary = false; }

		//------------------------------------------------
		// Can Stand ?
		//------------------------------------------------
		bool		CanStandUndergroundCreature() const;
		bool		CanStandGroundCreature() const;
		bool		CanStandFlyingCreature() const;


		//------------------------------------------------
		//
		// Sight and filter
		//
		//------------------------------------------------
		//void			SetFilterSpriteID(TYPE_SPRITEID id)	{ m_FilterSpriteID = id; }
		//TYPE_SPRITEID	GetFilterSpriteID() const			{ return m_FilterSpriteID; }

		// Light..
		BYTE			GetLight() const					{ return m_Light; }
		void			SetLight(BYTE light)				{ m_Light = light; }
		void			AddLight()							{ if (m_Light<0xFF) m_Light++; }
		void			RemoveLight() 						{ if (m_Light>0) m_Light--; }

		//------------------------------------------------
		//
		// Portal
		//
		//------------------------------------------------
		BYTE	IsPortal() const		{ return m_fProperty & FLAG_SECTOR_PORTAL; }
		void	SetPortal()				{ m_fProperty |= FLAG_SECTOR_PORTAL; }
		void	UnSetPortal()			{ m_fProperty &= ~FLAG_SECTOR_PORTAL; }
		int							GetPortalSize() const	{ return static_cast<int>(m_listPortal.size()); }
		PORTAL_LIST::const_iterator	GetPortalBegin() const	{ return m_listPortal.begin(); }
		bool						AddPortal(int type, int zoneID);
		void						ClearPortal();

		//------------------------------------------------
		//
		// SectorSound
		//
		//------------------------------------------------
		const SECTORSOUND_LIST&				GetSectorSoundList() const	{ return m_listSectorSound; }
		int									GetSectorSoundSize() const	{ return static_cast<int>(m_listSectorSound.size()); }
		SECTORSOUND_LIST::const_iterator	GetSectorSoundBegin() const	{ return m_listSectorSound.begin(); }
		bool								AddSectorSound(int zoneSoundID, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
		bool								AddSectorSound(const SECTORSOUND_INFO& info);
		void								ClearSectorSound();

		//------------------------------------------------
		//
		// Safe - safety zones
		//
		//------------------------------------------------
		void				SetSafe(BYTE flag)		{ m_fProperty2 = flag; }
		void				AddSafe(BYTE flag)		{ m_fProperty2 |= flag; }
		void				RemoveSafe(BYTE flag)	{ m_fProperty2 &= ~flag; }
		BYTE				GetSafe() const			{ return m_fProperty2; }
		BYTE				IsSafeSlayer() const	{ return (m_fProperty2 & FLAG_SECTOR_SAFE_COMMON) || (m_fProperty2 & FLAG_SECTOR_SAFE_SLAYER); }
		BYTE				IsSafeVampire() const	{ return (m_fProperty2 & FLAG_SECTOR_SAFE_COMMON) || (m_fProperty2 & FLAG_SECTOR_SAFE_VAMPIRE); }
		BYTE				IsSafeOusters() const	{ return (m_fProperty2 & FLAG_SECTOR_SAFE_COMMON) || (m_fProperty2 & FLAG_SECTOR_SAFE_OUSTERS); }
		BYTE				IsSafeComplete() const	{ return (m_fProperty2 & FLAG_SECTOR_SAFE_COMMON); }

		//------------------------------------------------
		//
		// Visited flag - for path finding
		//
		//------------------------------------------------
		void				SetVisitedFlag()		{ m_bVisitedFlag = true; }
		void				UnSetVisitedFlag()		{ m_bVisitedFlag = false; }
		bool				IsVisitedFlag() const	{ return m_bVisitedFlag; }


		// The earth ousters' level 140 skill, Fury of Gnome
		bool		HasFuryOfGnome() const			{ return m_bFuryOfGnome; }
		void		SetFuryOfGnome()				{ m_bFuryOfGnome = true; }
		void		UnSetFuryOfGnome()				{ m_bFuryOfGnome = false; }

		// For quests - whether the tile is shown
		bool		HasDisableTileImage() const			{ return m_bDisableTileImage; }
		void		SetDisableTileImage()				{ m_bDisableTileImage = true; }
		void		UnSetDisableTileImage()				{ m_bDisableTileImage = false; }
	public :
		//----------------------------------------------------------------------
		// Key Value of Map
		//----------------------------------------------------------------------
		// The values are a little fragile..
		enum POSITION_VALUE
		{
			POSITION_ITEM = 1,					// the item's node

			POSITION_UNDERGROUNDCREATURE=2,		// the underground creatures' nodes (2~14)
			POSITION_UNDERGROUNDCREATURE_MAX=14,

			POSITION_GROUNDCREATURE=15,			// the ground creatures' nodes (15~49)
			POSITION_GROUNDCREATURE_MAX=49,

			POSITION_FLYINGCREATURE=50,			// the flying creatures' nodes (50~79)
			POSITION_FLYINGCREATURE_MAX=79,

			POSITION_PORTAL=80,					// where a zone change happens
			POSITION_IMAGEOBJECT = 81,			// the image objects' nodes (81~149)
			POSITION_EFFECT	= 150				// the effects' nodes (150~255)	// unused
		};




	protected :
		// The tile sprite
		TYPE_SPRITEID		m_SpriteID;

		// The objects that can be in a sector
		OBJECT_MAP			m_mapObject;

		// The effects that can be in a sector
		EFFECT_LIST			m_listEffect;

		// The sprite ID used as the filter
		//TYPE_SPRITEID	m_FilterSpriteID;

		PORTAL_LIST			m_listPortal;

		SECTORSOUND_LIST	m_listSectorSound;

		// How many creatures
		BYTE				m_nGroundCreature;
		BYTE				m_nUndergroundCreature;
		BYTE				m_nFlyingCreature;

		// How many image objects
		BYTE				m_nImageObject;

		// The flags
		BYTE				m_fProperty;

		// The safety zone flags
		BYTE				m_fProperty2;

		// The light
		BYTE				m_Light;


		bool				m_bDarkness;
		bool				m_bSanctuary;		// [new skill]
		bool				m_bGroundElemental;		// [new skill]
		bool				m_bDarknessForbidden;
		bool				m_bVisitedFlag;

		bool				m_bFuryOfGnome;
		bool				m_bDisableTileImage;
};

#endif


