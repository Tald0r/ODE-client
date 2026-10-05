cmake_minimum_required(VERSION 3.21)

project(DarkEdenClient VERSION 1.0.0)

# Dependency discovery on macOS. Homebrew installs under /opt/homebrew on
# Apple Silicon and /usr/local on Intel; CMake searches the second by
# default and the first only through PATH (every <dir>/bin on PATH makes
# <dir> a search prefix) or CMAKE_PREFIX_PATH. `brew shellenv` exports
# HOMEBREW_PREFIX, so a shell that can run brew can find its packages
# here without the preset. Every SDL2 package - Homebrew, vcpkg, a
# source install - ships a CMake config that puts its own include
# directory on the SDL2::SDL2 target, which is why the older global
# `-I/opt/homebrew/include/SDL2` that used to sit above project() is
# gone: it pre-empted whatever find_package() chose, and on a machine
# with both Homebrew and vcpkg the headers and the library disagreed.
if(APPLE AND DEFINED ENV{HOMEBREW_PREFIX} AND IS_DIRECTORY "$ENV{HOMEBREW_PREFIX}")
	list(APPEND CMAKE_PREFIX_PATH "$ENV{HOMEBREW_PREFIX}")
endif()

# C++ standard. EXTENSIONS OFF asks GCC and Clang for -std=c++20 rather
# than gnu++20; MSVC has no equivalent switch here, so its conformance
# mode is set with the other MSVC options below.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Note: We do NOT define _DEBUG here because it conflicts with DebugInfo.h
# Instead, rely on the new DebugLog.h system which is always available
# Debug builds will have console output enabled via log_init() in GameInit.cpp

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sanitizer configuration. The build wiring lives below the output directory
# settings, because the MSVC branch copies the sanitizer runtime beside the
# executables and needs to know where those land.
option(USE_ASAN "Enable AddressSanitizer for memory error detection" OFF)
option(USE_TSAN "Enable ThreadSanitizer for race condition detection" OFF)
option(USE_UBSAN "Enable UndefinedBehaviorSanitizer for undefined behavior detection" OFF)

# SDL backend is now mandatory on all platforms (Windows native support removed)
set(USE_SDL_BACKEND ON CACHE BOOL "Use SDL backend instead of native Windows APIs" FORCE)

option(BUILD_ENGINE "Build new SDL-based engine" ON)
if(EMSCRIPTEN)
	set(_build_tools_default OFF)
else()
	set(_build_tools_default ON)
endif()
option(BUILD_RESOURCE_TOOLS "Build native resource validators and viewers" ${_build_tools_default})
option(BUILD_TESTS "Build unit tests for the static libraries" OFF)
option(REQUIRE_TEST_TOOLS "Require the complete CTest suite and its Bash/Perl tools" OFF)
if(REQUIRE_TEST_TOOLS AND NOT BUILD_TESTS)
    message(FATAL_ERROR "REQUIRE_TEST_TOOLS requires BUILD_TESTS=ON")
endif()
option(DARKEDEN_MACOS_BUNDLE "macOS: build DarkEden as an application bundle (bin/DarkEden.app) instead of a bare executable" OFF)

# Platform macros, decided once.
#
# basic/Platform.h reads the platform off the compiler builtins and
# defines PLATFORM_WINDOWS, PLATFORM_LINUX, PLATFORM_MACOS and
# PLATFORM_POSIX; the build adds nothing to that on Linux or macOS. It
# used to add PLATFORM_MACOS to every non-Windows target, in six
# places, which told a Linux build it was macOS and left the Linux
# spelling the socket layer tested for (__LINUX__) defined by nothing
# (docs/linux-macos-port-assessment-2026-09-07.md, area A).
#
# On Windows the wire layer still wants __WIN32__ and __WINDOWS__ - the
# original VC6 project passed both via /D, and Client/Packet branches
# on them with bare `#if __WINDOWS__` (see the DarkEden target for the
# history). packetwire publishes one list through its link interface,
# so gamemodel, VS_UI, the executable and ordinary test consumers inherit
# identical header definitions. The viewer tools compile a few Client/ files without it, as
# they always did; they link neither packetwire nor gamemodel.
set(DARKEDEN_PLATFORM_DEFINITIONS "")
if(WIN32)
	set(DARKEDEN_PLATFORM_DEFINITIONS __WIN32__ __WINDOWS__)
