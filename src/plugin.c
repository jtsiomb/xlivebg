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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <alloca.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include "opengl.h"
#include "xlivebg.h"
#include "app.h"
#include "imageman.h"
#include "util.h"
#include "cfg.h"
#include "treestore.h"

static int load_plugins(const char *dirpath);
static void update_cfg(const char *cfgpath, struct ts_value *tsval);
static const char *get_builtin_str(const char *cfgpath);
static float *get_builtin_num(const char *cfgpath);
static int *get_builtin_int(const char *cfgpath);
static float *get_builtin_vec(const char *cfgpath);

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
	DIR *dir;
	char *home = get_home_dir();
	char *dirpath;

#ifndef NDEBUG
	/* special-case: during development, it helps if I can just load plugins from
	 * the current directory without installing them anywhere. So check to see if
	 * there's a plugins directory in cwd, and if so, collect all shared libs in
	 * subdirectories of that. And do it the quick&dirty way...
	 */
	if((dir = opendir("plugins"))) {
		closedir(dir);
		if(system("mkdir -p bin; find plugins -name '*.so' -exec cp '{}' bin ';'") == 0) {
			load_plugins("bin");
		}
	}
#endif

	load_plugins(PREFIX "/lib/xlivebg");

	dirpath = alloca(strlen(home) + 32);

	sprintf(dirpath, "%s/.local/lib/xlivebg", home);
	load_plugins(dirpath);

	sprintf(dirpath, "%s/.xlivebg/plugins", home);
	load_plugins(dirpath);
}

