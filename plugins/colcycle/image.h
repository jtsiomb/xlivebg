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
#ifndef IMAGE_H_
#define IMAGE_H_

enum cycle_mode {
	CYCLE_NORMAL = 0,
	CYCLE_UNUSED,	/* ? */
	CYCLE_REVERSE = 2,
	CYCLE_PINGPONG = 3,
	CYCLE_SINE_HALF = 4,	/* sine -> [0, range/2] */
	CYCLE_SINE = 5			/* sine -> [0, range] */
};

struct color {
	unsigned char r, g, b;
};

struct colrange {
	int low, high;
	int cmode;
	unsigned int rate;
	struct colrange *next;
};

struct image {
	int width, height;
	int bpp;
	struct color palette[256];
	struct colrange *range;
	int num_ranges;
	unsigned char *pixels;
};

int gen_test_image(struct image *img);
int load_image(struct image *img, const char *fname);
void destroy_image(struct image *img);

#endif	/* IMAGE_H_ */
