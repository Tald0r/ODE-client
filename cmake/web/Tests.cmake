# The same pixel oracle runs natively and in a real browser WebGL context.
add_executable(web_sprite_tests
    tests/render/main.cpp tests/render/test_sprite_gpu.cpp tests/render/benchmark.cpp
    tests/framework/test_framework.cpp)
target_include_directories(web_sprite_tests PRIVATE tests/framework)
target_link_libraries(web_sprite_tests PRIVATE SpriteLib darkeden_xbrz)
set_target_properties(web_sprite_tests PROPERTIES SUFFIX ".html")
target_link_options(web_sprite_tests PRIVATE -sASSERTIONS=1 -sEXIT_RUNTIME=1)

# The vendored server rules' parity vectors (third_party/decore), asserted
# in WebAssembly: web.yml runs this under node. The vectors are embedded, so
# the .js and .wasm pair runs wherever it is copied. tests/CMakeLists.txt
# defines the native decore_tests and skips it under Emscripten.
file(GLOB _decore_test_sources CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/decore/*.cpp)
add_executable(decore_tests
    tests/framework/test_main.cpp tests/framework/test_framework.cpp ${_decore_test_sources})
target_include_directories(decore_tests PRIVATE tests/framework)
target_link_libraries(decore_tests PRIVATE decore)
target_compile_definitions(decore_tests PRIVATE "DECORE_VECTOR_DIR=\"/decore/vectors\"")
target_link_options(decore_tests PRIVATE -sASSERTIONS=1 -sEXIT_RUNTIME=1
    "--embed-file=${CMAKE_SOURCE_DIR}/third_party/decore/domain/vectors@/decore/vectors")
# The embedded rows are a link input the build cannot see: relink on a resync.
file(GLOB _decore_vectors CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/third_party/decore/domain/vectors/*)
set_property(TARGET decore_tests APPEND PROPERTY LINK_DEPENDS ${_decore_vectors})
