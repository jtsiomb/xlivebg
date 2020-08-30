/*
MiniGLUT - minimal GLUT subset without dependencies
Copyright (C) 2020  John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#if defined(__unix__)

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <GL/glx.h>
#define BUILD_X11

#ifndef GLX_SAMPLE_BUFFERS_ARB
#define GLX_SAMPLE_BUFFERS_ARB	100000
#define GLX_SAMPLES_ARB			100001
#endif
#ifndef GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB
#define GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB	0x20b2
#endif

static Display *dpy;
static Window win, root;
static int scr;
static GLXContext ctx;
static Atom xa_wm_proto, xa_wm_del_win;
static Atom xa_net_wm_state, xa_net_wm_state_fullscr;
static Atom xa_motif_wm_hints;
static Atom xa_motion_event, xa_button_press_event, xa_button_release_event, xa_command_event;
static unsigned int evmask;
static Cursor blank_cursor;

static int have_netwm_fullscr(void);

#elif defined(_WIN32)

#include <windows.h>
#define BUILD_WIN32

static HRESULT CALLBACK handle_message(HWND win, unsigned int msg, WPARAM wparam, LPARAM lparam);

static HINSTANCE hinst;
static HWND win;
static HDC dc;
static HGLRC ctx;

#else
#error unsupported platform
#endif
#include <GL/gl.h>
#include "miniglut.h"

struct ctx_info {
	int rsize, gsize, bsize, asize;
	int zsize, ssize;
	int dblbuf;
	int samples;
	int stereo;
	int srgb;
};

static void cleanup(void);
static void create_window(const char *title);
static void get_window_pos(int *x, int *y);
static void get_window_size(int *w, int *h);
static void get_screen_size(int *scrw, int *scrh);

static long get_msec(void);
static void panic(const char *msg);
static void sys_exit(int status);
static int sys_write(int fd, const void *buf, int count);


static int init_x = -1, init_y, init_width = 256, init_height = 256;
static unsigned int init_mode;

static struct ctx_info ctx_info;
static int cur_cursor = GLUT_CURSOR_INHERIT;

static glut_cb cb_display;
static glut_cb cb_idle;
static glut_cb_reshape cb_reshape;
static glut_cb_state cb_vis, cb_entry;
static glut_cb_keyb cb_keydown, cb_keyup;
static glut_cb_special cb_skeydown, cb_skeyup;
static glut_cb_mouse cb_mouse;
static glut_cb_motion cb_motion, cb_passive;
static glut_cb_sbmotion cb_sball_motion, cb_sball_rotate;
static glut_cb_sbbutton cb_sball_button;

static int fullscreen;
static int prev_win_x, prev_win_y, prev_win_width, prev_win_height;

static int win_width, win_height;
static int mapped;
static int quit;
static int upd_pending;
static int modstate;

void glutInit(int *argc, char **argv)
{
#ifdef BUILD_X11
	Pixmap blankpix = 0;
	XColor xcol;

	if(!(dpy = XOpenDisplay(0))) {
		panic("Failed to connect to the X server\n");
	}
	scr = DefaultScreen(dpy);
	root = RootWindow(dpy, scr);
	xa_wm_proto = XInternAtom(dpy, "WM_PROTOCOLS", False);
	xa_wm_del_win = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	xa_motif_wm_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
	if(have_netwm_fullscr()) {
		xa_net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
		xa_net_wm_state_fullscr = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	}

	xa_motion_event = XInternAtom(dpy, "MotionEvent", True);
	xa_button_press_event = XInternAtom(dpy, "ButtonPressEvent", True);
	xa_button_release_event = XInternAtom(dpy, "ButtonReleaseEvent", True);
	xa_command_event = XInternAtom(dpy, "CommandEvent", True);

	evmask = ExposureMask | StructureNotifyMask;

	if((blankpix = XCreateBitmapFromData(dpy, root, (char*)&blankpix, 1, 1))) {
		blank_cursor = XCreatePixmapCursor(dpy, blankpix, blankpix, &xcol, &xcol, 0, 0);
		XFreePixmap(dpy, blankpix);
	}

#endif
#ifdef BUILD_WIN32
	WNDCLASSEX wc = {0};

	hinst = GetModuleHandle(0);

	wc.cbSize = sizeof wc;
	wc.hbrBackground = GetStockObject(BLACK_BRUSH);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hIcon = wc.hIconSm = LoadIcon(0, IDI_APPLICATION);
	wc.hInstance = hinst;
	wc.lpfnWndProc = handle_message;
	wc.lpszClassName = "MiniGLUT";
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	if(!RegisterClassEx(&wc)) {
		panic("Failed to register \"MiniGLUT\" window class\n");
	}

	if(init_x == -1) {
		get_screen_size(&init_x, &init_y);
		init_x >>= 3;
		init_y >>= 3;
	}
#endif
}

void glutInitWindowPosition(int x, int y)
{
	init_x = x;
	init_y = y;
}

void glutInitWindowSize(int xsz, int ysz)
{
	init_width = xsz;
	init_height = ysz;
}

void glutInitDisplayMode(unsigned int mode)
{
	init_mode = mode;
}

void glutCreateWindow(const char *title)
{
	create_window(title);
}

void glutExit(void)
{
	quit = 1;
}

void glutMainLoop(void)
{
	while(!quit) {
		glutMainLoopEvent();
	}
}

void glutPostRedisplay(void)
{
	upd_pending = 1;
}

#define UPD_EVMASK(x) \
	do { \
		if(func) { \
			evmask |= x; \
		} else { \
			evmask &= ~(x); \
		} \
		if(win) XSelectInput(dpy, win, evmask); \
	} while(0)


void glutIdleFunc(glut_cb func)
{
	cb_idle = func;
}

void glutDisplayFunc(glut_cb func)
{
	cb_display = func;
}

void glutReshapeFunc(glut_cb_reshape func)
{
	cb_reshape = func;
}

void glutVisibilityFunc(glut_cb_state func)
{
	cb_vis = func;
#ifdef BUILD_X11
	UPD_EVMASK(VisibilityChangeMask);
#endif
}

void glutEntryFunc(glut_cb_state func)
{
	cb_entry = func;
#ifdef BUILD_X11
	UPD_EVMASK(EnterWindowMask | LeaveWindowMask);
#endif
}

void glutKeyboardFunc(glut_cb_keyb func)
{
	cb_keydown = func;
#ifdef BUILD_X11
	UPD_EVMASK(KeyPressMask);
#endif
}

void glutKeyboardUpFunc(glut_cb_keyb func)
{
	cb_keyup = func;
#ifdef BUILD_X11
	UPD_EVMASK(KeyReleaseMask);
#endif
}

void glutSpecialFunc(glut_cb_special func)
{
	cb_skeydown = func;
#ifdef BUILD_X11
	UPD_EVMASK(KeyPressMask);
#endif
}

void glutSpecialUpFunc(glut_cb_special func)
{
	cb_skeyup = func;
#ifdef BUILD_X11
	UPD_EVMASK(KeyReleaseMask);
#endif
}

void glutMouseFunc(glut_cb_mouse func)
{
	cb_mouse = func;
#ifdef BUILD_X11
	UPD_EVMASK(ButtonPressMask | ButtonReleaseMask);
#endif
}

void glutMotionFunc(glut_cb_motion func)
{
	cb_motion = func;
#ifdef BUILD_X11
	UPD_EVMASK(ButtonMotionMask);
#endif
}

void glutPassiveMotionFunc(glut_cb_motion func)
{
	cb_passive = func;
#ifdef BUILD_X11
	UPD_EVMASK(PointerMotionMask);
#endif
}

void glutSpaceballMotionFunc(glut_cb_sbmotion func)
{
	cb_sball_motion = func;
}

void glutSpaceballRotateFunc(glut_cb_sbmotion func)
{
	cb_sball_rotate = func;
}

void glutSpaceballButtonFunc(glut_cb_sbbutton func)
{
	cb_sball_button = func;
}

int glutGet(unsigned int s)
{
	int x, y;
	switch(s) {
	case GLUT_WINDOW_X:
		get_window_pos(&x, &y);
		return x;
	case GLUT_WINDOW_Y:
		get_window_pos(&x, &y);
		return y;
	case GLUT_WINDOW_WIDTH:
		get_window_size(&x, &y);
		return x;
	case GLUT_WINDOW_HEIGHT:
		get_window_size(&x, &y);
		return y;
	case GLUT_WINDOW_BUFFER_SIZE:
		return ctx_info.rsize + ctx_info.gsize + ctx_info.bsize + ctx_info.asize;
	case GLUT_WINDOW_STENCIL_SIZE:
		return ctx_info.ssize;
	case GLUT_WINDOW_DEPTH_SIZE:
		return ctx_info.zsize;
	case GLUT_WINDOW_RED_SIZE:
		return ctx_info.rsize;
	case GLUT_WINDOW_GREEN_SIZE:
		return ctx_info.gsize;
	case GLUT_WINDOW_BLUE_SIZE:
		return ctx_info.bsize;
	case GLUT_WINDOW_ALPHA_SIZE:
		return ctx_info.asize;
	case GLUT_WINDOW_DOUBLEBUFFER:
		return ctx_info.dblbuf;
	case GLUT_WINDOW_RGBA:
		return 1;
	case GLUT_WINDOW_NUM_SAMPLES:
		return ctx_info.samples;
	case GLUT_WINDOW_STEREO:
		return ctx_info.stereo;
	case GLUT_WINDOW_SRGB:
		return ctx_info.srgb;
	case GLUT_WINDOW_CURSOR:
		return cur_cursor;
	case GLUT_SCREEN_WIDTH:
		get_screen_size(&x, &y);
		return x;
	case GLUT_SCREEN_HEIGHT:
		get_screen_size(&x, &y);
		return y;
	case GLUT_INIT_DISPLAY_MODE:
		return init_mode;
	case GLUT_INIT_WINDOW_X:
		return init_x;
	case GLUT_INIT_WINDOW_Y:
		return init_y;
	case GLUT_INIT_WINDOW_WIDTH:
		return init_width;
	case GLUT_INIT_WINDOW_HEIGHT:
		return init_height;
	case GLUT_ELAPSED_TIME:
		return get_msec();
	default:
		break;
	}
	return 0;
}

int glutGetModifiers(void)
{
	return modstate;
}

static int is_space(int c)
{
	return c == ' ' || c == '\t' || c == '\v' || c == '\n' || c == '\r';
}

static const char *skip_space(const char *s)
{
	while(*s && is_space(*s)) s++;
	return s;
}

int glutExtensionSupported(char *ext)
{
	const char *str, *eptr;

	if(!(str = (const char*)glGetString(GL_EXTENSIONS))) {
		return 0;
	}

	while(*str) {
		str = skip_space(str);
		eptr = skip_space(ext);
		while(*str && !is_space(*str) && *eptr && *str == *eptr) {
			str++;
			eptr++;
		}
		if((!*str || is_space(*str)) && !*eptr) {
			return 1;
		}
		while(*str && !is_space(*str)) str++;
	}

	return 0;
}


/* --------------- UNIX/X11 implementation ----------------- */
#ifdef BUILD_X11
enum {
    SPNAV_EVENT_ANY,  /* used by spnav_remove_events() */
    SPNAV_EVENT_MOTION,
    SPNAV_EVENT_BUTTON  /* includes both press and release */
};

