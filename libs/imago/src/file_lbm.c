/*
libimago - a multi-format image file input/output library.
Copyright (C) 2017 John Tsiombikas <nuclear@member.fsf.org>

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

/* -- LBM (PNM/ILBM) module -- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if defined(__WATCOMC__) || defined(WIN32)
#include <malloc.h>
#else
#if !defined(__FreeBSD__) && !defined(__OpenBSD__)
#include <alloca.h>
#endif
#endif
#include "imago2.h"
#include "ftype_module.h"
#include "byteord.h"

#ifdef __GNUC__
#define PACKED	__attribute__((packed))
#endif

#define MKID(a, b, c, d)	(((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

#define IS_IFF_CONTAINER(id)	((id) == IFF_FORM || (id) == IFF_CAT || (id) == IFF_LIST)

enum {
	IFF_FORM = MKID('F', 'O', 'R', 'M'),
	IFF_CAT = MKID('C', 'A', 'T', ' '),
	IFF_LIST = MKID('L', 'I', 'S', 'T'),
	IFF_ILBM = MKID('I', 'L', 'B', 'M'),
	IFF_PBM = MKID('P', 'B', 'M', ' '),
	IFF_BMHD = MKID('B', 'M', 'H', 'D'),
	IFF_CMAP = MKID('C', 'M', 'A', 'P'),
	IFF_BODY = MKID('B', 'O', 'D', 'Y'),
	IFF_CRNG = MKID('C', 'R', 'N', 'G')
};

struct chdr {
	uint32_t id;
	uint32_t size;
};

#if defined(__WATCOMC__) || defined(_MSC_VER)
#pragma push(pack, 1)
#endif
struct bitmap_header {
	uint16_t width, height;
	int16_t xoffs, yoffs;
	uint8_t nplanes;
	uint8_t masking;
	uint8_t compression;
	uint8_t padding;
	uint16_t colorkey;
	uint8_t aspect_num, aspect_denom;
	int16_t pgwidth, pgheight;
} PACKED;
#if defined(__WATCOMC__) || defined(_MSC_VER)
#pragma pop(pack)
#endif

enum {
	MASK_NONE,
	MASK_PLANE,
	MASK_COLORKEY,
	MASK_LASSO
};

struct crng {
	uint16_t padding;
	uint16_t rate;
	uint16_t flags;
	uint8_t low, high;
};

enum {
	CRNG_ENABLE = 1,
	CRNG_REVERSE = 2
};


static int check_file(struct img_io *io);
static int read_file(struct img_pixmap *img, struct img_io *io);
static int write_file(struct img_pixmap *img, struct img_io *io);

static int read_header(struct img_io *io, struct chdr *hdr);
static int read_ilbm_pbm(struct img_io *io, uint32_t type, uint32_t size, struct img_pixmap *img);
static void convert_rgb(struct img_pixmap *img, unsigned char *pal);
static int read_bmhd(struct img_io *io, struct bitmap_header *bmhd);
static int read_crng(struct img_io *io, struct crng *crng);
static int read_body_ilbm(struct img_io *io, struct bitmap_header *bmhd, struct img_pixmap *img);
static int read_body_pbm(struct img_io *io, struct bitmap_header *bmhd, struct img_pixmap *img);
static int read_compressed_scanline(struct img_io *io, unsigned char *scanline, int width);

#ifdef IMAGO_LITTLE_ENDIAN
static uint16_t img_swap16(uint16_t x);
static uint32_t img_swap32(uint32_t x);
#endif


int img_register_lbm(void)
{
	static struct ftype_module mod = {".lbm:.ilbm:.iff", check_file, read_file, write_file };
	return img_register_module(&mod);
}

static int check_file(struct img_io *io)
{
	uint32_t type;
	struct chdr hdr;
	long pos = io->seek(0, SEEK_CUR, io->uptr);

	while(read_header(io, &hdr) != -1) {
		if(IS_IFF_CONTAINER(hdr.id)) {
			type = img_read_uint32_be(io);
			if(type == IFF_ILBM || type == IFF_PBM ) {
				io->seek(pos, SEEK_SET, io->uptr);
				return 0;
			}
			hdr.size -= sizeof type;	/* so we will seek fwd correctly */
		}
		io->seek(hdr.size, SEEK_CUR, io->uptr);
	}

	io->seek(pos, SEEK_SET, io->uptr);
	return -1;
}

