//----------------------------------------------------------------------
// ProfileManager.cpp
//----------------------------------------------------------------------
#include "../../Client/Client_PCH.h"
#ifdef PLATFORM_WINDOWS
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/dir.h>
#endif
#include "CSpritePack.h"

	#include "Properties.h"

#include "ProfileManager.h"

// std::filesystem directory enumeration, in place of the _findfirst /
// _findnext walks these two functions used to run
// (docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md, priority 6).
#include "DirectoryListing.h"
#include "DataPath.h"

#include <string>
#include <vector>



//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
ProfileManager*		g_pProfileManager = NULL;

//----------------------------------------------------------------------
//
// constructor / destructor
//
//----------------------------------------------------------------------
ProfileManager::ProfileManager()
{
	InitializeCriticalSection(&m_Lock);
}

ProfileManager::~ProfileManager()
{
	Release();

	DeleteCriticalSection(&m_Lock);
}

//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
ProfileManager::Release()
{
	m_Profiles.clear();
}

//----------------------------------------------------------------------
// Has Profile
//----------------------------------------------------------------------
bool
ProfileManager::HasProfile(const char* pName) const
{
	return m_Profiles.find( std::string(pName) ) != m_Profiles.end();
}

//----------------------------------------------------------------------
// Add Profile
//----------------------------------------------------------------------
// 이미 있거나 말거나 관계없다.
//----------------------------------------------------------------------
void
ProfileManager::AddProfile(const char* pName, const char* pFilename)
{
	m_Profiles[std::string(pName)] = std::string(pFilename);
}

