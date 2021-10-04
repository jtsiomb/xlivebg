/*
xlivebg - live wallpapers for the x window system
copyright (c) 2019-2021  john tsiombikas <nuclear@member.fsf.org>

this program is free software: you can redistribute it and/or modify
it under the terms of the gnu general public license as published by
the free software foundation, either version 3 of the license, or
(at your option) any later version.

this program is distributed in the hope that it will be useful,
but without any warranty; without even the implied warranty of
merchantability or fitness for a particular purpose.  see the
gnu general public license for more details.

you should have received a copy of the gnu general public license
along with this program.  if not, see <https://www.gnu.org/licenses/>.
*/
/* plugin author: mdecicco */
#include "xlivebg.h"
#include "props.h"
#include "ps3.h"

static struct xlivebg_plugin plugin = {
	"ps3",
	"PS3 xmb wave",
	PROPLIST,
	XLIVEBG_30FPS,
	init, deinit,
	start, stop,
	draw,
	prop,
	0, 0
};


int register_plugin(void) {
	return xlivebg_register_plugin(&plugin);
}
