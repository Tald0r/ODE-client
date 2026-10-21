//----------------------------------------------------------------------
// test_data_path.cpp
//----------------------------------------------------------------------
//
// Basic::ResolveDataPath and Basic::NormalizeDataPath
// (basic/DataPath.cpp). The game's data paths are written the Windows
// way - backslashes, and a letter case that need not match the disk
// (docs/build/linux-macos-port-assessment-2026-09-07.md, area F: of the 206
// entries in FileDef.inf, 38 exist only under a different case). On a
// case-sensitive filesystem those opens fail, so the resolver folds the
// separators and, when the path does not exist as spelled, finds each
// component case-insensitively. It never invents a file: a path that
// matches nothing comes back folded and otherwise unchanged, and the
// caller's open fails as loudly as it would have.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "DataPath.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

struct SScratchDirectory
{
	std::filesystem::path	Path;

	SScratchDirectory()
	{
		const long long llStamp =
			std::chrono::steady_clock::now().time_since_epoch().count();

		std::error_code Error;

		Path = std::filesystem::temp_directory_path(Error)
			/ ("datapath_" + std::to_string(llStamp));

		std::filesystem::remove_all(Path, Error);
		std::filesystem::create_directories(Path / "Data" / "Info", Error);
		std::filesystem::create_directories(Path / "Data" / "Image", Error);

		AddFile("Data/Info/FileDef.inf");
		AddFile("Data/Image/Etc.spk");
	}

	~SScratchDirectory()
	{
		std::error_code Error;
		std::filesystem::remove_all(Path, Error);
	}

	void	AddFile(const char* pName) const
	{
		std::ofstream File(Path / pName, std::ios::binary);
		File << "x";
	}

	// The scratch directory's path as the resolver spells it: the resolver
	// rewrites every component to the disk's case, and the temp directory's
	// own spelling (TMP/TEMP on Windows) need not match the disk's.
	std::string	Name() const
	{
		return Basic::ResolveDataPath(Path.generic_string());
	}
};

bool	Opens(const std::string& sPath)
{
	std::ifstream File(sPath.c_str(), std::ios::binary);
	return File.is_open();
}

} // namespace

//----------------------------------------------------------------------
// A path that exists as spelled, once the separators are folded, is
// returned folded and otherwise untouched.
//----------------------------------------------------------------------
TEST(DataPath, ResolveFoldsBackslashesOnAnExistingPath)
{
	const SScratchDirectory Scratch;

	const std::string sResolved =
		Basic::ResolveDataPath(Scratch.Name() + "\\Data\\Info\\FileDef.inf");

	CHECK(Scratch.Name() + "/Data/Info/FileDef.inf" == sResolved);
	CHECK(Opens(sResolved));
}

//----------------------------------------------------------------------
// A path whose components differ from the disk only in case resolves to
// the disk's spelling, and opens - on every filesystem, including the
// case-sensitive ones where the input would not.
//----------------------------------------------------------------------
TEST(DataPath, ResolveFindsComponentsCaseInsensitively)
{
	const SScratchDirectory Scratch;

	const std::string sResolved =
		Basic::ResolveDataPath(Scratch.Name() + "/data/IMAGE/etc.SPK");

	CHECK(Scratch.Name() + "/Data/Image/Etc.spk" == sResolved);
	CHECK(Opens(sResolved));
}

//----------------------------------------------------------------------
// A path that matches nothing is not invented: it comes back folded and
// otherwise as given, so the caller's open fails where it always did.
//----------------------------------------------------------------------
TEST(DataPath, ResolveLeavesAMissingPathAlone)
{
	const SScratchDirectory Scratch;

	const std::string sResolved =
		Basic::ResolveDataPath(Scratch.Name() + "\\Data\\Image\\Missing.spk");

	CHECK(Scratch.Name() + "/Data/Image/Missing.spk" == sResolved);
	CHECK_EQ(false, Opens(sResolved));
}

//----------------------------------------------------------------------
// A relative path resolves against the working directory the same way.
//----------------------------------------------------------------------
TEST(DataPath, ResolveHandlesARelativePath)
{
	const SScratchDirectory Scratch;

	std::error_code Error;
	const std::filesystem::path Previous = std::filesystem::current_path(Error);
	std::filesystem::current_path(Scratch.Path, Error);

	const std::string sResolved = Basic::ResolveDataPath("data\\info\\filedef.INF");

	std::filesystem::current_path(Previous, Error);

	CHECK(std::string("Data/Info/FileDef.inf") == sResolved);
}

//----------------------------------------------------------------------
// An empty path is returned empty; nothing is listed for it.
//----------------------------------------------------------------------
TEST(DataPath, ResolveOfEmptyIsEmpty)
{
	CHECK(Basic::ResolveDataPath("").empty());
}

//----------------------------------------------------------------------
// NormalizeDataPath is the entry the game calls. On Windows it is the
// identity - the Win32 file APIs take backslashes and ignore case, and
// the live platform's opens are byte-identical to what they were; off
// Windows it is ResolveDataPath.
//----------------------------------------------------------------------
TEST(DataPath, NormalizeIsIdentityOnWindowsAndResolvesElsewhere)
{
	const SScratchDirectory Scratch;

	const std::string sInput = Scratch.Name() + "\\data\\image\\ETC.spk";
	const std::string sNormalized = Basic::NormalizeDataPath(sInput);

#ifdef _WIN32
	CHECK(sInput == sNormalized);
#else
	CHECK(Scratch.Name() + "/Data/Image/Etc.spk" == sNormalized);
#endif
	CHECK(Opens(sNormalized));
}