struct spnav_event_motion {
    int type;
    int x, y, z;
    int rx, ry, rz;
    unsigned int period;
    int *data;
};

struct spnav_event_button {
    int type;
    int press;
    int bnum;
};

union spnav_event {
    int type;
    struct spnav_event_motion motion;
    struct spnav_event_button button;
};


static void handle_event(XEvent *ev);

static int spnav_window(Window win);
static int spnav_event(const XEvent *xev, union spnav_event *event);
static int spnav_remove_events(int type);


void glutMainLoopEvent(void)
{
	XEvent ev;

	if(!cb_display) {
		panic("display callback not set");
	}

	if(!upd_pending && !cb_idle) {
		XNextEvent(dpy, &ev);
		handle_event(&ev);
		if(quit) goto end;
	}
	while(XPending(dpy)) {
		XNextEvent(dpy, &ev);
		handle_event(&ev);
		if(quit) goto end;
	}

	if(cb_idle) {
		cb_idle();
	}

	if(upd_pending && mapped) {
		upd_pending = 0;
		cb_display();
	}

end:
	if(quit) {
		cleanup();
	}
}

static void cleanup(void)
{
	if(win) {
		spnav_window(root);
		glXMakeCurrent(dpy, 0, 0);
		XDestroyWindow(dpy, win);
	}
}

static KeySym translate_keysym(KeySym sym)
{
	switch(sym) {
	case XK_Escape:
		return 27;
	case XK_BackSpace:
		return '\b';
	case XK_Linefeed:
		return '\r';
	case XK_Return:
		return '\n';
	case XK_Delete:
		return 127;
	case XK_Tab:
		return '\t';
	default:
		break;
	}
	return sym;
}

static void handle_event(XEvent *ev)
{
	KeySym sym;
	union spnav_event sev;

	switch(ev->type) {
	case MapNotify:
		mapped = 1;
		break;
	case UnmapNotify:
		mapped = 0;
		break;
	case ConfigureNotify:
		if(cb_reshape && (ev->xconfigure.width != win_width || ev->xconfigure.height != win_height)) {
			win_width = ev->xconfigure.width;
			win_height = ev->xconfigure.height;
			cb_reshape(ev->xconfigure.width, ev->xconfigure.height);
		}
		break;

	case ClientMessage:
		if(ev->xclient.message_type == xa_wm_proto) {
			if(ev->xclient.data.l[0] == xa_wm_del_win) {
				quit = 1;
			}
		}
		if(spnav_event(ev, &sev)) {
			switch(sev.type) {
			case SPNAV_EVENT_MOTION:
				if(cb_sball_motion) {
					cb_sball_motion(sev.motion.x, sev.motion.y, sev.motion.z);
				}
				if(cb_sball_rotate) {
					cb_sball_rotate(sev.motion.rx, sev.motion.ry, sev.motion.rz);
				}
				spnav_remove_events(SPNAV_EVENT_MOTION);
				break;

			case SPNAV_EVENT_BUTTON:
				if(cb_sball_button) {
					cb_sball_button(sev.button.bnum + 1, sev.button.press ? GLUT_DOWN : GLUT_UP);
				}
				break;

			default:
				break;
			}
		}
		break;

	case Expose:
		upd_pending = 1;
		break;

	case KeyPress:
	case KeyRelease:
		modstate = ev->xkey.state & (ShiftMask | ControlMask | Mod1Mask);
		if(!(sym = XLookupKeysym(&ev->xkey, 0))) {
			break;
		}
		sym = translate_keysym(sym);
		if(sym < 256) {
			if(ev->type == KeyPress) {
				if(cb_keydown) cb_keydown((unsigned char)sym, ev->xkey.x, ev->xkey.y);
			} else {
				if(cb_keyup) cb_keyup((unsigned char)sym, ev->xkey.x, ev->xkey.y);
			}
		} else {
			if(ev->type == KeyPress) {
				if(cb_skeydown) cb_skeydown(sym, ev->xkey.x, ev->xkey.y);
			} else {
				if(cb_skeyup) cb_skeyup(sym, ev->xkey.x, ev->xkey.y);
			}
		}
		break;

	case ButtonPress:
	case ButtonRelease:
		modstate = ev->xbutton.state & (ShiftMask | ControlMask | Mod1Mask);
		if(cb_mouse) {
			int bn = ev->xbutton.button - Button1;
			cb_mouse(bn, ev->type == ButtonPress ? GLUT_DOWN : GLUT_UP,
					ev->xbutton.x, ev->xbutton.y);
		}
		break;

	case MotionNotify:
		if(ev->xmotion.state & (Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask)) {
			if(cb_motion) cb_motion(ev->xmotion.x, ev->xmotion.y);
		} else {
			if(cb_passive) cb_passive(ev->xmotion.x, ev->xmotion.y);
		}
		break;

	case VisibilityNotify:
		if(cb_vis) {
			cb_vis(ev->xvisibility.state == VisibilityFullyObscured ? GLUT_NOT_VISIBLE : GLUT_VISIBLE);
		}
		break;
	case EnterNotify:
		if(cb_entry) cb_entry(GLUT_ENTERED);
		break;
	case LeaveNotify:
		if(cb_entry) cb_entry(GLUT_LEFT);
		break;
	}
}

void glutSwapBuffers(void)
{
	glXSwapBuffers(dpy, win);
}

/* BUG:
 * set_fullscreen_mwm removes the decorations with MotifWM hints, and then it
 * needs to resize the window to make it fullscreen. The way it does this is by
 * querying the size of the root window (see get_screen_size), which in the
 * case of multi-monitor setups will be the combined size of all monitors.
 * This is problematic; the way to solve it is to use the XRandR extension, or
 * the Xinerama extension, to figure out the dimensions of the correct video
 * output, which would add potentially two extension support libraries to our
 * dependencies list.
 * Moreover, any X installation modern enough to support XR&R will almost
 * certainly be running a window manager supporting the EHWM
 * _NET_WM_STATE_FULLSCREEN method (set_fullscreen_ewmh), which does not rely
 * on manual resizing, and is used in preference if available, making this
 * whole endeavor pointless.
 * So I'll just leave it with set_fullscreen_mwm covering the entire
 * multi-monitor area for now.
 */

struct mwm_hints {
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long input_mode;
	unsigned long status;
};

#define MWM_HINTS_DECORATIONS	2
#define MWM_DECOR_ALL			1

static void set_fullscreen_mwm(int fs)
{
	struct mwm_hints hints;
	int scr_width, scr_height;

	if(fs) {
		get_window_pos(&prev_win_x, &prev_win_y);
		get_window_size(&prev_win_width, &prev_win_height);
		get_screen_size(&scr_width, &scr_height);

		hints.decorations = 0;
		hints.flags = MWM_HINTS_DECORATIONS;
		XChangeProperty(dpy, win, xa_motif_wm_hints, xa_motif_wm_hints, 32,
				PropModeReplace, (unsigned char*)&hints, 5);

		XMoveResizeWindow(dpy, win, 0, 0, scr_width, scr_height);
	} else {
		XDeleteProperty(dpy, win, xa_motif_wm_hints);
		XMoveResizeWindow(dpy, win, prev_win_x, prev_win_y, prev_win_width, prev_win_height);
	}
}

static int have_netwm_fullscr(void)
{
	int fmt;
	long offs = 0;
	unsigned long i, count, rem;
	Atom prop[8], type;
	Atom xa_net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);

	do {
		XGetWindowProperty(dpy, root, xa_net_supported, offs, 8, False, AnyPropertyType,
				&type, &fmt, &count, &rem, (unsigned char**)prop);

		for(i=0; i<count; i++) {
			if(prop[i] == xa_net_wm_state_fullscr) {
				return 1;
			}
		}
		offs += count;
	} while(rem > 0);

	return 0;
}

