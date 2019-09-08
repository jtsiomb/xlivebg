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

static void draw(long tmsec, void *cls)
{
	glClearColor(1, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
}
