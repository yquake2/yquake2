/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2011 Yamagi Burmeister
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * Functionpointers to every function in the game.so.
 *
 * =======================================================================
 */
static const fnlist_entry_t fnentries_die[] =
{
	{"button_killed", (byte *)button_killed},
	{"door_killed", (byte *)door_killed},
	{"door_secret_die", (byte *)door_secret_die},
	{"gib_die", (byte *)gib_die},
	{"debris_die", (byte *)debris_die},
	{"func_explosive_explode", (byte *)func_explosive_explode},
	{"barrel_delay", (byte *)barrel_delay},
	{"commander_body_die", (byte *)commander_body_die},
	{"misc_deadsoldier_die", (byte *)misc_deadsoldier_die},
	{"turret_driver_die", (byte *)turret_driver_die},
	{"berserk_die", (byte *)berserk_die},
	{"boss2_die", (byte *)boss2_die},
	{"jorg_die", (byte *)jorg_die},
	{"makron_torso_die", (byte *)makron_torso_die},
	{"makron_die", (byte *)makron_die},
	{"brain_die", (byte *)brain_die},
	{"chick_die", (byte *)chick_die},
	{"flipper_die", (byte *)flipper_die},
	{"floater_die", (byte *)floater_die},
	{"flyer_die", (byte *)flyer_die},
	{"gladiator_die", (byte *)gladiator_die},
	{"gunner_die", (byte *)gunner_die},
	{"hover_die", (byte *)hover_die},
	{"infantry_die", (byte *)infantry_die},
	{"insane_die", (byte *)insane_die},
	{"medic_die", (byte *)medic_die},
	{"mutant_die", (byte *)mutant_die},
	{"parasite_die", (byte *)parasite_die},
	{"soldier_die", (byte *)soldier_die},
	{"supertank_die", (byte *)supertank_die},
	{"tank_die", (byte *)tank_die},
	{"body_die", (byte *)body_die},
	{"player_die", (byte *)player_die},
};
static const functionList_t fnlist_die =
{
	"die",
	fnentries_die, ARREND(fnentries_die),
};

static const fnlist_entry_t fnentries_pain[] =
{
	{"berserk_pain", (byte *)berserk_pain},
	{"boss2_pain", (byte *)boss2_pain},
	{"jorg_pain", (byte *)jorg_pain},
	{"makron_pain", (byte *)makron_pain},
	{"brain_pain", (byte *)brain_pain},
	{"chick_pain", (byte *)chick_pain},
	{"flipper_pain", (byte *)flipper_pain},
	{"floater_pain", (byte *)floater_pain},
	{"flyer_pain", (byte *)flyer_pain},
	{"gladiator_pain", (byte *)gladiator_pain},
	{"gunner_pain", (byte *)gunner_pain},
	{"hover_pain", (byte *)hover_pain},
	{"infantry_pain", (byte *)infantry_pain},
	{"insane_pain", (byte *)insane_pain},
	{"medic_pain", (byte *)medic_pain},
	{"mutant_pain", (byte *)mutant_pain},
	{"parasite_pain", (byte *)parasite_pain},
	{"soldier_pain", (byte *)soldier_pain},
	{"supertank_pain", (byte *)supertank_pain},
	{"tank_pain", (byte *)tank_pain},
	{"player_pain", (byte *)player_pain},
};
static const functionList_t fnlist_pain =
{
	"pain",
	fnentries_pain, ARREND(fnentries_pain),
};

static const fnlist_entry_t fnentries_prethink[] =
{
	{"misc_blackhole_transparent", (byte *)misc_blackhole_transparent},
	{"misc_viper_bomb_prethink", (byte *)misc_viper_bomb_prethink},
};
static const functionList_t fnlist_prethink =
{
	"prethink",
	fnentries_prethink, ARREND(fnentries_prethink),
};

