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
#ifndef XLIVEBG_H_
#define XLIVEBG_H_

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199900L
#include <stdint.h>
#else
#include <inttypes.h>
#endif

/* microsecond intervals for common framerates for upd_interval */
#define XLIVEBG_NOUPD	0
#define XLIVEBG_15FPS	66666
#define XLIVEBG_20FPS	50000
#define XLIVEBG_25FPS	40000
#define XLIVEBG_30FPS	33333

enum {
	XLIVEBG_FIT_FULL,
	XLIVEBG_FIT_CROP,
	XLIVEBG_FIT_STRETCH
};


typedef int (*xlivebg_init_func)(void*);
typedef void (*xlivebg_cleanup_func)(void*);
typedef void (*xlivebg_start_func)(long, void*);
typedef void (*xlivebg_stop_func)(void*);
typedef void (*xlivebg_draw_func)(long, void*);
typedef void (*xlivebg_prop_func)(const char*, void*);

struct xlivebg_image {
	int width, height;
	uint32_t *pixels;
	char *path;
	unsigned int tex;
};

struct xlivebg_screen {
	int x, y, width, height;
	int root_width, root_height;
	int phys_width, phys_height;
	float aspect;

	char *name;

	struct xlivebg_image *bgimg;
	int num_bgimg;

	int vport[4];
};

struct xlivebg_plugin {
	const char *name, *desc;
	const char *props;	/* list of properties this plugin uses (to restart or call prop when any change) */
	long upd_interval;	/* requested update interval in microseconds */
	xlivebg_init_func init;		/* called during init, with a valid OpenGL context */
	xlivebg_cleanup_func cleanup;	/* called during shutdown (optional) */
	xlivebg_start_func start;	/* called when the plugin is activated (optional) */
	xlivebg_stop_func stop;		/* called when the plugin is deactivated (optional) */
	xlivebg_draw_func draw;		/* called to draw every frame */
	xlivebg_prop_func prop;		/* called when a property in props has changed (optional) */

	void *data, *so;
};

int xlivebg_register_plugin(struct xlivebg_plugin *plugin);

int xlivebg_screen_count(void);
struct xlivebg_screen *xlivebg_screen(int idx);

struct xlivebg_image *xlivebg_bg_image(int scr);
struct xlivebg_image *xlivebg_anim_mask(int scr);

int xlivebg_fit_mode(int scr);
float xlivebg_crop_zoom(int scr);
void xlivebg_crop_dir(int scr, float *dirvec);

/* plugin configuration interface */
int xlivebg_havecfg(const char *cfgpath);
const char *xlivebg_getcfg_str(const char *cfgpath, const char *def_val);
float xlivebg_getcfg_num(const char *cfgpath, float def_val);
int xlivebg_getcfg_int(const char *cfgpath, int def_val);
float *xlivebg_getcfg_vec(const char *cfgpath, float *def_val);

void xlivebg_gl_viewport(int scr);

/* xlivebg_calc_image_proj returns a projection matrix suitable for
 * transforming a fullscreen quad (-1,1), in such a way as to abide by the fit
 * options in the configuration, when used to show an image with the specified
 * aspect ratio.
 *
 * xform is the output matrix, should point to an array of 16 floats (GL order)
 */

void xlivebg_calc_image_proj(int scr, float img_aspect, float *xform);
/* calls xlivebg_calc_image_proj, then sets the projection matrix */
void xlivebg_gl_image_proj(int scr, float img_aspect);

void xlivebg_mouse_pos(int *mx, int *my);

#endif	/* XLIVEBG_H_ */
