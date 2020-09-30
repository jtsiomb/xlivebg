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
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ctrl.h"

static int cmd_generic(int argc, char **argv);
static int cmd_getupd(int argc, char **argv);
static int cmd_list(int argc, char **argv);
static int cmd_lsprop(int argc, char **argv);
static int cmd_setprop(int argc, char **argv);
static int cmd_getprop(int argc, char **argv);

static int read_line(int fd, char *line, int maxsz);

static int parse_args(int argc, char **argv);
static void print_usage(void);

static struct {
	const char *name;
	int (*func)(int, char**);
} commands[] = {
	{"ping", cmd_generic},
	{"save", cmd_generic},
	{"getupd", cmd_getupd},
	{"list", cmd_list},
	{"switch", cmd_generic},
	{"lsprop", cmd_lsprop},
	{"setprop", cmd_setprop},
	{"getprop", cmd_getprop},
	{0, 0}
};

static int sock;
static int cmd;


static void timeout(int s)
{
	shutdown(sock, SHUT_RD);
}

int client_main(int argc, char **argv)
{
	int res;
	struct sockaddr_un addr;

	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("failed to create UNIX domain socket");
		return 1;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_PATH);

	if(connect(sock, (struct sockaddr*)&addr, SUN_LEN(&addr)) == -1) {
		fprintf(stderr, "failed to connect to UNIX domain socket: " SOCK_PATH ": %s\n",
				strerror(errno));
		return 1;
	}

	signal(SIGALRM, timeout);
	alarm(8);

	res = commands[cmd].func(argc, argv);
	alarm(0);
	return res == -1 ? 1 : 0;
}


static int cmd_generic(int argc, char **argv)
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

	write(sock, cmd, len);
	if(read_line(sock, buf, sizeof buf) >= 0 && strcmp(buf, "OK!\n") == 0) {
		return 0;
	}

	fprintf(stderr, "Command %s failed\n", argv[1]);
	return -1;
}

static int cmd_getupd(int argc, char **argv)
{
	char buf[256];
	char *endp;
	int state = 0;
	long val;

	write(sock, "getupd\n", 7);

	while(read_line(sock, buf, sizeof buf) >= 0) {
		switch(state) {
		case 0:
			if(strcmp(buf, "OK!\n") != 0) {
				fprintf(stderr, "getupd command failed\n");
				return -1;
			}
			state++;
			break;

		case 1:
		case 2:
			val = strtol(buf, &endp, 10);
			if(endp == buf || (state == 1 && val != 1)) {
				fprintf(stderr, "Got invalid response to getupd command!\n");
				return -1;
			}
			if(state > 1) {
				printf("%ld us (%g fps)\n", val, 1000000.0f / val);
				return 0;
			}
			state++;
		}
	}

	return 0;
}

static int cmd_list(int argc, char **argv)
{
	int state = 0;
	int num_lines = 0;
	char buf[256];
	char *endp;

	write(sock, "list\n", 5);

	while(read_line(sock, buf, sizeof buf) >= 0) {
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

static int cmd_lsprop(int argc, char **argv)
{
	char buf[256];
	int level = 0;

	write(sock, "lsprop\n", 7);

	if(read_line(sock, buf, sizeof buf) == -1 || strcmp(buf, "OK!\n") != 0) {
		fprintf(stderr, "Property list command failed\n");
		return -1;
	}

	while(read_line(sock, buf, sizeof buf) >= 0) {
		char *ptr = buf;
		while(*ptr) {
			char c = *ptr++;
			switch(c) {
			case '{': level++; break;
			case '}': level--; break;
			}
			putchar(c);
		}
		if(level <= 0) break;
	}

	return 0;
}

static int cmd_setprop(int argc, char **argv)
{
	char buf[512];
	int len, ival;
	float fval;
	char *endp;
	char *arg_type, *arg_name, *arg_value;

	if(argc < 5) {
		fprintf(stderr, "not enough arguments\n");
		return -1;
	}

	arg_type = argv[2];
	arg_name = argv[3];
	arg_value = argv[4];

	if(strcmp(arg_type, "text") == 0) {
		len = sprintf(buf, "propstr %s %s\n", arg_name, arg_value);
	} else if(strcmp(arg_type, "number") == 0) {
		fval = strtod(arg_value, &endp);
		if(*endp != 0) {
			fprintf(stderr, "trying to pass \"%s\" as a number\n", arg_value);
			return -1;
		}
		len = sprintf(buf, "propnum %s %g\n", arg_name, fval);
	} else if(strcmp(arg_type, "integer") == 0) {
		ival = strtol(arg_value, &endp, 10);
		if(*endp != 0) {
			fprintf(stderr, "trying to pass \"%s\" as an integer\n", arg_value);
			return -1;
		}
		len = sprintf(buf, "propint %s %d\n", arg_name, ival);
	} else /*if(strcmp(arg_type, "vector") == 0) */{
		/* TODO */
		fprintf(stderr, "unrecognized property type: %s\n", arg_type);
		return -1;
	}

	write(sock, buf, len);
	if(read_line(sock, buf, sizeof buf) == -1 || memcmp(buf, "OK!\n", 4) != 0) {
		fprintf(stderr, "cmd_setprop: failed to send property\n");
		return -1;
	}
	return 0;
}

static int cmd_getprop(int argc, char **argv)
{
	int i, len, count;
	char buf[512];
	char *arg_type, *arg_name, *cmd;

	if(argc < 4) {
		fprintf(stderr, "not enough arguments\n");
		return -1;
	}

	arg_type = argv[2];
	arg_name = argv[3];

	if(strcmp(arg_type, "text") == 0) {
		cmd = "getpropstr";
	} else if(strcmp(arg_type, "number") == 0) {
		cmd = "getpropnum";
	} else if(strcmp(arg_type, "integer") == 0) {
		cmd = "getpropint";
	} else if(strcmp(arg_type, "vector") == 0) {
		cmd = "getpropvec";
	} else {
		fprintf(stderr, "unrecognized property type: %s\n", arg_type);
		return -1;
	}

	len = sprintf(buf, "%s %s\n", cmd, arg_name);
	write(sock, buf, len);

	if(read_line(sock, buf, sizeof buf) == -1 || memcmp(buf, "OK!\n", 4) != 0) {
		goto err;
	}
	if(read_line(sock, buf, sizeof buf) == -1) {
		goto err;
	}
	if((count = atoi(buf)) <= 0) {
		return 0;
	}

	for(i=0; i<count; i++) {
		if(read_line(sock, buf, sizeof buf) == -1) {
			goto err;
		}
		fputs(buf, stdout);
	}
	return 0;

err:
	fprintf(stderr, "cmd_getprop: failed to retrieve property: %s (%s)\n", arg_name, arg_type);
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
	printf("  ping: check to see if xlivebg is running\n");
	printf("  save: save current settings to user configuration file\n");
	printf("  getupd: print the current graphics update rate\n");
	printf("  list: get list of available live wallpapers\n");
	printf("  switch <name>: switch live wallpaper\n");
	printf("  lsprop [name]: list properties of named or current live wallpaper\n");
	printf("  setprop <type> <property> <value>: sets a property\n");
	printf("  getprop <type>: prints the current value of a property\n");
	printf("  help: print usage and exit\n");
}
