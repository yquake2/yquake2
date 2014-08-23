set(YQ2_TARGET "client")

set(YQ2_TARGET_BIN_DIR "${YQ2_BIN_DIR}")

if (YQ2_USE_CDA)
    add_definitions("-DCDA")
endif ()

if (YQ2_USE_OGG)
    add_definitions("-DOGG")
endif ()

if (YQ2_USE_OPENAL)
    if (WIN32)
        set(YQ2_OPENAL_LIB_NAME "OpenAL32.dll")
    elseif (YQ2_IS_OPENBSD)
        set(YQ2_OPENAL_LIB_NAME "libopenal.so")
    elseif (YQ2_IS_DARWIN)
        set(
            YQ2_OPENAL_LIB_NAME
            "/System/Library/Frameworks/OpenAL.framework/OpenAL")
    else ()
        set(YQ2_OPENAL_LIB_NAME "libopenal.so.1")
    endif ()

    add_definitions("-DUSE_OPENAL")
    add_definitions("-DDEFAULT_OPENAL_DRIVER=\"${YQ2_OPENAL_LIB_NAME}\"")
endif ()

if (YQ2_USE_SDL2)
    add_definitions("-DSDL2")
endif ()

if (YQ2_USE_JPEG)
    add_definitions("-DRETEXTURE")
endif ()

if (YQ2_USE_ZLIB)
    add_definitions("-DZLIB" "-DNOUNCRYPT")
endif ()

if (MSVC)
    add_definitions("-Dstrcasecmp=_stricmp")
    add_definitions("-Dsnprintf=_snprintf")
    add_definitions("-D__func__=__FUNCTION__")
    add_definitions("-Dinline=__inline")
endif ()

set(
    YQ2_TARGET_SOURCES
    "../backends/generic/qal.c"
    "../backends/generic/vid.c"
    "../backends/generic/qgl.c"
    "../backends/sdl/cd.c"
    "../backends/sdl/input.c"
    "../backends/sdl/refresh.c"
    "../backends/sdl/sound.c"
    "cl_cin.c"
    "cl_console.c"
    "cl_download.c"
    "cl_effects.c"
    "cl_entities.c"
    "cl_input.c"
    "cl_inventory.c"
    "cl_keyboard.c"
    "cl_lights.c"
    "cl_main.c"
    "cl_network.c"
    "cl_parse.c"
    "cl_particles.c"
    "cl_prediction.c"
    "cl_screen.c"
    "cl_tempentities.c"
    "cl_view.c"
    "refresh/r_draw.c"
    "refresh/r_image.c"
    "refresh/r_light.c"
    "refresh/r_lightmap.c"
    "refresh/r_main.c"
    "refresh/r_mesh.c"
    "refresh/r_misc.c"
    "refresh/r_model.c"
    "refresh/r_scrap.c"
    "refresh/r_surf.c"
    "refresh/r_warp.c"
    "refresh/files/md2.c"
    "refresh/files/pcx.c"
    "refresh/files/sp2.c"
    "refresh/files/tga.c"
    "refresh/files/jpeg.c"
    "refresh/files/wal.c"
    "menu/menu.c"
    "menu/qmenu.c"
    "menu/videomenu.c"
    "sound/ogg.c"
    "sound/openal.c"
    "sound/sound.c"
    "sound/wave.c"
    "../common/argproc.c"
    "../common/clientserver.c"
    "../common/collision.c"
    "../common/crc.c"
    "../common/cmdparser.c"
    "../common/cvar.c"
    "../common/filesystem.c"
    "../common/glob.c"
    "../common/md4.c"
    "../common/movemsg.c"
    "../common/misc.c"
    "../common/netchan.c"
    "../common/pmove.c"
    "../common/szone.c"
    "../common/zone.c"
    "../common/shared/flash.c"
    "../common/shared/rand.c"
    "../common/shared/shared.c"
    "../server/sv_cmd.c"
    "../server/sv_conless.c"
    "../server/sv_entities.c"
    "../server/sv_game.c"
    "../server/sv_init.c"
    "../server/sv_main.c"
    "../server/sv_save.c"
    "../server/sv_send.c"
    "../server/sv_user.c"
    "../server/sv_world.c"
)