static const fnlist_entry_t fnentries_think[] =
{
	{"Move_Done", (byte *)Move_Done},
	{"Move_Final", (byte *)Move_Final},
	{"Move_Begin", (byte *)Move_Begin},
	{"Think_AccelMove", (byte *)Think_AccelMove},
	{"AngleMove_Done", (byte *)AngleMove_Done},
	{"AngleMove_Final", (byte *)AngleMove_Final},
	{"AngleMove_Begin", (byte *)AngleMove_Begin},
	{"plat_go_down", (byte *)plat_go_down},
	{"wait_and_change_think", (byte *)wait_and_change_think},
	{"button_return", (byte *)button_return},
	{"door_go_down", (byte *)door_go_down},
	{"Think_CalcMoveSpeed", (byte *)Think_CalcMoveSpeed},
	{"Think_SpawnDoorTrigger", (byte *)Think_SpawnDoorTrigger},
	{"train_next", (byte *)train_next},
	{"func_train_find", (byte *)func_train_find},
	{"trigger_elevator_init", (byte *)trigger_elevator_init},
	{"func_timer_think", (byte *)func_timer_think},
	{"door_secret_move2", (byte *)door_secret_move2},
	{"door_secret_move4", (byte *)door_secret_move4},
	{"door_secret_move6", (byte *)door_secret_move6},
	{"G_FreeEdict", (byte *)G_FreeEdict},
	{"DoRespawn", (byte *)DoRespawn},
	{"MegaHealth_think", (byte *)MegaHealth_think},
	{"drop_make_touchable", (byte *)drop_make_touchable},
	{"droptofloor", (byte *)droptofloor},
	{"gib_think", (byte *)gib_think},
	{"TH_viewthing", (byte *)TH_viewthing},
	{"func_object_release", (byte *)func_object_release},
	{"barrel_explode", (byte *)barrel_explode},
	{"M_droptofloor", (byte *)M_droptofloor},
	{"misc_blackhole_think", (byte *)misc_blackhole_think},
	{"misc_eastertank_think", (byte *)misc_eastertank_think},
	{"misc_easterchick_think", (byte *)misc_easterchick_think},
	{"misc_easterchick2_think", (byte *)misc_easterchick2_think},
	{"commander_body_think", (byte *)commander_body_think},
	{"commander_body_drop", (byte *)commander_body_drop},
	{"misc_banner_think", (byte *)misc_banner_think},
	{"misc_satellite_dish_think", (byte *)misc_satellite_dish_think},
	{"func_clock_think", (byte *)func_clock_think},
	{"M_FliesOff", (byte *)M_FliesOff},
	{"M_FliesOn", (byte *)M_FliesOn},
	{"monster_triggered_spawn", (byte *)monster_triggered_spawn},
	{"monster_think", (byte *)monster_think},
	{"walkmonster_start_go", (byte *)walkmonster_start_go},
	{"flymonster_start_go", (byte *)flymonster_start_go},
	{"swimmonster_start_go", (byte *)swimmonster_start_go},
	{"Target_Help_Think", (byte *)Target_Help_Think},
	{"target_explosion_explode", (byte *)target_explosion_explode},
	{"target_crosslevel_target_think", (byte *)target_crosslevel_target_think},
	{"target_laser_think", (byte *)target_laser_think},
	{"target_laser_start", (byte *)target_laser_start},
	{"target_lightramp_think", (byte *)target_lightramp_think},
	{"target_earthquake_think", (byte *)target_earthquake_think},
	{"multi_wait", (byte *)multi_wait},
	{"turret_breach_think", (byte *)turret_breach_think},
	{"turret_breach_finish_init", (byte *)turret_breach_finish_init},
	{"turret_driver_think", (byte *)turret_driver_think},
	{"turret_driver_link", (byte *)turret_driver_link},
	{"Think_Delay", (byte *)Think_Delay},
	{"Grenade_Explode", (byte *)Grenade_Explode},
	{"bfg_explode", (byte *)bfg_explode},
	{"bfg_think", (byte *)bfg_think},
	{"Think_Boss3Stand", (byte *)Think_Boss3Stand},
	{"makron_torso_think", (byte *)makron_torso_think},
	{"MakronSpawn", (byte *)MakronSpawn},
	{"hover_deadthink", (byte *)hover_deadthink},
	{"BossExplode", (byte *)BossExplode},
	{"SP_CreateUnnamedSpawn", (byte *)SP_CreateUnnamedSpawn},
	{"SP_CreateCoopSpots", (byte *)SP_CreateCoopSpots},
	{"SP_FixCoopSpots", (byte *)SP_FixCoopSpots},
};
static const functionList_t fnlist_think =
{
	"think",
	fnentries_think, ARREND(fnentries_think),
};

