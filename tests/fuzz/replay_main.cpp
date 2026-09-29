//----------------------------------------------------------------------
// replay_main.cpp
//----------------------------------------------------------------------
//
// A main() for a fuzz target without libFuzzer: runs every input named
// on the command line through LLVMFuzzerTestOneInput once, so a seed
// corpus and the committed regressions run as an ordinary ctest on
// every compiler, sanitizer or not.
//
//   fuzz_replay_<target> <file-or-directory>...
//
// A directory contributes its regular files (not recursively, in name
// order, skipping names that start with '.'). A file whose name ends in
// .hex holds the input as hex digits, the tests/golden style (one line,
// whitespace ignored); any other file is the input's raw bytes.
//
// Exit status: 0 when every input ran; 1 when a path does not exist, a
// file cannot be read, a .hex file is malformed, or there was no input
// at all, so a test pointed at an empty or misspelled directory fails
// instead of passing over nothing. A crash in the target ends the
// process like any crash.
//
//----------------------------------------------------------------------

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

// The Windows headers exactly as basic/Platform.h and the socket tests
// include them, last, so their min/max macros reach no standard header.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock.h>
#endif

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv);
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace {

namespace fs = std::filesystem;

int	HexDigit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

bool	DecodeHex(const std::string& text, std::vector<std::uint8_t>& out)
{
	out.clear();
	int high = -1;
	for (char c : text)
	{
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
			continue;
		const int digit = HexDigit(c);
		if (digit < 0)
			return false;
		if (high < 0)
		{
			high = digit;
		}
		else
		{
			out.push_back((std::uint8_t)((high << 4) | digit));
			high = -1;
		}
	}
	return high < 0;
}

bool	ReadInput(const fs::path& path, std::vector<std::uint8_t>& out)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		std::fprintf(stderr, "replay: cannot read %s\n", path.string().c_str());
		return false;
	}
	const std::string bytes((std::istreambuf_iterator<char>(file)),
				std::istreambuf_iterator<char>());

	if (path.extension() == ".hex")
	{
		if (!DecodeHex(bytes, out))
		{
			std::fprintf(stderr, "replay: %s is not an even run of hex digits\n",
				path.string().c_str());
			return false;
		}
		return true;
	}

	out.assign(bytes.begin(), bytes.end());
	return true;
}

// Expands the command line into the list of input files, in order.
bool	CollectInputs(int argc, char** argv, std::vector<fs::path>& inputs)
{
	for (int i = 1; i < argc; i++)
	{
		const fs::path arg(argv[i]);
		std::error_code error;

		if (fs::is_directory(arg, error))
		{
			std::vector<fs::path> files;
			for (const fs::directory_entry& entry : fs::directory_iterator(arg, error))
			{
				const std::string name = entry.path().filename().string();
				if (name.empty() || name[0] == '.')
					continue;
				std::error_code typeError;
				if (entry.is_regular_file(typeError))
					files.push_back(entry.path());
			}
			if (error)
			{
				std::fprintf(stderr, "replay: cannot list %s\n", argv[i]);
				return false;
			}
			std::sort(files.begin(), files.end());
			inputs.insert(inputs.end(), files.begin(), files.end());
		}
		else if (fs::is_regular_file(arg, error))
		{
			inputs.push_back(arg);
		}
		else
		{
			std::fprintf(stderr, "replay: no such file or directory: %s\n", argv[i]);
			return false;
		}
	}
	return true;
}

} // namespace

int main(int argc, char** argv)
{
#ifdef _WIN32
	// The socket layer's objects are built without a connection, but
	// Winsock must be started before any of them is destroyed.
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::fprintf(stderr, "replay: WSAStartup failed\n");
		return 1;
	}
#endif

	std::vector<fs::path> inputs;
	if (!CollectInputs(argc, argv, inputs))
		return 1;
	if (inputs.empty())
	{
		std::fprintf(stderr, "replay: no inputs (usage: %s <file-or-directory>...)\n",
			argc > 0 ? argv[0] : "replay");
		return 1;
	}

	LLVMFuzzerInitialize(&argc, &argv);

	std::vector<std::uint8_t> data;
	for (const fs::path& path : inputs)
	{
		if (!ReadInput(path, data))
			return 1;
		// Copied into an allocation of exactly the input's size, as
		// libFuzzer does, so a read past the end is a sanitizer report.
		std::unique_ptr<std::uint8_t[]> exact(new std::uint8_t[data.size()]);
		if (!data.empty())
			std::memcpy(exact.get(), data.data(), data.size());
		LLVMFuzzerTestOneInput(exact.get(), data.size());
	}

	std::printf("replayed %u inputs\n", (unsigned int)inputs.size());

#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}
