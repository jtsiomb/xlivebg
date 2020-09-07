#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "xmutil.h"
#include "cmd.h"
#include "bg.h"

#define BNCOL_WIDTH	42
#define BNCOL_HEIGHT 32

static int init_gui(void);
static void create_menu(void);
static int create_bglist(Widget par);
static void file_menu_handler(Widget lst, void *cls, void *calldata);
static void help_menu_handler(Widget lst, void *cls, void *calldata);
static void bgselect_handler(Widget lst, void *cls, void *calldata);
static void bgimage_use_change(Widget w, void *cls, void *calldata);
static void bgimage_path_change(const char *path);
static void bgmask_use_change(Widget w, void *cls, void *calldata);
static void bgmask_path_change(const char *path);
static void colopt_change(Widget w, void *cls, void *calldata);
static void bncolor_handler(Widget w, void *cls, void *calldata);
static void gen_wallpaper_ui(void);

XtAppContext app;
Widget app_shell;

static Widget win;
static int use_bgimage, use_bgmask;
static Widget bn_endcol, frm_cur;

int main(int argc, char **argv)
{
	if(!(app_shell = XtVaOpenApplication(&app, "xlivebg-gui", 0, 0, &argc, argv,
					0, sessionShellWidgetClass, (void*)0))) {
		fprintf(stderr, "failed to initialize ui\n");
		return -1;
	}

	if(cmd_ping() == -1) {
		/* TODO messagebox */
		messagebox(XmDIALOG_ERROR, "Fatal error", "No response from xlivebg. Make sure it's running!");
		return 1;
	}

	win = XmCreateMainWindow(app_shell, "mainwin", 0, 0);
	XtManageChild(win);

	if(init_gui() == -1) {
		return 1;
	}

	XtRealizeWidget(app_shell);
	XtAppMainLoop(app);
	return 0;
}

static char *clean_path_str(char *s)
{
	char *end;
	while(*s && isspace(*s)) s++;

	end = s;
	while(*end && *end != '\n' && *end != '\r') end++;
	*end = 0;

	return s;
}

static int init_gui(void)
{
	Widget frm, subfrm, vbox, subvbox, hbox;
	char buf[512];
	char *str;

	create_menu();

	hbox = xm_rowcol(win, XmHORIZONTAL);

	frm = xm_frame(hbox, "Global settings");
	frm_cur = xm_frame(hbox, "Wallpaper settings");

	/* global settings */
	vbox = xm_rowcol(frm, XmVERTICAL);

	xm_label(vbox, "Wallpapers:");
	if(create_bglist(vbox) == -1) {
		return -1;
	}

	if(cmd_getprop_str("xlivebg.image", buf, sizeof buf) != -1) {
		str = clean_path_str(buf);
		use_bgimage = 1;
	} else {
		str = 0;
		use_bgimage = 0;
	}
	subfrm = xm_frame(vbox, "Background image");
	subvbox = xm_rowcol(subfrm, XmVERTICAL);
	xm_checkbox(subvbox, "Use image", use_bgimage, bgimage_use_change, 0);
	create_pathfield(subvbox, str, 0, bgimage_path_change);

	if(cmd_getprop_str("xlivebg.anim_mask", buf, sizeof buf) != -1) {
		str = clean_path_str(buf);
		use_bgmask = 1;
	} else {
		str = 0;
		use_bgmask = 0;
	}
	subfrm = xm_frame(vbox, "Animation mask");
	subvbox = xm_rowcol(subfrm, XmVERTICAL);
	xm_checkbox(subvbox, "Use mask", use_bgmask, bgmask_use_change, 0);
	create_pathfield(subvbox, str, 0, bgmask_path_change);

	subfrm = xm_frame(vbox, "Color");
	hbox = xm_rowcol(subfrm, XmHORIZONTAL);
	xm_va_option_menu(hbox, colopt_change, 0, "Solid color", "Vertical gradient", "Horizontal gradient", (void*)0);
	/*xm_button(hbox, "start", 0, 0);*/
	xm_drawn_button(hbox, BNCOL_WIDTH, BNCOL_HEIGHT, bncolor_handler, 0);
	/*bn_endcol = xm_button(hbox, "end", 0, 0);*/
	bn_endcol = xm_drawn_button(hbox, BNCOL_WIDTH, BNCOL_HEIGHT, bncolor_handler, 0);
	XtSetSensitive(bn_endcol, 0);

	gen_wallpaper_ui();

	return 0;
}

