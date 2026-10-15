//----------------------------------------------------------------------
// MZone.h
//----------------------------------------------------------------------
//
// Creature : managed by ID. Moves around wildly. (sigh)
// Item     : managed by ID. It can vanish (creature picks it up) and appear (drop)
// Obstacle : fixed to a Sector. Does not move.
// ImageObject : ??
//
//----------------------------------------------------------------------
// Information stored in the File
//----------------------------------------------------------------------
//
// [ MapData : Filename.MD ]   - loaded by Zone.
// width size, height size
//   Zone ID
// attributes
// width*height Sector infos (TileSpriteID, Flag)
// Obstacle count, Obstacle count * Obstacle info
// ImageObject count, ImageObject count * (ImageObject, ImageObjectSectorInfo)
//
//
//----------------------------------------------------------------------
// On-line Update
//----------------------------------------------------------------------
//
// If the File is managed as one,
// data cannot be added in the middle of the File, so the whole File must be loaded,
// updated in Memory, and the whole File saved.
// (!) there may be a speed problem.
//
// But keeping each Data part in a separate file is messy. (sigh)
// If the data no longer changes once a Zone is created..
// using a single File should be no problem.
//
//----------------------------------------------------------------------

#ifndef	__MZONE_H__
#define	__MZONE_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include <list>
#include <map>
#include <set>
#include <vector>
#include "MTypeDef.h"
#include "MSector.h"
class MObstacle;
class MItem;
class MCreature;
class MEffect;
class MPlayer;
class MImageObject;
#include "MHelicopterManager.h"
#include "../basic/CPositionList.h"
#include "ZoneFileHeader.h"
#include "DelayedSoundQueue.h"

#include <fstream>

#include "PortalFlag.h"
typedef std::list<UI_PORTAL_FLAG> UI_PORTAL_LIST;
typedef std::vector<UI_PORTAL_LIST> UI_PORTAL_VECTOR;

//----------------------------------------------------------------------
//
// The Zone class is a 2D array of Sectors.
//
//----------------------------------------------------------------------
class MZone {
	//--------------------------------------------------------------		
	// typedef
	//--------------------------------------------------------------	
	public :
		// building map
		typedef std::map<TYPE_OBJECTID, MImageObject*>				IMAGEOBJECT_MAP;

		// define IMAGEOBJECT_POSITION_LIST.
		typedef	CPositionList<TYPE_SECTORPOSITION>							IMAGEOBJECT_POSITION_LIST;

		// a map of IMAGEOBJECT_POSITION_LIST.
		typedef	std::map<TYPE_OBJECTID, IMAGEOBJECT_POSITION_LIST*>	IMAGEOBJECT_POSITION_LIST_MAP;


		// creature map
		typedef std::map<TYPE_OBJECTID, MCreature*>		CREATURE_MAP;

		// Item map	
		typedef std::map<TYPE_OBJECTID, MItem*>			ITEM_MAP;

		// Obstacle list
		//typedef std::list<MObstacle*>					OBSTACLE_LIST;

		// Effect list
		typedef std::map<TYPE_OBJECTID, MEffect*>		EFFECT_MAP;

		// 2004, 8, 31, sobeit add start - WaitEffectList
		// Wiait Effect list
		typedef std::list<MEffect*>						WAIT_EFFECT_LIST;
		// 2004, 8, 31, sobeit add end - WaitEffectList
	public :
		MZone(TYPE_SECTORPOSITION width=0, TYPE_SECTORPOSITION height=0);
		~MZone();

		//--------------------------------------------------------------		
		// init / release
		//--------------------------------------------------------------		
		void		Init(TYPE_SECTORPOSITION width, TYPE_SECTORPOSITION height);
		void		Release();
		
		//--------------------------------------------------------------
		// Update
		//--------------------------------------------------------------
		void		Update();

		//--------------------------------------------------------------
		// Release Object
		//--------------------------------------------------------------
		void		ReleaseObject();


