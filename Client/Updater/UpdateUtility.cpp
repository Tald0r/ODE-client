//----------------------------------------------------------------------
// UpdateUtility.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include <io.h>
#include <direct.h>
#include "UpdateUtility.h"
//#include "SPKFileLib.h"
#include "CFileIndexTable.h"
// std::filesystem directory enumeration, in place of the _findfirst /
// _findnext walks UUFDeleteDirectory() and UUFDeleteFiles() used to run
// (docs/history/cpp17-cpp20-compatibility-assessment-2026-09-04.md, priority 6).
// Included before the NULL redefinition below so that the standard
// headers it pulls in see the compiler's own NULL.
#include "DirectoryListing.h"


#define	NULL	0

//----------------------------------------------------------------------
// Pack의 Size의 byte 수
//----------------------------------------------------------------------
typedef	unsigned short		TYPE_PACKSIZE;
#define	SIZE_PACKSIZE		2

// 한번에 카피하는 byte수
#define	SIZE_BUFFER			4096


//----------------------------------------------------------------------
// Has Permission
//----------------------------------------------------------------------
// Directory나 File 접근시에.. 다른 Directory에 있는걸 건들이면 안된다.
//----------------------------------------------------------------------
bool
UUFHasPermission(const char* filename)
{	
	//------------------------------------------------------------
	// filename이 없을 경우 
	//------------------------------------------------------------
	if (filename==NULL)
	{	
		return false;
	}

	//------------------------------------------------------------
	// 첫 문자가 "\"이면 안된다. 
	// (최상위 디렉토리로 접근...가능한가? - -;; 그냥 함 해봄..)
	//------------------------------------------------------------
	if (filename[0]=='\\' || filename[0]=='/')
	{
		return false;
	}

	//------------------------------------------------------------
	// ":"이 들어가면 안된다. (드라이브를 바꿀 수 있다.)
	//------------------------------------------------------------
	if (strchr( filename, ':' )!=NULL)
	{
		return false;
	}
	
	//------------------------------------------------------------
	// ".." 이 들어가면 안된다. (상위 디렉토리이므로)
	//------------------------------------------------------------
//	if (strstr( filename, ".." )!=NULL)
//	{
//		return false;
//	}
   

	return true;
}

//----------------------------------------------------------------------
// Create Directory
//----------------------------------------------------------------------
// dirName : 현재 directory 밑에 생성할려는 directory 이름
//----------------------------------------------------------------------
// 새로운 Directory를 생성한다.
//----------------------------------------------------------------------
bool
UUFCreateDirectory(const char* dirName)
{
	// Directory 생성
	if (UUFHasPermission( dirName ))
	{
		// 제대로 생성한 경우
		if (_mkdir( dirName )==0)
		{
			return true;
		}
	}

	return false;
}

