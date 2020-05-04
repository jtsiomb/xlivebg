/*
colcycle - color cycling image viewer
Copyright (C) 2016  John Tsiombikas <nuclear@member.fsf.org>

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
#include <string.h>
#include <math.h>
#include <errno.h>
#ifdef __WATCOMC__
#include "inttypes.h"
#include <direct.h>
#else
#include <inttypes.h>
#include <dirent.h>
#endif
#include <sys/stat.h>
#include "app.h"
#include "image.h"
#include "xlivebg.h"

/* define this if the assembly 32->64bit multiplication in cycle_offset doesn't
 * work for you for some reason, and you wish to compile the floating point
 * version of the time calculation.
 */
#undef NO_ASM

#if defined(NO_ASM) || (!defined(__i386__) && !defined(__x86_64__) && !defined(__X86__))
#define USE_FLOAT
#endif


#ifndef M_PI
#define M_PI 3.141593
#endif

struct ss_node {
	char *path;
	struct image *img;
	struct ss_node *next;
};

static void set_image_palette(struct image *img);
static void show_image(struct image *img, long time_msec);
static int load_slideshow(const char *path);
static int load_slide(void);

int fbwidth, fbheight;
unsigned char *fbpixels;

static struct image *img;
static int blend = 1;
static int fade_dir;
static unsigned long fade_start, fade_dur = 600;
static int change_pending;
static unsigned long showing_since, show_time = 15000;

static struct ss_node *sslist;

int colc_init(int argc, char **argv)
{
	if(argv[1]) {
		struct stat st;

		if(stat(argv[1], &st) == -1) {
			fprintf(stderr, "failed to stat path: %s\n", argv[1]);
			return -1;
		}

		if(S_ISDIR(st.st_mode)) {
			if(load_slideshow(argv[1]) == -1) {
				fprintf(stderr, "failed to load slideshow in dir: %s\n", argv[1]);
				return -1;
			}
			if(load_slide() == -1) {
				return -1;
			}

		} else {
			if(!(img = malloc(sizeof *img))) {
				perror("failed to allocate image structure");
				return -1;
			}
			if(colc_load_image(img, argv[1]) == -1) {
				fprintf(stderr, "failed to load image: %s\n", argv[1]);
				return -1;
			}
		}
	} else {
		if(!(img = malloc(sizeof *img))) {
			perror("failed to allocate image structure");
			return -1;
		}
		if(colc_gen_test_image(img) == -1) {
			fprintf(stderr, "failed to generate test image\n");
			return -1;
		}
	}

	set_image_palette(img);
	show_image(img, 0);
	return 0;
}

void colc_cleanup(void)
{
	if(sslist) {
		struct ss_node *start = sslist;
		sslist = sslist->next;
		start->next = 0;	/* break the circle */

		while(sslist) {
			struct ss_node *node = sslist;
			sslist = sslist->next;
			colc_destroy_image(node->img);
			free(node->path);
			free(node);
		}

	} else {
		colc_destroy_image(img);
		free(img);
	}
}

/* tx is fixed point with 10 bits decimal */
static void palfade(int dir, long tx)
{
	int i;

	if(!img) return;

	if(dir == -1) {
		tx = 1024 - tx;
	}

	for(i=0; i<256; i++) {
		int r = (img->palette[i].r * tx) >> 10;
		int g = (img->palette[i].g * tx) >> 10;
		int b = (img->palette[i].b * tx) >> 10;
		set_palette(i, r, g, b);
	}
}


/* modes:
 *  1. normal
 *  2. reverse
 *  3. ping-pong
 *  4. sine -> [0, rsize/2]
 *  5. sine -> [0, rsize]
 *
 * returns: offset in 24.8 fixed point
 */
#ifdef USE_FLOAT
static int32_t cycle_offset(enum cycle_mode mode, int32_t rate, int32_t rsize, int32_t msec)
{
	float offs;
	float tm = (rate / 280.0) * (float)msec / 1000.0;

	switch(mode) {
	case CYCLE_PINGPONG:
		offs = fmod(tm, (float)(rsize * 2));
		if(offs >= rsize) offs = (rsize * 2) - offs;
		break;

	case CYCLE_SINE:
	case CYCLE_SINE_HALF:
		{
			float x = fmod(tm, (float)rsize);
			offs = sin((x * M_PI * 2.0) / (float)rsize) + 1.0;
			offs *= rsize / (mode == CYCLE_SINE_HALF ? 4.0f : 2.0f);
		}
		break;

	default:	/* normal or reverse */
		offs = tm;
	}
	return (int32_t)(offs * 256.0);
}

