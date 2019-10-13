#define GL_GLEXT_PROTOTYPES 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <xlivebg.h>
#include "app.h"

static int init(void *cls);
static void cleanup(void *cls);
static void start(long time_msec, void *cls);
static void stop(void *cls);
static void draw(long time_msec, void *cls);
static void draw_screen(int scr_idx, long time_msec);
static unsigned int create_program(const char *vsdr, const char *psdr);
static unsigned int create_shader(unsigned int type, const char *sdr);
static unsigned int next_pow2(unsigned int x);
static int enable_vsync(void);
static int init_glext(void);

static void (*glx_swap_interval_ext)(Display*, Window, int);
static void (*glx_swap_interval_mesa)(int);
static void (*glx_swap_interval_sgi)(int);

struct xlivebg_plugin colc_plugin = {
	"colcycle",
	"Color cycling",
	XLIVEBG_20FPS,
	init, cleanup,
	start, stop,
	draw,
	0
};

static int tex_xsz, tex_ysz;
static unsigned int img_tex, pal_tex, prog;
static unsigned char pal[256 * 3];
static int pal_valid;

static float verts[] = {
	-1, -1, 1, -1, 1, 1,
	-1, -1, 1, 1, -1, 1
};
static unsigned int vbo;

static const char *vsdr =
	"uniform mat4 xform;\n"
	"uniform vec2 uvscale;\n"
	"attribute vec4 attr_vertex;\n"
	"varying vec2 uv;\n"
	"void main()\n"
	"{\n"
	"\tgl_Position = xform * attr_vertex;\n"
	"\tuv = (attr_vertex.xy * vec2(0.5, -0.5) + 0.5) * uvscale;\n"
	"}\n";

static const char *psdr =
	"uniform sampler2D img_tex;\n"
	"uniform sampler1D pal_tex;\n"
	"varying vec2 uv;\n"
	"void main()\n"
	"{\n"
	"\tfloat cidx = texture2D(img_tex, uv).x;\n"
	"\tvec3 color = texture1D(pal_tex, cidx).xyz;\n"
	"\tgl_FragColor.xyz = color;\n"
	"\tgl_FragColor.a = 1.0;\n"
	"}\n";


void register_plugin(void)
{
	xlivebg_register_plugin(&colc_plugin);
}

void resize(int xsz, int ysz)
{
	if(xsz == fbwidth && ysz == fbheight) return;

	fbwidth = xsz;
	fbheight = ysz;

	tex_xsz = next_pow2(fbwidth);
	tex_ysz = next_pow2(fbheight);

	glBindTexture(GL_TEXTURE_2D, img_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tex_xsz, tex_ysz, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);
}


void set_palette(int idx, int r, int g, int b)
{
	unsigned char *pptr = pal + idx * 3;
	pptr[0] = r;
	pptr[1] = g;
	pptr[2] = b;
	pal_valid = 0;
}

static int init(void *cls)
{
	return 0;
}

static void cleanup(void *cls)
{
}

static void start(long msec, void *cls)
{
	int loc;
	char *argv[] = {"colcycle", 0, 0};

	argv[1] = (char*)xlivebg_getcfg_str("xlivebg.colcycle.imagedir", argv[1]);
	argv[1] = (char*)xlivebg_getcfg_str("xlivebg.colcycle.image", argv[1]);

	init_glext();

	fbwidth = 640;
	fbheight = 480;
	fbpixels = malloc(fbwidth * fbheight);

	colc_init(argv[1] ? 2 : 1, argv);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	tex_xsz = next_pow2(fbwidth);
	tex_ysz = next_pow2(fbheight);

	glGenTextures(1, &img_tex);
	glBindTexture(GL_TEXTURE_2D, img_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tex_xsz, tex_ysz, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fbwidth, fbheight, GL_LUMINANCE, GL_UNSIGNED_BYTE, fbpixels);

	glGenTextures(1, &pal_tex);
	glBindTexture(GL_TEXTURE_1D, pal_tex);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, pal);

	if((prog = create_program(vsdr, psdr))) {
		glBindAttribLocation(prog, 0, "attr_vertex");
		glLinkProgram(prog);
		glUseProgram(prog);
		if((loc = glGetUniformLocation(prog, "img_tex")) >= 0) {
			glUniform1i(loc, 0);
		}
		if((loc = glGetUniformLocation(prog, "pal_tex")) >= 0) {
			glUniform1i(loc, 1);
		}
		if((loc = glGetUniformLocation(prog, "uvscale")) >= 0) {
			glUniform2f(loc, (float)fbwidth / (float)tex_xsz, (float)fbheight / (float)tex_ysz);
		}
	}
	glUseProgram(0);

	if(enable_vsync() == -1) {
		fprintf(stderr, "failed to enable vsync\n");
	}
}

static void stop(void *cls)
{
	colc_cleanup();
}

