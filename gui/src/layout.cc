#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include "layout.h"

UILayout::UILayout(Fl_Group *group, UILayoutType type)
{
	this->group = group;
	this->type = type;
	padding = spacing = 5;
	x = y = width = height = -1;

	Fl_Group::current(0);
}

void UILayout::add(Fl_Widget *w)
{
	int xsz, ysz;

	if(width < 0) {
		x = y = padding;
		width = height = 0;
	}

	group->add(w);
	w->position(x, y);
	xsz = w->w();
	ysz = w->h();

	if(type == UILAYOUT_VERTICAL) {
		y += ysz + spacing;
		height += ysz + spacing;
		if(width < xsz) width = xsz;
	} else {
		x += xsz + spacing;
		width += xsz + spacing;
		if(height < ysz) width = ysz;
	}
}

void UILayout::add_autosize(Fl_Widget *w)
{
	int xsz, ysz;
	w->measure_label(xsz, ysz);
	w->size(xsz * 3, ysz * 2);

	add(w);
}

void UILayout::resize_group() const
{
	group->size(width + padding * 2, height + padding * 2);
}
