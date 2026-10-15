//----------------------------------------------------------------------
// MZone.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include <math.h>
#include <memory>
#include "MZoneDef.h"
#include "ZoneMapData.h"
#include "MSector.h"
#include "MCreature.h"
#include "MFakeCreature.h"
#include "PlayerStateHost.h"
#include "MImageObject.h"
#include "MShadowObject.h"
#include "MAnimationObject.h"
#include "MShadowAnimationObject.h"
#include "MInteractionObject.h"
#include "MPortal.h"
#include "MItem.h"
#include "MEffect.h"
#include "MZone.h"
// Note: MTopView.h removed - MZone doesn't actually use MTopView (decoupled for demo/game flexibility)
#include "GameOptionsHost.h"
#include "MEffectGeneratorTable.h"
#include "MZoneTable.h"
#include "AmbientSoundState.h"
#include "WorldEventsHost.h"
#include "DebugLog.h"
#include "ViewSectors.h"
#include "MViewDef.h"
#include "FrameClock.h"
#include "ZoneFlags.h"
#include "Packet/Properties.h"
#include "EffectSpriteTypeDef.h"
#include "EffectSpriteTypeHelper.h"
#include "SoundDef.h"
#include "MEffectSpriteTypeTable.h"
#include "MHelicopterManager.h"
#include "MParty.h"
#include <fstream>
#include "SkillDef.h"
#include "MNPC.h"
#include "MGuildType.h"
#include "MEffectStatusDef.h"

// Note: VS_UI.h removed - MZone doesn't actually use VS_UI classes (decoupled for demo/game flexibility)
#include "MHelpDef.h"
//#define	new			DEBUG_NEW
//#define	delete		DEBUG_DELETE

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
MZone*				g_pZone				= NULL;


//GameNew Mode  add by sonc 2006.9.27
//end 
#ifdef __METROTECH_TEST__
	extern int		g_iSpeed;
#endif

#ifdef	OUTPUT_DEBUG
	//extern CMessageArray		g_DebugMessage;
	#ifdef __METROTECH_TEST__
//		#define OUTPUT_DEBUG_UPDATE_ZONE
	#endif
//	#define OUTPUT_DEBUG_UPDATE_EFFECT
#endif

// Print info about ImageObject
//#ifdef __OUTPUT_IMAGEOBJECT__

#define	VISION_SECTOR_WIDTH_HALF				12
#define	VISION_SECTOR_HEIGHT_HALF				13

#define NEW_VISION_SECTOR_WIDTH_HALF			10
#define NEW_VISION_SECTOR_HEIGHT_HALF_UP		14
#define NEW_VISION_SECTOR_HEIGHT_HALF_DOWN		14

//----------------------------------------------------------------------
// Is RelicTable [relic fix]
//----------------------------------------------------------------------
bool
IsRelicTable( MItem* pItem )
{
	if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
	{
		MCreature* pCreature = ((MCorpse*)pItem)->GetCreature();
		
		return (pCreature->GetCreatureType()>=371 && pCreature->GetCreatureType()<=376) ||
				(pCreature->GetCreatureType()>=560 && pCreature->GetCreatureType()<=563) ||
				(pCreature->GetCreatureType()>=526 && pCreature->GetCreatureType()<=549);
	}

	return false;
}

//----------------------------------------------------------------------
// define function
//----------------------------------------------------------------------
#define	CheckCreatureInDarkness( sector, x, y )						\
		{															\
			if (PlayerState::Creature()->GetX()==x && PlayerState::Creature()->GetY()==y)		\
			{														\
				PlayerState::Creature()->CheckInDarkness();						\
			}														\
																	\
			int numCreature = sector.GetCreatureSize();				\
																	\
			if (numCreature > 0)									\
			{														\
				OBJECT_MAP::iterator iCreature = sector.GetCreatureIterator();	\
																		\
				for (int i=0; i<numCreature; i++, iCreature ++)			\
				{														\
					if (iCreature==sector.GetObjectEnd()					\
						|| iCreature->first > MSector::POSITION_FLYINGCREATURE_MAX)	\
						break;												\
																			\
					MCreature* pCreature = (MCreature*)iCreature->second;	\
																		\
					if (pCreature!=NULL)								\
					{													\
						pCreature->CheckInDarkness();					\
					}													\
				}														\
			}															\
																		\
			if (sector.IsExistItem())									\
			{															\
				MItem* pItem = sector.GetItem();						\
																		\
				if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)	\
				{														\
					MCreature* pCreature = ((MCorpse*)pItem)->GetCreature();	\
																		\
					if (pCreature!=NULL)								\
					{													\
						pCreature->CheckInDarkness();					\
					}													\
				}														\
			}															\
		}
				
//----------------------------------------------------------------------
// RemoveSectorEffect [new skill 9]
//----------------------------------------------------------------------
#define RemoveSectorEffect( sX, sY, id )						\
		{														\
			MSector& sector = m_ppSector[sY][sX];				\
																\
			bool bSanctuary = sector.HasSanctuary();			\
			sector.RemoveEffect( id );							\
																\
			if (bSanctuary && !sector.HasSanctuary())			\
			{													\
				for (int i=-1; i<=1; i++)						\
				{												\
					for (int j=-1; j<=1; j++)					\
					{											\
						int x0 = sX + i;						\
						int y0 = sY + j;						\
																\
						if ((i==0 && j==0)						\
							|| x0<0 || x0>=m_Width				\
							|| y0<0 || y0>=m_Height)			\
						{										\
							continue;							\
						}										\
																\
						MSector& sector2 = m_ppSector[y0][x0];	\
						sector2.UnSetSanctuary();				\
					}											\
				}												\
			}													\
		}


//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------

MZone::MZone(TYPE_SECTORPOSITION width, TYPE_SECTORPOSITION height)
{
	m_Width = 0;
	m_Height =0;

	m_ppSector	= NULL;
	//m_pPlayer	= NULL;

	Init(width, height);

	m_bZoneSoundLoaded = false;

	DEBUG_ADD("Load HornInfo");

	// Skip horn loading if g_pFileDef is not initialized (e.g., in demo mode)
	if (g_pFileDef != nullptr) {
		int zoneNum, portalNum;
		UI_PORTAL_FLAG portal;
		UI_PORTAL_LIST portalList;

		ifstream hornInfoFile(g_pFileDef->getProperty("FILE_INFO_HORN").c_str(), ios::binary);
		hornInfoFile.read((char *)&zoneNum, sizeof(int));
		for(int zone = 0; zone < zoneNum; zone++)
		{
			hornInfoFile.read((char *)&portalNum, sizeof(int));
			for(int p = 0; p < portalNum; p++)
			{
				hornInfoFile.read((char *)&portal.zone_id, sizeof(int));
				hornInfoFile.read((char *)&portal.x, sizeof(int));
				hornInfoFile.read((char *)&portal.y, sizeof(int));
				hornInfoFile.read((char *)&portal.portal_x, sizeof(int));
				hornInfoFile.read((char *)&portal.portal_y, sizeof(int));
				portalList.push_back(portal);
			}
			m_horn.push_back(portalList);
			portalList.clear();
		}
		hornInfoFile.close();
		DEBUG_ADD("Load HornInfo OK");
	}
}

