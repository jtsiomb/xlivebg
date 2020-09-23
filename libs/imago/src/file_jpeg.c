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

/* -- JPEG module -- */
#ifndef NO_JPEG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#define HAVE_BOOLEAN
#endif

#include <jpeglib.h>
#include "imago2.h"
#include "ftype_module.h"

#define INPUT_BUF_SIZE	512
#define OUTPUT_BUF_SIZE	512

/* data source manager: adapted from jdatasrc.c */
struct src_mgr {
	struct jpeg_source_mgr pub;

	struct img_io *io;
	unsigned char buffer[INPUT_BUF_SIZE];
	int start_of_file;
};

/* data destination manager: adapted from jdatadst.c */
struct dst_mgr {
	struct jpeg_destination_mgr pub;

	struct img_io *io;
	unsigned char buffer[OUTPUT_BUF_SIZE];
};

static int check(struct img_io *io);
static int read(struct img_pixmap *img, struct img_io *io);
static int write(struct img_pixmap *img, struct img_io *io);

/* read source functions */
static void init_source(j_decompress_ptr jd);
static boolean fill_input_buffer(j_decompress_ptr jd);
static void skip_input_data(j_decompress_ptr jd, long num_bytes);
static void term_source(j_decompress_ptr jd);

/* write destination functions */
static void init_destination(j_compress_ptr jc);
static boolean empty_output_buffer(j_compress_ptr jc);
static void term_destination(j_compress_ptr jc);

int img_register_jpeg(void)
{
	static struct ftype_module mod = {".jpg:.jpeg", check, read, write};
	return img_register_module(&mod);
}


static int check(struct img_io *io)
{
	unsigned char sig[10];

	long pos = io->seek(0, SEEK_CUR, io->uptr);

	if(io->read(sig, 10, io->uptr) < 10) {
		io->seek(pos, SEEK_SET, io->uptr);
		return -1;
	}

	if(memcmp(sig, "\xff\xd8\xff\xe0", 4) != 0 && memcmp(sig, "\xff\xd8\xff\xe1", 4) != 0
			&& memcmp(sig, "\xff\xd8\xff\xdb", 4) != 0 && memcmp(sig + 6, "JFIF", 4) != 0) {
		io->seek(pos, SEEK_SET, io->uptr);
		return -1;
	}
	io->seek(pos, SEEK_SET, io->uptr);
	return 0;
}

static int read(struct img_pixmap *img, struct img_io *io)
{
	int i, nlines = 0;
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	struct src_mgr src;
	unsigned char **scanlines;

	io->seek(0, SEEK_CUR, io->uptr);

	cinfo.err = jpeg_std_error(&jerr);	/* XXX change... */
	jpeg_create_decompress(&cinfo);

	src.pub.init_source = init_source;
	src.pub.fill_input_buffer = fill_input_buffer;
	src.pub.skip_input_data = skip_input_data;
	src.pub.resync_to_restart = jpeg_resync_to_restart;
	src.pub.term_source = term_source;
	src.pub.next_input_byte = 0;
	src.pub.bytes_in_buffer = 0;
	src.io = io;
	cinfo.src = (struct jpeg_source_mgr*)&src;

	jpeg_read_header(&cinfo, 1);
	cinfo.out_color_space = JCS_RGB;

	if(img_set_pixels(img, cinfo.image_width, cinfo.image_height, IMG_FMT_RGB24, 0) == -1) {
		jpeg_destroy_decompress(&cinfo);
		return -1;
	}

	if(!(scanlines = malloc(img->height * sizeof *scanlines))) {
		jpeg_destroy_decompress(&cinfo);
		return -1;
	}
	scanlines[0] = img->pixels;
	for(i=1; i<img->height; i++) {
		scanlines[i] = scanlines[i - 1] + img->width * img->pixelsz;
	}

	jpeg_start_decompress(&cinfo);
	while(nlines < img->height) {
		int res = jpeg_read_scanlines(&cinfo, scanlines + nlines, img->height - nlines);
		nlines += res;
	}
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	free(scanlines);
	return 0;
}

