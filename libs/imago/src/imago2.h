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

#ifndef IMAGO2_H_
#define IMAGO2_H_

#include <stdio.h>

#ifdef __cplusplus
#define IMG_OPTARG(arg, val)	arg = val
#else
#define IMG_OPTARG(arg, val)	arg
#endif

/* XXX if you change this make sure to also change pack/unpack arrays in conv.c */
enum img_fmt {
	IMG_FMT_GREY8,
	IMG_FMT_RGB24,
	IMG_FMT_RGBA32,
	IMG_FMT_GREYF,
	IMG_FMT_RGBF,
	IMG_FMT_RGBAF,
	IMG_FMT_BGRA32,
	IMG_FMT_RGB565,

	NUM_IMG_FMT
};

struct img_pixmap {
	void *pixels;
	int width, height;
	enum img_fmt fmt;
	int pixelsz;
	char *name;
};

struct img_io {
	void *uptr;	/* user-data */

	size_t (*read)(void *buf, size_t bytes, void *uptr);
	size_t (*write)(void *buf, size_t bytes, void *uptr);
	long (*seek)(long offs, int whence, void *uptr);
};

#ifdef __cplusplus
extern "C" {
#endif

/* initialize the img_pixmap structure */
void img_init(struct img_pixmap *img);
/* destroys the img_pixmap structure, freeing the pixel buffer (if available)
 * and any other memory held by the pixmap.
 */
void img_destroy(struct img_pixmap *img);

/* convenience function that allocates an img_pixmap struct and then initializes it.
 * returns null if the malloc fails.
 */
struct img_pixmap *img_create(void);
/* frees a pixmap previously allocated with img_create (free followed by img_destroy) */
void img_free(struct img_pixmap *img);

int img_set_name(struct img_pixmap *img, const char *name);

/* set the image pixel format */
int img_set_format(struct img_pixmap *img, enum img_fmt fmt);

/* copies one pixmap to another.
 * equivalent to: img_set_pixels(dest, src->width, src->height, src->fmt, src->pixels)
 */
int img_copy(struct img_pixmap *dest, struct img_pixmap *src);

/* allocates a pixel buffer of the specified dimensions and format, and copies the
 * pixels given through the pix pointer into it.
 * the pix pointer can be null, in which case there's no copy, just allocation.
 *
 * C++: fmt and pix have default parameters IMG_FMT_RGBA32 and null respectively.
 */
int img_set_pixels(struct img_pixmap *img, int w, int h, IMG_OPTARG(enum img_fmt fmt, IMG_FMT_RGBA32), IMG_OPTARG(void *pix, 0));

/* Simplified image loading
 * Loads the specified file, and returns a pointer to an array of pixels of the
 * requested pixel format. The width and height of the image are returned through
 * the xsz and ysz pointers.
 * If the image cannot be loaded, the function returns null.
 *
 * C++: the format argument is optional and defaults to IMG_FMT_RGBA32
 */
void *img_load_pixels(const char *fname, int *xsz, int *ysz, IMG_OPTARG(enum img_fmt fmt, IMG_FMT_RGBA32));

/* Simplified image saving
 * Reads an array of pixels supplied through the pix pointer, of dimensions xsz
 * and ysz, and pixel-format fmt, and saves it to a file.
 * The output filetype is guessed by the filename suffix.
 *
 * C++: the format argument is optional and defaults to IMG_FMT_RGBA32
 */
int img_save_pixels(const char *fname, void *pix, int xsz, int ysz, IMG_OPTARG(enum img_fmt fmt, IMG_FMT_RGBA32));

/* Frees the memory allocated by img_load_pixels */
void img_free_pixels(void *pix);

/* Loads an image file into the supplied pixmap */
int img_load(struct img_pixmap *img, const char *fname);
/* Saves the supplied pixmap to a file. The output filetype is guessed by the filename suffix */
int img_save(struct img_pixmap *img, const char *fname);

/* Reads an image from an open FILE* into the supplied pixmap */
int img_read_file(struct img_pixmap *img, FILE *fp);
/* Writes the supplied pixmap to an open FILE* */
int img_write_file(struct img_pixmap *img, FILE *fp);

/* Reads an image using user-defined file-i/o functions (see img_io_set_*) */
int img_read(struct img_pixmap *img, struct img_io *io);
/* Writes an image using user-defined file-i/o functions (see img_io_set_*) */
int img_write(struct img_pixmap *img, struct img_io *io);

/* Converts an image to the specified pixel format */
int img_convert(struct img_pixmap *img, enum img_fmt tofmt);

/* Converts an image from an integer pixel format to the corresponding floating point one */
int img_to_float(struct img_pixmap *img);
/* Converts an image from a floating point pixel format to the corresponding integer one */
int img_to_integer(struct img_pixmap *img);

/* Returns non-zero (true) if the supplied image is in a floating point pixel format */
int img_is_float(struct img_pixmap *img);
/* Returns non-zero (true) if the supplied image has an alpha channel */
int img_has_alpha(struct img_pixmap *img);
/* Returns non-zero (true) if the supplied image is greyscale */
int img_is_greyscale(struct img_pixmap *img);


/* don't use these for anything performance-critical */
void img_setpixel(struct img_pixmap *img, int x, int y, void *pixel);
void img_getpixel(struct img_pixmap *img, int x, int y, void *pixel);

void img_setpixel1i(struct img_pixmap *img, int x, int y, int pix);
void img_setpixel1f(struct img_pixmap *img, int x, int y, float pix);
void img_setpixel4i(struct img_pixmap *img, int x, int y, int r, int g, int b, int a);
void img_setpixel4f(struct img_pixmap *img, int x, int y, float r, float g, float b, float a);

void img_getpixel1i(struct img_pixmap *img, int x, int y, int *pix);
void img_getpixel1f(struct img_pixmap *img, int x, int y, float *pix);
void img_getpixel4i(struct img_pixmap *img, int x, int y, int *r, int *g, int *b, int *a);
void img_getpixel4f(struct img_pixmap *img, int x, int y, float *r, float *g, float *b, float *a);


/* OpenGL helper functions */

/* Returns the equivalent OpenGL "format" as expected by the 7th argument of glTexImage2D */
unsigned int img_fmt_glfmt(enum img_fmt fmt);
/* Returns the equivalent OpenGL "type" as expected by the 8th argument of glTexImage2D */
unsigned int img_fmt_gltype(enum img_fmt fmt);
/* Returns the equivalent OpenGL "internal format" as expected by the 3rd argument of glTexImage2D */
unsigned int img_fmt_glintfmt(enum img_fmt fmt);

/* Same as above, based on the pixel format of the supplied image */
unsigned int img_glfmt(struct img_pixmap *img);
unsigned int img_gltype(struct img_pixmap *img);
unsigned int img_glintfmt(struct img_pixmap *img);

/* Creates an OpenGL texture from the image, and returns the texture id, or 0 for failure */
unsigned int img_gltexture(struct img_pixmap *img);

/* Load an image and create an OpenGL texture out of it */
unsigned int img_gltexture_load(const char *fname);
unsigned int img_gltexture_read_file(FILE *fp);
unsigned int img_gltexture_read(struct img_io *io);

/* These functions can be used to fill an img_io struct before it's passed to
 * one of the user-defined i/o image reading/writing functions (img_read/img_write).
 *
 * User-defined i/o functions:
 *
 * - size_t read_func(void *buffer, size_t bytes, void *user_ptr)
 * Must try to fill the buffer with the specified number of bytes, and return
 * the number of bytes actually read.
 *
 * - size_t write_func(void *buffer, size_t bytes, void *user_ptr)
 * Must write the specified number of bytes from the supplied buffer and return
 * the number of bytes actually written.
 *
 * - long seek_func(long offset, int whence, void *user_ptr)
 * Must seek offset bytes from: the beginning of the file if whence is SEEK_SET,
 * the current position if whence is SEEK_CUR, or the end of the file if whence is
 * SEEK_END, and return the resulting file offset from the beginning of the file.
 * (i.e. seek_func(0, SEEK_CUR, user_ptr); must be equivalent to an ftell).
 *
 * All three functions get the user-data pointer set through img_io_set_user_data
 * as their last argument.
 *
 * Note: obviously you don't need to set a write function if you're only going
 * to call img_read, or the read and seek function if you're only going to call
 * img_write.
 *
 * Note: if the user-supplied write function is buffered, make sure to flush
 * (or close the file) after img_write returns.
 */
void img_io_set_user_data(struct img_io *io, void *uptr);
void img_io_set_read_func(struct img_io *io, size_t (*read)(void*, size_t, void*));
void img_io_set_write_func(struct img_io *io, size_t (*write)(void*, size_t, void*));
void img_io_set_seek_func(struct img_io *io, long (*seek)(long, int, void*));


#ifdef __cplusplus
}
#endif


#endif	/* IMAGO_H_ */
