#ifndef XMUTIL_H_
#define XMUTIL_H_

#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/MessageB.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/DrawnB.h>
#include <Xm/ToggleB.h>
#include <Xm/List.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <Xm/TextF.h>
#include <Xm/FileSB.h>

/* motif widget creation convenience wrappers */
Widget xm_label(Widget par, const char *text);
Widget xm_frame(Widget par, const char *title);
Widget xm_rowcol(Widget par, int orient);	/* XmVERTICAL/XmHORIZONTAL */
Widget xm_button(Widget par, const char *text, XtCallbackProc cb, void *cls);
Widget xm_drawn_button(Widget par, int width, int height, XtCallbackProc cb, void *cls);
Widget xm_checkbox(Widget par, const char *text, int checked, XtCallbackProc cb, void *cls);
Widget xm_textfield(Widget par, const char *text, XtCallbackProc cb, void *cls);
Widget xm_option_menu(Widget par);
Widget xm_va_option_menu(Widget par, XtCallbackProc cb, void *cls, ...);

int xm_get_border_size(Widget w);

/* higher level app-specific utility functions and composite "widgets" */
Widget create_pathfield(Widget par, const char *defpath, const char *filter,
		void (*handler)(const char*));

void messagebox(int type, const char *title, const char *msg);
void color_picker_dialog(unsigned short *col);

#endif /* XMUTIL_H_ */
