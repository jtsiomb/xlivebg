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
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#if defined(__WATCOMC__) || defined(_MSC_VER) || defined(WIN32)
#include <malloc.h>
#else
#include <alloca.h>
#endif
#include "image.h"
#include "imagelbm.h"

#ifndef M_PI
#define M_PI 3.141593
#endif

static int flatten_crange_list(struct image *img);

int gen_test_image(struct image *img)
{
	int i, j;
	unsigned char *pptr;

	img->width = 640;
	img->height = 480;
	img->bpp = 8;

	if(!(img->range = malloc(sizeof *img->range))) {
		return -1;
	}
	img->num_ranges = 1;
	img->range[0].low = 0;
	img->range[0].high = 255;
	img->range[0].cmode = CYCLE_NORMAL;
	img->range[0].rate = 5000;

	if(!(img->pixels = malloc(img->width * img->height))) {
		free(img->range);
		return -1;
	}

	for(i=0; i<256; i++) {
		float theta = M_PI * 2.0 * (float)i / 256.0;
		float r = cos(theta) * 0.5 + 0.5;
		float g = sin(theta) * 0.5 + 0.5;
		float b = -cos(theta) * 0.5 + 0.5;
		img->palette[i].r = (int)(r * 255.0);
		img->palette[i].g = (int)(g * 255.0);
		img->palette[i].b = (int)(b * 255.0);
	}

	pptr = img->pixels;
	for(i=0; i<img->height; i++) {
		int c = (i << 8) / img->height;
		for(j=0; j<img->width; j++) {
			int chess = ((i >> 6) & 1) == ((j >> 6) & 1);
			*pptr++ = (chess ? c : c + 128) & 0xff;
		}
	}
	return 0;
}

#define MAX_TOKEN_SIZE	256
static int image_block(FILE *fp, struct image *img);
static int nextc = -1;
static char token[MAX_TOKEN_SIZE];

int load_image(struct image *img, const char *fname)
{
	FILE *fp;
	int c;

	if(!(fp = fopen(fname, "rb"))) {
		fprintf(stderr, "failed to open file: %s: %s\n", fname, strerror(errno));
		return -1;
	}

	if(file_is_lbm(fp)) {
		if(load_image_lbm(img, fp) == -1) {
			fclose(fp);
			return -1;
		}
		fclose(fp);
		flatten_crange_list(img);
		return 0;
	}

	/* find the start of the root block */
	while((c = fgetc(fp)) != -1 && c != '{');
	if(feof(fp)) {
		fprintf(stderr, "invalid image format, no image block found: %s\n", fname);
		return -1;
	}

	nextc = c;	/* prime the pump with the extra char we read above */
	if(image_block(fp, img) == -1) {
		fprintf(stderr, "failed to read image block from: %s\n", fname);
		fclose(fp);
		return -1;
	}
	fclose(fp);

	flatten_crange_list(img);
	return 0;
}

void destroy_image(struct image *img)
{
	if(img) {
		free(img->pixels);
		free(img->range);
		memset(img, 0, sizeof *img);
	}
}

static int flatten_crange_list(struct image *img)
{
	struct colrange *list = img->range;
	struct colrange *rptr;

	if(img->num_ranges <= 0) {
		return 0;
	}

	if(!(img->range = malloc(img->num_ranges * sizeof *img->range))) {
		perror("flatten_crange_list: failed to allocate range array\n");
		return -1;
	}

	rptr = img->range;
	while(list) {
		struct colrange *rng = list;
		list = list->next;
		*rptr++ = *rng;
		free(rng);
	}
	return 0;
}

/* ---- parser ---- */

enum {
	TOKEN_NUM,
	TOKEN_NAME,
	TOKEN_STR
};

static int next_char(FILE *fp)
{
	while((nextc = fgetc(fp)) != -1 && isspace(nextc));
	return nextc;
}

static int next_token(FILE *fp)
{
	char *ptr;

	if(nextc == -1) {
		return -1;
	}

	switch(nextc) {
	case '{':
	case '}':
	case ',':
	case '[':
	case ']':
	case ':':
		token[0] = nextc;
		token[1] = 0;
		nextc = next_char(fp);
		return token[0];

	case '\'':
		ptr = token;
		nextc = next_char(fp);
		while(nextc != -1 && nextc != '\'') {
			*ptr++ = nextc;
			nextc = fgetc(fp);
		}
		nextc = next_char(fp);
		return TOKEN_STR;

	default:
		break;
	}

	if(isalpha(nextc)) {
		ptr = token;
		while(nextc != -1 && isalpha(nextc)) {
			*ptr++ = nextc;
			nextc = next_char(fp);
		}
		*ptr = 0;
		return TOKEN_NAME;
	}
	if(isdigit(nextc)) {
		ptr = token;
		while(nextc != -1 && isdigit(nextc)) {
			*ptr++ = nextc;
			nextc = next_char(fp);
		}
		*ptr = 0;
		return TOKEN_NUM;
	}

	token[0] = nextc;
	token[1] = 0;
	fprintf(stderr, "next_token: unexpected character: %c\n", nextc);
	return -1;
}

static int expect(FILE *fp, int tok)
{
	if(next_token(fp) != tok) {
		return 0;
	}
	return 1;
}

static const char *toktypestr(int tok)
{
	static char buf[] = "' '";
	switch(tok) {
	case TOKEN_NUM:
		return "number";
	case TOKEN_NAME:
		return "name";
	case TOKEN_STR:
		return "string";
	default:
		break;
	}
	buf[1] = tok;
	return buf;
}

#define EXPECT(fp, x) \
	do { \
		if(!expect(fp, x)) { \
			fprintf(stderr, "%s: expected: %s, found: %s\n", __func__, toktypestr(x), token); \
			return -1; \
		} \
	} while(0)

