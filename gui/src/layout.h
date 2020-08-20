#ifndef LAYOUT_H_
#define LAYOUT_H_

class Fl_Group;
class Fl_Widget;

enum UILayoutType { UILAYOUT_HORIZONTAL, UILAYOUT_VERTICAL };

class UILayout {
private:
	Fl_Group *group;

public:
	UILayoutType type;
	int x, y, width, height;
	int padding, spacing;

	explicit UILayout(Fl_Group *group, UILayoutType type = UILAYOUT_VERTICAL);

	void add(Fl_Widget *w);
	void add_autosize(Fl_Widget *w);

	void resize_group() const;
};

#endif	/* LAYOUT_H_ */
