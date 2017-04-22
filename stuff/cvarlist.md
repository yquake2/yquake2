Yamagi Quake II Console Variables
---------------------------------

This lists explains most console variables (cvars) added by Yamagi
Quake II. Most of the original clients (Vanilla Quake II) cvars are
still in place. Please note: There's normally no need to change any
cvar! Use the menu instead.


* **al_device**: OpenAL device to use. In most cases there's no need to
  change this, since the default device is normally the right choice.

* **basedir**: Directory from which the game data is loaded. Can be used
  in startup scripts, to test binaries, etc. If not set the directory
  containing the binaries is used.

* **cin_force43**: If set to `1` (the default) cinematics are displayed
  with an aspect ratio of 4:3, regardless what the actual windows size
  or resolution is.

* **cl_async**: If set to `1` (the default) the client is asynchronous.
  The client framerate is fixed, the renderer framerate is variable.
  This makes it possible to renderer as much frames as desired without
  any physics and movement problems. The client framerate is controlled
  by *cl_maxfps*, set to `60` by defaut. The renderer framerate is
  controlled by *gl_maxfps*. There're two constraints: *gl_maxfps* must
  be the same or greater than *cl_maxfps*. In case that the vsync is
  active *gl_maxfps* must not be lower than the display refresh rate. If
  *cl_async* is set to `0` *gl_maxfps* is the same as *cl_maxfps*, use 
  *cl_maxfps* to set the framerate.

* **cl_drawfps**: Shows the framecounter. The shown value is rather
  inaccurate, the imprecision is about 5%.

* **cl_gun**: Decides weather the gun is drawn. If set to `0` the gun
  is omitted. If set to `1` the gun is only drawn if the FOV is equal
  or smaller than 90. This was the default with Vanilla Quake II. If set
  to `2` the gun is drawn regardless of the FOX. This is the default
  in Yamagi Quake II.

* **fov**: Sets the field of view. If the *horplus* cvar is set to `1`,
  this is forced to 90.

* **gl3_debugcontext**: Enables the OpenGL 3.2 renderers debug context,
  e.g. prints warnings and errors thrown by the GPU driver. This is a
  pure debug cvar and has high overhead.

* **gl3_intensity** / **intensity**: Sets the color intensity. Since
  OpenGL 1.4 and 3.2 have different scales there're 2 cvar.
  **intensity** for OpenGL 1.4 and **gl3_intensity** for OpenGL 3.2.

* **gl3_overbrightbits** / **gl_overbrightbits**: Enables overbright
  bits, brightness scaling of all textures. OpenGL 1.4 is controlled
  through **gl_overbrightbits** and employs a rather simple algorithm.
  Possible values are `0` (no overbright bits), `1` (correct lighting
  for water), `2` (scale by factor 2) and `3` (scale by factor 3).
  OpenGL 3.2 has a much better algorithm that is controlled by
  *gl3_overbrightbits*. The cvar gives the scaling factor as a floating
  point number.

* **gl3_particle_size** / **gl3_particle_fade_factor**: OpenGL 3.2
  particle attributes. The look and feel of the  OpenGL 1.4 particles
  can be archived by setting *gl3_particle_size* to `20` and
  *gl3_particle_fade_factor* to `10`.

* **gl_anisotropic**: Anisotropic filtering. Possible values are
  dependent on the GPU driver, most of them support `1`, `2`, `4`, `8`
  and `16`. Anisotropic filtering gives a huge improvement to texture
  quality by a negligible performance impact.

* **gl_consolescale** / **gl_hudscale** / **gl_menuscale**: Scale the
  console, the HUD and the menu. The value given is the scale factor, a
  factor of `1` means no scaling. Values greater `1` make the objects
  bigger, values lower 1 smaller. The special value `-1` set the optimal
  scaling factor for the current resolution.

* **gl_customheight** / **gl_customwidth**: Specifies a custom
  resolution, the windows will be *gl_customheight* pixels high and
  *gl_customwidth* pixels wide. Set *gl_mode* to `-1` to use the custom
  resolution.

* **gl_farsee**: Normally Quake II renders only up to 4096 units. If set
  to `1` the limit is increased to 8192 units.

* **gl_msaa_samples**: Full scene anti aliasing samples. The number of
  samples is dependent on the GPU driver, most drivers support at least
  `2`, `4` and `8` samples. If an invalid value is set the value is
  reverted the highest number of samples supported. Especially on OpenGL
  3.2 anti aliasing is expensive and can lead to a huge performance hit.

* **gl_retexturing**: If set to `1` (the default) and a retexturing pack
  is installed, the high resolution textures are used.

* **gl_shadows**: Enables rendering of shadows. Quake IIs shadows are
  very simple and are prone to render errors. 

* **gl_swapinterval**: Enables the vsync.

* **gl_zfix**: Sometimes 2 or even more surfaces overlap and flicker. If
  this cvar is set to `1` the renderer inserts a small gap between the
  overlapping surfaces to mitigate the flickering. This may lead to
  small render errors.

* **horplus**: If set to 1 (the default) the horplus algorithm is used
  to calculate an optimal FOX. If enabled *fov* is forced to `90`.

* **in_grab**: Defines how the mouse is grabbed by Quake IIs window. If
  set to `0` the mouse is never grabbed and if set to `1` it's always
  grabbed. If set to `2` (the default) the mouse is grabbed during
  gameplay and released otherwise.

* **ogg_enable**: Enable Ogg/Vorbis playback.

* **ogg_ignoretrack0**: Normally Quake II disabled the background music
  if a major objective has been archived. Setting this cvar to `1`
  disables this, the music keeps playing.

* **s_doppler**: If set to `1` (the default) doppler effects are enabled.
  This is only supported by the OpenAL sound backend.

* **s_openal**: Use OpenAL for sound playback. This is enabled by
  default.  OpenAL gives a huge quality boost over the classic sound
  system.

* **s_underwater**: Dampen sounds if submerged. Enabled by default.

* **vid_renderer**: Selects the renderer library. Possible options are
  `gl1` (the default) for OpenGL 1.4 and `gl3` for OpenGL 3.2.

