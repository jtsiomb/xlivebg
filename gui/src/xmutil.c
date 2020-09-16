#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "xmutil.h"

typedef unsigned int uint32;

static void rgb_to_hsv(float r, float g, float b, float *h, float *s, float *v);
static void hsv_to_rgb(float *r, float *g, float *b, float h, float s, float v);

extern XtAppContext app;
extern Widget app_shell;

Widget xm_label(Widget par, const char *text)
{
	Widget w;
	Arg arg;
	XmString str = XmStringCreateSimple((char*)text);
	XtSetArg(arg, XmNlabelString, str);
	w = XmCreateLabel(par, "label", &arg, 1);
	XmStringFree(str);
	XtManageChild(w);
	return w;
}

Widget xm_frame(Widget par, const char *title)
{
	Widget w;
	Arg args[2];
	XmString str = XmStringCreateSimple((char*)title);

	w = XmCreateFrame(par, "frame", 0, 0);
	XtSetArg(args[0], XmNframeChildType, XmFRAME_TITLE_CHILD);
	XtSetArg(args[1], XmNlabelString, str);
	XtManageChild(XmCreateLabelGadget(w, "label", args, 2));
	XtManageChild(w);
	XmStringFree(str);
	return w;
}

Widget xm_rowcol(Widget par, int orient)
{
	Widget w;
	Arg arg;

	XtSetArg(arg, XmNorientation, orient);
	w = XmCreateRowColumn(par, "rowcolumn", &arg, 1);
	XtManageChild(w);
	return w;
}

Widget xm_form(Widget par)
{
	Widget w = XmCreateForm(par, "form", 0, 0);
	XtManageChild(w);
	return w;
}

Widget xm_button(Widget par, const char *text, XtCallbackProc cb, void *cls)
{
	Widget w;
	Arg arg;
	XmString str = XmStringCreateSimple((char*)text);

	XtSetArg(arg, XmNlabelString, str);
	w = XmCreatePushButton(par, "button", &arg, 1);
	XmStringFree(str);
	XtManageChild(w);

	if(cb) {
		XtAddCallback(w, XmNactivateCallback, cb, cls);
	}
	return w;
}


Widget xm_drawn_button(Widget par, int width, int height, XtCallbackProc cb, void *cls)
{
	Widget w;
	int borders;

	w = XmCreateDrawnButton(par, "button", 0, 0);
	XtManageChild(w);

	borders = 2 * xm_get_border_size(w);

	XtVaSetValues(w, XmNwidth, borders + width, XmNheight, borders + height, (void*)0);

	if(cb) {
		XtAddCallback(w, XmNactivateCallback, cb, cls);
		XtAddCallback(w, XmNexposeCallback, cb, cls);
		XtAddCallback(w, XmNresizeCallback, cb, cls);
	}
	return w;
}

Widget xm_checkbox(Widget par, const char *text, int checked, XtCallbackProc cb, void *cls)
{
	Widget w;
	Arg arg;
	XmString str = XmStringCreateSimple((char*)text);

	XtSetArg(arg, XmNlabelString, str);
	w = XmCreateToggleButton(par, "checkbox", &arg, 1);
	XmToggleButtonSetState(w, checked, False);
	XmStringFree(str);
	XtManageChild(w);

	if(cb) {
		XtAddCallback(w, XmNvalueChangedCallback, cb, cls);
	}
	return w;
}

Widget xm_textfield(Widget par, const char *text, XtCallbackProc cb, void *cls)
{
	Widget w;

	w = XmCreateTextField(par, "textfield", 0, 0);
	XtManageChild(w);

	if(text) {
		XmTextFieldSetString(w, (char*)text);
	}
	if(cb) {
		XtAddCallback(w, XmNactivateCallback, cb, cls);
	}
	return w;
}

Widget xm_option_menu(Widget par)
{
	Widget w, wsub;
	Arg arg;

	w = XmCreatePulldownMenu(par, "pulldown", 0, 0);

	XtSetArg(arg, XmNsubMenuId, w);
	wsub = XmCreateOptionMenu(par, "optionmenu", &arg, 1);
	XtManageChild(wsub);
	return w;
}

Widget xm_va_option_menu(Widget par, XtCallbackProc cb, void *cls, ...)
{
	Widget w;
	va_list ap;
	char *s;
	int idx = 0;

	w = xm_option_menu(par);

	va_start(ap, cls);
	while((s = va_arg(ap, char*))) {
		Widget bn = xm_button(w, s, cb, cls);
		XtVaSetValues(bn, XmNuserData, (void*)idx++, (void*)0);
	}
	va_end(ap);

	return w;
}

