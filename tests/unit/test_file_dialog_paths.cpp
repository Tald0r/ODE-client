#include "test_framework.h"
#include "FileDialogListing.h"

TEST(FileDialogPaths, ParentOfDriveRootStaysAtRoot)
{
	std::string path = "C:\\*.*";
	Basic::ChangeDialogSearchPath("\\..", path);
	Basic::NormalizeDialogSearchPath(path);
	CHECK(std::string(path) == "C:\\*.*");
}

TEST(FileDialogPaths, NullFilterMeansNoSuffixes)
{
	CHECK(Basic::SplitDialogFilters(nullptr).empty());
}

TEST(FileDialogPaths, FilterDelimitersKeepEmptySuffixes)
{
	CHECK(Basic::SplitDialogFilters(".bmp;.jpg") == std::vector<std::string>({".bmp", ".jpg"}));
	CHECK(Basic::SplitDialogFilters("") == std::vector<std::string>({""}));
	CHECK(Basic::SplitDialogFilters(";.bmp;") == std::vector<std::string>({"", ".bmp", ""}));
}

TEST(FileDialogPaths, NormalizedDriveAndChildPaths)
{
	std::string path = "C:\\";
	Basic::NormalizeDialogSearchPath(path);
	CHECK(std::string(path) == "C:\\*.*");
	Basic::ChangeDialogSearchPath("\\pictures", path);
	Basic::NormalizeDialogSearchPath(path);
	CHECK(std::string(path) == "C:\\pictures\\*.*");
	Basic::ChangeDialogSearchPath("\\..", path);
	Basic::NormalizeDialogSearchPath(path);
	CHECK(std::string(path) == "C:\\*.*");
}

TEST(FileDialogPaths, FiltersHaveNoFixedSuffixBuffer)
{
	for (const size_t length : {size_t{29}, size_t{30}, size_t{6000}})
	{
		const std::string suffix(length, 'x');
		const auto filters = Basic::SplitDialogFilters((suffix + ";;.jpg;").c_str());
		CHECK(filters == std::vector<std::string>({suffix, "", ".jpg", ""}));
	}
}

TEST(FileDialogPaths, EmptyPathsAndRepeatedNormalizationAreSafe)
{
	std::string path;
	Basic::NormalizeDialogSearchPath(path);
	CHECK(path == "./*.*");
	Basic::NormalizeDialogSearchPath(path);
	CHECK(path == "./*.*");
	CHECK(Basic::DialogDirectoryPath(path) == "./");
	path = "C:";
	Basic::NormalizeDialogSearchPath(path);
	CHECK(path == "C:\\*.*");
}

TEST(FileDialogPaths, NativeAndNetworkRootsCannotBeTraversedAbove)
{
	for (const std::string& root : {std::string("/"), std::string("\\\\server\\share\\"),
		std::string("\\\\?\\UNC\\server\\share\\"), std::string("\\\\?\\uNc\\server\\share\\"),
		std::string("\\\\?\\C:\\")})
	{
		std::string path = root;
		Basic::NormalizeDialogSearchPath(path);
		Basic::ChangeDialogSearchPath("\\..", path);
		CHECK(path == root + "*.*");
		Basic::ChangeDialogSearchPath("\\pictures", path);
		Basic::ChangeDialogSearchPath("\\..", path);
		CHECK(path == root + "*.*");
	}
}

TEST(FileDialogPaths, LongDirectoriesAndDotNamesUseOwnedStorage)
{
	std::string path = "/" + std::string(6000, 'd');
	const std::string parent = path + "/";
	Basic::NormalizeDialogSearchPath(path);
	Basic::ChangeDialogSearchPath("\\.pictures", path);
	CHECK(path == parent + ".pictures/*.*");
	Basic::ChangeDialogSearchPath("\\..", path);
	CHECK(path == parent + "*.*");
	Basic::ChangeDialogSearchPath("\\stars*.*", path);
	CHECK(path == parent + "stars*.*/*.*");
}

TEST(FileDialogPaths, InvalidDirectoryEntriesDoNotChangeTheLocation)
{
	std::string path = "C:\\pictures\\*.*";
	const char* entries[] = {nullptr, "", "file.jpg", "\\", "\\.", "\\other/escape", "\\other\\escape"};
	for (const char* entry : entries)
	{
		Basic::ChangeDialogSearchPath(entry, path);
		CHECK(path == "C:\\pictures\\*.*");
	}
}

TEST(FileDialogPaths, StartupAlwaysHasAnInitializedCurrentDirectory)
{
	const auto native = Basic::MakeDialogDirectories(0, "/home/player");
	CHECK_EQ(1, native.paths.size());
	CHECK_EQ(0, native.current);
	CHECK(native.paths[native.current] == "/home/player/*.*");
	const auto missing = Basic::MakeDialogDirectories(0, "");
	CHECK_EQ(1, missing.paths.size());
	CHECK_EQ(0, missing.current);
	CHECK(missing.paths[missing.current] == "./*.*");
	const auto drives = Basic::MakeDialogDirectories((DWORD{1} << 2) | (DWORD{1} << 25), "Z:\\pictures");
	CHECK_EQ(2, drives.paths.size());
	CHECK_EQ(1, drives.current);
	CHECK(drives.paths[drives.current] == "Z:\\pictures\\*.*");
	const auto network = Basic::MakeDialogDirectories(DWORD{1} << 2, "\\\\server\\share");
	CHECK_EQ(2, network.paths.size());
	CHECK_EQ(1, network.current);
	CHECK(network.paths[network.current] == "\\\\server\\share\\*.*");
	const auto stars = Basic::MakeDialogDirectories(0, "/home/stars*.*");
	CHECK(stars.paths[stars.current] == "/home/stars*.*/*.*");
}
