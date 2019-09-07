#include <GL/gl.h>
#include <GL/glu.h>
#include "app.h"


static void draw_cube(void);

static int win_width, win_height;


int app_init(int argc, char **argv)
{
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	return 0;
}

void app_cleanup(void)
{
}

void app_draw(void)
{
	float tm = (float)msec / 10.0f;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glViewport(0, 0, win_width, win_height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(50.0, (float)win_width / (float)win_height, 0.5, 500.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, 0, -5);

	glPushMatrix();
	glRotatef(tm, 1, 0, 0);
	glRotatef(tm, 0, 1, 0);
	draw_cube();
	glPopMatrix();
}

void app_reshape(int x, int y)
{
	win_width = x;
	win_height = y;
}

void app_keyboard(int key, int pressed)
{
	switch(key) {
	case 27:
		app_quit();
		break;
	}
}

static void draw_cube(void)
{
	glBegin(GL_QUADS);
	/* -Z */
	glColor3f(1, 0, 1);
	glVertex3f(1, -1, -1);
	glVertex3f(-1, -1, -1);
	glVertex3f(-1, 1, -1);
	glVertex3f(1, 1, -1);
	/* -Y */
	glColor3f(0, 1, 1);
	glVertex3f(-1, -1, -1);
	glVertex3f(1, -1, -1);
	glVertex3f(1, -1, 1);
	glVertex3f(-1, -1, 1);
	/* -X */
	glColor3f(1, 1, 0);
	glVertex3f(-1, -1, -1);
	glVertex3f(-1, -1, 1);
	glVertex3f(-1, 1, 1);
	glVertex3f(-1, 1, -1);
	/* +X */
	glColor3f(1, 0, 0);
	glVertex3f(1, -1, 1);
	glVertex3f(1, -1, -1);
	glVertex3f(1, 1, -1);
	glVertex3f(1, 1, 1);
	/* +Y */
	glColor3f(0, 1, 0);
	glVertex3f(-1, 1, 1);
	glVertex3f(1, 1, 1);
	glVertex3f(1, 1, -1);
	glVertex3f(-1, 1, -1);
	/* +Z */
	glColor3f(0, 0, 1);
	glVertex3f(-1, -1, 1);
	glVertex3f(1, -1, 1);
	glVertex3f(1, 1, 1);
	glVertex3f(-1, 1, 1);
	glEnd();
}