Widget xm_sliderf(Widget par, const char *text, float val, float min, float max,
		XtCallbackProc cb, void *cls)
{
	Widget w;
	Arg argv[16];
	int argc = 0;
	XmString xmstr;
	float delta;
	int s = 1;
	int ndecimal = 0;

	if((delta = max - min) <= 1e-6) {
		return xm_label(par, "INVALID SLIDER RANGE");
	}
	while(delta < 100.0f) {
		delta *= 10.0f;
		s *= 10;
		ndecimal++;
	}

	if(text) {
		xmstr = XmStringCreateSimple((char*)text);
		XtSetArg(argv[argc], XmNtitleString, xmstr), argc++;
	}
	if(ndecimal) {
		XtSetArg(argv[argc], XmNdecimalPoints, ndecimal), argc++;
	}
	XtSetArg(argv[argc], XmNminimum, min * s), argc++;
	XtSetArg(argv[argc], XmNmaximum, max * s), argc++;
	XtSetArg(argv[argc], XmNvalue, val * s), argc++;
	XtSetArg(argv[argc], XmNshowValue, True), argc++;
	XtSetArg(argv[argc], XmNorientation, XmHORIZONTAL), argc++;

	w = XmCreateScale(par, "scale", argv, argc);
	XtManageChild(w);

	if(text) XmStringFree(xmstr);

	if(cb) {
		XtAddCallback(w, XmNdragCallback, cb, cls);
		XtAddCallback(w, XmNvalueChangedCallback, cb, cls);
	}
	return w;
}

Widget xm_slideri(Widget par, const char *text, int val, int min, int max,
		XtCallbackProc cb, void *cls)
{
	Widget w;
	Arg argv[16];
	int argc = 0;
	XmString xmstr;

	if(max <= min) {
		return xm_label(par, "INVALID SLIDER RANGE");
	}

	if(text) {
		xmstr = XmStringCreateSimple((char*)text);
		XtSetArg(argv[argc], XmNtitleString, xmstr), argc++;
	}
	XtSetArg(argv[argc], XmNminimum, min), argc++;
	XtSetArg(argv[argc], XmNmaximum, max), argc++;
	XtSetArg(argv[argc], XmNvalue, val), argc++;
	XtSetArg(argv[argc], XmNshowValue, True), argc++;
	XtSetArg(argv[argc], XmNorientation, XmHORIZONTAL), argc++;

	w = XmCreateScale(par, "scale", argv, argc);
	XtManageChild(w);

	if(text) XmStringFree(xmstr);

	if(cb) {
		XtAddCallback(w, XmNdragCallback, cb, cls);
		XtAddCallback(w, XmNvalueChangedCallback, cb, cls);
	}
	return w;

}

int xm_get_border_size(Widget w)
{
	Dimension highlight, shadow;

	XtVaGetValues(w, XmNhighlightThickness, &highlight,
			XmNshadowThickness, &shadow, (void*)0);

	return highlight + shadow;
}

void xm_attach_form(Widget w, unsigned int dirmask)
{
	Arg args[8];
	int i, count = 0;
	/* these must correspond to the XM_TOP, XM_BOTTOM, etc ... bits in xmutil.h */
	static char *attname[] = {
		XmNtopAttachment, XmNbottomAttachment, XmNleftAttachment, XmNrightAttachment
	};

	for(i=0; i<4; i++) {
		if(dirmask & 1) {
			XtSetArg(args[count], attname[i], XmATTACH_FORM);
			count++;
		}
		dirmask >>= 1;
	}

	if(count) {
		XtSetValues(w, args, count);
	}
}

static void *dirattach(unsigned int dir)
{
	switch(dir) {
	case XM_TOP: return XmNtopAttachment;
	case XM_BOTTOM: return XmNbottomAttachment;
	case XM_LEFT: return XmNleftAttachment;
	case XM_RIGHT: return XmNrightAttachment;
	}
	return 0;
}
static void *dirwidget(unsigned int dir)
{
	switch(dir) {
	case XM_TOP: return XmNtopWidget;
	case XM_BOTTOM: return XmNbottomWidget;
	case XM_LEFT: return XmNleftWidget;
	case XM_RIGHT: return XmNrightWidget;
	}
	return 0;
}

void xm_attach_widget(Widget w, unsigned int dir, Widget wtarg)
{
	XtVaSetValues(w, dirattach(dir), XmATTACH_WIDGET, dirwidget(dir), wtarg, (void*)0);
}

static void filesel_handler(Widget dlg, void *cls, void *calldata);

