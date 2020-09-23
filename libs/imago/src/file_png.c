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

/* -- PNG module -- */
#ifndef NO_PNG

#include <stdlib.h>
#include <string.h>
#include <png.h>
#include "imago2.h"
#include "ftype_module.h"

static int check_file(struct img_io *io);
static int read_file(struct img_pixmap *img, struct img_io *io);
static int write_file(struct img_pixmap *img, struct img_io *io);

static void read_func(png_struct *png, unsigned char *data, size_t len);
static void write_func(png_struct *png, unsigned char *data, size_t len);
static void flush_func(png_struct *png);

static int png_type_to_fmt(int color_type, int channel_bits);
static int fmt_to_png_type(enum img_fmt fmt);


int img_register_png(void)
{
	static struct ftype_module mod = {".png", check_file, read_file, write_file};
	return img_register_module(&mod);
}

static int check_file(struct img_io *io)
{
	unsigned char sig[8];
	int res;
	long pos = io->seek(0, SEEK_CUR, io->uptr);

	if(io->read(sig, 8, io->uptr) < 8) {
		io->seek(pos, SEEK_SET, io->uptr);
		return -1;
	}

	res = png_sig_cmp(sig, 0, 8) == 0 ? 0 : -1;
	io->seek(pos, SEEK_SET, io->uptr);
	return res;
}

static int read_file(struct img_pixmap *img, struct img_io *io)
{
	png_struct *png;
	png_info *info;
	int channel_bits, color_type, ilace_type, compression, filtering, fmt;
	png_uint_32 xsz, ysz;

	if(!(png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0))) {
		return -1;
	}

	if(!(info = png_create_info_struct(png))) {
		png_destroy_read_struct(&png, 0, 0);
		return -1;
	}

	if(setjmp(png_jmpbuf(png))) {
		png_destroy_read_struct(&png, &info, 0);
		return -1;
	}

	png_set_read_fn(png, io, read_func);
	png_set_sig_bytes(png, 0);
	png_read_png(png, info, 0, 0);

	png_get_IHDR(png, info, &xsz, &ysz, &channel_bits, &color_type, &ilace_type,
			&compression, &filtering);
	if((fmt = png_type_to_fmt(color_type, channel_bits)) == -1) {
		png_destroy_read_struct(&png, &info, 0);
		return -1;
	}

	if(img_set_pixels(img, xsz, ysz, fmt, 0) == -1) {
		png_destroy_read_struct(&png, &info, 0);
		return -1;
	}


	if(channel_bits == 8) {
		unsigned int i;
		unsigned char **lineptr = (unsigned char**)png_get_rows(png, info);
		unsigned char *dest = img->pixels;

		for(i=0; i<ysz; i++) {
			memcpy(dest, lineptr[i], xsz * img->pixelsz);
			dest += xsz * img->pixelsz;
		}
	} else {
		unsigned int i, j, num_elem;
		unsigned char **lineptr = (unsigned char**)png_get_rows(png, info);
		float *dest = img->pixels;

		num_elem = img->pixelsz / sizeof(float);
		for(i=0; i<ysz; i++) {
			for(j=0; j<xsz * num_elem; j++) {
				unsigned short val = (lineptr[i][j * 2] << 8) | lineptr[i][j * 2 + 1];
				*dest++ = (float)val / 65535.0;
			}
		}
	}


	png_destroy_read_struct(&png, &info, 0);
	return 0;
}


static int write_file(struct img_pixmap *img, struct img_io *io)
{
	png_struct *png;
	png_info *info;
	png_text txt;
	struct img_pixmap tmpimg;
	unsigned char **rows;
	unsigned char *pixptr;
	int i, coltype;

	img_init(&tmpimg);

	if(!(png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0))) {
		return -1;
	}
	if(!(info = png_create_info_struct(png))) {
		png_destroy_write_struct(&png, 0);
		return -1;
	}

	/* if the input image is floating-point, we need to convert it to integer */
	if(img_is_float(img)) {
		if(img_copy(&tmpimg, img) == -1) {
			return -1;
		}
		if(img_to_integer(&tmpimg) == -1) {
			img_destroy(&tmpimg);
			return -1;
		}
		img = &tmpimg;
	}

	txt.compression = PNG_TEXT_COMPRESSION_NONE;
	txt.key = "Software";
	txt.text = "libimago2";
	txt.text_length = 0;

	if(setjmp(png_jmpbuf(png))) {
		png_destroy_write_struct(&png, &info);
		img_destroy(&tmpimg);
		return -1;
	}
	png_set_write_fn(png, io, write_func, flush_func);

	coltype = fmt_to_png_type(img->fmt);
	png_set_IHDR(png, info, img->width, img->height, 8, coltype, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_text(png, info, &txt, 1);

	if(!(rows = malloc(img->height * sizeof *rows))) {
		png_destroy_write_struct(&png, &info);
		img_destroy(&tmpimg);
		return -1;
	}

	pixptr = img->pixels;
	for(i=0; i<img->height; i++) {
		rows[i] = pixptr;
		pixptr += img->width * img->pixelsz;
	}
	png_set_rows(png, info, rows);

	png_write_png(png, info, 0, 0);
	png_write_end(png, info);
	png_destroy_write_struct(&png, &info);

	free(rows);

	img_destroy(&tmpimg);
	return 0;
}

static void read_func(png_struct *png, unsigned char *data, size_t len)
{
	struct img_io *io = (struct img_io*)png_get_io_ptr(png);

	if(io->read(data, len, io->uptr) == -1) {
		longjmp(png_jmpbuf(png), 1);
	}
}

static void write_func(png_struct *png, unsigned char *data, size_t len)
{
	struct img_io *io = (struct img_io*)png_get_io_ptr(png);

	if(io->write(data, len, io->uptr) == -1) {
		longjmp(png_jmpbuf(png), 1);
	}
}

static void flush_func(png_struct *png)
{
	/* XXX does it matter that we can't flush? */
}

static int png_type_to_fmt(int color_type, int channel_bits)
{
	/* only 8 and 16 bits per channel ar supported at the moment */
	if(channel_bits != 8 && channel_bits != 16) {
		return -1;
	}

	switch(color_type) {
	case PNG_COLOR_TYPE_RGB:
		return channel_bits == 16 ? IMG_FMT_RGBF : IMG_FMT_RGB24;

	case PNG_COLOR_TYPE_RGB_ALPHA:
		return channel_bits == 16 ? IMG_FMT_RGBAF : IMG_FMT_RGBA32;

	case PNG_COLOR_TYPE_GRAY:
		return channel_bits == 16 ? IMG_FMT_GREYF : IMG_FMT_GREY8;

	default:
		break;
	}
	return -1;
}

static int fmt_to_png_type(enum img_fmt fmt)
{
	switch(fmt) {
	case IMG_FMT_GREY8:
		return PNG_COLOR_TYPE_GRAY;

	case IMG_FMT_RGB24:
		return PNG_COLOR_TYPE_RGB;

	case IMG_FMT_RGBA32:
		return PNG_COLOR_TYPE_RGBA;

	default:
		break;
	}
	return -1;
}

#else
/* building with PNG support disabled */

int img_register_png(void)
{
	return -1;
}

#endif
