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
#define GL_GLEXT_PROTOTYPES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include "xlivebg.h"

static int init(void *cls);
static void cleanup(void *cls);
static void start(long time_msec, void *cls);
static void resize(int x, int y);
static void stop(void *cls);
static void prop(const char *prop, void *cls);
static void draw(long time_msec, void *cls);
static unsigned int create_shader(const char *src, unsigned int type);
static unsigned int create_sdrprog(const char *vsrc, const char *psrc);

#define PROPLIST	\
	"proplist {\n" \
	"    prop {\n" \
	"        id = \"raindrops\"\n" \
	"        desc = \"number of raindrops per second\"\n" \
	"        type = \"number\"\n" \
	"        range = [0, 100]\n" \
	"    }\n" \
	"}\n"

static struct xlivebg_plugin plugin = {
	"ripple",
	"Ripple",
	PROPLIST,
	XLIVEBG_20FPS,
	init, cleanup,
	start, stop,
	draw,
	prop,
	0, 0
};

static int scr_width, scr_height;
static float scr_aspect;
static unsigned int fbo;
static unsigned int ripple_tex[3];
static unsigned int blobtex, dummy_mask_tex;
static unsigned int frame;
static unsigned int sdr_blur, sdr_vis;
static int blur_delta_loc;

static float mpos[2], prev_mpos[2];
static float rain_rate, pending_drops;
static long prev_upd;

extern const char ripple_vsdr, ripple_blur_psdr, ripple_psdr;

#define BLOBTEX_SIZE	64
#define TEX_SRC		(frame & 1)
#define TEX_DEST	((frame ^ 1) & 1)
#define TEX_AUX		2

#define TEX_SIZE_DIV	2
#define PLONK_SIZE		0.01


int register_plugin(void)
{
	return xlivebg_register_plugin(&plugin);
}

static int init(void *cls)
{
	xlivebg_defcfg_num("xlivebg.ripple.raindrops", 0);
	return 0;
}

static void cleanup(void *cls)
{
}

static void start(long time_msec, void *cls)
{
	int i, j, loc;
	unsigned char *blob;
	struct xlivebg_screen *scr = xlivebg_screen(0);
	static unsigned char whitepix[256];

	scr_width = scr_height = 0;

	if(!(sdr_vis = create_sdrprog(&ripple_vsdr, &ripple_psdr))) {
		return;
	}
	glUseProgram(sdr_vis);
	/* tex_img in unit 0, tex_mask in unit 1, tex_ripple in unit 2 */
	if((loc = glGetUniformLocation(sdr_vis, "tex_mask"))) {
		glUniform1i(loc, 1);
	}
	if((loc = glGetUniformLocation(sdr_vis, "tex_ripple"))) {
		glUniform1i(loc, 2);
	}

	if(!(sdr_blur = create_sdrprog(&ripple_vsdr, &ripple_blur_psdr))) {
		glUseProgram(0);
		glDeleteProgram(sdr_vis);
		return;
	}
	glUseProgram(sdr_blur);
	blur_delta_loc = glGetUniformLocation(sdr_blur, "delta");
	if((loc = glGetUniformLocation(sdr_blur, "dest")) >= 0) {
		glUniform1i(loc, 1);
	}

	/* create dummy mask texture */
	memset(whitepix, 0xff, sizeof whitepix);
	glGenTextures(1, &dummy_mask_tex);
	glBindTexture(GL_TEXTURE_2D, dummy_mask_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitepix);


	/* create the blob texture used to stamp waves onto the grid */
	blob = alloca(BLOBTEX_SIZE * BLOBTEX_SIZE);
	for(i=0; i<BLOBTEX_SIZE; i++) {
		float v = (float)i / BLOBTEX_SIZE * 2.0f - 1.0f;
		for(j=0; j<BLOBTEX_SIZE; j++) {
			float u = (float)j / BLOBTEX_SIZE * 2.0f - 1.0f;
			float dsq = u * u + v * v;
			float fade = 1.0f - (dsq > 1.0f ? 1.0f : dsq);
			float val = dsq == 0.0f ? 1.0f : 0.05f * fade / dsq;
			int ival = (int)(val * 255.0f);
			if(ival > 255) ival = 255;
			blob[i * BLOBTEX_SIZE + j] = ival;
		}
	}

	glGenTextures(1, &blobtex);
	glBindTexture(GL_TEXTURE_2D, blobtex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, BLOBTEX_SIZE, BLOBTEX_SIZE, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, blob);

	/* create the ping-pong textures */
	glGenTextures(3, ripple_tex);
	for(i=0; i<3; i++) {
		glBindTexture(GL_TEXTURE_2D, ripple_tex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	/* and allocate storage for the textures to cover the whole root window */
	resize(scr->root_width, scr->root_height);

	/* create the FBO */
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ripple_tex[TEX_DEST], 0);
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "framebuffer incomplete!\n");
	}

	/* initialize the first texture to 0.5 (which maps to 0 in the wave calculation) */
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	prop("raindrops", 0);

	pending_drops = 0;
	prev_upd = time_msec;
}

