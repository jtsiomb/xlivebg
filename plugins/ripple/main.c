#define GL_GLEXT_PROTOTYPES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <xlivebg.h>

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
	XLIVEBG_20FPS,
	init, cleanup,
	start, stop,
	draw,
	0
};

static unsigned int fbo;
static unsigned int ripple_tex[2];
static int cur_fbtex;
static unsigned int sdr_ripple, sdr_vis;

extern const char ripple_vsdr, ripple_blur_psdr;

void register_plugin(void)
{
	xlivebg_register_plugin(&plugin);
}

static int init(void *cls)
{
	if(!(sdr_ripple = create_sdrprog(&ripple_vsdr, &ripple_blur_psdr))) {
		return -1;
	}

	return 0;
}

static void cleanup(void *cls)
{
	if(sdr_ripple) {
		glDeleteProgram(sdr_ripple);
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
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
			ripple_tex[cur_fbtex], 0);
}

static void stop(void *cls)
{
	glDeleteFramebuffers(1, &fbo);
	glDeleteTextures(2, ripple_tex);
}

static void update_ripple(long time_msec)
{
	glPushAttrib(GL_ENABLE_BIT | GL_TRANSFORM_BIT);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

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

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glPopAttrib();
}

static void draw(long time_msec, void *cls)
{
	int i, num_scr, mx, my;
	float mpos[2];
	struct xlivebg_image *img;
	struct xlivebg_screen *scr;

	update_ripple(time_msec);

	xlivebg_mouse_pos(&mx, &my);

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	num_scr = xlivebg_screen_count();
	for(i=0; i<num_scr; i++) {
		scr = xlivebg_screen(i);
		xlivebg_gl_viewport(i);

		/* this will only resize if the size has changed */
		resize(scr->root_width, scr->root_height);

		if((img = xlivebg_bg_image(i)) && img->tex) {
			glBindTexture(GL_TEXTURE_2D, img->tex);
			glEnable(GL_TEXTURE_2D);

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
		}

		mpos[0] = (float)mx / (float)scr->root_width * 2.0f - 1.0f;
		mpos[1] = 1.0f - (float)my / (float)scr->root_height * 2.0f;

		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glColor3f(1, 0, 0);
		glVertex2f(mpos[0] - 0.05, mpos[1] - 0.05);
		glVertex2f(mpos[0] + 0.05, mpos[1] - 0.05);
		glVertex2f(mpos[0] + 0.05, mpos[1] + 0.05);
		glVertex2f(mpos[0] - 0.05, mpos[1] + 0.05);
		glEnd();
	}
}

static void resize(int x, int y)
{
	int i;
	unsigned char *pixels;
	static int prev_x, prev_y;

	if(prev_x == x && prev_y == y) return;

	if(x > prev_x || y > prev_y) {
		if((pixels = malloc(x * y))) {
			memset(pixels, 128, x * y);
		}
	}

	for(i=0; i<2; i++) {
		glBindTexture(GL_TEXTURE_2D, ripple_tex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, x, y, 0, GL_LUMINANCE,
				GL_UNSIGNED_BYTE, pixels);
	}

	prev_x = x;
	prev_y = y;
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