static int load_plugins(const char *dirpath)
{
	int num = 0;
	DIR *dir;
	struct dirent *dent;
	struct stat st;
	char fname[1024];
	void *so;
	int (*reg)(void);

	if(!(dir = opendir(dirpath))) {
		return -1;
	}

	printf("xlivebg: searching for plugins in %s\n", dirpath);

	while((dent = readdir(dir))) {
		snprintf(fname, sizeof fname, "%s/%s", dirpath, dent->d_name);
		fname[sizeof fname - 1] = 0;

		if(stat(fname, &st) == -1 || !S_ISREG(st.st_mode)) {
			continue;
		}

		if((so = dlopen(fname, RTLD_LAZY))) {
			if((reg = dlsym(so, "register_plugin")) && reg() != -1) {
				plugins[num_plugins - 1]->so = so;
				num++;
			} else {
				dlclose(so);
			}
		} else {
			fprintf(stderr, "failed to open plugin: %s: %s\n", fname, dlerror());
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

struct xlivebg_plugin *find_plugin(const char *name)
{
	int i;

	for(i=0; i<num_plugins; i++) {
		if(strcasecmp(name, plugins[i]->name) == 0) {
			return plugins[i];
		}
	}
	return 0;
}

void activate_plugin(struct xlivebg_plugin *plugin)
{
	printf("xlivebg: activating plugin: %s\n", plugin->name);
	if(act) {
		if(act->stop) {
			act->stop(act->data);
		}
		xlivebg_destroy_gl();
	}

	if(xlivebg_init_gl() == -1) {
		fprintf(stderr, "xlivebg: failed to initialize OpenGL\n");
		return;
	}

	if(plugin->start) {
		if(plugin->start(msec, plugin->data) == -1) {
			fprintf(stderr, "xlivebg: plugin %s failed to start\n", plugin->name);
			if(act && act != plugin) {
				activate_plugin(act);
			}
		}
	}
	act = plugin;

	upd_interval_usec = act->upd_interval;

	free(cfg.act_plugin);
	cfg.act_plugin = strdup(plugin->name);
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

	dlclose(plugins[idx]->so);

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
	if(find_plugin(plugin->name)) {
		fprintf(stderr, "xlivebg: failed to register \"%s\": a plugin by that name already exists\n",
				plugin->name);
		return -1;
	}

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

int xlivebg_memory_image(struct xlivebg_image *img, void *data, long datasz)
{
	if(load_image_mem(img, data, datasz) == -1) {
		return -1;
	}
	add_image(img);
	return 0;
}

unsigned int xlivebg_image_texture(struct xlivebg_image *img)
{
	update_texture(img);
	return img->tex;
}

int xlivebg_fit_mode(int scr)
{
	/* TODO per-screen */
	return cfg.fit;
}

float xlivebg_crop_zoom(int scr)
{
	/* TODO per-screen */
	return cfg.zoom;
}

void xlivebg_crop_dir(int scr, float *dirvec)
{
	/* TODO per-screen */
	dirvec[0] = cfg.crop_dir[0];
	dirvec[1] = cfg.crop_dir[1];
}

/* plugin configuration interface */
int xlivebg_havecfg(const char *cfgpath)
{
	if(!cfg.ts) return 0;
	return ts_lookup(cfg.ts, cfgpath) ? 1 : 0;
}

const char *xlivebg_getcfg_str(const char *cfgpath, const char *def_val)
{
	const char *str;
	if(!cfg.ts) return def_val;
	if((str = get_builtin_str(cfgpath))) {
		return str;
	}
	return ts_lookup_str(cfg.ts, cfgpath, def_val);
}

float xlivebg_getcfg_num(const char *cfgpath, float def_val)
{
	float *ptr;
	if(!cfg.ts) return def_val;
	if((ptr = get_builtin_num(cfgpath))) {
		return *ptr;
	}
	return ts_lookup_num(cfg.ts, cfgpath, def_val);
}

int xlivebg_getcfg_int(const char *cfgpath, int def_val)
{
	int *ptr;
	if(!cfg.ts) return def_val;
	if((ptr = get_builtin_int(cfgpath))) {
		return *ptr;
	}
	return ts_lookup_int(cfg.ts, cfgpath, def_val);
}

float *xlivebg_getcfg_vec(const char *cfgpath, float *def_val)
{
	float *vec;
	if(!cfg.ts) return def_val;
	if((vec = get_builtin_vec(cfgpath))) {
		return vec;
	}
	return ts_lookup_vec(cfg.ts, cfgpath, def_val);
}

/* returns pointer to the value of a specific attribute. If it doesn't exist,
 * creates it and any intermedate path components first.
 */
static struct ts_value *touch_node(const char *cfgpath)
{
	char *pathbuf, *ptr, *aname;
	struct ts_node *node, *child;
	struct ts_attr *attr;

	if(!cfg.ts) {
		if(!(cfg.ts = ts_alloc_node()) || !(cfg.ts->name = strdup("xlivebg"))) {
			ts_free_node(cfg.ts);
		}
	}

	pathbuf = alloca(strlen(cfgpath) + 1);
	strcpy(pathbuf, cfgpath);

	/* separate the attribute name from the rest of the node chain*/
	if(!(aname = strrchr(pathbuf, '.')) || !*++aname) {
		return 0;
	}
	aname[-1] = 0;

	/* grab the first element of the path, and make sure it's xlivebg */
	if(!(ptr = strtok(pathbuf, ".")) || strcmp(ptr, "xlivebg") != 0) {
		return 0;
	}

	node = cfg.ts;
	while((ptr = strtok(0, "."))) {
		if(!(child = ts_get_child(node, ptr))) {
			/* no such path component, create it and move on */
			if(!(child = ts_alloc_node()) || !(child->name = strdup(ptr))) {
				ts_free_node(child);
				return 0;
			}
			ts_add_child(node, child);
		}
		node = child;
	}

	/* reached the end of the chain, see if we have the attribute */
	if(!(attr = ts_get_attr(node, aname))) {
		if(!(attr = ts_alloc_attr()) || ts_set_attr_name(attr, aname) == -1) {
			return 0;
		}
		ts_add_attr(node, attr);
	}

	return &attr->val;
}

int xlivebg_setcfg_str(const char *cfgpath, const char *str)
{
	struct ts_value *aval;
	if(!(aval = touch_node(cfgpath))) {
		return -1;
	}
	if(ts_set_value_str(aval, str) == -1) {
		return -1;
	}
	update_cfg(cfgpath, aval);
	return 0;
}

int xlivebg_setcfg_num(const char *cfgpath, float val)
{
	struct ts_value *aval;
	if(!(aval = touch_node(cfgpath))) {
		return -1;
	}
	if(ts_set_valuef(aval, val) == -1) {
		return -1;
	}
	update_cfg(cfgpath, aval);
	return 0;
}

int xlivebg_setcfg_int(const char *cfgpath, int val)
{
	struct ts_value *aval;
	if(!(aval = touch_node(cfgpath))) {
		return -1;
	}
	if(ts_set_valuei(aval, val) == -1) {
		return -1;
	}
	update_cfg(cfgpath, aval);
	return 0;
}

int xlivebg_setcfg_vec(const char *cfgpath, float *vec)
{
	struct ts_value *aval;
	if(!(aval = touch_node(cfgpath))) {
		return -1;
	}
	if(ts_set_valuef_arr(aval, 4, vec) == -1) {
		return -1;
	}
	update_cfg(cfgpath, aval);
	return 0;
}

int xlivebg_rmcfg(const char *cfgpath)
{
	struct ts_attr *attr = ts_lookup(cfg.ts, cfgpath);
	if(!attr) return -1;

	if(ts_remove_attr(attr->node, attr) == -1) {
		return -1;
	}
	ts_free_attr(attr);
	update_cfg(cfgpath, 0);
	return 0;
}

int xlivebg_defcfg_str(const char *cfgpath, const char *str)
{
	if(xlivebg_havecfg(cfgpath)) return 0;
	return xlivebg_setcfg_str(cfgpath, str);
}

int xlivebg_defcfg_num(const char *cfgpath, float val)
{
	if(xlivebg_havecfg(cfgpath)) return 0;
	return xlivebg_setcfg_num(cfgpath, val);
}

int xlivebg_defcfg_int(const char *cfgpath, int val)
{
	if(xlivebg_havecfg(cfgpath)) return 0;
	return xlivebg_setcfg_int(cfgpath, val);
}

int xlivebg_defcfg_vec(const char *cfgpath, float *vec)
{
	if(xlivebg_havecfg(cfgpath)) return 0;
	return xlivebg_setcfg_vec(cfgpath, vec);
}


/* helpers */
void xlivebg_gl_viewport(int sidx)
{
	struct xlivebg_screen *scr = xlivebg_screen(sidx);

	glViewport(scr->vport[0], scr->vport[1], scr->vport[2], scr->vport[3]);
}

void xlivebg_clear(unsigned int mask)
{
	static float varr[2][24] = {
		{0, 0, 0, 1, 1, 0, 0, 0, 0, -1, 1, 0, 0, 0, 0, -1, -1, 0, 0, 0, 0, 1, -1, 0},
		{5, 5, 5, -1, 1, 0, 5, 5, 5, -1, -1, 0, 5, 5, 5, 1, -1, 0, 5, 5, 5, 1, 1, 0}
	};
	float *vptr;
	int i, cur_prog = 0;

	if(cfg.bgmode < 0 || cfg.bgmode > 2) return;

	if(cfg.bgmode == XLIVEBG_BG_SOLID) {
		glClearColor(cfg.color[0].r, cfg.color[0].g, cfg.color[0].b, 1);
		glClear(mask);
		return;
	}
	vptr = varr[cfg.bgmode - 1];

	glClear(mask & ~GL_COLOR_BUFFER_BIT);

	glPushAttrib(GL_ENABLE_BIT | GL_TRANSFORM_BIT);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_TEXTURE_2D);

	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);

	if(xlivebg_gl_use_program) {
		glGetIntegerv(GL_CURRENT_PROGRAM, &cur_prog);
		xlivebg_gl_use_program(0);
	}
	if(xlivebg_gl_bind_buffer) {
		xlivebg_gl_bind_buffer(GL_ARRAY_BUFFER, 0);
	}

	vptr[0] = vptr[6] = cfg.color[0].r;
	vptr[1] = vptr[7] = cfg.color[0].g;
	vptr[2] = vptr[8] = cfg.color[0].b;
	vptr[12] = vptr[18] = cfg.color[1].r;
	vptr[13] = vptr[19] = cfg.color[1].g;
	vptr[14] = vptr[20] = cfg.color[1].b;

	glInterleavedArrays(GL_C3F_V3F, 0, vptr);

	for(i=0; i<num_screens; i++) {
		xlivebg_gl_viewport(i);
		glDrawArrays(GL_QUADS, 0, 4);
	}

	glPopClientAttrib();
	if(cur_prog) {
		xlivebg_gl_use_program(cur_prog);
	}

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glPopAttrib();
}

void xlivebg_calc_image_proj(int sidx, float img_aspect, float *xform)
{
	static const float ident[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
	float vpscale;
	struct xlivebg_screen *scr = xlivebg_screen(sidx);
	float aspect = (float)scr->width / (float)scr->height;
	int fit_mode = xlivebg_fit_mode(sidx);

	memcpy(xform, ident, sizeof ident);

	if(fit_mode != XLIVEBG_FIT_STRETCH) {
		if(aspect > img_aspect) {
			vpscale = xform[0] = img_aspect / aspect;
			xform[5] = 1.0f;
		} else {
			vpscale = xform[5] = aspect / img_aspect;
			xform[0] = 1.0f;
		}

		if(fit_mode == XLIVEBG_FIT_CROP) {
			float cropscale, maxpan, tx, ty;
			float cdir[2];

			cropscale = 1.0f / vpscale;
			cropscale = 1.0f + (cropscale - 1.0f) * xlivebg_crop_zoom(sidx);
			maxpan = cropscale - 1.0f;

			xform[0] *= cropscale;
			xform[5] *= cropscale;

			xlivebg_crop_dir(sidx, cdir);
			if(aspect > img_aspect) {
				tx = 0.0f;
				ty = -cdir[1] * maxpan;
			} else {
				tx = -cdir[0] * maxpan;
				ty = 0.0f;
			}

			xform[12] = tx;
			xform[13] = ty;
		} else {
			xform[12] = xform[13] = 0.0f;
		}
	}
}

void xlivebg_gl_image_proj(int scr, float img_aspect)
{
	float mat[16];
	xlivebg_calc_image_proj(scr, img_aspect, mat);

	glPushAttrib(GL_TRANSFORM_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(mat);
	glPopAttrib();
}

void xlivebg_mouse_pos(int *mx, int *my)
{
	app_getmouse(mx, my);
}

static char *skip_space(char *s)
{
	while(*s && isspace(*s)) s++;
	return s;
}

/* returns 1 if it handled the update, 0 otherwise */
static int update_builtin_cfg(const char *cfgpath, struct ts_value *tsval)
{
	if(strcmp(cfgpath, CFGNAME_ACTIVE) == 0 && tsval) {
		struct xlivebg_plugin *p = find_plugin(tsval->str);
		if(p) activate_plugin(p);
		return 1;
	}
	if(strcmp(cfgpath, CFGNAME_IMAGE) == 0 || strcmp(cfgpath, CFGNAME_ANIM_MASK) == 0) {
		struct xlivebg_image *img = 0;
		const char *fname = 0;
		if(tsval && tsval->str && *tsval->str) {
			int idx = find_image(tsval->str);
			if(idx >= 0) {
				img = get_image(idx);
			} else {
				if(!(img = malloc(sizeof *img)) || load_image(img, tsval->str) == -1) {
					fprintf(stderr, "update_builtin_cfg(%s<-%s): failed to load image\n", cfgpath, tsval->str);
					free(img);
					return 1;
				}
				add_image(img);
			}
			fname = tsval->str;
		}
		if(strcmp(cfgpath, CFGNAME_IMAGE) == 0) {
			free(cfg.image);
			cfg.image = fname ? strdup(fname) : 0;
			set_bg_image(0, img);
		} else {
			free(cfg.anm_mask);
			cfg.anm_mask = fname ? strdup(fname) : 0;
			set_anim_mask(0, img);
		}
		return 1;
	}
	if(strcmp(cfgpath, CFGNAME_COLOR) == 0) {
		if(tsval) {
			memcpy(cfg.color, tsval->vec, sizeof *cfg.color);
		} else {
			memset(cfg.color, 0, sizeof *cfg.color);
		}
		return 1;
	}
	if(strcmp(cfgpath, CFGNAME_COLOR2) == 0) {
		if(tsval) {
			memcpy(cfg.color + 1, tsval->vec, sizeof *cfg.color);
		} else {
			memset(cfg.color, 0, sizeof *cfg.color);
		}
		return 1;
	}
	if(strcmp(cfgpath, CFGNAME_BGMODE) == 0) {
		if(tsval) {
			cfg.bgmode = cfg_parse_bgmode(tsval->str);
		} else {
			cfg.bgmode = 0;
		}
		return 1;
	}
	if(strcmp(cfgpath, CFGNAME_FPS) == 0) {
		if(tsval) {
			cfg.fps_override = tsval->inum;
			if(cfg.fps_override > 0) {
				cfg.fps_override_interval = 1000000 / cfg.fps_override;
			}
		} else {
			cfg.fps_override = -1;
		}
		return 1;
	}
	if(strcmp(cfgpath, CFGNAME_FIT) == 0) {
		cfg.fit = tsval ? cfg_parse_fit(tsval->str) : 0;
		return 1;
	}
	if(strcmp(cfgpath, CFGNAME_CROP_ZOOM) == 0) {
		cfg.zoom = tsval ? tsval->fnum : 1;
		return 1;
	}
	if(strcmp(cfgpath, CFGNAME_CROP_DIR) == 0) {
		if(tsval) {
			cfg.crop_dir[0] = tsval->vec[0];
			cfg.crop_dir[1] = tsval->vec[1];
		} else {
			cfg.crop_dir[0] = cfg.crop_dir[1] = 0;
		}
		return 1;
	}

	return 0;
}

static void update_cfg(const char *cfgpath, struct ts_value *tsval)
{
	struct xlivebg_plugin *p;
	char *aname, *ptr, *end;
	char *buf;
	int len, aname_len;

	/* first give update_builtin_cfg a chance to handle it */
	if(update_builtin_cfg(cfgpath, tsval)) {
		return;
	}

	if(!(p = get_active_plugin())) {
		return;
	}

	buf = alloca(strlen(p->name) + 16);
	sprintf(buf, "xlivebg.%s.", p->name);
	if(!strstr(cfgpath, buf)) {
		/* this property has nothing to do with the current plugin */
		return;
	}

	/* if the plugin didn't specify a property list or a prop callback, we'll have to restart it */
	if(!p->props || !p->prop) {
		printf("update_cfg: restarting live wallpaper\n");
		if(p->stop) p->stop(p->data);
		activate_plugin(p);
		return;
	}

	if((aname = strrchr(cfgpath, '.'))) {
		aname++;
	} else {
		aname = (char*)cfgpath;
	}
	aname_len = strlen(aname);

	/* check to see if the attribute name matches any attributes declared by the plugin */
	ptr = p->props;
	while((ptr = strstr(ptr, "id"))) {
		ptr = skip_space(ptr + 3);
		if(*ptr != '=') continue;
		ptr = skip_space(ptr + 1);
		if(*ptr != '"') continue;
		end = ++ptr;
		while(*end && *end != '\n' && *end != '"') end++;
		if(*end != '"') continue;

		len = end - ptr;
		if(len == aname_len && memcmp(aname, ptr, len) == 0) {
			/* found at least one match, notify the plugin and return */
			p->prop(aname, p->data);
			break;
		}
	}
}

static const char *get_builtin_str(const char *cfgpath)
{
	if(strcmp(cfgpath, CFGNAME_ACTIVE) == 0) {
		return cfg.act_plugin;
	}
	if(strcmp(cfgpath, CFGNAME_IMAGE) == 0) {
		return cfg.image;
	}
	if(strcmp(cfgpath, CFGNAME_ANIM_MASK) == 0) {
		return cfg.anm_mask;
	}
	return 0;
}

static float *get_builtin_num(const char *cfgpath)
{
	if(strcmp(cfgpath, CFGNAME_CROP_ZOOM) == 0) {
		return &cfg.zoom;
	}
	return 0;
}

static int *get_builtin_int(const char *cfgpath)
{
	if(strcmp(cfgpath, CFGNAME_FPS) == 0) {
		return &cfg.fps_override;
	}
	if(strcmp(cfgpath, CFGNAME_FIT) == 0) {
		return &cfg.fit;
	}
	if(strcmp(cfgpath, CFGNAME_BGMODE) == 0) {
		return &cfg.bgmode;
	}
	return 0;
}

static float *get_builtin_vec(const char *cfgpath)
{
	if(strcmp(cfgpath, CFGNAME_COLOR) == 0) {
		return &cfg.color[0].r;
	}
	if(strcmp(cfgpath, CFGNAME_COLOR2) == 0) {
		return &cfg.color[1].r;
	}
	if(strcmp(cfgpath, CFGNAME_CROP_DIR) == 0) {
		return cfg.crop_dir;
	}
	return 0;
}
