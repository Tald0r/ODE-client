/*-----------------------------------------------------------------------------

	FileDialogListing.cpp

	See FileDialogListing.h.

	2026.09.05

-----------------------------------------------------------------------------*/

#include "FileDialogListing.h"
#include "StringReduction.h"
#include <cstring>
#include <string_view>
#include <utility>

std::vector<std::string> Basic::SplitDialogFilters(const char* type)
{
	std::vector<std::string> filters;
	if (!type) return filters;
	const std::string_view text(type);
	size_t start = 0;
	for (;;)
	{
		const size_t end = text.find(';', start);
		filters.emplace_back(text.substr(start, end == text.npos ? text.npos : end - start));
		if (end == text.npos) break;
		start = end + 1;
	}
	return filters;
}

namespace {
char PathSeparator(const std::string& path)
{
	return (path.size() >= 2 && path[1] == ':') || path.starts_with('\\') ? '\\' : '/';
}

bool IsSeparator(char ch, char separator)
{
	return ch == '/' || (separator == '\\' && ch == '\\');
}

size_t RootLength(const std::string& path, char separator)
{
	if (path.size() >= 3 && path[1] == ':' && IsSeparator(path[2], separator)) return 3;
	if (path.starts_with("\\\\"))
	{
		const bool extendedUnc = path.size() >= 8 && path.starts_with("\\\\?\\") &&
			(path[4] == 'U' || path[4] == 'u') && (path[5] == 'N' || path[5] == 'n') &&
			(path[6] == 'C' || path[6] == 'c') && IsSeparator(path[7], separator);
		const size_t serverStart = extendedUnc ? 8 : 2;
		const size_t serverEnd = path.find_first_of("\\/", serverStart);
		if (serverEnd == path.npos) return path.size();
		const size_t shareEnd = path.find_first_of("\\/", serverEnd + 1);
		return shareEnd == path.npos ? path.size() : shareEnd + 1;
	}
	return !path.empty() && IsSeparator(path.front(), separator) ? 1 : 0;
}
} // namespace

void Basic::NormalizeDialogSearchPath(std::string& path)
{
	if (path.ends_with("*.*")) path.resize(path.size() - 3);
	if (path.empty()) path = ".";
	const char separator = PathSeparator(path);
	if (!IsSeparator(path.back(), separator)) path += separator;
	path += "*.*";
}

std::string Basic::DialogDirectoryPath(std::string path)
{
	NormalizeDialogSearchPath(path);
	path.resize(path.size() - 3);
	return path;
}

void Basic::ChangeDialogSearchPath(const char* directory, std::string& path)
{
	// Copy before modifying path, since the argument can alias its storage.
	const std::string entry = directory ? directory : "";
	NormalizeDialogSearchPath(path);
	if (entry.size() < 2 || entry.front() != '\\') return;
	const std::string child = entry.substr(1);
	if (child == ".") return;
	std::string base = DialogDirectoryPath(path);
	const char separator = PathSeparator(base);
	if (child == "..")
	{
		const size_t root = RootLength(base, separator);
		while (base.size() > root && IsSeparator(base.back(), separator)) base.pop_back();
		if (base.size() > root)
		{
			const size_t parent = base.find_last_of(separator == '\\' ? "\\/" : "/");
			if (parent == base.npos) base = ".";
			else base.resize(parent < root ? root : parent);
		}
	}
	else
	{
		if (child.find_first_of(separator == '\\' ? "\\/" : "/") != child.npos) return;
		base += child;
		base += separator;
	}
	NormalizeDialogSearchPath(base);
	path = std::move(base);
}

Basic::DialogDirectories Basic::MakeDialogDirectories(DWORD driveMask, const std::string& currentPath)
{
	DialogDirectories result;
	bool matched = false;
	for (unsigned i = 0; i < 26; ++i)
	{
		if ((driveMask & (DWORD{1} << i)) == 0) continue;
		std::string path(1, static_cast<char>('a' + i));
		path += ":\\";
		if (currentPath.size() >= 2 && currentPath[1] == ':' &&
			(static_cast<unsigned int>(currentPath[0]) == 'a' + i || static_cast<unsigned int>(currentPath[0]) == 'A' + i))
		{
			path = currentPath;
			result.current = result.paths.size();
			matched = true;
		}
		if (!path.empty() && !IsSeparator(path.back(), PathSeparator(path))) path += PathSeparator(path);
		NormalizeDialogSearchPath(path);
		result.paths.push_back(std::move(path));
	}
	if (result.paths.empty() || (!matched && !currentPath.empty()))
	{
		std::string path = currentPath;
		if (!path.empty() && !IsSeparator(path.back(), PathSeparator(path))) path += PathSeparator(path);
		NormalizeDialogSearchPath(path);
		result.current = result.paths.size();
		result.paths.push_back(std::move(path));
	}
	return result;
}

std::string Basic::BuildDialogPathLabel(const std::string& path,
		const std::vector<std::string>& filters)
{
	std::string title = path;
	// Stored paths end in "*.*". Preserve the '*' before suffix filters,
	// or remove the whole search pattern when there are no filters.
	if (title.ends_with("*.*"))
		title.resize(title.size() - (filters.empty() ? 3 : 2));
	bool first = true;
	for (const auto& filter : filters)
	{
		if (!first) title += ';';
		title += filter;
		first = false;
	}
	return title;
}

std::string Basic::ShortenDialogLabel(const std::string& label)
{
	std::string name = label;
	ReduceString(name.data(), 38);
	name.resize(std::strlen(name.c_str()));
	return name;
}

namespace {

// Deliberately not toupper(), which is undefined for a negative char:
// every CP949 byte outside ASCII is negative here and must be left alone.
inline char	UpperChar(char str)
{
	if(!(str>='a'&&str<='z')) return str;
	else return str-32;
}

} // anonymous namespace

/*-----------------------------------------------------------------------------
  Suffix filter
-----------------------------------------------------------------------------*/
bool
Basic::MatchesAnySuffixCaseInsensitive(const std::string& sName,
		const std::vector<std::string>& vSuffixes)
{
	for(size_t i = 0; i < vSuffixes.size(); i++)
	{
		const std::string&	sSuffix = vSuffixes[i];

		if(sSuffix.size() > sName.size()) continue;

		bool	bMatches = true;

		for(size_t j = 0; bMatches && j < sSuffix.size(); j++)
		{
			if(UpperChar(sName[sName.size() - j - 1])
				!= UpperChar(sSuffix[sSuffix.size() - j - 1]))
				bMatches = false;
		}

		if(bMatches) return true;
	}

	return false;
}

/*-----------------------------------------------------------------------------
  Insertion sort into the dialog's parallel vectors

  A directory's position is the first entry that sorts after it or is a
  file; a file's is the first FILE that sorts after it, which is what
  keeps every directory ahead of every file even when a file name begins
  below '\' (0x5C).
-----------------------------------------------------------------------------*/
void
Basic::InsertDialogEntry(std::vector<std::string>& vNames,
		std::vector<DWORD>& vAttributes, const std::string& sName,
		DWORD dwAttributes)
{
	const bool	bIsDirectory = (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	size_t		uAt;

	for(uAt = 0; uAt < vNames.size(); uAt++)
	{
		if(bIsDirectory)
		{
			if(vNames[uAt] > sName || vNames[uAt][0] != '\\') break;
		}
		else
		{
			if(vNames[uAt] > sName && vNames[uAt][0] != '\\') break;
		}
	}

	vNames.insert(vNames.begin() + uAt, sName);
	vAttributes.insert(vAttributes.begin() + uAt, dwAttributes);
}