MZone::~MZone()
{
	Release();
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Init
//----------------------------------------------------------------------
void
MZone::Init(TYPE_SECTORPOSITION width, TYPE_SECTORPOSITION height)
{
	if (width == 0 || height == 0)
		return;

	// Allocate every row before replacing the live grid. A failed allocation
	// must not leave Release() walking uninitialized row pointers.
	std::vector<std::unique_ptr<MSector[]>> ownedRows;
	ownedRows.reserve(height);
	auto rows = std::make_unique<MSector*[]>(height);
	for (unsigned y = 0; y < height; ++y) {
		ownedRows.push_back(std::make_unique<MSector[]>(width));
		rows[y] = ownedRows.back().get();
	}
	Release();
	m_ppSector = rows.release();
	for (auto& row : ownedRows) row.release();
	m_Width = width;
	m_Height = height;
}

//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
MZone::Release()
{
	//---------------------------------
	// remove player position
	//---------------------------------
	//m_pPlayer = NULL;

	DEBUG_ADD( "Release ImageObject" );
	
	//---------------------------------
	// remove ImageObject
	//---------------------------------
	IMAGEOBJECT_MAP::iterator iImageObject = m_mapImageObject.begin();

	MImageObject *pImageObject = NULL;
	while (iImageObject != m_mapImageObject.end())
	{
		pImageObject = (*iImageObject).second;
		if (pImageObject!=NULL)
		{
			delete pImageObject;					
		}
		iImageObject++;
	}
	m_mapImageObject.clear();


	DEBUG_ADD( "Start Release Object" );
	
	//---------------------------------
	// remove objects
	//---------------------------------
	ReleaseObject();

	//---------------------------------
	// remove Obstacle
	//---------------------------------
	/*
	OBSTACLE_LIST::iterator iObstacle = m_listObstacle.begin();

	MObstacle *pObstacle = NULL;
	while (iObstacle != m_listObstacle.end())
	{
		pObstacle = *iObstacle;
		if (pObstacle != NULL)
			delete pObstacle;
		iObstacle++;
	}
	m_listObstacle.clear();
	*/

	DEBUG_ADD_FORMAT( "MZone::Release() - %d x %d", m_Width, m_Height);

	//---------------------------------
	// remove sector
	//---------------------------------	
	if (m_ppSector!=NULL)
	{
		int i;
		for (i=0; i<m_Height; i++)
		{
			delete [] m_ppSector[i];			
		}

		delete [] m_ppSector;		
		
		m_ppSector=NULL;
		m_Width = 0;
		m_Height =0;
	}


	m_bZoneSoundLoaded = false;
	DEBUG_ADD("zone rel ok");
}

//----------------------------------------------------------------------
// Release Object
//----------------------------------------------------------------------
// remove the instance objects.
//----------------------------------------------------------------------
void
MZone::ReleaseObject()
{
	m_HelicopterManager.Release();

	//---------------------------------
	// remove Creatures in the map
	//---------------------------------
	DEBUG_ADD_FORMAT("MZone::ReleaseCreature. size=%d", m_mapCreature.size());
	
	CREATURE_MAP::iterator iCreature;
	iCreature = m_mapCreature.begin();

	MCreature *pCreature = NULL;
	while (iCreature != m_mapCreature.end())
	{
		pCreature = (*iCreature).second;
		// do not delete the player
		if (pCreature!=NULL && pCreature->GetClassType()!=MCreature::CLASS_PLAYER)
		{
			DEBUG_ADD_FORMAT("RemoveCreature: [%s] id=%d, (%d, %d)", pCreature->GetName(), pCreature->GetID(), pCreature->GetX(), pCreature->GetY());
			
			/*
			if (g_pParty!=NULL && pCreature->IsPlayerParty())
			{
				PARTY_INFO* pInfo = g_pParty->GetMemberInfo( GetName() );

				if (pInfo!=NULL)
				{
					pInfo->bInSight = false;					
					pInfo->zoneID = 60002;
				}
			}
			*/
			
			// remove from sector
			if (m_ppSector!=NULL)
			{
				int id = pCreature->GetID();
				int x = pCreature->GetX();
				int y = pCreature->GetY();
				int serverX = pCreature->GetServerX();
				int serverY = pCreature->GetServerY();

				UnSetServerBlock(pCreature->GetMoveType(), serverX, serverY);

				{
					MSector*	pSector			= SectorAt(x, y);
					MSector*	pServerSector	= SectorAt(serverX, serverY);

					//------------------------------------------------
					// Remove it from its sector.
					//------------------------------------------------
					if (pSector==NULL || !pSector->RemoveCreature(id))
					{
						if (pServerSector==NULL || !pServerSector->RemoveCreature(id))
						{
							DEBUG_ADD_FORMAT_WAR("Can't RemoveCreatureWhenRelease! ID=%d client(%d,%d), server(%d,%d)", pCreature->GetID(), x,y,serverX,serverY);
						}
					}

					/*
					bool removed = m_ppSector[y][x].RemoveGroundCreature( id );
					if (!removed)
					{
						removed = m_ppSector[serverY][serverX].RemoveGroundCreature( id );

						if (!removed)
						{
							removed = m_ppSector[y][x].RemoveFlyingCreature( id );

							if (!removed)
							{
								removed = m_ppSector[serverY][serverX].RemoveFlyingCreature( id );

								if (!removed)
								{
									removed = m_ppSector[y][x].RemoveUndergroundCreature( id );

									if (!removed)
									{
										removed = m_ppSector[serverY][serverX].RemoveUndergroundCreature( id );

										DEBUG_ADD_FORMAT("[Error] Can't Remove Creature. ID=%d. xy=(%d, %d), sxy=(%d, %d)", id, x, y, serverX, serverY);
									}
								}
							}
						}
					}
					*/
				}
			}
		
			DEBUG_ADD("RemoveCreature: OK");
			
			delete pCreature;

			DEBUG_ADD("DeleteCreature: OK");
		}
		iCreature++;
	}
	m_mapCreature.clear();

	//---------------------------------
	// remove FakeCreatures in the map
	//---------------------------------
	DEBUG_ADD_FORMAT("MZone::ReleaseFakeCreature. size=%d", m_mapFakeCreature.size());
	
	CREATURE_MAP::iterator iFakeCreature= m_mapFakeCreature.begin();

	while (iFakeCreature != m_mapFakeCreature.end())
	{
		pCreature = (*iFakeCreature).second;
		if (pCreature!=NULL)
		{
			delete pCreature;					
		}
		iFakeCreature++;
	}
	m_mapFakeCreature.clear();


	//---------------------------------
	// remove Items in the map
	//---------------------------------
	DEBUG_ADD_FORMAT("MZone::ReleaseItem. size=%d", m_mapItem.size());
	
	ITEM_MAP::iterator iItem = m_mapItem.begin();

	MItem *pItem = NULL;
	while (iItem != m_mapItem.end())
	{
		pItem = (*iItem).second;
		if (pItem!=NULL)
		{
			// The creature loop above routes its subscripts through SectorAt;
			// this one writes through its sector, so it needs the same bound.
			// The m_ppSector NULL test alone does not cover a position off
			// the map, which an item can carry just as a creature can.
			MSector*	pItemSector = SectorAt(pItem->GetX(), pItem->GetY());

			if (pItemSector!=NULL)
			{
				MSector& sector = *pItemSector;
				sector.RemoveItem();

				// just in case there is a problem.. clean up corpses..
				if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
				{
					sector.UnSetBlockServerGround();

					// [relic fix]
					if (IsRelicTable(pItem))
					{
						sector.UnSetBlockGround();
						sector.UnSetBlockFlying();
					}
				}
			}

			delete pItem;					
		}
		iItem++;
	}
	m_mapItem.clear();

	//---------------------------------
	// remove Effect
	//---------------------------------
	DEBUG_ADD_FORMAT("MZone::ReleaseEffect. size=%d", m_mapEffect.size());
	
	EFFECT_MAP::iterator iEffect = m_mapEffect.begin();

	MEffect *pEffect = NULL;
	while (iEffect != m_mapEffect.end())
	{
		pEffect = iEffect->second;
		if (pEffect != NULL)
		{
			// A chasing effect may be off the map (the zone keeps those): it
			// is in no sector.
			if (SectorAt(pEffect->GetX(), pEffect->GetY())!=NULL)
			{
				// [new skill 9]
				RemoveSectorEffect( pEffect->GetX(), pEffect->GetY(), pEffect->GetID() );				
			}

			delete pEffect;			
		}
		iEffect++;
	}
	m_mapEffect.clear();

	//---------------------------------
	// remove Ground Effect
	//---------------------------------
	DEBUG_ADD_FORMAT("MZone::ReleaseGroundEffect. size=%d", m_mapGroundEffect.size());
	
	iEffect = m_mapGroundEffect.begin();

	while (iEffect != m_mapGroundEffect.end())
	{
		pEffect = iEffect->second;
		if (pEffect != NULL)
		{	
			delete pEffect;			
		}
		iEffect++;
	}
	m_mapGroundEffect.clear();


	//---------------------------------
	// remove Sound
	//---------------------------------
	DEBUG_ADD_FORMAT("MZone::ReleaseSound. size=%d", m_DelayedSounds.GetSize());
	m_DelayedSounds.Clear();

	// 2004, 11, 29, sobeit add start
	//-----------------------------------------------
	// delete wait effect list...
	//-----------------------------------------------
	DEBUG_ADD_FORMAT("MZone::ReleaseWaitEffect. size=%d", m_listWaitEffect.size());
	WAIT_EFFECT_LIST::iterator iWaitEffect = m_listWaitEffect.begin();

	while (iWaitEffect != m_listWaitEffect.end())
	{
		MEffect*	pNode = *iWaitEffect;

		delete pNode;

		iWaitEffect ++;
	}
	m_listWaitEffect.clear();
	// 2004, 11, 29, sobeit add end
	
	DEBUG_ADD("MZone::Release OK");
}

//----------------------------------------------------------------------
// Save To File SectorSound
//----------------------------------------------------------------------
// Width*Height
// SectorSoundInfo
// //MZoneSoundTable
//----------------------------------------------------------------------
bool		
MZone::SaveToFileSectorSound(std::ofstream& file)
{
	//-----------------------------------------------------------
	// size
	//-----------------------------------------------------------
	file.write((const char*)&m_Width, SIZE_SECTORPOSITION);
	file.write((const char*)&m_Height, SIZE_SECTORPOSITION);

	//-----------------------------------------------------------
	// SectorSoundInfo
	//-----------------------------------------------------------
	for (int x=0; x<m_Width; x++)
	{
		for (int y=0; y<m_Height; y++)
		{
			const SECTORSOUND_LIST& listSound = m_ppSector[y][x].GetSectorSoundList();

			SECTORSOUND_LIST::const_iterator iSound = listSound.begin();

			int num = static_cast<int>(listSound.size());

			// save the number of sounds in one Sector
			file.write((const char*)&num, 4);

			// save each sound
			while (iSound != listSound.end())
			{
				const SECTORSOUND_INFO& info = *iSound;

				info.SaveToFile( file );

				iSound++;
			}			
		}
	}

	/*
	//-----------------------------------------------------------
	// MZoneSoundTable
	//-----------------------------------------------------------
	// a bit.. awkward.. (sigh)
	// anyway, for convenience (sigh) we put them together too..
	//-----------------------------------------------------------
	bool bExistZoneTable = (g_pZoneTable!=NULL);

	// for checking..
	file.write((const char*)&bExistZoneTable, 1);

	if (bExistZoneTable)
	{
		g_pZoneTable->SaveToFile( file );

		return true;
	}	

	return false;
	*/

	return true;
}

//----------------------------------------------------------------------
// Load From File SectorSound
//----------------------------------------------------------------------
// check the size with Width*Height, and
// load SectorSoundInfo, and
// // load the MZoneSoundTable.
//----------------------------------------------------------------------
bool		
MZone::LoadFromFileSectorSound(std::ifstream& file)
{
	m_bZoneSoundLoaded = false;

	/*
	//---------------------------------------------------------------
	// load the sound info into the Sectors.
	//---------------------------------------------------------------
	// test code
	for (int x=0; x<m_Width; x++)
	{
		for (int y=0; y<m_Height; y++)
		{
			// ZoneSound number 1 is heard at 50,50, so add the info to every sector
			// actually there is no need to add it unless the distance is close enough.
			// for now, just a test since I could not be bothered.. (sigh)
			//		DEBUG_ADD_FORMAT("AddSectorSound[%d][%d]", y, x);
			
		
			m_ppSector[y][x].AddSectorSound( 1, 30, 30 );		
		}
	}
	*/

	//-----------------------------------------------------------
	// size
	//-----------------------------------------------------------
	//TYPE_SECTORPOSITION width, height;

	//file.read((char*)&width, SIZE_SECTORPOSITION);
	//file.read((char*)&height, SIZE_SECTORPOSITION);

	//if (m_Width!=width || m_Height!=height)
	{
		// when the Zone size and the SectorSound info size differ
		// hopeless (sigh)
	//	return false;
	}

	//-----------------------------------------------------------
	// SectorSoundInfo
	//-----------------------------------------------------------
	//
	// define it as follows..
	//
	// -         : a Sector with no sound
	// A,C,D     : a Sector with 1 SECTORSOUND_INFO (each with a different sound)
	// B         : a Sector with 2 SECTORSOUND_INFO
	// E         : a Sector with 3 SECTORSOUND_INFO
	//   a~z       : SECTORSOUND_INFO
	//
	// take one line as an example..
	//
	// ex) -----AAABBBAAC--DEEEEE---
	//
	//	   2						// repeat count in one line: 2 (-----AAABBBAAC, --DEEEEE )
	//                              // the trailing silent sectors at the end of a line can be ignored.
	//
	//      5						// first repeat. number of silent sectors (-----)
	//       4						// number of sound kinds ( AAA, BBB, AA, C )
	//        {3 { 1 a }}			// 1 info for 3 sectors ( AAA )
	//        {3 { 2 b, b' }}		// 2 infos for 3 sectors ( BBB )
	//        {2 { 1 a }}			// 1 info for 2 sectors ( AA )
	//        {1 { 1 c }}			// 1 info for 1 sector ( C )
	//
	//      2                       // second repeat. number of silent sectors (--)
	//       2						// number of sound kinds ( D, EEEE )
	//        {1 { 1 d }}			// 1 info for 1 sector ( D )
	//        {5 { 3 e, e', e" }}   // 3 infos for 5 sectors ( EEEEE )
	//
	//-----------------------------------------------------------
	BYTE				num;
	SECTORSOUND_INFO	info;

	for (int y=0; y<m_Height; y++)
	{
		//-----------------------------------------------------------
		// repeat count : { number of zeros, number of sound kinds * { info * count } }
		//-----------------------------------------------------------
		file.read((char*)&num, 1);

		int x = 0;
		int numCount = num;
		
		//-----------------------------------------------------------
		// repeat as many times as the repeat count..
		//-----------------------------------------------------------
		for (int i=0; i<numCount; i++)
		{
			//-----------------------------------------------------
			// number of silent sectors
			//-----------------------------------------------------
			file.read((char*)&num, 1);	
			x += num;	// skip coordinates

			//-----------------------------------------------------
			// in the sectors that make sound in this repeat
			// number of kinds of non-consecutive SECTORSOUND_INFO
			//-----------------------------------------------------
			file.read((char*)&num, 1);	
			int numSoundType = num;

			//-----------------------------------------------------
			// repeat for each kind of SECTORSOUND_INFO
			//-----------------------------------------------------
			for (int j=0; j<numSoundType; j++)
			{
				//-----------------------------------------------------
				// how many consecutive sectors have the same info?
				//-----------------------------------------------------
				file.read((char*)&num, 1);		// assert( num > 0 );
				int numSector = num;

				//-----------------------------------------------------
				// how many SECTORSOUND_INFO does the sector have?
				//-----------------------------------------------------
				file.read((char*)&num, 1);		// assert( num > 0 );
				int numSound = num;

				//-----------------------------------------------------
				// while reading the sound, add the info to the consecutive sectors.
				//-----------------------------------------------------
				for (int n=0; n<numSound; n++)
				{
					//-----------------------------------------------------
					// load one SECTORSOUND_INFO
					//-----------------------------------------------------
					info.LoadFromFile( file );
					
					int xPlusNumSector = x + numSector;
					for (int k=x; k<xPlusNumSector; k++)
					{
						m_ppSector[y][k].AddSectorSound( info );
					}
				}

				x += numSector;
			}
		}
	}

	/*
	for (int x=0; x<m_Width; x++)
	{
		for (int y=0; y<m_Height; y++)
		{
			file.read((char*)&num, 1);

			#ifdef _DEBUG
				if (file.eof())
				{
					int a = 0;
				}
			#endif

			intNum = num;

			for (int n=0; n<intNum; n++)
			{
				info.LoadFromFile( file );

				#ifdef _DEBUG
					if (file.eof())
					{
						int a = 0;
					}
				#endif

				m_ppSector[y][x].AddSectorSound( info );			
			}
		}
	}
	*/
	

	//-----------------------------------------------------------
	// MZoneSoundTable
	//-----------------------------------------------------------
	// a bit.. awkward.. (sigh)
	// anyway, for convenience (sigh) we put them together too..
	//-----------------------------------------------------------
	/*
	bool bExistZoneTable;

	// for checking..
	file.read((char*)&bExistZoneTable, 1);

	if (bExistZoneTable)
	{
		g_pZoneTable->LoadFromFile( file );
		
		m_bZoneSoundLoaded = true;

		return true;
	}

	return false;
	*/

	m_bZoneSoundLoaded = true;
	return true;

}

//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
// width size, height size
//   Zone ID
// attributes
// width*height Sector infos (TileSpriteID, Flag)
// Obstacle count, Obstacle count * Obstacle info
// ImageObject count, ImageObject count * (ImageObject, ImageObjectSectorInfo)
//----------------------------------------------------------------------
bool
MZone::LoadFromFile(std::ifstream& file)
{
	ZoneMapData loaded;
	if (!loaded.LoadFromFile(file, false)) {
		DEBUG_ADD_ERR("Invalid or truncated map file");
		return false;
	}

	// Parsing, record ownership and all resource limits are in gamemodel.
	// No live world state changes until the complete map has been accepted.
	Init(loaded.width, loaded.height);
	m_Info = loaded.info;
	m_fpTile = loaded.tileOffset;
	m_fpImageObject = loaded.imageOffset;
	for (unsigned y = 0; y < loaded.height; ++y) {
		for (unsigned x = 0; x < loaded.width; ++x) {
			const auto& data = loaded.sectors[std::size_t(y) * loaded.width + x];
			m_ppSector[y][x].Set(data.sprite, data.property);
			m_ppSector[y][x].SetLight(data.light);
		}
	}
	for (auto& entry : loaded.objects) {
		auto* object = entry.object.get();
		if (!AddImageObject(object)) {
			Release();
			return false;
		}
		entry.object.release(); // The zone map now owns this object.
		for (auto p = entry.positions.GetIterator(); p != entry.positions.GetEnd(); ++p)
			SetImageObjectSector(p->X, p->Y, object->GetID());
	}
	return true;
}

//----------------------------------------------------------------------
// Set Player 
//
// The Player is drawn at the center of the Zone.
// Place the Player on a Sector of the Zone.
//----------------------------------------------------------------------
void		
MZone::SetPlayer()//MPlayer* pPlayer)
{
	// Player Setting
	//m_pPlayer = pPlayer; 

	// Sector Setting
	//m_ppSector[m_pPlayer->GetY()][m_pPlayer->GetX()].AddGroundCreature(pPlayer);
	//m_ppSector[g_pPlayer->GetY()][g_pPlayer->GetX()].AddGroundCreature(&g_pPlayer->;

	// make the Sectors within the Player's sight visible
	//SetSight(g_pPlayer->GetX(), g_pPlayer->GetY(), g_pPlayer->GetLightSight());

	AddCreature( PlayerState::Creature() );
	
	//---------------------------------------------------------------
	// send my info to the party.
	//---------------------------------------------------------------
	PlayerState::SendPositionInfoToParty();
	PlayerState::SendStatusInfoToParty();
}

//----------------------------------------------------------------------
// Set Player 
//
// The Player is drawn at the center of the Zone.
// Place the Player on a Sector of the Zone.
//----------------------------------------------------------------------
void		
MZone::RemovePlayer()//MPlayer* pPlayer)
{
	// Player Setting
	//m_pPlayer = pPlayer; 

	// Sector Setting
	//m_ppSector[m_pPlayer->GetY()][m_pPlayer->GetX()].AddGroundCreature(pPlayer);
	//m_ppSector[g_pPlayer->GetY()][g_pPlayer->GetX()].RemoveGroundCreature(&g_pPlayer->;

	// make the Sectors within the Player's sight visible
	//SetSight(g_pPlayer->GetX(), g_pPlayer->GetY(), g_pPlayer->GetLightSight());

	RemoveCreature( PlayerState::GetID() );
}


//----------------------------------------------------------------------
// Can it enter sector (x,y)?
//----------------------------------------------------------------------
// check every case where it cannot go
// and return false.
//----------------------------------------------------------------------
bool		
MZone::CanMove(BYTE creatureType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	// when outside the Zone area
	if (x>=m_Width || y>=m_Height)
		return false;

	const MSector& sector = m_ppSector[y][x];

	//------------------------------------------------------
	// [new skill] cannot move into Sanctuary.
	//------------------------------------------------------
	if (sector.HasSanctuary()
		&& creatureType != MCreature::CREATURE_FAKE_NO_BLOCK
		&& creatureType != MCreature::CREATURE_FAKE_UNDERGROUND
		&& creatureType != MCreature::CREATURE_FAKE_GROUND
		&& creatureType != MCreature::CREATURE_FAKE_FLYING
		)
	{
		return false;
	}


	// when another Object already exists
	//if (m_ppSector[y][x].IsExistObject())
	{
		// check differently depending on objectType.
		switch (creatureType)
		{
			// UNDERGROUND CREATURE
			case MCreature::CREATURE_UNDERGROUND :
			case MCreature::CREATURE_FAKE_UNDERGROUND :
				return sector.CanStandUndergroundCreature();// || sector.IsPortal();

			// CREATURE
			case MCreature::CREATURE_GROUND :
			case MCreature::CREATURE_FAKE_GROUND :
				return sector.CanStandGroundCreature();// || sector.IsPortal();

			// FLYING CREATURE
			case MCreature::CREATURE_FLYING :
			case MCreature::CREATURE_FAKE_FLYING :
				return sector.CanStandFlyingCreature();// || sector.IsPortal();

			// fake always moves.
			case MCreature::CREATURE_FAKE_NO_BLOCK :
				return true;
		}		
	}

	return true;
}

//----------------------------------------------------------------------
// Set ServerBlock
//----------------------------------------------------------------------
void		
MZone::SetServerBlock(BYTE creatureType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	// when outside the Zone area
	if (x>=m_Width || y>=m_Height)
		return;

	// when another Object already exists
	//if (m_ppSector[y][x].IsExistObject())
	{
		// check differently depending on objectType.
		switch (creatureType)
		{
			// UNDERGROUND CREATURE
			case MCreature::CREATURE_UNDERGROUND :
				m_ppSector[y][x].SetBlockServerUnderground();
			break;

			// CREATURE
			case MCreature::CREATURE_GROUND :
				m_ppSector[y][x].SetBlockServerGround();
			break;

			// FLYING CREATURE
			case MCreature::CREATURE_FLYING :
				m_ppSector[y][x].SetBlockServerFlying();
			break;
		}		
	}
}

//----------------------------------------------------------------------
// UnSet ServerBlock
//----------------------------------------------------------------------
void		
MZone::UnSetServerBlock(BYTE creatureType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	(void)creatureType;
	// when outside the Zone area
	if (x>=m_Width || y>=m_Height)
		return;

	// when another Object already exists
	//if (m_ppSector[y][x].IsExistObject())
	{
		// check differently depending on objectType.
		
		// 2002.3.29
		// release all blocks for now, since something seemed off (sigh)
		/*
		switch (creatureType)
		{
			// UNDERGROUND CREATURE
			case MCreature::CREATURE_UNDERGROUND :
				m_ppSector[y][x].UnSetBlockServerUnderground();
			break;

			// CREATURE
			case MCreature::CREATURE_GROUND :
				m_ppSector[y][x].UnSetBlockServerGround();
			break;

			// FLYING CREATURE
			case MCreature::CREATURE_FLYING :
				m_ppSector[y][x].UnSetBlockServerFlying();
			break;
		}
		*/
		MSector& sector = m_ppSector[y][x];
		sector.UnSetBlockServerUnderground();
		sector.UnSetBlockServerGround();
		sector.UnSetBlockServerFlying();
	}
}


//----------------------------------------------------------------------
// Move Ground Creature
//----------------------------------------------------------------------
// Move the Creature at (xo,yo) to (xn,yn).
//----------------------------------------------------------------------
bool		
MZone::MoveGroundCreature(MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn)
{
	//-------------------------------------------------------
	// no check when outside the Zone area.
	//-------------------------------------------------------
	if (xn>=m_Width || yn>=m_Height) return false;

	// if it is the Player --> do not move.
	if (pCreature->GetClassType()==MCreature::CLASS_PLAYER
		// fake has no meaning of movement.
		|| pCreature->IsFakeCreature())
	{
		return true;
	}

	// if it has already moved..
	//if (xo==xn && yo==yn)
	//	return false;

	
	//MCreature* pExistCreature = NULL;

	int id = pCreature->GetID();

	//------------------------------------------------
	// remove from the sector.
	//------------------------------------------------
	// The cells it is said to be in may lie off the map (a packet's tile
	// goes on unchecked): no sector there to take it from.
	MSector* pOldSector = SectorAt(xo, yo);
	if (pOldSector==NULL || !pOldSector->RemoveCreature(id))
	{
		int xs = pCreature->GetServerX();
		int ys = pCreature->GetServerY();

		MSector* pServerSector = SectorAt(xs, ys);
		if (pServerSector==NULL || !pServerSector->RemoveCreature(id))
		{	
			DEBUG_ADD_FORMAT_WAR("Can't Remove! [Ground] ID=%d (%d,%d) --> (%d,%d)", pCreature->GetID(), xo,yo,xn,yn);
		}
	}

	//------------------------------------------------
	// add to the sector.
	//------------------------------------------------
	if (!m_ppSector[yn][xn].AddGroundCreature( pCreature ))
	{
		DEBUG_ADD_FORMAT_WAR("Can't Add GroundCreature! ID=%d (%d,%d)", pCreature->GetID(), xn,yn);
	}

	
	/*
	if (!m_ppSector[yo][xo].RemoveGroundCreature(id, pExistCreature))
	{	
		if (!m_ppSector[yo][xo].RemoveFlyingCreature(id, pExistCreature))
		{
			if (!m_ppSector[yo][xo].RemoveUndergroundCreature(id, pExistCreature))
			{
				int xs = pCreature->GetServerX();
				int ys = pCreature->GetServerY();

				if (!m_ppSector[ys][xs].RemoveFlyingCreature(id, pExistCreature))
				{	
					if (!m_ppSector[ys][xs].RemoveGroundCreature(id, pExistCreature))
					{
						if (!m_ppSector[ys][xs].RemoveUndergroundCreature(id, pExistCreature))
						{

							int id = 0;
							if (pExistCreature!=NULL)
								id = pExistCreature->GetID();

							// remove failed
							//MessageBox(g_hWnd, "Already Removed![Flying]", NULL, MB_OK);
							DEBUG_ADD_FORMAT("Already Removed! [Ground] ID=%d (%d,%d) --> (%d,%d) ExistID = %d", pCreature->GetID(), xo,yo,xn,yn, id);
						}
					}
				}
			}
		}
	}	

	MCreature* pOldCreature = m_ppSector[yn][xn].GetGroundCreature();

	//------------------------------------------------
	// when nobody is there
	//------------------------------------------------
	if (pOldCreature == NULL)
	{
		// add								
		if (!m_ppSector[yn][xn].AddGroundCreature( pCreature ))
		{
			DEBUG_ADD_FORMAT("Can't Add GroundCreature! ID=%d (%d,%d)", pCreature->GetID(), xn,yn);			
		}
	}
	//------------------------------------------------
	// when another character is already in the new spot...
	//------------------------------------------------
	else
	{
		DEBUG_ADD_FORMAT("Already ExistCreature! [Ground] ID=%d (%d,%d) --> (%d,%d) ExistID = %d", pCreature->GetID(), xo,yo,xn,yn, pOldCreature->GetID());				
		
		// when the character there first is the Player
		//if (pOldCreature->GetID()==g_pPlayer->GetID())
		//{
			// do not add.
			// return to the state before moving.
			// add								
		//	m_ppSector[xo][yo].AddGroundCreature( pCreature );

		//	return false;
		//}
		//else
		{
			//------------------------------------------------
			// remove the character that was there first..
			//------------------------------------------------						
			m_ppSector[yn][xn].RemoveGroundCreature( pOldCreature );

			#ifdef OUTPUT_DEBUG
				if (pOldCreature!=NULL)
					DEBUG_ADD_FORMAT("Remove Creature by New Creature! ID=%d (%d,%d)", pOldCreature->GetID(), xn,yn);
			#endif

			//------------------------------------------------
			// add								
			//------------------------------------------------
			m_ppSector[yn][xn].AddGroundCreature( pCreature );

			// for confirmation
			pCreature->SetX( xn );
			pCreature->SetY( yn );
			
			//------------------------------------------------
			// add the removed character at the correct position.
			//------------------------------------------------
			if (pOldCreature!=NULL)
			{
				int x = pOldCreature->GetServerX();
				int y = pOldCreature->GetServerY();
				
				// if alive.. and the coordinates differ..
				if (x!=xn || y!=yn)
				{
					if (pOldCreature->IsAlive())					
					{				
						pOldCreature->SetStop();
						pOldCreature->MovePosition(	x, y );
					}
					else
					{
						pOldCreature->MovePosition(	x, y );
					}
				}
			}
		}
	}		
	*/

	return true;
}

//----------------------------------------------------------------------
// Move Flying Creature
//----------------------------------------------------------------------
// Move the FlyingCreature at (xo,yo) to (xn,yn).
//----------------------------------------------------------------------
bool		
MZone::MoveFlyingCreature(MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn)
{
	//-------------------------------------------------------
	// no check when outside the Zone area.
	//-------------------------------------------------------
	if (xn>=m_Width || yn>=m_Height) return false;

	// if it is the Player --> do not move.
	if (pCreature->GetClassType()==MCreature::CLASS_PLAYER
		// fake has no meaning of movement.
		|| pCreature->IsFakeCreature())
	{
		return true;
	}

	// if it has already moved..
	//if (xo==xn && yo==yn)
	//	return false;

	//MCreature* pExistCreature = NULL;

	int id = pCreature->GetID();

	//------------------------------------------------
	// remove from the sector.
	//------------------------------------------------
	// The cells it is said to be in may lie off the map (a packet's tile
	// goes on unchecked): no sector there to take it from.
	MSector* pOldSector = SectorAt(xo, yo);
	if (pOldSector==NULL || !pOldSector->RemoveCreature(id))
	{
		int xs = pCreature->GetServerX();
		int ys = pCreature->GetServerY();

		MSector* pServerSector = SectorAt(xs, ys);
		if (pServerSector==NULL || !pServerSector->RemoveCreature(id))
		{	
			DEBUG_ADD_FORMAT_WAR("Can't Remove! [Flying] ID=%d (%d,%d) --> (%d,%d)", pCreature->GetID(), xo,yo,xn,yn);
		}
	}

	//------------------------------------------------
	// add to the sector.
	//------------------------------------------------
	if (!m_ppSector[yn][xn].AddFlyingCreature( pCreature ))
	{
		DEBUG_ADD_FORMAT_WAR("Can't Add FlyingCreature! ID=%d (%d,%d)", pCreature->GetID(), xn,yn);
	}

	/*
	// when remove failed...
	if (!m_ppSector[yo][xo].RemoveFlyingCreature(id, pExistCreature))
	{	
		if (!m_ppSector[yo][xo].RemoveGroundCreature(id, pExistCreature))
		{
			if (!m_ppSector[yo][xo].RemoveUndergroundCreature(id, pExistCreature))
			{
				int xs = pCreature->GetServerX();
				int ys = pCreature->GetServerY();

				if (!m_ppSector[ys][xs].RemoveFlyingCreature(id, pExistCreature))
				{	
					if (!m_ppSector[ys][xs].RemoveGroundCreature(id, pExistCreature))
					{
						if (!m_ppSector[ys][xs].RemoveUndergroundCreature(id, pExistCreature))
						{

							int id = 0;
							if (pExistCreature!=NULL)
								id = pExistCreature->GetID();

							// remove failed
							//MessageBox(g_hWnd, "Already Removed![Flying]", NULL, MB_OK);
							DEBUG_ADD_FORMAT("Already Removed! [Flying] ID=%d (%d,%d) --> (%d,%d) ExistID = %d", pCreature->GetID(), xo,yo,xn,yn, id);
						}
					}
				}
			}
		}
	}

	// test
	MCreature* pOldCreature = m_ppSector[yn][xn].GetFlyingCreature();

	// when nobody is there
	if (pOldCreature == NULL)
	{
		// add								
		m_ppSector[yn][xn].AddFlyingCreature( pCreature );
	}
	// when another character is already in the new spot...
	else
	{
		DEBUG_ADD_FORMAT("Already ExistCreature! [Flying] ID=%d (%d,%d) --> (%d,%d) ExistID = %d", pCreature->GetID(), xo,yo,xn,yn, pOldCreature->GetID());				

		// when the character there first is the Player
		//if (pOldCreature->GetID()==g_pPlayer->GetID())
		//{
			// do not add.
			// return to the state before moving.
			// add								
		//	m_ppSector[xo][yo].AddFlyingCreature( pCreature );

		//	return false;
		//}
		//else
		{
			// remove the character that was there first..
			m_ppSector[yn][xn].RemoveFlyingCreature( pOldCreature );

			#ifdef OUTPUT_DEBUG
				if (pOldCreature!=NULL)
					DEBUG_ADD_FORMAT("[Flying]Remove Creature by New Creature! ID=%d (%d,%d)", pOldCreature->GetID(), xn,yn);
			#endif

			// add								
			m_ppSector[yn][xn].AddFlyingCreature( pCreature );

			// for confirmation
			pCreature->SetX( xn );
			pCreature->SetY( yn );

			//------------------------------------------------
			// add the removed character at the correct position.
			//------------------------------------------------
			if (pOldCreature!=NULL)
			{
				int x = pOldCreature->GetServerX();
				int y = pOldCreature->GetServerY();
				
				// if alive.. and the coordinates differ..
				if (x!=xn || y!=yn)
				{
					if (pOldCreature->IsAlive())					
					{				
						pOldCreature->SetStop();
						pOldCreature->MovePosition(	x, y );
					}
					else
					{
						pOldCreature->MovePosition(	x, y );
					}
				}
			}
		}
	}		
	*/

	return true;
}

//----------------------------------------------------------------------
// Move Ungerground Creature
//----------------------------------------------------------------------
// Move the UndergroundCreature at (xo,yo) to (xn,yn).
//----------------------------------------------------------------------
bool		
MZone::MoveUndergroundCreature(MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn)
{
	//-------------------------------------------------------
	// no check when outside the Zone area.
	//-------------------------------------------------------
	if (xn>=m_Width || yn>=m_Height) return false;

	// if it is the Player --> do not move.
	if (pCreature->GetClassType()==MCreature::CLASS_PLAYER
		// fake has no meaning of movement.
		|| pCreature->IsFakeCreature())
	{
		return true;
	}

	// if it has already moved..
	//if (xo==xn && yo==yn)
	//	return false;

	
	//MCreature* pExistCreature = NULL;

	int id = pCreature->GetID();

	//------------------------------------------------
	// remove from the sector.
	//------------------------------------------------
	// The cells it is said to be in may lie off the map (a packet's tile
	// goes on unchecked): no sector there to take it from.
	MSector* pOldSector = SectorAt(xo, yo);
	if (pOldSector==NULL || !pOldSector->RemoveCreature(id))
	{
		int xs = pCreature->GetServerX();
		int ys = pCreature->GetServerY();

		MSector* pServerSector = SectorAt(xs, ys);
		if (pServerSector==NULL || !pServerSector->RemoveCreature(id))
		{	
			DEBUG_ADD_FORMAT_WAR("Can't Remove! [Underground] ID=%d (%d,%d) --> (%d,%d)", pCreature->GetID(), xo,yo,xn,yn);
		}
	}

	//------------------------------------------------
	// add to the sector.
	//------------------------------------------------
	if (!m_ppSector[yn][xn].AddUndergroundCreature( pCreature ))
	{
		DEBUG_ADD_FORMAT_WAR("Can't Add UndergroundCreature! ID=%d (%d,%d)", pCreature->GetID(), xn,yn);
	}

	/*
	// when remove failed...
	if (!m_ppSector[yo][xo].RemoveUndergroundCreature(id, pExistCreature))
	{	
		if (!m_ppSector[yo][xo].RemoveGroundCreature(id, pExistCreature))
		{
			if (!m_ppSector[yo][xo].RemoveFlyingCreature(id, pExistCreature))
			{
				int xs = pCreature->GetServerX();
				int ys = pCreature->GetServerY();

				if (!m_ppSector[ys][xs].RemoveFlyingCreature(id, pExistCreature))
				{	
					if (!m_ppSector[ys][xs].RemoveGroundCreature(id, pExistCreature))
					{
						if (!m_ppSector[ys][xs].RemoveUndergroundCreature(id, pExistCreature))
						{

							int id = 0;
							if (pExistCreature!=NULL)
								id = pExistCreature->GetID();

							// remove failed
							//MessageBox(g_hWnd, "Already Removed![Flying]", NULL, MB_OK);
							DEBUG_ADD_FORMAT("Already Removed! [Under] ID=%d (%d,%d) --> (%d,%d) ExistID = %d", pCreature->GetID(), xo,yo,xn,yn, id);
						}
					}
				}
			}
		}
	}

	// test
	MCreature* pOldCreature = m_ppSector[yn][xn].GetUndergroundCreature();

	// when nobody is there
	if (pOldCreature == NULL)
	{
		// add								
		m_ppSector[yn][xn].AddUndergroundCreature( pCreature );
	}
	// when another character is already in the new spot...
	else
	{
		DEBUG_ADD_FORMAT("Already ExistCreature! [Underground] ID=%d (%d,%d) --> (%d,%d) ExistID = %d", pCreature->GetID(), xo,yo,xn,yn, pOldCreature->GetID());				

		// when the character there first is the Player
		//if (pOldCreature->GetID()==g_pPlayer->GetID())
		//{
			// do not add.
			// return to the state before moving.
			// add								
		//	m_ppSector[xo][yo].AddUndergroundCreature( pCreature );

		//	return false;
		//}
		//else
		{
			//------------------------------------------------
			// remove the character that was there first..
			//------------------------------------------------						
			m_ppSector[yn][xn].RemoveUndergroundCreature( pOldCreature );

			#ifdef OUTPUT_DEBUG
				if (pOldCreature!=NULL)
					DEBUG_ADD_FORMAT("Remove Creature by New Creature! ID=%d (%d,%d)", pOldCreature->GetID(), xn,yn);
			#endif

			//------------------------------------------------
			// add								
			//------------------------------------------------
			m_ppSector[yn][xn].AddUndergroundCreature( pCreature );
			
			//------------------------------------------------
			// add the removed character at the correct position.
			//------------------------------------------------
			if (pOldCreature!=NULL)
			{
				int x = pOldCreature->GetServerX();
				int y = pOldCreature->GetServerY();
				
				// if alive.. and the coordinates differ..
				if (x!=xn || y!=yn)
				{
					if (pOldCreature->IsAlive())					
					{				
						pOldCreature->SetStop();
						pOldCreature->MovePosition(	x, y );
					}
					else
					{
						pOldCreature->MovePosition(	x, y );
					}
				}
			}
		}
	}	
	*/

	return true;
}

//----------------------------------------------------------------------
// Get CreatureID
//----------------------------------------------------------------------
// Find the ID by name.
// flag : 1 excludes npc
//----------------------------------------------------------------------
TYPE_OBJECTID		
MZone::GetCreatureID(const char* pName, int flag) const
{
	CREATURE_MAP::const_iterator	iCreature = m_mapCreature.begin();

	MCreature* pCreature;

	//------------------------------------------------------
	// Action for all Creatures
	//------------------------------------------------------
	while (iCreature != m_mapCreature.end())
	{
		pCreature = iCreature->second;

		// when not the player..
		if (pCreature->GetID()!=PlayerState::GetID()
			&& strcmp(pCreature->GetName(), pName)==0
			&& (flag == 0 || (flag == 1 && !pCreature->IsNPC()) )
			)
		{
			return pCreature->GetID();
		}

		iCreature++;
	}	

	return OBJECTID_NULL;
}

//----------------------------------------------------------------------
// Return the ID of the Creature at Sector (x,y).
//----------------------------------------------------------------------

TYPE_OBJECTID
MZone::GetCreatureID(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	//-------------------------------------------------------
	// no check when outside the Zone area.
	//-------------------------------------------------------
	if (x>=m_Width || y>=m_Height) return OBJECTID_NULL;

	// if the sector has something
	MCreature*	pCreature = m_ppSector[y][x].GetGroundCreature();

	// if there is a Creature
	if (pCreature != NULL)
	{			
		return pCreature->GetID();
	}

	return OBJECTID_NULL;

}

//----------------------------------------------------------------------
// Return the Creature at Sector (x,y).
//----------------------------------------------------------------------
MCreature*
MZone::GetCreatureBySector(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	//-------------------------------------------------------
	// no check when outside the Zone area.
	//-------------------------------------------------------
	if (x>=m_Width || y>=m_Height) return NULL;

	// if the sector has something
	MCreature*	pCreature = m_ppSector[y][x].GetGroundCreature();

	// if there is a Creature
	if (pCreature != NULL)
	{			
		return pCreature;
	}

	return NULL;

}
//----------------------------------------------------------------------
// Return the ID of the FlyingCreature at Sector (x,y).
//----------------------------------------------------------------------
TYPE_OBJECTID
MZone::GetFlyingCreatureID(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	//-------------------------------------------------------
	// no check when outside the Zone area.
	//-------------------------------------------------------
	if (x>=m_Width || y>=m_Height) return OBJECTID_NULL;

	// if the sector has something
	MCreature*	pCreature = (MCreature*)m_ppSector[y][x].GetFlyingCreature();

	// if there is a Creature
	if (pCreature != NULL)
	{			
		return pCreature->GetID();
	}

	return OBJECTID_NULL;

}


//----------------------------------------------------------------------
// Return the ID of the UndergroundCreature at Sector (x,y).
//----------------------------------------------------------------------
TYPE_OBJECTID
MZone::GetUndergroundCreatureID(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	//-------------------------------------------------------
	// no check when outside the Zone area.
	//-------------------------------------------------------
	if (x>=m_Width || y>=m_Height) return OBJECTID_NULL;

	// if the sector has something
	MCreature*	pCreature = (MCreature*)m_ppSector[y][x].GetUndergroundCreature();

	// if there is a Creature
	if (pCreature != NULL)
	{			
		return pCreature->GetID();
	}

	return OBJECTID_NULL;

}


//----------------------------------------------------------------------
// Update
//----------------------------------------------------------------------
void
MZone::Update()
{
	//------------------------------------------
	// move other Creatures.
	//------------------------------------------
	UpdateAllCreature();

	//------------------------------------------
	// move fake Creatures.
	//------------------------------------------
	UpdateFakeCreature();

	//------------------------------------------
	// UpdateItem
	//------------------------------------------
	UpdateItem();

	#ifdef OUTPUT_DEBUG_UPDATE_ZONE
		DEBUG_ADD("MZone::before Effect");
	#endif

	//------------------------------------------
	// move Effects.
	//------------------------------------------
	UpdateWaitEffects();
	UpdateEffects();
	UpdateGroundEffects();
	//------------------------------------------
	// Helicopter
	//------------------------------------------
	m_HelicopterManager.Update();

	//------------------------------------------
	// Sound handling
	//------------------------------------------
	UpdateSound();

	#ifdef OUTPUT_DEBUG_UPDATE_ZONE
		DEBUG_ADD("MZone::Update OK");
	#endif
}

//----------------------------------------------------------------------
// Move All Creature
//----------------------------------------------------------------------
// Make all Creatures in the Zone act.
// except the Player
// Creatures outside the Player's sight must be removed.
//----------------------------------------------------------------------
void
MZone::UpdateAllCreature()
{
	CREATURE_MAP::iterator	iCreature = m_mapCreature.begin();

	MCreature* pCreature;

	//	DEBUG_ADD_FORMAT("[UpdateAllCreature] %d Creature(s)", m_mapCreature.size());

	const DWORD resetDelay = GameOptions::PersonalShopUpdateTime() > 0 ? GameOptions::PersonalShopUpdateTime() : 0;
	static MonotonicClock::TimePoint nextResetTime = g_FrameNow + MonotonicClock::Millis(resetDelay);
	bool bResetPersonalShopValue = false;
	
	if( g_FrameNow > nextResetTime )
	{
		nextResetTime = g_FrameNow + MonotonicClock::Millis(resetDelay);
		bResetPersonalShopValue = true;
	}	
	
	//------------------------------------------------------
	// Action for all Creatures
	//------------------------------------------------------
	while (iCreature != m_mapCreature.end())
	{
		pCreature = ((*iCreature).second);

		// Remember the pre-tick pixel position for draw-phase interpolation
		// (60 fps rendering between 62 ms ticks). Must precede Action() -
		// the player can even Action() twice per tick under ghost or
		// dragon-eyes below, and the snapshot has to be the pre-move state.
		pCreature->SnapshotDrawState();

		#ifdef OUTPUT_DEBUG_UPDATE_ZONE
				DEBUG_ADD_FORMAT("[UpdateCreature] id=%d, cType=%d, x=%d, y=%d, action=%d", 
									(int)pCreature->GetID(), (int)pCreature->GetCreatureType(),
									(int)pCreature->GetX(), (int)pCreature->GetY(), (int)pCreature->GetAction());
		#endif

		if( pCreature->CurPernalShop() == 1 && bResetPersonalShopValue )
			pCreature->SetPersonalShopMsgTime( rand() & 0x7F );
		
		//------------------------------------------------------
		//
		// if it is the Player
		//
		//------------------------------------------------------
		if (pCreature->GetClassType()==MCreature::CLASS_PLAYER)
		{
#ifdef __METROTECH_TEST__
//			for(int i = 0; i < abs(g_iSpeed)-1; i++)
//			{
//				g_pPlayer->Action();
////				g_pPlayer->UpdateAttachEffect();
//			}
#endif
			if(PlayerState::HasEffectStatus( EFFECTSTATUS_GHOST )
				||PlayerState::HasEffectStatus( EFFECTSTATUS_DRAGON_EYES )
				)
				PlayerState::Creature()->Action();
			
			PlayerState::Creature()->Action();

			#ifdef OUTPUT_DEBUG_UPDATE_ZONE
				DEBUG_ADD_FORMAT("PlAok");
			#endif

			PlayerState::Creature()->UpdateAttachEffect();

			#ifdef OUTPUT_DEBUG_UPDATE_ZONE
				DEBUG_ADD_FORMAT("PlEok");
			#endif			
		}
		//------------------------------------------------------
		//
		// if it is another Creature
		//
		//------------------------------------------------------
		else
		{
			int invisibleCount = pCreature->GetInvisibleCount();
			if(pCreature->HasEffectStatus( EFFECTSTATUS_DRAGON_EYES ))
				pCreature->Action();
			pCreature->Action();
			pCreature->UpdateAttachEffect();

			//------------------------------------------------------
			// when the Creature is dead
			//------------------------------------------------------
			// Remove from the Zone and turn into an MCorpse.
			//------------------------------------------------------
			/*
			if (pCreature->IsDead())
			{
				#ifdef CONNECT_SERVER
					// when the death motion has finished
					// and there is no AttachEffect either..
					if (pCreature->GetActionCount()==pCreature->GetActionCountMax()
						&& !pCreature->IsExistAttachEffect())
					{	
						// turn the creature into a corpse.
						AddCorpseFromCreature( pCreature );

						// remove the creature from the map.
						CREATURE_MAP::iterator	iCreatureTemp = iCreature;
						iCreature++;

						m_mapCreature.erase( iCreatureTemp );

						continue;
					}
				#endif
			}
			*/

			//------------------------------------------------------
			// if invisibleCount is 64, this character must disappear from the Zone.
			//------------------------------------------------------
			if (pCreature->GetInvisibleCount()==64
				&& invisibleCount!=64)
			{
				//-------------------------------------------------------
				// in sniping mode it always disappears (nobody can see it)
				//
				// for invisibility, it disappears if the player is a slayer
				// and has no detect_hidden.
				//-------------------------------------------------------
				if (//pCreature->HasEffectStatus(EFFECTSTATUS_SNIPPING_MODE)|| 
					pCreature->HasEffectStatus(EFFECTSTATUS_INVISIBILITY)
						&& !PlayerState::Creature()->IsVampire()
						&& !PlayerState::HasEffectStatus(EFFECTSTATUS_DETECT_INVISIBILITY)
						&& !PlayerState::HasEffectStatus(EFFECTSTATUS_OBSERVING_EYE)
						&& !PlayerState::HasEffectStatus(EFFECTSTATUS_GNOME_WHISPER)
//						&& !g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
						)
				{
					//-------------------------------------------------------
					// when not the player
					//-------------------------------------------------------
					int id			= pCreature->GetID();
					int x			= pCreature->GetX();
					int y			= pCreature->GetY();
					int serverX		= pCreature->GetServerX();
					int serverY		= pCreature->GetServerY();

					//----------------------------------------
					// remove helicopter
					//----------------------------------------
					if (pCreature->IsSlayer())
					{
						m_HelicopterManager.RemoveHelicopter( pCreature->GetID() );
					}				
					
					// Found it --> remove
					bool removed = true;

					MSector*	pSector			= SectorAt(x, y);
					MSector*	pServerSector	= SectorAt(serverX, serverY);

					//------------------------------------------------
					// Remove it from its sector.
					//------------------------------------------------
					if (pSector==NULL || !pSector->RemoveCreature(id))
					{
						if (pServerSector==NULL || !pServerSector->RemoveCreature(id))
						{
							DEBUG_ADD_FORMAT_WAR("Can't RemoveCreature! ID=%d client(%d,%d), server(%d,%d)", pCreature->GetID(), x,y,serverX,serverY);

							removed = false;
						}
					}

					UnSetServerBlock( pCreature->GetMoveType(), pCreature->GetServerX(), pCreature->GetServerY() );

					//------------------------------------------------
					// Only the sector that still holds the pointer can
					// release it. Freeing one that no sector gave up
					// leaves that sector pointing at dead memory, so
					// the creature stays owned by m_mapCreature and
					// Release() collects it when the zone goes away.
					// MZone::RemoveCreature guards the same delete.
					//------------------------------------------------
					if (removed)
					{
						delete pCreature;

						// remove the creature from the map
						CREATURE_MAP::iterator	iCreatureTemp = iCreature;
						iCreature++;

						m_mapCreature.erase( iCreatureTemp );

						continue;
					}
				}
			}

		}

		iCreature++;
	}

#ifdef __METROTECH_TEST__
	extern int	g_iSpeed;
	if(g_iSpeed < 0)
	{
		CREATURE_MAP::iterator itr = m_mapCreature.begin();
		CREATURE_MAP::iterator endItr = m_mapCreature.end();
		
		while(itr != endItr)
		{
			MCreature *pCreature = itr->second;
			if(pCreature->IsPlayer() && pCreature->GetID() != PlayerState::GetID() && !pCreature->IsPlayerParty())
				break;
			
			itr++;
		}
		
		if(itr == endItr)
		{
			g_iSpeed = -4;
		}
		else
		{
			g_iSpeed = -1;
		}
	}
#endif
	
	#ifdef OUTPUT_DEBUG_UPDATE_ZONE
		DEBUG_ADD("UpdateAllCreature OK");
	#endif
}

//----------------------------------------------------------------------
// Create Corpse  From Creature
//----------------------------------------------------------------------
// Create a corpse (MCorpse).. referring to the Creature..
//----------------------------------------------------------------------
/*
bool
MZone::AddCorpseFromCreature(MCreature* pCreature)
{
	DEBUG_ADD_FORMAT("Add Corpse From Creature id=%d", pCreature->GetID());

	int id = pCreature->GetID();
	int sX = pCreature->GetX();
	int sY = pCreature->GetY();

	//----------------------------------------
	// remove the chat.
	//----------------------------------------
	pCreature->ClearChatString();

	//----------------------------------------
	// Remove the Creature from the Sector
	//----------------------------------------
	// when not the player..
	//----------------------------------------
	bool removed = false;
	if (id != PlayerState::GetID())
	{
		int serverX = pCreature->GetServerX();
		int serverY = pCreature->GetServerY();

		removed = m_ppSector[sY][sX].RemoveGroundCreature( id );
		if (!removed)
		{
			removed = m_ppSector[serverY][serverX].RemoveGroundCreature( id );

			if (!removed)
			{
				removed = m_ppSector[sY][sX].RemoveFlyingCreature( id );

				if (!removed)
				{
					removed = m_ppSector[serverY][serverX].RemoveFlyingCreature( id );

					if (!removed)
					{
						removed = m_ppSector[sY][sX].RemoveUndergroundCreature( id );

						if (!removed)
						{
							removed = m_ppSector[serverY][serverX].RemoveUndergroundCreature( id );

							DEBUG_ADD_FORMAT("[Error] Can't Remove Creature to corpse. ID=%d. xy=(%d, %d), sxy=(%d, %d)", id, sX, sY, serverX, serverY);
						}
					}
				}
			}
		}
	}

	// if the Creature was not removed...
	#ifdef OUTPUT_DEBUG
		if (!removed)
		{			
			DEBUG_ADD_FORMAT("[Create Corpse] Can't Remove Creature from Sector: %d, (%d,%d)", id, sX, sY);
		}
	#endif
	
	//----------------------------------------
	//
	// create a corpse and set the Creature.
	//
	//----------------------------------------
	// position received from the server
	int sSX = pCreature->GetServerX();
	int sSY = pCreature->GetServerY();

	MCorpse* pCorpse = (MCorpse*)MItem::NewItem( ITEM_CLASS_CORPSE );
	pCorpse->SetID( id );
	pCorpse->SetItemType( 0 );
	pCorpse->SetCreature( pCreature );
	pCorpse->SetPosition( sSX, sSY );

	// set item count
	pCorpse->SetNumber( pCreature->GetItemCount() );


	//----------------------------------------	
	// if another Item is already there?? remove it.
	//----------------------------------------
	if (m_ppSector[sSY][sSX].IsExistItem())
	{
		MItem* pOldItem = m_ppSector[sSY][sSX].GetItem();
		
		#ifdef OUTPUT_DEBUG
			if (pOldItem!=NULL)
			{
				DEBUG_ADD_FORMAT("Already Exist Item. Remove Old Item: %d, (%d,%d)", pOldItem->GetID(), sSX, sSY);
			}
		#endif

		bool bRemove = RemoveItem( pOldItem->GetID() );

		#ifdef OUTPUT_DEBUG
			if (!bRemove)
			{
					DEBUG_ADD_FORMAT("Can't Remove Old Item");
			}
		#endif
	}

	//----------------------------------------
	// Add Item to the Zone
	//----------------------------------------
	#ifdef OUTPUT_DEBUG
		if (!AddItem( pCorpse ))
		{
			// hmm.. you could say this case is impossible. (sigh)
			DEBUG_ADD_FORMAT("[Error] Can't Add Corpse to Sector id=%d (%d,%d)", pCorpse->GetID(), sSX, sSY);
		
			delete pCorpse;

			return false;
		}

		return true;
	#else
		return AddItem( pCorpse );
	#endif
}
*/

//----------------------------------------------------------------------
// Create Corpse  From Creature
//----------------------------------------------------------------------
// Create a corpse (MCorpse).. referring to the Creature..
//----------------------------------------------------------------------
bool
MZone::AddCorpseFromCreature(TYPE_OBJECTID id)
{
	DEBUG_ADD_FORMAT("Add Corpse From Creature id=%d", id);

	if (id==PlayerState::GetID())
	{
		DEBUG_ADD("No Player Corpse");
		return false;
	}

	CREATURE_MAP::iterator iCreature = m_mapCreature.find( id );

	if (iCreature==m_mapCreature.end())
	{
		DEBUG_ADD("No such creature");
		return false;
	}

	MCreature* pCreature = iCreature->second;

	UnSetServerBlock( pCreature->GetMoveType(), pCreature->GetServerX(), pCreature->GetServerY() );

	// Remove from the Zone.
	m_mapCreature.erase( iCreature );	
	
	int sX = pCreature->GetX();
	int sY = pCreature->GetY();

	//----------------------------------------
	// remove the chat.
	//----------------------------------------
	if(pCreature->GetCreatureType() != 723) // Awakened Gilles de Rais.. removing here would drop the line spoken on death..
		pCreature->ClearChatString();

	//----------------------------------------
	// Remove the Creature from the Sector
	//----------------------------------------
	// when not the player..
	//----------------------------------------
	[[maybe_unused]] bool removed = false;
	if (id != PlayerState::GetID()
		&& !pCreature->IsFakeCreature())
	{
		//------------------------------------------------
		// remove from the sector.
		//------------------------------------------------
		removed = true;
		// Either cell may lie off the map (a packet's tile goes on
		// unchecked): no sector there to take it from.
		MSector* pSector = SectorAt(sX, sY);
		if (pSector==NULL || !pSector->RemoveCreature(id))
		{
			int serverX = pCreature->GetServerX();
			int serverY = pCreature->GetServerY();

			MSector* pServerSector = SectorAt(serverX, serverY);
			if (pServerSector==NULL || !pServerSector->RemoveCreature(id))
			{	
				removed = false;
				DEBUG_ADD_FORMAT_WAR("Can't RemoveCreatureToCorpse! ID=%d client(%d,%d), server(%d,%d)", pCreature->GetID(), sX,sY,serverX,serverY);
			}			
		}
		

		/*
		removed = m_ppSector[sY][sX].RemoveGroundCreature( id );
		if (!removed)
		{
			removed = m_ppSector[serverY][serverX].RemoveGroundCreature( id );

			if (!removed)
			{
				removed = m_ppSector[sY][sX].RemoveFlyingCreature( id );

				if (!removed)
				{
					removed = m_ppSector[serverY][serverX].RemoveFlyingCreature( id );

					if (!removed)
					{
						removed = m_ppSector[sY][sX].RemoveUndergroundCreature( id );

						if (!removed)
						{
							removed = m_ppSector[serverY][serverX].RemoveUndergroundCreature( id );

							DEBUG_ADD_FORMAT("[Error] Can't Remove Creature to corpse. ID=%d. xy=(%d, %d), sxy=(%d, %d)", id, sX, sY, serverX, serverY);
						}
					}
				}
			}
		}
		*/
	}

	// if the Creature was not removed...
	#ifdef OUTPUT_DEBUG
		if (!removed)
		{			
			DEBUG_ADD_FORMAT_WAR("[Create Corpse] Can't Remove Creature from Sector: %d, (%d,%d)", id, sX, sY);
		}
	#endif
	
	//----------------------------------------
	//
	// create a corpse and set the Creature.
	//
	//----------------------------------------
	// position received from the server
	int sSX = pCreature->GetServerX();
	int sSY = pCreature->GetServerY();

	MCorpse* pCorpse = (MCorpse*)MItem::NewItem( ITEM_CLASS_CORPSE );
	pCorpse->SetID( id );
	pCorpse->SetItemType( 0 );
	pCorpse->SetCreature( pCreature );
	pCorpse->SetPosition( sSX, sSY );

	// set item count
	pCorpse->SetNumber( pCreature->GetItemCount() );


	//----------------------------------------	
	// if another Item is already there?? remove it.
	//----------------------------------------
	MSector* pCorpseSector = SectorAt(sSX, sSY);
	if (pCorpseSector!=NULL && pCorpseSector->IsExistItem())
	{
		MItem* pOldItem = pCorpseSector->GetItem();
		
		#ifdef OUTPUT_DEBUG
			if (pOldItem!=NULL)
			{
				DEBUG_ADD_FORMAT("Already Exist Item. Remove Old Item: %d, (%d,%d)", pOldItem->GetID(), sSX, sSY);
			}
		#endif

		[[maybe_unused]] bool bRemove = RemoveItem( pOldItem->GetID() );

		#ifdef OUTPUT_DEBUG
			if (!bRemove)
			{
					DEBUG_ADD_FORMAT_WAR("Can't Remove Old Item");
			}
		#endif
	}

	//----------------------------------------
	// Add Item to the Zone
	//----------------------------------------
	if (!AddItem( pCorpse ))
	{
		// Refused (its server cell off the map): the corpse goes, and the
		// creature with it, out of the zone already.
		DEBUG_ADD_FORMAT_ERR("[Error] Can't Add Corpse to Sector id=%d (%d,%d)", pCorpse->GetID(), sSX, sSY);
		delete pCorpse;
		return false;
	}

	return true;

}


//----------------------------------------------------------------------
// Add Portal
//----------------------------------------------------------------------
// The rect area is a portal that moves to zoneID.
//----------------------------------------------------------------------
void						
MZone::AddPortal(int type, int zoneID, const RECT& rect)
{
	int top = rect.top;
	int left = rect.left;
	int bottom = rect.bottom;
	int right = rect.right;

	if (!ClipSectorRect(left, top, right, bottom, m_Width, m_Height))
		return;

	for (int i=top; i<=bottom; i++)
	{
		for (int j=left; j<=right; j++)
		{
			m_ppSector[i][j].AddPortal( type, zoneID );
		}
	}
}

//----------------------------------------------------------------------
// Keep Object In PlayerSight
//----------------------------------------------------------------------
// Among all Creatures/Items belonging to the Zone,
// keep only those within the Player's sight
// and remove the rest.
// Must match the Server's sight range.
//----------------------------------------------------------------------
void
MZone::KeepObjectInSight(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, BYTE sight)
{	
	(void)sight;
//	int sight15 = sight + (sight>>1);

	//------------------------------------------------------
	// Set the Player's sight range.
	//------------------------------------------------------
//	int minX = min( sight, VISION_SECTOR_WIDTH_HALF );
//	int minY = min( sight, VISION_SECTOR_HEIGHT_HALF );
//	int minY2 = min( sight, VISION_SECTOR_HEIGHT_HALF );//-2 );

	int minX = NEW_VISION_SECTOR_WIDTH_HALF;
	int minY = NEW_VISION_SECTOR_HEIGHT_HALF_UP;
	int minY2 = NEW_VISION_SECTOR_HEIGHT_HALF_DOWN;

	int sX1 = x-minX;
	int sY1 = y-minY;
	int sX2 = x+minX;
	int sY2 = y+minY2;

	//------------------------------------------------------
	// Skip if outside the Zone area...
	//------------------------------------------------------
	if (sX1 < 0) 
	{					
		sX1 = 0;	
	}

	if (sX2 >= m_Width)
	{
		sX2 = m_Width-1;
	}

	if (sY1 < 0)
	{
		sY1 = 0;	
	}

	if (sY2 >= m_Height)
	{
		sY2 = m_Height-1;
	}
	
	//------------------------------------------------------
	//
	// for all Creatures....
	//
	//------------------------------------------------------
	CREATURE_MAP::iterator	iCreature = m_mapCreature.begin();

	MCreature* pCreature;

	while (iCreature != m_mapCreature.end())
	{
		pCreature = iCreature->second;

		//-----------------------------------------------------
		// Only check and remove if not the Player.
		//-----------------------------------------------------
		if (pCreature->GetClassType()==MCreature::CLASS_PLAYER || pCreature->GetID() < 10000)	// client creatures are not removed
		{	
			iCreature++;
		}		
		else
		{
			//-----------------------------------------------------
			// Remove only if the Creature's Server coordinates
			// are not included in the Player's sight.
			//-----------------------------------------------------			
			int cX = pCreature->GetServerX();
			int cY = pCreature->GetServerY();
//			int dist = abs(x - cX) + abs(y - cY);

			DEBUG_ADD_FORMAT("[KeepObjectInSight] %d : %d : %d , %d : %d : %d",
				sX1, cX, sX2, sY1, cY, sY2 );
			//-----------------------------------------------------			
			// skip if within the sight range..
			//-----------------------------------------------------			
			if (cX>=sX1 && cX<=sX2 &&
				cY>=sY1 && cY<=sY2 
//				&& dist <= sight15
				)
			{
				iCreature++;			
			}
			//-----------------------------------------------------			
			// if not within the sight range... remove~~
			//-----------------------------------------------------			
			else
			{
				// just delete iTemp.
				CREATURE_MAP::iterator iTemp = iCreature;

				// next one..
				iCreature++;

				DEBUG_ADD_FORMAT("[Remove Creature by Sight] id=%d xy=(%d, %d)", pCreature->GetID(), cX, cY);						
				
				MCreature* pCreature = (*iTemp).second;
				
				//----------------------------------------
				// Remove from Sector
				//----------------------------------------
				// when not the player..
				//----------------------------------------
				int id = pCreature->GetID();
				int x = pCreature->GetX();
				int y = pCreature->GetY();
				
				bool removed = true;

				MSector*	pSector			= SectorAt(x, y);
				MSector*	pServerSector	= SectorAt(cX, cY);

				//------------------------------------------------
				// Remove it from its sector.
				//------------------------------------------------
				if (pSector==NULL || !pSector->RemoveCreature(id))
				{
					if (pServerSector==NULL || !pServerSector->RemoveCreature(id))
					{
						DEBUG_ADD_FORMAT_WAR("Can't RemoveCreature! ID=%d client(%d,%d), server(%d,%d)", pCreature->GetID(), x,y,cX,cY);

						removed = false;
					}
				}

				/*
				bool removed = m_ppSector[y][x].RemoveGroundCreature( id );
				if (!removed)
				{
					removed = m_ppSector[serverY][serverX].RemoveGroundCreature( id );

					if (!removed)
					{
						removed = m_ppSector[y][x].RemoveFlyingCreature( id );

						if (!removed)
						{
							removed = m_ppSector[serverY][serverX].RemoveFlyingCreature( id );

							if (!removed)
							{
								removed = m_ppSector[y][x].RemoveUndergroundCreature( id );

								if (!removed)
								{
									removed = m_ppSector[serverY][serverX].RemoveUndergroundCreature( id );

									DEBUG_ADD_FORMAT("[Error] Can't Remove Creature by sight. ID=%d. xy=(%d, %d), sxy=(%d, %d)", id, x, y, serverX, serverY);
								}
							}
						}
					}
				}
				*/

				//-------------------------------------------------------
				// when the one about to be removed is in the player's party
				//-------------------------------------------------------
				/*
				if (pCreature->IsPlayerParty()
					&& g_pParty!=NULL)
				{
					PARTY_INFO* pInfo = g_pParty->GetMemberInfo( pCreature->GetName() );
						
					if (pInfo!=NULL)
					{
						pInfo->bInSight = false;
						pInfo->zoneID = 60002;
					}
				}
				*/
				
				UnSetServerBlock( pCreature->GetMoveType(), pCreature->GetServerX(), pCreature->GetServerY() );

				//------------------------------------------------
				// Same rule as MZone::RemoveCreature: free it only
				// once a sector has given up its pointer, otherwise
				// that sector is left pointing at dead memory. It
				// stays in m_mapCreature and Release() collects it.
				//------------------------------------------------
				if (removed)
				{
					delete pCreature;

					m_mapCreature.erase(iTemp);
				}
			}
		}		
	}

	//------------------------------------------------------
	//
	// for all Items....
	//
	//------------------------------------------------------
	ITEM_MAP::iterator	iItem = m_mapItem.begin();

	MItem* pItem;

	while (iItem != m_mapItem.end())
	{
		pItem = iItem->second;
	
		//-----------------------------------------------------
		// Remove only if the Item's coordinates
		// are not included in the Player's sight.
		//-----------------------------------------------------			
		int iX = pItem->GetX();
		int iY = pItem->GetY();
//		int dist = abs(x - iX) + abs(y - iY);

		//-----------------------------------------------------			
		// skip if within the sight range..
		//-----------------------------------------------------			
		if (iX>=sX1 && iX<=sX2 &&
			iY>=sY1 && iY<=sY2 
//			&& dist <= sight15
			)
		{
			iItem++;			
		}
		//-----------------------------------------------------			
		// if not within the sight range... remove~~
		//-----------------------------------------------------			
		else
		{
			// just delete iTemp.
			ITEM_MAP::iterator iTemp = iItem;

			// next one..
			iItem++;

			DEBUG_ADD_FORMAT("[Remove Item by Sight] id=%d xy=(%d, %d)", pItem->GetID(), iX, iY);					
				
			MItem* pItem = (*iTemp).second;

			// [relic fix]
			MSector& sector = m_ppSector[iY][iX];				
			sector.RemoveItem( pItem->GetID() );

			// [relic fix]
			if (IsRelicTable(pItem))
			{
				sector.UnSetBlockGround();
				sector.UnSetBlockFlying();
			}
				
			// remove from memory
			//delete pItem;				
			delete pItem;
			pItem = NULL;
				
			m_mapItem.erase(iTemp);	
		}					
	}
	
	//------------------------------------------------------
	// Delete Effects
	//------------------------------------------------------
	EFFECT_MAP::iterator	iEffect = m_mapEffect.begin();

	MEffect* pEffect;

	while (iEffect != m_mapEffect.end())
	{
		pEffect = iEffect->second;
	
		//-----------------------------------------------------
		// Remove only if the Item's coordinates
		// are not included in the Player's sight.
		//-----------------------------------------------------			
		int iX = pEffect->GetX();
		int iY = pEffect->GetY();
//		int dist = abs(x - iX) + abs(y - iY);

		//-----------------------------------------------------			
		// skip if within the sight range..
		//-----------------------------------------------------			
		if (iX>=sX1 && iX<=sX2 &&
			iY>=sY1 && iY<=sY2 
//			&& dist <= sight15
			)
		{
			iEffect++;			
		}
		//-----------------------------------------------------			
		// if not within the sight range... remove~~
		//-----------------------------------------------------			
		else if (pEffect->GetEffectType()!=MEffect::EFFECT_CHASE &&
				(pEffect->GetFrameID() < EFFECTSPRITETYPE_MAP_BLACK_LARGE_SMOKE ||
				pEffect->GetFrameID() > EFFECTSPRITETYPE_MAP_BLACK_SMALL_SMOKE_3)&&
				pEffect->GetFrameID() != EFFECTSPRITETYPE_GDR_LAIR_POTAL)
		{
			// just delete iTemp.
			EFFECT_MAP::iterator iTemp = iEffect;

			// next one..
			iEffect++;

			DEBUG_ADD_FORMAT("[Remove Effect by Sight] id=%d xy=(%d, %d)", pEffect->GetID(), iX, iY);					
				
			MEffect* pEffect = iTemp->second;

			// [new skill 9]
			RemoveSectorEffect( iX, iY, pEffect->GetID() );
				
			// remove from memory
			delete pEffect;				
				
			m_mapEffect.erase(iTemp);	
		}	
		else
		{
			iEffect++;
		}
	}
}

//----------------------------------------------------------------------
// Add Creature to the Zone
//----------------------------------------------------------------------
// Must be new'd outside.
// Check whether it already exists, and add it if not.
//----------------------------------------------------------------------
bool		
MZone::AddCreature(MCreature* pCreature)
{
	DEBUG_ADD_FORMAT("MZone::AddCreature id = %d", pCreature->GetID());

	if (pCreature->GetX()==SECTORPOSITION_NULL
		|| pCreature->GetY()==SECTORPOSITION_NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] Position Not Set");

		return false;
	}

	CREATURE_MAP::iterator	theIterator;

	theIterator = m_mapCreature.find(pCreature->GetID());
	
	// add if it is a Creature that does not exist yet
	if (theIterator == m_mapCreature.end())
	{		
		// heh heh
		if (g_pParty!=NULL 
			&& pCreature->GetName()!=NULL
			&& g_pParty->HasMember( pCreature->GetName() )
			&& !pCreature->IsNPC() )
		{
			pCreature->SetPlayerParty();
				
			//-------------------------------------------------------
			// when the one who just came in is in the player's party
			//-------------------------------------------------------
			PARTY_INFO* pInfo = g_pParty->GetMemberInfo( pCreature->GetName() );
				
			if (pInfo!=NULL)
			{
				pInfo->bInSight = true;
				pInfo->HP = static_cast<WORD>(pCreature->GetHP());
				pInfo->MaxHP = static_cast<WORD>(pCreature->GetMAX_HP());
				pInfo->guildID = pCreature->GetGuildNumber();
			}			
		}

		//----------------------------------------
		// when the player is Hallu
		//----------------------------------------
		if (PlayerState::HasEffectStatus(EFFECTSTATUS_HALLUCINATION))
		{
			pCreature->SetHalluCreature( WorldEvents::GetRandomMonsterTypeInZone() );
		}

		//----------------------------------------
		// set again according to the movement method..
		//----------------------------------------
		if (pCreature->IsUndergroundCreature())
		{
			// just bury it.. (sigh)
		}
		else
		{
			if ((*g_pCreatureTable)[pCreature->GetCreatureType()].IsFlyingCreature())
			{
				pCreature->SetFlyingCreature();
			}			
			else
			{
				pCreature->SetGroundCreature();
			}
		}

		// A move type the switch below does not place leaves this false,
		// so the creature is reported as not added rather than inserted
		// into m_mapCreature without a sector to remove it from later.
		bool bAdd = false;
		//----------------------------------------
		// Sector Setting
		//----------------------------------------
		// if it is the Player
		//----------------------------------------
		if (pCreature->GetClassType()==MCreature::CLASS_PLAYER)
		{
			//m_mapCreature.insert(CREATURE_MAP::value_type(pCreature->GetID(), pCreature));
			bAdd = true;
		}
		//----------------------------------------
		// when not the Player
		//----------------------------------------
		else
		{			
			int x = pCreature->GetX();
			int y = pCreature->GetY();

			//----------------------------------------
			// The position arrives with the packet that
			// created this creature and is only tested
			// against SECTORPOSITION_NULL above, so the
			// sector it names still has to be checked.
			// The player is handled by the branch above
			// and is deliberately not rejected here.
			//----------------------------------------
			MSector*	pSector = SectorAt(x, y);

			if (pSector==NULL)
			{
				DEBUG_ADD_FORMAT_ERR("[Error] AddCreature: position (%d, %d) is outside the zone (%d x %d)", x, y, (int)m_Width, (int)m_Height);

				return false;
			}

			switch (pCreature->GetMoveType())
			{
				case MCreature::CREATURE_UNDERGROUND :
					bAdd = pSector->AddUndergroundCreature(pCreature);

					//if (!bAdd)
					{
					//	m_ppSector[pCreature->GetY()][pCreature->GetX()].RemoveUndergroundCreature();
					//	bAdd = m_ppSector[pCreature->GetY()][pCreature->GetX()].AddUndergroundCreature(pCreature);
					}
				break;

				case MCreature::CREATURE_GROUND :
					bAdd = pSector->AddGroundCreature(pCreature);

					//if (!bAdd)
					{
					//	m_ppSector[pCreature->GetY()][pCreature->GetX()].RemoveGroundCreature();
					//	bAdd = m_ppSector[pCreature->GetY()][pCreature->GetX()].AddGroundCreature(pCreature);		
					}
				break;

				case MCreature::CREATURE_FLYING :
					bAdd = pSector->AddFlyingCreature(pCreature);

					//if (!bAdd)
					{
					//	m_ppSector[pCreature->GetY()][pCreature->GetX()].RemoveFlyingCreature();
					//	bAdd = m_ppSector[pCreature->GetY()][pCreature->GetX()].AddFlyingCreature(pCreature);
					}
				break;

				//----------------------------------------
				// The CREATURE_FAKE_* move types belong to
				// MFakeCreature, which lives in
				// m_mapFakeCreature and never occupies a
				// sector. Only CREATURE_FAKE_UNDERGROUND
				// can reach here - the branch above rewrites
				// every other move type to GROUND or FLYING
				// - but nothing places it, so it must not be
				// reported as added.
				//----------------------------------------
				default :
					DEBUG_ADD_FORMAT_ERR("[Error] AddCreature: move type %d has no sector", (int)pCreature->GetMoveType());
				break;
			}
		}

		if (bAdd)
		{
			m_mapCreature.insert(CREATURE_MAP::value_type(pCreature->GetID(), pCreature));

			pCreature->SetZone( this );
			
			return true;
		}		

		DEBUG_ADD_FORMAT_ERR("[Error] AddCreature Failed!");

		return false;
	}

	// if it is a Creature that already exists,
	DEBUG_ADD_FORMAT("Add Failed - Already Exist in Zone");

	return false;
}

	