endif()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(USE_ASAN OR USE_TSAN OR USE_UBSAN)
    # MSVC and clang-cl. Checked before the GNU|Clang branch because CMake
    # reports clang-cl as "Clang" while it takes MSVC-style flags.
    if(MSVC)
        if(USE_TSAN OR USE_UBSAN)
            message(FATAL_ERROR
                "This toolchain provides AddressSanitizer only. "
                "USE_TSAN and USE_UBSAN need GCC or Clang.")
        endif()

        # cl.exe accepts /fsanitize=address whether or not the sanitizer is
        # installed, and then fails at link time with an LNK1104 naming a
        # clang_rt file, which does not say what to install. Fail here instead,
        # with instructions. The runtime ships beside the compiler.
        get_filename_component(MSVC_TOOL_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        find_file(ASAN_RUNTIME_DLL
            NAMES clang_rt.asan_dynamic-x86_64.dll clang_rt.asan_dynamic-i386.dll
            PATHS "${MSVC_TOOL_DIR}"
            NO_DEFAULT_PATH)

        if(NOT ASAN_RUNTIME_DLL)
            message(FATAL_ERROR
                "AddressSanitizer is not installed for this toolchain.\n"
                "Visual Studio Installer -> Modify -> Individual components -> "
                "\"C++ AddressSanitizer\".\n"
                "Looked for clang_rt.asan_dynamic-*.dll in ${MSVC_TOOL_DIR}")
        endif()

        add_compile_options(/fsanitize=address)

        # /RTC1 is rejected outright next to /fsanitize=address and CMake puts it
        # in the default Debug flags, so it has to come back out. Strip it from
        # every configuration rather than from Debug alone: the multi-config
        # generators pick the configuration at build time, not here. /ZI (edit
        # and continue) is unsupported too, so demote it to plain /Zi.
        foreach(_asan_cfg "" "_DEBUG" "_RELEASE" "_RELWITHDEBINFO" "_MINSIZEREL")
            foreach(_asan_lang C CXX)
                set(_asan_var "CMAKE_${_asan_lang}_FLAGS${_asan_cfg}")
                string(REPLACE "/RTC1"  "" ${_asan_var} "${${_asan_var}}")
                string(REPLACE "/RTCsu" "" ${_asan_var} "${${_asan_var}}")
                string(REPLACE "/ZI" "/Zi" ${_asan_var} "${${_asan_var}}")
            endforeach()
        endforeach()

        # Incremental linking is unsupported under the sanitizer. Remove what is
        # there before appending, so the flag cannot appear twice.
        foreach(_asan_cfg "" "_DEBUG" "_RELEASE" "_RELWITHDEBINFO" "_MINSIZEREL")
            foreach(_asan_kind EXE SHARED MODULE)
                set(_asan_var "CMAKE_${_asan_kind}_LINKER_FLAGS${_asan_cfg}")
                string(REPLACE "/INCREMENTAL:NO" "" ${_asan_var} "${${_asan_var}}")
                string(REPLACE "/INCREMENTAL"    "" ${_asan_var} "${${_asan_var}}")
                set(${_asan_var} "${${_asan_var}} /INCREMENTAL:NO")
            endforeach()
        endforeach()

        # The sanitizer runtime is a DLL that must be findable at run time.
        # Visual Studio puts the compiler directory on PATH when it launches the
        # debugger, so F5 finds it; running the executable from a shell or from
        # Explorer does not. Keep a copy beside the executables.
        set(_asan_out_dirs "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        foreach(_asan_cfg ${CMAKE_CONFIGURATION_TYPES})
            list(APPEND _asan_out_dirs "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_asan_cfg}")
        endforeach()
        foreach(_asan_dir ${_asan_out_dirs})
            file(COPY "${ASAN_RUNTIME_DLL}" DESTINATION "${_asan_dir}")
        endforeach()

        message(STATUS "AddressSanitizer enabled (/fsanitize=address)")
        message(STATUS "  runtime: ${ASAN_RUNTIME_DLL}")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(SANITIZER_FLAGS "")

        if(USE_ASAN)
            set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        endif()

        if(USE_TSAN)
            set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=thread")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
        endif()

        if(USE_UBSAN)
            set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=undefined")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
        endif()

        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS}")

        message(STATUS "Sanitizer flags: ${SANITIZER_FLAGS}")
    else()
        message(WARNING "USE_ASAN/USE_TSAN/USE_UBSAN are not wired up for ${CMAKE_CXX_COMPILER_ID}; building without them.")
    endif()
endif()

# libFuzzer targets (tests/fuzz/CMakeLists.txt). Off by default and in
# every preset but linux-fuzz and macos-fuzz; the replay build of the
# same targets is part of every native test tree and needs none of this.
#
# -fsanitize=fuzzer-no-link instruments every target for coverage, so
# the libraries a fuzz target links report the edges it reaches. The
# instrumentation's callbacks resolve out of the sanitizer runtime,
# which is why AddressSanitizer is required: without it, every other
# executable in the tree would fail to link. libFuzzer's own runtime
# (its main) is linked into the fuzz targets alone - from
# DARKEDEN_LIBFUZZER_ARCHIVE when set, for a compiler that ships none,
# as Apple Clang does not; otherwise through -fsanitize=fuzzer.
option(BUILD_FUZZERS "Build the libFuzzer packet-read targets (Clang, with USE_ASAN)" OFF)
set(DARKEDEN_LIBFUZZER_ARCHIVE "" CACHE FILEPATH
    "libFuzzer runtime archive for the fuzz targets (libclang_rt.fuzzer_*.a); empty links -fsanitize=fuzzer")
if(BUILD_FUZZERS)
    if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
        message(FATAL_ERROR
            "BUILD_FUZZERS needs Clang or Apple Clang with the GNU-style driver "
            "(this is ${CMAKE_CXX_COMPILER_ID}). The replay targets build "
            "without it on every compiler.")
    endif()
    if(NOT BUILD_TESTS)
        message(FATAL_ERROR "BUILD_FUZZERS requires BUILD_TESTS=ON: the targets live under tests/fuzz")
    endif()
    if(NOT USE_ASAN)
        message(FATAL_ERROR "BUILD_FUZZERS requires USE_ASAN=ON: the coverage callbacks resolve out of its runtime")
    endif()
    if(DARKEDEN_LIBFUZZER_ARCHIVE AND NOT EXISTS "${DARKEDEN_LIBFUZZER_ARCHIVE}")
        message(FATAL_ERROR "DARKEDEN_LIBFUZZER_ARCHIVE does not exist: ${DARKEDEN_LIBFUZZER_ARCHIVE}")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=fuzzer-no-link")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=fuzzer-no-link")
    if(DARKEDEN_LIBFUZZER_ARCHIVE)
        message(STATUS "Fuzz targets: -fsanitize=fuzzer-no-link, libFuzzer from ${DARKEDEN_LIBFUZZER_ARCHIVE}")
    else()
        message(STATUS "Fuzz targets: -fsanitize=fuzzer-no-link, libFuzzer through -fsanitize=fuzzer")
    endif()
endif()

# Runtime checks for the ordinary Debug build.
#
# /RTC1 is /RTCs plus /RTCu: it reports a local read before it was written, and
# damage to a stack frame around a call. It is what turned an uninitialised bool
# in C_VS_UI_INFO::MouseControl into a diagnostic instead of a branch on garbage.
#
# It has been in this build only because CMake happens to put it in the built-in
# CMAKE_<LANG>_FLAGS_DEBUG. Nothing here asked for it, so anyone overriding those
# flags - a toolchain file, a preset, a CI script - would drop it without a word.
# Ask for it explicitly, and only add it if it is not already there, so the
# default stays authoritative when it is present.
#
# Debug only: /RTC is rejected alongside any optimisation, so there is no
# meaningful RelWithDebInfo variant of this.
#
# Skipped under USE_ASAN because the two are mutually exclusive - the sanitizer
# block above strips /RTC1 for exactly that reason. That is the division of
# labour between the two Debug trees, not an oversight: this one reports
# uninitialised locals and stack frame damage, the sanitized one reports heap
# overflows, use-after-free and double frees. Neither subsumes the other.
if(MSVC AND NOT USE_ASAN)
    foreach(_rtc_lang C CXX)
        if(NOT CMAKE_${_rtc_lang}_FLAGS_DEBUG MATCHES "/RTC")
            set(CMAKE_${_rtc_lang}_FLAGS_DEBUG "${CMAKE_${_rtc_lang}_FLAGS_DEBUG} /RTC1")
        endif()
    endforeach()

    message(STATUS "Debug runtime checks: /RTC1 (uninitialised locals, stack frames)")
endif()