static void set_fullscreen_ewmh(int fs)
{
	XClientMessageEvent msg = {0};

	msg.type = ClientMessage;
	msg.window = win;
	msg.message_type = xa_net_wm_state;	/* _NET_WM_STATE */
	msg.format = 32;
	msg.data.l[0] = fs ? 1 : 0;
	msg.data.l[1] = xa_net_wm_state_fullscr;	/* _NET_WM_STATE_FULLSCREEN */
	msg.data.l[2] = 0;
	msg.data.l[3] = 1;	/* source regular application */
	XSendEvent(dpy, root, False, SubstructureNotifyMask | SubstructureRedirectMask, (XEvent*)&msg);
}

static void set_fullscreen(int fs)
{
	if(fullscreen == fs) return;

	if(xa_net_wm_state && xa_net_wm_state_fullscr) {
		set_fullscreen_ewmh(fs);
		fullscreen = fs;
	} else if(xa_motif_wm_hints) {
		set_fullscreen_mwm(fs);
		fullscreen = fs;
	}
}

void glutPositionWindow(int x, int y)
{
	set_fullscreen(0);
	XMoveWindow(dpy, win, x, y);
}

void glutReshapeWindow(int xsz, int ysz)
{
	set_fullscreen(0);
	XResizeWindow(dpy, win, xsz, ysz);
}

void glutFullScreen(void)
{
	set_fullscreen(1);
}

void glutSetWindowTitle(const char *title)
{
	XTextProperty tprop;
	if(!XStringListToTextProperty((char**)&title, 1, &tprop)) {
		return;
	}
	XSetWMName(dpy, win, &tprop);
	XFree(tprop.value);
}

void glutSetIconTitle(const char *title)
{
	XTextProperty tprop;
	if(!XStringListToTextProperty((char**)&title, 1, &tprop)) {
		return;
	}
	XSetWMIconName(dpy, win, &tprop);
	XFree(tprop.value);
}

void glutSetCursor(int cidx)
{
	Cursor cur = None;

	switch(cidx) {
	case GLUT_CURSOR_LEFT_ARROW:
		cur = XCreateFontCursor(dpy, XC_left_ptr);
		break;
	case GLUT_CURSOR_INHERIT:
		break;
	case GLUT_CURSOR_NONE:
		cur = blank_cursor;
		break;
	default:
		return;
	}

	XDefineCursor(dpy, win, cur);
	cur_cursor = cidx;
}

static XVisualInfo *choose_visual(unsigned int mode)
{
	XVisualInfo *vi;
	int attr[32];
	int *aptr = attr;
	int *samples = 0;

	if(mode & GLUT_DOUBLE) {
		*aptr++ = GLX_DOUBLEBUFFER;
	}

	if(mode & GLUT_INDEX) {
		*aptr++ = GLX_BUFFER_SIZE;
		*aptr++ = 1;
	} else {
		*aptr++ = GLX_RGBA;
		*aptr++ = GLX_RED_SIZE; *aptr++ = 4;
		*aptr++ = GLX_GREEN_SIZE; *aptr++ = 4;
		*aptr++ = GLX_BLUE_SIZE; *aptr++ = 4;
	}
	if(mode & GLUT_ALPHA) {
		*aptr++ = GLX_ALPHA_SIZE;
		*aptr++ = 4;
	}
	if(mode & GLUT_DEPTH) {
		*aptr++ = GLX_DEPTH_SIZE;
		*aptr++ = 16;
	}
	if(mode & GLUT_STENCIL) {
		*aptr++ = GLX_STENCIL_SIZE;
		*aptr++ = 1;
	}
	if(mode & GLUT_ACCUM) {
		*aptr++ = GLX_ACCUM_RED_SIZE; *aptr++ = 1;
		*aptr++ = GLX_ACCUM_GREEN_SIZE; *aptr++ = 1;
		*aptr++ = GLX_ACCUM_BLUE_SIZE; *aptr++ = 1;
	}
	if(mode & GLUT_STEREO) {
		*aptr++ = GLX_STEREO;
	}
	if(mode & GLUT_SRGB) {
		*aptr++ = GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB;
	}
	if(mode & GLUT_MULTISAMPLE) {
		*aptr++ = GLX_SAMPLE_BUFFERS_ARB;
		*aptr++ = 1;
		*aptr++ = GLX_SAMPLES_ARB;
		samples = aptr;
		*aptr++ = 32;
	}
	*aptr++ = None;

	if(!samples) {
		return glXChooseVisual(dpy, scr, attr);
	}
	while(!(vi = glXChooseVisual(dpy, scr, attr)) && *samples) {
		*samples >>= 1;
		if(!*samples) {
			aptr[-3] = None;
		}
	}
	return vi;
}

static void create_window(const char *title)
{
	XSetWindowAttributes xattr = {0};
	XVisualInfo *vi;
	unsigned int xattr_mask;
	unsigned int mode = init_mode;

	if(!(vi = choose_visual(mode))) {
		mode &= ~GLUT_SRGB;
		if(!(vi = choose_visual(mode))) {
			panic("Failed to find compatible visual\n");
		}
	}

	if(!(ctx = glXCreateContext(dpy, vi, 0, True))) {
		XFree(vi);
		panic("Failed to create OpenGL context\n");
	}

	glXGetConfig(dpy, vi, GLX_RED_SIZE, &ctx_info.rsize);
	glXGetConfig(dpy, vi, GLX_GREEN_SIZE, &ctx_info.gsize);
	glXGetConfig(dpy, vi, GLX_BLUE_SIZE, &ctx_info.bsize);
	glXGetConfig(dpy, vi, GLX_ALPHA_SIZE, &ctx_info.asize);
	glXGetConfig(dpy, vi, GLX_DEPTH_SIZE, &ctx_info.zsize);
	glXGetConfig(dpy, vi, GLX_STENCIL_SIZE, &ctx_info.ssize);
	glXGetConfig(dpy, vi, GLX_DOUBLEBUFFER, &ctx_info.dblbuf);
	glXGetConfig(dpy, vi, GLX_STEREO, &ctx_info.stereo);
	glXGetConfig(dpy, vi, GLX_SAMPLES_ARB, &ctx_info.samples);
	glXGetConfig(dpy, vi, GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB, &ctx_info.srgb);

	xattr.background_pixel = BlackPixel(dpy, scr);
	xattr.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
	xattr_mask = CWBackPixel | CWColormap | CWBackPixmap | CWBorderPixel;
	if(!(win = XCreateWindow(dpy, root, init_x, init_y, init_width, init_height, 0,
			vi->depth, InputOutput, vi->visual, xattr_mask, &xattr))) {
		XFree(vi);
		glXDestroyContext(dpy, ctx);
		panic("Failed to create window\n");
	}
	XFree(vi);

	XSelectInput(dpy, win, evmask);

	spnav_window(win);

	glutSetWindowTitle(title);
	glutSetIconTitle(title);
	XSetWMProtocols(dpy, win, &xa_wm_del_win, 1);
	XMapWindow(dpy, win);

	glXMakeCurrent(dpy, win, ctx);
}

static void get_window_pos(int *x, int *y)
{
	Window child;
	XTranslateCoordinates(dpy, win, root, 0, 0, x, y, &child);
}

static void get_window_size(int *w, int *h)
{
	XWindowAttributes wattr;
	XGetWindowAttributes(dpy, win, &wattr);
	*w = wattr.width;
	*h = wattr.height;
}

static void get_screen_size(int *scrw, int *scrh)
{
	XWindowAttributes wattr;
	XGetWindowAttributes(dpy, root, &wattr);
	*scrw = wattr.width;
	*scrh = wattr.height;
}


/* spaceball */
enum {
  CMD_APP_WINDOW = 27695,
  CMD_APP_SENS
};

static Window get_daemon_window(Display *dpy);
static int catch_badwin(Display *dpy, XErrorEvent *err);

#define SPNAV_INITIALIZED	(xa_motion_event)

static int spnav_window(Window win)
{
	int (*prev_xerr_handler)(Display*, XErrorEvent*);
	XEvent xev;
	Window daemon_win;

	if(!SPNAV_INITIALIZED) {
		return -1;
	}

	if(!(daemon_win = get_daemon_window(dpy))) {
		return -1;
	}

	prev_xerr_handler = XSetErrorHandler(catch_badwin);

	xev.type = ClientMessage;
	xev.xclient.send_event = False;
	xev.xclient.display = dpy;
	xev.xclient.window = win;
	xev.xclient.message_type = xa_command_event;
	xev.xclient.format = 16;
	xev.xclient.data.s[0] = ((unsigned int)win & 0xffff0000) >> 16;
	xev.xclient.data.s[1] = (unsigned int)win & 0xffff;
	xev.xclient.data.s[2] = CMD_APP_WINDOW;

	XSendEvent(dpy, daemon_win, False, 0, &xev);
	XSync(dpy, False);

	XSetErrorHandler(prev_xerr_handler);
	return 0;
}

