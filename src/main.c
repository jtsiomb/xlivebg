/*
xlivebg - live wallpapers for the X window system
Copyright (C) 2019-2020  John Tsiombikas <nuclear@member.fsf.org>

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif
#include <X11/extensions/shape.h>
#include <GL/glx.h>
#include "opengl.h"
#include "app.h"
#include "xlivebg.h"
#include "cfg.h"
#include "ctrl.h"
#include "imageman.h"

/* create_xwindow flags */
enum {
	WIN_REGULAR	= 1
};

/* defined in toon_root.c */
Window ToonGetRootWindow(Display *dpy, int scr, Window *par);

/* defined in client.c */
int client_main(int argc, char **argv);

static Window create_xwindow(int width, int height, unsigned int flags);
static void netwm_setprop_atom(Window win, const char *prop, const char *val);
static XVisualInfo *choose_visual(void);
static void detect_outputs(void);
static int proc_xevent(XEvent *ev);
static void send_expose(Window win);
static void sighandler(int s);
static int parse_args(int argc, char **argv);
static void print_usage(const char *argv0);

static Display *dpy;
static int scr;
static Window win, root;
static GLXContext ctx;
static XVisualInfo *visinf;
static Atom xa_wm_proto, xa_wm_delwin;
static int have_xrandr, xrandr_evbase, xrandr_errbase;

static volatile int quit;
static int mapped;
static int win_width, win_height;
static int dblbuf;
static int opt_new_win, opt_preview;
static Window new_win_parent;

static struct timeval tv, tv0;

