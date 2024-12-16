/*

Copyright (c) 2021, Dominic Szablewski - https://phoboslab.org
SPDX-License-Identifier: MIT


QOI - The "Quite OK Image" format for fast, lossless image compression

-- About

QOI encodes and decodes images in a lossless format. Compared to stb_image and
stb_image_write QOI offers 20x-50x faster encoding, 3x-4x faster decoding and
20% better compression.


-- Synopsis

// Define `QOI_IMPLEMENTATION` in *one* C/C++ file before including this
// library to create the implementation.

#define QOI_IMPLEMENTATION
#include "qoi.h"

// Encode and store an RGBA buffer to the file system. The qoi_desc describes
// the input pixel data.
qoi_write("image_new.qoi", rgba_pixels, &(qoi_desc){
	.width = 1920,
	.height = 1080,
	.channels = 4,
	.colorspace = QOI_SRGB
});

// Load and decode a QOI image from the file system into a 32bbp RGBA buffer.
// The qoi_desc struct will be filled with the width, height, number of channels
// and colorspace read from the file header.
qoi_desc desc;
void *rgba_pixels = qoi_read("image.qoi", &desc, 4);



-- Documentation

This library provides the following functions;
- qoi_read    -- read and decode a QOI file
- qoi_decode  -- decode the raw bytes of a QOI image from memory
- qoi_write   -- encode and write a QOI file
- qoi_encode  -- encode an rgba buffer into a QOI image in memory

See the function declaration below for the signature and more information.

If you don't want/need the qoi_read and qoi_write functions, you can define
QOI_NO_STDIO before including this library.

This library uses malloc() and free(). To supply your own malloc implementation
you can define QOI_MALLOC and QOI_FREE before including this library.

This library uses memset() to zero-initialize the index. To supply your own
implementation you can define QOI_ZEROARR before including this library.


-- Data Format

A QOI file has a 14 byte header, followed by any number of data "chunks" and an
8-byte end marker.

struct qoi_header_t {
	char     magic[4];   // magic bytes "qoif"
	uint32_t width;      // image width in pixels (BE)
	uint32_t height;     // image height in pixels (BE)
	uint8_t  channels;   // 3 = RGB, 4 = RGBA
	uint8_t  colorspace; // 0 = sRGB with linear alpha, 1 = all channels linear
};

Images are encoded row by row, left to right, top to bottom. The decoder and
encoder start with {r: 0, g: 0, b: 0, a: 255} as the previous pixel value. An
image is complete when all pixels specified by width * height have been covered.

Pixels are encoded as
 - a run of the previous pixel
 - an index into an array of previously seen pixels
 - a difference to the previous pixel value in r,g,b
 - full r,g,b or r,g,b,a values

The color channels are assumed to not be premultiplied with the alpha channel
("un-premultiplied alpha").

A running array[64] (zero-initialized) of previously seen pixel values is
maintained by the encoder and decoder. Each pixel that is seen by the encoder
and decoder is put into this array at the position formed by a hash function of
the color value. In the encoder, if the pixel value at the index matches the
current pixel, this index position is written to the stream as QOI_OP_INDEX.
The hash function for the index is:

	index_position = (r * 3 + g * 5 + b * 7 + a * 11) % 64

Each chunk starts with a 2- or 8-bit tag, followed by a number of data bits. The
bit length of chunks is divisible by 8 - i.e. all chunks are byte aligned. All
values encoded in these data bits have the most significant bit on the left.

The 8-bit tags have precedence over the 2-bit tags. A decoder must check for the
presence of an 8-bit tag first.

The byte stream's end is marked with 7 0x00 bytes followed a single 0x01 byte.


The possible chunks are:


.- QOI_OP_INDEX ----------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------+-----------------|
|  0  0 |     index       |
`-------------------------`
2-bit tag b00
6-bit index into the color index array: 0..63

A valid encoder must not issue 2 or more consecutive QOI_OP_INDEX chunks to the
same index. QOI_OP_RUN should be used instead.


.- QOI_OP_DIFF -----------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------+-----+-----+-----|
|  0  1 |  dr |  dg |  db |
`-------------------------`
2-bit tag b01
2-bit   red channel difference from the previous pixel between -2..1
2-bit green channel difference from the previous pixel between -2..1
2-bit  blue channel difference from the previous pixel between -2..1

The difference to the current channel values are using a wraparound operation,
so "1 - 2" will result in 255, while "255 + 1" will result in 0.

Values are stored as unsigned integers with a bias of 2. E.g. -2 is stored as
0 (b00). 1 is stored as 3 (b11).

The alpha value remains unchanged from the previous pixel.


.- QOI_OP_LUMA -------------------------------------.
|         Byte[0]         |         Byte[1]         |
|  7  6  5  4  3  2  1  0 |  7  6  5  4  3  2  1  0 |
|-------+-----------------+-------------+-----------|
|  1  0 |  green diff     |   dr - dg   |  db - dg  |
`---------------------------------------------------`
2-bit tag b10
6-bit green channel difference from the previous pixel -32..31
4-bit   red channel difference minus green channel difference -8..7
4-bit  blue channel difference minus green channel difference -8..7

The green channel is used to indicate the general direction of change and is
encoded in 6 bits. The red and blue channels (dr and db) base their diffs off
of the green channel difference and are encoded in 4 bits. I.e.:
	dr_dg = (cur_px.r - prev_px.r) - (cur_px.g - prev_px.g)
	db_dg = (cur_px.b - prev_px.b) - (cur_px.g - prev_px.g)

The difference to the current channel values are using a wraparound operation,
so "10 - 13" will result in 253, while "250 + 7" will result in 1.

Values are stored as unsigned integers with a bias of 32 for the green channel
and a bias of 8 for the red and blue channel.

The alpha value remains unchanged from the previous pixel.


.- QOI_OP_RUN ------------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------+-----------------|
|  1  1 |       run       |
`-------------------------`
2-bit tag b11
6-bit run-length repeating the previous pixel: 1..62

The run-length is stored with a bias of -1. Note that the run-lengths 63 and 64
(b111110 and b111111) are illegal as they are occupied by the QOI_OP_RGB and
QOI_OP_RGBA tags.


.- QOI_OP_RGB ------------------------------------------.
|         Byte[0]         | Byte[1] | Byte[2] | Byte[3] |
|  7  6  5  4  3  2  1  0 | 7 .. 0  | 7 .. 0  | 7 .. 0  |
|-------------------------+---------+---------+---------|
|  1  1  1  1  1  1  1  0 |   red   |  green  |  blue   |
`-------------------------------------------------------`
8-bit tag b11111110
8-bit   red channel value
8-bit green channel value
8-bit  blue channel value

The alpha value remains unchanged from the previous pixel.


.- QOI_OP_RGBA ---------------------------------------------------.
|         Byte[0]         | Byte[1] | Byte[2] | Byte[3] | Byte[4] |
|  7  6  5  4  3  2  1  0 | 7 .. 0  | 7 .. 0  | 7 .. 0  | 7 .. 0  |
|-------------------------+---------+---------+---------+---------|
|  1  1  1  1  1  1  1  1 |   red   |  green  |  blue   |  alpha  |
`-----------------------------------------------------------------`
8-bit tag b11111111
8-bit   red channel value
8-bit green channel value
8-bit  blue channel value
8-bit alpha channel value

*/