# Parallel compilation.
#
# MSVC compiles the sources of one project one at a time unless asked
# otherwise. msbuild's own -m flag only overlaps separate *projects*, so the
# two targets that dominate this build - DarkEden and VS_UI, several hundred
# translation units each - still come out largely serial. /MP adds the
# file-level parallelism inside a project, which is where the wall clock
# actually goes.
#
# Set at directory scope here, above every add_subdirectory and every
# add_library/add_executable below, so it reaches all of them - DarkEden,
# VS_UI, TextSystem, basic, DXLib, SpriteLib, framelib, the viewer tools and
# the test binaries. Pinning it onto individual .vcxproj files instead does
# not survive the next CMake regeneration, since those files are generated.
#
# Restricted to C and CXX because the resource compiler does not accept /MP.
#
# /MP is rejected alongside #import and /Gm; neither appears in this tree.
# It also multiplies with msbuild -m, so `cmake --build . -- -m` can
# oversubscribe the machine - that is the intended trade, not an accident.
if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/MP>)

    # Source and narrow string literals use UTF-8, including files with a BOM.
    # The Windows ANSI code page must not change compiled resource bytes.
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/utf-8>)

    message(STATUS "Parallel compilation: /MP (per-project, all targets)")

    # Strict conformance. /permissive- turns off the MSVC extensions that
    # would let removed syntax back in silently; /Zc:__cplusplus reports
    # the real standard so library code can test it. Added through
    # add_compile_options, not CMAKE_CXX_FLAGS: overriding the flags
    # variable drops CMake's MSVC defaults, /EHsc among them, and without
    # it a C++ exception cannot be caught by type.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/permissive->)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>)

    message(STATUS "Conformance: /permissive- /Zc:__cplusplus (all C++ targets)")
    message(STATUS "Source and execution encoding: UTF-8 (all C/C++ targets)")
endif()

# One warning policy reaches every compiled target, including new subdirectories.
# CMake already defaults MSVC to /W3; the extra level-1 warnings make pointer
# truncation/extension and undefined preprocessor conditions explicit.
add_library(project_warnings INTERFACE)
if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # clang-cl accepts the MSVC /w1xxxx flags but does not enable the matching
    # diagnostics. Forward Clang options explicitly through its CL driver.
    target_compile_options(project_warnings INTERFACE
        "$<$<COMPILE_LANGUAGE:C,CXX>:/clang:-Wall;/clang:-Wextra;/clang:-Wundef>")
elseif(MSVC)
    target_compile_options(project_warnings INTERFACE
        $<$<COMPILE_LANGUAGE:C,CXX>:/W3>
        $<$<COMPILE_LANGUAGE:C,CXX>:/w14311>
        $<$<COMPILE_LANGUAGE:C,CXX>:/w14312>
        $<$<COMPILE_LANGUAGE:C,CXX>:/w14668>)
else()
    target_compile_options(project_warnings INTERFACE
        "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wundef>"
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wundef>")
endif()
link_libraries(project_warnings)

# Find SDL2 (needed for engine and DXLib)
if(EMSCRIPTEN)
	list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/web")
	include(cmake/web/Dependencies.cmake)
else()
if(BUILD_ENGINE OR USE_SDL_BACKEND OR NOT WIN32)
	find_package(SDL2 REQUIRED)
	find_package(SDL2_image REQUIRED)
	find_package(SDL2_ttf REQUIRED)
endif()

# Find iconv for basic/TextEncoding.cpp. On Windows
# this isn't a system library - it comes from vcpkg - so it needs an actual
# find_package() to resolve a full path; a bare target_link_libraries(iconv)
# just passes the literal string "iconv" to the linker, which then can't
# find iconv.lib in its default search paths (LNK1104) even though it's
# right there in the vcpkg install.
find_package(Iconv REQUIRED)

# Find JPEG library (needed for UtilityFunction.cpp)
find_package(JPEG QUIET)
if(NOT JPEG_FOUND)
	# Try pkg-config as fallback
	find_package(PkgConfig QUIET)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(JPEG libjpeg)
	endif()
endif()
if(JPEG_FOUND OR JPEG_LIBRARIES)
	message(STATUS "JPEG library found: ${JPEG_LIBRARIES}")
else()
	message(WARNING "JPEG library not found - JPG loading/saving may not work")
endif()
endif()

# Subdirectories
add_subdirectory(third_party/unrar)
# The rules the client and the server compute alike, vendored from the
# server (third_party/decore/README.md). Here, after
# link_libraries(project_warnings), so it builds under the warning policy.
add_subdirectory(third_party/decore)
add_subdirectory(basic)
add_subdirectory(Client/DXLib)
add_subdirectory(Client/SpriteLib)

# Additional libraries from VC6 project
add_subdirectory(Client/framelib)

# ============================================================
# packetwire - the wire library
# ============================================================
#
# Everything under Client/Packet - every .cpp, since task 5.1's fifth
# slice (2026-09-09): sockets, streams, the encrypter,
# StringStream/Exception, the wire info classes, the two player classes
# and the inbound peer connection manager, every packet class in every direction, and
# the factory/validator tables (docs/RESTRUCTURING.md tasks 1.1, 2.4 and
# 5.1). The packet handlers
# live in Client/PacketHandler and compile into the executable, except
# the few that reach only model state, which gamemodel lists (task 4.15);
# the packet headers only declare them.
#
# Membership is tests/arch/packetwire_files.txt, one repository-relative
# path per line. Three readers, one file, so the target and the rules
# cannot disagree: this target, the include-graph checker
# (tests/arch/check_includes.pl - whose rule W0 also demands that every
# .cpp under Client/Packet be listed there or in
# tests/arch/packetwire_holdouts.txt) and the ratchet script
# (tests/ratchet/ratchets.sh, R4). Deliberately not file(GLOB): a moved
# or new file has to be listed on purpose. CMAKE_CONFIGURE_DEPENDS makes
# an edit to the list re-run configure, which a bare file(STRINGS) would
# not.
#
# Absolute paths so the list(REMOVE_ITEM ...) against the globbed
# CLIENT_MAIN_SOURCES below matches - file(GLOB) returns absolute paths,
# and a relative entry is silently not removed (that mismatch is exactly
# how sources end up compiled into both a library and the executable,
# see the LNK4217 note in CLAUDE.md).
set(PACKETWIRE_MEMBERSHIP_FILE ${CMAKE_CURRENT_SOURCE_DIR}/tests/arch/packetwire_files.txt)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PACKETWIRE_MEMBERSHIP_FILE})
# Every line is stripped before it is matched, so an indented path is
# read the same way the checker and the ratchet script read it (the
# checker rejects indentation outright, so the three cannot drift).
file(STRINGS ${PACKETWIRE_MEMBERSHIP_FILE} _packetwire_lines)
set(PACKETWIRE_SOURCES)
foreach(_line IN LISTS _packetwire_lines)
	string(STRIP "${_line}" _line)
	if(NOT _line MATCHES "^Client/Packet/.*\\.cpp$")
		continue()
	endif()
	if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_line})
		message(FATAL_ERROR "packetwire membership lists a file that does not exist: ${_line}")
	endif()
	list(APPEND PACKETWIRE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${_line})
endforeach()
list(LENGTH PACKETWIRE_SOURCES _packetwire_count)
# 518 today; a truncated list must not configure as "the library".
if(_packetwire_count LESS 400)
	message(FATAL_ERROR "packetwire membership file yielded only ${_packetwire_count} sources - is ${PACKETWIRE_MEMBERSHIP_FILE} intact?")
endif()

add_library(packetwire STATIC ${PACKETWIRE_SOURCES})

target_include_directories(packetwire PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/basic
	${CMAKE_CURRENT_SOURCE_DIR}/Client
	${CMAKE_CURRENT_SOURCE_DIR}/Client/Packet
)