int main(int argc, char **argv)
{
	int xfd, len;
	XWindowAttributes attr;

	len = strlen(argv[0]);
	if(len >= 11 && memcmp(argv[0] + len - 11, "xlivebg-cmd", 11) == 0) {
		return client_main(argc, argv);
	}
	if(argv[1] && strcmp(argv[1], "cmd") == 0) {
		argv[1] = argv[0];
		return client_main(--argc, argv + 1);
	}

	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	if(!(dpy = XOpenDisplay(0))) {
		fprintf(stderr, "failed to open connection to the X server\n");
		return 1;
	}
	xfd = ConnectionNumber(dpy);
	scr = DefaultScreen(dpy);
	root = RootWindow(dpy, scr);

	/* By default set scr_width/scr_height to the root dimensions. Will be
	 * overriden below, if XR&R is available.
	 */
	XGetWindowAttributes(dpy, root, &attr);
	scr_width = attr.width;
	scr_height = attr.height;

	xa_wm_proto = XInternAtom(dpy, "WM_PROTOCOLS", False);
	xa_wm_delwin = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

	/* if win is already set, means the user passed -w <id> */
	if(!win) {
		if(opt_new_win) {
			unsigned int flags = 0;

			if(opt_preview) {
				/* fake screen dimensions */
				scr_width = 800;
				scr_height = 600;
				flags = WIN_REGULAR;
			}
			if(!(win = create_xwindow(scr_width, scr_height, flags))) {
				return 1;
			}
		} else {
			Window parent;
			win = ToonGetRootWindow(dpy, scr, &parent);
			printf("detected root window: %x\n", (unsigned int)win);
		}
	}
	XSelectInput(dpy, win, ExposureMask | StructureNotifyMask);

	signal(SIGINT, sighandler);
	signal(SIGILL, sighandler);
	signal(SIGSEGV, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGPIPE, SIG_IGN);

	init_cfg();

	if(ctrl_init() == -1) {
		fprintf(stderr, "Failed to create control socket, is xlivebg already running?\n");
		XCloseDisplay(dpy);
		return 1;
	}

#ifdef HAVE_XRANDR
	if(!opt_preview && (have_xrandr = XRRQueryExtension(dpy, &xrandr_evbase, &xrandr_errbase))) {
		/* detect number of screens and initialize screen structures */
		detect_outputs();
		XRRSelectInput(dpy, root, RRScreenChangeNotifyMask);
	} else
#endif
	{
		num_screens = 1;
		screen[0].x = screen[0].y = 0;
		screen[0].width = screen[0].root_width = scr_width;
		screen[0].height = screen[0].root_height = scr_height;
		screen[0].aspect = (float)scr_width / (float)scr_height;
		screen[0].vport[0] = screen[0].vport[1] = 0;
		screen[0].vport[2] = scr_width;
		screen[0].vport[3] = scr_height;
		printf("output: %dx%d\n", scr_width, scr_height);
	}

	if(app_init(argc, argv) == -1) {
		ctrl_shutdown();
		XCloseDisplay(dpy);
		return 1;
	}

	gettimeofday(&tv0, 0);

	while(!quit) {
		fd_set rdset;
		struct timeval *timeout;
		int i, max_fd, num_ctrl_sock;
		long interval;
		int *ctrl_sock;

		while(XPending(dpy)) {
			XEvent ev;
			XNextEvent(dpy, &ev);
			if(proc_xevent(&ev) == -1 || quit) {
				goto done;
			}
		}

		gettimeofday(&tv, 0);
		msec = (tv.tv_sec - tv0.tv_sec) * 1000 + (tv.tv_usec - tv0.tv_usec) / 1000;

		app_draw();
		if(dblbuf) {
			glXSwapBuffers(dpy, win);
		} else {
			glFlush();
		}

		/* wait for as long as requested by the live wallpaper, or until an
		 * event arrives
		 */
		FD_ZERO(&rdset);
		FD_SET(xfd, &rdset);
		max_fd = xfd;

		ctrl_sock = ctrl_sockets(&num_ctrl_sock);
		for(i=0; i<num_ctrl_sock; i++) {
			int s = ctrl_sock[i];
			FD_SET(s, &rdset);
			if(s > max_fd) max_fd = s;
		}

		interval = (cfg.fps_override > 0) ? cfg.fps_override_interval : upd_interval_usec;

		if(interval > 0) {
			struct timeval now;
			long usec;

			gettimeofday(&now, 0);

			usec = interval - (now.tv_sec - tv.tv_sec) * 1000000 - (now.tv_usec - tv.tv_usec);
			if(usec < 0) usec = 0;

			tv.tv_sec = usec / 1000000;
			tv.tv_usec = usec % 1000000;
			timeout = &tv;
		} else {
			timeout = 0;
		}

		if(select(max_fd + 1, &rdset, 0, 0, timeout) > 0) {
			/* ignore X events, we'll just handle those at the top of the loop
			 * shortly. just handle control socket input
			 */
			for(i=0; i<num_ctrl_sock; i++) {
				if(FD_ISSET(ctrl_sock[i], &rdset)) {
					ctrl_process(ctrl_sock[i]);
				}
			}
		}
	}

done:
	ctrl_shutdown();
	send_expose(win);
	if(visinf) {
		XFree(visinf);
	}
	xlivebg_destroy_gl();
	if(opt_new_win) {
		XDestroyWindow(dpy, win);
	}
	XCloseDisplay(dpy);
	return 0;
}

void app_quit(void)
{
	quit = 1;
}

unsigned int app_getmouse(int *x, int *y)
{
	static int prev_mx, prev_my;
	static unsigned int prev_mask;
	int wx, wy;
	Window rootret, childret;
	unsigned int bmask;

	if(XQueryPointer(dpy, root, &rootret, &childret, x, y, &wx, &wy, &bmask)) {
		prev_mx = *x;
		prev_my = *y;
		prev_mask = bmask;
	} else {
		*x = prev_mx;
		*y = prev_my;
	}
	return prev_mask;
}

