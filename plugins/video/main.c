#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>
#include "xlivebg.h"

static int init(void *cls);
static void start(long tmsec, void *cls);
static void stop(void *cls);
static void prop(const char *prop, void *cls);
static void draw(long tmsec, void *cls);
static void draw_static(long tmsec, float aspect);

#define PROPLIST	\
	"proplist {\n" \
	"    prop {\n" \
	"        id = \"video\"\n" \
	"        desc = \"video file to play\"\n" \
	"        type = \"filename\"\n" \
	"    }\n" \
	"}\n"

static struct xlivebg_plugin plugin = {
	"video",
	"Video playback",
	PROPLIST,
	XLIVEBG_25FPS,
	init, 0,
	start, stop,
	draw,
	prop,
	0, 0
};

static const char *vidfile;
static unsigned int static_tex;
#define STATIC_SZ	128

int register_plugin(void)
{
	return xlivebg_register_plugin(&plugin);
}

static int init(void *cls)
{
	xlivebg_defcfg_str("xlivebg.video.video", "");
	return 0;
}

static void start(long tmsec, void *cls)
{
	prop("video", 0);

	glGenTextures(1, &static_tex);
	glBindTexture(GL_TEXTURE_2D, static_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, STATIC_SZ, STATIC_SZ, 0,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);
}

static void stop(void *cls)
{
	glDeleteTextures(1, &static_tex);
}

static void prop(const char *prop, void *cls)
{
	if(strcmp(prop, "video") == 0) {
		vidfile = xlivebg_getcfg_str("xlivebg.video.video", 0);
	}
}

static void draw(long tmsec, void *cls)
{
	int i, num_scr;
	struct xlivebg_screen *scr;

	xlivebg_clear(GL_COLOR_BUFFER_BIT);

	num_scr = xlivebg_screen_count();
	for(i=0; i<num_scr; i++) {
		xlivebg_gl_viewport(i);
		scr = xlivebg_screen(i);

		/* TODO */
		draw_static(tmsec, scr->aspect);
	}
}

#define STATIC_SCALE	6
static void draw_static(long tmsec, float aspect)
{
	int i, j;
	unsigned char stbuf[STATIC_SZ * STATIC_SZ];
	unsigned char *ptr = stbuf;

	for(i=0; i<STATIC_SZ; i++) {
		int blank = i & 2;
		for(j=0; j<STATIC_SZ; j++) {
			unsigned char r = rand();
			*ptr++ = blank ? r >> 2 : r;
		}
	}

	glBindTexture(GL_TEXTURE_2D, static_tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, STATIC_SZ, STATIC_SZ, GL_LUMINANCE,
			GL_UNSIGNED_BYTE, stbuf);

	glEnable(GL_TEXTURE_2D);

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(-1, -1);
	glTexCoord2f(STATIC_SCALE * aspect, 0);
	glVertex2f(1, -1);
	glTexCoord2f(STATIC_SCALE * aspect, STATIC_SCALE);
	glVertex2f(1, 1);
	glTexCoord2f(0, STATIC_SCALE);
	glVertex2f(-1, 1);
	glEnd();

	glDisable(GL_TEXTURE_2D);
}
