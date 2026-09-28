cmake_minimum_required(VERSION 3.20)

# Basic library - Platform abstraction and utilities
# This is the lowest level library that all others depend on

project(basic)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection and library selection
if(WIN32)
	option(USE_SDL_BACKEND "Use SDL backend on Windows" OFF)

	if(USE_SDL_BACKEND)
		message(STATUS "Basic: Using SDL backend on Windows")
		# SDL2 should be found in parent scope
		if(NOT TARGET SDL2::SDL2)
			find_package(SDL2 REQUIRED)
		endif()
	else()
		message(STATUS "Basic: Using native Windows backend")
	endif()
else()
	# Force SDL on non-Windows platforms
	# SDL2 should be found in parent scope
	if(NOT TARGET SDL2::SDL2)
		find_package(SDL2 REQUIRED)
	endif()
	message(STATUS "Basic: Using SDL backend (required on this platform)")
endif()

# Basic library sources
set(BASIC_SOURCES
	# Directory enumeration on std::filesystem
	# (docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md, priority
	# 6). Here for the same reason DebugLog and SafeFormat are: the
	# callers being migrated off _findfirst are in the executable, and
	# basic is the one library every target already links.
	DirectoryListing.cpp
	# The game's Windows-spelled data paths resolved to the disk - separators
	# folded, components matched case-insensitively - for the port (area F of
	# docs/linux-macos-port-assessment-2026-09-07.md). Here because the open
	# sites are in the executable, VS_UI and packetwire alike.
	DataPath.cpp
	DisplaySettings.cpp
	# The file dialog's list building (docs/RESTRUCTURING.md task 3.1,
	# "moved, then fixed"). Its caller is
	# C_VS_UI_FILE_DIALOG::RefreshFileList in VS_UI, which no test binary
	# links, so the two functions live here for the reason DebugLog and
	# SafeFormat do: basic is the one library every target already links.
	FileDialogListing.cpp
	# The chunked file read behind SendFileInfo, which sends a peer the
	# profile file it asked for (docs/RESTRUCTURING.md task 3.1, "moved,
	# then fixed"). RequestFileManager is in the executable, which no test
	# binary links.
	FileChunkReader.cpp
	BasicData.cpp
	Timer2.cpp
	BasicException.cpp
	ColorDraw.cpp
	# The logging facility (docs/RESTRUCTURING.md task 5.1). It lived in
	# Client/ and was a gamemodel member, which put it above packetwire
	# in the link order and out of reach of the wire layer's include
	# rules. It needs nothing but the C library and Platform.h, so the
	# bottom library is where it belongs: every target links basic, and
	# this directory is a PUBLIC include directory, so #include
	# "DebugLog.h" resolves everywhere it did before.
	DebugLog.cpp
	# The checked formatter (docs/RESTRUCTURING.md task 5.4). It is here
	# for the same reason DebugLog is: the call sites that need it are in
	# the executable, in VS_UI and in the packet handlers, and basic is
	# the one library all of them already link. It needs nothing but the
	# C library.
	SafeFormat.cpp
	StringReduction.cpp
	TextWrap.cpp
	HelpMarkup.cpp
	DescriptorText.cpp
	HelpLayout.cpp
	MailTemplate.cpp
	MasterCommands.cpp
	TextEncoding.cpp
	ResourceText.cpp
	RarArchive.cpp
	MemoryPool.cpp
	CMessageArray.cpp
	ConfigFile.cpp
	# The monotonic clock adapter (docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md,
	# modernization backlog priority 5). Timer2.cpp is its first caller
	# and lives here already; the UI timers that follow are in VS_UI and
	# in the executable, both of which link basic.
	MonotonicClock.cpp
)

# Platform-specific sources
# Windows native implementation removed (SDL2 migration) - Always use SDL backend
list(APPEND BASIC_SOURCES PlatformSDL.cpp)
message(STATUS "Basic: Compiling PlatformSDL.cpp")

# Create static library
add_library(basic STATIC ${BASIC_SOURCES})
target_link_libraries(basic PUBLIC Iconv::Iconv)
target_link_libraries(basic PRIVATE darkeden_unrar)

# Platform.h is public, so its backend selection must reach every consumer.
if(USE_SDL_BACKEND OR NOT WIN32)
	target_compile_definitions(basic PUBLIC PLATFORM_USE_SDL)
endif()

# Public include directories
target_include_directories(basic
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
)

# Link SDL2 if using SDL backend
if(USE_SDL_BACKEND OR NOT WIN32)
	if(TARGET SDL2::SDL2)
		target_link_libraries(basic PUBLIC SDL2::SDL2)
	else()
		target_link_libraries(basic PUBLIC ${SDL2_LIBRARIES})
		if(SDL2_INCLUDE_DIRS)
			target_include_directories(basic PUBLIC ${SDL2_INCLUDE_DIRS})
		endif()
	endif()
endif()

# Platform-specific link libraries
if(WIN32 AND NOT USE_SDL_BACKEND)
	# Windows native libraries
	target_link_libraries(basic PUBLIC
		winmm
	)

endif()

# This library deliberately carries MSVC pragmas in its portable sources.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(basic PRIVATE
		-Wno-unknown-pragmas
	)
endif()

# Installation
install(TARGETS basic
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)

install(FILES
	Platform.h
	Typedef.h
	PlatformUtil.h
	DirectoryListing.h
	FileDialogListing.h
	BasicMemory.h
	BasicException.h
	DLL.h
	timer2.h
	ColorDraw.h
	InputCodes.h
	AudioTypes.h
	SafeFormat.h
	TextUtf8.h
	TextEncoding.h
	ResourceText.h
	RarArchive.h
	StringReduction.h
	MemoryPool.h
	ScrollRange.h
	MonotonicClock.h
	DESTINATION include/basic
)
