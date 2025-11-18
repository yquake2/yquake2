# Yamagi Quake II Console Variables

This lists explains most console variables (cvars) added by Yamagi
Quake II. Most of the original clients (Vanilla Quake II) cvars are
still in place, however due to architectural changes some of them
have been renamed. The prefixes are:

* No prefix: General stuff.
* `cl_`: Client.
* `gl_`: Common to all OpenGL renderers.
* `gl1_`: OpenGL 1.4 and OpenGL ES1 renderers.
* `gl3_`: OpenGL 3.2 and OpenGL ES3 renderers.
* `ogg_`: Ogg/Vorbis music playback.
* `r_`: Common to all renderers.
* `s_`: Sound system.
* `sv_`: Server 
* `sw_`: Software renderer.
* `vid_`: Video backend.

All cvars may be given at command line through `+set cvar value` or
typed into the console. The console can be opended with *Left Shift +
Esc*.

Keep in mind that some cvars need quotation marks around the arguments.
When giving such cvars at the command line the argument string must be
surrounded by ticks. For example `+set sv_maplist '"q2dm1 q2dm2"'`.


## Command line arguments

These are not console variables, they cannot be entered into the
console, only be given through the command line at startup. While cvars
are prefixed with a `+`, arguments are starting with a `-`. For example
it's `+set busywait 0` (setting the `busywait` cvar) and `-portable`
(setting the `portable` argument).

* **cfgdir**: The name (not the path) of the configuration directory.

* **datadir**: Directory from which the game data is loaded. Can be used
  in startup scripts, to test binaries, etc. If not set, the directory
  containing the binaries is used.

* **portable**: Makes Quake II portable, all runtime data like the the
  config, savegames and so on is stored next to the executable and not
  in the users home directory.


## General

* **aimfix**: Fix aiming. When set to to `0` (the default) aiming is
  slightly inaccurate, bullets and the like have a little drift. When
  set to `1` they hit exactly were the crosshair is.
  
* **busywait**: By default this is set to `1`, causing Quake II to spin
  in a very tight loop until it's time to process the next frame. This
  is a very accurate way to determine the internal timing, but comes with
  a relatively high CPU usage. If set to `0` Quake II lays itself to
  sleep and tells the operating system to send a wakeup signal when it's
  time for the next frame. The latter is more CPU friendly but can be
  rather inaccurate, especially on Windows. Use with care.

* **sv_optimize_sp_loadtime** / **sv_optimize_mp_loadtime**: These cvars
  enable/disable optimizations that speed up level load times (or more
  accurately, client connection).  
  sp stands for singleplayer and mp for multiplayer, respectively.  
  The sp version is enabled by default (value 7) while multiplayer
  is 0.  
  The cvar value is a bitmask for 3 optimization features:

  - **1: Message utilization**: When the server sends the client
    configstrings and other data during the connection process, the
    message buffer is only used around 50-60%. When this flag is
    enabled, the message is used much better, dramatically reducing the
    amount of messages needed to deliver all the data to the client.
  - **2: Server send rate**: By default, the server sends messages to
    clients once every 0.1 seconds, roughly. This slows down sending
    data to clients, especially in singleplayer. This is normal for
    active clients, but for connecting/inactive clients, this delay is
    unnecessary. When this flag is set, the server will send messages to
    inactive clients ~8x more frequently.
  - **4: Reconnection**: When the server changes maps, like on level
    transitions, the server first sends a "changing" command to all
    clients, and then a "reconnect" command. The delay between these
    commands can be quite long, ~1 second. This flag will avoid this
    delay when set, by sending the two commands within the same message.

  Simply add these flag values together to get the cvar value you want.
  For example, sendrate + reconnect = 2 + 4 = 6.
  Set to 7 for all optimizations, or 0 to disable them entirely.

* **cl_maxfps**: The approximate framerate for client/server ("packet")
  frames if *cl_async* is `1`. If set to `-1` (the default), the engine
  will choose a packet framerate appropriate for the render framerate.  
  See `cl_async` for more information.

