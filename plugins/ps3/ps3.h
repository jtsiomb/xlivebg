/*
xlivebg - live wallpapers for the X window system
Copyright (C) 2019-2021  John Tsiombikas <nuclear@member.fsf.org>

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
/* Plugin author: mdecicco */
typedef struct {
	float x, y, z;
} vec3;

typedef struct {
	vec3 pos;
	//vec3 norm;
} vertex;

int init(void *cls);
void deinit(void *cls);
int start(long tmsec, void *cls);
void stop(void *cls);
void prop(const char *name, void *cls);
void draw(long tmsec, void *cls);
void draw_wave(long tmsec);
void perspective(float *m, float vfov, float aspect, float znear, float zfar);
