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
	BGATTR_BOOL,
	BGATTR_TEXT,
	BGATTR_NUMBER,
	BGATTR_INTEGER,
	BGATTR_COLOR,
	BGATTR_FILENAME = 0x100,
	BGATTR_DIRNAME = 0x200,
	BGATTR_PATHNAME = 0x300
};


struct bgattr_text {
	int type;
	int multiline;
	char *text;
};

struct bgattr_number {
	int type;
	float start, end;
	float value;
};

struct bgattr_integer {
	int type;
	int start, end;
	int value;
};

struct bgattr_color {
	int type;
	float color[3];
};

struct bgattr_path {
	int type;
	char *path;
};

union bgattr {
	int type;
	struct bgattr_text text;
	struct bgattr_number number;
	struct bgattr_integer integer;
	struct bgattr_color color;
	struct bgattr_path path;
};

struct bginfo {
	char *name, *desc;
	char *props_str;

	union bgattr *attr;
	int num_attr;
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
