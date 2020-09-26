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
static void savecfg_handler(Widget w, void *cls, void *calldata);
static void file_menu_handler(Widget lst, void *cls, void *calldata);
static void help_menu_handler(Widget lst, void *cls, void *calldata);
static void bgselect_handler(Widget lst, void *cls, void *calldata);
static void bgimage_use_change(Widget w, void *cls, void *calldata);
static void bgimage_path_change(const char *path, void *cls);
static void bgmask_use_change(Widget w, void *cls, void *calldata);
static void bgmask_path_change(const char *path, void *cls);
static void colopt_change(Widget w, void *cls, void *calldata);
static void color_change(int r, int g, int b, void *cls);
static void numprop_change(Widget w, void *cls, void *calldata);
static void intprop_change(Widget w, void *cls, void *calldata);
static void pathprop_change(const char *path, void *cls);
static void fitmode_change(Widget w, void *cls, void *calldata);
static void cropzoom_change(Widget w, void *cls, void *calldata);
static void cropdir_change(Widget w, void *cls, void *calldata);
static void forcefps_handler(Widget w, void *cls, void *calldata);
static void fps_change(Widget w, void *cls, void *calldata);
static void gen_wallpaper_ui(void);
static void set_status(const char *s);

XtAppContext app;
Widget app_shell;

static Widget win;
static int use_bgimage, use_bgmask;
static Widget bn_endcol, frm_cur;
static Widget lb_status;
static Widget slider_fps;
static int user_fps = 30;
static float bgcol[2][4];
static float cropdir[4];
static Widget frm_cropopt;

