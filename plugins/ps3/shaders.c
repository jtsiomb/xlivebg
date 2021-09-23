#define GL_GLEXT_PROTOTYPES
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>

unsigned int create_shader(const char *src, unsigned int type) {
	unsigned int sdr;
	int status, log_len;

	printf("compiling %s shader ... ", type == GL_VERTEX_SHADER ? "vertex" : "pixel");
	fflush(stdout);

	if(!(sdr = glCreateShader(type))) {
		fprintf(stderr, "failed to create shader\n");
		return 0;
	}
	glShaderSource(sdr, 1, &src, 0);
	glCompileShader(sdr);

	glGetShaderiv(sdr, GL_COMPILE_STATUS, &status);
	glGetShaderiv(sdr, GL_INFO_LOG_LENGTH, &log_len);

	printf("%s\n", status ? "done" : "failed");
	if(log_len) {
		char *buf = (char*)alloca(log_len + 1);
		glGetShaderInfoLog(sdr, log_len + 1, 0, buf);
		printf("%s\n", buf);
	}

	if(!status) {
		glDeleteShader(sdr);
		return 0;
	}
	return sdr;
}

unsigned int create_sdrprog(const char *vsrc, const char *psrc) {
	int status, log_len;
	unsigned int prog;
	unsigned int vs, ps;

	if(!(vs = create_shader(vsrc, GL_VERTEX_SHADER))) {
		return 0;
	}
	if(!(ps = create_shader(psrc, GL_FRAGMENT_SHADER))) {
		glDeleteShader(vs);
		return 0;
	}

	printf("linking shader program ... ");
	fflush(stdout);

	if(!(prog = glCreateProgram())) {
		glDeleteShader(vs);
		glDeleteShader(ps);
		fprintf(stderr, "failed to create shader program\n");
		return 0;
	}
	glAttachShader(prog, vs);
	glAttachShader(prog, ps);
	glLinkProgram(prog);

	glGetProgramiv(prog, GL_LINK_STATUS, &status);
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_len);

	puts(status ? "done" : "failed");
	if(log_len) {
		char *buf = (char*)alloca(log_len + 1);
		glGetProgramInfoLog(prog, log_len + 1, 0, buf);
		puts(buf);
	}

	if(!status) {
		glDeleteProgram(prog);
		return 0;
	}
	return prog;
}
