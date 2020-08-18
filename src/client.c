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
/* This is the command-line client for sending commands to xlivebg through the
 * UNIX domain socket.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ctrl.h"

static int cmd_generic(int s, int argc, char **argv);
static int cmd_list(int s, int argc, char **argv);

static int read_line(int fd, char *line, int maxsz);

static int parse_args(int argc, char **argv);
static void print_usage(void);

static int cmd = -1;

static struct {
	const char *name;
	int (*func)(int, int, char**);
} commands[] = {
	{"list", cmd_list},
	{"switch", cmd_generic},
	{0, 0}
};

int client_main(int argc, char **argv)
{
	int s;
	struct sockaddr_un addr;

	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	if((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("failed to create UNIX domain socket");
		return 1;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_PATH);

	if(connect(s, (struct sockaddr*)&addr, SUN_LEN(&addr)) == -1) {
		fprintf(stderr, "failed to connect to UNIX domain socket: " SOCK_PATH ": %s\n",
				strerror(errno));
		return 1;
	}

	if(commands[cmd].func(s, argc, argv) == -1) {
		return 1;
	}

	return 0;
}


static int cmd_generic(int s, int argc, char **argv)
{
	int i, len = 0;
	char *cmd, *ptr;
	char buf[5];

	for(i=1; i<argc; i++) {
		len += strlen(argv[i]) + 1;
	}
	ptr = cmd = alloca(len + 1);

	for(i=1; i<argc; i++) {
		ptr += sprintf(ptr, "%s ", argv[i]);
	}
	ptr[-1] = '\n';
	*ptr = 0;

	write(s, cmd, len);
	if(read_line(s, buf, sizeof buf) >= 0 && strcmp(buf, "OK!\n") == 0) {
		return 0;
	}

	fprintf(stderr, "Command %s failed\n", argv[1]);
	return -1;
}

static int cmd_list(int s, int argc, char **argv)
{
	int state = 0;
	int num_lines = 0;
	char buf[1024];
	char *endp;

	write(s, "list\n", 5);

	while(read_line(s, buf, sizeof buf) >= 0) {
		switch(state) {
		case 0:
			if(strcmp(buf, "OK!\n") != 0) {
				fprintf(stderr, "List command failed\n");
				return -1;
			}
			state++;
			break;

		case 1:
			num_lines = strtol(buf, &endp, 10);
			if(endp == buf) {
				fprintf(stderr, "Got invalid response to list command!\n");
				return -1;
			}
			state++;
			break;

		case 2:
			fputs(buf, stdout);
			if(--num_lines <= 0) return 0;
			break;
		}
	}
	return -1;
}

static char inbuf[1024];
static char *inbuf_head = inbuf;
static char *inbuf_tail = inbuf;

static int read_line(int fd, char *line, int maxsz)
{
	int rd;

	while(maxsz > 1) {	/* >1 to leave space for the terminator */
		if(inbuf_head == inbuf_tail) {
			while((rd = read(fd, inbuf, sizeof inbuf)) == -1 && rd == EINTR);
			if(rd == 0) return -1;
			inbuf_head = inbuf;
			inbuf_tail = inbuf + rd;
		}
		if((*line++ = *inbuf_head++) == '\n') {
			break;
		}
		maxsz--;
	}

	*line = 0;
	return 0;
}

static int parse_args(int argc, char **argv)
{
	int i;

	if(!argv[1]) {
		fprintf(stderr, "missing command\n");
		return -1;
	}

	if(strcmp(argv[1], "help") == 0) {
		print_usage();
		exit(0);
	}

	cmd = -1;
	for(i=0; commands[i].name; i++) {
		if(strcmp(argv[1], commands[i].name) == 0) {
			cmd = i;
			break;
		}
	}

	if(cmd < 0) {
		fprintf(stderr, "unknown command: %s\n", argv[1]);
		return -1;
	}
	return 0;
}

static void print_usage(void)
{
	printf("Usage: xlivebg-cmd <command> [cmd-args]\n");
	printf("Commands:\n");
	printf("  list: get list of available live wallpapers\n");
	printf("  switch <name>: switch live wallpaper\n");
	printf("  help: print usage and exit\n");
}
