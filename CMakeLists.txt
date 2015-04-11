cmake_minimum_required(VERSION 3.0)
#yquake cmake configuration
project (yquake2)

set(YQUAKE2_MAJOR_VERSION 0)
set(YQUAKE2_MINOR_VERSION 0)
set(YQUAKE2_PATCH_VERSION 0)

# Cmake module search path
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake/modules
                      ${CMAKE_MODULE_PATH})

#These variables will act as our list of include folders and linker flags
set(yquake2LinkerFlags)
set(yquake2IncludeDirectories)
set(yquake2LinkerDirectories)

#Set directory locations (allowing us to move directories easily)
set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
set(BACKENDS_SRC_DIR ${SOURCE_DIR}/backends)
set(COMMON_SRC_DIR ${SOURCE_DIR}/common)
set(GAME_SRC_DIR ${SOURCE_DIR}/game)
set(SERVER_SRC_DIR ${SOURCE_DIR}/server)
set(CLIENT_SRC_DIR ${SOURCE_DIR}/client)

#Required libraries to build the different
#components of the tutorials. Find them and add the include/linker
#directories and flags(In case the package manager find it in a weird place)
find_package(SDL2 REQUIRED)
add_definitions(-DSDL2)
list(APPEND yquake2IncludeDirectories ${SDL2_INCLUDE_DIR})
list(APPEND yquake2LinkerFlags ${SDL2_LIBRARY})

find_package(OpenGL REQUIRED)
list(APPEND yquake2IncludeDirectories ${OPENGL_INCLUDE_DIR})
list(APPEND yquake2LinkerFlags ${OPENGL_LIBRARIES})

find_package(ZLIB REQUIRED)
list(APPEND yquake2IncludeDirectories ${ZLIB_INCLUDE_DIRS})
list(APPEND yquake2LinkerFlags ${ZLIB_LIBRARIES})

find_package(OpenAL REQUIRED)
add_definitions(-DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER)
list(APPEND yquake2IncludeDirectories ${OPENAL_INCLUDE_DIR})
list(APPEND yquake2LinkerFlags ${OPENAL_LIBRARY})

list(APPEND yquake2LinkerFlags "-L/usr/lib -lm -ldl -rdynamic")

add_definitions(-DZIP -DNOUNCRYPT)

#With all of those libraries and user defined paths added,
#lets give them to the compiler and linker.
include_directories(${yquake2IncludeDirectories})
link_directories(${yquake2LinkerDirectories})


set(Backends-Unix-Source
	${BACKENDS_SRC_DIR}/unix/main.c
	${BACKENDS_SRC_DIR}/unix/network.c
	${BACKENDS_SRC_DIR}/unix/signalhandler.c
	${BACKENDS_SRC_DIR}/unix/system.c
	${BACKENDS_SRC_DIR}/unix/shared/hunk.c
	)

set(Backends-Windows-Source
	${BACKENDS_SRC_DIR}/windows/network.c
	${BACKENDS_SRC_DIR}/windows/system.c
	${BACKENDS_SRC_DIR}/windows/shared/mem.c
	)

#Set the nessesary platform specific source
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(Platform-Specific-Source ${Backends-Windows-Source})
else()
	set(Platform-Specific-Source ${Backends-Unix-Source})
endif()

