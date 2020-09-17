#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "xlivebg.h"
#include "data.h"

#define DEF_STAR_COUNT	3000
#define DEF_STAR_SPEED	5.0f
#define DEF_STAR_SIZE	1.6f
#define DEF_FOLLOW		0.25f
#define DEF_FOLLOW_SPEED	0.6f
static float def_star_color[] = {1, 1, 1, 1};

#define MAX_STAR_COUNT	65536
#define STAR_DEPTH	80

struct vec3 {
	float x, y, z;
};

struct star {
	struct vec3 pos;
	float lenxy;
};

struct vertex {
	float u, v;
	unsigned char r, g, b, a;
	float x, y, z;
};

static int init(void *cls);
static void start(long tmsec, void *cls);
static void prop(const char *name, void *cls);
static void draw(long tmsec, void *cls);
static void draw_stars(long tmsec);
static void perspective(float *m, float vfov, float aspect, float znear, float zfar);

static struct star *star;
static struct xlivebg_image pimg, bolt;
static float targ[2], cam[2];

static struct vertex *varr;
static unsigned short *iarr;

#define PROPLIST	\
	"proplist {\n" \
	"    prop {\n" \
	"        id = \"count\"\n" \
	"        desc = \"number of stars\"\n" \
	"        type = \"integer\"\n" \
	"        range = [500, 5000]\n" \
	"    }\n" \
	"    prop {\n" \
	"        id = \"speed\"\n" \
	"        desc = \"travel speed\"\n" \
	"        type = \"number\"\n" \
	"        range = [1, 60]\n" \
	"    }\n" \
	"    prop {\n" \
	"        id = \"size\"\n" \
	"        desc = \"star size\"\n" \
	"        type = \"number\"\n" \
	"        range = [0.25, 4.0]\n" \
	"    }\n" \
	"    prop {\n" \
	"        id = \"color\"\n" \
	"        desc = \"star color\"\n" \
	"        type = \"color\"\n" \
	"    }\n" \
	"    prop {\n" \
	"        id = \"follow\"\n" \
	"        desc = \"follow mouse pointer\"\n" \
	"        type = \"number\"\n" \
	"        range = [0, 1]\n" \
	"    }\n" \
	"    prop {\n" \
	"        id = \"follow_speed\"\n" \
	"        desc = \"speed at which the view changes to follow the mouse\"\n" \
	"        type = \"number\"\n" \
	"        range = [0.25, 5]\n" \
	"    }\n" \
	"}\n"


static struct xlivebg_plugin plugin = {
	"stars",
	"Starfield effect",
	PROPLIST,
	XLIVEBG_20FPS,
	init, 0,
	start, 0,
	draw,
	prop,
	0, 0
};

static long prev_upd;
static int star_count;
static float star_speed, star_size;
static float follow;
static float follow_speed;
static float star_col[3];

void register_plugin(void)
{
	xlivebg_register_plugin(&plugin);
}

static int init(void *cls)
{
	if(xlivebg_memory_image(&pimg, pimg_data, PIMG_DATA_SIZE) == -1) {
		return -1;
	}
	if(xlivebg_memory_image(&bolt, bolt_data, BOLT_DATA_SIZE) == -1) {
		return -1;
	}

	xlivebg_defcfg_int("xlivebg.stars.count", DEF_STAR_COUNT);
	xlivebg_defcfg_num("xlivebg.stars.speed", DEF_STAR_SPEED);
	xlivebg_defcfg_num("xlivebg.stars.size", DEF_STAR_SIZE);
	xlivebg_defcfg_vec("xlivebg.stars.color", def_star_color);
	xlivebg_defcfg_num("xlivebg.stars.follow", DEF_FOLLOW);
	xlivebg_defcfg_num("xlivebg.stars.follow_speed", DEF_FOLLOW_SPEED);
	return 0;
}

static void start(long tmsec, void *cls)
{
	prop("count", 0);
	prop("speed", 0);
	prop("size", 0);
	prop("color", 0);
	prop("follow", 0);
	prop("follow_speed", 0);

	prev_upd = tmsec;
}