static const fnlist_entry_t fnentries_touch[] =
{
	{"Touch_Plat_Center", (byte *)Touch_Plat_Center},
	{"rotating_touch", (byte *)rotating_touch},
	{"button_touch", (byte *)button_touch},
	{"Touch_DoorTrigger", (byte *)Touch_DoorTrigger},
	{"door_touch", (byte *)door_touch},
	{"Touch_Item", (byte *)Touch_Item},
	{"drop_temp_touch", (byte *)drop_temp_touch},
	{"gib_touch", (byte *)gib_touch},
	{"path_corner_touch", (byte *)path_corner_touch},
	{"point_combat_touch", (byte *)point_combat_touch},
	{"func_object_touch", (byte *)func_object_touch},
	{"barrel_touch", (byte *)barrel_touch},
	{"misc_viper_bomb_touch", (byte *)misc_viper_bomb_touch},
	{"teleporter_touch", (byte *)teleporter_touch},
	{"Touch_Multi", (byte *)Touch_Multi},
	{"trigger_push_touch", (byte *)trigger_push_touch},
	{"hurt_touch", (byte *)hurt_touch},
	{"trigger_gravity_touch", (byte *)trigger_gravity_touch},
	{"trigger_monsterjump_touch", (byte *)trigger_monsterjump_touch},
	{"blaster_touch", (byte *)blaster_touch},
	{"Grenade_Touch", (byte *)Grenade_Touch},
	{"rocket_touch", (byte *)rocket_touch},
	{"bfg_touch", (byte *)bfg_touch},
	{"mutant_jump_touch", (byte *)mutant_jump_touch},
};
static const functionList_t fnlist_touch =
{
	"touch",
	fnentries_touch, ARREND(fnentries_touch),
};

static const fnlist_entry_t fnentries_use[] =
{
	{"Use_Plat", (byte *)Use_Plat},
	{"rotating_use", (byte *)rotating_use},
	{"button_use", (byte *)button_use},
	{"door_use", (byte *)door_use},
	{"train_use", (byte *)train_use},
	{"trigger_elevator_use", (byte *)trigger_elevator_use},
	{"func_timer_use", (byte *)func_timer_use},
	{"func_conveyor_use", (byte *)func_conveyor_use},
	{"door_secret_use", (byte *)door_secret_use},
	{"use_killbox", (byte *)use_killbox},
	{"Use_Item", (byte *)Use_Item},
	{"Use_Areaportal", (byte *)Use_Areaportal},
	{"light_use", (byte *)light_use},
	{"func_wall_use", (byte *)func_wall_use},
	{"func_object_use", (byte *)func_object_use},
	{"func_explosive_spawn", (byte *)func_explosive_spawn},
	{"func_explosive_use", (byte *)func_explosive_use},
	{"misc_blackhole_use", (byte *)misc_blackhole_use},
	{"commander_body_use", (byte *)commander_body_use},
	{"misc_viper_use", (byte *)misc_viper_use},
	{"misc_viper_bomb_use", (byte *)misc_viper_bomb_use},
	{"misc_strogg_ship_use", (byte *)misc_strogg_ship_use},
	{"misc_satellite_dish_use", (byte *)misc_satellite_dish_use},
	{"target_string_use", (byte *)target_string_use},
	{"func_clock_use", (byte *)func_clock_use},
	{"monster_use", (byte *)monster_use},
	{"monster_triggered_spawn_use", (byte *)monster_triggered_spawn_use},
	{"Use_Target_Tent", (byte *)Use_Target_Tent},
	{"Use_Target_Speaker", (byte *)Use_Target_Speaker},
	{"Use_Target_Help", (byte *)Use_Target_Help},
	{"use_target_secret", (byte *)use_target_secret},
	{"use_target_goal", (byte *)use_target_goal},
	{"use_target_explosion", (byte *)use_target_explosion},
	{"use_target_changelevel", (byte *)use_target_changelevel},
	{"use_target_splash", (byte *)use_target_splash},
	{"use_target_spawner", (byte *)use_target_spawner},
	{"use_target_blaster", (byte *)use_target_blaster},
	{"trigger_crosslevel_trigger_use", (byte *)trigger_crosslevel_trigger_use},
	{"target_laser_use", (byte *)target_laser_use},
	{"target_lightramp_use", (byte *)target_lightramp_use},
	{"target_earthquake_use", (byte *)target_earthquake_use},
	{"Use_Multi", (byte *)Use_Multi},
	{"trigger_enable", (byte *)trigger_enable},
	{"trigger_relay_use", (byte *)trigger_relay_use},
	{"trigger_key_use", (byte *)trigger_key_use},
	{"trigger_counter_use", (byte *)trigger_counter_use},
	{"hurt_use", (byte *)hurt_use},
	{"Use_Boss3", (byte *)Use_Boss3},
};
static const functionList_t fnlist_use =
{
	"use",
	fnentries_use, ARREND(fnentries_use),
};