set(Game-Source
	${COMMON_SRC_DIR}/shared/flash.c
	${COMMON_SRC_DIR}/shared/rand.c
	${COMMON_SRC_DIR}/shared/shared.c
	${GAME_SRC_DIR}/g_ai.c
	${GAME_SRC_DIR}/g_chase.c
	${GAME_SRC_DIR}/g_cmds.c
	${GAME_SRC_DIR}/g_combat.c
	${GAME_SRC_DIR}/g_func.c
	${GAME_SRC_DIR}/g_items.c
	${GAME_SRC_DIR}/g_main.c
	${GAME_SRC_DIR}/g_misc.c
	${GAME_SRC_DIR}/g_monster.c
	${GAME_SRC_DIR}/g_phys.c
	${GAME_SRC_DIR}/g_spawn.c
	${GAME_SRC_DIR}/g_svcmds.c
	${GAME_SRC_DIR}/g_target.c
	${GAME_SRC_DIR}/g_trigger.c
	${GAME_SRC_DIR}/g_turret.c
	${GAME_SRC_DIR}/g_utils.c
	${GAME_SRC_DIR}/g_weapon.c
	${GAME_SRC_DIR}/monster/berserker/berserker.c
	${GAME_SRC_DIR}/monster/boss2/boss2.c
	${GAME_SRC_DIR}/monster/boss3/boss3.c
	${GAME_SRC_DIR}/monster/boss3/boss31.c
	${GAME_SRC_DIR}/monster/boss3/boss32.c
	${GAME_SRC_DIR}/monster/brain/brain.c
	${GAME_SRC_DIR}/monster/chick/chick.c
	${GAME_SRC_DIR}/monster/flipper/flipper.c
	${GAME_SRC_DIR}/monster/float/float.c
	${GAME_SRC_DIR}/monster/flyer/flyer.c
	${GAME_SRC_DIR}/monster/gladiator/gladiator.c
	${GAME_SRC_DIR}/monster/gunner/gunner.c
	${GAME_SRC_DIR}/monster/hover/hover.c
	${GAME_SRC_DIR}/monster/infantry/infantry.c
	${GAME_SRC_DIR}/monster/insane/insane.c
	${GAME_SRC_DIR}/monster/medic/medic.c
	${GAME_SRC_DIR}/monster/misc/move.c
	${GAME_SRC_DIR}/monster/mutant/mutant.c
	${GAME_SRC_DIR}/monster/parasite/parasite.c
	${GAME_SRC_DIR}/monster/soldier/soldier.c
	${GAME_SRC_DIR}/monster/supertank/supertank.c
	${GAME_SRC_DIR}/monster/tank/tank.c
	${GAME_SRC_DIR}/player/client.c
	${GAME_SRC_DIR}/player/hud.c
	${GAME_SRC_DIR}/player/trail.c
	${GAME_SRC_DIR}/player/view.c
	${GAME_SRC_DIR}/player/weapon.c
	${GAME_SRC_DIR}/savegame/savegame.c
	)

set(Client-Source
	${BACKENDS_SRC_DIR}/generic/qal.c
	${BACKENDS_SRC_DIR}/generic/vid.c
 	${BACKENDS_SRC_DIR}/generic/qgl.c
	${BACKENDS_SRC_DIR}/sdl/cd.c
	${BACKENDS_SRC_DIR}/sdl/input.c
	${BACKENDS_SRC_DIR}/sdl/refresh.c
	${BACKENDS_SRC_DIR}/sdl/sound.c
	${CLIENT_SRC_DIR}/cl_cin.c
	${CLIENT_SRC_DIR}/cl_console.c
	${CLIENT_SRC_DIR}/cl_download.c
	${CLIENT_SRC_DIR}/cl_effects.c
	${CLIENT_SRC_DIR}/cl_entities.c
	${CLIENT_SRC_DIR}/cl_input.c
	${CLIENT_SRC_DIR}/cl_inventory.c
	${CLIENT_SRC_DIR}/cl_keyboard.c
	${CLIENT_SRC_DIR}/cl_lights.c
	${CLIENT_SRC_DIR}/cl_main.c
	${CLIENT_SRC_DIR}/cl_network.c
	${CLIENT_SRC_DIR}/cl_parse.c
	${CLIENT_SRC_DIR}/cl_particles.c
	${CLIENT_SRC_DIR}/cl_prediction.c
	${CLIENT_SRC_DIR}/cl_screen.c
	${CLIENT_SRC_DIR}/cl_tempentities.c
	${CLIENT_SRC_DIR}/cl_view.c
	${CLIENT_SRC_DIR}/refresh/r_draw.c
	${CLIENT_SRC_DIR}/refresh/r_image.c
	${CLIENT_SRC_DIR}/refresh/r_light.c
	${CLIENT_SRC_DIR}/refresh/r_lightmap.c
	${CLIENT_SRC_DIR}/refresh/r_main.c
	${CLIENT_SRC_DIR}/refresh/r_mesh.c
	${CLIENT_SRC_DIR}/refresh/r_misc.c
	${CLIENT_SRC_DIR}/refresh/r_model.c
	${CLIENT_SRC_DIR}/refresh/r_scrap.c
	${CLIENT_SRC_DIR}/refresh/r_surf.c
	${CLIENT_SRC_DIR}/refresh/r_warp.c
	${CLIENT_SRC_DIR}/refresh/files/md2.c
	${CLIENT_SRC_DIR}/refresh/files/pcx.c
	${CLIENT_SRC_DIR}/refresh/files/sp2.c
	${CLIENT_SRC_DIR}/refresh/files/stb.c
	${CLIENT_SRC_DIR}/refresh/files/wal.c
	${CLIENT_SRC_DIR}/menu/menu.c
	${CLIENT_SRC_DIR}/menu/qmenu.c
	${CLIENT_SRC_DIR}/menu/videomenu.c
	${CLIENT_SRC_DIR}/sound/ogg.c
	${CLIENT_SRC_DIR}/sound/openal.c
	${CLIENT_SRC_DIR}/sound/sound.c
	${CLIENT_SRC_DIR}/sound/wave.c
	${COMMON_SRC_DIR}/argproc.c
	${COMMON_SRC_DIR}/clientserver.c
	${COMMON_SRC_DIR}/collision.c
	${COMMON_SRC_DIR}/crc.c
	${COMMON_SRC_DIR}/cmdparser.c
	${COMMON_SRC_DIR}/cvar.c
	${COMMON_SRC_DIR}/filesystem.c
	${COMMON_SRC_DIR}/glob.c
	${COMMON_SRC_DIR}/md4.c
	${COMMON_SRC_DIR}/movemsg.c
	${COMMON_SRC_DIR}/misc.c
	${COMMON_SRC_DIR}/netchan.c
	${COMMON_SRC_DIR}/pmove.c
	${COMMON_SRC_DIR}/szone.c
	${COMMON_SRC_DIR}/zone.c
	${COMMON_SRC_DIR}/shared/flash.c
	${COMMON_SRC_DIR}/shared/rand.c
	${COMMON_SRC_DIR}/shared/shared.c
	${COMMON_SRC_DIR}/unzip/ioapi.c
	${COMMON_SRC_DIR}/unzip/unzip.c
	${SERVER_SRC_DIR}/sv_cmd.c
	${SERVER_SRC_DIR}/sv_conless.c
	${SERVER_SRC_DIR}/sv_entities.c
	${SERVER_SRC_DIR}/sv_game.c
	${SERVER_SRC_DIR}/sv_init.c
	${SERVER_SRC_DIR}/sv_main.c
	${SERVER_SRC_DIR}/sv_save.c
	${SERVER_SRC_DIR}/sv_send.c
	${SERVER_SRC_DIR}/sv_user.c
	${SERVER_SRC_DIR}/sv_world.c
	)

