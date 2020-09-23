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
#include <string.h>
#include "imago2.h"
#include "inttypes.h"

/* pixel-format conversions are sub-optimal at the moment to avoid
 * writing a lot of code. optimize at some point ?
 */

#define CLAMP(x, a, b)	((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))

struct pixel {
	float r, g, b, a;
};

static void unpack_grey8(struct pixel *unp, void *pptr, int count);
static void unpack_rgb24(struct pixel *unp, void *pptr, int count);
static void unpack_rgba32(struct pixel *unp, void *pptr, int count);
static void unpack_bgra32(struct pixel *unp, void *pptr, int count);
static void unpack_greyf(struct pixel *unp, void *pptr, int count);
static void unpack_rgbf(struct pixel *unp, void *pptr, int count);
static void unpack_rgbaf(struct pixel *unp, void *pptr, int count);
static void unpack_rgb565(struct pixel *unp, void *pptr, int count);

static void pack_grey8(void *pptr, struct pixel *unp, int count);
static void pack_rgb24(void *pptr, struct pixel *unp, int count);
static void pack_rgba32(void *pptr, struct pixel *unp, int count);
static void pack_bgra32(void *pptr, struct pixel *unp, int count);
static void pack_greyf(void *pptr, struct pixel *unp, int count);
static void pack_rgbf(void *pptr, struct pixel *unp, int count);
static void pack_rgbaf(void *pptr, struct pixel *unp, int count);
static void pack_rgb565(void *pptr, struct pixel *unp, int count);

/* XXX keep in sync with enum img_fmt at imago2.h */
static void (*unpack[])(struct pixel*, void*, int) = {
	unpack_grey8,
	unpack_rgb24,
	unpack_rgba32,
	unpack_bgra32,
	unpack_greyf,
	unpack_rgbf,
	unpack_rgbaf,
	unpack_rgb565
};

/* XXX keep in sync with enum img_fmt at imago2.h */
static void (*pack[])(void*, struct pixel*, int) = {
	pack_grey8,
	pack_rgb24,
	pack_rgba32,
	pack_bgra32,
	pack_greyf,
	pack_rgbf,
	pack_rgbaf,
	pack_rgb565
};


int img_convert(struct img_pixmap *img, enum img_fmt tofmt)
{
	struct pixel pbuf[8];
	int bufsz = (img->width & 7) == 0 ? 8 : ((img->width & 3) == 0 ? 4 : 1);
	int i, num_pix = img->width * img->height;
	int num_iter = num_pix / bufsz;
	char *sptr, *dptr;
	struct img_pixmap nimg;

	if(img->fmt == tofmt) {
		return 0;	/* nothing to do */
	}

	img_init(&nimg);
	if(img_set_pixels(&nimg, img->width, img->height, tofmt, 0) == -1) {
		img_destroy(&nimg);
		return -1;
	}

	sptr = img->pixels;
	dptr = nimg.pixels;

	for(i=0; i<num_iter; i++) {
		unpack[img->fmt](pbuf, sptr, bufsz);
		pack[tofmt](dptr, pbuf, bufsz);

		sptr += bufsz * img->pixelsz;
		dptr += bufsz * nimg.pixelsz;
	}

	img_copy(img, &nimg);
	img_destroy(&nimg);
	return 0;
}

/* the following functions *could* benefit from SIMD */

static void unpack_grey8(struct pixel *unp, void *pptr, int count)
{
	int i;
	unsigned char *pix = pptr;

	for(i=0; i<count; i++) {
		unp->r = unp->g = unp->b = (float)*pix++ / 255.0;
		unp->a = 1.0;
		unp++;
	}
}

static void unpack_rgb24(struct pixel *unp, void *pptr, int count)
{
	int i;
	unsigned char *pix = pptr;

	for(i=0; i<count; i++) {
		unp->r = (float)*pix++ / 255.0;
		unp->g = (float)*pix++ / 255.0;
		unp->b = (float)*pix++ / 255.0;
		unp->a = 1.0;
		unp++;
	}
}

static void unpack_rgba32(struct pixel *unp, void *pptr, int count)
{
	int i;
	unsigned char *pix = pptr;

	for(i=0; i<count; i++) {
		unp->r = (float)*pix++ / 255.0;
		unp->g = (float)*pix++ / 255.0;
		unp->b = (float)*pix++ / 255.0;
		unp->a = (float)*pix++ / 255.0;
		unp++;
	}
}

static void unpack_bgra32(struct pixel *unp, void *pptr, int count)
{
	int i;
	unsigned char *pix = pptr;

	for(i=0; i<count; i++) {
		unp->a = (float)*pix++ / 255.0;
		unp->r = (float)*pix++ / 255.0;
		unp->g = (float)*pix++ / 255.0;
		unp->b = (float)*pix++ / 255.0;
		unp++;
	}
}