static void prop(const char *name, void *cls)
{
	int i;
	float width;
	float *vec;

	if(strcmp(name, "count") == 0) {
		star_count = xlivebg_getcfg_int("xlivebg.stars.count", DEF_STAR_COUNT);
		if(star_count > MAX_STAR_COUNT) {
			star_count = MAX_STAR_COUNT;
		}

		free(star);
		if((star = malloc(star_count * sizeof *star))) {
			width = STAR_DEPTH / 3.0f;
			for(i=0; i<star_count; i++) {
				star[i].pos.x = width * (2.0f * (float)rand() / (float)RAND_MAX - 1.0f);
				star[i].pos.y = width * (2.0f * (float)rand() / (float)RAND_MAX - 1.0f);
				star[i].pos.z = STAR_DEPTH * (float)rand() / (float)RAND_MAX;

				star[i].lenxy = sqrt(star[i].pos.x * star[i].pos.x + star[i].pos.y * star[i].pos.y);
			}
		}
		varr = malloc(star_count * 4 * sizeof *varr);
		if((iarr = malloc(star_count * 6 * sizeof *iarr))) {
			unsigned short *iptr = iarr;
			for(i=0; i<star_count; i++) {
				int voffs = i * 4;
				*iptr++ = voffs;
				*iptr++ = voffs + 1;
				*iptr++ = voffs + 2;
				*iptr++ = voffs;
				*iptr++ = voffs + 2;
				*iptr++ = voffs + 3;
			}
		}


	} else if(strcmp(name, "speed") == 0) {
		star_speed = xlivebg_getcfg_num("xlivebg.stars.speed", DEF_STAR_SPEED);

	} else if(strcmp(name, "size") == 0) {
		star_size = xlivebg_getcfg_num("xlivebg.stars.size", DEF_STAR_SIZE);

	} else if(strcmp(name, "color") == 0) {
		vec = xlivebg_getcfg_vec("xlivebg.stars.color", def_star_color);
		memcpy(star_col, vec, sizeof star_col);

	} else if(strcmp(name, "follow") == 0) {
		follow = xlivebg_getcfg_num("xlivebg.stars.follow", DEF_FOLLOW);

	} else if(strcmp(name, "follow_speed") == 0) {
		follow_speed = xlivebg_getcfg_num("xlivebg.stars.follow_speed", DEF_FOLLOW_SPEED);
	}
}

static void draw(long tmsec, void *cls)
{
	int i, num_scr, mx, my;
	struct xlivebg_screen *scr;
	float proj[16];
	float aspect;
	long dtms;

	dtms = tmsec - prev_upd;
	prev_upd = tmsec;

	if(follow > 0.0f) {
		float t = follow_speed * (dtms / 1000.0f);

		scr = xlivebg_screen(0);
		xlivebg_mouse_pos(&mx, &my);
		aspect = (float)scr->root_width / (float)scr->root_height;
		targ[0] = ((float)mx / (float)scr->root_width - 0.5f) * follow * aspect;
		targ[1] = (0.5f - (float)my / (float)scr->root_height) * follow;

		cam[0] += (targ[0] - cam[0]) * t;
		cam[1] += (targ[1] - cam[1]) * t;
	}

	glClear(GL_COLOR_BUFFER_BIT);

	num_scr = xlivebg_screen_count();
	for(i=0; i<num_scr; i++) {
		scr = xlivebg_screen(i);
		xlivebg_gl_viewport(i);

		glMatrixMode(GL_PROJECTION);
		perspective(proj, 50.0 / 180.0 * M_PI, scr->aspect, 0.1, STAR_DEPTH * 1.2);
		glLoadMatrixf(proj);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		if(follow > 0.0f) {
			gluLookAt(0, 0, 0, cam[0], cam[1], -1, 0, 1, 0);
		}

		draw_stars(tmsec);
	}
}

static unsigned char cur_col[4];
#define COLOR(r, g, b, a)	\
	do { \
		cur_col[0] = (r) * 255.0f; \
		cur_col[1] = (g) * 255.0f; \
		cur_col[2] = (b) * 255.0f; \
		cur_col[3] = (a) * 255.0f; \
	} while(0)
