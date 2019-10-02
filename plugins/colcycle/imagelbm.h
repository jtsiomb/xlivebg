/*
colcycle - color cycling image viewer
Copyright (C) 2016-2017  John Tsiombikas <nuclear@member.fsf.org>

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
#ifndef IMAGE_LBM_H_
#define IMAGE_LBM_H_

#include <stdio.h>
#include "image.h"

int file_is_lbm(FILE *fp);
int load_image_lbm(struct image *img, FILE *fp);

#endif	/* IMAGE_LBM_H_ */
