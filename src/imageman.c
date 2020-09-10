/*
xlivebg - live wallpapers for the X window system
Copyright (C) 2019  John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>
#include <imago2.h>
#include "imageman.h"
#include "cfg.h"

static int gen_test_image(struct xlivebg_image *img, int width, int height);
static void update_texture(struct xlivebg_image *img);

static struct xlivebg_image **images;
static int num_images, max_images;

static struct xlivebg_image *bg, *bgmask;


void init_imgman(void)
{
	struct xlivebg_image *img;

	if((img = malloc(sizeof *img))) {
		if(gen_test_image(img, 1920, 1080) == -1) {
			free(img);
		} else {
			add_image(img);
		}
	}

	if(cfg.image) {
		if((img = malloc(sizeof *img))) {
			if(load_image(img, cfg.image) == -1) {
				free(img);
			} else {
				add_image(img);
				bg = img;
			}
		}
	}
	if(cfg.anm_mask) {
		if((img = malloc(sizeof *img))) {
			if(load_image(img, cfg.anm_mask) == -1) {
				free(img);
			} else {
				add_image(img);
				bgmask = img;
			}
		}
	}
}

void destroy_all_textures(void)
{
	int i;
	for(i=0; i<num_images; i++) {
		if(images[i]->tex) {
			glDeleteTextures(1, &images[i]->tex);
			images[i]->tex = 0;
		}
	}
}

int create_image(struct xlivebg_image *img, int width, int height, uint32_t *pix)
{
	memset(img, 0, sizeof *img);

	if(width > 0 && height > 0) {
		if(!(img->pixels = malloc(width * height * sizeof *img->pixels))) {
			return -1;
		}
		img->width = width;
		img->height = height;
	}

	if(pix) {
		memcpy(img->pixels, pix, width * height * sizeof *img->pixels);
	}
	return 0;
}

void destroy_image(struct xlivebg_image *img)
{
	if(img) {
		free(img->pixels);
		img->pixels = 0;
	}
}

int load_image(struct xlivebg_image *img, const char *fname)
{
	memset(img, 0, sizeof *img);
	if(!(img->pixels = img_load_pixels(fname, &img->width, &img->height, IMG_FMT_RGBA32))) {
		fprintf(stderr, "xlivebg: failed to load image: %s\n", fname);
		return -1;
	}
	if((img->path = malloc(strlen(fname) + 1))) {
		strcpy(img->path, fname);
	}
	return 0;
}

int add_image(struct xlivebg_image *img)
{
	if(num_images >= max_images) {
		int nmax = max_images ? max_images * 2 : 16;
		struct xlivebg_image **tmp = realloc(images, nmax * sizeof *images);
		if(!tmp) {
			perror("add_image");
			return -1;
		}
		images = tmp;
		max_images = nmax;
	}
	images[num_images++] = img;
	return 0;
}

struct xlivebg_image *get_image(int idx)
{
	struct xlivebg_image *img;

	if(idx < 0 || idx >= num_images) {
		return 0;
	}

	img = images[idx];
	update_texture(img);
	return img;
}

int get_image_count(void)
{
	return num_images;
}

int find_image(const char *name)
{
	int i;
	for(i=0; i<num_images; i++) {
		if(images[i]->path && strcmp(name, images[i]->path) == 0) {
			return i;
		}
	}
	return -1;
}

int dump_image(struct xlivebg_image *img, const char *fname)
{
	return img_save_pixels(fname, img->pixels, img->width, img->height, IMG_FMT_RGBA32);
}

int dump_image_tex(struct xlivebg_image *img, const char *fname)
{
	int res;
	void *pixels;

	if(!(pixels = malloc(img->width * img->height * 4))) {
		return -1;
	}
	glPushAttrib(GL_TEXTURE_BIT);
	glBindTexture(GL_TEXTURE_2D, img->tex);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glPopAttrib();

	res = img_save_pixels(fname, pixels, img->width, img->height, IMG_FMT_RGBA32);
	free(pixels);
	return res;
}

struct xlivebg_image *get_bg_image(int scr)
{
	struct xlivebg_image *img = bg ? bg : images[0];
	update_texture(img);
	return img;
}

struct xlivebg_image *get_anim_mask(int scr)
{
	update_texture(bgmask);
	return bgmask;
}


void set_bg_image(int scr, struct xlivebg_image *img)
{
	bg = img;
}

void set_anim_mask(int scr, struct xlivebg_image *img)
{
	bgmask = img;
}

static int gen_test_image(struct xlivebg_image *img, int width, int height)
{
	int i, j;
	uint32_t *pptr;

	if(create_image(img, width, height, 0) == -1) {
		return -1;
	}
	pptr = img->pixels;

	for(i=0; i<height; i++) {
		for(j=0; j<width; j++) {
			int xor = i ^ j;

			uint32_t r = (xor << 1) & 0xff;
			uint32_t g = xor & 0xff;
			uint32_t b = (xor >> 1) & 0xff;

			*pptr++ = (r << 16) | (g << 8) | b;
		}
	}

	return 0;
}

static void update_texture(struct xlivebg_image *img)
{
	if(!img) return;

	if(!img->tex) {
		glGenTextures(1, &img->tex);
		glBindTexture(GL_TEXTURE_2D, img->tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img->width, img->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img->pixels);
	}
}
