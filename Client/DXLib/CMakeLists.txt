cmake_minimum_required(VERSION 3.20)

# DXLib Library - DirectX abstraction with backend selection
# Supports both Windows DirectX and SDL2 backends

project(dxlib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection and backend selection
if(WIN32)
	option(USE_SDL_BACKEND "Use SDL2 backend instead of DirectX" OFF)

	if(USE_SDL_BACKEND)
		message(STATUS "DXLib: Using SDL2 backend")
		find_package(SDL2 REQUIRED)
		find_package(SDL2_mixer)
	else()
		message(STATUS "DXLib: Using native Windows DirectX backend")
		# DirectX SDK paths - user may need to set these
		# set(DXSDK_DIR "$ENV{DXSDK_DIR}" CACHE PATH "DirectX SDK Directory")
		# include_directories(${DXSDK_DIR}/Include)
	endif()
endif()

# Non-Windows platforms must use SDL
if(NOT WIN32)
	# No platform macro: basic/Platform.h detects it from the compiler.
	find_package(SDL2 REQUIRED)
	find_package(SDL2_mixer)
	message(STATUS "DXLib: Using SDL2 backend (required on this platform)")
endif()

# Check if SDL2_mixer is available. SDL2_mixer >= 2.6 (what vcpkg ships)
# exports SDL2_mixer::SDL2_mixer; the older SDL2::SDL2_mixer spelling and the
# plain-variable FindSDL2_mixer path are kept for other package sources.
if(TARGET SDL2_mixer::SDL2_mixer OR TARGET SDL2::SDL2_mixer OR SDL2_MIXER_LIBRARIES)
	set(HAVE_SDL2_MIXER TRUE)
	message(STATUS "DXLib: SDL2_mixer found - audio support enabled")
	add_definitions(-DHAVE_SDL2_MIXER)
else()
	set(HAVE_SDL2_MIXER FALSE)
	message(STATUS "DXLib: SDL2_mixer NOT found - audio support disabled")
endif()

# DXLib source files
set(DXLIB_SOURCES
	# Cached sound buffers and caller-owned music files.
	CSoundPartManager.cpp
	COGGSTREAM.cpp

	# Platform backend
	DXLibBackendSDL.cpp
	DXInputHost.cpp

	# Core DirectDraw implementations
	CDirectDraw.cpp
	CDirectDrawSurface.cpp

	# Static member definitions for CDirectDraw
	CDirectDraw_StaticMembers.cpp

	# Adapter layers - Input (always available)
	CDirectInput_Adapter.cpp
)

# Audio: CSDLAudio / CSDLMusic / CSDLStream each have two definitions.
# The *_Adapter.cpp files are the real SDL_mixer implementations; the plain
# files are "not implemented" fallbacks that let the client link (silently)
# when SDL2_mixer is absent. Exactly one set goes in, never both.
if(HAVE_SDL2_MIXER)
	list(APPEND DXLIB_SOURCES
		CDirectSound_Adapter.cpp
		CDirectMusic_Adapter.cpp
		CDirectSoundStream_Adapter.cpp
	)
else()
	list(APPEND DXLIB_SOURCES
		CDirectSound.cpp
		CDirectMusic.cpp
		CDirectSoundStream.cpp
	)
endif()

# Add Windows-specific sources if using Windows backend
if(WIN32 AND NOT USE_SDL_BACKEND)
	# Add original DXLib files here (DirectX backend)
	# For now, we only support SDL backend in the adapter
	message(WARNING "Windows DirectX backend not yet implemented. Please use USE_SDL_BACKEND=ON")
endif()

# Create library
add_library(dxlib STATIC ${DXLIB_SOURCES})
set_property(TARGET dxlib PROPERTY DARKEDEN_HAS_SDL_MIXER ${HAVE_SDL2_MIXER})

if(USE_SDL_BACKEND OR NOT WIN32)
	target_compile_definitions(dxlib PUBLIC DXLIB_USE_SDL_BACKEND)
endif()

# Public include directories
target_include_directories(dxlib
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/basic  # For platform abstraction
)

# Link SDL2 if using SDL backend
if(USE_SDL_BACKEND OR NOT WIN32)
	if(TARGET SDL2::SDL2)
		target_link_libraries(dxlib PUBLIC SDL2::SDL2)
	else()
		target_link_libraries(dxlib PUBLIC ${SDL2_LIBRARIES})
		target_include_directories(dxlib PUBLIC ${SDL2_INCLUDE_DIRS})
	endif()

	# Link SDL2_mixer if available
	if(HAVE_SDL2_MIXER)
		if(TARGET SDL2_mixer::SDL2_mixer)
			target_link_libraries(dxlib PUBLIC SDL2_mixer::SDL2_mixer)
		elseif(TARGET SDL2::SDL2_mixer)
			target_link_libraries(dxlib PUBLIC SDL2::SDL2_mixer)
		else()
			target_link_libraries(dxlib PUBLIC ${SDL2_MIXER_LIBRARIES})
			target_include_directories(dxlib PUBLIC ${SDL2_MIXER_INCLUDE_DIRS})
		endif()
	endif()
endif()

# Link basic library for platform abstraction
target_link_libraries(dxlib PUBLIC basic SpriteLib)

# Windows-specific libraries
if(WIN32 AND NOT USE_SDL_BACKEND)
	target_link_libraries(dxlib PUBLIC
		dinput8
		dsound
		dxguid
		winmm
	)
endif()

# Installation
install(TARGETS dxlib
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)

# Install headers
install(FILES
	CSoundPartManager.h
	COGGSTREAM.h
	CPartManager.h
	DXLibBackend.h
	DXLib.h
	CDirectInput.h
	CDirectSound.h
	CDirectMusic.h
	CDirectSoundStream.h
	DESTINATION include/dxlib
)

# Build options message
message(STATUS "")
message(STATUS "=== DXLib Configuration ===")
message(STATUS "  Use SDL Backend:     ${USE_SDL_BACKEND}")
message(STATUS "  SDL2_mixer Support:  ${HAVE_SDL2_MIXER}")
message(STATUS "  C++ Standard:        ${CMAKE_CXX_STANDARD}")
message(STATUS "================================")
message(STATUS "")
