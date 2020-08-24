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
#include "bg.h"
#include "cmd.h"

static struct bginfo *bglist;
static int bglist_size;

int bg_create_list(void)
{
	static char *liststr;
	static int liststr_size;
	char *ptr, *end;
	int size;
	struct bginfo *bg;

	free(bglist);

	if(!liststr) {
		liststr_size = 512;
		if(!(liststr = malloc(liststr_size))) {
			fprintf(stderr, "Failed to create wallpaper list buffer (%db)\n", liststr_size);
			liststr_size = 0;
			return -1;
		}
	}

	if((size = cmd_list(liststr, liststr_size)) <= 0) {
		return -1;
	}
	if(size > liststr_size) {
		free(liststr);
		liststr_size = size;
		if(!(liststr = malloc(liststr_size))) {
			fprintf(stderr, "Failed to create wallpaper list buffer (%db)\n", liststr_size);
			liststr_size = 0;
			return -1;
		}
		if(cmd_list(liststr, liststr_size) <= 0) {
			return -1;
		}
	}

	bglist_size = 0;
	ptr = liststr;
	while(*ptr) {
		if(*ptr++ == ':') bglist_size++;
	}

	if(!(bglist = malloc(bglist_size * sizeof *bglist))) {
		fprintf(stderr, "Failed to allocate wallpaper list\n");
		bglist_size = 0;
		return -1;
	}

	bglist_size = 0;
	ptr = liststr;
	while(*ptr) {
		end = ptr;
		while(*end && *end != ':') end++;
		*end = 0;
		if(end > ptr) {
			bg = bglist + bglist_size++;
			bg->name = strdup(ptr);
		}
		while(*end && *end != '\n') end++;

		ptr = *end ? end + 1 : end;
	}

	return 0;
}

void bg_destroy_list(void)
{
	free(bglist);
	bglist_size = 0;
}
