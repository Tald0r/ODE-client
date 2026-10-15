include_guard(GLOBAL)

# Emscripten builds these pinned SDK ports for WebAssembly. Finding native SDL
# packages here would put host libraries on the browser link line.
set(_web_ports
    --use-port=sdl2
    --use-port=sdl2_image:formats=bmp,png,jpg
    --use-port=sdl2_ttf
    --use-port=sdl2_mixer:formats=ogg,mp3
    --use-port=libjpeg)
# Seed SDL's nested port builds before parallel project compilation starts.
file(WRITE "${CMAKE_BINARY_DIR}/web-ports.c" "int darkeden_web_ports;\n")
execute_process(COMMAND "${CMAKE_C_COMPILER}" ${_web_ports}
    -c "${CMAKE_BINARY_DIR}/web-ports.c" -o "${CMAKE_BINARY_DIR}/web-ports.o"
    RESULT_VARIABLE _web_ports_result OUTPUT_VARIABLE _web_ports_output ERROR_VARIABLE _web_ports_error)
if(NOT _web_ports_result EQUAL 0)
    message(FATAL_ERROR "Cannot build Emscripten ports:\n${_web_ports_output}\n${_web_ports_error}")
endif()
# Protocol and resource loaders depend on catching C++ exceptions.
add_compile_options(${_web_ports} -fexceptions)
add_link_options(${_web_ports} -fexceptions
    -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=2147483648
    -sSTACK_SIZE=4194304 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2)

foreach(_sdl SDL2 SDL2_image SDL2_ttf SDL2_mixer)
    add_library(${_sdl}::${_sdl} INTERFACE IMPORTED GLOBAL)
    set(${_sdl}_FOUND TRUE)
endforeach()
set(SDL2_MIXER_FOUND TRUE)

# musl's iconv cannot encode the game's legacy CJK wire/resource formats.
# Use the same GNU implementation used by the native Windows build.
include(ExternalProject)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
find_program(_web_make make REQUIRED)
find_program(_web_emconfigure emconfigure REQUIRED)
set(_iconv_install "${CMAKE_BINARY_DIR}/web-iconv")
file(MAKE_DIRECTORY "${_iconv_install}/include")
ExternalProject_Add(web_iconv
    URL https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.18.tar.gz
    URL_HASH SHA256=3b08f5f4f9b4eb82f151a7040bfd6fe6c6fb922efe4b1659c66ea933276965e8
    CONFIGURE_COMMAND ${_web_emconfigure} <SOURCE_DIR>/configure
        --host=wasm32-unknown-emscripten --prefix=${_iconv_install}
        --disable-shared --enable-static --disable-nls
    BUILD_COMMAND ${_web_make} -j4
    INSTALL_COMMAND ${_web_make} install
    BUILD_BYPRODUCTS "${_iconv_install}/lib/libiconv.a"
    LOG_CONFIGURE TRUE LOG_BUILD TRUE LOG_INSTALL TRUE)
add_library(Iconv::Iconv STATIC IMPORTED GLOBAL)
set_target_properties(Iconv::Iconv PROPERTIES
    IMPORTED_LOCATION "${_iconv_install}/lib/libiconv.a"
    INTERFACE_INCLUDE_DIRECTORIES "${_iconv_install}/include")
add_dependencies(Iconv::Iconv web_iconv)
set(Iconv_FOUND TRUE)