/* -----------------------------------------------------------------------------
Header - Public functions */

#ifndef QOI_H
#define QOI_H

#define _CRT_SECURE_NO_WARNINGS

#ifdef __cplusplus
extern "C" {
#endif

	/* A pointer to a qoi_desc struct has to be supplied to all of qoi's functions.
	It describes either the input format (for qoi_write and qoi_encode), or is
	filled with the description read from the file header (for qoi_read and
	qoi_decode).

	The colorspace in this qoi_desc is an enum where
		0 = sRGB, i.e. gamma scaled RGB channels and a linear alpha channel
		1 = all channels are linear
	You may use the constants QOI_SRGB or QOI_LINEAR. The colorspace is purely
	informative. It will be saved to the file header, but does not affect
	how chunks are en-/decoded. */

#define QOI_SRGB   0
#define QOI_LINEAR 1

	typedef struct {
		unsigned int width;
		unsigned int height;
		unsigned char channels;
		unsigned char colorspace;
	} qoi_desc;

#ifndef QOI_NO_STDIO

	/* Encode raw RGB or RGBA pixels into a QOI image and write it to the file
	system. The qoi_desc struct must be filled with the image width, height,
	number of channels (3 = RGB, 4 = RGBA) and the colorspace.

	The function returns 0 on failure (invalid parameters, or fopen or malloc
	failed) or the number of bytes written on success. */

	int qoi_write(const char* filename, const void* data, const qoi_desc* desc);


	/* Read and decode a QOI image from the file system. If channels is 0, the
	number of channels from the file header is used. If channels is 3 or 4 the
	output format will be forced into this number of channels.

	The function either returns NULL on failure (invalid data, or malloc or fopen
	failed) or a pointer to the decoded pixels. On success, the qoi_desc struct
	will be filled with the description from the file header.

	The returned pixel data should be free()d after use. */

	void* qoi_read(const char* filename, qoi_desc* desc, int channels);

#endif /* QOI_NO_STDIO */


	/* Encode raw RGB or RGBA pixels into a QOI image in memory.

	The function either returns NULL on failure (invalid parameters or malloc
	failed) or a pointer to the encoded data on success. On success the out_len
	is set to the size in bytes of the encoded data.

	The returned qoi data should be free()d after use. */

	void* qoi_encode(const void* data, const qoi_desc* desc, int* out_len);


	/* Decode a QOI image from memory.

	The function either returns NULL on failure (invalid parameters or malloc
	failed) or a pointer to the decoded pixels. On success, the qoi_desc struct
	is filled with the description from the file header.

	The returned pixel data should be free()d after use. */

	void* qoi_decode(const void* data, int size, qoi_desc* desc, int channels);

	void* qoi_encode_parallel(const void* data, const qoi_desc* desc, int* out_len);

	void* qoi_decode_parallel(const void* data, int size, qoi_desc* desc, int channels);


#ifdef __cplusplus
}
#endif
#endif /* QOI_H */


/* -----------------------------------------------------------------------------
Implementation */

#ifdef QOI_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>

#ifndef QOI_MALLOC
#define QOI_MALLOC(sz) malloc(sz)
#define QOI_FREE(p)    free(p)
#endif
#ifndef QOI_ZEROARR
#define QOI_ZEROARR(a) memset((a),0,sizeof(a))
#endif

#define QOI_OP_INDEX  0x00 /* 00xxxxxx */
#define QOI_OP_DIFF   0x40 /* 01xxxxxx */
#define QOI_OP_LUMA   0x80 /* 10xxxxxx */
#define QOI_OP_RUN    0xc0 /* 11xxxxxx */
#define QOI_OP_RGB    0xfe /* 11111110 */
#define QOI_OP_RGBA   0xff /* 11111111 */

#define QOI_MASK_2    0xc0 /* 11000000 */

