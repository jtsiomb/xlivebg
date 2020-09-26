/*
xlivebg - live wallpapers for the X window system
Copyright (C) 2019-2020  John Tsiombikas <nuclear@member.fsf.org>

Stereoplay - an OpenGL stereoscopic video player.
Copyright (C) 2011  John Tsiombikas <nuclear@member.fsf.org>

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
#ifndef VID_H_
#define VID_H_

#include <inttypes.h>

struct video_file;

#ifdef __cplusplus
extern "C" {
#endif

struct video_file *vid_open(const char *fname);
void vid_close(struct video_file *vf);

float vid_fps(struct video_file *vf);

/* returns the interval between frames in microseconds */
unsigned int vid_frame_interval(struct video_file *vf);

int vid_frame_width(struct video_file *vf);
int vid_frame_height(struct video_file *vf);
size_t vid_frame_size(struct video_file *vf);

int vid_get_frame(struct video_file *vf, uint32_t *img);

#ifdef __cplusplus
}
#endif

#endif	/* VID_H_ */
