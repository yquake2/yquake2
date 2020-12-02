# Yamagi Quake II Console Variables

This lists explains most console variables (cvars) added by Yamagi
Quake II. Most of the original clients (Vanilla Quake II) cvars are
still in place, however due to architectural changes some of them
have been renamed. The prefixes are:

* No prefix: General stuff.
* `cl_`: Client.
* `gl_`: Common to all OpenGL renderers.
* `gl1_`: OpenGL 1.4 renderer.
* `gl3_`: OpenGL 3.2 renderer.
* `ogg_`: Ogg/Vorbis music playback.
* `r_`: Common to all renderers.
* `s_`: Sound system.
* `sw_`: Software renderer.
* `vid_`: Video backend.

All cvars may be given at command line through `+set cvar value` or typed
into the console. The console can be opended with *Left Shift + Esc*.

Keep in mind that some cvars need quotation marks around the arguments.
When giving such cvars at the command line the argument string must be
surrounded by ticks. For example `+set sv_maplist '"q2dm1 q2dm2"'`.


## Command line arguments

These are not console variables, they cannot be entered into the
console, only be given through the command line at startup. While cvars
are prefixed with a `+`, arguments are starting with a `-`. For example
it's `+set busywait 0` (setting the `busywait` cvar) and `-portable`
(setting the `portable` argument).

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
  time for the next frame. The later is more CPU friendly but rather
  inaccurate, especially on Windows. Use with care.

* **cl_anglekicks**: If set to `0` angle kicks (weapon recoil, damage
  hits and the like) are ignored. Cheat protected. Defaults to `1`.

* **cl_async**: If set to `1` (the default) the client is asynchronous.
  The client framerate is fixed, the renderer framerate is variable.
  This makes it possible to renderer as many frames as desired without
  any physics and movement problems. The client framerate is controlled
  by *cl_maxfps*, set to `60` by default. The renderer framerate is
  controlled by *vid_maxfps*. There are two constraints:

  * *vid_maxfps* must be the same or greater than *cl_maxfps*.
  * In case that the vsync is active, *vid_maxfps* must not be lower
	than the display refresh rate.

  Both constraints are enforced.

  If *cl_async* is set to `0` *vid_maxfps* is the same as *cl_maxfps*,
  use *cl_maxfps* to set the framerate.

* **cl_loadpaused**: If set to `1` (the default) the client is put into
  pause mode during single player savegame load. This prevents monsters
  and the environment from hurting the player while the client is still
  connecting. If set to `2` the client stays in pause mode after
  loading. If set to `0` pause mode is never entered, this is the
  Vanilla Quake II behaviour.

* **cl_showfps**: Shows the framecounter. Set to `2` for more and to
  `3` for even more informations.

* **in_grab**: Defines how the mouse is grabbed by Yamagi Quake IIs
  window. If set to `0` the mouse is never grabbed and if set to `1`
  it's always grabbed. If set to `2` (the default) the mouse is grabbed
  during gameplay and released otherwise (in menu, videos, console or if
  game is paused).

* **coop_pickup_weapons**: In coop a weapon can be picked up only once.
  For example, if the player already has the shotgun they cannot pickup
  a second shotgun found at a later time, thus not getting the ammo that
  comes with it. This breaks the balacing. If set to `1` a weapon can be
  picked up if a) the player doesn't have it or b) it wasn't already
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

* **g_disruptor (Ground Zero only)**: This boolean cvar controls the
  availability of the Disruptor weapon to players. The Disruptor is
  a weapon that was cut from Ground Zero during development but all
  of its code and assets were still present in the source code and
  the released game. This is basically a player-held version of the
  2nd Widow boss' tracker weapon - a black-ish ball of energy.
  When this cvar is set to 1 you can use the "give Disruptor" and
  "give rounds X" commands to give yourself the weapon and its ammo,
  and its items, weapon_disintegrator and ammo_disruptor, can be
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

* **s_doppler**: If set to `1` doppler effects are enabled. This is only
  supported by the OpenAL sound backend.

* **s_openal**: Use OpenAL for sound playback. This is enabled by
  default. OpenAL gives a huge quality boost over the classic sound
  system and supports surround speakers and HRTF for headphones. OpenAL
  is much more reliable than the classic sound system, especially on
  modern systems like Windows 10 or Linux with PulseAudio.

* **s_underwater**: Dampen sounds if submerged. Enabled by default.


## Graphics (all renderers)