# Byte-identical compilation to how these files compiled inside the
# DarkEden target: mirror its compile definitions exactly (see the
# DarkEden target below for why each exists). __GAME_CLIENT__=1 was in
# this list until task 5.2's tenth slice (2026-09-13) retired the macro
# from the whole build; ratchet R15 keeps the sources at zero live
# tokens of it, so nothing here or in Client_PCH.h defines it any more.
target_compile_definitions(packetwire PRIVATE
	USE_SDL_BACKEND
	SPRITELIB_BACKEND_SDL
)
target_compile_definitions(packetwire PUBLIC ${DARKEDEN_PLATFORM_DEFINITIONS})

target_link_libraries(packetwire PUBLIC basic)
include(cmake/WebSocket.cmake)

# Socket.cpp/SocketImpl.cpp call Winsock directly; carry the import
# library with the target so every consumer resolves it.
if(WIN32)
	target_link_libraries(packetwire PUBLIC ws2_32)
endif()

message(STATUS "packetwire library: ${_packetwire_count} sources from tests/arch/packetwire_files.txt")

# ============================================================
# gamemodel - game-model code with no SDL/UI/dxlib reach
# (docs/RESTRUCTURING.md task 4.1): the pure data tables and the
# string/logging support they are built on. Same membership-file
# discipline as packetwire: tests/arch/gamemodel_files.txt is read by
# this block (.cpp lines), the include checker (rules M0-M2) and the
# ratchet script (R4). Header lines in the file are the checker's
# allowed set and are skipped here.
# ============================================================
set(GAMEMODEL_MEMBERSHIP_FILE ${CMAKE_CURRENT_SOURCE_DIR}/tests/arch/gamemodel_files.txt)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GAMEMODEL_MEMBERSHIP_FILE})
file(STRINGS ${GAMEMODEL_MEMBERSHIP_FILE} _gamemodel_lines)
set(GAMEMODEL_SOURCES)
foreach(_line IN LISTS _gamemodel_lines)
	string(STRIP "${_line}" _line)
	if(NOT _line MATCHES "^Client/.*\\.cpp$")
		continue()
	endif()
	if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_line})
		message(FATAL_ERROR "gamemodel membership lists a file that does not exist: ${_line}")
	endif()
	list(APPEND GAMEMODEL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${_line})
endforeach()
list(LENGTH GAMEMODEL_SOURCES _gamemodel_count)
# 10 today; a truncated list must not configure as "the library".
if(_gamemodel_count LESS 8)
	message(FATAL_ERROR "gamemodel membership file yielded only ${_gamemodel_count} sources - is ${GAMEMODEL_MEMBERSHIP_FILE} intact?")
endif()

add_library(gamemodel STATIC ${GAMEMODEL_SOURCES})

target_include_directories(gamemodel PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/basic
	${CMAKE_CURRENT_SOURCE_DIR}/Client
	${CMAKE_CURRENT_SOURCE_DIR}/Client/framelib
	${CMAKE_CURRENT_SOURCE_DIR}/Client/Packet
)

# Same definitions as packetwire, for the same reason: these files used
# to compile inside VS_UI and the executable, and identical
# preprocessing is the point of the move.
target_compile_definitions(gamemodel PRIVATE
	USE_SDL_BACKEND
	SPRITELIB_BACKEND_SDL
)

# packetwire for Properties (the file-definition table the string
# table's language test reads); framelib for CAnimationFrame, the item
# model's animation base (task 4.4 - its .cpp holds the constructor).
# basic owns the shared text codec and publishes its iconv dependency.
# decore is the server's rules, vendored (third_party/decore); PUBLIC, so
# a consumer that includes a model header reaching "domain/X.h" builds.
target_link_libraries(gamemodel PUBLIC basic packetwire framelib decore)

message(STATUS "gamemodel library: ${_gamemodel_count} sources from tests/arch/gamemodel_files.txt")

# ============================================================
# TextSystem - New SDL-based Text System (Abstracted)
# ============================================================
if(USE_SDL_BACKEND OR NOT WIN32)
	set(TEXTSYSTEM_SOURCES
		Client/TextSystem/TextService.cpp
		Client/TextSystem/TextBackendSDL.cpp
		Client/TextSystem/RenderTargetSpriteSurface.cpp
	)

	add_library(TextSystem STATIC ${TEXTSYSTEM_SOURCES})

	target_include_directories(TextSystem PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/Client/TextSystem
		${CMAKE_CURRENT_SOURCE_DIR}/Client/SpriteLib
		${CMAKE_CURRENT_SOURCE_DIR}/Client/DXLib
		${CMAKE_CURRENT_SOURCE_DIR}/Client
		${CMAKE_CURRENT_SOURCE_DIR}/basic
		${SDL2_INCLUDE_DIRS}
	)

	target_link_libraries(TextSystem PUBLIC
		SpriteLib
		SDL2::SDL2
		SDL2_ttf::SDL2_ttf
	)

	target_compile_definitions(TextSystem PRIVATE SPRITELIB_BACKEND_SDL USE_SDL_BACKEND)

	# Text encoding conversion is provided by basic through SpriteLib.
	message(STATUS "TextSystem library will be built from ${TEXTSYSTEM_SOURCES}")
endif()

if(BUILD_ENGINE)
	add_subdirectory(tools/engine/sprite)
endif()