static void unpack_greyf(struct pixel *unp, void *pptr, int count)
{
	int i;
	float *pix = pptr;

	for(i=0; i<count; i++) {
		unp->r = unp->g = unp->b = *pix++;
		unp->a = 1.0;
		unp++;
	}
}

static void unpack_rgbf(struct pixel *unp, void *pptr, int count)
{
	int i;
	float *pix = pptr;

	for(i=0; i<count; i++) {
		unp->r = *pix++;
		unp->g = *pix++;
		unp->b = *pix++;
		unp->a = 1.0;
		unp++;
	}
}

static void unpack_rgbaf(struct pixel *unp, void *pptr, int count)
{
	int i;
	float *pix = pptr;

	for(i=0; i<count; i++) {
		unp->r = *pix++;
		unp->g = *pix++;
		unp->b = *pix++;
		unp->a = *pix++;
		unp++;
	}
}

static void unpack_rgb565(struct pixel *unp, void *pptr, int count)
{
	int i;
	uint16_t *pix = pptr;

	for(i=0; i<count; i++) {
		uint16_t r, g, b, p = *pix++;
		b = (p & 0x1f) << 3;
		if(b & 8) b |= 7;	/* fill LSbits with whatever bit 0 was */
		g = (p >> 2) & 0xfc;
		if(g & 4) g |= 3;	/* ditto */
		r = (p >> 8) & 0xf8;
		if(r & 8) r |= 7;	/* same */

		unp->r = (float)r / 255.0f;
		unp->g = (float)g / 255.0f;
		unp->b = (float)b / 255.0f;
		unp->a = 1.0f;
		unp++;
	}
}


static void pack_grey8(void *pptr, struct pixel *unp, int count)
{
	int i;
	unsigned char *pix = pptr;

	for(i=0; i<count; i++) {
		int lum = (int)(255.0 * (unp->r + unp->g + unp->b) / 3.0);
		*pix++ = CLAMP(lum, 0, 255);
		unp++;
	}
}

static void pack_rgb24(void *pptr, struct pixel *unp, int count)
{
	int i;
	unsigned char *pix = pptr;

	for(i=0; i<count; i++) {
		int r = (int)(unp->r * 255.0);
		int g = (int)(unp->g * 255.0);
		int b = (int)(unp->b * 255.0);

		*pix++ = CLAMP(r, 0, 255);
		*pix++ = CLAMP(g, 0, 255);
		*pix++ = CLAMP(b, 0, 255);
		unp++;
	}
}

static void pack_rgba32(void *pptr, struct pixel *unp, int count)
{
	int i;
	unsigned char *pix = pptr;

	for(i=0; i<count; i++) {
		int r = (int)(unp->r * 255.0);
		int g = (int)(unp->g * 255.0);
		int b = (int)(unp->b * 255.0);
		int a = (int)(unp->a * 255.0);

		*pix++ = CLAMP(r, 0, 255);
		*pix++ = CLAMP(g, 0, 255);
		*pix++ = CLAMP(b, 0, 255);
		*pix++ = CLAMP(a, 0, 255);
		unp++;
	}
}

static void pack_bgra32(void *pptr, struct pixel *unp, int count)
{
	int i;
	unsigned char *pix = pptr;

	for(i=0; i<count; i++) {
		int r = (int)(unp->r * 255.0);
		int g = (int)(unp->g * 255.0);
		int b = (int)(unp->b * 255.0);
		int a = (int)(unp->a * 255.0);

		*pix++ = CLAMP(b, 0, 255);
		*pix++ = CLAMP(g, 0, 255);
		*pix++ = CLAMP(r, 0, 255);
		*pix++ = CLAMP(a, 0, 255);
		unp++;
	}
}

static void pack_greyf(void *pptr, struct pixel *unp, int count)
{
	int i;
	float *pix = pptr;

	for(i=0; i<count; i++) {
		*pix++ = (unp->r + unp->g + unp->b) / 3.0;
		unp++;
	}
}

static void pack_rgbf(void *pptr, struct pixel *unp, int count)
{
	int i;
	float *pix = pptr;

	for(i=0; i<count; i++) {
		*pix++ = unp->r;
		*pix++ = unp->g;
		*pix++ = unp->b;
		unp++;
	}
}

static void pack_rgbaf(void *pptr, struct pixel *unp, int count)
{
	memcpy(pptr, unp, count * sizeof *unp);
}


static void pack_rgb565(void *pptr, struct pixel *unp, int count)
{
	int i;
	uint16_t *pix = pptr;

	for(i=0; i<count; i++) {
		uint16_t r = (uint16_t)(unp->r * 31.0f);
		uint16_t g = (uint16_t)(unp->g * 63.0f);
		uint16_t b = (uint16_t)(unp->b * 31.0f);
		if(r > 31) r = 31;
		if(g > 63) g = 63;
		if(b > 31) b = 31;
		*pix++ = (r << 11) | (g << 5) | b;
		unp++;
	}
}
