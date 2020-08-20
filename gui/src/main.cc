#include <stdio.h>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include "layout.h"

bool init_gui();

Fl_Window *win;

int main(int argc, char **argv)
{
	if(!init_gui()) {
		return 1;
	}
	win->show(argc, argv);
	return Fl::run();
}

bool init_gui()
{
	int win_width = 512, win_height = 512;

	win = new Fl_Window(win_width, win_height, "Configure xlivebg");
	win->border(1);

	UILayout vbox = UILayout(win, UILAYOUT_VERTICAL);

	for(int i=0; i<5; i++) {
		char name[16];
		sprintf(name, "button %d", i);
		Fl_Button *bn = new Fl_Button(0, 0, 0, 0, strdup(name));
		vbox.add_autosize(bn);
	}
	vbox.resize_group();

	Fl_Box *rbox = new Fl_Box(win->w() - 1, win->h() - 1, 1, 1);
	rbox->box(FL_BORDER_BOX);
	win->add(rbox);
	rbox->hide();
	win->resizable(rbox);
	win->init_sizes();
	win->size_range(1, 1);

	return true;
}
