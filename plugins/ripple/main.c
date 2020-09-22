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
static void stop(void *cls);
static void draw(long time_msec, void *cls);
static void resize(int x, int y);
static unsigned int create_shader(const char *src, unsigned int type);
static unsigned int create_sdrprog(const char *vsrc, const char *psrc);

static struct xlivebg_plugin plugin = {
	"ripple",
	"Ripple",
	0,
	XLIVEBG_20FPS,
	init, cleanup,
	start, stop,
	draw,
	0
};

static int scr_width, scr_height;
static float scr_aspect;
static unsigned int fbo;
static unsigned int ripple_tex[2];
static int cur_ripple_tex;
static unsigned int sdr_blur, sdr_vis;
static int blur_dir_loc, blur_delta_loc, blur_intens_loc;

static float mpos[2], prev_mpos[2];
static float mouse_delta;

extern const char ripple_vsdr, ripple_blur_psdr, ripple_psdr;

int register_plugin(void)
{
	return xlivebg_register_plugin(&plugin);
}

static int init(void *cls)
{
	int loc;

	if(!(sdr_blur = create_sdrprog(&ripple_vsdr, &ripple_blur_psdr))) {
		return -1;
	}
	blur_dir_loc = glGetUniformLocation(sdr_blur, "dir");
	blur_delta_loc = glGetUniformLocation(sdr_blur, "delta");
	blur_intens_loc = glGetUniformLocation(sdr_blur, "intensity");

	if(!(sdr_vis = create_sdrprog(&ripple_vsdr, &ripple_psdr))) {
		glDeleteProgram(sdr_blur);
		return -1;
	}
	glUseProgram(sdr_vis);
	if((loc = glGetUniformLocation(sdr_vis, "tex_img")) != -1) {
		glUniform1i(loc, 0);
	}
	if((loc = glGetUniformLocation(sdr_vis, "tex_ripple")) != -1) {
		glUniform1i(loc, 1);
	}
	glUseProgram(0);

	return 0;
}

static void cleanup(void *cls)
{
	if(sdr_blur) {
		glDeleteProgram(sdr_blur);
	}
	if(sdr_vis) {
		glDeleteProgram(sdr_vis);
	}
}

static void start(long time_msec, void *cls)
{
	int i;
	struct xlivebg_screen *scr = xlivebg_screen(0);

	glGenTextures(2, ripple_tex);
	for(i=0; i<2; i++) {
		glBindTexture(GL_TEXTURE_2D, ripple_tex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	resize(scr->root_width, scr->root_height);

	glGenFramebuffers(1, &fbo);
}

static void stop(void *cls)
{
	glDeleteFramebuffers(1, &fbo);
	glDeleteTextures(2, ripple_tex);
}

static void update_ripple(long time_msec)
{
	int i;
	float strength, xsz, ysz;

	strength = mouse_delta;
	xsz = 0.01;
	ysz = 0.01 * scr_aspect;

	glPushAttrib(GL_ENABLE_BIT | GL_TRANSFORM_BIT);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, scr_width, scr_height);

	printf("upd: bind texture: %d\n", cur_ripple_tex);
	glBindTexture(GL_TEXTURE_2D, ripple_tex[cur_ripple_tex]);
	cur_ripple_tex ^= 1;
	printf("upd: fb texture: %d\n", cur_ripple_tex);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
			ripple_tex[cur_ripple_tex], 0);


	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	glUseProgram(sdr_blur);
	if(blur_delta_loc >= 0) {
		glUniform2f(blur_delta_loc, 1.0f / scr_width, 1.0f / scr_height);
	}
	if(blur_intens_loc >= 0) {
		glUniform1f(blur_intens_loc, 1.0);
	}

	for(i=0; i<2; i++) {
		if(blur_dir_loc >= 0) {
			if(i) {
				glUniform2f(blur_dir_loc, 0, 1);
			} else {
				glUniform2f(blur_dir_loc, 1, 0);
			}
		}

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
	}


	glUseProgram(0);
	glBegin(GL_QUADS);
	glColor3f(1, 1, 1);//strength, strength, strength);
	glVertex2f(mpos[0] - xsz, mpos[1] - ysz);
	glVertex2f(mpos[0] + xsz, mpos[1] - ysz);
	glVertex2f(mpos[0] + xsz, mpos[1] + ysz);
	glVertex2f(mpos[0] - xsz, mpos[1] + ysz);
	glEnd();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glPopAttrib();
}

static void draw(long time_msec, void *cls)
{
	int i, num_scr, mx, my;
	struct xlivebg_image *img;
	struct xlivebg_screen *scr;
	float dx, dy;
	float uv_xoffs, uv_yoffs, uv_xscale, uv_yscale;

	scr = xlivebg_screen(0);
	/* this will only resize if the size has changed */
	resize(scr->root_width, scr->root_height);

	xlivebg_mouse_pos(&mx, &my);
	prev_mpos[0] = mpos[0];
	prev_mpos[1] = mpos[1];
	mpos[0] = (float)mx / (float)scr->root_width * 2.0f - 1.0f;
	mpos[1] = (float)my / (float)scr->root_height * 2.0f - 1.0f;
	dx = fabs(mpos[0] - prev_mpos[0]);
	dy = fabs(mpos[1] - prev_mpos[1]);
	mouse_delta = dx > dy ? dx : dy;

	update_ripple(time_msec);

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	num_scr = xlivebg_screen_count();
	for(i=0; i<num_scr; i++) {
		scr = xlivebg_screen(i);
		xlivebg_gl_viewport(i);

		if((img = xlivebg_bg_image(i)) && img->tex) {
			glBindTexture(GL_TEXTURE_2D, img->tex);

			glActiveTexture(GL_TEXTURE1);
			printf("draw: ripple tex: %d\n", cur_ripple_tex);
			glBindTexture(GL_TEXTURE_2D, ripple_tex[cur_ripple_tex]);

			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
			uv_xscale = (float)scr->width / (float)scr->root_width;
			uv_yscale = (float)scr->height / (float)scr->root_height;
			uv_xoffs = (float)scr->x / (float)scr->root_width;
			uv_yoffs = (float)scr->y / (float)scr->root_height;
			glTranslatef(uv_xoffs, uv_yoffs, 0);
			glScalef(uv_xscale, uv_yscale, 1);

			glActiveTexture(GL_TEXTURE0);

			glUseProgram(sdr_vis);

			glBegin(GL_QUADS);
			glColor3f(1, 1, 1);
			glTexCoord2f(0, 1);
			glVertex2f(-1, -1);
			glTexCoord2f(1, 1);
			glVertex2f(1, -1);
			glTexCoord2f(1, 0);
			glVertex2f(1, 1);
			glTexCoord2f(0, 0);
			glVertex2f(-1, 1);
			glEnd();

			glActiveTexture(GL_TEXTURE1);
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
			glActiveTexture(GL_TEXTURE0);
		}
	}
}

static void resize(int x, int y)
{
	int i;
	unsigned char *pixels;

	if(scr_width == x && scr_height == y) return;

	if(x > scr_width || y > scr_height) {
		if((pixels = malloc(x * y))) {
			memset(pixels, 128, x * y);
		}
	}

	for(i=0; i<2; i++) {
		glBindTexture(GL_TEXTURE_2D, ripple_tex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, x, y, 0, GL_LUMINANCE,
				GL_UNSIGNED_BYTE, pixels);
	}

	scr_width = x;
	scr_height = y;
	scr_aspect = (float)x / (float)y;
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