static const fnlist_entry_t fnentries_blocked[] =
{
	{"plat_blocked", (byte *)plat_blocked},
	{"rotating_blocked", (byte *)rotating_blocked},
	{"door_blocked", (byte *)door_blocked},
	{"train_blocked", (byte *)train_blocked},
	{"door_secret_blocked", (byte *)door_secret_blocked},
	{"turret_blocked", (byte *)turret_blocked},
};
static const functionList_t fnlist_blocked =
{
	"blocked",
	fnentries_blocked, ARREND(fnentries_blocked),
};

static const fnlist_entry_t fnentries_mi_stand[] =
{
	{"infantry_stand", (byte *)infantry_stand},
	{"berserk_stand", (byte *)berserk_stand},
	{"boss2_stand", (byte *)boss2_stand},
	{"jorg_stand", (byte *)jorg_stand},
	{"makron_stand", (byte *)makron_stand},
	{"brain_stand", (byte *)brain_stand},
	{"chick_stand", (byte *)chick_stand},
	{"flipper_stand", (byte *)flipper_stand},
	{"floater_stand", (byte *)floater_stand},
	{"flyer_stand", (byte *)flyer_stand},
	{"gladiator_stand", (byte *)gladiator_stand},
	{"gunner_stand", (byte *)gunner_stand},
	{"hover_stand", (byte *)hover_stand},
	{"insane_stand", (byte *)insane_stand},
	{"medic_stand", (byte *)medic_stand},
	{"mutant_stand", (byte *)mutant_stand},
	{"parasite_stand", (byte *)parasite_stand},
	{"soldier_stand", (byte *)soldier_stand},
	{"supertank_stand", (byte *)supertank_stand},
	{"tank_stand", (byte *)tank_stand},
};
static const functionList_t fnlist_mi_stand =
{
	"mi_stand",
	fnentries_mi_stand, ARREND(fnentries_mi_stand),
};

static const fnlist_entry_t fnentries_mi_walk[] =
{
	{"berserk_walk", (byte *)berserk_walk},
	{"boss2_walk", (byte *)boss2_walk},
	{"jorg_walk", (byte *)jorg_walk},
	{"makron_walk", (byte *)makron_walk},
	{"brain_walk", (byte *)brain_walk},
	{"chick_walk", (byte *)chick_walk},
	{"flipper_walk", (byte *)flipper_walk},
	{"floater_walk", (byte *)floater_walk},
	{"flyer_walk", (byte *)flyer_walk},
	{"gladiator_walk", (byte *)gladiator_walk},
	{"gunner_walk", (byte *)gunner_walk},
	{"hover_walk", (byte *)hover_walk},
	{"infantry_walk", (byte *)infantry_walk},
	{"insane_walk", (byte *)insane_walk},
	{"medic_walk", (byte *)medic_walk},
	{"mutant_walk", (byte *)mutant_walk},
	{"parasite_start_walk", (byte *)parasite_start_walk},
	{"soldier_walk", (byte *)soldier_walk},
	{"supertank_walk", (byte *)supertank_walk},
	{"tank_walk", (byte *)tank_walk},
};
static const functionList_t fnlist_mi_walk =
{
	"mi_walk",
	fnentries_mi_walk, ARREND(fnentries_mi_walk),
};

