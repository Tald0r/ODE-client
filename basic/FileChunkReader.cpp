/*-----------------------------------------------------------------------------

	FileChunkReader.cpp

-----------------------------------------------------------------------------*/

#include "FileChunkReader.h"

namespace Basic {

void
FileChunkReader::Open(const std::string& path)
{
	m_Stream.open( path.c_str(), std::ios::in | std::ios::binary );

	if (m_Stream.is_open())
	{
		m_Stream.seekg( 0, std::ios::end );

		m_BytesLeft = static_cast<DWORD>(m_Stream.tellg());

		m_Stream.seekg( 0, std::ios::beg );
	}
	else
	{
		m_BytesLeft = 0;
	}
}

DWORD
FileChunkReader::Read(char* pBuffer, DWORD count)
{
	m_Stream.read(pBuffer, count);

	DWORD nRead = static_cast<DWORD>(m_Stream.gcount());

	m_BytesLeft -= nRead;

	return nRead;
}

void
FileChunkReader::Unread(DWORD nBack)
{
	// A short read - the file's last chunk - leaves the stream failed, and
	// a failed stream ignores seekg; the bytes to hand back were read, so
	// the stream is good again. The offset is negated as a signed type:
	// negating the unsigned count gave 2^32 - nBack, a seek past the end.
	m_Stream.clear();
	m_Stream.seekg( -static_cast<std::streamoff>(nBack), std::ios::cur );
	m_BytesLeft += nBack;
}

void
FileChunkReader::Close()
{
	m_Stream.close();
}

}
