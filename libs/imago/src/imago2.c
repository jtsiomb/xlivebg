/*
libimago - a multi-format image file input/output library.
Copyright (C) 2010-2020 John Tsiombikas <nuclear@member.fsf.org>

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imago2.h"
#include "ftype_module.h"

static int pixel_size(enum img_fmt fmt);
static size_t def_read(void *buf, size_t bytes, void *uptr);
static size_t def_write(void *buf, size_t bytes, void *uptr);
static long def_seek(long offset, int whence, void *uptr);


void img_init(struct img_pixmap *img)
{
	img->pixels = 0;
	img->width = img->height = 0;
	img->fmt = IMG_FMT_RGBA32;
	img->pixelsz = pixel_size(img->fmt);
	img->name = 0;
}


void img_destroy(struct img_pixmap *img)
{
	free(img->pixels);
	img->pixels = 0;	/* just in case... */
	img->width = img->height = 0xbadbeef;
	free(img->name);
}

struct img_pixmap *img_create(void)
{
	struct img_pixmap *p;

	if(!(p = malloc(sizeof *p))) {
		return 0;
	}
	img_init(p);
	return p;
}

void img_free(struct img_pixmap *img)
{
	img_destroy(img);
	free(img);
}

int img_set_name(struct img_pixmap *img, const char *name)
{
	char *tmp;

	if(!(tmp = malloc(strlen(name) + 1))) {
		return -1;
	}
	strcpy(tmp, name);
	img->name = tmp;
	return 0;
}

int img_set_format(struct img_pixmap *img, enum img_fmt fmt)
{
	if(img->pixels) {
		return img_convert(img, fmt);
	}
	img->fmt = fmt;
	return 0;
}

int img_copy(struct img_pixmap *dest, struct img_pixmap *src)
{
	return img_set_pixels(dest, src->width, src->height, src->fmt, src->pixels);
}

int img_set_pixels(struct img_pixmap *img, int w, int h, enum img_fmt fmt, void *pix)
{
	void *newpix;
	int pixsz = pixel_size(fmt);

	if(!(newpix = malloc(w * h * pixsz))) {
		return -1;
	}

	if(pix) {
		memcpy(newpix, pix, w * h * pixsz);
	} else {
		memset(newpix, 0, w * h * pixsz);
	}

	free(img->pixels);
	img->pixels = newpix;
	img->width = w;
	img->height = h;
	img->pixelsz = pixsz;
	img->fmt = fmt;
	return 0;
}

void *img_load_pixels(const char *fname, int *xsz, int *ysz, enum img_fmt fmt)
{
	struct img_pixmap img;

	img_init(&img);

	if(img_load(&img, fname) == -1) {
		return 0;
	}
	if(img.fmt != fmt) {
		if(img_convert(&img, fmt) == -1) {
			img_destroy(&img);
			return 0;
		}
	}

	*xsz = img.width;
	*ysz = img.height;
	return img.pixels;
}

int img_save_pixels(const char *fname, void *pix, int xsz, int ysz, enum img_fmt fmt)
{
	int res;
	struct img_pixmap img;

	img_init(&img);
	img.fmt = fmt;
	img.width = xsz;
	img.height = ysz;
	img.pixels = pix;

	res = img_save(&img, fname);
	img.pixels = 0;
	img_destroy(&img);
	return res;
}

void img_free_pixels(void *pix)
{
	free(pix);
}

int img_load(struct img_pixmap *img, const char *fname)
{
	int res;
	FILE *fp;

	if(!(fp = fopen(fname, "rb"))) {
		return -1;
	}
	res = img_read_file(img, fp);
	fclose(fp);
	return res;
}

/* TODO implement filetype selection */
int img_save(struct img_pixmap *img, const char *fname)
{
	int res;
	FILE *fp;

	img_set_name(img, fname);

	if(!(fp = fopen(fname, "wb"))) {
		return -1;
	}
	res = img_write_file(img, fp);
	fclose(fp);
	return res;
}