const char *file_dialog(Widget shell, const char *start_dir, const char *filter, char *buf, int bufsz)
{
	Widget dlg;
	Arg argv[3];
	int argc = 0;
	XmString xmstr_startdir = 0, xmstr_filter = 0;

	if(start_dir && *start_dir) {
		xmstr_startdir = XmStringCreateSimple((char*)start_dir);
		XtSetArg(argv[argc], XmNdirectory, xmstr_startdir), argc++;
	}
	if(filter && *filter) {
		xmstr_filter = XmStringCreateSimple((char*)filter);
		XtSetArg(argv[argc], XmNdirMask, xmstr_filter), argc++;
	}
	XtSetArg(argv[argc], XmNpathMode, XmPATH_MODE_RELATIVE), argc++;

	if(bufsz < sizeof bufsz) {
		fprintf(stderr, "file_dialog: insufficient buffer size: %d\n", bufsz);
		return 0;
	}
	memcpy(buf, &bufsz, sizeof bufsz);

	dlg = XmCreateFileSelectionDialog(app_shell, "filesb", argv, argc);
	XtAddCallback(dlg, XmNcancelCallback, filesel_handler, 0);
	XtAddCallback(dlg, XmNokCallback, filesel_handler, buf);
	XtManageChild(dlg);

	if(xmstr_startdir) XmStringFree(xmstr_startdir);
	if(xmstr_filter) XmStringFree(xmstr_filter);

	while(XtIsManaged(dlg)) {
		XtAppProcessEvent(app, XtIMAll);
	}

	return *buf ? buf : 0;
}

static void filesel_handler(Widget dlg, void *cls, void *calldata)
{
	char *fname;
	char *buf = cls;
	int bufsz;
	XmFileSelectionBoxCallbackStruct *cbs = calldata;

	if(buf) {
		memcpy(&bufsz, buf, sizeof bufsz);
		*buf = 0;

		if(!(fname = XmStringUnparse(cbs->value, XmFONTLIST_DEFAULT_TAG, XmCHARSET_TEXT,
						XmCHARSET_TEXT, 0, 0, XmOUTPUT_ALL))) {
			return;
		}

		strncpy(buf, fname, bufsz - 1);
		buf[bufsz - 1] = 0;
		XtFree(fname);
	}
	XtUnmanageChild(dlg);
}


static void pathfield_browse(Widget bn, void *cls, void *calldata);
static void pathfield_modify(Widget txf, void *cls, void *calldata);

Widget create_pathfield(Widget par, const char *defpath, const char *filter,
		void (*handler)(const char*, void*), void *cls)
{
	Widget hbox, tx_path;
	Arg args[3];

	hbox = xm_rowcol(par, XmHORIZONTAL);

	XtSetArg(args[0], XmNcolumns, 40);
	XtSetArg(args[1], XmNeditable, 0);
	XtSetArg(args[2], XmNuserData, cls);
	tx_path = XmCreateTextField(hbox, "textfield", args, 3);
	XtManageChild(tx_path);
	if(defpath) XmTextFieldSetString(tx_path, (char*)defpath);
	XtAddCallback(tx_path, XmNvalueChangedCallback, pathfield_modify, (void*)handler);

	xm_button(hbox, "...", pathfield_browse, tx_path);
	return tx_path;
}

static void pathfield_browse(Widget bn, void *cls, void *calldata)
{
	char buf[512];
	char *s, *src, *dst, *lastslash, *initdir = 0;

	if((s = XmTextFieldGetString(cls)) && *s) {
		lastslash = 0;
		src = s;
		dst = buf;
		while(*src && src - s < sizeof buf - 1) {
			if(*src == '/') lastslash = dst;
			*dst++ = *src++;
		}
		*dst = 0;

		if(lastslash) *lastslash = 0;
		if(*buf) {
			initdir = buf;
		}
	}

	if(file_dialog(app_shell, initdir, 0, buf, sizeof buf)) {
		XmTextFieldSetString(cls, buf);
	}
}

static void pathfield_modify(Widget txf, void *cls, void *calldata)
{
	void *udata;
	void (*usercb)(const char*, void*) = (void (*)(const char*, void*))cls;

	char *text = XmTextFieldGetString(txf);
	if(usercb) {
		XtVaGetValues(txf, XmNuserData, &udata, (void*)0);
		usercb(text, udata);
	}
	XtFree(text);
}

struct colbn_data {
	Display *dpy;
	Screen *scr;
	int scrn;
	Window win;
	GC gc;
	int border, width, height;

	unsigned short color[3];
	void (*usercb)(int, int, int, void*);
	void *userdata;
};