static void create_menu(void)
{
	XmString sfile, shelp, squit, sabout, squit_key;
	Widget menubar;

	sfile = XmStringCreateSimple("File");
	shelp = XmStringCreateSimple("Help");
	menubar = XmVaCreateSimpleMenuBar(win, "menubar", XmVaCASCADEBUTTON, sfile, 'F',
			XmVaCASCADEBUTTON, shelp, 'H', (void*)0);
	XtManageChild(menubar);
	XmStringFree(sfile);
	XmStringFree(shelp);

	squit = XmStringCreateSimple("Quit");
	squit_key = XmStringCreateSimple("Ctrl-Q");
	XmVaCreateSimplePulldownMenu(menubar, "filemenu", 0, file_menu_handler,
			XmVaPUSHBUTTON, squit, 'Q', "Ctrl<Key>q", squit_key, (void*)0);
	XmStringFree(squit);
	XmStringFree(squit_key);

	sabout = XmStringCreateSimple("About");
	XmVaCreateSimplePulldownMenu(menubar, "helpmenu", 1, help_menu_handler,
			XmVaPUSHBUTTON, sabout, 'A', 0, 0, (void*)0);
	XmStringFree(sabout);
}

static int create_bglist(Widget par)
{
	int i, num;
	struct bginfo *bg;
	Widget lst;

	if(bg_create_list() == -1) {
		return -1;
	}
	num = bg_list_size();

	lst = XmCreateScrolledList(par, "bglist", 0, 0);
	XtManageChild(lst);
	XtVaSetValues(lst, XmNselectionPolicy, XmSINGLE_SELECT, XmNvisibleItemCount, 5, (void*)0);
	for(i=0; i<num; i++) {
		bg = bg_list_item(i);
		XmString name = XmStringCreateSimple(bg->name);
		XmListAddItemUnselected(lst, name, 0);
		XmStringFree(name);
		printf("adding: %s\n", bg->name);
	}

	if((bg = bg_active())) {
		int line = bg - bg_list_item(0) + 1;
		XmListSelectPos(lst, line, False);
		if(line >= num - 5) {
			XmListSetBottomPos(lst, num);
		} else {
			XmListSetPos(lst, line < 5 ? line : line - 2);
		}
	}

	XtAddCallback(lst, XmNdefaultActionCallback, bgselect_handler, 0);
	XtAddCallback(lst, XmNsingleSelectionCallback, bgselect_handler, 0);
	return 0;
}

static void file_menu_handler(Widget lst, void *cls, void *calldata)
{
	/*int item = (int)cls;*/
	exit(0);
}


static void help_menu_handler(Widget lst, void *cls, void *calldata)
{
	static const char *about_text = "xlivebg configuration tool\n\n"
		"Copyright (C) 2020 John Tsiombikas <nuclear@member.fsf.org>\n\n"
		"xlivebg-gui is free software, feel free to use, modify and/or\n"
		"redistribute it, under the terms of the GNU General Public License\n"
		"version 3, or at your option any later version published by the\n"
		"Free Software Foundation.\n"
		"See http://www.gnu.org/licenses/gpl.txt for details.\n";
	messagebox(XmDIALOG_INFORMATION, "About xlivebg-gui", about_text);
}

static void bgselect_handler(Widget lst, void *cls, void *calldata)
{
	XmListCallbackStruct *cbs = calldata;
	char *selitem = XmStringUnparse(cbs->item, 0, 0, 0, 0, 0, XmOUTPUT_ALL);
	if(selitem) {
		bg_switch(selitem);
		XtFree(selitem);
		gen_wallpaper_ui();
	}
}

static void bgimage_use_change(Widget w, void *cls, void *calldata)
{
	Widget txf;
	char *str;

	if(XmToggleButtonGetState(w)) {
		use_bgimage = 1;
		txf = XtNameToWidget(XtParent(w), "rowcolumn.textfield");
		if(txf && (str = XmTextFieldGetString(txf))) {
			cmd_setprop_str("xlivebg.image", str);
		}
	} else {
		use_bgimage = 0;
	}
}

static void bgimage_path_change(const char *path)
{
	if(use_bgimage) {
		cmd_setprop_str("xlivebg.image", path);
	}
}

static void bgmask_use_change(Widget w, void *cls, void *calldata)
{
	Widget txf;
	char *str;

	if(XmToggleButtonGetState(w)) {
		use_bgmask = 1;
		txf = XtNameToWidget(XtParent(w), "rowcolumn.textfield");
		if(txf && (str = XmTextFieldGetString(txf))) {
			cmd_setprop_str("xlivebg.anim_mask", str);
		}
	} else {
		use_bgmask = 0;
	}
}

static void bgmask_path_change(const char *path)
{
	if(use_bgmask) {
		cmd_setprop_str("xlivebg.anim_mask", path);
	}
}

