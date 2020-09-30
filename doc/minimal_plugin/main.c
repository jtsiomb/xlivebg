#include <stdio.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include "xlivebg.h"

static int init(void *cls);
static void start(long tmsec, void *cls);
static void draw(long tmsec, void *cls);
static void prop(const char *prop, void *cls);

/* this is a convenient way to define a property list (see xlivebg_plugin
 * structure below). The property list is retrieved by the GUI configuration
 * tool, to generate UI elements for tweaking all the live wallpaper parameters.
 * In this case we're defining a property of type "number" that goes from 0 to
 * 100, and controls the animation speed.
 */
#define PROPLIST	\
	"proplist {\n" \
	"    prop {\n" \
	"        id = \"speed\"\n" \
	"        desc = \"animation speed\"\n" \
	"        type = \"number\"\n" \
	"        range = [0, 10]\n" \
	"    }\n" \
	"}\n"

static struct xlivebg_plugin plugin = {
	"minimal",
	"Minimal live wallpaper example",
	PROPLIST,
	XLIVEBG_20FPS,
	init, 0,
	start, 0,
	draw,
	prop,
	0, 0
};

static float speed;

int register_plugin(void)
{
	return xlivebg_register_plugin(&plugin);
}

static int init(void *cls)
{
	xlivebg_defcfg_num("xlivebg.minimal.speed", 1.0f);
	return 0;
}

static void start(long tmsec, void *cls)
{
	prop("speed", 0);
}

static void prop(const char *prop, void *cls)
{
	if(strcmp(prop, "speed") == 0) {
		speed = xlivebg_getcfg_num("xlivebg.minimal.speed", 1.0f);
	}
}

static void anim_color(float *col, float t)
{
	col[0] = sin(t) * 0.5f + 0.5f;
	col[1] = cos(t) * 0.5f + 0.5f;
	col[2] = -sin(t) * 0.5f + 0.5f;
}

static void draw(long tmsec, void *cls)
{
	int i, num_scr;
	struct xlivebg_image *img;
	float xform[16];  /* matrix for fiting the bg image to the screen correctly */
	float color[3];
	float t = (float)tmsec / 1000.0f;

	anim_color(color, t * speed);

	glClearColor(color[0], color[1], color[2], 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	/* use xlivebg_clear(GL_COLOR_BUFFER_BIT) instead of calling glClearColor/glClear
	 * directly, if you wish xlivebg to handle clearing the framebuffer according
	 * to the background color and mode (solid, h/v gradient) selected by the user.
	 */

	/* for every screen ... */
	num_scr = xlivebg_screen_count();
	for(i=0; i<num_scr; i++) {
		/* xlivebg_gl_viewport calls glViewport for the part of the desktop which
		 * corresponds to screen i
		 */
		xlivebg_gl_viewport(i);

		/* xlivebg_bg_image(i) returns a pointer to the currently selected background
		 * image (see struct xlivebg_image in xlivebg.h), or null if none is selected.
		 */
		if((img = xlivebg_bg_image(i)) && img->tex) {
			/* xlivebg_calc_image_proj is a helper, which calculates the projection matrix
			 * required to fit a quad defined in the range [-1, 1] in both axes, to the
			 * screen according to the "fit" setting selected by the user, and the
			 * aspect ratio of the image we're going to display.
			 */
			xlivebg_calc_image_proj(i, (float)img->width / img->height, xform);
			glMatrixMode(GL_PROJECTION);
			glLoadMatrixf(xform);

			/* bind the background image texture, and draw the texture-mapped quad */
			glBindTexture(GL_TEXTURE_2D, img->tex);
			glEnable(GL_TEXTURE_2D);

			glBegin(GL_QUADS);
			glColor3f(color[0], color[1], color[2]);
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
	}
}