		//--------------------------------------------------------------
		// file I/O
		//--------------------------------------------------------------		
		bool		LoadFromFile(std::ifstream& file);

		bool		SaveToFileSectorSound(std::ofstream& file);
		bool		LoadFromFileSectorSound(std::ifstream& file);

		//--------------------------------------------------------------		
		// Zone File Header part
		//--------------------------------------------------------------		
		const MString&	GetVersion() const			{ return m_Info.ZoneVersion; } // last updated date
		WORD			GetID() const				{ return m_Info.ZoneID; }			// zone ID
		WORD			GetGroupID() const			{ return m_Info.ZoneGroupID; }	// zone group ID
		const MString&	GetName() const				{ return m_Info.ZoneName; }		// zone name
		BYTE			GetType() const				{ return m_Info.ZoneType; }		// zone type
		BYTE			GetLevel() const			{ return m_Info.ZoneLevel; }		// level of the monsters in the zone. Between 1 and 10.
		const MString&	GetDescription() const		{ return m_Info.Description; }	// zone description



		//--------------------------------------------------------------
		//
		// Creature
		//
		//--------------------------------------------------------------
		void		SetPlayer();//MPlayer* pPlayer);
		void		RemovePlayer();
		
		// Creature Map : total number of creatures in the Zone
		int			GetCreatureNumber() const		{ return static_cast<int>(m_mapCreature.size()); }
		CREATURE_MAP::const_iterator GetCreatureBegin()		{ return m_mapCreature.begin(); }

		// Move
		void		UpdateAllCreature();

		// Remove Objects outside the player's sight.
		void		KeepObjectInSight(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, BYTE sight);

