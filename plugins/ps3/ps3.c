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
#define GL_GLEXT_PROTOTYPES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "xlivebg.h"
#include "ps3.h"
#include "props.h"
#include "noise.h"
#include "shaders.h"

static vertex *varr = NULL;
static unsigned int *iarr = NULL;
static long prev_upd;
static unsigned int w, l;
static unsigned int prog;
static unsigned int vao, vbo, ibo;
static unsigned int x_inc_l, z_inc_l, time_l, chaos_l, detail_l, speed_l, scale_l;
static unsigned int sec_chaos_l, sec_detail_l, sec_speed_l, sec_scale_l;
static unsigned int light_a_l, wave_color_l;

static float chaos = CHAOS_DEFAULT;
static float detail = DETAIL_DEFAULT;
static float speed = SPEED_DEFAULT;
static float scale = SCALE_DEFAULT;
static float light_a = LIGHT_ANGLE_DEFAULT;
static float wave_color[3] = WAVE_COLOR_DEFAULT_INIT;

extern const char wave_v, wave_f;


int init(void *cls) {
	xlivebg_defcfg_num("xlivebg.ps3.light_angle", LIGHT_ANGLE_DEFAULT);
	xlivebg_defcfg_num("xlivebg.ps3.chaos", CHAOS_DEFAULT);
	xlivebg_defcfg_num("xlivebg.ps3.detail", DETAIL_DEFAULT);
	xlivebg_defcfg_num("xlivebg.ps3.speed", SPEED_DEFAULT);
	xlivebg_defcfg_num("xlivebg.ps3.scale", SCALE_DEFAULT);
	xlivebg_defcfg_vec("xlivebg.ps3.wave_color", WAVE_COLOR_DEFAULT);
	w = 1250;
	l = 500;
	prog = vao = vbo = ibo = 0;

	varr = (vertex*)malloc(sizeof(vertex) * w * l);
	iarr = (unsigned int*)malloc(sizeof(unsigned int) * w * l * 4);
	unsigned int* ibase = iarr;
	for (unsigned int x = 0;x < w;x++) {
		for (unsigned int z = 0;z < l;z++) {
			vertex* v = &varr[x + (z * w)];
			v->pos.x = (((float)x / (float)w) - 0.5f) * 2.1f;
			v->pos.z = ((float)z / (float)l) - 0.5f;
			v->pos.y = 0.0f;

			if (x < (w - 1) && z < (l - 1)) {
				ibase[0] = (x + 0) + ((z + 0) * w);
				ibase[1] = (x + 0) + ((z + 1) * w);
				ibase[2] = (x + 1) + ((z + 1) * w);
				ibase[3] = (x + 1) + ((z + 0) * w);
				ibase += 4;
			}
		}
	}
	return 0;
}

void deinit(void* cls) {
}

void stop(void* cls) {
	if (vbo) glDeleteBuffers(1, &vbo);
	if (ibo) glDeleteBuffers(1, &ibo);
	if (vao) glDeleteVertexArrays(1, &vao);
	if (prog) glDeleteProgram(prog);
	if (varr) free(varr);
	if (iarr) free(iarr);
}

int start(long tmsec, void *cls) {
	prop("light_angle", 0);
	prop("chaos", 0);
	prop("detail", 0);
	prop("speed", 0);
	prop("scale", 0);
	prop("wave_color", 0);

	prev_upd = tmsec;

	printf("creating shaders\n");
	prog = create_sdrprog(&wave_v, &wave_f);

	if (!prog) return -1;
	x_inc_l = glGetUniformLocation(prog, "x_inc");
	z_inc_l = glGetUniformLocation(prog, "z_inc");
	time_l = glGetUniformLocation(prog, "time");
	chaos_l = glGetUniformLocation(prog, "chaos");
	detail_l = glGetUniformLocation(prog, "detail");
	speed_l = glGetUniformLocation(prog, "speed");
	scale_l = glGetUniformLocation(prog, "scale");
	sec_chaos_l = glGetUniformLocation(prog, "sec_chaos");
	sec_detail_l = glGetUniformLocation(prog, "sec_detail");
	sec_speed_l = glGetUniformLocation(prog, "sec_speed");
	sec_scale_l = glGetUniformLocation(prog, "sec_scale");
	light_a_l = glGetUniformLocation(prog, "light_a");
	wave_color_l = glGetUniformLocation(prog, "wave_color");

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * w * l, varr, GL_STATIC_DRAW);

	glGenBuffers(1, &ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 4 * (w - 1) * (l - 1), iarr, GL_STATIC_DRAW);

	// position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)0);
	glEnableVertexAttribArray(0);

	// normal
	// glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)sizeof(vec3));
	// glEnableVertexAttribArray(1);

	glBindVertexArray(0);

	return 0;
}

