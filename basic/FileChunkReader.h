/*-----------------------------------------------------------------------------

	FileChunkReader.h

	A file read a chunk at a time, with a count of the bytes still to
	read. SendFileInfo (Client/RequestFileManager.h) sends a peer the file
	it asked for through one: a chunk per turn of the send loop, and the
	bytes the socket did not take handed back to be read again. Here
	rather than in the executable so a test binary can link it, for the
	reason DebugLog and SafeFormat are: basic is the one library every
	target already links.

-----------------------------------------------------------------------------*/

#ifndef __FILE_CHUNK_READER_H__
#define __FILE_CHUNK_READER_H__

#include "Platform.h"

#include <fstream>
#include <string>

namespace Basic {

class FileChunkReader
{
	public :
		// Opens path for binary reading; the bytes left are its size, or
		// none when it does not open or its size cannot be read, and then
		// it is left closed.
		void		Open(const std::string& path);

		// Reads up to count bytes into pBuffer and returns how many it read.
		DWORD		Read(char* pBuffer, DWORD count);

		// Hands back the last nBack bytes read, to be read again.
		void		Unread(DWORD nBack);

		void		Close();

		bool		IsOpen() const			{ return m_Stream.is_open(); }
		DWORD		GetBytesLeft() const	{ return m_BytesLeft; }

	private :
		std::ifstream	m_Stream;
		DWORD			m_BytesLeft = 0;
};

}

#endif
