#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
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

static Display *dpy;

static int win_width = 512;
static int win_height = 512;

static utk_widget *root;
static utk_widget *win;

#define XFONT_PATTERN	"-*-helvetica-medium-r-*-*-18-*-*-*-*-*-*-*"
#define FIRST_GLYPH		((int)' ')
#define NUM_GLYPHS		((int)'~' - FIRST_GLYPH)
static XFontStruct *font;
static int font_dlist_base;


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
	glutPassiveMotionFunc(motion);

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
	utk_widget *vbox, *hbox;

	dpy = glutXDisplay();

	if(!(font = XLoadQueryFont(dpy, XFONT_PATTERN))) {
		fprintf(stderr, "failed to find font: " XFONT_PATTERN "\n");
		return -1;
	}
	font_dlist_base = glGenLists(NUM_GLYPHS);
	glXUseXFont(font->fid, FIRST_GLYPH, NUM_GLYPHS, font_dlist_base);

	root = utk_init(win_width, win_height);
	win = utk_create_window(root, 50, 50, win_width-100, win_height-100, "foo");
	utk_show(win);

	vbox = utk_create_vbox(win, UTK_DEF_PADDING, UTK_DEF_SPACING);
	utk_create_label(vbox, "a label");
	utk_create_label(vbox, "another label");

	utk_create_button(vbox, "a button", 0, 0, 0, 0);

	utk_print_widget_tree(root);

	glClearColor(1, 0, 0, 1);
	return 0;
}

static void cleanup(void)
{
	glDeleteLists(font_dlist_base, NUM_GLYPHS);
	XFreeFont(dpy, font);
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

	utk_keyboard_event(key, 1);
	glutPostRedisplay();
}

static void keyup(unsigned char key, int x, int y)
{
	utk_keyboard_event(key, 0);
	glutPostRedisplay();
}

static int glut_skey_to_keysym(int key)
{
	switch(key) {
	case GLUT_KEY_LEFT:
		return UTK_KEY_LEFT;

	case GLUT_KEY_RIGHT:
		return UTK_KEY_RIGHT;

	case GLUT_KEY_UP:
		return UTK_KEY_UP;

	case GLUT_KEY_DOWN:
		return UTK_KEY_DOWN;

	case GLUT_KEY_HOME:
		return UTK_KEY_HOME;

	case GLUT_KEY_END:
		return UTK_KEY_END;

	default:
		break;
	}

	return 0;
}

static void skeydown(int key, int x, int y)
{
	if(!(key = glut_skey_to_keysym(key))) return;
	utk_keyboard_event(key, 1);
	glutPostRedisplay();
}

static void skeyup(int key, int x, int y)
{
	if(!(key = glut_skey_to_keysym(key))) return;
	utk_keyboard_event(key, 0);
	glutPostRedisplay();
}

static void mouse(int bn, int st, int x, int y)
{
	utk_mbutton_event(bn, st == GLUT_DOWN ? 1 : 0, x, y);
	glutPostRedisplay();
}

static void motion(int x, int y)
{
	utk_mmotion_event(x, y);
	glutPostRedisplay();
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
	glRasterPos2f(CONVX(x), CONVY(y - font->descent));
	glListBase(font_dlist_base - FIRST_GLYPH);
	glCallLists(strlen(txt), GL_UNSIGNED_BYTE, txt);
}

static int text_spacing(void)
{
	return font->ascent + font->descent;
}

static int text_width(const char *txt, int sz)
{
	return XTextWidth(font, txt, strlen(txt));
}

