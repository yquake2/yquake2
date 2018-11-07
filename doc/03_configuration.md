# Configuration Guide

Yamagi Quake II provides a lot of configuration options. This guides
shows how to configure Yamagi Quake II to match you needs. This guide
is for advanced users, if you just want to play you're likely happy
with the defaults and the options that can be set through the menu.


## Choosing a Renderer

Yamagi Quake II ships with 3 renderers:

* The **OpenGL 3.2** renderer: This renderer was developed for the needs
  of modern hardware and is usually the best choice for OpenGL 3.2
  capable graphics cards. It provides a very detailed look and feel,
  matching the dark and dirty atmosphere on Stroggos. The texturing
  renderer of the OpenGL 3.2 renderer looks mostly the same on all GPU
  drivers. The default lighting may be too bright or too dark, it can be
  adjusted through the menu or through the *vid_gamma* cvar.
* The **OpenGL 1.4** renderer: This is a slightly enhanced version of
  the original OpenGL renderer shipped in 1997 with Quake II. It's
  provided for older graphics cards, not able to run the OpenGL 3.2
  renderer. The OpenGL 1.4 renderer has some limitations. The look and
  feel is highly dependent on the GPU driver and the platforms OpenGL
  implementation, especially the texture rendering may vary to a wide
  margin. The global lighting may not be rendered correctly, especially
  liquids may be too dark or too bright.
* The **Software** renderer: Shipped for historical reasons only.
  Setting the OpenGL 3.2 renderer to match the software renderers looks
  and feel is often the better choice since it's faster and provides
  colored lighting. The software renderer may show some rendering errors
  on widescreen resolutions.


## Choosing a Sound System

Yamagi Quake II ships with 2 sound system:

* The **OpenAL** sound system: This is the default and highly
  recommended. It provides full surround sound support and even HRTF.
  But also the plain stereo playback is much better then in the original
  sound system. The setup is done mostly through OpenAL, have a look at
  the documentation of your OpenAL library.
* The **SDL** sound system: This is the classic sound system, providing
  an experience like the original client. Set `s_openal` to `0` and
  execute an `snd_restart` to activate it. The classic sound system
  may be problematic on modern systems like Windows 10 or Linux with
  Pulseaudio.


## Tuning for Precise Timings

Yamagi Quake II comes with a highly evolved asynchronous client. While
the default settings are usually good, some players may want to tune for
more precise timing or better vertical synchronization accuracy.

Quake II was never mend to run on todays hardware. Modern hardware is
hundred times faster than the hardware of 1997. With faster hardware
inaccuracies scattered all over the code become visible and a problem.
We're unable to fix those inaccuracies, because the game data, the
network protocol and the whole look and feel depends on them. We can
just try work around them.

Additionally modern high resolution LCD displays are much more prone to
tearing and missed frames then the low resolution CRT displays of the
1990th. This is a big problem because precise timings and keeping the
frame rate constant are at least partly mutual exclusive. So players
have the choice:

* Keep an accurate frame rate, rendering exactly as many frames as the
  display can show. For example on a common 59.95hz display the game
  should try to render about 60 frames per second and rely on vertical
  synchronization (vsync) to slow itself down to 59.95 frames per
  second. With this approach tearing and missed frames (perceivable as
  micro stuttering) are minimized, but on the other hand the timings may
  be a little bit off.
* Keep precise timings and the cost of a less accurate frame rate. With
  this approach the timings are optimal, e.g. movement like strafejumps
  and the clipping against walls are a precise as possible. But the
  frame rate may be a little off, leading to tearing and missed frames.

The first approach is the default. To switch over to precise timing the
following console variables must be altered. There's no need to alter
all of them for good results, it depends on the display, the hardware,
the GPU driver and the preferences of the player.

* Make sure that `busywait` is set to `1`. That's the default. Setting
  it to `0` saves some CPU time but is plain deadly for the timings.
  You'll never get precise timing and tearing- and / or micro stuttering
  free gameplay with busy waits switched off!
* `r_vsync` can be set to `0`. Enabling the  vertical synchronization
  allows the GPU driver to wait for the display, thus stealing time from
  Yamagi Quake II. This stolen time is missing to the internal time
  accounting, while Yamagi Quake II tries to determine how much time was
  lost that's always an imprecise guess. Disabling vertical
  synchronization will always cause tearing!
* `cl_maxfps` must be set to a value lower than `vid_maxfps`. The margin
  between the two values should be at least 3 frames per second, better
  5 frames per second. Yamagi Quake II internally enforces a margin of
  5%. If `cl_maxfps` is set too high (above 90) the good old 125hz bug
  will trigger and the physics will be off. That may be desired.
* `vid_displayrefreshrate` must be set to the framerate of the display.
  The default is `-1` which means autodetect. In most cases that's okay,
  but for precise timings it's a good idea to override the autodetected
  value and set the display refresh rate by hand. The displays EDID info
  or the GPU driver may be lying. Only full numbers can be given. If
  round up there's a risk for imprecise timing. When round down the risk
  if micro stuttering increases.
