/*
xlivebg - live wallpapers for the X window system
Copyright (C) 2019-2020  John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef BG_H_
#define BG_H_

enum {
	BGPROP_BOOL,
	BGPROP_TEXT,
	BGPROP_NUMBER,
	BGPROP_INTEGER,
	BGPROP_COLOR,
	BGPROP_FILENAME = 0x100,
	BGPROP_DIRNAME = 0x200,
	BGPROP_PATHNAME = 0x300
};

struct bgprop_text {
	int multiline;
	char *text;
};

struct bgprop_number {
	float start, end;
	float value;
};

struct bgprop_integer {
	int start, end;
	int value;
};

struct bgprop_color {
	float color[3];
};

struct bgprop_path {
	char *path;
};

struct bgprop {
	int type;
	char *name, *desc;
	struct bgprop_text text;
	struct bgprop_number number;
	struct bgprop_integer integer;
	struct bgprop_color color;
	struct bgprop_path path;
};

struct bginfo {
	char *name, *desc;

	struct bgprop *prop;
	int num_prop;
};

#ifdef __cplusplus
extern "C" {
#endif

int bg_create_list(void);
void bg_destroy_list(void);

struct bginfo *bg_list_item(int idx);
int bg_list_size(void);

struct bginfo *bg_active(void);
int bg_active_name(char *buf, int bufsz);

int bg_switch(const char *name);

#ifdef __cplusplus
}
#endif

#endif	/* BG_H_ */
