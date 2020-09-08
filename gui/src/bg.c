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
#include "treestore.h"
#include "bg.h"
#include "cmd.h"

static int parse_proplist(struct bginfo *bg, char *plist, int plist_size);

static struct bginfo *bglist;
static int bglist_size;

int bg_create_list(void)
{
	static char *rbuf;
	static int rbuf_size;
	char *ptr, *end;
	int i, size;
	struct bginfo *bg;

	free(bglist);

	if(!rbuf) {
		rbuf_size = 512;
		if(!(rbuf = malloc(rbuf_size))) {
			fprintf(stderr, "Failed to allocate wallpaper list buffer (%db)\n", rbuf_size);
			rbuf_size = 0;
			return -1;
		}
	}

retry_list:
	if((size = cmd_list(rbuf, rbuf_size)) <= 0) {
		fprintf(stderr, "Failed to retrieve wallpaper list\n");
		return -1;
	}
	if(size > rbuf_size) {
		free(rbuf);
		rbuf_size = size;
		if(!(rbuf = malloc(rbuf_size))) {
			fprintf(stderr, "Failed to allocae wallpaper list buffer (%db)\n", rbuf_size);
			rbuf_size = 0;
			return -1;
		}
		goto retry_list;
	}

	bglist_size = 0;
	ptr = rbuf;
	while(*ptr) {
		if(*ptr++ == ':') bglist_size++;
	}

	if(!(bglist = calloc(bglist_size, sizeof *bglist))) {
		fprintf(stderr, "Failed to allocate wallpaper list\n");
		bglist_size = 0;
		return -1;
	}

	bg = bglist;
	bglist_size = 0;
	ptr = rbuf;
	for(;;) {
		if(!(end = strchr(ptr, ':'))) break;
		*end = 0;
		if(!(bg->name = strdup(ptr))) {
			fprintf(stderr, "Failed to allocate wallpaper name\n");
			goto fail;
		}
		bglist_size++;
		ptr = end + 1;
		if(!(end = strchr(ptr, '\n'))) break;
		*end = 0;
		if(!(bg->desc = strdup(ptr))) {
			fprintf(stderr, "Failed to allocate wallpaper description\n");
			goto fail;
		}

		bg++;
		ptr = end + 1;
	}

	/* retrieve the list of properties for each wallpaper plugin */
	for(i=0; i<bglist_size; i++) {
		bg = bglist + i;
retry_proplist:
		if((size = cmd_proplist(bg->name, rbuf, rbuf_size)) <= 0) {
			fprintf(stderr, "Failed to retreive property list for wallpaper: %s\n", bg->name);
			continue;
		}
		if(size > rbuf_size) {
			free(rbuf);
			if(!(ptr = malloc(size))) {
				fprintf(stderr, "Failed to allocate wallpaper properties buffer (%db)\n", rbuf_size);
				continue;
			}
			rbuf = ptr;
			rbuf_size = size;
			goto retry_proplist;
		}

		parse_proplist(bg, rbuf, size);
	}

	return 0;

fail:
	while(--bg >= bglist) {
		free(bg->name);
		free(bg->desc);
	}
	free(bglist);
	bglist = 0;
	bglist_size = 0;
	return -1;
}

void bg_destroy_list(void)
{
	int i, j;
	for(i=0; i<bglist_size; i++) {
		free(bglist[i].name);
		free(bglist[i].desc);

		for(j=0; j<bglist[i].num_prop; j++) {
			free(bglist[i].prop[j].name);
			free(bglist[i].prop[j].fullname);
			free(bglist[i].prop[j].desc);
		}
		free(bglist[i].prop);
	}
	free(bglist);
	bglist = 0;
	bglist_size = 0;
}

struct bginfo *bg_list_item(int idx)
{
	if(idx < 0 || idx >= bglist_size) {
		return 0;
	}
	return bglist + idx;
}

int bg_list_size(void)
{
	return bglist_size;
}

struct bginfo *bg_active(void)
{
	char buf[256];
	char *end;
	int i;

	if(cmd_getprop_str("xlivebg.active", buf, sizeof buf) == -1) {
		return 0;
	}
	if((end = strrchr(buf, '\n'))) *end = 0;

	for(i=0; i<bglist_size; i++) {
		if(strcmp(bglist[i].name, buf) == 0) {
			return bglist + i;
		}
	}
	return 0;
}