static int read_file(struct img_pixmap *img, struct img_io *io)
{
	uint32_t type;
	struct chdr hdr;

	while(read_header(io, &hdr) != -1) {
		if(IS_IFF_CONTAINER(hdr.id)) {
			type = img_read_uint32_be(io);
			hdr.size -= sizeof type;	/* to compensate for having advanced 4 more bytes */

			if(type == IFF_ILBM) {
				if(read_ilbm_pbm(io, type, hdr.size, img) == -1) {
					return -1;
				}
				return 0;
			}
			if(type == IFF_PBM) {
				if(read_ilbm_pbm(io, type, hdr.size, img) == -1) {
					return -1;
				}
				return 0;
			}
		}
		io->seek(hdr.size, SEEK_CUR, io->uptr);
	}
	return 0;
}

static int write_file(struct img_pixmap *img, struct img_io *io)
{
	return -1;	/* TODO */
}

static int read_header(struct img_io *io, struct chdr *hdr)
{
	if(io->read(hdr, sizeof *hdr, io->uptr) < sizeof *hdr) {
		return -1;
	}
#ifdef IMAGO_LITTLE_ENDIAN
	hdr->id = img_swap32(hdr->id);
	hdr->size = img_swap32(hdr->size);
#endif
	return 0;
}

static int read_ilbm_pbm(struct img_io *io, uint32_t type, uint32_t size, struct img_pixmap *img)
{
	int res = -1;
	struct chdr hdr;
	struct bitmap_header bmhd;
	struct crng crng;
	/*struct colrange *crnode;*/
	unsigned char pal[3 * 256];
	long start = io->seek(0, SEEK_CUR, io->uptr);

	memset(img, 0, sizeof *img);

	while(read_header(io, &hdr) != -1 && io->seek(0, SEEK_CUR, io->uptr) - start < size) {
		switch(hdr.id) {
		case IFF_BMHD:
			assert(hdr.size == 20);
			if(read_bmhd(io, &bmhd) == -1) {
				return -1;
			}
			img->width = bmhd.width;
			img->height = bmhd.height;
			if(bmhd.nplanes > 8) {
				/* TODO */
				fprintf(stderr, "libimago: %d planes found, only paletized LBM files supported\n", bmhd.nplanes);
				return -1;
			}
			if(img_set_pixels(img, img->width, img->height, IMG_FMT_RGB24, 0)) {
				return -1;
			}
			break;

		case IFF_CMAP:
			assert(hdr.size / 3 <= 256);

			if(io->read(pal, hdr.size, io->uptr) < hdr.size) {
				return -1;
			}
			break;

		case IFF_CRNG:
			assert(hdr.size == sizeof crng);

			if(read_crng(io, &crng) == -1) {
				return -1;
			}
			if(crng.low != crng.high && crng.rate > 0) {
				/* XXX color cycling not currently supported
				if(!(crnode = malloc(sizeof *crnode))) {
					return -1;
				}
				crnode->low = crng.low;
				crnode->high = crng.high;
				crnode->cmode = (crng.flags & CRNG_REVERSE) ? CYCLE_REVERSE : CYCLE_NORMAL;
				crnode->rate = crng.rate;
				crnode->next = img->range;
				img->range = crnode;
				++img->num_ranges;
				*/
			}
			break;

		case IFF_BODY:
			if(!img->pixels) {
				fprintf(stderr, "libimago: malformed LBM image: encountered BODY chunk before BMHD\n");
				return -1;
			}
			if(type == IFF_ILBM) {
				if(read_body_ilbm(io, &bmhd, img) == -1) {
					return -1;
				}
			} else {
				assert(type == IFF_PBM);
				if(read_body_pbm(io, &bmhd, img) == -1) {
					return -1;
				}
			}

			convert_rgb(img, pal);

			res = 0;	/* sucessfully read image */
			break;

		default:
			/* skip unknown chunks */
			io->seek(hdr.size, SEEK_CUR, io->uptr);
			if(io->seek(0, SEEK_CUR, io->uptr) & 1) {
				/* chunks must start at even offsets */
				io->seek(1, SEEK_CUR, io->uptr);
			}
		}
	}

	return res;
}

static void convert_rgb(struct img_pixmap *img, unsigned char *pal)
{
	int i, npixels = img->width * img->height;
	unsigned char *sptr, *dptr = img->pixels;

	dptr = (unsigned char*)img->pixels + npixels * 3;
	sptr = (unsigned char*)img->pixels + npixels;

	for(i=0; i<npixels; i++) {
		int c = *--sptr;
		*--dptr = pal[c * 3 + 2];
		*--dptr = pal[c * 3 + 1];
		*--dptr = pal[c * 3];
	}
}