#else	/* fixed point variant for specific platforms (currently x86) */

#ifdef __GNUC__
#define CALC_TIME(res, anim_rate, time_msec) \
	asm volatile ( \
		"\n\tmull %1"	/* edx:eax <- eax(rate << 8) * msec */ \
		"\n\tmovl $280000, %%ebx" \
		"\n\tdivl %%ebx"	/* eax <- edx:eax / ebx */ \
		: "=a" (res) \
		: "g" ((uint32_t)(time_msec)), "a" ((anim_rate) << 8) \
		: "ebx", "edx" )
#endif	/* __GNUC__ */

#ifdef __WATCOMC__
#define CALC_TIME(res, anim_rate, time_msec) \
	(res) = calc_time_offset(anim_rate, time_msec)

int32_t calc_time_offset(int32_t anim_rate, int32_t time_msec);
#pragma aux calc_time_offset = \
		"shl eax, 8" /* eax(rate) <<= 8 convert to 24.8 fixed point */ \
		"mul ebx"	/* edx:eax <- eax(rate<<8) * ebx(msec) */ \
		"mov ebx, 280000" \
		"div ebx"	/* eax <- edx:eax / ebx */ \
		modify [eax ebx ecx] \
		value [eax] \
		parm [eax] [ebx];
#endif	/* __WATCOMC__ */

static int32_t cycle_offset(enum cycle_mode mode, int32_t rate, int32_t rsize, int32_t msec)
{
	int32_t offs, tm;

	CALC_TIME(tm, rate, msec);

	switch(mode) {
	case CYCLE_PINGPONG:
		rsize <<= 8;	/* rsize -> 24.8 fixed point */
		offs = tm % (rsize * 2);
		if(offs > rsize) offs = (rsize * 2) - offs;
		rsize >>= 8;	/* back to 32.0 */
		break;

	case CYCLE_SINE:
	case CYCLE_SINE_HALF:
		{
			float t = (float)tm / 256.0;	/* convert fixed24.8 -> float */
			float x = fmod(t, (float)(rsize * 2));
			float foffs = sin((x * M_PI * 2.0) / (float)rsize) + 1.0;
			if(mode == CYCLE_SINE_HALF) {
				foffs *= rsize / 4.0;
			} else {
				foffs *= rsize / 2.0;
			}
			offs = (int32_t)(foffs * 256.0);	/* convert float -> fixed24.8 */
		}
		break;

	default:	/* normal or reverse */
		offs = tm;
	}

	return offs;
}
#endif	/* USE_FLOAT */

#define LERP_FIXED_T(a, b, xt) ((((a) << 8) + ((b) - (a)) * (xt)) >> 8)