//----------------------------------------------------------------------
// Remove Directory
//----------------------------------------------------------------------
// dirName : 지울려는 directory 이름
//----------------------------------------------------------------------
// Directory를 지운다.
//----------------------------------------------------------------------
bool
UUFDeleteDirectory(const char* dirName)
{
	if (UUFHasPermission( dirName ))
	{
		// 제대로 지운 경우
		if (_rmdir( dirName )==0)
		{
			return true;
		}
		// Not removed because something is inside.
		else //if (errno!=ENOENT)
		{
			char CWD[_MAX_PATH];

			// Remember the current directory.
			GetCurrentDirectory(_MAX_PATH, CWD);
			
			if (_chdir( dirName ) == 0)
			{
				//---------------------------------------------------
				// Delete the files one by one. T_T;
				//
				// The _chdir dance is kept, so remove() still takes a
				// bare name relative to dirName and the working
				// directory is restored below exactly as it was.
				//
				// Files only, where the legacy "*.*" also matched
				// subdirectories: remove() cannot delete a directory on
				// Windows, so the entries no longer listed are exactly
				// the ones this loop could never have acted on. The
				// pattern is "*" and not "*.*" because a '.' is a
				// literal to Basic::ListDirectory while Win32 read
				// "*.*" as "everything".
				//---------------------------------------------------
				std::vector<Basic::SDirectoryEntry>	vFiles;

				// Read every file.
				if ( Basic::ListDirectory( ".", "*", vFiles ) )
				{
					for (size_t iFile=0; iFile<vFiles.size(); iFile++)
					{
						const std::string&	sFilename = vFiles[iFile].sName;

						// No need to delete names starting with a dot.
						// "." and ".." are never listed, so the test now
						// only skips dotfiles.
						if (!sFilename.empty() && sFilename[0] != '.')
						{
							remove( sFilename.c_str() );
						}
					}
				}
				else
				{
					// A readable but empty directory lists nothing and
					// returns true, which is what _findfirst( "*.*" ) did
					// - "." always matched, so the legacy walk never took
					// this branch for a directory it had just chdir'd
					// into. This branch is taken for a directory that
					// cannot be enumerated, as before, and also for a
					// mid-walk error, which the listing treats as
					// all-or-nothing where the legacy walk kept what it
					// had already deleted.
					//
					// This early return leaves the working directory in
					// dirName. The leak is pre-existing and deliberately
					// not fixed here, though the widened failure set
					// means slightly more runs can reach it.
					return false;
				}

//				_chdir( "..\\" );
				SetCurrentDirectory(CWD);
				if (_rmdir( dirName )==0)
				{
					// 잘 지워졌다.
					return true;
				}
			}		
			else
			{
				return false;
			}
		}		
	}

	return false;
}

//----------------------------------------------------------------------
// Remove Files
//----------------------------------------------------------------------
// dirName : 지울려는 directory 이름
//----------------------------------------------------------------------
// Directory안의 파일들을 지운다.
//----------------------------------------------------------------------
bool
UUFDeleteFiles(const char *path, const char *fileext)
{
	char cwd[512];
	_getcwd(cwd, 512);

	if (_chdir( path ) == 0)
	{
		//---------------------------------------------------
		// Delete the files one by one. T_T;
		//
		// The _chdir dance is kept, so remove() still takes a bare
		// name relative to path and the working directory is restored
		// below exactly as it was.
		//
		// Files only, where the legacy pattern could also match a
		// subdirectory: remove() cannot delete a directory on Windows,
		// so the entries no longer listed are exactly the ones this
		// loop could never have acted on.
		//---------------------------------------------------

		//---------------------------------------------------
		// fileext is caller-supplied and was a _findfirst pattern, and
		// Win32 reads "*.*" as "everything" rather than as "names
		// containing a dot". Basic::ListDirectory takes the '.'
		// literally, so the spelling is translated here rather than at
		// a call site: this function has no caller in the tree, so
		// there is no call site to fix, and the header keeps
		// advertising the DOS spelling callers would reach for. The
		// remaining DOS-only quirks - "*." meaning "no extension", a
		// trailing '?' matching zero characters - are not translated;
		// no pattern in this tree uses either.
		//---------------------------------------------------
		const char*	pPattern = (fileext != NULL && strcmp( fileext, "*.*" ) == 0) ? "*" : fileext;

		std::vector<Basic::SDirectoryEntry>	vFiles;

		// Read every file.
		if ( Basic::ListDirectory( ".", pPattern, vFiles ) )
		{
			for (size_t iFile=0; iFile<vFiles.size(); iFile++)
			{
				const std::string&	sFilename = vFiles[iFile].sName;

				// No need to delete names starting with a dot. "." and
				// ".." are never listed, so the test now only skips
				// dotfiles.
				if (!sFilename.empty() && sFilename[0] != '.')
				{
					remove( sFilename.c_str() );
				}
			}
		}

	}
	_chdir(cwd);

	return true;
}

