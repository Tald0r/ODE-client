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
	m_Stream.seekg( -nBack, std::ios::cur );
	m_BytesLeft += nBack;
}

void
FileChunkReader::Close()
{
	m_Stream.close();
}

}
