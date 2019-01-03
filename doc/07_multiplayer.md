# Multiplayer Servers

Generally running a Yamagi Quake II server is the same as running a Vanilla Quake2 server, so
the old guides (e.g. http://it.rcmd.org/networks/quake2/q2_linux_server_howto.php) should still apply.

One thing to keep in mind is that the server must be restarted at least every 49 days, because
the Quake2 network protocol represents time in milliseconds as a 32bit int and after 49 days that
int overflows back to zero leading to all kinds of trouble.  
This problem has always existed in Quake2 and is not fixable (at least not without breaking
compatibility with the existing network protocol), but back in Win9x days this was less of a problem
because 1. Windows crashed frequently anyway and 2. Win9x had the same bug and crashed after 49 days
or so...

Apart from this, we'll only document changes/additions here.

## HTTP Downloads

Like r1q2 and some other Quake2 source ports, we allow downloading game data for multiplayer via http.  
This is a lot faster than Quake2's internal protocol that was used in Vanilla Quake2 and Yamagi Quake II
up to version 7.30.

As a **client** you don't have to do anything, just use a Yamagi Quake II version newer than 7.30
and if you build it yourself don't disable cURL support.

For **servers** the following must be done:

### Put the game data on a http server

The directory structure on the server must be the same as in the game, so if you want to provide
`maps/blub.bsp` and your server base path is `http://example.com/q2data/`, then you must put that
map into `http://example.com/q2data/maps/blub.bsp`.

You can either just put the raw .bsp (and files the bsp needs like textures) on your http server, or
you can upload a whole `.pak` or `.pk3` that contains the needed data. If you're using a `.pak`
or `.pk3` you need a **file list** that's hosted on your http server.

#### Map specific file lists

One way is to have one file list per map that's running on your server.  
If your server is rotating between `maps/blub.bsp` and `maps/bla.bsp`, you'd have
`http://example.com/q2data/maps/blub.filelist` and `http://example.com/q2data/maps/bla.filelist`.

A file list is a plain text file that lists one file path per line that's to be downloaded.
Those paths are relative to the server base path and **must not** begin with a slash!

So if `maps/bla.bsp` needs `bla.pak` and `textures.pak`, your `http://example.com/q2data/maps/bla.filelist`
would look like:
```
bla.pak
textures.pak
```

#### Global File List

Instead of map-specific file lists, you could have one global file list. All those files are downloaded
when someone connects to your server, regardless of the currently running map.

This global file list must be at your server base path and must be called `.filelist`.

So in our example `http://example.com/q2data/.filelist` could look like:
```
bla.pak
blub.pak
textures.pak
```
or

```
maps/bla.bsp
maps/blub.bsp
textures/my_tex.wal
```

### Configure the quake2 server to tell clients about the http server

All you have to do is to set the `sv_downloadserver` CVar to your server base path, so in our example
you could start your dedicated server with 	`q2ded +set sv_downloadserver http://example.com/q2data/`
(+ your other options).

This CVar will be set to connecting Multiplayer Clients and if they support http downloading they
will try to load missing game data from that server.