static const fnlist_entry_t fnentries_mi_run[] =
{
	{"berserk_run", (byte *)berserk_run},
	{"boss2_run", (byte *)boss2_run},
	{"jorg_run", (byte *)jorg_run},
	{"makron_run", (byte *)makron_run},
	{"brain_run", (byte *)brain_run},
	{"chick_run", (byte *)chick_run},
	{"flipper_start_run", (byte *)flipper_start_run},
	{"floater_run", (byte *)floater_run},
	{"flyer_run", (byte *)flyer_run},
	{"gladiator_run", (byte *)gladiator_run},
	{"gunner_run", (byte *)gunner_run},
	{"hover_run", (byte *)hover_run},
	{"infantry_run", (byte *)infantry_run},
	{"insane_run", (byte *)insane_run},
	{"medic_run", (byte *)medic_run},
	{"mutant_run", (byte *)mutant_run},
	{"parasite_start_run", (byte *)parasite_start_run},
	{"soldier_run", (byte *)soldier_run},
	{"supertank_run", (byte *)supertank_run},
	{"tank_run", (byte *)tank_run},
};
static const functionList_t fnlist_mi_run =
{
	"mi_run",
	fnentries_mi_run, ARREND(fnentries_mi_run),
};

static const fnlist_entry_t fnentries_mi_melee[] =
{
	{"berserk_melee", (byte *)berserk_melee},
	{"brain_melee", (byte *)brain_melee},
	{"chick_melee", (byte *)chick_melee},
	{"flipper_melee", (byte *)flipper_melee},
	{"floater_melee", (byte *)floater_melee},
	{"flyer_melee", (byte *)flyer_melee},
	{"gladiator_melee", (byte *)gladiator_melee},
	{"mutant_melee", (byte *)mutant_melee},
};
static const functionList_t fnlist_mi_melee =
{
	"mi_melee",
	fnentries_mi_melee, ARREND(fnentries_mi_melee),
};

static const fnlist_entry_t fnentries_mi_attack[] =
{
	{"boss2_attack", (byte *)boss2_attack},
	{"jorg_attack", (byte *)jorg_attack},
	{"makron_attack", (byte *)makron_attack},
	{"chick_attack", (byte *)chick_attack},
	{"floater_attack", (byte *)floater_attack},
	{"flyer_attack", (byte *)flyer_attack},
	{"gladiator_attack", (byte *)gladiator_attack},
	{"gunner_attack", (byte *)gunner_attack},
	{"hover_start_attack", (byte *)hover_start_attack},
	{"infantry_attack", (byte *)infantry_attack},
	{"medic_attack", (byte *)medic_attack},
	{"mutant_jump", (byte *)mutant_jump},
	{"parasite_attack", (byte *)parasite_attack},
	{"soldier_attack", (byte *)soldier_attack},
	{"supertank_attack", (byte *)supertank_attack},
	{"tank_attack", (byte *)tank_attack},
};
static const functionList_t fnlist_mi_attack =
{
	"mi_attack",
	fnentries_mi_attack, ARREND(fnentries_mi_attack),
};

static const fnlist_entry_t fnentries_mi_checkattack[] =
{
	{"M_CheckAttack", (byte *)M_CheckAttack},
	{"Boss2_CheckAttack", (byte *)Boss2_CheckAttack},
	{"Jorg_CheckAttack", (byte *)Jorg_CheckAttack},
	{"Makron_CheckAttack", (byte *)Makron_CheckAttack},
	{"medic_checkattack", (byte *)medic_checkattack},
	{"mutant_checkattack", (byte *)mutant_checkattack},
};
static const functionList_t fnlist_mi_checkattack =
{
	"mi_checkattack",
	fnentries_mi_checkattack, ARREND(fnentries_mi_checkattack),
};

