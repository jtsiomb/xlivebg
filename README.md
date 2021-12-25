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

  - Project website: http://nuclear.mutantstargoat.com/sw/xlivebg
  - Source code repository: https://github.com/jtsiomb/xlivebg
  - Latest release (1.0): http://nuclear.mutantstargoat.com/sw/xlivebg/releases/xlivebg-1.0.tar.gz

Building and Installing
-------

After running:
```
git clone https://github.com/jtsiomb/xlivebg/
cd xlivebg
```

Debian will require at least the following libraries to be installed via:
```
sudo apt install libx11-dev libxext-dev libxrandr-dev libglx-dev libpng-dev libjpeg-dev libmotif-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libglu1-mesa-dev 
```

Then you can build normally with:
```
./configure
make
sudo make install
```


License
-------
Copyright (C) 2019-2021 John Tsiombikas <nuclear@member.fsf.org>

This program is free software, feel free to use, modify and/or redistribute it
under the terms of the GNU General Public License version 3, or at your option
any later version published by the Free Software Foundation. See COPYING for
details.

Contact
-------
For bug reports, head over to the github issues page:
https://github.com/jtsiomb/xlivebg/issues

For xlivebg-related discussion, support questions, development, bug reports,
feature requests, and patches, feel free to join the xlivebg mailing list:
  - Subscribe: xlivebg-subscribe@mutantstargoat.com
  - Post: xlivebg@mutantstargoat.com

Mailing list rules: no large attachments, no html emails, no top-posting. Send
only plain text emails,	hard-wrapped at 72 columns. Replies should quote only
the relevant part of the previous message, and respond immediately under it

For anything else, feel free to contact me at: nuclear@member.fsf.org. Only
plain-text emails, hard-wrapped at 72-columns will be accepted.

FAQ
---
1. Q: I'm running xlivebg and nothing happens. I'm still seeing the previous
      desktop wallpaper, a black screen, or a corrupted/broken screen.

   A: Try passing the `-n` option to xlivebg. This forces xlivebg to create its
      own "desktop" window, instead of trying to draw on the root or virtual
      root window.