static int write(struct img_pixmap *img, struct img_io *io)
{
	int i, nlines = 0;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	struct dst_mgr dest;
	struct img_pixmap tmpimg;
	unsigned char **scanlines;

	img_init(&tmpimg);

	if(img->fmt != IMG_FMT_RGB24) {
		if(img_copy(&tmpimg, img) == -1) {
			return -1;
		}
		if(img_convert(&tmpimg, IMG_FMT_RGB24) == -1) {
			img_destroy(&tmpimg);
			return -1;
		}
		img = &tmpimg;
	}

	if(!(scanlines = malloc(img->height * sizeof *scanlines))) {
		img_destroy(&tmpimg);
		return -1;
	}
	scanlines[0] = img->pixels;
	for(i=1; i<img->height; i++) {
		scanlines[i] = scanlines[i - 1] + img->width * img->pixelsz;
	}

	cinfo.err = jpeg_std_error(&jerr);	/* XXX */
	jpeg_create_compress(&cinfo);

	dest.pub.init_destination = init_destination;
	dest.pub.empty_output_buffer = empty_output_buffer;
	dest.pub.term_destination = term_destination;
	dest.io = io;
	cinfo.dest = (struct jpeg_destination_mgr*)&dest;

	cinfo.image_width = img->width;
	cinfo.image_height = img->height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 95, 0);

	jpeg_start_compress(&cinfo, 1);
	while(nlines < img->height) {
		int res = jpeg_write_scanlines(&cinfo, scanlines + nlines, img->height - nlines);
		nlines += res;
	}
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	free(scanlines);
	img_destroy(&tmpimg);
	return 0;
}

/* -- read source functions --
 * the following functions are adapted from jdatasrc.c in jpeglib
 */
static void init_source(j_decompress_ptr jd)
{
	struct src_mgr *src = (struct src_mgr*)jd->src;
	src->start_of_file = 1;
}

static boolean fill_input_buffer(j_decompress_ptr jd)
{
	struct src_mgr *src = (struct src_mgr*)jd->src;
	size_t nbytes;

	nbytes = src->io->read(src->buffer, INPUT_BUF_SIZE, src->io->uptr);

	if(nbytes <= 0) {
		if(src->start_of_file) {
			return 0;
		}
		/* insert a fake EOI marker */
		src->buffer[0] = 0xff;
		src->buffer[1] = JPEG_EOI;
		nbytes = 2;
	}

	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;
	src->start_of_file = 0;
	return 1;
}

static void skip_input_data(j_decompress_ptr jd, long num_bytes)
{
	struct src_mgr *src = (struct src_mgr*)jd->src;

	if(num_bytes > 0) {
		while(num_bytes > (long)src->pub.bytes_in_buffer) {
			num_bytes -= (long)src->pub.bytes_in_buffer;
			fill_input_buffer(jd);
		}
		src->pub.next_input_byte += (size_t)num_bytes;
		src->pub.bytes_in_buffer -= (size_t)num_bytes;
	}
}

static void term_source(j_decompress_ptr jd)
{
	/* nothing to see here, move along */
}


/* -- write destination functions --
 * the following functions are adapted from jdatadst.c in jpeglib
 */
static void init_destination(j_compress_ptr jc)
{
	struct dst_mgr *dest = (struct dst_mgr*)jc->dest;

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

static boolean empty_output_buffer(j_compress_ptr jc)
{
	struct dst_mgr *dest = (struct dst_mgr*)jc->dest;

	if(dest->io->write(dest->buffer, OUTPUT_BUF_SIZE, dest->io->uptr) != OUTPUT_BUF_SIZE) {
		return 0;
	}

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
	return 1;
}

static void term_destination(j_compress_ptr jc)
{
	struct dst_mgr *dest = (struct dst_mgr*)jc->dest;
	size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

	/* write any remaining data in the buffer */
	if(datacount > 0) {
		dest->io->write(dest->buffer, datacount, dest->io->uptr);
	}
	/* XXX flush? ... */
}

#else
/* build without JPEG support */
int img_register_jpeg(void)
{
	return -1;
}
#endif