static void draw(long time_msec, void *cls)
{
	int i, num_scr, sy, loc, fit_mode;
	float aspect, fbaspect, vpscale;
	float xform[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
	struct xlivebg_screen *scr;

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	num_scr = xlivebg_screen_count();
	for(i=0; i<num_scr; i++) {
		scr = xlivebg_screen(i);
		sy = scr->root_height - scr->height - scr->y;
		glViewport(scr->x, sy, scr->width, scr->height);

		aspect = (float)scr->width / (float)scr->height;
		fbaspect = (float)fbwidth / (float)fbheight;

		fit_mode = xlivebg_fit_mode(i);

		if(fit_mode != XLIVEBG_FIT_STRETCH) {
			if(aspect > fbaspect) {
				vpscale = xform[0] = fbaspect / aspect;
				xform[5] = 1.0f;
			} else if(fbaspect > aspect) {
				vpscale = xform[5] = aspect / fbaspect;
				xform[0] = 1.0f;
			}

			if(fit_mode == XLIVEBG_FIT_CROP) {
				float cropscale, maxpan, tx, ty;
				float cdir[2];

				cropscale = 1.0f / vpscale;
				cropscale = 1.0f + (cropscale - 1.0f) * xlivebg_crop_zoom(i);
				maxpan = cropscale - 1.0f;

				xform[0] *= cropscale;
				xform[5] *= cropscale;

				xlivebg_crop_dir(i, cdir);
				if(aspect > fbaspect) {
					tx = 0.0f;
					ty = -cdir[1] * maxpan;
				} else {
					tx = -cdir[0] * maxpan;
					ty = 0.0f;
				}

				xform[12] = tx;
				xform[13] = ty;
			} else {
				xform[12] = xform[13] = 0.0f;
			}
		}

		glUseProgram(prog);
		if((loc = glGetUniformLocation(prog, "xform")) >= 0) {
			glUniformMatrix4fv(loc, 1, GL_FALSE, xform);
		}

		draw_screen(i, time_msec);
	}
}

static void draw_screen(int scr_idx, long time_msec)
{
	*(uint32_t*)fbpixels = 0xbadf00d;
	colc_draw(time_msec);

	if(*(uint32_t*)fbpixels != 0xbadf00d) {
		/* update texture data if the framebuffer changed */
		glBindTexture(GL_TEXTURE_2D, img_tex);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fbwidth, fbheight, GL_LUMINANCE, GL_UNSIGNED_BYTE, fbpixels);
	}
	if(!pal_valid) {
		/* update the palette texture */
		glBindTexture(GL_TEXTURE_1D, pal_tex);
		glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 256, GL_RGB, GL_UNSIGNED_BYTE, pal);
		pal_valid = 1;
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, img_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, pal_tex);
	glActiveTexture(GL_TEXTURE0);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	assert(glGetError() == GL_NO_ERROR);
}

static unsigned int create_program(const char *vsdr, const char *psdr)
{
	unsigned int vs, ps, prog;
	int status;

	if(!(vs = create_shader(GL_VERTEX_SHADER, vsdr))) {
		return 0;
	}
	if(!(ps = create_shader(GL_FRAGMENT_SHADER, psdr))) {
		glDeleteShader(vs);
		return 0;
	}

	prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, ps);
	glLinkProgram(prog);

	glGetProgramiv(prog, GL_LINK_STATUS, &status);
	if(!status) {
		fprintf(stderr, "failed to link shader program\n");
		glDeleteProgram(prog);
		prog = 0;
	}
	return prog;
}

static unsigned int create_shader(unsigned int type, const char *src)
{
	unsigned int sdr;
	int status, info_len;

	sdr = glCreateShader(type);
	glShaderSource(sdr, 1, &src, 0);
	glCompileShader(sdr);

	glGetShaderiv(sdr, GL_COMPILE_STATUS, &status);
	if(!status) {
		fprintf(stderr, "failed to compile %s shader\n", type == GL_VERTEX_SHADER ? "vertex" : "pixel");
	}

	glGetShaderiv(sdr, GL_INFO_LOG_LENGTH, &info_len);
	if(info_len) {
		char *buf = alloca(info_len + 1);
		glGetShaderInfoLog(sdr, info_len, 0, buf);
		buf[info_len] = 0;
		if(buf[0]) {
			fprintf(stderr, "compiler output:\n%s\n", buf);
		}
	}

	if(!status) {
		glDeleteShader(sdr);
		sdr = 0;
	}
	return sdr;
}

static unsigned int next_pow2(unsigned int x)
{
	--x;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

static int enable_vsync(void)
{
	if(glx_swap_interval_ext) {
		Display *dpy = glXGetCurrentDisplay();
		Window win = glXGetCurrentDrawable();
		glx_swap_interval_ext(dpy, win, 1);
		return 0;
	}
	if(glx_swap_interval_mesa) {
		glx_swap_interval_mesa(1);
		return 0;
	}
	if(glx_swap_interval_sgi) {
		glx_swap_interval_sgi(1);
		return 0;
	}
	return -1;
}

#define get_proc_addr(s)	(void (*)())glXGetProcAddress((unsigned char*)(s));

static int init_glext(void)
{
	static int init_done;
	const char *extstr;
	XWindowAttributes wattr;
	Display *dpy = glXGetCurrentDisplay();
	Window win = glXGetCurrentDrawable();
	int scr;

	if(init_done) return 0;
	init_done = 1;

	XGetWindowAttributes(dpy, win, &wattr);
	scr = XScreenNumberOfScreen(wattr.screen);

	if(!(extstr = glXQueryExtensionsString(dpy, scr))) {
		return -1;
	}

	if(strstr(extstr, "GLX_EXT_swap_control")) {
		glx_swap_interval_ext = get_proc_addr("glXSwapIntervalEXT");
	}
	if(strstr(extstr, "GLX_MESA_swap_control")) {
		glx_swap_interval_mesa = get_proc_addr("glXSwapIntervalMESA");
	}
	if(strstr(extstr, "GLX_SGI_swap_control")) {
		glx_swap_interval_sgi = get_proc_addr("glXSwapIntervalSGI");
	}
	return 0;
}