#define QOI_COLOR_HASH(C) (C.rgba.r*3 + C.rgba.g*5 + C.rgba.b*7 + C.rgba.a*11)
#define QOI_MAGIC \
	(((unsigned int)'q') << 24 | ((unsigned int)'o') << 16 | \
	 ((unsigned int)'i') <<  8 | ((unsigned int)'f'))
#define QOI_HEADER_SIZE 14

/* 2GB is the max file size that this implementation can safely handle. We guard
against anything larger than that, assuming the worst case with 5 bytes per
pixel, rounded down to a nice clean value. 400 million pixels ought to be
enough for anybody. */
#define QOI_PIXELS_MAX ((unsigned int)400000000)

typedef union {
	struct { unsigned char r, g, b, a; } rgba;
	unsigned int v;
} qoi_rgba_t;

static const unsigned char qoi_padding[8] = { 0,0,0,0,0,0,0,1 };

static void qoi_write_32(unsigned char* bytes, int* p, unsigned int v) {
	bytes[(*p)++] = (0xff000000 & v) >> 24;
	bytes[(*p)++] = (0x00ff0000 & v) >> 16;
	bytes[(*p)++] = (0x0000ff00 & v) >> 8;
	bytes[(*p)++] = (0x000000ff & v);
}

static unsigned int qoi_read_32(const unsigned char* bytes, int* p) {
	unsigned int a = bytes[(*p)++];
	unsigned int b = bytes[(*p)++];
	unsigned int c = bytes[(*p)++];
	unsigned int d = bytes[(*p)++];
	return a << 24 | b << 16 | c << 8 | d;
}

void* qoi_encode(const void* data, const qoi_desc* desc, int* out_len) {
	int i, max_size, p, run;
	int px_len, px_end, px_pos, channels;
	unsigned char* bytes;
	const unsigned char* pixels;
	qoi_rgba_t index[64];
	qoi_rgba_t px, px_prev;

	if (
		data == NULL || out_len == NULL || desc == NULL ||
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 ||
		desc->height >= QOI_PIXELS_MAX / desc->width
		) {
		return NULL;
	}

	max_size =
		desc->width * desc->height * (desc->channels + 1) +
		QOI_HEADER_SIZE + sizeof(qoi_padding);

	p = 0;
	bytes = (unsigned char*)QOI_MALLOC(max_size);
	if (!bytes) {
		return NULL;
	}

	qoi_write_32(bytes, &p, QOI_MAGIC);
	qoi_write_32(bytes, &p, desc->width);
	qoi_write_32(bytes, &p, desc->height);
	bytes[p++] = desc->channels;
	bytes[p++] = desc->colorspace;


	pixels = (const unsigned char*)data;

	QOI_ZEROARR(index);

	run = 0;
	px_prev.rgba.r = 0;
	px_prev.rgba.g = 0;
	px_prev.rgba.b = 0;
	px_prev.rgba.a = 255;
	px = px_prev;

	px_len = desc->width * desc->height * desc->channels;
	px_end = px_len - desc->channels;
	channels = desc->channels;

	for (px_pos = 0; px_pos < px_len; px_pos += channels) {
		px.rgba.r = pixels[px_pos + 0];
		px.rgba.g = pixels[px_pos + 1];
		px.rgba.b = pixels[px_pos + 2];

		if (channels == 4) {
			px.rgba.a = pixels[px_pos + 3];
		}

		if (px.v == px_prev.v) {
			run++;
			if (run == 62 || px_pos == px_end) {
				bytes[p++] = QOI_OP_RUN | (run - 1);
				run = 0;
			}
		}
		else {
			int index_pos;

			if (run > 0) {
				bytes[p++] = QOI_OP_RUN | (run - 1);
				run = 0;
			}

			index_pos = QOI_COLOR_HASH(px) % 64;

			if (index[index_pos].v == px.v) {
				bytes[p++] = QOI_OP_INDEX | index_pos;
			}
			else {
				index[index_pos] = px;

				if (px.rgba.a == px_prev.rgba.a) {
					signed char vr = px.rgba.r - px_prev.rgba.r;
					signed char vg = px.rgba.g - px_prev.rgba.g;
					signed char vb = px.rgba.b - px_prev.rgba.b;

					signed char vg_r = vr - vg;
					signed char vg_b = vb - vg;

					if (
						vr > -3 && vr < 2 &&
						vg > -3 && vg < 2 &&
						vb > -3 && vb < 2
						) {
						bytes[p++] = QOI_OP_DIFF | (vr + 2) << 4 | (vg + 2) << 2 | (vb + 2);
					}
					else if (
						vg_r > -9 && vg_r <  8 &&
						vg   > -33 && vg   < 32 &&
						vg_b >  -9 && vg_b < 8
						) {
						bytes[p++] = QOI_OP_LUMA | (vg + 32);
						bytes[p++] = (vg_r + 8) << 4 | (vg_b + 8);
					}
					else {
						bytes[p++] = QOI_OP_RGB;
						bytes[p++] = px.rgba.r;
						bytes[p++] = px.rgba.g;
						bytes[p++] = px.rgba.b;
					}
				}
				else {
					bytes[p++] = QOI_OP_RGBA;
					bytes[p++] = px.rgba.r;
					bytes[p++] = px.rgba.g;
					bytes[p++] = px.rgba.b;
					bytes[p++] = px.rgba.a;
				}
			}
		}
		px_prev = px;
	}

	for (i = 0; i < (int)sizeof(qoi_padding); i++) {
		bytes[p++] = qoi_padding[i];
	}

	*out_len = p;
	return bytes;
}

