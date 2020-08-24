#include <stdio.h>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include "layout.h"
#include "cmd.h"
#include "bg.h"

static bool init_gui();
static Fl_Hold_Browser *create_bglist();

Fl_Window *win;

int main(int argc, char **argv)
{
	if(cmd_ping() == -1) {
		fl_message_title("Ping failed");
		fl_alert("No response from xlivebg. Make sure it's running!");
		return 1;
	}
	if(!init_gui()) {
		return 1;
	}
	win->show(argc, argv);
	return Fl::run();
}

static bool init_gui()
{
	int win_width = 512, win_height = 512;

	win = new Fl_Window(win_width, win_height, "Configure xlivebg");

	UILayout vbox = UILayout(win, UILAYOUT_VERTICAL);

	Fl_Hold_Browser *bglist = create_bglist();
	vbox.add(bglist);
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


static Fl_Hold_Browser *create_bglist()
{
	int max_width = 200;

	if(bg_create_list() == -1) {
		return 0;
	}
	int num = bg_list_size();

	for(int i=0; i<num; i++) {
		struct bginfo *bg = bg_list_item(i);
		int w = (int)fl_width(bg->name);
		if(w > max_width) max_width = w;
	}

	printf("height: %d\n", fl_height() * 5);
	Fl_Hold_Browser *list = new Fl_Hold_Browser(0, 0, max_width, 128);
	for(int i=0; i<num; i++) {
		struct bginfo *bg = bg_list_item(i);
		list->add(bg->name, bg);
		printf("adding: %s\n", bg->name);
	}

	bg_destroy_list();
	return list;
}