static void colbn_destructor(Widget bn, void *cls, void *calldata)
{
	struct colbn_data *cdata = cls;
	if(cdata->gc) XFreeGC(cdata->dpy, cdata->gc);
	free(cdata);
}

static void colbn_handler(Widget bn, void *cls, void *calldata)
{
	XColor xcol;
	Colormap cmap;
	struct colbn_data *cdata = cls;
	XmDrawnButtonCallbackStruct *cbs = calldata;
	XSegment lseg[2];

	if(!cdata->dpy) {
		cdata->dpy = XtDisplay(bn);
		cdata->win = XtWindow(bn);
		cdata->scr = XtScreen(bn);
		cdata->scrn = XScreenNumberOfScreen(cdata->scr);
		cdata->gc = XCreateGC(cdata->dpy, cdata->win, 0, 0);

		XSetFillStyle(cdata->dpy, cdata->gc, FillSolid);
		XSetLineAttributes(cdata->dpy, cdata->gc, 5, LineSolid, CapButt, JoinMiter);
	}

	switch(cbs->reason) {
	case XmCR_ACTIVATE:
		if(!color_picker_dialog(cdata->color)) break;

		if(cdata->usercb) {
			cdata->usercb(cdata->color[0], cdata->color[1], cdata->color[2], cdata->userdata);
		}
		/* XXX try to just fallthrough instead? */
		XClearArea(cdata->dpy, cdata->win, 0, 0, 0, 0, True);	/* force expose */
		break;

	case XmCR_EXPOSE:
		cmap = DefaultColormapOfScreen(cdata->scr);

		if(XtIsSensitive(bn)) {
			xcol.red = cdata->color[0];
			xcol.green = cdata->color[1];
			xcol.blue = cdata->color[2];
			XAllocColor(cdata->dpy, cmap, &xcol);
			XSetForeground(cdata->dpy, cdata->gc, xcol.pixel);
			XFillRectangle(cdata->dpy, cdata->win, cdata->gc, cdata->border, cdata->border,
					cdata->width, cdata->height);
		} else {
			xcol.red = xcol.green = xcol.blue = 32768;
			XAllocColor(cdata->dpy, cmap, &xcol);
			XSetForeground(cdata->dpy, cdata->gc, xcol.pixel);
			XFillRectangle(cdata->dpy, cdata->win, cdata->gc, cdata->border, cdata->border,
					cdata->width, cdata->height);

			lseg[0].x1 = lseg[0].y1 = 5;
			lseg[0].x2 = cdata->width - 5;
			lseg[0].y2 = cdata->height - 5;
			lseg[1].x1 = lseg[1].y2 = 5;
			lseg[1].y1 = cdata->height - 5;
			lseg[1].x2 = cdata->width - 5;

			XSetForeground(cdata->dpy, cdata->gc, BlackPixel(cdata->dpy, cdata->scrn));
			XDrawSegments(cdata->dpy, cdata->win, cdata->gc, lseg, 2);
		}
		break;
	}
}

Widget color_button(Widget par, int width, int height, int r, int g, int b,
		void (*handler)(int, int, int, void*), void *cls)
{
	Widget bn;
	struct colbn_data *cdata;

	if(!(cdata = calloc(1, sizeof *cdata))) {
		fprintf(stderr, "xmutil: color_button failed to allocate colbn_data buffer\n");
		return 0;
	}
	cdata->color[0] = r;
	cdata->color[1] = g;
	cdata->color[2] = b;
	cdata->usercb = handler;
	cdata->userdata = cls;

	bn = xm_drawn_button(par, width, height, colbn_handler, cdata);
	XtAddCallback(bn, XmNdestroyCallback, colbn_destructor, cdata);

	cdata->border = xm_get_border_size(bn);
	cdata->width = width;
	cdata->height = height;

	return bn;
}

static char *msgbox_text;
static int msgbox_textsz;

#define MSGBOX_FORMAT_TEXT(fmt, dofail) \
	do {	\
		char *tmp; \
		int len, newsz; \
		va_list ap;	\
		if(!msgbox_text) { \
			msgbox_textsz = 256; \
			if(!(msgbox_text = malloc(msgbox_textsz))) { \
				perror("Failed to allocate messagebox text buffer"); \
				dofail; \
			} \
		} \
		for(;;) {	\
			va_start(ap, fmt);	\
			len = vsnprintf(msgbox_text, msgbox_textsz, fmt, ap);	\
			va_end(ap);	\
			if(len == strlen(msgbox_text)) break;	\
			newsz = len == -1 ? msgbox_textsz << 1 : len + 1;	\
			if(!(tmp = malloc(newsz))) {	\
				fprintf(stderr, "Failed to resize messagebox buffer to %d bytes\n", newsz);	\
				dofail;	\
			}	\
			free(msgbox_text);	\
			msgbox_text = tmp;	\
			msgbox_textsz = newsz;	\
		}	\
	} while(0)

