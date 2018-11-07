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

All cvars may be given at command line trough `+set cvar value` or typed
into the console. The console can be opended with *shift-esc*.


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

* **busywait**: By default this is set to `1`. causing Quake II to spin
  in a very tight loop until it's time to process the next frame. This
  is a very accurate way to determine the internal timing but comes with
  a relatively high CPU usage. If set to `0` Quake II lays itself to
  sleep and tells the operating system to send a wakeup signal when it's
  time for the next frame. The later is more CPU friendly but rather
  inaccurate, especially on Windows. Use with care.

* **cl_async**: If set to `1` (the default) the client is asynchronous.
  The client framerate is fixed, the renderer framerate is variable.
  This makes it possible to renderer as many frames as desired without
  any physics and movement problems. The client framerate is controlled
  by *cl_maxfps*, set to `60` by default. The renderer framerate is
  controlled by *vid_maxfps*. There are two constraints:

  * *vid_maxfps* must be the same or greater than *cl_maxfps*.
  * In case that the vsync is active, *vid_maxfps* must not be lower
	than the display refresh rate.

	If *cl_async* is set to `0` *vid_maxfps* is the same as *cl_maxfps*,
	use *cl_maxfps* to set the framerate.

* **cl_showfps**: Shows the framecounter.

* **in_grab**: Defines how the mouse is grabbed by Yamagi Quake IIs
  window. If set to `0` the mouse is never grabbed and if set to `1`
  it's always grabbed. If set to `2` (the default) the mouse is grabbed
  during gameplay and released otherwise (in menu, videos, console or if
  game is paused).


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

* **s_doppler**: If set to `1` (the default) doppler effects are
  enabled. This is only supported by the OpenAL sound backend.

* **s_openal**: Use OpenAL for sound playback. This is enabled by
  default. OpenAL gives a huge quality boost over the classic sound
  system and supports surround speakers and HRTF.

* **s_underwater**: Dampen sounds if submerged. Enabled by default.


## Graphics (all renderers)

* Most old `r_*` cvars, but renamed to `gl_*`

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
  90`, it defaults to `80` because that looks a bit better.

* **horplus**: If set to 1 (the default) the horplus algorithm is used
  to calculate an optimal horizontal and vertical field of view,
  independent of the window or screen aspect ratio or resolution. If
  enabled *fov* is forced to `90`.

* **vid_gamma**: The value used for gamma correction. Higher value looks
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
  factor for the current resolution.

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
  enabled, the game won't render more than 60fps on most displays (or
  120 on a 120hz display etc).

* **r_vsync**: Enables the vsync: frames are synchronized with
  display refresh rate, should (but doesn't always) prevent tearing.


## Graphics (GL renderers only)

* **gl_anisotropic**: Anisotropic filtering. Possible values are
  dependent on the GPU driver, most of them support `1`, `2`, `4`, `8`
  and `16`. Anisotropic filtering gives a huge improvement to texture
  quality by a negligible performance impact.

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
  the overlapping surfaces to mitigate the flickering. This may lead to
  small render errors.


## Graphics (OpenGL 1.4 only)

* **gl1_intensity**: Sets the color intensity used for 3D rendering.
  Must be a floating point value, at least `1.0` - default is `2.0`.
  Applied when textures are loaded, so it needs a `vid_restart`!

* **gl1_overbrightbits**: Enables overbright bits, brightness scaling of
  lightmaps and models. Higher values make shadows less dark. Possible
  values are `0` (no overbright bits), `1` (correct lighting for water),
  `2` (scale by factor 2) and `3` (scale by factor 3).  Applied in
  realtime, does not need `vid_restart`.

* **gl1_stencilshadow**: If `gl_shadows` is set to `1`, this makes them
  look a bit better (no flickering) by using the stencil buffer. (This
  is always done in OpenGL 3.2, so not configurable there)


## Graphics (OpenGL 3.2 only)

* **gl3_debugcontext**: Enables the OpenGL 3.2 renderers debug context,
  e.g. prints warnings and errors emitted by the GPU driver.  Not
  supported on macOS. This is a pure debug cvar and slows down
  rendering.

* **gl3_intensity**: Sets the color intensity used for 3D rendering.
  Similar to OpenGL 1.4 `gl1_intensity`, but more flexible: can be any
  value between 0.0 (completely dark) and 256.0 (very bright).  Good
  values are between `1.0` and `2.0`, default is `1.5`.  Applied in
  realtime via shader, so it does *not* need a `vid_restart`.

* **gl3_intensity_2D**: The same for 2D rendering (HUD, menu, console,
  videos)

* **gl3_overbrightbits**: Enables overbright bits, brightness scaling of
  lightmaps and models. Higher values make shadows less dark.  Similar
  to OpenGL 1.4 `gl1_overbrightbits`, but allows any floating point
  number.  Default is `1.3`. In the OpenGL 3.2 renderer, no lighting
  fixes for water are needed, so `1.0` has no special meaning.

* **gl3_particle_size**: The size of particles - Default is `40`.

* **gl3_particle_fade_factor**: "softness" of particles: higher values
  look less soft. Defaults to `1.2`. A value of `10` looks similar to
  the OpenGL 1.4 particles.

* **gl3_particle_square**: If set to `1`, particles are rendered as
  squares, like in the old software renderer or Quake 1. Default is `0`.