* When running with vertical synchronisation enabled `vid_maxfps` can be
  set to any value higher then the display refresh rate. If the vertical
  synchronisation is disabled `vid_maxfps` should be set to the desired
  target frame rate.

Putting it all together we come up with three so to say standard
configurations that can be used as a baseline for experiments:

* Precise frame rate and slightly imprecise timings:
  * `busywait` set to `1`.
  * `r_vsync` set to `1`.
  * `cl_maxfps` set to `60`.
  * `vid_displayrefreshrate` set to `-1`.
  * `vid_maxfps` set to `300`.
* Somewhat precise timing and some micro stuttering:
  * `busywait` set to `1`.
  * `r_vsync` set to `1`.
  * `cl_maxfps` set to `60`.
  * `vid_displayrefreshrate` set to the displays refresh rate minus 1.
  * `vid_maxfps` set to `300`.
* Precise timing at the cost of tearing:
  * `busywait` set to `1`.
  * `r_vsync` set to `0`.
  * `cl_maxfps` set to `60`.
  * `vid_displayrefreshrate` set to the displays refresh rate plus 1.
  * `vid_maxfps` set to the desired frame rate.

And there's always the option to disable the asynchronous client all
together by setting `cl_async` to `1`. In that case `cl_maxfps` and
`vid_maxfps` are tight together, like in the original Quake II release
each renderframe is a clientframe. With that both precise timings and
tearing / micro stuttering free rendering can be archieved by setting
`cl_maxfps` to a value higher then the displays refresh rate and
activating the vertical syncrhonization by setting `r_vsync` to `1`.
But if `cl_maxfps` is set above about 90 frames per second the 125hz
bug will trigger and the physics will be off. Additionally it will
flood servers with packages, at least one package per frame. That may
be considered abuse.


## Getting a classic look and feel

Yamagi Quake II has some features to provide a better experience on
modern hardware. For example widescreen support, HUD scaling or FOV
alterations. Not all users may like these changes.


### HUD scaling

All levels of scaling can be switched off in the *Video* menu. It's also
possible to switch only parts of the scaling off, for example the menu
and the console can be scaled, but the HUD not. The cvars are:

* `r_consolescale` for the console.
* `r_hudscale` for the HUD.
* `r_menuscale` for the menu.
* `crosshair_scale` for the crosshair.

Please note that's not always clear which GUI elements are part of what
subsystem. The loading plaque is part of the menu and not of the HUD,
for example.


### Field of View

Yamagi Quake II has a different FOV calculation then the original
client. Yamagi Quake II determines the optimal FOV (horizontal and
vertical) with the *Horplus* algorithm and the gun is rendered always
with a static FOV of 80, the original client only had a vertical FOV
applied to everything.

* The FOV itself can be altered through the *Video* menu or the `fov`
  cvar. That gives a smaller or wider FOV, but not the classic Quake II
  look because the Horplus algorithm is still active.
* The Horplus algorithm can be disabled by setting `horplus` to `0`.
* The gun can be rendered with the global FOV by setting `r_gunfov` to
  the same value as `fov`.


### 4/3 Cinematics

While the original Quake II client stretched cinematics over the whole
window, Yamagi Quake II always renders them with 4/3 aspect. The old
behavior can be enforced by setting `cin_force43` to `0`.


### Sound system

As already said above, Yamagi Quake II has two sound systems. The old
one and OpenAL. Additionally some new sound effects were added. We
recommend to stay with OpenAL, even if the stereo rendering is somewhat
different to the old sound system. OpenAL is much more distortion free
and more reliable, especially on modern platforms like Windows 10 or
Linux with PulseAudio.

The new sound effects can be disabled with:

* `s_doppler` set to `0` disables the doppler effect.
* `s_underwater` set to `0` disables the underwater effect.


### Renderer

While Yamagi Quake II still supports the Software renderer, configuring
one of the OpenGL renderers to give a classic look and feel is often the
better choice. The OpenGL renderers are much faster, more reliable and
support colored lighting.

General cvars:

* `cl_lights` set to `0` disables the dynamic lights.
* `gl_texturemode` set to `GL_NEAREST_MIPMAP_LINEAR` disabled the
  texture filtering, giving a nice pixel look.

The OpenGL 1.4 renderer:

* `gl1_pointparameters`: When set to `0` the particles are rendered as
  squares. May be already the case if the GPU driver doesn't support
  point parameters.

The OpenGL 3.2 renderer:

* `gl3_particle_square`: When set to `1` the particles are rendered as
  squares.


## Retexturing Packs

Yamagi Quake II has full support for retexturing packs. They just need
to be installed and should be picked up automatically. To disable the
retexturing pack at a later time set `gl_retexturing` to `0`.

The most comprehensive retexturing pack can be found here:
https://deponie.yamagi.org/quake2/texturepack/ It can be installed by
placing the zip-Files in the *baseq2* directory. Please note that some
textures are broken, especially the model textures.
