xlivebg - live wallpapers for the X window system
=================================================

About
-----

xlivebg is a live wallpaper framework, and collection of live wallpapers, for
the X window system. xlivebg is independent of window managers and desktop
environments, and should work with any of them, or even with no window manager
at all.

Live wallpapers are a way to have an animated desktop background, instead of a
simple static image. They are essentially programs which use OpenGL to display
an animated moving image in place of the traditional wallpaper.

Live wallpapers are written as plugins for xlivebg. All X11-specific
initialization, OpenGL context creation, and root window management are
performend by xlivebg; the live wallpaper plugins need only provide a `draw`
function, which is called by xlivebg at (approximately) fixed intervals. For
more details on how to write a live wallpaper plugin, see the bundled live
wallpapers under the `plugins` directory.

xlivebg searches for installed live wallpapers in the following directories:
 - `PREFIX/lib/xlivebg` (default PREFIX is `/usr/local`).
 - `~/.local/lib/xlivebg`
 - `~/.xlivebg/plugins`

License
-------
Copyright (C) 2019 John Tsiombikas <nuclear@member.fsf.org>

This program is free software, feel free to use, modify and/or redistribute it
under the terms of the GNU General Public License version 3, or at your option
any later version published by the Free Software Foundation.

Installation
------------
xlivebg has no dependencies other than Xlib and OpenGL, and is written in
ANSI/ISO C89, making it extremely easy to build. Just type `make`, and then
`make install` as root, to install system-wide. This will only install the main
executable and header file for building live wallpapers.

To also install the bundled live wallpapers, run `make install-all` as root.

The default installation prefix is `/usr/local`. If you wish to change that,
simply modify the `PREFIX = /usr/local` line at the top of the main makefile, or
override it by running `make PREFIX=some/path install` explicitly.