		// Can it enter sector (x,y)?
		bool		CanMove(BYTE creatureType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
		void		SetServerBlock(BYTE creatureType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
		void		UnSetServerBlock(BYTE creatureType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);

		// Move the Creature at (xo,yo) to (xn,yn).
		bool		MoveGroundCreature(MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn);
		bool		MoveFlyingCreature(MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn);
		bool		MoveUndergroundCreature(MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn);

		// Return the ID of the Creature at Sector (x,y).
		
		TYPE_OBJECTID		GetCreatureID(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
		TYPE_OBJECTID		GetFlyingCreatureID(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);
		TYPE_OBJECTID		GetUndergroundCreatureID(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);

		TYPE_OBJECTID		GetCreatureID(const char* pName, int flag = 0) const;

		// other creatures : Add, Get, Remove
		bool		AddCreature(MCreature* pCreature);	// Add Creature to the Zone
		bool		RemoveCreature(TYPE_OBJECTID	id);			// Remove Creature from the Zone
		MCreature*	GetCreature(TYPE_OBJECTID id);				// Read a Creature of the Zone
		MCreature*	GetCreatureOnly(TYPE_OBJECTID id);

		// 2004, 8, 12 sobeit add start
		MCreature*  GetCreatureBySector(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
		// 2004, 8, 12 sobeit add end
		//MCreature*	GetUndergroundCreature(TYPE_OBJECTID id);				// Read an UndergroundCreature of the Zone
		//MCreature*	GetFlyingCreature(TYPE_OBJECTID id);				// Read a FlyingCreature of the Zone

		// Remove Creature from Sector
		void		RemoveUndergroundCreature(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, TYPE_OBJECTID id)	{ m_ppSector[sY][sX].RemoveUndergroundCreature(id); }
		void		RemoveGroundCreature(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, TYPE_OBJECTID id)		{ m_ppSector[sY][sX].RemoveGroundCreature(id); }
		void		RemoveFlyingCreature(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, TYPE_OBJECTID id)		{ m_ppSector[sY][sX].RemoveFlyingCreature(id); }

		// Hallu related
		void		ChangeToHalluCreature();
		void		RemoveHalluCreature();


		//--------------------------------------------------------------
		//
		//							Item
		//
		//--------------------------------------------------------------
		bool		AddItem(MItem* pItem, BOOL bDropping=false);				// Add Item to the Zone, bDropping tells whether it is starting to drop?
		bool		RemoveItem(TYPE_OBJECTID	id);	// Remove Item from the Zone (deletes it)
		bool		PickupItem(TYPE_OBJECTID	id);	// Remove Item from the Zone
		MItem*		GetItem(TYPE_OBJECTID id);			// Read an Item of the Zone
		MItem*		GetCorpseKilledByMe(int limitItemCount = 0);				// Return one monster I killed in the Zone.
		
		// Return the ID of the Item at Sector (x,y).
		TYPE_OBJECTID		GetItemID(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);	

		// A Creature turning into a corpse... (sigh)
		//bool		AddCorpseFromCreature(MCreature* pCreature);
		bool		AddCorpseFromCreature(TYPE_OBJECTID id);

		// Item Map : total number of Items in the Zone
		int			GetItemNumber() const		{ return static_cast<int>(m_mapItem.size()); }
		ITEM_MAP::const_iterator GetItemBegin()		{ return m_mapItem.begin(); }

		// update item
		void		UpdateItem();


		//--------------------------------------------------------------
		//
		//							ImageObject
		//
		//--------------------------------------------------------------
		bool		AddImageObject(MImageObject* pImageObject);	// Add ImageObject to the Zone
		bool		RemoveImageObject(TYPE_OBJECTID	id);			// Remove ImageObject from the Zone
		MImageObject*	GetImageObject(TYPE_OBJECTID id);				// Read an ImageObject of the Zone
		void		SetImageObjectSector(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, TYPE_OBJECTID id);	// ImageObject Sector
		void		UnSetImageObjectSector(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, TYPE_OBJECTID id);	// Unset ImageObject Sector

		// Add map_viewer access interface
		const IMAGEOBJECT_MAP&	GetImageObjectMap() const { return m_mapImageObject; }	

		//--------------------------------------------------------------
		//
		//							Obstacle
		//
		//--------------------------------------------------------------
		//bool		AddObstacle(MObstacle* pObstacle);	// Add Obstacle to the Zone
		//bool		RemoveObstacle(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY);	// Remove Obstacle from the Zone
		//void		ChangeFrameObstacle();				// Frame change (temporary function!!!)

		//--------------------------------------------------------------
		//
		//							 Effect
		//
		//--------------------------------------------------------------
		// Takes ownership on entry; destroys rejected effects, retains accepted ones.
		bool		AddEffect(MEffect* pEffect, DWORD dwWaitCount = 0);
		bool		RemoveEffect(TYPE_OBJECTID id);
		void		UpdateEffects();				// Frame change (temporary function!!!)
		// 2004, 8, 31, sobeit add start
		void		UpdateWaitEffects();
		// 2004, 8, 31, sobeit add end
		MEffect*	GetEffect(TYPE_OBJECTID id) const;
		int			GetEffectCount()				{ return static_cast<int>(m_mapEffect.size()); }
		EFFECT_MAP::const_iterator GetEffects()		{ return m_mapEffect.begin(); }
		bool		RemoveTileEffect(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int effectStatus, int serverID=0);
		bool		RemoveTileEffect(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, TYPE_EFFECTSPRITETYPE type, int serverID=0);
		void		ChangeSwapViceType(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, TYPE_EFFECTSPRITETYPE type, WORD wDelay = 0xffff);
		void		RemoveSwapViceType();
		//--------------------------------------------------------------
		// Ground Effect - attached to the Tile..
		//--------------------------------------------------------------
		bool		AddGroundEffect(MEffect* pEffect);	// Add Effect to the Zone
		bool		RemoveGroundEffect(TYPE_OBJECTID id);
		MEffect*	GetGroundEffect(TYPE_OBJECTID id) const;
		void		UpdateGroundEffects();				// Frame change (temporary function!!!)
		int			GetGroundEffectCount()				{ return static_cast<int>(m_mapGroundEffect.size()); }
		EFFECT_MAP::const_iterator GetGroundEffects()	{ return m_mapGroundEffect.begin(); }
		EFFECT_MAP::const_iterator GetGroundEffectsEnd()	{ return m_mapGroundEffect.end(); }

		//--------------------------------------------------------------
		//
		//						 Item
		//
		//--------------------------------------------------------------
		void		AddSound(const SOUND_NODE& sound);
		void		UpdateSound();					// sound output


		//--------------------------------------------------------------
		//
		//							Sector
		//
		//--------------------------------------------------------------		
		bool		IsPositionInZone(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)	{ return x<m_Width && y<m_Height; }

		void		SetSector(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, const MSector& sector)	{ m_ppSector[y][x]=sector; }

		void		SetBlockAllSector(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)				{ m_ppSector[sY][sX].SetBlockAll(); }		// make it an All Block sector.
		void		UnSetBlockAllSector(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)			{ m_ppSector[sY][sX].UnSetBlockAll(); }	// make it an All Block sector.

		void		SetBlockUndergroundSector(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)		{ m_ppSector[sY][sX].SetBlockUnderground(); }		// set UndergroundBlock sector
		void		UnSetBlockUndergroundSector(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)	{ m_ppSector[sY][sX].UnSetBlockUnderground(); }	// clear UndergroundBlock sector

		void		SetBlockGroundSector(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)			{ m_ppSector[sY][sX].SetBlockGround(); }	// set Ground Block sector
		void		UnSetBlockGroundSector(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)		{ m_ppSector[sY][sX].UnSetBlockGround(); }	// clear Ground Block sector

		void		SetBlockFlyingSector(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)	{ m_ppSector[sY][sX].SetBlockFlying(); }					// make it a Flying Block sector.
		void		UnSetBlockFlyingSector(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)	{ m_ppSector[sY][sX].UnSetBlockFlying(); }			// make it a Flying Block sector.

		void		SetVisitedFlag(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)				{ m_ppSector[sY][sX].SetVisitedFlag(); }
		void		UnSetVisitedFlag(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)			{ m_ppSector[sY][sX].UnSetVisitedFlag(); }
		bool		IsVisitedFlag(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) const		{ return m_ppSector[sY][sX].IsVisitedFlag(); }
		
		//--------------------------------------------------------------		
		//
		//		get
		//
		//--------------------------------------------------------------		
		TYPE_SECTORPOSITION			GetWidth() const				{ return m_Width; }
		TYPE_SECTORPOSITION			GetHeight()	const				{ return m_Height; }
		const MSector&	GetSector(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) const	{ return m_ppSector[y][x]; }
		//const MPlayer&	GetPlayer()	const				{ return *m_pPlayer; }
	
		long						GetTileFilePosition() const			{ return m_fpTile; }
		long						GetImageObjectFilePosition() const	{ return m_fpImageObject; }

		//--------------------------------------------------------------
		// Get Tile SpriteID
		//--------------------------------------------------------------
		void						GetNearSpriteSet(std::set<TYPE_SPRITEID>& TileSSM, std::set<TYPE_SPRITEID>& ImageObjectSSM, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) const;


		//--------------------------------------------------------------		
		//
		// portal
		//
		//--------------------------------------------------------------		
		void						AddPortal(int type, int zoneID, const RECT& rect);

		//--------------------------------------------------------------		
		//
		// Safe
		//
		//--------------------------------------------------------------		
		void						SetSafeSector( const RECT& safeRect, BYTE fSafe );


		//--------------------------------------------------------------
		//
		// Fake Creature
		//
		//--------------------------------------------------------------
		// Creature Map : total number of creatures in the Zone
		int			GetFakeCreatureNumber() const		{ return static_cast<int>(m_mapFakeCreature.size()); }
		CREATURE_MAP::const_iterator GetFakeCreatureBegin()		{ return m_mapFakeCreature.begin(); }

		// Move
		void		UpdateFakeCreature();

		// other creatures : Add, Get, Remove
		bool		AddFakeCreature(MCreature* pCreature);	// Add Creature to the Zone
		bool		RemoveFakeCreature(TYPE_OBJECTID id);			// Remove Creature from the Zone
		MCreature*	GetFakeCreature(TYPE_OBJECTID id);				// Read a Creature of the Zone
	
		//--------------------------------------------------------------
		// ZoneSound
		//--------------------------------------------------------------
		bool		IsZoneSoundLoaded() const		{ return m_bZoneSoundLoaded; }

		//--------------------------------------------------------------
		// HelicopterManager
		//--------------------------------------------------------------
		bool			AddHelicopter(TYPE_OBJECTID creatureID, int x0, int y0);
		MHelicopter*	GetHelicopter(TYPE_OBJECTID creatureID) const;
		void			RemoveHelicopter(TYPE_OBJECTID creatureID);
		UI_PORTAL_VECTOR &	GetHorn() { return m_horn; }
		
		void			SetCurrentZoneID( WORD id ) { m_Info.ZoneID = id; }			// Ahhh why is the zoneid inside the map!!

		BYTE			GetPKType();

		
	protected :
		//--------------------------------------------------------------
		// Bounds-checked sector lookup.
		//
		// m_ppSector is a bare row-pointer array, and creature and effect
		// coordinates reach the zone straight out of packets, so a
		// subscript driven by one of those has to come through here: off
		// the map it yields NULL instead of a wild row pointer. Sites that
		// index from a loop already bounded by m_Width/m_Height do not
		// need it.
		//--------------------------------------------------------------
		MSector*		SectorAt(int x, int y)
		{
			if (m_ppSector==NULL
				|| x<0 || y<0
				|| x>=(int)m_Width || y>=(int)m_Height)
			{
				return NULL;
			}

			return &m_ppSector[y][x];
		}

		const MSector*	SectorAt(int x, int y) const
		{
			if (m_ppSector==NULL
				|| x<0 || y<0
				|| x>=(int)m_Width || y>=(int)m_Height)
			{
				return NULL;
			}

			return &m_ppSector[y][x];
		}

		// Zone File Header
		FILEINFO_ZONE_HEADER	m_Info;

		TYPE_SECTORPOSITION					m_Width;		// width of the Zone
		TYPE_SECTORPOSITION					m_Height;		// height of the Zone
		MSector**				m_ppSector;		// each Sector

		// Other Creature
		CREATURE_MAP			m_mapCreature;	// other creatures	

		// Other Creature
		ITEM_MAP				m_mapItem;		// Item map

		// ImageObjects
		IMAGEOBJECT_MAP			m_mapImageObject;	// ImageObject map

		// Obstacle
		//OBSTACLE_LIST			m_listObstacle;	// Obstacle List

		// Effects
		EFFECT_MAP				m_mapEffect;	// Effect map

		// Effects
		EFFECT_MAP				m_mapGroundEffect;	// Effect map

		// File Position of the picture info needed by the Zone
		long					m_fpTile;
		long					m_fpImageObject;

		DelayedSoundQueue		m_DelayedSounds;

		// a service creature that runs only on the client (sigh)
		CREATURE_MAP			m_mapFakeCreature;

		// has the sound been loaded.. heh heh
		bool					m_bZoneSoundLoaded;

		MHelicopterManager		m_HelicopterManager;
		UI_PORTAL_VECTOR		m_horn;
		
		// 2004, 8, 31, sobeit add start
		WAIT_EFFECT_LIST		m_listWaitEffect;
		// 2004, 8, 31, sobeit add end
};

// Zone
extern MZone*						g_pZone;

#endif