//----------------------------------------------------------------------
// Remove Creature from the Zone
//----------------------------------------------------------------------
// deleted internally.
//----------------------------------------------------------------------
bool
MZone::RemoveCreature(TYPE_OBJECTID id)
{
	DEBUG_ADD_FORMAT("Remove Creature. ID=%d", id);

	CREATURE_MAP::iterator	theIterator;

	// Find the Creature whose ID is id.
	theIterator = m_mapCreature.find(id);
    
	// there is no Creature with that id.
	if (theIterator == m_mapCreature.end())
		return false;


	MCreature* pCreature = (*theIterator).second;

	
	//-------------------------------------------------------
	// when the one about to be removed is in the player's party
	//-------------------------------------------------------
	/*
	if (pCreature->IsPlayerParty()
		&& g_pParty!=NULL)
	{
		PARTY_INFO* pInfo = g_pParty->GetMemberInfo( pCreature->GetName() );
			
		if (pInfo!=NULL)
		{
			pInfo->bInSight = false;
			pInfo->zoneID = 60002;
		}
	}
	*/
	
	//----------------------------------------
	// Remove from Sector
	//----------------------------------------
	// when it is the player
	//----------------------------------------
	if (pCreature==NULL || pCreature->GetClassType()==MCreature::CLASS_PLAYER)
	{
		m_mapCreature.erase(theIterator);
		
		return true;
	}
	
	//----------------------------------------
	// remove helicopter
	//----------------------------------------
	if (pCreature->IsSlayer())
	{
		m_HelicopterManager.RemoveHelicopter( id );
	}

	//----------------------------------------
	// when not the player
	//----------------------------------------	
	int x			= pCreature->GetX();
	int y			= pCreature->GetY();
	int serverX		= pCreature->GetServerX();
	int serverY		= pCreature->GetServerY();

	// let's search for the ID regardless of move type..
	/*
	switch (pCreature->GetMoveType())
	{
		case MCreature::CREATURE_GROUND :					
			removed = m_ppSector[y][x].RemoveGroundCreature( creatureID );
			if (!removed)
			{
				removed = m_ppSector[serverY][serverX].RemoveGroundCreature( creatureID );
			}
		break;

		case MCreature::CREATURE_FLYING :
			removed = m_ppSector[y][x].RemoveFlyingCreature( creatureID );
			if (!removed)
			{
				removed = m_ppSector[serverY][serverX].RemoveFlyingCreature( creatureID );
			}
		break;

		case MCreature::CREATURE_UNDERGROUND :
			removed = m_ppSector[y][x].RemoveUndergroundCreature( creatureID );
			if (!removed)
			{
				removed = m_ppSector[serverY][serverX].RemoveUndergroundCreature( creatureID );
			}
		break;
	}
	*/

	// Found it -- now take it out of its sector.
	bool removed = true;

	//------------------------------------------------
	// Both coordinate pairs are whatever the creature
	// currently carries, and a handler can reposition
	// an existing creature from packet data after
	// AddCreature validated it, so neither pair is
	// guaranteed to be on the map by the time we get
	// here. A missing sector counts as "not removed",
	// which is exactly what the guarded delete below
	// wants to hear.
	//------------------------------------------------
	MSector*	pSector       = SectorAt(x, y);
	MSector*	pServerSector = SectorAt(serverX, serverY);

	if (pSector==NULL || !pSector->RemoveCreature(id))
	{
		if (pServerSector==NULL || !pServerSector->RemoveCreature(id))
		{
			DEBUG_ADD_FORMAT_WAR("Can't RemoveCreature! ID=%d client(%d,%d), server(%d,%d)", pCreature->GetID(), x,y,serverX,serverY);

			removed = false;
		}
	}

	/*
	removed = m_ppSector[y][x].RemoveGroundCreature( id );
	if (!removed)
	{
		removed = m_ppSector[serverY][serverX].RemoveGroundCreature( id );

		if (!removed)
		{
			removed = m_ppSector[y][x].RemoveFlyingCreature( id );

			if (!removed)
			{
				removed = m_ppSector[serverY][serverX].RemoveFlyingCreature( id );

				if (!removed)
				{
					removed = m_ppSector[y][x].RemoveUndergroundCreature( id );

					if (!removed)
					{
						removed = m_ppSector[serverY][serverX].RemoveUndergroundCreature( id );

						DEBUG_ADD_FORMAT("[Error] Can't Remove Creature. ID=%d. xy=(%d, %d), sxy=(%d, %d)", id, x, y, serverX, serverY);
					}
				}
			}
		}
	}
	*/

	// just clear the server coordinates. 2002.3.29
	UnSetServerBlock( pCreature->GetMoveType(), pCreature->GetServerX(), pCreature->GetServerY() );


	// remove from memory: only when not the Player
	if (removed)
	{
		//UnSetServerBlock( pCreature->GetMoveType(), pCreature->GetServerX(), pCreature->GetServerY() );

		delete pCreature;

		m_mapCreature.erase(theIterator);
	}			

	return removed;
}