static int palette(FILE *fp, struct image *img)
{
	int tok, cidx = 0;
	struct color col, *cptr;

	EXPECT(fp, '[');

	while((tok = next_token(fp)) == '[') {
		cptr = cidx < 256 ? &img->palette[cidx] : &col;
		++cidx;

		EXPECT(fp, TOKEN_NUM);
		cptr->r = atoi(token);
		EXPECT(fp, ',');
		EXPECT(fp, TOKEN_NUM);
		cptr->g = atoi(token);
		EXPECT(fp, ',');
		EXPECT(fp, TOKEN_NUM);
		cptr->b = atoi(token);
		EXPECT(fp, ']');

		if(nextc == ',') {
			next_token(fp);	/* skip comma */
		}
	}

	if(tok != ']') {
		fprintf(stderr, "palette must be closed by a ']' token\n");
		return -1;
	}
	return 0;
}

static int crange(FILE *fp, struct colrange *rng)
{
	int val;
	char name[MAX_TOKEN_SIZE];

	EXPECT(fp, '{');

	while(nextc != -1 && nextc != '}') {
		EXPECT(fp, TOKEN_NAME);
		strcpy(name, token);
		EXPECT(fp, ':');
		EXPECT(fp, TOKEN_NUM);
		val = atoi(token);

		if(strcmp(name, "reverse") == 0) {
			rng->cmode = val;
		} else if(strcmp(name, "rate") == 0) {
			rng->rate = val;
		} else if(strcmp(name, "low") == 0) {
			rng->low = val;
		} else if(strcmp(name, "high") == 0) {
			rng->high = val;
		} else {
			fprintf(stderr, "invalid attribute %s in cycles range\n", name);
			return -1;
		}

		if(nextc == ',') {
			next_token(fp);
		}
	}

	EXPECT(fp, '}');
	return 0;
}

static int cycles(FILE *fp, struct image *img)
{
	struct colrange *list = 0, *rng;

	EXPECT(fp, '[');

	img->num_ranges = 0;
	while(nextc == '{') {
		if(!(rng = malloc(sizeof *rng))) {
			perror("failed to allocate color range");
			goto err;
		}
		if(crange(fp, rng) == -1) {
			free(rng);
			goto err;
		}
		if(rng->low != rng->high && rng->rate > 0) {
			rng->next = list;
			list = rng;
			++img->num_ranges;
		} else {
			free(rng);
		}

		if(nextc == ',') {
			next_token(fp);	/* eat the comma */
		}
	}

	img->range = list;

	if(!expect(fp, ']')) {
		fprintf(stderr, "cycles: missing closing bracket\n");
		goto err;
	}
	return 0;

err:
	while(list) {
		rng = list;
		list = list->next;
		free(rng);
	}
	img->num_ranges = 0;
	return -1;
}

static int pixels(FILE *fp, struct image *img)
{
	int tok, num_pixels;
	unsigned char *pptr;

	if(img->width <= 0 || img->height <= 0) {
		fprintf(stderr, "pixel block found before defining the image dimensions!\n");
		return -1;
	}
	num_pixels = img->width * img->height;
	if(!(img->pixels = malloc(num_pixels))) {
		perror("failed to allocate pixels");
		return -1;
	}
	pptr = img->pixels;

	EXPECT(fp, '[');

	while((tok = next_token(fp)) == TOKEN_NUM) {
		if(pptr - img->pixels >= num_pixels) {
			pptr = 0;
			fprintf(stderr, "more pixel data provided than the specified image dimensions\n");
		}
		if(!pptr) continue;
		*pptr++ = atoi(token);

		if(nextc == ',') {
			next_token(fp);	/* eat comma */
		}
	}

	if(tok != ']') {
		printf("pixels: missing closing bracket\n");
		return -1;
	}
	return 0;
}

static int attribute(FILE *fp, struct image *img)
{
	char *attr_name;

	if(!expect(fp, TOKEN_NAME)) {
		return -1;
	}
	attr_name = alloca(strlen(token) + 1);
	strcpy(attr_name, token);

	if(!expect(fp, ':')) {
		return -1;
	}

	if(strcmp(attr_name, "filename") == 0) {
		if(!expect(fp, TOKEN_STR)) {
			fprintf(stderr, "attribute: filename should be a string\n");
			return -1;
		}

	} else if(strcmp(attr_name, "width") == 0 || strcmp(attr_name, "height") == 0) {
		int *dst = attr_name[0] == 'w' ? &img->width : &img->height;
		if(!expect(fp, TOKEN_NUM)) {
			fprintf(stderr, "attribute: %s should be a number\n", attr_name);
			return -1;
		}
		*dst = atoi(token);

	} else if(strcmp(attr_name, "colors") == 0) {
		if(palette(fp, img) == -1) {
			fprintf(stderr, "attribute: colors should be a palette\n");
			return -1;
		}

	} else if(strcmp(attr_name, "cycles") == 0) {
		if(cycles(fp, img) == -1) {
			fprintf(stderr, "attribute: cycles should be a list of palranges\n");
			return -1;
		}

	} else if(strcmp(attr_name, "pixels") == 0) {
		if(pixels(fp, img) == -1) {
			fprintf(stderr, "attribute: pixels should be a list of indices\n");
			return -1;
		}

	} else {
		fprintf(stderr, "unknown attribute: %s\n", attr_name);
		return -1;
	}
	return 0;
}

static int image_block(FILE *fp, struct image *img)
{
	EXPECT(fp, '{');

	img->width = img->height = -1;
	img->bpp = 8;

	while(attribute(fp, img) != -1) {
		if(nextc == ',') {
			next_token(fp);	/* eat comma */
		}
	}
	return 0;
}
