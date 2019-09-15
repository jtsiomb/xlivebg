#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "treestore.h"
#include "cfg.h"

void init_cfg(void)
{
	float def_col[] = {0, 0, 0, 0};
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

	if((str = ts_lookup_str(ts, "xlivebg.image", 0))) {
		cfg.image = strdup(str);
	}
	if((str = ts_lookup_str(ts, "xlivebg.anim-mask", 0))) {
		cfg.anm_mask = strdup(str);
	}
	if((vec = ts_lookup_vec(ts, "xlivebg.color", 0))) {
		memcpy(def_col, vec, sizeof def_col);
	}
	vec = ts_lookup_vec(ts, "xlivebg.color-top", def_col);
	memcpy(cfg.color, vec, sizeof *cfg.color);
	vec = ts_lookup_vec(ts, "xlivebg.color-bottom", def_col);
	memcpy(cfg.color + 1, vec, sizeof *cfg.color);

	cfg.fps_override = ts_lookup_int(ts, "xlivebg.fps", -1);
}
