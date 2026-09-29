//----------------------------------------------------------------------
// MChatManager.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MChatManager.h"


	#include "MGameStringTable.h"


//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
MChatManager*		g_pChatManager = NULL;
const MChatHost*	MChatManager::s_pHost = NULL;

//----------------------------------------------------------------------
// Static
//----------------------------------------------------------------------
char MChatManager::s_MaskString[256] = //"^^; -_-; !_!; o_O; *_*; m_m; u_u; p_q; =_=; -_+; $_$; v_v; Y_Y; o_o; O_O; w_w; #_#; ._.; n_n; &_&; @_@; 0_0; _-_; +_+; +_=; ~_~;";
		"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
		
char MChatManager::s_MaskString2[256] = 
		"#&*%!$#%&*@!%&$&#*&$@#*!%$&@#&*%!$#%&*@!%&$&#*&$@&#!%$*&%*#@#*!%$&@#&*%!$#%&*@!%&$&#*&$@#*!%$&@#&*!$#%&*@!%&$&#*&$@#*!%$&@#&*%!$#%&!$#%&*@!%&$&#*&$@#*!%$&@#&*%!$#%&*";
		//".....................................................................................................................................................................";

//----------------------------------------------------------------------
// Mask Char
//----------------------------------------------------------------------
// The n-th character of a mask text, which repeats. The texts are
// shorter than their 256-byte arrays (the rest is NULs), so indexing them
// by n directly cut a long line with a NUL and then read past the array.
//----------------------------------------------------------------------
static char
MaskChar(const char* mask, int n)
{
	return mask[ n % static_cast<int>(strlen(mask)) ];
}

//----------------------------------------------------------------------
//
// constructor / destructor
// 
//----------------------------------------------------------------------
MChatManager::MChatManager()
{
	m_bIgnoreMode = false;
}

MChatManager::~MChatManager()
{
}

//----------------------------------------------------------------------
// Host
//----------------------------------------------------------------------
const MChatHost*
MChatManager::SetHost(const MChatHost* pHost)
{
	const MChatHost* pPrevious = s_pHost;
	s_pHost = pHost;
	return pPrevious;
}

