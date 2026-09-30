#----------------------------------------------------------------------
# Packet-read fuzz targets
#----------------------------------------------------------------------
#
# Three libFuzzer targets over bytes the client receives. Two read what
# a server sends, one per connection of the client's ClientPlayer, both
# through client_stream_reader.h, which says what they mirror; the third
# reads one UDP datagram from anyone who can reach the client's
# peer-to-peer port, as ClientCommunicationManager::Update does:
#
#   client_stream        fuzz_client_stream.cpp, the game connection
#   client_login_stream  fuzz_client_login_stream.cpp, the login
#                        connection
#   client_datagram      fuzz_client_datagram.cpp, the UDP socket
#
# Each is built two ways:
#
#   fuzz_replay_<target>  the target with replay_main.cpp, in every
#                         native BUILD_TESTS tree and on every
#                         compiler. The ctest of the same name runs
#                         the seed corpus and the committed crash
#                         regressions through it. A finding that
#                         aborted (a libc assert, an escaping
#                         std::exception) fails in every tree; one
#                         that corrupted memory fails only where a
#                         sanitizer sees it (an ASan tree, and Clang's
#                         UBSan for an invalid enum load), and the
#                         unit tests in test_packet_fuzz_findings.cpp
#                         are what catch it everywhere.
#   fuzz_<target>         the target with libFuzzer's main, only under
#                         BUILD_FUZZERS (the linux-fuzz and macos-fuzz
#                         presets; the top-level CMakeLists.txt checks
#                         the compiler).
#
# Not built under Emscripten (tests/CMakeLists.txt): nothing there runs
# a native ctest.
#----------------------------------------------------------------------

set(FUZZ_SUPPORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../support)

#----------------------------------------------------------------------
# The replay ctest. Its inputs are the seed corpus, written into the
# build tree by tools/fuzz/golden2corpus.pl from tests/golden and
# tests/wire-layout.txt (the fuzz_corpus_<target> setup step, so the
# corpus follows the goldens without being committed twice), and
# tests/fuzz/regressions/<target>: one .hex per fixed finding, the
# crash input as found. Run in the build tree, since a failed Assert
# appends to assertion_failed.log in the working directory. Needs perl
# (found in tests/CMakeLists.txt); without it the check is skipped, not
# passed.
#
#   darkeden_fuzz_target(<target> <source> <golden name regex> [DATAGRAM])
#
# DATAGRAM seeds the target from the .datagram. goldens, one raw
# datagram per seed (golden2corpus.pl's --datagram mode); without it the
# seeds are stream inputs built from the other goldens and the layout.
#----------------------------------------------------------------------
function(darkeden_fuzz_target target source name_regex)
	add_executable(fuzz_replay_${target}
		replay_main.cpp
		${source}
	)
	target_include_directories(fuzz_replay_${target} PRIVATE ${FUZZ_SUPPORT_DIR})
	target_link_libraries(fuzz_replay_${target} PRIVATE packetwire)

	if(BUILD_FUZZERS)
		add_executable(fuzz_${target} ${source})
		target_include_directories(fuzz_${target} PRIVATE ${FUZZ_SUPPORT_DIR})
		target_link_libraries(fuzz_${target} PRIVATE packetwire)
		if(DARKEDEN_LIBFUZZER_ARCHIVE)
			# A libFuzzer runtime from another LLVM than the compiler's
			# own (Apple Clang ships none; see CMakePresets.json's
			# macos-fuzz).
			target_link_libraries(fuzz_${target} PRIVATE ${DARKEDEN_LIBFUZZER_ARCHIVE})
		else()
			target_link_options(fuzz_${target} PRIVATE -fsanitize=fuzzer)
		endif()
	endif()

	if(PERL_EXECUTABLE)
		set(corpus ${CMAKE_CURRENT_BINARY_DIR}/corpus/${target})
		if(ARGV3 STREQUAL "DATAGRAM")
			add_test(
				NAME fuzz_corpus_${target}
				COMMAND ${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/fuzz/golden2corpus.pl
					--datagram
					${CMAKE_SOURCE_DIR}/tests/golden
					${corpus}
					${name_regex}
			)
		else()
			add_test(
				NAME fuzz_corpus_${target}
				COMMAND ${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/fuzz/golden2corpus.pl
					${CMAKE_SOURCE_DIR}/tests/golden
					${CMAKE_SOURCE_DIR}/tests/wire-layout.txt
					${corpus}
					${name_regex}
			)
		endif()
		set_tests_properties(fuzz_corpus_${target} PROPERTIES
			FIXTURES_SETUP fuzz_${target}_corpus)

		add_test(
			NAME fuzz_replay_${target}
			COMMAND fuzz_replay_${target}
				${corpus}
				${CMAKE_CURRENT_SOURCE_DIR}/regressions/${target}
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		)
		set_tests_properties(fuzz_replay_${target} PROPERTIES
			FIXTURES_REQUIRED fuzz_${target}_corpus)
	else()
		message(STATUS "perl not found: the fuzz_replay_${target} test is not registered")
	endif()
endfunction()

# The game connection: GC packets. The login connection: LC packets.
# The UDP socket: the RC datagram packets.
darkeden_fuzz_target(client_stream fuzz_client_stream.cpp ^GC)
darkeden_fuzz_target(client_login_stream fuzz_client_login_stream.cpp ^LC)
darkeden_fuzz_target(client_datagram fuzz_client_datagram.cpp ^RC DATAGRAM)
