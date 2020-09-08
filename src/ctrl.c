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
#include <alloca.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ctrl.h"
#include "plugin.h"
#include "cfg.h"

struct client {
	int s;
	char inbuf[4096];
	char *inp;
};

#define MAX_CLIENTS	16
static struct client clients[MAX_CLIENTS];
static int lis = -1;

static void proc_cmd(int s, char *cmdstr);

int ctrl_init(void)
{
	int i;
	struct sockaddr_un addr;

	for(i=0; i<MAX_CLIENTS; i++) {
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
	static int sock[MAX_CLIENTS];
	int i, n = 0;

	if(lis == -1) {
		*count = 0;
		return 0;
	}

	sock[n++] = lis;
	for(i=0; i<MAX_CLIENTS; i++) {
		if(clients[i].s >= 0) {
			sock[n++] = clients[i].s;
		}
	}
	*count = n;
	return sock;
}

void ctrl_process(int s)
{
	int i, len;
	struct client *c;
	char buf[1024];
	char *ptr;

	if(s == lis) {
		c = 0;
		for(i=0; i<MAX_CLIENTS; i++) {
			if(clients[i].s == -1) {
				c = clients + i;
				break;
			}
		}
		if(!c) {
			fprintf(stderr, "ctrl_process: too many incoming connections\n");
			return;
		}

		if((s = accept(lis, 0, 0)) == -1) {
			perror("ctrl_process: failed to accept incoming connection");
			return;
		}
		fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);

		c->s = s;
		c->inp = c->inbuf;
		return;
	}

	c = 0;
	for(i=0; i<MAX_CLIENTS; i++) {
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
		close(s);
		c->s = -1;
	}
}

static void send_status(int s, int status)
{
	write(s, status ? "OK!\n" : "ERR\n", 4);
}

static int proc_cmd_list(int s, int argc, char **argv);
static int proc_cmd_switch(int s, int argc, char **argv);
static int proc_cmd_proplist(int s, int argc, char **argv);
static int proc_cmd_setprop(int s, int argc, char **argv);
static int proc_cmd_getprop(int s, int argc, char **argv);
static int proc_cmd_save(int s, int argc, char **argv);
static int proc_cmd_cfgpath(int s, int argc, char **argv);
static int proc_cmd_ping(int s, int argc, char **argv);

struct {
	const char *cmd;
	int (*proc)(int, int, char**);
} cmdfunc[] = {
	{"list", proc_cmd_list},
	{"switch", proc_cmd_switch},
	{"lsprop", proc_cmd_proplist},
	{"propstr", proc_cmd_setprop},
	{"propnum", proc_cmd_setprop},
	{"propint", proc_cmd_setprop},
	{"propvec", proc_cmd_setprop},
	{"getpropstr", proc_cmd_getprop},
	{"getpropnum", proc_cmd_getprop},
	{"getpropint", proc_cmd_getprop},
	{"getpropvec", proc_cmd_getprop},
	{"save", proc_cmd_save},
	{"cfgpath", proc_cmd_cfgpath},
	{"ping", proc_cmd_ping},
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
		fprintf(stderr, "proc_cmd_switch: no such plugin: \"%s\"\n", argv[1]);
		return -1;
	}
	printf("CTRL: switch plugin: %s\n", p->name);
	send_status(s, 1);
	activate_plugin(p);
	return 0;
}

static int proc_cmd_proplist(int s, int argc, char **argv)
{
	int len, num_lines;
	struct xlivebg_plugin *p;
	char buf[32];
	char *ptr;

	if(argv[1]) {
		if(!(p = find_plugin(argv[1]))) {
			send_status(s, 0);
			fprintf(stderr, "proc_cmd_proplist: no such plugin: \"%s\"\n", argv[1]);
			return -1;
		}
	} else {
		if(!(p = get_active_plugin())) {
			send_status(s, 0);
			fprintf(stderr, "proc_cmd_proplist: no active plugin, and none specified\n");
			return -1;
		}
	}

	if(!p->props) {
		send_status(s, 0);
		return 0;
	}

	send_status(s, 1);

	num_lines = 0;
	ptr = p->props;
	while(*ptr) {
		if(*ptr++ == '\n') num_lines++;
	}

	len = sprintf(buf, "%d\n", num_lines);
	write(s, buf, len);

	len = strlen(p->props);
	write(s, p->props, len);
	return 0;
}

static int propcmd_type(const char *argv0)
{
	if(strcmp(argv0 + 4, "str") == 0) {
		return XLIVEBG_PROP_STRING;
	}
	if(strcmp(argv0 + 4, "num") == 0) {
		return XLIVEBG_PROP_NUMBER;
	}
	if(strcmp(argv0 + 4, "int") == 0) {
		return XLIVEBG_PROP_INTEGER;
	}
	if(strcmp(argv0 + 4, "vec") == 0) {
		return XLIVEBG_PROP_VECTOR;
	}
	return XLIVEBG_PROP_UNKNOWN;
}

