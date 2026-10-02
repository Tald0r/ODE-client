#include "Client_PCH.h"
#include "ServerInfoFileParser.h"
#include <iostream>
#include <fstream>

using namespace std;
ServerInfoFileParser * g_pConfigForeign = NULL;


ServerInfoFileParser::ServerInfoFileParser(std::string infofilename)
{
	m_FileName = infofilename;
}

ServerInfoFileParser::~ServerInfoFileParser()
{
}

std::string			ServerInfoFileParser::getProperty(int dimension, std::string key)
{
	std::ifstream file( m_FileName.c_str() );

	bool bStart = false;
	int dim=0;
	
	std::string ukey = key;
	ukey += ":";
		
	// A failed open or read need not set eofbit. Read complete lines and
	// stop on any stream failure, including files removed between lookups.
	std::string line;
	while( std::getline(file, line) )
	{
		// Windows text streams consume CRLF; normalize it on other hosts too.
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		std::erase(line, ' ');

		if( line.empty() || line.c_str()[0] == '#')
			continue;

		if( line.c_str()[0] == '@' )
		{
			bStart = !bStart;
			if( !bStart )
				dim++;				
		}
		
		if( dim == dimension && bStart )
		{
			int pos = static_cast<int>(line.find( ukey ));
			if( pos == 0 )
			{
				pos = static_cast<int>(line.find(":"));
				if( pos == -1 )
					continue;
				
				string ret = line.c_str()+pos+1;
				file.close();
				return  ret;
			}
		}
	}
	
	file.close();
	//MessageBox( NULL, key.c_str(), "Cannot find key",MB_OK);
	return "";
}

int					ServerInfoFileParser::getPropertyInt( int dimension, std::string key)
{
	string re = getProperty( dimension, key );
	if (re.empty() ) return -1;
	
	return atoi( re.c_str() );
}