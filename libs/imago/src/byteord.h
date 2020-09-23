/*
libimago - a multi-format image file input/output library.
Copyright (C) 2010-2017 John Tsiombikas <nuclear@member.fsf.org>

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
#ifndef IMAGO_BYTEORD_H_
#define IMAGO_BYTEORD_H_

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199900
#include <stdint.h>
#else
#include <sys/types.h>
#endif
#include "imago2.h"

#if  defined(__i386__) || defined(__ia64__) || defined(WIN32) || \
    (defined(__alpha__) || defined(__alpha)) || \
     defined(__arm__) || \
    (defined(__mips__) && defined(__MIPSEL__)) || \
     defined(__SYMBIAN32__) || \
     defined(__x86_64__) || \
     defined(__LITTLE_ENDIAN__)
/* little endian */
#define IMAGO_LITTLE_ENDIAN

#define img_read_int16_le(f)		img_read_int16(f)
#define img_write_int16_le(f, v)	img_write_int16(f, v)
#define img_read_int16_be(f)		img_read_int16_inv(f)
#define img_write_int16_be(f, v)	img_write_int16_inv(f, v)
#define img_read_uint16_le(f)		img_read_uint16(f)
#define img_write_uint16_le(f, v)	img_write_uint16(f, v)
#define img_read_uint16_be(f)		img_read_uint16_inv(f)
#define img_write_uint16_be(f, v)	img_write_uint16_inv(f, v)

#define img_read_uint32_be(f)		img_read_uint32_inv(f)
#else
/* big endian */
#define IMAGO_BIG_ENDIAN

#define img_read_int16_le(f)		img_read_int16_inv(f)
#define img_write_int16_le(f, v)	img_write_int16_inv(f, v)
#define img_read_int16_be(f)		img_read_int16(f)
#define img_write_int16_be(f, v)	img_write_int16(f, v)
#define img_read_uint16_le(f)		img_read_uint16_inv(f)
#define img_write_uint16_le(f, v)	img_write_uint16_inv(f, v)
#define img_read_uint16_be(f)		img_read_uint16(f)
#define img_write_uint16_be(f, v)	img_write_uint16(f, v)

#define img_read_uint32_be(f)		img_read_uint32(f)
#endif	/* endian check */

int16_t img_read_int16(struct img_io *io);
int16_t img_read_int16_inv(struct img_io *io);
uint16_t img_read_uint16(struct img_io *io);
uint16_t img_read_uint16_inv(struct img_io *io);

void img_write_int16(struct img_io *io, int16_t val);
void img_write_int16_inv(struct img_io *io, int16_t val);
void img_write_uint16(struct img_io *io, uint16_t val);
void img_write_uint16_inv(struct img_io *io, uint16_t val);

uint32_t img_read_uint32(struct img_io *io);
uint32_t img_read_uint32_inv(struct img_io *io);


#endif	/* IMAGO_BYTEORD_H_ */
