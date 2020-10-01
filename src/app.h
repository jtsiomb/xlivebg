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
#ifndef APP_H_
#define APP_H_

#include "xlivebg.h"

extern unsigned int bgtex;
extern unsigned long msec;
extern long upd_interval_usec;

extern int scr_width, scr_height;

#define MAX_SCR	32
extern struct xlivebg_screen screen[MAX_SCR];
extern int num_screens;

int app_init(int argc, char **argv);
void app_cleanup(void);

void app_draw(void);
void app_reshape(int x, int y);

void app_keyboard(int key, int pressed);

void app_quit(void);
unsigned int app_getmouse(int *x, int *y);


int xlivebg_init_gl(void);
void xlivebg_destroy_gl(void);


#endif	/* APP_H_ */
