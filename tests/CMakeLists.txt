cmake_minimum_required(VERSION 3.21)

#----------------------------------------------------------------------
# Unit tests
#----------------------------------------------------------------------
#
# Only code compiled into a static library can be linked into a test
# binary. Today that means basic/, framelib/ (header-only templates),
# SpriteLib/, DXLib/, TextSystem/, VS_UI, packetwire (the whole wire
# layer: streams, info classes, every packet class and the factory
# tables - docs/RESTRUCTURING.md task 2.4) and gamemodel. Game logic that
# is compiled straight into the DarkEden executable, including the packet
# handlers under Client/PacketHandler, is not reachable from here; the
# eight handlers gamemodel lists (task 4.15) are the exception.
#
#----------------------------------------------------------------------

add_library(test_framework STATIC
	framework/test_framework.cpp
)

target_include_directories(test_framework PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/framework
)

#----------------------------------------------------------------------
# Test sources are globbed so adding a test only means adding a file to
# unit/, matching how the rest of this project collects sources.
#----------------------------------------------------------------------
file(GLOB UNIT_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp)

#----------------------------------------------------------------------
# There is no stubs/ directory any more (docs/RESTRUCTURING.md task
# 5.3). It held the globals and functions a linked library referenced
# but the DarkEden executable defined, and every one of them turned out
# to be a seam worth cutting rather than filling. Of the libraries this
# binary links, g_pLast was reached only by TextService::RenderText,
# which draws through the back buffer and has ~45 live call sites; it
# is defined in Client/TextServiceScreen.cpp now, on the executable
# side of the split. SendBugReport moved into packetwire in 5.1.
# (VS_UI reaches g_pLast too, and is not linked here.)
# A library that needs
# something from the program around it asks through a host struct - see
# MItemHost, MPriceHost, WireHost - which a test can install, rather
# than through a symbol a test binary has to invent.
#----------------------------------------------------------------------
add_executable(unit_tests
	framework/test_main.cpp
	support/platform_c_consumer.c
	# Installs the preload fit check (support/packet_stream_preload.h)
	support/packet_stream_access.cpp
	${UNIT_TEST_SOURCES}
)

target_link_libraries(unit_tests PRIVATE
	test_framework
	basic
	SpriteLib
	TextSystem
	packetwire
	gamemodel
	dxlib
)