//----------------------------------------------------------------------
// Read a Creature of the Zone
//----------------------------------------------------------------------
// also search among items. (sigh)
//----------------------------------------------------------------------
MCreature*	
MZone::GetCreature(TYPE_OBJECTID id)
{
	CREATURE_MAP::iterator	theIterator;

	// Find the Creature whose ID is id.
	theIterator = m_mapCreature.find(id);

	// return NULL if not found.
	if (theIterator == m_mapCreature.end()) 
	{
		ITEM_MAP::iterator iItem = m_mapItem.find(id);

		if (iItem==m_mapItem.end())
		{
			return NULL;
		}

		MItem* pItem = iItem->second;

		if (pItem!=NULL
			&& pItem->GetItemClass()==ITEM_CLASS_CORPSE)
		{
			MCreature* pCreature = ((MCorpse*)pItem)->GetCreature();

			if (pCreature!=NULL
				&& pCreature->GetActionCount() < pCreature->GetActionCountMax())
			{
				return pCreature;
			}
		}
		
		return NULL;
	}

	// if found, return that Creature.
	return (*theIterator).second;
}

//----------------------------------------------------------------------
// Read a Creature of the Zone
//----------------------------------------------------------------------
MCreature*	
MZone::GetCreatureOnly(TYPE_OBJECTID id)
{
	CREATURE_MAP::iterator	theIterator;

	// Find the Creature whose ID is id.
	theIterator = m_mapCreature.find(id);

	// return NULL if not found.
	if (theIterator == m_mapCreature.end()) 
	{
		return NULL;
	}

	// if found, return that Creature.
	return theIterator->second;
}