static Bool match_events(Display *dpy, XEvent *xev, char *arg)
{
	int evtype = *(int*)arg;

	if(xev->type != ClientMessage) {
		return False;
	}

	if(xev->xclient.message_type == xa_motion_event) {
		return !evtype || evtype == SPNAV_EVENT_MOTION ? True : False;
	}
	if(xev->xclient.message_type == xa_button_press_event ||
			xev->xclient.message_type == xa_button_release_event) {
		return !evtype || evtype == SPNAV_EVENT_BUTTON ? True : False;
	}
	return False;
}

static int spnav_remove_events(int type)
{
	int rm_count = 0;
	XEvent xev;
	while(XCheckIfEvent(dpy, &xev, match_events, (char*)&type)) {
		rm_count++;
	}
	return rm_count;
}

static int spnav_event(const XEvent *xev, union spnav_event *event)
{
	int i;
	int xmsg_type;

	xmsg_type = xev->xclient.message_type;

	if(xmsg_type != xa_motion_event && xmsg_type != xa_button_press_event &&
			xmsg_type != xa_button_release_event) {
		return 0;
	}

	if(xmsg_type == xa_motion_event) {
		event->type = SPNAV_EVENT_MOTION;
		event->motion.data = &event->motion.x;

		for(i=0; i<6; i++) {
			event->motion.data[i] = xev->xclient.data.s[i + 2];
		}
		event->motion.period = xev->xclient.data.s[8];
	} else {
		event->type = SPNAV_EVENT_BUTTON;
		event->button.press = xmsg_type == xa_button_press_event ? 1 : 0;
		event->button.bnum = xev->xclient.data.s[2];
	}
	return event->type;
}

static int mglut_strcmp(const char *s1, const char *s2)
{
	while(*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	return *s1 - *s2;
}

static Window get_daemon_window(Display *dpy)
{
	Window win;
	XTextProperty wname;
	Atom type;
	int fmt;
	unsigned long nitems, bytes_after;
	unsigned char *prop;

	XGetWindowProperty(dpy, root, xa_command_event, 0, 1, False, AnyPropertyType,
			&type, &fmt, &nitems, &bytes_after, &prop);
	if(!prop) {
		return 0;
	}

	win = *(Window*)prop;
	XFree(prop);

	if(!XGetWMName(dpy, win, &wname) || mglut_strcmp("Magellan Window", (char*)wname.value) != 0) {
		return 0;
	}

	return win;
}

static int catch_badwin(Display *dpy, XErrorEvent *err)
{
	return 0;
}



#endif	/* BUILD_X11 */


/* --------------- windows implementation ----------------- */
#ifdef BUILD_WIN32
static int reshape_pending;

static void update_modkeys(void);
static int translate_vkey(int vkey);
static void handle_mbutton(int bn, int st, WPARAM wparam, LPARAM lparam);

#ifdef MINIGLUT_WINMAIN
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hprev, char *cmdline, int showcmd)
{
	int argc = 1;
	char *argv[] = { "miniglut.exe", 0 };
	return main(argc, argv);
}
#endif

void glutMainLoopEvent(void)
{
	MSG msg;

	if(!cb_display) {
		panic("display callback not set");
	}

	if(reshape_pending && cb_reshape) {
		reshape_pending = 0;
		get_window_size(&win_width, &win_height);
		cb_reshape(win_width, win_height);
	}

	if(!upd_pending && !cb_idle) {
		GetMessage(&msg, 0, 0, 0);
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if(quit) return;
	}
	while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if(quit) return;
	}

	if(cb_idle) {
		cb_idle();
	}

	if(upd_pending && mapped) {
		upd_pending = 0;
		cb_display();
	}
}

static void cleanup(void)
{
	if(win) {
		wglMakeCurrent(dc, 0);
		wglDeleteContext(ctx);
		UnregisterClass("MiniGLUT", hinst);
	}
}

void glutSwapBuffers(void)
{
	SwapBuffers(dc);
}