void* qoi_decode(const void* data, int size, qoi_desc* desc, int channels) {
	const unsigned char* bytes;
	unsigned int header_magic;
	unsigned char* pixels;
	qoi_rgba_t index[64];
	qoi_rgba_t px;
	int px_len, chunks_len, px_pos;
	int p = 0, run = 0;

	if (
		data == NULL || desc == NULL ||
		(channels != 0 && channels != 3 && channels != 4) ||
		size < QOI_HEADER_SIZE + (int)sizeof(qoi_padding)
		) {
		return NULL;
	}

	bytes = (const unsigned char*)data;

	header_magic = qoi_read_32(bytes, &p);
	desc->width = qoi_read_32(bytes, &p);
	desc->height = qoi_read_32(bytes, &p);
	desc->channels = bytes[p++];
	desc->colorspace = bytes[p++];

	if (
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 ||
		header_magic != QOI_MAGIC ||
		desc->height >= QOI_PIXELS_MAX / desc->width
		) {
		return NULL;
	}

	if (channels == 0) {
		channels = desc->channels;
	}

	px_len = desc->width * desc->height * channels;
	pixels = (unsigned char*)QOI_MALLOC(px_len);
	if (!pixels) {
		return NULL;
	}

	QOI_ZEROARR(index);
	px.rgba.r = 0;
	px.rgba.g = 0;
	px.rgba.b = 0;
	px.rgba.a = 255;

	chunks_len = size - (int)sizeof(qoi_padding);
	for (px_pos = 0; px_pos < px_len; px_pos += channels) {
		if (run > 0) {
			run--;
		}
		else if (p < chunks_len) {
			int b1 = bytes[p++];

			if (b1 == QOI_OP_RGB) {
				px.rgba.r = bytes[p++];
				px.rgba.g = bytes[p++];
				px.rgba.b = bytes[p++];
			}
			else if (b1 == QOI_OP_RGBA) {
				px.rgba.r = bytes[p++];
				px.rgba.g = bytes[p++];
				px.rgba.b = bytes[p++];
				px.rgba.a = bytes[p++];
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
				px = index[b1];
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
				px.rgba.r += ((b1 >> 4) & 0x03) - 2;
				px.rgba.g += ((b1 >> 2) & 0x03) - 2;
				px.rgba.b += (b1 & 0x03) - 2;
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
				int b2 = bytes[p++];
				int vg = (b1 & 0x3f) - 32;
				px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
				px.rgba.g += vg;
				px.rgba.b += vg - 8 + (b2 & 0x0f);
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
				run = (b1 & 0x3f);
			}

			index[QOI_COLOR_HASH(px) % 64] = px;
		}

		pixels[px_pos + 0] = px.rgba.r;
		pixels[px_pos + 1] = px.rgba.g;
		pixels[px_pos + 2] = px.rgba.b;

		if (channels == 4) {
			pixels[px_pos + 3] = px.rgba.a;
		}
	}

	return pixels;
}

#include <omp.h>

void* qoi_encode_parallel_optimized(const void* data, const qoi_desc* desc, int* out_len) {
	if (!data || !desc || !out_len || desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 || desc->colorspace > 1 ||
		desc->height >= QOI_PIXELS_MAX / desc->width) {
		return NULL;
	}

	int max_size = desc->width * desc->height * (desc->channels + 1) +
		QOI_HEADER_SIZE + sizeof(qoi_padding);
	unsigned char* bytes = (unsigned char*)QOI_MALLOC(max_size);
	if (!bytes) return NULL;

	// Write header (sequential)
	int p = 0;
	qoi_write_32(bytes, &p, QOI_MAGIC);
	qoi_write_32(bytes, &p, desc->width);
	qoi_write_32(bytes, &p, desc->height);
	bytes[p++] = desc->channels;
	bytes[p++] = desc->colorspace;

	const unsigned char* pixels = (const unsigned char*)data;
	int channels = desc->channels;
	int width = desc->width;
	int height = desc->height;
	int total_pixels = width * height;

	// Use smaller chunks but process them sequentially
	int pixels_per_chunk = width * 8; // Process 8 rows at a time
	int num_chunks = (total_pixels + pixels_per_chunk - 1) / pixels_per_chunk;

	// Arrays to store output positions and sizes
	int* chunk_sizes = (int*)QOI_MALLOC(num_chunks * sizeof(int));
	if (!chunk_sizes) {
		QOI_FREE(bytes);
		return NULL;
	}

	// First pass: calculate sizes
#pragma omp parallel for schedule(dynamic)
	for (int chunk = 0; chunk < num_chunks; chunk++) {
		int chunk_start = chunk * pixels_per_chunk;
		int chunk_end = chunk_start + pixels_per_chunk;
		if (chunk_end > total_pixels) chunk_end = total_pixels;

		qoi_rgba_t px = { 0, 0, 0, 255 };
		qoi_rgba_t px_prev = { 0, 0, 0, 255 };
		qoi_rgba_t index[64] = { 0 };
		int run = 0;
		int local_size = 0;

		// If not first chunk, get previous pixel state
		if (chunk > 0) {
			int prev_pos = (chunk_start - 1) * channels;
			px_prev.rgba.r = pixels[prev_pos + 0];
			px_prev.rgba.g = pixels[prev_pos + 1];
			px_prev.rgba.b = pixels[prev_pos + 2];
			px_prev.rgba.a = channels == 4 ? pixels[prev_pos + 3] : 255;
		}

		for (int pos = chunk_start; pos < chunk_end; pos++) {
			int px_pos = pos * channels;

			px.rgba.r = pixels[px_pos + 0];
			px.rgba.g = pixels[px_pos + 1];
			px.rgba.b = pixels[px_pos + 2];
			px.rgba.a = channels == 4 ? pixels[px_pos + 3] : 255;

			if (px.v == px_prev.v) {
				run++;
				if (run == 62 || pos == chunk_end - 1) {
					local_size++;
					run = 0;
				}
			}
			else {
				if (run > 0) {
					local_size++;
					run = 0;
				}

				int index_pos = QOI_COLOR_HASH(px) % 64;

				if (index[index_pos].v == px.v) {
					local_size++;
				}
				else {
					index[index_pos] = px;

					if (px.rgba.a == px_prev.rgba.a) {
						signed char vr = px.rgba.r - px_prev.rgba.r;
						signed char vg = px.rgba.g - px_prev.rgba.g;
						signed char vb = px.rgba.b - px_prev.rgba.b;
						signed char vg_r = vr - vg;
						signed char vg_b = vb - vg;

						if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3 && vb < 2) {
							local_size++;
						}
						else if (vg_r > -9 && vg_r < 8 && vg > -33 && vg < 32 &&
							vg_b > -9 && vg_b < 8) {
							local_size += 2;
						}
						else {
							local_size += 4;
						}
					}
					else {
						local_size += 5;
					}
				}
			}
			px_prev = px;
		}
		chunk_sizes[chunk] = local_size;
	}

	// Calculate positions
	int write_pos = p;
	for (int i = 0; i < num_chunks; i++) {
		int current_pos = write_pos;
		write_pos += chunk_sizes[i];
	}

	// Second pass: actual encoding
