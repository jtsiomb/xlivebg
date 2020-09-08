#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "treestore.h"
#include "cfg.h"

char *cfgpath;

void init_cfg(void)
{
	static float zero_vec[4];
	float color[4];
	struct ts_node *ts;
	const char *str;
	float *vec;

	/* init default state */
	memset(&cfg, 0, sizeof cfg);
	cfg.fps_override = -1;

	/* load a config file if there is one */
	if(!(cfgpath = get_config_path())) {
		if(!(ts = ts_alloc_node()) || !(ts->name = strdup("xlivebg"))) {
			fprintf(stderr, "failed to create root config node\n");
			return;
		}
		return;
	}
	printf("Using config file: %s\n", cfgpath);
	if(!(ts = ts_load(cfgpath))) {
		fprintf(stderr, "failed to load config file\n");
		return;
	}
	if(strcmp(ts->name, "xlivebg") != 0) {
		fprintf(stderr, "invalid or corrupted config file. missing xlivebg root node\n");
		ts_free_tree(ts);
		return;
	}

	if((str = ts_lookup_str(ts, CFGNAME_ACTIVE, 0))) {
		cfg.act_plugin = strdup(str);
	}

	if((str = ts_lookup_str(ts, CFGNAME_IMAGE, 0))) {
		cfg.image = strdup(str);
	}
	if((str = ts_lookup_str(ts, CFGNAME_ANIM_MASK, 0))) {
		cfg.anm_mask = strdup(str);
	}
	if((vec = ts_lookup_vec(ts, CFGNAME_COLOR, 0))) {
		memcpy(color, vec, sizeof color);
	} else {
		memcpy(color, zero_vec, sizeof color);
	}
	vec = ts_lookup_vec(ts, CFGNAME_COLOR_TOP, color);
	memcpy(cfg.color, vec, sizeof *cfg.color);
	vec = ts_lookup_vec(ts, CFGNAME_COLOR_BOT, color);
	memcpy(cfg.color + 1, vec, sizeof *cfg.color);

	cfg.fps_override = ts_lookup_int(ts, CFGNAME_FPS, -1);

	if((str = ts_lookup_str(ts, CFGNAME_FIT, 0))) {
		cfg.fit = cfg_parse_fit(str);
	}

	cfg.zoom = ts_lookup_num(ts, CFGNAME_CROP_ZOOM, 1.0f);
	vec = ts_lookup_vec(ts, CFGNAME_CROP_DIR, zero_vec);
	cfg.crop_dir[0] = vec[0];
	cfg.crop_dir[1] = vec[1];

	cfg.ts = ts;
}

int cfg_parse_fit(const char *str)
{
	if(strcasecmp(str, "full") == 0) {
		return XLIVEBG_FIT_FULL;
	}
	if(strcasecmp(str, "crop") == 0) {
		return XLIVEBG_FIT_CROP;
	}
	if(strcasecmp(str, "stretch") == 0) {
		return XLIVEBG_FIT_STRETCH;
	}

	fprintf(stderr, "invalid value to option \"" CFGNAME_FIT "\": %s\n", str);
	return XLIVEBG_FIT_FULL;
}
