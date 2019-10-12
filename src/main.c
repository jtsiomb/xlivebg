/*
xlivebg - live wallpapers for the X window system
Copyright (C) 2019  John Tsiombikas <nuclear@member.fsf.org>

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
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <GL/glx.h>
#include "app.h"
#include "xlivebg.h"
#include "cfg.h"

/* defined in toon_root.c */
Window ToonGetRootWindow(Display *dpy, int scr, Window *par);

static Window create_xwindow(int width, int height);
static XVisualInfo *choose_visual(void);
static int init_gl(void);
static void destroy_gl(void);
static void detect_outputs(void);
static int proc_xevent(XEvent *ev);
static void send_expose(Window win);
static void sighandler(int s);

static Display *dpy;
static int scr;
static Window win, root;
static GLXContext ctx;
static XVisualInfo *visinf;
static Atom xa_wm_proto, xa_wm_delwin;
static int have_xrandr, xrandr_evbase, xrandr_errbase;

static volatile int quit;
static int mapped;
static int width, height;
static int dblbuf;
static int opt_new_win;
static Window new_win_parent;

static struct timeval tv, tv0;

int main(int argc, char **argv)
{
	int i, xfd;
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
				return 1;
			}

		} else if(strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "-new-win") == 0) {
			opt_new_win = 1;

			if(argv[i + 1] && (val = strtol(argv[i + 1], &endp, 0)) > 0 && endp != argv[i + 1]) {
				i++;
				new_win_parent = (Window)val;
			}

		} else {
			fprintf(stderr, "invalid argument: %s\n", argv[i]);
			return 1;
		}
	}

	if(!(dpy = XOpenDisplay(0))) {
		fprintf(stderr, "failed to open connection to the X server\n");
		return 1;
	}
	xfd = ConnectionNumber(dpy);
	scr = DefaultScreen(dpy);
	root = RootWindow(dpy, scr);

	xa_wm_proto = XInternAtom(dpy, "WM_PROTOCOLS", False);
	xa_wm_delwin = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

	if(!win) {
		if(opt_new_win) {
			XWindowAttributes attr;
			XGetWindowAttributes(dpy, root, &attr);
			if(!(win = create_xwindow(attr.width, attr.height))) {
				return 1;
			}
		} else {
			Window parent;
			win = ToonGetRootWindow(dpy, scr, &parent);
			printf("detected root window: %x\n", (unsigned int)win);
		}
	}

	signal(SIGINT, sighandler);
	signal(SIGILL, sighandler);
	signal(SIGSEGV, sighandler);

	init_cfg();

	if(init_gl() == -1) {
		fprintf(stderr, "failed to create OpenGL context\n");
		XCloseDisplay(dpy);
		return 1;
	}
	XSelectInput(dpy, win, ExposureMask | StructureNotifyMask);

	if((have_xrandr = XRRQueryExtension(dpy, &xrandr_evbase, &xrandr_errbase))) {
		/* detect number of screens and initialize screen structures */
		detect_outputs();
		XRRSelectInput(dpy, root, RRScreenChangeNotifyMask);
	} else {
		num_screens = 1;
		screen[0].x = screen[0].y = 0;
		screen[0].width = screen[0].root_width = scr_width;
		screen[0].height = screen[0].root_height = scr_height;
		screen[0].aspect = (float)scr_width / (float)scr_height;
	}

	if(app_init(argc, argv) == -1) {
		destroy_gl();
		XCloseDisplay(dpy);
		return 1;
	}

	gettimeofday(&tv0, 0);

	while(!quit) {
		fd_set rdset;
		struct timeval *timeout;

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

		if(upd_interval_usec > 0) {
			struct timeval now;
			long usec;

			gettimeofday(&now, 0);

			usec = upd_interval_usec - (now.tv_sec - tv.tv_sec) * 1000000 - (now.tv_usec - tv.tv_usec);
			if(usec < 0) usec = 0;

			tv.tv_sec = usec / 1000000;
			tv.tv_usec = usec % 1000000;
			timeout = &tv;
		} else {
			timeout = 0;
		}

		select(xfd + 1, &rdset, 0, 0, timeout);
	}

done:
	send_expose(win);
	if(visinf) {
		XFree(visinf);
	}
	destroy_gl();
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

static Window create_xwindow(int width, int height)
{
	Atom net_wintype, net_wintype_desktop;
	XSetWindowAttributes xattr;
	long xattr_mask, evmask;
	Window parent;

	if(!(visinf = choose_visual())) {
		fprintf(stderr, "failed to find appropriate visual\n");
		return 0;
	}

	xattr.colormap = XCreateColormap(dpy, root, visinf->visual, AllocNone);
	xattr.background_pixel = BlackPixel(dpy, scr);
	xattr.override_redirect = 1;
	xattr_mask = CWColormap | CWBackPixel | CWOverrideRedirect;

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

	Xutf8SetWMProperties(dpy, win, "xlivebg", "xlivebg", 0, 0, 0, 0, 0);
	XSetWMProtocols(dpy, win, &xa_wm_delwin, 1);

	net_wintype = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
	net_wintype_desktop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
	XChangeProperty(dpy, win, net_wintype, XA_ATOM, 32, PropModeReplace,
			(unsigned char*)&net_wintype_desktop, 1);

	XMapWindow(dpy, win);
	XLowerWindow(dpy, win);
	return win;
}

static XVisualInfo *choose_visual(void)
{
	int glxattr[] = {
		GLX_RGBA, GLX_DOUBLEBUFFER,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DEPTH_SIZE, 1,
		GLX_SAMPLE_BUFFERS, 1,
		GLX_SAMPLES, 8,
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

static int init_gl(void)
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

	width = wattr.width;
	height = wattr.height;
	printf("window size: %dx%d\n", width, height);

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
	app_reshape(width, height);
	XFree(vi);
	return 0;
}

static void destroy_gl(void)
{
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

		scr->root_width = scr_width;
		scr->root_height = scr_height;
		scr->aspect = (float)scr->width / (float)scr->height;

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
		if(ev->xconfigure.width != width || ev->xconfigure.height != height) {
			width = ev->xconfigure.width;
			height = ev->xconfigure.height;
			app_reshape(width, height);
		}
		break;

	case KeyPress:
	case KeyRelease:
		if((sym = XLookupKeysym(&ev->xkey, 0))) {
			app_keyboard(sym, ev->type == KeyPress);
		}
		break;

	default:
		if(have_xrandr) {
			if(ev->type == xrandr_evbase + RRScreenChangeNotify) {
				printf("Video outputs changed, reconfiguring\n");
				XRRUpdateConfiguration(ev);
				detect_outputs();
			}
		}
		break;
	}

	return 0;
}

static void send_expose(Window win)
{
	XEvent ev = {0};

	XClearWindow(dpy, win);

	ev.type = Expose;
	ev.xexpose.width = width;
	ev.xexpose.height = height;
	ev.xexpose.send_event = True;

	printf("sending expose: %dx%d (xid: %x)\n", width, height, (unsigned int)win);
	XSendEvent(dpy, win, False, Expose, &ev);
}

static void sighandler(int s)
{
	quit = 1;
}
