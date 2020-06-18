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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#if defined(__WATCOMC__) || defined(_MSC_VER)
#include <malloc.h>
#else
#include <alloca.h>
#endif
#include "imagelbm.h"

#ifdef __GNUC__
#define PACKED	__attribute__((packed))
#endif

#ifdef __BYTE_ORDER__
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define LENDIAN
#else
#define BENDIAN
#endif
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

#ifdef __WATCOMC__
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
#ifdef __WATCOMC__
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

static int read_header(FILE *fp, struct chdr *hdr);
static int read_ilbm_pbm(FILE *fp, uint32_t type, uint32_t size, struct image *img);
static int read_bmhd(FILE *fp, struct bitmap_header *bmhd);
static int read_crng(FILE *fp, struct crng *crng);
static int read_body_ilbm(FILE *fp, struct bitmap_header *bmhd, struct image *img);
static int read_body_pbm(FILE *fp, struct bitmap_header *bmhd, struct image *img);
static int read_compressed_scanline(FILE *fp, unsigned char *scanline, int width);
static int read16(FILE *fp, uint16_t *res);
static int read32(FILE *fp, uint32_t *res);
static uint16_t swap16(uint16_t x);
static uint32_t swap32(uint32_t x);

int file_is_lbm(FILE *fp)
{
	uint32_t type;
	struct chdr hdr;

	while(read_header(fp, &hdr) != -1) {
		if(IS_IFF_CONTAINER(hdr.id)) {
			if(read32(fp, &type) == -1) {
				break;
			}

			if(type == IFF_ILBM || type == IFF_PBM ) {
				rewind(fp);
				return 1;
			}
			hdr.size -= sizeof type;	/* so we will seek fwd correctly */
		}
		fseek(fp, hdr.size, SEEK_CUR);
	}
	fseek(fp, 0, SEEK_SET);
	return 0;
}

void print_chunkid(uint32_t id)
{
	char str[5] = {0};
#ifdef LENDIAN
	id = swap32(id);
#endif
	memcpy(str, &id, 4);
	puts(str);
}

int load_image_lbm(struct image *img, FILE *fp)
{
	uint32_t type;
	struct chdr hdr;

	while(read_header(fp, &hdr) != -1) {
		if(IS_IFF_CONTAINER(hdr.id)) {
			if(read32(fp, &type) == -1) {
				break;
			}
			hdr.size -= sizeof type;	/* to compensate for having advanced 4 more bytes */

			if(type == IFF_ILBM) {
				if(read_ilbm_pbm(fp, type, hdr.size, img) == -1) {
					return -1;
				}
				return 0;
			}
			if(type == IFF_PBM) {
				if(read_ilbm_pbm(fp, type, hdr.size, img) == -1) {
					return -1;
				}
				return 0;
			}
		}
		fseek(fp, hdr.size, SEEK_CUR);
	}
	return 0;
}

static int read_header(FILE *fp, struct chdr *hdr)
{
	if(fread(hdr, 1, sizeof *hdr, fp) < sizeof *hdr) {
		return -1;
	}
#ifdef LENDIAN
	hdr->id = swap32(hdr->id);
	hdr->size = swap32(hdr->size);
#endif
	return 0;
}

static int read_ilbm_pbm(FILE *fp, uint32_t type, uint32_t size, struct image *img)
{
	int i, res = -1;
	struct chdr hdr;
	struct bitmap_header bmhd;
	struct crng crng;
	struct colrange *crnode;
	unsigned char pal[3 * 256];
	unsigned char *pptr;
	long start = ftell(fp);

	memset(img, 0, sizeof *img);

	while(read_header(fp, &hdr) != -1 && ftell(fp) - start < size) {
		switch(hdr.id) {
		case IFF_BMHD:
			assert(hdr.size == 20);
			if(read_bmhd(fp, &bmhd) == -1) {
				return -1;
			}
			img->width = bmhd.width;
			img->height = bmhd.height;
			img->bpp = bmhd.nplanes;
			if(bmhd.nplanes > 8) {
				fprintf(stderr, "%d planes found, only paletized LBM files supported\n", bmhd.nplanes);
				return -1;
			}
			if(!(img->pixels = malloc(img->width * img->height))) {
				fprintf(stderr, "failed to allocate %dx%d image\n", img->width, img->height);
				return -1;
			}
			break;

		case IFF_CMAP:
			assert(hdr.size / 3 <= 256);

			if(fread(pal, 1, hdr.size, fp) < hdr.size) {
				fprintf(stderr, "failed to read colormap\n");
				return -1;
			}
			pptr = pal;
			for(i=0; i<256; i++) {
				img->palette[i].r = *pptr++;
				img->palette[i].g = *pptr++;
				img->palette[i].b = *pptr++;
			}
			break;

		case IFF_CRNG:
			assert(hdr.size == sizeof crng);

			if(read_crng(fp, &crng) == -1) {
				fprintf(stderr, "failed to read color cycling range chunk\n");
				return -1;
			}
			if(crng.low != crng.high && crng.rate > 0) {
				if(!(crnode = malloc(sizeof *crnode))) {
					fprintf(stderr, "failed to allocate color range node\n");
					return -1;
				}
				crnode->low = crng.low;
				crnode->high = crng.high;
				crnode->cmode = (crng.flags & CRNG_REVERSE) ? CYCLE_REVERSE : CYCLE_NORMAL;
				crnode->rate = crng.rate;
				crnode->next = img->range;
				img->range = crnode;
				++img->num_ranges;
			}
			break;

		case IFF_BODY:
			if(!img->pixels) {
				fprintf(stderr, "malformed ILBM image: encountered BODY chunk before BMHD\n");
				return -1;
			}
			if(type == IFF_ILBM) {
				if(read_body_ilbm(fp, &bmhd, img) == -1) {
					fprintf(stderr, "failed to read interleaved pixel data\n");
					return -1;
				}
			} else {
				assert(type == IFF_PBM);
				if(read_body_pbm(fp, &bmhd, img) == -1) {
					fprintf(stderr, "failed to read linear pixel data\n");
					return -1;
				}
			}
			res = 0;	/* sucessfully read image */
			break;

		default:
			/* skip unknown chunks */
			fseek(fp, hdr.size, SEEK_CUR);
			if(ftell(fp) & 1) {
				/* chunks must start at even offsets */
				fseek(fp, 1, SEEK_CUR);
			}
		}
	}

	return res;
}