static int proc_cmd_setprop(int s, int argc, char **argv)
{
	int type, count;
	int ival;
	float fval;
	char *endp;
	float vval[4] = {0, 0, 0, 1};

	if(argc < 3) {
		send_status(s, 0);
		fprintf(stderr, "proc_cmd_setprop: not enough arguments\n");
		return -1;
	}

	printf("CTRL: setprop %s %s\n", argv[1], argv[2]);

	type = propcmd_type(argv[0]);
	switch(type) {
	case XLIVEBG_PROP_STRING:
		if(xlivebg_setcfg_str(argv[1], argv[2]) == -1) {
			goto setfailed;
		}
		break;
	case XLIVEBG_PROP_NUMBER:
		fval = strtod(argv[2], &endp);
		if(endp == argv[2] || xlivebg_setcfg_num(argv[1], fval) == -1) {
			goto setfailed;
		}
		break;
	case XLIVEBG_PROP_INTEGER:
		ival = strtol(argv[2], &endp, 10);
		if(endp == argv[2] || xlivebg_setcfg_int(argv[1], ival) == -1) {
			goto setfailed;
		}
		break;
	case XLIVEBG_PROP_VECTOR:
		if(!(count = sscanf(argv[2], "[%f, %f, %f, %f]", vval, vval + 1, vval + 2, vval + 3))) {
			goto setfailed;
		}
		if(xlivebg_setcfg_vec(argv[2], vval) == -1) {
			goto setfailed;
		}
		break;
	default:
		send_status(s, 0);
		fprintf(stderr, "proc_cmd_setprop: invalid type\n");
		return -1;
	}

	send_status(s, 1);
	return 0;

setfailed:
	send_status(s, 0);
	fprintf(stderr, "proc_cmd_setprop: failed to set property %s <- %s\n", argv[1], argv[2]);
	return -1;
}

static int proc_cmd_getprop(int s, int argc, char **argv)
{
	int type, ival, len, count;
	float fval, *vval;
	char buf[256];
	const char *str, *ptr;

	if(argc < 2) {
		send_status(s, 0);
		fprintf(stderr, "proc_cmd_getprop: not enough arguments\n");
		return -1;
	}

	type = propcmd_type(argv[0] + 3);
	switch(type) {
	case XLIVEBG_PROP_STRING:
		if(!(str = xlivebg_getcfg_str(argv[1], 0))) {
			send_status(s, 0);
			fprintf(stderr, "proc_cmd_getprop: failed to get property %s (string)\n", argv[1]);
			return -1;
		}
		send_status(s, 1);
		count = 0;
		if(*str) {
			ptr = str;
			while(*ptr) if(*ptr++ == '\n') count++;
			if(ptr[-1] != '\n') count++;
		}
		len = sprintf(buf, "%d\n", count);
		write(s, buf, len);
		write(s, str, strlen(str));
		write(s, "\n", 1);
		break;

	case XLIVEBG_PROP_NUMBER:
		fval = xlivebg_getcfg_num(argv[1], 0.0f);
		send_status(s, 1);
		len = sprintf(buf, "1\n%g\n", fval);
		write(s, buf, len);
		break;

	case XLIVEBG_PROP_INTEGER:
		ival = xlivebg_getcfg_int(argv[1], 0);
		send_status(s, 1);
		len = sprintf(buf, "1\n%d\n", ival);
		write(s, buf, len);
		break;

	case XLIVEBG_PROP_VECTOR:
		if(!(vval = xlivebg_getcfg_vec(argv[1], 0))) {
			send_status(s, 0);
			fprintf(stderr, "proc_cmd_getprop: failed to get property %s (vector)\n", argv[1]);
			return -1;
		}
		send_status(s, 1);
		len = sprintf(buf, "1\n[%g %g %g %g]\n", vval[0], vval[1], vval[2], vval[3]);
		write(s, buf, len);
		break;

	default:
		send_status(s, 0);
		fprintf(stderr, "proc_cmd_setprop: invalid type\n");
		return -1;
	}

	return 0;
}

static int proc_cmd_save(int s, int argc, char **argv)
{
	fprintf(stderr, "proc_cmd_save not implemented yet\n");
	send_status(s, 0);	/* TODO */
	return 0;
}

static int proc_cmd_cfgpath(int s, int argc, char **argv)
{
	char *buf;
	int len;

	if(!cfgpath || !*cfgpath) {
		send_status(s, 0);
		return 0;
	}

	send_status(s, 1);
	len = strlen(cfgpath) + 3;
	buf = alloca(len + 1);
	sprintf(buf, "1\n%s\n", cfgpath);
	write(s, buf, len);
}

static int proc_cmd_ping(int s, int argc, char **argv)
{
	send_status(s, 1);
	return 0;
}