//----------------------------------------------------------------------
// Copy File
//----------------------------------------------------------------------
// SourceFile : 원본
// TargetFile : 목적 filename
//----------------------------------------------------------------------
// SourceFile을 TargetFile로 copy해서 새로운 file을 생성한다.
//----------------------------------------------------------------------
bool
UUFCopyFile(const char* FilenameSource, const char* FilenameTarget)
{				
	if (UUFHasPermission( FilenameTarget ) && UUFHasPermission( FilenameSource))				
	{
		std::ifstream fileSource(FilenameSource, ios::binary);
		std::ofstream fileTarget(FilenameTarget, ios::binary);
		
		// 추가
		char buffer[SIZE_BUFFER];
		int n;
		
		//---------------------------------------------------------------
		// addFile을 읽어서 originalFile의 끝에 붙인다.
		//---------------------------------------------------------------
		while (1)
		{
			fileSource.read(buffer, SIZE_BUFFER);
			
			n = fileSource.gcount();

			if (n > 0)
			{		
				fileTarget.write(buffer, n);
			}
			else
			{
				break;
			}
		}

		fileSource.close();
		fileTarget.close();		

		return true;
	}

	return false;
}
					
//----------------------------------------------------------------------
// Move File
//----------------------------------------------------------------------
// SourceFile : 원본
// TargetFile : 목적 filename
//----------------------------------------------------------------------
// SourceFile을 TargetFile로 rename해서 새로운 file을 생성한다.
//----------------------------------------------------------------------
bool
UUFMoveFile(const char* FilenameSource, const char* FilenameTarget)
{				
	if (UUFHasPermission( FilenameTarget ) && UUFHasPermission( FilenameSource))				
	{
		ifstream file;

		file.open(FilenameTarget, ios::binary | );
		
		if (file.is_open())
		{
			file.close();

			// 원래 있던게 있으면 지운다.
			if (remove( FilenameTarget )!=0)
			{
				// 못 지운 경우
				return false;
			}
		}		
	
		// 화일이름을 바꾼다.				
		if (rename( FilenameSource, FilenameTarget )==0)
		{
			return true;
		}		
	}

	return false;
}

//----------------------------------------------------------------------
// Delete File
//----------------------------------------------------------------------
// TargetFile : 삭제할 filename
//----------------------------------------------------------------------
// TargetFile을 지운다.
//----------------------------------------------------------------------
bool
UUFDeleteFile(const char* FilenameTarget)
{
	// file을 지운다.
	if (UUFHasPermission( FilenameTarget ))
	{
		if (remove( FilenameTarget )==0)
		{
			return true;
		}
	}

	return false;
}


//----------------------------------------------------------------------
// Rename File
//----------------------------------------------------------------------
// SourceFile : old filename
// TargetFile : new filename
//----------------------------------------------------------------------
// SourceFile을 TargetFile로 이름을 바꾼다.
// Directory name도 바꿀 수 있다.
//----------------------------------------------------------------------
bool
UUFRenameFile(const char* FilenameSource, const char* FilenameTarget)
{
	// 화일이름을 바꾼다.
	if (UUFHasPermission( FilenameTarget ))
	{
		if (rename( FilenameSource, FilenameTarget )==0)
		{
			return true;
		}
	}

	return false;
}