#pragma omp parallel for schedule(dynamic)
	for (int chunk = 0; chunk < num_chunks; chunk++) {
		int chunk_start = chunk * pixels_per_chunk;
		int chunk_end = chunk_start + pixels_per_chunk;
		if (chunk_end > total_pixels) chunk_end = total_pixels;

		unsigned char* local_buffer = (unsigned char*)QOI_MALLOC(chunk_sizes[chunk]);
		if (!local_buffer) continue;

		qoi_rgba_t px = { 0, 0, 0, 255 };
		qoi_rgba_t px_prev = { 0, 0, 0, 255 };
		qoi_rgba_t index[64] = { 0 };
		int run = 0;
		int local_size = 0;

		// If not first chunk, get previous pixel state
		if (chunk > 0) {
			int prev_pos = (chunk_start - 1) * channels;
			px_prev.rgba.r = pixels[prev_pos + 0];
			px_prev.rgba.g = pixels[prev_pos + 1];
			px_prev.rgba.b = pixels[prev_pos + 2];
			px_prev.rgba.a = channels == 4 ? pixels[prev_pos + 3] : 255;
		}

		for (int pos = chunk_start; pos < chunk_end; pos++) {
			int px_pos = pos * channels;

			px.rgba.r = pixels[px_pos + 0];
			px.rgba.g = pixels[px_pos + 1];
			px.rgba.b = pixels[px_pos + 2];
			px.rgba.a = channels == 4 ? pixels[px_pos + 3] : 255;

			if (px.v == px_prev.v) {
				run++;
				if (run == 62 || pos == chunk_end - 1) {
					local_buffer[local_size++] = QOI_OP_RUN | (run - 1);
					run = 0;
				}
			}
			else {
				if (run > 0) {
					local_buffer[local_size++] = QOI_OP_RUN | (run - 1);
					run = 0;
				}

				int index_pos = QOI_COLOR_HASH(px) % 64;

				if (index[index_pos].v == px.v) {
					local_buffer[local_size++] = QOI_OP_INDEX | index_pos;
				}
				else {
					index[index_pos] = px;

					if (px.rgba.a == px_prev.rgba.a) {
						signed char vr = px.rgba.r - px_prev.rgba.r;
						signed char vg = px.rgba.g - px_prev.rgba.g;
						signed char vb = px.rgba.b - px_prev.rgba.b;
						signed char vg_r = vr - vg;
						signed char vg_b = vb - vg;

						if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3 && vb < 2) {
							local_buffer[local_size++] = QOI_OP_DIFF |
								((vr + 2) << 4) | ((vg + 2) << 2) | (vb + 2);
						}
						else if (vg_r > -9 && vg_r < 8 && vg > -33 && vg < 32 &&
							vg_b > -9 && vg_b < 8) {
							local_buffer[local_size++] = QOI_OP_LUMA | (vg + 32);
							local_buffer[local_size++] = ((vg_r + 8) << 4) | (vg_b + 8);
						}
						else {
							local_buffer[local_size++] = QOI_OP_RGB;
							local_buffer[local_size++] = px.rgba.r;
							local_buffer[local_size++] = px.rgba.g;
							local_buffer[local_size++] = px.rgba.b;
						}
					}
					else {
						local_buffer[local_size++] = QOI_OP_RGBA;
						local_buffer[local_size++] = px.rgba.r;
						local_buffer[local_size++] = px.rgba.g;
						local_buffer[local_size++] = px.rgba.b;
						local_buffer[local_size++] = px.rgba.a;
					}
				}
			}
			px_prev = px;
		}

		// Copy to final buffer
		int write_offset = p;
		for (int i = 0; i < chunk; i++) {
			write_offset += chunk_sizes[i];
		}

#pragma omp critical
		{
			memcpy(bytes + write_offset, local_buffer, local_size);
		}

		QOI_FREE(local_buffer);
	}

	// Update final position
	int final_pos = p;
	for (int i = 0; i < num_chunks; i++) {
		final_pos += chunk_sizes[i];
	}
	p = final_pos;

	// Write padding
	memcpy(bytes + p, qoi_padding, sizeof(qoi_padding));
	p += sizeof(qoi_padding);

	QOI_FREE(chunk_sizes);
	*out_len = p;
	return bytes;
}