void glutPositionWindow(int x, int y)
{
	RECT rect;
	unsigned int flags = SWP_SHOWWINDOW;

	if(fullscreen) {
		rect.left = prev_win_x;
		rect.top = prev_win_y;
		rect.right = rect.left + prev_win_width;
		rect.bottom = rect.top + prev_win_height;
		SetWindowLong(win, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		fullscreen = 0;
		flags |= SWP_FRAMECHANGED;
	} else {
		GetWindowRect(win, &rect);
	}
	SetWindowPos(win, HWND_NOTOPMOST, x, y, rect.right - rect.left, rect.bottom - rect.top, flags);
}

void glutReshapeWindow(int xsz, int ysz)
{
	RECT rect;
	unsigned int flags = SWP_SHOWWINDOW;

	if(fullscreen) {
		rect.left = prev_win_x;
		rect.top = prev_win_y;
		SetWindowLong(win, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		fullscreen = 0;
		flags |= SWP_FRAMECHANGED;
	} else {
		GetWindowRect(win, &rect);
	}
	SetWindowPos(win, HWND_NOTOPMOST, rect.left, rect.top, xsz, ysz, flags);
}

void glutFullScreen(void)
{
	RECT rect;
	int scr_width, scr_height;

	if(fullscreen) return;

	GetWindowRect(win, &rect);
	prev_win_x = rect.left;
	prev_win_y = rect.top;
	prev_win_width = rect.right - rect.left;
	prev_win_height = rect.bottom - rect.top;

	get_screen_size(&scr_width, &scr_height);

	SetWindowLong(win, GWL_STYLE, 0);
	SetWindowPos(win, HWND_TOPMOST, 0, 0, scr_width, scr_height, SWP_SHOWWINDOW);

	fullscreen = 1;
}

void glutSetWindowTitle(const char *title)
{
	SetWindowText(win, title);
}

void glutSetIconTitle(const char *title)
{
}

void glutSetCursor(int cidx)
{
	switch(cidx) {
	case GLUT_CURSOR_NONE:
		ShowCursor(0);
		break;
	case GLUT_CURSOR_INHERIT:
	case GLUT_CURSOR_LEFT_ARROW:
	default:
		SetCursor(LoadCursor(0, IDC_ARROW));
		ShowCursor(1);
	}
}

#define WGL_DRAW_TO_WINDOW	0x2001
#define WGL_SUPPORT_OPENGL	0x2010
#define WGL_DOUBLE_BUFFER	0x2011
#define WGL_STEREO			0x2012
#define WGL_PIXEL_TYPE		0x2013
#define WGL_COLOR_BITS		0x2014
#define WGL_RED_BITS		0x2015
#define WGL_GREEN_BITS		0x2017
#define WGL_BLUE_BITS		0x2019
#define WGL_ALPHA_BITS		0x201b
#define WGL_ACCUM_BITS		0x201d
#define WGL_DEPTH_BITS		0x2022
#define WGL_STENCIL_BITS	0x2023

#define WGL_TYPE_RGBA		0x202b
#define WGL_TYPE_COLORINDEX	0x202c

#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB	0x20a9
#define WGL_SAMPLE_BUFFERS_ARB				0x2041
#define WGL_SAMPLES_ARB						0x2042

static PROC wglChoosePixelFormat;
static PROC wglGetPixelFormatAttribiv;

#define ATTR(a, v) \
	do { *aptr++ = (a); *aptr++ = (v); } while(0)

static unsigned int choose_pixfmt(unsigned int mode)
{
	unsigned int num_pixfmt, pixfmt = 0;
	int attr[32] = { WGL_DRAW_TO_WINDOW, 1, WGL_SUPPORT_OPENGL, 1 };

	int *aptr = attr;
	int *samples = 0;

	if(mode & GLUT_DOUBLE) {
		ATTR(WGL_DOUBLE_BUFFER, 1);
	}

	ATTR(WGL_PIXEL_TYPE, mode & GLUT_INDEX ? WGL_TYPE_COLORINDEX : WGL_TYPE_RGBA);
	ATTR(WGL_COLOR_BITS, 8);
	if(mode & GLUT_ALPHA) {
		ATTR(WGL_ALPHA_BITS, 4);
	}
	if(mode & GLUT_DEPTH) {
		ATTR(WGL_DEPTH_BITS, 16);
	}
	if(mode & GLUT_STENCIL) {
		ATTR(WGL_STENCIL_BITS, 1);
	}
	if(mode & GLUT_ACCUM) {
		ATTR(WGL_ACCUM_BITS, 1);
	}
	if(mode & GLUT_STEREO) {
		ATTR(WGL_STEREO, 1);
	}
	if(mode & GLUT_SRGB) {
		ATTR(WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, 1);
	}
	if(mode & GLUT_MULTISAMPLE) {
		ATTR(WGL_SAMPLE_BUFFERS_ARB, 1);
		*aptr++ = WGL_SAMPLES_ARB;
		samples = aptr;
		*aptr++ = 32;
	}
	*aptr++ = 0;

	while((!wglChoosePixelFormat(dc, attr, 0, 1, &pixfmt, &num_pixfmt) || !num_pixfmt) && samples && *samples) {
		*samples >>= 1;
		if(!*samples) {
			aptr[-3] = 0;
		}
	}
	return pixfmt;
}

static PIXELFORMATDESCRIPTOR tmppfd = {
	sizeof tmppfd, 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
	PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 8, 0,
	PFD_MAIN_PLANE, 0, 0, 0, 0
};
#define TMPCLASS	"TempMiniGLUT"

#define GETATTR(attr, vptr) \
	do { \
		int gattr = attr; \
		wglGetPixelFormatAttribiv(dc, pixfmt, 0, 1, &gattr, vptr); \
	} while(0)

static int create_window_wglext(const char *title, int width, int height)
{
	WNDCLASSEX wc = {0};
	HWND tmpwin = 0;
	HDC tmpdc = 0;
	HGLRC tmpctx = 0;
	int pixfmt;

	/* create a temporary window and GL context, just to query and retrieve
	 * the wglChoosePixelFormatEXT function
	 */
	wc.cbSize = sizeof wc;
	wc.hbrBackground = GetStockObject(BLACK_BRUSH);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hIcon = wc.hIconSm = LoadIcon(0, IDI_APPLICATION);
	wc.hInstance = hinst;
	wc.lpfnWndProc = DefWindowProc;
	wc.lpszClassName = TMPCLASS;
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	if(!RegisterClassEx(&wc)) {
		return 0;
	}
	if(!(tmpwin = CreateWindow(TMPCLASS, "temp", WS_OVERLAPPEDWINDOW, 0, 0,
					width, height, 0, 0, hinst, 0))) {
		goto fail;
	}
	tmpdc = GetDC(tmpwin);

	if(!(pixfmt = ChoosePixelFormat(tmpdc, &tmppfd)) ||
			!SetPixelFormat(tmpdc, pixfmt, &tmppfd) ||
			!(tmpctx = wglCreateContext(tmpdc))) {
		goto fail;
	}
	wglMakeCurrent(tmpdc, tmpctx);

	if(!(wglChoosePixelFormat = wglGetProcAddress("wglChoosePixelFormatARB"))) {
		if(!(wglChoosePixelFormat = wglGetProcAddress("wglChoosePixelFormatEXT"))) {
			goto fail;
		}
		if(!(wglGetPixelFormatAttribiv = wglGetProcAddress("wglGetPixelFormatAttribivEXT"))) {
			goto fail;
		}
	} else {
		if(!(wglGetPixelFormatAttribiv = wglGetProcAddress("wglGetPixelFormatAttribivARB"))) {
			goto fail;
		}
	}
	wglMakeCurrent(0, 0);
	wglDeleteContext(tmpctx);
	DestroyWindow(tmpwin);
	UnregisterClass(TMPCLASS, hinst);

	/* create the real window and context */
	if(!(win = CreateWindow("MiniGLUT", title, WS_OVERLAPPEDWINDOW, init_x,
					init_y, width, height, 0, 0, hinst, 0))) {
		panic("Failed to create window\n");
	}
	dc = GetDC(win);

	if(!(pixfmt = choose_pixfmt(init_mode))) {
		panic("Failed to find suitable pixel format\n");
	}
	if(!SetPixelFormat(dc, pixfmt, &tmppfd)) {
		panic("Failed to set the selected pixel format\n");
	}
	if(!(ctx = wglCreateContext(dc))) {
		panic("Failed to create the OpenGL context\n");
	}
	wglMakeCurrent(dc, ctx);

	GETATTR(WGL_RED_BITS, &ctx_info.rsize);
	GETATTR(WGL_GREEN_BITS, &ctx_info.gsize);
	GETATTR(WGL_BLUE_BITS, &ctx_info.bsize);
	GETATTR(WGL_ALPHA_BITS, &ctx_info.asize);
	GETATTR(WGL_DEPTH_BITS, &ctx_info.zsize);
	GETATTR(WGL_STENCIL_BITS, &ctx_info.ssize);
	GETATTR(WGL_DOUBLE_BUFFER, &ctx_info.dblbuf);
	GETATTR(WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, &ctx_info.srgb);
	GETATTR(WGL_SAMPLES_ARB, &ctx_info.samples);
	return 0;

fail:
	if(tmpctx) {
		wglMakeCurrent(0, 0);
		wglDeleteContext(tmpctx);
	}
	if(tmpwin) {
		DestroyWindow(tmpwin);
	}
	UnregisterClass(TMPCLASS, hinst);
	return -1;
}


static void create_window(const char *title)
{
	int pixfmt;
	PIXELFORMATDESCRIPTOR pfd = {0};
	RECT rect;
	int width, height;

	rect.left = init_x;
	rect.top = init_y;
	rect.right = init_x + init_width;
	rect.bottom = init_y + init_height;
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, 0);
	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	if(create_window_wglext(title, width, height) == -1) {

		if(!(win = CreateWindow("MiniGLUT", title, WS_OVERLAPPEDWINDOW,
					rect.left, rect.top, width, height, 0, 0, hinst, 0))) {
			panic("Failed to create window\n");
		}
		dc = GetDC(win);

		pfd.nSize = sizeof pfd;
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		if(init_mode & GLUT_STEREO) {
			pfd.dwFlags |= PFD_STEREO;
		}
		pfd.iPixelType = init_mode & GLUT_INDEX ? PFD_TYPE_COLORINDEX : PFD_TYPE_RGBA;
		pfd.cColorBits = 24;
		if(init_mode & GLUT_ALPHA) {
			pfd.cAlphaBits = 8;
		}
		if(init_mode & GLUT_ACCUM) {
			pfd.cAccumBits = 24;
		}
		if(init_mode & GLUT_DEPTH) {
			pfd.cDepthBits = 24;
		}
		if(init_mode & GLUT_STENCIL) {
			pfd.cStencilBits = 8;
		}
		pfd.iLayerType = PFD_MAIN_PLANE;

		if(!(pixfmt = ChoosePixelFormat(dc, &pfd))) {
			panic("Failed to find suitable pixel format\n");
		}
		if(!SetPixelFormat(dc, pixfmt, &pfd)) {
			panic("Failed to set the selected pixel format\n");
		}
		if(!(ctx = wglCreateContext(dc))) {
			panic("Failed to create the OpenGL context\n");
		}
		wglMakeCurrent(dc, ctx);

		DescribePixelFormat(dc, pixfmt, sizeof pfd, &pfd);
		ctx_info.rsize = pfd.cRedBits;
		ctx_info.gsize = pfd.cGreenBits;
		ctx_info.bsize = pfd.cBlueBits;
		ctx_info.asize = pfd.cAlphaBits;
		ctx_info.zsize = pfd.cDepthBits;
		ctx_info.ssize = pfd.cStencilBits;
		ctx_info.dblbuf = pfd.dwFlags & PFD_DOUBLEBUFFER ? 1 : 0;
		ctx_info.samples = 0;
		ctx_info.srgb = 0;
	}

	ShowWindow(win, 1);
	SetForegroundWindow(win);
	SetFocus(win);
	upd_pending = 1;
	reshape_pending = 1;
}

static HRESULT CALLBACK handle_message(HWND win, unsigned int msg, WPARAM wparam, LPARAM lparam)
{
	static int mouse_x, mouse_y;
	int x, y, key;

	switch(msg) {
	case WM_CLOSE:
		if(win) DestroyWindow(win);
		break;

	case WM_DESTROY:
		cleanup();
		quit = 1;
		PostQuitMessage(0);
		break;

	case WM_PAINT:
		upd_pending = 1;
		ValidateRect(win, 0);
		break;

	case WM_SIZE:
		x = lparam & 0xffff;
		y = lparam >> 16;
		if(x != win_width && y != win_height) {
			win_width = x;
			win_height = y;
			if(cb_reshape) {
				reshape_pending = 0;
				cb_reshape(win_width, win_height);
			}
		}
		break;

	case WM_SHOWWINDOW:
		mapped = wparam;
		if(cb_vis) cb_vis(mapped ? GLUT_VISIBLE : GLUT_NOT_VISIBLE);
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		update_modkeys();
		key = translate_vkey(wparam);
		if(key < 256) {
			if(cb_keydown) {
				cb_keydown((unsigned char)key, mouse_x, mouse_y);
			}
		} else {
			if(cb_skeydown) {
				cb_skeydown(key, mouse_x, mouse_y);
			}
		}
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		update_modkeys();
		key = translate_vkey(wparam);
		if(key < 256) {
			if(cb_keyup) {
				cb_keyup((unsigned char)key, mouse_x, mouse_y);
			}
		} else {
			if(cb_skeyup) {
				cb_skeyup(key, mouse_x, mouse_y);
			}
		}
		break;

	case WM_LBUTTONDOWN:
		handle_mbutton(0, 1, wparam, lparam);
		break;
	case WM_MBUTTONDOWN:
		handle_mbutton(1, 1, wparam, lparam);
		break;
	case WM_RBUTTONDOWN:
		handle_mbutton(2, 1, wparam, lparam);
		break;
	case WM_LBUTTONUP:
		handle_mbutton(0, 0, wparam, lparam);
		break;
	case WM_MBUTTONUP:
		handle_mbutton(1, 0, wparam, lparam);
		break;
	case WM_RBUTTONUP:
		handle_mbutton(2, 0, wparam, lparam);
		break;

	case WM_MOUSEMOVE:
		if(wparam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) {
			if(cb_motion) cb_motion(lparam & 0xffff, lparam >> 16);
		} else {
			if(cb_passive) cb_passive(lparam & 0xffff, lparam >> 16);
		}
		break;

	case WM_SYSCOMMAND:
		wparam &= 0xfff0;
		if(wparam == SC_KEYMENU || wparam == SC_SCREENSAVE || wparam == SC_MONITORPOWER) {
			return 0;
		}
	default:
		return DefWindowProc(win, msg, wparam, lparam);
	}

	return 0;
}

static void update_modkeys(void)
{
	if(GetKeyState(VK_SHIFT) & 0x8000) {
		modstate |= GLUT_ACTIVE_SHIFT;
	} else {
		modstate &= ~GLUT_ACTIVE_SHIFT;
	}
	if(GetKeyState(VK_CONTROL) & 0x8000) {
		modstate |= GLUT_ACTIVE_CTRL;
	} else {
		modstate &= ~GLUT_ACTIVE_CTRL;
	}
	if(GetKeyState(VK_MENU) & 0x8000) {
		modstate |= GLUT_ACTIVE_ALT;
	} else {
		modstate &= ~GLUT_ACTIVE_ALT;
	}
}

static int translate_vkey(int vkey)
{
	switch(vkey) {
	case VK_PRIOR: return GLUT_KEY_PAGE_UP;
	case VK_NEXT: return GLUT_KEY_PAGE_DOWN;
	case VK_END: return GLUT_KEY_END;
	case VK_HOME: return GLUT_KEY_HOME;
	case VK_LEFT: return GLUT_KEY_LEFT;
	case VK_UP: return GLUT_KEY_UP;
	case VK_RIGHT: return GLUT_KEY_RIGHT;
	case VK_DOWN: return GLUT_KEY_DOWN;
	default:
		break;
	}

	if(vkey >= 'A' && vkey <= 'Z') {
		vkey += 32;
	} else if(vkey >= VK_F1 && vkey <= VK_F12) {
		vkey -= VK_F1 + GLUT_KEY_F1;
	}

	return vkey;
}

static void handle_mbutton(int bn, int st, WPARAM wparam, LPARAM lparam)
{
	int x, y;

	update_modkeys();

	if(cb_mouse) {
		x = lparam & 0xffff;
		y = lparam >> 16;
		cb_mouse(bn, st ? GLUT_DOWN : GLUT_UP, x, y);
	}
}

static void get_window_pos(int *x, int *y)
{
	RECT rect;
	GetWindowRect(win, &rect);
	*x = rect.left;
	*y = rect.top;
}

static void get_window_size(int *w, int *h)
{
	RECT rect;
	GetClientRect(win, &rect);
	*w = rect.right - rect.left;
	*h = rect.bottom - rect.top;
}

static void get_screen_size(int *scrw, int *scrh)
{
	*scrw = GetSystemMetrics(SM_CXSCREEN);
	*scrh = GetSystemMetrics(SM_CYSCREEN);
}
#endif	/* BUILD_WIN32 */

#if defined(__unix__) || defined(__APPLE__)
#include <sys/time.h>

#ifdef MINIGLUT_USE_LIBC
#define sys_gettimeofday(tv, tz)	gettimeofday(tv, tz)
#else
static int sys_gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

static long get_msec(void)
{
	static struct timeval tv0;
	struct timeval tv;

	sys_gettimeofday(&tv, 0);
	if(tv0.tv_sec == 0 && tv0.tv_usec == 0) {
		tv0 = tv;
		return 0;
	}
	return (tv.tv_sec - tv0.tv_sec) * 1000 + (tv.tv_usec - tv0.tv_usec) / 1000;
}
#endif	/* UNIX */
#ifdef _WIN32
static long get_msec(void)
{
	static long t0;
	long tm;

#ifdef MINIGLUT_NO_WINMM
	tm = GetTickCount();
#else
	tm = timeGetTime();
#endif
	if(!t0) {
		t0 = tm;
		return 0;
	}
	return tm - t0;
}
#endif

static void panic(const char *msg)
{
	const char *end = msg;
	while(*end) end++;
	sys_write(2, msg, end - msg);
	sys_exit(1);
}


#ifdef MINIGLUT_USE_LIBC
#include <stdlib.h>
#ifdef __unix__
#include <unistd.h>
#endif

static void sys_exit(int status)
{
	exit(status);
}

static int sys_write(int fd, const void *buf, int count)
{
	return write(fd, buf, count);
}

#else	/* !MINIGLUT_USE_LIBC */

#ifdef __linux__
#ifdef __x86_64__
static void sys_exit(int status)
{
	asm volatile(
		"syscall\n\t"
		:: "a"(60), "D"(status));
}
static int sys_write(int fd, const void *buf, int count)
{
	long res;
	asm volatile(
		"syscall\n\t"
		: "=a"(res)
		: "a"(1), "D"(fd), "S"(buf), "d"(count));
	return res;
}
static int sys_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	int res;
	asm volatile(
		"syscall\n\t"
		: "=a"(res)
		: "a"(96), "D"(tv), "S"(tz));
	return res;
}
#endif	/* __x86_64__ */
#ifdef __i386__
static void sys_exit(int status)
{
	asm volatile(
		"int $0x80\n\t"
		:: "a"(1), "b"(status));
}
static int sys_write(int fd, const void *buf, int count)
{
	int res;
	asm volatile(
		"int $0x80\n\t"
		: "=a"(res)
		: "a"(4), "b"(fd), "c"(buf), "d"(count));
	return res;
}
static int sys_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	int res;
	asm volatile(
		"int $0x80\n\t"
		: "=a"(res)
		: "a"(78), "b"(tv), "c"(tz));
	return res;
}
#endif	/* __i386__ */
#endif	/* __linux__ */

