#include "imago2.h"

/* to avoid dependency to OpenGL, I'll define all the relevant GL macros manually */
#define GL_UNSIGNED_BYTE		0x1401
#define GL_FLOAT				0x1406

#define GL_LUMINANCE			0x1909
#define GL_RGB					0x1907
#define GL_RGBA					0x1908

#define GL_RGBA32F				0x8814
#define GL_RGB32F				0x8815
#define GL_LUMINANCE32F			0x8818

#define GL_TEXTURE_2D			0x0de1
#define GL_TEXTURE_WRAP_S		0x2802
#define GL_TEXTURE_WRAP_T		0x2803
#define GL_TEXTURE_MAG_FILTER	0x2800
#define GL_TEXTURE_MIN_FILTER	0x2801
#define GL_LINEAR				0x2601
#define GL_LINEAR_MIPMAP_LINEAR	0x2703
#define GL_REPEAT				0x2901
#define GL_GENERATE_MIPMAP_SGIS	0x8191


typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef void GLvoid;

/* for the same reason I'll load GL functions dynamically */
#ifndef WIN32
typedef void (*gl_gen_textures_func)(GLsizei, GLuint*);
typedef void (*gl_bind_texture_func)(GLenum, GLuint);
typedef void (*gl_tex_parameteri_func)(GLenum, GLenum, GLint);
typedef void (*gl_tex_image2d_func)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid*);
typedef void (*gl_generate_mipmap_func)(GLenum);
typedef GLenum (*gl_get_error_func)(void);
#else
typedef void (__stdcall *gl_gen_textures_func)(GLsizei, GLuint*);
typedef void (__stdcall *gl_bind_texture_func)(GLenum, GLuint);
typedef void (__stdcall *gl_tex_parameteri_func)(GLenum, GLenum, GLint);
typedef void (__stdcall *gl_tex_image2d_func)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid*);
typedef void (__stdcall *gl_generate_mipmap_func)(GLenum);
typedef GLenum (__stdcall *gl_get_error_func)(void);
#endif

static gl_gen_textures_func gl_gen_textures;
static gl_bind_texture_func gl_bind_texture;
static gl_tex_parameteri_func gl_tex_parameteri;
static gl_tex_image2d_func gl_tex_image2d;
static gl_generate_mipmap_func gl_generate_mipmap;
static gl_get_error_func gl_get_error;

static int load_glfunc(void);

unsigned int img_fmt_glfmt(enum img_fmt fmt)
{
	switch(fmt) {
	case IMG_FMT_GREY8:
	case IMG_FMT_GREYF:
		return GL_LUMINANCE;

	case IMG_FMT_RGB24:
	case IMG_FMT_RGBF:
		return GL_RGB;

	case IMG_FMT_RGBA32:
	case IMG_FMT_RGBAF:
		return GL_RGBA;

	default:
		break;
	}
	return 0;
}

unsigned int img_fmt_gltype(enum img_fmt fmt)
{
	switch(fmt) {
	case IMG_FMT_GREY8:
	case IMG_FMT_RGB24:
	case IMG_FMT_RGBA32:
		return GL_UNSIGNED_BYTE;

	case IMG_FMT_GREYF:
	case IMG_FMT_RGBF:
	case IMG_FMT_RGBAF:
		return GL_FLOAT;

	default:
		break;
	}
	return 0;
}

unsigned int img_fmt_glintfmt(enum img_fmt fmt)
{
	switch(fmt) {
	case IMG_FMT_GREY8:
		return GL_LUMINANCE;
	case IMG_FMT_RGB24:
		return GL_RGB;
	case IMG_FMT_RGBA32:
		return GL_RGBA;
	case IMG_FMT_GREYF:
		return GL_LUMINANCE32F;
	case IMG_FMT_RGBF:
		return GL_RGB32F;
	case IMG_FMT_RGBAF:
		return GL_RGBA32F;
	default:
		break;
	}
	return 0;
}

unsigned int img_glfmt(struct img_pixmap *img)
{
	return img_fmt_glfmt(img->fmt);
}