static void colopt_change(Widget w, void *cls, void *calldata)
{
	int idx;
	void *ptr;

	XtVaGetValues(w, XmNuserData, &ptr, (void*)0);
	idx = (int)ptr;

	if(idx <= 0) {
		XtSetSensitive(bn_endcol, 0);
	} else {
		XtSetSensitive(bn_endcol, 1);
	}
}

static void bncolor_handler(Widget w, void *cls, void *calldata)
{
	Display *dpy;
	Screen *scr;
	int scrn;
	Window win;
	GC gc;
	Colormap cmap;
	XColor col;
	int border;
	XmDrawnButtonCallbackStruct *cbs = calldata;

	switch(cbs->reason) {
	case XmCR_ACTIVATE:
		color_picker_dialog(0);
		break;

	case XmCR_EXPOSE:
		dpy = XtDisplay(w);
		win = XtWindow(w);
		scr = XtScreen(w);
		scrn = XScreenNumberOfScreen(scr);
		gc = XDefaultGCOfScreen(scr);
		cmap = DefaultColormapOfScreen(scr);
		border = xm_get_border_size(w);
		XtVaSetValues(w, XmNwidth, 2 * border + BNCOL_WIDTH,
				XmNheight, 2 * border + BNCOL_HEIGHT, (void*)0);

		if(XtIsSensitive(w)) {
			col.red = 65535;
			col.green = col.blue = 0;
			XAllocColor(dpy, cmap, &col);
			XSetForeground(dpy, gc, col.pixel);
		} else {
			col.red = col.green = col.blue = 32768;
			XAllocColor(dpy, cmap, &col);
			XSetForeground(dpy, gc, col.pixel);
		}
		XSetFillStyle(dpy, gc, FillSolid);
		XFillRectangle(dpy, win, gc, border, border, BNCOL_WIDTH, BNCOL_HEIGHT);

		if(!XtIsSensitive(w)) {
			static XSegment lseg[] = {
				{5, 5, BNCOL_WIDTH - 5, BNCOL_HEIGHT - 5},
				{5, BNCOL_HEIGHT - 5, BNCOL_WIDTH - 5, 5}
			};
			XSetLineAttributes(dpy, gc, 5, LineSolid, CapButt, JoinMiter);
			XSetForeground(dpy, gc, BlackPixel(dpy, scrn));
			XDrawSegments(dpy, win, gc, lseg, 2);
		}
		break;
	}
}


static void gen_wallpaper_ui(void)
{
	int i, bval;
	Widget w, vbox, hbox;
	struct bginfo *bg;
	XmString xmstr;
	char buf[1024];
	struct bgprop *prop;

	if(!(bg = bg_active())) {
		return;
	}

	if((vbox = XtNameToWidget(frm_cur, "rowcol"))) {
		XtDestroyWidget(vbox);
	}
	vbox = xm_rowcol(frm_cur, XmVERTICAL);

	if((w = XtNameToWidget(frm_cur, "label"))) {
		sprintf(buf, "Wallpaper settings: %s", bg->name);
		xmstr = XmStringCreateSimple(buf);
		XtVaSetValues(w, XmNlabelString, xmstr, (void*)0);
		XmStringFree(xmstr);
	}

	prop = bg->prop;
	for(i=0; i<bg->num_prop; i++) {
		sprintf(buf, "xlivebg.%s.%s", bg->name, prop->name);
		switch(prop->type) {
		case BGPROP_BOOL:
			if(cmd_getprop_int(buf, &bval) != -1) {
				xm_checkbox(vbox, prop->name, bval, 0, 0);
			}
			break;

		case BGPROP_TEXT:
			/* TODO handle multi-line text */
			if(cmd_getprop_str(buf, buf, sizeof buf) != -1) {
				hbox = xm_rowcol(vbox, XmHORIZONTAL);
				xm_label(hbox, prop->name);
				xm_textfield(hbox, 0, 0, 0);
			}
			break;

		case BGPROP_NUMBER:
			/* TODO slider */
			break;

		case BGPROP_INTEGER:
			/* TODO same */
			break;

		case BGPROP_COLOR:
			/* TODO color */
			break;

		case BGPROP_PATHNAME:
		case BGPROP_FILENAME:
		case BGPROP_DIRNAME:
			if(cmd_getprop_str(buf, buf, sizeof buf) != -1) {
				hbox = xm_rowcol(vbox, XmHORIZONTAL);
				xm_label(hbox, prop->name);
				create_pathfield(hbox, 0, 0, 0);
			}
			break;
		}

		prop++;
	}
}