# ============================================================
# Unit tests
# ============================================================
if(BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

# ============================================================
# VS_UI Library - UI System (按需编译必要的 Client 文件)
# ============================================================
if(USE_SDL_BACKEND OR NOT WIN32)
	# VS_UI source files
	file(GLOB_RECURSE VS_UI_SRC_SOURCES CONFIGURE_DEPENDS
		VS_UI/src/*.cpp
		VS_UI/*.cpp  # Include VS_UI root directory for RarFile.cpp
	)

	# Exclude Windows-specific files
	if(NOT WIN32)
		list(FILTER VS_UI_SRC_SOURCES EXCLUDE REGEX ".*Imm.*")
		list(FILTER VS_UI_SRC_SOURCES EXCLUDE REGEX ".*WebBrowser.*")
		# U_edit is now cross-platform (SDL-based), so don't exclude it
		# Exclude problematic files in VS_UI root directory
		# Note: u_window.cpp is now cross-platform compatible
	endif()

	# Exclude native Win32 GDI code superseded by Client/RenderingFunctions.cpp.
	# The excluded IMM implementation Ci.cpp was deleted; Ci_macOS.cpp supplies
	# the compatibility classes used with SDL text input. USE_SDL_BACKEND is
	# forced ON above for all platforms including WIN32 ("Windows native support
	# removed"), so these must be excluded regardless of platform, not just NOT WIN32.
	if(USE_SDL_BACKEND)
		list(FILTER VS_UI_SRC_SOURCES EXCLUDE REGEX "VS_UI/src/hangul/FL2\\.cpp")
	endif()

	# VS_UI/WinMain.cpp - a second, never-linked WinMain that drove the
	# widget system without ever calling InitGame() - was excluded here on
	# both platforms until docs/RESTRUCTURING.md task 5.2's sixth slice
	# deleted it (2026-09-09), together with Client/OtherClass/, two
	# request-side packet factory managers no target compiled and no file
	# referenced. History keeps both.

	# VS_UI builds from its own sources only. It used to add a list of
	# Client/*.cpp files (VS_UI_CLIENT_SOURCES) that the executable's glob
	# compiled as well, and the executable's objects were the ones that
	# linked; the list is gone and those files compile once, into the
	# executable (docs/RESTRUCTURING.md task 4.0). The last holdout was
	# Client/Client.cpp, which the non-Windows build filtered out of the
	# executable and appended here instead; it now compiles into the
	# executable on every platform like the rest of Client/ (the port
	# assessment, area A).
	add_library(VS_UI STATIC ${VS_UI_SRC_SOURCES})
	# Compile the shared header with this target's own definitions/options.
	# The VS_UI wrapper keeps its warning pragmas local to existing includers;
	# forcing that wrapper into every source would hide additional diagnostics.
	target_precompile_headers(VS_UI PRIVATE
		"$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/Client/Client_PCH.h>"
	)

	# VS_UI_WebBrowser.h/.cpp use ATL (atlbase.h, CComBSTR, CComVariant) for the
	# legacy in-game IE browser control. ATL ships alongside the MSVC toolset
	# under atlmfc/include but is not on the default include path. Locate it
	# by searching installed VS instances directly rather than trusting
	# CMAKE_CXX_COMPILER, which can point at a toolset instance (e.g. a
	# CMakeCache generated against a since-removed VS Build Tools install)
	# that no longer matches whichever MSBuild is actually used to build.
	if(MSVC)
		file(GLOB _atlmfc_candidates
			"C:/Program Files/Microsoft Visual Studio/*/*/VC/Tools/MSVC/*/atlmfc/include"
			"C:/Program Files (x86)/Microsoft Visual Studio/*/*/VC/Tools/MSVC/*/atlmfc/include"
		)
		if(_atlmfc_candidates)
			list(SORT _atlmfc_candidates)
			list(GET _atlmfc_candidates -1 _atlmfc_include_dir)
			# ATL's headers (atlbase.h et al.) embed a
			# #pragma comment(lib, "atls.lib") - the linker needs this
			# directory too (DarkEden.exe, the final link target that
			# actually pulls in VS_UI, is set up further below), or it
			# can't find atls.lib even though the include dir above lets
			# it compile fine (LNK1104). Derive it from the same toolset
			# instance as the include dir above, not just any installed
			# atlmfc/lib, so headers and binary stay from the same version.
			string(REPLACE "/include" "/lib" _atlmfc_lib_dir "${_atlmfc_include_dir}")
			if(CMAKE_SIZEOF_VOID_P EQUAL 8)
				set(_atlmfc_lib_dir "${_atlmfc_lib_dir}/x64")
			else()
				set(_atlmfc_lib_dir "${_atlmfc_lib_dir}/x86")
			endif()
		endif()
	endif()

	# Include directories
	target_include_directories(VS_UI PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/VS_UI
		${CMAKE_CURRENT_SOURCE_DIR}/VS_UI/src/header
		${CMAKE_CURRENT_SOURCE_DIR}/VS_UI/src/widget
		${CMAKE_CURRENT_SOURCE_DIR}/VS_UI/src/hangul
		${CMAKE_CURRENT_SOURCE_DIR}/VS_UI/src/Imm
		${CMAKE_CURRENT_SOURCE_DIR}/Client
		${CMAKE_CURRENT_SOURCE_DIR}/Client/TextSystem
		${CMAKE_CURRENT_SOURCE_DIR}/Client/SpriteLib
		${CMAKE_CURRENT_SOURCE_DIR}/Client/DXLib
		${CMAKE_CURRENT_SOURCE_DIR}/Client/framelib
		${CMAKE_CURRENT_SOURCE_DIR}/Client/Packet
		${CMAKE_CURRENT_SOURCE_DIR}/Client/WinLib
		${CMAKE_CURRENT_SOURCE_DIR}
		${_atlmfc_include_dir}
	)

	# Link libraries
	target_link_libraries(VS_UI PUBLIC
		basic
		packetwire
		gamemodel
		dxlib
		SpriteLib
		framelib
		TextSystem
		SDL2::SDL2
		SDL2_image::SDL2_image
		SDL2_ttf::SDL2_ttf
	)

	# No SDL2main here on any platform. On Windows it would collide with
	# Client/Client.cpp's own WinAPI WinMain (the deleted VS_UI/WinMain.cpp
	# used to duplicate this reasoning); on Linux and macOS <SDL_main.h>
	# leaves main() alone, so the
	# library is empty there and the target is one a package may not
	# even export (Homebrew's config creates it only when libSDL2main.a
	# is present). The DarkEden target has the same note.

	# Define _LIB to match the original VC6 library configuration: the
	# #ifndef _LIB regions of VS_UI are its standalone UI test harness
	# (its own dialogs, gpC_press_button, message-queue accessors) and stay
	# out of the game build. PUBLIC, not PRIVATE: the headers change class
	# layout under _LIB (C_VS_UI_TITLE's m_pC_dialog, for one), so every
	# translation unit that includes them - the executable's above all -
	# must see the same definition as the library's own objects, or the two
	# sides of a call disagree about where the members are (the code-health
	# review's _LIB ODR finding; docs/RESTRUCTURING.md task 4.0).
	target_compile_definitions(VS_UI PUBLIC _LIB)

	list(LENGTH VS_UI_SRC_SOURCES _vsui_count)
	message(STATUS "VS_UI library: ${_vsui_count} sources from VS_UI/")
endif()

# ============================================================
# Resource Management Tools
# ============================================================

# Resource Validator - checks all resource files for integrity
if(BUILD_RESOURCE_TOOLS)
add_executable(resource_validator
	Client/tools/resource_validator.cpp
)

target_include_directories(resource_validator PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(resource_validator PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ============================================================
# Resource Viewing Tools
# ============================================================

# Sprite Viewer - view .spk sprite pack files (C++ version using Client/SpriteLib)
add_executable(sprite_viewer
	tools/viewers/sprite_viewer/main.cpp
)

target_include_directories(sprite_viewer PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/Client/SpriteLib
	${CMAKE_CURRENT_SOURCE_DIR}/Client/DXLib
)

target_link_libraries(sprite_viewer PRIVATE
	SpriteLib
	dxlib
	basic
	SDL2::SDL2
)

# main() gets renamed to SDL_main by <SDL.h> on Windows; SDL2main.lib
# supplies the real WinMain/main that forwards to it. Unlike DarkEden (which
# defines its own WinMain and must skip this), this tool has no WinMain of
# its own, so it needs SDL2main - without it, link fails with LNK2019 on
# `main` (see invoke_main in exe_main.obj).
if(WIN32)
	target_link_libraries(sprite_viewer PRIVATE SDL2::SDL2main)
endif()

set_target_properties(sprite_viewer PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)


# Zone Parser - parse and dump .map zone files (standalone C++ version)
add_executable(zone_parser
	tools/viewers/zone_parser/main.cpp
)

target_link_libraries(zone_parser PRIVATE)

set_target_properties(zone_parser PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Item Viewer - view .ispk indexed sprite pack files (C++ version using Client/SpriteLib)
add_executable(item_viewer
	tools/viewers/item_viewer/main.cpp
)

target_include_directories(item_viewer PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/Client/SpriteLib
	${CMAKE_CURRENT_SOURCE_DIR}/Client/DXLib
)

target_link_libraries(item_viewer PRIVATE
	SpriteLib
	dxlib
	basic
	SDL2::SDL2
)

# See sprite_viewer above for why this is needed on Windows.
if(WIN32)
	target_link_libraries(item_viewer PRIVATE SDL2::SDL2main)
endif()

set_target_properties(item_viewer PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Creature Viewer - view creature animations (C++ version using Client/SpriteLib + Client/framelib)
add_executable(creature_viewer
	tools/viewers/creature_viewer/main.cpp
)

target_include_directories(creature_viewer PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/Client/SpriteLib
	${CMAKE_CURRENT_SOURCE_DIR}/Client/DXLib
	${CMAKE_CURRENT_SOURCE_DIR}/Client/framelib
)

target_link_libraries(creature_viewer PRIVATE
	SpriteLib
	framelib
	dxlib
	basic
	SDL2::SDL2
)

# See sprite_viewer above for why this is needed on Windows.
if(WIN32)
	target_link_libraries(creature_viewer PRIVATE SDL2::SDL2main)
endif()

set_target_properties(creature_viewer PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# The map loader uses tools/engine/sprite/zone.c.
if(BUILD_ENGINE)
	# Map Viewer - browse zone maps with pan/zoom (C++ version using Client/SpriteLib)
	add_executable(map_viewer
		tools/viewers/map_viewer/main.cpp
		tools/viewers/map_viewer/zoneloader.cpp
		Client/TileRenderer.cpp
	)

	target_include_directories(map_viewer PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/Client
		${CMAKE_CURRENT_SOURCE_DIR}/Client/SpriteLib
		${CMAKE_CURRENT_SOURCE_DIR}/Client/DXLib
		${CMAKE_CURRENT_SOURCE_DIR}/tools/engine/sprite/include
	)

	target_link_libraries(map_viewer PRIVATE
		sprite
		SpriteLib
		dxlib
		basic
		SDL2::SDL2
	)

	# See sprite_viewer above for why this is needed on Windows.
	if(WIN32)
		target_link_libraries(map_viewer PRIVATE SDL2::SDL2main)
	endif()

	set_target_properties(map_viewer PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
	)
endif()

# Effect Viewer - test and debug effect system
add_executable(effect_viewer
	tools/viewers/effect_viewer/main.cpp
	tools/viewers/effect_viewer/EffectSpriteTypeTable.cpp
	Client/EffectResourceContainer.cpp
)

target_include_directories(effect_viewer PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/Client
	${CMAKE_CURRENT_SOURCE_DIR}/Client/Packet
	${CMAKE_CURRENT_SOURCE_DIR}/Client/SpriteLib
	${CMAKE_CURRENT_SOURCE_DIR}/Client/DXLib
	${CMAKE_CURRENT_SOURCE_DIR}/Client/framelib
	${CMAKE_CURRENT_SOURCE_DIR}/basic
)

target_link_libraries(effect_viewer PRIVATE
	gamemodel # The same effect implementation as the game and unit tests
	SpriteLib
	framelib
	dxlib
	basic
	SDL2::SDL2
)

# See sprite_viewer above for why this is needed on Windows.
if(WIN32)
	target_link_libraries(effect_viewer PRIVATE SDL2::SDL2main)
endif()

set_target_properties(effect_viewer PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
endif()


# Examples (always built if backend is enabled)
# Examples directory removed during cleanup
# if(USE_SDL_BACKEND OR NOT WIN32)
# 	add_subdirectory(examples)
# endif()

# Dark Eden Client Executable
if(USE_SDL_BACKEND OR NOT WIN32)
	# Collect remaining Client source files (excluding those already in VS_UI)
	file(GLOB CLIENT_MAIN_SOURCES CONFIGURE_DEPENDS
		Client/*.cpp
		Client/Packet/*.cpp
		Client/Packet/*/*.cpp
		# The packet handlers (task 2.4): executable-side, bound by
		# Client/PacketHandlerRegistry.cpp; the wire classes they handle
		# are in packetwire. The handlers gamemodel lists (task 4.15) are
		# removed from this list below with the other gamemodel members.
		Client/PacketHandler/*.cpp
		Client/SXml/*.cpp
		Client/WinLib/*.cpp
	)

	# Add additional source files
	list(APPEND CLIENT_MAIN_SOURCES
		# Globals.cpp - the globals and print helpers the game needs
		Client/Globals.cpp
	)

	# The eight files this list used to remove one by one - the port-era
	# exclusion graveyard (GlobalVariables, MissingGlobals, GameHelpers,
	# GameFunctions, GamePacketFunctions, ActionFunctions and a stale
	# copy of the GCNotifyWin packet class and its handler) - are deleted
	# (docs/RESTRUCTURING.md task 5.2, fourth slice). Every function they
	# defined was defined by a file the build does compile, so there was
	# nothing in them to keep.

	# packetwire members compile in the packetwire library now; leaving
	# them here too would compile them into the executable a second time
	# (the existing LNK4217 double-compilation trap). What stays of
	# Client/Packet/*.cpp after this is exactly
	# tests/arch/packetwire_holdouts.txt - nothing, since task 5.1's
	# fifth slice; the file is where the next exception would be listed.
	list(REMOVE_ITEM CLIENT_MAIN_SOURCES ${PACKETWIRE_SOURCES})
	# EXISTS is case-insensitive on Windows but REMOVE_ITEM is not: a
	# membership entry whose case differs from the on-disk name would pass
	# the existence check above and still be globbed into the executable
	# - compiled twice, silently. Assert the removal actually happened.
	foreach(_src IN LISTS PACKETWIRE_SOURCES)
		list(FIND CLIENT_MAIN_SOURCES ${_src} _still_in_exe)
		if(NOT _still_in_exe EQUAL -1)
			message(FATAL_ERROR "packetwire member is still in the executable's source list (case mismatch in tests/arch/packetwire_files.txt?): ${_src}")
		endif()
	endforeach()

	# The same for gamemodel (tests/arch/gamemodel_files.txt): its members
	# come from the Client/ glob above.
	list(REMOVE_ITEM CLIENT_MAIN_SOURCES ${GAMEMODEL_SOURCES})
	foreach(_src IN LISTS GAMEMODEL_SOURCES)
		list(FIND CLIENT_MAIN_SOURCES ${_src} _still_in_exe)
		if(NOT _still_in_exe EQUAL -1)
			message(FATAL_ERROR "gamemodel member is still in the executable's source list (case mismatch in tests/arch/gamemodel_files.txt?): ${_src}")
		endif()
	endforeach()

	# CAvi.cpp is NOT excluded (re-checked 2026-08-22): the "never
	# instantiated" claim this exclusion used to be based on doesn't hold -
	# COpeningUpdate::PlayMPG() does instantiate CAVI and call OpenMPG()/
	# Play()/Close(); this was only an unresolved-external link error while
	# compile errors elsewhere stopped masking it. The MCI_DGV_* concern
	# (current Windows SDKs no longer declare those digital-video structures)
	# is real, but moot either way - that MCI driver (MCIAVI.DRV) was never
	# shipped for 64-bit Windows at all, so even defining the missing
	# structures ourselves would only compile, not actually work on x64.
	# CAVI's methods are stubbed out directly in CAvi.cpp instead (all
	# platforms, not just non-Windows) rather than excluding the file.

	# The custom MP3/Huffman decoder (mp3/huffman/reader/header/synfilt/
	# subdecoder/soundbuf/BIT_RES, once duplicated between Client/ and
	# Client/DXLib/) was deleted 2026-08-31: no CMakeLists ever compiled the
	# DXLib copies, nothing outside the family referenced its symbols, and
	# MP3/OGG decoding now comes from SDL2_mixer.

	# Same duplication again: CSpritePal.cpp was SpriteLib.lib-only in the
	# original VC6 project (client-master_vs6/Client/SpriteLib/SpriteLib.dsp)
	# and never part of Client.dsp (the EXE). The root-level copy here was
	# compiling straight into DarkEden.exe too, fighting the SpriteLib.lib
	# copy for every CSpritePal symbol at link time (LNK2005).
	list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX "/Client/CSpritePal\\.cpp$")

	# Backup files (any *_bak-* copy of a source) are stale duplicates of
	# their non-"_bak" counterpart and were never meant to compile on any
	# platform - this used to be scoped inside the `if(NOT WIN32)` block
	# below, so on Windows these still got globbed in and fought their
	# current counterparts for the same symbols at link time (LNK2005).
	list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*\\.bak.*")
	list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*_bak-.*")

	# Exclude Windows-specific files
	if(NOT WIN32)
		list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*Imm.*")
		list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*D3D.*")
		list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*Direct3.*")
		# list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*GameMain.*")  # Re-enabled for cross-platform support
		list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*EXECryptor.*")
		list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*GetWinVer.*")
		# Three anchored "^Client/..." filters used to sit here, for the
		# root-level *Handler.cpp files, Client.cpp and ClientFunction.cpp.
		# None ever matched: the glob yields absolute paths. There is no
		# root-level *Handler.cpp any more, and the other two compile into
		# the executable on every platform, so the filters are gone rather
		# than fixed - fixing them would have changed what links.
		# MWorkThread and MMusic are needed on all platforms
		# list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*MWorkThread.*")
		# list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*MMusic\\.cpp")
		# Note: PacketFunction.cpp now included with proper platform guards
		# Windows-specific NetBIOS/IPX/SPX code is wrapped in #ifdef PLATFORM_WINDOWS
		# list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*PacketFunction\\.cpp")
		# ProfileManager needed on all platforms
		# list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*ProfileManager\\.cpp")
		list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*VolumeOut.*\\.cpp")
		list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX ".*WavePackFileManager\\.cpp")
		# Exclude Windows-specific WinMain files. Client/Client.cpp is not
		# filtered any more: its WinMain is behind PLATFORM_WINDOWS and the
		# rest of the file is ClientMain, the game's startup on every
		# platform (the port assessment's area D); SDLMain.cpp supplies
		# main() off Windows and calls it.
		list(FILTER CLIENT_MAIN_SOURCES EXCLUDE REGEX "WinLib/CWinMain\\.cpp")
	endif()

	# Add DarkEden executable
	# WIN32 keyword required on Windows: Client/Client.cpp (compiled into the
	# executable) defines a real WinAPI WinMain, so the exe must link as a GUI
	# subsystem app (not console/main).
	if(WIN32)
		add_executable(DarkEden WIN32 ${CLIENT_MAIN_SOURCES})
	else()
		add_executable(DarkEden ${CLIENT_MAIN_SOURCES})
	endif()
	# Keep this PCH private and separate from VS_UI: their compile definitions
	# differ, so sharing the compiled artifact with REUSE_FROM is not valid.
	target_precompile_headers(DarkEden PRIVATE
		"$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/Client/Client_PCH.h>"
	)

	# Set output directory
	set_target_properties(DarkEden PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
		# Without this, Visual Studio's "Local Windows Debugger" (F5) runs
		# DarkEden.exe with the .vcxproj's own directory (build/vs2019) as
		# the working directory, not the exe's own folder (bin/<Config>) -
		# a mismatch trap, since GameInit.cpp loads "Data/Info/FileDef.inf"
		# and other game data via paths relative to the working directory.
		# Match double-click-from-Explorer behavior (CWD = exe's folder)
		# so "Data/" only needs to exist in one place regardless of how
		# the exe is launched.
		VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DarkEden>"
	)

	# Make F5 in the Visual Studio IDE launch the game. Without this the
	# generated solution defaults to ALL_BUILD as the startup project, which is
	# a CMake utility target with no executable behind it, so running it fails
	# with "Unable to start program ...\ALL_BUILD. Access is denied." rather
	# than anything that hints at the actual cause.
	if(MSVC)
		set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT DarkEden)
	endif()

	# atls.lib for VS_UI_WebBrowser.h's ATL usage - see the
	# _atlmfc_include_dir/_atlmfc_lib_dir search above (VS_UI target).
	if(_atlmfc_lib_dir)
		target_link_directories(DarkEden PRIVATE ${_atlmfc_lib_dir})
	endif()

	# Link libraries (now includes VS_UI and TextSystem)
	target_link_libraries(DarkEden PRIVATE
		SpriteLib
		dxlib
		basic
		packetwire
		gamemodel
		framelib
		VS_UI
		TextSystem
		SDL2::SDL2
	)

	# Standard Windows import libraries for APIs this project calls
	# directly (Winsock: SocketAPI.cpp/DatagramSocket.cpp/...; Winmm:
	# WavePackFileManager.cpp's mmio* calls; Comctl32: InitCommonControls). None of
	# these were previously in the link line, so every symbol from them
	# was an unresolved external (LNK2001/LNK2019) once the code that
	# calls them started compiling.
	if(WIN32)
		target_link_libraries(DarkEden PRIVATE
			ws2_32
			winmm
			comctl32
			netapi32
		)
	endif()

	# SDL2main is not linked on any platform. On Windows it provides a
	# WinMain that forwards to SDL_main(), and Client/Client.cpp defines
	# WinMain directly (not via the SDL_main macro convention), so the two
	# would collide (LNK2005). On Linux and macOS <SDL_main.h> does not
	# rename main() - SDL_MAIN_NEEDED and SDL_MAIN_AVAILABLE are defined
	# for neither - so SDLMain.cpp's main() is the real entry point and
	# the library has nothing to add; it used to be linked there anyway,
	# through a target Homebrew's config only exports when libSDL2main.a
	# happens to be installed.

	# Keep the static archive reader's redistribution notices beside each build.
	add_custom_command(TARGET DarkEden POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:DarkEden>/licenses/unrar"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/third_party/unrar/license.txt"
			"${CMAKE_CURRENT_SOURCE_DIR}/third_party/unrar/acknow.txt"
			"${CMAKE_CURRENT_SOURCE_DIR}/third_party/unrar/README.md"
			"$<TARGET_FILE_DIR:DarkEden>/licenses/unrar"
		VERBATIM)

	# The macOS application bundle, off by default: a bare executable in
	# bin/ beside the Data tree is what the Linux and Windows builds
	# produce and what tools/ci/verify-linux.sh runs. With the option on,
	# bin/DarkEden.app is built instead and the game looks for its data
	# in the bundle's Resources directory and then beside the bundle
	# (Client/Client.cpp, the data-root search off Windows). The plist
	# keys: NSHighResolutionCapable so a Retina display gives the SDL
	# window a backing store at native scale rather than a magnified
	# one, and the identifier the OS keys its preferences and its
	# permission prompts on.
	if(APPLE AND DARKEDEN_MACOS_BUNDLE)
		set_target_properties(DarkEden PROPERTIES
			MACOSX_BUNDLE TRUE
			MACOSX_BUNDLE_BUNDLE_NAME "Dark Eden"
			MACOSX_BUNDLE_GUI_IDENTIFIER "org.opendarkeden.client"
			MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
			MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
			MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/tools/macos/Info.plist.in"
		)
	endif()

	# Link iconv library
	if(Iconv_FOUND)
		target_link_libraries(DarkEden PRIVATE Iconv::Iconv)
	else()
		target_link_libraries(DarkEden PRIVATE iconv)
	endif()

	# Link JPEG library if available
	if(JPEG_LIBRARIES)
		target_link_libraries(DarkEden PRIVATE ${JPEG_LIBRARIES})
		if(JPEG_INCLUDE_DIRS)
			target_include_directories(DarkEden PRIVATE ${JPEG_INCLUDE_DIRS})
		endif()
	endif()

	# Include directories
	target_include_directories(DarkEden PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/basic
		${CMAKE_CURRENT_SOURCE_DIR}/Client
		${CMAKE_CURRENT_SOURCE_DIR}/Client/TextSystem
		${CMAKE_CURRENT_SOURCE_DIR}/Client/DXLib
		${CMAKE_CURRENT_SOURCE_DIR}/Client/SpriteLib
		${CMAKE_CURRENT_SOURCE_DIR}/Client/Packet
		${CMAKE_CURRENT_SOURCE_DIR}/Client/framelib
#		${CMAKE_CURRENT_SOURCE_DIR}/tools/engine/sprite/include
#		${CMAKE_CURRENT_SOURCE_DIR}/tools/engine/ui/include
		${CMAKE_CURRENT_SOURCE_DIR}/VS_UI/src/header
		${CMAKE_CURRENT_SOURCE_DIR}/VS_UI/src/widget
		${CMAKE_CURRENT_SOURCE_DIR}/VS_UI/src/hangul
	)

	# Add Immersion library include path only on Windows
	if(WIN32)
		target_include_directories(DarkEden PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}/VS_UI/src/Imm
		)
	endif()

	# Compile definitions
	target_compile_definitions(DarkEden PRIVATE
		USE_SDL_BACKEND
		SPRITELIB_BACKEND_SDL
	)

	# Platform-specific definitions come from packetwire's link interface.
	# On Windows it carries __WIN32__ and __WINDOWS__, and here is
	# why. __WIN32__ was always passed via /D in the original VC6 build
	# (Client.dsp) and Client/Packet/Exception.h's __BEGIN_DEBUG/__END_DEBUG
	# (and Assert1.h's Assert()) macros are conditioned on it. Without it,
	# neither the NDEBUG nor the __WIN32__ branch matches on a Windows
	# Debug build, leaving those macros undefined across every Packet/*
	# translation unit that uses them.
	# __WINDOWS__ was likewise always passed via /D in Client.dsp.
	# Several Packet/*.cpp files (SocketAPI.cpp, PacketFileAPI.cpp, ...)
	# branch on bare `#if __WINDOWS__`/`#elif __WINDOWS__` (not
	# `defined(__WINDOWS__)`) to pick the Windows vs. POSIX code path;
	# an undefined macro reads as 0 in a #if, so without this those
	# branches silently compiled neither path, leaving locals like
	# fd/result/client/nSent declared in the branch that never ran.
	# Off Windows the list is empty: the POSIX branches test
	# PLATFORM_POSIX, which basic/Platform.h derives from the compiler.

	message(STATUS "DarkEden executable will be built from remaining Client files (VS_UI compiled separately)")
endif()

if(EMSCRIPTEN)
	include(cmake/web/Client.cmake)
	include(cmake/web/Tests.cmake)
endif()

# Generate the actual, transitive options of every compiled target for CTest.
# Inspect after all subdirectories/targets exist, rather than maintaining a list
# that could silently omit the next library or executable.
function(collect_warning_policy directory)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(kind ${target} TYPE)
        if(kind MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
            set_property(GLOBAL APPEND_STRING PROPERTY WARNING_POLICY_CONTENT
                "${target}: $<JOIN:$<TARGET_PROPERTY:${target},COMPILE_OPTIONS>, >\n")
        endif()
    endforeach()
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        collect_warning_policy("${child}")
    endforeach()
endfunction()
collect_warning_policy("${CMAKE_SOURCE_DIR}")
get_property(warning_policy_content GLOBAL PROPERTY WARNING_POLICY_CONTENT)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/warning-policy-$<CONFIG>-$<COMPILE_LANGUAGE>.txt"
    CONTENT "${warning_policy_content}" TARGET project_warnings)

# Print configuration summary
message(STATUS "")
message(STATUS "=== Dark Eden Client Configuration ===")
message(STATUS "  Build Tests:      ${BUILD_TESTS}")
message(STATUS "  Build Fuzzers:    ${BUILD_FUZZERS}")
message(STATUS "  Use SDL Backend:  ${USE_SDL_BACKEND}")
message(STATUS "  Build Engine:     ${BUILD_ENGINE}")
message(STATUS "  C++ Standard:     ${CMAKE_CXX_STANDARD}")
if(APPLE)
	message(STATUS "  macOS bundle:     ${DARKEDEN_MACOS_BUNDLE}")
endif()
message(STATUS "=======================================")
message(STATUS "")
