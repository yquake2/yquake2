# File Listings

These are listings of all assets of a correct Yamagi Quake II
installation. These may be used to verify if the installation
is complete.


## Game Data

The game data. See inline comments (lines starting with #) for
details. The gama data tree is the same on all platform. The
executable may be placed in the tree or in an own directory.
See the [02_installation.md](Installation Guide) for details.

We're using the Windows notation in this list. It's the same
for other platforms, but executables and libraries may have
other or no file extensions.

The three Addons - Three Wave Capture the Flag, The Reckoning
and Ground Zero - are optional, the main game will run fine
without them. All releases have the music under the mod that
it belongs to, **only** the GOG.com release has it at top
level.

```
# Top level directory
# -------------------

# Shared libaries (Windows only):
/curl.dll
/openal32.dll
/SDL2.DLL

# Renderer libraries:
/ref_gl1.dll
/ref_gl3.dll
/ref_soft.dll

# The Executables:
/quake2.exe
/yquake2.exe


# baseq2/ (Main game)
# -------------------

# Game library:
/baseq2/game.dll

# maps.lst is used in the Multiplayer menus:
/baseq2/maps.lst

# The *.pak files contain the game assets:
/baseq2/pak0.pak
/baseq2/pak1.pak
/baseq2/pak2.pak

# yq2.cfg is an optional alternative default config:
/baseq2/yq2.cfg

# Player models, needed for multiplayer.
/baseq2/players/crakhor
/baseq2/players/crakhor/a_grenades.md2
/baseq2/players/crakhor/w_bfg.md2
/baseq2/players/crakhor/w_blaster.md2
/baseq2/players/crakhor/w_chainfist.md2
/baseq2/players/crakhor/w_chaingun.md2
/baseq2/players/crakhor/w_disrupt.md2
/baseq2/players/crakhor/w_etfrifle.md2
/baseq2/players/crakhor/w_glauncher.md2
/baseq2/players/crakhor/w_grapple.md2
/baseq2/players/crakhor/w_hyperblaster.md2
/baseq2/players/crakhor/w_machinegun.md2
/baseq2/players/crakhor/w_phalanx.md2
/baseq2/players/crakhor/w_plasma.md2
/baseq2/players/crakhor/w_plauncher.md2
/baseq2/players/crakhor/w_railgun.md2
/baseq2/players/crakhor/w_ripper.md2
/baseq2/players/crakhor/w_rlauncher.md2
/baseq2/players/crakhor/w_shotgun.md2
/baseq2/players/crakhor/w_sshotgun.md2

/baseq2/players/cyborg/a_grenades.md2
/baseq2/players/cyborg/bump1.wav
/baseq2/players/cyborg/death1.wav
/baseq2/players/cyborg/death2.wav
/baseq2/players/cyborg/death3.wav
/baseq2/players/cyborg/death4.wav
/baseq2/players/cyborg/drown1.wav
/baseq2/players/cyborg/fall1.wav
/baseq2/players/cyborg/fall2.wav
/baseq2/players/cyborg/gurp1.wav
/baseq2/players/cyborg/gurp2.wav
/baseq2/players/cyborg/jump1.wav
/baseq2/players/cyborg/oni911_i.pcx
/baseq2/players/cyborg/oni911.pcx
/baseq2/players/cyborg/pain100_1.wav
/baseq2/players/cyborg/pain100_2.wav
/baseq2/players/cyborg/pain25_1.wav
/baseq2/players/cyborg/pain25_2.wav
/baseq2/players/cyborg/pain50_1.wav
/baseq2/players/cyborg/pain50_2.wav
/baseq2/players/cyborg/pain75_1.wav
/baseq2/players/cyborg/pain75_2.wav
/baseq2/players/cyborg/ps9000_i.pcx
/baseq2/players/cyborg/ps9000.pcx
/baseq2/players/cyborg/tris.md2
/baseq2/players/cyborg/tyr574_i.pcx
/baseq2/players/cyborg/tyr574.pcx
/baseq2/players/cyborg/w_bfg.md2
/baseq2/players/cyborg/w_blaster.md2
/baseq2/players/cyborg/w_chainfist.md2
/baseq2/players/cyborg/w_chaingun.md2
/baseq2/players/cyborg/w_disrupt.md2
/baseq2/players/cyborg/w_etfrifle.md2
/baseq2/players/cyborg/w_glauncher.md2
/baseq2/players/cyborg/w_grapple.md2
/baseq2/players/cyborg/w_hyperblaster.md2
/baseq2/players/cyborg/w_machinegun.md2
/baseq2/players/cyborg/w_phalanx.md2
/baseq2/players/cyborg/w_plasma.md2
/baseq2/players/cyborg/w_plauncher.md2
/baseq2/players/cyborg/w_railgun.md2
/baseq2/players/cyborg/w_ripper.md2
/baseq2/players/cyborg/w_rlauncher.md2
/baseq2/players/cyborg/w_shotgun.md2
/baseq2/players/cyborg/w_sshotgun.md2
/baseq2/players/cyborg/weapon.md2
/baseq2/players/cyborg/weapon.pcx
/baseq2/players/cyborg/weapon.pcx.pcx

/baseq2/players/female/a_grenades.md2
/baseq2/players/female/athena_i.pcx
/baseq2/players/female/athena.pcx
/baseq2/players/female/brianna_i.pcx
/baseq2/players/female/brianna.pcx
/baseq2/players/female/cobalt_i.pcx
/baseq2/players/female/cobalt.pcx
/baseq2/players/female/death1.wav
/baseq2/players/female/death2.wav
/baseq2/players/female/death3.wav
/baseq2/players/female/death4.wav
/baseq2/players/female/drown.wav
/baseq2/players/female/ensign_i.pcx
/baseq2/players/female/ensign.pcx
/baseq2/players/female/fall1.wav
/baseq2/players/female/fall2.wav
/baseq2/players/female/gurp1.wav
/baseq2/players/female/gurp2.wav
/baseq2/players/female/jezebel_i.pcx
/baseq2/players/female/jezebel.pcx
/baseq2/players/female/jump1.wav
/baseq2/players/female/jungle_i.pcx
/baseq2/players/female/jungle.pcx
/baseq2/players/female/lotus_i.pcx
/baseq2/players/female/lotus.pcx
/baseq2/players/female/pain100_1.wav
/baseq2/players/female/pain100_2.wav
/baseq2/players/female/pain25_1.wav
/baseq2/players/female/pain25_2.wav
/baseq2/players/female/pain50_1.wav
/baseq2/players/female/pain50_2.wav
/baseq2/players/female/pain75_1.wav
/baseq2/players/female/pain75_2.wav
/baseq2/players/female/stiletto_i.pcx
/baseq2/players/female/stiletto.pcx
/baseq2/players/female/tris.md2
/baseq2/players/female/venus_i.pcx
/baseq2/players/female/venus.pcx
/baseq2/players/female/voodoo_i.pcx
/baseq2/players/female/voodoo.pcx
/baseq2/players/female/w_bfg.md2
/baseq2/players/female/w_blaster.md2
/baseq2/players/female/w_chainfist.md2
/baseq2/players/female/w_chaingun.md2
/baseq2/players/female/w_disrupt.md2
/baseq2/players/female/w_etfrifle.md2
/baseq2/players/female/w_glauncher.md2
/baseq2/players/female/w_grapple.md2
/baseq2/players/female/w_hyperblaster.md2
/baseq2/players/female/w_machinegun.md2
/baseq2/players/female/w_phalanx.md2
/baseq2/players/female/w_plasma.md2
/baseq2/players/female/w_plauncher.md2
/baseq2/players/female/w_railgun.md2
/baseq2/players/female/w_ripper.md2
/baseq2/players/female/w_rlauncher.md2
/baseq2/players/female/w_shotgun.md2
/baseq2/players/female/w_sshotgun.md2
/baseq2/players/female/weapon.md2
/baseq2/players/female/weapon.pcx

/baseq2/players/male/a_grenades.md2
/baseq2/players/male/bump1.wav
/baseq2/players/male/cipher_i.pcx
/baseq2/players/male/cipher.pcx
/baseq2/players/male/claymore_i.pcx
/baseq2/players/male/claymore.pcx
/baseq2/players/male/death1.wav
/baseq2/players/male/death2.wav
/baseq2/players/male/death3.wav
/baseq2/players/male/death4.wav
/baseq2/players/male/drown1.wav
/baseq2/players/male/fall1.wav
/baseq2/players/male/fall2.wav
/baseq2/players/male/flak_i.pcx
/baseq2/players/male/flak.pcx
/baseq2/players/male/grunt_i.pcx
/baseq2/players/male/grunt.pcx
/baseq2/players/male/gurp1.wav
/baseq2/players/male/gurp2.wav
/baseq2/players/male/howitzer_i.pcx
/baseq2/players/male/howitzer.pcx
/baseq2/players/male/jump1.wav
/baseq2/players/male/major_i.pcx
/baseq2/players/male/major.pcx
/baseq2/players/male/nightops_i.pcx
/baseq2/players/male/nightops.pcx
/baseq2/players/male/pain100_1.wav
/baseq2/players/male/pain100_2.wav
/baseq2/players/male/pain25_1.wav
/baseq2/players/male/pain25_2.wav
/baseq2/players/male/pain50_1.wav
/baseq2/players/male/pain50_2.wav
/baseq2/players/male/pain75_1.wav
/baseq2/players/male/pain75_2.wav
/baseq2/players/male/pointman_i.pcx
/baseq2/players/male/pointman.pcx
/baseq2/players/male/psycho_i.pcx
/baseq2/players/male/psycho.pcx
/baseq2/players/male/rampage_i.pcx
/baseq2/players/male/rampage.pcx
/baseq2/players/male/razor_i.pcx
/baseq2/players/male/razor.pcx
/baseq2/players/male/recon_i.pcx
/baseq2/players/male/recon.pcx
/baseq2/players/male/scout_i.pcx
/baseq2/players/male/scout.pcx
/baseq2/players/male/sniper_i.pcx
/baseq2/players/male/sniper.pcx
/baseq2/players/male/tris.md2
/baseq2/players/male/viper_i.pcx
/baseq2/players/male/viper.pcx
/baseq2/players/male/w_bfg.md2
/baseq2/players/male/w_blaster.md2
/baseq2/players/male/w_chainfist.md2
/baseq2/players/male/w_chaingun.md2
/baseq2/players/male/w_disrupt.md2
/baseq2/players/male/w_etfrifle.md2
/baseq2/players/male/w_glauncher.md2
/baseq2/players/male/w_grapple.md2
/baseq2/players/male/w_hyperblaster.md2
/baseq2/players/male/w_machinegun.md2
/baseq2/players/male/w_phalanx.md2
/baseq2/players/male/w_plasma.md2
/baseq2/players/male/w_plauncher.md2
/baseq2/players/male/w_railgun.md2
/baseq2/players/male/w_ripper.md2
/baseq2/players/male/w_rlauncher.md2
/baseq2/players/male/w_shotgun.md2
/baseq2/players/male/w_sshotgun.md2
/baseq2/players/male/weapon.md2
/baseq2/players/male/weapon.pcx

# Cinematics, videos played between units.
/baseq2/video/end.cin
/baseq2/video/eou1_.cin
/baseq2/video/eou2_.cin
/baseq2/video/eou3_.cin
/baseq2/video/eou4_.cin
/baseq2/video/eou5_.cin
/baseq2/video/eou6_.cin
/baseq2/video/eou7_.cin
/baseq2/video/eou8_.cin
/baseq2/video/idlog.cin
/baseq2/video/ntro.cin

# OGG files. For standard releases, NOT for the GOG.com release:
/baseq2/music/02.ogg
/baseq2/music/03.ogg
/baseq2/music/04.ogg
/baseq2/music/05.ogg
/baseq2/music/06.ogg
/baseq2/music/07.ogg
/baseq2/music/08.ogg
/baseq2/music/09.ogg
/baseq2/music/10.ogg
/baseq2/music/11.ogg


# ctf/ (Three Wave Capture the Flag)
# ----------------------------------

# Game library:
/ctf/game.dll

# The *.pak files contain the game assets:
/ctf/pak0.pak


# rogue/ (Ground Zero)
# --------------------

# Game library:
/rogue/game.dll

# The *.pak files contain the game assets:
/rogue/pak0.pak

# Cinematics, videos played between units.
/rogue/video/logo.cin
/rogue/video/rend.cin
/rogue/video/reu1_.cin
/rogue/video/reu2_.cin
/rogue/video/reu3_.cin
/rogue/video/reu4_.cin
/rogue/video/rintro.cin

# OGG files. For standard releases, NOT for the GOG.com release:
/rogue/music/02.ogg
/rogue/music/03.ogg
/rogue/music/04.ogg
/rogue/music/05.ogg
/rogue/music/06.ogg
/rogue/music/07.ogg
/rogue/music/08.ogg
/rogue/music/09.ogg
/rogue/music/10.ogg
/rogue/music/11.ogg


# xatrix/ (The Reckoning)
# -----------------------

# Game library:
/xatrix/game.dll

# The *.pak files contain the game assets:
/xatrix/pak0.pak

# Cinematics, videos played between units.
/xatrix/video/idlog.cin
/xatrix/video/logo.cin
/xatrix/video/xin.cin
/xatrix/video/xout.cin
/xatrix/video/xu1.cin
/xatrix/video/xu2.cin
/xatrix/video/xu3.cin

# OGG files. For standard releases, NOT for the GOG.com release:
/xatrix/music/02.ogg
/xatrix/music/03.ogg
/xatrix/music/04.ogg
/xatrix/music/05.ogg
/xatrix/music/06.ogg
/xatrix/music/07.ogg
/xatrix/music/08.ogg
/xatrix/music/09.ogg
/xatrix/music/10.ogg
/xatrix/music/11.ogg


# music/ (GOG.com music files)
# ----------------------------

# These are for the GOG.com release only.
/music/Track02.ogg
/music/Track03.ogg
/music/Track04.ogg
/music/Track05.ogg
/music/Track06.ogg
/music/Track07.ogg
/music/Track08.ogg
/music/Track09.ogg
/music/Track10.ogg
/music/Track11.ogg
/music/Track12.ogg
/music/Track13.ogg
/music/Track14.ogg
/music/Track15.ogg
/music/Track16.ogg
/music/Track17.ogg
/music/Track18.ogg
/music/Track19.ogg
/music/Track20.ogg
/music/Track21.ogg

```


## Checksums

SHA1 and md5 checksums of all pak files.

```
SHA1 (baseq2/pak0.pak) = 1dd586a3230d1ac5bfd34e57cc796000d4c252c2
SHA1 (baseq2/pak1.pak) = 588ef09965dee539521b4eb6da4337ce78a2ea21
SHA1 (baseq2/pak2.pak) = 67e76a7f3234646507ae59ec1bf755637c1dad03
SHA1 (ctf/pak0.pak) = 11b2362521cfe27473d79f72172531d206d07e14
SHA1 (rogue/pak0.pak) = 8d3979e90f2ffd97692c5d1461b6ddaa5bf84ce6
SHA1 (xatrix/pak0.pak) = 05070e2567ac3f726b4ef8eb62f0c645cdc24547
```

```
MD5 (baseq2/pak0.pak) = 1ec55a724dc3109fd50dde71ab581d70
MD5 (baseq2/pak1.pak) = 42663ea709b7cd3eb9b634b36cfecb1a
MD5 (baseq2/pak2.pak) = c8217cc5557b672a87fc210c2347d98d
MD5 (ctf/pak0.pak) = 1f6bd3d4c08f7ed8c037b12fcffd2eb5
MD5 (rogue/pak0.pak) = 5e2ecbe9287152a1e6e0d77b3f47dcb2
MD5 (xatrix/pak0.pak) = f5e7b04f7d6b9530c59c5e1daa873b51
```
