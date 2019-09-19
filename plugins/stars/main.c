#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <xlivebg.h>

#define STAR_COUNT	2048
#define STAR_DEPTH	100
#define STAR_SPEED	10.0f
#define STAR_SIZE	0.035f

struct vec3 {
	float x, y, z;
};

static int init(void *cls);
static void draw(long tmsec, void *cls);
static void draw_stars(long tmsec);
static void perspective(float *m, float vfov, float aspect, float znear, float zfar);

static struct vec3 star[STAR_COUNT];
static float star_lenxy[STAR_COUNT];


static struct xlivebg_plugin plugin = {
	"stars",
	"Starfield effect",
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
	int i;

	for(i=0; i<STAR_COUNT; i++) {
		star[i].x = 2.0f * (float)rand() / (float)RAND_MAX - 1.0f;
		star[i].y = 2.0f * (float)rand() / (float)RAND_MAX - 1.0f;
		star[i].z = 2.0f * (float)rand() / (float)RAND_MAX - 1.0f;

		star_lenxy[i] = sqrt(star[i].x * star[i].x + star[i].y * star[i].y);
	}
	return 0;
}

static void draw(long tmsec, void *cls)
{
	int i, num_scr;
	struct xlivebg_screen *scr;
	float proj[16];

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	num_scr = xlivebg_screen_count();
	for(i=0; i<num_scr; i++) {
		scr = xlivebg_screen(i);
		glViewport(scr->x, scr->y, scr->width, scr->height);

		glMatrixMode(GL_PROJECTION);
		perspective(proj, 50.0 / 180.0 * M_PI, scr->aspect, 0.5, 500.0);
		glLoadMatrixf(proj);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		draw_stars(tmsec);
	}
}

static void draw_stars(long tmsec)
{
	int i;
	float z, t, x, y, x0, y0, x1, y1, theta, sz;
	struct vec3 pos;
	double tsec = (double)tmsec / 1000.0;

	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	//glEnable(GL_TEXTURE_2D);
	//psys.get_attr()->get_texture()->bind();

	glBegin(GL_QUADS);
	for(i=0; i<STAR_COUNT; i++) {
		pos = star[i];
		z = fmod(pos.z + tsec * STAR_SPEED, STAR_DEPTH);
		t = z / STAR_DEPTH;
		pos.z = z - STAR_DEPTH;

		theta = atan2(pos.y / star_lenxy[i], pos.x / star_lenxy[i]);

		x = 0;
		y = -STAR_SIZE;

		x0 = x * cos(theta) - y * sin(theta);
		y0 = x * sin(theta) + y * cos(theta);

		y = STAR_SIZE;

		x1 = x * cos(theta) - y * sin(theta);
		y1 = x * sin(theta) + y * cos(theta);

		x0 += pos.x;
		x1 += pos.x;
		y0 += pos.y;
		y1 += pos.y;

		glColor4f(1, 1, 1, t);
		glTexCoord2f(0, 1);
		glVertex3f(x0, y0, pos.z);
		glTexCoord2f(1, 1);
		glVertex3f(x1, y1, pos.z);
		glTexCoord2f(1, 0);
		glVertex3f(x1, y1, pos.z - STAR_SIZE * 16.0);
		glTexCoord2f(0, 0);
		glVertex3f(x0, y0, pos.z - STAR_SIZE * 16.0);
	}
	glEnd();

	//star_tex->bind();
	sz = STAR_SIZE * 4.0;
	glBegin(GL_QUADS);
	for(i=0; i<STAR_COUNT; i++) {
		pos = star[i];
		z = fmod(pos.z + tsec * STAR_SPEED, STAR_DEPTH);
		t = z / STAR_DEPTH;
		pos.z = z - STAR_DEPTH;

		glColor4f(1, 1, 1, t);
		glTexCoord2f(0, 0);
		glVertex3f(pos.x - sz, pos.y - sz, pos.z);
		glTexCoord2f(1, 0);
		glVertex3f(pos.x + sz, pos.y - sz, pos.z);
		glTexCoord2f(1, 1);
		glVertex3f(pos.x + sz, pos.y + sz, pos.z);
		glTexCoord2f(0, 1);
		glVertex3f(pos.x - sz, pos.y + sz, pos.z);
	}
	glEnd();

	glPopAttrib();
}

static void perspective(float *m, float vfov, float aspect, float znear, float zfar)
{
	float s = 1.0f / (float)tan(vfov / 2.0f);
	float range = znear - zfar;

	memset(m, 0, sizeof *m);
	m[0] = s / aspect;
	m[5] = s;
	m[10] = (znear + zfar) / range;
	m[14] = 2.0f * znear * zfar / range;
	m[11] = -1.0f;
}

