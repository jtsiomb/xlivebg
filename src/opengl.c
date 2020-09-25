#include <stdio.h>
#include <stdlib.h>
#include "opengl.h"
#include <GL/glx.h>

#define GETGLFUNC(name) glXGetProcAddress((unsigned char*)name)

GLUSEPROGRAMFUNC xlivebg_gl_use_program;
GLBINDBUFFERFUNC xlivebg_gl_bind_buffer;

int init_opengl(void)
{
	if(!(xlivebg_gl_use_program = (GLUSEPROGRAMFUNC)GETGLFUNC("glUseProgram"))) {
		xlivebg_gl_use_program = (GLUSEPROGRAMFUNC)GETGLFUNC("glUseProgramObjectARB");
	}
	if(!(xlivebg_gl_bind_buffer = (GLBINDBUFFERFUNC)GETGLFUNC("glBindBuffer"))) {
		xlivebg_gl_bind_buffer = (GLBINDBUFFERFUNC)GETGLFUNC("glBindBufferARB");
	}
	return 0;
}

void dump_texture(unsigned int tex, const char *fname)
{
	FILE *fp;
	int i, width, height;
	unsigned char *pixels;

	glBindTexture(GL_TEXTURE_2D, tex);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

	if(!(pixels = malloc(width * height * 3))) {
		return;
	}
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

	if(!(fp = fopen(fname, "wb"))) {
		free(pixels);
		return;
	}
	fprintf(fp, "P6\n%d %d\n255\n", width, height);
	for(i=0; i<width * height * 3; i++) {
		fputc(pixels[i], fp);
	}
	fclose(fp);
	free(pixels);
}