if (WIN32)
    set(
        YQ2_TARGET_SOURCES
        ${YQ2_TARGET_SOURCES}
        "../backends/windows/network.c"
        "../backends/windows/system.c"
        "../backends/windows/shared/mem.c"
        "../../stuff/icon/icon.rc"
        "../../stuff/icon/Quake2.ico"
    )
else ()
    set(
        YQ2_TARGET_SOURCES
        ${YQ2_TARGET_SOURCES}
        "../backends/unix/main.c"
        "../backends/unix/network.c"
        "../backends/unix/signalhandler.c"
        "../backends/unix/system.c"
        "../backends/unix/shared/hunk.c"
    )
endif ()

if (YQ2_USE_ZLIB)
    set(
        YQ2_TARGET_SOURCES
        ${YQ2_TARGET_SOURCES}
        "../common/unzip/ioapi.c"
        "../common/unzip/unzip.c"
    )
endif ()

include_directories(
    "${SDL_INCLUDE_DIR}"
)

if (YQ2_USE_OGG)
    include_directories(
        "${YQ2_OGG_INCLUDE_DIR}"
        "${YQ2_VORBIS_INCLUDE_DIR}"
    )
endif ()

if (WIN32)
    include_directories(
        "../.."
    )
endif ()

if (YQ2_USE_JPEG)
    include_directories(
        "${JPEG_INCLUDE_DIR}"
    )
endif ()

if (YQ2_USE_OPENAL)
    include_directories(
        "${OPENAL_INCLUDE_DIR}"
    )
endif ()

if (YQ2_USE_ZLIB)
    include_directories(
        "${ZLIB_INCLUDE_DIR}"
    )
endif ()

add_executable(
    "${YQ2_TARGET}"
    ${YQ2_TARGET_SOURCES}
)

target_link_libraries(
    "${YQ2_TARGET}"
    "${SDL_LIBRARY}"
)

if (YQ2_USE_JPEG)
    target_link_libraries(
        "${YQ2_TARGET}"
        "${JPEG_LIBRARY}"
    )
endif ()

if (YQ2_USE_OGG)
    target_link_libraries(
        "${YQ2_TARGET}"
        "${YQ2_VORBISFILE_LIBRARY}"
        "${YQ2_VORBIS_LIBRARY}"
        "${YQ2_OGG_LIBRARY}"
    )
endif ()

if (YQ2_USE_OPENAL)
    target_link_libraries(
        "${YQ2_TARGET}"
        "${OPENAL_LIBRARY}"
    )
endif ()

if (YQ2_USE_ZLIB)
    target_link_libraries(
        "${YQ2_TARGET}"
        "${ZLIB_LIBRARY}"
    )
endif ()

if (WIN32)
    target_link_libraries(
        "${YQ2_TARGET}"
        "opengl32"
        "winmm"
        "ws2_32"
    )

    if (YQ2_USE_SDL2)
        target_link_libraries(
            "${YQ2_TARGET}"
            "${SDLMAIN_LIBRARY}"
            "imm32"
            "version"
        )
    endif ()
endif ()

set_target_properties(
    "${YQ2_TARGET}"
    PROPERTIES
    OUTPUT_NAME "quake2"
    RUNTIME_OUTPUT_DIRECTORY "${YQ2_TARGET_BIN_DIR}"
)

if (MSVC)
    if (YQ2_USE_STATIC_LIBS)
        set_target_properties(
            "${YQ2_TARGET}"
            PROPERTIES
            LINK_FLAGS_DEBUG "/NODEFAULTLIB:LIBCMT.lib"
            LINK_FLAGS_RELEASE "/NODEFAULTLIB:LIBCMTD.lib"
            LINK_FLAGS_MINSIZEREL "/NODEFAULTLIB:LIBCMTD.lib"
            LINK_FLAGS_RELWITHDEBINFO "/NODEFAULTLIB:LIBCMTD.lib"
        )
    endif ()
endif ()

if (YQ2_IS_GCC)
    if (YQ2_USE_STATIC_LIBS)
        set_target_properties(
            "${YQ2_TARGET}"
            PROPERTIES
            LINK_FLAGS "-static"
        )
    endif ()
endif ()

if (WIN32)
    set_target_properties(
        "${YQ2_TARGET}"
        PROPERTIES
        WIN32_EXECUTABLE "TRUE"
    )
endif ()