//----------------------------------------------------------------------
// Append Pack
//----------------------------------------------------------------------
// SourceFile		: Pack(to add)
// TargetFile		: Pack(original)
// SourceIndexFile	: PackIndex(to add)
// TargetIndexFile	: PackIndex(original)
//----------------------------------------------------------------------
// TargetFile의 끝에 SourceFile을 추가한다.
// 단, TargetFile의 전체 개수를 추가된 것 만큼 증가해야 한다.    
//----------------------------------------------------------------------
bool
UUFAppendPack(const char* FilenameAdd, const char* FilenameOriginal,
			const char* FilenameIndexAdd, const char* FilenameIndexOriginal)
{	
	//--------------------------------------------------
	// Index도 추가하는가?
	//--------------------------------------------------
	bool bAppendIndex;

	if (FilenameIndexAdd==NULL && FilenameIndexOriginal==NULL)
	{
		bAppendIndex = false;
	}
	else
	{
		bAppendIndex = true;
	}

	//--------------------------------------------------
	// permission 체크
	//--------------------------------------------------
	if (UUFHasPermission( FilenameOriginal ) 
		&& UUFHasPermission( FilenameAdd)

		// index가 없거나.. 있을때는 permission있어야 한다.
		&& (!bAppendIndex ||
			UUFHasPermission( FilenameIndexOriginal ) 
			&& UUFHasPermission( FilenameIndexAdd )
		))		
	{
		//---------------------------------------------------------------
		// 정해진 Filesize랑 같은가?
		//---------------------------------------------------------------
		//if (!IsFileSizeBeforeOK())
		//{
			// Update하려고 하는데 원하는 Filesize가 아니면.. 심각하다!!						
		//	return false;
		//}

		//---------------------------------------------------------------
		//
		//					Pack을 추가한다.
		//
		//---------------------------------------------------------------
		// 추가할 수 있게 한다.
		std::ifstream addFile(FilenameAdd, ios::binary);
		class fstream originalFile(FilenameOriginal, ios::in | ios::out | ios::binary);

		TYPE_PACKSIZE	sourceCount, targetCount;
		
		//---------------------------------------------------------------
		// source의 개수를 저장해 둔다.
		//---------------------------------------------------------------
		addFile.read((char*)&sourceCount, SIZE_PACKSIZE);

		//---------------------------------------------------------------
		// target File Pointer를 끝으로..
		//---------------------------------------------------------------
		originalFile.seekp(0, ios::end);

		//---------------------------------------------------------------
		// Original File의 크기를 기억해둔다.
		//---------------------------------------------------------------
		long originalPackFileSize = originalFile.tellp();

		// 추가
		char buffer[SIZE_BUFFER];
		int n;
		//---------------------------------------------------------------
		// addFile을 읽어서 originalFile의 끝에 붙인다.
		//---------------------------------------------------------------
		while (1)
		{
			addFile.read(buffer, SIZE_BUFFER);
			
			n = addFile.gcount();

			if (n > 0)
			{		
				originalFile.write(buffer, n);
			}
			else
			{
				break;
			}
		}

		//---------------------------------------------------------------
		// 개수를 변경시켜준다. (originalFile + addFile)
		//---------------------------------------------------------------
		originalFile.seekg(0, ios::beg);
		originalFile.read((char*)&targetCount, SIZE_PACKSIZE);

		targetCount += sourceCount;

		originalFile.seekp(0, ios::beg);				
		originalFile.write((const char*)&targetCount, SIZE_PACKSIZE);

		// 끝
		addFile.close();
		originalFile.close();

		//---------------------------------------------------------------
		// 정해진 Filesize랑 같은가?
		//---------------------------------------------------------------
		//if (!IsFileSizeAfterOK())
		//{
			// Update를 했는데 원하는 Filesize가 아니면.. 심각하다!!
			// 심각한 경우이다.
		//	return false;
		//}

		
		//---------------------------------------------------------------
		//
		//					Pack Index를 추가한다.
		//
		//---------------------------------------------------------------	
		
		// Index를 추가할려는 경우에만..
		if (bAppendIndex)
		{
			//---------------------------------------------------------------
			// 정해진 Filesize랑 같은가?
			//---------------------------------------------------------------
			//if (!IsFileSizeBeforeOK())
			//{
				// Update하려고 하는데 원하는 Filesize가 아니면.. 심각하다!!						
			//	return false;
			//}


			// 추가할 수 있게 한다.
			std::ifstream addIndexFile(FilenameIndexAdd, ios::binary);
			class fstream originalIndexFile(FilenameIndexOriginal, ios::in | ios::out | ios::binary);	

			TYPE_PACKSIZE	targetCount;

			//---------------------------------------------------------------
			// source index를 load한다.
			//---------------------------------------------------------------
			CFileIndexTable	sourceIndexFile;
			sourceIndexFile.LoadFromFile( addIndexFile );

			//---------------------------------------------------------------
			// target File Pointer를 끝으로..
			//---------------------------------------------------------------
			originalIndexFile.seekp(0, ios::end);
			
			//---------------------------------------------------------------
			// OriginalFile의 크기에서부터 file pointer가 시작되므로
			//---------------------------------------------------------------
			long targetEnd = originalPackFileSize - SIZE_PACKSIZE;

			//---------------------------------------------------------------
			// Source의 각 file position을 
			// targetEnd만큼 증가시켜서 originalIndexFile에 추가한다.
			//---------------------------------------------------------------
			long sourceIndex;
			for (int i=0; i<sourceIndexFile.GetSize(); i++)
			{
				sourceIndex = targetEnd + sourceIndexFile[i];
				originalIndexFile.write((const char*)&sourceIndex, 4);
			}								

			//---------------------------------------------------------------
			// 개수를 변경시켜준다. (originalIndexFile + addIndexFile)
			//---------------------------------------------------------------
			originalIndexFile.seekg(0, ios::beg);
			originalIndexFile.read((char*)&targetCount, SIZE_PACKSIZE);

			targetCount += sourceIndexFile.GetSize();

			originalIndexFile.seekp(0, ios::beg);				
			originalIndexFile.write((const char*)&targetCount, SIZE_PACKSIZE);

			// 끝
			addIndexFile.close();
			originalIndexFile.close();

			//---------------------------------------------------------------
			// 정해진 Filesize랑 같은가?
			//---------------------------------------------------------------
			//if (!IsFileSizeAfterOK())
			//{
				// Update를 했는데 원하는 Filesize가 아니면.. 심각하다!!
				// 심각한 경우이다.
			//	return false;
			//}	
		}

		return true;
	}

	return false;
}



