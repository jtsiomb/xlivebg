#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "treestore.h"
#include "cfg.h"

void init_cfg(void)
{
	static float zero_vec[4];
	float color[4];
	struct ts_node *ts;
	char *cfgpath;
	const char *str;
	float *vec;

	/* init default state */
	memset(&cfg, 0, sizeof cfg);
	cfg.fps_override = -1;

	/* load a config file if there is one */
	if(!(cfgpath = get_config_path())) {
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

	cfg.act_plugin = (char*)ts_lookup_str(ts, "xlivebg.active", 0);

	if((str = ts_lookup_str(ts, "xlivebg.image", 0))) {
		cfg.image = strdup(str);
	}
	if((str = ts_lookup_str(ts, "xlivebg.anim_mask", 0))) {
		cfg.anm_mask = strdup(str);
	}
	if((vec = ts_lookup_vec(ts, "xlivebg.color", 0))) {
		memcpy(color, vec, sizeof color);
	} else {
		memcpy(color, zero_vec, sizeof color);
	}
	vec = ts_lookup_vec(ts, "xlivebg.color_top", color);
	memcpy(cfg.color, vec, sizeof *cfg.color);
	vec = ts_lookup_vec(ts, "xlivebg.color_bottom", color);
	memcpy(cfg.color + 1, vec, sizeof *cfg.color);

	cfg.fps_override = ts_lookup_int(ts, "xlivebg.fps", -1);

	if((str = ts_lookup_str(ts, "xlivebg.fit", 0))) {
		if(strcasecmp(str, "full") == 0) {
			cfg.fit = XLIVEBG_FIT_FULL;
		} else if(strcasecmp(str, "crop") == 0) {
			cfg.fit = XLIVEBG_FIT_CROP;
		} else {
			fprintf(stderr, "invalid value to option \"xlivebg.fit\": %s\n", str);
		}
	}

	cfg.zoom = ts_lookup_num(ts, "xlivebg.crop_zoom", 1.0f);
	vec = ts_lookup_vec(ts, "xlivebg.crop_dir", zero_vec);
	cfg.crop_dir[0] = vec[0];
	cfg.crop_dir[1] = vec[1];

	cfg.ts = ts;
}