static void stop(void *cls)
{
	if(ripple_tex[0]) {
		glDeleteTextures(3, ripple_tex);
		ripple_tex[0] = ripple_tex[1] = ripple_tex[2] = 0;
	}
	if(blobtex) glDeleteTextures(1, &blobtex);
	if(dummy_mask_tex) glDeleteTextures(1, &dummy_mask_tex);
	if(fbo) {
		glDeleteFramebuffers(1, &fbo);
		fbo = 0;
	}
	if(sdr_vis) glDeleteProgram(sdr_vis);
	if(sdr_blur) glDeleteProgram(sdr_blur);
}

static void prop(const char *prop, void *cls)
{
	if(strcmp(prop, "raindrops") == 0) {
		rain_rate = xlivebg_getcfg_num("xlivebg.ripple.raindrops", 0);
	}
}

/* TODO: pow2 */
static void resize(int x, int y)
{
	int i;
	unsigned char *pixels;

	if(x == scr_width && y == scr_height) return;

	scr_width = x;
	scr_height = y;
	scr_aspect = (float)x / (float)y;

	pixels = alloca(x * y);
	memset(pixels, 127, x * y);

	for(i=0; i<3; i++) {
		glBindTexture(GL_TEXTURE_2D, ripple_tex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, x / TEX_SIZE_DIV, y / TEX_SIZE_DIV,
				0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);
	}

	glUseProgram(sdr_blur);
	glUniform2f(blur_delta_loc, (float)TEX_SIZE_DIV / x, (float)TEX_SIZE_DIV / y);
}

static void plonk(float u, float v)
{
	glBegin(GL_QUADS);
	glColor3f(1, 1, 1);
	glTexCoord2f(0, 0);
	glVertex2f(u - PLONK_SIZE, v + PLONK_SIZE * scr_aspect);
	glTexCoord2f(1, 0);
	glVertex2f(u + PLONK_SIZE, v + PLONK_SIZE * scr_aspect);
	glTexCoord2f(1, 1);
	glVertex2f(u + PLONK_SIZE, v - PLONK_SIZE * scr_aspect);
	glTexCoord2f(0, 1);
	glVertex2f(u - PLONK_SIZE, v - PLONK_SIZE * scr_aspect);
	glEnd();
}