int main(int argc, char **argv)
{
	if(!(app_shell = XtVaOpenApplication(&app, "xlivebg-gui", 0, 0, &argc, argv,
					0, sessionShellWidgetClass, (void*)0))) {
		fprintf(stderr, "failed to initialize ui\n");
		return -1;
	}
	XtVaSetValues(app_shell, XmNallowShellResize, True, (void*)0);

	if(cmd_ping() == -1) {
		messagebox(XmDIALOG_ERROR, "Fatal error", "No response from xlivebg. Make sure it's running!");
		return 1;
	}

	win = XmCreateMainWindow(app_shell, "mainwin", 0, 0);
	XtManageChild(win);

	if(init_gui() == -1) {
		return 1;
	}

	lb_status = xm_label(win, "");
	XtVaSetValues(win, XmNmessageWindow, lb_status, (void*)0);

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
	Widget bn, lb, chkbox, slider, frm, subfrm, optmenu;
	Widget vbox, subvbox, hbox, form, rightform;
	char buf[512];
	char *str;
	float fval;
	int forcefps, fit, bgmode;

	create_menu();

	form = xm_form(win, 0);

	frm = xm_frame(form, "Global settings");
	rightform = xm_form(form, 0);

	xm_attach_form(frm, XM_TOP | XM_LEFT | XM_BOTTOM);
	xm_attach_form(rightform, XM_TOP | XM_RIGHT | XM_BOTTOM);
	xm_attach_widget(rightform, XM_LEFT, frm);

	frm_cur = xm_frame(rightform, "Wallpaper settings");
	xm_attach_form(frm_cur, XM_TOP | XM_LEFT | XM_RIGHT);
	bn = xm_button(rightform, "Save configuration", savecfg_handler, 0);
	xm_attach_form(bn, XM_BOTTOM | XM_LEFT | XM_RIGHT);

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
	create_pathfield(subvbox, str, 0, bgimage_path_change, 0);

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
	create_pathfield(subvbox, str, 0, bgmask_path_change, 0);

	bgmode = 0;
	if(cmd_getprop_str("xlivebg.bgmode", buf, sizeof buf) != -1) {
		str = clean_path_str(buf);
		if(strcmp(str, "vgrad") == 0) {
			bgmode = 1;
		} else if(strcmp(str, "hgrad") == 0) {
			bgmode = 2;
		}
	}
	cmd_getprop_vec("xlivebg.color", bgcol[0]);
	cmd_getprop_vec("xlivebg.color2", bgcol[1]);

	subfrm = xm_frame(vbox, "Color");
	hbox = xm_rowcol(subfrm, XmHORIZONTAL);
	optmenu = xm_va_option_menu(hbox, colopt_change, 0, "Solid color", "Vertical gradient", "Horizontal gradient", (void*)0);
	xm_select_option(optmenu, bgmode);
	color_button(hbox, BNCOL_WIDTH, BNCOL_HEIGHT, bgcol[0][0] * 65535.0f, bgcol[0][1] * 65535.0f,
			bgcol[0][2] * 65535.0f, color_change, 0);
	bn_endcol = color_button(hbox, BNCOL_WIDTH, BNCOL_HEIGHT, bgcol[1][0] * 65535.0f,
			bgcol[1][1] * 65535.0f, bgcol[1][2] * 65535.0f, color_change, (void*)1);
	XtSetSensitive(bn_endcol, bgmode > 0);

	/* --- fit --- */
	subfrm = xm_frame(vbox, "Background image fit");
	subvbox = xm_rowcol(subfrm, XmVERTICAL);

	hbox = xm_rowcol(subvbox, XmHORIZONTAL);
	xm_label(hbox, "mode");
	fit = 0;
	if(cmd_getprop_str("xlivebg.fit", buf, sizeof buf) != -1) {
		str = clean_path_str(buf);
		if(strcmp(str, "crop") == 0) {
			fit = 1;
		} else if(strcmp(str, "stretch") == 0) {
			fit = 2;
		}
	}
	optmenu = xm_va_option_menu(hbox, fitmode_change, 0, "Full", "Crop", "Stretch", (void*)0);
	xm_select_option(optmenu, fit);

	frm_cropopt = xm_frame(subvbox, "Crop options");
	form = xm_form(frm_cropopt, 3);

	fval = 1;
	cmd_getprop_num("xlivebg.crop_zoom", &fval);
	lb = xm_label(form, "zoom");
	xm_attach_pos(lb, XM_TOP, 0);
	xm_attach_pos(lb, XM_RIGHT, 1);
	slider = xm_sliderf(form, 0, fval, 0, 1, cropzoom_change, 0);
	XtVaSetValues(slider, XmNshowValue, False, (void*)0);
	xm_attach_pos(slider, XM_TOP, 0);
	xm_attach_pos(slider, XM_LEFT, 1);
	xm_attach_form(slider, XM_RIGHT);

	cropdir[0] = cropdir[1] = 0;
	cmd_getprop_vec("xlivebg.crop_dir", cropdir);
	lb = xm_label(form, "horizontal pan");
	xm_attach_pos(lb, XM_TOP, 1);
	xm_attach_pos(lb, XM_RIGHT, 1);
	slider = xm_sliderf(form, 0, cropdir[0], -1, 1, cropdir_change, cropdir);
	XtVaSetValues(slider, XmNshowValue, False, (void*)0);
	xm_attach_pos(slider, XM_TOP, 1);
	xm_attach_pos(slider, XM_LEFT, 1);
	xm_attach_form(slider, XM_RIGHT);

	lb = xm_label(form, "vertical pan");
	xm_attach_pos(lb, XM_TOP, 2);
	xm_attach_pos(lb, XM_RIGHT, 1);
	slider = xm_sliderf(form, 0, cropdir[1], -1, 1, cropdir_change, cropdir + 1);
	XtVaSetValues(slider, XmNshowValue, False, (void*)0);
	xm_attach_pos(slider, XM_TOP, 2);
	xm_attach_pos(slider, XM_LEFT, 1);
	xm_attach_form(slider, XM_RIGHT);

	if(fit != 1) XtSetSensitive(frm_cropopt, 0);

	/* --- framerate override --- */
	subfrm = xm_frame(vbox, "Frame rate");
	//hbox = xm_rowcol(subfrm, XmHORIZONTAL);
	form = xm_form(subfrm, 0);

	user_fps = -1;
	cmd_getprop_int("xlivebg.fps", &user_fps);

	forcefps = user_fps > 0;
	chkbox = xm_checkbox(form, "Force", forcefps, forcefps_handler, 0);
	xm_attach_form(chkbox, XM_TOP | XM_BOTTOM | XM_LEFT);

	if(!forcefps) {
		long upd_rate;
		if(cmd_getupd(&upd_rate) >= 0) {
			user_fps = 1000000 / upd_rate;
		}
	}

	slider_fps = xm_slideri(form, 0, user_fps, 1, user_fps > 60 ? user_fps : 60,
			fps_change, 0);
	XtSetSensitive(slider_fps, forcefps);
	xm_attach_form(slider_fps, XM_TOP | XM_BOTTOM);
	xm_attach_widget(slider_fps, XM_LEFT, chkbox);

	lb = xm_label(form, "fps");
	xm_attach_form(lb, XM_TOP | XM_BOTTOM | XM_RIGHT);
	xm_attach_widget(slider_fps, XM_RIGHT, lb);

	gen_wallpaper_ui();

	return 0;
}

