/*
colcycle - color cycling image viewer
Copyright (C) 2016  John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef APP_H_
#define APP_H_

extern int fbwidth, fbheight;
extern unsigned char *fbpixels;
extern unsigned long time_msec;

int colc_init(int argc, char **argv);
void colc_cleanup(void);

void colc_draw(void);

/* defined in main_*.c */
void set_palette(int idx, int r, int g, int b);
void resize(int x, int y);

#endif	/* APP_H_ */
