project("yquake2")

cmake_minimum_required(VERSION 2.8)

set(YQ2_BIN_DIR "${CMAKE_BINARY_DIR}/bin")

if (CMAKE_SYSTEM_NAME MATCHES "kOpenBSD.*|OpenBSD.*")
    set(YQ2_IS_OPENBSD "TRUE")
elseif (CMAKE_SYSTEM_NAME MATCHES ".*Darwin.*")
    set(YQ2_IS_DARWIN "TRUE")
endif ()

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(YQ2_IS_GCC "TRUE")
endif ()

option(YQ2_USE_CDA "Use CD audio." OFF)
option(YQ2_USE_JPEG "Use JPEG for retexturing." ON)
option(YQ2_USE_OGG "Use Ogg/Vorbis playback." ON)
option(YQ2_USE_OPENAL "Use OpenAL." ON)
option(YQ2_USE_SDL2 "Use SDL2." ON)
option(YQ2_USE_ZLIB "Use ZLib." ON)
option(YQ2_USE_STATIC_LIBS "Use static libraries." ON)

if (YQ2_USE_CDA AND YQ2_USE_SDL2)
    set(YQ2_USE_CDA "OFF")
endif ()

if (YQ2_USE_JPEG)
    find_package(JPEG 8 REQUIRED)
endif ()

if (YQ2_USE_OGG)
    set(
        YQ2_OGG_INCLUDE_DIR
        "OGG-HEADERS-NOT-FOUND"
        CACHE PATH
        "Path to Ogg headers.")

    set(
        YQ2_OGG_LIBRARY
        "OGG-LIBRARY-NOT-FOUND"
        CACHE PATH
        "Path to Ogg library.")

    set(
        YQ2_VORBIS_INCLUDE_DIR
        "VORBIS-HEADERS-NOT-FOUND"
        CACHE PATH
        "Path to Vorbis headers.")

    set(
        YQ2_VORBIS_LIBRARY
        "VORBIS-LIBRARY-NOT-FOUND"
        CACHE PATH
        "Path to Vorbis library.")

    set(
        YQ2_VORBISFILE_LIBRARY
        "VORBISFILE-LIBRARY-NOT-FOUND"
        CACHE PATH
        "Path to Vorbisfile library.")
endif ()

if (YQ2_USE_OPENAL)
    find_package(OpenAL REQUIRED)
endif ()

if (YQ2_USE_SDL2)
    find_package(SDL 2 REQUIRED)
else ()
    find_package(SDL 1.2 REQUIRED)
endif ()

if (YQ2_USE_ZLIB)
    find_package(ZLIB 1.2 REQUIRED)
endif ()

if (YQ2_USE_STATIC_LIBS)
    if (MSVC)
        add_compile_options("$<$<CONFIG:DEBUG>:/MTd>")
        add_compile_options("$<$<NOT:$<CONFIG:DEBUG>>:/MT>")
    endif ()
endif ()

if (YQ2_IS_GCC)
    add_compile_options(
        "-fno-strict-aliasing"
        "-fomit-frame-pointer"
        "-Wall"
    )
endif ()

add_subdirectory("src/client")
add_subdirectory("src/game")
