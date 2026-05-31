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
 * Prototypes for every function in the game.so.
 *
 * =======================================================================
 */

// die
extern void button_killed (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void door_killed (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void door_secret_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void gib_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void debris_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void func_explosive_explode (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void barrel_delay (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void commander_body_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void misc_deadsoldier_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void turret_driver_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void berserk_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void boss2_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void jorg_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void makron_torso_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void makron_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void brain_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void chick_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void flipper_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void floater_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void flyer_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void gladiator_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void gunner_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void hover_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void infantry_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void insane_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void medic_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void mutant_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void parasite_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void soldier_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void supertank_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void tank_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void body_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
extern void player_die (edict_t *, edict_t *, edict_t *, int, const vec3_t);
// pain
extern void berserk_pain (edict_t *, edict_t *, float, int);
extern void boss2_pain (edict_t *, edict_t *, float, int);
extern void jorg_pain (edict_t *, edict_t *, float, int);
extern void makron_pain (edict_t *, edict_t *, float, int);
extern void brain_pain (edict_t *, edict_t *, float, int);
extern void chick_pain (edict_t *, edict_t *, float, int);
extern void flipper_pain (edict_t *, edict_t *, float, int);
extern void floater_pain (edict_t *, edict_t *, float, int);
extern void flyer_pain (edict_t *, edict_t *, float, int);
extern void gladiator_pain (edict_t *, edict_t *, float, int);
extern void gunner_pain (edict_t *, edict_t *, float, int);
extern void hover_pain (edict_t *, edict_t *, float, int);
extern void infantry_pain (edict_t *, edict_t *, float, int);
extern void insane_pain (edict_t *, edict_t *, float, int);
extern void medic_pain (edict_t *, edict_t *, float, int);
extern void mutant_pain (edict_t *, edict_t *, float, int);
extern void parasite_pain (edict_t *, edict_t *, float, int);
extern void soldier_pain (edict_t *, edict_t *, float, int);
extern void supertank_pain (edict_t *, edict_t *, float, int);
extern void tank_pain (edict_t *, edict_t *, float, int);
extern void player_pain (edict_t *, edict_t *, float, int);
// prethink
extern void misc_blackhole_transparent (edict_t *);
extern void misc_viper_bomb_prethink (edict_t *);
// think
extern void Move_Done (edict_t *);
extern void Move_Final (edict_t *);
extern void Move_Begin (edict_t *);
extern void Think_AccelMove (edict_t *);
extern void AngleMove_Done (edict_t *);
extern void AngleMove_Final (edict_t *);
extern void AngleMove_Begin (edict_t *);
extern void plat_go_down (edict_t *);
extern void wait_and_change_think (edict_t *);
extern void button_return (edict_t *);
extern void door_go_down (edict_t *);
extern void Think_CalcMoveSpeed (edict_t *);
extern void Think_SpawnDoorTrigger (edict_t *);
extern void train_next (edict_t *);
extern void func_train_find (edict_t *);
extern void trigger_elevator_init (edict_t *);
extern void func_timer_think (edict_t *);
extern void door_secret_move2 (edict_t *);
extern void door_secret_move4 (edict_t *);
extern void door_secret_move6 (edict_t *);
extern void G_FreeEdict (edict_t *);
extern void DoRespawn (edict_t *);
extern void MegaHealth_think (edict_t *);
extern void drop_make_touchable (edict_t *);
extern void droptofloor (edict_t *);
extern void gib_think (edict_t *);
extern void TH_viewthing (edict_t *);
extern void func_object_release (edict_t *);
extern void barrel_explode (edict_t *);
extern void M_droptofloor (edict_t *);
extern void misc_blackhole_think (edict_t *);
extern void misc_eastertank_think (edict_t *);
extern void misc_easterchick_think (edict_t *);
extern void misc_easterchick2_think (edict_t *);
extern void commander_body_think (edict_t *);
extern void commander_body_drop (edict_t *);
extern void misc_banner_think (edict_t *);
extern void misc_satellite_dish_think (edict_t *);
extern void func_clock_think (edict_t *);
extern void M_FliesOff (edict_t *);
extern void M_FliesOn (edict_t *);
extern void monster_triggered_spawn (edict_t *);
extern void monster_think (edict_t *);
extern void walkmonster_start_go (edict_t *);
extern void flymonster_start_go (edict_t *);
extern void swimmonster_start_go (edict_t *);
extern void Target_Help_Think (edict_t *);
extern void target_explosion_explode (edict_t *);
extern void target_crosslevel_target_think (edict_t *);
extern void target_laser_think (edict_t *);
extern void target_laser_start (edict_t *);
extern void target_lightramp_think (edict_t *);
extern void target_earthquake_think (edict_t *);
extern void multi_wait (edict_t *);
extern void turret_breach_think (edict_t *);
extern void turret_breach_finish_init (edict_t *);
extern void turret_driver_think (edict_t *);
extern void turret_driver_link (edict_t *);
extern void Think_Delay (edict_t *);
extern void Grenade_Explode (edict_t *);
extern void bfg_explode (edict_t *);
extern void bfg_think (edict_t *);
extern void Think_Boss3Stand (edict_t *);
extern void makron_torso_think (edict_t *);
extern void MakronSpawn (edict_t *);
extern void hover_deadthink (edict_t *);
extern void BossExplode (edict_t *);
extern void SP_CreateUnnamedSpawn (edict_t *);
extern void SP_CreateCoopSpots (edict_t *);
extern void SP_FixCoopSpots (edict_t *);
// touch
extern void Touch_Plat_Center (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void rotating_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void button_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void Touch_DoorTrigger (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void door_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void Touch_Item (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void drop_temp_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void gib_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void path_corner_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void point_combat_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void func_object_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void barrel_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void misc_viper_bomb_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void teleporter_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void Touch_Multi (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void trigger_push_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void hurt_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void trigger_gravity_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void trigger_monsterjump_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void blaster_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void Grenade_Touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void rocket_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void bfg_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
extern void mutant_jump_touch (edict_t *, edict_t *, const cplane_t *, const csurface_t *);
// use
extern void Use_Plat (edict_t *, edict_t *, edict_t *);
extern void rotating_use (edict_t *, edict_t *, edict_t *);
extern void button_use (edict_t *, edict_t *, edict_t *);
extern void door_use (edict_t *, edict_t *, edict_t *);
extern void train_use (edict_t *, edict_t *, edict_t *);
extern void trigger_elevator_use (edict_t *, edict_t *, edict_t *);
extern void func_timer_use (edict_t *, edict_t *, edict_t *);
extern void func_conveyor_use (edict_t *, edict_t *, edict_t *);
extern void door_secret_use (edict_t *, edict_t *, edict_t *);
extern void use_killbox (edict_t *, edict_t *, edict_t *);
extern void Use_Item (edict_t *, edict_t *, edict_t *);
extern void Use_Areaportal (edict_t *, edict_t *, edict_t *);
extern void light_use (edict_t *, edict_t *, edict_t *);
extern void func_wall_use (edict_t *, edict_t *, edict_t *);
extern void func_object_use (edict_t *, edict_t *, edict_t *);
extern void func_explosive_spawn (edict_t *, edict_t *, edict_t *);
extern void func_explosive_use (edict_t *, edict_t *, edict_t *);
extern void misc_blackhole_use (edict_t *, edict_t *, edict_t *);
extern void commander_body_use (edict_t *, edict_t *, edict_t *);
extern void misc_viper_use (edict_t *, edict_t *, edict_t *);
extern void misc_viper_bomb_use (edict_t *, edict_t *, edict_t *);
extern void misc_strogg_ship_use (edict_t *, edict_t *, edict_t *);
extern void misc_satellite_dish_use (edict_t *, edict_t *, edict_t *);
extern void target_string_use (edict_t *, edict_t *, edict_t *);
extern void func_clock_use (edict_t *, edict_t *, edict_t *);
extern void monster_use (edict_t *, edict_t *, edict_t *);
extern void monster_triggered_spawn_use (edict_t *, edict_t *, edict_t *);
extern void Use_Target_Tent (edict_t *, edict_t *, edict_t *);
extern void Use_Target_Speaker (edict_t *, edict_t *, edict_t *);
extern void Use_Target_Help (edict_t *, edict_t *, edict_t *);
extern void use_target_secret (edict_t *, edict_t *, edict_t *);
extern void use_target_goal (edict_t *, edict_t *, edict_t *);
extern void use_target_explosion (edict_t *, edict_t *, edict_t *);
extern void use_target_changelevel (edict_t *, edict_t *, edict_t *);
extern void use_target_splash (edict_t *, edict_t *, edict_t *);
extern void use_target_spawner (edict_t *, edict_t *, edict_t *);
extern void use_target_blaster (edict_t *, edict_t *, edict_t *);
extern void trigger_crosslevel_trigger_use (edict_t *, edict_t *, edict_t *);
extern void target_laser_use (edict_t *, edict_t *, edict_t *);
extern void target_lightramp_use (edict_t *, edict_t *, edict_t *);
extern void target_earthquake_use (edict_t *, edict_t *, edict_t *);
extern void Use_Multi (edict_t *, edict_t *, edict_t *);
extern void trigger_enable (edict_t *, edict_t *, edict_t *);
extern void trigger_relay_use (edict_t *, edict_t *, edict_t *);
extern void trigger_key_use (edict_t *, edict_t *, edict_t *);
extern void trigger_counter_use (edict_t *, edict_t *, edict_t *);
extern void hurt_use (edict_t *, edict_t *, edict_t *);
extern void Use_Boss3 (edict_t *, edict_t *, edict_t *);
// blocked
extern void plat_blocked (edict_t *, edict_t *);
extern void rotating_blocked (edict_t *, edict_t *);
extern void door_blocked (edict_t *, edict_t *);
extern void train_blocked (edict_t *, edict_t *);
extern void door_secret_blocked (edict_t *, edict_t *);
extern void turret_blocked (edict_t *, edict_t *);
// monsterinfo.stand
extern void infantry_stand (edict_t *);
extern void berserk_stand (edict_t *);
extern void boss2_stand (edict_t *);
extern void jorg_stand (edict_t *);
extern void makron_stand (edict_t *);
extern void brain_stand (edict_t *);
extern void chick_stand (edict_t *);
extern void flipper_stand (edict_t *);
extern void floater_stand (edict_t *);
extern void flyer_stand (edict_t *);
extern void gladiator_stand (edict_t *);
extern void gunner_stand (edict_t *);
extern void hover_stand (edict_t *);
extern void insane_stand (edict_t *);
extern void medic_stand (edict_t *);
extern void mutant_stand (edict_t *);
extern void parasite_stand (edict_t *);
extern void soldier_stand (edict_t *);
extern void supertank_stand (edict_t *);
extern void tank_stand (edict_t *);
// monsterinfo.walk
extern void berserk_walk (edict_t *);
extern void boss2_walk (edict_t *);
extern void jorg_walk (edict_t *);
extern void makron_walk (edict_t *);
extern void brain_walk (edict_t *);
extern void chick_walk (edict_t *);
extern void flipper_walk (edict_t *);
extern void floater_walk (edict_t *);
extern void flyer_walk (edict_t *);
extern void gladiator_walk (edict_t *);
extern void gunner_walk (edict_t *);
extern void hover_walk (edict_t *);
extern void infantry_walk (edict_t *);
extern void insane_walk (edict_t *);
extern void medic_walk (edict_t *);
extern void mutant_walk (edict_t *);
extern void parasite_start_walk (edict_t *);
extern void soldier_walk (edict_t *);
extern void supertank_walk (edict_t *);
extern void tank_walk (edict_t *);
// monsterinfo.run
extern void berserk_run (edict_t *);
extern void boss2_run (edict_t *);
extern void jorg_run (edict_t *);
extern void makron_run (edict_t *);
extern void brain_run (edict_t *);
extern void chick_run (edict_t *);
extern void flipper_start_run (edict_t *);
extern void floater_run (edict_t *);
extern void flyer_run (edict_t *);
extern void gladiator_run (edict_t *);
extern void gunner_run (edict_t *);
extern void hover_run (edict_t *);
extern void infantry_run (edict_t *);
extern void insane_run (edict_t *);
extern void medic_run (edict_t *);
extern void mutant_run (edict_t *);
extern void parasite_start_run (edict_t *);
extern void soldier_run (edict_t *);
extern void supertank_run (edict_t *);
extern void tank_run (edict_t *);
// monsterinfo.melee
extern void berserk_melee (edict_t *);
extern void brain_melee (edict_t *);
extern void chick_melee (edict_t *);
extern void flipper_melee (edict_t *);
extern void floater_melee (edict_t *);
extern void flyer_melee (edict_t *);
extern void gladiator_melee (edict_t *);
extern void mutant_melee (edict_t *);
// monsterinfo.attack
extern void boss2_attack (edict_t *);
extern void jorg_attack (edict_t *);
extern void makron_attack (edict_t *);
extern void chick_attack (edict_t *);
extern void floater_attack (edict_t *);
extern void flyer_attack (edict_t *);
extern void gladiator_attack (edict_t *);
extern void gunner_attack (edict_t *);
extern void hover_start_attack (edict_t *);
extern void infantry_attack (edict_t *);
extern void medic_attack (edict_t *);
extern void mutant_jump (edict_t *);
extern void parasite_attack (edict_t *);
extern void soldier_attack (edict_t *);
extern void supertank_attack (edict_t *);
extern void tank_attack (edict_t *);
// monsterinfo.checkattack
extern qboolean M_CheckAttack (edict_t *);
extern qboolean Boss2_CheckAttack (edict_t *);
extern qboolean Jorg_CheckAttack (edict_t *);
extern qboolean Makron_CheckAttack (edict_t *);
extern qboolean medic_checkattack (edict_t *);
extern qboolean mutant_checkattack (edict_t *);
// monsterinfo.dodge
extern void brain_dodge (edict_t *, edict_t *, float);
extern void chick_dodge (edict_t *, edict_t *, float);
extern void gunner_dodge (edict_t *, edict_t *, float);
extern void infantry_dodge (edict_t *, edict_t *, float);
extern void medic_dodge (edict_t *, edict_t *, float);
extern void soldier_dodge (edict_t *, edict_t *, float);
// monsterinfo.idle
extern void brain_idle (edict_t *);
extern void flipper_idle (edict_t *);
extern void floater_idle (edict_t *);
extern void flyer_idle (edict_t *);
extern void gladiator_idle (edict_t *);
extern void infantry_fidget (edict_t *);
extern void medic_idle (edict_t *);
extern void mutant_idle (edict_t *);
extern void parasite_idle (edict_t *);
extern void tank_idle (edict_t *);
// monsterinfo.search
extern void berserk_search (edict_t *);
extern void boss2_search (edict_t *);
extern void jorg_search (edict_t *);
extern void brain_search (edict_t *);
extern void chick_search (edict_t *);
extern void flipper_search (edict_t *);
extern void gladiator_search (edict_t *);
extern void gunner_search (edict_t *);
extern void hover_search (edict_t *);
extern void infantry_search (edict_t *);
extern void medic_search (edict_t *);
extern void mutant_search (edict_t *);
extern void parasite_search (edict_t *);
extern void supertank_search (edict_t *);
// monsterinfo.sight
extern void berserk_sight (edict_t *, edict_t *);
extern void brain_sight (edict_t *, edict_t *);
extern void chick_sight (edict_t *, edict_t *);
extern void flipper_sight (edict_t *, edict_t *);
extern void floater_sight (edict_t *, edict_t *);
extern void flyer_sight (edict_t *, edict_t *);
extern void gladiator_sight (edict_t *, edict_t *);
extern void gunner_sight (edict_t *, edict_t *);
extern void hover_sight (edict_t *, edict_t *);
extern void infantry_sight (edict_t *, edict_t *);
extern void medic_sight (edict_t *, edict_t *);
extern void mutant_sight (edict_t *, edict_t *);
extern void parasite_sight (edict_t *, edict_t *);
extern void soldier_sight (edict_t *, edict_t *);
extern void tank_sight (edict_t *, edict_t *);
// moveinfo.endfunc
extern void plat_hit_top (edict_t *);
extern void plat_hit_bottom (edict_t *);
extern void button_done (edict_t *);
extern void button_wait (edict_t *);
extern void door_hit_top (edict_t *);
extern void door_hit_bottom (edict_t *);
extern void train_wait (edict_t *);
extern void door_secret_move1 (edict_t *);
extern void door_secret_move3 (edict_t *);
extern void door_secret_move5 (edict_t *);
extern void door_secret_done (edict_t *);