//----------------------------------------------------------------------
// Add ImageObject to the Zone
//----------------------------------------------------------------------
// Must be new'd outside.
// Check whether it already exists, and add it if not.
//----------------------------------------------------------------------
bool		
MZone::AddImageObject(MImageObject* pImageObject)
{
	if(pImageObject != NULL)
	{
		IMAGEOBJECT_MAP::iterator	theIterator;

		theIterator = m_mapImageObject.find(pImageObject->GetID());
		
		// add if it is an ImageObject that does not exist yet
		if (theIterator == m_mapImageObject.end())
		{
			m_mapImageObject.insert(IMAGEOBJECT_MAP::value_type(pImageObject->GetID(), pImageObject));

			return true;
		}

		// if it is an ImageObject that already exists,
	}
	return false;
}

	

//----------------------------------------------------------------------
// Remove ImageObject from the Zone
//----------------------------------------------------------------------
// deleted internally.
//----------------------------------------------------------------------
bool
MZone::RemoveImageObject(TYPE_OBJECTID	id)
{
	IMAGEOBJECT_MAP::iterator	theIterator;

	// Find the ImageObject whose ID is id.
	theIterator = m_mapImageObject.find(id);
    
	// there is no ImageObject with that id.
	if (theIterator == m_mapImageObject.end())
		return false;


	// if found --> remove
	MImageObject* pImageObject = (*theIterator).second;	


	//???????????????????????????????????????????????????????
	//
	// shouldn't the Sector where the ImageObject exists be removed too??
	//
	//???????????????????????????????????????????????????????

	m_mapImageObject.erase(theIterator);

	// remove from memory
	delete pImageObject;	

	return true;
}

//----------------------------------------------------------------------
// Read an ImageObject of the Zone
//----------------------------------------------------------------------
MImageObject*	
MZone::GetImageObject(TYPE_OBJECTID id)
{
	IMAGEOBJECT_MAP::iterator	theIterator;

	// Find the ImageObject whose ID is id.
	theIterator = m_mapImageObject.find(id);

	// return NULL if not found.
	if (theIterator == m_mapImageObject.end()) 
		return NULL;

	// if found, return that ImageObject.
	return (*theIterator).second;
}

//----------------------------------------------------------------------
// SetImageObjectSector(sX,sY)
//----------------------------------------------------------------------
// Set an ImageObject on a Sector of the Zone
//----------------------------------------------------------------------
void		
MZone::SetImageObjectSector(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, TYPE_OBJECTID id)
{
	IMAGEOBJECT_MAP::iterator	theIterator;

	// Guard against corrupted/out-of-range coordinates (WASM traps on OOB).
	if (sX >= m_Width || sY >= m_Height)
	{
#ifdef __EMSCRIPTEN__
		static int s_badPosCount = 0;
		if (s_badPosCount < 5)
		{
			printf("WARN: ImageObject position out of bounds: (%u,%u) size=%ux%u\n",
				(unsigned)sX, (unsigned)sY, (unsigned)m_Width, (unsigned)m_Height);
			s_badPosCount++;
		}
#endif
		return;
	}

	// Find the ImageObject whose ID is id.
	theIterator = m_mapImageObject.find(id);

	// if none
	if (theIterator == m_mapImageObject.end()) 
		return;
	
	// Set the ImageObject on Sector (sX,sY).
	m_ppSector[sY][sX].AddImageObject((*theIterator).second);
}

//----------------------------------------------------------------------
// UnSetImageObjectSector(sX,sY)
//----------------------------------------------------------------------
// UnSet an ImageObject on a Sector of the Zone
//----------------------------------------------------------------------
void		
MZone::UnSetImageObjectSector(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, TYPE_OBJECTID id)
{
	if (sX >= m_Width || sY >= m_Height)
	{
		return;
	}

	//-------------------------------------------------------
	// Set the ImageObject on Sector (sX,sY).
	//-------------------------------------------------------
	m_ppSector[sY][sX].RemoveImageObject(id);
}

//----------------------------------------------------------------------
// Add Obstacle to the Zone
//----------------------------------------------------------------------
// Must be new'd outside.
// If it can be added to the Sector, add it to m_listObstacle.
//----------------------------------------------------------------------
/*
bool		
MZone::AddObstacle(MObstacle* pObstacle)
{	
	if (!m_ppSector[pObstacle->GetY()][pObstacle->GetX()].AddObstacle(pObstacle))
	{
		// when it was not added properly
		return false;
	}

	// Since it was added to the Sector, add it to the list too.
	// This is to make it easy to delete from memory later.
	m_listObstacle.push_front(pObstacle);

	return true;
}
*/

//----------------------------------------------------------------------
// Remove Obstacle from the Zone
//----------------------------------------------------------------------
// Remove the Obstacle at Sector (sX,sY).
// It must be removed from the map as well.
//----------------------------------------------------------------------
// There is no reason for this function to be called while the game runs,
// because an Obstacle is a fixed object.
//----------------------------------------------------------------------
//bool		
//MZone::RemoveObstacle(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY)
//{
	/*
	MObstacle* pObstacle;

	// when removed properly
	if (m_ppSector[sY][sX].RemoveObstacle(pObstacle))
	{
		// the removed MObstacle is held in pObstacle.
		// remove the Obstacle with the same ID from mapObstacle.
		OBSTACLE_MAP::iterator	iObstacle = m_mapObstacle.find(pObstacle->GetID());
    
		// there is no ImageObjectPositionList with that id.
		if (iObstacle == m_mapObstacle.end())
			return false;

		// delete from the map
		m_mapObstacle.erase( iObstacle );

		// remove from memory
		delete pObstacle;
		
		return true;
	}

	// when not removed properly (why?) - because it was not there?
	*/
	//return false;
//}


//----------------------------------------------------------------------
// Change Frame Obstacle
//----------------------------------------------------------------------
// Temporary function : changes the Frame of all Obstacles.
//----------------------------------------------------------------------
/*
void
MZone::ChangeFrameObstacle()
{
	OBSTACLE_LIST::iterator iObstacle = m_listObstacle.begin();

	MObstacle* pObstacle;
	while (iObstacle != m_listObstacle.end())
	{
		pObstacle = *iObstacle;

		pObstacle->NextFrame();

		iObstacle++;
	}
	
}
*/

//----------------------------------------------------------------------
// Return the ID of the Item at Sector (x,y).
//----------------------------------------------------------------------
TYPE_OBJECTID
MZone::GetItemID(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	//-------------------------------------------------------
	// no check when outside the Zone area.
	//-------------------------------------------------------
	if (//x<0 || y<0 || 
		x>=m_Width || y>=m_Height) return OBJECTID_NULL;

	// if the sector has something
	MItem*	pItem = m_ppSector[y][x].GetItem();

	// if there is an Item
	if (pItem != NULL)
	{			
		return pItem->GetID();
	}

	return OBJECTID_NULL;

	
}

//----------------------------------------------------------------------
// Add Item to the Zone
//----------------------------------------------------------------------
// Must be new'd outside.
// Check whether it already exists, and add it if not.
//----------------------------------------------------------------------
bool		
MZone::AddItem(MItem* pItem, BOOL bDropping)
{
	//--------------------------------------------------------	
	// when there is a dropping Frame...
	//--------------------------------------------------------	
	if (pItem->GetItemClass()!=ITEM_CLASS_MOTORCYCLE)	// a motorcycle does not drop. No sound either.
	{
		if (bDropping 
			&& pItem->GetDropFrameID()!=FRAMEID_NULL)				// when there is no falling image
		{
			pItem->SetDropping();
		}
		else
		{
			//--------------------------------------------------------	
			// output sound when it drops right away
			//--------------------------------------------------------	
			//PlaySound( pItem->GetTileSoundID(),
			//					false,
			//					g_pPlayer->GetX(), g_pPlayer->GetY());
		}
	}

	//--------------------------------------------------------	
	// add item to the zone
	//--------------------------------------------------------	
	ITEM_MAP::iterator	theIterator;

	theIterator = m_mapItem.find(pItem->GetID());
	
	// add if it is an Item that does not exist yet
	if (theIterator == m_mapItem.end())
	{
		// Item coordinates reach the zone straight out of GCAddItemToZone,
		// exactly as creature coordinates do in AddCreature, so the sector
		// they name has to be checked before it is written through.
		MSector*	pSector = SectorAt(pItem->GetX(), pItem->GetY());

		if (pSector==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] AddItem: position (%d, %d) is outside the zone (%d x %d)", (int)pItem->GetX(), (int)pItem->GetY(), (int)m_Width, (int)m_Height);

			return false;
		}

		MSector& sector = *pSector;

		if (sector.AddItem(pItem))
		{		
			m_mapItem.insert(ITEM_MAP::value_type(pItem->GetID(), pItem));

			// [relic fix] - it is an item, but forcibly block it.
			if (IsRelicTable(pItem))
			{
				sector.SetBlockGround();
				sector.SetBlockFlying();
			}

			bool bSpecialItem = false;

			// attach the Adam's Holy Land forced effect
			if(pItem->GetItemClass() == ITEM_CLASS_BLOOD_BIBLE)
			{
				bSpecialItem = true;
				WorldEvents::ExecuteActionInfoFromMainNode(
					ARMEGA_TILE+pItem->GetItemType(),		// the value itself is RESULT_ACTIONINFO.
					
					pItem->GetX(), pItem->GetY(), 0,
					DIRECTION_DOWN,
					
					OBJECTID_NULL,
					pItem->GetX(), pItem->GetY(), 0,
					
					0xFFFF,	
					
					NULL
					);
			}

			// attach the Adam's Holy Land forced effect
			if(pItem->GetItemClass() == ITEM_CLASS_CASTLE_SYMBOL)
			{
				int TempEffetType[6] = {	AMBER_OF_GUARD_TILE,
											AMBER_OF_AVENGER_TILE,
											AMBER_OF_IMMORTAL_TILE,
											AMBER_OF_CURSE_TILE,
											AMBER_OF_LIFE_TILE,
											AMBER_OF_LIGHT_TILE,
										};
				int TempItemType = pItem->GetItemType();

				if(TempItemType >5 ) TempItemType = 0;
				bSpecialItem = true;

				WorldEvents::ExecuteActionInfoFromMainNode(
					TempEffetType[TempItemType],		// the value itself is RESULT_ACTIONINFO.
					
					pItem->GetX(), pItem->GetY(), 0,
					DIRECTION_DOWN,
					
					OBJECTID_NULL,
					pItem->GetX(), pItem->GetY(), 0,
					
					0xFFFF,	
					
					NULL
					);
			}

			if(bSpecialItem)
			{
				WorldEvents::ExecuteActionInfoFromMainNode(
					DROP_BLOOD_BIBLE,		// the value itself is RESULT_ACTIONINFO.
					
					pItem->GetX(), pItem->GetY(), 0,
					DIRECTION_DOWN,
					
					OBJECTID_NULL,
					pItem->GetX(), pItem->GetY(), 0,
					
					0xFFFF,	
					
					NULL
					);
			}
			
			// 2004, 10, 25, sobeit add start - attach Dragon Eye effect
			if(pItem->GetItemClass() == ITEM_CLASS_WAR_ITEM)
			{
				int TempItemType = pItem->GetItemType();
				if(0 == TempItemType) // when it is a Dragon Eye
				{
					WorldEvents::ExecuteActionInfoFromMainNode(
						SKILL_CLIENT_DRAGON_EYES,		// the value itself is RESULT_ACTIONINFO.
						
						pItem->GetX(), pItem->GetY(), 0,
						DIRECTION_DOWN,
						
						OBJECTID_NULL,
						pItem->GetX(), pItem->GetY(), 0,
						
						0xFFFF,	
						
						NULL
						);
				}
			}
			// 2004, 10, 25, sobeit add end - attach Dragon Eye effect

			return true;
		}

		DEBUG_ADD_FORMAT_ERR("[Error] Already Exist Item id=%d, xy(%d,%d), cl=%d, ty=%d",
								pItem->GetID(), 
								pItem->GetX(), pItem->GetY(), 
								(int)pItem->GetItemClass(), 
								(int)pItem->GetItemType());
		

		return false;
	}

	// if it is an Item that already exists,
	return false;
}
	

//----------------------------------------------------------------------
// Remove Item from the Zone
//----------------------------------------------------------------------
// deleted internally.
//----------------------------------------------------------------------
bool
MZone::RemoveItem(TYPE_OBJECTID id)
{
	DEBUG_ADD_FORMAT("Remove Item. ID=%d", id);
	
	ITEM_MAP::iterator	theIterator;

	ITEM_MAP::iterator tempItr = m_mapItem.begin();
	while( tempItr != m_mapItem.end() )
	{
		tempItr++;
	}

	// Find the Item whose ID is id.
	theIterator = m_mapItem.find(id);
    
	// there is no Item with that id.
	if (theIterator == m_mapItem.end())
		return false;


	// if found --> remove

	// Remove from Sector
	MItem* pItem = NULL;
	// [relic fix]

	int sector_y = (*theIterator).second->GetY(), sector_x = (*theIterator).second->GetX();
	MSector& sector = m_ppSector[sector_y][sector_x];
	bool removed = sector.RemoveItem(pItem);

	if (removed && pItem!=NULL)
	{
		// [relic fix]
		if (IsRelicTable(pItem))
		{
			sector.UnSetBlockGround();
			sector.UnSetBlockFlying();
		}
		bool bSpecialItem = false;
		
		// attach the Adam's Holy Land forced effect
		if(pItem->GetItemClass() == ITEM_CLASS_BLOOD_BIBLE)
		{
			bSpecialItem = true;
						
			if (!g_pZone->RemoveTileEffect( sector_x,sector_y,   (TYPE_EFFECTSPRITETYPE)(EFFECTSPRITETYPE_ARMEGA_TILE + pItem->GetItemType()) ) )
			{
				DEBUG_ADD_ERR("[Error] Can't Remove Effect From Tile (ARMEGA_TILE HardCoding)");
			}
		}
		
		// 2004, 10, 25, sobeit add start - detach the Dragon Eye effect
		if(pItem->GetItemClass() == ITEM_CLASS_WAR_ITEM)
		{
			if(0 == pItem->GetItemType())
			{
				if (!g_pZone->RemoveTileEffect( sector_x,sector_y,   (TYPE_EFFECTSPRITETYPE)(EFFECTSPRITETYPE_DRAGON_EYE)) )
				{
					DEBUG_ADD_ERR("[Error] Can't Remove Effect From Tile (ITEM_CLASS_WAR_ITEM)");
				}
			}
		}
		// 2004, 10, 25, sobeit add end
		// attach the Adam's Holy Land forced effect
		if(pItem->GetItemClass() == ITEM_CLASS_CASTLE_SYMBOL)
		{
			bSpecialItem = true;

			int TempEffetType[6] = {	EFFECTSPRITETYPE_AMBER_OF_GUARD_TILE,
										EFFECTSPRITETYPE_AMBER_OF_AVENGER_TILE,
										EFFECTSPRITETYPE_AMBER_OF_IMMORTAL_TILE,
										EFFECTSPRITETYPE_AMBER_OF_CURSE_TILE,
										EFFECTSPRITETYPE_AMBER_OF_LIFE,
										EFFECTSPRITETYPE_AMBER_OF_LIGHT,
										};
				int TempItemType = pItem->GetItemType();
				if(TempItemType >5 ) TempItemType = 0;
			if (!g_pZone->RemoveTileEffect( sector_x,sector_y,  (TYPE_EFFECTSPRITETYPE)(TempEffetType[TempItemType]) ) )
			{
				DEBUG_ADD_ERR("[Error] Can't Remove Effect From Tile (AMBER_OF_GUARD_TILE HardCoding)");
			}
		}
		
		if(bSpecialItem)
		{
			if (!g_pZone->RemoveTileEffect( sector_x,sector_y,   (TYPE_EFFECTSPRITETYPE)EFFECTSPRITETYPE_DROP_BLOOD_BIBLE) )
			{
				DEBUG_ADD_ERR("[Error] Can't Remove Effect From Tile (DropBloodBible HardCoding)");
			}
		}

		m_mapItem.erase(theIterator);

		//------------------------------------------------------
		// It must be deleted if it is in the Item Check Buffer. 2001.8.27
		//------------------------------------------------------
		MItem* pCheckItem = PlayerState::GetItemCheckBuffer();

		if (pCheckItem!=NULL)
		{
			DEBUG_ADD_FORMAT("Remove Item. ID=%d", id);

			if (pItem->GetID()==pCheckItem->GetID())
			{
				//---------------------------------------------
				// remove the item check buffer.
				//---------------------------------------------
				PlayerState::ClearItemCheckBuffer();
			}
		}
		else
		{
			DEBUG_ADD("Check Item is NULL");
		}

		// remove from memory
		//delete pItem;	
		delete pItem;
		pItem = NULL;
	}

	return removed;
}