static void update_ripple(long time_msec)
{
	int mouse_moved;
	float dt = (time_msec - prev_upd) / 1000.0f;
	prev_upd = time_msec;

	pending_drops += rain_rate * dt;
	mouse_moved = mpos[0] != prev_mpos[0] || mpos[1] != prev_mpos[1];

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, scr_width / TEX_SIZE_DIV, scr_height / TEX_SIZE_DIV);

	/* draw any new drops in the previous buffer first */
	if(mouse_moved || pending_drops >= 1.0f) {
		/*float dx = mpos[0] - prev_mpos[0];
		float dy = mpos[1] - prev_mpos[1];*/
		glUseProgram(0);

		glPushAttrib(GL_ENABLE_BIT);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, blobtex);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);

		while(pending_drops >= 1.0f) {
			plonk(rand() * 2.0f / RAND_MAX - 1.0f, rand() * 2.0f / RAND_MAX - 1.0f);
			pending_drops -= 1.0f;
		}

		if(mouse_moved) {
			plonk(mpos[0], mpos[1]);
		}

		glDisable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
	}

	/* increment the frame counter, which effectively swaps the ping-pong buffers */
	frame++;

	/* attach the new destination buffer to the FBO */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
			ripple_tex[TEX_DEST], 0);

	/* copy the contents of the destination texture to the auxiliary */
	glBindTexture(GL_TEXTURE_2D, ripple_tex[2]);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, scr_width / TEX_SIZE_DIV,
			scr_height / TEX_SIZE_DIV);

	/* do the ripple blur effect from src -> dest */
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, ripple_tex[TEX_AUX]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ripple_tex[TEX_SRC]);
	glUseProgram(sdr_blur);

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(-1, -1);
	glTexCoord2f(1, 0);
	glVertex2f(1, -1);
	glTexCoord2f(1, 1);
	glVertex2f(1, 1);
	glTexCoord2f(0, 1);
	glVertex2f(-1, 1);
	glEnd();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void draw(long time_msec, void *cls)
{
	int mx, my, i, num_scr;
	struct xlivebg_screen *scr;
	struct xlivebg_image *img;
	float xform[16];
	float uoffs, voffs, uscale, vscale;

	xlivebg_clear(GL_COLOR_BUFFER_BIT);

	scr = xlivebg_screen(0);
	resize(scr->root_width, scr->root_height);	/* nop if size is unchanged */

	xlivebg_mouse_pos(&mx, &my);
	prev_mpos[0] = mpos[0];
	prev_mpos[1] = mpos[1];
	mpos[0] = (float)mx / scr->root_width * 2.0f - 1.0f;
	mpos[1] = (float)my / scr->root_height * 2.0f - 1.0f;

	update_ripple(time_msec);

	glUseProgram(sdr_vis);
	/* use the destination of the blur from update_ripple as texture 1*/
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, ripple_tex[TEX_DEST]);

	num_scr = xlivebg_screen_count();
	for(i=0; i<num_scr; i++) {
		scr = xlivebg_screen(i);
		xlivebg_gl_viewport(i);

		if((img = xlivebg_bg_image(i)) && img->tex) {
			struct xlivebg_image *amask = xlivebg_anim_mask(i);

			xlivebg_calc_image_proj(i, (float)img->width / img->height, xform);
			glMatrixMode(GL_PROJECTION);
			glLoadMatrixf(xform);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, amask ? amask->tex : dummy_mask_tex);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, img->tex);

			uoffs = (float)scr->x / scr->root_width;
			voffs = (float)scr->y / scr->root_height;
			uscale = (float)scr->width / scr->root_width;
			vscale = (float)scr->height / scr->root_height;

			glBegin(GL_QUADS);
			glColor3f(1, 0, 0);
			glTexCoord2f(0, 1);
			glMultiTexCoord2f(GL_TEXTURE1, uoffs, voffs + vscale);
			glVertex2f(-1, -1);
			glColor3f(0, 1, 0);
			glTexCoord2f(1, 1);
			glMultiTexCoord2f(GL_TEXTURE1, uoffs + uscale, voffs + vscale);
			glVertex2f(1, -1);
			glColor3f(0, 0, 1);
			glTexCoord2f(1, 0);
			glMultiTexCoord2f(GL_TEXTURE1, uoffs + uscale, voffs);
			glVertex2f(1, 1);
			glColor3f(1, 0, 1);
			glTexCoord2f(0, 0);
			glMultiTexCoord2f(GL_TEXTURE1, uoffs, voffs);
			glVertex2f(-1, 1);
			glEnd();
		}
	}

	glUseProgram(0);
}

static unsigned int create_shader(const char *src, unsigned int type)
{
	unsigned int sdr;
	int status, log_len;

	printf("compiling %s shader ... ", type == GL_VERTEX_SHADER ? "vertex" : "pixel");
	fflush(stdout);

	if(!(sdr = glCreateShader(type))) {
		fprintf(stderr, "failed to create shader\n");
		return 0;
	}
	glShaderSource(sdr, 1, &src, 0);
	glCompileShader(sdr);

	glGetShaderiv(sdr, GL_COMPILE_STATUS, &status);
	glGetShaderiv(sdr, GL_INFO_LOG_LENGTH, &log_len);

	printf("%s\n", status ? "done" : "failed");
	if(log_len) {
		char *buf = alloca(log_len + 1);
		glGetShaderInfoLog(sdr, log_len + 1, 0, buf);
		printf("%s\n", buf);
	}

	if(!status) {
		glDeleteShader(sdr);
		return 0;
	}
	return sdr;
}

static unsigned int create_sdrprog(const char *vsrc, const char *psrc)
{
	int status, log_len;
	unsigned int prog;
	unsigned int vs, ps;

	if(!(vs = create_shader(vsrc, GL_VERTEX_SHADER))) {
		return 0;
	}
	if(!(ps = create_shader(psrc, GL_FRAGMENT_SHADER))) {
		glDeleteShader(vs);
		return 0;
	}

	printf("linking shader program ... ");
	fflush(stdout);

	if(!(prog = glCreateProgram())) {
		glDeleteShader(vs);
		glDeleteShader(ps);
		fprintf(stderr, "failed to create shader program\n");
		return 0;
	}
	glAttachShader(prog, vs);
	glAttachShader(prog, ps);
	glLinkProgram(prog);

	glGetProgramiv(prog, GL_LINK_STATUS, &status);
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_len);

	puts(status ? "done" : "failed");
	if(log_len) {
		char *buf = alloca(log_len + 1);
		glGetProgramInfoLog(prog, log_len + 1, 0, buf);
		puts(buf);
	}

	if(!status) {
		glDeleteProgram(prog);
		return 0;
	}
	return prog;
}
