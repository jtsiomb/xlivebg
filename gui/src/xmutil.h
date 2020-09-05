#ifndef XMUTIL_H_
#define XMUTIL_H_

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

/* motif widget creation convenience wrappers */
Widget xm_label(Widget par, const char *text);
Widget xm_frame(Widget par, const char *title);
Widget xm_rowcol(Widget par, int orient);	/* XmVERTICAL/XmHORIZONTAL */
Widget xm_button(Widget par, const char *text, XtCallbackProc cb, void *cls);

/* higher level app-specific utility functions and composite "widgets" */
Widget create_pathfield(Widget par, const char *filter, void (*handler)(const char*));

#endif /* XMUTIL_H_ */
