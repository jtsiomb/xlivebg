#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "xmutil.h"

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
	Arg arg;

	w = XmCreateFrame(par, "frame", 0, 0);
	XtSetArg(arg, XmNframeChildType, XmFRAME_TITLE_CHILD);
	XtManageChild(XmCreateLabelGadget(w, (char*)title, &arg, 1));
	XtManageChild(w);
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

int xm_get_border_size(Widget w)
{
	Dimension highlight, shadow;

	XtVaGetValues(w, XmNhighlightThickness, &highlight,
			XmNshadowThickness, &shadow, (void*)0);

	return highlight + shadow;
}

static void filesel_handler(Widget dlg, void *cls, void *calldata);

const char *file_dialog(Widget shell, const char *start_dir, const char *filter, char *buf, int bufsz)
{
	Widget dlg;
	Arg argv[3];
	int argc = 0;

	if(bufsz < sizeof bufsz) {
		fprintf(stderr, "file_dialog: insufficient buffer size: %d\n", bufsz);
		return 0;
	}
	memcpy(buf, &bufsz, sizeof bufsz);

	if(start_dir) {
		XmString s = XmStringCreateSimple((char*)start_dir);
		XtSetArg(argv[argc], XmNdirectory, s), argc++;
		XmStringFree(s);
	}
	if(filter) {
		XmString s = XmStringCreateSimple((char*)filter);
		XtSetArg(argv[argc], XmNdirMask, s), argc++;
		XmStringFree(s);
	}
	XtSetArg(argv[argc], XmNpathMode, XmPATH_MODE_RELATIVE), argc++;

	dlg = XmCreateFileSelectionDialog(app_shell, "filesb", argv, argc);
	XtAddCallback(dlg, XmNcancelCallback, filesel_handler, 0);
	XtAddCallback(dlg, XmNokCallback, filesel_handler, buf);
	XtManageChild(dlg);

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
		void (*handler)(const char*))
{
	Widget hbox, tx_path;
	Arg args[2];

	hbox = xm_rowcol(par, XmHORIZONTAL);

	XtSetArg(args[0], XmNcolumns, 40);
	XtSetArg(args[1], XmNeditable, 0);
	tx_path = XmCreateTextField(hbox, "textfield", args, 2);
	XtManageChild(tx_path);
	if(defpath) XmTextFieldSetString(tx_path, (char*)defpath);
	XtAddCallback(tx_path, XmNvalueChangedCallback, pathfield_modify, (void*)handler);

	xm_button(hbox, "...", pathfield_browse, tx_path);
	return tx_path;
}

static void pathfield_browse(Widget bn, void *cls, void *calldata)
{
	char buf[512];

	if(file_dialog(app_shell, 0, 0, buf, sizeof buf)) {
		XmTextFieldSetString(cls, buf);
	}
}

static void pathfield_modify(Widget txf, void *cls, void *calldata)
{
	void (*usercb)(const char*) = (void (*)(const char*))cls;

	char *text = XmTextFieldGetString(txf);
	if(usercb) usercb(text);
	XtFree(text);
}