void messagebox(int type, const char *title, const char *msg, ...)
{
	XmString stitle, smsg;
	Widget dlg;

	MSGBOX_FORMAT_TEXT(msg, return);

	stitle = XmStringCreateSimple((char*)title);
	smsg = XmStringCreateLtoR(msgbox_text, XmFONTLIST_DEFAULT_TAG);

	switch(type) {
	case XmDIALOG_WARNING:
		dlg = XmCreateInformationDialog(app_shell, "warnmsg", 0, 0);
		break;
	case XmDIALOG_ERROR:
		dlg = XmCreateErrorDialog(app_shell, "errormsg", 0, 0);
		break;
	case XmDIALOG_INFORMATION:
	default:
		dlg = XmCreateInformationDialog(app_shell, "infomsg", 0, 0);
		break;
	}
	XtVaSetValues(dlg, XmNdialogTitle, stitle, XmNmessageString, smsg, (void*)0);
	XtVaSetValues(dlg, XmNdialogStyle, XmDIALOG_APPLICATION_MODAL, (void*)0);
	XmStringFree(stitle);
	XmStringFree(smsg);
	XtUnmanageChild(XmMessageBoxGetChild(dlg, XmDIALOG_HELP_BUTTON));
	XtUnmanageChild(XmMessageBoxGetChild(dlg, XmDIALOG_CANCEL_BUTTON));
	XtManageChild(dlg);

	while(XtIsManaged(dlg)) {
		XtAppProcessEvent(app, XtIMAll);
	}
}

static void qdlg_handler(Widget dlg, void *cls, void *calldata)
{
	int *resp = cls;
	*resp = 1;
}

int questionbox(const char *title, const char *msg, ...)
{
	XmString stitle, smsg;
	Widget dlg;
	Arg argv[16];
	int argc = 0;
	int resp = 0;

	MSGBOX_FORMAT_TEXT(msg, return -1);

	stitle = XmStringCreateSimple((char*)title);
	smsg = XmStringCreateLtoR(msgbox_text, XmFONTLIST_DEFAULT_TAG);

	XtSetArg(argv[argc], XmNdialogTitle, stitle), argc++;
	XtSetArg(argv[argc], XmNmessageString, smsg), argc++;
	XtSetArg(argv[argc], XmNdialogStyle, XmDIALOG_APPLICATION_MODAL), argc++;
	dlg = XmCreateQuestionDialog(app_shell, "questiondlg", argv, argc);
	XmStringFree(stitle);
	XmStringFree(smsg);
	XtUnmanageChild(XmMessageBoxGetChild(dlg, XmDIALOG_HELP_BUTTON));

	XtAddCallback(dlg, XmNokCallback, qdlg_handler, &resp);
	XtManageChild(dlg);

	while(XtIsManaged(dlg)) {
		XtAppProcessEvent(app, XtIMAll);
	}
	return resp;
}


/* ---- color selection dialog ---- */
struct coldlg_data {
	Display *dpy;
	Screen *scr;
	Window root;
	GC gc;
	XImage ximg;
	Widget cbox;
	Widget rgbslider[3];
	float hsv[3];
	int label_width, status;
};

static void update_colbox_image(struct coldlg_data *cdata);

#define CBOX_WIDTH		180
#define CBOX_HEIGHT		180
#define HUEBAR_WIDTH	32
#define HUEBAR_HEIGHT	CBOX_HEIGHT
#define COLSEL_HEIGHT	32
#define COLOR_WIDGET_WIDTH	(CBOX_WIDTH + HUEBAR_WIDTH)
#define COLOR_WIDGET_HEIGHT (CBOX_HEIGHT + COLSEL_HEIGHT)

static void coldlg_handler(Widget dlg, void *cls, void *calldata)
{
	struct coldlg_data *cdata = cls;

	if(cdata) cdata->status = 1;

	XtUnmanageChild(dlg);
}

