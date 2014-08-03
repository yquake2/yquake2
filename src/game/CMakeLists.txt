set(YQ2_TARGET "baseq2")

set(YQ2_TARGET_BIN_DIR "${YQ2_BIN_DIR}/baseq2")

set(
    YQ2_TARGET_SOURCES
    "../common/shared/flash.c"
    "../common/shared/rand.c"
    "../common/shared/shared.c"
    "g_ai.c"
    "g_chase.c"
    "g_cmds.c"
    "g_combat.c"
    "g_func.c"
    "g_items.c"
    "g_main.c"
    "g_misc.c"
    "g_monster.c"
    "g_phys.c"
    "g_spawn.c"
    "g_svcmds.c"
    "g_target.c"
    "g_trigger.c"
    "g_turret.c"
    "g_utils.c"
    "g_weapon.c"
    "monster/berserker/berserker.c"
    "monster/boss2/boss2.c"
    "monster/boss3/boss3.c"
    "monster/boss3/boss31.c"
    "monster/boss3/boss32.c"
    "monster/brain/brain.c"
    "monster/chick/chick.c"
    "monster/flipper/flipper.c"
    "monster/float/float.c"
    "monster/flyer/flyer.c"
    "monster/gladiator/gladiator.c"
    "monster/gunner/gunner.c"
    "monster/hover/hover.c"
    "monster/infantry/infantry.c"
    "monster/insane/insane.c"
    "monster/medic/medic.c"
    "monster/misc/move.c"
    "monster/mutant/mutant.c"
    "monster/parasite/parasite.c"
    "monster/soldier/soldier.c"
    "monster/supertank/supertank.c"
    "monster/tank/tank.c"
    "player/client.c"
    "player/hud.c"
    "player/trail.c"
    "player/view.c"
    "player/weapon.c"
    "savegame/savegame.c"
)

if (MSVC)
    add_definitions("-Dstrcasecmp=_stricmp")
endif ()

add_library(
    "${YQ2_TARGET}"
    SHARED
    ${YQ2_TARGET_SOURCES}
)

set_target_properties(
    "${YQ2_TARGET}"
    PROPERTIES
    PREFIX ""
    OUTPUT_NAME "game"
    RUNTIME_OUTPUT_DIRECTORY "${YQ2_TARGET_BIN_DIR}"
)

if (YQ2_IS_GCC)
    if (YQ2_USE_STATIC_LIBS)
        set_target_properties(
            "${YQ2_TARGET}"
            PROPERTIES
            LINK_FLAGS "-static"
        )
    endif ()
endif ()
