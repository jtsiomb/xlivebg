#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "plugin.h"

#define SOCK_PATH	"/tmp/xlivebg.sock"

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

static void proc_cmd(char *cmdstr);

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
				proc_cmd(c->inbuf);
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

static void proc_cmd(char *cmdstr)
{
	/* XXX temp hack just to see if this works */
	struct xlivebg_plugin *p;

	if(!(p = find_plugin(cmdstr))) {
		fprintf(stderr, "no such plugin: \"%s\"\n", cmdstr);
		return;
	}
	activate_plugin(p);
}
