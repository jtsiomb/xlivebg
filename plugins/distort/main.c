#include <stdio.h>
#include <math.h>
#include <GL/gl.h>
#include <xlivebg.h>

static int init(void *cls);
static void draw(long tmsec, void *cls);

static struct xlivebg_plugin plugin = {
	"distort",
	"Image distortion effect on the background image",
	XLIVEBG_30FPS,
	init, 0,
	0, 0,
	draw,
	0
};

void register_plugin(void)
{
	xlivebg_register_plugin(&plugin);
}

static int init(void *cls)
{
	return 0;
}

#define USUB	24
#define VSUB	20

static float wave(float x, float frq, float amp, float t)
{
	t *= 0.5;
	return x + (cos(x * frq + t)/* + cos(x * frq * 2.0f + t) * 0.5f*/) * amp;
}

static float dmask(float x)
{
	float s = sin(x * M_PI) * 20.0f;
	return s > 1.0f ? 1.0f : s;
}

#define COLOR(u, v)	\
	do { \
		float m = dmask(u) * dmask(v); \
		glColor3f(m, m, m); \
	} while(0)

static void distquad(float aspect, float t)
{
	int i, j;
	float freq = 8.0f;
	float ampl = 0.025;
	float du = 1.0f / (float)(USUB - 1);
	float dv = 1.0f / (float)(VSUB - 1);
	float dx = du * 2.0f;
	float dy = dv * 2.0f;

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	glBegin(GL_QUADS);
	for(i=0; i<VSUB; i++) {
		float v0 = (float)i * dv;
		float v1 = v0 + dv;
		float y = v0 * 2.0f - 1.0f;

		v0 = wave(v0, freq * 2.0f, dmask(v0) * ampl * 0.75, t);
		v1 = wave(v1, freq * 2.0f, dmask(v1) * ampl * 0.75, t);

		for(j=0; j<USUB; j++) {
			float u0 = (float)j * du;
			float u1 = u0 + du;
			float x = u0 * 2.0f - 1.0f;

			u0 = wave(u0, freq, dmask(u0) * ampl, t);
			u1 = wave(u1, freq, dmask(u1) * ampl, t);

			glTexCoord2f(u0, 1.0f - v0);
			glVertex2f(x, y);
			glTexCoord2f(u1, 1.0f - v0);
			glVertex2f(x + dx, y);
			glTexCoord2f(u1, 1.0f - v1);
			glVertex2f(x + dx, y + dy);
			glTexCoord2f(u0, 1.0f - v1);
			glVertex2f(x, y + dy);
		}
	}
	glEnd();
}

static void draw(long tmsec, void *cls)
{
	int i, num_scr;
	struct xlivebg_screen *scr;
	struct xlivebg_image *img;
	float t = (float)tmsec / 1000.0f;

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	num_scr = xlivebg_screen_count();
	for(i=0; i<num_scr; i++) {
		scr = xlivebg_screen(i);
		glViewport(scr->x, scr->y, scr->width, scr->height);

		if((img = xlivebg_bgimage(i)) && img->tex) {
			glBindTexture(GL_TEXTURE_2D, img->tex);
			glEnable(GL_TEXTURE_2D);

			distquad(scr->aspect, t);
		}
	}
}
