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
#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include "vid.h"

struct video_file {
	AVFormatContext *avctx;
	AVCodecContext *cctx;
	AVCodec *codec;
	int vstream, audio_stream;
	struct SwsContext *sws;

	AVFrame *frm, *rgbfrm;

	long frameno;
};

struct video_file *vid_open(const char *fname)
{
	static int initialized;
	struct video_file *vf;
	int i;

	if(!initialized) {
		av_register_all();
		initialized = 1;
	}

	if(!(vf = malloc(sizeof *vf))) {
		fprintf(stderr, "open_video(%s): failed to allocate memory: %s\n", fname, strerror(errno));
		return 0;
	}
	memset(vf, 0, sizeof *vf);
	vf->frameno = -1;

	if(avformat_open_input(&vf->avctx, fname, 0, 0) != 0) {
		fprintf(stderr, "open_video(%s): failed to open file\n", fname);
		vid_close(vf);
		return 0;
	}

	if(avformat_find_stream_info(vf->avctx, 0) < 0) {
		fprintf(stderr, "open_video(%s): failed to find stream info\n", fname);
		vid_close(vf);
		return 0;
	}

	vf->vstream = -1;
	for(i=0; i<vf->avctx->nb_streams; i++) {
		if(vf->avctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			vf->vstream = i;
			break;
		}
	}
	if(vf->vstream == -1) {
		fprintf(stderr, "open_video(%s): didn't find a video stream\n", fname);
		vid_close(vf);
		return 0;
	}
	vf->cctx = vf->avctx->streams[vf->vstream]->codec;

	if(!(vf->codec = avcodec_find_decoder(vf->cctx->codec_id))) {
		fprintf(stderr, "open_video(%s): unsupported codec\n", fname);
		vid_close(vf);
		return 0;
	}

	if(avcodec_open2(vf->cctx, vf->codec, 0) < 0) {
		fprintf(stderr, "open_video(%s): failed to open codec\n", fname);
		vid_close(vf);
		return 0;
	}

	if(!(vf->frm = av_frame_alloc()) || !(vf->rgbfrm = av_frame_alloc())) {
		fprintf(stderr, "open_video(%s): failed to allocate frame\n", fname);
		vid_close(vf);
		return 0;
	}

	vf->sws = sws_getContext(vf->cctx->width, vf->cctx->height, vf->cctx->pix_fmt,
			vf->cctx->width, vf->cctx->height, AV_PIX_FMT_BGR32, SWS_POINT, 0, 0, 0);
	if(!vf->sws) {
		fprintf(stderr, "open_video(%s): failed to allocate sws context\n", fname);
		vid_close(vf);
		return 0;
	}


	printf("using codec: %s\n", vf->codec->name);
	printf("fps: %f (%u usec frame interval)\n", vid_fps(vf), vid_frame_interval(vf));
	printf("size: %dx%d\n", vid_frame_width(vf), vid_frame_height(vf));

	return vf;
}

void vid_close(struct video_file *vf)
{
	if(!vf) return;

	/* TODO how do we deallocate sws contexts? */
	if(vf->rgbfrm) av_free(vf->rgbfrm);
	if(vf->frm) av_free(vf->frm);
	if(vf->cctx) avcodec_close(vf->cctx);
	if(vf->avctx) avformat_close_input(&vf->avctx);
	free(vf);
}


float vid_fps(struct video_file *vf)
{
	float inv_tb = (float)vf->cctx->time_base.den / (float)vf->cctx->time_base.num;
	return inv_tb / (float)vf->cctx->ticks_per_frame;
}

unsigned int vid_frame_interval(struct video_file *vf)
{
	float fps = vid_fps(vf);
	return 1000000 / fps;
}

int vid_frame_width(struct video_file *vf)
{
	return vf->cctx->width;
}

int vid_frame_height(struct video_file *vf)
{
	return vf->cctx->height;
}

size_t vid_frame_size(struct video_file *vf)
{
	return vf->cctx->width * vf->cctx->height * 4;
}

int vid_get_frame(struct video_file *vf, void *img)
{
	AVPacket packet;
	int res, frame_done = 0;
	int num_retries = 0;

	av_image_fill_arrays(vf->rgbfrm->data, vf->rgbfrm->linesize, (unsigned char*)img,
			AV_PIX_FMT_BGR32, vf->cctx->width, vf->cctx->height, 1);

retry:
	while((res = av_read_frame(vf->avctx, &packet)) >= 0) {
		if(packet.stream_index == vf->vstream) {
			avcodec_decode_video2(vf->cctx, vf->frm, &frame_done, &packet);

			if(frame_done) {
				sws_scale(vf->sws, (const uint8_t**)vf->frm->data, vf->frm->linesize,
						0, vf->cctx->height, vf->rgbfrm->data, vf->rgbfrm->linesize);
				av_packet_unref(&packet);
				vf->frameno++;
				return 0;
			}
		}
		av_packet_unref(&packet);
	}

	if(res == AVERROR_EOF && num_retries == 0) {
		int64_t start_time = vf->avctx->start_time;
		avformat_seek_file(vf->avctx, vf->vstream, INT_MIN, start_time, start_time, 0);
		avcodec_flush_buffers(vf->cctx);
		vf->frameno = -1;
		num_retries++;
		goto retry;
	}

	return -1;
}

long vid_frameno(struct video_file *vf)
{
	return vf->frameno;
}