static Window create_xwindow(int width, int height, unsigned int flags)
{
	XSetWindowAttributes xattr;
	long xattr_mask, evmask;
	Window parent;
	Region rgn;
	XRectangle rect;

	if(!(visinf = choose_visual())) {
		fprintf(stderr, "failed to find appropriate visual\n");
		return 0;
	}

	xattr.colormap = XCreateColormap(dpy, root, visinf->visual, AllocNone);
	xattr.background_pixel = BlackPixel(dpy, scr);
	xattr_mask = CWColormap | CWBackPixel;

	parent = new_win_parent ? new_win_parent : root;

	if(!(win = XCreateWindow(dpy, parent, 0, 0, width, height, 0,
			visinf->depth, InputOutput, visinf->visual, xattr_mask, &xattr))) {
		fprintf(stderr, "failed to create X window\n");
		XFree(visinf);
		visinf = 0;
		return 0;
	}

	evmask = ExposureMask | StructureNotifyMask | KeyPressMask;
	XSelectInput(dpy, win, evmask);

	XmbSetWMProperties(dpy, win, "xlivebg", "xlivebg", 0, 0, 0, 0, 0);
	XSetWMProtocols(dpy, win, &xa_wm_delwin, 1);

	if(!(flags & WIN_REGULAR)) {
		netwm_setprop_atom(win, "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DESKTOP");
	}

#ifdef ShapeInput
	rgn = XCreateRegion();
	rect.x = rect.y = 0;
	rect.width = rect.height = 1;
	XUnionRectWithRegion(&rect, rgn, rgn);
	XShapeCombineRegion(dpy, win, ShapeInput, 0, 0, rgn, ShapeSet);
	XDestroyRegion(rgn);
#endif

	XMapWindow(dpy, win);
	return win;
}

static void netwm_setprop_atom(Window win, const char *prop, const char *val)
{
	Atom xa_prop, xa_val;

	xa_prop = XInternAtom(dpy, prop, False);
	xa_val = XInternAtom(dpy, val, False);

	XChangeProperty(dpy, win, xa_prop, XA_ATOM, 32, PropModeReplace, (unsigned char*)&xa_val, 1);
}

static XVisualInfo *choose_visual(void)
{
	int glxattr[32] = {
		GLX_RGBA, GLX_DOUBLEBUFFER,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DEPTH_SIZE, 1,
		/*
		GLX_SAMPLE_BUFFERS, 1,
		GLX_SAMPLES, 8,
		*/
		None
	};
	int *sample_buffers = glxattr + 10;
	int *num_samples = glxattr + 13;
	XVisualInfo *res;

	do {
		res = glXChooseVisual(dpy, scr, glxattr);
		*num_samples >>= 1;
		if(*num_samples <= 1) {
			*sample_buffers = None;
		}
	} while(!res && *num_samples > 0);

	return res;
}

int xlivebg_init_gl(void)
{
	XVisualInfo *vi, vitmpl;
	int numvi, val, rbits, gbits, bbits, zbits;
	XWindowAttributes wattr;

	XGetWindowAttributes(dpy, win, &wattr);
	vitmpl.visualid = XVisualIDFromVisual(wattr.visual);
	if(!(vi = XGetVisualInfo(dpy, VisualIDMask, &vitmpl, &numvi))) {
		fprintf(stderr, "failed to get info about the visual of the window\n");
		return -1;
	}
	assert(numvi == 1);

	printf("window size: %dx%d\n", wattr.width, wattr.height);

	glXGetConfig(dpy, vi, GLX_USE_GL, &val);
	if(!val) {
		fprintf(stderr, "window visual doesn't support OpenGL\n");
		XFree(vi);
		return -1;
	}
	glXGetConfig(dpy, vi, GLX_RGBA, &val);
	if(!val) {
		fprintf(stderr, "window visual isn't TrueColor\n");
		XFree(vi);
		return -1;
	}

	glXGetConfig(dpy, vi, GLX_DOUBLEBUFFER, &dblbuf);
	glXGetConfig(dpy, vi, GLX_RED_SIZE, &rbits);
	glXGetConfig(dpy, vi, GLX_GREEN_SIZE, &gbits);
	glXGetConfig(dpy, vi, GLX_BLUE_SIZE, &bbits);
	glXGetConfig(dpy, vi, GLX_DEPTH_SIZE, &zbits);

	printf("window visual: %lx\n", vitmpl.visualid);
	printf(" %s-buffered RGBA %d%d%d, %d bits zbuffer\n", dblbuf ? "double" : "single",
			rbits, gbits, bbits, zbits);

	if(!(ctx = glXCreateContext(dpy, vi, 0, 1))) {
		fprintf(stderr, "failed to create OpenGL context on window\n");
		XFree(vi);
		return -1;
	}
	glXMakeCurrent(dpy, win, ctx);
	init_opengl();
	app_reshape(wattr.width, wattr.height);
	XFree(vi);
	return 0;
}

