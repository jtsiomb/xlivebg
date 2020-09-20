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