void* qoi_decode_parallel_optimized(const void* data, int size, qoi_desc* desc, int channels) {
	if (data == NULL || desc == NULL ||
		(channels != 0 && channels != 3 && channels != 4) ||
		size < QOI_HEADER_SIZE + (int)sizeof(qoi_padding)) {
		return NULL;
	}

	const unsigned char* bytes = (const unsigned char*)data;
	int p = 0;

	// Read header (sequential)
	unsigned int header_magic = qoi_read_32(bytes, &p);
	desc->width = qoi_read_32(bytes, &p);
	desc->height = qoi_read_32(bytes, &p);
	desc->channels = bytes[p++];
	desc->colorspace = bytes[p++];

	if (desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 || header_magic != QOI_MAGIC ||
		desc->height >= QOI_PIXELS_MAX / desc->width) {
		return NULL;
	}

	if (channels == 0) {
		channels = desc->channels;
	}

	// Decode sequentially first
	int px_len = desc->width * desc->height * channels;
	unsigned char* pixels = (unsigned char*)QOI_MALLOC(px_len);
	if (!pixels) {
		return NULL;
	}

	// Sequential decode
	qoi_rgba_t index[64];
	QOI_ZEROARR(index);
	qoi_rgba_t px = { {0, 0, 0, 255} };
	int run = 0;
	int px_pos = 0;
	int chunks_len = size - (int)sizeof(qoi_padding);

	while (px_pos < px_len && p < chunks_len) {
		if (run > 0) {
			pixels[px_pos] = px.rgba.r;
			pixels[px_pos + 1] = px.rgba.g;
			pixels[px_pos + 2] = px.rgba.b;
			if (channels == 4) {
				pixels[px_pos + 3] = px.rgba.a;
			}
			px_pos += channels;
			run--;
			continue;
		}

		int b1 = bytes[p++];

		if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
			px = index[b1];
		}
		else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
			px.rgba.r += ((b1 >> 4) & 0x03) - 2;
			px.rgba.g += ((b1 >> 2) & 0x03) - 2;
			px.rgba.b += (b1 & 0x03) - 2;
		}
		else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
			int b2 = bytes[p++];
			int vg = (b1 & 0x3f) - 32;
			px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
			px.rgba.g += vg;
			px.rgba.b += vg - 8 + (b2 & 0x0f);
		}
		else if (b1 == QOI_OP_RGB) {
			px.rgba.r = bytes[p++];
			px.rgba.g = bytes[p++];
			px.rgba.b = bytes[p++];
		}
		else if (b1 == QOI_OP_RGBA) {
			px.rgba.r = bytes[p++];
			px.rgba.g = bytes[p++];
			px.rgba.b = bytes[p++];
			px.rgba.a = bytes[p++];
		}
		else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
			run = (b1 & 0x3f);
		}

		index[QOI_COLOR_HASH(px) % 64] = px;
		pixels[px_pos] = px.rgba.r;
		pixels[px_pos + 1] = px.rgba.g;
		pixels[px_pos + 2] = px.rgba.b;
		if (channels == 4) {
			pixels[px_pos + 3] = px.rgba.a;
		}
		px_pos += channels;
	}

	// If we need channel conversion, do it in parallel
	if (channels != desc->channels) {
		unsigned char* final_pixels = (unsigned char*)QOI_MALLOC(px_len);
		if (!final_pixels) {
			QOI_FREE(pixels);
			return NULL;
		}

#pragma omp parallel for schedule(static)
		for (int i = 0; i < desc->width * desc->height; i++) {
			int src_pos = i * desc->channels;
			int dst_pos = i * channels;
			final_pixels[dst_pos] = pixels[src_pos];
			final_pixels[dst_pos + 1] = pixels[src_pos + 1];
			final_pixels[dst_pos + 2] = pixels[src_pos + 2];
			if (channels == 4) {
				final_pixels[dst_pos + 3] = desc->channels == 4 ? pixels[src_pos + 3] : 255;
			}
		}

		QOI_FREE(pixels);
		return final_pixels;
	}

	return pixels;
}

