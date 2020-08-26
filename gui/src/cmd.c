#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "cmd.h"

#define SOCK_PATH	"/tmp/xlivebg.sock"

static int read_line(int fd, char *line, int maxsz);
static int read_status(int fd);
static int num_resp_lines(int s);
static int read_multi_lines(int s, char *buf, int maxsz);

static char cmdbuf[512];


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

int cmd_ping(void)
{
	int s;

	if((s = open_conn()) == -1) {
		return -1;
	}

	write(s, "ping\n", 5);
	if(read_status(s) == -1) {
		close(s);
		return -1;
	}
	close(s);
	return 0;
}

int cmd_list(char *buf, int maxsz)
{
	int s, res;

	if((s = open_conn()) == -1) {
		return -1;
	}
	write(s, "list\n", 5);

	if(read_status(s) <= 0) {
		close(s);
		return -1;
	}
	res = read_multi_lines(s, buf, maxsz);
	close(s);
	return res;
}

int cmd_getprop_str(const char *name, char *buf, int maxsz)
{
	int s, len, res;

	if((s = open_conn()) == -1) {
		return -1;
	}

	len = sprintf(cmdbuf, "getpropstr %s\n", name);
	write(s, cmdbuf, len);
	if(read_status(s) <= 0) {
		close(s);
		return -1;
	}

	res = read_multi_lines(s, buf, maxsz);
	close(s);
	return res;
}

int cmd_getprop_int(const char *name, int *ret)
{
	int s, len;

	if((s = open_conn()) == -1) {
		return -1;
	}

	len = sprintf(cmdbuf, "getpropint %s\n", name);
	write(s, cmdbuf, len);
	if(read_status(s) <= 0) {
		close(s);
		return -1;
	}
	if(num_resp_lines(s) <= 0 || read_line(s, cmdbuf, sizeof cmdbuf) == -1) {
		close(s);
		return -1;
	}
	close(s);

	*ret = atoi(cmdbuf);
	return 0;
}

int cmd_getprop_num(const char *name, float *ret)
{
	int s, len;

	if((s = open_conn()) == -1) {
		return -1;
	}

	len = sprintf(cmdbuf, "getpropnum %s\n", name);
	write(s, cmdbuf, len);
	if(read_status(s) <= 0) {
		close(s);
		return -1;
	}
	if(num_resp_lines(s) <= 0 || read_line(s, cmdbuf, sizeof cmdbuf) == -1) {
		close(s);
		return -1;
	}
	close(s);

	*ret = atof(cmdbuf);
	return 0;
}

int cmd_getprop_vec(const char *name, float *ret)
{
	int i, s, len;

	if((s = open_conn()) == -1) {
		return -1;
	}

	len = sprintf(cmdbuf, "getpropvec %s\n", name);
	write(s, cmdbuf, len);
	if(read_status(s) <= 0) {
		close(s);
		return -1;
	}
	if(num_resp_lines(s) <= 0 || read_line(s, cmdbuf, sizeof cmdbuf) == -1) {
		close(s);
		return -1;
	}
	close(s);

	len = sscanf(cmdbuf, "[%f, %f, %f, %f]", ret, ret + 1, ret + 2, ret + 3);
	for(i=len; i<4; i++) {
		ret[i] = 0;
	}
	return 0;
}

int cmd_setprop_str(const char *name, const char *val)
{
	int s, len, res;

	if((s = open_conn()) == -1) {
		return -1;
	}
	len = sprintf(cmdbuf, "propstr %s %s\n", name, val);
	write(s, cmdbuf, len);
	res = read_status(s);
	close(s);
	return res;
}

int cmd_setprop_int(const char *name, int val)
{
	int s, len, res;

	if((s = open_conn()) == -1) {
		return -1;
	}
	len = sprintf(cmdbuf, "propint %s %d\n", name, val);
	write(s, cmdbuf, len);
	res = read_status(s);
	close(s);
	return res;
}

int cmd_setprop_num(const char *name, float val)
{
	int s, len, res;

	if((s = open_conn()) == -1) {
		return -1;
	}
	len = sprintf(cmdbuf, "propnum %s %g\n", name, val);
	write(s, cmdbuf, len);
	res = read_status(s);
	close(s);
	return res;
}

int cmd_setprop_vec(const char *name, float *val)
{
	int s, len, res;

	if((s = open_conn()) == -1) {
		return -1;
	}
	len = sprintf(cmdbuf, "propvec %s [%g, %g, %g, %g]\n", name, val[0], val[1], val[2], val[3]);
	write(s, cmdbuf, len);
	res = read_status(s);
	close(s);
	return res;
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

static int read_status(int fd)
{
	char buf[8];
	if(read_line(fd, buf, sizeof buf) == -1) {
		return -1;
	}
	if(strcmp(buf, "OK!\n") != 0) {
		return 0;
	}
	return 1;
}

static int num_resp_lines(int s)
{
	int count;
	if(read_line(s, cmdbuf, sizeof cmdbuf) == -1) {
		return 0;
	}
	count = atoi(cmdbuf);
	return count < 0 ? 0 : count;
}

/* returns the full size of the response, even if maxsz was less than that, and
 * the result was truncated to fit in buf.
 */
static int read_multi_lines(int s, char *buf, int maxsz)
{
	int i, count, str_size = 0;
	char *dptr, *sptr;

	count = num_resp_lines(s);

	dptr = buf;
	for(i=0; i<count; i++) {
		if(read_line(s, cmdbuf, sizeof cmdbuf) == -1) {
			return -1;
		}
		sptr = cmdbuf;
		while(*sptr) {
			if(maxsz-- > 0) {
				*dptr++ = *sptr++;
			}
			str_size++;
		}
	}
	*dptr = 0;

	return str_size;
}