static void colbox_expose(Widget cbox, void *cls, void *calldata)
{
	XColor xcol;
	unsigned long bgcol;
	struct coldlg_data *cdata = cls;
	Window win = XtWindow(cbox);
	Colormap cmap;
	float rgb[3];

	XPutImage(cdata->dpy, win, cdata->gc, &cdata->ximg, 0, 0, 0, 0, COLOR_WIDGET_WIDTH, CBOX_HEIGHT);

	XtVaGetValues(cdata->cbox, XmNbackground, &bgcol, (void*)0);
	XSetForeground(cdata->dpy, cdata->gc, bgcol);
	XFillRectangle(cdata->dpy, win, cdata->gc, 0, CBOX_HEIGHT, COLOR_WIDGET_WIDTH, COLSEL_HEIGHT);

	hsv_to_rgb(rgb, rgb + 1, rgb + 2, cdata->hsv[0], cdata->hsv[1], cdata->hsv[2]);
	cmap = DefaultColormap(cdata->dpy, XScreenNumberOfScreen(cdata->scr));
	xcol.red = rgb[0] * 65535.0f;
	xcol.green = rgb[1] * 65535.0f;
	xcol.blue = rgb[2] * 65535.0f;
	XAllocColor(cdata->dpy, cmap, &xcol);
	XSetForeground(cdata->dpy, cdata->gc, xcol.pixel);
	XFillRectangle(cdata->dpy, win, cdata->gc, cdata->label_width * 3 / 2, CBOX_HEIGHT + 5,
			COLOR_WIDGET_WIDTH / 3, COLSEL_HEIGHT - 5);
}

static void colbox_mouse(Widget widget, XEvent *xev, char **argv, unsigned int *argcptr)
{
	int x, y;
	float r, g, b;
	struct coldlg_data *cdata;

	XtVaGetValues(widget, XmNuserData, &cdata, (void*)0);

	switch(xev->type) {
	case ButtonPressMask:
		x = xev->xbutton.x;
		y = xev->xbutton.y;
		if(0) {
	case MotionNotify:
			x = xev->xmotion.x;
			y = xev->xmotion.y;
		}

		if(x < 0 || x >= COLOR_WIDGET_WIDTH || y < 0 || y >= CBOX_HEIGHT) {
			return;
		}

		if(x < CBOX_WIDTH) {
			cdata->hsv[1] = (float)x / (float)CBOX_WIDTH;
			cdata->hsv[2] = 1.0f - (float)y / (float)CBOX_HEIGHT;
		} else {
			cdata->hsv[0] = 1.0f - (float)y / (float)HUEBAR_HEIGHT;
		}
		update_colbox_image(cdata);

		hsv_to_rgb(&r, &g, &b, cdata->hsv[0], cdata->hsv[1], cdata->hsv[2]);
		XmScaleSetValue(cdata->rgbslider[0], r * 255.0f);
		XmScaleSetValue(cdata->rgbslider[1], g * 255.0f);
		XmScaleSetValue(cdata->rgbslider[2], b * 255.0f);
	}
}

#define H_THRES		(1.0f / HUEBAR_HEIGHT)
#define S_THRES		(1.0f / CBOX_WIDTH)
#define V_THRES		(1.0f / CBOX_HEIGHT)

static void update_colbox_image(struct coldlg_data *cdata)
{
	int i, j, r, g, b;
	float h, s, v, sel_h, sel_s, sel_v;
	float color[3];
	uint32 *pixels, *pptr, pcol;
	XColor xcol;
	Colormap cmap;

	sel_h = cdata->hsv[0];
	sel_s = cdata->hsv[1];
	sel_v = cdata->hsv[2];

	cmap = DefaultColormap(cdata->dpy, XScreenNumberOfScreen(cdata->scr));
	XtVaGetValues(cdata->cbox, XmNbackground, &xcol.pixel, (void*)0);
	XQueryColor(cdata->dpy, cmap, &xcol);

	pptr = pixels = (uint32*)cdata->ximg.data;

	for(i=0; i<CBOX_HEIGHT; i++) {
		v = 1.0f - (float)i / (float)CBOX_HEIGHT;
		for(j=0; j<CBOX_WIDTH; j++) {
			s = (float)j / (float)CBOX_WIDTH;

			hsv_to_rgb(color, color + 1, color + 2, sel_h, s, v);
			r = color[0] * 255.0f;
			g = color[1] * 255.0f;
			b = color[2] * 255.0f;
			pcol = (r << 16) | (g << 8) | b;

			if(fabs(v - sel_v) <= V_THRES || fabs(s - sel_s) <= S_THRES) {
				pcol = ~pcol;
			}

			*pptr++ = pcol;
		}
		pptr += HUEBAR_WIDTH;
	}

	pptr = pixels + CBOX_WIDTH;
	for(i=0; i<HUEBAR_HEIGHT; i++) {
		h = 1.0f - (float)i / (float)HUEBAR_HEIGHT;

		hsv_to_rgb(color, color + 1, color + 2, h, 1.0f, 1.0f);
		r = color[0] * 255.0f;
		g = color[1] * 255.0f;
		b = color[2] * 255.0f;
		pcol = (r << 16) | (g << 8) | b;

		if(fabs(h - sel_h) <= H_THRES) {
			pcol = ~pcol;
		}

		for(j=0; j<5; j++) {
			pptr[j] = xcol.pixel;
		}
		for(j=5; j<HUEBAR_WIDTH; j++) {
			pptr[j] = pcol;
		}
		pptr += COLOR_WIDGET_WIDTH;
	}

	colbox_expose(cdata->cbox, cdata, 0);
}