//----------------------------------------------------------------------
// Remove Item from the Zone
//----------------------------------------------------------------------
// Only remove from the zone, do not delete.
//----------------------------------------------------------------------
bool
MZone::PickupItem(TYPE_OBJECTID id)
{
	ITEM_MAP::iterator	theIterator;

	// Find the Item whose ID is id.
	theIterator = m_mapItem.find(id);
    
	// there is no Item with that id.
	if (theIterator == m_mapItem.end())
		return false;


	// if found --> remove

	// Remove from Sector
	MItem* pItem = NULL;
	int sector_y = (*theIterator).second->GetY(), sector_x = (*theIterator).second->GetX();
	bool removed = m_ppSector[sector_y][sector_x].RemoveItem(pItem);

	bool bSpecialItem = false;

	if(pItem != NULL && removed)
	{
		// attach the Adam's Holy Land forced effect
		if(pItem->GetItemClass() == ITEM_CLASS_BLOOD_BIBLE)
		{
			bSpecialItem = true;
						
			if (!g_pZone->RemoveTileEffect( sector_x,sector_y,   (TYPE_EFFECTSPRITETYPE)(EFFECTSPRITETYPE_ARMEGA_TILE + pItem->GetItemType() )) )
			{
				DEBUG_ADD_ERR("[Error] Can't Remove Effect From Tile (ARMEGA_TILE HardCoding)");
			}
		}
		
		// 2004, 10, 25, sobeit add start - detach the Dragon Eye effect
		if(pItem->GetItemClass() == ITEM_CLASS_WAR_ITEM)
		{
			if(0 == pItem->GetItemType())
			{
				if (!g_pZone->RemoveTileEffect( sector_x,sector_y,   (TYPE_EFFECTSPRITETYPE)(EFFECTSPRITETYPE_DRAGON_EYE)) )
				{
					DEBUG_ADD_ERR("[Error] Can't Remove Effect From Tile (ITEM_CLASS_WAR_ITEM)");
				}
			}
		}
		// 2004, 10, 25, sobeit add end
		
		// attach the Adam's Holy Land forced effect
		if(pItem->GetItemClass() == ITEM_CLASS_CASTLE_SYMBOL)
		{
			bSpecialItem = true;

			int TempEffetType[6] = {	EFFECTSPRITETYPE_AMBER_OF_GUARD_TILE,
									EFFECTSPRITETYPE_AMBER_OF_AVENGER_TILE,
									EFFECTSPRITETYPE_AMBER_OF_IMMORTAL_TILE,
									EFFECTSPRITETYPE_AMBER_OF_CURSE_TILE,
									EFFECTSPRITETYPE_AMBER_OF_LIFE,
									EFFECTSPRITETYPE_AMBER_OF_LIGHT,
									};
			int TempItemType = pItem->GetItemType();
			if(TempItemType >5 ) TempItemType = 0;
			if (!g_pZone->RemoveTileEffect( sector_x,sector_y,  (TYPE_EFFECTSPRITETYPE)(TempEffetType[TempItemType] )) )
			{
				DEBUG_ADD_ERR("[Error] Can't Remove Effect From Tile (AMBER_OF_GUARD_TILE HardCoding)");
			}
		}
		
		if(bSpecialItem)
		{
			if (!g_pZone->RemoveTileEffect( sector_x,sector_y,   (TYPE_EFFECTSPRITETYPE)EFFECTSPRITETYPE_DROP_BLOOD_BIBLE) )
			{
				DEBUG_ADD_ERR("[Error] Can't Remove Effect From Tile (DropBloodBible HardCoding)");
			}
		}
	}


	if (removed)
	{
		m_mapItem.erase(theIterator);
	}

	return removed;
}

//----------------------------------------------------------------------
// Read an Item of the Zone
//----------------------------------------------------------------------
MItem*	
MZone::GetItem(TYPE_OBJECTID id)
{
	ITEM_MAP::iterator	theIterator;

	// Find the Item whose ID is id.
	theIterator = m_mapItem.find(id);

	// return NULL if not found.
	if (theIterator == m_mapItem.end()) 
		return NULL;

	// if found, return that Item.
	return (*theIterator).second;
}

//----------------------------------------------------------------------
// Add Effect to the Zone
//----------------------------------------------------------------------
// Add to m_mapEffect.
// Add listEffect to the Sector.
//----------------------------------------------------------------------
bool
MZone::AddEffect(MEffect* pNewEffect, DWORD dwWaitCount)
{
	// Callers transfer a live, exclusively owned effect on entry. Keep it owned
	// locally until a zone container accepts it, including on exceptions.
	std::unique_ptr<MEffect> effect(pNewEffect);
	if (!effect)
		return false;

	if (dwWaitCount)
	{
		m_listWaitEffect.push_back(pNewEffect);
		effect.release();
		return true;
	}

	if (g_pEffectSpriteTypeTable == NULL ||
		g_pEffectSpriteTypeTable->GetInternalPointer() == NULL ||
		g_pEffectSpriteTypeTable->GetSize() == 0)
	{
		DEBUG_ADD_ERR("AddEffect: effect sprite table is unavailable");
		return false;
	}

	const int x = pNewEffect->GetX();
	const int y = pNewEffect->GetY();
	const TYPE_FRAMEID frameID = pNewEffect->GetFrameID();

	// Safe access with boundary checking
	int tableSize = g_pEffectSpriteTypeTable->GetSize();
	bool bDarkNess = false;
	bool bAcidSwamp = false;
	bool bProminence = false;

	if (EFFECTSPRITETYPE_DARKNESS_1_1 < tableSize &&
	    EFFECTSPRITETYPE_DARKNESS_3_5 < tableSize &&
	    EFFECTSPRITETYPE_GRAY_DARKNESS_1_1 < tableSize &&
	    EFFECTSPRITETYPE_GRAY_DARKNESS_3_5 < tableSize) {
		bDarkNess = (frameID >= (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_DARKNESS_1_1].FrameID &&
			frameID <= (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_DARKNESS_3_5].FrameID) ||
			(frameID >= (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_GRAY_DARKNESS_1_1].FrameID &&
			frameID <= (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_GRAY_DARKNESS_3_5].FrameID);
	}

	if (EFFECTSPRITETYPE_ACID_SWAP_1 < tableSize &&
	    EFFECTSPRITETYPE_ACID_SWAP_3 < tableSize) {
		bAcidSwamp = (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_ACID_SWAP_1].FrameID <= frameID &&
			(*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_ACID_SWAP_3].FrameID >= frameID;
	}

	if (EFFECTSPRITETYPE_PROMINENCE_ING < tableSize &&
	    EFFECTSPRITETYPE_PROMINENCE_START < tableSize &&
	    EFFECTSPRITETYPE_PROMINENCE2_ING < tableSize &&
	    EFFECTSPRITETYPE_PROMINENCE3_START < tableSize) {
		bProminence = ((*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_PROMINENCE_ING].FrameID <= frameID &&
			(*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_PROMINENCE_START].FrameID >= frameID) ||
			((*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_PROMINENCE2_ING].FrameID <= frameID &&
			(*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_PROMINENCE3_START].FrameID >= frameID);
	}


	if(bDarkNess)
	{
		//---------------------------------------------------------
		// The effect position is not tested against the zone until
		// the boundary check further down, which deliberately lets
		// a chase effect outside the map through, so this lookup
		// has to tolerate an off-map position. The block right
		// below already guards its own read the same way.
		//---------------------------------------------------------
		const MSector*	pSector = SectorAt(x, y);

		if(pSector!=NULL && pSector->HasDarknessForbidden() == true )
		{
			return false;
		}
	}
	// Darkness is not laid around a flagpole.
	if( bDarkNess )
	{
		if( !( x < 0 || x >= GetWidth() || y < 0 || y >=GetHeight() ) )
		{
			const MSector& sector= m_ppSector[y][x];
			const MItem* pItem = sector.GetItem();
			
			// Darkness is not stamped where the flagpole is.
			if( pItem != NULL && pItem->GetItemClass() == ITEM_CLASS_CORPSE &&
				((MCorpse*)pItem)->GetCreature() != NULL &&
				((MCorpse*)pItem)->GetCreature()->GetCreatureType() == 670 )
			{
				return false;
			}
		}		
	}

	// If a Mercy Ground is already laid, Darkness, grass and sea of fire are not laid.
	if( bDarkNess || bAcidSwamp || bProminence )
	{
		EFFECT_MAP::iterator iGroundEffect = m_mapGroundEffect.begin();
		
		while (iGroundEffect != m_mapGroundEffect.end())
		{
			MEffect* pEffect = iGroundEffect->second;
			
			TYPE_FRAMEID	frameID_s = (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_MERCY_GROUND_1].FrameID;
			TYPE_FRAMEID	frameID_e = (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_MERCY_GROUND_9].FrameID;
			TYPE_FRAMEID	regenTowerTile = (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_REGEN_TOWER_GROUND].FrameID;
			
			// Do not stamp Darkness if a Mercy Ground is laid.
			if( pEffect->GetFrameID() >= frameID_s && pEffect->GetFrameID() <= frameID_e )
			{
				int fix_y =   ( pEffect->GetFrameID() - frameID_s) /3 -1;
				int fix_x = ( ( pEffect->GetFrameID() - frameID_s) %3) -1;

				if( pEffect->GetX() + fix_x == x &&	pEffect->GetY() + fix_y == y )
				{
					return false;
				}
			} 
			else if( pEffect->GetFrameID() == regenTowerTile && x == pEffect->GetX() && y == pEffect->GetY() )
			{
				return false;
			}
			
			iGroundEffect++;
		}
	}
		

	//---------------------------------------------------------
	// when crossing the Zone boundary..
	//---------------------------------------------------------
	if (x<0 || x>=m_Width
		|| y<0 || y>=m_Height)
	{
		DEBUG_ADD("OuterZone");

		// a chase effect is fine to cross.
		if (pNewEffect->GetEffectType()==MEffect::EFFECT_CHASE)
		{
			TYPE_OBJECTID id = pNewEffect->GetID();

			// add only if not present.
			if (m_mapEffect.find( id )==m_mapEffect.end())
			{
				// add to list
				m_mapEffect[id] = pNewEffect;
				effect.release();

				DEBUG_ADD("ChaseEffect");

				return true;
			}
		}

		DEBUG_ADD("deleteEff");

		
		return false;
	}

	MSector &sector = m_ppSector[y][x];

	//---------------------------------------------------------
	// Compare with existing Effects..
	//
	// only for Effects fixed to a Sector...
	//---------------------------------------------------------
	BOOL AddOK = TRUE;

	if (pNewEffect->GetEffectType()==MEffect::EFFECT_SECTOR && !pNewEffect->IsMulti())
	{
		DEBUG_ADD("getSector");

		
		if (sector.IsExistEffect())
		{
			EFFECT_LIST::const_iterator iEffect = sector.GetEffectIterator();
			
			int num = sector.GetEffectSize();

			DEBUG_ADD_FORMAT("secEffNum=%d", num);

			for (int i=0; i<num; i++)
			{
				MEffect* pEffect = *iEffect;

				//---------------------------------------------------------
				// if the image is the same, treat it as the same Effect.
				// keep the one with the longer time.
				//---------------------------------------------------------
				// find the same type.
				if (pEffect->GetEffectType()==MEffect::EFFECT_SECTOR)
				{
					int fid1 = pNewEffect->GetFrameID();
					int fid2 = pEffect->GetFrameID();

					int est1 = ::GetEffectSpriteType( (BLT_TYPE)pNewEffect->GetBltType(), fid1 );
					int est2 = ::GetEffectSpriteType( (BLT_TYPE)pEffect->GetBltType(), fid2 );
					
//					// 2004, 10, 19, sobeit add start because of the Dosl 140 skill
//					if(est1 == EFFECTSPRITETYPE_SPIT_STREAM ||
//						est1 == EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW ||
//						est1 == EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW_SHADOW ||
//						est1 == EFFECTSPRITETYPE_GREAT_RUFFIAN_1_AXE_GROUND ||
//						est1 == EFFECTSPRITETYPE_GREAT_RUFFIAN_1_AXE_WAVE
//						)
//					{
//						AddOK = TRUE; // this skill can show the same image twice or more on one tile at different positions..
//					}
//					else
//					// 2004, 10, 19, sobeit add end
					if (fid1 == fid2)
					{
						// extend the time only if the one being added ends later.
						if (pNewEffect->GetEndFrame() > pEffect->GetEndFrame() &&
							est1 != EFFECTSPRITETYPE_DRAGON_FIRE_CRACKER) 
						{
							int count = pNewEffect->GetEndFrame() - g_CurrentFrame;
							
							pEffect->SetCount( count );
						}

						// do not add if there is the same one..
						AddOK = FALSE;
						
						if( est1 ==	EFFECTSPRITETYPE_DRAGON_FIRE_CRACKER )
							AddOK = TRUE;

						break;
					}
					// or if it is Darkness... hmm~~ (sigh)
					else
					{					
						if (
							( est1>=EFFECTSPRITETYPE_DARKNESS_1_1
							&& est1<=EFFECTSPRITETYPE_DARKNESS_3_5
							&& est2>=EFFECTSPRITETYPE_DARKNESS_1_1
							&& est2<=EFFECTSPRITETYPE_DARKNESS_3_5

							&& ((est1>=EFFECTSPRITETYPE_DARKNESS_2_1
								&& est1<=EFFECTSPRITETYPE_DARKNESS_2_5
								&& est2>=EFFECTSPRITETYPE_DARKNESS_2_1
								&& est2<=EFFECTSPRITETYPE_DARKNESS_2_5)
								|| (est1>=EFFECTSPRITETYPE_DARKNESS_1_1
								&& est1<=EFFECTSPRITETYPE_DARKNESS_1_5
								&& est2>=EFFECTSPRITETYPE_DARKNESS_1_1
								&& est2<=EFFECTSPRITETYPE_DARKNESS_1_5)
								|| (est1>=EFFECTSPRITETYPE_DARKNESS_3_1
								&& est1<=EFFECTSPRITETYPE_DARKNESS_3_5
								&& est2>=EFFECTSPRITETYPE_DARKNESS_3_1
								&& est2<=EFFECTSPRITETYPE_DARKNESS_3_5)) 
								)||

							(	est1>=EFFECTSPRITETYPE_GRAY_DARKNESS_1_1
							&& est1<=EFFECTSPRITETYPE_GRAY_DARKNESS_3_5
							&& est2>=EFFECTSPRITETYPE_GRAY_DARKNESS_1_1
							&& est2<=EFFECTSPRITETYPE_GRAY_DARKNESS_3_5

							&& ((est1>=EFFECTSPRITETYPE_GRAY_DARKNESS_2_1
								&& est1<=EFFECTSPRITETYPE_GRAY_DARKNESS_2_5
								&& est2>=EFFECTSPRITETYPE_GRAY_DARKNESS_2_1
								&& est2<=EFFECTSPRITETYPE_GRAY_DARKNESS_2_5)
								|| (est1>=EFFECTSPRITETYPE_GRAY_DARKNESS_1_1
								&& est1<=EFFECTSPRITETYPE_GRAY_DARKNESS_1_5
								&& est2>=EFFECTSPRITETYPE_GRAY_DARKNESS_1_1
								&& est2<=EFFECTSPRITETYPE_GRAY_DARKNESS_1_5)
								|| (est1>=EFFECTSPRITETYPE_GRAY_DARKNESS_3_1
								&& est1<=EFFECTSPRITETYPE_GRAY_DARKNESS_3_5
								&& est2>=EFFECTSPRITETYPE_GRAY_DARKNESS_3_1
								&& est2<=EFFECTSPRITETYPE_GRAY_DARKNESS_3_5))
								)								
							)
						{
							// extend the time only if the one being added ends later.
							if (pNewEffect->GetEndFrame() > pEffect->GetEndFrame()) 
							{
								int count = pNewEffect->GetEndFrame() - g_CurrentFrame;
								
								pEffect->SetCount( count );
							}

							// do not add if there is the same one..
							AddOK = FALSE;

							// only for repeating darkness..
							if ((est1>=EFFECTSPRITETYPE_DARKNESS_2_1 &&
								est1<=EFFECTSPRITETYPE_DARKNESS_2_5) ||
								(est1>=EFFECTSPRITETYPE_GRAY_DARKNESS_2_1 &&
								est1<=EFFECTSPRITETYPE_GRAY_DARKNESS_2_5))
							{
								sector.SetDarkness();
							}

						
							CheckCreatureInDarkness( sector, x, y );

							break;
						}	// [new skill]
							// when there is a sanctuary
						else if (est1>=EFFECTSPRITETYPE_SANCTUARY_1
								&& est1<=EFFECTSPRITETYPE_SANCTUARY_3)
						{
							// also check the nearby tiles. [new skill 9]
							for (int i=-1; i<=1; i++)
							{							
								for (int j=-1; j<=1; j++)
								{	
									int x0 = x + i;
									int y0 = y + j;

									if (x0<0 || x0>=m_Width
										|| y0<0 || y0>=m_Height)
									{
										continue;
									}

									MSector& sector2 = m_ppSector[y0][x0];
									sector2.SetSanctuary();
								}
							}
						}

					}
				}

				iEffect++;
			}
		}
	}
			
	//---------------------------------------------------------
	// when it is OK to add
	//---------------------------------------------------------
	if (AddOK)
	{
		DEBUG_ADD("addOK");

		TYPE_OBJECTID id = pNewEffect->GetID();

		// add only if not present.
		if (m_mapEffect.find( id )==m_mapEffect.end())
		{
			// add to list
			m_mapEffect[id] = pNewEffect;
			effect.release();

			// add to sector
			MSector& sector = m_ppSector[y][x];

			DEBUG_ADD("secAddEff");
			sector.AddEffect( pNewEffect );

			DEBUG_ADD("chkDark");
			CheckCreatureInDarkness( sector, x, y );
			DEBUG_ADD("chkDarkOK");

			return true;
		}
	}

	// set sight
//	if (pEffect->GetBltType()==BLT_EFFECT)
//	{
//		SetLight(pEffect->GetX(), pEffect->GetY(), pEffect->GetLight());		
//	}

	DEBUG_ADD("delEff");

	DEBUG_ADD("reF");
	return false;
}

//----------------------------------------------------------------------
// Get Effect
//----------------------------------------------------------------------
MEffect*	
MZone::GetEffect(TYPE_OBJECTID id) const
{
	EFFECT_MAP::const_iterator iEffect = m_mapEffect.find( id );

	if (iEffect==m_mapEffect.end())
	{
		return NULL;
	}

	return iEffect->second;
}

//----------------------------------------------------------------------
// Remove Effect ( id )
//----------------------------------------------------------------------
// The effect id has almost no meaning, but..
// there are cases where it must be deleted by force.. (sigh)
//----------------------------------------------------------------------
bool		
MZone::RemoveEffect(TYPE_OBJECTID id)
{
	EFFECT_MAP::iterator iEffect = m_mapEffect.find( id );

	if (iEffect==m_mapEffect.end())
	{
		return false;
	}

	MEffect* pEffect = iEffect->second;

	int x = pEffect->GetX();
	int y = pEffect->GetY();

	// when not crossing the boundary
	if (x>=0 && x<m_Width
		&& y>=0 && y<m_Height)
	{
		MSector& sector = m_ppSector[y][x];
		
		// [new skill 9]
		RemoveSectorEffect( x, y, pEffect->GetID() );

		CheckCreatureInDarkness( sector, x, y );
	}

	delete pEffect;

	m_mapEffect.erase( iEffect );

	return true;
}

