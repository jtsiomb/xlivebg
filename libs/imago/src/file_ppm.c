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

/* -- Portable Pixmap (PPM) module (also supports PGM) -- */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "imago2.h"
#include "ftype_module.h"
#include "byteord.h"

static int check(struct img_io *io);
static int read(struct img_pixmap *img, struct img_io *io);
static int write(struct img_pixmap *img, struct img_io *io);

int img_register_ppm(void)
{
	static struct ftype_module mod = {".ppm:.pgm:.pnm", check, read, write};
	return img_register_module(&mod);
}


static int check(struct img_io *io)
{
	char id[2];
	int res = -1;
	long pos = io->seek(0, SEEK_CUR, io->uptr);

	if(io->read(id, 2, io->uptr) < 2) {
		io->seek(pos, SEEK_SET, io->uptr);
		return -1;
	}

	if(id[0] == 'P' && (id[1] == '6' || id[1] == '3' || id[1] == '5')) {
		res = 0;
	}
	io->seek(pos, SEEK_SET, io->uptr);
	return res;
}

static int iofgetc(struct img_io *io)
{
	char c;
	return io->read(&c, 1, io->uptr) < 1 ? -1 : c;
}

static char *iofgets(char *buf, int size, struct img_io *io)
{
	int c;
	char *ptr = buf;

	while(--size > 0 && (c = iofgetc(io)) != -1) {
		*ptr++ = c;
		if(c == '\n') break;
	}
	*ptr = 0;

	return ptr == buf ? 0 : buf;
}

static int read(struct img_pixmap *img, struct img_io *io)
{
	char buf[256];
	int xsz, ysz, maxval, got_hdrlines = 1;
	int i, greyscale, numval, valsize, fbsize, text;
	enum img_fmt fmt;

	if(!iofgets(buf, sizeof buf, io)) {
		return -1;
	}
	if(!(buf[0] == 'P' && (buf[1] == '6' || buf[1] == '3' || buf[1] == '5'))) {
		return -1;
	}
	greyscale = buf[1] == '5' ? 1 : 0;
	text = buf[1] == '3' ? 1 : 0;

	while(got_hdrlines < 3 && iofgets(buf, sizeof buf, io)) {
		if(buf[0] == '#') continue;

		switch(got_hdrlines) {
		case 1:
			if(sscanf(buf, "%d %d\n", &xsz, &ysz) < 2) {
				return -1;
			}
			break;

		case 2:
			if(sscanf(buf, "%d\n", &maxval) < 1) {
				return -1;
			}
		default:
			break;
		}
		got_hdrlines++;
	}

	if(xsz < 1 || ysz < 1 || maxval <= 0 || maxval > 65535) {
		return -1;
	}

	valsize = maxval < 256 ? 1 : 2;
	numval = xsz * ysz * (greyscale ? 1 : 3);
	fbsize = numval * valsize;

	if(valsize > 1) {
		fmt = greyscale ? IMG_FMT_GREYF : IMG_FMT_RGBF;
	} else {
		fmt = greyscale ? IMG_FMT_GREY8 : IMG_FMT_RGB24;
	}

	if(img_set_pixels(img, xsz, ysz, fmt, 0) == -1) {
		return -1;
	}

	if(!text) {
		if(io->read(img->pixels, fbsize, io->uptr) < (unsigned int)fbsize) {
			return -1;
		}
		if(maxval == 255) {
			return 0;	/* we're done, no conversion necessary */
		}

		if(maxval < 256) {
			unsigned char *ptr = img->pixels;
			for(i=0; i<numval; i++) {
				unsigned char c = *ptr * 255 / maxval;
				*ptr++ = c;
			}
		} else {
			/* we allocated a floating point framebuffer, and dropped the 16bit pixels
			 * into it. To convert it in-place we'll iterate backwards from the end, since
			 * otherwise each 32bit floating point value we store, would overwrite the next
			 * pixel.
			 */
			uint16_t *src = (uint16_t*)img->pixels + numval;
			float *dest = (float*)img->pixels + numval;

			for(i=0; i<numval; i++) {
				uint16_t val = *--src;
#ifdef IMAGO_LITTLE_ENDIAN
				val = (val >> 8) | (val << 8);
#endif
				*--dest = (float)val / (float)maxval;
			}
		}
	} else {
		char *pptr = img->pixels;
		int c = iofgetc(io);

		for(i=0; i<numval; i++) {
			char *valptr = buf;

			while(c != -1 && isspace(c)) {
				c = iofgetc(io);
			}

			while(c != -1 && !isspace(c) && valptr - buf < sizeof buf - 1) {
				*valptr++ = c;
				c = iofgetc(io);
			}
			if(c == -1) break;
			*valptr = 0;

			*pptr++ = atoi(buf) * 255 / maxval;
		}
	}
	return 0;
}

static int write(struct img_pixmap *img, struct img_io *io)
{
	int i, sz, nval, res = -1;
	char buf[256];
	float *fptr, maxfval;
	struct img_pixmap tmpimg;
	static const char *fmt = "P%d\n#written by libimago2\n%d %d\n%d\n";
	int greyscale = img_is_greyscale(img);

	nval = greyscale ? 1 : 3;

	img_init(&tmpimg);

	switch(img->fmt) {
	case IMG_FMT_RGBA32:
		if(img_copy(&tmpimg, img) == -1) {
			goto done;
		}
		if(img_convert(&tmpimg, IMG_FMT_RGB24) == -1) {
			goto done;
		}
		img = &tmpimg;

	case IMG_FMT_RGB24:
	case IMG_FMT_GREY8:
		sprintf(buf, fmt, greyscale ? 5 : 6, img->width, img->height, 255);
		if(io->write(buf, strlen(buf), io->uptr) < strlen(buf)) {
			goto done;
		}
		sz = img->width * img->height * nval;
		if(io->write(img->pixels, sz, io->uptr) < (unsigned int)sz) {
			goto done;
		}
		res = 0;
		break;

	case IMG_FMT_RGBAF:
		if(img_copy(&tmpimg, img) == -1) {
			goto done;
		}
		if(img_convert(&tmpimg, IMG_FMT_RGBF) == -1) {
			goto done;
		}
		img = &tmpimg;

	case IMG_FMT_RGBF:
	case IMG_FMT_GREYF:
		sprintf(buf, fmt, greyscale ? 5 : 6, img->width, img->height, 65535);
		if(io->write(buf, strlen(buf), io->uptr) < strlen(buf)) {
			goto done;
		}
		fptr = img->pixels;
		maxfval = 0;
		for(i=0; i<img->width * img->height * nval; i++) {
			float val = *fptr++;
			if(val > maxfval) maxfval = val;
		}
		fptr = img->pixels;
		for(i=0; i<img->width * img->height * nval; i++) {
			uint16_t val = (uint16_t)(*fptr++ / maxfval * 65535.0);
			img_write_uint16_be(io, val);
		}
		res = 0;
		break;

	default:
		break;
	}

done:
	img_destroy(&tmpimg);
	return res;
}
