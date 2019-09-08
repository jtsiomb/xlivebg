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
#include <unistd.h>
#include <pwd.h>
#include "util.h"

static char *homedir, *cfgfile;

char *get_home_dir(void)
{
	char *home;
	struct passwd *pw;

	if(!homedir) {
		if(!(home = getenv("HOME"))) {
			if(!(pw = getpwuid(getpid()))) {
				perror("get_home_dir: getpwuid failed");
				abort();
			}
			home = pw->pw_dir;
		}

		if(!(homedir = malloc(strlen(home) + 1))) {
			perror("get_home_dir: malloc failed");
			abort();
		}
		strcpy(homedir, home);
	}

	return homedir;
}

char *get_config_path(void)
{
	FILE *fp;
	char *fname = 0;
	char *home = get_home_dir();

	if(!cfgfile) {
		if(!(fname = malloc(strlen(home) + 32))) {
			perror("get_config_path: malloc failed");
			return 0;
		}

		sprintf(fname, "%s/.xlivebg/config", home);
		if((fp = fopen(fname, "r"))) {
			fclose(fp);
			goto done;
		}
		sprintf(fname, "%s/.config/xlivebg.conf", home);
		if((fp = fopen(fname, "r"))) {
			fclose(fp);
			goto done;
		}
		sprintf(fname, "/etc/xlivebg.conf");
		if((fp = fopen(fname, "r"))) {
			fclose(fp);
			goto done;
		}

done:	cfgfile = fname;
	}

	return cfgfile;
}