* **vid_displayrefreshrate**: Sets the displays refresh rate. The
  default `-1` let the game determine the refresh rate automatically.
  Often the default setting is okay, but some graphics drivers report
  wrong refresh rates. For example 59hz are reported while the display
  has 59.95hz.

* **vid_renderer**: Selects the renderer library. Possible options are
  `gl1` (the default) for the old OpenGL 1.4 renderer, `gl3` for the new
  OpenGL 3.2 renderer and `soft` for the software renderer.

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
  FOVs.  A value of `75` should look identical to the old code at `fov
  90`, it defaults to `80` because that looks a bit better. Set to `-1`
  for the same value as `fov`.

* **horplus**: If set to 1 (the default) the horplus algorithm is used
  to calculate an optimal horizontal and vertical field of view,
  independent of the window or screen aspect ratio or resolution.

* **vid_gamma**: The value used for gamma correction. Higher values look
  brighter. The OpenGL 1.4 and software renderers use "Hardware Gamma",
  setting the Gamma of the whole screen to this value in realtime
  (except on MacOS where it's applied to textures on load and thus needs
  a `vid_restart` after changing). The OpenGL 3.2 renderer applies this
  to the window in realtime via shaders (on all platforms).  This is
  also set by the brightness slider in the video menu.

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

* **vid_maxfps**: The maximum framerate, if `cl_async` is `1`. Otherwise
  `cl_maxfps` is used as maximum framerate. See `cl_async` description
  above for more information.  *Note* that vsync (`r_vsync`) also
  restricts the framerate to the monitor refresh rate, so if vsync is
  enabled, the game won't render more than frame than the display can
  show.

* **r_vsync**: Enables the vsync: frames are synchronized with
  display refresh rate, should (but doesn't always) prevent tearing.
  Set to `1` for normal vsync and `2` for adaptive vsync.


## Graphics (GL renderers only)

* **gl_anisotropic**: Anisotropic filtering. Possible values are
  dependent on the GPU driver, most of them support `1`, `2`, `4`, `8`
  and `16`. Anisotropic filtering gives a huge improvement to texture
  quality by a negligible performance impact.

* **gl_fixsurfsky**: Some maps misuse sky surfaces for interior
  lightning. The original renderer had a bug that made such surfaces
  mess up the lightning of entities near them. If set to `0` (the
  default) the bug is there and maps look like their developers
  intended. If set to `1` the bug is fixed and the lightning correct.

* **gl_msaa_samples**: Full scene anti aliasing samples. The number of
  samples depends on the GPU driver, most drivers support at least `2`,
  `4` and `8` samples. If an invalid value is set, the value is reverted
  to the highest number of samples supported. Especially on OpenGL 3.2
  anti aliasing is expensive and can lead to a huge performance hit, so
  try setting it to a lower value if the framerate is too low.

* **gl_nolerp_list**: list separate by spaces of textures omitted from
  bilinear filtering. Used by default to exclude the console and HUD
  fonts.  Make sure to include the default values when extending the
  list.

* **gl_retexturing**: If set to `1` (the default) and a retexturing pack
  is installed, the high resolution textures are used.

* **gl_shadows**: Enables rendering of shadows. Quake IIs shadows are
  very simple and are prone to render errors.

* **gl_zfix**: Sometimes two or even more surfaces overlap and flicker.
  If this cvar is set to `1` the renderer inserts a small gap between
  the overlapping surfaces to mitigate the flickering. This may make
  things better or worse, depending on the map.


## Graphics (OpenGL 1.4 only)

* **gl1_intensity**: Sets the color intensity. Must be a floating point
  value, at least `1.0` - default is `2.0`. Applied when textures are
  loaded, so it needs a `vid_restart`.

* **gl1_overbrightbits**: Enables overbright bits, brightness scaling of
  lightmaps and models. Higher values make shadows less dark. Possible
  values are `0` (no overbright bits), `1` (more correct lighting for
  water), `2` (scale by factor 2) and `3` (scale by factor 3). Applied
  in realtime, does not need `vid_restart`.

* **gl1_particle_square**: If set to `1` particles are rendered as
  squares.

* **gl1_stencilshadow**: If `gl_shadows` is set to `1`, this makes them
  look a bit better (no flickering) by using the stencil buffer.


## Graphics (OpenGL 3.2 only)

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
  number.  Default is `1.3`. In the OpenGL 3.2 renderer, no lighting
  fixes for water are needed, so `1.0` has no special meaning.

* **gl3_particle_size**: The size of particles - Default is `40`.

* **gl3_particle_fade_factor**: "softness" of particles: higher values
  look less soft. Defaults to `1.2`. A value of `10` looks similar to
  the OpenGL 1.4 particles.

* **gl3_particle_square**: If set to `1`, particles are rendered as
  squares, like in the old software renderer or Quake 1. Default is `0`.

## Graphics (Software only)

* **sw_gunzposition**: Z offset for the gun. In the original code this
  was always `0`, which will draw the gun too near to the player if a
  custom gun field of few is used. Defaults to `8`, which is more or
  less optimal for the default gun field of view of 80.

## Graphics (Vulkan only)

* **vk_validation**: Toggle validation layers:
  * `0` - disabled (default in Release)
  * `1` - only errors and warnings
  * `2` - best-practices validation

* **vk_retexturing**: Apply retexturing:
  * `0` - dont use retexturing logic and dont load the high resolution
          textures,
  * `1` - load the high resolution textures if pack is installed.
  * `2` - try to load the pack or scale up all 8bit textures if pack is
          not installed.

* **vk_nolerp_list**: list separate by spaces of textures omitted from
  bilinear filtering. Used by default to exclude the console and HUD
  fonts.  Make sure to include the default values when extending the
  list.

* **vk_strings**: Print some basic Vulkan/GPU information.

* **vk_mem**: Print dynamic vertex/index/uniform/triangle fan buffer
  memory usage statistics.

* **vk_device**: Specify index of the preferred Vulkan device on systems
  with multiple GPUs:
  * `-1` - prefer first DISCRETE_GPU (default)
  * `0..n` - use device #n (full list of devices is returned by
    `vk_strings` command)

