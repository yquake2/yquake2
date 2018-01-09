Yamagi Quake II Console Variables
=================================

This lists explains most console variables (cvars) added by Yamagi
Quake II. Most of the original clients (Vanilla Quake II) cvars are
still in place, however the `r_*` renderer cvars have been renamed
to `gl_*` and there are cvars specific to the OpenGL3.2 renderer that
start with `gl3_`.
Please note: There's normally no need to change any cvar! Use the menu instead.

General:
--------

* **basedir**: Directory from which the game data is loaded. Can be used
  in startup scripts, to test binaries, etc. If not set, the directory
  containing the binaries is used.  
  To use this cvar, set it at startup, like `./quake2 +set basedir /path/to/quake2`

* **cl_async**: If set to `1` (the default) the client is asynchronous.
  The client framerate is fixed, the renderer framerate is variable.
  This makes it possible to renderer as many frames as desired without
  any physics and movement problems. The client framerate is controlled
  by *cl_maxfps*, set to `60` by defaut. The renderer framerate is
  controlled by *vid_maxfps*. There are two constraints: *vid_maxfps* must
  be the same or greater than *cl_maxfps*. In case that the vsync is
  active, *vid_maxfps* must not be lower than the display refresh rate.
  If *cl_async* is set to `0` *vid_maxfps* is the same as *cl_maxfps*, use
  *cl_maxfps* to set the framerate.

* **cl_showfps**: Shows the framecounter. The shown value is rather
  inaccurate and gets less precise with higher framerates, as it only
  measures full milliseconds.

* **in_grab**: Defines how the mouse is grabbed by Quake IIs window. If
  set to `0` the mouse is never grabbed and if set to `1` it's always
  grabbed. If set to `2` (the default) the mouse is grabbed during
  gameplay and released otherwise (in menu, console or if game is paused).


Audio:
------

* **al_device**: OpenAL device to use. In most cases there's no need to
  change this, since the default device is normally the right choice.

* **al_driver**: OpenAL library to use. This is usefull if for some
   reasons several OpenAL libraries are available and Quake II picks the
   wrong one. The given value is the name of the library, for example
   `libopenal.so.1`.

* **ogg_enable**: Enable Ogg/Vorbis music playback.

* **ogg_ignoretrack0**: Normally Quake II disabled the background music
  if a major objective has been archived by setting the music track to 0.  
  Setting this cvar to `1` disables this behavior, the music keeps playing.

* **s_doppler**: If set to `1` (the default) doppler effects are enabled.
  This is only supported by the OpenAL sound backend.

* **s_openal**: Use OpenAL for sound playback. This is enabled by
  default. OpenAL gives a huge quality boost over the classic sound
  system and supports surround speakers and HRTF.

* **s_underwater**: Dampen sounds if submerged. Enabled by default.


Graphics (all renderers):
-------------------------

* Most old `r_*` cvars, but renamed to `gl_*`

* **vid_renderer**: Selects the renderer library. Possible options are
  `gl1` (the default) for the old OpenGL 1.4 renderer and `gl3` for 
  the new OpenGL 3.2 renderer.

* **cin_force43**: If set to `1` (the default) cinematics are displayed
  with an aspect ratio of 4:3, regardless what the actual windows size
  or resolution is.

* **cl_gun**: Decides whether the gun is drawn. If set to `0` the gun
  is omitted. If set to `1` the gun is only drawn if the FOV is equal
  or smaller than 90. This was the default with Vanilla Quake II. If set
  to `2` the gun is drawn regardless of the FOV. This is the default
  in Yamagi Quake II.

* **fov**: Sets the field of view. If the *horplus* cvar is set to `1`,
  this is forced to 90.

* **horplus**: If set to 1 (the default) the horplus algorithm is used
  to calculate an optimal horizontal and vertical field of view, independent
  of the window or screen aspect ratio or resolution.  
  If enabled *fov* is forced to `90`.

* **vid_gamma**: The value used for gamma correction. Higher value looks
  brighter. The GL1 renderer uses "Hardware Gamma", setting the Gamma of
  your whole screen to this value in realtime (except on OSX where it's
  applied to textures on load and thus needs a `vid_restart` after changing).
  The GL3 renderer applies this to the window in realtime via shaders
  (on all platforms).  
  This is also set by the brightness slider in the video menu.

* **gl_anisotropic**: Anisotropic filtering. Possible values are
  dependent on the GPU driver, most of them support `1`, `2`, `4`, `8`
  and `16`. Anisotropic filtering gives a huge improvement to texture
  quality by a negligible performance impact.