#ifdef _WIN32
static void sys_exit(int status)
{
	ExitProcess(status);
}
static int sys_write(int fd, const void *buf, int count)
{
	unsigned long wrsz = 0;

	HANDLE out = GetStdHandle(fd == 1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
	if(!WriteFile(out, buf, count, &wrsz, 0)) {
		return -1;
	}
	return wrsz;
}
#endif	/* _WIN32 */
#endif	/* !MINIGLUT_USE_LIBC */


/* ----------------- primitives ------------------ */
#ifdef MINIGLUT_USE_LIBC
#include <stdlib.h>
#include <math.h>

void mglut_sincos(float angle, float *sptr, float *cptr)
{
	*sptr = sin(angle);
	*cptr = cos(angle);
}

float mglut_atan(float x)
{
	return atan(x);
}

#else	/* !MINIGLUT_USE_LIBC */

#ifdef __GNUC__
void mglut_sincos(float angle, float *sptr, float *cptr)
{
	asm volatile(
		"flds %2\n\t"
		"fsincos\n\t"
		"fstps %1\n\t"
		"fstps %0\n\t"
		: "=m"(*sptr), "=m"(*cptr)
		: "m"(angle)
	);
}

float mglut_atan(float x)
{
	float res;
	asm volatile(
		"flds %1\n\t"
		"fld1\n\t"
		"fpatan\n\t"
		"fstps %0\n\t"
		: "=m"(res)
		: "m"(x)
	);
	return res;
}
#endif

#ifdef _MSC_VER
void mglut_sincos(float angle, float *sptr, float *cptr)
{
	float s, c;
	__asm {
		fld angle
		fsincos
		fstp c
		fstp s
	}
	*sptr = s;
	*cptr = c;
}

float mglut_atan(float x)
{
	float res;
	__asm {
		fld x
		fld1
		fpatan
		fstp res
	}
	return res;
}
#endif

#ifdef __WATCOMC__
#pragma aux mglut_sincos = \
	"fsincos" \
	"fstp dword ptr [edx]" \
	"fstp dword ptr [eax]" \
	parm[8087][eax][edx]	\
	modify[8087];

#pragma aux mglut_atan = \
	"fld1" \
	"fpatan" \
	parm[8087] \
	value[8087] \
	modify [8087];
#endif	/* __WATCOMC__ */

#endif	/* !MINIGLUT_USE_LIBC */

#define PI	3.1415926536f

void glutSolidSphere(float rad, int slices, int stacks)
{
	int i, j, k, gray;
	float x, y, z, s, t, u, v, phi, theta, sintheta, costheta, sinphi, cosphi;
	float du = 1.0f / (float)slices;
	float dv = 1.0f / (float)stacks;

	glBegin(GL_QUADS);
	for(i=0; i<stacks; i++) {
		v = i * dv;
		for(j=0; j<slices; j++) {
			u = j * du;
			for(k=0; k<4; k++) {
				gray = k ^ (k >> 1);
				s = gray & 1 ? u + du : u;
				t = gray & 2 ? v + dv : v;
				theta = s * PI * 2.0f;
				phi = t * PI;
				mglut_sincos(theta, &sintheta, &costheta);
				mglut_sincos(phi, &sinphi, &cosphi);
				x = sintheta * sinphi;
				y = costheta * sinphi;
				z = cosphi;

				glColor3f(s, t, 1);
				glTexCoord2f(s, t);
				glNormal3f(x, y, z);
				glVertex3f(x * rad, y * rad, z * rad);
			}
		}
	}
	glEnd();
}

void glutWireSphere(float rad, int slices, int stacks)
{
	glPushAttrib(GL_POLYGON_BIT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glutSolidSphere(rad, slices, stacks);
	glPopAttrib();
}

void glutSolidCube(float sz)
{
	int i, j, idx, gray, flip, rotx;
	float vpos[3], norm[3];
	float rad = sz * 0.5f;

	glBegin(GL_QUADS);
	for(i=0; i<6; i++) {
		flip = i & 1;
		rotx = i >> 2;
		idx = (~i & 2) - rotx;
		norm[0] = norm[1] = norm[2] = 0.0f;
		norm[idx] = flip ^ ((i >> 1) & 1) ? -1 : 1;
		glNormal3fv(norm);
		vpos[idx] = norm[idx] * rad;
		for(j=0; j<4; j++) {
			gray = j ^ (j >> 1);
			vpos[i & 2] = (gray ^ flip) & 1 ? rad : -rad;
			vpos[rotx + 1] = (gray ^ (rotx << 1)) & 2 ? rad : -rad;
			glTexCoord2f(gray & 1, gray >> 1);
			glVertex3fv(vpos);
		}
	}
	glEnd();
}

void glutWireCube(float sz)
{
	glPushAttrib(GL_POLYGON_BIT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glutSolidCube(sz);
	glPopAttrib();
}

static void draw_cylinder(float rbot, float rtop, float height, int slices, int stacks)
{
	int i, j, k, gray;
	float x, y, z, s, t, u, v, theta, phi, sintheta, costheta, sinphi, cosphi, rad;
	float du = 1.0f / (float)slices;
	float dv = 1.0f / (float)stacks;

	rad = rbot - rtop;
	phi = mglut_atan((rad < 0 ? -rad : rad) / height);
	mglut_sincos(phi, &sinphi, &cosphi);

	glBegin(GL_QUADS);
	for(i=0; i<stacks; i++) {
		v = i * dv;
		for(j=0; j<slices; j++) {
			u = j * du;
			for(k=0; k<4; k++) {
				gray = k ^ (k >> 1);
				s = gray & 2 ? u + du : u;
				t = gray & 1 ? v + dv : v;
				rad = rbot + (rtop - rbot) * t;
				theta = s * PI * 2.0f;
				mglut_sincos(theta, &sintheta, &costheta);

				x = sintheta * cosphi;
				y = costheta * cosphi;
				z = sinphi;

				glColor3f(s, t, 1);
				glTexCoord2f(s, t);
				glNormal3f(x, y, z);
				glVertex3f(sintheta * rad, costheta * rad, t * height);
			}
		}
	}
	glEnd();
}

void glutSolidCone(float base, float height, int slices, int stacks)
{
	draw_cylinder(base, 0, height, slices, stacks);
}

void glutWireCone(float base, float height, int slices, int stacks)
{
	glPushAttrib(GL_POLYGON_BIT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glutSolidCone(base, height, slices, stacks);
	glPopAttrib();
}

void glutSolidCylinder(float rad, float height, int slices, int stacks)
{
	draw_cylinder(rad, rad, height, slices, stacks);
}

void glutWireCylinder(float rad, float height, int slices, int stacks)
{
	glPushAttrib(GL_POLYGON_BIT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glutSolidCylinder(rad, height, slices, stacks);
	glPopAttrib();
}

void glutSolidTorus(float inner_rad, float outer_rad, int sides, int rings)
{
	int i, j, k, gray;
	float x, y, z, s, t, u, v, phi, theta, sintheta, costheta, sinphi, cosphi;
	float du = 1.0f / (float)rings;
	float dv = 1.0f / (float)sides;

	glBegin(GL_QUADS);
	for(i=0; i<rings; i++) {
		u = i * du;
		for(j=0; j<sides; j++) {
			v = j * dv;
			for(k=0; k<4; k++) {
				gray = k ^ (k >> 1);
				s = gray & 1 ? u + du : u;
				t = gray & 2 ? v + dv : v;
				theta = s * PI * 2.0f;
				phi = t * PI * 2.0f;
				mglut_sincos(theta, &sintheta, &costheta);
				mglut_sincos(phi, &sinphi, &cosphi);
				x = sintheta * sinphi;
				y = costheta * sinphi;
				z = cosphi;

				glColor3f(s, t, 1);
				glTexCoord2f(s, t);
				glNormal3f(x, y, z);

				x = x * inner_rad + sintheta * outer_rad;
				y = y * inner_rad + costheta * outer_rad;
				z *= inner_rad;
				glVertex3f(x, y, z);
			}
		}
	}
	glEnd();
}

void glutWireTorus(float inner_rad, float outer_rad, int sides, int rings)
{
	glPushAttrib(GL_POLYGON_BIT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glutSolidTorus(inner_rad, outer_rad, sides, rings);
	glPopAttrib();
}

#define NUM_TEAPOT_INDICES	(sizeof teapot_index / sizeof *teapot_index)
#define NUM_TEAPOT_VERTS	(sizeof teapot_verts / sizeof *teapot_verts)

#define NUM_TEAPOT_PATCHES	(NUM_TEAPOT_INDICES / 16)

#define PATCH_SUBDIV	7

static float teapot_part_flip[] = {
	1, 1, 1, 1,			/* rim flip */
	1, 1, 1, 1,			/* body1 flip */
	1, 1, 1, 1,			/* body2 flip */
	1, 1, 1, 1,			/* lid patch 1 flip */
	1, 1, 1, 1,			/* lid patch 2 flip */
	1, -1,				/* handle 1 flip */
	1, -1,				/* handle 2 flip */
	1, -1,				/* spout 1 flip */
	1, -1,				/* spout 2 flip */
	1, 1, 1, 1			/* bottom flip */
};

static float teapot_part_rot[] = {
	0, 90, 180, 270,	/* rim rotations */
	0, 90, 180, 270,	/* body patch 1 rotations */
	0, 90, 180, 270,	/* body patch 2 rotations */
	0, 90, 180, 270,	/* lid patch 1 rotations */
	0, 90, 180, 270,	/* lid patch 2 rotations */
	0, 0,				/* handle 1 rotations */
	0, 0,				/* handle 2 rotations */
	0, 0,				/* spout 1 rotations */
	0, 0,				/* spout 2 rotations */
	0, 90, 180, 270		/* bottom rotations */
};


static int teapot_index[] = {
	/* rim */
	102, 103, 104, 105, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	102, 103, 104, 105, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	102, 103, 104, 105, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	102, 103, 104, 105, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	/* body1 */
	12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
	12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
	12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
	12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
	/* body 2 */
	24, 25, 26, 27, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	24, 25, 26, 27, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	24, 25, 26, 27, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	24, 25, 26, 27, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	/* lid 1 */
	96, 96, 96, 96, 97, 98, 99, 100, 101, 101, 101, 101, 0,  1,  2, 3,
	96, 96, 96, 96, 97, 98, 99, 100, 101, 101, 101, 101, 0,  1,  2, 3,
	96, 96, 96, 96, 97, 98, 99, 100, 101, 101, 101, 101, 0,  1,  2, 3,
	96, 96, 96, 96, 97, 98, 99, 100, 101, 101, 101, 101, 0,  1,  2, 3,
	/* lid 2 */
	0,  1,  2,  3, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
	0,  1,  2,  3, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
	0,  1,  2,  3, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
	0,  1,  2,  3, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
	/* handle 1 */
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56,
	/* handle 2 */
	53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 28, 65, 66, 67,
	53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 28, 65, 66, 67,
	/* spout 1 */
	68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83,
	68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83,
	/* spout 2 */
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	/* bottom */
	118, 118, 118, 118, 124, 122, 119, 121, 123, 126, 125, 120, 40, 39, 38, 37,
	118, 118, 118, 118, 124, 122, 119, 121, 123, 126, 125, 120, 40, 39, 38, 37,
	118, 118, 118, 118, 124, 122, 119, 121, 123, 126, 125, 120, 40, 39, 38, 37,
	118, 118, 118, 118, 124, 122, 119, 121, 123, 126, 125, 120, 40, 39, 38, 37
};


static float teapot_verts[][3] = {
	{  0.2000,  0.0000, 2.70000 }, {  0.2000, -0.1120, 2.70000 },
	{  0.1120, -0.2000, 2.70000 }, {  0.0000, -0.2000, 2.70000 },
	{  1.3375,  0.0000, 2.53125 }, {  1.3375, -0.7490, 2.53125 },
	{  0.7490, -1.3375, 2.53125 }, {  0.0000, -1.3375, 2.53125 },
	{  1.4375,  0.0000, 2.53125 }, {  1.4375, -0.8050, 2.53125 },
	{  0.8050, -1.4375, 2.53125 }, {  0.0000, -1.4375, 2.53125 },
	{  1.5000,  0.0000, 2.40000 }, {  1.5000, -0.8400, 2.40000 },
	{  0.8400, -1.5000, 2.40000 }, {  0.0000, -1.5000, 2.40000 },
	{  1.7500,  0.0000, 1.87500 }, {  1.7500, -0.9800, 1.87500 },
	{  0.9800, -1.7500, 1.87500 }, {  0.0000, -1.7500, 1.87500 },
	{  2.0000,  0.0000, 1.35000 }, {  2.0000, -1.1200, 1.35000 },
	{  1.1200, -2.0000, 1.35000 }, {  0.0000, -2.0000, 1.35000 },
	{  2.0000,  0.0000, 0.90000 }, {  2.0000, -1.1200, 0.90000 },
	{  1.1200, -2.0000, 0.90000 }, {  0.0000, -2.0000, 0.90000 },
	{ -2.0000,  0.0000, 0.90000 }, {  2.0000,  0.0000, 0.45000 },
	{  2.0000, -1.1200, 0.45000 }, {  1.1200, -2.0000, 0.45000 },
	{  0.0000, -2.0000, 0.45000 }, {  1.5000,  0.0000, 0.22500 },
	{  1.5000, -0.8400, 0.22500 }, {  0.8400, -1.5000, 0.22500 },
	{  0.0000, -1.5000, 0.22500 }, {  1.5000,  0.0000, 0.15000 },
	{  1.5000, -0.8400, 0.15000 }, {  0.8400, -1.5000, 0.15000 },
	{  0.0000, -1.5000, 0.15000 }, { -1.6000,  0.0000, 2.02500 },
	{ -1.6000, -0.3000, 2.02500 }, { -1.5000, -0.3000, 2.25000 },
	{ -1.5000,  0.0000, 2.25000 }, { -2.3000,  0.0000, 2.02500 },
	{ -2.3000, -0.3000, 2.02500 }, { -2.5000, -0.3000, 2.25000 },
	{ -2.5000,  0.0000, 2.25000 }, { -2.7000,  0.0000, 2.02500 },
	{ -2.7000, -0.3000, 2.02500 }, { -3.0000, -0.3000, 2.25000 },
	{ -3.0000,  0.0000, 2.25000 }, { -2.7000,  0.0000, 1.80000 },
	{ -2.7000, -0.3000, 1.80000 }, { -3.0000, -0.3000, 1.80000 },
	{ -3.0000,  0.0000, 1.80000 }, { -2.7000,  0.0000, 1.57500 },
	{ -2.7000, -0.3000, 1.57500 }, { -3.0000, -0.3000, 1.35000 },
	{ -3.0000,  0.0000, 1.35000 }, { -2.5000,  0.0000, 1.12500 },
	{ -2.5000, -0.3000, 1.12500 }, { -2.6500, -0.3000, 0.93750 },
	{ -2.6500,  0.0000, 0.93750 }, { -2.0000, -0.3000, 0.90000 },
	{ -1.9000, -0.3000, 0.60000 }, { -1.9000,  0.0000, 0.60000 },
	{  1.7000,  0.0000, 1.42500 }, {  1.7000, -0.6600, 1.42500 },
	{  1.7000, -0.6600, 0.60000 }, {  1.7000,  0.0000, 0.60000 },
	{  2.6000,  0.0000, 1.42500 }, {  2.6000, -0.6600, 1.42500 },
	{  3.1000, -0.6600, 0.82500 }, {  3.1000,  0.0000, 0.82500 },
	{  2.3000,  0.0000, 2.10000 }, {  2.3000, -0.2500, 2.10000 },
	{  2.4000, -0.2500, 2.02500 }, {  2.4000,  0.0000, 2.02500 },
	{  2.7000,  0.0000, 2.40000 }, {  2.7000, -0.2500, 2.40000 },
	{  3.3000, -0.2500, 2.40000 }, {  3.3000,  0.0000, 2.40000 },
	{  2.8000,  0.0000, 2.47500 }, {  2.8000, -0.2500, 2.47500 },
	{  3.5250, -0.2500, 2.49375 }, {  3.5250,  0.0000, 2.49375 },
	{  2.9000,  0.0000, 2.47500 }, {  2.9000, -0.1500, 2.47500 },
	{  3.4500, -0.1500, 2.51250 }, {  3.4500,  0.0000, 2.51250 },
	{  2.8000,  0.0000, 2.40000 }, {  2.8000, -0.1500, 2.40000 },
	{  3.2000, -0.1500, 2.40000 }, {  3.2000,  0.0000, 2.40000 },
	{  0.0000,  0.0000, 3.15000 }, {  0.8000,  0.0000, 3.15000 },
	{  0.8000, -0.4500, 3.15000 }, {  0.4500, -0.8000, 3.15000 },
	{  0.0000, -0.8000, 3.15000 }, {  0.0000,  0.0000, 2.85000 },
	{  1.4000,  0.0000, 2.40000 }, {  1.4000, -0.7840, 2.40000 },
	{  0.7840, -1.4000, 2.40000 }, {  0.0000, -1.4000, 2.40000 },
	{  0.4000,  0.0000, 2.55000 }, {  0.4000, -0.2240, 2.55000 },
	{  0.2240, -0.4000, 2.55000 }, {  0.0000, -0.4000, 2.55000 },
	{  1.3000,  0.0000, 2.55000 }, {  1.3000, -0.7280, 2.55000 },
	{  0.7280, -1.3000, 2.55000 }, {  0.0000, -1.3000, 2.55000 },
	{  1.3000,  0.0000, 2.40000 }, {  1.3000, -0.7280, 2.40000 },
	{  0.7280, -1.3000, 2.40000 }, {  0.0000, -1.3000, 2.40000 },
	{  0.0000,  0.0000, 0.00000 }, {  1.4250, -0.7980, 0.00000 },
	{  1.5000,  0.0000, 0.07500 }, {  1.4250,  0.0000, 0.00000 },
	{  0.7980, -1.4250, 0.00000 }, {  0.0000, -1.5000, 0.07500 },
	{  0.0000, -1.4250, 0.00000 }, {  1.5000, -0.8400, 0.07500 },
	{  0.8400, -1.5000, 0.07500 }
};

static void draw_patch(int *index, int flip, float scale);
static float bernstein(int i, float x);

void glutSolidTeapot(float size)
{
	int i;

	size /= 2.0;

	for(i=0; i<NUM_TEAPOT_PATCHES; i++) {
		float flip = teapot_part_flip[i];
		float rot = teapot_part_rot[i];

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glTranslatef(0, -3.15 * size * 0.5, 0);
		glRotatef(rot, 0, 1, 0);
		glScalef(1, 1, flip);
		glRotatef(-90, 1, 0, 0);

		draw_patch(teapot_index + i * 16, flip < 0.0 ? 1 : 0, size);

		glPopMatrix();
	}
}

void glutWireTeapot(float size)
{
	glPushAttrib(GL_POLYGON_BIT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glutSolidTeapot(size);
	glPopAttrib();
}


static void bezier_patch(float *res, float *cp, float u, float v)
{
	int i, j;

	res[0] = res[1] = res[2] = 0.0f;

	for(j=0; j<4; j++) {
		for(i=0; i<4; i++) {
			float bu = bernstein(i, u);
			float bv = bernstein(j, v);

			res[0] += cp[0] * bu * bv;
			res[1] += cp[1] * bu * bv;
			res[2] += cp[2] * bu * bv;

			cp += 3;
		}
	}
}

static float rsqrt(float x)
{
	float xhalf = x * 0.5f;
	int i = *(int*)&x;
	i = 0x5f3759df - (i >> 1);
	x = *(float*)&i;
	x = x * (1.5f - xhalf * x * x);
	return x;
}


#define CROSS(res, a, b) \
	do { \
		(res)[0] = (a)[1] * (b)[2] - (a)[2] * (b)[1]; \
		(res)[1] = (a)[2] * (b)[0] - (a)[0] * (b)[2]; \
		(res)[2] = (a)[0] * (b)[1] - (a)[1] * (b)[0]; \
	} while(0)

#define NORMALIZE(v) \
	do { \
		float s = rsqrt((v)[0] * (v)[0] + (v)[1] * (v)[1] + (v)[2] * (v)[2]); \
		(v)[0] *= s; \
		(v)[1] *= s; \
		(v)[2] *= s; \
	} while(0)

#define DT	0.001

static void bezier_patch_norm(float *res, float *cp, float u, float v)
{
	float tang[3], bitan[3], tmp[3];

	bezier_patch(tang, cp, u + DT, v);
	bezier_patch(tmp, cp, u - DT, v);
	tang[0] -= tmp[0];
	tang[1] -= tmp[1];
	tang[2] -= tmp[2];

	bezier_patch(bitan, cp, u, v + DT);
	bezier_patch(tmp, cp, u, v - DT);
	bitan[0] -= tmp[0];
	bitan[1] -= tmp[1];
	bitan[2] -= tmp[2];

	CROSS(res, tang, bitan);
	NORMALIZE(res);
}



static float bernstein(int i, float x)
{
	float invx = 1.0f - x;

	switch(i) {
	case 0:
		return invx * invx * invx;
	case 1:
		return 3.0f * x * invx * invx;
	case 2:
		return 3.0f * x * x * invx;
	case 3:
		return x * x * x;
	default:
		break;
	}
	return 0.0f;
}

static void draw_patch(int *index, int flip, float scale)
{
	static const float uoffs[2][4] = {{0, 0, 1, 1}, {1, 1, 0, 0}};
	static const float voffs[4] = {0, 1, 1, 0};

	int i, j, k;
	float cp[16 * 3];
	float pt[3], n[3];
	float u, v;
	float du = 1.0 / PATCH_SUBDIV;
	float dv = 1.0 / PATCH_SUBDIV;

	/* collect control points */
	for(i=0; i<16; i++) {
		cp[i * 3] = teapot_verts[index[i]][0];
		cp[i * 3 + 1] = teapot_verts[index[i]][1];
		cp[i * 3 + 2] = teapot_verts[index[i]][2];
	}

	glBegin(GL_QUADS);
	glColor3f(1, 1, 1);

	u = 0;
	for(i=0; i<PATCH_SUBDIV; i++) {
		v = 0;
		for(j=0; j<PATCH_SUBDIV; j++) {

			for(k=0; k<4; k++) {
				bezier_patch(pt, cp, u + uoffs[flip][k] * du, v + voffs[k] * dv);

				/* top/bottom normal hack */
				if(pt[2] > 3.14) {
					n[0] = n[1] = 0.0f;
					n[2] = 1.0f;
				} else if(pt[2] < 0.00001) {
					n[0] = n[1] = 0.0f;
					n[2] = -1.0f;
				} else {
					bezier_patch_norm(n, cp, u + uoffs[flip][k] * du, v + voffs[k] * dv);
				}

				glTexCoord2f(u, v);
				glNormal3fv(n);
				glVertex3f(pt[0] * scale, pt[1] * scale, pt[2] * scale);
			}

			v += dv;
		}
		u += du;
	}

	glEnd();
}