target_include_directories(unit_tests PRIVATE
	${CMAKE_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/basic
	${CMAKE_SOURCE_DIR}/Client/framelib
	${CMAKE_SOURCE_DIR}/Client/TextSystem
	# Test-only seams into the packetwire streams (support/packet_stream_access.h)
	${CMAKE_CURRENT_SOURCE_DIR}/support
)

# Packet header definitions arrive through packetwire, including in tests
# added later. Only fixture locations need per-source definitions.
set_source_files_properties(
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_wire_layout.cpp
	PROPERTIES
		INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/generated"
		COMPILE_DEFINITIONS "WIRE_LAYOUT_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/wire-layout.txt\""
)
set_source_files_properties(
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_packet_goldens.cpp
	PROPERTIES
		COMPILE_DEFINITIONS "WIRE_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/golden\""
)

add_test(NAME unit_tests COMMAND unit_tests)

#----------------------------------------------------------------------
# decore_tests - the shared parity vectors of the vendored server rules
# (third_party/decore/README.md), every row asserted against the copy
# this toolchain built. It links only decore and the framework, which
# proves the copy is self-contained; the vector directory arrives as a
# compile definition, as WIRE_GOLDEN_DIR does above. Globbed like unit/,
# so a later slice adds a file. The Emscripten build defines its own
# decore_tests, with the vectors embedded, in cmake/web/Tests.cmake.
#----------------------------------------------------------------------
if(NOT EMSCRIPTEN)
	file(GLOB DECORE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/decore/*.cpp)
	add_executable(decore_tests
		framework/test_main.cpp
		${DECORE_TEST_SOURCES}
	)
	target_link_libraries(decore_tests PRIVATE test_framework decore)
	target_compile_definitions(decore_tests PRIVATE
		"DECORE_VECTOR_DIR=\"${CMAKE_SOURCE_DIR}/third_party/decore/domain/vectors\"")
	add_test(NAME decore_tests COMMAND decore_tests)
endif()

# Runs the texture compositor on SDL's deterministic software device in CI.
# Invoke sprite_gpu_tests --accelerated locally to require a real GPU device;
# that run fails, rather than silently skipping, if acceleration is unavailable.
add_executable(sprite_gpu_tests render/main.cpp render/test_sprite_gpu.cpp render/benchmark.cpp)
target_link_libraries(sprite_gpu_tests PRIVATE test_framework SpriteLib darkeden_xbrz)
add_test(NAME sprite_gpu_tests COMMAND sprite_gpu_tests)

# Exercise the actual settings reader/writer and key bindings, including old files.
add_executable(user_option_tests
	framework/test_main.cpp
	support/test_user_option.cpp
)
target_include_directories(user_option_tests PRIVATE ${CMAKE_SOURCE_DIR}/Client)
target_link_libraries(user_option_tests PRIVATE test_framework VS_UI basic)
add_test(NAME user_option_tests COMMAND user_option_tests)

# Link real UI components as their dependencies become independently testable.
add_executable(ui_tests
	framework/test_main.cpp
	ui/test_button.cpp
	ui/test_event_button.cpp
	ui/test_skin_manager.cpp
	ui/test_xml.cpp
	ui/test_resource_reader.cpp
	ui/test_ui_metadata.cpp
	ui/test_ui_runtime.cpp
	ui/test_help_messages.cpp
	ui/test_help_image_cache.cpp
	ui/test_portal_data.cpp
	ui/test_line_editor.cpp
	ui/test_editor_focus.cpp
	ui/test_ui_message_receiver.cpp
	ui/test_uitext_overrides.cpp
	ui/test_profile_path.cpp
)
target_link_libraries(ui_tests PRIVATE test_framework VS_UI basic)
# The shipped English overrides of the packed UI text, checked as files.
set_source_files_properties(
	${CMAKE_CURRENT_SOURCE_DIR}/ui/test_uitext_overrides.cpp
	PROPERTIES
		COMPILE_DEFINITIONS "UITEXT_DIR=\"${CMAKE_SOURCE_DIR}/tools/i18n/ui-text/Data/Ui/txt\""
)
add_test(NAME ui_tests COMMAND ui_tests)

#----------------------------------------------------------------------
# Shrink-only ratchets (tests/ratchet/ratchets.sh) - fails when a debt
# metric rises AND when it falls unrecorded (tighten the baseline in
# the same commit). Needs bash (Git for Windows ships one); without it
# the check is skipped, not passed.
#----------------------------------------------------------------------
if(REQUIRE_TEST_TOOLS)
	find_program(BASH_EXECUTABLE bash REQUIRED)
else()
	find_program(BASH_EXECUTABLE bash)
endif()
if(BASH_EXECUTABLE)
	add_test(
		NAME ratchets
		COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/ratchet/ratchets.sh ${CMAKE_BINARY_DIR}
	)
else()
	message(STATUS "bash not found: the ratchets test is not registered")
endif()

#----------------------------------------------------------------------
# Include-graph architecture rules (tests/arch/check_includes.pl) -
# walks the packetwire quote-include closure and fails on forbidden
# edges (W1: only Client/Packet, basic, Client_PCH.h; W2: no
# exe-defined debug facilities). Skipped without perl, not passed.
# find_program lives here, above every block that tests the variable -
# it used to sit further down, which would leave any earlier test
# silently unregistered on the first configure of a fresh tree.
#----------------------------------------------------------------------
if(REQUIRE_TEST_TOOLS)
	find_program(PERL_EXECUTABLE perl REQUIRED)
else()
	find_program(PERL_EXECUTABLE perl)
endif()
if(PERL_EXECUTABLE)
	add_test(
		NAME arch_includes
		COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/arch/check_includes.pl
	)
else()
	message(STATUS "perl not found: the arch_includes test is not registered")
endif()

# decore_vendored - the de-core copy is what tools/decore/sync.pl wrote:
# the MANIFEST hashes match, nothing under third_party/decore/domain is
# unlisted, and the decore target builds every vendored .cpp. A hand edit
# fails it; resync instead. Skipped without perl, not passed.
if(PERL_EXECUTABLE)
	add_test(
		NAME decore_vendored
		COMMAND ${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/decore/sync.pl --verify-manifest
	)
else()
	message(STATUS "perl not found: the decore_vendored test is not registered")
endif()

if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(warning_compiler_style CLANG_CL)
elseif(MSVC)
	set(warning_compiler_style MSVC)
else()
	set(warning_compiler_style GNU)
endif()
add_test(NAME warning_policy COMMAND ${CMAKE_COMMAND}
	"-DPOLICY_FILE=${CMAKE_BINARY_DIR}/warning-policy-$<CONFIG>-CXX.txt"
	"-DCOMPILER_STYLE=${warning_compiler_style}"
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_warning_policy.cmake)
if(PERL_EXECUTABLE)
	add_test(NAME warning_budget_parser COMMAND ${PERL_EXECUTABLE}
		${CMAKE_CURRENT_SOURCE_DIR}/tools/test_warning_budget.pl)
endif()

# Source bytes must match the UTF-8 compiler contract on every platform.
if(PERL_EXECUTABLE)
	add_test(
		NAME source_encoding
		COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_source_encoding.pl
	)
else()
	message(STATUS "perl not found: the source_encoding test is not registered")
endif()

#----------------------------------------------------------------------
# Format-arity audit (tests/tools/check_format_arity.pl) - the check the
# checked formatter cannot make for itself.
#
# SafeFormat::Format makes a data-file format string safe, but it cannot
# tell that the entry MEANS what its call site passes: an entry with one
# %s more than the call site hands over is no longer a memory-safety
# defect, but it now prints a literal "%s" in the user interface. This
# compares every converted site against the built-in English table -
# which is not a sample, since InitGameStringTable installs it over the
# file data on the English path. Needs perl (found above); without it
# the check is skipped, not passed.
#----------------------------------------------------------------------
if(PERL_EXECUTABLE)
	add_test(
		NAME format_arity
		COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_format_arity.pl
	)
else()
	message(STATUS "perl not found: the format_arity test is not registered")
endif()

#----------------------------------------------------------------------
# Packet-index audit (tests/tools/check_packet_indices.pl) - the other
# half of code-health priority 1.
#
# Ratchet R3 covers the lengths a server sends into a copy. This covers
# the indices it sends into a subscript, and it walks the VALUE rather
# than the spelling: the population is not `array[pPacket->getSlotID()]`
# but a local assigned from it and used twenty lines later. Handlers are
# executable-side, so this is the only kind of check they can have.
#
# The repo root is passed so the script does not have to guess it from
# its own location. Needs perl (found above); without it the check is
# skipped, not passed.
#----------------------------------------------------------------------
if(PERL_EXECUTABLE)
	add_test(
		NAME packet_indices
		COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_packet_indices.pl ${CMAKE_SOURCE_DIR}
	)
else()
	message(STATUS "perl not found: the packet_indices test is not registered")
endif()

#----------------------------------------------------------------------
# The committed WireInventory.inc must match a fresh generation, so a
# new or changed factory cannot go unconstructed and uninventoried. Needs perl (Git for
# Windows ships one, found above); without it the check is skipped,
# not passed.
#----------------------------------------------------------------------
if(PERL_EXECUTABLE)
	add_test(
		NAME wire_inventory_fresh
		COMMAND ${CMAKE_COMMAND}
			-DPERL_EXECUTABLE=${PERL_EXECUTABLE}
			-DSOURCE_DIR=${CMAKE_SOURCE_DIR}
			-P ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_wire_inventory_fresh.cmake
	)
else()
	message(STATUS "perl not found: the wire_inventory_fresh test is not registered")
endif()

#----------------------------------------------------------------------
# Packet-read fuzzing (tests/fuzz/CMakeLists.txt): the replay ctest in
# every native test tree, the libFuzzer targets under BUILD_FUZZERS.
# Below the find_program(perl) above, which its corpus step needs.
#----------------------------------------------------------------------
if(NOT EMSCRIPTEN)
	add_subdirectory(fuzz)
endif()