unsigned int img_gltype(struct img_pixmap *img)
{
	return img_fmt_gltype(img->fmt);
}

unsigned int img_glintfmt(struct img_pixmap *img)
{
	return img_fmt_glintfmt(img->fmt);
}

unsigned int img_gltexture(struct img_pixmap *img)
{
	unsigned int tex;
	unsigned int intfmt, fmt, type;

	if(!gl_gen_textures) {
		if(load_glfunc() == -1) {
			fprintf(stderr, "imago: failed to initialize the OpenGL helpers\n");
			return 0;
		}
	}

	intfmt = img_glintfmt(img);
	fmt = img_glfmt(img);
	type = img_gltype(img);

	gl_gen_textures(1, &tex);
	gl_bind_texture(GL_TEXTURE_2D, tex);
	gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	if(!gl_generate_mipmap) {
		gl_tex_parameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, 1);
		gl_get_error();	/* clear errors in case SGIS_generate_mipmap is not supported */
	}
	gl_tex_image2d(GL_TEXTURE_2D, 0, intfmt, img->width, img->height, 0, fmt, type, img->pixels);
	if(gl_generate_mipmap) {
		gl_generate_mipmap(GL_TEXTURE_2D);
	}
	return tex;
}

unsigned int img_gltexture_load(const char *fname)
{
	struct img_pixmap img;
	unsigned int tex;

	img_init(&img);
	if(img_load(&img, fname) == -1) {
		img_destroy(&img);
		return 0;
	}

	tex = img_gltexture(&img);
	img_destroy(&img);
	return tex;
}

unsigned int img_gltexture_read_file(FILE *fp)
{
	struct img_pixmap img;
	unsigned int tex;

	img_init(&img);
	if(img_read_file(&img, fp) == -1) {
		img_destroy(&img);
		return 0;
	}

	tex = img_gltexture(&img);
	img_destroy(&img);
	return tex;
}

unsigned int img_gltexture_read(struct img_io *io)
{
	struct img_pixmap img;
	unsigned int tex;

	img_init(&img);
	if(img_read(&img, io) == -1) {
		img_destroy(&img);
		return 0;
	}

	tex = img_gltexture(&img);
	img_destroy(&img);
	return tex;
}

#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>

#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT	((void*)0)
#endif

#endif
#ifdef WIN32
#include <windows.h>
#endif

static int load_glfunc(void)
{
#if defined(__unix__) || defined(__APPLE__)
	gl_gen_textures = (gl_gen_textures_func)dlsym(RTLD_DEFAULT, "glGenTextures");
	gl_bind_texture = (gl_bind_texture_func)dlsym(RTLD_DEFAULT, "glBindTexture");
	gl_tex_parameteri = (gl_tex_parameteri_func)dlsym(RTLD_DEFAULT, "glTexParameteri");
	gl_tex_image2d = (gl_tex_image2d_func)dlsym(RTLD_DEFAULT, "glTexImage2D");
	gl_generate_mipmap = (gl_generate_mipmap_func)dlsym(RTLD_DEFAULT, "glGenerateMipmap");
	gl_get_error = (gl_get_error_func)dlsym(RTLD_DEFAULT, "glGetError");
#endif

#ifdef WIN32
	HANDLE dll = LoadLibrary("opengl32.dll");
	if(dll) {
		gl_gen_textures = (gl_gen_textures_func)GetProcAddress(dll, "glGenTextures");
		gl_bind_texture = (gl_bind_texture_func)GetProcAddress(dll, "glBindTexture");
		gl_tex_parameteri = (gl_tex_parameteri_func)GetProcAddress(dll, "glTexParameteri");
		gl_tex_image2d = (gl_tex_image2d_func)GetProcAddress(dll, "glTexImage2D");
		gl_generate_mipmap = (gl_generate_mipmap_func)GetProcAddress(dll, "glGenerateMipmap");
		gl_get_error = (gl_get_error_func)GetProcAddress(dll, "glGetError");
	}
#endif

	return (gl_gen_textures && gl_bind_texture && gl_tex_parameteri && gl_tex_image2d && gl_get_error) ? 0 : -1;
}
