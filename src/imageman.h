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
#ifndef IMAGEMAN_H_
#define IMAGEMAN_H_

#include "xlivebg.h"

void init_imgman(void);

int create_image(struct xlivebg_image *img, int width, int height, uint32_t *pix);
void destroy_image(struct xlivebg_image *img);

int load_image(struct xlivebg_image *img, const char *fname);
int add_image(struct xlivebg_image *img);

struct xlivebg_image *get_image(int idx);
int get_image_count(void);

#endif	/* IMAGEMAN_H_ */