void prop(const char *name, void *cls) {
	if (strcmp(name, "light_angle") == 0) {
		light_a = xlivebg_getcfg_num("xlivebg.ps3.light_angle", LIGHT_ANGLE_DEFAULT);
	} else if (strcmp(name, "chaos") == 0) {
		chaos = xlivebg_getcfg_num("xlivebg.ps3.chaos", CHAOS_DEFAULT);
	} else if (strcmp(name, "detail") == 0) {
		detail = xlivebg_getcfg_num("xlivebg.ps3.detail", DETAIL_DEFAULT);
	} else if (strcmp(name, "speed") == 0) {
		speed = xlivebg_getcfg_num("xlivebg.ps3.speed", SPEED_DEFAULT);
	} else if (strcmp(name, "scale") == 0) {
		scale = xlivebg_getcfg_num("xlivebg.ps3.scale", SCALE_DEFAULT);
	} else if (strcmp(name, "wave_color") == 0) {
		float* col = xlivebg_getcfg_vec("xlivebg.ps3.wave_color", WAVE_COLOR_DEFAULT);
		wave_color[0] = col[0];
		wave_color[1] = col[1];
		wave_color[2] = col[2];
	}
}

void draw(long tmsec, void *cls) {
	float proj[16];
	prev_upd = tmsec;

	xlivebg_clear(GL_COLOR_BUFFER_BIT);

	int num_scr = xlivebg_screen_count();
	for (int i = 0;i < num_scr;i++) {
		struct xlivebg_screen* scr = xlivebg_screen(i);
		xlivebg_gl_viewport(i);
		draw_wave(tmsec);
	}
}

void draw_wave(long tmsec) {
	if (!prog || !vao || !vbo || !ibo) return;

	// todo: move this to vertex shader / calculate normals
	float tsec = (float)tmsec / 1000.0f;

	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(prog);
	glUniform1f(x_inc_l, (1.0f / (float)w) * 2.1f);
	glUniform1f(z_inc_l, (1.0f / (float)l));
	glUniform1f(time_l, (tsec * 0.1f) + 50.0f);
	glUniform1f(chaos_l, chaos * 0.3f);
	glUniform1f(detail_l, detail * 0.1f);
	glUniform1f(speed_l, speed * 0.1f);
	glUniform1f(scale_l, scale * 50.0f);
	glUniform1f(sec_chaos_l, SECONDARY_CHAOS_DEFAULT * 0.3f);
	glUniform1f(sec_detail_l, SECONDARY_DETAIL_DEFAULT * 0.1f);
	glUniform1f(sec_speed_l, SECONDARY_SPEED_DEFAULT * 0.1f);
	glUniform1f(sec_scale_l, SECONDARY_SCALE_DEFAULT * 50.0f);
	glUniform1f(light_a_l, light_a * 0.0174532925f);
	glUniform3fv(wave_color_l, 1, wave_color);

	glBindVertexArray(vao);
	
	glDrawElements(GL_QUADS, 4 * (w - 1) * (l - 1), GL_UNSIGNED_INT, 0);

	glBindVertexArray(0);
	glUseProgram(0);


	glPopAttrib();
}

void perspective(float *m, float vfov, float aspect, float znear, float zfar) {
	float s = 1.0f / (float)tan(vfov / 2.0f);
	float range = znear - zfar;

	memset(m, 0, 16 * sizeof *m);
	m[0] = s / aspect;
	m[5] = s;
	m[10] = (znear + zfar) / range;
	m[14] = 2.0f * znear * zfar / range;
	m[11] = -1.0f;
}

