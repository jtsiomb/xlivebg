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
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ctrl.h"
#include "plugin.h"

struct client {
	int s;
	char inbuf[4096];
	char *inp;
};

#define MAX_SOCK	16
static int sockbuf[MAX_SOCK + 1];
static int *sock;
static int lis = -1;
static int num_sock;
static struct client clients[MAX_SOCK];

static void proc_cmd(int s, char *cmdstr);

int ctrl_init(void)
{
	int i;
	struct sockaddr_un addr;

	for(i=0; i<MAX_SOCK; i++) {
		clients[i].s = -1;
	}

	if(lis >= 0) {
		fprintf(stderr, "ctrl_init: already initialized!\n");
		return -1;
	}

	if((lis = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "ctrl_init: failed to create socket: %s\n", strerror(errno));
		return -1;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_PATH);
	if(bind(lis, (struct sockaddr*)&addr, SUN_LEN(&addr)) == -1) {
		perror("ctrl_init: failed to bind listening socket (" SOCK_PATH ")");
		close(lis);
		lis = -1;
		return -1;
	}
	listen(lis, 8);

	sockbuf[0] = lis;
	sock = sockbuf + 1;
	num_sock = 0;
	return 0;
}

void ctrl_shutdown(void)
{
	if(lis == -1) return;

	close(lis);
	lis = -1;
	remove(SOCK_PATH);
}

int *ctrl_sockets(int *count)
{
	if(lis == -1) {
		*count = 0;
		return 0;
	}
	*count = num_sock + 1;
	return sockbuf;
}

void ctrl_process(int s)
{
	int i, len;
	struct client *c;
	char buf[1024];
	char *ptr;

	if(s == lis) {
		if(num_sock >= MAX_SOCK) {
			fprintf(stderr, "ctrl_process: too many incoming connections\n");
			return;
		}
		if((s = accept(lis, 0, 0)) == -1) {
			perror("ctrl_process: failed to accept incoming connection");
			return;
		}
		fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);

		c = 0;
		for(i=0; i<MAX_SOCK; i++) {
			if(clients[i].s == -1) {
				c = clients + i;
				break;
			}
		}
		c->s = s;
		c->inp = c->inbuf;
		sock[num_sock++] = s;
		return;
	}

	c = 0;
	for(i=0; i<MAX_SOCK; i++) {
		if(s == clients[i].s) {
			c = clients + i;
			break;
		}
	}
	if(!c) {
		fprintf(stderr, "ctrl_process: unknown client socket (%d)\n", s);
		return;
	}

	while((len = read(s, buf, sizeof buf)) > 0) {
		ptr = c->inp;
		for(i=0; i<len; i++) {
			if(buf[i] == '\n') {
				*ptr = 0;
				proc_cmd(s, c->inbuf);
				ptr = c->inbuf;
			} else {
				if(ptr < c->inbuf + sizeof c->inbuf - 1) {
					*ptr++ = buf[i];
				}
			}
		}
		c->inp = ptr;
	}

	if(len == 0) {
		printf("ctrl_process: client disconnected (%d)\n", s);
		close(s);
		c->s = -1;
		num_sock--;
	}
}

static void send_status(int s, int status)
{
	write(s, status ? "OK!\n" : "ERR\n", 4);
}

static int proc_cmd_list(int s, int argc, char **argv);
static int proc_cmd_switch(int s, int argc, char **argv);

struct {
	const char *cmd;
	int (*proc)(int, int, char**);
} cmdfunc[] = {
	{"list", proc_cmd_list},
	{"switch", proc_cmd_switch},
	{0, 0}
};

static void proc_cmd(int s, char *cmdstr)
{
	int i, argc;
	char **argv;
	char *ptr;

	argc = 1;
	ptr = cmdstr;
	while(*ptr) {
		if(isspace(*ptr++)) argc++;
	}
	argv = alloca(argc * sizeof *argv);
	argc = 0;

	while((ptr = strtok(argc ? 0 : cmdstr, " \t\r\n\v"))) {
		argv[argc++] = ptr;
	}
	argv[argc] = 0;

	for(i=0; cmdfunc[i].proc; i++) {
		if(strcmp(argv[0], cmdfunc[i].cmd) == 0) {
			if(cmdfunc[i].proc(s, argc, argv) == -1) {
				send_status(s, 0);
				fprintf(stderr, "execution of command %s failed\n", argv[0]);
			}
			return;
		}
	}

	fprintf(stderr, "unrecognized command: %s\n", argv[0]);
}

static int proc_cmd_list(int s, int argc, char **argv)
{
	struct xlivebg_plugin *p;
	int i, len, num = get_plugin_count();
	char msg[1024];

	send_status(s, 1);
	len = sprintf(msg, "%d\n", num);
	write(s, msg, len);

	for(i=0; i<num; i++) {
		p = get_plugin(i);
		len = snprintf(msg, sizeof msg, "%s:%s\n", p->name, p->desc);
		write(s, msg, len);
	}
	return 0;
}

static int proc_cmd_switch(int s, int argc, char **argv)
{
	struct xlivebg_plugin *p;

	if(!argv[1]) {
		fprintf(stderr, "proc_cmd_switch: missing argument\n");
		return -1;
	}

	if(!(p = find_plugin(argv[1]))) {
		send_status(s, 0);
		fprintf(stderr, "no such plugin: \"%s\"\n", argv[1]);
		return -1;
	}
	send_status(s, 1);
	activate_plugin(p);
	return 0;
}
