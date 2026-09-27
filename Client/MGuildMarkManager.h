//----------------------------------------------------------------------
// MGuildMarkManager.h
//----------------------------------------------------------------------
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
		// Create( guildID, pFilename은 *.bmp || *.jpg )
		//----------------------------------------------------------
		bool			CreateGuildMark(WORD guildID, const char* pFilename);
		static bool		CreateGuildMark(const char* pFilename, CSprite *&pSprite, CSprite *&pSpriteSmall);

		//----------------------------------------------------------
		// Add / Get
		//----------------------------------------------------------
		bool			HasGuildMark(WORD guildID) const;
		bool			AddGuildMark(WORD guildID, CSprite* pSprite, CSprite* pSpriteSmall);
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
		bool			SaveGuildMark(WORD guildID, CSprite* pSprite, CSprite* pSpriteSmall);
		static bool		MergeGuildMark(const char* pSPKFilenameOrg, const char* pIDMapperFilenameOrg,
										const char* pSPKFilenameApp, const char* pIDMapperFilenameApp);
//		bool			LoadGradeMark(WORD gradeSpriteID);

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

