/*-----------------------------------------------------------------------------

	DataPath.h

	The game's data paths, as the code and the data tables spell them,
	resolved to what the disk has.

	The paths are Windows paths: backslashes (Data\Image\Etc.spk, in
	FileDef.inf and in about 20 source files), and a letter case that need not
	match the file - of the 206 entries in FileDef.inf, 38 exist only under
	a different case in the shipped tree. Win32 accepts both; a
	case-sensitive filesystem accepts neither, and the open fails.

	ResolveDataPath folds the separators and then walks the path one
	component at a time: a component that names an entry exactly is kept,
	one that matches an entry only case-insensitively becomes that entry's
	spelling, and one that matches nothing stops the walk - the rest is
	appended as given, so the caller's open fails where it always did, and
	no file is ever invented. Directory listings are cached; the data tree
	does not change while the game runs.

	NormalizeDataPath is what the game calls at its open sites. It is the
	identity on Windows, where the live platform's opens stay
	byte-identical, and ResolveDataPath elsewhere.

	docs/build/linux-macos-port-assessment-2026-09-07.md, area F.

-----------------------------------------------------------------------------*/

#ifndef __DATAPATH_H__
#define __DATAPATH_H__

#include <string>
#include <string_view>
#include <vector>

namespace Basic {

// Separator folding and case-insensitive component resolution, on every
// platform (tests exercise it on Windows too).
std::string	ResolveDataPath(std::string_view sPath);

// The game's entry: identity on Windows, ResolveDataPath elsewhere.
std::string	NormalizeDataPath(std::string_view sPath);

// The directory the game should run in: the first candidate under which
// the marker file (Data\Info\FileDef.inf, as the game spells it - the
// caller passes it, so this header need not know) exists, resolved as
// above; returned absolute, case-resolved and without a trailing
// separator, so the caller can chdir to it and record it. Empty when no
// candidate qualifies; a candidate that cannot be made absolute is
// skipped, not fatal. The game reads its data relative to the working
// directory; on Windows that is the executable's own directory, and off
// Windows the launcher decides it - a shell gives the directory it was
// in, Finder gives "/", and an application bundle keeps the executable
// three directories below anything a user would put the data beside.
// Client.cpp offers the working directory, the executable's directory
// and the directory beside the bundle, in that order, and changes to the
// first that has the data (docs/build/linux-macos-port-assessment-2026-09-07.md,
// areas F and G).
std::string	FindDataRoot(const std::vector<std::string>& vCandidates, std::string_view sMarker);

} // namespace Basic

#endif // __DATAPATH_H__