#define TEXCOORD(tu, tv) (vptr->u = tu, vptr->v = tv)
#define VERTEX(vx, vy, vz) \
	do { \
		vptr->x = vx; \
		vptr->y = vy; \
		vptr->z = vz; \
		vptr->r = cur_col[0]; \
		vptr->g = cur_col[1]; \
		vptr->b = cur_col[2]; \
		vptr->a = cur_col[3]; \
		vptr++; \
	} while(0)

static void draw_stars(long tmsec)
{
	int i;
	float z, t, x, y, x0, y0, x1, y1, theta, sz, ssize;
	struct vec3 pos;
	double tsec = (double)tmsec / 1000.0;
	struct vertex *vptr;

	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, xlivebg_image_texture(&bolt));

	ssize = star_size * 0.035f;

	vptr = varr;
	//glBegin(GL_QUADS);
	for(i=0; i<star_count; i++) {
		pos = star[i].pos;
		z = fmod(pos.z + tsec * star_speed, STAR_DEPTH);
		t = z / STAR_DEPTH;
		pos.z = z - STAR_DEPTH;

		theta = atan2(pos.y / star[i].lenxy, pos.x / star[i].lenxy);

		x = 0;
		y = -ssize;

		x0 = x * cos(theta) - y * sin(theta);
		y0 = x * sin(theta) + y * cos(theta);

		y = ssize;

		x1 = x * cos(theta) - y * sin(theta);
		y1 = x * sin(theta) + y * cos(theta);

		x0 += pos.x;
		x1 += pos.x;
		y0 += pos.y;
		y1 += pos.y;

		COLOR(star_col[0], star_col[1], star_col[2], t);
		TEXCOORD(0, 1);
		VERTEX(x0, y0, pos.z);
		TEXCOORD(1, 1);
		VERTEX(x1, y1, pos.z);
		TEXCOORD(1, 0);
		VERTEX(x1, y1, pos.z - ssize * 16.0);
		TEXCOORD(0, 0);
		VERTEX(x0, y0, pos.z - ssize * 16.0);
	}
	//glEnd();

	glInterleavedArrays(GL_T2F_C4UB_V3F, 0, varr);
	glDrawElements(GL_TRIANGLES, star_count * 6, GL_UNSIGNED_SHORT, iarr);

	vptr = varr;
	glBindTexture(GL_TEXTURE_2D, xlivebg_image_texture(&pimg));
	sz = ssize * 4.0;
	//glBegin(GL_QUADS);
	for(i=0; i<star_count; i++) {
		pos = star[i].pos;
		z = fmod(pos.z + tsec * star_speed, STAR_DEPTH);
		t = z / STAR_DEPTH;
		pos.z = z - STAR_DEPTH;

		COLOR(star_col[0], star_col[1], star_col[2], t);
		TEXCOORD(0, 0);
		VERTEX(pos.x - sz, pos.y - sz, pos.z - ssize);
		TEXCOORD(1, 0);
		VERTEX(pos.x + sz, pos.y - sz, pos.z - ssize);
		TEXCOORD(1, 1);
		VERTEX(pos.x + sz, pos.y + sz, pos.z - ssize);
		TEXCOORD(0, 1);
		VERTEX(pos.x - sz, pos.y + sz, pos.z - ssize);
	}
	//glEnd();
	glDrawElements(GL_TRIANGLES, star_count * 6, GL_UNSIGNED_SHORT, iarr);

	glDisableClientState(GL_VERTEX_ARRAY_POINTER);
	glDisableClientState(GL_COLOR_ARRAY_POINTER);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY_POINTER);

	glPopAttrib();
}

static void perspective(float *m, float vfov, float aspect, float znear, float zfar)
{
	float s = 1.0f / (float)tan(vfov / 2.0f);
	float range = znear - zfar;

	memset(m, 0, 16 * sizeof *m);
	m[0] = s / aspect;
	m[5] = s;
	m[10] = (znear + zfar) / range;
	m[14] = 2.0f * znear * zfar / range;
	m[11] = -1.0f;
}