* **cl_async**: Run render frames independently of client/server frames.  
  If set to `0`, client, server (gamecode) and the renderer run synchronous,
  (like Quake2 originally did) which means that for every rendered frame
  a client- and server-frame is executed, which includes the gamecode and
  physics/movement-simulation etc. At higher framerates (above 95 or so)
  this leads to movement bugs, like being able to jump higher than expected
  (kind of like the infamous Quake 3 125Hz bug).  
  For `cl_async 0`, *vid_maxfps* (or, if vsync is enabled, the display
  refresh rate) is used and *cl_maxfps* is ignored.
  
  If *cl_async* is set to `1` (the default) the client is asynchronous,
  which means that there can be multiple render frames between client-
  and server-frames. This makes it possible to renderer as many frames
  as desired without physics and movement problems.  
  The client framerate is controlled by *cl_maxfps*,  
  the renderer framerate is controlled by *vid_maxfps*.  
  
  As client/server frames ("packet frames") are only run together with
  a render frame, the *real* client/server framerate is always rounded to
  a fraction of the renderframerate that's closest to *cl_maxfps*.  
  So if for example *vid_maxfps* is `60` and *cl_maxfps* is `50`, it will
  be rounded to `60` and every renderframe is also a packet frame.  
  If *vid_maxfps* is `60` and *cl_maxfps* is `40`, it will be rounded to
  `30` and every second render frame is also a packet frame.
  
  It seems like the best working packet framerate is `60` (which means that
  the render framerate should be a multiple of that), otherwise values
  between `45` and `90` seem to work ok, lower and higher values can lead
  to buggy movement, jittering and other issues.  
  Setting *cl_maxfps* to `-1` (the default since 8.02) will automatically
  choose a packet framerate that's *both* a fraction of *vid_maxfps*
  (or display refreshrate if vsync is on) *and* between 45 and 90.
  
* **cl_http_downloads**: Allow HTTP download. Set to `1` by default, set
  to `0` to disable.

* **cl_http_filelists**: Allow downloading and processing of filelists.
  A filelist can contain an arbitrary number of files which are
  downloaded as soon asthe filelist is found on the server. Set to `1`
  by default, set to `0` to disable.

* **cl_http_max_connections**: Maximum number of parallel downloads. Set
  to `4` by default. A higher number may help with slow servers.

* **cl_http_verifypeer**: SSL certificate validation. Set to `1`
  by default, set to `0` to disable.

* **cl_http_proxy**: Proxy to use, empty by default.

* **cl_http_show_dw_progress**: Show a HTTP download progress bar.

* **cl_http_bw_limit_rate**: Average speed transfer threshold for
`cl_http_bw_limit_tmout` variable. Set `0` by default.

* **cl_http_bw_limit_tmout**: Seconds before the download is aborted
  when the speed transfer is below the var set by
  `cl_http_bw_limit_rate`. Set `0` by default.

* **cl_kickangles**: If set to `0` angle kicks (weapon recoil, damage
  hits and the like) are ignored. Cheat-protected. Defaults to `1`.

* **cl_laseralpha**: Controls how see-through laserbeams are.
  The value ranges from 0.0 to 1.0, from completely invisible to
  completely opaque. So higher value means better visibility.
  Defaults to `0.3`.

* **cl_limitsparksounds**: If set to `1` the number of sound generated
  when shooting into power screen and power shields is limited to 16.
  This works around global volume drops in some OpenAL implementations
  if too many sounds are played at the same time. This was the default
  behavior between Yamagi Quake II 7.10 and 7.45. Defaults to `0`.

* **cl_loadpaused**: If set to `1` (the default) the client is put into
  pause mode during single player savegame load. This prevents monsters
  and the environment from hurting the player while the client is still
  connecting. If set to `2` the client stays in pause mode after
  loading. If set to `0` pause mode is never entered, this is the
  Vanilla Quake II behaviour.

* **cl_model_preview_start**: Start frame value in multiplayer model
  preview.  `-1` - don't show animation. Defaults to `84` for show
  salute animation.

* **cl_model_preview_end**: End frame value in multiplayer model
  preview.  `-1` - don't show animation. Defaults to `94` for show
  salute animation.

* **cl_nodownload_list**: Whitespace separated list of substrings, files
  having one these strings in their name are never downloaded. Empty by
  default. Note that some substrings are always forbidden, for security
  reasons these cannot be overridden: '.dll', '.dylib' and '.so' to
  prevent downloading of libraries which could be injected into the
  Yamagi Quake II process. '..' or ':' inside filenames and '/' or '.'
  at the beginning of filenames to prevent downloading files into
  arbitrary directories.

* **cl_r1q2_lightstyle**: Since the first release Yamagi Quake II used
  the R1Q2 colors for the dynamic lights of rockets. Set to `0` to get
  the Vanilla Quake II colors. Defaults to `1`.

* **cl_showfps**: Shows the framecounter. Set to `2` for more and to
  `3` for even more informations.
  
* **cl_showspeed**:  Shows the players speed. Set to `1` to display both
  overall speed and (horizontal speed) in Quake Units (QU) respectfully
  at the top right corner of the screen. Set to `2` to show only the
  horizontal speed under the crosshair.
  
* **cl_unpaused_scvis**: If set to `1` (the default) the client unpause
  when the screen becomes visible.

* **cl_centertime**: Time in ms that takes the `centerview` command to
  finish. `0` is immediate (Vanilla Q2 behaviour). Default `180`.

