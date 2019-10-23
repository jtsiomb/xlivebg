#include <stdio.h>
#include <GL/gl.h>
#include <xlivebg.h>

static int init(void *cls);
static void cleanup(void *cls);
static void start(long time_msec, void *cls);
static void stop(void *cls);
static void draw(long time_msec, void *cls);

static struct xlivebg_plugin plugin = {
	"ripple",
	"Ripple",
	XLIVEBG_20FPS,
	init, cleanup,
	start, stop,
	draw,
	0
};

static unsigned int ripple_tex;


void register_plugin(void)
{
	xlivebg_register_plugin(&plugin);
}

static int init(void *cls)
{
	glGenTextures(1, &ripple_tex);
	glBindTexture(GL_TEXTURE_2D, ripple_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return 0;
}

static void cleanup(void *cls)
{
	glDeleteTextures(1, &ripple_tex);
}

static void start(long time_msec, void *cls)
{
	struct xlivebg_screen *scr = xlivebg_screen(0);

	glBindTexture(GL_TEXTURE_2D, ripple_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, scr->root_width, scr->root_height,
			0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);
}

static void stop(void *cls)
{
}

static void draw(long time_msec, void *cls)
{
	int i, num_scr, mx, my;
	float mpos[2];
	struct xlivebg_image *img;
	struct xlivebg_screen *scr;

	xlivebg_mouse_pos(&mx, &my);

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	num_scr = xlivebg_screen_count();
	for(i=0; i<num_scr; i++) {
		scr = xlivebg_screen(i);
		xlivebg_gl_viewport(i);

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