static const fnlist_entry_t fnentries_mi_dodge[] =
{
	{"brain_dodge", (byte *)brain_dodge},
	{"chick_dodge", (byte *)chick_dodge},
	{"gunner_dodge", (byte *)gunner_dodge},
	{"infantry_dodge", (byte *)infantry_dodge},
	{"medic_dodge", (byte *)medic_dodge},
	{"soldier_dodge", (byte *)soldier_dodge},
};
static const functionList_t fnlist_mi_dodge =
{
	"mi_dodge",
	fnentries_mi_dodge, ARREND(fnentries_mi_dodge),
};

static const fnlist_entry_t fnentries_mi_idle[] =
{
	{"brain_idle", (byte *)brain_idle},
	{"floater_idle", (byte *)floater_idle},
	{"flyer_idle", (byte *)flyer_idle},
	{"gladiator_idle", (byte *)gladiator_idle},
	{"infantry_fidget", (byte *)infantry_fidget},
	{"medic_idle", (byte *)medic_idle},
	{"mutant_idle", (byte *)mutant_idle},
	{"parasite_idle", (byte *)parasite_idle},
	{"tank_idle", (byte *)tank_idle},
};
static const functionList_t fnlist_mi_idle =
{
	"mi_idle",
	fnentries_mi_idle, ARREND(fnentries_mi_idle),
};

static const fnlist_entry_t fnentries_mi_search[] =
{
	{"berserk_search", (byte *)berserk_search},
	{"boss2_search", (byte *)boss2_search},
	{"jorg_search", (byte *)jorg_search},
	{"brain_search", (byte *)brain_search},
	{"chick_search", (byte *)chick_search},
	{"flipper_search", (byte *)flipper_search},
	{"gladiator_search", (byte *)gladiator_search},
	{"gunner_search", (byte *)gunner_search},
	{"hover_search", (byte *)hover_search},
	{"infantry_search", (byte *)infantry_search},
	{"medic_search", (byte *)medic_search},
	{"mutant_search", (byte *)mutant_search},
	{"parasite_search", (byte *)parasite_search},
	{"supertank_search", (byte *)supertank_search},
};
static const functionList_t fnlist_mi_search =
{
	"mi_search",
	fnentries_mi_search, ARREND(fnentries_mi_search),
};

static const fnlist_entry_t fnentries_mi_sight[] =
{
	{"berserk_sight", (byte *)berserk_sight},
	{"brain_sight", (byte *)brain_sight},
	{"chick_sight", (byte *)chick_sight},
	{"flipper_sight", (byte *)flipper_sight},
	{"floater_sight", (byte *)floater_sight},
	{"flyer_sight", (byte *)flyer_sight},
	{"gladiator_sight", (byte *)gladiator_sight},
	{"gunner_sight", (byte *)gunner_sight},
	{"hover_sight", (byte *)hover_sight},
	{"infantry_sight", (byte *)infantry_sight},
	{"medic_sight", (byte *)medic_sight},
	{"mutant_sight", (byte *)mutant_sight},
	{"parasite_sight", (byte *)parasite_sight},
	{"soldier_sight", (byte *)soldier_sight},
	{"tank_sight", (byte *)tank_sight},
};
static const functionList_t fnlist_mi_sight =
{
	"mi_sight",
	fnentries_mi_sight, ARREND(fnentries_mi_sight),
};

static const fnlist_entry_t fnentries_mv_end[] =
{
	{"door_secret_move5", (byte *)door_secret_move5},
	{"door_secret_move3", (byte *)door_secret_move3},
	{"door_secret_move1", (byte *)door_secret_move1},
	{"train_wait", (byte *)train_wait},
	{"door_hit_bottom", (byte *)door_hit_bottom},
	{"door_hit_top", (byte *)door_hit_top},
	{"button_wait", (byte *)button_wait},
	{"button_done", (byte *)button_done},
	{"plat_hit_top", (byte *)plat_hit_top},
	{"plat_hit_bottom", (byte *)plat_hit_bottom},
};
static const functionList_t fnlist_mv_end =
{
	"mv_end",
	fnentries_mv_end, ARREND(fnentries_mv_end),
};