int img_read_file(struct img_pixmap *img, FILE *fp)
{
	struct img_io io = {0, def_read, def_write, def_seek};

	io.uptr = fp;
	return img_read(img, &io);
}

int img_write_file(struct img_pixmap *img, FILE *fp)
{
	struct img_io io = {0, def_read, def_write, def_seek};

	io.uptr = fp;
	return img_write(img, &io);
}

int img_read(struct img_pixmap *img, struct img_io *io)
{
	struct ftype_module *mod;

	if((mod = img_find_format_module(io))) {
		return mod->read(img, io);
	}
	return -1;
}

int img_write(struct img_pixmap *img, struct img_io *io)
{
	struct ftype_module *mod;

	if(!img->name || !(mod = img_guess_format(img->name))) {
		/* TODO throw some sort of warning? */
		/* TODO implement some sort of module priority or let the user specify? */
		if(!(mod = img_get_module(0))) {
			return -1;
		}
	}

	return mod->write(img, io);
}

int img_to_float(struct img_pixmap *img)
{
	enum img_fmt targ_fmt;

	switch(img->fmt) {
	case IMG_FMT_GREY8:
		targ_fmt = IMG_FMT_GREYF;
		break;

	case IMG_FMT_RGB24:
		targ_fmt = IMG_FMT_RGBF;
		break;

	case IMG_FMT_RGBA32:
		targ_fmt = IMG_FMT_RGBAF;
		break;

	default:
		return 0;	/* already float */
	}

	return img_convert(img, targ_fmt);
}

int img_to_integer(struct img_pixmap *img)
{
	enum img_fmt targ_fmt;

	switch(img->fmt) {
	case IMG_FMT_GREYF:
		targ_fmt = IMG_FMT_GREY8;
		break;

	case IMG_FMT_RGBF:
		targ_fmt = IMG_FMT_RGB24;
		break;

	case IMG_FMT_RGBAF:
		targ_fmt = IMG_FMT_RGBA32;
		break;

	default:
		return 0;	/* already integer */
	}

	return img_convert(img, targ_fmt);
}

int img_is_float(struct img_pixmap *img)
{
	return img->fmt >= IMG_FMT_GREYF && img->fmt <= IMG_FMT_RGBAF;
}

int img_has_alpha(struct img_pixmap *img)
{
	if(img->fmt == IMG_FMT_RGBA32 || img->fmt == IMG_FMT_RGBAF) {
		return 1;
	}
	return 0;
}

int img_is_greyscale(struct img_pixmap *img)
{
	return img->fmt == IMG_FMT_GREY8 || img->fmt == IMG_FMT_GREYF;
}


void img_setpixel(struct img_pixmap *img, int x, int y, void *pixel)
{
	char *dest = (char*)img->pixels + (y * img->width + x) * img->pixelsz;
	memcpy(dest, pixel, img->pixelsz);
}

void img_getpixel(struct img_pixmap *img, int x, int y, void *pixel)
{
	char *dest = (char*)img->pixels + (y * img->width + x) * img->pixelsz;
	memcpy(pixel, dest, img->pixelsz);
}

void img_setpixel1i(struct img_pixmap *img, int x, int y, int pix)
{
	img_setpixel4i(img, x, y, pix, pix, pix, pix);
}

void img_setpixel1f(struct img_pixmap *img, int x, int y, float pix)
{
	img_setpixel4f(img, x, y, pix, pix, pix, pix);
}

void img_setpixel4i(struct img_pixmap *img, int x, int y, int r, int g, int b, int a)
{
	if(img_is_float(img)) {
		img_setpixel4f(img, x, y, r / 255.0, g / 255.0, b / 255.0, a / 255.0);
	} else {
		unsigned char pixel[4];
		pixel[0] = r;
		pixel[1] = g;
		pixel[2] = b;
		pixel[3] = a;

		img_setpixel(img, x, y, pixel);
	}
}