int bg_switch(const char *name)
{
	if(!name || !*name) return -1;
	return cmd_setprop_str("xlivebg.active", name);
}


static int proptype(const char *s)
{
	if(strcmp(s, "bool") == 0) return BGPROP_BOOL;
	if(strcmp(s, "text") == 0) return BGPROP_TEXT;
	if(strcmp(s, "number") == 0) return BGPROP_NUMBER;
	if(strcmp(s, "integer") == 0) return BGPROP_INTEGER;
	if(strcmp(s, "color") == 0) return BGPROP_COLOR;
	if(strcmp(s, "filename") == 0) return BGPROP_FILENAME;
	if(strcmp(s, "dirname") == 0) return BGPROP_DIRNAME;
	if(strcmp(s, "pathname") == 0) return BGPROP_PATHNAME;
	return -1;
}

struct memfile {
	char *buf;
	int cur, size;
};

static long memread(void *buf, size_t bytes, void *uptr)
{
	struct memfile *mf = uptr;
	int bleft;

	if(mf->cur >= mf->size) return -1;

	bleft = mf->size - mf->cur;
	if(bytes > bleft) bytes = bleft;

	memcpy(buf, mf->buf + mf->cur, bytes);
	mf->cur += bytes;
	return bytes;
}

static int parse_proplist(struct bginfo *bg, char *plist, int plist_size)
{
	struct memfile memfile;
	struct ts_io io = {&memfile, memread, 0};
	struct ts_node *root, *node;
	const char *str, *idstr, *typestr;
	struct bgprop *prop;
	float *vec;

	memfile.buf = plist;
	memfile.cur = 0;
	memfile.size = plist_size;

	if(!(root = ts_load_io(&io))) {
		fprintf(stderr, "failed to parse property list\n");
		return -1;
	}
	if(strcmp(root->name, "proplist") != 0) {
		fprintf(stderr, "parse_proplist(%s): unexpected root node \"%s\" (expected: proplist)\n",
				bg->name, root->name);
		ts_free_tree(root);
		return -1;
	}

	if(!(prop = malloc(root->child_count * sizeof *prop))) {
		fprintf(stderr, "parse_proplist(%s): failed to allocate %d properties\n",
				bg->name, root->child_count);
		ts_free_tree(root);
		return -1;
	}
	bg->prop = prop;

	bg->num_prop = 0;
	node = root->child_list;
	while(node) {
		if(!(idstr = ts_get_attr_str(node, "id", 0)) ||
				!(typestr = ts_get_attr_str(node, "type", 0))) {
			fprintf(stderr, "parse_proplist(%s): invalid property %d\n", bg->name, bg->num_prop);
			goto skip;
		}
		memset(prop, 0, sizeof *prop);	/* clear previous attempts */
		if((prop->type = proptype(typestr)) == -1) {
			fprintf(stderr, "parse_proplist(%s): invalid property type: %s\n", bg->name, typestr);
			goto skip;
		}
		if(!(prop->name = strdup(idstr))) {
			fprintf(stderr, "parse_proplist(%s): failed to allocate prop name: %s\n", bg->name, idstr);
			goto skip;
		}
		if(!(prop->fullname = malloc(strlen(idstr) + strlen(bg->name) + 16))) {
			fprintf(stderr, "parse_proplist(%s): failed to allocate prop fullname: xlivebg.%s.%s\n", bg->name, bg->name, idstr);
			goto skip;
		}
		sprintf(prop->fullname, "xlivebg.%s.%s", bg->name, idstr);

		if((str = ts_get_attr_str(node, "desc", 0))) {
			prop->desc = strdup(str);
		}

		switch(prop->type) {
		case BGPROP_TEXT:
			prop->text.multiline = ts_get_attr_int(node, "multiline", 0);
			break;

		case BGPROP_NUMBER:
			if((vec = ts_get_attr_vec(node, "range", 0))) {
				prop->number.start = vec[0];
				prop->number.end = vec[1];
			}
			break;

		case BGPROP_INTEGER:
			if((vec = ts_get_attr_vec(node, "range", 0))) {
				prop->integer.start = vec[0];
				prop->integer.end = vec[1];
			}
			break;
		}

		bg->num_prop++;
		prop++;

		if(0) {
skip:		free(prop->name);
			free(prop->fullname);
			free(prop->desc);
		}
		node = node->next;
	}

	ts_free_tree(root);
	return 0;
}