void* qoi_encode_parallel(const void* data, const qoi_desc* desc, int* out_len) {
	if (!data || !desc || !out_len || desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 || desc->colorspace > 1 ||
		desc->height >= QOI_PIXELS_MAX / desc->width) {
		return NULL;
	}

	int max_size = desc->width * desc->height * (desc->channels + 1) +
		QOI_HEADER_SIZE + sizeof(qoi_padding);
	unsigned char* bytes = (unsigned char*)QOI_MALLOC(max_size);
	if (!bytes) return NULL;

	// Write header (sequential)
	int p = 0;
	qoi_write_32(bytes, &p, QOI_MAGIC);
	qoi_write_32(bytes, &p, desc->width);
	qoi_write_32(bytes, &p, desc->height);
	bytes[p++] = desc->channels;
	bytes[p++] = desc->colorspace;

	const unsigned char* pixels = (const unsigned char*)data;
	int channels = desc->channels;
	int width = desc->width;
	int height = desc->height;

	// Allocate arrays for row sizes and positions
	int* row_sizes = (int*)QOI_MALLOC(height * sizeof(int));
	int* row_positions = (int*)QOI_MALLOC(height * sizeof(int));
	if (!row_sizes || !row_positions) {
		QOI_FREE(bytes);
		return NULL;
	}

	// First pass: Calculate sizes
#pragma omp parallel
	{
		unsigned char* local_buffer = (unsigned char*)QOI_MALLOC(width * (channels + 1));
		if (local_buffer) {
			qoi_rgba_t index[64];
			qoi_rgba_t px_prev = { 0, 0, 0, 255 };
			qoi_rgba_t px = { 0 };

#pragma omp for schedule(static, 1)
			for (int y = 0; y < height; y++) {
				int local_size = 0;
				int run = 0;

				QOI_ZEROARR(index);
				px_prev.rgba.r = 0;
				px_prev.rgba.g = 0;
				px_prev.rgba.b = 0;
				px_prev.rgba.a = 255;

				size_t row_start = y * width * channels;

				for (int x = 0; x < width; x++) {
					size_t px_pos = row_start + x * channels;

					px.rgba.r = pixels[px_pos + 0];
					px.rgba.g = pixels[px_pos + 1];
					px.rgba.b = pixels[px_pos + 2];
					px.rgba.a = channels == 4 ? pixels[px_pos + 3] : 255;

					if (px.v == px_prev.v) {
						run++;
						if (run == 62 || x == width - 1) {
							local_size++;
							run = 0;
						}
					}
					else {
						if (run > 0) {
							local_size++;
							run = 0;
						}

						int index_pos = QOI_COLOR_HASH(px) % 64;

						if (index[index_pos].v == px.v) {
							local_size++;
						}
						else {
							index[index_pos] = px;

							if (px.rgba.a == px_prev.rgba.a) {
								signed char vr = px.rgba.r - px_prev.rgba.r;
								signed char vg = px.rgba.g - px_prev.rgba.g;
								signed char vb = px.rgba.b - px_prev.rgba.b;
								signed char vg_r = vr - vg;
								signed char vg_b = vb - vg;

								if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3 && vb < 2) {
									local_size++;
								}
								else if (vg_r > -9 && vg_r < 8 && vg > -33 && vg < 32 &&
									vg_b > -9 && vg_b < 8) {
									local_size += 2;
								}
								else {
									local_size += 4;
								}
							}
							else {
								local_size += 5;
							}
						}
					}

					px_prev = px;
				}

				if (run > 0) {
					local_size++;
				}

				row_sizes[y] = local_size;
			}

			QOI_FREE(local_buffer);
		}
	}

	// Calculate positions
	row_positions[0] = p;  // Start after header
	for (int i = 1; i < height; i++) {
		row_positions[i] = row_positions[i - 1] + row_sizes[i - 1];
	}

	// Second pass: Actually encode and write data
#pragma omp parallel
	{
		unsigned char* local_buffer = (unsigned char*)QOI_MALLOC(width * (channels + 1));
		if (local_buffer) {
			qoi_rgba_t index[64];
			qoi_rgba_t px_prev = { 0, 0, 0, 255 };
			qoi_rgba_t px = { 0 };

#pragma omp for schedule(static, 1)
			for (int y = 0; y < height; y++) {
				int local_size = 0;
				int run = 0;

				QOI_ZEROARR(index);
				px_prev.rgba.r = 0;
				px_prev.rgba.g = 0;
				px_prev.rgba.b = 0;
				px_prev.rgba.a = 255;

				size_t row_start = y * width * channels;

				for (int x = 0; x < width; x++) {
					size_t px_pos = row_start + x * channels;

					px.rgba.r = pixels[px_pos + 0];
					px.rgba.g = pixels[px_pos + 1];
					px.rgba.b = pixels[px_pos + 2];
					px.rgba.a = channels == 4 ? pixels[px_pos + 3] : 255;

					if (px.v == px_prev.v) {
						run++;
						if (run == 62 || x == width - 1) {
							local_buffer[local_size++] = QOI_OP_RUN | (run - 1);
							run = 0;
						}
					}
					else {
						if (run > 0) {
							local_buffer[local_size++] = QOI_OP_RUN | (run - 1);
							run = 0;
						}

						int index_pos = QOI_COLOR_HASH(px) % 64;

						if (index[index_pos].v == px.v) {
							local_buffer[local_size++] = QOI_OP_INDEX | index_pos;
						}
						else {
							index[index_pos] = px;

							if (px.rgba.a == px_prev.rgba.a) {
								signed char vr = px.rgba.r - px_prev.rgba.r;
								signed char vg = px.rgba.g - px_prev.rgba.g;
								signed char vb = px.rgba.b - px_prev.rgba.b;
								signed char vg_r = vr - vg;
								signed char vg_b = vb - vg;

								if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3 && vb < 2) {
									local_buffer[local_size++] = QOI_OP_DIFF |
										((vr + 2) << 4) | ((vg + 2) << 2) | (vb + 2);
								}
								else if (vg_r > -9 && vg_r < 8 && vg > -33 && vg < 32 &&
									vg_b > -9 && vg_b < 8) {
									local_buffer[local_size++] = QOI_OP_LUMA | (vg + 32);
									local_buffer[local_size++] = ((vg_r + 8) << 4) | (vg_b + 8);
								}
								else {
									local_buffer[local_size++] = QOI_OP_RGB;
									local_buffer[local_size++] = px.rgba.r;
									local_buffer[local_size++] = px.rgba.g;
									local_buffer[local_size++] = px.rgba.b;
								}
							}
							else {
								local_buffer[local_size++] = QOI_OP_RGBA;
								local_buffer[local_size++] = px.rgba.r;
								local_buffer[local_size++] = px.rgba.g;
								local_buffer[local_size++] = px.rgba.b;
								local_buffer[local_size++] = px.rgba.a;
							}
						}
					}

					px_prev = px;
				}

				if (run > 0) {
					local_buffer[local_size++] = QOI_OP_RUN | (run - 1);
				}

				// Write directly to the correct position
				memcpy(bytes + row_positions[y], local_buffer, local_size);
			}

			QOI_FREE(local_buffer);
		}
	}

	// Calculate final position
	p = row_positions[height - 1] + row_sizes[height - 1];

	// Write padding (sequential)
	memcpy(bytes + p, qoi_padding, sizeof(qoi_padding));
	p += sizeof(qoi_padding);

	QOI_FREE(row_sizes);
	QOI_FREE(row_positions);

	*out_len = p;
	return bytes;
}