* **in_grab**: Defines how the mouse is grabbed by Yamagi Quake IIs
  window. If set to `0` the mouse is never grabbed and if set to `1`
  it's always grabbed. If set to `2` (the default) the mouse is grabbed
  during gameplay and released otherwise (in menu, videos, console or if
  game is paused).

* **singleplayer**: Only available in the dedicated server. Vanilla
  Quake II enforced that either `coop` or `deathmatch` is set to `1`
  when running the dedicated server. That made it impossible to play
  single player campaigns over the dedicated server. When set to `1`,
  both `coop` and `deathmatch` are forced to `0` and `maxclients` is
  forced to `1`. This can be used to run a dedicated server with an old
  single player mod, where the source code isn't available, inside a
  Windows 98 or XP VM and connect over network from a non Windows
  system.

* **coop_pickup_weapons**: In coop a weapon can be picked up only once.
  For example, if the player already has the shotgun they cannot pickup
  a second shotgun found at a later time, thus not getting the ammo that
  comes with it. This breaks the balancing. If set to `1` a weapon can
  be picked up if a) the player doesn't have it or b) it wasn't already
  picked up by another player. Defaults to `1`.

* **coop_elevator_delay**: In coop it's often hard to get on the same
  elevator together, because they're immediately triggered once the
  first player steps on it. This cvar sets a delay for the elevator to
  wait before moving, so other players have some time to get on it.
  Defaults to `1.0` (seconds).

* **coop_baseq2 (Ground Zero only)**: In Ground Zero, entity spawnflags
  (which difficulty modes / game modes level entities spawn in) are
  interpreted a bit differently. In original Quake 2, if an entity is
  set to not spawn on any difficulty, it is treated as deathmatch-only,
  however, in Ground Zero this same condition is treated as coop-only.
  This causes maps made for original Quake 2, including the entire
  Quake 2 campaign, to not work correctly when played in Ground Zero
  in co-op mode. This cvar, when set to 1, restores the original
  interpretation and enables you to play original Quake 2 maps in
  Ground Zero co-op. Though keep in mind that Ground Zero maps will
  not work correctly when this cvar is enabled so remember to
  disable it again before playing Ground Zero maps in co-op. By
  default this cvar is disabled (set to 0).

* **g_commanderbody_nogod**: If set to `1` the tank commanders body
  entity can be destroyed. If the to `0` (the default) it is
  indestructible.

* **g_footsteps**: If set to `1` (the default) footstep sounds are
  generated when the player is on ground and faster than 255. This is
  the behaviour of Vanilla Quake II. If set to `2` footestep sound
  always generated as long as the player is on ground. If set to `3`
  footsteps are always generated. If set to `0` footstep sounds are
  never generated. Cheat protected to `1`. Note that there isn't a
  reliable way to figure out if the player is on ground. Footsteps
  may not be generated in all circumstances, especially when the player
  is moving over stairs and slopes.

* **g_monsterfootsteps**: If set to `1` monster footstep are generated.
  By default this cvar is disabled (set to 0). Additional footstep
  sounds are required. See the installation guide for details.

* **g_fix_triggered**: This cvar, when set to `1`, forces monsters to
  spawn in normally if they are set to a triggered spawn but do not
  have a targetname. There are a few cases of this in Ground Zero and
  The Reckoning. This cvar is disabled by default to maintain the
  original gameplay experience.

* **g_machinegun_norecoil**: Disable machine gun recoil in single player. 
  By default this is set to `0`, this keeps the original machine gun 
  recoil in single player. When set to `1` the recoil is disabled in
  single player, the same way as in multiplayer.  
  This cvar only works if the game.dll implements this behaviour.

* **g_quick_weap**: If set to `1`, both *weapprev* and *weapnext*
  commands will "count" how many times they have been called, making
  possible to skip weapons by quickly tapping one of these keys.
  By default this cvar is set to `1`, and will only work if the
  game.dll implements this behaviour.

* **g_swap_speed**: Sets the speed of the "changing weapon" animation.
  Default is `1`. If set to `2`, it will be double the speed, `3` is
  the triple... up until the max of `8`, since there are at least 2
  frames of animation that will be played compulsorily, on every weapon.
  Cheat-protected, has to be a positive integer. As with the last one,
  will only work if the game.dll implements this behaviour.

* **g_disruptor (Ground Zero only)**: This boolean cvar controls the
  availability of the Disruptor weapon to players. The Disruptor is
  a weapon that was cut from Ground Zero during development but all
  of its code and assets were still present in the source code and
  the released game. This is basically a player-held version of the
  2nd Widow boss' tracker weapon - a black-ish ball of energy.
  When this cvar is set to 1 you can use the "give Disruptor" and
  "give rounds X" commands to give yourself the weapon and its ammo,
  and its items, weapon\_disintegrator and ammo\_disruptor, can be
  spawned in maps (in fact, some official Ground Zero maps contain
  these entities). This cvar is set to 0 by default.

