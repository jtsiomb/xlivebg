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
#include <Xm/Form.h>
#include <Xm/TextF.h>
#include <Xm/FileSB.h>
#include <Xm/Scale.h>
#include <Xm/SSpinB.h>
#include <Xm/SelectioB.h>
#include <Xm/DialogS.h>
#include <Xm/PanedW.h>
#include <Xm/DrawingA.h>

/* motif widget creation convenience wrappers */
Widget xm_label(Widget par, const char *text);
Widget xm_frame(Widget par, const char *title);
Widget xm_rowcol(Widget par, int orient);	/* XmVERTICAL/XmHORIZONTAL */
Widget xm_form(Widget par, int grid);
Widget xm_button(Widget par, const char *text, XtCallbackProc cb, void *cls);
Widget xm_drawn_button(Widget par, int width, int height, XtCallbackProc cb, void *cls);
Widget xm_checkbox(Widget par, const char *text, int checked, XtCallbackProc cb, void *cls);
Widget xm_textfield(Widget par, const char *text, XtCallbackProc cb, void *cls);
Widget xm_option_menu(Widget par);
Widget xm_va_option_menu(Widget par, XtCallbackProc cb, void *cls, ...);
Widget xm_sliderf(Widget par, const char *text, float val, float min, float max,
		XtCallbackProc cb, void *cls);
Widget xm_slideri(Widget par, const char *text, int val, int min, int max,
		XtCallbackProc cb, void *cls);

Widget xm_spinboxi(Widget par, int val, int min, int max, XtCallbackProc cb, void *cls);

int xm_get_border_size(Widget w);

enum { XM_TOP = 1, XM_BOTTOM = 2, XM_LEFT = 4, XM_RIGHT = 8 };
void xm_attach_form(Widget w, unsigned int dirmask);
void xm_attach_widget(Widget w, unsigned int dir, Widget wtarg);
void xm_attach_pos(Widget w, unsigned int dir, int pos);
void xm_attach_pos_full(Widget w, int x0, int y0, int x1, int y1);	/* -1: leave unattached */

void xm_set_sliderf_value(Widget w, float val);
float xm_get_sliderf_value(Widget w);
int xm_select_option(Widget w, int opt);

/* higher level app-specific utility functions and composite "widgets" */
Widget create_pathfield(Widget par, const char *defpath, const char *filter,
		void (*handler)(const char*, void*), void *cls);

Widget color_button(Widget par, int width, int height, int r, int g, int b,
		void (*handler)(int, int, int, void*), void *cls);

void messagebox(int type, const char *title, const char *msg, ...);
int questionbox(const char *title, const char *msg, ...);
int color_picker_dialog(unsigned short *col);

#endif /* XMUTIL_H_ */