void* qoi_decode_parallel(const void* data, int size, qoi_desc* desc, int channels) {
	if (data == NULL || desc == NULL ||
		(channels != 0 && channels != 3 && channels != 4) ||
		size < QOI_HEADER_SIZE + (int)sizeof(qoi_padding)) {
		return NULL;
	}

	const unsigned char* bytes = (const unsigned char*)data;
	int p = 0;

	// Read header (sequential)
	unsigned int header_magic = qoi_read_32(bytes, &p);
	desc->width = qoi_read_32(bytes, &p);
	desc->height = qoi_read_32(bytes, &p);
	desc->channels = bytes[p++];
	desc->colorspace = bytes[p++];

	if (desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 || header_magic != QOI_MAGIC ||
		desc->height >= QOI_PIXELS_MAX / desc->width) {
		return NULL;
	}

	if (channels == 0) {
		channels = desc->channels;
	}

	int px_len = desc->width * desc->height * channels;
	unsigned char* pixels = (unsigned char*)QOI_MALLOC(px_len);
	if (!pixels) {
		return NULL;
	}

	// Sequential decode to temporary buffer
	unsigned char* decoded = (unsigned char*)QOI_MALLOC(px_len);
	if (!decoded) {
		QOI_FREE(pixels);
		return NULL;
	}

	qoi_rgba_t index[64];
	QOI_ZEROARR(index);
	qoi_rgba_t px = { {0, 0, 0, 255} };
	int run = 0;
	int px_pos = 0;
	int chunks_len = size - (int)sizeof(qoi_padding);

	// Sequential decode
	while (px_pos < px_len && p < chunks_len) {
		if (run > 0) {
			run--;
		}
		else {
			int b1 = bytes[p++];

			if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
				px = index[b1];
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
				px.rgba.r += ((b1 >> 4) & 0x03) - 2;
				px.rgba.g += ((b1 >> 2) & 0x03) - 2;
				px.rgba.b += (b1 & 0x03) - 2;
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
				int b2 = bytes[p++];
				int vg = (b1 & 0x3f) - 32;
				px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
				px.rgba.g += vg;
				px.rgba.b += vg - 8 + (b2 & 0x0f);
			}
			else if (b1 == QOI_OP_RGB) {
				px.rgba.r = bytes[p++];
				px.rgba.g = bytes[p++];
				px.rgba.b = bytes[p++];
			}
			else if (b1 == QOI_OP_RGBA) {
				px.rgba.r = bytes[p++];
				px.rgba.g = bytes[p++];
				px.rgba.b = bytes[p++];
				px.rgba.a = bytes[p++];
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
				run = (b1 & 0x3f);
			}

			index[QOI_COLOR_HASH(px) % 64] = px;
		}

		decoded[px_pos] = px.rgba.r;
		decoded[px_pos + 1] = px.rgba.g;
		decoded[px_pos + 2] = px.rgba.b;
		if (desc->channels == 4) {
			decoded[px_pos + 3] = px.rgba.a;
		}
		px_pos += desc->channels;
	}

	// Parallel copy with channel conversion if needed
	const int block_size = 4096;  // Process in blocks for better cache utilization
#pragma omp parallel for schedule(static)
	for (int block = 0; block < (desc->width * desc->height + block_size - 1) / block_size; block++) {
		int start = block * block_size;
		int end = min(start + block_size, desc->width * desc->height);

		for (int i = start; i < end; i++) {
			int src_pos = i * desc->channels;
			int dst_pos = i * channels;

			pixels[dst_pos] = decoded[src_pos];
			pixels[dst_pos + 1] = decoded[src_pos + 1];
			pixels[dst_pos + 2] = decoded[src_pos + 2];
			if (channels == 4) {
				pixels[dst_pos + 3] = desc->channels == 4 ? decoded[src_pos + 3] : 255;
			}
		}
	}

	QOI_FREE(decoded);
	return pixels;
}



#ifndef QOI_NO_STDIO
#include <stdio.h>

int qoi_write(const char* filename, const void* data, const qoi_desc* desc) {
	FILE* f = fopen(filename, "wb");
	int size, err;
	void* encoded;

	if (!f) {
		return 0;
	}

	encoded = qoi_encode(data, desc, &size);
	if (!encoded) {
		fclose(f);
		return 0;
	}

	fwrite(encoded, 1, size, f);
	fflush(f);
	err = ferror(f);
	fclose(f);

	QOI_FREE(encoded);
	return err ? 0 : size;
}

void* qoi_read(const char* filename, qoi_desc* desc, int channels) {
	FILE* f = fopen(filename, "rb");
	int size, bytes_read;
	void* pixels, * data;

	if (!f) {
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	if (size <= 0 || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return NULL;
	}

	data = QOI_MALLOC(size);
	if (!data) {
		fclose(f);
		return NULL;
	}

	bytes_read = fread(data, 1, size, f);
	fclose(f);
	pixels = (bytes_read != size) ? NULL : qoi_decode(data, bytes_read, desc, channels);
	QOI_FREE(data);
	return pixels;
}

#endif /* QOI_NO_STDIO */
#endif /* QOI_IMPLEMENTATION */