* **nextdemo**: Defines the next command to run after maps from the
  `nextserver` list. By default this is set to the empty string.

* **nextserver**: Used for looping the introduction demos.


## Audio

* **al_device**: OpenAL device to use. In most cases there's no need to
  change this, since the default device is normally the correct choice.

* **al_driver**: OpenAL library to use. This is useful if for some
   reasons several OpenAL libraries are available and Quake II picks the
   wrong one. The given value is the name of the library, for example
   `libopenal.so.1`.

* **ogg_enable**: Enable Ogg/Vorbis music playback.

* **ogg_ignoretrack0**: Normally Quake II disables the background music
  if a major objective has been archived by setting the music track to
  0.  Setting this cvar to `1` disables this behavior, the music keeps
  playing.

* **ogg_shuffle**: Ogg/Vorbis playback mode. Supported modes are:
  - `0`: Loop the current track (the default).
  - `1`: Play the current track once, then stop.
  - `2`: Play all available tracks in a linear sequence.
  - `3`: Shuffle through the available tracks, never play the same track
         twice in a row.
  - `4`: Shuffle through the available tracks, may play the same track
         multiple times in a row.

* **s_doppler**: If set to `1` doppler effects are enabled. This is only
  supported by the OpenAL sound backend.

* **s_openal**: Use OpenAL for sound playback. This is enabled by
  default. OpenAL gives a huge quality boost over the classic sound
  system and supports surround speakers and HRTF for headphones. OpenAL
  is much more reliable than the classic sound system, especially on
  modern systems like Windows 10 or Linux with PulseAudio or Pipewire.

* **s_sdldriver**: Can be set to the name of a SDL audio driver. If set
  to `auto` (the default), SDL chooses the driver. If set to anything
  else the given driver is forced, regardless if supported by SDL or the
  platform or not.

* **s_underwater**: Dampen sounds if submerged. Enabled by default.

* **s_occlusion_strength**: If set bigger than `0` sound occlusion effects
  are enabled. This is only supported by the OpenAL sound backend. By
  default this cvar is disabled (set to 0).

* **s_reverb_preset**: Enable reverb effect. By default this cvar is
  disabled (set to `-1`). Possible values:
  - `-2`: Auto reverb effect select,
  - `-1`: Disable reverb effect,
  - `>=0`: select predefined effect.

* **s_bsp_soundpos**: Controls the positioning of sounds played by BSP
  entities (doors, elevators, ...). Internally the game treats the
  positioning of BSP entities differently. There are 3 modes to choose
  from. Values 0, 1 and 2.

  - **Mode 0: Original behavior**: Sound events play at the center of
    the bbox, loop sound plays at origin (0, 0, 0). This is how you are
    used to it functioning from before this cvar was added. This is
    considered a bug/code oversight, hence why the default value is 1.
  - **Mode 1: Bbox center**: Both sound events and loop sounds play at
    bbox center. While this works better than mode 0, very long/large
    bboxes, like the elevator at the start of jail4, will play the sound
    too far away for you to hear it.
  - **Mode 2: Bbox edge**: Sound events and loop sounds play at the edge
    of the bbox, closest to the player. Now you hear the bbox itself,
    rather than the point at its center.

  Default mode is 1.

* **cl_audiopaused**: If set to `1` the sounds pause when the game does.


## Graphics (all renderers)

* **cin_force43**: If set to `1` (the default) cinematics are displayed
  with an aspect ratio of 4:3, regardless what the actual windows size
  or resolution is.

* **cl_gun**: Decides whether the gun is drawn. If set to `0` the gun
  is omitted. If set to `1` the gun is only drawn if the FOV is equal
  or smaller than 90. This was the default with Vanilla Quake II. If set
  to `2` the gun is drawn regardless of the FOV. This is the default
  in Yamagi Quake II.

* **fov**: Sets the field of view.

* **r_gunfov**: The weapons are rendered with a custom field of view,
  independently of the global **fov**, so they are not distorted at high
  FOVs. A value of `75` should look identical to the old code at `fov
  90`, it defaults to `80` because that looks a bit better. Set to `-1`
  for the same value as `fov`.

* **horplus**: If set to 1 (the default) the horplus algorithm is used
  to calculate an optimal horizontal and vertical field of view,
  independent of the window or screen aspect ratio or resolution.

* **r_consolescale** / **r_hudscale** / **r_menuscale** and
  **crosshair_scale**: Scale the console, the HUD, the menu and the
  crosshair. The value given is the scale factor, a factor of `1` means
  no scaling. Values greater `1` make the objects bigger, values lower 1
  smaller. The special value `-1` (default) sets the optimal scaling
  factor for the current resolution. All cvars are set through the
  scaling slider in the video menu.

