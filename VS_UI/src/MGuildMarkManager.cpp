//----------------------------------------------------------------------
// MGuildMarkManager.cpp
//----------------------------------------------------------------------
// The guild, grade and level marks the UI and the view draw (task 4.230,
// VS_UI's since then): the grade and level marks from their packs, and a
// guild's two marks from the guild mark pack at the sprite ID the guild
// info mapper gives it, loaded once and cached.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MGuildMarkManager.h"
#include "MGuildInfoMapper.h"
#include "CSprite555.h"
#include "CSprite565.h"
#include "basic/ColorDraw.h"
#include "basic/DebugLog.h"
#include <memory>
#include <vector>

//----------------------------------------------------------------------
// global
//----------------------------------------------------------------------
MGuildMarkManager* g_pGuildMarkManager = NULL;


//----------------------------------------------------------------------
//
//						MGuildMarkManager
//
//----------------------------------------------------------------------
MGuildMarkManager::MGuildMarkManager(const char* pGuildMarkSPKFilename, 
									const char* pGuildInfoMapperFilename,
									const char* pSlayerGradeMarkSPKFilename,
									const char* pVampireGradeMarkSPKFilename,
									const char* pOustersGradeMarkSPKFilename,
									const char* pLevelMarkSPKFilename)
{
	m_GuildMarkSPKFilename = pGuildMarkSPKFilename;

//	m_SlayerGradeMarkSPKFilename = pSlayerGradeMarkSPKFilename;
//	m_VampireGradeMarkSPKFilename = pVampireGradeMarkSPKFilename;

	// The sprite ID mapper is read from its file.
	if (pGuildInfoMapperFilename!=NULL)
	{
		m_GuildMarkSpriteMapperFilename = pGuildInfoMapperFilename;		
	}

//	m_GradeMarks.Init(GRADE_MARK_MAX<<2, ColorDraw::Is565());
	m_GradeMarks[RACE_SLAYER].LoadFromFileRunning(pSlayerGradeMarkSPKFilename);
	m_GradeMarks[RACE_VAMPIRE].LoadFromFileRunning(pVampireGradeMarkSPKFilename);
	m_GradeMarks[RACE_OUSTERS].LoadFromFileRunning(pOustersGradeMarkSPKFilename);
	m_LevelMarks.LoadFromFileRunning(pLevelMarkSPKFilename);
}

MGuildMarkManager::~MGuildMarkManager()
{
	Release();
}

//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void		
MGuildMarkManager::Release()
{
	GUILDMARK_MAP::iterator iMark = m_GuildMarks.begin();

	while (iMark != m_GuildMarks.end())
	{
		CSprite* pSprite = iMark->second.pSprite;
		CSprite* pSpriteSmall = iMark->second.pSpriteSmall;

		if (pSprite!=NULL)
		{
			delete pSprite;
		}

		if (pSpriteSmall!=NULL)
		{
			delete pSpriteSmall;
		}

		iMark++;
	}
	m_GuildMarks.clear();
}

//----------------------------------------------------------------------
// Get GuildMark
//----------------------------------------------------------------------
CSprite*	
MGuildMarkManager::GetGuildMark(WORD guildID)
{
	GUILDMARK_MAP::iterator iMark = m_GuildMarks.find(guildID);

	if (iMark != m_GuildMarks.end())
	{
		return m_GuildMarks[guildID].pSprite;
	}

	return NULL;
}

//----------------------------------------------------------------------
// Get GuildMarkSmall
//----------------------------------------------------------------------
CSprite*	
MGuildMarkManager::GetGuildMarkSmall(WORD guildID)
{
	GUILDMARK_MAP::iterator iMark = m_GuildMarks.find(guildID);

	if (iMark != m_GuildMarks.end())
	{
		return m_GuildMarks[guildID].pSpriteSmall;
	}

	return NULL;
}

//----------------------------------------------------------------------
// Has GuildMark
//----------------------------------------------------------------------
bool			
MGuildMarkManager::HasGuildMark(WORD guildID) const
{
	GUILDMARK_MAP::const_iterator iMark = m_GuildMarks.find(guildID);

	if (iMark != m_GuildMarks.end())
	{
		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Load GuildMark
//----------------------------------------------------------------------
// The guild info mapper gives the guild's sprite ID; the guild mark pack's
// index gives where that sprite (and the small one after it) starts, so
// both can be loaded at once.
//----------------------------------------------------------------------
bool
MGuildMarkManager::LoadGuildMark(WORD guildID)
{
	// Negative cache entries retain their failed result.
	const auto cached = m_GuildMarks.find(guildID);
	if (cached != m_GuildMarks.end())
		return cached->second.pSprite != nullptr && cached->second.pSpriteSmall != nullptr;

	const char* filename = m_GuildMarkSPKFilename.GetString();
	const GUILD_INFO* info = g_pGuildInfoMapper ? g_pGuildInfoMapper->Get(guildID) : nullptr;
	if (filename && info) {
		const unsigned spriteID = info->GetSpriteID();
		std::ifstream data;
		std::vector<int> offsets;
		if (CTypePackDetail::ReadRunningIndex(filename, data, offsets) &&
			offsets.size() >= 2 && spriteID < offsets.size() - 1) {
			std::unique_ptr<CSprite> largeSprite, smallSprite;
			if (ColorDraw::Is565()) {
				largeSprite = std::make_unique<CSprite565>();
				smallSprite = std::make_unique<CSprite565>();
			} else {
				largeSprite = std::make_unique<CSprite555>();
				smallSprite = std::make_unique<CSprite555>();
			}
			// Each member uses its own checked four-byte index entry.
			data.seekg(offsets[spriteID]);
			if (CTypePackDetail::LoadElement(*largeSprite, data, filename, spriteID)) {
				data.seekg(offsets[spriteID + 1]);
				if (CTypePackDetail::LoadElement(*smallSprite, data, filename, spriteID + 1)) {
					m_GuildMarks[guildID] = GUILD_SPRITES(largeSprite.get(), smallSprite.get());
					largeSprite.release();
					smallSprite.release();
					return true;
				}
			}
		}
		LOG_ERROR("Rejected guild sprite pair: source=%s guild=%u id=%u", filename,
			static_cast<unsigned>(guildID), spriteID);
	}

	// Keep the existing negative-cache policy without publishing partial art.
	m_GuildMarks[guildID] = GUILD_SPRITES(nullptr, nullptr);
	return false;
}

//----------------------------------------------------------------------
// Get GradeMark
//----------------------------------------------------------------------
CSprite*	
MGuildMarkManager::GetGradeMark(WORD gradeID, Race race)
{
	if(race >= RACE_MAX)
		return NULL;

	return &m_GradeMarks[race][gradeID*2];
}

//----------------------------------------------------------------------
// Get GradeMarkSmall
//----------------------------------------------------------------------
CSprite*	
MGuildMarkManager::GetGradeMarkSmall(WORD gradeID, Race race)
{
	if(race >= RACE_MAX)
		return NULL;
	
	return &m_GradeMarks[race][gradeID*2+1];
}

//----------------------------------------------------------------------
// Get LevelMark
//----------------------------------------------------------------------
CSprite*	
MGuildMarkManager::GetLevelMark(WORD level)
{
	return &m_LevelMarks[level*2];
}

//----------------------------------------------------------------------
// Get LevelMarkSmall
//----------------------------------------------------------------------
CSprite*	
MGuildMarkManager::GetLevelMarkSmall(WORD level)
{
	return &m_LevelMarks[level*2+1];
}
