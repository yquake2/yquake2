# Configuration Guide

Yamagi Quake II provides a lot of configuration options. This guides
shows how to configure Yamagi Quake II to match you needs. This guide
is for advanced users, if you just want to play you're likely happy
with the defaults and the options that can be set through the menu.


## Choosing a Renderer

Yamagi Quake II ships with 4 renderers:

* The **OpenGL 3.2** renderer: This renderer was developed for the needs
  of modern graphics hardware and is usually the best choice for OpenGL
  3.2 capable graphics cards. It provides a very detailed look and feel,
  matching the dark and dirty atmosphere on Stroggos. The texture
  rendering looks mostly the same on all GPU drivers. Depending on the
  display, the default lighting may be too bright or too dark, it can be
  adjusted through the menu or through the *vid_gamma* cvar.
* The **OpenGL ES3** renderer: This is pretty much the same as the
  OpenGL 3.2 renderer (and uses the same cvars for configuration), but
  uses OpenGL ES 3.0 instead of "desktop" OpenGL, so it also works on
  the Raspberry Pi 4, for example. Reportedly it also has slightly
  better performance on Wayland, at least with the open source AMD drivers.
* The **OpenGL 1.4** renderer: This is a slightly enhanced version of
  the original OpenGL renderer shipped in 1997 with the retail release.
  It's provided for older graphics cards, not able to run the OpenGL 3.2
  renderer. The OpenGL 1.4 renderer has some limitations. The look and
  feel is highly dependent on the GPU driver and the platforms OpenGL
  implementation, especially the texture rendering may vary to a wide
  margin. The global lighting may not be rendered correctly, especially
  liquids may be too dark or too bright.
* The **Software** renderer: Shipped for historical reasons only.
  Setting the OpenGL 3.2 renderer to match the software renderers look
  and feel is often the better choice, since it's faster and provides
  colored lighting. The software renderer may show some rendering errors
  on widescreen resolutions.


## Choosing a Sound System

Yamagi Quake II ships with 2 sound system:

* The **OpenAL** sound system: This is the default and highly
  recommended. It provides full surround sound support and even HRTF for
  headphones. But also the plain stereo playback is much better than in
  the original sound system. The setup is done mostly through OpenAL,
  have a look at the documentation of your OpenAL library.
* The **SDL** sound system: This is the classic sound system, providing
  an experience like the original client. Set `s_openal` to `0` and
  execute an `snd_restart` to activate it. The classic sound system may
  be somewhat problematic on modern systems like Windows 10 or Linux
  with Pulseaudio.


## Tuning for Precise Timings

Yamagi Quake II comes with a highly evolved asynchronous client. While
the default settings are usually good, some players may want to tune for
more precise timing or better vertical synchronization accuracy.

Quake II was never meant to run on todays hardware. Modern hardware is
hundred times faster than the hardware of 1997. Faster hardware brings
higher framerates, which make inaccuracies scattered all over the code
visible and a problem.
We're unable to fix all those inaccuracies, because the game data, the
network protocol and the whole look and feel depends on them. We can
just try work around them.

If your computer is **fast enough** to reach at least 60fps in Yamagi Quake II
(this should be the case for most machines these days, including Laptops
 with integrated GPUs; a notable exception are Raspberry PIs),
the following settings should give you the best results:

* Make sure that `busywait` is set to `1`. That's the default. Setting
  it to `0` saves some CPU time (and thus increase battery life and
  reduce heat), but can mess up the timings, especially on Windows,
  which can lead to tearing- and / or micro stuttering.
* Set `cl_async` to `1` (the default) to avoid glitches especially in
  physics/movement when rendering at high framerates (>90fps).
* `cl_maxfps` should usually be set to `-1` (the **new** default) so
  the engine can choose a packet framerate that should be ideal.
* If your display's refreshrate isn't detected correctly, set
  `vid_displayrefreshrate`.
* If your display has a vertical refreshrate of 60Hz, just enable vsync
  (`r_vsync 1`) and make sure `vid_maxfps` is at least `60`, which
  by default is the case.
* If you have a display that runs at more than 60Hz, enabling vsync should
  still make the game run well, but you get best results by running it at
  a render framerate that's a multiple of 60fps, for example by setting
  `vid_maxfps` to `120` (for 144Hz displays).
* If you have vsync disabled, you should set `vid_maxfps` to a framerate
  that your computer can reach.

For **slower machines** (that can't consistently reach 60fps) it makes
sense to disable the asynchronous client all together by setting
`cl_async` to `0` and limiting the framerate to a value <= 90, to avoid
the aforementioned glitches in parts of the game where your machine
reaches more than 90fps after all.  
This is done by setting `vid_maxfps` (to `80`, for example). 
`cl_maxfps` is ignored with `cl_async 0`, and every render frame is
also a client/server frame.

The [cl_async entry in the cvar documentation](040_cvarlist.md#general)
has some more information on the asynchronous client.

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
Linux with PulseAudio / Pipewire.

The new sound effects can be disabled with:

* `s_doppler` set to `0` disables the doppler effect.
* `s_underwater` set to `0` disables the underwater effect.


### Renderer

While Yamagi Quake II still supports the Software renderer, configuring
one of the OpenGL renderers to give a classic look and feel is often the
better choice. The OpenGL renderers are much faster, more reliable and
support colored lighting.

General cvars:

* `cl_lights`: Set to `0` to disable the dynamic lighting.

Both OpenGL renderers:
* `gl_texturemode`: Set to `GL_NEAREST` to disable the texture
  filtering, giving a classic pixel look. Additionally disabling
  anisostropic filtering makes it look even more authentic.

The OpenGL 1.4 renderer:

* `gl1_pointparameters`: When set to `0` the particles are rendered as
  blurry octagon. May be already the case if the GPU driver doesn't
  support point parameters.

The OpenGL 3.2 renderer:

* `gl3_particle_square`: When set to `1` the particles are rendered as
  squares.

* `gl3_colorlight`: When set to `0`, the lights and lightmaps are
  colorless (greyscale-only), like in the original soft renderer.

## Retexturing Packs

Yamagi Quake II has full support for retexturing packs. They just need
to be installed and should be picked up automatically. To disable the
retexturing pack at a later time set `r_retexturing` to `0`.

* The most comprehensive build retexturing pack can be found here:
  https://deponie.yamagi.org/quake2/assets/texturepack/
* And there's an AI upscale of the original textures:
  https://github.com/Calinou/quake2-neural-upscale/releases

Retexturing packs can be installed by placing the pak or zip files in
the *baseq2* directory.