//----------------------------------------------------------------------
// Append Info
//----------------------------------------------------------------------
// FilenameAdd		: 추가될 Information File
// FilenameOriginal : 원래의 Information File
//----------------------------------------------------------------------
// TargetFile의 끝에 SourceFile을 추가한다.
// 단, TargetFile의 전체 개수를 추가된 것 만큼 증가해야 한다.    
//----------------------------------------------------------------------
bool
UUFAppendInfo(const char* FilenameAdd, const char* FilenameOriginal)
{
	if (UUFHasPermission( FilenameOriginal ) && UUFHasPermission( FilenameAdd))				
	{			
		//------------------------------------------
		// 정해진 Filesize랑 같은가?
		//------------------------------------------
		//if (!IsFileSizeBeforeOK())
		//{
			// Update하려고 하는데 원하는 Filesize가 아니면.. 심각하다!!						
		//	return false;
		//}

		// 추가할 수 있게 한다.
		std::ifstream sourceFile(FilenameAdd, ios::binary);
		class fstream targetFile(FilenameOriginal, ios::in | ios::out | ios::binary);
		

		int sourceCount, targetCount;
		// source의 개수를 저장해 둔다.
		sourceFile.read((char*)&sourceCount, 4);

		// target File Pointer를 끝으로..
		targetFile.seekp(0, ios::end);

		// 추가
		char buffer[SIZE_BUFFER];
		int n;
		// SourceFile을 읽어서 TargetFile의 끝에 붙인다.
		while (1)
		{
			sourceFile.read(buffer, SIZE_BUFFER);
			
			n = sourceFile.gcount();

			if (n > 0)
			{		
				targetFile.write(buffer, n);
			}
			else
			{
				break;
			}
		}

		// 개수를 변경시켜준다. (targetFile + sourceFile)
		targetFile.seekg(0, ios::beg);
		targetFile.read((char*)&targetCount, 4);

		targetCount += sourceCount;

		targetFile.seekp(0, ios::beg);				
		targetFile.write((const char*)&targetCount, 4);

		// 끝
		sourceFile.close();
		targetFile.close();


		//------------------------------------------
		// 정해진 Filesize랑 같은가?
		//------------------------------------------
		//if (!IsFileSizeAfterOK())
		//{
			// Update를 했는데 원하는 Filesize가 아니면.. 심각하다!!
			// 심각한 경우이다.
		//	return false;
		//}
	}

	return true;
}		

//----------------------------------------------------------------------
// Update SpritePack
//----------------------------------------------------------------------
// SpritePack에서 특정한 sprite들만 교체한다.
//----------------------------------------------------------------------
