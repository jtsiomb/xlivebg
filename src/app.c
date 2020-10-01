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
#include <stdio.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <dlfcn.h>
#include "app.h"
#include "imageman.h"
#include "plugin.h"
#include "cfg.h"

unsigned int bgtex;
unsigned long msec;
long upd_interval_usec;

int scr_width, scr_height;

struct xlivebg_screen screen[MAX_SCR];
int num_screens;


int app_init(int argc, char **argv)
{
	int i, num_plugins;
	struct xlivebg_plugin *first_plugin = 0;

	init_imgman();
	init_plugins();

	num_plugins = get_plugin_count();
	for(i=0; i<num_plugins; i++) {
		struct xlivebg_plugin *plugin = get_plugin(i);

		if(plugin->init && plugin->init(plugin->data) == -1) {
			fprintf(stderr, "xlivebg: plugin %s failed to initialize\n", plugin->name);
			remove_plugin(i--);
			num_plugins--;
			continue;
		}

		if(!first_plugin) first_plugin = plugin;
	}

	if(cfg.act_plugin) {
		struct xlivebg_plugin *p = find_plugin(cfg.act_plugin);
		if(!p) {
			fprintf(stderr, "xlivebg: failed to activate plugin %s: not found\n", cfg.act_plugin);
		} else {
			activate_plugin(p);
		}
	}

	if(!get_active_plugin() && first_plugin) {
		activate_plugin(first_plugin);
	}

	return 0;
}

void app_cleanup(void)
{
	int i, num_plugins;

	num_plugins = get_plugin_count();
	for(i=0; i<num_plugins; i++) {
		struct xlivebg_plugin *plugin = get_plugin(i);
		void *so = plugin->so;

		if(plugin->cleanup) {
			plugin->cleanup(plugin->data);
		}

		dlclose(so);
	}
}

void app_draw(void)
{
	struct xlivebg_plugin *plugin = get_active_plugin();

	if(plugin) {
		plugin->draw(msec, plugin->data);
	} else {
		glClearColor(0.2, 0.1, 0.1, 1);
		glClear(GL_COLOR_BUFFER_BIT);
	}

#ifdef PRINT_FPS
	{
		static long frame, prev_fps_upd;
		frame++;
		if(msec - prev_fps_upd >= 1000) {
			long fps = 10000 * frame / (msec - prev_fps_upd);
			printf("\rframerate: %4ld.%02ld fps", fps / 10, fps % 10);

			frame = 0;
			prev_fps_upd = msec;
			fflush(stdout);
		}
	}
#endif
}

void app_reshape(int x, int y)
{
	scr_width = x;
	scr_height = y;
}

void app_keyboard(int key, int pressed)
{
	switch(key) {
	case 27:
		app_quit();
		break;
	}
}