//----------------------------------------------------------------------
// FindDataRoot: the first candidate holding the marker file, as an
// absolute, case-resolved path with no trailing separator, or empty.
// The candidates a Finder launch or a bundle produces are a working
// directory without the data ("/"), then the executable's directory,
// then the directory beside the bundle - and the data tree may spell
// its directories in any case off Windows. The marker is passed the way
// the game spells it, backslashes included.
//----------------------------------------------------------------------

namespace {

const char* const kMarker = "Data\\Info\\FileDef.inf";

} // namespace

TEST(DataPath, FindDataRootPicksTheFirstCandidateWithTheMarker)
{
	SScratchDirectory Scratch;

	std::error_code Error;
	std::filesystem::create_directories(Scratch.Path / "empty", Error);

	const std::string sEmpty = (Scratch.Path / "empty").generic_string();
	const std::string sRoot  = Scratch.Path.generic_string();

	const std::string sFound = Basic::FindDataRoot({ sEmpty, sRoot, sEmpty }, kMarker);

	CHECK(Scratch.Name() == sFound);
	CHECK(std::filesystem::path(sFound).is_absolute());
	CHECK(!sFound.empty() && sFound.back() != '/');
}

TEST(DataPath, FindDataRootIsEmptyWhenNoCandidateHasTheMarker)
{
	SScratchDirectory Scratch;

	std::error_code Error;
	std::filesystem::create_directories(Scratch.Path / "empty", Error);

	const std::string sEmpty = (Scratch.Path / "empty").generic_string();
	const std::string sMissing = (Scratch.Path / "missing").generic_string();

	CHECK("" == Basic::FindDataRoot({ sEmpty, sMissing, "" }, kMarker));
	CHECK("" == Basic::FindDataRoot({}, kMarker));
	// A file is not a directory the data could be under.
	CHECK("" == Basic::FindDataRoot({ (Scratch.Path / "Data/Info/FileDef.inf").generic_string() }, kMarker));
}

TEST(DataPath, FindDataRootSeesAMarkerSpelledInAnotherCase)
{
	// The data tree is shipped with "data" and "info" in whatever case
	// the archive gave them; the marker must be found through the same
	// case-insensitive resolution the game's opens use.
	SScratchDirectory Scratch;

	std::error_code Error;
	const std::filesystem::path Other = Scratch.Path / "other";
	std::filesystem::create_directories(Other / "data" / "info", Error);
	{
		std::ofstream File(Other / "data" / "info" / "filedef.inf", std::ios::binary);
		File << "x";
	}

	const std::string sFound = Basic::FindDataRoot({ Other.generic_string() }, kMarker);

	CHECK(Basic::ResolveDataPath(Other.generic_string()) == sFound);
}

TEST(DataPath, FindDataRootReturnsTheCandidateAsTheDiskSpellsIt)
{
	// A candidate asked for as "Beside" when the directory is "beside":
	// on a case-sensitive disk chdir() accepts only the disk's spelling,
	// so that is what comes back - with the candidate's trailing
	// separator gone, as a bundle-relative candidate carries one.
	SScratchDirectory Scratch;

	std::error_code Error;
	const std::filesystem::path Beside = Scratch.Path / "beside";
	std::filesystem::create_directories(Beside / "Data" / "Info", Error);
	{
		std::ofstream File(Beside / "Data" / "Info" / "FileDef.inf", std::ios::binary);
		File << "x";
	}

	const std::string sAsked = (Scratch.Path / "Beside").generic_string() + "/";
	const std::string sFound = Basic::FindDataRoot({ sAsked }, kMarker);

	CHECK(Basic::ResolveDataPath(Beside.generic_string()) == sFound);
	CHECK(!sFound.empty() && sFound.back() != '/');
}

TEST(DataPath, FindDataRootAcceptsARelativeCandidateAndReturnsItAbsolute)
{
	SScratchDirectory Scratch;

	// "." is the first candidate Client.cpp offers: the working
	// directory, whatever the launcher made it.
	std::error_code Error;
	const std::filesystem::path Before = std::filesystem::current_path(Error);
	std::filesystem::current_path(Scratch.Path, Error);

	const std::string sFound = Basic::FindDataRoot({ "." }, kMarker);

	std::filesystem::current_path(Before, Error);

	CHECK(std::filesystem::path(sFound).is_absolute());
	// absolute(".") ends in "/." and the normalised form of that ends
	// in "/" on libstdc++ and libc++; the caller gets one spelling.
	CHECK(!sFound.empty() && sFound.back() != '/');
	// By identity, not spelling: current_path() is the real path, and
	// on macOS the temp directory is reached through a symlink
	// (/var/folders -> /private/var/folders).
	CHECK(std::filesystem::equivalent(Scratch.Path, std::filesystem::path(sFound), Error));
}
