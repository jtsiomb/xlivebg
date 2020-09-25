#ifndef OPENGL_H_
#define OPENGL_H_

#include <GL/gl.h>

#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM foo
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER foo
#endif

typedef void (*GLUSEPROGRAMFUNC)(unsigned int);
typedef void (*GLBINDBUFFERFUNC)(unsigned int, unsigned int);

extern GLUSEPROGRAMFUNC xlivebg_gl_use_program;
extern GLBINDBUFFERFUNC xlivebg_gl_bind_buffer;

int init_opengl(void);

void dump_texture(unsigned int tex, const char *fname);

#endif	/* OPENGL_H_ */