//----------------------------------------------------------------------
// Remove Tile Effect
//----------------------------------------------------------------------
// Delete the effect on the tile.
//----------------------------------------------------------------------
bool		
MZone::RemoveTileEffect(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int effectStatus, int serverID)
{
	if (effectStatus >= g_pEffectStatusTable->GetSize())
	{
		DEBUG_ADD_FORMAT_ERR("[Error]EffectStatus is NULL or Over: %d", effectStatus);

		return false;
	}

	TYPE_EFFECTSPRITETYPE type = (*g_pEffectStatusTable)[effectStatus].EffectSpriteType;

	//----------------------------------------------------------------------
	// check special cases according to EffectStatus
	//----------------------------------------------------------------------
	switch (effectStatus)
	{
		case EFFECTSTATUS_TRYING_POSITION :
		case EFFECTSTATUS_TRAP_INSTALLED :
		{
			bool bDelete = false;
			
			EFFECT_MAP::iterator iGroundEffect = m_mapGroundEffect.begin();
			
			while (iGroundEffect != m_mapGroundEffect.end())
			{
				MEffect* pEffect = iGroundEffect->second;
				
				TYPE_FRAMEID	frameID	= (*g_pEffectSpriteTypeTable)[type].FrameID;

				if( pEffect->GetFrameID() == frameID &&
					pEffect->GetX() == sX &&
					pEffect->GetY() == sY )
				{
					delete pEffect;
					m_mapGroundEffect.erase( iGroundEffect );
					break;
				}
				
				iGroundEffect++;
			}
			
			DEBUG_ADD_WAR("EffectSpriteType of EffectStatus is NULL");
			
			return bDelete;
		}
		break;

		//----------------------------------------------------------------
		// the vampire portal is added to m_mapGroundEffect..
		//----------------------------------------------------------------		
		case EFFECTSTATUS_VAMPIRE_PORTAL :
		{
			bool bDelete = false;

			EFFECT_MAP::iterator iGroundEffect = m_mapGroundEffect.begin();

			while (iGroundEffect != m_mapGroundEffect.end())
			{
				MEffect* pEffect = iGroundEffect->second;

				MEffectTarget* pEffectTarget = pEffect->GetEffectTarget();

				if (pEffectTarget!=NULL)
				{
					//------------------------------------------------------------------
					// when it is a Portal
					//------------------------------------------------------------------
					if (pEffectTarget->GetEffectTargetType()==MEffectTarget::EFFECT_TARGET_PORTAL)
					{
						MPortalEffectTarget* pPortalEffectTarget = (MPortalEffectTarget*)pEffectTarget;

						int portalServerID = pPortalEffectTarget->GetServerID();
						
						if (serverID==portalServerID)
						{
							delete pEffect;

							m_mapGroundEffect.erase( iGroundEffect );

							break;
						}
					}
				}

				iGroundEffect++;
			}

			DEBUG_ADD_WAR("EffectSpriteType of EffectStatus is NULL");

			return bDelete;
		}
		break;	// - -;
		
		case EFFECTSTATUS_GROUND_ELEMENTAL_AURA:
			if( g_pZone != NULL && g_pZone->IsPositionInZone(sX, sY) )
			{
				const_cast<MSector &>(g_pZone->GetSector(sX, sY)).UnSetGroundElemental();
			}
			break;
		
		case EFFECTSTATUS_DARKNESS_FORBIDDEN:
			if( g_pZone != NULL && g_pZone->IsPositionInZone(sX, sY) )
			{
				const_cast<MSector &>(g_pZone->GetSector(sX, sY)).UnSetDarknessForbidden();
			}
			break;
			
		case EFFECTSTATUS_FURY_OF_GNOME:
			if( g_pZone != NULL && g_pZone->IsPositionInZone(sX, sY) )
			{
				const_cast<MSector &>(g_pZone->GetSector(sX, sY)).UnSetFuryOfGnome();
			}
			break;	
		case EFFECTSTATUS_DRAGON_TORNADO:
		case EFFECTSTATUS_DRAGON_TORNADO_CHILD:
			{
				return RemoveFakeCreature(serverID);
			}
			
		case EFFECTSTATUS_DELETE_TILE:
			if( g_pZone != NULL && g_pZone->IsPositionInZone(sX, sY) )
			{
				const_cast<MSector &>(g_pZone->GetSector(sX, sY)).UnSetDisableTileImage();
				WorldEvents::SetFirstDraw();
			}
			break;	
			return true;
	}


	return RemoveTileEffect(sX, sY, type, serverID);

}



//----------------------------------------------------------------------
// Remove Tile Effect
//----------------------------------------------------------------------
// Delete the effect on the tile.
//----------------------------------------------------------------------
bool		
MZone::RemoveTileEffect(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, TYPE_EFFECTSPRITETYPE type, int serverID)
{
	(void)serverID;
	if (type==EFFECTSPRITETYPE_NULL)
	{
		DEBUG_ADD_ERR("[Error]EffectSpriteType of EffectStatus is NULL");

		return false;
	}

	TYPE_FRAMEID	frameID = (*g_pEffectSpriteTypeTable)[type].FrameID;

	if (frameID==FRAMEID_NULL)
	{
		DEBUG_ADD_ERR("[Error]FrameID of EffectSpriteType is NULL");

		return false;
	}

	// The packet's tile may lie off the map (GCDeleteEffectFromTile does not
	// check it): nothing is there to remove.
	MSector* pSector = SectorAt(sX, sY);
	if (pSector==NULL)
	{
		return false;
	}

	MSector& sector = *pSector;

	//--------------------------------------------------
	// check whether it is a sector with an effect
	//--------------------------------------------------
	if (sector.IsExistEffect())
	{
		int num = sector.GetEffectSize();

		bool isRemoveDarknessEffect = (frameID>=EFFECTSPRITETYPE_DARKNESS_1_1
								&& frameID<=EFFECTSPRITETYPE_DARKNESS_3_5) ||
								(frameID>=EFFECTSPRITETYPE_GRAY_DARKNESS_1_1 
								&& frameID <= EFFECTSPRITETYPE_GRAY_DARKNESS_3_5 
//								||	frameID >= EFFECTSPRITETYPE_MAP_BLACK_LARGE_SMOKE &&
//								frameID <= EFFECTSPRITETYPE_MAP_BLACK_SMALL_SMOKE_3
								);

		EFFECT_LIST::const_iterator iEffect = sector.GetEffectIterator();

		for (int i=0; i<num; i++)
		{
			MEffect* pEffect = *iEffect;

			//--------------------------------------------------
			// check and delete only the effects present in the sector.
			//--------------------------------------------------
			if (pEffect->GetEffectType() == MEffect::EFFECT_SECTOR)
			{
				bool isExistDarknessEffect = ((pEffect->GetFrameID()>=EFFECTSPRITETYPE_DARKNESS_1_1
											&& pEffect->GetFrameID()<=EFFECTSPRITETYPE_DARKNESS_3_5) ||
											(pEffect->GetFrameID() >= EFFECTSPRITETYPE_GRAY_DARKNESS_1_1 &&
											pEffect->GetFrameID() <= EFFECTSPRITETYPE_GRAY_DARKNESS_3_5) 
//											||	pEffect->GetFrameID() <= EFFECTSPRITETYPE_MAP_BLACK_LARGE_SMOKE &&
//											pEffect->GetFrameID() >= EFFECTSPRITETYPE_MAP_BLACK_SMALL_SMOKE_3;
											);
				//--------------------------------------------------
				// when it is the same effect type
				//--------------------------------------------------
				if (pEffect->GetFrameID() == frameID

					// when it is Darkness... hardcoded.. (sigh)
					|| (isRemoveDarknessEffect
					&& isExistDarknessEffect)					
					)
				{
					// remove from sector [new skill 9]
					RemoveSectorEffect( sX, sY, pEffect->GetID() );

					
					//--------------------------------------------------
					// also check and delete from the zone's list.
					//--------------------------------------------------
					// delete
					EFFECT_MAP::iterator iZoneEffect = m_mapEffect.begin();

					BOOL found = FALSE;
					while (iZoneEffect != m_mapEffect.end())
					{
						MEffect* pZoneEffect = iZoneEffect->second;

						// if it is the same Effect..
						if (pZoneEffect->GetID()==pEffect->GetID())
						{
							m_mapEffect.erase( iZoneEffect );

							found = TRUE;

							break;
						}

						iZoneEffect++;
					}

					if (!found)
					{
						EFFECT_MAP::iterator iGroundEffect = m_mapGroundEffect.begin();

						while (iGroundEffect != m_mapGroundEffect.end())
						{
							MEffect* pGroundEffect = iGroundEffect->second;

							// if it is the same Effect..
							if (pGroundEffect->GetID()==pEffect->GetID())
							{
								m_mapGroundEffect.erase( iGroundEffect );

								break;
							}

							iGroundEffect++;
						}
					}

					// for darkness, show the character that was on that tile.
					if (isRemoveDarknessEffect)
					{
						CheckCreatureInDarkness( sector, sX, sY );
					}

					delete pEffect;
					
					return true;
				}
			}

			iEffect++;
		}
	}

	DEBUG_ADD_WAR("EffectSpriteType of EffectStatus is NULL");
	
	return false;
}


//----------------------------------------------------------------------
// Change Frame Effect
//----------------------------------------------------------------------
// Temporary function : changes the Frame of all Effects.
//----------------------------------------------------------------------
void
MZone::UpdateEffects()
{
	EFFECT_MAP::iterator iEffect = m_mapEffect.begin();

	EFFECT_MAP::iterator iTemp;
	
	MEffect* pEffect;
	int count = static_cast<int>(m_mapEffect.size());


	int		sX, sY;		// coordinates before the change
	int		nX, nY;		// coordinates after the change
	int		light;
	int		id;
	
//	bool bChangeSight;

	for (int i=0; i<count; i++)	
	{
		pEffect = iEffect->second;

		if(pEffect->IsDelayFrame() == true)
		{
			iEffect++;
			continue;
		}

		// remember the previous coordinates
		id		= pEffect->GetID();
		sX		= pEffect->GetX();
		sY		= pEffect->GetY();
		light	= pEffect->GetLight();

		// does the sight need to change?
//		bChangeSight = pEffect->GetBltType()==BLT_EFFECT;

		BOOL sXYinZone = (sX>=0 && sX<m_Width && sY>=0 && sY<m_Height);
		
		//---------------------------------------
		//
		// when moved properly
		//
		//---------------------------------------
		if (pEffect->Update())
		{
			nX = pEffect->GetX();
			nY = pEffect->GetY();

			BOOL nXYinZone = (nX>=0 && nX<m_Width && nY>=0 && nY<m_Height);

			//---------------------------------------
			// if not a chase effect..
			// when it crossed the Zone boundary.. --> remove it.
			//---------------------------------------
			if (pEffect->GetEffectType()!=MEffect::EFFECT_CHASE
//				&& (pEffect->GetFrameID() <= EFFECTSPRITETYPE_MAP_BLACK_LARGE_SMOKE ||
//					pEffect->GetFrameID() >= EFFECTSPRITETYPE_MAP_BLACK_SMALL_SMOKE_3)
				&& !nXYinZone)
			{
				// remove sight
//				if (bChangeSight)
//				{
//					UnSetLight(sX, sY, light);
//				}

				if (sXYinZone)
				{
					// remove from sector [new skill 9]
					RemoveSectorEffect( sX, sY, id );
				}
										
				#ifdef OUTPUT_DEBUG_UPDATE_EFFECT
					DEBUG_ADD("delete pEffect0");
					// delete memory
					delete pEffect;			
					DEBUG_ADD("delete OK");
				#else
					delete pEffect;
				#endif
				
				iTemp = iEffect;
				iEffect++;

				// remove from the list.
				m_mapEffect.erase(iTemp);	

				continue;
			}

			//-----------------------------------------------------------
			// Handle according to the kind of Effect.
			//-----------------------------------------------------------
			// Effect fixed to a Sector
			//-----------------------------------------------------------
			if (pEffect->GetEffectType() == MEffect::EFFECT_SECTOR)
			{
				// if it is a glowing Effect and the light size has changed..
//				if (bChangeSight && light!=pEffect->GetLight())
//				{
//					UnSetLight(sX, sY, light);
//					SetLight(sX, sY, pEffect->GetLight());
//				}				
			}
			//-----------------------------------------------------------
			// moving Effect
			//-----------------------------------------------------------
			else
			{
				// for a straight Effect --> must check the Sector according to the Sector coordinates.
				// compare with the previous coordinates
				//case MEffect::EFFECT_LINEAR :
				//case MEffect::EFFECT_PARABOLA :
				//case MEffect::EFFECT_GUIDANCE :
				//case MEffect::EFFECT_ATTACH :

				//------------------------------------------------------
				// if the coordinates changed..
				//------------------------------------------------------
				if (sX!=nX || sY!=nY)
				{
					//------------------------------------------------------
					// remove it if it entered a blocked Sector.
					//------------------------------------------------------
					//
					// might it differ depending on the kind of Block??
					//
					// air only ...
					if (0)
						// because the server does not check blocks,
						// ignore the block.
						//
						//pEffect->GetEffectType()!=MEffect::EFFECT_CHASE
						//&& nXYinZone && m_ppSector[nY][nX].IsBlockFlying())						
					{
						// remove sight
//						if (bChangeSight)
//						{
//							UnSetLight(sX, sY, light);
//						}

						// remove from sector
						if (sXYinZone)
						{
							// [new skill 9]
							RemoveSectorEffect(sX, sY, id);
						}
													
						// delete memory
						#ifdef OUTPUT_DEBUG_UPDATE_EFFECT
							DEBUG_ADD("delete pEffect1");
							// delete memory
							delete pEffect;			
							DEBUG_ADD("delete OK");
						#else
							delete pEffect;
						#endif
						
						iTemp = iEffect;
						iEffect++;

						// remove from the list.
						m_mapEffect.erase(iTemp);	

						continue;
					}

					// set sight again
//					if (bChangeSight)
//					{
//						UnSetLight(sX, sY, light);
//						SetLight(pEffect->GetX(), pEffect->GetY(), pEffect->GetLight());
//					}

					// take it out of the previous coordinates
					if (sXYinZone)
					{
						// [new skill 9]
						RemoveSectorEffect(sX, sY, id);
					}

					// add it at the new coordinates.
					if (nXYinZone)
					{
						m_ppSector[nY][nX].AddEffect( pEffect );
					}
				}		
				//------------------------------------------------------
				// if the light size (sight) has changed..
				//------------------------------------------------------
				else if (light != pEffect->GetLight())
				{
					// set sight again
//					if (bChangeSight)
//					{
//						UnSetLight(sX, sY, light);
//						SetLight(sX, sY, pEffect->GetLight());
//					}
				}
			}

			//-----------------------------------------------
			//
			// Before this Effect ends, because of LinkCount
			// if there is a next linked Effect it must be generated.
			//
			// when the current Frame passed EndLinkFrame
			//
			//-----------------------------------------------
			if (g_CurrentFrame >= pEffect->GetEndLinkFrame()
				&& pEffect->GetLinkSize() != 0)
			{
				// in GenerateNext
				// the EffectTarget of pEffect is set to NULL,
				// so there is no need to delete it here.
				#ifdef OUTPUT_DEBUG_UPDATE_EFFECT
					DEBUG_ADD("GenerateNext1");
					g_pEffectGeneratorTable->GenerateNext( pEffect );
					DEBUG_ADD("GenerateNext1 OK");
				#else
					g_pEffectGeneratorTable->GenerateNext( pEffect );
				#endif

				// pEffect must still exist, so it must not be deleted.
			}
			
			
			iEffect++;
		}
		//---------------------------------------
		//
		// when fully moved = when it must be removed
		//
		//---------------------------------------
		else
		{
			//-----------------------------------------------
			//
			// remove Effect
			//
			//-----------------------------------------------
			// remove sight
			//if (bChangeSight)
			//{
				//UnSetLight(sX, sY, light);
			//}			
			//-----------------------------------------------
			//
			// if there is a next linked Effect it must be generated.
			//
			//-----------------------------------------------
			if (pEffect->GetLinkSize() != 0)
			{
				#ifdef OUTPUT_DEBUG_UPDATE_EFFECT
					DEBUG_ADD("GenerateNext2");
					g_pEffectGeneratorTable->GenerateNext( pEffect );
					DEBUG_ADD("GenerateNext2 OK");
				#else
					g_pEffectGeneratorTable->GenerateNext( pEffect );
				#endif
			}		
			
			// remove from sector
			if (sXYinZone)
			{
				MSector& sector = m_ppSector[sY][sX];
				
				// [new skill 9]
				RemoveSectorEffect( sX, sY, id );

				CheckCreatureInDarkness( sector, sX, sY );				
			}
							
			#ifdef OUTPUT_DEBUG_UPDATE_EFFECT
				DEBUG_ADD("delete pEffect2");
				// delete memory
				delete pEffect;			
				DEBUG_ADD("delete OK");
			#else
				delete pEffect;
			#endif

			iTemp = iEffect;
			iEffect++;

			// remove from the list.
			m_mapEffect.erase(iTemp);	
		}		
	}	
}



//----------------------------------------------------------------------
// Add Effect to the Zone
//----------------------------------------------------------------------
// Add to m_mapGroundEffect.
// A GroundEffect only has pixel coordinates,
// so there is no need to check Sector coordinates.
//----------------------------------------------------------------------
bool		
MZone::AddGroundEffect(MEffect* pEffect)
{	
	TYPE_OBJECTID id = pEffect->GetID();
	BOOL	AddOK = TRUE;
	
	if( m_mapGroundEffect.empty() )
		AddOK = TRUE;
	else
	{
		EFFECT_MAP::iterator Itr = m_mapGroundEffect.begin();
		EFFECT_MAP::iterator endItr = m_mapGroundEffect.end();
		
		int NewEffectX = pEffect->GetX();
		int NewEffectY = pEffect->GetY();
		
		while( Itr != endItr )
		{
			MEffect *pOldEffect = Itr->second;			
			
			if(NewEffectX == pOldEffect->GetX() && NewEffectY == pOldEffect->GetY())
			{
				int est1 = ::GetEffectSpriteType( (BLT_TYPE) pEffect->GetBltType(), pEffect->GetFrameID() );
				int est2 = ::GetEffectSpriteType( (BLT_TYPE) pOldEffect->GetBltType(), pOldEffect->GetFrameID() );

				if( est1 == EFFECTSPRITETYPE_SUMMON_GROUND_ELEMENTAL_END && est2 == EFFECTSPRITETYPE_SUMMON_GROUND_ELEMENTAL_ING)
				{
					pOldEffect->SetCount( 0 );
				}

				if( (est1 == EFFECTSPRITETYPE_MAGIC_ELUSION_ING && est2 == EFFECTSPRITETYPE_MAGIC_ELUSION_ING) ||
					(est1 >= EFFECTSPRITETYPE_MERCY_GROUND_1 && est1 <= EFFECTSPRITETYPE_MERCY_GROUND_9 &&
					est1 == est2))
				{
					if( pEffect->GetEndFrame() > pOldEffect->GetEndFrame() )
					{
						int count = pEffect->GetEndFrame() - g_CurrentFrame;
						pOldEffect->SetCount( count );
					}
					AddOK = FALSE;
				}
				else if ( est2 == EFFECTSPRITETYPE_REGEN_TOWER_GROUND && 
					(est1 == EFFECTSPRITETYPE_ICE_FIELD_2 ||
					est1 == EFFECTSPRITETYPE_ICE_FIELD_1 ||
					est1 == EFFECTSPRITETYPE_ICE_FIELD_3
					)
					)
				{
					AddOK = FALSE;
				}
// 				else if ( est1 == EFFECTSPRITETYPE_SUMMON_CLAY_LOOP || 
// 						  est1 == EFFECTSPRITETYPE_SUMMON_CLAY_START ||
// 						  est1 == EFFECTSPRITETYPE_SUMMON_CLAY_END)
// 				{
// 					AddOK = FALSE;
// 				}
			}
			if(AddOK == FALSE)
				break;			
			Itr++;
		}
	}
	if(AddOK == TRUE)
	{
		m_mapGroundEffect[id] = pEffect;
	}		
		/*
		//---------------------------------------------------------
		// assume the sector coordinates of a selectable Effect do not change.
		//---------------------------------------------------------
		if (pEffect->IsSelectable())
		{	
			int x = pEffect->GetX();
			int y = pEffect->GetY();

			//---------------------------------------------------------
			// add to the Sector only if it does not cross the Zone boundary.
			//---------------------------------------------------------
			if (x<0 || x>=m_Width
				|| y<0 || y>=m_Height)
			{		
			}
			else
			{
				m_ppSector[y][x].AddEffect( pEffect );
			}
		}
		*/

	return true;
//	return false;
}

//----------------------------------------------------------------------
// Get GroundEffect
//----------------------------------------------------------------------
MEffect*	
MZone::GetGroundEffect(TYPE_OBJECTID id) const
{
	EFFECT_MAP::const_iterator iEffect = m_mapGroundEffect.find( id );

	if (iEffect==m_mapGroundEffect.end())
	{
		return NULL;
	}

	return iEffect->second;
}

//----------------------------------------------------------------------
// Remove GroundEffect ( id )
//----------------------------------------------------------------------
// The effect id has almost no meaning, but..
// there are cases where it must be deleted by force.. (sigh)
//----------------------------------------------------------------------
bool		
MZone::RemoveGroundEffect(TYPE_OBJECTID id)
{
	EFFECT_MAP::iterator iEffect = m_mapGroundEffect.find( id );

	if (iEffect==m_mapGroundEffect.end())
	{
		return false;
	}

	MEffect* pEffect = iEffect->second;

	/*
	//---------------------------------------------------------
	// assume the sector coordinates of a selectable Effect do not change.
	//---------------------------------------------------------
	if (pEffect->IsSelectable())
	{
		int x = pEffect->GetX();
		int y = pEffect->GetY();

		//---------------------------------------------------------
		// add to the Sector only if it does not cross the Zone boundary.
		//---------------------------------------------------------
		if (x<0 || x>=m_Width
			|| y<0 || y>=m_Height)
		{		
		}
		else
		{
			m_ppSector[y][x].RemoveEffect( pEffect->GetID() );
		}
	}
	*/

	delete pEffect;

	m_mapGroundEffect.erase( iEffect );

	return true;
}


