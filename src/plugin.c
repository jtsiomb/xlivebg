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
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include "xlivebg.h"
#include "app.h"
#include "imageman.h"
#include "util.h"
#include "cfg.h"
#include "treestore.h"

static int load_plugins(const char *dirpath);

static struct xlivebg_plugin *act;
static struct xlivebg_plugin **plugins;
static int num_plugins, max_plugins;

/* searches for, and loads, all available plugins.
 * search paths:
 *  - PREFIX/lib/xlivebg/
 *  - $HOME/.local/lib/xlivebg/
 *  - $HOME/.xlivebg/plugins/
 */
void init_plugins(void)
{
	char *home = get_home_dir();
	char *dirpath;

	load_plugins(PREFIX "/lib/xlivebg");

	dirpath = alloca(strlen(home) + 32);

	sprintf(dirpath, "%s/.local/lib/xlivebg", home);
	load_plugins(dirpath);

	sprintf(dirpath, "%s/.xlivebg/plugins", home);
	load_plugins(dirpath);
}

static int load_plugins(const char *dirpath)
{
	int num;
	DIR *dir;
	struct dirent *dent;
	char fname[1024];
	void *so;
	void (*reg)(void);

	if(!(dir = opendir(dirpath))) {
		return -1;
	}

	printf("xlivebg: searching for plugins in %s\n", dirpath);

	while((dent = readdir(dir))) {
		snprintf(fname, sizeof fname, "%s/%s", dirpath, dent->d_name);
		fname[sizeof fname - 1] = 0;

		if((so = dlopen(fname, RTLD_LAZY))) {
			if((reg = dlsym(so, "register_plugin"))) {
				reg();
				plugins[num_plugins - 1]->so = so;
				num++;
			} else {
				dlclose(so);
			}
		}
	}

	closedir(dir);
	return num;
}

struct xlivebg_plugin *get_plugin(int idx)
{
	return plugins[idx];
}

int get_plugin_count(void)
{
	return num_plugins;
}

void activate_plugin(struct xlivebg_plugin *plugin)
{
	if(act && act->stop) {
		act->stop(act->data);
	}

	act = plugin;
	if(plugin->start) {
		plugin->start(msec, plugin->data);
	}

	upd_interval_usec = act->upd_interval;
}

struct xlivebg_plugin *get_active_plugin(void)
{
	return act;
}

int remove_plugin(int idx)
{
	if(idx < 0 || idx >= num_plugins) {
		return -1;
	}

	dlclose(plugins[num_plugins - 1]->so);

	if(idx == num_plugins - 1) {
		num_plugins--;
		return 0;
	}

	memmove(plugins + idx, plugins + idx + 1, (num_plugins - idx - 1) * sizeof *plugins);
	num_plugins--;
	return 0;
}


/* ---- plugin API ---- */

int xlivebg_register_plugin(struct xlivebg_plugin *plugin)
{
	if(num_plugins >= max_plugins) {
		int nmax = max_plugins ? max_plugins * 2 : 16;
		struct xlivebg_plugin **tmp = realloc(plugins, nmax * sizeof *plugins);
		if(!tmp) {
			perror("xlivebg_register_plugin");
			return -1;
		}
		plugins = tmp;
		max_plugins = nmax;
	}
	plugins[num_plugins++] = plugin;
	printf("xlivebg: registered plugin: %s\n", plugin->name);
	return 0;
}

int xlivebg_screen_count(void)
{
	return num_screens;
}

struct xlivebg_screen *xlivebg_screen(int idx)
{
	return screen + idx;
}

struct xlivebg_image *xlivebg_bg_image(int scr)
{
	return get_bg_image(scr);
}

struct xlivebg_image *xlivebg_anim_mask(int scr)
{
	return get_anim_mask(scr);
}

/* plugin configuration interface */
int xlivebg_havecfg(const char *cfgpath)
{
	if(!cfg.ts) return 0;
	return ts_lookup(cfg.ts, cfgpath) ? 1 : 0;
}

const char *xlivebg_getcfg_str(const char *cfgpath, const char *def_val)
{
	if(!cfg.ts) return def_val;
	return ts_lookup_str(cfg.ts, cfgpath, def_val);
}

float xlivebg_getcfg_num(const char *cfgpath, float def_val)
{
	if(!cfg.ts) return def_val;
	return ts_lookup_num(cfg.ts, cfgpath, def_val);
}

int xlivebg_getcfg_int(const char *cfgpath, int def_val)
{
	if(!cfg.ts) return def_val;
	return ts_lookup_int(cfg.ts, cfgpath, def_val);
}

float *xlivebg_getcfg_vec(const char *cfgpath, float *def_val)
{
	if(!cfg.ts) return def_val;
	return ts_lookup_vec(cfg.ts, cfgpath, def_val);
}
