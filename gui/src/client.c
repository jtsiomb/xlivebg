#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "client.h"

#define SOCK_PATH	"/tmp/xlivebg.sock"

static int read_line(int fd, char *line, int maxsz);

static int open_conn(void)
{
	int s;
	struct sockaddr_un addr;

	if((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("failed to create UNIX domain socket");
		return -1;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_PATH);

	if(connect(s, (struct sockaddr*)&addr, SUN_LEN(&addr)) == -1) {
		fprintf(stderr, "failed to connect to UNIX domain socket: " SOCK_PATH ": %s\n",
				strerror(errno));
		close(s);
		return -1;
	}

	return s;
}

int ping_xlivebg(void)
{
	int s;
	char buf[16];

	if((s = open_conn()) == -1) {
		return -1;
	}

	write(s, "ping\n", 5);
	if(read_line(s, buf, sizeof buf) == -1) {
		return -1;
	}
	close(s);
	return 0;
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