* **vk_msaa**: Toggle MSAA:
  * `0` - off (default)
  * `1` - MSAAx2
  * `2` - MSAAx4
  * `3` - MSAAx8
  * `4` - MSAAx16

* **vk_sampleshading**: Toggle sample shading for MSAA. (default: `1`)

* **vk_flashblend**: Toggle the blending of lights onto the environment.
  (default: `0`)

* **vk_polyblend**: Blend fullscreen effects: blood, powerups etc.
  (default: `1`)

* **vk_skymip**: Toggle the usage of mipmap information for the sky
  graphics. (default: `0`)

* **vk_finish**: Inserts a `vkDeviceWaitIdle()` call on frame render
  start (default: `0`). Don't use this, it's there just for the sake of
  having a `gl_finish` equivalent!

* **vk_custom_particles**: Toggle particles type:
  * `0` - textured triangles for particle rendering
  * `1` - between using POINT_LIST (default)
  * `2` - textured square for particle rendering

* **vk_particle_size**: Rendered particle size. (default: `40`)

* **vk_particle_att_a**: Intensity of the particle A attribute.
  (default: `0.01`)

* **vk_particle_att_b**: Intensity of the particle B attribute.
  (default: `0`)

* **vk_particle_att_c**: Intensity of the particle C attribute.
 (default: `0.01`)

* **vk_particle_min_size**: The minimum size of a rendered particle.
 (default: `2`)

* **vk_particle_max_size**: The maximum size of a rendered particle.
  (default: `40`)

* **vk_shadows**: Draw experimental entity shadows. (default: `0`)

* **vk_picmip**: Shrink factor for the textures. (default: `0`)

* **vk_dynamic**: Use dynamic lighting. (default: `1`)

* **vk_showtris**: Display mesh triangles. (default: `0`)

* **vk_lightmap**: Display lightmaps. (default: `0`)

* **vk_aniso**: Toggle anisotropic filtering. (default: `1`)

* **vk_postprocess**: Toggle additional color/gamma correction.
  (default: `1`)

* **vk_mip_nearfilter**: Use nearest-neighbor filtering for mipmaps.
  (default: `0`)

* **vk_texturemode**: Change current texture filtering mode:
  * `VK_NEAREST` - nearest-neighbor interpolation, no mipmaps
  * `VK_LINEAR` - linear interpolation, no mipmaps
  * `VK_MIPMAP_NEAREST` - nearest-neighbor interpolation with mipmaps
  * `VK_MIPMAP_LINEAR` - linear interpolation with mipmaps (default)

* **vk_lmaptexturemode**: Same as `vk_texturemode` but applied to
  lightmap textures.

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
