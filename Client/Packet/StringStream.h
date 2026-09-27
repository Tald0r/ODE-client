//////////////////////////////////////////////////////////////////////
//
// Filename    : StringStream.h
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

#ifndef __STRING_STREAM_H__
#define __STRING_STREAM_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

// include files
#include "Types.h"
#include <list>
#include <string>

// end of stream
const char eos = '\n';

class StringStream {
	
public :
	
	// constructor
	StringStream ();
	
	// destructor
	~StringStream ();
	
	
public :
	
	// add string to stream
	StringStream & operator << ( bool T );
	StringStream & operator << ( char T );
	StringStream & operator << ( uchar T );
	StringStream & operator << ( short T );
	StringStream & operator << ( ushort T );
	StringStream & operator << ( int T );
	StringStream & operator << ( uint T );
	StringStream & operator << ( long T );
	StringStream & operator << ( ulong T );
	// SOCKET is `UINT_PTR` (64-bit) on Windows x64 - without this overload,
	// streaming a SOCKET is ambiguous between the uint/ulong overloads
	// above (both an equally-ranked narrowing conversion), error C2593.
	StringStream & operator << ( ulonglong T );
	StringStream & operator << ( float T );
	StringStream & operator << ( double T );
	StringStream & operator << ( const char * str );
	StringStream & operator << ( const std::string & str );

	// make string
	std::string toString () const;
	
	// true if stream is empty
	bool isEmpty () const noexcept { return m_Size == 0; }

private :
	
	// list of string
	std::list<std::string> m_Strings;

	// size of string which will be generated. size_t, not ushort: the
	// accumulator wrapped at 64 KiB, making isEmpty() report an empty
	// stream over real content and toString()'s reserve() under-reserve.
	size_t m_Size;
	
	// inserted flag 
	mutable bool m_bInserted;
	
	// buffer for string will be generated
	mutable std::string m_Buffer;

};

#endif
