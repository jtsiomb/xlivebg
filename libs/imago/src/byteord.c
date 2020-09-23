/*
libimago - a multi-format image file input/output library.
Copyright (C) 2010-2019 John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "byteord.h"

int16_t img_read_int16(struct img_io *io)
{
	int16_t v;
	io->read(&v, 2, io->uptr);
	return v;
}

int16_t img_read_int16_inv(struct img_io *io)
{
	int16_t v;
	io->read(&v, 2, io->uptr);
	return ((v >> 8) & 0xff) | (v << 8);
}

uint16_t img_read_uint16(struct img_io *io)
{
	uint16_t v;
	io->read(&v, 2, io->uptr);
	return v;
}

uint16_t img_read_uint16_inv(struct img_io *io)
{
	int16_t v;
	io->read(&v, 2, io->uptr);
	return (v >> 8) | (v << 8);
}


void img_write_int16(struct img_io *io, int16_t val)
{
	io->write(&val, 2, io->uptr);
}

void img_write_int16_inv(struct img_io *io, int16_t val)
{
	val = ((val >> 8) & 0xff) | (val << 8);
	io->write(&val, 2, io->uptr);
}

void img_write_uint16(struct img_io *io, uint16_t val)
{
	io->write(&val, 2, io->uptr);
}

void img_write_uint16_inv(struct img_io *io, uint16_t val)
{
	val = (val >> 8) | (val << 8);
	io->write(&val, 2, io->uptr);
}

uint32_t img_read_uint32(struct img_io *io)
{
	uint32_t v;
	io->read(&v, 4, io->uptr);
	return v;
}

uint32_t img_read_uint32_inv(struct img_io *io)
{
	uint32_t v;
	io->read(&v, 4, io->uptr);
	return (v << 24) | ((v & 0xff00) << 8) | ((v & 0xff0000) >> 8) | (v >> 24);
}
