xlivebg - live wallpapers for the X window system
=================================================

![logo](http://nuclear.mutantstargoat.com/sw/xlivebg/xlivebg_logo2_sm.png)

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
function, which is called by xlivebg at (approximately) fixed intervals.

For further information, build instructions, configuration and setup, and a
guide for writing live wallpapers, refer to the manual:
http://nuclear.mutantstargoat.com/sw/xlivebg/manual
Also available under the `doc` directory in the source tree.

License
-------
Copyright (C) 2019-2020 John Tsiombikas <nuclear@member.fsf.org>

This program is free software, feel free to use, modify and/or redistribute it
under the terms of the GNU General Public License version 3, or at your option
any later version published by the Free Software Foundation.

FAQ
---
1. Q: I'm running a standalone compositor (like `xcompmgr`), and xlivebg does
      nothing, or produces a corrupted picture.

   A: Try passing the `-n` option to xlivebg, to instruct it to create its own
      "desktop" window, instead of trying to draw on the root or virtual root.
