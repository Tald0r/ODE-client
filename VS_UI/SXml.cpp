//////////////////////////////////////////////////////////////////////////////
/// \file XML.cpp
/// \author excel96
/// \date 2003.7.25
///
/// \todo
/// \bug
/// \warning
//////////////////////////////////////////////////////////////////////////////

//#include "SFCPCH.h"
#include "Client_PCH.h"

#include "SXml.h"
#include <memory>
#include <limits>
#include <string_view>
#include "PacketAssert.h"
using namespace std;

//#pragma warning (push, 1)
//	#include <xercesc/sax/SAXParseException.hpp>
//	#include <xercesc/sax/SAXException.hpp>
//	#include <xercesc/framework/MemBufInputSource.hpp>
//	#include <stdio.h>
//	#include <stdarg.h>
//	#include <time.h>
//	#include <fstream>
//#pragma warning (pop)

//////////////////////////////////////////////////////////////////////////////
/// \brief 
/// \param str 
/// \return string 
//////////////////////////////////////////////////////////////////////////////
string XMLUtil::WideCharToString(const wchar_t* text, int length)
{
	if (!text || length < -1) return {};
	if (length == -1) {
		const size_t size = wcslen(text);
		if (size > static_cast<size_t>((std::numeric_limits<int>::max)())) return {};
		length = static_cast<int>(size);
	}
	if (length == 0) return {};
	const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
	if (bytes <= 0) return {};
	std::string output(static_cast<size_t>(bytes), '\0');
	if (WideCharToMultiByte(CP_UTF8, 0, text, length, output.data(), bytes, nullptr, nullptr) != bytes) return {};
	return output;
}

string XMLUtil::trim(const string& str)
{
	if (str.size() == 0) return "";

	static const char * WhiteSpaces = " \t\n\r";
	size_t begin = str.find_first_not_of(WhiteSpaces);
	size_t end = str.find_last_not_of(WhiteSpaces);

	if (begin == string::npos)
	{
		if (end == string::npos) return "";
		else begin = 0;
	}
	else if (end == string::npos)
	{
		end = str.size();
	}

	return str.substr(begin , end - begin + 1);
}

//////////////////////////////////////////////////////////////////////////////
/// \brief 
/// \param fmt 
/// \param ... 
//////////////////////////////////////////////////////////////////////////////
void XMLUtil::filelog(char* fmt, ...)
{
	(void)fmt;
//	std::ofstream file(XML_ERROR_FILENAME, ios::out | ios::app);
//	if (file.is_open())
//	{
//		va_list valist;
//		va_start(valist, fmt);
//		char message_buffer[30000] = {0, };
//		vsprintf(message_buffer, fmt, valist);
////		int nchars = _vsnprintf(message_buffer, 30000, fmt, valist);
////		if (nchars == -1 || nchars > 30000)
////		{
////			filelog(NULL, "filelog buffer overflow!");
////			throw ("filelog() : more buffer size needed for log");
////		}
//		va_end(valist);
//
//		time_t now = time(0);
//		char time_buffer[256] = {0, };
//		sprintf(time_buffer, "%s : ", ctime(&now));
//
//		file.write(time_buffer, (streamsize)strlen(time_buffer));
//		file.write(message_buffer, (streamsize)strlen(message_buffer));
//		file.write("\n", (streamsize)strlen("\n"));
//	}
}

//////////////////////////////////////////////////////////////////////////////
//
//	XMLAttribute
//
//////////////////////////////////////////////////////////////////////////////

XMLAttribute::XMLAttribute( IN const string &name, IN const string &value )
: m_Name( name ), m_Value( value )
{
}

XMLAttribute::~XMLAttribute()
{
}

OUT const char*
XMLAttribute::GetName() const
{
	return m_Name.c_str();
}

OUT const char*
XMLAttribute::ToString() const
{
	return m_Value.c_str();
}

OUT int
XMLAttribute::ToInt() const
{
	return atoi( m_Value.c_str() );
}

OUT DWORD
XMLAttribute::ToHex() const
{
	return strtol( m_Value.c_str(), NULL, 16 );
}

OUT bool
XMLAttribute::ToBool() const
{
	return ( m_Value == "true" ) ? true : false;
}


//////////////////////////////////////////////////////////////////////////////
//
//	XMLTree
//
//////////////////////////////////////////////////////////////////////////////

XMLTree::XMLTree()
: m_pParent( NULL )
{
}

XMLTree::XMLTree( IN const string& name )
: m_Name( name ), m_pParent( NULL )
{
}

XMLTree::~XMLTree()
{
	Release();
}

OUT const string&
XMLTree::GetName() const
{
	return m_Name;
}

void
XMLTree::SetName( IN const string& name )
{
	m_Name = name;
}

OUT const string&
XMLTree::GetText() const
{
	return m_Text;
}

void
XMLTree::SetText( IN const string& text )
{
	m_Text = text;
}

OUT const XMLTree*
XMLTree::GetParent() const
{
	return m_pParent;
}

void
XMLTree::SetParent( IN XMLTree* pParent )
{
	m_pParent = pParent;
}

void XMLTree::AddAttribute(const string& name, const string& value)
{
	if (m_AttributesMap.contains(name)) return;
	auto attribute = std::make_unique<XMLAttribute>(name, value);
	m_AttributesVector.push_back(attribute.get());
	try { m_AttributesMap.emplace(name, attribute.get()); }
	catch (...) { m_AttributesVector.pop_back(); throw; }
	attribute.release();
}

