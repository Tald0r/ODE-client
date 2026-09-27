//--------------------------------------------------------------------------
// MString.h
//--------------------------------------------------------------------------

#ifndef	__MSTRING_H__
#define	__MSTRING_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#define	MAX_BUFFER_LENGTH		1024

#include <string.h>
#include <fstream>

#include "SafeFormat.h"

class MString {
	public :
		MString();
		MString(const MString& str);
		MString(const char* str);
		virtual ~MString();

		//----------------------------------------------------
		// Init / Release
		//----------------------------------------------------
		void	Init(int len);
		void	Release();

		//----------------------------------------------------
		// assign
		//----------------------------------------------------
		void	operator = (const MString& str);
		void	operator = (const char* str);
		template <typename ...Args>
		void Format(const char* format, Args... args)
		{
			char buffer[MAX_BUFFER_LENGTH];
			SafeFormat::Format(buffer, format, args...);
			*this = buffer;
		}

		// Checked formatting with readable storage even for an empty result.
		template <typename ...Args>
		void	FormatChecked(const char* format, Args... args)
		{
			char	Buffer[MAX_BUFFER_LENGTH];

			SafeFormat::Format(Buffer, sizeof(Buffer), format, args...);

			*this = Buffer;

			// operator=(const char*) keeps no allocation for an empty
			// string, so GetString() comes back NULL - and the call
			// sites pass GetString() straight to code that reads it
			// (g_pSystemMessage->Add). An empty result is reachable
			// whenever the entry is missing, since GetGameString
			// answers "" for one, so a formatter that cannot be trusted
			// to return a readable string would not be worth much.
			if (GetString() == NULL)
			{
				Init(0);
			}
		}

		//----------------------------------------------------
		// get
		//----------------------------------------------------
		size_t		GetLength()	const		{ return m_Length; }
		char*		GetString() const		{ return m_pString; }
		operator const char*() const		{ return m_pString; }
		operator	char*() const		{ return m_pString; }		

		//----------------------------------------------------
		// other operator
		//----------------------------------------------------
		bool		operator == (const char* str)		{ return strcmp(m_pString, str)==0; }
		bool		operator == (const MString& str)	{ return strcmp(m_pString, str.m_pString)==0; }
		bool		operator >	(const char* str)		{ return strcmp(m_pString, str)>0; }
		bool		operator >	(const MString& str)	{ return strcmp(m_pString, str.m_pString)>0; }
		bool		operator <	(const char* str)		{ return strcmp(m_pString, str)<0; }
		bool		operator <	(const MString& str)	{ return strcmp(m_pString, str.m_pString)<0; }
		bool		operator != (const char* str)		{ return strcmp(m_pString, str)!=0; }
		bool		operator != (const MString& str)	{ return strcmp(m_pString, str.m_pString)!=0; }

		//----------------------------------------------------
		// File I/O: resource encoding on disk, UTF-8 in memory.
		// Malformed loaded bytes become U+FFFD. Saves reject text that cannot
		// be represented in the declared resource encoding.
		//----------------------------------------------------
		virtual void	SaveToFile(std::ofstream& file);
		virtual void	LoadFromFile(std::ifstream& file);

	protected :
		size_t	m_Length;
		char*	m_pString;
};


#endif