static int read_bmhd(FILE *fp, struct bitmap_header *bmhd)
{
	if(fread(bmhd, sizeof *bmhd, 1, fp) < 1) {
		return -1;
	}
#ifdef LENDIAN
	bmhd->width = swap16(bmhd->width);
	bmhd->height = swap16(bmhd->height);
	bmhd->xoffs = swap16(bmhd->xoffs);
	bmhd->yoffs = swap16(bmhd->yoffs);
	bmhd->colorkey = swap16(bmhd->colorkey);
	bmhd->pgwidth = swap16(bmhd->pgwidth);
	bmhd->pgheight = swap16(bmhd->pgheight);
#endif
	return 0;
}

static int read_crng(FILE *fp, struct crng *crng)
{
	if(fread(crng, sizeof *crng, 1, fp) < 1) {
		return -1;
	}
#ifdef LENDIAN
	crng->rate = swap16(crng->rate);
	crng->flags = swap16(crng->flags);
#endif
	return 0;
}

/* scanline: [bp0 row][bp1 row]...[bpN-1 row][opt mask row]
 * each uncompressed row is width / 8 bytes
 */
static int read_body_ilbm(FILE *fp, struct bitmap_header *bmhd, struct image *img)
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
				if(read_compressed_scanline(fp, rowbuf, rowsz) == -1) {
					return -1;
				}
			} else {
				if(fread(rowbuf, 1, rowsz, fp) < rowsz) {
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
			fseek(fp, rowsz, SEEK_CUR);
		}

		dest += img->width;
	}
	return 0;
}

static int read_body_pbm(FILE *fp, struct bitmap_header *bmhd, struct image *img)
{
	int i;
	int npixels = img->width * img->height;
	unsigned char *dptr = img->pixels;

	assert(bmhd->width == img->width);
	assert(bmhd->height == img->height);
	assert(img->pixels);

	if(bmhd->compression) {
		for(i=0; i<img->height; i++) {
			if(read_compressed_scanline(fp, dptr, img->width) == -1) {
				return -1;
			}
			dptr += img->width;
		}

	} else {
		/* uncompressed */
		if(fread(img->pixels, 1, npixels, fp) < npixels) {
			return -1;
		}
	}
	return 0;
}

static int read_compressed_scanline(FILE *fp, unsigned char *scanline, int width)
{
	int i, count, x = 0;
	signed char ctl;

	while(x < width) {
		if(fread(&ctl, 1, 1, fp) < 1) return -1;

		if(ctl == -128) continue;

		if(ctl >= 0) {
			count = ctl + 1;
			if(fread(scanline, 1, count, fp) < count) return -1;
			scanline += count;

		} else {
			unsigned char pixel;
			count = 1 - ctl;
			if(fread(&pixel, 1, 1, fp) < 1) return -1;

			for(i=0; i<count; i++) {
				*scanline++ = pixel;
			}
		}

		x += count;
	}

	return 0;
}

static int read16(FILE *fp, uint16_t *res)
{
	if(fread(res, sizeof *res, 1, fp) < 1) {
		return -1;
	}
#ifdef LENDIAN
	*res = swap16(*res);
#endif
	return 0;
}

static int read32(FILE *fp, uint32_t *res)
{
	if(fread(res, sizeof *res, 1, fp) < 1) {
		return -1;
	}
#ifdef LENDIAN
	*res = swap32(*res);
#endif
	return 0;
}

static uint16_t swap16(uint16_t x)
{
	return (x << 8) | (x >> 8);
}

static uint32_t swap32(uint32_t x)
{
	return (x << 24) | ((x & 0xff00) << 8) | ((x & 0xff0000) >> 8) | (x >> 24);
}