void xlivebg_destroy_gl(void)
{
	destroy_all_textures();

	glXMakeCurrent(dpy, 0, 0);
	glXDestroyContext(dpy, ctx);
}

static void detect_outputs(void)
{
	int i;

	for(i=0; i<num_screens; i++) {
		free(screen[i].name);
	}

	num_screens = get_num_outputs(dpy);
	printf("detected %d outputs:\n", num_screens);
	for(i=0; i<num_screens; i++) {
		struct xlivebg_screen *scr = screen + i;
		get_output(dpy, i, scr);

		printf(" [%d] %s: %dx%d+%d+%d\n", i, scr->name ? scr->name : "UNK",
				scr->width, scr->height, scr->x, scr->y);
	}
}

static int proc_xevent(XEvent *ev)
{
	KeySym sym;

	switch(ev->type) {
	case MapNotify:
		mapped = 1;
		break;

	case UnmapNotify:
		mapped = 0;
		break;

	case ConfigureNotify:
		if(ev->xconfigure.width != win_width || ev->xconfigure.height != win_height) {
			win_width = ev->xconfigure.width;
			win_height = ev->xconfigure.height;
			app_reshape(win_width, win_height);
		}
		break;

	case KeyPress:
	case KeyRelease:
		if((sym = XLookupKeysym(&ev->xkey, 0))) {
			app_keyboard(sym, ev->type == KeyPress);
		}
		break;

	case ClientMessage:
		if(ev->xclient.message_type == xa_wm_proto) {
			if(ev->xclient.data.l[0] == xa_wm_delwin) {
				return -1;
			}
		}
		break;

	default:
#ifdef HAVE_XRANDR
		if(have_xrandr) {
			if(ev->type == xrandr_evbase + RRScreenChangeNotify) {
				printf("Video outputs changed, reconfiguring\n");
				XRRUpdateConfiguration(ev);
				detect_outputs();
			}
		}
#endif
		break;
	}

	return 0;
}

static void send_expose(Window win)
{
	XEvent ev = {0};

	XClearWindow(dpy, win);

	ev.type = Expose;
	ev.xexpose.width = win_width;
	ev.xexpose.height = win_height;
	ev.xexpose.send_event = True;

	printf("sending expose: %dx%d (xid: %x)\n", win_width, win_height, (unsigned int)win);
	XSendEvent(dpy, win, False, Expose, &ev);
}

static void sighandler(int s)
{
	quit = 1;
}

static int parse_args(int argc, char **argv)
{
	int i;
	char *endp;
	long val;

	for(i=1; i<argc; i++) {
		if(strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "-window") == 0) {
			win = 0;
			if(argv[++i]) {
				val = strtol(argv[i], &endp, 0);
				if(endp != argv[i] && val > 0) {
					win = (Window)val;
				}
			}
			if(!win) {
				fprintf(stderr, "%s must be followed by a window id (see xwininfo)\n", argv[i - 1]);
				return -1;
			}

		} else if(strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "-preview") == 0) {
			opt_new_win = 1;
			opt_preview = 1;
			win = 0;

		} else if(strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-root") == 0) {
			opt_new_win = 0;
			win = 0;

		} else if(strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "-new-win") == 0) {
			opt_new_win = 1;

			if(argv[i + 1] && (val = strtol(argv[i + 1], &endp, 0)) > 0 && endp != argv[i + 1]) {
				i++;
				new_win_parent = (Window)val;
			}

		} else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
			print_usage(argv[0]);
			exit(0);

		} else {
			fprintf(stderr, "invalid argument: %s\n", argv[i]);
			print_usage(argv[0]);
			return -1;
		}
	}

	return 0;
}

static void print_usage(const char *argv0)
{
	printf("Usage: %s [options]\n", argv0);
	printf("Options:\n");
	printf("  -n, -new-win: create own desktop window\n");
	printf("  -r, -root: draw on the root window (default)\n");
	printf("  -w <id>, -window <id>: draw on specified window\n");
	printf("  -p, -preview: show preview on a new window\n");
	printf("  -h, -help: print usage information and exit\n");
}