* **r_customheight** / **r_customwidth**: Specifies a custom resolution,
  the windows will be *r_customheight* pixels high and *r_customwidth*
  pixels wide. Set *r_mode* to `-1` to use the custom resolution.

* **r_farsee**: Normally Quake II renders only up to 4096 units. If set
  to `1` the limit is increased to 8192 units. This helps with some
  custom maps and is problematic with other custom maps.

* **r_fixsurfsky**: Some maps misuse sky surfaces for interior
  lighting. The original renderer had a bug that made such surfaces
  mess up the lighting of entities near them. If set to `0` (the
  default) the bug is there and maps look like their developers
  intended. If set to `1` the bug is fixed and the lighting correct.

* **r_vsync**: Enables the vsync: frames are synchronized with
  display refresh rate, should (but doesn't always) prevent tearing.
  Set to `1` for normal vsync and `2` for adaptive vsync.

* **r_anisotropic**: Anisotropic filtering. Possible values are
  dependent on the GPU driver, most of them support `1`, `2`, `4`, `8`
  and `16`. Anisotropic filtering gives a huge improvement to texture
  quality by a negligible performance impact.

* **r_msaa_samples**: Full scene anti aliasing samples. The number of
  samples depends on the GPU driver, most drivers support at least `2`,
  `4` and `8` samples. If an invalid value is set, the value is reverted
  to the highest number of samples supported. Especially on OpenGL 3.2
  anti aliasing is expensive and can lead to a huge performance hit, so
  try setting it to a lower value if the framerate is too low.

* **r_videos_unfiltered**: If set to `1`, don't use bilinear texture
  filtering on videos (defaults to `0`).

* **r_2D_unfiltered**: If set to `1`, don't filter textures of 2D
  elements like menus and the HUD (defaults to `0`).

* **r_lerp_list**: List separated by spaces of 2D textures that *should*
  be filtered bilinearly, even if `r_2D_unfiltered` is set to `1`.

* **r_nolerp_list**: List separated by spaces of textures omitted from
  bilinear filtering (mostly relevant if `r_2D_unfiltered` is `0`).
  Used by default to exclude the console and HUD font and crosshairs.
  Make sure to include the default values when extending the list.

* **r_retexturing**: If set to `1` (the default) and a retexturing pack
  is installed, the high resolution textures are used.

* **r_scale8bittextures**: If set to `1`, scale up all 8bit textures.

* **r_shadows**: Enables rendering of shadows. Quake IIs shadows are
  very simple and are prone to render errors.

* **vid_displayrefreshrate**: Sets the displays refresh rate. The
  default `-1` let the game determine the refresh rate automatically.
  Often the default setting is okay, but some graphics drivers report
  wrong refresh rates. For example 59hz are reported while the display
  has 59.95hz.

* **vid_gamma**: The value used for gamma correction. Higher values look
  brighter. The OpenGL 3.2 OpenGL ES3 and Vulkan renderers apply this to
  the window in realtime via shaders (on all platforms). When the game
  is built against SDL2, the OpenGL 1.4 renderer uses "hardware gamma"
  when available, increasing the brightness of the whole screen. When
  the game is built using SDL3, the OpenGL 1.4 renderer requires a
  `vid_restart` since gamma values are precomputed at renderer start
  only. On MacOS the situation is similar; `vid_restart` is required.
  This cvar is also set by the brightness slider in the video menu.

* **vid_fullscreen**: Sets the fullscreen mode. When set to `0` (the
  default) the game runs in window mode. When set to `1` the games
  switches the display to the requested resolution. That resolution
  must be supported by the display, otherwise the game tries several
  steps to recover. When set to `2` a fullscreen window is created.
  It's recommended to use the displays native resolution with the
  fullscreen window, use `r_mode -2` to switch to it.

* **vid_highdpiaware**: When set to `1` the client is high DPI aware
  and scales the window (and thus the requested resolution) by the
  scaling factor of the underlying display. Example: The displays
  scaling factor is 1.25 and the user requests 1920x1080. The client
  will render at 1920\*1.25x1080\*1.25=2400x1350.  
  When set to `0` the client leaves the decision if the window should
  be scaled to the underlying compositor. Scaling applied by the
  compositor may introduce blur and sluggishness.  
  Currently high dpi awareness is only supported under Wayland.  
  Defaults to `0` when build against SDL2 and to `1` when build against
  SDL3.

* **vid_maxfps**: The maximum framerate. *Note* that vsync (`r_vsync`) 
  also restricts the framerate to the monitor refresh rate, so if vsync
  is enabled, the game won't render more than frame than the display can
  show. Defaults to `300`.  
  Related to this: *cl_maxfps* and *cl_async*.

* **vid_pauseonfocuslost**: When set to `1` the game is paused as soon
  as it's window looses focus. It will work only in situation were the
  game can be paused, e.g. not in multiplayer games. Defaults to `0`.

* **vid_renderer**: Selects the renderer library. Possible options are
  `gl3` (the default) for the OpenGL 3.2 renderer, `gles3` for the
  OpenGL ES3 renderer, gl1 for the original OpenGL 1.4 renderer and
  `soft` for the software renderer.


## Graphics (GL renderers only)

* **gl_zfix**: Sometimes two or even more surfaces overlap and flicker.
  If this cvar is set to `1` the renderer inserts a small gap between
  the overlapping surfaces to mitigate the flickering. This may make
  things better or worse, depending on the map.

* **gl_polyblend**: Toggles the palette blending effect, a.k.a. the
  "flash" you see when getting injured or picking up an item. In GL1 is
  also used for looking underwater. Default is `1` (enabled).

* **gl_znear**: Sets the distance to the *near depth clipping plane* of
  the player view. Reducing it may allow some weapon animations to not
  get "clipped" by the player view (e.g. railgun firing), at the risk
  of heavy glitches with some hardware configurations. Default is `4`.

* **gl_texturemode**: How textures are filtered.

  - `GL_NEAREST`: No filtering (using value of *nearest* source pixel),
    mipmaps not used
  - `GL_LINEAR`: Bilinear filtering, mipmaps not used
  - `GL_LINEAR_MIPMAP_NEAREST`: The default - Bilinear filtering when
    scaling up, using mipmaps with nearest/no filtering when scaling
    down

  Other supported values: `GL_NEAREST_MIPMAP_NEAREST`,
  `GL_NEAREST_MIPMAP_LINEAR`, `GL_LINEAR_MIPMAP_LINEAR`


## Graphics (OpenGL 1.4 and OpenGL ES1 only)

* **gl1_intensity**: Sets the color intensity. Must be a floating point
  value, at least `1.0` - default is `2.0`. Applied when textures are
  loaded, so it needs a `vid_restart`.

* **gl1_minlight**: Sets the minimum light level on screen. Increasing
  this illuminates darker scenes, at the expense of the atmosphere.
  Posible values go from `0` (default, show the full spectrum of light
  & dark) to `255` (equal to `r_fullbright 1`). Requires `vid_restart`.

* **gl1_multitexture**: Enables (`1`, default) the blending of color and
  light textures on a single drawing pass; disabling this (`0`) does one
  pass for color and another for light. Requires a `vid_restart` when
  changed.

* **gl1_overbrightbits**: Enables overbright bits, brightness scaling of
  lightmaps and models. Higher values make shadows less dark. Possible
  values are `0` (no overbright bits), `1` (more correct lighting for
  liquids), `2` (scale lighting by factor 2), and `4` (scale by factor
  4). Applied in realtime, does not need `vid_restart`.

* **gl1_particle_square**: If set to `1` particles are rendered as
  squares.

* **gl1_stencilshadow**: If `gl_shadows` is set to `1`, this makes them
  look a bit better (no flickering) by using the stencil buffer. Does
  not work when `gl1_stereo` is `3`, `4` or `5`.

* **gl1_waterwarp**: Intensity of the "squeeze/stretch" effect on the
  FOV when diving underwater. Can be any floating point number, `0`
  disables it (Vanilla Quake II look). Default `1.0`.

* **gl1_lightmapcopies**: When enabled (`1`), keep 3 copies of the same
  lightmap rotating, shifting to another one when drawing a new frame.
  Meant for mobile/embedded devices, where changing textures just shown
  (dynamic lighting) causes slowdown. By default in GL1 is disabled,
  while in GLES1 is enabled. Needs `gl1_multitexture 1` & `vid_restart`.

* **gl1_discardfb**: If `1`, clear color, depth and stencil buffers at
  the start of a frame, and discard them at the end if possible. If
  `2`, do only depth and stencil, no color. Increases performance in
  mobile / embedded. Default in GL1 is `0`, while in GLES1 is `1`.


## Graphics (OpenGL 3.2 and OpenGL ES3 only)

* **gl3_debugcontext**: Enables the OpenGL 3.2 renderers debug context,
  e.g. prints warnings and errors emitted by the GPU driver.  Not
  supported on macOS. This is a pure debug cvar and slows down
  rendering.

* **gl3_intensity**: Sets the color intensity used for 3D rendering.
  Similar to OpenGL 1.4 `gl1_intensity`, but more flexible: can be any
  value between 0.0 (completely dark) and 256.0 (very bright).  Good
  values are between `1.0` and `2.0`, default is `1.5`.  Applied in
  realtime via shader, so it does not need a `vid_restart`.

* **gl3_intensity_2D**: The same for 2D rendering (HUD, menu, console,
  videos)

* **gl3_overbrightbits**: Enables overbright bits, brightness scaling of
  lightmaps and models. Higher values make shadows less dark. Similar
  to OpenGL 1.4 `gl1_overbrightbits`, but allows any floating point
  number. Default is `1.3`. In the OpenGL 3.2 renderer, no lighting
  fixes for water are needed, so `1.0` has no special meaning.

* **gl3_particle_size**: The size of particles - Default is `40`.

* **gl3_particle_fade_factor**: "softness" of particles: higher values
  look less soft. Defaults to `1.2`. A value of `10` looks similar to
  the OpenGL 1.4 particles.

* **gl3_particle_square**: If set to `1`, particles are rendered as
  squares, like in the old software renderer or Quake 1. Default is `0`.

* **gl3_colorlight**: When set to `0`, the lights and lightmaps are
  colorless (greyscale-only), like in the original soft renderer.
  Default is `1`.

* **gl3_usefbo**: When set to `1` (the default), an OpenGL Framebuffer
  Object is used to implement a warping underwater-effect (like the
  software renderer has). Set to `0` to disable this, in case you don't
  like the effect or it's too slow on your machine.


## Graphics (Software only)

* **sw_gunzposition**: Z offset for the gun. In the original code this
  was always `0`, which will draw the gun too near to the player if a
  custom gun field of view is used. Defaults to `8`, which is more or
  less optimal for the default gun field of view of 80.

* **sw_colorlight**: enable experimental color lighting.


## Gamepad

* **in_initjoy**: Toggles initialization of game controller. Default is
  `1`, which enables gamepad usage; `0` disables its detection at
  startup. Can only be set from command line.

* **joy_escbutton**: Defines which button is used in the gamepad as
  the `Esc` key, to pull the main menu and 'cancel' / 'go back' on its
  options. Valid values are `0` = Start / Menu / Plus (default), `1` =
  Back / Select / Minus, or `2` = Guide / Home / PS. Requires a game
  restart, or gamepad replug, when changed.

* **joy_labels**: Defines style of button labels in binding menus. Note
  that binding through console only uses the SDL nomenclature (`0`).
  Default is `-1`, which requires at least SDL 2.0.12 to work.
  - `-1`: *Autodetect*, sets to `0` if gamepad type isn't detected
  - `0`: *SDL*, face buttons appear as cardinal points
  - `1`: *Xbox*, with One / Series X / S labels
  - `2`: *Playstation*, 4 & 5 format
  - `3`: *Switch*, traditional Nintendo button format

* **joy_confirm**: Style of *confirm* and *cancel* buttons in menus. As
  with the previous one, SDL 2.0.12 is required for `-1` to work.
  - `-1`: *Autodetect*, sets to `1` if Nintendo gamepad, `0` otherwise
  - `0`: *Standard style*: SOUTH to confirm, EAST to cancel
  - `1`: *Japanese style*: EAST to confirm, SOUTH to cancel

* **joy_sensitivity**: Simple sensitivity adjustment for yaw and pitch.
  Changing this applies a preset that adjusts the advanced cvars listed
  below. Default `3`.

* **joy_advanced**: Show or hide the following advanced sensitivity
  cvars in the menu:
  - **joy_yawspeed**: How quickly the view turns left or right in
    degrees per second. Default `160`.
  - **joy_pitchspeed**: How quickly the view looks up or down in
    degrees per second. Default `120`.
  - **joy_extra_yawspeed**: Additional yaw speed that is applied when
    the stick is fully deflected. Default `220`.
  - **joy_extra_pitchspeed**: Additional pitch speed that is applied
    when the stick is fully deflected. Default `0`.
  - **joy_ramp_time**: Ramp-up time required for any extra yaw or pitch
    speed to be fully applied. Default `0.35` (350 milliseconds).

* **joy_layout**: Allows to select the stick layout of the gamepad.
  - `0`: *Default*, left stick moves, right aims
  - `1`: *Southpaw*, left stick aims, right moves
  - `2`: *Legacy*, left moves forward/backward and turns, right strafes
         and looks up/down
  - `3`: *Legacy Southpaw*, swapped sticks version of previous one
  - `4`: *Flick Stick*, left stick moves, right checks your surroundings
         in 360º, gyro required for looking up/down
  - `5`: *Flick Stick Southpaw*, swapped sticks version of last one

* **joy_forwardsensitivity**: Ramp-up proportion for moving forward and
  backward; affected axis depends on `joy_layout` value. Default `1.0`.

* **joy_sidesensitivity**: Ramp-up proportion for moving side to side;
  affected axis depends on `joy_layout` value. Default `1.0`.

* **joy_left_deadzone** / **joy_right_deadzone**: Inner, circular
  deadzone for each stick, where inputs below this radius will be
  ignored. Default is `0.16` (16% of possible stick travel).

* **joy_left_snapaxis** / **joy_right_snapaxis**: Ratio on the value of
  one axis (X or Y) to snap you to the other. It creates an axial
  deadzone with the shape of a "bowtie", which will help you to do
  perfectly horizontal or vertical movements the more you mark a
  direction with the stick. Increasing this too much will reduce speed
  for the diagonals, but will help you to mark 90º/180º turns with Flick
  Stick. Default `0.15`.

* **joy_left_expo** / **joy_right_expo**: Exponents on the response
  curve on each stick. Increasing this will make small movements to
  represent much smaller inputs, which helps precision with the sticks.
  `1.0` is linear. Default `2.0` (quadratic curve).

* **joy_outer_threshold**: Defines the outer boundary where stick input
  is considered to be at maximum. A small amount may be needed for some
  controllers. Additionally, this cvar defines the boundary where any
  extra yaw or pitch speed is applied. Default `0.02` (outer 2% of
  stick range).

* **joy_trigger**: Trigger pressure required to register it as a
  button press. Default `0.2` (20% of total trigger displacement).

* **joy_flick_threshold**: Used only with Flick Stick, specifies the
  distance from the center of the stick that will make the player flick
  or rotate. Default `0.65` (65%).

* **joy_flick_smoothed**: Flick Stick only, rotations below this angle
  (in degrees per frame) will be smoothed. Reducing this will increase
  responsiveness at the cost of jittery movement. Most gamepads will
  work nicely with a value between `4` and `16`. Default `16`.

* **gyro_mode**: Operation mode for the gyroscope sensor of the game
  controller. Options are `0` = always off, `1` = off with the
  `+gyroaction` bind to enable, `2` = on with `+gyroaction` to
  disable (default), `3` = always on.

* **gyro_space**: How motion input is converted into yaw and pitch
  rotations. `0` = local space, `1` = player space (default),
  `2` = world space.

* **gyro_local_roll**: How gyro roll contributes to turning when using
  local space. `0` = off, `1` = on (default), `2` = inverted.

* **gyro_yawsensitivity**: How quickly the view turns left or right. A
  value of 1.0 means one full rotation of the controller rotates the
  in-game view by 360 degrees. Default `2.5`.

* **gyro_pitchsensitivity**: How quickly the view looks up or down.
  Default `2.5`.

* **gyro_tightening**: Threshold of rotation, in degrees per second,
  below which gyro input is reduced to stabilize aiming. Default `3.5`.

* **gyro_smoothing**: Threshold of rotation, in degrees per second,
  below which gyro input is averaged to reduce jitter. Default `2.5`.

* **gyro_smoothing_window**: The smoothing window size, in seconds.
  Default `0.125`.

* **gyro_acceleration**: Toggles the use of gyro acceleration.
  `0` = off (default), `1` = on.

* **gyro_accel_multiplier**: Multiplier that increases gyro sensitivity
  when rotating the controller quickly. Applied linearly, ramping up
  from the lower to upper threshold. Default `2.0`.

* **gyro_accel_lower_thresh**: The lower input threshold, in degrees per
  second, from which acceleration starts ramping up towards the upper
  threshold. Default `0`.

* **gyro_accel_upper_thresh**: The upper input threshold, in degrees per
  second, at which full acceleration is applied. Default `75`.

* **gyro_calibration_(a/x/y/z)**: Offset values on each axis of the gyro
  which helps it reach true "zero movement", complete stillness. These
  values are wrong if you see your in-game view "drift" when leaving
  the controller alone. As these vary by device, it's better to use
  'calibrate' in the 'gamepad' -> 'gyro' menu to set them.

* **joy_haptic_magnitude**: Haptic magnitude value, By default this cvar
  is `0.0` or disabled. Valid values are positive, e.g. 0..2.0.

* **joy_haptic_filter**: List of sound file names produced haptic
  feedback separated by space. `*` could be used for replace part of
  file name as regular expression. `!` at the beginning of value could
  be used for skip file name equal to value.

* **joy_haptic_distance**: Haptic maximum effect distance value, By
  default this cvar is `100.0`. Any positive value is valid. E.g. effect
  of shoot to a barrel has 58 points when player stay near the barrel.

* **s_feedback_kind**: Select kind of controller feedback to use. By
  default this cvar is `0`. Possible values:
  - `0`: Rumble feedback,
  - `1`: Haptic feedback.


## cvar operations

cvar operations are special commands that allow the programmatic
manipulation of cvar values. They can be used for scripting and the
like.

* **dec <cvar> [val]**: Decrements the given cvar by `1` or the optional
  value `val`.

* **inc <cvar> [val]**: Increments the given cvar by `1` or the optional
  value `val`.

* **reset <cvar>**: Reset the given cvar to it's default value.

* **resetall**: Reset all known cvar to their default values.

* **toggle <cvar> [val0] [val1]**: Toggle the given cvar between `0` and
  `1`. If the optional arguments `val0` and `val1` are given the given
  cvar is toggled between them.