//----------------------------------------------------------------------
// Update GroundEffects
//----------------------------------------------------------------------
// Temporary function : changes the Frame of all GroundEffects.
//----------------------------------------------------------------------
void
MZone::UpdateGroundEffects()
{
	EFFECT_MAP::iterator iEffect = m_mapGroundEffect.begin();

	EFFECT_MAP::iterator iTemp;
	
	MEffect* pEffect;
	int count = static_cast<int>(m_mapGroundEffect.size());


	
	for (int i=0; i<count; i++)	
	{
		pEffect = iEffect->second;

		//---------------------------------------
		//
		// when moved properly
		//
		//---------------------------------------
		if (pEffect->Update())
		{
			//-----------------------------------------------
			//
			// Before this Effect ends, because of LinkCount
			// if there is a next linked Effect it must be generated.
			//
			// when the current Frame passed EndLinkFrame
			//
			//-----------------------------------------------
			if (g_CurrentFrame >= pEffect->GetEndLinkFrame()
				&& pEffect->GetLinkSize() != 0)
			{
				// in GenerateNext
				// the EffectTarget of pEffect is set to NULL,
				// so there is no need to delete it here.
				g_pEffectGeneratorTable->GenerateNext( pEffect );

				// pEffect must still exist, so it must not be deleted.
			}

			// next..
			iEffect++;
		}
		//---------------------------------------
		//
		// when fully moved = when it must be removed
		//
		//---------------------------------------
		else
		{
			//-----------------------------------------------
			//
			// if there is a next linked Effect it must be generated.
			//
			//-----------------------------------------------
			if (pEffect->GetLinkSize() != 0)
			{
				g_pEffectGeneratorTable->GenerateNext( pEffect );
			}				
			
			//---------------------------------------------------------
			// assume the sector coordinates of a selectable Effect do not change.
			//---------------------------------------------------------
			/*
			if (pEffect->IsSelectable())
			{
				int x = pEffect->GetX();
				int y = pEffect->GetY();

				//---------------------------------------------------------
				// remove from the Sector only if it does not cross the Zone boundary
				//---------------------------------------------------------
				if (x<0 || x>=m_Width
					|| y<0 || y>=m_Height)
				{		
				}
				else
				{
					m_ppSector[y][x].RemoveEffect( pEffect->GetID() );
				}
			}
			*/
												
			// delete memory
			delete pEffect;			

			iTemp = iEffect;
			iEffect++;

			// remove from the list.
			m_mapGroundEffect.erase(iTemp);	
		}		
	}	
}

//----------------------------------------------------------------------
// Get Near SpriteSet
//----------------------------------------------------------------------
// Search and return the SpriteSets of the Tiles and ImageObjects near (sX,sY).
//----------------------------------------------------------------------
void						
MZone::GetNearSpriteSet(std::set<TYPE_SPRITEID>& TileSSM, std::set<TYPE_SPRITEID>& ImageObjectSSM, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) const
{
	//----------------------------------------------------------------------
	//
	// search ImageObject
	//
	//----------------------------------------------------------------------	
	POINT firstSector;
	firstSector.x = sX + g_SECTOR_SKIP_PLAYER_LEFT*3;
	firstSector.y = sY + g_SECTOR_SKIP_PLAYER_UP*3;
	int sX1 = firstSector.x + SECTOR_SKIP_LEFT*3;
	int sY1 = firstSector.y + SECTOR_SKIP_UP*3;
	int sX2 = firstSector.x + g_SECTOR_WIDTH*3;
	int sY2 = firstSector.y + g_SECTOR_HEIGHT*3;
	
	//------------------------------------------------------
	// Skip if outside the Zone area...
	//------------------------------------------------------
	if (sX1 < 0) 
	{					
		//tilePoint.x += -sX1 * TILE_X;
		sX1 = 0;	
	}

	if (sX2 >= m_Width)
	{
		sX2 = m_Width-1;
	}

	if (sY1 < 0)
	{
		//tilePoint.y += -sY1 * TILE_Y;
		sY1 = 0;	
	}

	if (sY2 >= m_Height)
	{
		sY2 = m_Height-1;
	}
		
	DEBUG_ADD_FORMAT("GetNearSpriteSet: (%d, %d) ~ (%d, %d)", sX1, sY1, sX2, sY2);

	//------------------------------------------------------
	// search the ImageObjects of each Sector
	//------------------------------------------------------
	int y,x,i;

	int maxTile = WorldEvents::GetTileSpriteCount();

	for (y=sY1; y<=sY2; y++)
	{				
		for (x=sX1; x<=sX2; x++)
		{				
			const MSector& sector = m_ppSector[y][x];

			//--------------------------------------------
			// save the Tile SpriteID.
			//--------------------------------------------
			int spriteID = sector.GetSpriteID();

			if (spriteID < maxTile)
			{
				TileSSM.insert( spriteID );
			}

			//--------------------------------------------
			// if there is an ImageObject..
			//--------------------------------------------
			if (sector.IsExistImageObject())
			{
				OBJECT_MAP::const_iterator iImageObject = sector.GetImageObjectIterator();

				//--------------------------------------------				
				// search all ImageObjects in the Sector.
				// add all the SpriteIDs of each ImageObject.
				//--------------------------------------------
				for (i=0; i<sector.GetImageObjectSize(); i++)
				{
					MImageObject* const pImageObject = (MImageObject*)((*iImageObject).second);
					
					if (pImageObject->GetSpriteID() != SPRITEID_NULL)
					{
						ImageObjectSSM.insert( pImageObject->GetSpriteID() );
					}
					
					// if there is a Frame... also search the Frame's SpriteID
					if (pImageObject->IsAnimation())
					{
						DEBUG_ADD_FORMAT("AnimationObject: (%d, %d)",x, y);

						int fid = ((MAnimationObject*)pImageObject)->GetFrameID();

						// Each frame's SpriteID too (none past the view's pack).
						const int frameCount = WorldEvents::GetImageObjectFrameCount(fid);
						for (int j=0; j<frameCount; j++)
						{
							const TYPE_SPRITEID frameSpriteID = WorldEvents::GetImageObjectFrameSprite(fid, j);
							if (frameSpriteID != SPRITEID_NULL)
							{
								ImageObjectSSM.insert( frameSpriteID );
							}
						}
					}
					
					
					iImageObject++;
				}
			}
		}
	}
}


//----------------------------------------------------------------------
// Add Sound
//----------------------------------------------------------------------
// Store a Sound to be played at some point.
//----------------------------------------------------------------------
void
MZone::AddSound(const SOUND_NODE& sound)
{
	m_DelayedSounds.Add(sound);
}

//----------------------------------------------------------------------
// Update Sound
//----------------------------------------------------------------------
// Check a Sound to be played at some point and make the sound.
//----------------------------------------------------------------------
void
MZone::UpdateSound()
{
	m_DelayedSounds.Update(g_FrameNow, [](const SOUND_NODE& sound) {
		WorldEvents::PlaySoundAt(sound.GetSoundID(), false, sound.GetX(), sound.GetY());
	});

	const int zoneID = g_bZonePlayerInLarge ? g_nZoneLarge : g_nZoneSmall;
	const ZONETABLE_INFO* zone = g_pZoneTable ? g_pZoneTable->Get(zoneID) : nullptr;
	std::optional<AmbientSoundPosition> player;
	if (MCreature* pPlayer = PlayerState::Creature()) player = {pPlayer->GetX(), pPlayer->GetY()};
	const auto actions = g_ZoneAmbientSounds.Update(g_FrameNow, zoneID, zone, player);
	if (actions.stop) WorldEvents::StopSound(*actions.stop);
	if (actions.play)
		WorldEvents::PlaySoundAt(actions.play->id, actions.play->loop, actions.play->x, actions.play->y);
}

//----------------------------------------------------------------------
// Update Item
//----------------------------------------------------------------------
// When the Item is a corpse.. (sigh)
//----------------------------------------------------------------------
void
MZone::UpdateItem()
{
	ITEM_MAP::iterator iItem = m_mapItem.begin();

	while (iItem!= m_mapItem.end())
	{
		MItem*	pItem = (*iItem).second;

		//----------------------------------------------------------------------
		// for a corpse.. handle the Effects attached to the corpse.
		//----------------------------------------------------------------------
		if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
		{
			MCorpse* pCorpse = (MCorpse*)pItem;

			MCreature* pCreature = pCorpse->GetCreature();

			if (pCreature!=NULL)
			{
				if (pCreature->GetNextAction()!=ACTION_STAND
					|| pCreature->GetActionCount()<pCreature->GetActionCountMax()
					|| pCreature->GetCreatureType() == 672 )
				{
					pCreature->Action();
				}
				pCreature->UpdateAttachEffect();
			}
		}

		//----------------------------------------------------------------------
		// when it is a falling Item..
		//----------------------------------------------------------------------
		else if (pItem->IsDropping())
		{
			pItem->NextDropFrame();

			//--------------------------------------------------------
			// when it has finished falling
			//--------------------------------------------------------
			if (!pItem->IsDropping())
			{
				WorldEvents::PlaySoundAt( pItem->GetTileSoundID(),
							false,
							PlayerState::Creature()->GetX(), PlayerState::Creature()->GetY());
			}
		}
		
		iItem++;		
	}

}


//----------------------------------------------------------------------
// Add FakeCreature to the Zone
//----------------------------------------------------------------------
// Must be new'd outside.
// Check whether it already exists, and add it if not.
//----------------------------------------------------------------------
bool		
MZone::AddFakeCreature(MCreature* pCreature)
{
	DEBUG_ADD_FORMAT("MZone::AddFakeCreature id = %d", pCreature->GetID());

	CREATURE_MAP::iterator	theIterator;

	theIterator = m_mapFakeCreature.find(pCreature->GetID());
	
	// add if it is a Creature that does not exist yet
	if (theIterator == m_mapFakeCreature.end())
	{
		m_mapFakeCreature.insert(CREATURE_MAP::value_type(pCreature->GetID(), pCreature));
		
		return true;
	}		

	// if it is a Creature that already exists,
	DEBUG_ADD_FORMAT("Add Failed - Already Exist in Zone");

	return false;
}

	

//----------------------------------------------------------------------
// Remove FakeCreature from the Zone
//----------------------------------------------------------------------
// deleted internally.
//----------------------------------------------------------------------
bool
MZone::RemoveFakeCreature(TYPE_OBJECTID id)
{
	CREATURE_MAP::iterator	theIterator;

	// Find the Creature whose ID is id.
	theIterator = m_mapFakeCreature.find(id);
    
	// there is no Creature with that id.
	if (theIterator == m_mapFakeCreature.end())
		return false;


	MFakeCreature* pFakeCreature = (MFakeCreature *)theIterator->second;

	if(pFakeCreature != NULL)
	{
		// for a pet
		if(pFakeCreature->GetOwnerID() != OBJECTID_NULL)
		{
			MCreature *pCreature = GetCreature(pFakeCreature->GetOwnerID());

			if(pCreature != NULL)
			{
				if(pCreature->GetPetID() == pFakeCreature->GetID())
					pCreature->SetPetID(OBJECTID_NULL);
				else if(pCreature->GetElementalID() == pFakeCreature->GetID())
					pCreature->SetElementalID(OBJECTID_NULL);
			}
		}

		delete pFakeCreature;
	}

	m_mapFakeCreature.erase(theIterator);

	return true;
}


//----------------------------------------------------------------------
// Read a Creature of the Zone
//----------------------------------------------------------------------
MCreature*	
MZone::GetFakeCreature(TYPE_OBJECTID id)
{
	CREATURE_MAP::iterator	theIterator;

	// Find the Creature whose ID is id.
	theIterator = m_mapFakeCreature.find(id);

	// return NULL if not found.
	if (theIterator == m_mapFakeCreature.end()) 
	{
		return NULL;
	}

	// if found, return that Creature.
	return (*theIterator).second;
}

//----------------------------------------------------------------------
// Update FakeCreature
//----------------------------------------------------------------------
void		
MZone::UpdateFakeCreature()
{
	CREATURE_MAP::iterator	iCreature = m_mapFakeCreature.begin();

	MCreature* pCreature;
	
	//	DEBUG_ADD_FORMAT("[UpdateAllCreature] %d Creature(s)", m_mapCreature.size());
	
	//------------------------------------------------------
	// Action for all Creatures
	//------------------------------------------------------
	while (iCreature != m_mapFakeCreature.end())
	{
		pCreature = iCreature->second;

		//------------------------------------------------------
		// only for MFakeCreature..
		//------------------------------------------------------
		if (pCreature->GetClassType()==MCreature::CLASS_FAKE)
		{
			MFakeCreature* pFakeCreature = (MFakeCreature*)pCreature;

			pFakeCreature->Action();
			pFakeCreature->UpdateAttachEffect();

			//------------------------------------------------------
			// when the FakeCreature disappears
			//------------------------------------------------------
			if (pFakeCreature->IsFakeEnd())
			{	
				CREATURE_MAP::iterator	iCreatureTemp = iCreature;
				iCreature++;

				m_mapFakeCreature.erase( iCreatureTemp );

				delete pFakeCreature;

				continue;
			}
		}

		iCreature++;
	}
}

//----------------------------------------------------------------------
// Add Helicopter
//----------------------------------------------------------------------
bool
MZone::AddHelicopter(TYPE_OBJECTID creatureID, int x0, int y0)
{
	MHelicopter* pHelicopter = new MHelicopter;

	if (!pHelicopter->SetChaseCreature( creatureID, x0, y0 )
		|| !m_HelicopterManager.AddHelicopter( pHelicopter ))
	{
		DEBUG_ADD_WAR("Can't Add Helicopter");

		delete pHelicopter;

		return false;
	}

	return true;
}

//----------------------------------------------------------------------
// Get Helicopter
//----------------------------------------------------------------------
MHelicopter*	
MZone::GetHelicopter(TYPE_OBJECTID creatureID) const
{
	return m_HelicopterManager.GetHelicopter(creatureID);
}

//----------------------------------------------------------------------
// Remove Helicopter
//----------------------------------------------------------------------
void		
MZone::RemoveHelicopter(TYPE_OBJECTID creatureID)
{
	m_HelicopterManager.RemoveHelicopter( creatureID );
}

//----------------------------------------------------------------------
// Change To HalluCreature
//----------------------------------------------------------------------
void		
MZone::ChangeToHalluCreature()
{
	CREATURE_MAP::iterator	iCreature = m_mapCreature.begin();

	MCreature* pCreature;

	DEBUG_ADD("Zone::ChangeToHalluCreature");
	
	//------------------------------------------------------
	// Action for all Creatures
	//------------------------------------------------------
	while (iCreature != m_mapCreature.end())
	{
		pCreature = iCreature->second;

		// when not the player...
		if (pCreature!=PlayerState::Creature())
		{
			pCreature->SetHalluCreature( WorldEvents::GetRandomMonsterTypeInZone() );
		}

		iCreature ++;
	}

	DEBUG_ADD("Zone::ChangeToHalluCreature OK");

}

//----------------------------------------------------------------------
// Remove HalluCreature
//----------------------------------------------------------------------
void		
MZone::RemoveHalluCreature()
{
	CREATURE_MAP::iterator	iCreature = m_mapCreature.begin();

	MCreature* pCreature;

	DEBUG_ADD("Zone::RemoveHalluCreature");
	
	//------------------------------------------------------
	// Action for all Creatures
	//------------------------------------------------------
	while (iCreature != m_mapCreature.end())
	{
		pCreature = iCreature->second;

		// when not the player...
		if (pCreature!=PlayerState::Creature())
		{
			pCreature->UnSetHalluCreature();
		}

		iCreature ++;
	}

	DEBUG_ADD("Zone::RemoveHalluCreature OK");
/*
	ITEM_MAP::iterator	iItem = m_mapItem.begin();

	MItem *pItem;

	while (iItem != m_mapItem.end())
	{
		pItem = iItem->second;

		if (pItem != NULL && pItem->GetItemClass()==ITEM_CLASS_CORPSE)
		{
			MCorpse* pCorpse = (MCorpse*)pItem;
			
			MCreature* pCreature = pCorpse->GetCreature();
			
			if (pCreature!=NULL && pCreature != PlayerState::Creature())
			{
				pCreature->UnSetHalluCreature();
			}
		}

		iItem++;
	}

	DEBUG_ADD("Zone::RemoveHalluCorpse OK");
*/
}

//----------------------------------------------------------------------
// Set Safe Sector
//----------------------------------------------------------------------
void						
MZone::SetSafeSector( const RECT& rect, BYTE fSafe )
{
	int top = rect.top;
	int left = rect.left;
	int bottom = rect.bottom;
	int right = rect.right;

	if (!ClipSectorRect(left, top, right, bottom, m_Width, m_Height))
		return;

	for (int i=top; i<=bottom; i++)
	{
		for (int j=left; j<=right; j++)
		{
			m_ppSector[i][j].SetSafe( fSafe );
		}
	}
}

MItem *
MZone::GetCorpseKilledByMe(int limitItemCount)
{
	ITEM_MAP::iterator itr = m_mapItem.begin();
	ITEM_MAP::iterator endItr = m_mapItem.end();

	while(itr != endItr)
	{
		MItem* pItem = itr->second;

		if (pItem!=NULL
			&& pItem->GetItemClass()==ITEM_CLASS_CORPSE && pItem->GetNumber() > static_cast<TYPE_ITEM_NUMBER>(limitItemCount))
		{
			MCreature* pCreature = ((MCorpse*)pItem)->GetCreature();

			if (pCreature!=NULL
				&& pCreature->GetDrainCreatureID() == PlayerState::GetID())
			{
				return pItem;
			}
		}

		itr++;
	}

	return NULL;
}

BYTE			
MZone::GetPKType()
{
	if(g_pZoneTable)
	{
		ZONETABLE_INFO* pZoneInfo = g_pZoneTable->Get( GetID() );
		if(NULL != pZoneInfo)
			return pZoneInfo->PKType;
	}

	return 0;
}


// 2004, 8, 30, sobeit add start - sweep vice test
//----------------------------------------------------------------------
// ChangeSwapViceType
//----------------------------------------------------------------------
// Change the SwapViceType on the tile.
//----------------------------------------------------------------------
void		
MZone::ChangeSwapViceType(TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, TYPE_EFFECTSPRITETYPE type, WORD wDelay)
{
	(void)sX;
	(void)sY;
	EFFECT_MAP::iterator iEffect = m_mapGroundEffect.begin();

	MEffect *pEffect = NULL;
	while (iEffect != m_mapGroundEffect.end())
	{
		pEffect = iEffect->second;
		if (pEffect != NULL)
		{
			if( pEffect->GetFrameID() == EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_1 ||
					pEffect->GetFrameID() == EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_2 ||
					pEffect->GetFrameID() == EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_3 )
				{
					pEffect->SetFrameID(type, pEffect->GetMaxFrame());
					pEffect->SetDelayFrame(wDelay);
					return;
			}
		}
		iEffect++;
	}

//	
//
//	MSector& sector = m_ppSector[sY][sX];
//
//	//--------------------------------------------------
//	// check whether it is a sector with an effect
//	//--------------------------------------------------
//	if (sector.IsExistEffect())
//	{
//		int num = sector.GetEffectSize();
//
//		EFFECT_LIST::const_iterator iEffect = sector.GetEffectIterator();
//
//		for (int i=0; i<num; i++)
//		{
//			MEffect* pEffect = *iEffect;
//
//			//--------------------------------------------------
//			// check and delete only the effects present in the sector.
//			//--------------------------------------------------
//			if (pEffect->GetEffectType() == MEffect::EFFECT_SECTOR)
//			{
//				if( pEffect->GetFrameID() == EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_1 ||
//					pEffect->GetFrameID() == EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_2 ||
//					pEffect->GetFrameID() == EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_3 )
//				{
//					pEffect->SetFrameID(type, pEffect->GetMaxFrame());
//					pEffect->SetDelayFrame(wDelay);
//					return;
//				}
//			}
//
//			iEffect++;
//		}
//	}
	return ;
}

void		
MZone::RemoveSwapViceType()
{
	EFFECT_MAP::iterator iEffect = m_mapGroundEffect.begin();

	MEffect *pEffect = NULL;
	while (iEffect != m_mapGroundEffect.end())
	{
		pEffect = iEffect->second;
		if (pEffect != NULL)
		{
			if( pEffect->GetFrameID() == EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_1 ||
					pEffect->GetFrameID() == EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_2 ||
					pEffect->GetFrameID() == EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_3 )
				{
					
//				// remove from list
					iEffect = m_mapGroundEffect.erase( iEffect );
					// free memory
					delete pEffect;

					continue;
			}
		}
		iEffect++;
	}

}
// 2004, 8, 30, sobeit add end - sweep vice test

// 2004, 8, 31, sobeit add start
void
MZone::UpdateWaitEffects()
{
	WAIT_EFFECT_LIST::iterator iWaitEffect = m_listWaitEffect.begin();

	while (iWaitEffect != m_listWaitEffect.end())
	{
		MEffect*	pNode = *iWaitEffect;
		if( !pNode->IsWaitFrame())
		{
			iWaitEffect = m_listWaitEffect.erase(iWaitEffect);
			// AddEffect owns the node now; the wait list must not retain it
			// while rejection or an exception destroys it.
			AddEffect( pNode );
		}
		else
			iWaitEffect ++;
	}
}
// 2004, 8, 31, sobeit add end
