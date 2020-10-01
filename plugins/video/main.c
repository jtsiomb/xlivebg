/*
xlivebg - live wallpapers for the X window system
Copyright (C) 2019-2020  John Tsiombikas <nuclear@member.fsf.org>

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
#include <pthread.h>
#include "xlivebg.h"
#include "vid.h"

static int init(void *cls);
static int start(long tmsec, void *cls);
static void stop(void *cls);
static void prop(const char *prop, void *cls);
static void draw(long tmsec, void *cls);
static void draw_static(long tmsec, float aspect);
static unsigned int nextpow2(unsigned int x);
static unsigned char *nextfrm(unsigned char *frm);
static void stop_thread(void);
static void *thread_func(void *arg);

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
	XLIVEBG_15FPS,
	init, 0,
	start, stop,
	draw,
	prop,
	0, 0
};

#define NUM_BUF_FRAMES	16
#define STATIC_SZ	128

static struct video_file *vidfile;
static int vid_width, vid_height, tex_width, tex_height;
static unsigned int vid_tex, static_tex;
static unsigned long interval;
static long prev_tmsec;

static unsigned char *framebuf, *framebuf_end, *inframe, *outframe;
static int frame_size;

static int playing;
static pthread_t thread;
static pthread_mutex_t frm_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t frm_cond = PTHREAD_COND_INITIALIZER;


int register_plugin(void)
{
	return xlivebg_register_plugin(&plugin);
}

static int init(void *cls)
{
	xlivebg_defcfg_str("xlivebg.video.video", "");
	return 0;
}

static int start(long tmsec, void *cls)
{
	playing = 1;

	glGenTextures(1, &static_tex);
	glBindTexture(GL_TEXTURE_2D, static_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, STATIC_SZ, STATIC_SZ, 0,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);

	glGenTextures(1, &vid_tex);
	glBindTexture(GL_TEXTURE_2D, vid_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	prop("video", 0);
	prev_tmsec = tmsec;

	return 0;
}

static void stop(void *cls)
{
	stop_thread();

	if(static_tex) {
		glDeleteTextures(1, &static_tex);
	}
	if(vid_tex) {
		glDeleteTextures(1, &vid_tex);
	}
	tex_width = tex_height = 0;

	if(vidfile) {
		vid_close(vidfile);
		vidfile = 0;
	}
	free(framebuf);
	framebuf = 0;

	plugin.upd_interval = XLIVEBG_15FPS;
}

static void prop(const char *prop, void *cls)
{
	if(strcmp(prop, "video") == 0) {
		int tw, th;
		struct video_file *vf;
		unsigned char *fb;
		const char *fname = xlivebg_getcfg_str("xlivebg.video.video", 0);

		if(!fname || !*fname) {
			stop_thread();
			if(vidfile) {
				vid_close(vidfile);
				vidfile = 0;
			}
			plugin.upd_interval = XLIVEBG_15FPS;
			return;
		}

		if(!(vf = vid_open(fname))) {
			return;
		}

		if(!(fb = malloc(vid_frame_size(vf) * NUM_BUF_FRAMES))) {
			fprintf(stderr, "video: failed to allocate frame buffer %u bytes\n", (unsigned int)vid_frame_size(vf));
			vid_close(vf);
			return;
		}

		stop_thread();

		pthread_mutex_lock(&frm_mutex);
		free(framebuf);
		frame_size = vid_frame_size(vf);
		framebuf = fb;
		framebuf_end = fb + frame_size * NUM_BUF_FRAMES;
		inframe = outframe = framebuf;

		if(vidfile) vid_close(vidfile);
		vidfile = vf;
		pthread_mutex_unlock(&frm_mutex);

		plugin.upd_interval = vid_frame_interval(vidfile);

		vid_width = vid_frame_width(vidfile);
		vid_height = vid_frame_height(vidfile);
		tw = nextpow2(vid_width);
		th = nextpow2(vid_height);

		if(tex_width < tw || tex_height < th) {
			tex_width = tw;
			tex_height = th;

			glBindTexture(GL_TEXTURE_2D, vid_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA,
					GL_UNSIGNED_BYTE, 0);
		}
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glScalef((float)vid_width / tex_width, (float)vid_height / tex_height, 1);

		interval = 0;

		pthread_create(&thread, 0, thread_func, 0);
	}
}

static void update_frame(unsigned long dt)
{
	unsigned char *frame = 0;

	interval += dt * 1000;

	pthread_mutex_lock(&frm_mutex);
	while(interval >= plugin.upd_interval && inframe != outframe) {
		frame = outframe;
		outframe = nextfrm(outframe);

		interval -= plugin.upd_interval;
	}
	if(frame) {
		pthread_cond_signal(&frm_cond);	/* wakeup thread, we removed frames */

		glBindTexture(GL_TEXTURE_2D, vid_tex);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vid_width, vid_height, GL_RGBA,
				GL_UNSIGNED_BYTE, frame);
	}
	pthread_mutex_unlock(&frm_mutex);
}

static void draw(long tmsec, void *cls)
{
	int i, num_scr;
	struct xlivebg_screen *scr;
	float xform[16];
	long dt;

	dt = tmsec - prev_tmsec;
	prev_tmsec = tmsec;

	if(vidfile) {
		update_frame(dt);
	} else {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
	}

	xlivebg_clear(GL_COLOR_BUFFER_BIT);

	num_scr = xlivebg_screen_count();
	for(i=0; i<num_scr; i++) {
		xlivebg_gl_viewport(i);
		scr = xlivebg_screen(i);

		if(vidfile) {
			xlivebg_calc_image_proj(i, (float)vid_width / (float)vid_height, xform);
			glMatrixMode(GL_PROJECTION);
			glLoadMatrixf(xform);

			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, vid_tex);

			glBegin(GL_QUADS);
			glTexCoord2f(0, 1);
			glVertex2f(-1, -1);
			glTexCoord2f(1, 1);
			glVertex2f(1, -1);
			glTexCoord2f(1, 0);
			glVertex2f(1, 1);
			glTexCoord2f(0, 0);
			glVertex2f(-1, 1);
			glEnd();

		} else {
			draw_static(tmsec, scr->aspect);
		}
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

static unsigned int nextpow2(unsigned int x)
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

static unsigned char *nextfrm(unsigned char *frm)
{
	frm += frame_size;
	return frm >= framebuf_end ? framebuf : frm;
}

static void stop_thread(void)
{
	pthread_mutex_lock(&frm_mutex);
	if(!playing) {
		pthread_mutex_unlock(&frm_mutex);
		return;
	}
	playing = 0;
	pthread_mutex_unlock(&frm_mutex);

	pthread_cond_signal(&frm_cond);
	pthread_join(thread, 0);
}

static void *thread_func(void *arg)
{
	int res;
	unsigned char *frm, *next = 0;

	pthread_mutex_lock(&frm_mutex);
	playing = 1;

	for(;;) {
		while(playing && (next = nextfrm(inframe)) == outframe) {
			pthread_cond_wait(&frm_cond, &frm_mutex);
		}
		frm = inframe;
		if(!playing) break;
		pthread_mutex_unlock(&frm_mutex);

		res = vid_get_frame(vidfile, frm);

		pthread_mutex_lock(&frm_mutex);
		if(res != -1) inframe = next;
	}
	pthread_mutex_unlock(&frm_mutex);

	printf("video: exiting thread\n");
	return 0;
}
