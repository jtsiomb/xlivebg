#include <stdio.h>
#include <stdlib.h>
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

static char *msgbox_text;
static int msgbox_textsz;

#define MSGBOX_FORMAT_TEXT(fmt, dofail) \
	do {	\
		char *tmp; \
		int len, newlen; \
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
			newlen = len == -1 ? msgbox_textsz << 1 : len;	\
			if(!(tmp = malloc(newlen))) {	\
				fprintf(stderr, "Failed to resize messagebox buffer to %d bytes\n", newlen);	\
				dofail;	\
			}	\
			free(msgbox_text);	\
			msgbox_text = tmp;	\
			msgbox_textsz = newlen;	\
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

void color_picker_dialog(unsigned short *col)
{
	messagebox(XmDIALOG_INFORMATION, "Not implemented yet", "TODO: create a color selection dialog");
}