static void coldlg_rgbslider(Widget slider, void *cls, void *calldata)
{
	int r, g, b;
	struct coldlg_data *cdata = cls;

	XmScaleGetValue(cdata->rgbslider[0], &r);
	XmScaleGetValue(cdata->rgbslider[1], &g);
	XmScaleGetValue(cdata->rgbslider[2], &b);

	rgb_to_hsv(r / 255.0f, g / 255.0f, b / 255.0f, cdata->hsv, cdata->hsv + 1, cdata->hsv + 2);
	update_colbox_image(cdata);
}

int color_picker_dialog(unsigned short *col)
{
	static unsigned short defcol[3];
	Widget w, dlg, frm, cbox, rslider, gslider, bslider;
	Dimension width, height;
	Arg args[16];
	XmString xs_ok, xs_cancel;
	struct coldlg_data cdata;
	static const char *transl_str =
		"<Btn1Down>: colbox_mouse()\n"
		"<Btn1Up>: colbox_mouse()\n"
		"<Btn1Motion>: colbox_mouse()\n";
	static int actions_registered;

	if(!actions_registered) {
		XtActionsRec act;

		act.string = "colbox_mouse";
		act.proc = colbox_mouse;

		XtAppAddActions(app, &act, 1);
		actions_registered = 1;
	}


	xs_ok = XmStringCreateSimple("OK");
	xs_cancel = XmStringCreateSimple("Cancel");

	XtSetArg(args[0], XmNokLabelString, xs_ok);
	XtSetArg(args[1], XmNcancelLabelString, xs_cancel);
	dlg = XmCreateTemplateDialog(app_shell, "colordlg", args, 2);

	XtAddCallback(dlg, XmNokCallback, coldlg_handler, &cdata);
	XtAddCallback(dlg, XmNcancelCallback, coldlg_handler, 0);

	frm = XmCreateForm(dlg, "form", 0, 0);
	XtManageChild(frm);

	/* color selector widgets */
	XtSetArg(args[0], XmNtranslations, XtParseTranslationTable(transl_str));
	XtSetArg(args[1], XmNwidth, COLOR_WIDGET_WIDTH);
	XtSetArg(args[2], XmNheight, COLOR_WIDGET_HEIGHT);
	XtSetArg(args[3], XmNresizePolicy, XmRESIZE_NONE);
	cbox = XmCreateDrawingArea(frm, "colorbox", args, 4);
	XtVaSetValues(cbox, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, (void*)0);
	XtVaSetValues(cbox, XmNuserData, &cdata, (void*)0);

	XtAddCallback(cbox, XmNexposeCallback, colbox_expose, &cdata);

	cdata.dpy = XtDisplay(cbox);
	cdata.scr = XtScreen(cbox);
	cdata.root = RootWindowOfScreen(cdata.scr);

	cdata.gc = XCreateGC(cdata.dpy, cdata.root, 0, 0);

	memset(&cdata.ximg, 0, sizeof cdata.ximg);
	cdata.ximg.width = COLOR_WIDGET_WIDTH;
	cdata.ximg.height = COLOR_WIDGET_HEIGHT;
	cdata.ximg.format = ZPixmap;
	cdata.ximg.data = malloc(COLOR_WIDGET_WIDTH * COLOR_WIDGET_HEIGHT * 4);
	cdata.ximg.byte_order = cdata.ximg.bitmap_bit_order = LSBFirst;	/* XXX */
	cdata.ximg.bitmap_unit = 8;
	cdata.ximg.bitmap_pad = 8;
	cdata.ximg.depth = 24;
	cdata.ximg.bits_per_pixel = 32;
	cdata.ximg.bytes_per_line = COLOR_WIDGET_WIDTH * 4;
	cdata.ximg.red_mask = 0xff0000;
	cdata.ximg.green_mask = 0xff00;
	cdata.ximg.blue_mask = 0xff;
	XInitImage(&cdata.ximg);
	XtManageChild(cbox);

	cdata.cbox = cbox;
	cdata.status = 0;

	if(!col) col = defcol;
	rgb_to_hsv(col[0] / 65535.0f, col[1] / 65535.0f, col[2] / 65535.0f,
			cdata.hsv, cdata.hsv + 1, cdata.hsv + 2);

	w = xm_label(cbox, "Color:");
	XtVaGetValues(w, XmNwidth, &width, XmNheight, &height, (void*)0);
	XtVaSetValues(w, XmNx, 0, XmNy, (CBOX_HEIGHT + 5 + COLOR_WIDGET_HEIGHT - height) / 2, (void*)0);
	cdata.label_width = width;

	/* RGB sliders */
	rslider = xm_slideri(frm, "Red", (int)col[0] * 255 / 65535, 0, 255, coldlg_rgbslider, &cdata);
	gslider = xm_slideri(frm, "Green", (int)col[1] * 255 / 65535, 0, 255, coldlg_rgbslider, &cdata);
	bslider = xm_slideri(frm, "Blue", (int)col[2] * 255 / 65535, 0, 255, coldlg_rgbslider, &cdata);

	XtVaSetValues(rslider, XmNtopAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM,
			XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, cbox, (void*)0);
	XtVaSetValues(gslider, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, rslider,
			XmNleftAttachment, XmATTACH_WIDGET,	XmNleftWidget, cbox,
			XmNrightAttachment, XmATTACH_FORM, (void*)0);
	XtVaSetValues(bslider, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, gslider,
			XmNleftAttachment, XmATTACH_WIDGET,	XmNleftWidget, cbox,
			XmNrightAttachment, XmATTACH_FORM, (void*)0);

	cdata.rgbslider[0] = rslider;
	cdata.rgbslider[1] = gslider;
	cdata.rgbslider[2] = bslider;

	XtManageChild(dlg);

	update_colbox_image(&cdata);

	while(XtIsManaged(dlg)) {
		XtAppProcessEvent(app, XtIMAll);
	}

	free(cdata.ximg.data);
	XFreeGC(cdata.dpy, cdata.gc);

	if(cdata.status) {
		float rgb[3];
		hsv_to_rgb(rgb, rgb + 1, rgb + 2, cdata.hsv[0], cdata.hsv[1], cdata.hsv[2]);
		col[0] = rgb[0] * 65535.0f;
		col[1] = rgb[1] * 65535.0f;
		col[2] = rgb[2] * 65535.0f;
	}
	return cdata.status;
}