static void create_menu(void)
{
	XmString sfile, shelp, ssave, ssave_key, squit, sabout, squit_key;
	Widget menubar;

	sfile = XmStringCreateSimple("File");
	shelp = XmStringCreateSimple("Help");
	menubar = XmVaCreateSimpleMenuBar(win, "menubar", XmVaCASCADEBUTTON, sfile, 'F',
			XmVaCASCADEBUTTON, shelp, 'H', (void*)0);
	XtManageChild(menubar);
	XmStringFree(sfile);
	XmStringFree(shelp);

	ssave = XmStringCreateSimple("Save");
	ssave_key = XmStringCreateSimple("Ctrl-S");
	squit = XmStringCreateSimple("Quit");
	squit_key = XmStringCreateSimple("Ctrl-Q");
	XmVaCreateSimplePulldownMenu(menubar, "filemenu", 0, file_menu_handler,
			XmVaPUSHBUTTON, ssave, 'S', "Ctrl<Key>s", ssave_key,
			XmVaPUSHBUTTON, squit, 'Q', "Ctrl<Key>q", squit_key,
			(void*)0);
	XmStringFree(ssave);
	XmStringFree(ssave_key);
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

static void savecfg_handler(Widget w, void *cls, void *calldata)
{
	char cfgfile[512];

	if(cmd_cfgpath(cfgfile, sizeof cfgfile) != -1) {
		if(!questionbox("Are you sure?", "Saving will overwrite \"%s\", are you sure?", cfgfile)) {
			return;
		}
	}
	cmd_save();
}

static void file_menu_handler(Widget lst, void *cls, void *calldata)
{
	int item = (int)cls;
	switch(item) {
	case 0:
		savecfg_handler(0, 0, 0);
		break;

	case 1:
		exit(0);
	}
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
		cmd_rmprop("xlivebg.image");
	}
}

static void bgimage_path_change(const char *path, void *cls)
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
		cmd_rmprop("xlivebg.anim_mask");
	}
}

static void bgmask_path_change(const char *path, void *cls)
{
	if(use_bgmask) {
		cmd_setprop_str("xlivebg.anim_mask", path);
	}
}

static void colopt_change(Widget w, void *cls, void *calldata)
{
	int idx;
	void *ptr;
	static const char *bgmode_str[] = {"solid", "vgrad", "hgrad"};

	XtVaGetValues(w, XmNuserData, &ptr, (void*)0);
	idx = (int)ptr;

	cmd_setprop_str("xlivebg.bgmode", bgmode_str[idx]);

	if(idx <= 0) {
		XtSetSensitive(bn_endcol, 0);
	} else {
		XtSetSensitive(bn_endcol, 1);
	}
}

static void color_change(int r, int g, int b, void *cls)
{
	int cidx = (int)cls;
	static const char *colprop_str[] = {"xlivebg.color", "xlivebg.color2"};

	bgcol[cidx][0] = r / 65535.0f;
	bgcol[cidx][1] = g / 65535.0f;
	bgcol[cidx][2] = b / 65535.0f;
	cmd_setprop_vec(colprop_str[cidx], bgcol[cidx]);
}

static void numprop_change(Widget w, void *cls, void *calldata)
{
	int i, ndecimal = 0;
	float val, s = 1.0f;
	struct bgprop *prop = cls;
	XmScaleCallbackStruct *cbs = calldata;

	XtVaGetValues(w, XmNdecimalPoints, &ndecimal, (void*)0);
	for(i=0; i<ndecimal; i++) {
		s *= 0.1f;
	}
	val = cbs->value * s;
	if(val != prop->number.value) {
		prop->number.value = val;
		cmd_setprop_num(prop->fullname, val);
	}
}

static void intprop_change(Widget w, void *cls, void *calldata)
{
	struct bgprop *prop = cls;
	XmScaleCallbackStruct *cbs = calldata;

	if(cbs->value != prop->integer.value) {
		prop->integer.value = cbs->value;
		cmd_setprop_int(prop->fullname, cbs->value);
	}
}

static void pathprop_change(const char *path, void *cls)
{
	struct bgprop *prop = cls;

	cmd_setprop_str(prop->fullname, path);
}

static void boolprop_change(Widget w, void *cls, void *calldata)
{
	struct bgprop *prop = cls;
	XmToggleButtonCallbackStruct *cbs = calldata;

	if(cbs->set != prop->integer.value) {
		prop->integer.value = cbs->set;
		cmd_setprop_int(prop->fullname, cbs->set);
	}
}

