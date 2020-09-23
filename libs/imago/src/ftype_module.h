/*
libimago - a multi-format image file input/output library.
Copyright (C) 2010 John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FTYPE_MODULE_H_
#define FTYPE_MODULE_H_

#include "imago2.h"

struct ftype_module {
	char *suffix;	/* used for format autodetection during saving only */

	int (*check)(struct img_io *io);
	int (*read)(struct img_pixmap *img, struct img_io *io);
	int (*write)(struct img_pixmap *img, struct img_io *io);
};

int img_register_module(struct ftype_module *mod);

struct ftype_module *img_find_format_module(struct img_io *io);
struct ftype_module *img_guess_format(const char *fname);
struct ftype_module *img_get_module(int idx);


#endif	/* FTYPE_MODULE_H_ */