void colc_draw(long time_msec)
{
	int i, j;

	if(!img) return;

	if(sslist) {
		/* if there is a slideshow list of images, handle switching with fades between them */
		if(!fade_dir && (change_pending || time_msec - showing_since > show_time)) {
			fade_dir = -1;
			fade_start = time_msec;
			change_pending = 0;
		}

		if(fade_dir) {
			unsigned long dt = time_msec - fade_start;

			if(dt >= fade_dur) {
				if(fade_dir == -1) {
					sslist = sslist->next;
					if(load_slide() == -1) {
						return;
					}
					show_image(img, time_msec);
					fade_dir = 1;
					time_msec = fade_start = time_msec;
					dt = 0;
				} else {
					set_image_palette(img);
					fade_dir = 0;
				}
			}

			if(fade_dir) {
				long tx = ((long)dt << 10) / fade_dur;
				palfade(fade_dir, tx);
			}
			showing_since = time_msec;
			return;
		}
	}

	/* for each cycling range in the image ... */
	for(i=0; i<img->num_ranges; i++) {
		int32_t offs, rsize, ioffs;
		int rev;

		if(!img->range[i].rate) continue;
		rsize = img->range[i].high - img->range[i].low + 1;

		offs = cycle_offset(img->range[i].cmode, img->range[i].rate, rsize, time_msec);

		ioffs = (offs >> 8) % rsize;

		/* reverse when rev is 2 */
		rev = img->range[i].cmode == CYCLE_REVERSE ? 1 : 0;

		for(j=0; j<rsize; j++) {
			int pidx, to, next;

			pidx = j + img->range[i].low;

			if(rev) {
				to = (j + ioffs) % rsize;
				next = (to + 1) % rsize;
			} else {
				if((to = (j - ioffs) % rsize) < 0) {
					to += rsize;
				}
				if((next = to - 1) < 0) {
					next += rsize;
				}
			}
			to += img->range[i].low;

			if(blend) {
				int r, g, b;
				int32_t frac_offs = offs & 0xff;

				next += img->range[i].low;

				r = LERP_FIXED_T(img->palette[to].r, img->palette[next].r, frac_offs);
				g = LERP_FIXED_T(img->palette[to].g, img->palette[next].g, frac_offs);
				b = LERP_FIXED_T(img->palette[to].b, img->palette[next].b, frac_offs);

				set_palette(pidx, r, g, b);
			} else {
				set_palette(pidx, img->palette[to].r, img->palette[to].g, img->palette[to].b);
			}
		}
	}
}


static void set_image_palette(struct image *img)
{
	int i;
	for(i=0; i<256; i++) {
		set_palette(i, img->palette[i].r, img->palette[i].g, img->palette[i].b);
	}
}

struct xlivebg_plugin colc_plugin;

static void show_image(struct image *img, long time_msec)
{
	int i, j;
	unsigned char *dptr;
	unsigned int max_rate = 0;

	resize(img->width, img->height);

	dptr = fbpixels;
	for(i=0; i<fbheight; i++) {
		for(j=0; j<fbwidth; j++) {
			unsigned char c = 0;
			if(i < img->height && j < img->width) {
				c = img->pixels[i * img->width + j];
			}
			*dptr++ = c;
		}
	}

	showing_since = time_msec;

	for(i=0; i<img->num_ranges; i++) {
		if(img->range[i].rate > max_rate) {
			max_rate = img->range[i].rate;
		}
	}
	colc_plugin.upd_interval = max_rate * 10;
}

static int load_slideshow(const char *path)
{
	DIR *dir;
	struct dirent *dent;
	struct ss_node *head = 0, *tail = 0, *node;

	if(!(dir = opendir(path))) {
		fprintf(stderr, "failed to open directory: %s: %s\n", path, strerror(errno));
		return -1;
	}

	while((dent = readdir(dir))) {
		int sz;

		if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0) {
			continue;
		}
		sz = strlen(path) + strlen(dent->d_name) + 1;	/* +1 for a slash */

		if(!(node = malloc(sizeof *node))) {
			perror("failed to allocate slideshow node");
			goto err;
		}
		if(!(node->path = malloc(sz + 1))) {
			perror("failed to allocate image path");
			free(node);
			goto err;
		}
		sprintf(node->path, "%s/%s", path, dent->d_name);
		node->img = 0;
		node->next = 0;

		if(head) {
			tail->next = node;
			tail = node;
		} else {
			head = tail = node;
		}
	}
	closedir(dir);

	sslist = head;
	tail->next = head;	/* make circular */
	return 0;

err:
	closedir(dir);
	while(head) {
		node = head;
		head = head->next;
		free(node->path);
		free(node);
	}
	return -1;
}

static int load_slide(void)
{
	struct ss_node *start = sslist;

	img = 0;
	do {
		if(sslist->path) {
			if(!sslist->img) {
				if(!(sslist->img = malloc(sizeof *sslist->img))) {
					perror("failed to allocate image structure");
					return -1;
				}
				if(colc_load_image(sslist->img, sslist->path) == -1) {
					fprintf(stderr, "failed to load image: %s\n", sslist->path);
					free(sslist->path);
					sslist->path = 0;
					free(sslist->img);
					sslist->img = 0;
				}
			}
			img = sslist->img;
		}

		sslist = sslist->next;
	} while(!img && sslist != start);

	return img ? 0 : -1;
}