static void colprop_change(int r, int g, int b, void *cls)
{
	float vval[4];
	struct bgprop *prop = cls;

	if(r != prop->color.r || g != prop->color.g || b != prop->color.b) {
		prop->color.r = r;
		prop->color.g = g;
		prop->color.b = b;
		vval[0] = r / 65535.0f;
		vval[1] = g / 65535.0f;
		vval[2] = b / 65535.0f;
		vval[3] = 1.0f;
		cmd_setprop_vec(prop->fullname, vval);
	}
}

static void fitmode_change(Widget w, void *cls, void *calldata)
{
	void *udata;
	int idx;
	static const char *optstr[] = {"full", "crop", "stretch"};

	XtVaGetValues(w, XmNuserData, &udata, (void*)0);
	idx = (int)udata;

	XtSetSensitive(frm_cropopt, idx == 1);
	cmd_setprop_str("xlivebg.fit", optstr[idx]);
}

static void cropzoom_change(Widget w, void *cls, void *calldata)
{
	float val = xm_get_sliderf_value(w);
	cmd_setprop_num("xlivebg.crop_zoom", val);
}

static void cropdir_change(Widget w, void *cls, void *calldata)
{
	float *valptr = cls;

	*valptr = xm_get_sliderf_value(w);
	cmd_setprop_vec("xlivebg.crop_dir", cropdir);
}

static void forcefps_handler(Widget w, void *cls, void *calldata)
{
	XmToggleButtonCallbackStruct *cbs = calldata;

	XtSetSensitive(slider_fps, cbs->set);

	if(cbs->set) {
		XtVaSetValues(slider_fps, XmNvalue, user_fps, (void*)0);
		cmd_setprop_int("xlivebg.fps", user_fps);
	} else {
		cmd_setprop_int("xlivebg.fps", -1);
	}
}

static void fps_change(Widget w, void *cls, void *calldata)
{
	XmScaleCallbackStruct *cbs = calldata;

	if(cbs->value != user_fps) {
		user_fps = cbs->value;
		cmd_setprop_int("xlivebg.fps", user_fps);
	}
}

static void gen_wallpaper_ui(void)
{
	int i, bval, ival;
	float fval, vval[4];
	Widget w, vbox, hbox;
	struct bginfo *bg;
	XmString xmstr;
	char buf[1024];
	struct bgprop *prop;

	if(!(bg = bg_active())) {
		return;
	}

	if((vbox = XtNameToWidget(frm_cur, "vbox"))) {
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
		switch(prop->type) {
		case BGPROP_BOOL:
			if(cmd_getprop_int(prop->fullname, &bval) != -1) {
				prop->integer.value = bval;
				w = xm_checkbox(vbox, prop->name, bval, boolprop_change, prop);
			}
			break;

		case BGPROP_TEXT:
			/* TODO handle multi-line text */
			if(cmd_getprop_str(prop->fullname, buf, sizeof buf) != -1) {
				hbox = xm_rowcol(vbox, XmHORIZONTAL);
				xm_label(hbox, prop->name);
				xm_textfield(hbox, 0, 0, 0);
			}
			break;

		case BGPROP_NUMBER:
			if(cmd_getprop_num(prop->fullname, &fval) != -1) {
				prop->number.value = fval;
				w = xm_sliderf(vbox, prop->name, fval, prop->number.start, prop->number.end,
						numprop_change, prop);
			}
			break;

		case BGPROP_INTEGER:
			if(cmd_getprop_int(prop->fullname, &ival) != -1) {
				prop->integer.value = ival;
				w = xm_sliderf(vbox, prop->name, ival, prop->integer.start, prop->integer.end,
						intprop_change, prop);
			}
			break;

		case BGPROP_COLOR:
			if(cmd_getprop_vec(prop->fullname, vval) != -1) {
				hbox = xm_rowcol(vbox, XmHORIZONTAL);
				xm_label(hbox, prop->name);
				color_button(hbox, 48, 24, vval[0] * 65535.0f, vval[1] * 65535.0f,
						vval[2] * 65535.0f, colprop_change, prop);
			}
			break;

		case BGPROP_PATHNAME:
		case BGPROP_FILENAME:
		case BGPROP_DIRNAME:
			if(cmd_getprop_str(prop->fullname, buf, sizeof buf) != -1) {
				hbox = xm_rowcol(vbox, XmHORIZONTAL);
				xm_label(hbox, prop->name);
				create_pathfield(hbox, clean_path_str(buf), 0, pathprop_change, prop);
			}
			break;
		}

		prop++;
	}
}

static void set_status(const char *s)
{
	XmString xs = XmStringCreateSimple((char*)s);
	XtVaSetValues(lb_status, XmNlabelString, xs, (void*)0);
	XmStringFree(xs);
}