static inline float min3(float a, float b, float c)
{
	float ret = a;
	if (b < ret) ret = b;
	if (c < ret) ret = c;
	return ret;
}

static inline float max3(float a, float b, float c)
{
	float ret = a;
	if (b > ret) ret = b;
	if (c > ret) ret = c;
	return ret;
}

/* rgb_to_hsv and hsv_to_rgb written by samurai for ubertk in the long long ago */
static void rgb_to_hsv(float r, float g, float b, float *h, float *s, float *v)
{
	float min, max, delta;

	min = min3( r, g, b );
	max = max3( r, g, b );
	*v = max;

	delta = max - min;

	if( max != 0 )
		*s = delta / max;
	else {
		*s = 0;
		*h = -1;
		return;
	}

	if(!delta) delta = 1.0f;

	if( r == max )
		*h = ( g - b ) / delta;
	else if( g == max )
		*h = 2 + ( b - r ) / delta;
	else
		*h = 4 + ( r - g ) / delta;

	*h *= 60;
	if( *h < 0 )
		*h += 360;

	*h /= 360;
}


#define RETRGB(red, green, blue) \
	do { \
		*r = (red); \
		*g = (green); \
		*b = (blue); \
		return; \
	} while(0)

static void hsv_to_rgb(float *r, float *g, float *b, float h, float s, float v)
{
	float sec, frac, o, p, q;
	int hidx;

	if(s == 0.0f) {
		*r = *g = *b = v;
		return;
	}

	sec = floor(h * (360.0f / 60.0f));
	frac = (h * (360.0f / 60.0f)) - sec;

	o = v * (1.0f - s);
	p = v * (1.0f - s * frac);
	q = v * (1.0f - s * (1.0f - frac));

	hidx = (int)sec;
	switch(hidx) {
	default:
	case 0: RETRGB(v, q, o);
	case 1: RETRGB(p, v, o);
	case 2: RETRGB(o, v, q);
	case 3: RETRGB(o, p, v);
	case 4: RETRGB(q, o, v);
	case 5: RETRGB(v, o, p);
	}
}

