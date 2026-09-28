# de-core: the rules the client and the server compute alike, vendored
# byte for byte from the server's src/domain by tools/decore/sync.pl (see
# README.md). This file is the client's own; nothing under domain/ is.
#
# The source list is explicit, not a glob, so a new server file joins the
# build only when a sync adds it to MANIFEST and this list names it; the
# decore_vendored ctest fails while the two disagree.
add_library(decore STATIC
	domain/Formulas.cpp
	domain/ItemDurability.cpp
	domain/ItemPrice.cpp
)

# Consumers include "domain/<Header>.h". The target's own include path is
# pinned to this directory, as the server pins de-core's, so the copy
# cannot quietly start including a client header.
target_include_directories(decore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(decore PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

# The same source must give the same numbers as the server's build: no
# fused multiply-add and no fast-math reassociation. MSVC's default
# /fp:precise does not contract, and a bare -f flag there would be D9002.
if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(decore PRIVATE /clang:-ffp-contract=off)
elseif(NOT MSVC)
	target_compile_options(decore PRIVATE -ffp-contract=off -fno-fast-math)
endif()