bool
MChatManager::HostFilteringCurse()
{
	if (s_pHost==NULL || s_pHost->FilteringCurse==NULL)
	{
		return true;
	}

	return s_pHost->FilteringCurse();
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
void				
MChatManager::SaveToFile(const char* filename)
{
	std::ofstream file(filename, ios::binary);

	m_mapCurseEng.SaveToFile( file );
	m_mapCurseKor1.SaveToFile( file );
	m_mapCurseKor2.SaveToFile( file );
	m_mapCurseKor3.SaveToFile( file );
	m_mapCurseKor4.SaveToFile( file );
	m_mapID.SaveToFile( file );
	
	file.close();
}

//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
void				
MChatManager::LoadFromFile(const char* filename)
{
	std::ifstream file(filename, ios::binary);

	m_mapCurseEng.LoadFromFile( file );
	m_mapCurseKor1.LoadFromFile( file );
	m_mapCurseKor2.LoadFromFile( file );
	m_mapCurseKor3.LoadFromFile( file );
	m_mapCurseKor4.LoadFromFile( file );
	m_mapID.LoadFromFile( file );

	file.close();
}

//----------------------------------------------------------------------
// Load From File Curse
//----------------------------------------------------------------------
// text file에서 욕 읽기
//----------------------------------------------------------------------
void				
MChatManager::LoadFromFileCurse(const char* filename)
{
	std::ifstream file(filename);

	if (!file.is_open())
	{
		return;
	}

	char str[256];

	//-----------------------------------------------------
	// text file에서 죽죽~~ 읽어들인다.
	//-----------------------------------------------------
	while (!file.eof())
	{
		file >> str;

		bool bEng = true;
		bool bKor = true;

		char* strTemp = str;
		char ch;

		//-----------------------------------------------------
		// 단어가 영어인지 한글인지 판단한다.
		//-----------------------------------------------------
		while ((ch=*strTemp++))
		{
			//-----------------------------------------------------
			// 한글인 경우
			//-----------------------------------------------------
			if (ch & 0x80)
			{
				if (*strTemp=='\0')
				{
					bKor = false;
					break;
				}

				strTemp++;

				bEng = false;
			}
			//-----------------------------------------------------
			// 영어인 경우
			//-----------------------------------------------------
			else if (ch>='a' && ch<='z')
			{
				bKor = false;
			}
			//-----------------------------------------------------
			// 아니면 .. 버린다.
			//-----------------------------------------------------
			else				
			{
				bEng = false;
				bKor = false;

				break;
			}
		}

		//-----------------------------------------------------
		// 영어..라고 판단된 경우
		//-----------------------------------------------------
		if (bEng)
		{
			m_mapCurseEng.Add(str);
		}
		//-----------------------------------------------------
		// 한글..이라고 판단된 경우
		//-----------------------------------------------------
		else if (bKor)
		{
			//-----------------------------------------------------
			// 한글 1,2,3,4자만 허용한다.
			//-----------------------------------------------------
			switch (strlen(str))
			{
				case 2 : m_mapCurseKor1.Add(str); break;
				case 4 : m_mapCurseKor2.Add(str); break;
				case 6 : m_mapCurseKor3.Add(str); break;
				case 8 : m_mapCurseKor4.Add(str); break;
			}
		}
	}

	file.close();
}

//----------------------------------------------------------------------
// Remove Curse
//----------------------------------------------------------------------
bool				
MChatManager::RemoveCurse(char* str, bool bForce) const
{
		if ((!HostFilteringCurse() && bForce == false) || str==NULL)
		{
			return false;
		}


	bool existCurseEng = false;
	bool existCurseKor = false;

	int len = static_cast<int>(strlen(str));
	int i;
	int index;
	
	//const char* strMask = //"^^; -_-; !_!; o_O; *_*; m_m; u_u; p_q; =_=; -_+; $_$; v_v; Y_Y; o_o; O_O; w_w; #_#; ._.; n_n; &_&; @_@; 0_0; _-_; +_+; +_=; ~_~;";
	//	"#&*%!$#%&*@!%&$&#*&$@#*!%$&@#&*%!$#%&*@!%&$&#*&$@&#!%$*&%*#@#*!%$&@#&*%!$#%&*@!%&$&#*&$@#*!%$&@#&*%!$#%&*@!%&$&#*&$@#*!%$&@";

	//------------------------------------------------------------
	// the string with only the letters that matter
	//------------------------------------------------------------
	char*	strFiltered = new char [len+1];		

	//------------------------------------------------------------
	// each filtered letter's index in the original string
	//------------------------------------------------------------
	int*	indexFiltered = new int [len+1];		

	//------------------------------------------------------------
	// whether each filtered letter is part of a curse
	//------------------------------------------------------------
	//bool*	isCurse = new bool [len+1];
	BYTE*	isCurse = new BYTE [len+1];
	

	//------------------------------------------------------------
	//
	//					English curses
	//
	//------------------------------------------------------------
	// Keep only the letters, lower-cased, and
	// (!) look for the curses in that string.
	//------------------------------------------------------------
	// hi, hello! f.u.c.k!~~!
	// --> hihellofuck
	//	 Keeping each letter's index in the original string makes
	//	 replacing it there easy.
	//
	// The same curse can occur more than once, so strstr runs repeatedly.
	//------------------------------------------------------------
	char*	strFilteredPtr = strFiltered;
	int*	indexFilteredPtr = indexFiltered;
	BYTE*	isCursePtr = isCurse;
	
	char*	strOrg = str;	// the cursor over the original
	
	char	ch;
	const char toLower = 'a'-'A';

	//------------------------------------------------------------
	// Filter out the English letters, lower-cased.
	//------------------------------------------------------------
	i = 0;
	index = 0;
	while ((ch = *strOrg++))//, ch != '\0')
	{
		//----------------------------------------------
		// lower case --> kept as it is
		//----------------------------------------------
		if (ch >= 'a' && ch <= 'z')
		{
			*strFilteredPtr++	= ch;
			*indexFilteredPtr++ = i;
			*isCursePtr++		= false;	// not a curse yet
			index++;
		}		
		//----------------------------------------------
		// upper case --> lower-cased
		//----------------------------------------------
		else if (ch >= 'A' && ch <= 'Z')
		{
			*strFilteredPtr++	= ch + toLower;
			*indexFilteredPtr++ = i;
			*isCursePtr++		= false;	// not a curse yet
			index++;
		}
		
		//----------------------------------------------
		// anything else is skipped
		//----------------------------------------------

		i++;
	}
	*strFilteredPtr = '\0';
	
	//------------------------------------------------------------
	// English curses are looked for only when there are English letters.
	//------------------------------------------------------------
	if (index!=0)
	{
		MStringMap::const_iterator iString = m_mapCurseEng.begin();
		
		//------------------------------------------------------------
		// Look for every curse in strFiltered.
		//------------------------------------------------------------
		while (iString != m_mapCurseEng.end())
		{
			const MString* pString = iString->second;

			if (pString!=NULL)
			{
				strFilteredPtr = strFiltered;

				char* pFind = NULL;

				//---------------------------------------------------
				// find the curse in strFiltered
				//---------------------------------------------------
				while ((pFind = strstr( strFilteredPtr, pString->GetString() )))
				{					
					int lenCurse = static_cast<int>(pString->GetLength());

					//---------------------------------------------------
					// found: mark its letters, counted from the start of
					// strFiltered (not from where this search started, so
					// a second occurrence is marked where it is)
					//---------------------------------------------------
					memset( isCurse+(pFind-strFiltered), lenCurse, lenCurse);
					
					//---------------------------------------------------
					// where the next search starts
					//---------------------------------------------------
					strFilteredPtr = pFind + lenCurse;

					existCurseEng = true;	// a curse was found
				}
			}

			iString++;
		}

		//------------------------------------------------------------
		// Mask the curses found.
		//------------------------------------------------------------
		for (int i=0; i<index; i++)
		{
			if ( isCurse[i] )
			{
				// a curse: mask its letter in the original string
				str[ indexFiltered[i] ] = MaskChar( s_MaskString, i );
			}
		}

	}

	
	//------------------------------------------------------------
	// Korean curses of one, two, three and four syllables, each
	// length on its own. Spaces and symbols are skipped, so spacing
	// a curse out does not hide it.
	// (1) Look up each window of the string among the curses of
	//     that length.
	//------------------------------------------------------------
	//     하이 뭐라고 우헤헤헤 안돼~~
	// --> 하이뭐라고우헤헤헤안돼
	//	 Keeping each byte's index in the original string makes
	//	 replacing it there easy.
	//
	//1 syllable ( 하, 이, 뭐, 라, 고, 우, 헤, 헤, 헤, 안, 돼 )
	//2 syllables( 하이, 이뭐, 뭐라, 고우, 우헤, 헤헤, 헤헤, 헤안, 안돼 )
	//3 syllables(하이뭐, 이뭐라, 뭐라고, 라고우, 고우헤, 우헤헤, 헤헤헤, 헤헤안, 헤안돼)
	//4 syllables.... and so on
	//
	// Comparisons: O( stringLength * (log(1-syllable curses) + ... + log(n-syllable curses)) )
	//
	// e.g. 40 syllables * (log(1000)+log(1000)+log(1000)) = 40*30 = 1200
	//
	//------------------------------------------------------------
	strFilteredPtr = strFiltered;
	indexFilteredPtr = indexFiltered;
	isCursePtr = isCurse;
	
	strOrg = str;	// the cursor over the original
	
	//------------------------------------------------------------
	// Filter out the Korean (two-byte) characters.
	//------------------------------------------------------------
	i = 0;
	index = 0;
	while ((ch = *strOrg++))//, ch != '\0')
	{
		//----------------------------------------------
		// a Korean character's lead byte
		//----------------------------------------------
		if (ch & 0x80)
		{
			char chNext = *strOrg++;

			//----------------------------------------------
			// two bytes, so the next byte must be there
			//----------------------------------------------
			if (chNext=='\0')
			{
				// the string ends after the lead byte
				break;
			}
			
			//----------------------------------------------
			// a whole two-byte character
			//----------------------------------------------
			*strFilteredPtr++	= ch;
			*indexFilteredPtr++ = i++;
			*isCursePtr++		= false;	// not a curse yet

			*strFilteredPtr++	= chNext;
			*indexFilteredPtr++ = i;
			*isCursePtr++		= false;	// not a curse yet

			index+=2;
		}		
	
		//----------------------------------------------
		// anything else is skipped
		//----------------------------------------------

		i++;
	}
	*strFilteredPtr = '\0';

	//------------------------------------------------------------
	// Look for the Korean curses, one length at a time.
	//------------------------------------------------------------
	if (RemoveCurseKorean(strFiltered, 2, m_mapCurseKor1, isCurse))
	{
		existCurseKor = true;	// a curse was found
	}

	if (RemoveCurseKorean(strFiltered, 4, m_mapCurseKor2, isCurse))
	{
		existCurseKor = true;	// a curse was found
	}

	if (RemoveCurseKorean(strFiltered, 6, m_mapCurseKor3, isCurse))
	{
		existCurseKor = true;	// a curse was found
	}

	if (RemoveCurseKorean(strFiltered, 8, m_mapCurseKor4, isCurse))
	{
		existCurseKor = true;	// a curse was found
	}

	//------------------------------------------------------------
	// If there is a Korean curse,
	//------------------------------------------------------------
	if (existCurseKor)
	{
		//------------------------------------------------------------
		// replace the curses found. A replacement goes over the Korean
		// bytes from the word's first on, as far as it reaches and
		// there are Korean bytes: only the first index entries of
		// indexFiltered are set.
		//------------------------------------------------------------
		
		for (int i=0; i<index; i++)
		{
			if ( isCurse[i] )
			{
				// 2004, 10, 26, sobeit modify start - curse filter change
				
				int j = 0;
				switch(isCurse[i])
				{
				case 2:
					{
						char* pChangeString = (*g_pGameStringTable)[UI_STRING_MESSAGE_REMOVE_CURSE_1].GetString();
						for(j = 0; static_cast<size_t>(j)<(*g_pGameStringTable)[UI_STRING_MESSAGE_REMOVE_CURSE_1].GetLength() ; j++)
						{
							if( (i+j) < index )
								str[ indexFiltered[i+j] ] = pChangeString[j];
						}
					}
					i+= 1;
					break;
				case 4:
					{
						char* pChangeString = (*g_pGameStringTable)[UI_STRING_MESSAGE_REMOVE_CURSE_2].GetString();
						for(j = 0; static_cast<size_t>(j)<(*g_pGameStringTable)[UI_STRING_MESSAGE_REMOVE_CURSE_2].GetLength() ; j++)
						{
							if( (i+j) < index )
								str[ indexFiltered[i+j] ] = pChangeString[j];
						}
					}
					i+= 3;
					break;
				case 6:
					{
						char* pChangeString = (*g_pGameStringTable)[UI_STRING_MESSAGE_REMOVE_CURSE_3].GetString();
						for(j = 0; static_cast<size_t>(j)<(*g_pGameStringTable)[UI_STRING_MESSAGE_REMOVE_CURSE_3].GetLength() ; j++)
						{
							if( (i+j) < index )
								str[ indexFiltered[i+j] ] = pChangeString[j];
						}
					}
					i+= 5;
					break;
				case 8:
					{
						char* pChangeString = (*g_pGameStringTable)[UI_STRING_MESSAGE_REMOVE_CURSE_4].GetString();
						for(j = 0; static_cast<size_t>(j)<(*g_pGameStringTable)[UI_STRING_MESSAGE_REMOVE_CURSE_4].GetLength() ; j++)
						{
							if( (i+j) < index )
								str[ indexFiltered[i+j] ] = pChangeString[j];
						}
					}
					i+= 7;
					break;
				default:
					str[ indexFiltered[i] ] = MaskChar( s_MaskString, i );
					break;
				}

				// 2004, 10, 26, sobeit modify end - curse filter change
			}
		}
	}

	delete [] strFiltered;
	delete [] indexFiltered;
	delete [] isCurse;	

	//------------------------------------------------------------
	// Was there an English or a Korean curse?
	//------------------------------------------------------------
	if (existCurseEng || existCurseKor)
	{
		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// RemoveCurseKorean
//----------------------------------------------------------------------
// strKor	: 한글만 들어있는 string(공백,특수문자,영어,깨진한글.. 등은 없다!)
// lenCurse : 검색하려는 욕의 고정된 길이 byte수(한글이므로 2의 배수여야 한다)
// mapCurse : 욕들이 들어있는 MStringMap. 같은 길이의 욕들만 있다.
// isCurse	: 욕이 있는 위치의 정보
//----------------------------------------------------------------------
// --> 하이뭐라고우헤헤헤안돼
//1글자( 하, 이, 뭐, 라, 고, 우, 헤, 헤, 헤, 안, 돼 )
//2글자( 하이, 이뭐, 뭐라, 고우, 우헤, 헤헤, 헤헤, 헤안, 안돼 )
//3글자(하이뭐, 이뭐라, 뭐라고, 라고우, 고우헤, 우헤헤, 헤헤헤, 헤헤안, 헤안돼)
//----------------------------------------------------------------------
bool				
MChatManager::RemoveCurseKorean(const char* strKor, 
								int byteCurse, const MStringMap& mapCurse, 
								BYTE* isCurse) const
{
	int len = static_cast<int>(strlen(strKor));

	//---------------------------------------------------------
	// string 길이가 짧은 경우
	//---------------------------------------------------------
	if (len < byteCurse)
	{
		return false;
	}

	bool existCurse = false;

	//---------------------------------------------------------
	// 체크하면서 NULL을 찍기 때문에.. copy해서 사용한다.
	//---------------------------------------------------------
	char* strCheck = new char [len+1];
	memcpy(strCheck, strKor, static_cast<size_t>(len) + 1);
	
	char* strCheckPtr = strCheck;
	
	int maxCheck = len - (byteCurse-2);

	for (int i=0; i<maxCheck; i+=2)
	{
		//---------------------------------------------------------
		// 필요한 부분까지 체크하기 위해서 NULL을 찍는다.
		//---------------------------------------------------------
		char* strCheckNull = strCheckPtr + byteCurse;
		char previousNull = *strCheckNull;
		*strCheckNull = '\0';

		//---------------------------------------------------------
		// 선택한 단어가 욕map에 있는지 찾아본다.
		//---------------------------------------------------------
		MString tempStr(strCheckPtr);
		MStringMap::const_iterator iString = mapCurse.find( &tempStr );

		//---------------------------------------------------------
		// 욕인 경우 (정말 욕일까? - -;)
		//---------------------------------------------------------
		if (iString != mapCurse.end())
		{
			// 욕 길이만큼.. 욕이라고 체크해둔다.
			// 2004, 10, 26, sobeit modify start - 욕필터 수정
			//memset( isCurse+(strCheckPtr-strCheck), true, byteCurse);
			memset( isCurse+(strCheckPtr-strCheck), byteCurse, byteCurse);
			// 2004, 10, 26, sobeit modify end - 욕필터 수정

			existCurse = true;
		}		
	
		//---------------------------------------------------------
		// NULL로 해둔 부분을 원래대로 돌린다.
		//---------------------------------------------------------
		*strCheckNull = previousNull;

		strCheckPtr += 2;
	}

	delete [] strCheck;

	return existCurse;
}

//----------------------------------------------------------------------
// Add Mask
//----------------------------------------------------------------------
// Masks characters of str here and there, each kept with a chance of
// percent, using s_MaskString2's characters.
// percent is 0 to 100:
// 0 masks everything, 100 masks nothing.
//----------------------------------------------------------------------
void
MChatManager::AddMask(char* str, int percent) const
{
#ifdef OUTPUT_DEBUG
	percent = max( percent, 75 );
#endif

	if (percent >= 100)
	{
		return;
	}

	char ch;

	// 0 ~ 100 --> 0 ~ 63
	int pro = percent * 63 / 100;
	
	int index = rand() & 0x0F;

	while (ch = *str, ch != '\0')
	{
		int maskLen = 0;

		//-------------------------------------------------------
		// a space is kept
		//-------------------------------------------------------
		if (ch==' ')
		{			
		}
		//-------------------------------------------------------
		// a Korean (two-byte) character
		//-------------------------------------------------------
		else if (ch & 0x80)
		{
			maskLen = 2;			
		}
		//-------------------------------------------------------
		// a one-byte character
		//-------------------------------------------------------
		else 
		{
			maskLen = 1;
		}		

		//-------------------------------------------------------
		// mask it?
		//-------------------------------------------------------
		if (maskLen!=0)
		{
			int bMask = (rand() & 0x3F) >= pro;	// decide

			if (bMask)
			{
				for (int i=0; i<maskLen; i++)
				{
					if (*str != '\0')
					{			
						*str = MaskChar( s_MaskString2, index++ );
						str++;
					}					
				}
			}
			else
			{
				str += maskLen;
			}
		}
		else
		{
			str++;
		}
	}	
}