set(Server-Source
	${COMMON_SRC_DIR}/argproc.c
	${COMMON_SRC_DIR}/clientserver.c
	${COMMON_SRC_DIR}/collision.c
	${COMMON_SRC_DIR}/crc.c
	${COMMON_SRC_DIR}/cmdparser.c
	${COMMON_SRC_DIR}/cvar.c
	${COMMON_SRC_DIR}/filesystem.c
	${COMMON_SRC_DIR}/glob.c
	${COMMON_SRC_DIR}/md4.c
	${COMMON_SRC_DIR}/misc.c
	${COMMON_SRC_DIR}/movemsg.c
	${COMMON_SRC_DIR}/netchan.c
	${COMMON_SRC_DIR}/pmove.c
	${COMMON_SRC_DIR}/szone.c
	${COMMON_SRC_DIR}/zone.c
	${COMMON_SRC_DIR}/shared/rand.c
	${COMMON_SRC_DIR}/shared/shared.c
	${COMMON_SRC_DIR}/unzip/ioapi.c
	${COMMON_SRC_DIR}/unzip/unzip.c
	${SERVER_SRC_DIR}/sv_cmd.c
	${SERVER_SRC_DIR}/sv_conless.c
	${SERVER_SRC_DIR}/sv_entities.c
	${SERVER_SRC_DIR}/sv_game.c
	${SERVER_SRC_DIR}/sv_init.c
	${SERVER_SRC_DIR}/sv_main.c
	${SERVER_SRC_DIR}/sv_save.c
	${SERVER_SRC_DIR}/sv_send.c
	${SERVER_SRC_DIR}/sv_user.c
	${SERVER_SRC_DIR}/sv_world.c
	)

#Build game.so
add_library(game SHARED ${Game-Source})
set_target_properties(game PROPERTIES 
						   PREFIX ""
						   LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/baseq2")
target_link_libraries(game ${yquake2LinkerFlags})

add_executable(quake2 ${Client-Source} ${Platform-Specific-Source})
target_link_libraries(quake2 ${yquake2LinkerFlags})

add_executable(q2ded ${Server-Source} ${Platform-Specific-Source})
set_target_properties(q2ded PROPERTIES
							COMPILE_DEFINITIONS "DEDICATED_ONLY")
target_link_libraries(q2ded ${yquake2LinkerFlags})