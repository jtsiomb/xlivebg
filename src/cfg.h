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
#ifndef CFG_H_
#define CFG_H_

#include "util.h"
#include "treestore.h"

struct cfg {
	char *act_plugin;
	char *image, *anm_mask;
	int bgmode;
	struct color color[2];
	int fps_override;
	long fps_override_interval;
	int fit;
	float zoom;
	float crop_dir[2];

	struct ts_node *ts;
};

extern struct cfg cfg;
extern char *cfgpath;

#define CFGNAME_ACTIVE		"xlivebg.active"
#define CFGNAME_IMAGE		"xlivebg.image"
#define CFGNAME_ANIM_MASK	"xlivebg.anim_mask"
#define CFGNAME_BGMODE		"xlivebg.bgmode"
#define CFGNAME_COLOR		"xlivebg.color"
#define CFGNAME_COLOR2		"xlivebg.color2"
#define CFGNAME_FPS			"xlivebg.fps"
#define CFGNAME_FIT			"xlivebg.fit"
#define CFGNAME_CROP_ZOOM	"xlivebg.crop_zoom"
#define CFGNAME_CROP_DIR	"xlivebg.crop_dir"

void init_cfg(void);
int save_cfg(const char *fname);

int cfg_parse_fit(const char *str);
int cfg_parse_bgmode(const char *str);

#endif	/* CFG_H_ */
