#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utk/cubertk.h>
#include "miniglut.h"

static int init(void);
static void cleanup(void);
static void display(void);
static void reshape(int x, int y);
static void keydown(unsigned char key, int x, int y);
static void keyup(unsigned char key, int x, int y);
static void skeydown(int key, int x, int y);
static void skeyup(int key, int x, int y);
static void mouse(int bn, int st, int x, int y);
static void motion(int x, int y);

/* UTK drawing callbacks */
static void color(int r, int g, int b, int a);
static void clip(int x1, int y1, int x2, int y2);
static void image(int x, int y, const void *pix, int xsz, int ysz);
static void rect(int x1, int y1, int x2, int y2);
static void line(int x1, int y1, int x2, int y2, int width);
static void text(int x, int y, const char *txt, int sz);
static int text_spacing(void);
static int text_width(const char *txt, int sz);

static int win_width = 512;
static int win_height = 512;

static utk_widget *root;
static utk_widget *win;

int main(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitWindowSize(win_width, win_height);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutCreateWindow("xlivebg configuration");

	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keydown);
	glutKeyboardUpFunc(keyup);
	glutSpecialFunc(skeydown);
	glutSpecialUpFunc(skeyup);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);

	utk_set_color_func(color);
	utk_set_clip_func(clip);
	utk_set_image_func(image);
	utk_set_rect_func(rect);
	utk_set_line_func(line);
	utk_set_text_func(text);
	utk_set_text_spacing_func(text_spacing);
	utk_set_text_width_func(text_width);

	if(init() == -1) {
		return 1;
	}
	atexit(cleanup);

	glutMainLoop();
	return 0;
}

static int init(void)
{
	glClearColor(1, 0, 0, 1);

	root = utk_init(win_width, win_height);
	win = utk_create_window(root, 50, 50, win_width-100, win_height-100, "foo");
	utk_show(win);

	utk_print_widget_tree(UBERTK_PRINT_ROOT);
	return 0;
}

static void cleanup(void)
{
}

static void display(void)
{
	glClear(GL_COLOR_BUFFER_BIT);

	utk_draw(root);

	glutSwapBuffers();
}

static void reshape(int x, int y)
{
	win_width = x;
	win_height = y;
	glViewport(0, 0, x, y);
}

static void keydown(unsigned char key, int x, int y)
{
	if(key == 27) exit(0);
}

static void keyup(unsigned char key, int x, int y)
{
}

static void skeydown(int key, int x, int y)
{
}

static void skeyup(int key, int x, int y)
{
}

static void mouse(int bn, int st, int x, int y)
{
}

static void motion(int x, int y)
{
}

/* UTK callbacks */
#define CONVX(x)	(2.0 * (float)(x) / (float)win_width - 1.0)
#define CONVY(y)	(1.0 - 2.0 * (float)(y) / (float)win_height)

static void color(int r, int g, int b, int a)
{
	glColor4ub(r, g, b, a);
}

static void clip(int x1, int y1, int x2, int y2)
{
	if(x1 == x2 && y1 == y2 && x1 == y1 && x1 == 0) {
		glDisable(GL_SCISSOR_TEST);
	} else {
		glEnable(GL_SCISSOR_TEST);
	}
	glScissor(x1, win_height - y2, x2 - x1, y2 - y1);
}

static void image(int x, int y, const void *pix, int xsz, int ysz)
{
	glPixelZoom(1, -1);
	glRasterPos2f(CONVX(x), CONVY(y));
	glDrawPixels(xsz, ysz, GL_BGRA, GL_UNSIGNED_BYTE, pix);
}

static void rect(int x1, int y1, int x2, int y2)
{
	glRectf(CONVX(x1), CONVY(y1), CONVX(x2), CONVY(y2));
}

static void line(int x1, int y1, int x2, int y2, int width)
{
	glLineWidth((float)width);
	glBegin(GL_LINES);
	glVertex2f(CONVX(x1), CONVY(y1));
	glVertex2f(CONVX(x2), CONVY(y2));
	glEnd();
}

static void text(int x, int y, const char *txt, int sz)
{
}

static int text_spacing(void)
{
	return 10;
}

static int text_width(const char *txt, int sz)
{
	return 10 * strlen(txt);
}

