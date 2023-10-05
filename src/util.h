/*
xlivebg - live wallpapers for the X window system
Copyright (C) 2019  John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef UTIL_H_
#define UTIL_H_

#include <stddef.h>
#include <sys/un.h>
#include <X11/Xlib.h>
#include "xlivebg.h"

#ifndef SUN_LEN
#define SUN_LEN(p)	(offsetof(struct sockaddr_un, sun_path) + strlen((p)->sun_path))
#endif

struct color {
	float r, g, b, a;
};

char *get_home_dir(void);
char *get_config_path(void);
char *get_save_config_path(void);

int get_num_outputs(Display *dpy);
void get_output(Display *dpy, int idx, struct xlivebg_screen *scr);

#endif	/* UTIL_H_ */