* **r_consolescale** / **r_hudscale** / **r_menuscale**, **crosshair_scale**:
  Scale the console, the HUD, the menu and the crosshair. The value given
  is the scale factor, a factor of `1` means no scaling. Values greater
  `1` make the objects bigger, values lower 1 smaller. The special value
  `-1` sets the optimal scaling factor for the current resolution.

* **r_customheight** / **r_customwidth**: Specifies a custom
  resolution, the windows will be *r_customheight* pixels high and
  *r_customwidth* pixels wide. Set *r_mode* to `-1` to use the custom
  resolution.

* **r_farsee**: Normally Quake II renders only up to 4096 units. If set
  to `1` the limit is increased to 8192 units.

* **vid_maxfps**: The maximum framerate, if `cl_async` is `1`. Otherwise
  `cl_maxfps` is used as maximum framerate. See `cl_async` description
  above for more information.  
  *Note* that vsync (`gl_swapinterval`) also restricts the framerate to
  the monitor refresh rate, so if vsync is enabled, you won't get more than
  60fps on most displays (or 120 on a 120hz display etc).

* **gl_msaa_samples**: Full scene anti aliasing samples. The number of
  samples depends on the GPU driver, most drivers support at least
  `2`, `4` and `8` samples. If an invalid value is set, the value is
  reverted to the highest number of samples supported. Especially on OpenGL
  3.2 anti aliasing is expensive and can lead to a huge performance hit,
  so try setting it to a lower value if your framerate is too low.

* **gl_nolerp_list**: list seperate by spaces of textures ommitted from
  bilinear filtering. Used by default to exclude the console and HUD fonts.
  Make sure to include the default values when extending the list.

* **gl_retexturing**: If set to `1` (the default) and a retexturing pack
  is installed, the high resolution textures are used.

* **gl_shadows**: Enables rendering of shadows. Quake IIs shadows are
  very simple and are prone to render errors.

* **gl_swapinterval**: Enables the vsync: frames are synchronized with
  display refresh rate, should (but doesn't always) prevent tearing.

* **gl_zfix**: Sometimes two or even more surfaces overlap and flicker.
  If this cvar is set to `1` the renderer inserts a small gap between
  the overlapping surfaces to mitigate the flickering. This may lead to
  small render errors.

Graphics (GL1 only):
--------------------

* **intensity**: Sets the color intensity used for 3D rendering.
  Must be a floating point value, at least `1.0` - default is `2.0`.  
  Applied when textures are loaded, so it needs a `vid_restart`!

* **gl1_overbrightbits**: Enables overbright bits, brightness scaling of
  lightmaps and models. Higher values make shadows less dark.  
  Possible values are `0` (no overbright bits), `1` (correct lighting
  for water), `2` (scale by factor 2) and `3` (scale by factor 3).  
  Applied in realtime, does not need `vid_restart`.

* **gl_stencilshadow**: If `gl_shadows` is set to `1`, this makes them
  look a bit better (no flickering) by using the stencil buffer.
  (This is always done in GL3, so not configurable there)

Graphics (GL3 only):
--------------------

* **gl3_debugcontext**: Enables the OpenGL 3.2 renderers debug context,
  e.g. prints warnings and errors emmitted by the GPU driver.  
  Not supported on OSX. This is a pure debug cvar and slows down rendering.

* **gl3_intensity**: Sets the color intensity used for 3D rendering.
  Similar to GL1 `intensity`, but more flexible: can be any value between
  0.0 (completely dark) and 256.0 (very bright).
  Good values are between `1.0` and `2.0`, default is `1.5`.
  Applied in realtime via shader, so it does *not* need a `vid_restart`.

* **gl3_intensity_2D**: The same for 2D rendering (HUD, menu, console, videos)

* **gl3_overbrightbits**: Enables overbright bits, brightness scaling of
  lightmaps and models. Higher values make shadows less dark.  
  Similar to GL1's `gl1_overbrightbits`, but allows any floating point number.  
  Default is `1.3`. In the OpenGL3.2 renderer, no lighting fixes for water
  are needed, so `1.0` has no special meaning.

* **gl3_particle_size**: The size of particles - Default is `40`.

* **gl3_particle_fade_factor**: "softness" of particles: higher values
  look less soft. Defaults to `1.2`.  
  A value of `10` looks similar to the GL1 particles.

* **gl3_particle_square**: If set to `1`, particles are rendered as squares,
  like in the old software renderer or Quake1. Default is `0`.