static int read_bmhd(struct img_io *io, struct bitmap_header *bmhd)
{
	if(io->read(bmhd, sizeof *bmhd, io->uptr) < 1) {
		return -1;
	}
#ifdef IMAGO_LITTLE_ENDIAN
	bmhd->width = img_swap16(bmhd->width);
	bmhd->height = img_swap16(bmhd->height);
	bmhd->xoffs = img_swap16(bmhd->xoffs);
	bmhd->yoffs = img_swap16(bmhd->yoffs);
	bmhd->colorkey = img_swap16(bmhd->colorkey);
	bmhd->pgwidth = img_swap16(bmhd->pgwidth);
	bmhd->pgheight = img_swap16(bmhd->pgheight);
#endif
	return 0;
}

static int read_crng(struct img_io *io, struct crng *crng)
{
	if(io->read(crng, sizeof *crng, io->uptr) < 1) {
		return -1;
	}
#ifdef IMAGO_LITTLE_ENDIAN
	crng->rate = img_swap16(crng->rate);
	crng->flags = img_swap16(crng->flags);
#endif
	return 0;
}

/* scanline: [bp0 row][bp1 row]...[bpN-1 row][opt mask row]
 * each uncompressed row is width / 8 bytes
 */
static int read_body_ilbm(struct img_io *io, struct bitmap_header *bmhd, struct img_pixmap *img)
{
	int i, j, k, bitidx;
	int rowsz = img->width / 8;
	unsigned char *src, *dest = img->pixels;
	unsigned char *rowbuf = alloca(rowsz);

	assert(bmhd->width == img->width);
	assert(bmhd->height == img->height);
	assert(img->pixels);

	for(i=0; i<img->height; i++) {

		memset(dest, 0, img->width);	/* clear the whole scanline to OR bits into place */

		for(j=0; j<bmhd->nplanes; j++) {
			/* read a row corresponding to bitplane j */
			if(bmhd->compression) {
				if(read_compressed_scanline(io, rowbuf, rowsz) == -1) {
					return -1;
				}
			} else {
				if(io->read(rowbuf, rowsz, io->uptr) < rowsz) {
					return -1;
				}
			}

			/* distribute all bits across the linear output scanline */
			src = rowbuf;
			bitidx = 0;

			for(k=0; k<img->width; k++) {
				dest[k] |= ((*src >> (7 - bitidx)) & 1) << j;

				if(++bitidx >= 8) {
					bitidx = 0;
					++src;
				}
			}
		}

		if(bmhd->masking & MASK_PLANE) {
			/* skip the mask (1bpp) */
			io->seek(rowsz, SEEK_CUR, io->uptr);
		}

		dest += img->width;
	}
	return 0;
}

static int read_body_pbm(struct img_io *io, struct bitmap_header *bmhd, struct img_pixmap *img)
{
	int i;
	int npixels = img->width * img->height;
	unsigned char *dptr = img->pixels;

	assert(bmhd->width == img->width);
	assert(bmhd->height == img->height);
	assert(img->pixels);

	if(bmhd->compression) {
		for(i=0; i<img->height; i++) {
			if(read_compressed_scanline(io, dptr, img->width) == -1) {
				return -1;
			}
			dptr += img->width;
		}

	} else {
		/* uncompressed */
		if(io->read(img->pixels, npixels, io->uptr) < npixels) {
			return -1;
		}
	}

	return 0;
}

static int read_compressed_scanline(struct img_io *io, unsigned char *scanline, int width)
{
	int i, count, x = 0;
	signed char ctl;

	while(x < width) {
		if(io->read(&ctl, 1, io->uptr) < 1) return -1;

		if(ctl == -128) continue;

		if(ctl >= 0) {
			count = ctl + 1;
			if(io->read(scanline, count, io->uptr) < count) return -1;
			scanline += count;

		} else {
			unsigned char pixel;
			count = 1 - ctl;
			if(io->read(&pixel, 1, io->uptr) < 1) return -1;

			for(i=0; i<count; i++) {
				*scanline++ = pixel;
			}
		}

		x += count;
	}

	return 0;
}

#ifdef IMAGO_LITTLE_ENDIAN
static uint16_t img_swap16(uint16_t x)
{
	return (x << 8) | (x >> 8);
}

static uint32_t img_swap32(uint32_t x)
{
	return (x << 24) | ((x & 0xff00) << 8) | ((x & 0xff0000) >> 8) | (x >> 24);
}
#endif