void img_setpixel4f(struct img_pixmap *img, int x, int y, float r, float g, float b, float a)
{
	if(img_is_float(img)) {
		float pixel[4];
		pixel[0] = r;
		pixel[1] = g;
		pixel[2] = b;
		pixel[3] = a;

		img_setpixel(img, x, y, pixel);
	} else {
		img_setpixel4i(img, x, y, (int)(r * 255.0), (int)(g * 255.0), (int)(b * 255.0), (int)(a * 255.0));
	}
}

void img_getpixel1i(struct img_pixmap *img, int x, int y, int *pix)
{
	int junk[3];
	img_getpixel4i(img, x, y, pix, junk, junk + 1, junk + 2);
}

void img_getpixel1f(struct img_pixmap *img, int x, int y, float *pix)
{
	float junk[3];
	img_getpixel4f(img, x, y, pix, junk, junk + 1, junk + 2);
}

void img_getpixel4i(struct img_pixmap *img, int x, int y, int *r, int *g, int *b, int *a)
{
	if(img_is_float(img)) {
		float pixel[4] = {0, 0, 0, 0};
		img_getpixel(img, x, y, pixel);
		*r = pixel[0] * 255.0;
		*g = pixel[1] * 255.0;
		*b = pixel[2] * 255.0;
		*a = pixel[3] * 255.0;
	} else {
		unsigned char pixel[4];
		img_getpixel(img, x, y, pixel);
		*r = pixel[0];
		*g = pixel[1];
		*b = pixel[2];
		*a = pixel[3];
	}
}

void img_getpixel4f(struct img_pixmap *img, int x, int y, float *r, float *g, float *b, float *a)
{
	if(img_is_float(img)) {
		float pixel[4] = {0, 0, 0, 0};
		img_getpixel(img, x, y, pixel);
		*r = pixel[0];
		*g = pixel[1];
		*b = pixel[2];
		*a = pixel[3];
	} else {
		unsigned char pixel[4];
		img_getpixel(img, x, y, pixel);
		*r = pixel[0] / 255.0;
		*g = pixel[1] / 255.0;
		*b = pixel[2] / 255.0;
		*a = pixel[3] / 255.0;
	}
}

void img_io_set_user_data(struct img_io *io, void *uptr)
{
	io->uptr = uptr;
}

void img_io_set_read_func(struct img_io *io, size_t (*read)(void*, size_t, void*))
{
	io->read = read;
}

void img_io_set_write_func(struct img_io *io, size_t (*write)(void*, size_t, void*))
{
	io->write = write;
}

void img_io_set_seek_func(struct img_io *io, long (*seek)(long, int, void*))
{
	io->seek = seek;
}


static int pixel_size(enum img_fmt fmt)
{
	switch(fmt) {
	case IMG_FMT_GREY8:
		return 1;
	case IMG_FMT_RGB24:
		return 3;
	case IMG_FMT_RGBA32:
	case IMG_FMT_BGRA32:
		return 4;
	case IMG_FMT_GREYF:
		return sizeof(float);
	case IMG_FMT_RGBF:
		return 3 * sizeof(float);
	case IMG_FMT_RGBAF:
		return 4 * sizeof(float);
	case IMG_FMT_RGB565:
		return 2;
	default:
		break;
	}
	return 0;
}

static size_t def_read(void *buf, size_t bytes, void *uptr)
{
	return uptr ? fread(buf, 1, bytes, uptr) : 0;
}

static size_t def_write(void *buf, size_t bytes, void *uptr)
{
	return uptr ? fwrite(buf, 1, bytes, uptr) : 0;
}

static long def_seek(long offset, int whence, void *uptr)
{
	if(!uptr || fseek(uptr, offset, whence) == -1) {
		return -1;
	}
	return ftell(uptr);
}

