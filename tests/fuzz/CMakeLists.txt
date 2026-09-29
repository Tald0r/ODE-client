#----------------------------------------------------------------------
# Packet-read fuzz targets
#----------------------------------------------------------------------
#
# fuzz_client_stream.cpp is a libFuzzer target over the bytes a game
# server sends the client (its header says what it mirrors). It is
# built two ways:
#
#   fuzz_replay_client_stream  the target with replay_main.cpp, in every
#                              native BUILD_TESTS tree and on every
#                              compiler. The ctest of the same name runs
#                              the seed corpus and the committed crash
#                              regressions through it, so a fixed
#                              finding cannot come back unnoticed.
#   fuzz_client_stream         the target with libFuzzer's main, only
#                              under BUILD_FUZZERS (the linux-fuzz and
#                              macos-fuzz presets; the top-level
#                              CMakeLists.txt checks the compiler).
#
# Not built under Emscripten (tests/CMakeLists.txt): nothing there runs
# a native ctest.
#----------------------------------------------------------------------

set(FUZZ_SUPPORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../support)

add_executable(fuzz_replay_client_stream
	replay_main.cpp
	fuzz_client_stream.cpp
)
target_include_directories(fuzz_replay_client_stream PRIVATE ${FUZZ_SUPPORT_DIR})
target_link_libraries(fuzz_replay_client_stream PRIVATE packetwire)

if(BUILD_FUZZERS)
	add_executable(fuzz_client_stream fuzz_client_stream.cpp)
	target_include_directories(fuzz_client_stream PRIVATE ${FUZZ_SUPPORT_DIR})
	target_link_libraries(fuzz_client_stream PRIVATE packetwire)
	if(DARKEDEN_LIBFUZZER_ARCHIVE)
		# A libFuzzer runtime from another LLVM than the compiler's own
		# (Apple Clang ships none; see CMakePresets.json's macos-fuzz).
		target_link_libraries(fuzz_client_stream PRIVATE ${DARKEDEN_LIBFUZZER_ARCHIVE})
	else()
		target_link_options(fuzz_client_stream PRIVATE -fsanitize=fuzzer)
	endif()
endif()

#----------------------------------------------------------------------
# The replay ctest. Its inputs are the seed corpus, written into the
# build tree by tools/fuzz/golden2corpus.pl from tests/golden and
# tests/wire-layout.txt (the fuzz_corpus_client_stream setup step, so
# the corpus follows the goldens without being committed twice), and
# tests/fuzz/regressions/client_stream: one .hex per fixed finding,
# the crash input as found. Run in the build tree, since a failed
# Assert appends to assertion_failed.log in the working directory.
# Needs perl (found in tests/CMakeLists.txt); without it the check is
# skipped, not passed.
#----------------------------------------------------------------------
if(PERL_EXECUTABLE)
	set(FUZZ_CLIENT_STREAM_CORPUS ${CMAKE_CURRENT_BINARY_DIR}/corpus/client_stream)
	add_test(
		NAME fuzz_corpus_client_stream
		COMMAND ${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/fuzz/golden2corpus.pl
			${CMAKE_SOURCE_DIR}/tests/golden
			${CMAKE_SOURCE_DIR}/tests/wire-layout.txt
			${FUZZ_CLIENT_STREAM_CORPUS}
			^GC
	)
	set_tests_properties(fuzz_corpus_client_stream PROPERTIES
		FIXTURES_SETUP fuzz_client_stream_corpus)

	add_test(
		NAME fuzz_replay_client_stream
		COMMAND fuzz_replay_client_stream
			${FUZZ_CLIENT_STREAM_CORPUS}
			${CMAKE_CURRENT_SOURCE_DIR}/regressions/client_stream
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
	set_tests_properties(fuzz_replay_client_stream PROPERTIES
		FIXTURES_REQUIRED fuzz_client_stream_corpus)
else()
	message(STATUS "perl not found: the fuzz_replay_client_stream test is not registered")
endif()
