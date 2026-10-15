//----------------------------------------------------------------------
// MGuildMarkManager.h
//----------------------------------------------------------------------
// The guild, grade and level marks (task 4.230: compiled in VS_UI as
// VS_UI/src/MGuildMarkManager.cpp). Creating, saving and merging guild
// marks, which nothing called, went with it.
//----------------------------------------------------------------------

#ifndef __MGUILD_MARK_MANAGER_H__
#define __MGUILD_MARK_MANAGER_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include "CSprite.h"
#include "CSpritePack.h"
#include "MString.h"
#include "RaceType.h"
#include <map>

#include "GuildMarkTypes.h"
//----------------------------------------------------------------------
// MGuildMarkManager
//----------------------------------------------------------------------
class MGuildMarkManager {
	public :
		struct GUILD_SPRITES
		{
			GUILD_SPRITES(CSprite* _pSprite=NULL, CSprite* _pSpriteSmall=NULL)
			{
				pSprite = _pSprite;
				pSpriteSmall = _pSpriteSmall;
			}

			CSprite* pSprite;
			CSprite* pSpriteSmall;
		};

		typedef std::map<WORD, GUILD_SPRITES>		GUILDMARK_MAP;
		
	public :
		MGuildMarkManager(	const char* pGuildMarkSPKFilename, 
							const char* pGuildMarkSpriteIDMapperFilename,
							const char *pSlayerGradeMarkSPKFilename,
							const char *pVampireGradeMarkSPKFilename,
							const char *pOustersGradeMarkSPKFilename,
							const char *pLevelMarkSPKFilename
							);
		~MGuildMarkManager();

		bool			IsInit() const		{ return (m_GuildMarkSPKFilename.GetLength()+m_GuildMarkSpriteMapperFilename.GetLength())!=0; }

		//----------------------------------------------------------
		// Release
		//----------------------------------------------------------
		void			Release();

		//----------------------------------------------------------
		// Get
		//----------------------------------------------------------
		bool			HasGuildMark(WORD guildID) const;
		CSprite*		GetGuildMark(WORD guildID);
		CSprite*		GetGuildMarkSmall(WORD guildID);

		CSprite*		GetGradeMark(WORD gradeID, Race race);
		CSprite*		GetGradeMarkSmall(WORD gradeID, Race race);

		CSprite*		GetLevelMark(WORD level);
		CSprite*		GetLevelMarkSmall(WORD level);

		//----------------------------------------------------------
		// File I/O
		//----------------------------------------------------------
		bool			LoadGuildMark(WORD guildID);

	protected :
		MString						m_GuildMarkSPKFilename;
		MString						m_GuildMarkSpriteMapperFilename;
		GUILDMARK_MAP				m_GuildMarks;

//		MString						m_SlayerGradeMarkSPKFilename;
//		MString						m_VampireGradeMarkSPKFilename;
		CSpritePack					m_GradeMarks[RACE_MAX];
		CSpritePack					m_LevelMarks;
};

//----------------------------------------------------------------------
// global
//----------------------------------------------------------------------
extern MGuildMarkManager* g_pGuildMarkManager;

#endif

