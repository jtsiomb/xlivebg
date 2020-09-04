#include <stdio.h>
#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/MessageB.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/List.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <Xm/TextF.h>
#include <Xm/FileSB.h>
#include "cmd.h"
#include "bg.h"

static int init_gui(void);
static void create_menu(void);
static Widget create_pathfield(Widget par);
static int create_bglist(Widget par);
static void file_menu_handler(Widget lst, void *cls, void *calldata);
static void help_menu_handler(Widget lst, void *cls, void *calldata);
static void bgselect_handler(Widget lst, void *cls, void *calldata);
static void browse_handler(Widget bn, void *cls, void *calldata);
static void pathfield_modify_handler(Widget txf, void *cls, void *calldata);
static void messagebox(int type, const char *title, const char *msg);

static Widget app_shell, win;
static XtAppContext app;

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

static int init_gui(void)
{
	Widget w, frm, vbox, wimgpath;
	Arg arg;
	char buf[512];

	create_menu();

	frm = XmCreateFrame(win, "frame", 0, 0);
	XtSetArg(arg, XmNframeChildType, XmFRAME_TITLE_CHILD);
	w = XmCreateLabelGadget(frm, "Global settings", &arg, 1);
	XtManageChild(w);

	vbox = XmCreateRowColumn(frm, "rowcolumn", 0, 0);
	XtManageChild(vbox);

	XtManageChild(XmCreateLabel(vbox, "Wallpapers:", 0, 0));
	if(create_bglist(vbox) == -1) {
		return -1;
	}

	XtManageChild(XmCreateLabel(vbox, "Background image:", 0, 0));
	wimgpath = create_pathfield(vbox);

	if(cmd_getprop_str("xlivebg.image", buf, sizeof buf) != -1) {
		char *ptr = buf;
		while(*ptr && *ptr != '\n' && *ptr != '\r') ptr++;
		*ptr = 0;
		XmTextFieldSetString(wimgpath, buf);
	}
	XtManageChild(wimgpath);

	XtManageChild(frm);
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

/* TODO specify files/directories/both, optional wildcards, etc */
static Widget create_pathfield(Widget par)
{
	Widget hbox, tx_path, bn_browse;
	Arg args[2];

	hbox = XmCreateRowColumn(par, "rowcolumn", 0, 0);
	XtVaSetValues(hbox, XmNorientation, XmHORIZONTAL, (void*)0);
	XtManageChild(hbox);

	XtSetArg(args[0], XmNcolumns, 40);
	XtSetArg(args[1], XmNeditable, 0);
	tx_path = XmCreateTextField(hbox, "textfield", args, 2);
	XtAddCallback(tx_path, XmNvalueChangedCallback, pathfield_modify_handler, 0);

	bn_browse = XmCreatePushButton(hbox, "...", 0, 0);
	XtManageChild(bn_browse);
	XtAddCallback(bn_browse, XmNactivateCallback, browse_handler, tx_path);

	return tx_path;
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
	}
}

static void filesel_handler(Widget dlg, void *cls, void *calldata)
{
	char *fname;
	XmFileSelectionBoxCallbackStruct *cbs = calldata;

	if(cls) {
		Widget field = cls;
		if(!(fname = XmStringUnparse(cbs->value, XmFONTLIST_DEFAULT_TAG, XmCHARSET_TEXT,
						XmCHARSET_TEXT, 0, 0, XmOUTPUT_ALL))) {
			return;
		}

		XtVaSetValues(field, XmNvalue, fname, (void*)0);
		XtFree(fname);
	}
	XtUnmanageChild(dlg);
}

static void browse_handler(Widget bn, void *cls, void *calldata)
{
	Widget dlg;
	Arg arg;

	XtSetArg(arg, XmNpathMode, XmPATH_MODE_RELATIVE);
	dlg = XmCreateFileSelectionDialog(app_shell, "filesb", &arg, 1);
	XtAddCallback(dlg, XmNcancelCallback, filesel_handler, 0);
	XtAddCallback(dlg, XmNokCallback, filesel_handler, cls);
	XtManageChild(dlg);

	while(XtIsManaged(dlg)) {
		XtAppProcessEvent(app, XtIMAll);
	}
}

static void pathfield_modify_handler(Widget txf, void *cls, void *calldata)
{
	char *text = XmTextFieldGetString(txf);
	printf("path changed: %s\n", text);
	XtFree(text);
}

static void messagebox(int type, const char *title, const char *msg)
{
	XmString stitle = XmStringCreateSimple((char*)title);
	XmString smsg = XmStringCreateLtoR((char*)msg, XmFONTLIST_DEFAULT_TAG);
	Widget dlg;

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
