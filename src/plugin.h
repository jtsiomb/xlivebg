/*
xlivebg - live wallpapers for the X window system
Copyright (C) 2019  John Tsiombikas <nuclear@member.fsf.org>

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
#ifndef PLUGIN_H_
#define PLUGIN_H_

#include "xlivebg.h"

void init_plugins(void);

struct xlivebg_plugin *get_plugin(int idx);
int get_plugin_count(void);

void activate_plugin(struct xlivebg_plugin *plugin);
struct xlivebg_plugin *get_active_plugin(void);

int remove_plugin(int idx);

#endif	/* PLUGIN_H_ */