//----------------------------------------------------------------------
// Remove Profile
//----------------------------------------------------------------------
bool
ProfileManager::RemoveProfile(const char* pName)
{
	PROFILE_MAP::iterator iProfile = m_Profiles.find( std::string(pName) );

	if (iProfile!=m_Profiles.end())
	{
		m_Profiles.erase( iProfile );

		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Get UserInfo
//----------------------------------------------------------------------
const char*
ProfileManager::GetFilename(const char* pName) const
{
	PROFILE_MAP::const_iterator iProfile = m_Profiles.find( std::string(pName) );

	if (iProfile!=m_Profiles.end())
	{
		return iProfile->second.c_str();
	}

	return NULL;
}

//----------------------------------------------------------------------
// Init Profiles
//----------------------------------------------------------------------
// 프로그램이 실행될 때 한번 실행시켜주면 된다.
//
// Profile/*.bmp 를 읽어서 Profile/*.spr로 바꿔주면 된다.
//----------------------------------------------------------------------
void		
ProfileManager::InitProfiles()
{
	//-----------------------------------------------------------------
	// Profile Directory가 없으면 생성한다.
	//-----------------------------------------------------------------
	char CWD[_MAX_PATH];

	std::string sTest = g_pFileDef->getProperty("DIR_PROFILE");

	if (_getcwd( CWD, _MAX_PATH )!=NULL)
	{	
		if (_chdir( g_pFileDef->getProperty("DIR_PROFILE").c_str()) == 0)
		{
			// 있다면.. 다시 원래 DIR로..
			_chdir( CWD );
		}
		else
		{
			// DIR_PROFILE이 없다면.. 생성..
#ifdef PLATFORM_WINDOWS
			_mkdir( g_pFileDef->getProperty("DIR_PROFILE").c_str() );
#else
			mkdir( g_pFileDef->getProperty("DIR_PROFILE").c_str(), 0755 );
#endif
		}		
	}

	const std::string sProfileDir = g_pFileDef->getProperty("DIR_PROFILE");

	char bmpFilename[256];

	//-----------------------------------------------------------------
	// Every entry in the profile directory, which is what the legacy
	// "<dir>\*.*" _findfirst walk saw: on Win32 that pattern matches
	// every name - a name with no dot included - and subdirectories with
	// it, so the listing is asked for the same set. The only entries the
	// old walk produced that this one cannot are "." and "..", and both
	// are shorter than the eight characters the loop below requires.
	//-----------------------------------------------------------------
	std::vector<Basic::SDirectoryEntry>	vProfileFiles;

	if ( Basic::ListDirectory( sProfileDir.c_str(), "*", vProfileFiles,
			Basic::LIST_FILES_AND_DIRECTORIES ) )
	{
		CSpritePack SPK;

		// [0]은 작은거 (30, 38)
		// [1]은 큰거 (110, 139)
		SPK.Init( 2);

		// A CDirectDrawSurface-based branch used to run here on Windows,
		// with a CSpriteSurface `surface` used to Blt/Lock the loaded BMP
		// into SPK[]. With SPRITELIB_BACKEND_SDL (the only backend this
		// project builds, Windows included) CSpriteSurface no longer
		// inherits from CDirectDrawSurface and that branch never
		// type-checked; it's now a stub (see the notes further below), so
		// that intermediate surface isn't needed here any more either.
		const POINT bigSize = { 55, 70 };
		const POINT smallSize = { 30, 38 };

		// Note: SDL backend doesn't have InitOffsurface, surface will be created when needed

		for (size_t iFile=0; iFile<vProfileFiles.size(); iFile++)
		{
			const std::string&	sFilename = vProfileFiles[iFile].sName;

			//---------------------------------------------------------
			// _finddata_t::name capped a name at 259 bytes; a listing
			// entry carries no such cap, and the .spki path built below
			// is one byte longer than the path built here. An entry
			// that would not leave room for both is skipped rather than
			// written past the end of these 256-byte buffers.
			//---------------------------------------------------------
			if (sProfileDir.size() + 1 + sFilename.size() + 1 >= sizeof(bmpFilename))
			{
				continue;
			}

			snprintf(bmpFilename, sizeof(bmpFilename), "%s\\%s", sProfileDir.c_str(), sFilename.c_str());

			//---------------------------------------------------------
			// Read the bmp and turn it into a sprite.
			//---------------------------------------------------------
			char charName[256], spkFilename[256];
			int lenFilename = (int)sFilename.size();

			// "name.bmp"
			if (lenFilename< 8)
			{
				continue;
			}

			strncpy( charName, sFilename.c_str(), lenFilename-4 );	// cut the .bmp off
			charName[lenFilename-4] = '\0';

			// CDirectDrawSurface-based loading used to run here on Windows,
			// but with SPRITELIB_BACKEND_SDL CSpriteSurface no longer
			// inherits from CDirectDrawSurface (LoadImageToSurface() has no
			// overload for it), so that branch never type-checked - see the
			// stub notes further below. Profile image loading is not yet
			// implemented on the SDL backend (Windows included); this is a
			// non-critical feature (profile character portraits).
			// TODO: Implement SDL_image based loading
			WORD* lpSurface;
			unsigned short pitch;

			// For now, just initialize empty sprites
			// The profile will load but without character portrait image
			lpSurface = new WORD[smallSize.x * smallSize.y];
			memset(lpSurface, 0, smallSize.x * smallSize.y * 2);
			pitch = smallSize.x * 2;
			SPK[0].SetPixelNoColorkey(lpSurface, pitch, smallSize.x, smallSize.y);
			delete[] lpSurface;

			lpSurface = new WORD[bigSize.x * bigSize.y];
			memset(lpSurface, 0, bigSize.x * bigSize.y * 2);
			pitch = bigSize.x * 2;
			SPK[1].SetPixelNoColorkey(lpSurface, pitch, bigSize.x, bigSize.y);
			delete[] lpSurface;

			// filename.spk. The directory was joined with a backslash above;
			// off Windows it is folded to '/' here, or the pack would be a file
			// named "UserSet\<name>.spk" beside the directory instead of in it.
			int lenBmpFilename = strlen(bmpFilename);
			snprintf(spkFilename, sizeof(spkFilename), "%.*sspk", lenBmpFilename-3, bmpFilename);
			const std::string sSpkFilename = Basic::NormalizeDataPath(spkFilename);

			// filename.spki, derived the way the readers derive it
			const std::string sSpkiFilename = sSpkFilename + "i";

			std::ofstream	spkFile(sSpkFilename, ios::binary);
			std::ofstream	spkiFile(sSpkiFilename, ios::binary);
			SPK.SaveToFile( spkFile, spkiFile );
			spkFile.close();
			spkiFile.close();

			g_pProfileManager->AddProfile( charName, sSpkFilename.c_str() );
		}
	}
}

//----------------------------------------------------------------------
// Delete Profiles
//----------------------------------------------------------------------
// 프로그램이 실행될 때 한번 실행시켜주면 된다.
//
// Profile/*.spr 화일을 모두 지우면 된다.
//----------------------------------------------------------------------
void		
ProfileManager::DeleteProfiles()
{
	const std::string sProfileDir = g_pFileDef->getProperty("DIR_PROFILE");

	char spkFilename[256];

	//-----------------------------------------------------------------
	// Both walks below list files only, where the legacy _findfirst
	// patterns also matched a subdirectory of the same name. remove()
	// cannot delete a directory on Windows, so the entries that are no
	// longer listed are exactly the ones the loop body could never have
	// acted on.
	//-----------------------------------------------------------------
	std::vector<Basic::SDirectoryEntry>	vProfileFiles;

	//-----------------------------------------------------------------
	// Find the *.spk files.
	//-----------------------------------------------------------------
	if ( Basic::ListDirectory( sProfileDir.c_str(), "*.spk*", vProfileFiles ) )
	{
		for (size_t iFile=0; iFile<vProfileFiles.size(); iFile++)
		{
			const std::string&	sFilename = vProfileFiles[iFile].sName;

			if (sProfileDir.size() + 1 + sFilename.size() >= sizeof(spkFilename))
			{
				continue;
			}

			snprintf(spkFilename, sizeof(spkFilename), "%s\\%s", sProfileDir.c_str(), sFilename.c_str());
			remove(Basic::NormalizeDataPath(spkFilename).c_str());
		}
	}

	//-----------------------------------------------------------------
	// Find the *-spk temporary files.
	//-----------------------------------------------------------------
	if ( Basic::ListDirectory( sProfileDir.c_str(), "*-spk*", vProfileFiles ) )
	{
		for (size_t iFile=0; iFile<vProfileFiles.size(); iFile++)
		{
			const std::string&	sFilename = vProfileFiles[iFile].sName;

			if (sProfileDir.size() + 1 + sFilename.size() >= sizeof(spkFilename))
			{
				continue;
			}

			snprintf(spkFilename, sizeof(spkFilename), "%s\\%s", sProfileDir.c_str(), sFilename.c_str());
			remove(Basic::NormalizeDataPath(spkFilename).c_str());
		}
	}
}