OUT const XMLAttribute *
XMLTree::GetAttribute( IN const string& name ) const
{
	if(m_AttributesMap.empty() == true)
		return NULL;

	ATTRIBUTES_MAP::const_iterator itr = m_AttributesMap.find( name );

	if( itr == m_AttributesMap.end() )
		return NULL;

	return itr->second;
}

OUT XMLTree*
XMLTree::AddChild(const string& name)
{
	auto child = std::make_unique<XMLTree>(name);
	AddChildOnlyVector(child.get());
	return child.release();
}

OUT XMLTree*
XMLTree::AddChild(XMLTree* child)
{
	if (!child || m_ChildrenMap.contains(child->GetName())) return nullptr;
	return AddChildOnlyVector(child);
}

OUT XMLTree*
XMLTree::AddChildOnlyVector(XMLTree* child)
{
	if (!child) return nullptr;
	// Roll back before adopting if map allocation fails. Keep vector growth
	// geometric for large quest lists; the map retains the first repeated name.
	m_ChildrenVector.push_back(child);
	try { m_ChildrenMap.emplace(child->GetName(), child); }
	catch (...) { m_ChildrenVector.pop_back(); throw; }
	child->SetParent(this);
	return child;
}

OUT const XMLTree*
XMLTree::GetChild( IN const string& name ) const
{
	CHILDREN_MAP::const_iterator itr = m_ChildrenMap.find( name );

	if ( itr == m_ChildrenMap.end() )
		return NULL;

	return itr->second;
}

OUT const XMLTree*
XMLTree::GetChild( IN size_t index ) const
{
	return ( ( index < m_ChildrenVector.size() ) ? m_ChildrenVector[index] : NULL );
}
// 2004, 7, 13 sobeit add start
OUT const XMLTree*
XMLTree::GetChildByAttr( IN size_t index , IN const string& name) const
{
	const XMLAttribute * TempAttr;
	int MaxChildCount = GetChildCount();
	for(int i = 0; i<MaxChildCount; i++)
	{
		TempAttr = m_ChildrenVector[i]->GetAttribute(name);
		if(NULL != TempAttr)
		{
			if(static_cast<size_t>(TempAttr->ToInt()) == index)
				return m_ChildrenVector[i];
		}
	}
	return NULL;
}
// 2004, 7, 13 sobeit add end
OUT size_t
XMLTree::GetChildCount() const
{
	return m_ChildrenVector.size();
}

void XMLTree::Release()
{
	ATTRIBUTES_VECTOR::iterator itr = m_AttributesVector.begin();
	ATTRIBUTES_VECTOR::iterator endItr = m_AttributesVector.end();

	while(itr != endItr)
	{
		delete *itr;
		itr++;
	}

	m_AttributesMap.clear();
	m_AttributesVector.clear();

	CHILDREN_VECTOR::iterator itr2 = m_ChildrenVector.begin();
	CHILDREN_VECTOR::iterator endItr2 = m_ChildrenVector.end();

	while(itr2 != endItr2)
	{
		delete *itr2;
		itr2++;
	}

	m_ChildrenMap.clear();
	m_ChildrenVector.clear();
}

namespace {
std::string EscapeXml(std::string_view text)
{
	std::string escaped;
	for (const char c : text) {
		switch (c) {
		case '&': escaped += "&amp;"; break;
		case '<': escaped += "&lt;"; break;
		case '>': escaped += "&gt;"; break;
		case '\'': escaped += "&apos;"; break;
		case '"': escaped += "&quot;"; break;
		default: escaped += c; break;
		}
	}
	return escaped;
}
}

void
XMLTree::Save( const char* pFilename )
{
	std::ofstream file( pFilename, ios::out | ios::trunc );

	if ( !file.is_open() ) return;

	file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
	
	Save(file, 0);
}

void
XMLTree::Save(std::ofstream& file, size_t indent )
{
	for ( size_t i = 0; i < indent; i++ )
		file << "\t";

	file << "<" << m_Name;

	ATTRIBUTES_VECTOR::iterator itr = m_AttributesVector.begin();
	ATTRIBUTES_VECTOR::iterator endItr = m_AttributesVector.end();

	while( itr != endItr)
	{
		file << " " << (*itr)->GetName() << "='" << EscapeXml((*itr)->ToString()) << "'";

		itr++;
	}

	if (m_ChildrenVector.empty() == true && GetText().empty() == true)
	{
		file << "/>" << endl;
	}
	else
	{
		file << ">" << EscapeXml(GetText());

		if(m_ChildrenVector.empty() == true)
		{
			file << "</" << m_Name << ">" << endl;
		}
		else
		{
			file << endl;

			CHILDREN_VECTOR::iterator itr = m_ChildrenVector.begin();
			CHILDREN_VECTOR::iterator endItr = m_ChildrenVector.end();

			while( itr != endItr)
			{
				(*itr)->Save( file, indent + 1 );

				itr++;
			}

			for ( size_t i = 0; i < indent; i++ )
				file << "\t";

			file << "</" << m_Name << ">" << endl;
		}
	}
}


XMLParser::XMLParser() = default;
XMLParser::~XMLParser() = default;
